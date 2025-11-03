cmake_minimum_required(VERSION 3.16)

# Project information
project(Catime VERSION 1.0.0 LANGUAGES C RC)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Set default build type to Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Add Windows specific definitions
add_definitions(-D_WINDOWS)

# Collect all source files
file(GLOB_RECURSE SOURCES "src/*.c")

# Explicitly list refactored modules for clarity (optional, GLOB_RECURSE already includes them)

# Configuration modules (refactored from config_core.c):
#   src/config_core.c       - Coordinator (simplified)
#   src/config_defaults.c   - Default values and metadata
#   src/config_loader.c     - Reading and parsing
#   src/config_applier.c    - Applying to global variables
#   src/config_writer.c     - Collecting and writing

# Dialog modules (refactored from dialog_procedure.c - 2057 lines → 6 modules):
#   src/dialog_common.c       - Common infrastructure (~400 lines)
#   src/dialog_error.c        - Error dialogs (~50 lines)
#   src/dialog_input.c        - Generic input dialogs (~400 lines)
#   src/dialog_info.c         - About, website, etc. (~350 lines)
#   src/dialog_pomodoro.c     - Pomodoro dialogs (~300 lines)
#   src/dialog_notification.c - Notification settings (~400 lines)

# Font modules (refactored from font.c - 767 lines → 4 specialized modules):
#   src/font/font_ttf_parser.c    - Binary TTF/OTF parsing (~180 lines)
#   src/font/font_path_manager.c  - Path resolution and auto-recovery (~240 lines)
#   src/font/font_manager.c       - Loading, preview, GDI management (~330 lines)
#   src/font/font_config.c        - Configuration persistence (~50 lines)

# Utility modules (extracted from multiple files - eliminates duplication):
#   src/utils/string_convert.c    - UTF-8 ↔ UTF-16 conversion (~80 lines)
#   src/utils/path_utils.c        - Path manipulation (~260 lines)

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
    libs/miniaudio
)

# Add compile definitions
target_compile_definitions(catime PRIVATE
    MINIAUDIO_IMPLEMENTATION
    UNICODE
    _UNICODE
)

# Set compiler flags based on build type
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_definitions(catime PRIVATE NDEBUG)
    
    # Optimization flags for MinGW
    target_compile_options(catime PRIVATE
        -O3
        -mtune=generic
        -ffunction-sections
        -fdata-sections
        -fno-strict-aliasing
        -flto
        -fno-exceptions
        -fomit-frame-pointer
        -fmerge-all-constants
        -fno-math-errno
        -fno-trapping-math
        -ffast-math
        -Os
        -Wno-unknown-warning-option
    )
    
    # Link-time optimization and size reduction
    target_link_options(catime PRIVATE
        -mwindows
        -flto
        -Wl,--gc-sections
        -s
        -Wl,--strip-all
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
    winmm
    comctl32
    dwmapi
    user32
    gdi32
    shlwapi
    advapi32
    iphlpapi
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