#include "iocp_server.h"
#include <assert.h>

Server::Server(int port, int numThreads) 
    : port(port), 
      serverSocket(INVALID_SOCKET), 
      iocpHandle(INVALID_HANDLE_VALUE), 
      isRunning(false) 
{
    // Создаем рабочие потоки (но они начнут работать только после startServer)
    workerThreads.reserve(numThreads);
}

Server::~Server() {
    stopServer();
}

bool Server::startServer() {
    // Инициализация Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed: " << WSAGetLastError() << "\n";
        return false;
    }

    // Последовательная инициализация компонентов сервера
    if (!createSocket()) return false;
    if (!bindSocket()) return false;
    if (!startListening()) return false;
    if (!setupIOCP()) return false;

    isRunning = true;
    
    // Создаем рабочие потоки после успешного старта сервера
    for (int i = 0; i < workerThreads.capacity(); ++i) {
        workerThreads.emplace_back(&Server::workerThread, this);
    }
    
    // Запускаем поток для принятия соединений
    std::thread acceptor(&Server::acceptConnections, this);
    acceptor.detach();
    
    std::cout << "Server started on port " << port 
              << " with " << workerThreads.size() << " worker threads.\n";
    return true;
}

bool Server::createSocket() {
    serverSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, 
                           NULL, 0, WSA_FLAG_OVERLAPPED);
    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed: " << WSAGetLastError() << "\n";
        WSACleanup();
        return false;
    }
    
    // Устанавливаем опцию для быстрого повторного использования адреса
    int yes = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, 
                  (char*)&yes, sizeof(yes)) == SOCKET_ERROR) {
        std::cerr << "Setsockopt failed: " << WSAGetLastError() << "\n";
        closesocket(serverSocket);
        WSACleanup();
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
        std::cerr << "Bind failed: " << WSAGetLastError() << "\n";
        closesocket(serverSocket);
        WSACleanup();
        return false;
    }
    return true;
}

bool Server::startListening() {
    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Listen failed: " << WSAGetLastError() << "\n";
        closesocket(serverSocket);
        WSACleanup();
        return false;
    }
    return true;
}

bool Server::setupIOCP() {
    // Создаем порт завершения ввода-вывода
    iocpHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    if (iocpHandle == NULL) {
        std::cerr << "CreateIoCompletionPort failed: " << GetLastError() << "\n";
        closesocket(serverSocket);
        WSACleanup();
        return false;
    }
    
    // Связываем серверный сокет с портом завершения
    if (CreateIoCompletionPort((HANDLE)serverSocket, iocpHandle, 
                             (ULONG_PTR)serverSocket, 0) == NULL) {
        std::cerr << "CreateIoCompletionPort for server socket failed: " 
                  << GetLastError() << "\n";
        CloseHandle(iocpHandle);
        closesocket(serverSocket);
        WSACleanup();
        return false;
    }
    
    return true;
}

void Server::acceptConnections() {
    while (isRunning) {
        SOCKET clientSocket = WSAAccept(serverSocket, NULL, NULL, NULL, 0);
        if (clientSocket == INVALID_SOCKET) {
            if (isRunning) {
                std::cerr << "Accept failed: " << WSAGetLastError() << "\n";
            }
            continue;
        }
        
        std::cout << "Client connected. Socket: " << clientSocket << "\n";
        
        // Создаем и инициализируем контекст для нового клиента
        auto* context = new ClientContext();
        ZeroMemory(context, sizeof(ClientContext));
        context->socket = clientSocket;
        context->wsaBuf.buf = context->buffer;
        context->wsaBuf.len = sizeof(context->buffer);
        context->operationType = OperationType::Read;
        
        // Связываем клиентский сокет с портом завершения
        if (CreateIoCompletionPort((HANDLE)clientSocket, iocpHandle, 
                                 (ULONG_PTR)context, 0) == NULL) {
            std::cerr << "CreateIoCompletionPort for client socket failed: " 
                      << GetLastError() << "\n";
            delete context;
            closesocket(clientSocket);
            continue;
        }
        
        // Начинаем асинхронное чтение данных
        startAsyncReceive(context);
    }
}

void Server::startAsyncReceive(ClientContext* context) {
    DWORD flags = 0;
    ZeroMemory(&context->overlapped, sizeof(OVERLAPPED));
    context->wsaBuf.buf = context->buffer;
    context->wsaBuf.len = sizeof(context->buffer);
    context->operationType = OperationType::Read;
    
    int result = WSARecv(context->socket, &context->wsaBuf, 1, 
                        NULL, &flags, &context->overlapped, NULL);
    
    if (result == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
        std::cerr << "WSARecv failed: " << WSAGetLastError() << "\n";
        closesocket(context->socket);
        delete context;
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

        // Пропускаем системные сообщения
        if (overlapped == nullptr) continue;
        
        context = CONTAINING_RECORD(overlapped, ClientContext, overlapped);
        
        // Обработка отключения клиента
        if (!success || bytesTransferred == 0) {
            std::cout << "Client disconnected. Socket: " << context->socket << "\n";
            closesocket(context->socket);
            delete context;
            continue;
        }
        
        // Обработка данных
        handleIOCompletion(context, bytesTransferred);
    }
}

void Server::handleIOCompletion(ClientContext* context, DWORD bytesTransferred) {
    if (context->operationType == OperationType::Read) {
        // Обработка полученных данных
        context->buffer[bytesTransferred] = '\0';
        std::cout << "Received from socket " << context->socket 
                  << ": " << context->buffer << "\n";
        
        // Подготавливаем ответ
        ZeroMemory(&context->overlapped, sizeof(OVERLAPPED));
        context->wsaBuf.buf = context->buffer;
        context->wsaBuf.len = bytesTransferred;
        context->operationType = OperationType::Write;
        
        // Отправляем ответ
        if (WSASend(context->socket, &context->wsaBuf, 1, 
                   NULL, 0, &context->overlapped, NULL) == SOCKET_ERROR) {
            if (WSAGetLastError() != WSA_IO_PENDING) {
                std::cerr << "WSASend failed: " << WSAGetLastError() << "\n";
                closesocket(context->socket);
                delete context;
            }
        }
    }
    else if (context->operationType == OperationType::Write) {
        // После отправки начинаем новое чтение
        startAsyncReceive(context);
    }
}

void Server::run() {
    // Основной поток просто ждет, пока сервер не будет остановлен
    while (isRunning) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void Server::stopServer() {
    if (!isRunning) return;
    
    isRunning = false;
    
    // Останавливаем рабочие потоки
    for (size_t i = 0; i < workerThreads.size(); ++i) {
        PostQueuedCompletionStatus(iocpHandle, 0, 0, nullptr);
    }
    
    // Дожидаемся завершения всех потоков
    for (auto& thread : workerThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    workerThreads.clear();
    
    // Закрываем сокеты и дескрипторы
    if (serverSocket != INVALID_SOCKET) {
        closesocket(serverSocket);
        serverSocket = INVALID_SOCKET;
    }
    
    if (iocpHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(iocpHandle);
        iocpHandle = INVALID_HANDLE_VALUE;
    }
    
    WSACleanup();
    std::cout << "Server stopped.\n";
}