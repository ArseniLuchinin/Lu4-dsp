cmake_minimum_required(VERSION 3.10)
project(Module LANGUAGES CXX CUDA)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CUDA_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


add_library(${PROJECT_NAME} SHARED
    include/IModule.hpp
    include/Variables.hpp
    include/VirtualTransmitter.hpp
    src/IModule.cpp
    src/IVirtualRX.cpp
    src/VirtualTransmitter.cpp
    src/Vаriables.cpp
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        include
    PRIVATE
        ${DATA_CONTAINERS_INCLUDE_DIRS}
)

add_library(logger INTERFACE)
target_link_libraries(logger INTERFACE
    Boost::log
    Boost::log_setup
    Boost::thread
    Boost::system
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    tomlplusplus::tomlplusplus
)

set_target_properties(Module PROPERTIES LINKER_LANGUAGE CXX)
target_compile_definitions(logger INTERFACE
    BOOST_LOG_DYN_LINK
)
