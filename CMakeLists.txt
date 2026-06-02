cmake_minimum_required(VERSION 3.20)
project(sdr_task_api VERSION 2.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── PkgConfig (needed for spdlog fallback path) ───────────────────────────────
find_package(PkgConfig REQUIRED)

# ── nlohmann/json ─────────────────────────────────────────────────────────────
# Try the system package first; fall back to FetchContent (no network required
# if the source is already in the CMake cache / _deps directory).
find_package(nlohmann_json 3.11 QUIET)
if(NOT nlohmann_json_FOUND)
    message(STATUS "nlohmann_json not found on system; fetching v3.11.3 from GitHub")
    include(FetchContent)
    FetchContent_Declare(nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG        v3.11.3)
    FetchContent_MakeAvailable(nlohmann_json)
endif()

# ── spdlog ────────────────────────────────────────────────────────────────────
# Priority: cmake package (installed from source) > pkg-config > FetchContent.
find_package(spdlog QUIET)
if(NOT spdlog_FOUND)
    pkg_check_modules(SPDLOG QUIET spdlog)
    if(SPDLOG_FOUND)
        message(STATUS "spdlog found via pkg-config ${SPDLOG_VERSION}")
        add_library(spdlog::spdlog INTERFACE IMPORTED)
        target_include_directories(spdlog::spdlog INTERFACE ${SPDLOG_INCLUDE_DIRS})
        target_link_libraries(spdlog::spdlog INTERFACE ${SPDLOG_LIBRARIES})
        target_compile_definitions(spdlog::spdlog INTERFACE SPDLOG_FMT_EXTERNAL)
    else()
        message(STATUS "spdlog not found via find_package or pkg-config; fetching from GitHub")
        include(FetchContent)
        FetchContent_Declare(spdlog
            GIT_REPOSITORY https://github.com/gabime/spdlog.git
            GIT_TAG        v1.14.1
            GIT_SHALLOW    TRUE
            EXCLUDE_FROM_ALL)
        FetchContent_MakeAvailable(spdlog)
        set(_SPDLOG_USE_EXTERNAL_FMT FALSE)
    endif()
else()
    message(STATUS "spdlog found via cmake: ${spdlog_VERSION}")
endif()

# ── Library ───────────────────────────────────────────────────────────────────
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

# SPDLOG_FMT_EXTERNAL: only when using system/pkg-config spdlog which requires
# external fmt. When spdlog is fetched it ships bundled fmt — adding
# SPDLOG_FMT_EXTERNAL there would require linking system libfmt explicitly.
set(_spdlog_ext_fmt_defs SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG)
if(NOT DEFINED _SPDLOG_USE_EXTERNAL_FMT OR _SPDLOG_USE_EXTERNAL_FMT)
    list(APPEND _spdlog_ext_fmt_defs SPDLOG_FMT_EXTERNAL)
endif()
target_compile_definitions(sdr_task_api PRIVATE ${_spdlog_ext_fmt_defs})

# ── Tests (only when this is the top-level project) ───────────────────────────
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    enable_testing()
    add_subdirectory(tests)
endif()

# ── Configure summary ─────────────────────────────────────────────────────────
message(STATUS "")
message(STATUS "┌─ SdrTaskApi ${PROJECT_VERSION} ─────────────────────────────┐")
message(STATUS "│  C++ standard : C++20                                │")
message(STATUS "│  Build type   : ${CMAKE_BUILD_TYPE}                         ")
message(STATUS "│  Dependencies :                                      │")
message(STATUS "│    nlohmann_json : required  (system or FetchContent)│")
message(STATUS "│    spdlog        : required  (system or FetchContent)│")
message(STATUS "│    googletest    : test-only (system or FetchContent)│")
message(STATUS "└──────────────────────────────────────────────────────┘")
message(STATUS "")
