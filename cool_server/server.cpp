#include "server.h"
#include <iostream>

Server::Server(int port) 
    : port_(port), 
      listen_socket_(INVALID_SOCKET),
      is_running_(false) {}

Server::~Server() {
    stop();
}

void Server::start() {
    // Инициализация Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        throw std::runtime_error("WSAStartup failed");
    }

    // Создание сокета
    listen_socket_ = SocketUtils::createSocket();
    SocketUtils::setReuseAddr(listen_socket_);
    SocketUtils::bindSocket(listen_socket_, port_);
    SocketUtils::startListening(listen_socket_);

    // Настройка IOCP
    if (!iocp_.setup()) {
        throw std::runtime_error("IOCP setup failed");
    }
    iocp_.associateSocket(listen_socket_, static_cast<ULONG_PTR>(IOCPCore::CompletionKeyType::CONNECTION));

    // Коллбэк для новых подключений
    iocp_.setConnectionCallback([this](DWORD, ULONG_PTR, LPOVERLAPPED) {
        SOCKET client_socket = SocketUtils::createSocket();
        if (SocketUtils::acceptEx(listen_socket_, client_socket, nullptr, 0, 0)) {
            handleNewConnection(client_socket);
        }
    });

    // Запуск рабочих потоков
    iocp_.runWorkerThreads(4);
    is_running_ = true;
    std::cout << "Server started on port " << port_ << "\n";
}

void Server::stop() {
    if (!is_running_) return;
    
    is_running_ = false;
    iocp_.stop();
    SocketUtils::closeSocket(listen_socket_);
    WSACleanup();
    std::cout << "Server stopped\n";
}

void Server::handleNewConnection(SOCKET client_socket) {
    auto client = std::make_shared<websocket::WebSocketConnection>(client_socket, iocp_);
    
    client->setMessageCallback([this, client](const std::string& message) {
        handleClientMessage(client, message);
    });

    client->setCloseCallback([this, client]() {
        handleClientDisconnect(client);
    });

    clients_.insert(client);
    std::cout << "New client connected. Total clients: " << clients_.size() << "\n";
}

void Server::handleClientMessage(std::shared_ptr<websocket::WebSocketConnection> client, const std::string& message) {
    std::cout << "Received: " << message << "\n";
    client->sendText("Echo: " + message);  // Ответ эхо-сообщением
}

void Server::handleClientDisconnect(std::shared_ptr<websocket::WebSocketConnection> client) {
    clients_.erase(client);
    std::cout << "Client disconnected. Total clients: " << clients_.size() << "\n";
}