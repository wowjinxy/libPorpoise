@echo off
REM Build script for libPorpoise on Windows

setlocal

set BUILD_TYPE=%1
if "%BUILD_TYPE%"=="" set BUILD_TYPE=Release

set BUILD_DIR=build

echo Building libPorpoise...
echo Build type: %BUILD_TYPE%

REM Create build directory
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
cd "%BUILD_DIR%"

REM Configure
cmake .. -DCMAKE_BUILD_TYPE=%BUILD_TYPE%
if errorlevel 1 goto :error

REM Build
cmake --build . --config %BUILD_TYPE%
if errorlevel 1 goto :error

echo.
echo Build complete!
echo Binaries are in: %BUILD_DIR%\bin
echo Libraries are in: %BUILD_DIR%\lib
echo.
echo Run examples:
echo   .\bin\%BUILD_TYPE%\simple_example.exe
echo   .\bin\%BUILD_TYPE%\thread_example.exe

goto :end

:error
echo.
echo Build failed!
exit /b 1

:end
endlocal

