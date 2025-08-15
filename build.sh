#!/bin/bash

# Catime CMake Build Script for WSL with MinGW-64
# This script builds the Catime project using CMake and MinGW-64 cross-compiler
#
# Usage:
#   ./build.sh [BUILD_TYPE] [OUTPUT_DIR]
#   
# Parameters:
#   BUILD_TYPE  - Build configuration (Release/Debug, default: Release)
#   OUTPUT_DIR  - Output directory path (default: build)
#
# Examples:
#   ./build.sh                    # Release build in 'build' directory
#   ./build.sh Debug              # Debug build in 'build' directory
#   ./build.sh Release ./dist     # Release build in 'dist' directory
#   ./build.sh Debug ../output    # Debug build in '../output' directory

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Elegant progress bar function with gradient text colors
show_progress() {
    local current=$1
    local total=$2
    local message=$3
    local width=40
    local percentage=$((current * 100 / total))
    local filled=$((current * width / total))
    local empty=$((width - filled))
    
    # Choose message color based on the stage
    local message_color=""
    if [[ "$message" == *"Configuring"* ]]; then
        message_color="\x1b[38;2;100;200;255m"  # Light blue for configuring
    elif [[ "$message" == *"Analyzing"* ]]; then
        message_color="\x1b[38;2;255;200;100m"  # Orange for analyzing
    elif [[ "$message" == *"Compiling"* ]]; then
        message_color="\x1b[38;2;255;100;150m"  # Pink/red for compiling
    elif [[ "$message" == *"Finalizing"* ]]; then
        message_color="\x1b[38;2;100;255;150m"  # Green for finalizing
    else
        message_color="\x1b[38;2;200;200;200m"  # Default light gray
    fi
    
    printf "\r\x1b[1m\x1b[38;2;147;112;219m[%3d%%]\x1b[0m " $percentage
    printf "\x1b[38;2;138;43;226m"
    printf "%*s" $filled | tr ' ' '#'
    printf "\x1b[38;2;80;80;80m"
    printf "%*s" $empty | tr ' ' '.'
    printf "\x1b[0m ${message_color}%s\x1b[0m" "$message"
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

# Function to show help
show_help() {
    echo -e "${CYAN}Catime Build Script${NC}"
    echo ""
    echo -e "${YELLOW}Usage:${NC}"
    echo -e "  ./build.sh [BUILD_TYPE] [OUTPUT_DIR]"
    echo ""
    echo -e "${YELLOW}Parameters:${NC}"
    echo -e "  BUILD_TYPE   Build configuration (Release/Debug, default: Release)"
    echo -e "  OUTPUT_DIR   Output directory path (default: build)"
    echo ""
    echo -e "${YELLOW}Examples:${NC}"
    echo -e "  ./build.sh                    # Release build in 'build' directory"
    echo -e "  ./build.sh Debug              # Debug build in 'build' directory"
    echo -e "  ./build.sh Release ./dist     # Release build in 'dist' directory"
    echo -e "  ./build.sh Debug ../output    # Debug build in '../output' directory"
    echo ""
    echo -e "${YELLOW}Options:${NC}"
    echo -e "  -h, --help   Show this help message"
    echo ""
}

# Check for help flag
if [[ "$1" == "-h" || "$1" == "--help" ]]; then
    show_help
    exit 0
fi

# Configuration
BUILD_TYPE=${1:-Release}
BUILD_DIR=${2:-build}
TOOLCHAIN_FILE="mingw-w64-toolchain.cmake"

# Validate build type
if [[ "$BUILD_TYPE" != "Release" && "$BUILD_TYPE" != "Debug" ]]; then
    echo -e "${RED}✗ Invalid build type: $BUILD_TYPE${NC}"
    echo -e "${YELLOW}Valid options: Release, Debug${NC}"
    exit 1
fi

# Handle output directory
OUTPUT_DIR=${2:-build}
BUILD_DIR="build"

# Convert relative path to absolute if needed and validate
OUTPUT_DIR=$(realpath -m "$OUTPUT_DIR")

echo -e "${CYAN}Build configuration:${NC}"
echo -e "  Build type: ${YELLOW}$BUILD_TYPE${NC}"
echo -e "  Output directory: ${YELLOW}$OUTPUT_DIR${NC}"
echo ""



# Verify MinGW toolchain is available
if ! command -v x86_64-w64-mingw32-gcc &> /dev/null; then
    echo -e "${RED}✗ MinGW-64 toolchain not found!${NC}"
    echo -e "${YELLOW}Please install mingw-w64 toolchain:${NC}"
    echo -e "${CYAN}  Ubuntu/Debian: sudo apt install mingw-w64${NC}"
    echo -e "${CYAN}  Arch Linux: sudo pacman -S mingw-w64-gcc${NC}"
    echo -e "${CYAN}  Fedora: sudo dnf install mingw64-gcc${NC}"
    exit 1
fi

# Always ensure we have the latest toolchain file with correct paths
echo -e "${YELLOW}Updating MinGW-64 toolchain file...${NC}"
cat > "$TOOLCHAIN_FILE" << 'EOF'
# MinGW-64 Cross-compilation toolchain file
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Specify the cross compiler (let CMake find them in PATH)
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)

