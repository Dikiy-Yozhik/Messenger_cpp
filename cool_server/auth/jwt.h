#pragma once
#include <string>
#include <map>
#include <chrono>
#include <optional>

namespace auth {

class JWTService {
public:
    JWTService(const std::string& secret, std::chrono::seconds token_ttl = std::chrono::hours(24));
    
    // Генерация токена
    std::string generateToken(const std::string& user_id, const std::map<std::string, std::string>& payload = {});
    
    // Верификация токена
    std::optional<std::string> validateToken(const std::string& token);
    
    // Получение данных из токена
    std::optional<std::map<std::string, std::string>> getPayload(const std::string& token);

private:
    std::string secret_;
    std::chrono::seconds token_ttl_;
    
    std::string sign(const std::string& header, const std::string& payload);
    bool verify(const std::string& signature, const std::string& header, const std::string& payload);
    static std::string base64Encode(const std::string& data);
    static std::string base64Decode(const std::string& data);
};

} // namespace auth
