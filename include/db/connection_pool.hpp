#pragma once

#include <pqxx/pqxx>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <string>
#include <chrono>

namespace booking::db {

class ConnectionPool;

class PooledConnection {
public:
    PooledConnection(std::unique_ptr<pqxx::connection> conn, ConnectionPool& pool);
    ~PooledConnection();

    PooledConnection(const PooledConnection&) = delete;
    PooledConnection& operator=(const PooledConnection&) = delete;
    PooledConnection(PooledConnection&& other) noexcept;
    PooledConnection& operator=(PooledConnection&& other) noexcept;

    pqxx::connection& get();
    pqxx::connection* operator->();
    [[nodiscard]] bool is_valid() const;

private:
    std::unique_ptr<pqxx::connection> conn_;
    ConnectionPool* pool_{nullptr};
};

class ConnectionPool {
public:
    ConnectionPool(std::string conn_str, size_t pool_size);
    ~ConnectionPool();

    ConnectionPool(const ConnectionPool&) = delete;
    ConnectionPool& operator=(const ConnectionPool&) = delete;

    PooledConnection acquire(std::chrono::milliseconds timeout = std::chrono::milliseconds(5000));
    void release(std::unique_ptr<pqxx::connection> conn);
    [[nodiscard]] size_t available() const;
    [[nodiscard]] size_t capacity() const;

private:
    std::unique_ptr<pqxx::connection> create_connection();

    std::string conn_str_;
    size_t capacity_{0};
    std::queue<std::unique_ptr<pqxx::connection>> pool_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    bool stopped_{false};
};

} // namespace booking::db
