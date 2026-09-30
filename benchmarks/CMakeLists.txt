find_package(benchmark QUIET)

if (NOT benchmark_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        googlebenchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG v1.8.3
    )
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googlebenchmark)
endif()

add_executable(booking_benchmarks
    booking_benchmark.cpp
)

if (TARGET benchmark::benchmark)
    target_link_libraries(booking_benchmarks PRIVATE booking_core benchmark::benchmark)
elseif (TARGET benchmark)
    target_link_libraries(booking_benchmarks PRIVATE booking_core benchmark)
endif()
