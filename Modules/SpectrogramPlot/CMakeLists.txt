cmake_minimum_required(VERSION 3.10)
project(SpectrogramPlot-module LANGUAGES CXX CUDA)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(CUDAToolkit REQUIRED)
find_package(OpenCV REQUIRED COMPONENTS core imgcodecs)

add_library(${PROJECT_NAME} SHARED
    src/SpectrogramPlot.cpp
    src/SpectrogramPlotCuda.cu
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
        ${OpenCV_LIBS}
        DataContainers
        Module
        logger
)

configure_file(module.json ${CMAKE_CURRENT_BINARY_DIR}/module.json COPYONLY)
configure_file(README.md ${CMAKE_CURRENT_BINARY_DIR}/README.md COPYONLY)
