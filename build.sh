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

# set -e  # Disabled to allow proper error handling

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

# Function to format elapsed time
format_time() {
    local total_seconds=$1
    local hours=$((total_seconds / 3600))
    local minutes=$(((total_seconds % 3600) / 60))
    local seconds=$((total_seconds % 60))
    
    if [ $hours -gt 0 ]; then
        printf "%dh %dm %ds" $hours $minutes $seconds
    elif [ $minutes -gt 0 ]; then
        printf "%dm %ds" $minutes $seconds
    else
        printf "%ds" $seconds
    fi
}

# Record start time
START_TIME=$(date +%s)

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

CMAKE_RESULT=$?
if [ $CMAKE_RESULT -ne 0 ]; then
    echo ""
    echo -e "${RED}✗ CMake configuration failed!${NC}"
    echo ""
    echo -e "${YELLOW}Configuration error:${NC}"
    echo -e "${CYAN}─────────────────────────────────────────────────────────────${NC}"
    
    # Try to show relevant error lines first, fallback to full log
    ERROR_LINES=$(grep -E "(error|Error|ERROR|failed|Failed|FAILED|CMake Error|错误)" cmake_config.log 2>/dev/null)
    if [ -n "$ERROR_LINES" ]; then
        echo "$ERROR_LINES"
    else
        echo -e "${YELLOW}No specific error found, showing full configuration log:${NC}"
        echo ""
        cat cmake_config.log
    fi
    
    echo -e "${CYAN}─────────────────────────────────────────────────────────────${NC}"
    echo ""
    cd ..
    exit 1
fi

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

# Build in background and capture output
cmake --build . --config "$BUILD_TYPE" -j$(nproc) > build.log 2>&1 &
BUILD_PID=$!

# Monitor build progress with timeout
CURRENT_FILE=0
TIMEOUT_COUNT=0
MAX_TIMEOUT=600  # 10 minutes timeout
LAST_UPDATE_TIME=$(date +%s)

while kill -0 $BUILD_PID 2>/dev/null; do
    # Count compiled object files
    COMPILED=$(find . -name "*.obj" 2>/dev/null | wc -l)
    CURRENT_TIME=$(date +%s)
    
    if [ $COMPILED -gt $CURRENT_FILE ]; then
        CURRENT_FILE=$COMPILED
        LAST_UPDATE_TIME=$CURRENT_TIME
        if [ $CURRENT_FILE -le $TOTAL_FILES ]; then
            show_progress $CURRENT_FILE $TOTAL_FILES "Compiling ($CURRENT_FILE/$TOTAL_FILES files)..."
        fi
    else
        # Check if no progress for too long
        TIME_DIFF=$((CURRENT_TIME - LAST_UPDATE_TIME))
        if [ $TIME_DIFF -gt 30 ]; then  # 30 seconds without progress
            # Check if build process is actually dead or stuck
            if ! kill -0 $BUILD_PID 2>/dev/null; then
                break
            fi
            # Force break after timeout
            if [ $TIME_DIFF -gt $MAX_TIMEOUT ]; then
                echo ""
                echo -e "${YELLOW}⚠ Build timeout, terminating process...${NC}"
                kill $BUILD_PID 2>/dev/null
                sleep 1
                kill -9 $BUILD_PID 2>/dev/null
                break
            fi
        fi
    fi
    sleep 0.2
done

# Wait for build to complete and get result
wait $BUILD_PID 2>/dev/null
BUILD_RESULT=$?

# Ensure BUILD_RESULT is set (fallback to 1 if empty)
if [ -z "$BUILD_RESULT" ]; then
    BUILD_RESULT=1
fi

# If the process was killed due to timeout, mark as failed
WAIT_RESULT=$?
if [ $WAIT_RESULT -eq 143 ] || [ $WAIT_RESULT -eq 137 ]; then
    BUILD_RESULT=1
fi

# Final progress update for compilation step
if [ "$BUILD_RESULT" -eq 0 ]; then
    show_progress 100 100 "Compiling source files... ✓"
else
    echo ""
    echo -e "${RED}✗ Build failed${NC}"
fi
echo ""

# Step 4: Finalize (only if build succeeded)
if [ "$BUILD_RESULT" -eq 0 ]; then
    show_progress 0 100 "Finalizing build..."
    show_progress 100 100 "Finalizing build... ✓"
    echo ""
fi

# Function to show build errors
show_build_errors() {
    # Show build log content directly
    if [ -f "build.log" ]; then
        # First try to show error lines, if none found, show last 30 lines
        ERROR_LINES=$(grep -E "(error|Error|ERROR|failed|Failed|FAILED|undefined|Undefined|Error:|错误)" build.log 2>/dev/null)
        
        if [ -n "$ERROR_LINES" ]; then
            echo "$ERROR_LINES" | tail -n 20
        else
            tail -n 30 build.log
        fi
    else
        # If no build log, show cmake config errors
        if [ -f "cmake_config.log" ]; then
            cat cmake_config.log
        fi
    fi
}

# Check build result
if [ "$BUILD_RESULT" -ne 0 ]; then
    show_build_errors
    
    # Return to original directory before exiting
    cd ..
    exit 1
fi

# Check if executable was created
if [ -f "catime.exe" ]; then
    # Calculate elapsed time
    END_TIME=$(date +%s)
    ELAPSED_TIME=$((END_TIME - START_TIME))
    FORMATTED_TIME=$(format_time $ELAPSED_TIME)
    
    echo -e "${GREEN}✓ Build completed successfully!${NC}"
    echo -e "${PURPLE}Build time: ${FORMATTED_TIME}${NC}"
    
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
    
    # Only clean up log files on successful build
    echo -e "${YELLOW}Cleaning up temporary files...${NC}"
    rm -f cmake_config.log build.log
    
    # Return to original directory
    cd ..
else
    echo -e "${RED}✗ Build failed - executable not found!${NC}"
    show_build_errors
    
    # Return to original directory before exiting
    cd ..
    exit 1
fi