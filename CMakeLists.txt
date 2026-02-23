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

# Список исходных файлов
set(SOURCES
    src/main.cpp
    src/ModuleFactory.cpp
    src/Conveyor.cpp
    src/GpuFloatSignal.cpp
    src/CpuFloatSignal.cpp
)

# Создание исполняемого файла
add_executable(computing_server ${SOURCES})
target_include_directories(computing_server PRIVATE include)
target_link_libraries(computing_server PRIVATE CUDA::cudart )

add_library(logger INTERFACE)

target_link_libraries(logger INTERFACE
    Boost::log
    Boost::log_setup
    Boost::thread
    Boost::system
)

target_compile_definitions(logger INTERFACE
    BOOST_LOG_DYN_LINK
)

target_include_directories(computing_server PRIVATE
    ${HIREDIS_INCLUDE_DIRS}
    ${REDISPP_INCLUDE_DIRS}
)

target_link_libraries(computing_server PRIVATE
    ${HIREDIS_LIBRARIES}
    ${REDISPP_LIBRARIES}
    logger
)


add_subdirectory(Modules/FileSrc)
add_subdirectory(Modules/SumReduce)
add_subdirectory(Modules/SignalPlot)
