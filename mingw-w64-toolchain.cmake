# MinGW-64 Cross-compilation toolchain file
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Specify the cross compiler with full paths
set(CMAKE_C_COMPILER /usr/sbin/x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER /usr/sbin/x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER /usr/sbin/x86_64-w64-mingw32-windres)

# Where to find the target environment
set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)

# Search for programs in the build host directories
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# Search for libraries and headers in the target directories
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
