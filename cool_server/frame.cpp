#include "frame.h"
#include <array>
#include <random>

namespace websocket {

bool Frame::parseHeader(const std::vector<uint8_t>& data, FrameHeader& header) {
    if (data.size() < 2) {
        return false;
    }

    // Базовый заголовок (2 байта)
    header.fin = (data[0] & 0x80) != 0;
    header.opcode = static_cast<Opcode>(data[0] & 0x0F);
    header.masked = (data[1] & 0x80) != 0;

    uint8_t len_byte = data[1] & 0x7F;
    size_t header_size = 2;

    // Длина payload
    if (len_byte == 126) {
        if (data.size() < 4) return false;
        header.payload_length = (data[2] << 8) | data[3];
        header_size += 2;
    } 
    else if (len_byte == 127) {
        if (data.size() < 10) return false;
        header.payload_length = 
            (static_cast<uint64_t>(data[2]) << 56) |
            (static_cast<uint64_t>(data[3]) << 48) |
            (static_cast<uint64_t>(data[4]) << 40) |
            (static_cast<uint64_t>(data[5]) << 32) |
            (static_cast<uint64_t>(data[6]) << 24) |
            (static_cast<uint64_t>(data[7]) << 16) |
            (static_cast<uint64_t>(data[8]) << 8)  |
            data[9];
        header_size += 8;
    } 
    else {
        header.payload_length = len_byte;
    }

    // Проверка максимального размера
    if (header.payload_length > MAX_FRAME_SIZE) {
        throw std::runtime_error("Frame size exceeds maximum limit");
    }

    // Маскировка
    if (header.masked) {
        if (data.size() < header_size + 4) return false;
        header.masking_key = 
            (data[header_size] << 24) |
            (data[header_size+1] << 16) |
            (data[header_size+2] << 8) |
            data[header_size+3];
        header_size += 4;
    }

    return true;
}

size_t Frame::headerSize(const FrameHeader& header) {
    size_t size = 2; // Базовый заголовок
    
    if (header.payload_length >= 126) {
        size += (header.payload_length > 65535) ? 8 : 2;
    }
    
    if (header.masked) {
        size += 4;
    }
    
    return size;
}

std::vector<uint8_t> Frame::createFrame(Opcode opcode, const std::string& payload, bool masked) {
    validateOpcode(opcode);
    
    std::vector<uint8_t> frame;
    frame.reserve(MAX_HEADER_SIZE + payload.size());
    
    // Byte 1: FIN + opcode
    frame.push_back(0x80 | static_cast<uint8_t>(opcode));
    
    // Byte 2: MASK + длина
    if (payload.size() <= 125) {
        frame.push_back((masked ? 0x80 : 0x00) | static_cast<uint8_t>(payload.size()));
    } 
    else if (payload.size() <= 65535) {
        frame.push_back((masked ? 0x80 : 0x00) | 126);
        frame.push_back((payload.size() >> 8) & 0xFF);
        frame.push_back(payload.size() & 0xFF);
    } 
    else {
        frame.push_back((masked ? 0x80 : 0x00) | 127);
        for (int i = 7; i >= 0; --i) {
            frame.push_back((payload.size() >> (8 * i)) & 0xFF);
        }
    }
    
    // Маскировка
    if (masked) {
        static thread_local std::mt19937 gen(std::random_device{}());
        std::uniform_int_distribution<uint32_t> dist;
        uint32_t mask = dist(gen);
        
        frame.push_back((mask >> 24) & 0xFF);
        frame.push_back((mask >> 16) & 0xFF);
        frame.push_back((mask >> 8) & 0xFF);
        frame.push_back(mask & 0xFF);
        
        // Применяем маску
        std::string masked_payload = payload;
        for (size_t i = 0; i < masked_payload.size(); ++i) {
            masked_payload[i] ^= ((mask >> (8 * (3 - (i % 4)))) & 0xFF);
        }
        frame.insert(frame.end(), masked_payload.begin(), masked_payload.end());
    } 
    else {
        frame.insert(frame.end(), payload.begin(), payload.end());
    }
    
    return frame;
}

std::string Frame::decodePayload(const FrameHeader& header, const std::vector<uint8_t>& payload) {
    if (payload.size() != header.payload_length) {
        throw std::runtime_error("Payload length mismatch");
    }

    std::vector<uint8_t> decoded(payload.begin(), payload.end());
    
    if (header.masked) {
        for (size_t i = 0; i < decoded.size(); ++i) {
            decoded[i] ^= ((header.masking_key >> (8 * (3 - (i % 4)))) & 0xFF);
        }
    }
    
    return std::string(decoded.begin(), decoded.end());
}

void Frame::applyMask(uint32_t masking_key, uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        data[i] ^= ((masking_key >> (8 * (3 - (i % 4)))) & 0xFF);
    }
}

void Frame::validateOpcode(Opcode opcode) {
    switch (opcode) {
        case Opcode::Continuation:
        case Opcode::Text:
        case Opcode::Binary:
        case Opcode::Close:
        case Opcode::Ping:
        case Opcode::Pong:
            break;
        default:
            throw std::invalid_argument("Invalid WebSocket opcode");
    }
}

} // namespace websocket
