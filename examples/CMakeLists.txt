add_executable(set_brightness set_brightness.cpp)
target_link_libraries(set_brightness PRIVATE ledriver)

if(LEDRIVER_WINDOWS_SOCKETS)
    target_link_libraries(set_brightness PRIVATE ws2_32)
endif()
