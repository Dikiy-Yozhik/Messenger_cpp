#include "server.h"
#include <iostream>
#include <csignal>

std::unique_ptr<Server> server;

void signalHandler(int signal) {
    if (server) {
        server->stop();
    }
    exit(signal);
}

int main() {
    signal(SIGINT, signalHandler);  // Обработка Ctrl+C

    try {
        server = std::make_unique<Server>(8080);  // Порт 8080
        server->start();

        // Ожидание завершения
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}