# Almost Always Auto (AAA) Style Guide

**Last Updated:** February 2026

## Overview

The "Almost Always Auto" (AAA) principle is a modern C++ guideline that recommends using `auto` for local variable declarations in most cases. This practice reduces redundancy, prevents type mismatches, and makes code more maintainable.

**Core Principle:** Use `auto` for variable declarations unless there's a specific reason not to.

## Benefits of AAA

1. **Eliminates Redundancy**
   - Avoids repeating type names that are already obvious from initialization
   - Example: `auto ptr = std::make_unique<Foo>()` vs. `std::unique_ptr<Foo> ptr = std::make_unique<Foo>()`

2. **Safer Refactoring**
   - Change return type in one place, all `auto` variables adapt automatically
   - No need to hunt down and update type declarations throughout the codebase

3. **Prevents Uninitialized Variables**
   - `auto x;` is a compiler error
   - Forces explicit initialization at declaration site

4. **Matches Types Exactly**
   - Especially important for iterator types: `auto it = container.begin()` is always correct
   - Prevents subtle bugs from type mismatches (e.g., `int` vs. `size_t`)

5. **Improves Readability**
   - Focus on variable name and initialization, not redundant type information
   - Less visual clutter in variable declarations

## When to Use Auto

### ✅ Smart Pointers

**DO:**
```cpp
auto ptr = std::make_unique<GameState>();
auto shared = std::make_shared<SaveManager>(path);
auto weak = std::weak_ptr<Observer>(shared);
```

**DON'T:**
```cpp
std::unique_ptr<GameState> ptr = std::make_unique<GameState>();
std::shared_ptr<SaveManager> shared = std::make_shared<SaveManager>(path);
```

### ✅ Iterators

**DO:**
```cpp
auto it = container.begin();
auto end = container.end();
for (auto it = vec.begin(); it != vec.end(); ++it) {
    // ...
}
```

**DON'T:**
```cpp
std::vector<int>::iterator it = container.begin();
std::vector<int>::const_iterator end = container.end();
```

### ✅ Range-Based For Loops

**DO:**
```cpp
for (const auto& cell : grid) {
    // Read-only access
}

for (auto& cell : grid) {
    // Modify cells
    cell.setValue(5);
}

for (auto cell : grid) {
    // Copy each cell (rare, usually want const auto&)
}
```

**DON'T:**
```cpp
for (const Cell& cell : grid) {  // Redundant type specification
    // ...
}
```

### ✅ Structured Bindings

**DO:**
```cpp
auto [row, col] = findEmptyPosition();
auto [success, error] = validateMove(board, move);
auto [key, value] = *map.find("foo");
```

**DON'T:**
```cpp
std::pair<size_t, size_t> pos = findEmptyPosition();  // Verbose and loses information
size_t row = pos.first;
size_t col = pos.second;
```

### ✅ Lambda Expressions

**DO:**
```cpp
auto lambda = [](int x) { return x * 2; };
auto callback = [this](const Event& event) { handleEvent(event); };
```

**DON'T:**
```cpp
std::function<int(int)> lambda = [](int x) { return x * 2; };  // Overhead from type erasure
```

### ✅ Function Return Values with Complex Types

**DO:**
```cpp
auto result = validator.validateMove(board, move);  // std::expected<bool, ValidationError>
auto puzzle = generator.generate(Difficulty::Medium);  // std::optional<Board>
auto position = findEmptyCell(board);  // std::optional<Position>
```

**DON'T:**
```cpp
std::expected<bool, ValidationError> result = validator.validateMove(board, move);
std::optional<std::vector<std::vector<int>>> puzzle = generator.generate(Difficulty::Medium);
```

### ✅ Template Instantiations

**DO:**
```cpp
auto observable = core::Observable<model::GameState>();
auto container = std::unordered_map<std::string, std::vector<int>>();
```

**DON'T:**
```cpp
core::Observable<model::GameState> observable = core::Observable<model::GameState>();
std::unordered_map<std::string, std::vector<int>> container = std::unordered_map<std::string, std::vector<int>>();
```

### ✅ Type Aliases and Domain Types

**DO:**
```cpp
auto difficulty = core::Difficulty::Medium;
auto position = Position{.row = 5, .col = 3};
auto move = Move{position, 7, MoveType::PlaceNumber};
```

**DON'T:**
```cpp
core::Difficulty difficulty = core::Difficulty::Medium;  // Redundant namespace
Position position = Position{.row = 5, .col = 3};  // Redundant type
```

## When NOT to Use Auto

### ❌ Function Signatures

**DO:**
```cpp
// Explicit return type for API clarity
[[nodiscard]] std::optional<Position> findEmptyPosition(const Board& board) const;

// Explicit parameter types for readability
bool isValidMove(const Board& board, size_t row, size_t col, int value) const;
```

