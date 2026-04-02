add_executable(text_file_writer_module_tests
    text_file_writer_tests.cpp
)

set_target_properties(text_file_writer_module_tests PROPERTIES
    FOLDER "Tests/TextFileWriter"
)

target_include_directories(text_file_writer_module_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${MODULE_INCLUDE_DIRS}
        ${DATA_CONTAINERS_INCLUDE_DIRS}
)

target_link_libraries(text_file_writer_module_tests
    PRIVATE
        GTest::gtest_main
        TextFileWriter-module
        DataContainers
        Module
        CUDA::cudart
        logger
)

gtest_discover_tests(text_file_writer_module_tests
    PROPERTIES
        LABELS "module;module/TextFileWriter"
        TIMEOUT 120
)
