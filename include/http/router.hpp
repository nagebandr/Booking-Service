#pragma once

#include "db/booking_repository.hpp"
#include <boost/beast/http.hpp>

namespace booking::http {

namespace beast_http = boost::beast::http;

class Router {
public:
    explicit Router(db::BookingRepository& repository);

    beast_http::response<beast_http::string_body> handle_request(
        const beast_http::request<beast_http::string_body>& req);

private:
    beast_http::response<beast_http::string_body> handle_get_health(
        const beast_http::request<beast_http::string_body>& req);

    beast_http::response<beast_http::string_body> handle_get_events(
        const beast_http::request<beast_http::string_body>& req);

    beast_http::response<beast_http::string_body> handle_get_event(
        int event_id, const beast_http::request<beast_http::string_body>& req);

    beast_http::response<beast_http::string_body> handle_get_seats(
        int event_id, const beast_http::request<beast_http::string_body>& req);

    beast_http::response<beast_http::string_body> handle_post_booking(
        int event_id, const beast_http::request<beast_http::string_body>& req);

    beast_http::response<beast_http::string_body> handle_delete_booking(
        int booking_id, const beast_http::request<beast_http::string_body>& req);

    db::BookingRepository& repo_;
};

} // namespace booking::http
