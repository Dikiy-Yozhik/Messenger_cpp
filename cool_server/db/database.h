#pragma once
#include <sqlite3.h>
#include <string>
#include <vector>
#include <stdexcept>
#include <memory>
#include <optional>

namespace db {

class DatabaseException : public std::runtime_error {
public:
    DatabaseException(const std::string& msg) : std::runtime_error(msg) {}
};

class Statement; // Предварительное объявление

class Database {
public:
    Database(const std::string& path);
    ~Database();

    // Запрет копирования
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    // Выполнение SQL без возврата результата
    void execute(const std::string& sql);
    
    // Создание подготовленного выражения
    std::unique_ptr<Statement> prepare(const std::string& sql);

    // Быстрые методы для запросов
    template<typename... Args>
    std::optional<int> executeWithId(const std::string& sql, Args... args);

    template<typename... Args>
    std::vector<std::vector<std::string>> query(const std::string& sql, Args... args);

    // Транзакции
    void beginTransaction();
    void commit();
    void rollback();

    // Вспомогательные методы
    int lastInsertId() const;
    void enableForeignKeys(bool enable = true);

private:
    sqlite3* db_;
    friend class Statement;
};

// Подготовленное выражение
class Statement {
public:
    Statement(Database& db, const std::string& sql);
    ~Statement();

    // Привязка параметров
    void bind(int index, int value);
    void bind(int index, double value);
    void bind(int index, const std::string& value);
    void bind(int index, const std::vector<uint8_t>& value);
    void bindNull(int index);

    // Выполнение
    bool execute();
    bool fetchRow();

    // Получение данных
    int getInt(int column) const;
    double getDouble(int column) const;
    std::string getString(int column) const;
    std::vector<uint8_t> getBlob(int column) const;
    bool isNull(int column) const;

    // Сброс состояния
    void reset();

private:
    sqlite3_stmt* stmt_;
    Database& db_;
};

} // namespace db
