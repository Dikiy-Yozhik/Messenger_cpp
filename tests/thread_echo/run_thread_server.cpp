#include "thread_server.h"
#include <iostream>

int main() {
    Server server(8080);
    
    if (!server.startServer()) {
        return 1;
    }
    
    std::cout << "Server started. Press Enter to stop...\n";
    
    // Запускаем сервер в отдельном потоке
    std::thread serverThread(&Server::run, &server);
    
    // Ждем нажатия Enter для остановки сервера
    std::cin.get();
    
    server.closeServer();
    serverThread.join();
    
    std::cout << "Server stopped.\n";
    
    return 0;
}