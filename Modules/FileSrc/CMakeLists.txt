cmake_minimum_required(VERSION 3.10)
project(FileSrc-module)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Список исходных файлов
set(SOURCES
    src/FileSrc.cpp
)

# Создание исполняемого файла
add_library(${PROJECT_NAME} SHARED ${SOURCES})
target_include_directories(${PROJECT_NAME} 
    PRIVATE
        include
        ${MODULE_INCLUDE_DIRS}
        ${DATA_CONTAINERS_INCLUDE_DIRS}
        ..)

target_link_libraries(${PROJECT_NAME} 
    DataContainers
    Module
    logger
)