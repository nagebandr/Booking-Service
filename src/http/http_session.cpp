#include "http/http_session.hpp"
#include <iostream>

namespace booking::http {

HttpSession::HttpSession(tcp::socket&& socket, Router& router)
    : stream_(std::move(socket)), router_(router) {}

void HttpSession::run() {
    net::dispatch(
        stream_.get_executor(),
        beast::bind_front_handler(&HttpSession::do_read, shared_from_this()));
}

void HttpSession::do_read() {
    req_ = {};
    stream_.expires_after(std::chrono::seconds(30));

    beast_http::async_read(
        stream_,
        buffer_,
        req_,
        beast::bind_front_handler(&HttpSession::on_read, shared_from_this()));
}

void HttpSession::on_read(beast::error_code ec, std::size_t bytes_transferred) {
    boost::ignore_unused(bytes_transferred);

    if (ec == beast_http::error::end_of_stream) {
        return do_close();
    }

    if (ec) {
        // Suppress benign connection drops/timeouts in logs
        if (ec != net::error::operation_aborted && ec != beast::error::timeout) {
            std::cerr << "[HttpSession] read error: " << ec.message() << "\n";
        }
        return;
    }

    auto res = router_.handle_request(req_);
    send_response(std::move(res));
}

void HttpSession::send_response(beast_http::response<beast_http::string_body> response) {
    auto sp = std::make_shared<beast_http::response<beast_http::string_body>>(std::move(response));
    res_ = sp;

    beast_http::async_write(
        stream_,
        *sp,
        beast::bind_front_handler(
            &HttpSession::on_write,
            shared_from_this(),
            sp->need_eof()));
}

void HttpSession::on_write(bool keep_alive, beast::error_code ec, std::size_t bytes_transferred) {
    boost::ignore_unused(bytes_transferred);

    if (ec) {
        if (ec != net::error::operation_aborted) {
            std::cerr << "[HttpSession] write error: " << ec.message() << "\n";
        }
        return;
    }

    if (keep_alive) {
        // EOF needed; close connection
        return do_close();
    }

    res_.reset();
    do_read();
}

void HttpSession::do_close() {
    beast::error_code ec;
    stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
}

} // namespace booking::http
