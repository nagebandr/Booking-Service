#pragma once

#include <string>
#include <cstdint>

namespace booking {

struct Config {
    std::string host{"0.0.0.0"};
    uint16_t port{8080};
    int num_threads{4};

    std::string db_host{"postgres"};
    uint16_t db_port{5432};
    std::string db_name{"booking_db"};
    std::string db_user{"postgres"};
    std::string db_password{"postgres"};
    size_t db_pool_size{10};

    [[nodiscard]] std::string get_connection_string() const;
    static Config load_from_env();
};

} // namespace booking
