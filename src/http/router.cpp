#include "http/router.hpp"
#include "http/json_utils.hpp"
#include <regex>
#include <iostream>

namespace booking::http {

Router::Router(db::BookingRepository& repository)
    : repo_(repository) {}

beast_http::response<beast_http::string_body> Router::handle_request(
    const beast_http::request<beast_http::string_body>& req) {

    std::string_view target_view = req.target();
    // Strip query params if any
    auto query_pos = target_view.find('?');
    std::string path = std::string(target_view.substr(0, query_pos));

    // Handle OPTIONS for CORS pre-flight
    if (req.method() == beast_http::verb::options) {
        beast_http::response<beast_http::string_body> res{beast_http::status::no_content, req.version()};
        res.set(beast_http::field::server, "BookingService/1.0");
        res.set(beast_http::field::access_control_allow_origin, "*");
        res.set(beast_http::field::access_control_allow_methods, "GET, POST, DELETE, OPTIONS");
        res.set(beast_http::field::access_control_allow_headers, "Content-Type");
        res.keep_alive(req.keep_alive());
        return res;
    }

    try {
        if (req.method() == beast_http::verb::get && path == "/health") {
            return handle_get_health(req);
        }

        if (req.method() == beast_http::verb::get && (path == "/events" || path == "/events/")) {
            return handle_get_events(req);
        }

        // Regex patterns for parameterized endpoints
        static const std::regex event_seats_regex(R"(^/events/(\d+)/seats/?$)");
        static const std::regex event_bookings_regex(R"(^/events/(\d+)/bookings/?$)");
        static const std::regex event_detail_regex(R"(^/events/(\d+)/?$)");
        static const std::regex booking_delete_regex(R"(^/bookings/(\d+)/?$)");

        std::smatch match;

        if (req.method() == beast_http::verb::get && std::regex_match(path, match, event_seats_regex)) {
            int event_id = std::stoi(match[1].str());
            return handle_get_seats(event_id, req);
        }

        if (req.method() == beast_http::verb::post && std::regex_match(path, match, event_bookings_regex)) {
            int event_id = std::stoi(match[1].str());
            return handle_post_booking(event_id, req);
        }

        if (req.method() == beast_http::verb::get && std::regex_match(path, match, event_detail_regex)) {
            int event_id = std::stoi(match[1].str());
            return handle_get_event(event_id, req);
        }

        if (req.method() == beast_http::verb::delete_ && std::regex_match(path, match, booking_delete_regex)) {
            int booking_id = std::stoi(match[1].str());
            return handle_delete_booking(booking_id, req);
        }

        return make_error_response(beast_http::status::not_found, "Route not found", req.version(), req.keep_alive());

    } catch (const std::exception& e) {
        std::cerr << "[Router] Exception processing request: " << e.what() << "\n";
        return make_error_response(beast_http::status::internal_server_error, e.what(), req.version(), req.keep_alive());
    }
}

beast_http::response<beast_http::string_body> Router::handle_get_health(
    const beast_http::request<beast_http::string_body>& req) {
    bool db_ok = repo_.ping();
    nlohmann::json response_body = {
        {"status", db_ok ? "ok" : "degraded"},
        {"database", db_ok ? "connected" : "disconnected"}
    };
    auto status = db_ok ? beast_http::status::ok : beast_http::status::service_unavailable;
    return make_json_response(status, response_body, req.version(), req.keep_alive());
}

beast_http::response<beast_http::string_body> Router::handle_get_events(
    const beast_http::request<beast_http::string_body>& req) {
    auto events = repo_.get_events();
    nlohmann::json json_events = events;
    return make_json_response(beast_http::status::ok, json_events, req.version(), req.keep_alive());
}

beast_http::response<beast_http::string_body> Router::handle_get_event(
    int event_id, const beast_http::request<beast_http::string_body>& req) {
    auto event = repo_.get_event_by_id(event_id);
    if (!event.has_value()) {
        return make_error_response(beast_http::status::not_found, "Event not found", req.version(), req.keep_alive());
    }
    nlohmann::json json_event = *event;
    return make_json_response(beast_http::status::ok, json_event, req.version(), req.keep_alive());
}

beast_http::response<beast_http::string_body> Router::handle_get_seats(
    int event_id, const beast_http::request<beast_http::string_body>& req) {
    auto event = repo_.get_event_by_id(event_id);
    if (!event.has_value()) {
        return make_error_response(beast_http::status::not_found, "Event not found", req.version(), req.keep_alive());
    }
    auto seats = repo_.get_seats_by_event(event_id);
    nlohmann::json json_seats = seats;
    return make_json_response(beast_http::status::ok, json_seats, req.version(), req.keep_alive());
}

beast_http::response<beast_http::string_body> Router::handle_post_booking(
    int event_id, const beast_http::request<beast_http::string_body>& req) {

    nlohmann::json body;
    try {
        body = nlohmann::json::parse(req.body());
    } catch (const nlohmann::json::parse_error& e) {
        return make_error_response(beast_http::status::bad_request, "Invalid JSON body", req.version(), req.keep_alive());
    }

    if (!body.contains("user_id") || !body.contains("seat_id")) {
        return make_error_response(beast_http::status::bad_request, "Missing user_id or seat_id in body", req.version(), req.keep_alive());
    }

    int user_id = body["user_id"].get<int>();
    int seat_id = body["seat_id"].get<int>();

    auto result = repo_.book_seat(event_id, seat_id, user_id);

    switch (result.status) {
        case db::BookingStatus::Success: {
            nlohmann::json res_body = {
                {"message", result.message},
                {"booking", *result.booking}
            };
            return make_json_response(beast_http::status::created, res_body, req.version(), req.keep_alive());
        }
        case db::BookingStatus::SeatAlreadyBooked:
            return make_error_response(beast_http::status::conflict, result.message, req.version(), req.keep_alive());

        case db::BookingStatus::ConflictRetry:
            return make_error_response(beast_http::status::conflict, result.message, req.version(), req.keep_alive());

        case db::BookingStatus::SeatNotFound:
        case db::BookingStatus::EventNotFound:
        case db::BookingStatus::UserNotFound:
            return make_error_response(beast_http::status::not_found, result.message, req.version(), req.keep_alive());

        case db::BookingStatus::InternalError:
        default:
            return make_error_response(beast_http::status::internal_server_error, result.message, req.version(), req.keep_alive());
    }
}

beast_http::response<beast_http::string_body> Router::handle_delete_booking(
    int booking_id, const beast_http::request<beast_http::string_body>& req) {

    auto result = repo_.cancel_booking(booking_id);

    switch (result.status) {
        case db::CancelStatus::Success: {
            nlohmann::json res_body = {
                {"message", result.message},
                {"booking_id", booking_id}
            };
            return make_json_response(beast_http::status::ok, res_body, req.version(), req.keep_alive());
        }
        case db::CancelStatus::NotFound:
            return make_error_response(beast_http::status::not_found, result.message, req.version(), req.keep_alive());

        case db::CancelStatus::AlreadyCancelled:
            return make_error_response(beast_http::status::conflict, result.message, req.version(), req.keep_alive());

        case db::CancelStatus::InternalError:
        default:
            return make_error_response(beast_http::status::internal_server_error, result.message, req.version(), req.keep_alive());
    }
}

} // namespace booking::http
