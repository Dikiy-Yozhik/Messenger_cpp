#pragma once
#include "iocp_core.h"
#include "socket_utils.h"
#include "websocket_connection.h"
#include <memory>
#include <unordered_set>

class Server {
public:
    Server(int port);
    ~Server();

    void start();
    void stop();

private:
    void handleNewConnection(SOCKET client_socket);
    void handleClientMessage(std::shared_ptr<websocket::WebSocketConnection> client, const std::string& message);
    void handleClientDisconnect(std::shared_ptr<websocket::WebSocketConnection> client);

    int port_;
    SOCKET listen_socket_;
    IOCPCore iocp_;
    std::unordered_set<std::shared_ptr<websocket::WebSocketConnection>> clients_;
    std::atomic<bool> is_running_;
};