#!/bin/bash

# Catime CMake Build Script for WSL with MinGW-64
# This script builds the Catime project using CMake and MinGW-64 cross-compiler

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
NC='\033[0m' # No Color

# Display logo
echo -e "${PURPLE}"
echo "██████╗  █████╗ ████████╗██╗███╗   ███╗███████╗"
echo "██╔════╝ ██╔══██╗╚══██╔══╝██║████╗ ████║██╔════╝"
echo "██║      ███████║   ██║   ██║██╔████╔██║█████╗  "
echo "██║      ██╔══██║   ██║   ██║██║╚██╔╝██║██╔══╝  "
echo "╚██████╗ ██║  ██║   ██║   ██║██║ ╚═╝ ██║███████╗"
echo " ╚═════╝ ╚═╝  ╚═╝   ╚═╝   ╚═╝╚═╝     ╚═╝╚══════╝"
echo -e "${NC}"

# Configuration
BUILD_TYPE=${1:-Release}
BUILD_DIR="build"
TOOLCHAIN_FILE="mingw-w64-toolchain.cmake"

echo -e "${BLUE}Building Catime with CMake...${NC}"
echo -e "${YELLOW}Build Type: ${BUILD_TYPE}${NC}"

# Check if MinGW-64 is installed
if ! command -v x86_64-w64-mingw32-gcc &> /dev/null; then
    echo -e "${RED}Error: MinGW-64 cross-compiler not found!${NC}"
    echo -e "${YELLOW}Please install it with: sudo apt install mingw-w64${NC}"
    exit 1
fi

# Create toolchain file if it doesn't exist
if [ ! -f "$TOOLCHAIN_FILE" ]; then
    echo -e "${YELLOW}Creating MinGW-64 toolchain file...${NC}"
    cat > "$TOOLCHAIN_FILE" << 'EOF'
# MinGW-64 Cross-compilation toolchain file
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Specify the cross compiler
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)

# Where to find the target environment
set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)

# Search for programs in the build host directories
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# Search for libraries and headers in the target directories
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
EOF
fi

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure with CMake
echo -e "${YELLOW}Configuring project...${NC}"
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE="../$TOOLCHAIN_FILE" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_VERBOSE_MAKEFILE=ON

# Build the project
echo -e "${YELLOW}Building project...${NC}"
cmake --build . --config "$BUILD_TYPE" -j$(nproc)

# Check if build was successful
if [ -f "catime.exe" ]; then
    echo -e "${GREEN}Build completed successfully!${NC}"
    echo -e "${GREEN}Executable: $(pwd)/catime.exe${NC}"
    
    # Display file size
    SIZE=$(stat -c%s "catime.exe")
    if [ $SIZE -lt 1024 ]; then
        SIZE_TEXT="${SIZE} B"
    elif [ $SIZE -lt 1048576 ]; then
        SIZE_TEXT="$((SIZE/1024)) KB"
    else
        SIZE_TEXT="$((SIZE/1048576)) MB"
    fi
    echo -e "${GREEN}Size: ${SIZE_TEXT}${NC}"
else
    echo -e "${RED}Build failed!${NC}"
    exit 1
fi