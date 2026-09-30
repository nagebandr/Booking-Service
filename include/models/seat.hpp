#pragma once

#include <string>
#include <nlohmann/json.hpp>

namespace booking::models {

struct Seat {
    int id{0};
    int event_id{0};
    std::string seat_row;
    int seat_number{0};
    double price{0.0};
    bool is_booked{false};
    int version{0};
    std::string created_at;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Seat, id, event_id, seat_row, seat_number, price, is_booked, version, created_at)
};

} // namespace booking::models
