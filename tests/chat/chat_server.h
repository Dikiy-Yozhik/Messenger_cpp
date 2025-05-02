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
#include <mutex>
#include <algorithm>

enum class OperationType {
    Read,
    Write
};

enum class ClientState {
    AWAITING_NICKNAME,
    IN_CHAT
};

struct ClientContext {
    OVERLAPPED overlapped;
    SOCKET socket;
    WSABUF wsaBuf;
    char buffer[1024];
    DWORD bytesTransferred;
    DWORD flags;
    OperationType operationType;
};

struct ClientInfo {
    SOCKET socket;
    std::string nickname;
    ClientContext* context;
    ClientState state;
};

class Server {
private:
    SOCKET serverSocket;
    HANDLE iocpHandle;
    int port;
    std::atomic<bool> isRunning;
    std::vector<std::thread> workerThreads;
    std::vector<ClientInfo> clients;
    std::mutex clientsMutex;
    
    bool createSocket();
    bool bindSocket();
    bool startListening();
    bool setupIOCP();
    void workerThread();
    void handleIOCompletion(ClientContext* context, DWORD bytesTransferred);
    void acceptConnections();
    void startAsyncReceive(ClientContext* context);
    
    void handleNicknamePhase(ClientInfo& client, const std::string& message);
    void handleChatPhase(ClientInfo& client, const std::string& message);
    void broadcastMessage(const std::string& message, SOCKET excludeSocket = INVALID_SOCKET);
    void addClient(ClientInfo&& client);
    void removeClient(SOCKET socket);

public:
    Server(int port, int numThreads = std::thread::hardware_concurrency());
    ~Server();
    
    bool startServer();
    void stopServer();
    void run();
};

#endif // SERVER_H