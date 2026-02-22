
cmake_minimum_required(VERSION 3.10)
project(FileSrc-module)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(CUDAToolkit REQUIRED)

# Список исходных файлов
set(SOURCES
    src/FileSrc.cpp
    ../../src/GpuFloatSignal.cpp
)

# Создание исполняемого файла
add_library(${PROJECT_NAME} SHARED ${SOURCES})
target_include_directories(${PROJECT_NAME} 
    PRIVATE
        include
        ../../include
        ..)
target_link_libraries(${PROJECT_NAME} CUDA::cudart)