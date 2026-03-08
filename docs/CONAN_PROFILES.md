# Conan Profiles Guide

This document describes the available Conan profiles for building the sudoku-cpp project with different compilers and build configurations.

## Available Profiles

### Windows / MSVC Profiles

- **msvc-release**: MSVC with Release build (optimized, no debug symbols)
- **msvc-debug**: MSVC with Debug build (no optimization, full debug symbols)

> **Prerequisite:** Install Qt6 via the [Qt Online Installer](https://www.qt.io/download-qt-installer).
> Select the **MSVC 2022 64-bit** component for your Qt6 version.
> Then set `QT6_DIR` before building:
> ```bat
> set QT6_DIR=C:\Qt\6.10.0\msvc2022_64
> ```

#### Auto-generate an MSVC profile (recommended)

Run this once after installing Visual Studio to create a profile matching your installed compiler:

```bat
conan profile detect --name msvc-release
```

Conan will detect the MSVC version automatically. Verify it looks correct:

```bat
conan profile show --profile msvc-release
```

#### Manual profile content

If you prefer to create profiles manually, place these files in `%USERPROFILE%\.conan2\profiles\`:

**`msvc-release`:**
```ini
[settings]
arch=x86_64
build_type=Release
compiler=msvc
compiler.cppstd=23
compiler.runtime=dynamic
compiler.runtime_type=Release
compiler.version=194
os=Windows

[conf]
tools.cmake.cmaketoolchain:generator=Ninja
```

**`msvc-debug`:**
```ini
[settings]
arch=x86_64
build_type=Debug
compiler=msvc
compiler.cppstd=23
compiler.runtime=dynamic
compiler.runtime_type=Debug
compiler.version=194
os=Windows

[conf]
tools.cmake.cmaketoolchain:generator=Ninja
```

> **Note on `compiler.version`:** Conan uses the MSVC cl.exe major version prefix.
> VS2022 → `193`, VS2026 Preview (v18, cl.exe 19.4x) → `194`.
> Use `conan profile detect` to let Conan pick the right value automatically.
> `compiler.runtime=dynamic` uses `/MD`/`/MDd` (dynamic CRT), which is required
> to link against Qt6 shared libraries from the Qt installer.

### GCC 15 Profiles

- **gcc-15-release**: GCC 15 with Release build (optimized, no debug symbols)
- **gcc-15-debug**: GCC 15 with Debug build (no optimization, full debug symbols)
- **gcc-15-relwithdebinfo**: GCC 15 with RelWithDebInfo build (optimized with debug symbols)

### Clang 21 Profiles

- **clang-21-release**: Clang 21 with Release build (optimized, no debug symbols)
- **clang-21-debug**: Clang 21 with Debug build (no optimization, full debug symbols)
- **clang-21-relwithdebinfo**: Clang 21 with RelWithDebInfo build (optimized with debug symbols)

**Note:** Clang profiles use `compiler.version=20` in Conan settings (latest supported by Conan), but work with Clang 21.x installed on the system.

## Profile Configurations

### Common Settings (All Profiles)
- **Architecture:** x86_64
- **C++ Standard:** gnu23 (C++23 with GNU extensions)
- **Standard Library:** libstdc++11
- **Operating System:** Linux
- **CMake Generator:** Ninja

### Build Type Specific Settings

#### Release Profiles
- **Optimization:** Default compiler optimization (typically `-O3`)
- **Debug Symbols:** None
- **Use Case:** Production builds, performance testing

#### Debug Profiles
- **Optimization:** `-O0` (no optimization)
- **Debug Symbols:** `-g` (full debug information)
- **Frame Pointers:** `-fno-omit-frame-pointer` (complete stack traces)
- **Use Case:** Development, debugging with GDB, sanitizer testing

#### RelWithDebInfo Profiles
- **Optimization:** `-O2` (moderate optimization)
- **Debug Symbols:** `-g` (full debug information)
- **Frame Pointers:** `-fno-omit-frame-pointer` (complete stack traces)
- **Use Case:** Performance profiling, production debugging

## Usage Examples

### Initial Setup (One-Time Per Profile)

Install dependencies for a specific profile:

```bash
# GCC Debug
conan install . --profile=gcc-15-debug --build=missing --output-folder=build/GCC-Debug

# Clang Release
conan install . --profile=clang-21-release --build=missing --output-folder=build/Clang-Release

# GCC RelWithDebInfo
conan install . --profile=gcc-15-relwithdebinfo --build=missing --output-folder=build/GCC-RelWithDebInfo
```

### Building with CMake

After installing dependencies, configure and build:

```bash
# GCC Debug Build
cmake --preset conan-debug
cmake --build --preset conan-debug

# Using specific build directory for Clang
cmake -S . -B build/Clang-Release -DCMAKE_TOOLCHAIN_FILE=build/Clang-Release/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build/Clang-Release
```

### Recommended Workflows

#### Development Workflow (GCC Debug)
```bash
# Initial setup
conan install . --profile=gcc-15-debug --build=missing --output-folder=build/Debug
cmake --preset conan-debug

# Iterative development
cmake --build build/Debug
./build/Debug/bin/tests/unit_tests
```

#### Cross-Compiler Testing (GCC + Clang)
```bash
# Build with both compilers to catch compiler-specific issues
conan install . --profile=gcc-15-release --build=missing --output-folder=build/GCC-Release
conan install . --profile=clang-21-release --build=missing --output-folder=build/Clang-Release

cmake -S . -B build/GCC-Release -DCMAKE_TOOLCHAIN_FILE=build/GCC-Release/conan_toolchain.cmake
cmake --build build/GCC-Release

cmake -S . -B build/Clang-Release -DCMAKE_TOOLCHAIN_FILE=build/Clang-Release/conan_toolchain.cmake
cmake --build build/Clang-Release

# Run tests with both
./build/GCC-Release/bin/tests/unit_tests
./build/Clang-Release/bin/tests/unit_tests
```

#### Performance Profiling (RelWithDebInfo)
```bash
# Build with debug symbols but optimized
conan install . --profile=gcc-15-relwithdebinfo --build=missing --output-folder=build/Profile
cmake -S . -B build/Profile -DCMAKE_TOOLCHAIN_FILE=build/Profile/conan_toolchain.cmake
cmake --build build/Profile

# Profile with perf/valgrind
perf record -g ./build/Profile/bin/tests/unit_tests
perf report
```

## Profile Locations

All profiles are stored in: `~/.conan2/profiles/`

To view a profile:
```bash
conan profile show --profile gcc-15-debug
```

To list all available profiles:
```bash
ls -1 ~/.conan2/profiles/
```

## Updating Profiles

Profiles are plain text files and can be edited directly:

```bash
vim ~/.conan2/profiles/gcc-15-debug
```

Common modifications:
- Add sanitizer flags: `tools.build:cxxflags=["-g", "-O0", "-fsanitize=address"]`
- Enable warnings: `tools.build:cxxflags=["-Wall", "-Wextra", "-Werror"]`
- Change optimization level: `-O0`, `-O1`, `-O2`, `-O3`, `-Os`

## Troubleshooting

### "Invalid setting" Error for Clang Version
If you see an error about Clang version 21 being invalid, this is expected. Conan's settings only support up to version 20 for Clang. The profiles use `compiler.version=20` which is compatible with Clang 21.x installed on your system.

### Cache Invalidation
When switching between compilers, Conan may need to rebuild dependencies:
```bash
conan install . --profile=clang-21-debug --build=missing --output-folder=build/Clang-Debug
```

The `--build=missing` flag ensures dependencies are rebuilt for the new compiler if not in cache.

### Clean Build
To force a complete rebuild of dependencies:
```bash
conan remove "*" -c  # Clear cache (use with caution)
conan install . --profile=gcc-15-release --build=missing --output-folder=build/GCC-Release
```

## Migration from Old Profiles

The old generic profiles (`default`, `debug`, `relwithdebinfo`) can still be used but are not compiler-versioned:

```bash
# Old style (still works)
conan install . --profile=debug --build=missing --output-folder=build/Debug

# New style (recommended)
conan install . --profile=gcc-15-debug --build=missing --output-folder=build/GCC-Debug
```

The new profiles provide better traceability and allow mixing multiple compiler versions.
