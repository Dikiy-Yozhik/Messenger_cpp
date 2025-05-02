#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>

int main() {
    // Инициализация Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed.\n";
        return 1;
    }

    // Создание сокета
    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed: " << WSAGetLastError() << "\n";
        WSACleanup();
        return 1;
    }

    // Настройка адреса сервера
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(8080); // Порт 8080

    // Привязка сокета
    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed: " << WSAGetLastError() << "\n";
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    // Переход в режим прослушивания
    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Listen failed: " << WSAGetLastError() << "\n";
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Server is listening on port 8080...\n";

    // Принятие входящего соединения
    SOCKET clientSocket = accept(serverSocket, NULL, NULL);
    if (clientSocket == INVALID_SOCKET) {
        std::cerr << "Accept failed: " << WSAGetLastError() << "\n";
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Client connected.\n";

    // Буфер для приема и отправки данных
    char buffer[1024];
    int bytesReceived;

    // Получение и отправка данных
    do {
        // Получение данных от клиента
        bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesReceived > 0) {
            std::cout << "Received: " << std::string(buffer, bytesReceived) << "\n";
            
            // Отправка данных обратно клиенту
            send(clientSocket, buffer, bytesReceived, 0);
            std::cout << "Echoed back.\n";
        }
        else if (bytesReceived == 0) {
            std::cout << "Client disconnected.\n";
        }
        else {
            std::cerr << "Recv failed: " << WSAGetLastError() << "\n";
            closesocket(clientSocket);
            closesocket(serverSocket);
            WSACleanup();
            return 1;
        }
    } while (bytesReceived > 0);

    // Закрытие сокетов и очистка
    closesocket(clientSocket);
    closesocket(serverSocket);
    WSACleanup();

    return 0;
}