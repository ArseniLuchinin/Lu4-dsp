cmake_minimum_required(VERSION 3.10)
project(FFT-module)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(CUDAToolkit REQUIRED)

add_library(${PROJECT_NAME} SHARED
    src/FFT_cufft.cpp
    src/FFT_overlap_impl.cpp
    src/FFT_overlap_callback.cu
    src/example_fft_overlap.cpp
)

set_source_files_properties(src/example_fft_overlap.cpp PROPERTIES
    LANGUAGE CUDA
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    CUDA_SEPARABLE_COMPILATION ON
    CUDA_ARCHITECTURES 75  
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
        CUDA::cufft_static
        CUDA::culibos
        DataContainers
        Module
        logger
)
