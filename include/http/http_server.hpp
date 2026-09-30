#pragma once

#include "http/router.hpp"
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/strand.hpp>
#include <memory>
#include <string>
#include <vector>
#include <thread>

namespace booking::http {

namespace net = boost::asio;
using tcp = net::ip::tcp;

class HttpServer {
public:
    HttpServer(const std::string& address, uint16_t port, int num_threads, Router& router);
    ~HttpServer();

    void run();
    void stop();

private:
    void do_accept();
    void on_accept(boost::beast::error_code ec, tcp::socket socket);

    net::io_context ioc_;
    tcp::acceptor acceptor_;
    net::signal_set signals_;
    int num_threads_;
    Router& router_;
    std::vector<std::thread> thread_pool_;
};

} // namespace booking::http
