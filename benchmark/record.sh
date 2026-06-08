#!/usr/bin/env bash
#
# Record a benchmark run into the versioned history (benchmark/history/*.jsonl).
#
#   bash benchmark/record.sh                  # all suites
#   bash benchmark/record.sh segment_segment  # one suite (basename or path)
#
# Refuses to run with a dirty working tree so every measurement maps to a real
# commit — the dashboard's x-axis is the commit date, not the run date.
# Override the compiler/flags as usual:  CXX=g++ CXXFLAGS="-std=c++23 -O3" ...

set -Eeuo pipefail
shopt -s nullglob

root="$(git rev-parse --show-toplevel)"
cd "$root"

if [[ -n "$(git status --porcelain)" ]]; then
    echo "error: working tree has uncommitted changes." >&2
    echo "Commit them first — benchmarks are tagged to the current commit." >&2
    exit 1
fi

# Resolve requested suites (by basename or path) to sources for the runner.
if [[ $# -gt 0 ]]; then
    sources=()
    for name in "$@"; do
        if [[ -f "$name" ]]; then
            sources+=("$name")
            continue
        fi
        matches=(benchmark/*/"$name".cpp)
        if [[ ${#matches[@]} -eq 0 ]]; then
            echo "error: no benchmark suite named '$name'" >&2
            exit 1
        fi
        sources+=("${matches[@]}")
    done
    export PGL_BENCHMARK_SOURCES="${sources[*]}"
fi

report_dir="${PGL_BENCHMARK_REPORT_DIR:-build/benchmark/current-reports}"
export PGL_BENCHMARK_REPORT_DIR="$report_dir"

# Reuse the existing runner: it compiles, runs, and writes reports carrying the
# commit / cpu / compiler metadata under $report_dir.
bash benchmark/run_benchmark.sh

# Convert those reports into the versioned, per-suite history.
python3 benchmark/support/record_to_history.py \
    --reports "$report_dir" \
    --history benchmark/history

echo
echo "History updated. Review and commit benchmark/history/*.jsonl, then push."
