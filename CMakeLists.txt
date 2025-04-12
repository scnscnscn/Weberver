cmake_minimum_required(VERSION 3.30)
project(Webserver VERSION 1.00 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Building in Debug mode")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -Wall -Wextra -Wpedantic")
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "Building in Release mode")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")
else()
    message(FATAL_ERROR "Unsupported build type: ${CMAKE_BUILD_TYPE}. Supported types are Debug and Release.")
endif()

set(SOURCE_DIR src)
file(GLOB_RECURSE WEB_SERVER_SRC
        ${SOURCE_DIR}/buffer/*.cpp
        ${SOURCE_DIR}/config/*.cpp
        ${SOURCE_DIR}/http/*.cpp
        ${SOURCE_DIR}/log/*.cpp
        ${SOURCE_DIR}/pool/*.cpp
        ${SOURCE_DIR}/server/*.cpp
        ${SOURCE_DIR}/timer/*.cpp
)
add_library(webserver_lib STATIC ${WEB_SERVER_SRC})

target_include_directories(webserver_lib PUBLIC
        ${SOURCE_DIR}/buffer
        ${SOURCE_DIR}/config
        ${SOURCE_DIR}/http
        ${SOURCE_DIR}/log
        ${SOURCE_DIR}/pool
        ${SOURCE_DIR}/server
        ${SOURCE_DIR}/timer
        /usr/include/mysql
)

find_package(Threads REQUIRED)
target_link_libraries(webserver_lib PUBLIC Threads::Threads)

# 手动指定 MySQL 库文件路径
target_link_libraries(webserver_lib PUBLIC /usr/lib/x86_64-linux-gnu/libmysqlclient.so)

add_executable(Webserver main.cpp)
target_link_libraries(Webserver PRIVATE webserver_lib)