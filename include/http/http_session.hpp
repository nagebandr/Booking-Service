#pragma once

#include "http/router.hpp"
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <memory>

namespace booking::http {

namespace beast = boost::beast;
namespace beast_http = boost::beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

class HttpSession : public std::enable_shared_from_this<HttpSession> {
public:
    HttpSession(tcp::socket&& socket, Router& router);
    void run();

private:
    void do_read();
    void on_read(beast::error_code ec, std::size_t bytes_transferred);
    void send_response(beast_http::response<beast_http::string_body> response);
    void on_write(bool keep_alive, beast::error_code ec, std::size_t bytes_transferred);
    void do_close();

    beast::tcp_stream stream_;
    beast::flat_buffer buffer_;
    beast_http::request<beast_http::string_body> req_;
    Router& router_;
    std::shared_ptr<void> res_;
};

} // namespace booking::http
