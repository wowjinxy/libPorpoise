cmake_minimum_required(VERSION 3.15)
project(libPorpoise VERSION 0.1.0 LANGUAGES CXX C)

# Check for devkitPro and warn if found
# devkitPro headers are incompatible with this PC port
if(DEFINED ENV{INCLUDE})
    string(TOUPPER "$ENV{INCLUDE}" INCLUDE_UPPER)
    if(INCLUDE_UPPER MATCHES "DEVKIT")
        message(WARNING "===========================================================================")
        message(WARNING "WARNING: devkitPro detected in INCLUDE environment variable!")
        message(WARNING "devkitPro headers are incompatible with this PC port.")
        message(WARNING "Please remove devkitPro paths from your INCLUDE environment variable.")
        message(WARNING "This project uses standard system headers (MSVC/MinGW/GCC), not devkitPro.")
        message(WARNING "===========================================================================")
    endif()
endif()

# Set C/C++ standards - C++ is primary language
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable C++ features
set(CMAKE_CXX_EXTENSIONS OFF)

# Options
option(PORPOISE_BUILD_EXAMPLES "Build example programs" OFF)
option(PORPOISE_BUILD_PIKMIN_DEMO "Build the Pikmin demo target" OFF)
option(PORPOISE_BUILD_SHARED "Build shared library" OFF)
option(PORPOISE_USE_GECKO_MEMORY "Enable full memory emulation (locked cache support)" OFF)
option(PORPOISE_USE_COOPERATIVE_THREADS "Use cooperative thread scheduler (OSThread.cpp)" OFF)
set(PORPOISE_GUARD_MODE "LOG_RETURN" CACHE STRING "Runtime guard mode: OFF, LOG_RETURN, PANIC")
set_property(CACHE PORPOISE_GUARD_MODE PROPERTY STRINGS OFF LOG_RETURN PANIC)

# Output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Find SDL2 (prefer system install, fall back to NuGet package)
set(SDL2_INCLUDE_DIRS "")
set(SDL2_LIBRARIES "")
set(SDL2_IMPORTED_TARGET "")
find_package(SDL2 QUIET)
if(SDL2_FOUND)
    message(STATUS "Found SDL2 via find_package")
    set(SDL2_IMPORTED_TARGET SDL2::SDL2)
else()
    message(WARNING "SDL2 not found via find_package. Trying NuGet package (VS).")
    if(WIN32)
        # Prefer packages restored under ${CMAKE_BINARY_DIR}/packages, then fall back
        # to repository-level ${CMAKE_SOURCE_DIR}/packages if present.
        file(GLOB SDL2_NUGET_PACKAGES "${CMAKE_BINARY_DIR}/packages/sdl2.nuget.[0-9]*")
        list(LENGTH SDL2_NUGET_PACKAGES SDL2_NUGET_COUNT)
        if(SDL2_NUGET_COUNT EQUAL 0)
            file(GLOB SDL2_NUGET_PACKAGES "${CMAKE_SOURCE_DIR}/packages/sdl2.nuget.[0-9]*")
        endif()
        list(LENGTH SDL2_NUGET_PACKAGES SDL2_NUGET_COUNT)
        if(SDL2_NUGET_COUNT GREATER 0)
            list(SORT SDL2_NUGET_PACKAGES)
            list(GET SDL2_NUGET_PACKAGES -1 SDL2_NUGET_PATH)
            set(SDL2_INCLUDE_DIRS "${SDL2_NUGET_PATH}/build/native/include")
            if(CMAKE_SIZEOF_VOID_P EQUAL 8)
                set(SDL2_ARCH_DIR "x64")
            else()
                set(SDL2_ARCH_DIR "Win32")
            endif()
            set(SDL2_LIB_FILE "${SDL2_NUGET_PATH}/build/native/lib/${SDL2_ARCH_DIR}/SDL2.lib")
            if(NOT EXISTS "${SDL2_LIB_FILE}")
                message(WARNING "SDL2 NuGet library not found at ${SDL2_LIB_FILE}")
            else()
                set(SDL2_LIBRARIES "${SDL2_LIB_FILE}")
                if(NOT TARGET SDL2::SDL2)
                    add_library(SDL2::SDL2 UNKNOWN IMPORTED)
                endif()
                set_target_properties(SDL2::SDL2 PROPERTIES
                    IMPORTED_LOCATION "${SDL2_LIB_FILE}"
                    INTERFACE_INCLUDE_DIRECTORIES "${SDL2_INCLUDE_DIRS}"
                )
                set(SDL2_IMPORTED_TARGET SDL2::SDL2)
                message(STATUS "Using SDL2 from NuGet: ${SDL2_NUGET_PATH}")
            endif()
        else()
            message(WARNING "SDL2 NuGet package not restored yet (expected under ${CMAKE_BINARY_DIR}/packages)")
        endif()
    endif()
