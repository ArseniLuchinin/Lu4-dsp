cmake_minimum_required(VERSION 3.10)
project(computing_server LANGUAGES CXX CUDA)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CUDA_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(CUDAToolkit)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(Boost REQUIRED)


add_compile_definitions(NDEBUG)
# Список исходных файлов
set(SOURCES
    src/main.cpp
    src/ModuleFactory.cpp
    src/Conveyor.cpp
    src/GpuFloatSignal.cpp
    src/CpuFloatSignal.cpp
    src/RedisClient.cpp
)

# Создание исполняемого файла
add_executable(computing_server ${SOURCES})
target_include_directories(computing_server PRIVATE include)
target_link_libraries(computing_server PRIVATE CUDA::cudart OpenSSL::SSL OpenSSL::Crypto Threads::Threads)

if(TARGET Boost::headers)
    target_link_libraries(computing_server PRIVATE Boost::headers)
elseif(TARGET Boost::boost)
    target_link_libraries(computing_server PRIVATE Boost::boost)
endif()


add_subdirectory(Modules/FileSrc)
add_subdirectory(Modules/SumReduce)
