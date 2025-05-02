#include "websocket_connection.h"
#include <stdexcept>
#include <iostream>

namespace websocket {

WebSocketConnection::WebSocketConnection(SOCKET socket, IOCPCore& iocp)
    : socket_(socket), iocp_(iocp), is_closed_(false) {
    ZeroMemory(&read_operation_.overlapped, sizeof(OVERLAPPED));
    iocp_.associateSocket(socket_, reinterpret_cast<ULONG_PTR>(this));
    asyncRead();
}

WebSocketConnection::~WebSocketConnection() {
    close(1006, "Connection destroyed");
}

void WebSocketConnection::asyncRead() {
    if (is_closed_) return;

    std::lock_guard<std::mutex> lock(socket_mutex_);
    if (socket_ == INVALID_SOCKET) return;

    // Выделяем буфер с запасом (мин. 8KB, макс. 16MB)
    const size_t buffer_size = std::min(
        std::max(8192ULL, expected_payload_size_ + Frame::MAX_HEADER_SIZE),
        Frame::MAX_FRAME_SIZE
    );
    
    read_operation_.buffer.resize(buffer_size);
    WSABUF buf = { 
        .len = static_cast<ULONG>(read_operation_.buffer.size()),
        .buf = reinterpret_cast<CHAR*>(read_operation_.buffer.data())
    };

    DWORD flags = 0;
    if (WSARecv(socket_, &buf, 1, nullptr, &flags, &read_operation_.overlapped, nullptr) == SOCKET_ERROR) {
        if (WSAGetLastError() != WSA_IO_PENDING) {
            close(1006, "Read error");
        }
    }
}

void WebSocketConnection::handleIOCompletion(DWORD bytes, LPOVERLAPPED overlapped) {
    if (is_closed_) return;

    // Умный указатель для автоматического удаления AsyncOperation
    std::unique_ptr<AsyncOperation> op;

    // Если это не операция чтения, это операция записи (нужно освободить память)
    if (overlapped != &read_operation_.overlapped) {
        op.reset(reinterpret_cast<AsyncOperation*>(overlapped));
    }

    if (bytes == 0) {
        close(1005, "Connection closed");
        return;
    }

    try {
        if (overlapped == &read_operation_.overlapped) {
            read_operation_.buffer.resize(bytes);
            processData(read_operation_.buffer);
            asyncRead();
        }
        // Операция записи завершена - память освободится через unique_ptr
    } catch (const std::exception& e) {
        std::cerr << "WebSocket error: " << e.what() << "\n";
        close(1002, "Protocol error");
    }
}

void WebSocketConnection::processData(const std::vector<uint8_t>& data) {
    // Добавляем новые данные в буфер
    read_buffer_.insert(read_buffer_.end(), data.begin(), data.end());

    while (true) {
        // 1. Парсим заголовок
        FrameHeader header;
        if (!Frame::parseHeader(read_buffer_, header)) {
            return; // Ждём больше данных
        }

        // 2. Проверяем, что весь фрейм получен
        const size_t frame_size = Frame::headerSize(header) + header.payload_length;
        if (read_buffer_.size() < frame_size) {
            expected_payload_size_ = header.payload_length;
            return; // Не хватает данных
        }

        // 3. Извлекаем payload
        auto payload_begin = read_buffer_.begin() + Frame::headerSize(header);
        auto payload_end = payload_begin + header.payload_length;
        std::vector<uint8_t> payload(payload_begin, payload_end);

        // 4. Удаляем обработанные данные из буфера
        read_buffer_.erase(read_buffer_.begin(), read_buffer_.begin() + frame_size);

        // 5. Обрабатываем фрейм
        handleFrame(header, std::move(payload));
    }
}

void WebSocketConnection::handleFrame(const FrameHeader& header, std::vector<uint8_t>&& payload) {
    try {
        std::string message = Frame::decodePayload(header, payload);

        std::lock_guard<std::mutex> lock(callbacks_mutex_);
        switch (header.opcode) {
            case Opcode::Text:
                if (on_message_) on_message_(message);
                break;
            case Opcode::Binary:
                if (on_message_) on_message_(message); // Или отдельный binary_cb_
                break;
            case Opcode::Ping:
                sendPong(message);
                break;
            case Opcode::Close:
                close(1000, "Normal closure");
                break;
            default:
                break;
        }
    } catch (const std::exception& e) {
        close(1002, "Protocol error");
    }
}

void WebSocketConnection::sendText(const std::string& message) {
    std::vector<uint8_t> frame = Frame::createFrame(Opcode::Text, message);
    asyncWrite(std::move(frame));
}

void WebSocketConnection::sendBinary(const std::vector<uint8_t>& data) {
    // Преобразуем vector<uint8_t> в string
    std::string payload(data.begin(), data.end());
    auto frame = Frame::createFrame(Opcode::Binary, payload);
    asyncWrite(std::move(frame));
}

void WebSocketConnection::sendPong(const std::string& message) {
    std::vector<uint8_t> frame = Frame::createFrame(Opcode::Pong, message);
    asyncWrite(std::move(frame));
}

void WebSocketConnection::close(uint16_t code, const std::string& reason) {
    if (is_closed_.exchange(true)) return;

    std::lock_guard<std::mutex> lock(socket_mutex_);
    if (socket_ == INVALID_SOCKET) return;

    // Отправка фрейма закрытия (если нужно)
    std::vector<uint8_t> frame;
    if (code != 1000 || !reason.empty()) {
        std::string payload;
        payload.push_back(static_cast<char>(code >> 8));
        payload.push_back(static_cast<char>(code & 0xFF));
        payload += reason;
        frame = Frame::createFrame(Opcode::Close, payload);
    } else {
        frame = Frame::createFrame(Opcode::Close, "");
    }

    // Отправка фрейма закрытия (неблокирующая)
    asyncWrite(std::move(frame));

    // Закрытие сокета
    ::closesocket(socket_);
    socket_ = INVALID_SOCKET;

    // Вызов коллбэка
    std::lock_guard<std::mutex> cb_lock(callbacks_mutex_);
    if (on_close_) on_close_();
}

void WebSocketConnection::asyncWrite(std::vector<uint8_t>&& data) {
    if (is_closed_) return;

    std::lock_guard<std::mutex> lock(socket_mutex_);
    if (socket_ == INVALID_SOCKET) return;

    auto* op = new AsyncOperation();
    ZeroMemory(&op->overlapped, sizeof(OVERLAPPED));
    op->buffer = std::move(data);

    // Указываем ключ WRITE для IOCP
    ULONG_PTR write_key = static_cast<ULONG_PTR>(IOCPCore::CompletionKeyType::WRITE);
    iocp_.associateSocket(socket_, write_key);  // Привязываем сокет к ключу

    WSABUF buf = {
        .len = static_cast<ULONG>(op->buffer.size()),
        .buf = reinterpret_cast<CHAR*>(op->buffer.data())
    };

    if (WSASend(socket_, &buf, 1, nullptr, 0, &op->overlapped, nullptr) == SOCKET_ERROR) {
        if (WSAGetLastError() != WSA_IO_PENDING) {
            delete op;
            close(1006, "Write error");
        }
    }
}

void WebSocketConnection::setMessageCallback(MessageCallback cb) {
    std::lock_guard<std::mutex> lock(callbacks_mutex_);
    on_message_ = cb;
}

void WebSocketConnection::setCloseCallback(CloseCallback cb) {
    std::lock_guard<std::mutex> lock(callbacks_mutex_);
    on_close_ = cb;
}

} // namespace websocket