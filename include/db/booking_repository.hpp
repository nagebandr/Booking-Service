#pragma once

#include "db/connection_pool.hpp"
#include "models/user.hpp"
#include "models/event.hpp"
#include "models/seat.hpp"
#include "models/booking.hpp"
#include <vector>
#include <optional>
#include <string>

namespace booking::db {

enum class BookingStatus {
    Success,
    SeatNotFound,
    EventNotFound,
    UserNotFound,
    SeatAlreadyBooked,
    ConflictRetry,
    InternalError
};

struct BookingResult {
    BookingStatus status;
    std::optional<models::Booking> booking;
    std::string message;
};

enum class CancelStatus {
    Success,
    NotFound,
    AlreadyCancelled,
    InternalError
};

struct CancelResult {
    CancelStatus status;
    std::string message;
};

class BookingRepository {
public:
    explicit BookingRepository(ConnectionPool& pool);

    std::vector<models::Event> get_events();
    std::optional<models::Event> get_event_by_id(int event_id);

    std::vector<models::Seat> get_seats_by_event(int event_id);
    std::optional<models::Seat> get_seat_by_id(int seat_id);

    // Atomically books a seat using row-level locking (SELECT ... FOR UPDATE)
    BookingResult book_seat(int event_id, int seat_id, int user_id);

    // Atomically cancels a booking and frees the seat
    CancelResult cancel_booking(int booking_id);

    // Helper for health checks and tests
    bool ping();
    void reset_seat(int seat_id);

private:
    ConnectionPool& pool_;
};

} // namespace booking::db
