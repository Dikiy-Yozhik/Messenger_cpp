#include "chat_server.h"

Server::Server(int port, int numThreads) 
    : port(port), 
      serverSocket(INVALID_SOCKET), 
      iocpHandle(INVALID_HANDLE_VALUE), 
      isRunning(false) {
    workerThreads.reserve(numThreads);
}

Server::~Server() {
    stopServer();
}

bool Server::startServer() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed\n";
        return false;
    }

    if (!createSocket()) return false;
    if (!bindSocket()) return false;
    if (!startListening()) return false;
    if (!setupIOCP()) return false;

    isRunning = true;
    
    for (int i = 0; i < workerThreads.capacity(); ++i) {
        workerThreads.emplace_back(&Server::workerThread, this);
    }
    
    std::thread acceptor(&Server::acceptConnections, this);
    acceptor.detach();
    
    std::cout << "Chat server started on port " << port << "\n";
    return true;
}

bool Server::createSocket() {
    serverSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed\n";
        return false;
    }
    
    int yes = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&yes, sizeof(yes)) == SOCKET_ERROR) {
        std::cerr << "Setsockopt failed\n";
        closesocket(serverSocket);
        return false;
    }
    
    return true;
}

bool Server::bindSocket() {
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed\n";
        closesocket(serverSocket);
        return false;
    }
    return true;
}

bool Server::startListening() {
    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Listen failed\n";
        closesocket(serverSocket);
        return false;
    }
    return true;
}

bool Server::setupIOCP() {
    iocpHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    if (iocpHandle == NULL) {
        std::cerr << "CreateIoCompletionPort failed\n";
        closesocket(serverSocket);
        return false;
    }
    
    if (CreateIoCompletionPort((HANDLE)serverSocket, iocpHandle, (ULONG_PTR)serverSocket, 0) == NULL) {
        std::cerr << "Failed to associate server socket\n";
        CloseHandle(iocpHandle);
        closesocket(serverSocket);
        return false;
    }
    
    return true;
}

void Server::acceptConnections() {
    while (isRunning) {
        SOCKET clientSocket = WSAAccept(serverSocket, NULL, NULL, NULL, 0);
        if (clientSocket == INVALID_SOCKET) {
            if (isRunning) {
                std::cerr << "Accept failed\n";
            }
            continue;
        }
        
        std::cout << "New connection: " << clientSocket << "\n";
        
        auto* context = new ClientContext();
        ZeroMemory(context, sizeof(ClientContext));
        context->socket = clientSocket;
        context->wsaBuf.buf = context->buffer;
        context->wsaBuf.len = sizeof(context->buffer);
        context->operationType = OperationType::Read;
        
        if (CreateIoCompletionPort((HANDLE)clientSocket, iocpHandle, (ULONG_PTR)context, 0) == NULL) {
            std::cerr << "Failed to associate client\n";
            delete context;
            closesocket(clientSocket);
            continue;
        }
        
        addClient({clientSocket, "", context, ClientState::AWAITING_NICKNAME});
        
        std::string welcomeMsg = "Enter your nickname: ";
        send(clientSocket, welcomeMsg.c_str(), welcomeMsg.size(), 0);
        
        startAsyncReceive(context);
    }
}

void Server::startAsyncReceive(ClientContext* context) {
    DWORD flags = 0;
    ZeroMemory(&context->overlapped, sizeof(OVERLAPPED));
    context->wsaBuf.buf = context->buffer;
    context->wsaBuf.len = sizeof(context->buffer);
    context->operationType = OperationType::Read;
    
    if (WSARecv(context->socket, &context->wsaBuf, 1, NULL, &flags, &context->overlapped, NULL) == SOCKET_ERROR && 
        WSAGetLastError() != WSA_IO_PENDING) {
        std::cerr << "WSARecv failed\n";
        removeClient(context->socket);
    }
}

