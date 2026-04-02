add_executable(qpsk_mvp_tests
    qpsk_mvp_tests.cpp
)

set_target_properties(qpsk_mvp_tests PROPERTIES
    FOLDER "Tests/E2E"
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
        DataContainers
        Module
        CUDA::cudart
        logger
)

gtest_discover_tests(qpsk_mvp_tests
    PROPERTIES
        LABELS "e2e"
        TIMEOUT 120
)
