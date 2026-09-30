#pragma once

#include <string>
#include <nlohmann/json.hpp>

namespace booking::models {

struct User {
    int id{0};
    std::string name;
    std::string email;
    std::string created_at;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(User, id, name, email, created_at)
};

} // namespace booking::models
