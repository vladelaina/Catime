@echo off
setlocal enabledelayedexpansion

REM Helper function for colored output using PowerShell
set "ps_color=powershell -Command"

REM Catime CMake Build Script for Windows
REM This script builds the Catime project using CMake and MinGW-64

REM Configuration
set BUILD_TYPE=%1
if "%BUILD_TYPE%"=="" set BUILD_TYPE=Release
set BUILD_DIR=build

echo.
echo            _   _                
echo   ___ __ _^| ^|_^(_^)_ __ ___   ___ 
echo  / __/ _` ^| __^| ^| '_ ` _ \ / _ \
echo ^| ^(_^| ^(_^| ^| ^|_^| ^| ^| ^| ^| ^| ^|  __/
echo  \___\__,_^\^|__^|_^|_^| ^|_^| ^|_^|\_^|
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

REM Step 1: Configure with colored text
%ps_color% "Write-Host '[25%]' -ForegroundColor Magenta -NoNewline; Write-Host ' Configuring project...' -ForegroundColor Cyan"
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=%BUILD_TYPE% >cmake_config.log 2>&1
if errorlevel 1 (
    %ps_color% "Write-Host 'Configuration failed!' -ForegroundColor Red"
    echo Check cmake_config.log for details
    pause
    exit /b 1
)

REM Step 2: Analyze with colored text
%ps_color% "Write-Host '[50%]' -ForegroundColor Magenta -NoNewline; Write-Host ' Analyzing source files...' -ForegroundColor Yellow"
timeout /t 1 /nobreak >nul

REM Step 3: Build with colored text
%ps_color% "Write-Host '[75%]' -ForegroundColor Magenta -NoNewline; Write-Host ' Compiling source files...' -ForegroundColor Red"
cmake --build . --config %BUILD_TYPE% >build.log 2>&1
if errorlevel 1 (
    %ps_color% "Write-Host 'Build failed!' -ForegroundColor Red"
    echo Check build.log for details
    pause
    exit /b 1
)

REM Step 4: Finalize with colored text
%ps_color% "Write-Host '[100%]' -ForegroundColor Magenta -NoNewline; Write-Host ' Finalizing build...' -ForegroundColor Green"
script
REM Check if build was successful
if exist "catime.exe" (
    echo.
    %ps_color% "Write-Host '[OK] Build completed successfully!' -ForegroundColor Green"
    %ps_color% "Write-Host 'Output: %CD%\catime.exe' -ForegroundColor Cyan"
    
    REM Display file size with nice formatting
    for %%A in (catime.exe) do set SIZE=%%~zA
    if !SIZE! LSS 1024 (
        %ps_color% "Write-Host 'Size: !SIZE! B' -ForegroundColor Cyan"
    ) else if !SIZE! LSS 1048576 (
        set /a SIZE_KB=!SIZE!/1024
        %ps_color% "Write-Host 'Size: !SIZE_KB! KB' -ForegroundColor Cyan"
    ) else (
        set /a SIZE_MB=!SIZE!/1048576
        %ps_color% "Write-Host 'Size: !SIZE_MB! MB' -ForegroundColor Cyan"
    )
    
    REM Clean up log files
    del cmake_config.log build.log 2>nul
) else (
    %ps_color% "Write-Host '[ERROR] Build failed - executable not found!' -ForegroundColor Red"
    echo Check build.log for details
    pause
    exit /b 1
)

echo.
pause