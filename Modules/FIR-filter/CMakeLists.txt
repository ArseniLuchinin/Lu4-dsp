cmake_minimum_required(VERSION 3.18) 
project(FIR-filter-module LANGUAGES CXX CUDA) 

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CUDA_STANDARD 17)  # ← CUDA стандарт!
set(CMAKE_CUDA_STANDARD_REQUIRED ON)

find_package(CUDAToolkit REQUIRED)

add_library(${PROJECT_NAME} SHARED
    src/filter_kernal.cu
    src/FIR_filter.cpp
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    CUDA_STANDARD 17
    CUDA_ARCHITECTURES 75
    CUDA_SEPARABLE_COMPILATION ON
    POSITION_INDEPENDENT_CODE ON
)

# ✅ ИСПРАВЛЕНО: правильное имя файла
set_source_files_properties(src/filter_kernal.cu PROPERTIES  # ← kernal!
    LANGUAGE CUDA
    COMPILE_FLAGS "--expt-relaxed-constexpr"
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
    $<$<COMPILE_LANGUAGE:CUDA>:-Wall -Wextra>
)
