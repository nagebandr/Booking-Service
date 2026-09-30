#include <gtest/gtest.h>
#include "models/user.hpp"
#include "models/event.hpp"
#include "models/seat.hpp"
#include "models/booking.hpp"
#include "config.hpp"
#include <nlohmann/json.hpp>

using namespace booking;

TEST(ModelTest, UserJsonSerialization) {
    models::User u{1, "Alice", "alice@test.com", "2026-09-30T20:00:00Z"};
    nlohmann::json j = u;
    EXPECT_EQ(j["id"], 1);
    EXPECT_EQ(j["name"], "Alice");
    EXPECT_EQ(j["email"], "alice@test.com");

    auto u2 = j.get<models::User>();
    EXPECT_EQ(u2.id, u.id);
    EXPECT_EQ(u2.name, u.name);
    EXPECT_EQ(u2.email, u.email);
}

TEST(ModelTest, EventJsonSerialization) {
    models::Event ev{
        42, "Rock Concert", "Live in Arena", "Arena Hall", "2026-10-15T19:00:00Z", 500, "2026-09-30T10:00:00Z"
    };
    nlohmann::json j = ev;
    EXPECT_EQ(j["id"], 42);
    EXPECT_EQ(j["title"], "Rock Concert");
    EXPECT_EQ(j["total_seats"], 500);

    auto ev2 = j.get<models::Event>();
    EXPECT_EQ(ev2.title, ev.title);
    EXPECT_EQ(ev2.total_seats, ev.total_seats);
}

TEST(ModelTest, SeatJsonSerialization) {
    models::Seat seat{
        10, 1, "A", 42, 120.50, false, 0, "2026-09-30T10:00:00Z"
    };
    nlohmann::json j = seat;
    EXPECT_EQ(j["seat_number"], 42);
    EXPECT_EQ(j["seat_row"], "A");
    EXPECT_DOUBLE_EQ(j["price"].get<double>(), 120.50);
    EXPECT_FALSE(j["is_booked"].get<bool>());
}

TEST(ModelTest, BookingRequestParsing) {
    std::string json_str = R"({"user_id": 2, "seat_id": 42})";
    auto parsed = nlohmann::json::parse(json_str);
    auto req = parsed.get<models::BookingRequest>();
    EXPECT_EQ(req.user_id, 2);
    EXPECT_EQ(req.seat_id, 42);
}

TEST(ConfigTest, ConnectionStringFormatting) {
    Config cfg;
    cfg.db_host = "test-host";
    cfg.db_port = 5432;
    cfg.db_name = "test_db";
    cfg.db_user = "test_user";
    cfg.db_password = "secret_password";

    std::string conn_str = cfg.get_connection_string();
    EXPECT_NE(conn_str.find("host=test-host"), std::string::npos);
    EXPECT_NE(conn_str.find("port=5432"), std::string::npos);
    EXPECT_NE(conn_str.find("dbname=test_db"), std::string::npos);
    EXPECT_NE(conn_str.find("user=test_user"), std::string::npos);
    EXPECT_NE(conn_str.find("password=secret_password"), std::string::npos);
}
