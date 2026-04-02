add_executable(rrc_compute_module_tests
    rrc_compute_tests.cpp
)

set_target_properties(rrc_compute_module_tests PROPERTIES
    FOLDER "Tests/RRCCompute"
)

target_include_directories(rrc_compute_module_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${MODULE_INCLUDE_DIRS}
        ${DATA_CONTAINERS_INCLUDE_DIRS}
)

target_link_libraries(rrc_compute_module_tests
    PRIVATE
        GTest::gtest_main
        RRCCompute-module
        DataContainers
        Module
        logger
)

gtest_discover_tests(rrc_compute_module_tests
    PROPERTIES
        LABELS "module;module/RRCCompute"
        TIMEOUT 120
)
