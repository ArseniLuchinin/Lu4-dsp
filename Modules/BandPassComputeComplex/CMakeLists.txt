cmake_minimum_required(VERSION 3.10)
project(BandPassComputeComplex-module)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(${PROJECT_NAME} SHARED
    src/BandPassComputeComplex.cpp
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
        DataContainers
        Module
        logger
)

configure_file(module.json ${CMAKE_CURRENT_BINARY_DIR}/module.json COPYONLY)
configure_file(README.md ${CMAKE_CURRENT_BINARY_DIR}/README.md COPYONLY)
