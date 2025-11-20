@echo off
setlocal enabledelayedexpansion

REM Catime CMake Build Script for Windows
REM This script builds the Catime project using CMake and MinGW
REM Note: Output architecture (32-bit/64-bit) depends on your installed MinGW version

REM Configuration
set BUILD_TYPE=%1
if "%BUILD_TYPE%"=="" set BUILD_TYPE=Release
set BUILD_DIR=build

REM Record start time
set START_TIME=%TIME%

REM Display elegant gradient logo using PowerShell
chcp 65001 >nul
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
"[Console]::OutputEncoding = [System.Text.Encoding]::UTF8; ^
Write-Host ''; ^
Write-Host ''; ^
$e=[char]27; ^
Write-Host \"${e}[1m${e}[38;2;138;43;226m██████╗ ${e}[38;2;147;112;219m █████╗ ${e}[38;2;153;102;255m████████╗${e}[38;2;160;120;255m██╗${e}[38;2;186;85;211m███╗   ███╗${e}[38;2;221;160;221m███████╗${e}[0m\"; ^
Write-Host \"${e}[1m${e}[38;2;138;43;226m██╔════╝${e}[38;2;147;112;219m ██╔══██╗${e}[38;2;153;102;255m╚══██╔══╝${e}[38;2;160;120;255m██║${e}[38;2;186;85;211m████╗ ████║${e}[38;2;221;160;221m██╔════╝${e}[0m\"; ^
Write-Host \"${e}[1m${e}[38;2;138;43;226m██║     ${e}[38;2;147;112;219m ███████║${e}[38;2;153;102;255m   ██║   ${e}[38;2;160;120;255m██║${e}[38;2;186;85;211m██╔████╔██║${e}[38;2;221;160;221m█████╗  ${e}[0m\"; ^
Write-Host \"${e}[1m${e}[38;2;138;43;226m██║     ${e}[38;2;147;112;219m ██╔══██║${e}[38;2;153;102;255m   ██║   ${e}[38;2;160;120;255m██║${e}[38;2;186;85;211m██║╚██╔╝██║${e}[38;2;221;160;221m██╔══╝  ${e}[0m\"; ^
Write-Host \"${e}[1m${e}[38;2;138;43;226m╚██████╗${e}[38;2;147;112;219m ██║  ██║${e}[38;2;153;102;255m   ██║   ${e}[38;2;160;120;255m██║${e}[38;2;186;85;211m██║ ╚═╝ ██║${e}[38;2;221;160;221m███████╗${e}[0m\"; ^
Write-Host \"${e}[1m${e}[38;2;138;43;226m ╚═════╝${e}[38;2;147;112;219m ╚═╝  ╚═╝${e}[38;2;153;102;255m   ╚═╝   ${e}[38;2;160;120;255m╚═╝${e}[38;2;186;85;211m╚═╝     ╚═╝${e}[38;2;221;160;221m╚══════╝${e}[0m\"; ^
Write-Host ''"

REM Check if CMake is available
cmake --version >nul 2>&1
if errorlevel 1 (
    echo Error: CMake not found! Please install CMake and add it to PATH.
    pause
    exit /b 1
)

REM Check if MinGW is available and detect architecture
gcc --version >nul 2>&1
if errorlevel 1 (
    echo Error: MinGW GCC not found! Please install MinGW and add it to PATH.
    echo.
    echo For 32-bit builds ^(recommended for maximum compatibility^):
    echo   Download MinGW-w64 i686 version from:
    echo   https://github.com/niXman/mingw-builds-binaries/releases
    echo   Look for: i686-*-release-win32-*.7z
    pause
    exit /b 1
)

REM Detect compiler architecture
gcc -dumpmachine > arch_detect.tmp 2>&1
set /p COMPILER_ARCH=<arch_detect.tmp
del arch_detect.tmp

REM Determine if it's 32-bit or 64-bit
echo %COMPILER_ARCH% | findstr /i "i686 i386" >nul
if not errorlevel 1 (
    set ARCH_TYPE=32-bit
    set ARCH_COLOR=Green
) else (
    set ARCH_TYPE=64-bit
    set ARCH_COLOR=Yellow
)

echo Build configuration:
echo   Compiler: %COMPILER_ARCH%
echo   Target: %ARCH_TYPE%
echo   Build type: %BUILD_TYPE%
echo.

REM Create build directory
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
cd "%BUILD_DIR%"

REM Check for stale CMake cache (different source path)
if exist "CMakeCache.txt" (
    findstr /C:"CMAKE_HOME_DIRECTORY" CMakeCache.txt >nul 2>&1
    if not errorlevel 1 (
        for /f "tokens=2 delims==" %%i in ('findstr /C:"CMAKE_HOME_DIRECTORY" CMakeCache.txt') do (
            set CACHED_PATH=%%i
        )
        REM Compare cached path with current path (case-insensitive)
        echo !CACHED_PATH! | findstr /i /C:"%CD%\.." >nul
        if errorlevel 1 (
            echo.
            echo Detected stale CMake cache from different build environment
            echo Cleaning build directory...
            cd ..
            rmdir /s /q "%BUILD_DIR%" 2>nul
            mkdir "%BUILD_DIR%"
            cd "%BUILD_DIR%"
            echo Cache cleaned successfully
            echo.
        )
    )
)

