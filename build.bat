@echo off
setlocal enabledelayedexpansion

REM Catime CMake Build Script for Windows
REM This script builds the Catime project using CMake and MinGW-64

REM Configuration
set BUILD_TYPE=%1
if "%BUILD_TYPE%"=="" set BUILD_TYPE=Release
set BUILD_DIR=build

echo.
echo ██████╗  █████╗ ████████╗██╗███╗   ███╗███████╗
echo ██╔════╝ ██╔══██╗╚══██╔══╝██║████╗ ████║██╔════╝
echo ██║      ███████║   ██║   ██║██╔████╔██║█████╗  
echo ██║      ██╔══██║   ██║   ██║██║╚██╔╝██║██╔══╝  
echo ╚██████╗ ██║  ██║   ██║   ██║██║ ╚═╝ ██║███████╗
echo  ╚═════╝ ╚═╝  ╚═╝   ╚═╝   ╚═╝╚═╝     ╚═╝╚══════╝
echo.

echo Building Catime with CMake...
echo Build Type: %BUILD_TYPE%

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

REM Configure with CMake
echo Configuring project...
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=%BUILD_TYPE%
if errorlevel 1 (
    echo Configuration failed!
    pause
    exit /b 1
)

REM Build the project
echo Building project...
cmake --build . --config %BUILD_TYPE%
if errorlevel 1 (
    echo Build failed!
    pause
    exit /b 1
)

REM Check if build was successful
if exist "catime.exe" (
    echo.
    echo Build completed successfully!
    echo Executable: %CD%\catime.exe
    
    REM Display file size
    for %%A in (catime.exe) do set SIZE=%%~zA
    if !SIZE! LSS 1024 (
        echo Size: !SIZE! B
    ) else if !SIZE! LSS 1048576 (
        set /a SIZE_KB=!SIZE!/1024
        echo Size: !SIZE_KB! KB
    ) else (
        set /a SIZE_MB=!SIZE!/1048576
        echo Size: !SIZE_MB! MB
    )
) else (
    echo Build failed - executable not found!
    pause
    exit /b 1
)

echo.
pause