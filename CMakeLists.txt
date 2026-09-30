cmake_minimum_required(VERSION 3.20)
project(booking_service VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Compiler flags
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(-Wall -Wextra -Wpedantic -Wconversion -Wshadow -O2)
elseif (MSVC)
    add_compile_options(/W4 /permissive- /utf-8 /EHsc /O2)
endif()

# Threading
find_package(Threads REQUIRED)

# Boost (Asio, Beast)
find_package(Boost 1.74 REQUIRED)

# PostgreSQL and libpqxx
find_package(PostgreSQL REQUIRED)
find_package(libpqxx CONFIG QUIET)
if (NOT libpqxx_FOUND)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBPQXX REQUIRED libpqxx)
endif()

# nlohmann_json (system or FetchContent)
find_package(nlohmann_json 3.9 QUIET)
if (NOT nlohmann_json_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

# Core service library
add_library(booking_core STATIC
    src/config.cpp
    src/db/connection_pool.cpp
    src/db/booking_repository.cpp
    src/http/router.cpp
    src/http/http_session.cpp
    src/http/http_server.cpp
)

target_include_directories(booking_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${Boost_INCLUDE_DIRS}
    ${PostgreSQL_INCLUDE_DIRS}
)

if (TARGET libpqxx::pqxx)
    target_link_libraries(booking_core PUBLIC libpqxx::pqxx)
elseif (LIBPQXX_FOUND)
    target_include_directories(booking_core PUBLIC ${LIBPQXX_INCLUDE_DIRS})
    target_link_libraries(booking_core PUBLIC ${LIBPQXX_LIBRARIES})
else()
    target_link_libraries(booking_core PUBLIC pqxx pq)
endif()

target_link_libraries(booking_core PUBLIC
    Threads::Threads
    ${Boost_LIBRARIES}
    ${PostgreSQL_LIBRARIES}
    nlohmann_json::nlohmann_json
)

# Main executable
add_executable(booking_server src/main.cpp)
target_link_libraries(booking_server PRIVATE booking_core)

# Tests & Benchmarks options
option(BUILD_TESTS "Build test suite" ON)
option(BUILD_BENCHMARKS "Build benchmark suite" ON)

if (BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if (BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()
