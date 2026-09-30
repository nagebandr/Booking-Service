#include "http/http_server.hpp"
#include "http/http_session.hpp"
#include <iostream>

namespace booking::http {

HttpServer::HttpServer(const std::string& address, uint16_t port, int num_threads, Router& router)
    : ioc_(num_threads),
      acceptor_(net::make_strand(ioc_)),
      signals_(ioc_, SIGINT, SIGTERM),
      num_threads_(num_threads),
      router_(router) {

    boost::beast::error_code ec;
    auto endpoint = tcp::endpoint(net::ip::make_address(address, ec), port);
    if (ec) {
        throw std::runtime_error("Invalid IP address: " + address);
    }

    acceptor_.open(endpoint.protocol(), ec);
    if (ec) {
        throw std::runtime_error("Acceptor open failed: " + ec.message());
    }

    acceptor_.set_option(net::socket_base::reuse_address(true), ec);
    if (ec) {
        throw std::runtime_error("Set reuse_address failed: " + ec.message());
    }

    acceptor_.bind(endpoint, ec);
    if (ec) {
        throw std::runtime_error("Acceptor bind failed on " + address + ":" + std::to_string(port) + ": " + ec.message());
    }

    acceptor_.listen(net::socket_base::max_listen_connections, ec);
    if (ec) {
        throw std::runtime_error("Acceptor listen failed: " + ec.message());
    }

    // Register OS signal handler for graceful shutdown
    signals_.async_wait([this](boost::beast::error_code /*ec*/, int /*signo*/) {
        std::cout << "\n[HttpServer] Shutdown signal received. Stopping server...\n";
        stop();
    });
}

HttpServer::~HttpServer() {
    stop();
}

void HttpServer::run() {
    std::cout << "[HttpServer] Listening on "
              << acceptor_.local_endpoint().address().to_string() << ":"
              << acceptor_.local_endpoint().port() << " with "
              << num_threads_ << " worker thread(s)...\n";

    do_accept();

    thread_pool_.reserve(num_threads_);
    for (int i = 0; i < num_threads_; ++i) {
        thread_pool_.emplace_back([this] {
            ioc_.run();
        });
    }

    for (auto& t : thread_pool_) {
        if (t.joinable()) {
            t.join();
        }
    }
}

void HttpServer::stop() {
    ioc_.stop();
}

void HttpServer::do_accept() {
    acceptor_.async_accept(
        net::make_strand(ioc_),
        beast::bind_front_handler(&HttpServer::on_accept, this));
}

void HttpServer::on_accept(boost::beast::error_code ec, tcp::socket socket) {
    if (!ec) {
        std::make_shared<HttpSession>(std::move(socket), router_)->run();
    } else {
        if (ec != net::error::operation_aborted) {
            std::cerr << "[HttpServer] Accept error: " << ec.message() << "\n";
        }
    }

    if (acceptor_.is_open()) {
        do_accept();
    }
}

} // namespace booking::http
