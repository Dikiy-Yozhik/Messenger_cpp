#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <atomic>
#include <string>

std::atomic<bool> g_running(true);

void ClearCurrentLine() {
    std::cout << "\r" << std::string(80, ' ') << "\r";
}

void ReceiveMessages(SOCKET clientSocket) {
    char buffer[1024];
    while (g_running) {
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0';
            
            // Очищаем текущую строку и выводим сообщение
            ClearCurrentLine();
            std::cout << buffer << std::endl; // endl добавляет перевод строки
            
            // Выводим приглашение только если не было команды выхода
            if (g_running) {
                std::cout << "> " << std::flush;
            }
        } 
        else if (bytesReceived == 0) {
            ClearCurrentLine();
            std::cout << "Server closed the connection." << std::endl;
            g_running = false;
        } 
        else {
            if (WSAGetLastError() != WSAECONNRESET) {
                ClearCurrentLine();
                std::cerr << "Recv error: " << WSAGetLastError() << std::endl;
            }
            g_running = false;
        }
    }
}

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed." << std::endl;
        return 1;
    }

    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);

    std::cout << "Connecting to server..." << std::endl;
    if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Connect failed: " << WSAGetLastError() << std::endl;
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Connected to server!" << std::endl << std::endl;
    std::cout << "Enter your nickname: ";
    
    char nickname[256];
    std::cin.getline(nickname, sizeof(nickname));
    
    if (send(clientSocket, nickname, strlen(nickname), 0) == SOCKET_ERROR) {
        std::cerr << "Failed to send nickname: " << WSAGetLastError() << std::endl;
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    std::thread receiver(ReceiveMessages, clientSocket);
    
    std::cout << "Type messages (type 'exit' to quit):" << std::endl;
    std::cout << "> " << std::flush;
    
    char buffer[1024];
    while (g_running) {
        std::cin.getline(buffer, sizeof(buffer));
        
        if (!g_running) break;
        
        if (strcmp(buffer, "exit") == 0) {
            g_running = false;
            break;
        }
        
        // Отправляем сообщение без его предварительного вывода
        if (send(clientSocket, buffer, strlen(buffer), 0) == SOCKET_ERROR) {
            std::cerr << "Send failed" << std::endl;
            g_running = false;
        }
        
        // После отправки просто обновляем приглашение
        if (g_running) {
            std::cout << "> " << std::flush;
        }
    }

    shutdown(clientSocket, SD_SEND);
    if (receiver.joinable()) {
        receiver.join();
    }
    
    closesocket(clientSocket);
    WSACleanup();
    return 0;
}
