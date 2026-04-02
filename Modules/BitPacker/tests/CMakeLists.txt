add_executable(bit_packer_module_tests
    bit_packer_tests.cpp
)

set_target_properties(bit_packer_module_tests PROPERTIES
    FOLDER "Tests/BitPacker"
)

target_include_directories(bit_packer_module_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${MODULE_INCLUDE_DIRS}
        ${DATA_CONTAINERS_INCLUDE_DIRS}
)

target_link_libraries(bit_packer_module_tests
    PRIVATE
        GTest::gtest_main
        BitPacker-module
        DataContainers
        Module
        CUDA::cudart
        logger
)

gtest_discover_tests(bit_packer_module_tests
    PROPERTIES
        LABELS "module;module/BitPacker"
        TIMEOUT 120
)
