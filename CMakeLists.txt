cmake_minimum_required(VERSION 3.10)
project(computing_server LANGUAGES CXX CUDA)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CUDA_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(CUDAToolkit)
find_package(PkgConfig REQUIRED)
pkg_check_modules(HIREDIS REQUIRED hiredis)
pkg_check_modules(REDISPP REQUIRED redis++)
find_package(Boost REQUIRED COMPONENTS log log_setup thread system)


set(MODULE_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/Module/include")
set(DATA_CONTAINERS_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/DataContainers/include")

add_subdirectory(DataContainers)

add_subdirectory(Module)

add_subdirectory(Core)

add_executable(computing_server main.cpp)
target_include_directories(computing_server
    PRIVATE
        DataContainers/include
        Core/include
)

target_link_libraries(computing_server
    PRIVATE
        Core
        DataContainers
        logger
)

add_subdirectory(Modules/FileSrc)
add_subdirectory(Modules/SumReduce)
add_subdirectory(Modules/SignalPlot)
add_subdirectory(Modules/FFT)
add_subdirectory(Modules/CS2AS)
