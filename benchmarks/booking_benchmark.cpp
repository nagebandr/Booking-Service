#include <benchmark/benchmark.h>
#include "models/seat.hpp"
#include "models/event.hpp"
#include "models/booking.hpp"
#include "config.hpp"
#include "db/connection_pool.hpp"
#include <nlohmann/json.hpp>

// Benchmark JSON serialization of seat lists
static void BM_SeatJsonSerialization(benchmark::State& state) {
    std::vector<booking::models::Seat> seats;
    seats.reserve(100);
    for (int i = 0; i < 100; ++i) {
        seats.push_back({
            i + 1, 1, "Row-" + std::to_string(i / 10), (i % 10) + 1, 99.99, (i % 3 == 0), 1, "2026-09-30T10:00:00Z"
        });
    }

    for (auto _ : state) {
        nlohmann::json j = seats;
        std::string s = j.dump();
        benchmark::DoNotOptimize(s);
    }
}
BENCHMARK(BM_SeatJsonSerialization);

// Benchmark JSON parsing of booking requests
static void BM_BookingRequestParsing(benchmark::State& state) {
    std::string payload = R"({"user_id": 42, "seat_id": 108})";

    for (auto _ : state) {
        auto j = nlohmann::json::parse(payload);
        auto req = j.get<booking::models::BookingRequest>();
        benchmark::DoNotOptimize(req);
    }
}
BENCHMARK(BM_BookingRequestParsing);

BENCHMARK_MAIN();
