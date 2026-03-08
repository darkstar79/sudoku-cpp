@echo off
REM Build script for Windows with MSVC (Debug)
REM
REM Prerequisites:
REM   - Visual Studio 2022/2026 with "Desktop development with C++" workload
REM   - Qt6 installed via Qt Online Installer (msvc2022_64 kit)
REM   - Conan 2 and CMake available on PATH
REM
REM Qt6 discovery:
REM   Set the QT6_DIR environment variable to your Qt6 MSVC kit directory, e.g.:
REM     set QT6_DIR=C:\Qt\6.9.0\msvc2022_64
REM   If not set, the script will attempt common Qt installer locations.

setlocal EnableDelayedExpansion

echo ==================================================
echo Building sudoku-cpp for Windows (Debug)
echo ==================================================

REM ---------------------------------------------------------------------------
REM 1. Locate Visual Studio via vswhere (includes Insiders/Preview releases)
REM ---------------------------------------------------------------------------
set VSWHERE="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist %VSWHERE% (
    echo ERROR: vswhere.exe not found. Install Visual Studio Installer.
    exit /b 1
)

for /f "usebackq tokens=*" %%i in (`%VSWHERE% -latest -prerelease -property installationPath`) do (
    set VS_PATH=%%i
)
if not defined VS_PATH (
    echo ERROR: No Visual Studio installation found (including previews).
    exit /b 1
)
echo Found Visual Studio: %VS_PATH%

call "%VS_PATH%\VC\Auxiliary\Build\vcvarsall.bat" x64
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to initialize Visual Studio environment
    exit /b %ERRORLEVEL%
)

REM ---------------------------------------------------------------------------
REM 2. Locate Conan
REM ---------------------------------------------------------------------------
where conan >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: conan not found on PATH. Install with: pip install conan
    exit /b 1
)

REM ---------------------------------------------------------------------------
REM 3. Resolve Qt6 path
REM ---------------------------------------------------------------------------
if not defined QT6_DIR (
    REM Try common Qt installer locations (adjust version as needed)
    for %%v in (6.10.0 6.9.1 6.9.0 6.8.0 6.7.3 6.7.0) do (
        if exist "C:\Qt\%%v\msvc2022_64\lib\cmake\Qt6" (
            set QT6_DIR=C:\Qt\%%v\msvc2022_64
            echo Auto-detected Qt6: !QT6_DIR!
            goto :qt6_found
        )
    )
    echo WARNING: QT6_DIR not set and Qt6 not found in common locations.
    echo          Set QT6_DIR=C:\Qt\<version>\msvc2022_64 and retry.
    exit /b 1
)
:qt6_found
echo Using Qt6: %QT6_DIR%

REM ---------------------------------------------------------------------------
REM 4. Install Conan dependencies (Debug)
REM ---------------------------------------------------------------------------
echo [1/3] Installing dependencies with Conan (Debug)...
conan install . --build=missing --output-folder=build\Debug -s build_type=Debug
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Conan dependency installation failed
    exit /b %ERRORLEVEL%
)

REM ---------------------------------------------------------------------------
REM 5. Configure CMake
REM ---------------------------------------------------------------------------
echo [2/3] Configuring CMake (Debug)...
call build\Debug\generators\conanbuild.bat
cmake -S . -B build\Debug ^
    -DCMAKE_TOOLCHAIN_FILE=build\Debug\generators\conan_toolchain.cmake ^
    -DCMAKE_BUILD_TYPE=Debug ^
    -DCMAKE_PREFIX_PATH="%QT6_DIR%"
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: CMake configuration failed
    exit /b %ERRORLEVEL%
)

REM ---------------------------------------------------------------------------
REM 6. Build
REM ---------------------------------------------------------------------------
echo [3/3] Building project (Debug)...
cmake --build build\Debug --config Debug
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Build failed
    exit /b %ERRORLEVEL%
)

echo ==================================================
echo Debug build successful!
echo Executable: build\Debug\bin\sudoku.exe
echo ==================================================
