cmake_minimum_required(VERSION 3.10)
project(Core LANGUAGES CXX CUDA)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CUDA_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


add_library(${PROJECT_NAME}
    src/ModuleFactory.cpp
    src/Conveyor.cpp
    src/CheckGpu.cpp
    src/JsonReader.cpp

    include/ModuleFactory.hpp
    include/Conveyor.hpp
    include/JsonReader.hpp
)

target_include_directories(${PROJECT_NAME} PUBLIC
    ${HIREDIS_INCLUDE_DIRS}
    ${REDISPP_INCLUDE_DIRS}
    include
    ${MODULE_INCLUDE_DIRS}
    ${DATA_CONTAINERS_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${HIREDIS_LIBRARIES}
    ${REDISPP_LIBRARIES}
    Boost::json
    logger
)
