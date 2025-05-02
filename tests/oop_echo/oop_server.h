#ifndef SERVER_H
#define SERVER_H

#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <iostream>

class Server {
private:
    SOCKET serverSocket;
    SOCKET clientSocket;
    int port;
    bool isRunning;
    
    bool createSocket();
    bool bindSocket();
    bool startListening();
    bool acceptConnection();
    void handle();

public:
    Server(int port);
    ~Server();
    
    bool startServer();
    void run();
    void closeServer();
};

#endif // SERVER_H