REM Step 1: Configure with progress bar
powershell -NoProfile -Command "$e=[char]27; Write-Host -NoNewline \"${e}[1m${e}[38;2;147;112;219m[ 25%%]${e}[0m ${e}[38;2;138;43;226m##########${e}[38;2;80;80;80m..............................${e}[0m ${e}[38;2;100;200;255mConfiguring project...${e}[0m\""
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=%BUILD_TYPE% >cmake_config.log 2>&1
if errorlevel 1 (
    echo.
    powershell -NoProfile -Command "$e=[char]27; Write-Host \"${e}[91m✗ Configuration failed!${e}[0m\""
    echo Check cmake_config.log for details
    pause
    exit /b 1
)
powershell -NoProfile -Command "$e=[char]27; Write-Host \"`r${e}[1m${e}[38;2;147;112;219m[100%%]${e}[0m ${e}[38;2;138;43;226m########################################${e}[0m ${e}[38;2;100;200;255mConfiguring project... ✓${e}[0m\""

REM Step 2: Analyze and count source files
powershell -NoProfile -Command "$e=[char]27; Write-Host -NoNewline \"${e}[1m${e}[38;2;147;112;219m[ 50%%]${e}[0m ${e}[38;2;138;43;226m####################${e}[38;2;80;80;80m....................${e}[0m ${e}[38;2;255;200;100mAnalyzing source files...${e}[0m\""

REM Count total source files
for /f %%a in ('dir /s /b ..\src\*.c 2^>nul ^| find /c /v ""') do set TOTAL_FILES=%%a
set /a TOTAL_FILES=%TOTAL_FILES%+3
powershell -NoProfile -Command "$e=[char]27; Write-Host \"`r${e}[1m${e}[38;2;147;112;219m[100%%]${e}[0m ${e}[38;2;138;43;226m########################################${e}[0m ${e}[38;2;255;200;100mAnalyzing source files... ✓${e}[0m\""

REM Step 3: Build with real-time progress monitoring
REM Clean up any previous build completion marker
if exist "build_complete.tmp" del build_complete.tmp

REM Start build in background
start /b "" cmd /c "cmake --build . --config %BUILD_TYPE% -j8 >build.log 2>&1 & echo DONE >build_complete.tmp"

REM Start progress monitor
REM Get absolute path to the script
set SCRIPT_PATH=%CD%\..\build_monitor.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_PATH%" -BuildDir "." -TotalFiles %TOTAL_FILES%

REM Check build result
if exist "catime.exe" (
    del build_complete.tmp 2>nul
) else (
    echo.
    powershell -NoProfile -Command "$e=[char]27; Write-Host \"${e}[91m✗ Build failed!${e}[0m\""
    echo Check build.log for details
    del build_complete.tmp 2>nul
    pause
    exit /b 1
)

REM Step 4: Finalize with progress bar
powershell -NoProfile -Command "$e=[char]27; Write-Host -NoNewline \"${e}[1m${e}[38;2;147;112;219m[100%%]${e}[0m ${e}[38;2;138;43;226m########################################${e}[0m ${e}[38;2;100;255;150mFinalizing build...${e}[0m\""
timeout /t 1 /nobreak >nul
powershell -NoProfile -Command "$e=[char]27; Write-Host \"`r${e}[1m${e}[38;2;147;112;219m[100%%]${e}[0m ${e}[38;2;138;43;226m########################################${e}[0m ${e}[38;2;100;255;150mFinalizing build... ✓${e}[0m\""

REM Check if build was successful
if exist "catime.exe" (
    REM Calculate elapsed time
    set END_TIME=%TIME%
    
    REM Get file size
    for %%A in (catime.exe) do set SIZE=%%~zA
    set /a SIZE_KB=!SIZE!/1024
    
    echo.
    powershell -NoProfile -Command "$e=[char]27; Write-Host \"${e}[92m✓ Build completed successfully!${e}[0m\""
    
    REM Calculate time difference (simplified - shows end time)
    powershell -NoProfile -Command "$e=[char]27; Write-Host \"${e}[95mBuild time: %START_TIME% - %END_TIME%${e}[0m\""
    powershell -NoProfile -Command "$e=[char]27; Write-Host \"${e}[96mSize: !SIZE_KB! KB${e}[0m\""
    powershell -NoProfile -Command "$e=[char]27; Write-Host \"${e}[96mOutput: %CD%\catime.exe${e}[0m\""
    
    REM Keep log files for debugging
    powershell -NoProfile -Command "$e=[char]27; Write-Host \"${e}[96mBuild logs saved to: %BUILD_DIR%\${e}[0m\""
) else (
    echo.
    powershell -NoProfile -Command "$e=[char]27; Write-Host \"${e}[91m✗ Build failed - executable not found!${e}[0m\""
    echo Check build.log for details
    pause
    exit /b 1
)

echo.
pause