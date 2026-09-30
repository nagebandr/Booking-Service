#include "config.hpp"
#include "db/connection_pool.hpp"
#include "db/booking_repository.hpp"
#include "http/router.hpp"
#include "http/http_server.hpp"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    try {
        std::cout << "========================================\n";
        std::cout << "  Event Seat Booking Service (C++20)   \n";
        std::cout << "========================================\n";

        auto config = booking::Config::load_from_env();

        std::cout << "[Main] DB Connection: " << config.db_host << ":" << config.db_port 
                  << " (Database: " << config.db_name << ")\n";
        std::cout << "[Main] Initializing DB Connection Pool (size=" << config.db_pool_size << ")...\n";

        // Wait for database availability (useful in Docker environments)
        std::unique_ptr<booking::db::ConnectionPool> pool;
        int max_attempts = 15;
        for (int attempt = 1; attempt <= max_attempts; ++attempt) {
            try {
                pool = std::make_unique<booking::db::ConnectionPool>(
                    config.get_connection_string(), config.db_pool_size);
                std::cout << "[Main] Successfully connected to PostgreSQL.\n";
                break;
            } catch (const std::exception& e) {
                if (attempt == max_attempts) {
                    std::cerr << "[Main] Failed to connect to DB after " << max_attempts << " attempts: " << e.what() << "\n";
                    return 1;
                }
                std::cout << "[Main] Waiting for DB... (attempt " << attempt << "/" << max_attempts << ")\n";
                std::this_thread::sleep_for(std::chrono::seconds(2));
            }
        }

        booking::db::BookingRepository repository(*pool);

        if (!repository.ping()) {
            std::cerr << "[Main] Database ping failed!\n";
            return 1;
        }

        std::cout << "[Main] Initializing HTTP Router and Server...\n";
        booking::http::Router router(repository);
        booking::http::HttpServer server(config.host, config.port, config.num_threads, router);

        std::cout << "[Main] Server running on " << config.host << ":" << config.port << "\n";
        server.run();

        std::cout << "[Main] Server exited cleanly.\n";
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "[Main] Fatal error: " << e.what() << "\n";
        return 1;
    }
}
