@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "BUILD_TYPE=%~1"
set "BUILD_JOBS=%~2"

cmake --build . --target catime --config "%BUILD_TYPE%" -j%BUILD_JOBS% >build.log 2>&1
set "BUILD_EXIT_CODE=!ERRORLEVEL!"

if not "!BUILD_EXIT_CODE!"=="0" if not "%BUILD_JOBS%"=="1" (
    findstr /i /c:"out of memory" /c:"cannot allocate memory" /c:"virtual memory exhausted" build.log >nul
    if not errorlevel 1 (
        >>build.log echo.
        >>build.log echo Parallel build exhausted available memory; retrying catime with one job.
        cmake --build . --target catime --config "%BUILD_TYPE%" -j1 >>build.log 2>&1
        set "BUILD_EXIT_CODE=!ERRORLEVEL!"
    )
)

>build_exit_code.tmp echo !BUILD_EXIT_CODE!
>build_complete.tmp echo DONE

endlocal & exit /b %BUILD_EXIT_CODE%
