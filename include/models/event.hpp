#pragma once

#include <string>
#include <nlohmann/json.hpp>

namespace booking::models {

struct Event {
    int id{0};
    std::string title;
    std::string description;
    std::string venue;
    std::string start_time;
    int total_seats{0};
    std::string created_at;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Event, id, title, description, venue, start_time, total_seats, created_at)
};

} // namespace booking::models
