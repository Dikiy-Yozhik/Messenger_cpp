#include "chat_server.h"
#include <iostream>

int main() {
    Server server(8080);
    
    if (!server.startServer()) {
        return 1;
    }
    
    std::cout << "Server is running. Press Enter to stop...\n";
    std::cin.get();
    
    server.stopServer();
    
    return 0;
}
