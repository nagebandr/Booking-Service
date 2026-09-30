#pragma once

#include <boost/beast/http.hpp>
#include <nlohmann/json.hpp>
#include <string>

namespace booking::http {

namespace beast_http = boost::beast::http;

inline beast_http::response<beast_http::string_body> make_json_response(
    beast_http::status status,
    const nlohmann::json& json_body,
    unsigned int version,
    bool keep_alive) {
    beast_http::response<beast_http::string_body> res{status, version};
    res.set(beast_http::field::server, "BookingService/1.0 (C++20/Beast)");
    res.set(beast_http::field::content_type, "application/json");
    res.keep_alive(keep_alive);
    res.body() = json_body.dump(2);
    res.prepare_payload();
    return res;
}

inline beast_http::response<beast_http::string_body> make_error_response(
    beast_http::status status,
    const std::string& error_message,
    unsigned int version,
    bool keep_alive) {
    nlohmann::json body = {
        {"error", error_message},
        {"code", static_cast<int>(status)}
    };
    return make_json_response(status, body, version, keep_alive);
}

} // namespace booking::http
