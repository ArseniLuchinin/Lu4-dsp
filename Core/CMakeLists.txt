cmake_minimum_required(VERSION 3.10)
project(Core LANGUAGES CXX CUDA)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CUDA_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


add_library(${PROJECT_NAME}
    src/ModuleFactory.cpp
    src/Conveyor.cpp
    src/CheckGpu.cpp
    src/ConveyorFactory.cpp
    src/ConveyorOrchestrator.cpp

    include/ModuleFactory.hpp
    include/Conveyor.hpp
    include/ConveyorFactory.hpp
    include/ConveyorOrchestrator.hpp
)

target_include_directories(${PROJECT_NAME} PUBLIC
    include
    ${MODULE_INCLUDE_DIRS}
    ${DATA_CONTAINERS_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Boost::json
    logger
    Module
)
