# C++ Development & Debugging Best Practices

**Date:** January 2026
**Context:** Best practices learned from real-world debugging experiences in the Sudoku C++ project

---

## Table of Contents

1. [Debug Build Configuration](#debug-build-configuration)
2. [Why Debug Builds Matter](#why-debug-builds-matter)
3. [Development Workflow](#development-workflow)
4. [Common C++ Pitfalls](#common-cpp-pitfalls)
5. [Library API Pitfalls](#library-api-pitfalls)
6. [Debugging Methodology](#debugging-methodology)
7. [Code Quality Checklist](#code-quality-checklist)

---

## Debug Build Configuration

### CRITICAL: Separate Debug and Release Builds

**Key Learning from January 2026 SIGSEGV Investigation:**

Always maintain **separate Debug and Release build configurations** with Debug builds having full debug symbols in ALL dependencies, not just application code.

### Conan Debug Profile Setup

Create `~/.conan2/profiles/debug` with:

```ini
[settings]
arch=x86_64
build_type=Debug
compiler=gcc
compiler.cppstd=gnu23
compiler.libcxx=libstdc++11
compiler.version=15
os=Linux

[conf]
tools.cmake.cmaketoolchain:generator=Ninja
tools.build:cxxflags=["-g", "-O0", "-fno-omit-frame-pointer"]
tools.build:cflags=["-g", "-O0", "-fno-omit-frame-pointer"]
```

**Key Settings:**
- `build_type=Debug` → Conan builds ALL dependencies (yaml-cpp, spdlog, Catch2, libsodium) with debug symbols
- `-g` → Full debug symbols for GDB/LLDB
- `-O0` → No optimization (accurate debugging, variables not optimized away)
- `-fno-omit-frame-pointer` → Complete stack traces in debugger

**Installation:**
```bash
# One-time setup: Install debug dependencies
conan install . --profile=debug --build=missing --output-folder=build/Debug

# Configure and build
cmake --preset conan-debug
cmake --build --preset conan-debug

# Run with debugger
gdb --args ./build/Debug/bin/tests/unit_tests "TestName"
```

---

## Why Debug Builds Matter

**Debug builds catch bugs that Release builds miss:**

### 1. Bounds Checking

GCC/libstdc++ provides `-D_GLIBCXX_ASSERTIONS` in Debug mode:
- Runtime checks for `vector::operator[]`, iterator validity, etc.
- Example: Accessing `vec[0]` when `vec.size() == 0` → SIGABRT with clear error message
- Release mode: Undefined behavior (silent corruption or random crashes)

### 2. Debug Symbols in Dependencies

Can step INTO library code with full source visibility:
- Debug yaml-cpp, spdlog code with GDB
- See local variables, function arguments in library stack frames
- Example: YAML-cpp sparse array bug found by inspecting `YAML::Node` internals

### 3. Memory Safety

Easier to use with Valgrind, AddressSanitizer:
- Debug builds have predictable memory layouts
- No inlining makes stack traces clearer

### Real-World Example (January 2026)

```cpp
// Bug: Creating SavedGame without populating all fields
SavedGame game;
game.current_state = extractNumbers();
// Missing: game.original_puzzle (default: empty vector)

// Later: Serialization crashes
BoardSerializer::serializeIntBoard(game.original_puzzle);
// Tries to access original_puzzle[0][0] when size() == 0

// Release build: Undefined behavior (sometimes works, sometimes crashes)
// Debug build: Immediate SIGABRT with assertion:
//   Assertion '__n < this->size()' failed at stl_vector.h:1282
```

**GDB Stack Trace with Debug Symbols:**
```
#4  std::vector::operator[] at stl_vector.h:1282
    Assertion '__n < this->size()' failed
#5  board_serializer.cpp:9
    board = std::vector of length 0, capacity 0  ← Smoking gun!
#6  BoardSerializer::serializeIntBoard (board=std::vector of length 0)
#7  SaveManager::serializeToYaml at save_manager.cpp:454
#8  SaveManager::autoSave at save_manager.cpp:140
#9  GameViewModel::autoSave at game_view_model.cpp:203
```

Without debug symbols: Stack trace shows only addresses, can't see variable values.

---

## Development Workflow

### Use Debug Builds During Development

```bash
# Quick iteration
cmake --build build/Debug && ./build/Debug/bin/tests/unit_tests

# Investigate crash
gdb --args ./build/Debug/bin/tests/unit_tests "Failing Test"
(gdb) run
(gdb) bt full  # Full backtrace with local variables
(gdb) frame 5  # Jump to specific stack frame
(gdb) print variable_name
```

### Use Release Builds for Performance Testing

```bash
cmake --build build/Release
./build/Release/bin/tests/unit_tests  # Should still pass!
```

### Never Ship Debug Builds to Production

- 10-100x slower than Release
- 5-10x larger binary size
- Exposes internal implementation details

---

## Common C++ Pitfalls

### Incomplete Object Initialization

**Problem:** Structs/classes with many fields are easy to incompletely initialize.

**Example Bug (January 2026):**
```cpp
// SavedGame has 15+ fields
SavedGame game;
game.current_state = extractNumbers();
game.difficulty = getDifficulty();
// Forgot: original_puzzle, notes, hint_revealed_cells, etc.
// Result: Crash when serialization tries to access empty vectors
```

**Prevention Strategies:**

#### 1. Use Designated Initializers (C++20)

Compiler warns/errors if fields are missing when using designated initializer syntax with brace initialization.

#### 2. Factory Methods

```cpp
class GameState {
public:
    [[nodiscard]] core::SavedGame createSaveData() const {
        return {
            .current_state = extractNumbers(),
            .original_puzzle = extractGivenNumbers(),
            .notes = extractAllNotes(),
            // ... guaranteed complete initialization in one place
        };
    }
};
```

#### 3. Default Member Initializers

```cpp
struct SavedGame {
    std::vector<std::vector<int>> current_state{};
    std::vector<std::vector<int>> original_puzzle{};  // Safe default: empty
    std::vector<std::vector<std::vector<int>>> notes{};
};
```

#### 4. Static Analysis

Enable `cppcoreguidelines-pro-type-member-init` and `hicpp-member-init` checks in clang-tidy.

---

## Library API Pitfalls

### yaml-cpp Sparse Array Issue (January 2026)

```cpp
// WRONG: Assumes YAML maps behave like dense arrays
void deserializeNotes(const YAML::Node& node, ...) {
    forEachCell([&](size_t row, size_t col) {
        if (col < node[row].size()) {  // ❌ BUG!
            // node[row] is a MAP, not a sequence
            // size() returns number of KEYS, not max index
            // Example: {2: [1,2,3], 5: [4,5,6]} has size() == 2, not 6!
        }
    });
}

// CORRECT: Check node existence, not size
void deserializeNotes(const YAML::Node& node, ...) {
    forEachCell([&](size_t row, size_t col) {
        if (node[row] && node[row][col]) {  // ✅ Correct
            // Explicitly checks if key exists in map
        }
    });
}
```

**Lesson:** Don't assume third-party library APIs behave like STL. Read documentation carefully.

---

## Debugging Methodology

### When Facing Intermittent Crashes (Heisenbug)

1. **Reproduce in Debug build first** → Crashes become deterministic with assertions
2. **Use GDB with debug symbols** → Get exact crash location and variable values
3. **Check for uninitialized data** → Most common C++ bug category
4. **Verify all struct fields populated** → Missing initialization often causes crashes later
5. **Run under AddressSanitizer** → Catches use-after-free, buffer overflows
6. **Run under Valgrind** → Finds uninitialized reads, memory leaks

### Time Investment Analysis

- **Creating debug profile:** 30 minutes (one-time)
- **Debugging with symbols:** 2 hours to find + fix root cause
- **Debugging without symbols:** Days of speculation and guesswork

**ROI:** Debug infrastructure pays for itself on the first serious bug.

### GDB Quick Reference

```bash
# Launch with specific test
gdb --args ./build/Debug/bin/tests/unit_tests "Test Name"

# Basic commands
(gdb) run                    # Start execution
(gdb) bt                     # Backtrace (stack trace)
(gdb) bt full                # Backtrace with local variables
(gdb) frame 5                # Jump to stack frame 5
(gdb) print variable_name    # Print variable value
(gdb) info locals            # Show all local variables
(gdb) list                   # Show source code
(gdb) break file.cpp:123     # Set breakpoint
(gdb) continue               # Continue after breakpoint
```

### Valgrind Quick Reference

```bash
# Check for memory leaks
valgrind --leak-check=full ./build/Debug/bin/tests/unit_tests

# Check for uninitialized reads
valgrind --track-origins=yes ./build/Debug/bin/tests/unit_tests
```

### AddressSanitizer + UBSan Quick Reference

Build with ASan (memory errors) and UBSan (undefined behavior) using the CMake option:

```bash
# Build with sanitizers (uses Release for optimization + debug symbols)
conan install . --build=missing -s build_type=Release
cmake --preset conan-release -DENABLE_SANITIZERS=ON
cmake --build --preset conan-release

# Run tests (crashes immediately on memory errors or undefined behavior)
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
./build/Release/build/Release/bin/tests/unit_tests

# Run a specific test case
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
./build/Release/build/Release/bin/tests/unit_tests "TestName"
```

**Note:** Do not combine `-DENABLE_SANITIZERS=ON` with coverage builds.
The sanitizer option automatically disables coverage instrumentation.

---

## Code Quality Checklist

Before committing code, verify:

- ✅ **TDD Cycle**: Tests written BEFORE implementation (Red-Green-Refactor)
- ✅ **Test Coverage**: Happy path, edge cases, and error handling all tested
- ✅ **Coverage Targets**: ≥80% line/function coverage maintained
- ✅ **All tests pass**: Both Debug and Release builds
- ✅ **[[nodiscard]]** on all non-void return values (compiler enforces checking)
- ✅ All struct/class members initialized (use designated initializers)
- ✅ No raw `new`/`delete` (use `std::unique_ptr`, `std::shared_ptr`)
- ✅ No `const_cast` (fix const-correctness instead)
- ✅ `std::vector` access uses `.at()` in Debug builds (bounds checking)
- ✅ `std::optional` used instead of sentinel values (`-1`, `nullptr`)
- ✅ `std::expected` used for error handling (not exceptions or error codes)
- ✅ Valgrind/ASan clean (no leaks, no invalid reads)
- ✅ MVVM architecture respected (no Model dependencies in View layer)
- ✅ Dependency injection used (no direct instantiation of services)
- ✅ Single-threaded design (no `std::thread`, `std::async`, mutexes)

---

## Additional Resources

- [ARCHITECTURE_PRINCIPLES.md](ARCHITECTURE_PRINCIPLES.md) - Architectural design principles
- [CODE_QUALITY.md](CODE_QUALITY.md) - Code formatting and static analysis tools
- [README.md](../README.md#debugging) - Build configurations and debugging setup

---

## Appendix: Case Studies

### Case Study 1: SIGSEGV in SaveManager (January 2026)

**Symptom:** Intermittent crashes during auto-save in Release build.

**Investigation:**
1. Reproduced in Debug build → Immediate SIGABRT with assertion failure
2. GDB stack trace revealed `std::vector::operator[]` out-of-bounds access
3. Found `SavedGame::original_puzzle` was empty (not initialized)
4. Root cause: Incomplete object initialization in factory method

**Fix:** Updated `GameState::createSaveData()` to initialize all fields using designated initializers.

**Time to Fix:**
- Without debug build: 3 days of speculation
- With debug build: 2 hours (including fix and tests)

**Lesson:** Debug builds turn heisenbugs into deterministic failures.

### Case Study 2: yaml-cpp Sparse Array Bug (January 2026)

**Symptom:** Deserialization reading wrong notes data from save file.

**Investigation:**
1. Added logging to deserialization code
2. Found `node[row].size()` returned 2, not 9 (expected for full row)
3. GDB inspection of `YAML::Node` showed it was a map, not sequence
4. Realized `size()` counts keys, not max index

**Fix:** Changed from `col < node[row].size()` to `node[row] && node[row][col]`.

**Time to Fix:** 4 hours (including writing tests for edge cases)

**Lesson:** Third-party APIs don't always behave like STL containers.

### Case Study 3: Memoization Cache Truncation Bug (February 2026)

**Symptom:** `PuzzleRater` stress test failed intermittently (~15% of runs) with `Unsolvable` error on valid puzzles.

**Investigation:**

1. Initial suspicion was a strategy bug (Forcing Chain / Nice Loop were recently added)
2. Diagnostic test replayed solver steps against backtracking ground truth — found wrong deductions from Unique Rectangle, BUG, WXYZ-Wing, and Sue de Coq strategies
3. Key insight: Unique Rectangle assumes puzzle uniqueness. If the puzzle has multiple solutions, UR makes wrong eliminations
4. Independent brute-force solution counter confirmed 54/296 generated puzzles had multiple solutions despite `hasUniqueSolution()` returning `true`
5. Disabling the memoization cache → 0/286 non-unique puzzles, confirming the cache as root cause
6. Root cause: `countSolutionsHelper` cached solution counts after `count >= max_solutions` triggered early termination. The cached value was a lower bound, not the true count. Later lookups returned this undercount, making multi-solution puzzles appear unique

**Fix:** Guard all 6 cache insertion sites (3 scalar, 3 SIMD) with `if (count < max_solutions && !timed_out)` to only cache fully-explored subtree results.

**Time to Fix:** ~4 hours (including diagnosis, fix, verification across both code paths)

**Lessons:**

- Memoization caches must never store results from truncated searches — the cached value becomes a lower bound that corrupts future lookups
- When multiple strategies fail on the same puzzles, look upstream at puzzle generation/validation, not at the strategies themselves
- Independent verification tools (brute-force counter without caching) are essential for isolating cache-specific bugs
- Bugs in `hasUniqueSolution()` have cascading effects: any strategy that assumes uniqueness (UR, BUG) becomes incorrect
