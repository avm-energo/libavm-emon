add_executable(get_device_info get_device_info/main.cpp)
target_link_libraries(get_device_info PRIVATE ${PROJECT_NAME})

add_executable(get_analog_inputs get_analog_inputs/main.cpp)
target_link_libraries(get_analog_inputs PRIVATE ${PROJECT_NAME})

add_executable(set_limits set_limits/main.cpp)
target_link_libraries(set_limits PRIVATE ${PROJECT_NAME})