# Try to find the MinGW installation automatically
# Common paths on different systems
set(MINGW_PATHS 
    /usr/x86_64-w64-mingw32
    /usr/local/x86_64-w64-mingw32
    /opt/mingw64/x86_64-w64-mingw32
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
    set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)
endif()

# Search for programs in the build host directories
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# Search for libraries and headers in the target directories
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
EOF

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Step 1: Configure with CMake
show_progress 0 100 "Configuring project..."
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE="../$TOOLCHAIN_FILE" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCATIME_OUTPUT_DIR="$OUTPUT_DIR" \
    > cmake_config.log 2>&1
show_progress 100 100 "Configuring project... ✓"
echo ""

# Step 2: Count source files for progress tracking
show_progress 0 100 "Analyzing source files..."
TOTAL_FILES=$(find ../src -name "*.c" | wc -l)
TOTAL_FILES=$((TOTAL_FILES + 3))  # Add resource files
show_progress 100 100 "Analyzing source files... ✓"
echo ""

# Step 3: Build with progress monitoring
show_progress 0 100 "Compiling source files..."

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

# Final progress update for compilation step
show_progress 100 100 "Compiling source files... ✓"
echo ""

# Step 4: Finalize
show_progress 0 100 "Finalizing build..."
show_progress 100 100 "Finalizing build... ✓"
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
    
    # Display file size with nice formatting
    SIZE=$(stat -c%s "catime.exe")
    if [ $SIZE -lt 1024 ]; then
        SIZE_TEXT="${SIZE} B"
    elif [ $SIZE -lt 1048576 ]; then
        SIZE_TEXT="$((SIZE/1024)) KB"
    else
        SIZE_TEXT="$((SIZE/1048576)) MB"
    fi
    echo -e "${CYAN}Size: ${SIZE_TEXT}${NC}"
    
    # Create output directory and copy executable if different from build dir
    if [ "$(realpath "$OUTPUT_DIR")" != "$(realpath "../$BUILD_DIR")" ]; then
        mkdir -p "$OUTPUT_DIR"
        cp "catime.exe" "$OUTPUT_DIR/"
        echo -e "${CYAN}Output: $OUTPUT_DIR/catime.exe${NC}"
    else
        echo -e "${CYAN}Output: $(pwd)/catime.exe${NC}"
    fi
    
    # Clean up log files
    rm -f cmake_config.log build.log
    
    # Return to original directory
    cd ..
else
    echo -e "${RED}✗ Build failed - executable not found!${NC}"
    echo -e "${YELLOW}Check build.log for details${NC}"
    exit 1
fi