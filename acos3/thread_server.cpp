#include "thread_server.h"
#include <algorithm>
#include <chrono>

Server::Server(int port) : port(port), serverSocket(INVALID_SOCKET), isRunning(false) {}

Server::~Server() {
    closeServer();
}

bool Server::startServer() {
    // Инициализация Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed.\n";
        return false;
    }

    if (!createSocket() || !bindSocket() || !startListening()) {
        return false;
    }

    isRunning = true;
    return true;
}

bool Server::createSocket() {
    serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed: " << WSAGetLastError() << "\n";
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
    std::cout << "Server is listening on port " << port << "...\n";
    return true;
}

void Server::sendResponse(SOCKET clientSocket, const std::string& response) {
    std::lock_guard<std::mutex> socketLock(socketMutex);
    
    // Проверяем, что сокет еще валиден
    if (clientSocket == INVALID_SOCKET) {
        return;
    }
    
    int result = send(clientSocket, response.c_str(), response.length(), 0);
    if (result == SOCKET_ERROR) {
        int error = WSAGetLastError();
        if (error != WSAECONNRESET && error != WSAENOTSOCK) {
            std::lock_guard<std::mutex> consoleLock(consoleMutex);
            std::cerr << "Send failed for client " << clientSocket 
                      << ": " << error << "\n";
        }
    }
}

void Server::handleClient(SOCKET clientSocket) {
    char buffer[1024];
    int bytesReceived;
    
    {
        std::lock_guard<std::mutex> lock(consoleMutex);
        std::cout << "Client connected. Socket: " << clientSocket << "\n";
    }

    while (isRunning.load()) {
        bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        
        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0';
            std::string message(buffer, bytesReceived);
            
            {
                std::lock_guard<std::mutex> lock(consoleMutex);
                std::cout << "[Producer] Received from client " << clientSocket 
                          << ": " << message << " (queue size: " 
                          << taskQueue.size() << ")\n";
            }
            
            // Создаем задачу и кладем в общую очередь
            ClientTask task(clientSocket, message);
            taskQueue.push(task);
        }
        else if (bytesReceived == 0) {
            std::lock_guard<std::mutex> lock(consoleMutex);
            std::cout << "Client disconnected. Socket: " << clientSocket << "\n";
            break;
        }
        else {
            int error = WSAGetLastError();
            // Проверяем, не закрыто ли соединение
            if (error == WSAECONNRESET || error == WSAENOTCONN) {
                std::lock_guard<std::mutex> lock(consoleMutex);
                std::cout << "Client connection lost. Socket: " << clientSocket << "\n";
            }
            else {
                std::lock_guard<std::mutex> lock(consoleMutex);
                std::cerr << "Recv failed for client " << clientSocket 
                          << ": " << error << "\n";
            }
            break;
        }
    }
    
    // Закрываем сокет клиента
    {
        std::lock_guard<std::mutex> socketLock(socketMutex);
        closesocket(clientSocket);
    }
}

void Server::processTasks() {
    {
        std::lock_guard<std::mutex> lock(consoleMutex);
        std::cout << "[Consumer] Worker thread started. Thread ID: " 
                  << std::this_thread::get_id() << "\n";
    }
    
    while (isRunning.load() || !taskQueue.empty()) {
        try {
            // Берем задачу из очереди 
            ClientTask task = taskQueue.pop();
            
            // Проверяем, не пустая ли задача
            if (task.clientSocket == INVALID_SOCKET && task.message.empty()) {
                break;
            }
            
            {
                std::lock_guard<std::mutex> lock(consoleMutex);
                std::cout << "[Consumer] Processing task from client " 
                          << task.clientSocket << ": " << task.message << "\n";
            }
            
            // Имитация обработки
            std::string processedMessage = "PROCESSED: " + task.message;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            {
                std::lock_guard<std::mutex> lock(consoleMutex);
                std::cout << "[Consumer] Sending response to client " 
                          << task.clientSocket << ": " << processedMessage << "\n";
            }
            
            sendResponse(task.clientSocket, processedMessage);
            
        }
        catch (const std::exception& e) {
            std::lock_guard<std::mutex> lock(consoleMutex);
            std::cerr << "[Consumer] Exception in worker thread: " << e.what() << "\n";
        }
    }
    
    {
        std::lock_guard<std::mutex> lock(consoleMutex);
        std::cout << "[Consumer] Worker thread stopped. Thread ID: " 
                  << std::this_thread::get_id() << "\n";
    }
}

void Server::run() {
    if (!isRunning.load()) return;
    
    // Запускаем поток-обработчик 
    workerThreads.emplace_back(&Server::processTasks, this);
    
    {
        std::lock_guard<std::mutex> lock(consoleMutex);
        std::cout << "Worker thread created. Ready to accept connections...\n";
    }
    
    while (isRunning.load()) {
        SOCKET clientSocket = accept(serverSocket, NULL, NULL);
        if (clientSocket == INVALID_SOCKET) {
            if (isRunning.load()) {
                std::cerr << "Accept failed: " << WSAGetLastError() << "\n";
            }
            continue;
        }
        
        // Безопасное создание потока для клиента
        std::lock_guard<std::mutex> lock(consoleMutex);
        clientThreads.emplace_back(&Server::handleClient, this, clientSocket);
        clientThreads.back().detach();  
        
        std::cout << "New client connected. Socket: " << clientSocket 
                  << " (Active threads: " << clientThreads.size() << ")\n";
    }
}

void Server::closeServer() {
    if (!isRunning.load()) return;
    
    isRunning.store(false);
    
    {
        std::lock_guard<std::mutex> lock(consoleMutex);
        std::cout << "Stopping server...\n";
    }
    
    // Останавливаем очередь
    taskQueue.stop();
    
    // Закрываем серверный сокет
    if (serverSocket != INVALID_SOCKET) {
        closesocket(serverSocket);
        serverSocket = INVALID_SOCKET;
    }
    
    // Ждем завершения worker-потока
    if (!workerThreads.empty() && workerThreads[0].joinable()) {
        workerThreads[0].join();
    }
    
    // Удаляем зомби потоки из вектора
    clientThreads.erase(std::remove_if(clientThreads.begin(), clientThreads.end(),[](std::thread &t) {
        return !t.joinable();  }),clientThreads.end());
    
    workerThreads.clear();
    clientThreads.clear();
    
    WSACleanup();
    
    {
        std::lock_guard<std::mutex> lock(consoleMutex);
        std::cout << "Server stopped.\n";
    }
}
