#!/usr/bin/env bash
# Find potential dead code: headers in src/ that are not reachable from main.cpp's
# include graph, but ARE included by test files.
#
# Usage:
#   ./scripts/dead-code.sh                    # default: trace from src/main.cpp
#   ./scripts/dead-code.sh src/main.cpp       # explicit entry point

set -uo pipefail

ENTRY="${1:-src/main.cpp}"

if [ ! -f "$ENTRY" ]; then
    echo "Error: entry point '$ENTRY' not found" >&2
    exit 1
fi

RED='\033[0;31m'
YELLOW='\033[0;33m'
GREEN='\033[0;32m'
NC='\033[0m'

# Build the reachable set from the entry point using BFS over #include directives
declare -A visited

queue=("$ENTRY")
while [ ${#queue[@]} -gt 0 ]; do
    current="${queue[0]}"
    queue=("${queue[@]:1}")

    # Normalize path
    current=$(realpath --relative-to=. "$current" 2>/dev/null || echo "$current")

    [ -n "${visited[$current]+x}" ] && continue
    visited[$current]=1

    # Find all #include "..." (local includes only, skip system <> includes)
    includes=$(grep -oP '#include\s*"\K[^"]+' "$current" 2>/dev/null || true)
    dir=$(dirname "$current")

    for inc in $includes; do
        # Resolve relative to including file's directory, then try src/ roots
        resolved=""
        for try_path in "$dir/$inc" "src/$inc" "src/core/$inc" "src/view/$inc" \
                        "src/view_model/$inc" "src/model/$inc" "src/infrastructure/$inc" \
                        "src/core/strategies/$inc"; do
            if [ -f "$try_path" ]; then
                resolved=$(realpath --relative-to=. "$try_path")
                break
            fi
        done

        if [ -n "$resolved" ] && [ -z "${visited[$resolved]+x}" ]; then
            queue+=("$resolved")

            # Also add the .cpp counterpart if it exists (it's part of the compilation unit)
            cpp_file=$(echo "$resolved" | sed 's/\.h$/.cpp/')
            if [ -f "$cpp_file" ] && [ -z "${visited[$cpp_file]+x}" ]; then
                queue+=("$cpp_file")
            fi
        fi
    done
done

# Now check all headers in src/ against the reachable set
all_headers=$(find src/ -name "*.h" | sort)
total=$(echo "$all_headers" | wc -l)
dead_count=0
test_only_count=0

for header in $all_headers; do
    header=$(realpath --relative-to=. "$header")
    if [ -z "${visited[$header]+x}" ]; then
        test_includers=$(grep -rl "#include.*$(basename "$header")" tests/ \
            --include="*.cpp" --include="*.h" 2>/dev/null | wc -l || true)

        if [ "$test_includers" -gt 0 ]; then
            echo -e "${YELLOW}TEST-ONLY${NC}  $header  (included by $test_includers test file(s))"
            test_only_count=$((test_only_count + 1))
        else
            echo -e "${RED}DEAD${NC}       $header  (unreachable from $ENTRY)"
            dead_count=$((dead_count + 1))
        fi
    fi
done

if [ "$dead_count" -eq 0 ] && [ "$test_only_count" -eq 0 ]; then
    echo -e "${GREEN}No dead code found.${NC} All $total headers are reachable from $ENTRY."
else
    echo ""
    echo "Summary: $dead_count dead, $test_only_count test-only (out of $total headers)"
fi
