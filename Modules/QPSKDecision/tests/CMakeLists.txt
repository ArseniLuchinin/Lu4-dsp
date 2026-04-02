add_executable(qpsk_decision_module_tests
    qpsk_decision_tests.cpp
)

set_target_properties(qpsk_decision_module_tests PROPERTIES
    FOLDER "Tests/QPSKDecision"
)

target_include_directories(qpsk_decision_module_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${MODULE_INCLUDE_DIRS}
        ${DATA_CONTAINERS_INCLUDE_DIRS}
)

target_link_libraries(qpsk_decision_module_tests
    PRIVATE
        GTest::gtest_main
        QPSKDecision-module
        DataContainers
        Module
        CUDA::cudart
        logger
)

gtest_discover_tests(qpsk_decision_module_tests
    PROPERTIES
        LABELS "module;module/QPSKDecision"
        TIMEOUT 120
)
