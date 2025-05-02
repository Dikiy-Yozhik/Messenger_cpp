#include "handler.h"
#include "frame.h"
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <sstream>
#include <stdexcept>
#include <algorithm>

namespace websocket {

namespace {
    const std::string MAGIC_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    const std::string HANDSHAKE_HEADER = "HTTP/1.1 101 Switching Protocols\r\n"
                                         "Upgrade: websocket\r\n"
                                         "Connection: Upgrade\r\n"
                                         "Sec-WebSocket-Accept: ";
    
    std::string base64_encode(const std::string& input) {
        BIO* b64 = BIO_new(BIO_f_base64());
        BIO* mem = BIO_new(BIO_s_mem());
        BIO_push(b64, mem);
        BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
        BIO_write(b64, input.data(), static_cast<int>(input.size()));
        BIO_flush(b64);
        
        char* data;
        long len = BIO_get_mem_data(mem, &data);
        std::string result(data, len);
        
        BIO_free_all(b64);
        return result;
    }
}

RFC6455Handler::RFC6455Handler(size_t max_frame_size)
    : max_frame_size_(std::min(max_frame_size, Frame::MAX_FRAME_SIZE)) {}

std::unique_ptr<Connection> RFC6455Handler::handleHandshake(
    const std::string& request, 
    std::string& response) {
    
    std::string client_key;
    if (!parseHandshake(request, client_key)) {
        return nullptr;
    }
    
    // Вычисляем accept-key
    std::string combined = client_key + MAGIC_GUID;
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(combined.data()), combined.size(), hash);
    
    std::string accept = base64_encode(std::string(reinterpret_cast<char*>(hash), SHA_DIGEST_LENGTH));
    
    response = HANDSHAKE_HEADER + accept + "\r\n\r\n";
    return std::unique_ptr<Connection>(); // Заменить на реальную реализацию
}

bool RFC6455Handler::parseHandshake(const std::string& request, std::string& key) const {
    std::istringstream iss(request);
    std::string line;
    
    // Проверяем первую строку
    std::getline(iss, line);
    if (line.find("GET") == std::string::npos) {
        return false;
    }
    
    // Ищем необходимые заголовки
    bool upgrade_websocket = false;
    bool connection_upgrade = false;
    
    while (std::getline(iss, line)) {
        if (line.find("Sec-WebSocket-Key:") != std::string::npos) {
            size_t start = line.find(':') + 1;
            while (start < line.size() && (line[start] == ' ' || line[start] == '\t')) start++;
            size_t end = line.find('\r', start);
            key = line.substr(start, end - start);
        }
        else if (line.find("Upgrade: websocket") != std::string::npos) {
            upgrade_websocket = true;
        }
        else if (line.find("Connection: Upgrade") != std::string::npos) {
            connection_upgrade = true;
        }
    }
    
    return !key.empty() && upgrade_websocket && connection_upgrade;
}

void RFC6455Handler::handleData(Connection* connection, const std::vector<uint8_t>& data) {
    if (!connection) return;
    
    try {
        FrameHeader header;
        if (!Frame::parseHeader(data, header)) {
            throw std::runtime_error("Invalid frame header");
        }
        
        if (header.payload_length > max_frame_size_) {
            connection->close(1009, "Frame too large");
            return;
        }
        
        const size_t header_size = Frame::headerSize(header);
        if (data.size() < header_size + header.payload_length) {
            throw std::runtime_error("Incomplete frame");
        }
        
        std::vector<uint8_t> payload(
            data.begin() + header_size,
            data.begin() + header_size + header.payload_length
        );
        
        std::string message = Frame::decodePayload(header, payload);
        
        switch (header.opcode) {
            case Opcode::Text:
                // Используем публичный метод setMessageCallback для доступа к коллбэку
                connection->setMessageCallback([this, connection](const std::string& msg) {
                    if (connection) {
                        // Обработка сообщения
                    }
                });
                break;
                
            case Opcode::Binary:
                // Обработка бинарных данных
                break;
                
            case Opcode::Close:
                connection->close();
                break;
                
            case Opcode::Ping:
                connection->sendPong(message);  // Прямой вызов публичного метода
                break;
                
            case Opcode::Pong:
                // Игнорируем Pong
                break;
                
            default:
                throw std::runtime_error("Unsupported opcode");
        }
    }
    catch (const std::exception& e) {
        connection->setErrorCallback([this](const std::string& error) {
            // Обработка ошибки
        });
        connection->close(1002, "Protocol error");
    }
}

}