**DON'T:**
```cpp
// Return type unclear without looking at implementation
[[nodiscard]] auto findEmptyPosition(const auto& board) const;

// Parameters unclear (C++20 abbreviated function templates)
bool isValidMove(const auto& board, auto row, auto col, auto value) const;
```

### ❌ Type Not Obvious from Initialization

**DO:**
```cpp
size_t count = container.size();  // Explicit: we want size_t, not whatever size() returns
int value = 5;  // Clear intent for domain value (Sudoku values are int)
double ratio = 0.5;  // Explicit: double, not float
```

**DON'T:**
```cpp
auto count = container.size();  // Is this size_t? unsigned int? Something else?
auto value = 5;  // Is this int, long, size_t? Context-dependent
auto ratio = 0.5;  // Is this float or double? Ambiguous
```

### ❌ Narrowing Conversions

**DO:**
```cpp
int result = static_cast<int>(some_long_value);  // Explicit narrowing
float percentage = static_cast<float>(count) / total;  // Explicit conversion
```

**DON'T:**
```cpp
auto result = some_long_value;  // Implicit, may lose precision silently
auto percentage = count / total;  // Integer division, probably not intended
```

### ❌ Initializer Lists (Disambiguation Needed)

**DO:**
```cpp
std::vector<int> vec = {1, 2, 3};  // Explicit: we want a vector
std::array<int, 3> arr = {1, 2, 3};  // Explicit: we want an array
```

**DON'T:**
```cpp
auto vec = {1, 2, 3};  // Type is std::initializer_list<int>, probably not intended
```

### ❌ Proxy Types and Expression Templates

**DO:**
```cpp
bool result = vec[0] && vec[1];  // Explicit bool, not std::vector<bool>::reference
Eigen::MatrixXd mat = expr1 + expr2;  // Force evaluation, not expression template
```

**DON'T:**
```cpp
auto result = vec[0] && vec[1];  // Dangling reference if vec is std::vector<bool>
auto mat = expr1 + expr2;  // Deferred evaluation, may cause performance issues
```

## Common Patterns in This Codebase

### Game Logic

```cpp
// ✅ GOOD
auto state = model::GameState();
auto validator = std::make_shared<core::GameValidator>();
auto result = validator->isValidMove(board, row, col, value);

if (auto pos = findEmptyPosition(board); pos.has_value()) {
    const auto& [row, col] = pos.value();
    // Use row and col
}
```

### Observable Pattern

```cpp
// ✅ GOOD
auto observable = core::Observable<int>(42);

observable.subscribe([](const auto& value) {
    // value is int, deduced from Observable<int>
    std::cout << "New value: " << value << '\n';
});

observable.update([](auto& value) {
    value++;  // value is int&
});
```

### Dependency Injection

```cpp
// ✅ GOOD
auto container = core::DIContainer();
container.registerSingleton<core::IGameValidator>(
    std::make_shared<core::GameValidator>()
);

auto validator = container.resolve<core::IGameValidator>();  // Returns shared_ptr
```

### File I/O and Save Management

```cpp
// ✅ GOOD
auto save_result = save_manager->saveGame(game, settings);
if (!save_result.has_value()) {
    const auto& error = save_result.error();
    handleSaveError(error);
}

auto load_result = save_manager->loadGame(save_id);
```

## Quick Reference

| Situation | Use `auto`? | Reason |
|-----------|-------------|--------|
| Smart pointers (`make_unique`, `make_shared`) | ✅ Yes | Eliminates redundancy |
| Iterators | ✅ Yes | Matches exact type |
| Range-based for loops | ✅ Yes | Cleaner syntax |
| Structured bindings | ✅ Yes | Required for syntax |
| Lambda expressions | ✅ Yes | Avoids `std::function` overhead |
| Function return values (complex types) | ✅ Yes | Type already clear from call |
| Template instantiations | ✅ Yes | Eliminates redundancy |
| Function parameters | ❌ No | API clarity |
| Function return types | ❌ No | Documentation and safety |
| Literal integers/floats | ❌ No | Type ambiguous |
| Initializer lists `{...}` | ❌ No | Type ambiguous |
| Explicit narrowing | ❌ No | Intent should be clear |

## Migration Strategy

When updating existing code to use AAA:

1. **Start with new code** - Apply AAA to all new functions and classes
2. **Update during refactoring** - When modifying existing code, opportunistically convert to `auto`
3. **Don't batch convert** - Avoid mass "auto-ification" commits; change incrementally
4. **Focus on high-value targets** - Prioritize smart pointers, iterators, and complex template types
5. **Preserve clarity** - If `auto` makes code less clear, keep the explicit type

## References

- [Herb Sutter's AAA Style](https://herbsutter.com/2013/08/12/gotw-94-solution-aaa-style-almost-always-auto/)
- [C++ Core Guidelines ES.11](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Res-auto)
- [C++ Core Guidelines ES.12](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#es12-use-stdmove_iterator-when-move-semantics-could-improve-performance)
