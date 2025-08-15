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
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Progress bar function
show_progress() {
    local current=$1
    local total=$2
    local message=$3
    local width=40
    local percentage=$((current * 100 / total))
    local filled=$((current * width / total))
    local empty=$((width - filled))
    
    printf "\r\x1b[38;2;147;112;219m[%3d%%]\x1b[0m " $percentage
    printf "\x1b[38;2;138;43;226m"
    printf "%*s" $filled | tr ' ' '█'
    printf "\x1b[38;2;80;80;80m"
    printf "%*s" $empty | tr ' ' '░'
    printf "\x1b[0m %s" "$message"
}

# Spinner function for long operations
spinner() {
    local pid=$1
    local message=$2
    local spin='⠋⠙⠹⠸⠼⠴⠦⠧⠇⠏'
    local i=0
    
    while kill -0 $pid 2>/dev/null; do
        i=$(( (i+1) %10 ))
        printf "\r\x1b[38;2;147;112;219m%s\x1b[0m %s" "${spin:$i:1}" "$message"
        sleep 0.1
    done
    printf "\r\x1b[38;2;0;255;0m✓\x1b[0m %s\n" "$message"
}

# Display logo with elegant purple gradient (matching xmake)
echo ""
echo -e "\x1b[1m\x1b[38;2;138;43;226m██████╗ \x1b[38;2;147;112;219m █████╗ \x1b[38;2;153;102;255m████████╗\x1b[38;2;160;120;255m██╗\x1b[38;2;186;85;211m███╗   ███╗\x1b[38;2;221;160;221m███████╗\x1b[0m"
echo -e "\x1b[1m\x1b[38;2;138;43;226m██╔════╝\x1b[38;2;147;112;219m ██╔══██╗\x1b[38;2;153;102;255m╚══██╔══╝\x1b[38;2;160;120;255m██║\x1b[38;2;186;85;211m████╗ ████║\x1b[38;2;221;160;221m██╔════╝\x1b[0m"
echo -e "\x1b[1m\x1b[38;2;138;43;226m██║     \x1b[38;2;147;112;219m ███████║\x1b[38;2;153;102;255m   ██║   \x1b[38;2;160;120;255m██║\x1b[38;2;186;85;211m██╔████╔██║\x1b[38;2;221;160;221m█████╗  \x1b[0m"
echo -e "\x1b[1m\x1b[38;2;138;43;226m██║     \x1b[38;2;147;112;219m ██╔══██║\x1b[38;2;153;102;255m   ██║   \x1b[38;2;160;120;255m██║\x1b[38;2;186;85;211m██║╚██╔╝██║\x1b[38;2;221;160;221m██╔══╝  \x1b[0m"
echo -e "\x1b[1m\x1b[38;2;138;43;226m╚██████╗\x1b[38;2;147;112;219m ██║  ██║\x1b[38;2;153;102;255m   ██║   \x1b[38;2;160;120;255m██║\x1b[38;2;186;85;211m██║ ╚═╝ ██║\x1b[38;2;221;160;221m███████╗\x1b[0m"
echo -e "\x1b[1m\x1b[38;2;138;43;226m ╚═════╝\x1b[38;2;147;112;219m ╚═╝  ╚═╝\x1b[38;2;153;102;255m   ╚═╝   \x1b[38;2;160;120;255m╚═╝\x1b[38;2;186;85;211m╚═╝     ╚═╝\x1b[38;2;221;160;221m╚══════╝\x1b[0m"
echo ""

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

# Step 1: Configure with CMake
show_progress 1 4 "Configuring project..."
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE="../$TOOLCHAIN_FILE" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    > cmake_config.log 2>&1
echo ""

# Step 2: Count source files for progress tracking
show_progress 2 4 "Analyzing source files..."
TOTAL_FILES=$(find ../src -name "*.c" | wc -l)
TOTAL_FILES=$((TOTAL_FILES + 3))  # Add resource files
echo ""

# Step 3: Build with progress monitoring
show_progress 3 4 "Compiling source files..."
echo ""

# Build in background and monitor progress
cmake --build . --config "$BUILD_TYPE" -j$(nproc) > build.log 2>&1 &
BUILD_PID=$!

# Monitor build progress
CURRENT_FILE=0
while kill -0 $BUILD_PID 2>/dev/null; do
    # Count compiled object files
    COMPILED=$(find . -name "*.obj" 2>/dev/null | wc -l)
    if [ $COMPILED -gt $CURRENT_FILE ]; then
        CURRENT_FILE=$COMPILED
        if [ $CURRENT_FILE -le $TOTAL_FILES ]; then
            show_progress $CURRENT_FILE $TOTAL_FILES "Compiling ($CURRENT_FILE/$TOTAL_FILES files)..."
        fi
    fi
    sleep 0.2
done

# Wait for build to complete
wait $BUILD_PID
BUILD_RESULT=$?

# Final progress update
show_progress $TOTAL_FILES $TOTAL_FILES "Compilation complete"
echo ""

# Step 4: Finalize
show_progress 4 4 "Finalizing build..."
echo ""

# Check build result
if [ $BUILD_RESULT -ne 0 ]; then
    echo -e "${RED}✗ Build failed!${NC}"
    echo -e "${YELLOW}Check build.log for details${NC}"
    exit 1
fi

# Check if executable was created
if [ -f "catime.exe" ]; then
    echo -e "${GREEN}✓ Build completed successfully!${NC}"
    echo -e "${CYAN}📁 Output: $(pwd)/catime.exe${NC}"
    
    # Display file size with nice formatting
    SIZE=$(stat -c%s "catime.exe")
    if [ $SIZE -lt 1024 ]; then
        SIZE_TEXT="${SIZE} B"
    elif [ $SIZE -lt 1048576 ]; then
        SIZE_TEXT="$((SIZE/1024)) KB"
    else
        SIZE_TEXT="$((SIZE/1048576)) MB"
    fi
    echo -e "${CYAN}📊 Size: ${SIZE_TEXT}${NC}"
    
    # Clean up log files
    rm -f cmake_config.log build.log
else
    echo -e "${RED}✗ Build failed - executable not found!${NC}"
    echo -e "${YELLOW}Check build.log for details${NC}"
    exit 1
fi