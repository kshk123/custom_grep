cmake_minimum_required(VERSION 3.15)
project(customGrep LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

include_directories(${CMAKE_SOURCE_DIR}/inc)

add_library(CustomGrep
        src/CustomGrep.cpp
)
target_link_libraries(CustomGrep PRIVATE Threads::Threads)

add_executable(grep_exec
        src/main.cpp
)
target_link_libraries(grep_exec PRIVATE CustomGrep)

option(BUILD_TESTS "Build unit tests (Google Test)" OFF)

if(BUILD_TESTS)
    include(FetchContent)

    FetchContent_Declare(
            googletest
            URL https://github.com/google/googletest/archive/refs/tags/v1.13.0.zip
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    add_executable(test_custom_grep tests/TestCustomGrep.cpp)
    target_link_libraries(test_custom_grep PRIVATE CustomGrep GTest::gtest_main)
    target_include_directories(test_custom_grep PRIVATE ${CMAKE_SOURCE_DIR}/inc)

    enable_testing()
    add_test(NAME custom_grep_tests COMMAND test_custom_grep)
endif()
