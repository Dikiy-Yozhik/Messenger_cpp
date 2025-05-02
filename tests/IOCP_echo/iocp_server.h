#ifndef SERVER_H
#define SERVER_H

#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <iostream>
#include <vector>
#include <thread>
#include <memory>
#include <atomic>

enum class OperationType {
    Read,
    Write
};

// Структура для хранения информации о клиенте
struct ClientContext {
    OVERLAPPED overlapped;
    SOCKET socket;
    WSABUF wsaBuf;
    char buffer[1024];
    DWORD bytesTransferred;
    DWORD flags;
    OperationType operationType;  // Добавляем это поле
};

class Server {
private:
    SOCKET serverSocket;
    HANDLE iocpHandle;
    int port;
    std::atomic<bool> isRunning;
    std::vector<std::thread> workerThreads;
    
    bool createSocket();
    bool bindSocket();
    bool startListening();
    bool setupIOCP();
    void workerThread();
    void handleIOCompletion(ClientContext* context, DWORD bytesTransferred);
    void acceptConnections();
    void startAsyncReceive(ClientContext* context);

public:
    Server(int port, int numThreads = std::thread::hardware_concurrency());
    ~Server();
    
    bool startServer();
    void run();
    void stopServer();
};

#endif // SERVER_H