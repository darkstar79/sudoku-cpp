@echo off
REM Build script for Windows with MSVC

echo ==================================================
echo Building sudoku-cpp for Windows (Release)
echo ==================================================

REM Initialize Visual Studio environment with v145 toolset
echo Setting up Visual Studio environment with v145 toolset...
call "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvarsall.bat" x64 -vcvars_ver=14.5
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to initialize Visual Studio environment
    exit /b %ERRORLEVEL%
)

REM Step 1: Install Conan dependencies
echo [1/3] Installing dependencies with Conan...
"C:\Users\Alexa\AppData\Local\Python\pythoncore-3.14-64\Scripts\conan.exe" install . --profile=default --build=missing --output-folder=build\Release
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Conan dependency installation failed
    exit /b %ERRORLEVEL%
)

REM Step 2: Configure CMake (use Conan-provided CMake)
echo [2/3] Configuring CMake...
call build\Release\conanbuild.bat
cmake --preset conan-release
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: CMake configuration failed
    exit /b %ERRORLEVEL%
)

REM Step 3: Build project
echo [3/3] Building project...
cmake --build --preset conan-release
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Build failed
    exit /b %ERRORLEVEL%
)

echo ==================================================
echo Build successful!
echo Executable: build\Release\bin\sudoku.exe
echo ==================================================
