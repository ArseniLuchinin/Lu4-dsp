cmake_minimum_required(VERSION 3.10)
project(QPSKDecision-module)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(${PROJECT_NAME} SHARED
    src/QPSKDecision.cpp
    src/QPSKDecisionCuda.cu
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
        CUDA::cudart
        logger
)

if(BUILD_TESTING AND PROJECT_ENABLE_GTEST)
    add_subdirectory(tests)
endif()

configure_file(module.json ${CMAKE_CURRENT_BINARY_DIR}/module.json COPYONLY)
configure_file(README.md ${CMAKE_CURRENT_BINARY_DIR}/README.md COPYONLY)
