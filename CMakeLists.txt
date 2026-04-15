cmake_minimum_required(VERSION 3.16)

# Project information
project(Catime VERSION 1.0.0 LANGUAGES C RC)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(CATIME_ENABLE_WARNINGS "Enable stricter compiler warnings" ON)
option(CATIME_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)
option(CATIME_ENABLE_ASAN "Enable AddressSanitizer for analyzer builds" OFF)
option(CATIME_ENABLE_MSVC_ANALYZE "Enable MSVC static analysis" OFF)
option(CATIME_ENABLE_BINARY_HARDENING "Enable Win32 binary hardening flags" ON)

# Set default build type to Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Add Windows specific definitions
add_definitions(-D_WINDOWS)

# Collect all source files
file(GLOB_RECURSE SOURCES "src/*.c")

# Collect all header files
file(GLOB_RECURSE HEADERS "include/*.h")

# Collect resource files
set(RESOURCE_FILES
    resource/resource.rc
    resource/languages.rc
    resource/catime.rc
)

# Create executable
add_executable(catime ${SOURCES} ${HEADERS} ${RESOURCE_FILES})

# Add include directories
target_include_directories(catime PRIVATE
    include
    src
    libs/miniaudio
)

# Add compile definitions
target_compile_definitions(catime PRIVATE
    MINIAUDIO_IMPLEMENTATION
    UNICODE
    _UNICODE

    # miniaudio optimization: disable unused features
    MA_NO_GENERATION
    MA_NO_ENCODING

    # Disable unsupported audio formats
    MA_NO_VORBIS
    MA_NO_OPUS
)

if(MSVC)
    target_compile_definitions(catime PRIVATE
        _CRT_SECURE_NO_WARNINGS
        _CRT_NONSTDC_NO_WARNINGS
        strcasecmp=_stricmp
        strdup=_strdup
    )
endif()

if(MSVC)
    target_compile_options(catime PRIVATE /std:c11)
endif()

if(CATIME_ENABLE_WARNINGS)
    if(MSVC)
        target_compile_options(catime PRIVATE /W4)
        if(CATIME_WARNINGS_AS_ERRORS)
            target_compile_options(catime PRIVATE /WX)
        endif()
    elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(catime PRIVATE -Wall -Wextra -Wpedantic)
        if(CATIME_WARNINGS_AS_ERRORS)
            target_compile_options(catime PRIVATE -Werror)
        endif()
    endif()
endif()

# Set compiler flags based on build type
if(MSVC)
    target_link_options(catime PRIVATE /SUBSYSTEM:WINDOWS)

    if(CATIME_ENABLE_BINARY_HARDENING AND NOT CATIME_ENABLE_ASAN)
        target_compile_options(catime PRIVATE /guard:cf)
        target_link_options(catime PRIVATE /DYNAMICBASE /NXCOMPAT /guard:cf)
        if(CMAKE_SIZEOF_VOID_P EQUAL 4)
            target_link_options(catime PRIVATE /SAFESEH)
        endif()
    endif()

    if(CATIME_ENABLE_MSVC_ANALYZE)
        target_compile_options(catime PRIVATE
            /analyze
            /analyze:quiet
            /analyze:autolog
            /analyze:log:format:sarif
        )
    endif()

    if(CATIME_ENABLE_ASAN)
        target_compile_options(catime PRIVATE /fsanitize=address /Zi /Od)
        target_link_options(catime PRIVATE /fsanitize=address /DEBUG /INCREMENTAL:NO)
    elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_definitions(catime PRIVATE NDEBUG)
        target_compile_options(catime PRIVATE /O2 /Oi /Gy)
        target_link_options(catime PRIVATE /OPT:REF /OPT:ICF)
    else()
        target_compile_options(catime PRIVATE /Zi /Od)
        target_link_options(catime PRIVATE /DEBUG)
    endif()
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_definitions(catime PRIVATE NDEBUG)

    # Optimization flags for MinGW (size-optimized)
    target_compile_options(catime PRIVATE
        -Os                                      # Optimize for size (replaces -O3)
        -mtune=generic
        -ffunction-sections                      # Enable function-level linking
        -fdata-sections                          # Enable data-level linking
        -fno-strict-aliasing
        -flto                                    # Link-time optimization
        -fno-exceptions                          # Disable C++ exceptions
        -fomit-frame-pointer                     # Remove frame pointers
        -fmerge-all-constants                    # Merge identical constants
        -fno-math-errno                          # Don't set errno for math functions
        -fno-trapping-math                       # Allow math optimizations
        -ffast-math                              # Fast floating-point math
        -finline-small-functions
        -finline-functions-called-once
        -fno-unwind-tables                       # Remove unwind tables
        -fno-asynchronous-unwind-tables          # Remove async unwind tables
        -fno-ident                               # Remove compiler identification
        -fno-stack-protector                     # Disable stack protection (size reduction)
        -Wno-unknown-warning-option
    )

    # Link-time optimization and size reduction
    target_link_options(catime PRIVATE
        -mwindows                                # Windows GUI application
        -flto                                    # Link-time optimization
        -Wl,--gc-sections                        # Remove unused sections
        -Wl,--strip-all                          # Strip all symbols
        -Wl,--build-id=none                      # Remove build ID section
        -Wl,--no-insert-timestamp                # Remove PE timestamp for reproducibility
        -static                                  # Static link CRT
    )
else()
    # Debug flags
    target_compile_options(catime PRIVATE
        -g
        -O0
        -Wno-unknown-warning-option
    )
    
    target_link_options(catime PRIVATE
        -mwindows
    )
endif()

# Link Windows libraries
target_link_libraries(catime PRIVATE
    ole32
    shell32
    comdlg32
    uuid
    wininet
    winhttp
    winmm
    comctl32
    dwmapi
    user32
    gdi32
    shlwapi
    advapi32
    iphlpapi
    windowscodecs
    propsys
    crypt32
)

# Set output directory - use custom output dir if specified
if(DEFINED CATIME_OUTPUT_DIR)
    set(OUTPUT_DIR ${CATIME_OUTPUT_DIR})
else()
    set(OUTPUT_DIR ${CMAKE_BINARY_DIR})
endif()

set_target_properties(catime PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    OUTPUT_NAME "catime"
)

# Custom target to display build information
add_custom_command(TARGET catime POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Build completed successfully!"
    COMMAND ${CMAKE_COMMAND} -E echo "Output directory: ${CMAKE_BINARY_DIR}"
    COMMAND ${CMAKE_COMMAND} -E echo "Executable: $<TARGET_FILE:catime>"
)

# Option for debug mode
option(ENABLE_DEBUG "Enable debug mode" OFF)
if(ENABLE_DEBUG)
    target_compile_definitions(catime PRIVATE DEBUG_MODE)
endif()
