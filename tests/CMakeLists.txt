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

add_executable(qpsk_mvp_tests
    qpsk_mvp_tests.cpp
)

target_include_directories(qpsk_mvp_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../Modules/FileSrc/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../Modules/FIR-filter/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../Modules/RRCCompute/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../Modules/Decimator/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../Modules/CarrierPhaseEstimator/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../Modules/PhaseRotator/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../Modules/QPSKDecision/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../Modules/BitPacker/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../Modules/TextFileWriter/include
        ${MODULE_INCLUDE_DIRS}
        ${DATA_CONTAINERS_INCLUDE_DIRS}
)

target_compile_definitions(qpsk_mvp_tests
    PRIVATE
        PROJECT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(qpsk_mvp_tests
    PRIVATE
        GTest::gtest_main
        FileSrc-module
        FIR-filter-module
        RRCCompute-module
        Decimator-module
        CarrierPhaseEstimator-module
        PhaseRotator-module
        QPSKDecision-module
        BitPacker-module
        TextFileWriter-module
        DataContainers
        Module
        CUDA::cudart
        logger
)

gtest_discover_tests(qpsk_mvp_tests)
