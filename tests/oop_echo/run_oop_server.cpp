#include "oop_server.h"

int main() {
    Server server(8080);
    
    if (!server.startServer()) {
        return 1;
    }
    
    server.run();
    server.closeServer();
    
    return 0;
}
