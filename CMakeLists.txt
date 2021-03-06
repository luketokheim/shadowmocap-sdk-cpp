# We need to build with C++20 or newer
# cxx_std_20: New in version 3.12
cmake_minimum_required(VERSION 3.12)

project(shadowmocap)

# shadowmocap is a C++20 library
add_library(shadowmocap src/datastream.cpp src/message.cpp)
target_compile_features(shadowmocap PUBLIC cxx_std_20)
target_include_directories(shadowmocap PUBLIC include/)

# g++10 or newer on Linux
if(CMAKE_COMPILER_IS_GNUCXX)
    find_package(Threads)
    target_compile_options(shadowmocap PUBLIC -fcoroutines)  
    target_link_libraries(shadowmocap PUBLIC ${CMAKE_THREAD_LIBS_INIT})
endif()

# Asio 1.22 or newer required
find_package(asio CONFIG REQUIRED)
target_link_libraries(shadowmocap PUBLIC asio::asio)

option(
    ENABLE_TEST_COVERAGE
    "Enable unit test code coverage with g++ and gcov"
    OFF)

if (ENABLE_TEST_COVERAGE)
    target_compile_options(shadowmocap PUBLIC --coverage)
    target_link_options(shadowmocap PUBLIC --coverage)

    find_program(GCOVR_PATH gcovr REQUIRED)
    add_custom_target(
        coverage
        COMMAND
        ${GCOVR_PATH} --sonarqube coverage.xml --root ${PROJECT_SOURCE_DIR})
endif()

option(
    ENABLE_TEST_BENCHMARK
    "Enable benchmarks as part of testing"
    OFF)

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    include(CTest)
endif()

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME AND BUILD_TESTING)
    add_subdirectory(test)
    if(ENABLE_TEST_BENCHMARK)
        add_subdirectory(bench)
    endif()
endif()

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME AND WIN32)
    add_subdirectory(example)
endif()
