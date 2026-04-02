cmake_minimum_required(VERSION 3.10)
project(computing_server LANGUAGES CXX CUDA)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CUDA_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CTest)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMakeTargets")

find_package(CUDAToolkit)
find_package(PkgConfig REQUIRED)
pkg_check_modules(HIREDIS REQUIRED hiredis)
pkg_check_modules(REDISPP REQUIRED redis++)
find_package(Boost REQUIRED COMPONENTS log log_setup thread system json)
find_package(tomlplusplus CONFIG REQUIRED)


set(MODULE_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/Module/include")
set(DATA_CONTAINERS_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/DataContainers/include")

add_subdirectory(DataContainers)

add_subdirectory(Module)

add_subdirectory(Core)

if(BUILD_TESTING)
    find_package(GTest QUIET)
    if(GTest_FOUND)
        include(GoogleTest)
        set(PROJECT_ENABLE_GTEST ON)
    else()
        message(WARNING "GTest was not found. Test targets will be skipped.")
        set(PROJECT_ENABLE_GTEST OFF)
    endif()
endif()

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
        Module
        logger
)

function(add_all_subdirs DIR_PATH)
    file(GLOB SUBDIRS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/${DIR_PATH} 
         ${DIR_PATH}/*)
    foreach(SUBDIR ${SUBDIRS})
        set(FULL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/${DIR_PATH}/${SUBDIR}")
        if(IS_DIRECTORY ${FULL_PATH} AND 
           EXISTS "${FULL_PATH}/CMakeLists.txt")
            message(STATUS "Добавляю поддиректорию: ${SUBDIR}")
            add_subdirectory(${DIR_PATH}/${SUBDIR})
            message("found module: ${DIR_PATH}/${SUBDIR}")
        endif()
    endforeach()
endfunction()

add_all_subdirs("Modules")

enable_testing()
if(BUILD_TESTING AND PROJECT_ENABLE_GTEST)
    add_subdirectory(tests)
endif()

# Copy runtime config
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/variables.toml
    ${CMAKE_CURRENT_BINARY_DIR}/variables.toml
    COPYONLY
)
