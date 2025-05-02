#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <memory>

class SocketUtils {
public:
    static SOCKET createSocket();
    static bool setReuseAddr(SOCKET socket);
    static bool bindSocket(SOCKET socket, int port);
    static bool startListening(SOCKET socket);
    static bool setNonBlocking(SOCKET socket);
    static void closeSocket(SOCKET socket);
    
    static bool acceptEx(SOCKET listenSocket, SOCKET acceptSocket, 
                       void* outputBuffer, DWORD bytes, 
                       LPOVERLAPPED overlapped);
    
    static std::string getLastErrorString();
};