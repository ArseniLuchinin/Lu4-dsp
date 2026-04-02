add_executable(carrier_phase_estimator_module_tests
    carrier_phase_estimator_tests.cpp
)

set_target_properties(carrier_phase_estimator_module_tests PROPERTIES
    FOLDER "Tests/CarrierPhaseEstimator"
)

target_include_directories(carrier_phase_estimator_module_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${MODULE_INCLUDE_DIRS}
        ${DATA_CONTAINERS_INCLUDE_DIRS}
)

target_link_libraries(carrier_phase_estimator_module_tests
    PRIVATE
        GTest::gtest_main
        CarrierPhaseEstimator-module
        DataContainers
        Module
        CUDA::cudart
        logger
)

gtest_discover_tests(carrier_phase_estimator_module_tests
    PROPERTIES
        LABELS "module;module/CarrierPhaseEstimator"
        TIMEOUT 120
)
