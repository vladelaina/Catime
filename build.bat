@echo off
setlocal enabledelayedexpansion

REM Catime CMake Build Script for Windows
REM This script builds the Catime project using CMake and MinGW-64

REM Configuration
set BUILD_TYPE=%1
if "%BUILD_TYPE%"=="" set BUILD_TYPE=Release
set BUILD_DIR=build

echo.
echo [1m[38;2;138;43;226m██████╗ [38;2;147;112;219m █████╗ [38;2;153;102;255m████████╗[38;2;160;120;255m██╗[38;2;186;85;211m███╗   ███╗[38;2;221;160;221m███████╗[0m
echo [1m[38;2;138;43;226m██╔════╝[38;2;147;112;219m ██╔══██╗[38;2;153;102;255m╚══██╔══╝[38;2;160;120;255m██║[38;2;186;85;211m████╗ ████║[38;2;221;160;221m██╔════╝[0m
echo [1m[38;2;138;43;226m██║     [38;2;147;112;219m ███████║[38;2;153;102;255m   ██║   [38;2;160;120;255m██║[38;2;186;85;211m██╔████╔██║[38;2;221;160;221m█████╗  [0m
echo [1m[38;2;138;43;226m██║     [38;2;147;112;219m ██╔══██║[38;2;153;102;255m   ██║   [38;2;160;120;255m██║[38;2;186;85;211m██║╚██╔╝██║[38;2;221;160;221m██╔══╝  [0m
echo [1m[38;2;138;43;226m╚██████╗[38;2;147;112;219m ██║  ██║[38;2;153;102;255m   ██║   [38;2;160;120;255m██║[38;2;186;85;211m██║ ╚═╝ ██║[38;2;221;160;221m███████╗[0m
echo [1m[38;2;138;43;226m ╚═════╝[38;2;147;112;219m ╚═╝  ╚═╝[38;2;153;102;255m   ╚═╝   [38;2;160;120;255m╚═╝[38;2;186;85;211m╚═╝     ╚═╝[38;2;221;160;221m╚══════╝[0m
echo.



REM Check if CMake is available
cmake --version >nul 2>&1
if errorlevel 1 (
    echo Error: CMake not found! Please install CMake and add it to PATH.
    pause
    exit /b 1
)

REM Check if MinGW is available
gcc --version >nul 2>&1
if errorlevel 1 (
    echo Error: MinGW GCC not found! Please install MinGW-64 and add it to PATH.
    pause
    exit /b 1
)

REM Create build directory
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
cd "%BUILD_DIR%"

REM Step 1: Configure
echo [93m[25%%] Configuring project...[0m
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=%BUILD_TYPE% >cmake_config.log 2>&1
if errorlevel 1 (
    echo [91mConfiguration failed![0m
    echo Check cmake_config.log for details
    pause
    exit /b 1
)

REM Step 2: Analyze
echo [93m[50%%] Analyzing source files...[0m
timeout /t 1 /nobreak >nul

REM Step 3: Build
echo [93m[75%%] Compiling source files...[0m
cmake --build . --config %BUILD_TYPE% >build.log 2>&1
if errorlevel 1 (
    echo [91mBuild failed![0m
    echo Check build.log for details
    pause
    exit /b 1
)

REM Step 4: Finalize
echo [93m[100%%] Finalizing build...[0m

REM Check if build was successful
if exist "catime.exe" (
    echo.
    echo [92m✓ Build completed successfully![0m
    echo [96mOutput: %CD%\catime.exe[0m
    
    REM Display file size with nice formatting
    for %%A in (catime.exe) do set SIZE=%%~zA
    if !SIZE! LSS 1024 (
        echo [96mSize: !SIZE! B[0m
    ) else if !SIZE! LSS 1048576 (
        set /a SIZE_KB=!SIZE!/1024
        echo [96mSize: !SIZE_KB! KB[0m
    ) else (
        set /a SIZE_MB=!SIZE!/1048576
        echo [96mSize: !SIZE_MB! MB[0m
    )
    
    REM Clean up log files
    del cmake_config.log build.log 2>nul
) else (
    echo [91m✗ Build failed - executable not found![0m
    echo Check build.log for details
    pause
    exit /b 1
)

echo.
pause