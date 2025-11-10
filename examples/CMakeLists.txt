# Example programs for libPorpoise

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

# PAD (controller) example
add_executable(pad_test pad_test.c)
target_link_libraries(pad_test porpoise)
target_include_directories(pad_test PRIVATE ${CMAKE_SOURCE_DIR}/include)

# DVD (file system) example
add_executable(dvd_test dvd_test.c)
target_link_libraries(dvd_test porpoise)
target_include_directories(dvd_test PRIVATE ${CMAKE_SOURCE_DIR}/include)

# ARAM example
add_executable(aram_test aram_test.c)
target_link_libraries(aram_test porpoise)
target_include_directories(aram_test PRIVATE ${CMAKE_SOURCE_DIR}/include)

# VI (video interface) example
add_executable(vi_test vi_test.c)
target_link_libraries(vi_test porpoise)
target_include_directories(vi_test PRIVATE ${CMAKE_SOURCE_DIR}/include)

# CARD (memory card) example
add_executable(card_test card_test.c)
target_link_libraries(card_test porpoise)
target_include_directories(card_test PRIVATE ${CMAKE_SOURCE_DIR}/include)

