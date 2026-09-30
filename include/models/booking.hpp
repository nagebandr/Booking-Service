#pragma once

#include <string>
#include <nlohmann/json.hpp>

namespace booking::models {

struct Booking {
    int id{0};
    int event_id{0};
    int seat_id{0};
    int user_id{0};
    std::string status; // "CONFIRMED", "CANCELLED"
    std::string created_at;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Booking, id, event_id, seat_id, user_id, status, created_at)
};

struct BookingRequest {
    int user_id{0};
    int seat_id{0};

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(BookingRequest, user_id, seat_id)
};

} // namespace booking::models