endif()

if(NOT SDL2_IMPORTED_TARGET)
    # Create a dummy interface target to keep include paths working even if SDL2 isn't available yet
    if(NOT TARGET SDL2::SDL2)
        add_library(SDL2::SDL2 INTERFACE IMPORTED)
    endif()
    set(SDL2_IMPORTED_TARGET SDL2::SDL2)
endif()

if(SDL2_LIBRARIES STREQUAL "" AND SDL2_IMPORTED_TARGET)
    set(SDL2_LIBRARIES SDL2::SDL2)
endif()

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${SDL2_INCLUDE_DIRS})

# Default to real OS threads for stability across demos and game builds.
# Cooperative scheduler emulation can deadlock on PC (no real OSLoadContext swap).
if(PORPOISE_USE_COOPERATIVE_THREADS)
    set(OS_THREAD_SOURCE src/os/OSThread.cpp)
    message(WARNING "Using cooperative thread scheduler (OSThread.cpp). This may be unstable on PC.")
else()
    set(OS_THREAD_SOURCE src/os/OSThread.c)
    message(STATUS "Using real OS threads (OSThread.c)")
endif()

# GX backend source selection (ACPC-only)
set(PORPOISE_GX_SOURCES
    # GX backend imported from ACGC-PC-Port
    src/gx/pc_gx.c
    src/gx/pc_gx_texture.c
    src/gx/pc_gx_tev.c
    src/gx/pc_platform_stub.c
    src/gx/pc_texture_pack_stub.c
    src/gx/glad/src/gl.c
    src/gfx/render_acpc.cpp
)

# Source files
set(PORPOISE_SOURCES
    # Core OS
    src/os/OS.c
    src/os/OSFastCast.c
    src/os/OSStopwatch.c
    ${OS_THREAD_SOURCE}
    src/os/OSMutex.c
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
    src/os/OSUart.c

    # libPorpoise runtime guards
    src/porpoise/Guard.c
    
    # OS Init (Startup Entry Points)
    src/os/init/__start.c
    src/os/init/__ppc_eabi_init.c
    
    # Base (PowerPC Architecture)
    src/base/PPCArch.c
    src/base/PPCPm.c
    
    # DB (Dolphin Debugger)
    src/db/db.c
    src/db/dbprintf.c
    
    # PAD (Controller)
    src/pad/PAD.c
    src/pad/PADClamp.c
    src/pad/PADConfig.c
    
    # DVD (File System)
    src/dvd/DVD.c
    src/dvd/DVDQueue.c
    src/dvd/DVDLow.c
    src/dvd/DVDError.c
    src/dvd/DVDFatal.c
    
    # SI (Serial Interface - Controller Hardware)
    src/si/SI.c
    
    # AR (ARAM - Audio RAM)
    src/ar/AR.c
    src/ar/ARQ.c
    
    # VI (Video Interface)
    src/vi/VI.c
    src/vi/VIConfig.c
    
    # EXI (External Interface)
    src/exi/EXI.c
    
    # CARD (Memory Card)
    src/card/CARDBios.c
    src/card/CARDMount.c
    src/card/CARDOpen.c
    src/card/CARDCreate.c
    src/card/CARDDelete.c
    src/card/CARDRead.c
    src/card/CARDWrite.c
    src/card/CARDCheck.c
    src/card/CARDFormat.c
    src/card/CARDStat.c
    src/card/CARDRename.c
    src/card/CARDBlock.c
    
    # MTX (Matrix Math)
    src/mtx/mtx.c
    
    ${PORPOISE_GX_SOURCES}
    
    # Demo Library
    src/demo/DEMOInit.cpp
    src/demo/DEMOPad.cpp
    src/demo/DEMOPuts.cpp
    src/demo/DEMOWin.cpp
    src/demo/DEMOStats.cpp
    src/demo/DEMOFont.cpp
    src/demo/DEMOTpl.c
)

# Create library - C++ primary, C files compiled as C++
if(PORPOISE_BUILD_SHARED)
    add_library(porpoise SHARED ${PORPOISE_SOURCES})
else()
    add_library(porpoise STATIC ${PORPOISE_SOURCES})
endif()

# Always build the PC-targeted path in our headers/source
target_compile_definitions(porpoise PUBLIC TARGET_PC=1)

string(TOUPPER "${PORPOISE_GUARD_MODE}" PORPOISE_GUARD_MODE_UPPER)
if(PORPOISE_GUARD_MODE_UPPER STREQUAL "OFF")
    set(PORPOISE_GUARD_MODE_VALUE 0)
elseif(PORPOISE_GUARD_MODE_UPPER STREQUAL "LOG_RETURN")
    set(PORPOISE_GUARD_MODE_VALUE 1)
elseif(PORPOISE_GUARD_MODE_UPPER STREQUAL "PANIC")
    set(PORPOISE_GUARD_MODE_VALUE 2)
