# Quick Reference

Fast lookup guide for common commands, file locations, and MCP server operations.

---

## Table of Contents

- [Common Tasks](#common-tasks)
- [File Location Guide](#file-location-guide)
- [Build Commands](#build-commands)
- [Code Quality Commands](#code-quality-commands)
- [Coverage Workflow](#coverage-workflow)
- [Static Analysis Commands](#static-analysis-commands)

---

## Common Tasks

| Task | Command | Time |
|------|---------|------|
| **Run all tests** | `./build/Release/bin/tests/unit_tests && ./build/Release/bin/tests/integration_tests` | ~8s |
| **Run unit tests only** | `./build/Release/bin/tests/unit_tests` | ~6s |
| **Run integration tests only** | `./build/Release/bin/tests/integration_tests` | ~2s |
| **Format code** | `./scripts/format.sh` | ~2s |
| **Check coverage** | `./scripts/coverage.sh summary` | ~15s |
| **Generate coverage HTML** | `./scripts/coverage.sh html` | ~20s |
| **Static analysis (check)** | `./scripts/tidy.sh check` | ~12min |
| **Static analysis (fix)** | `./scripts/tidy.sh fix` | ~12min |
| **Build Release** | `cmake --build build/Release` | ~45s |
| **Build Debug** | `cmake --build build/Debug` | ~50s |
| **Clean build (Debug)** | `rm -rf build/Debug && cmake --preset conan-debug && cmake --build build/Debug` | ~1min |
| **Run main application** | `./build/Release/bin/sudoku` | instant |

---

## File Location Guide

### Core Interfaces (src/core/)

| Interface | Purpose | Implementation |
|-----------|---------|----------------|
| `i_game_validator.h` | Sudoku rule validation | `game_validator.{h,cpp}` |
| `i_puzzle_generator.h` | Puzzle generation | `puzzle_generator.{h,cpp}` |
| `i_save_manager.h` | Save/load persistence | `save_manager.{h,cpp}` |
| `i_statistics_manager.h` | Game statistics tracking | `statistics_manager.{h,cpp}` |
| `i_time_provider.h` | Time abstraction for testing | `time_provider.{h,cpp}`, `mock_time_provider.h` |
| `i_localization_manager.h` | UI string localization | `localization_manager.{h,cpp}` |

### Model Layer (Business Logic)

| Component | Path | Lines | Purpose |
|-----------|------|-------|---------|
| GameValidator | `src/core/game_validator.{h,cpp}` | 350 | Sudoku rule validation |
| PuzzleGenerator | `src/core/puzzle_generator.{h,cpp}` | 400 | Backtracking algorithm, difficulty-based generation |
| SaveManager | `src/core/save_manager.{h,cpp}` | 800 | YAML + zlib + ChaCha20-Poly1305 encryption |
| StatisticsManager | `src/core/statistics_manager.{h,cpp}` | 400 | Completion times, win streaks, per-difficulty stats |
| DIContainer | `src/core/di_container.{h,cpp}` | 200 | Dependency injection container |

### ViewModel Layer (Presentation Logic)

| Component | Path | Lines | Purpose |
|-----------|------|-------|---------|
| GameViewModel | `src/view_model/game_view_model.{h,cpp}` | 600 | Orchestrates services, exposes Observable<GameState> |

### View Layer (UI Rendering)

| Component | Path | Lines | Purpose |
|-----------|------|-------|---------|
| MainWindow | `src/view/main_window.{h,cpp}` | 800 | ImGui rendering, keyboard navigation |
| FontManager | `src/view/font_manager.{h,cpp}` | 150 | Font loading and management |
| ToastNotification | `src/view/toast_notification.{h,cpp}` | 100 | Notification system |

### Domain Models (src/model/)

| Component | Path | Lines | Purpose |
|-----------|------|-------|---------|
| GameState | `src/model/game_state.{h,cpp}` | 500 | Board state + undo/redo + timers |

### Tests

| Category | Path | Test Count | Purpose |
|----------|------|------------|---------|
| Unit Tests | `tests/unit/test_*.cpp` | 787 tests | Validator, generator, save, encryption, statistics, strategies |
| Integration Tests | `tests/integration/test_*_integration.cpp` | 9 cases | ViewModel, SaveManager, StatisticsManager |
| Test Helpers | `tests/helpers/test_utils.{h,cpp}` | N/A | TempTestDir, findEmptyCell(), board builders |
| Mock Objects | `tests/helpers/mock_*.h` | N/A | MockTimeProvider, MockGameValidator, etc. |

### Key Headers (include/)

| Header         | Purpose                                     |
|----------------|---------------------------------------------|
| `constants.h`  | Board/game constants, `valueToBit()` helper |
| `board_utils.h` | Board manipulation utilities               |

### Configuration & Build

| File | Purpose |
|------|---------|
| `CMakeLists.txt` | C++23 build configuration, flattened dirs |
| `conanfile.py` | Dependency management (SDL3, ImGui, Catch2, libsodium, yaml-cpp) |
| `.clang-format` | Code formatting rules |
| `.clang-tidy` | Static analysis configuration |
| `.gcovr.cfg` | Coverage reporting configuration |

---

## Build Commands

### CMake Presets

```bash
# Debug build (development, testing, debugging)
cmake --preset conan-debug

# Release build (performance measurement only)
cmake --preset conan-release
```

### Build Types

```bash
# Debug build (with symbols, assertions, -O0)
cmake --build build/Debug

# Release build (optimized, -O2)
cmake --build build/Release

# Specify target explicitly
cmake --build build/Debug --target sudoku
cmake --build build/Debug --target unit_tests
```

### Clean Build

```bash
# Remove build directory
rm -rf build/Debug

# Reconfigure
cmake --preset conan-debug

# Build
cmake --build build/Debug
```

### Reconfigure After Adding Files

```bash
# CMake GLOB doesn't auto-detect new files
cmake --preset conan-debug  # or conan-release
```

---

## Code Quality Commands

### Formatting

```bash
# Format all source files
./scripts/format.sh

# Check formatting without modifying files
clang-format --dry-run --Werror src/**/*.cpp include/**/*.h
```

### Pre-Commit Hook

```bash
# Install pre-commit hooks (one-time setup)
./scripts/setup-hooks.sh
```

---

## Coverage Workflow

### Generate Coverage Report

```bash
# Quick summary
./scripts/coverage.sh summary

# HTML report (opens in browser)
./scripts/coverage.sh html

# JSON report
./scripts/coverage.sh json
```

### Manual Coverage Commands

```bash
# 1. Build with coverage flags
cmake --build build/Debug

# 2. Run tests
./build/Debug/bin/tests/unit_tests
./build/Debug/bin/tests/integration_tests

# 3. Generate report
gcovr --config .gcovr.cfg --html-details coverage/index.html
```

### Coverage Targets

- **Line coverage:** ≥80% (current: 91.2%)
- **Function coverage:** ≥70% (current: 90.5%)
- **Branch coverage:** ≥55% (current: 59.8%)

---

## Static Analysis Commands

### clang-tidy

```bash
# Check all files (slow: ~12 minutes)
./scripts/tidy.sh check

# Auto-fix issues
./scripts/tidy.sh fix

# Check specific file
clang-tidy src/core/game_validator.cpp -p build/Debug
```

### Common Warnings to Fix

- Missing `[[nodiscard]]` on non-void returns
- Const-correctness issues
- Magic numbers (use named constants)
- Unused variables (use `[[maybe_unused]]` or remove)

---

## See Also

- [CODE_QUALITY.md](CODE_QUALITY.md) - Formatting, static analysis, CI/CD
- [TDD_GUIDE.md](TDD_GUIDE.md) - Test-driven development practices
- [DEBUGGING_GUIDE.md](DEBUGGING_GUIDE.md) - GDB, Valgrind, AddressSanitizer
- [TROUBLESHOOTING.md](TROUBLESHOOTING.md) - Common errors and solutions
- [ARCHITECTURE_PRINCIPLES.md](ARCHITECTURE_PRINCIPLES.md) - MVVM, SOLID, DIP
- [CPP_STYLE_GUIDE.md](CPP_STYLE_GUIDE.md) - C++23 coding standards
