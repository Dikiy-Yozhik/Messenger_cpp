#include "auth_service.h"
#include <openssl/rand.h>
#include <openssl/evp.h>
#include <stdexcept>

namespace auth {

AuthService::AuthService(JWTService& jwt_service, const std::string& pepper) 
    : jwt_service_(jwt_service), pepper_(pepper) {}

AuthResult AuthService::registerUser(const std::string& login, const std::string& password) {
    // 1. Проверка, что пользователь не существует (реализуйте через ваш UserRepository)
    
    // 2. Генерация соли и хеша
    std::string salt = generateSalt();
    std::string hashed_password = hashPassword(password, salt);
    
    // 3. Сохранение в БД (login, hashed_password, salt)
    
    // 4. Генерация токена
    std::string token = jwt_service_.generateToken(login);
    
    return {true, token, ""};
}

AuthResult AuthService::loginUser(const std::string& login, const std::string& password) {
    // 1. Получение пользователя из БД (реализуйте через UserRepository)
    // std::optional<User> user = user_repo_.getUserByLogin(login);
    
    // if (!user || user->password_hash != hashPassword(password, user->salt)) {
    //     return {false, "", "Invalid login or password"};
    // }
    
    // 2. Генерация токена
    std::string token = jwt_service_.generateToken(login);
    
    return {true, token, ""};
}

bool AuthService::validateToken(const std::string& token) {
    return jwt_service_.validateToken(token).has_value();
}

std::optional<std::string> AuthService::getUserIdFromToken(const std::string& token) {
    return jwt_service_.validateToken(token);
}

std::string AuthService::hashPassword(const std::string& password, const std::string& salt) {
    std::string data = password + salt + pepper_;
    
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int length = 0;
    
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
    EVP_DigestUpdate(ctx, data.data(), data.size());
    EVP_DigestFinal_ex(ctx, hash, &length);
    EVP_MD_CTX_free(ctx);
    
    return std::string(reinterpret_cast<char*>(hash), length);
}

std::string AuthService::generateSalt() {
    unsigned char salt[16];
    if (RAND_bytes(salt, sizeof(salt)) != 1) {
        throw std::runtime_error("Failed to generate salt");
    }
    return std::string(reinterpret_cast<char*>(salt), sizeof(salt));
}

} // namespace auth
