#ifndef SERVER_H
#define SERVER_H

#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <iostream>
#include <vector>
#include <thread>
#include <mutex>

class Server {
private:
    SOCKET serverSocket;
    int port;
    bool isRunning;
    std::vector<std::thread> clientThreads;
    std::mutex consoleMutex;
    
    bool createSocket();
    bool bindSocket();
    bool startListening();
    void handleClient(SOCKET clientSocket);
    void cleanupThreads();

public:
    Server(int port);
    ~Server();
    
    bool startServer();
    void run();
    void closeServer();
};

#endif // SERVER_H