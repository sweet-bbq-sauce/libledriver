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

set(LEDRIVER_WINDOWS_SOCKETS FALSE)

if(WIN32 OR MINGW OR CMAKE_CXX_COMPILER MATCHES "w64-mingw32|mingw")
    set(LEDRIVER_WINDOWS_SOCKETS TRUE)
endif()

add_library(ledriver STATIC ledriver.cpp)
target_include_directories(ledriver PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(LEDRIVER_WINDOWS_SOCKETS)
    target_link_libraries(ledriver PUBLIC ws2_32)
endif()

find_package(Doxygen)

if(DOXYGEN_FOUND)
    set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in)
    set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

    configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)

    add_custom_target(docs
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM
    )
endif()

option(BUILD_EXAMPLES "Build examples" ON)

if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
