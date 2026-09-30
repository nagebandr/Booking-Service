#include "config.hpp"
#include <cstdlib>
#include <sstream>
#include <thread>

namespace booking {

namespace {
std::string get_env_or_default(const char* env_var, const std::string& default_val) {
    const char* val = std::getenv(env_var);
    return (val != nullptr && *val != '\0') ? std::string(val) : default_val;
}

int get_env_int_or_default(const char* env_var, int default_val) {
    const char* val = std::getenv(env_var);
    if (val != nullptr && *val != '\0') {
        try {
            return std::stoi(val);
        } catch (...) {
            return default_val;
        }
    }
    return default_val;
}
} // namespace

std::string Config::get_connection_string() const {
    std::ostringstream oss;
    oss << "host=" << db_host
        << " port=" << db_port
        << " dbname=" << db_name
        << " user=" << db_user
        << " password=" << db_password;
    return oss.str();
}

Config Config::load_from_env() {
    Config cfg;
    cfg.host = get_env_or_default("APP_HOST", "0.0.0.0");
    cfg.port = static_cast<uint16_t>(get_env_int_or_default("APP_PORT", 8080));

    unsigned int hw = std::thread::hardware_concurrency();
    int default_threads = hw > 0 ? static_cast<int>(hw) : 4;
    cfg.num_threads = get_env_int_or_default("APP_THREADS", default_threads);

    cfg.db_host = get_env_or_default("DB_HOST", "postgres");
    cfg.db_port = static_cast<uint16_t>(get_env_int_or_default("DB_PORT", 5432));
    cfg.db_name = get_env_or_default("DB_NAME", "booking_db");
    cfg.db_user = get_env_or_default("DB_USER", "postgres");
    cfg.db_password = get_env_or_default("DB_PASSWORD", "postgres");
    cfg.db_pool_size = static_cast<size_t>(get_env_int_or_default("DB_POOL_SIZE", 16));

    return cfg;
}

} // namespace booking
