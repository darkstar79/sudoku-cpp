#!/bin/bash
set -e

BENCH_DIR="build/Release/bin/benchmarks"
RESULTS_DIR="benchmarks/baseline_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$RESULTS_DIR"

echo "=== Baseline Performance Measurement ==="
echo "Date: $(date)"
echo "CPU: $(lscpu | grep 'Model name' | cut -d: -f2 | xargs)"
echo "Cores: $(nproc)"
echo "Governor: $(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null || echo 'N/A')"

# Note: CPU governor setting skipped (requires sudo)
echo "Note: Running with current CPU governor settings"

# Run benchmarks for each difficulty (100 iterations for statistical significance)
# Note: Expert excluded due to low generation success rate
for difficulty in Easy Medium Hard; do
    echo ""
    echo "=== Benchmarking $difficulty puzzles ==="

    $BENCH_DIR/puzzle_generation_benchmark \
        --difficulty $difficulty \
        --iterations 100 \
        --verbose \
        | tee "$RESULTS_DIR/${difficulty}_results.txt"
done

echo ""
echo "Results saved to: $RESULTS_DIR"
echo "Summary:"
grep -E "(Mean|Median|Min|Max)" "$RESULTS_DIR"/*.txt || echo "Note: Summary format may vary"
