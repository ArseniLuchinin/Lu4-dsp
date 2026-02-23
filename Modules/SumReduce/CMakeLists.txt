cmake_minimum_required(VERSION 3.10)
project(SumReduce-module)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(CUDAToolkit REQUIRED)

# Список исходных файлов
set(SOURCES
    src/SumReduce.cu
    ../../src/GpuFloatSignal.cpp
)

add_library(SumReduce-module SHARED
    src/SumReduce.cu
    ../../src/GpuFloatSignal.cpp
)

set_target_properties(SumReduce-module PROPERTIES
    CUDA_SEPARABLE_COMPILATION ON
    CUDA_ARCHITECTURES 75  
)

target_include_directories(${PROJECT_NAME} 
    PRIVATE
        include
        ../../include
        ..
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        CUDA::cublas
        CUDA::cudart
)