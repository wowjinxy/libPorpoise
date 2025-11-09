# Example programs for Porpoise SDK

# Simple example
add_executable(simple_example simple.c)
target_link_libraries(simple_example porpoise)
target_include_directories(simple_example PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Thread example
add_executable(thread_example thread_test.c)
target_link_libraries(thread_example porpoise)
target_include_directories(thread_example PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Locked cache example (demonstrates full memory emulation)
add_executable(locked_cache_example locked_cache_example.c)
target_link_libraries(locked_cache_example porpoise)

add_executable(reset_button_example reset_button_example.c)
target_link_libraries(reset_button_example porpoise)
target_include_directories(reset_button_example PRIVATE ${CMAKE_SOURCE_DIR}/include)

target_include_directories(locked_cache_example PRIVATE ${CMAKE_SOURCE_DIR}/include)

