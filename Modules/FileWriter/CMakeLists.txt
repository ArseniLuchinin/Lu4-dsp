cmake_minimum_required(VERSION 3.18)
project(FileWriter-module LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

find_package(CUDAToolkit REQUIRED)

add_library(${PROJECT_NAME} SHARED
    src/FileWriter.cpp
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        include
        ${MODULE_INCLUDE_DIRS}
        ${DATA_CONTAINERS_INCLUDE_DIRS}
        ..
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        CUDA::cudart
        DataContainers
        Module
        logger
)

target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:-Wall>
)