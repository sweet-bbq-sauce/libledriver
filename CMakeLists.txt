cmake_minimum_required(VERSION 3.20)
project(libledriver
    VERSION 0.1
    DESCRIPTION "UDP client library for custom ESP32 RGB LED strip driver"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(ledriver STATIC ledriver.cpp)
target_include_directories(ledriver PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ledriver PUBLIC $<$<PLATFORM_ID:Windows>:ws2_32>)