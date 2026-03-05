# Sudoku Puzzle Generation Benchmarks

Generalized benchmark infrastructure for measuring puzzle generation performance with YAML-based configuration and CLI argument parsing.

## Quick Start

```bash
# Build benchmarks
cmake --build build/Release --target puzzle_generation_benchmark

# Run standard benchmark (20 iterations, Hard + Expert)
./build/Release/bin/benchmarks/puzzle_generation_benchmark

# Run quick smoke test (5 iterations)
./build/Release/bin/benchmarks/puzzle_generation_benchmark --config tests/benchmarks/configs/quick.yaml

# Run thorough analysis (100 iterations)
./build/Release/bin/benchmarks/puzzle_generation_benchmark --config tests/benchmarks/configs/thorough.yaml

# Run all benchmarks via CMake target
cmake --build build/Release --target run_benchmarks
```

## Configuration Files

Three pre-configured YAML files are provided in `configs/`:

### quick.yaml
**Purpose:** Fast smoke test to verify generation works
**Iterations:** 5 per difficulty
**Difficulties:** Hard, Expert
**Use case:** Quick validation after code changes

### standard.yaml
**Purpose:** Regular performance tracking and regression testing
**Iterations:** 20 per difficulty
**Difficulties:** Hard, Expert
**Use case:** Default benchmark for development

### thorough.yaml
**Purpose:** Deep statistical analysis for major optimizations
**Iterations:** 100 Hard, 50 Expert
**Difficulties:** Hard, Expert
**Use case:** Validating performance improvements

## Command-Line Options

```
Usage: puzzle_generation_benchmark [options]

Options:
  --config <file>     YAML config file (default: configs/standard.yaml)
  --difficulty <name> Override difficulty (Easy|Medium|Hard|Expert)
  --iterations <n>    Override number of puzzles to generate
  --verbose, -v       Enable per-iteration output
  --help, -h          Show this help message
```

## Examples

### Run with custom difficulty
```bash
./puzzle_generation_benchmark --difficulty Expert --iterations 50 --verbose
```

### Run quick test before committing
```bash
./puzzle_generation_benchmark --config configs/quick.yaml
```

### Validate optimization impact
```bash
# Before optimization
./puzzle_generation_benchmark --config configs/thorough.yaml > before.txt

# After optimization
./puzzle_generation_benchmark --config configs/thorough.yaml > after.txt

# Compare results
diff before.txt after.txt
```

### CI/CD Integration
```bash
# Exit code 0 if success rate >= 90%, 1 otherwise
./puzzle_generation_benchmark --config configs/standard.yaml || exit 1
```

## Output Format

```
=================================================================
Puzzle Generation Benchmark
=================================================================
Configuration: configs/standard.yaml
Difficulty:    Hard
Iterations:    20
=================================================================

Benchmarking Hard difficulty (20 iterations)...
  [ 1/20] ✓ 245.3ms (32 clues)
  [ 2/20] ✓ 198.7ms (31 clues)
  ...

=================================================================
BENCHMARK RESULTS
=================================================================

Hard Difficulty:
  Success Rate:  100.0% (20/20)
  Avg Time:      239.2ms
  Median Time:   230.5ms
  Range:         198.7ms - 312.4ms
  Avg Clues:     31.5
=================================================================
```

## Adding New Benchmarks

1. Create a new YAML config in `configs/`:

```yaml
benchmarks:
  - name: "My Custom Benchmark"
    difficulty: Hard
    iterations: 10
    max_attempts: 200
    verbose: true
```

2. Run with new config:

```bash
./puzzle_generation_benchmark --config configs/my_custom.yaml
```

## Architecture

### Shared Utilities

All benchmarks use shared utilities from `tests/helpers/benchmark_utils.{h,cpp}`:

- **BenchmarkConfig**: YAML-based configuration structure
- **BenchmarkResult**: Statistical results with printSummary()
- **runBenchmark()**: Template function for running benchmarks
- **countClues()**: Helper for counting non-zero cells

### Template Design

`runBenchmark()` is a template that works with any `IPuzzleGenerator` implementation, enabling testing with mock generators or alternative implementations.

## Migration from Old Benchmarks

This infrastructure replaces:
- `benchmark_phase2.cpp` → `configs/standard.yaml` (20 iterations)
- `benchmark_phase3_quick.cpp` → `configs/quick.yaml` (5 iterations)
- `benchmark_CMakeLists.txt` → `tests/benchmarks/CMakeLists.txt`

All functionality is preserved with added configurability and CLI argument support.

## Exit Codes

- **0**: Benchmark succeeded (success rate >= 90%)
- **1**: Benchmark failed (success rate < 90% or config error)

Use exit codes for automated testing and CI/CD validation.
