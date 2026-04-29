cmake_minimum_required(VERSION 3.20)

find_package(GTest QUIET)
if(NOT GTest_FOUND)
    include(FetchContent)
    FetchContent_Declare(googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.14.0)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

add_executable(sdr_api_tests
    unit/test_types.cpp
    unit/test_message_codec.cpp
)

target_link_libraries(sdr_api_tests PRIVATE
    GTest::gtest_main
    sdr_task_api
)

target_compile_definitions(sdr_api_tests PRIVATE
    SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_WARN
    SPDLOG_FMT_EXTERNAL
)

include(CTest)
include(GoogleTest)
gtest_discover_tests(sdr_api_tests)
