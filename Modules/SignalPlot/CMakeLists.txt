cmake_minimum_required(VERSION 3.10)
project(SignalPlot-module)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)  # Add this
add_subdirectory(external/matplotplusplus)

set(SOURCES
    src/SignalPlot.cpp
    ../../src/GpuFloatSignal.cpp
    ../../src/CpuFloatSignal.cpp
)

# Создание исполняемого файла
add_library(${PROJECT_NAME} SHARED ${SOURCES})
target_include_directories(${PROJECT_NAME} 
    PRIVATE
        include
        ../../include
        ..)

target_link_libraries(${PROJECT_NAME} PUBLIC matplot
    logger
)