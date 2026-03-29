cmake_minimum_required(VERSION 3.10)
project(DataContainers LANGUAGES CXX CUDA)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CUDA_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


add_library(${PROJECT_NAME} SHARED
    src/GpuSignal.cpp
    src/CpuFloatSignal.cpp
    src/CpuComplexSignal.cpp
    src/CpuByteSignal.cpp
)


target_link_libraries(${PROJECT_NAME} CUDA::cudart logger)

target_include_directories(${PROJECT_NAME} 
    PRIVATE
        include
        ${MODULE_INCLUDE_DIRS}
)
