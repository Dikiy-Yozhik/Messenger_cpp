#include "jwt.h"
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <sstream>
#include <iomanip>
#include <stdexcept>

namespace auth {

JWTService::JWTService(const std::string& secret, std::chrono::seconds token_ttl) 
    : secret_(secret), token_ttl_(token_ttl) {
    if (secret.empty()) {
        throw std::invalid_argument("JWT secret cannot be empty");
    }
}

std::string JWTService::generateToken(const std::string& user_id, const std::map<std::string, std::string>& payload) {
    // Header
    std::string header = R"({"alg":"HS256","typ":"JWT"})";
    std::string header_encoded = base64Encode(header);
    
    // Payload
    std::ostringstream payload_stream;
    payload_stream << R"({"sub":")" << user_id << R"(",)";
    payload_stream << R"("exp":)" << std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch() + token_ttl_).count();
    
    for (const auto& [key, value] : payload) {
        payload_stream << R"(,")" << key << R"(":")" << value << R"(")";
    }
    payload_stream << "}";
    
    std::string payload_encoded = base64Encode(payload_stream.str());
    
    // Signature
    std::string signature = sign(header_encoded, payload_encoded);
    
    return header_encoded + "." + payload_encoded + "." + signature;
}

std::optional<std::string> JWTService::validateToken(const std::string& token) {
    size_t dot1 = token.find('.');
    size_t dot2 = token.rfind('.');
    
    if (dot1 == std::string::npos || dot2 == std::string::npos || dot1 == dot2) {
        return std::nullopt;
    }
    
    std::string header_encoded = token.substr(0, dot1);
    std::string payload_encoded = token.substr(dot1 + 1, dot2 - dot1 - 1);
    std::string signature = token.substr(dot2 + 1);
    
    if (!verify(signature, header_encoded, payload_encoded)) {
        return std::nullopt;
    }
    
    try {
        std::string payload_str = base64Decode(payload_encoded);
        // Здесь нужно распарсить JSON и проверить exp (реализуйте самостоятельно)
        // Если токен просрочен, вернуть std::nullopt
        return payload_str; // В реальности нужно вернуть user_id
    } catch (...) {
        return std::nullopt;
    }
}

std::string JWTService::sign(const std::string& header, const std::string& payload) {
    std::string data = header + "." + payload;
    
    unsigned char digest[SHA256_DIGEST_LENGTH];
    HMAC_CTX* ctx = HMAC_CTX_new();
    HMAC_Init_ex(ctx, secret_.data(), secret_.size(), EVP_sha256(), nullptr);
    HMAC_Update(ctx, reinterpret_cast<const unsigned char*>(data.data()), data.size());
    unsigned int len;
    HMAC_Final(ctx, digest, &len);
    HMAC_CTX_free(ctx);
    
    std::string result;
    for (unsigned char i : digest) {
        result += static_cast<char>(i);
    }
    return base64Encode(result);
}

bool JWTService::verify(const std::string& signature, const std::string& header, const std::string& payload) {
    std::string expected_sign = sign(header, payload);
    return signature == expected_sign;
}

// Реализации base64Encode/base64Decode опущены для краткости
// (можно использовать готовые библиотеки или реализовать самостоятельно)

} // namespace auth
