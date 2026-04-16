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
        ${CMAKE_CURRENT_SOURCE_DIR}/../Modules/CarrierRecovery/include
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
        CarrierRecovery-module
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

# ============================================================================
# Data Container Copy Tests
# ============================================================================

add_executable(data_container_copy_tests
    data_container_copy_tests.cpp
)

set_target_properties(data_container_copy_tests PROPERTIES
    FOLDER "Tests/Unit"
)

target_include_directories(data_container_copy_tests
    PRIVATE
        ${MODULE_INCLUDE_DIRS}
        ${DATA_CONTAINERS_INCLUDE_DIRS}
)

target_link_libraries(data_container_copy_tests
    PRIVATE
        GTest::gtest_main
        DataContainers
        CUDA::cudart
)

gtest_discover_tests(data_container_copy_tests
    PROPERTIES
        LABELS "module"
        TIMEOUT 30
)

# ============================================================================
# Virtual TX/RX Broadcaster Tests
# ============================================================================

add_executable(virtual_tx_rx_tests
    virtual_tx_rx_tests.cpp
)

set_target_properties(virtual_tx_rx_tests PROPERTIES
    FOLDER "Tests/Unit"
)

target_include_directories(virtual_tx_rx_tests
    PRIVATE
        ${MODULE_INCLUDE_DIRS}
        ${DATA_CONTAINERS_INCLUDE_DIRS}
)

target_link_libraries(virtual_tx_rx_tests
    PRIVATE
        GTest::gtest_main
        DataContainers
        Module
        CUDA::cudart
)

gtest_discover_tests(virtual_tx_rx_tests
    PROPERTIES
        LABELS "module"
        TIMEOUT 60
)

# ============================================================================
# fetchParam resolution tests
# ============================================================================

add_executable(fetch_param_tests
    fetch_param_tests.cpp
)

set_target_properties(fetch_param_tests PROPERTIES
    FOLDER "Tests/Unit"
)

target_include_directories(fetch_param_tests
    PRIVATE
        ${MODULE_INCLUDE_DIRS}
        ${DATA_CONTAINERS_INCLUDE_DIRS}
)

target_link_libraries(fetch_param_tests
    PRIVATE
        GTest::gtest_main
        DataContainers
        Module
        CUDA::cudart
)

gtest_discover_tests(fetch_param_tests
    PROPERTIES
        LABELS "module"
        TIMEOUT 30
)
