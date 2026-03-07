# Troubleshooting Guide

This document provides solutions to common errors, build issues, and agent-specific failure modes encountered during development.

---

## Table of Contents

- [Common Errors](#common-errors)
- [CMake Configuration Issues](#cmake-configuration-issues)
- [Build Type Mismatches](#build-type-mismatches)
- [Conan Dependency Errors](#conan-dependency-errors)
- [Coverage Configuration Issues](#coverage-configuration-issues)
- [Test Failures](#test-failures)
- [SaveManager Encryption](#savemanager-encryption)
- [Agent-Specific Failure Modes](#agent-specific-failure-modes)
- [Diagnostic Commands](#diagnostic-commands)
- [Recovery Workflows](#recovery-workflows)

---

## Common Errors

| Error | Cause | Solution |
|-------|-------|----------|
| Test failures with timing issues | Using Release build for tests | Switch to Debug: `cmake --build build/Debug` |
| Coverage drop | New code without tests | Run `./scripts/coverage.sh html` to identify gaps |
| `cannot find -lsodium` | Conan cache corrupted | `conan remove "*" -c && conan install` |
| Linker errors after dependency update | Stale CMake cache | `rm -rf build && cmake --preset conan-release` |
| `No such file or directory` for headers | New files not detected by CMake | Reconfigure: `cmake --preset conan-debug` |
| Compilation errors after adding new files | GLOB didn't pick up new files | Reconfigure: `cmake --preset conan-debug` |
| `undefined reference` to new function | New .cpp file not compiled | Reconfigure: `cmake --preset conan-debug` |
| Test executable crashes on launch | Debug/Release mismatch in dependencies | Clean rebuild: `rm -rf build && cmake --preset conan-debug` |
| gcovr errors: `boolean format` | Wrong gcovr config format | Check `gcovr --help` for your version's syntax |
| Coverage missing for header-only code | Coverage flags only on source targets | Add `--coverage` to test executables too |
| Format check fails in CI | Local formatting not applied | Run `./scripts/format.sh` before commit |
| clang-tidy warnings | Code doesn't meet quality standards | Run `./scripts/tidy.sh check` and fix warnings |
| `PuzzleRater` returns `Unsolvable` on valid puzzles | Memoization cache storing truncated counts | Guard cache insertions with `if (count < max_solutions)` — see [MEMOIZATION_IMPLEMENTATION_SUMMARY.md](MEMOIZATION_IMPLEMENTATION_SUMMARY.md#bug-fix-cache-truncation-february-2026) |
| Unique Rectangle makes wrong eliminations | Puzzle has multiple solutions (uniqueness check bug) | Verify `hasUniqueSolution()` with independent brute-force counter |

---

## CMake Configuration Issues

### 1. GLOB-Based Source Lists Don't Auto-Detect New Files

**Problem:** CMake's `file(GLOB ...)` caches file lists. Adding/removing source files doesn't trigger rebuild.

**Symptoms:**
- New .cpp files not compiled
- Linker errors: `undefined reference to 'MyNewClass::myMethod()'`
- Header files not found: `No such file or directory`

**Solution:** Explicitly reconfigure CMake after adding/removing files:
```bash
cmake --preset conan-release  # or conan-debug
```

**Why this happens:** CMake runs `file(GLOB ...)` during configuration, not during build. Adding files doesn't invalidate the cached file list.

---

### 2. Stale CMake Cache After Configuration Changes

**Problem:** Changes to CMakeLists.txt or conanfile.py not reflected in build.

**Symptoms:**
- Compiler flags not updated
- Dependencies not linked
- Build type (Debug/Release) unchanged

**Solution:** Clean rebuild to force reconfiguration:
```bash
rm -rf build/Debug
cmake --preset conan-debug
cmake --build build/Debug
```

**When to clean rebuild:**
- After modifying CMakeLists.txt
- After updating Conan dependencies
- After changing build flags or options
- When in doubt about cache state

---

## Build Type Mismatches

### Problem: Mixing Debug and Release Builds

**Symptoms:**
- Segfaults in seemingly correct code
- Tests pass locally but fail in CI (or vice versa)
- Undefined behavior with optimizations enabled

**Cause:** Debug and Release builds have different:
- Optimization levels (-O0 vs -O2)
- Debug symbols (present vs stripped)
- Assertions (enabled vs disabled)
- Dependency builds (Conan profile mismatch)

**Solution:** Always specify build type consistently:

**For Development and Testing:**
```bash
cmake --preset conan-debug
cmake --build build/Debug
./build/Debug/bin/tests/unit_tests
```

**For Performance Measurements ONLY:**
```bash
cmake --preset conan-release
cmake --build build/Release
./build/Release/bin/tests/unit_tests
```

**Rule:** Use Debug for development, Release only for benchmarking.

---

## Conan Dependency Errors

### 1. `cannot find -lsodium` (Missing Conan Dependencies)

**Cause:** Conan cache corrupted or dependencies not installed.

**Solution:**
```bash
# Nuclear option: Clear cache and reinstall
conan remove "*" -c
conan install . --profile=conan-linux-x86_64 --build=missing

# Then reconfigure CMake
cmake --preset conan-debug
```

---

### 2. Wrong Conan Profile

**Symptoms:**
- Linker errors for yaml-cpp, spdlog, etc.

**Cause:** Using wrong profile for your platform.

**Solution:**
```bash
# Linux x86_64
conan install . --profile=conan-linux-x86_64 --build=missing

# Check available profiles
ls ~/.conan2/profiles/
```

---

### 3. Dependency Version Conflicts

**Symptoms:**
- Conan install fails with version conflict errors

**Solution:**
```bash
# Update conanfile.py to specify compatible versions
# Then clean install
conan remove "*" -c
conan install . --profile=conan-linux-x86_64 --build=missing
```

---

## Coverage Configuration Issues

### 1. gcovr Boolean Format Errors

**Problem:** gcovr config file uses wrong boolean format.

**Wrong:**
```ini
exclude-unreachable-branches = true  # Python-style boolean
```

**Solution:** Check gcovr documentation for your installed version:
```bash
gcovr --help | grep -A 5 "exclude-unreachable"
# Use the format specified in help output
```

---

### 2. Coverage Missing for Header-Only Code

**Problem:** Header-only code (like `observable.h`) shows 0% coverage.

**Cause:** Coverage compile flags (`--coverage`, `-fprofile-arcs`, `-ftest-coverage`) only on source targets, not test executables.

**Solution:** Add coverage flags to BOTH:
- Source libraries/executables
- Test executables

**CMakeLists.txt:**
```cmake
# Source library
target_compile_options(sudoku_lib PRIVATE --coverage)
target_link_options(sudoku_lib PRIVATE --coverage)

# Test executable (needed for header-only code)
target_compile_options(unit_tests PRIVATE --coverage)
target_link_options(unit_tests PRIVATE --coverage)
```

---

### 3. Deprecated gcovr Options

**Problem:** gcovr version changed, old options no longer valid.

**Solution:**
```bash
# Check your gcovr version
gcovr --version

# Check current options
gcovr --help

# Update .gcovr.cfg to match current version
```

---

### 4. Coverage Data Stale After CMake Changes

**Problem:** Coverage report doesn't reflect recent code changes.

**Solution:** Clean rebuild + re-run tests:
```bash
rm -rf build/Debug
cmake --preset conan-debug
cmake --build build/Debug
./build/Debug/bin/tests/unit_tests
./build/Debug/bin/tests/integration_tests
./scripts/coverage.sh html
```

---

## Test Failures

### 1. Timing-Dependent Test Failures

**Problem:** Tests fail intermittently with timing assertions.

**Wrong Approach:**
```cpp
// ❌ BAD: Using sleep_for (unreliable under load)
std::this_thread::sleep_for(std::chrono::milliseconds(50));
REQUIRE(elapsed >= 50ms);
```

**Solution:** Use `MockTimeProvider`:
```cpp
// ✅ GOOD: Deterministic time control
auto mockTime = std::make_shared<MockTimeProvider>();
GameState state(mockTime);

state.startTimer();
mockTime->advanceTime(std::chrono::minutes(5));  // Instant
REQUIRE(state.elapsedTime() == 5min);
```

**See [ITIMEPROVIDER_PATTERN.md](ITIMEPROVIDER_PATTERN.md) for comprehensive patterns.**

---

### 2. Invalid Test Data

**Problem:** Test helper generates invalid Sudoku boards (duplicate numbers in rows/columns/boxes).

**Consequence:** Tests pass with invalid data, bugs not caught.

**Solution:** Validate test data before use:
```cpp
// ✅ GOOD: Validate generated test data
auto board = createTestBoard();
REQUIRE(isValidBoard(board));  // Ensure Sudoku rules satisfied
```

**Use existing utilities:**
- `isValidBoard()` in test_utils.cpp
- `findEmptyCell()` for valid cell search
- Test data builders that guarantee valid boards

---

### 3. "Test is Flaky" Anti-Pattern

**Problem:** Test fails, assumption made that it's "pre-existing flakiness."

**Wrong Approach:** Dismiss failure, rerun tests until they pass.

**Correct Approach:**
1. **Investigate root cause** - every test failure is a potential bug
2. Check for timing issues → fix with MockTimeProvider
3. Check for invalid test data → validate with isValidBoard()
4. Check for race conditions → ensure single-threaded (no std::thread)
5. **If truly flaky:** Fix the test, don't ignore it

**Rule:** Never assume "flaky" without thorough investigation.

---

## SaveManager Encryption

### Problem: Wrong Encryption Key Length

**Requirement:** ChaCha20-Poly1305 requires **exactly 32-byte key** (derived via Argon2id).

**Symptoms:**
- `SaveError::EncryptionFailed`
- `SaveError::DecryptionFailed`

**Solution:**
```cpp
// ✅ GOOD: 32-byte key from Argon2id
std::array<unsigned char, 32> key = deriveKey(password);

// ❌ BAD: Arbitrary length
std::string key = "short_key";  // Not 32 bytes!
```

**See SaveManager implementation for correct key derivation.**

---

## Diagnostic Commands

### Check Build Configuration

```bash
# Check CMake configuration
cmake -LA build/Debug | grep -i "CMAKE_BUILD_TYPE"

# Check compiler flags
cmake --build build/Debug -- VERBOSE=1 | grep "c++"

# List compiled targets
cmake --build build/Debug --target help
```

---

### Verify Coverage Setup

```bash
# Check if coverage flags present
cmake -LA build/Debug | grep -i "coverage"

# Check gcovr version and options
gcovr --version
gcovr --help | less

# List generated .gcda files (coverage data)
find build/Debug -name "*.gcda"
```

---

### Inspect CMake Cache

```bash
# View all CMake cache variables
cmake -LA build/Debug | less

# Check specific variable
cmake -LA build/Debug | grep "SOME_VAR"

# Delete cache to start fresh
rm build/Debug/CMakeCache.txt
```

---

### Validate Test Data

```cpp
// In test code
auto board = createTestBoard();
REQUIRE(isValidBoard(board));  // Ensure valid Sudoku

// isValidBoard() checks:
// - No duplicate numbers in rows
// - No duplicate numbers in columns
// - No duplicate numbers in 3x3 boxes
```

---

## Recovery Workflows

### Clean Rebuild Procedure

**When to use:** Stale cache, mysterious build errors, linker issues.

```bash
# 1. Remove build directory
rm -rf build/Debug

# 2. Optionally clear Conan cache (if dependency issues)
conan remove "*" -c

# 3. Reinstall dependencies
conan install . --profile=conan-linux-x86_64 --build=missing

# 4. Reconfigure CMake
cmake --preset conan-debug

# 5. Build
cmake --build build/Debug

# 6. Run tests
./build/Debug/bin/tests/unit_tests
./build/Debug/bin/tests/integration_tests
```

---

### Conan Cache Reset

**When to use:** Corrupted Conan cache, missing dependencies.

```bash
# 1. Remove all Conan packages
conan remove "*" -c

# 2. Reinstall dependencies
conan install . --profile=conan-linux-x86_64 --build=missing

# 3. Reconfigure CMake (Conan generates new toolchain)
cmake --preset conan-debug

# 4. Build
cmake --build build/Debug
```

---

### CMake Reconfiguration Steps

**When to use:** After adding/removing files, changing CMakeLists.txt.

```bash
# Quick reconfigure (preserves cache)
cmake --preset conan-debug

# Full reconfigure (clears cache)
rm -rf build/Debug
cmake --preset conan-debug

# Then build
cmake --build build/Debug
```

---

## See Also

- [DEBUGGING_GUIDE.md](DEBUGGING_GUIDE.md) - GDB, Valgrind, AddressSanitizer
- [CODE_QUALITY.md](CODE_QUALITY.md) - Formatting, static analysis, CI/CD
- [TDD_GUIDE.md](TDD_GUIDE.md) - Test-driven development practices
- [ARCHITECTURE_PRINCIPLES.md](ARCHITECTURE_PRINCIPLES.md) - MVVM, SOLID, DIP
- [CPP_STYLE_GUIDE.md](CPP_STYLE_GUIDE.md) - C++23 coding standards
