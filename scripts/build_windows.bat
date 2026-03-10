@echo off
REM Build script for Windows with MSVC (Release)
REM
REM Prerequisites:
REM   - Visual Studio 2022/2026 with "Desktop development with C++" workload
REM   - Qt6 installed via Qt Online Installer (msvc2022_64 kit)
REM   - Conan 2 and CMake available on PATH
REM
REM Qt6 discovery:
REM   Set the QT6_DIR environment variable to your Qt6 MSVC kit directory, e.g.:
REM     set QT6_DIR=C:\Qt\6.10.2\msvc2022_64
REM   If not set, the script will attempt common Qt installer locations.

REM Self-logging: if not already redirected, re-invoke with output to build_log.txt
if not defined BUILD_LOGGING (
    set BUILD_LOGGING=1
    cmd /c "%~f0" %* > "%~dp0build_log.txt" 2>&1
    exit /b %ERRORLEVEL%
)

setlocal EnableDelayedExpansion

echo ==================================================
echo Building sudoku-cpp for Windows (Release)
echo ==================================================

REM ---------------------------------------------------------------------------
REM 1. Locate Visual Studio via vswhere (includes Insiders/Preview releases)
REM ---------------------------------------------------------------------------
set "VS_INSTALLER=%SystemDrive%\Program Files (x86)\Microsoft Visual Studio\Installer"
if exist "%VS_INSTALLER%\vswhere.exe" goto :vswhere_ok
echo ERROR: vswhere.exe not found. Install Visual Studio Installer.
exit /b 1
:vswhere_ok
REM Add installer dir to PATH so vswhere can be called without path (avoids (x86) in for/f backtick)
set "PATH=%VS_INSTALLER%;%PATH%"

for /f "usebackq tokens=*" %%i in (`vswhere -latest -prerelease -products * -property installationPath`) do set "VS_PATH=%%i"
if defined VS_PATH goto :vs_found
echo ERROR: No Visual Studio installation found (including previews).
exit /b 1
:vs_found
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
    for %%v in (6.10.2 6.10.1 6.10.0 6.9.1 6.9.0 6.8.0) do (
        if exist "C:\Qt\%%v\msvc2022_64\lib\cmake\Qt6" (
            set "QT6_DIR=C:\Qt\%%v\msvc2022_64"
            echo Auto-detected Qt6: !QT6_DIR!
            goto :qt6_found
        )
    )
    echo ERROR: QT6_DIR not set and Qt6 not found in common locations.
    echo        Set QT6_DIR=C:\Qt\^<version^>\msvc2022_64 and retry.
    exit /b 1
)
:qt6_found
echo Using Qt6: %QT6_DIR%

REM ---------------------------------------------------------------------------
REM 4. Install Conan dependencies
REM ---------------------------------------------------------------------------
echo [1/3] Installing dependencies with Conan...
conan install . --build=missing --output-folder=build\Release
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Conan dependency installation failed
    exit /b %ERRORLEVEL%
)

REM ---------------------------------------------------------------------------
REM 5. Configure CMake (apply Conan toolchain + Qt6 prefix path)
REM ---------------------------------------------------------------------------
echo [2/3] Configuring CMake...
call build\Release\generators\conanbuild.bat
cmake -S . -B build\Release -DCMAKE_TOOLCHAIN_FILE=build\Release\generators\conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=%QT6_DIR%"
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: CMake configuration failed
    exit /b %ERRORLEVEL%
)

REM ---------------------------------------------------------------------------
REM 6. Build
REM ---------------------------------------------------------------------------
echo [3/3] Building project...
cmake --build build\Release --config Release
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Build failed
    exit /b %ERRORLEVEL%
)

echo ==================================================
echo Build successful!
echo Executable: build\Release\bin\sudoku.exe
echo ==================================================
