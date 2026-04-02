add_executable(phase_rotator_module_tests
    phase_rotator_tests.cpp
)

set_target_properties(phase_rotator_module_tests PROPERTIES
    FOLDER "Tests/PhaseRotator"
)

target_include_directories(phase_rotator_module_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${MODULE_INCLUDE_DIRS}
        ${DATA_CONTAINERS_INCLUDE_DIRS}
)

target_link_libraries(phase_rotator_module_tests
    PRIVATE
        GTest::gtest_main
        PhaseRotator-module
        DataContainers
        Module
        CUDA::cudart
        logger
)

gtest_discover_tests(phase_rotator_module_tests
    PROPERTIES
        LABELS "module;module/PhaseRotator"
        TIMEOUT 20
)
