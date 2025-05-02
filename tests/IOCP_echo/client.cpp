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
    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed: " << WSAGetLastError() << "\n";
        WSACleanup();
        return 1;
    }

    // Настройка адреса сервера
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(8080); // Порт сервера
    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr); // Локальный адрес

    // Подключение к серверу
    if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Connect failed: " << WSAGetLastError() << "\n";
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Connected to server. Type messages to echo (type 'exit' to quit).\n";

    // Буфер для ввода и вывода
    char buffer[1024];
    int bytesReceived;

    // Цикл отправки и получения сообщений
    do {
        // Ввод сообщения
        std::cout << "> ";
        std::cin.getline(buffer, sizeof(buffer));

        // Проверка на команду выхода
        if (strcmp(buffer, "exit") == 0) {
            std::cout << "Closing connection...\n";
            break;
        }

        // Отправка сообщения серверу
        if (send(clientSocket, buffer, strlen(buffer), 0) == SOCKET_ERROR) {
            std::cerr << "Send failed: " << WSAGetLastError() << "\n";
            closesocket(clientSocket);
            WSACleanup();
            return 1;
        }

        // Получение ответа от сервера
        bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0'; // Добавляем завершающий нуль
            std::cout << "Server echo: " << buffer << "\n";
        }
        else if (bytesReceived == 0) {
            std::cout << "Server disconnected.\n";
        }
        else {
            std::cerr << "Recv failed: " << WSAGetLastError() << "\n";
            closesocket(clientSocket);
            WSACleanup();
            return 1;
        }
    } while (true);

    // Закрытие сокета и очистка
    closesocket(clientSocket);
    WSACleanup();

    return 0;
}