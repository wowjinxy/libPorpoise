cmake_minimum_required(VERSION 3.15)
project(libPorpoise VERSION 0.1.0 LANGUAGES C CXX)

# Set C/C++ standards
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options
option(PORPOISE_BUILD_EXAMPLES "Build example programs" ON)
option(PORPOISE_BUILD_SHARED "Build shared library" OFF)
option(PORPOISE_USE_GECKO_MEMORY "Enable full memory emulation (locked cache support)" OFF)

# Output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Find SDL2
find_package(SDL2 REQUIRED)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${SDL2_INCLUDE_DIRS})

# Source files
set(PORPOISE_SOURCES
    # Core OS
    src/os/OS.c
    src/os/OSThread.c
    src/os/OSTime.c
    src/os/OSAlarm.c
    src/os/OSAlloc.c
    src/os/OSCache.c
    src/os/OSContext.c
    src/os/OSError.c
    src/os/OSFont.c
    src/os/OSInterrupt.c
    src/os/OSMemory.c
    src/os/OSMessage.c
    src/os/OSSemaphore.c
    src/os/OSReset.c
    src/os/OSResetSW.c
    src/os/OSRtc.c
    src/os/GeckoMemory.c
    
    # PAD (Controller)
    src/pad/PAD.c
    src/pad/PADClamp.c
    src/pad/PADConfig.c
    
    # DVD (File System)
    src/dvd/DVD.c
)

# Create library
if(PORPOISE_BUILD_SHARED)
    add_library(porpoise SHARED ${PORPOISE_SOURCES})
else()
    add_library(porpoise STATIC ${PORPOISE_SOURCES})
endif()

# Set library properties
set_target_properties(porpoise PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

# Enable full memory emulation if requested
if(PORPOISE_USE_GECKO_MEMORY)
    target_compile_definitions(porpoise PUBLIC PORPOISE_USE_GECKO_MEMORY)
endif()

# Link SDL2
target_link_libraries(porpoise PUBLIC ${SDL2_LIBRARIES})

# Platform-specific settings
if(WIN32)
    target_compile_definitions(porpoise PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_link_libraries(porpoise PRIVATE winmm)
elseif(UNIX)
    target_link_libraries(porpoise PRIVATE pthread m)
endif()

# Install rules
install(TARGETS porpoise
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/dolphin
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)

# Examples
if(PORPOISE_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Print configuration
message(STATUS "libPorpoise Configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Library type: ${PORPOISE_BUILD_SHARED}")
message(STATUS "  Build examples: ${PORPOISE_BUILD_EXAMPLES}")
message(STATUS "  Full memory emulation: ${PORPOISE_USE_GECKO_MEMORY}")
