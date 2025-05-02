#include "socket_utils.h"
#include <iostream>
#include <mswsock.h>  // Для AcceptEx
#pragma comment(lib, "mswsock.lib")  // Линковка библиотеки
#pragma comment(lib, "ws2_32.lib")   // Линковка Winsock

SOCKET SocketUtils::createSocket() {
    SOCKET sock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
    if (sock == INVALID_SOCKET) {
        std::cerr << "Socket creation failed: " << getLastErrorString() << "\n";
    }
    return sock;
}

bool SocketUtils::setReuseAddr(SOCKET socket) {
    if (socket == INVALID_SOCKET) return false;
    
    int yes = 1;
    if (setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&yes), sizeof(yes)) == SOCKET_ERROR) {
        std::cerr << "Setsockopt SO_REUSEADDR failed: " << getLastErrorString() << "\n";
        return false;
    }
    return true;
}

bool SocketUtils::bindSocket(SOCKET socket, int port) {
    if (socket == INVALID_SOCKET) return false;
    
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(static_cast<u_short>(port));

    if (bind(socket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed: " << getLastErrorString() << "\n";
        return false;
    }
    return true;
}

bool SocketUtils::startListening(SOCKET socket) {
    if (socket == INVALID_SOCKET) return false;
    
    if (listen(socket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Listen failed: " << getLastErrorString() << "\n";
        return false;
    }
    return true;
}

bool SocketUtils::setNonBlocking(SOCKET socket) {
    if (socket == INVALID_SOCKET) return false;
    
    u_long mode = 1;
    if (ioctlsocket(socket, FIONBIO, &mode) == SOCKET_ERROR) {
        std::cerr << "Non-blocking mode failed: " << getLastErrorString() << "\n";
        return false;
    }
    return true;
}

void SocketUtils::closeSocket(SOCKET socket) {
    if (socket != INVALID_SOCKET) {
        closesocket(socket);
    }
}

bool SocketUtils::acceptEx(SOCKET listenSocket, SOCKET acceptSocket, 
                         void* outputBuffer, DWORD bytes, 
                         LPOVERLAPPED overlapped) {
    if (listenSocket == INVALID_SOCKET || acceptSocket == INVALID_SOCKET) {
        return false;
    }

    GUID guidAcceptEx = WSAID_ACCEPTEX;
    DWORD bytesReturned = 0;
    LPFN_ACCEPTEX lpfnAcceptEx = nullptr;
    
    if (WSAIoctl(listenSocket, SIO_GET_EXTENSION_FUNCTION_POINTER,
                &guidAcceptEx, sizeof(guidAcceptEx),
                &lpfnAcceptEx, sizeof(lpfnAcceptEx),
                &bytesReturned, nullptr, nullptr) == SOCKET_ERROR) {
        std::cerr << "WSAIoctl failed for AcceptEx: " << getLastErrorString() << "\n";
        return false;
    }

    const DWORD addrBufSize = sizeof(sockaddr_in) + 16;
    if (bytes < 2 * addrBufSize) {
        std::cerr << "Buffer too small for AcceptEx, need at least " 
                 << 2 * addrBufSize << " bytes\n";
        return false;
    }

    BOOL result = lpfnAcceptEx(listenSocket, 
                              acceptSocket,
                              outputBuffer,
                              0,                  // Reserved
                              addrBufSize,         // Local address
                              addrBufSize,         // Remote address
                              &bytesReturned,
                              overlapped);

    if (result == FALSE && WSAGetLastError() != WSA_IO_PENDING) {
        std::cerr << "AcceptEx failed: " << getLastErrorString() << "\n";
        return false;
    }

    return true;
}

std::string SocketUtils::getLastErrorString() {
    DWORD error = WSAGetLastError();
    if (error == 0) return "No error";

    LPSTR messageBuffer = nullptr;
    DWORD size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | 
        FORMAT_MESSAGE_FROM_SYSTEM | 
        FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, 
        error,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPSTR>(&messageBuffer),
        0, 
        nullptr);

    std::string message;
    if (size > 0 && messageBuffer) {
        message.assign(messageBuffer, size);
        LocalFree(messageBuffer);
    } else {
        message = "Unknown error " + std::to_string(error);
    }

    return message;
}