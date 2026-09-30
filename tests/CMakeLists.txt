find_package(GTest QUIET)

if (NOT GTest_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

add_executable(booking_tests
    unit_tests.cpp
    concurrency_tests.cpp
)

if (TARGET GTest::gtest_main)
    target_link_libraries(booking_tests PRIVATE booking_core GTest::gtest_main)
elseif (TARGET gtest_main)
    target_link_libraries(booking_tests PRIVATE booking_core gtest_main)
else()
    target_link_libraries(booking_tests PRIVATE booking_core GTest::GTest GTest::Main)
endif()

include(GoogleTest)
gtest_discover_tests(booking_tests)
