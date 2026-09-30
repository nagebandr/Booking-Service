#include <gtest/gtest.h>
#include "config.hpp"
#include "db/connection_pool.hpp"
#include "db/booking_repository.hpp"
#include <thread>
#include <vector>
#include <atomic>
#include <barrier>
#include <iostream>

using namespace booking;

class ConcurrencyTest : public ::testing::Test {
protected:
    std::unique_ptr<db::ConnectionPool> pool_;
    std::unique_ptr<db::BookingRepository> repo_;
    int test_event_id_{-1};
    int test_seat_id_{-1};
    int user_a_id_{-1};
    int user_b_id_{-1};

    void SetUp() override {
        Config cfg = Config::load_from_env();
        cfg.db_pool_size = 25; // Enough pool size for concurrent threads

        try {
            pool_ = std::make_unique<db::ConnectionPool>(cfg.get_connection_string(), cfg.db_pool_size);
            repo_ = std::make_unique<db::BookingRepository>(*pool_);

            auto conn = pool_->acquire();
            pqxx::work txn(conn.get());

            // Prepare clean test environment
            txn.exec(
                "INSERT INTO users (name, email) VALUES "
                "('User A', 'user_a@concurrency.test'), "
                "('User B', 'user_b@concurrency.test') "
                "ON CONFLICT (email) DO NOTHING;"
            );

            auto res_user_a = txn.exec_params("SELECT id FROM users WHERE email = $1;", "user_a@concurrency.test");
            user_a_id_ = res_user_a[0][0].as<int>();

            auto res_user_b = txn.exec_params("SELECT id FROM users WHERE email = $1;", "user_b@concurrency.test");
            user_b_id_ = res_user_b[0][0].as<int>();

            auto res_ev = txn.exec(
                "INSERT INTO events (title, venue, start_time, total_seats) "
                "VALUES ('Concurrency Concert', 'Lock Arena', NOW() + INTERVAL '10 days', 100) "
                "RETURNING id;"
            );
            test_event_id_ = res_ev[0][0].as<int>();

            // Create Seat #42
            auto res_seat = txn.exec_params(
                "INSERT INTO seats (event_id, seat_row, seat_number, price, is_booked) "
                "VALUES ($1, 'VIP', 42, 150.00, FALSE) "
                "RETURNING id;",
                test_event_id_
            );
            test_seat_id_ = res_seat[0][0].as<int>();

            txn.commit();
        } catch (const std::exception& e) {
            FAIL() << "DB setup failed: " << e.what();
        }
    }

    void TearDown() override {
        if (test_event_id_ > 0 && pool_) {
            try {
                auto conn = pool_->acquire();
                pqxx::work txn(conn.get());
                txn.exec_params("DELETE FROM events WHERE id = $1;", test_event_id_);
                txn.commit();
            } catch (...) {
            }
        }
    }
};

// Core test: User A and User B concurrently trying to book seat #42
TEST_F(ConcurrencyTest, TwoUsersSimultaneousBookingSeat42) {
    std::barrier sync_point(2);
    db::BookingResult res_a;
    db::BookingResult res_b;

    std::thread t1([&] {
        sync_point.arrive_and_wait(); // Wait for thread 2 so they fire at exact same time
        res_a = repo_->book_seat(test_event_id_, test_seat_id_, user_a_id_);
    });

    std::thread t2([&] {
        sync_point.arrive_and_wait(); // Wait for thread 1
        res_b = repo_->book_seat(test_event_id_, test_seat_id_, user_b_id_);
    });

    t1.join();
    t2.join();

    int success_count = 0;
    int conflict_count = 0;

    if (res_a.status == db::BookingStatus::Success) success_count++;
    else if (res_a.status == db::BookingStatus::SeatAlreadyBooked || res_a.status == db::BookingStatus::ConflictRetry) conflict_count++;

    if (res_b.status == db::BookingStatus::Success) success_count++;
    else if (res_b.status == db::BookingStatus::SeatAlreadyBooked || res_b.status == db::BookingStatus::ConflictRetry) conflict_count++;

    EXPECT_EQ(success_count, 1) << "Exactly one user must successfully book the seat";
    EXPECT_EQ(conflict_count, 1) << "The other user must receive a conflict/already booked error";

    // Verify database state
    auto seat = repo_->get_seat_by_id(test_seat_id_);
    ASSERT_TRUE(seat.has_value());
    EXPECT_TRUE(seat->is_booked);
}

// Stress test: 20 concurrent threads trying to book the exact same seat #42
TEST_F(ConcurrencyTest, HighContentionTwentyConcurrentBookings) {
    const int num_threads = 20;
    std::barrier sync_point(num_threads);

    std::atomic<int> success_count{0};
    std::atomic<int> conflict_count{0};
    std::atomic<int> other_error_count{0};

    std::vector<std::thread> threads;
    threads.reserve(num_threads);

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i] {
            // Alternate between user A and user B
            int user_id = (i % 2 == 0) ? user_a_id_ : user_b_id_;

            sync_point.arrive_and_wait(); // Release all 20 threads simultaneously

            auto result = repo_->book_seat(test_event_id_, test_seat_id_, user_id);

            if (result.status == db::BookingStatus::Success) {
                success_count.fetch_add(1);
            } else if (result.status == db::BookingStatus::SeatAlreadyBooked || 
                       result.status == db::BookingStatus::ConflictRetry) {
                conflict_count.fetch_add(1);
            } else {
                other_error_count.fetch_add(1);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count.load(), 1) << "Strictly 1 transaction must succeed!";
    EXPECT_EQ(conflict_count.load(), num_threads - 1) << "All other 19 transactions must fail with conflict";
    EXPECT_EQ(other_error_count.load(), 0);

    // Verify DB integrity
    auto conn = pool_->acquire();
    pqxx::nontransaction ntx(conn.get());
    pqxx::result r = ntx.exec_params(
        "SELECT COUNT(*) FROM bookings WHERE seat_id = $1 AND status = 'CONFIRMED';",
        test_seat_id_
    );
    EXPECT_EQ(r[0][0].as<int>(), 1);
}

// Test: Cancellation frees seat for next booking
TEST_F(ConcurrencyTest, CancelAndRebook) {
    auto book_res = repo_->book_seat(test_event_id_, test_seat_id_, user_a_id_);
    ASSERT_EQ(book_res.status, db::BookingStatus::Success);
    ASSERT_TRUE(book_res.booking.has_value());

    // Try booking again - should fail
    auto fail_res = repo_->book_seat(test_event_id_, test_seat_id_, user_b_id_);
    EXPECT_EQ(fail_res.status, db::BookingStatus::SeatAlreadyBooked);

    // Cancel booking
    auto cancel_res = repo_->cancel_booking(book_res.booking->id);
    EXPECT_EQ(cancel_res.status, db::CancelStatus::Success);

    // Now User B can book it
    auto rebook_res = repo_->book_seat(test_event_id_, test_seat_id_, user_b_id_);
    EXPECT_EQ(rebook_res.status, db::BookingStatus::Success);
}
