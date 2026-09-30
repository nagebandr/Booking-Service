#include "db/connection_pool.hpp"
#include <iostream>
#include <stdexcept>

namespace booking::db {

// PooledConnection implementation
PooledConnection::PooledConnection(std::unique_ptr<pqxx::connection> conn, ConnectionPool& pool)
    : conn_(std::move(conn)), pool_(&pool) {}

PooledConnection::~PooledConnection() {
    if (conn_ && pool_) {
        pool_->release(std::move(conn_));
    }
}

PooledConnection::PooledConnection(PooledConnection&& other) noexcept
    : conn_(std::move(other.conn_)), pool_(other.pool_) {
    other.pool_ = nullptr;
}

PooledConnection& PooledConnection::operator=(PooledConnection&& other) noexcept {
    if (this != &other) {
        if (conn_ && pool_) {
            pool_->release(std::move(conn_));
        }
        conn_ = std::move(other.conn_);
        pool_ = other.pool_;
        other.pool_ = nullptr;
    }
    return *this;
}

pqxx::connection& PooledConnection::get() {
    if (!conn_) {
        throw std::runtime_error("Attempted to access null database connection");
    }
    return *conn_;
}

pqxx::connection* PooledConnection::operator->() {
    return &get();
}

bool PooledConnection::is_valid() const {
    return conn_ && conn_->is_open();
}

// ConnectionPool implementation
ConnectionPool::ConnectionPool(std::string conn_str, size_t pool_size)
    : conn_str_(std::move(conn_str)), capacity_(pool_size) {
    for (size_t i = 0; i < capacity_; ++i) {
        pool_.push(create_connection());
    }
}

ConnectionPool::~ConnectionPool() {
    std::unique_lock<std::mutex> lock(mutex_);
    stopped_ = true;
    while (!pool_.empty()) {
        auto conn = std::move(pool_.front());
        pool_.pop();
        if (conn && conn->is_open()) {
            try {
                conn->close();
            } catch (...) {
            }
        }
    }
    cv_.notify_all();
}

std::unique_ptr<pqxx::connection> ConnectionPool::create_connection() {
    auto conn = std::make_unique<pqxx::connection>(conn_str_);
    if (!conn->is_open()) {
        throw std::runtime_error("Failed to open connection to PostgreSQL database");
    }
    return conn;
}

PooledConnection ConnectionPool::acquire(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    bool success = cv_.wait_for(lock, timeout, [this] {
        return stopped_ || !pool_.empty();
    });

    if (stopped_) {
        throw std::runtime_error("Connection pool is stopped");
    }

    if (!success || pool_.empty()) {
        throw std::runtime_error("Connection pool acquisition timeout");
    }

    auto conn = std::move(pool_.front());
    pool_.pop();

    // Verify connection health, reconnect if dropped
    if (!conn || !conn->is_open()) {
        try {
            conn = create_connection();
        } catch (const std::exception& e) {
            std::cerr << "[ConnectionPool] Reconnection failed: " << e.what() << "\n";
            throw;
        }
    }

    return PooledConnection(std::move(conn), *this);
}

void ConnectionPool::release(std::unique_ptr<pqxx::connection> conn) {
    if (!conn) return;

    std::unique_lock<std::mutex> lock(mutex_);
    if (stopped_) {
        try {
            conn->close();
        } catch (...) {
        }
        return;
    }

    pool_.push(std::move(conn));
    lock.unlock();
    cv_.notify_one();
}

size_t ConnectionPool::available() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return pool_.size();
}

size_t ConnectionPool::capacity() const {
    return capacity_;
}

} // namespace booking::db
