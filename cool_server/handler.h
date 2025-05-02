#pragma once

#include "frame.h"
#include <memory>
#include <functional>
#include <string>
#include <vector>

namespace websocket {

class Connection {
public:
    using MessageCallback = std::function<void(const std::string&)>;
    using CloseCallback = std::function<void()>;
    using ErrorCallback = std::function<void(const std::string&)>;

    virtual ~Connection() = default;
    
    virtual void sendText(const std::string& message) = 0;
    virtual void sendPong(const std::string& message) = 0;
    virtual void close(uint16_t code = 1000, const std::string& reason = "") = 0;
    
    void setMessageCallback(MessageCallback cb) { on_message_ = std::move(cb); }
    void setCloseCallback(CloseCallback cb) { on_close_ = std::move(cb); }
    void setErrorCallback(ErrorCallback cb) { on_error_ = std::move(cb); }

protected:
    MessageCallback on_message_;
    CloseCallback on_close_;
    ErrorCallback on_error_;
};

class Handler {
public:
    virtual ~Handler() = default;
    
    virtual std::unique_ptr<Connection> handleHandshake(
        const std::string& request,
        std::string& response) = 0;
    
    virtual void handleData(
        Connection* connection,
        const std::vector<uint8_t>& data) = 0;
};

class RFC6455Handler : public Handler {
public:
    explicit RFC6455Handler(size_t max_frame_size = Frame::MAX_FRAME_SIZE);
    
    std::unique_ptr<Connection> handleHandshake(
        const std::string& request,
        std::string& response) override;
    
    void handleData(
        Connection* connection,
        const std::vector<uint8_t>& data) override;

private:
    bool parseHandshake(const std::string& request, std::string& key) const;
    std::string generateResponse(const std::string& key) const;
    
    const size_t max_frame_size_;
};

} // namespace websocket
