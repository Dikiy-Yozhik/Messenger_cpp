#include "thread_server.h"
#include <algorithm>

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

void Server::handleClient(SOCKET clientSocket) {
    char buffer[1024];
    int bytesReceived;
    
    {
        std::lock_guard<std::mutex> lock(consoleMutex);
        std::cout << "Client connected. Socket: " << clientSocket << "\n";
    }

    do {
        bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesReceived > 0) {
            {
                std::lock_guard<std::mutex> lock(consoleMutex);
                std::cout << "Received from client " << clientSocket << ": " 
                          << std::string(buffer, bytesReceived) << "\n";
            }
            send(clientSocket, buffer, bytesReceived, 0);
        }
        else if (bytesReceived == 0) {
            std::lock_guard<std::mutex> lock(consoleMutex);
            std::cout << "Client disconnected. Socket: " << clientSocket << "\n";
        }
        else {
            std::lock_guard<std::mutex> lock(consoleMutex);
            std::cerr << "Recv failed for client " << clientSocket << ": " << WSAGetLastError() << "\n";
        }
    } while (bytesReceived > 0);

    closesocket(clientSocket);
}

void Server::cleanupThreads() {
    clientThreads.erase(
        std::remove_if(clientThreads.begin(), clientThreads.end(),
            [](std::thread &t) {
                return !t.joinable();
            }),
        clientThreads.end());
}

void Server::run() {
    if (!isRunning) return;
    
    while (isRunning) {
        SOCKET clientSocket = accept(serverSocket, NULL, NULL);
        if (clientSocket == INVALID_SOCKET) {
            if (isRunning) {
                std::cerr << "Accept failed: " << WSAGetLastError() << "\n";
            }
            continue;
        }

        // Очистка завершенных потоков без блокировки
        clientThreads.erase(
            std::remove_if(clientThreads.begin(), clientThreads.end(),
                [](std::thread &t) {
                    if (t.joinable()) {
                        t.join();
                        return true;
                    }
                    return false;
                }),
            clientThreads.end());

        // Создаем новый поток для обработки клиента
        clientThreads.emplace_back(&Server::handleClient, this, clientSocket);
        clientThreads.back().detach(); // Отсоединяем поток
    }
}

void Server::closeServer() {
    isRunning = false;
    
    // Закрываем сокет, чтобы выйти из accept
    if (serverSocket != INVALID_SOCKET) {
        closesocket(serverSocket);
        serverSocket = INVALID_SOCKET;
    }
    
    // Ожидаем завершения всех клиентских потоков
    cleanupThreads();
    
    WSACleanup();
}