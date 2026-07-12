@echo off
setlocal

set "BUILD_TYPE=%~1"
set "BUILD_JOBS=%~2"

cmake --build . --config "%BUILD_TYPE%" -j%BUILD_JOBS% >build.log 2>&1
set "BUILD_EXIT_CODE=%ERRORLEVEL%"

>build_exit_code.tmp echo %BUILD_EXIT_CODE%
>build_complete.tmp echo DONE

endlocal & exit /b %BUILD_EXIT_CODE%
