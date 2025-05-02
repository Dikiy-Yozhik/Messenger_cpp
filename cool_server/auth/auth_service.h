#pragma once
#include "jwt.h"
#include <string>
#include <optional>

namespace auth {

struct AuthResult {
    bool success;
    std::string token;
    std::string error_message;
};

class AuthService {
public:
    AuthService(JWTService& jwt_service, const std::string& pepper = "");
    
    AuthResult registerUser(const std::string& login, const std::string& password);
    AuthResult loginUser(const std::string& login, const std::string& password);
    bool validateToken(const std::string& token);
    std::optional<std::string> getUserIdFromToken(const std::string& token);

private:
    JWTService& jwt_service_;
    std::string pepper_;
    
    std::string hashPassword(const std::string& password, const std::string& salt);
    std::string generateSalt();
};

} // namespace auth
