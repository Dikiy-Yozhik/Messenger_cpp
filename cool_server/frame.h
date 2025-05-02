#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include <stdexcept>

namespace websocket {

enum class Opcode : uint8_t {
    Continuation = 0x0,
    Text         = 0x1,
    Binary       = 0x2,
    Close        = 0x8,
    Ping         = 0x9,
    Pong         = 0xA
};

struct FrameHeader {
    bool fin;
    Opcode opcode;
    bool masked;
    uint64_t payload_length;
    uint32_t masking_key;
};

class Frame {
public:
    static constexpr uint64_t MAX_FRAME_SIZE = 16 * 1024 * 1024; // 16MB
    static constexpr size_t MAX_HEADER_SIZE = 14;

    static bool parseHeader(const std::vector<uint8_t>& data, FrameHeader& header);
    static size_t headerSize(const FrameHeader& header);
    static std::vector<uint8_t> createFrame(Opcode opcode, const std::string& payload, bool masked = false);
    static std::string decodePayload(const FrameHeader& header, const std::vector<uint8_t>& payload);
    
private:
    static void applyMask(uint32_t masking_key, uint8_t* data, size_t len);
    static void validateOpcode(Opcode opcode);
};

} // namespace websocket
