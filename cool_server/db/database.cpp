#include "database.h"
#include <sqlite3.h>
#include <sstream>

namespace db {

Database::Database(const std::string& path) : db_(nullptr) {
    if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
        throw DatabaseException(sqlite3_errmsg(db_));
    }
}

Database::~Database() {
    if (db_) {
        sqlite3_close(db_);
    }
}

void Database::execute(const std::string& sql) {
    char* errMsg = nullptr;
    if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::string msg(errMsg);
        sqlite3_free(errMsg);
        throw DatabaseException(msg);
    }
}

std::unique_ptr<Statement> Database::prepare(const std::string& sql) {
    return std::make_unique<Statement>(*this, sql);
}

void Database::beginTransaction() {
    execute("BEGIN TRANSACTION");
}

void Database::commit() {
    execute("COMMIT");
}

void Database::rollback() {
    execute("ROLLBACK");
}

int Database::lastInsertId() const {
    return static_cast<int>(sqlite3_last_insert_rowid(db_));
}

void Database::enableForeignKeys(bool enable) {
    execute(enable ? "PRAGMA foreign_keys = ON" : "PRAGMA foreign_keys = OFF");
}

// Реализация Statement
Statement::Statement(Database& db, const std::string& sql) : db_(db), stmt_(nullptr) {
    if (sqlite3_prepare_v2(db_.db_, sql.c_str(), -1, &stmt_, nullptr) != SQLITE_OK) {
        throw DatabaseException(sqlite3_errmsg(db_.db_));
    }
}

Statement::~Statement() {
    if (stmt_) {
        sqlite3_finalize(stmt_);
    }
}

void Statement::bind(int index, int value) {
    sqlite3_bind_int(stmt_, index, value);
}

void Statement::bind(int index, double value) {
    sqlite3_bind_double(stmt_, index, value);
}

void Statement::bind(int index, const std::string& value) {
    sqlite3_bind_text(stmt_, index, value.c_str(), -1, SQLITE_TRANSIENT);
}

void Statement::bind(int index, const std::vector<uint8_t>& value) {
    sqlite3_bind_blob(stmt_, index, value.data(), static_cast<int>(value.size()), SQLITE_TRANSIENT);
}

void Statement::bindNull(int index) {
    sqlite3_bind_null(stmt_, index);
}

bool Statement::execute() {
    int rc = sqlite3_step(stmt_);
    if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
        throw DatabaseException(sqlite3_errmsg(db_.db_));
    }
    return rc == SQLITE_ROW;
}

bool Statement::fetchRow() {
    int rc = sqlite3_step(stmt_);
    if (rc != SQLITE_ROW && rc != SQLITE_DONE) {
        throw DatabaseException(sqlite3_errmsg(db_.db_));
    }
    return rc == SQLITE_ROW;
}

int Statement::getInt(int column) const {
    return sqlite3_column_int(stmt_, column);
}

double Statement::getDouble(int column) const {
    return sqlite3_column_double(stmt_, column);
}

std::string Statement::getString(int column) const {
    const unsigned char* text = sqlite3_column_text(stmt_, column);
    return text ? reinterpret_cast<const char*>(text) : "";
}

std::vector<uint8_t> Statement::getBlob(int column) const {
    const void* blob = sqlite3_column_blob(stmt_, column);
    int size = sqlite3_column_bytes(stmt_, column);
    return std::vector<uint8_t>(static_cast<const uint8_t*>(blob), 
                               static_cast<const uint8_t*>(blob) + size);
}

bool Statement::isNull(int column) const {
    return sqlite3_column_type(stmt_, column) == SQLITE_NULL;
}

void Statement::reset() {
    sqlite3_reset(stmt_);
    sqlite3_clear_bindings(stmt_);
}

} // namespace db
