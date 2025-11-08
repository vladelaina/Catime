# MinGW-32 Cross-compilation toolchain file
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR i686)

# Specify the cross compiler (let CMake find them in PATH)
set(CMAKE_C_COMPILER i686-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER i686-w64-mingw32-g++)
set(CMAKE_RC_COMPILER i686-w64-mingw32-windres)

# Try to find the MinGW installation automatically
# Common paths on different systems
set(MINGW_PATHS 
    /usr/i686-w64-mingw32
    /usr/local/i686-w64-mingw32
    /opt/mingw32/i686-w64-mingw32
)

# Find the actual installation path
foreach(path ${MINGW_PATHS})
    if(EXISTS ${path})
        set(CMAKE_FIND_ROOT_PATH ${path})
        break()
    endif()
endforeach()

# Fallback if not found
if(NOT CMAKE_FIND_ROOT_PATH)
    set(CMAKE_FIND_ROOT_PATH /usr/i686-w64-mingw32)
endif()

# Search for programs in the build host directories
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# Search for libraries and headers in the target directories
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
