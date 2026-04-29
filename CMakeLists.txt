cmake_minimum_required(VERSION 3.20)
project(sdr_task_api VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)

find_package(nlohmann_json 3.11 QUIET)
if(NOT nlohmann_json_FOUND)
    include(FetchContent)
    FetchContent_Declare(nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG        v3.11.3)
    FetchContent_MakeAvailable(nlohmann_json)
endif()

find_package(spdlog QUIET)
if(NOT spdlog_FOUND)
    pkg_check_modules(SPDLOG REQUIRED IMPORTED_TARGET spdlog)
    add_library(spdlog::spdlog INTERFACE IMPORTED)
    target_include_directories(spdlog::spdlog INTERFACE ${SPDLOG_INCLUDE_DIRS})
    target_link_libraries(spdlog::spdlog INTERFACE ${SPDLOG_LIBRARIES})
    target_compile_definitions(spdlog::spdlog INTERFACE SPDLOG_FMT_EXTERNAL)
endif()

add_library(sdr_task_api STATIC src/MessageCodec.cpp)

target_include_directories(sdr_task_api PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(sdr_task_api PUBLIC
    nlohmann_json::nlohmann_json
    spdlog::spdlog
)

target_compile_features(sdr_task_api PUBLIC cxx_std_20)

target_compile_definitions(sdr_task_api PRIVATE
    SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG
    SPDLOG_FMT_EXTERNAL
)

# Tests are only built when this is the top-level project
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    enable_testing()
    add_subdirectory(tests)
endif()
