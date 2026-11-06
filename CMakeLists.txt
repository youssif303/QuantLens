cmake_minimum_required(VERSION 3.21)
project(QuantLens VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(QL_BUILD_APP "Build the Qt application" ON)
option(QL_BUILD_TESTS "Build unit tests" ON)

# Pure C++ core: indicators, forecast, backtest (no Qt dependency).
add_library(ql_core STATIC
    src/core/Indicators.cpp
    src/core/Csv.cpp
    src/core/Forecast.cpp
    src/core/Backtest.cpp)
target_include_directories(ql_core PUBLIC src)
if(MSVC)
    target_compile_options(ql_core PRIVATE /W4)
else()
    target_compile_options(ql_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(QL_BUILD_TESTS)
    enable_testing()
    include(FetchContent)
    FetchContent_Declare(googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    add_executable(core_tests tests/core_tests.cpp)
    target_link_libraries(core_tests PRIVATE ql_core GTest::gtest_main)
    include(GoogleTest)
    gtest_discover_tests(core_tests)
endif()

if(QL_BUILD_APP)
    add_subdirectory(src/app)
endif()
