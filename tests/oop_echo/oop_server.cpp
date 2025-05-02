#include "oop_server.h"

Server::Server(int port) : port(port), serverSocket(INVALID_SOCKET), clientSocket(INVALID_SOCKET), isRunning(false) {}

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

bool Server::acceptConnection() {
    clientSocket = accept(serverSocket, NULL, NULL);
    if (clientSocket == INVALID_SOCKET) {
        std::cerr << "Accept failed: " << WSAGetLastError() << "\n";
        return false;
    }
    std::cout << "Client connected.\n";
    return true;
}

void Server::handle() {
    char buffer[1024];
    int bytesReceived;
    
    do {
        bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesReceived > 0) {
            std::cout << "Received: " << std::string(buffer, bytesReceived) << "\n";
            send(clientSocket, buffer, bytesReceived, 0);
            std::cout << "Echoed back.\n";
        }
        else if (bytesReceived == 0) {
            std::cout << "Client disconnected.\n";
        }
        else {
            std::cerr << "Recv failed: " << WSAGetLastError() << "\n";
        }
    } while (bytesReceived > 0);
}

void Server::run() {
    if (!isRunning) return;
    
    if (acceptConnection()) {
		handle();
        closesocket(clientSocket);
    }
}

void Server::closeServer() {
    if (clientSocket != INVALID_SOCKET) {
        closesocket(clientSocket);
    }
    if (serverSocket != INVALID_SOCKET) {
        closesocket(serverSocket);
    }
    WSACleanup();
    isRunning = false;
}