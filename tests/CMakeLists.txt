cmake_minimum_required(VERSION 3.10)

include(FetchContent)
include(GoogleTest)

find_package(GTest QUIET)

if(NOT GTest_FOUND)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    )
    FetchContent_MakeAvailable(googletest)
endif()

add_executable(rrc_compute_tests
    rrc_compute_tests.cpp
)

target_include_directories(rrc_compute_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../Modules/RRCCompute/include
        ${MODULE_INCLUDE_DIRS}
        ${DATA_CONTAINERS_INCLUDE_DIRS}
)

target_link_libraries(rrc_compute_tests
    PRIVATE
        GTest::gtest_main
        RRCCompute-module
        DataContainers
        Module
        logger
)

gtest_discover_tests(rrc_compute_tests)
