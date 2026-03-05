@echo off
REM Build script for Windows Debug configuration

echo ==================================================
echo Building sudoku-cpp for Windows (Debug)
echo ==================================================

REM Step 1: Install Conan dependencies
echo [1/3] Installing dependencies with Conan (Debug)...
"C:\Users\Alexa\AppData\Local\Python\pythoncore-3.14-64\Scripts\conan.exe" install . --profile=debug --build=missing --output-folder=build\Debug
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Conan dependency installation failed
    exit /b %ERRORLEVEL%
)

REM Step 2: Configure CMake
echo [2/3] Configuring CMake (Debug)...
cmake --preset conan-debug
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: CMake configuration failed
    exit /b %ERRORLEVEL%
)

REM Step 3: Build project
echo [3/3] Building project (Debug)...
cmake --build --preset conan-debug
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Build failed
    exit /b %ERRORLEVEL%
)

echo ==================================================
echo Debug build successful!
echo Executable: build\Debug\bin\sudoku.exe
echo ==================================================