else()
    message(FATAL_ERROR "Invalid PORPOISE_GUARD_MODE='${PORPOISE_GUARD_MODE}'. Use OFF, LOG_RETURN, or PANIC.")
endif()
target_compile_definitions(porpoise PUBLIC PORPOISE_GUARD_MODE=${PORPOISE_GUARD_MODE_VALUE})

# Set library properties - C++ is primary language
set_target_properties(porpoise PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    LINKER_LANGUAGE CXX
)

target_include_directories(porpoise PUBLIC
    ${CMAKE_SOURCE_DIR}/src/gx/glad/include
)
target_compile_definitions(porpoise PRIVATE
    PORPOISE_GX_SHADER_DIR="${CMAKE_SOURCE_DIR}/src/gx/shaders"
    PORPOISE_ACPC_SHADER_DIR="${CMAKE_SOURCE_DIR}/src/gx/shaders"
)

# Enable full memory emulation if requested
if(PORPOISE_USE_GECKO_MEMORY)
    target_compile_definitions(porpoise PUBLIC PORPOISE_USE_GECKO_MEMORY)
endif()

# Find OpenGL (for GX viewport and rendering)
find_package(OpenGL QUIET)
if(OpenGL_FOUND)
    target_link_libraries(porpoise PUBLIC OpenGL::GL)
    message(STATUS "Found OpenGL via find_package")
else()
    # Fallback: link OpenGL libraries directly
    if(WIN32)
        target_link_libraries(porpoise PRIVATE opengl32)
        message(STATUS "Linking OpenGL via opengl32")
    elseif(APPLE)
        find_library(OPENGL_LIBRARY OpenGL)
        if(OPENGL_LIBRARY)
            target_link_libraries(porpoise PRIVATE ${OPENGL_LIBRARY})
            message(STATUS "Found OpenGL library: ${OPENGL_LIBRARY}")
        else()
            message(WARNING "OpenGL library not found on macOS")
        endif()
    else()
        # Linux - try to find GL library
        find_library(OPENGL_LIBRARY GL)
        if(OPENGL_LIBRARY)
            target_link_libraries(porpoise PRIVATE ${OPENGL_LIBRARY})
            message(STATUS "Found OpenGL library: ${OPENGL_LIBRARY}")
        else()
            message(WARNING "OpenGL library not found. GX viewport functions may not work.")
        endif()
    endif()
endif()

# Link SDL2
if(SDL2_FOUND)
    target_link_libraries(porpoise PUBLIC ${SDL2_LIBRARIES})
else()
    # When SDL2_FOUND is false, use the interface target (NuGet will handle actual linking)
    target_link_libraries(porpoise PUBLIC SDL2::SDL2)
endif()

# Platform-specific settings
if(WIN32)
    target_compile_definitions(porpoise PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_link_libraries(porpoise PRIVATE winmm)
    
    # Add SDL2 NuGet package reference for Visual Studio projects
    if(MSVC)
        set_target_properties(porpoise PROPERTIES
            VS_PACKAGE_REFERENCES "SDL2;SDL2.redist"
        )
        message(STATUS "Added SDL2 NuGet package reference to porpoise target")
    endif()
    
    # Explicitly exclude devkitPro paths on Windows
    # Note: devkitPro headers are incompatible with this PC port
    # If you have devkitPro installed, ensure it's not in your INCLUDE environment variable
    if(MSVC)
        # MSVC: Use /X flag to ignore standard include paths, then re-add only what we need
        # This prevents devkitPro headers from being found
        message(STATUS "Using MSVC compiler - devkitPro paths will be excluded")
    elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        message(WARNING "GCC/Clang detected. If you have devkitPro installed, remove it from your PATH and INCLUDE environment variables.")
        message(WARNING "This project uses standard system headers, not devkitPro.")
    endif()
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
    if(EXISTS "${CMAKE_SOURCE_DIR}/examples/CMakeLists.txt")
        add_subdirectory(examples)
    else()
        message(WARNING "PORPOISE_BUILD_EXAMPLES is ON but examples/ is missing; skipping examples build.")
    endif()
endif()

if(PORPOISE_BUILD_PIKMIN_DEMO)
    add_subdirectory(demos/PikminDemo)
endif()

# Print configuration
message(STATUS "libPorpoise Configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Library type: ${PORPOISE_BUILD_SHARED}")
message(STATUS "  Build examples: ${PORPOISE_BUILD_EXAMPLES}")
message(STATUS "  Build Pikmin demo: ${PORPOISE_BUILD_PIKMIN_DEMO}")
message(STATUS "  Full memory emulation: ${PORPOISE_USE_GECKO_MEMORY}")
message(STATUS "  GX backend: ACPC (fixed)")
message(STATUS "  Guard mode: ${PORPOISE_GUARD_MODE_UPPER}")
