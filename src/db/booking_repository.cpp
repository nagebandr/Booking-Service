#include "db/booking_repository.hpp"
#include <iostream>

namespace booking::db {

BookingRepository::BookingRepository(ConnectionPool& pool)
    : pool_(pool) {}

bool BookingRepository::ping() {
    try {
        auto conn = pool_.acquire();
        pqxx::nontransaction ntx(conn.get());
        pqxx::result r = ntx.exec("SELECT 1");
        return !r.empty();
    } catch (...) {
        return false;
    }
}

std::vector<models::Event> BookingRepository::get_events() {
    auto conn = pool_.acquire();
    pqxx::nontransaction ntx(conn.get());
    pqxx::result r = ntx.exec(
        "SELECT id, title, COALESCE(description, '') AS description, venue, "
        "TO_CHAR(start_time, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS start_time, "
        "total_seats, "
        "TO_CHAR(created_at, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS created_at "
        "FROM events ORDER BY id ASC;"
    );

    std::vector<models::Event> events;
    events.reserve(r.size());
    for (const auto& row : r) {
        models::Event ev;
        ev.id = row["id"].as<int>();
        ev.title = row["title"].as<std::string>();
        ev.description = row["description"].as<std::string>();
        ev.venue = row["venue"].as<std::string>();
        ev.start_time = row["start_time"].as<std::string>();
        ev.total_seats = row["total_seats"].as<int>();
        ev.created_at = row["created_at"].as<std::string>();
        events.push_back(std::move(ev));
    }
    return events;
}

std::optional<models::Event> BookingRepository::get_event_by_id(int event_id) {
    auto conn = pool_.acquire();
    pqxx::nontransaction ntx(conn.get());
    pqxx::result r = ntx.exec_params(
        "SELECT id, title, COALESCE(description, '') AS description, venue, "
        "TO_CHAR(start_time, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS start_time, "
        "total_seats, "
        "TO_CHAR(created_at, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS created_at "
        "FROM events WHERE id = $1;",
        event_id
    );

    if (r.empty()) {
        return std::nullopt;
    }

    const auto& row = r[0];
    models::Event ev;
    ev.id = row["id"].as<int>();
    ev.title = row["title"].as<std::string>();
    ev.description = row["description"].as<std::string>();
    ev.venue = row["venue"].as<std::string>();
    ev.start_time = row["start_time"].as<std::string>();
    ev.total_seats = row["total_seats"].as<int>();
    ev.created_at = row["created_at"].as<std::string>();
    return ev;
}

std::vector<models::Seat> BookingRepository::get_seats_by_event(int event_id) {
    auto conn = pool_.acquire();
    pqxx::nontransaction ntx(conn.get());
    pqxx::result r = ntx.exec_params(
        "SELECT id, event_id, seat_row, seat_number, price::FLOAT8, is_booked, version, "
        "TO_CHAR(created_at, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS created_at "
        "FROM seats WHERE event_id = $1 ORDER BY seat_row, seat_number;",
        event_id
    );

    std::vector<models::Seat> seats;
    seats.reserve(r.size());
    for (const auto& row : r) {
        models::Seat s;
        s.id = row["id"].as<int>();
        s.event_id = row["event_id"].as<int>();
        s.seat_row = row["seat_row"].as<std::string>();
        s.seat_number = row["seat_number"].as<int>();
        s.price = row["price"].as<double>();
        s.is_booked = row["is_booked"].as<bool>();
        s.version = row["version"].as<int>();
        s.created_at = row["created_at"].as<std::string>();
        seats.push_back(std::move(s));
    }
    return seats;
}

std::optional<models::Seat> BookingRepository::get_seat_by_id(int seat_id) {
    auto conn = pool_.acquire();
    pqxx::nontransaction ntx(conn.get());
    pqxx::result r = ntx.exec_params(
        "SELECT id, event_id, seat_row, seat_number, price::FLOAT8, is_booked, version, "
        "TO_CHAR(created_at, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS created_at "
        "FROM seats WHERE id = $1;",
        seat_id
    );

    if (r.empty()) {
        return std::nullopt;
    }

    const auto& row = r[0];
    models::Seat s;
    s.id = row["id"].as<int>();
    s.event_id = row["event_id"].as<int>();
    s.seat_row = row["seat_row"].as<std::string>();
    s.seat_number = row["seat_number"].as<int>();
    s.price = row["price"].as<double>();
    s.is_booked = row["is_booked"].as<bool>();
    s.version = row["version"].as<int>();
    s.created_at = row["created_at"].as<std::string>();
    return s;
}

BookingResult BookingRepository::book_seat(int event_id, int seat_id, int user_id) {
    try {
        auto conn = pool_.acquire();
        // Start explicit PostgreSQL transaction
        pqxx::work txn(conn.get());

        // 1. Verify User exists
        auto user_res = txn.exec_params("SELECT id FROM users WHERE id = $1;", user_id);
        if (user_res.empty()) {
            return {BookingStatus::UserNotFound, std::nullopt, "User not found"};
        }

        // 2. Critical Row-Level Lock:
        // SELECT ... FOR UPDATE acquires an exclusive tuple lock on this specific seat row.
        // If another transaction is currently booking this seat, it will block here until
        // the other transaction commits or aborts.
        pqxx::result seat_res = txn.exec_params(
            "SELECT id, event_id, is_booked FROM seats "
            "WHERE id = $1 AND event_id = $2 "
            "FOR UPDATE;",
            seat_id, event_id
        );

        if (seat_res.empty()) {
            return {BookingStatus::SeatNotFound, std::nullopt, "Seat not found for the given event"};
        }

        bool is_booked = seat_res[0]["is_booked"].as<bool>();
        if (is_booked) {
            // Transaction aborts on scope exit without commit
            return {BookingStatus::SeatAlreadyBooked, std::nullopt, "Seat is already booked"};
        }

        // 3. Mark seat as booked and bump version
        txn.exec_params(
            "UPDATE seats SET is_booked = TRUE, version = version + 1 WHERE id = $1;",
            seat_id
        );

        // 4. Create the booking entry
        pqxx::result book_res = txn.exec_params(
            "INSERT INTO bookings (event_id, seat_id, user_id, status) "
            "VALUES ($1, $2, $3, 'CONFIRMED') "
            "RETURNING id, TO_CHAR(created_at, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS created_at;",
            event_id, seat_id, user_id
        );

        int booking_id = book_res[0]["id"].as<int>();
        std::string created_at = book_res[0]["created_at"].as<std::string>();

        // 5. Commit atomic transaction
        txn.commit();

        models::Booking booking{
            .id = booking_id,
            .event_id = event_id,
            .seat_id = seat_id,
            .user_id = user_id,
            .status = "CONFIRMED",
            .created_at = created_at
        };

        return {BookingStatus::Success, booking, "Seat successfully booked"};

    } catch (const pqxx::unique_violation& e) {
        // Double safety net: triggered if unique partial index idx_unique_confirmed_seat_booking fires
        return {BookingStatus::SeatAlreadyBooked, std::nullopt, "Seat already booked (unique constraint)"};
    } catch (const pqxx::serialization_failure& e) {
        return {BookingStatus::ConflictRetry, std::nullopt, "Concurrent transaction conflict (serialization failure)"};
    } catch (const std::exception& e) {
        std::cerr << "[BookingRepository] book_seat exception: " << e.what() << "\n";
        return {BookingStatus::InternalError, std::nullopt, std::string("Database error: ") + e.what()};
    }
}

CancelResult BookingRepository::cancel_booking(int booking_id) {
    try {
        auto conn = pool_.acquire();
        pqxx::work txn(conn.get());

        // Lock booking row
        pqxx::result r = txn.exec_params(
            "SELECT id, seat_id, status FROM bookings WHERE id = $1 FOR UPDATE;",
            booking_id
        );

        if (r.empty()) {
            return {CancelStatus::NotFound, "Booking not found"};
        }

        std::string current_status = r[0]["status"].as<std::string>();
        if (current_status != "CONFIRMED") {
            return {CancelStatus::AlreadyCancelled, "Booking is already cancelled"};
        }

        int seat_id = r[0]["seat_id"].as<int>();

        // Free the seat
        txn.exec_params(
            "UPDATE seats SET is_booked = FALSE, version = version + 1 WHERE id = $1;",
            seat_id
        );

        // Mark booking cancelled
        txn.exec_params(
            "UPDATE bookings SET status = 'CANCELLED' WHERE id = $1;",
            booking_id
        );

        txn.commit();
        return {CancelStatus::Success, "Booking cancelled successfully"};

    } catch (const std::exception& e) {
        std::cerr << "[BookingRepository] cancel_booking exception: " << e.what() << "\n";
        return {CancelStatus::InternalError, std::string("Database error: ") + e.what()};
    }
}

void BookingRepository::reset_seat(int seat_id) {
    try {
        auto conn = pool_.acquire();
        pqxx::work txn(conn.get());
        txn.exec_params("UPDATE seats SET is_booked = FALSE WHERE id = $1;", seat_id);
        txn.exec_params("DELETE FROM bookings WHERE seat_id = $1;", seat_id);
        txn.commit();
    } catch (...) {
    }
}

} // namespace booking::db
