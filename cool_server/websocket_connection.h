#pragma once

#include "iocp_core.h"
#include "frame.h"
#include <winsock2.h>
#include <vector>
#include <functional>
#include <atomic>
#include <mutex>
#include <memory>

namespace websocket {

class WebSocketConnection : public std::enable_shared_from_this<WebSocketConnection> {
public:
    using MessageCallback = std::function<void(const std::string&)>;
    using CloseCallback = std::function<void()>;

    WebSocketConnection(SOCKET socket, IOCPCore& iocp);
    ~WebSocketConnection();

    void sendText(const std::string& message);
    void sendBinary(const std::vector<uint8_t>& data);
    void sendPong(const std::string& message);
    void close(uint16_t code = 1000, const std::string& reason = "");

    void setMessageCallback(MessageCallback cb);
    void setCloseCallback(CloseCallback cb);

    void handleIOCompletion(DWORD bytes_transferred, LPOVERLAPPED overlapped);

private:
    struct AsyncOperation {
        OVERLAPPED overlapped;
        std::vector<uint8_t> buffer;
    };

    void asyncRead();
    void asyncWrite(std::vector<uint8_t>&& data);
    void processData(const std::vector<uint8_t>& data);
    void handleFrame(const FrameHeader& header, std::vector<uint8_t>&& payload);

    SOCKET socket_;
    IOCPCore& iocp_;
    std::atomic<bool> is_closed_;
    std::mutex socket_mutex_;
    AsyncOperation read_operation_;
    std::vector<uint8_t> fragmented_buffer_;
    Opcode current_opcode_ = Opcode::Continuation;
    
    std::mutex callbacks_mutex_;
    MessageCallback on_message_;
    CloseCallback on_close_;

    std::vector<uint8_t> read_buffer_;  // Заменяет фиксированный буфер
    size_t expected_payload_size_ = 0;
};

} // namespace websocket