# Observable Pattern: Reactive State Management

**Last Updated:** February 2026

## Overview

**CRITICAL**: The Observable pattern implementation has specific usage requirements. Violating these patterns will break observer notifications.

The Observable pattern provides reactive data binding between the ViewModel and View layers, ensuring the UI always reflects the current application state.

## Core Concept

Observable<T> wraps a value and notifies subscribers whenever the value changes. This enables automatic UI updates without manual synchronization.

## Correct Usage Patterns

### ✅ Using update() for Modifications

**This is the primary pattern for modifying Observable state:**

```cpp
// Modify Observable state with automatic notification
gameState.update([&value](model::GameState& state) {
    state.setSomeValue(value);
    state.modifyOtherValue();
}); // Observers notified automatically after lambda executes
```

**Why this works:**
- Lambda receives mutable reference to internal state
- Modifications happen in-place
- Observable automatically detects changes after lambda executes
- Observers notified if state changed

### ✅ Read-Only Access with get()

**For reading values without modification:**

```cpp
// Read Observable state (const reference, no modification)
const auto& state = observable.get();
int value = state.getValue();  // Read-only access
```

**Why this works:**
- Returns const reference (read-only)
- No notifications triggered (no modification)
- Zero-cost abstraction (no copying)

## Broken Patterns (DO NOT USE)

### ❌ Modify-then-Set Pattern

**This pattern is BROKEN and will NOT notify observers:**

```cpp
// This pattern is BROKEN and will NOT notify observers
auto& state = observable.get();  // ERROR: T& get() method removed
state.modify();                   // Modifies internal state
observable.set(state);           // Compares same object, no change detected!
// Result: Observers NEVER notified
```

### Why the Broken Pattern Fails

1. `get()` returns reference to internal state at memory address A
2. Modification changes data at address A
3. `set()` takes parameter by value, creates copy at address B
4. `set()` compares internal state (A) to parameter copy (B)
5. Both contain same data → comparison returns true (equal)
6. **No change detected** → observers NOT notified

**Solution:** The non-const `T& get()` method has been **removed** to prevent this broken pattern. Only `const T& get() const` is available.

## API Reference

### Observable<T> Methods

```cpp
class Observable<T> {
public:
    // Constructor
    explicit Observable(T initial_value);

    // Read-only access (const reference)
    const T& get() const;

    // Replace entire value (notifies if different)
    void set(T new_value);

    // Modify in-place with lambda (always checks for changes)
    template<typename Func>
    void update(Func&& updater);

    // Subscribe to changes
    void subscribe(std::function<void(const T&)> observer);

    // Explicit conversion (prevents accidental loss of reactivity)
    explicit operator T() const;
};
```

### Method Details

#### `const T& get() const`

Returns const reference for read-only access.

**Use when:** Reading values without modification.

```cpp
const auto& state = gameState.get();
int elapsedMs = state.getElapsedTime().count();
```

#### `void set(T new_value)`

Replaces entire value, notifies if different.

**Use when:** Replacing the entire state with a new value.

```cpp
gameState.set(model::GameState{});  // Reset to default state
```

#### `void update(Func&& updater)`

Modifies in-place with lambda, always checks for changes.

**Use when:** Modifying specific fields (most common case).

```cpp
gameState.update([](model::GameState& state) {
    state.placeNumber({3, 5}, 7);
    state.incrementMoveCount();
});
```

#### `void subscribe(std::function<void(const T&)> observer)`

Registers a callback to be notified on state changes.

**Use when:** Setting up reactive UI bindings.

```cpp
gameState.subscribe([this](const model::GameState& state) {
    updateUI(state);
});
```

## Thread Safety

**Observable is NOT thread-safe** (application is single-threaded by design):

- Qt GUI operations require main thread execution
- No `std::thread`, `std::async`, or `std::future` usage in codebase
- **Verification**: SaveManager and StatisticsManager use no mutexes (single-threaded architecture validated)
- Adding mutex-based thread safety is **inappropriate** for this architecture

## Why Observable Conversion is Explicit

The implicit conversion operator was changed to explicit to prevent bugs from accidental loss of reactivity.

### Rationale for `explicit operator T()`

```cpp
// ❌ WRONG (with implicit conversion):
Observable<int> count{42};
int x = count;  // Silently loses reactivity - x is now a copy
x++;  // count is NOT updated!

// ✅ CORRECT (with explicit conversion):
Observable<int> count{42};
int x = count.get();  // Explicit: developer aware of copying
// Or better: work directly with Observable
count.update([](int& val) { val++; });  // Reactivity preserved
```

**Key insight:** Implicit conversion makes it too easy to accidentally break reactivity. Explicit conversion forces developers to acknowledge they're extracting a copy.

## Migration Guide

### If you see compiler errors after explicit conversion change

1. **Preferred**: Use `.get()` for read-only access
   ```cpp
   const auto& value = observable.get();
   ```

2. **Alternative**: Use `static_cast<T>(observable)` for explicit conversion
   ```cpp
   int copy = static_cast<int>(observable);
   ```

3. **Best**: Avoid extracting values; use `.update()` to modify in-place
   ```cpp
   observable.update([](int& val) { val++; });
   ```

## Common Patterns in This Codebase

### ViewModel to View Binding

```cpp
// In GameViewModel:
class GameViewModel {
public:
    core::Observable<model::GameState>& getGameState() {
        return game_state_;
    }

private:
    core::Observable<model::GameState> game_state_;
};

// In MainWindow (View):
view_model_.getGameState().subscribe([this](const model::GameState& state) {
    // Automatically update UI when state changes
    renderBoard(state.getBoard());
    renderTimer(state.getElapsedTime());
});
```

### Updating Multiple Fields

```cpp
// Update multiple fields atomically
gameState.update([position, value](model::GameState& state) {
    state.placeNumber(position, value);
    state.clearNotes(position);
    state.incrementMoveCount();
    state.checkForCompletion();
});
// All observers notified once after all modifications
```

### Conditional Updates

```cpp
// Only update if condition met
gameState.update([&](model::GameState& state) {
    if (state.isValid()) {
        state.applyMove(move);
    }
});
// Observers notified only if state actually changed
```

## Testing Observable Notifications

```cpp
TEST_CASE("Observable notifies observers on state change", "[observable]") {
    Observable<int> observable(42);
    int notification_count = 0;
    int last_value = 0;

    observable.subscribe([&](int new_value) {
        notification_count++;
        last_value = new_value;
    });

    SECTION("set() triggers notification when value changes") {
        observable.set(100);
        REQUIRE(notification_count == 1);
        REQUIRE(last_value == 100);
    }

    SECTION("set() does NOT notify when value is same") {
        observable.set(42);  // Same as initial value
        REQUIRE(notification_count == 0);
    }

    SECTION("update() always checks for changes") {
        observable.update([](int& val) { val = 200; });
        REQUIRE(notification_count == 1);
        REQUIRE(last_value == 200);
    }
}
```

## Performance Considerations

- **Zero-cost abstraction**: `get()` returns const reference (no copying)
- **Efficient comparison**: Only notifies if value actually changed
- **Single notification**: Multiple updates in one `update()` call trigger one notification
- **Move semantics**: `set()` uses move semantics when possible

## Additional Resources

- [ARCHITECTURE_PRINCIPLES.md](ARCHITECTURE_PRINCIPLES.md) - Architectural design principles
- [src/core/observable.h](../src/core/observable.h) - Observable implementation
- [TDD_GUIDE.md](TDD_GUIDE.md) - Testing Observable notifications (section 5)