void Server::workerThread() {
    while (isRunning) {
        ClientContext* context = nullptr;
        DWORD bytesTransferred = 0;
        ULONG_PTR completionKey = 0;
        LPOVERLAPPED overlapped = nullptr;
        
        BOOL success = GetQueuedCompletionStatus(
            iocpHandle,
            &bytesTransferred,
            &completionKey,
            &overlapped,
            INFINITE);
        
        if (!isRunning) break;
        if (!overlapped) continue;
        
        context = CONTAINING_RECORD(overlapped, ClientContext, overlapped);
        
        if (!success || bytesTransferred == 0) {
            removeClient(context->socket);
            continue;
        }
        
        handleIOCompletion(context, bytesTransferred);
    }
}

void Server::handleIOCompletion(ClientContext* context, DWORD bytesTransferred) {
    context->buffer[bytesTransferred] = '\0';
    std::string message(context->buffer, bytesTransferred);
    
    std::lock_guard<std::mutex> lock(clientsMutex);
    auto it = std::find_if(clients.begin(), clients.end(),
        [context](const ClientInfo& ci) { return ci.context == context; });
    
    if (it == clients.end()) {
        closesocket(context->socket);
        delete context;
        return;
    }
    
    switch (it->state) {
        case ClientState::AWAITING_NICKNAME:
            handleNicknamePhase(*it, message);
            break;
        case ClientState::IN_CHAT:
            handleChatPhase(*it, message);
            break;
    }
    
    startAsyncReceive(context);
}

void Server::handleNicknamePhase(ClientInfo& client, const std::string& nickname) {
    client.nickname = nickname;
    client.state = ClientState::IN_CHAT;
    
    std::cout << "New user: " << client.nickname << "\n";
    
    std::string reply = "Welcome, " + client.nickname + "!\n";
    send(client.socket, reply.c_str(), reply.size(), 0);
    
    broadcastMessage("User " + client.nickname + " joined the chat\n");
}

void Server::handleChatPhase(ClientInfo& client, const std::string& message) {
    std::string fullMsg = client.nickname + ": " + message + "\n";
    std::cout << fullMsg;
    broadcastMessage(fullMsg, client.socket);
}

void Server::broadcastMessage(const std::string& message, SOCKET excludeSocket) {
    for (const auto& client : clients) {
        if (client.socket != excludeSocket && client.state == ClientState::IN_CHAT) {
            send(client.socket, message.c_str(), message.size(), 0);
        }
    }
}

void Server::addClient(ClientInfo&& client) {
    std::lock_guard<std::mutex> lock(clientsMutex);
    clients.push_back(std::move(client));
}

void Server::removeClient(SOCKET socket) {
    std::lock_guard<std::mutex> lock(clientsMutex);
    auto it = std::find_if(clients.begin(), clients.end(),
        [socket](const ClientInfo& ci) { return ci.socket == socket; });
    
    if (it != clients.end()) {
        std::cout << "User left: " << it->nickname << "\n";
        
        if (it->state == ClientState::IN_CHAT) {
            broadcastMessage("User " + it->nickname + " left the chat\n");
        }
        
        closesocket(it->socket);
        delete it->context;
        clients.erase(it);
    }
}

void Server::run() {
    while (isRunning) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void Server::stopServer() {
    if (!isRunning) return;
    
    isRunning = false;
    
    for (size_t i = 0; i < workerThreads.size(); ++i) {
        PostQueuedCompletionStatus(iocpHandle, 0, 0, nullptr);
    }
    
    for (auto& thread : workerThreads) {
        if (thread.joinable()) thread.join();
    }
    workerThreads.clear();
    
    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        for (auto& client : clients) {
            closesocket(client.socket);
            delete client.context;
        }
        clients.clear();
    }
    
    if (serverSocket != INVALID_SOCKET) {
        closesocket(serverSocket);
        serverSocket = INVALID_SOCKET;
    }
    
    if (iocpHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(iocpHandle);
        iocpHandle = INVALID_HANDLE_VALUE;
    }
    
    WSACleanup();
    std::cout << "Server stopped\n";
}
