add_executable(decimator_module_tests
    decimator_tests.cpp
)

set_target_properties(decimator_module_tests PROPERTIES
    FOLDER "Tests/Decimator"
)

target_include_directories(decimator_module_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${MODULE_INCLUDE_DIRS}
        ${DATA_CONTAINERS_INCLUDE_DIRS}
)

target_link_libraries(decimator_module_tests
    PRIVATE
        GTest::gtest_main
        Decimator-module
        DataContainers
        Module
        CUDA::cudart
        logger
)

gtest_discover_tests(decimator_module_tests
    PROPERTIES
        LABELS "module;module/Decimator"
        TIMEOUT 120
)
