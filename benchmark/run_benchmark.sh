#!/usr/bin/env bash

set -Eeuo pipefail
shopt -s nullglob

usage() {
    cat <<'EOF'
Usage: bash benchmark/run_benchmark.sh [-n NUMBERS] [suite...]

Compiles each benchmark/<suite>/*.cpp, runs it, and writes per-suite reports
plus an aggregated JSON and markdown summary under build/benchmark/.

With no arguments, every suite under benchmark/*/ is built and run. Otherwise
each argument selects a suite by basename (e.g. convex_convex) or by path
(e.g. benchmark/convex/convex_convex.cpp). Explicit arguments override
PGL_BENCHMARK_SOURCES.

Options:
  -n, --numbers NUMBERS  Comma/space list of number families to run; overrides
                         PGL_BENCHMARK_NUMBERS. One or more of: int, double,
                         rational, rational60, rationalbigint, rationalbigint60
  -h, --help             Show this help and exit

Configuration is otherwise through environment variables:

  CXX                          Compiler to use            (default: c++)
  CXXFLAGS                     Compile flags              (default: -std=c++23 -O3 -DNDEBUG -Wall -Wextra -pedantic)
  PGL_BENCHMARK_SOURCES        Space-separated list of .cpp suites to build
                               (default: all benchmark/*/*.cpp, excluding support/)
  PGL_BENCHMARK_NUMBERS        Comma/space list of number families to run; one or
                               more of: int, double, rational, rational60,
                               rationalbigint, rationalbigint60
  PGL_BENCHMARK_REPETITIONS    Positive integer, times to run each suite (default: 1)
  PGL_BENCHMARK_REPORT_DIR     Where per-run reports are written
                               (default: build/benchmark/current-reports)
  PGL_BENCHMARK_JSON           Aggregated JSON output path
                               (default: build/benchmark/benchmark.json)
  PGL_BENCHMARK_SUMMARY        Markdown summary output path
                               (default: build/benchmark/benchmark.md)

Examples:
  bash benchmark/run_benchmark.sh
  bash benchmark/run_benchmark.sh convex_convex
  bash benchmark/run_benchmark.sh -n int,rational convex_convex
  PGL_BENCHMARK_NUMBERS="int,rational" bash benchmark/run_benchmark.sh
  CXX=clang++ CXXFLAGS="-std=c++20 -O2 -Wall" bash benchmark/run_benchmark.sh
EOF
}

# Parse options and positional suite arguments.
positional=()
while [[ $# -gt 0 ]]; do
    case "$1" in
        -h | --help)
            usage
            exit 0
            ;;
        -n | --numbers)
            if [[ $# -lt 2 ]]; then
                echo "error: $1 requires an argument" >&2
                exit 2
            fi
            PGL_BENCHMARK_NUMBERS="$2"
            shift 2
            ;;
        -n=* | --numbers=*)
            PGL_BENCHMARK_NUMBERS="${1#*=}"
            shift
            ;;
        --)
            shift
            positional+=("$@")
            break
            ;;
        -*)
            echo "error: unknown option '$1'" >&2
            echo "Run 'bash benchmark/run_benchmark.sh --help' for usage." >&2
            exit 2
            ;;
        *)
            positional+=("$1")
            shift
            ;;
    esac
done

# Resolve any positional suite arguments (basename or path) to sources, mirroring
# benchmark/record.sh. Explicit arguments override PGL_BENCHMARK_SOURCES.
if [[ ${#positional[@]} -gt 0 ]]; then
    arg_sources=()
    for name in "${positional[@]}"; do
        if [[ -f "$name" ]]; then
            arg_sources+=("$name")
            continue
        fi
        matches=(benchmark/*/"$name".cpp)
        if [[ ${#matches[@]} -eq 0 ]]; then
            echo "error: no benchmark suite named '$name'" >&2
            echo "Run 'bash benchmark/run_benchmark.sh --help' for usage." >&2
            exit 2
        fi
        arg_sources+=("${matches[@]}")
    done
    PGL_BENCHMARK_SOURCES="${arg_sources[*]}"
fi

# Export so the benchmark binary (which reads it via getenv) sees a value passed
# through -n as well as one inherited from the environment.
if [[ -n "${PGL_BENCHMARK_NUMBERS:-}" ]]; then
    export PGL_BENCHMARK_NUMBERS
fi

CXX="${CXX:-c++}"
CXXFLAGS="${CXXFLAGS:--std=c++23 -O3 -DNDEBUG -Wall -Wextra -pedantic}"
PGL_BENCHMARK_REPETITIONS="${PGL_BENCHMARK_REPETITIONS:-1}"
PGL_BENCHMARK_REPORT_DIR="${PGL_BENCHMARK_REPORT_DIR:-build/benchmark/current-reports}"
PGL_BENCHMARK_JSON="${PGL_BENCHMARK_JSON:-build/benchmark/benchmark.json}"
PGL_BENCHMARK_SUMMARY="${PGL_BENCHMARK_SUMMARY:-build/benchmark/benchmark.md}"

bin_dir="build/benchmark/bin"

canonical_number_filter() {
    local value="${1,,}"
    value="${value//[^a-z0-9]/}"

    case "$value" in
        int) echo "int" ;;
        double) echo "double" ;;
        rational|rationali64|rational64) echo "rational" ;;
        rational60) echo "rational60" ;;
        rationalbigint|rationalbi|bigint) echo "rationalbigint" ;;
        rationalbigint60|bigint60) echo "rationalbigint60" ;;
        *) echo "" ;;
    esac
}

mkdir -p "$bin_dir" "$PGL_BENCHMARK_REPORT_DIR" "$(dirname "$PGL_BENCHMARK_JSON")" "$(dirname "$PGL_BENCHMARK_SUMMARY")"
rm -f "$PGL_BENCHMARK_REPORT_DIR"/*.txt

if [[ -n "${PGL_BENCHMARK_SOURCES:-}" ]]; then
    read -r -a sources <<< "$PGL_BENCHMARK_SOURCES"
else
    sources=(benchmark/*/*.cpp)
fi

filtered_sources=()
for source in "${sources[@]}"; do
    if [[ "$source" == benchmark/support/* ]]; then
        continue
    fi
    filtered_sources+=("$source")
done

if [[ "${#filtered_sources[@]}" -eq 0 ]]; then
    echo "No benchmark .cpp file found!"
    exit 1
fi

if ! [[ "$PGL_BENCHMARK_REPETITIONS" =~ ^[1-9][0-9]*$ ]]; then
    echo "PGL_BENCHMARK_REPETITIONS must be a positive integer"
    exit 1
fi

if [[ -n "${PGL_BENCHMARK_NUMBERS:-}" ]]; then
    read -r -a requested_numbers <<< "${PGL_BENCHMARK_NUMBERS//,/ }"
    for requested_number in "${requested_numbers[@]}"; do
        if [[ -z "$(canonical_number_filter "$requested_number")" ]]; then
            echo "Unknown benchmark number family: $requested_number"
            echo "Supported values: int, double, rational, rational60, rationalbigint, rationalbigint60"
            exit 1
        fi
    done
fi

# LC_ALL=C so the "Model name" label is not localized; fall back to cpuinfo.
cpu_model="$(LC_ALL=C lscpu 2>/dev/null | awk -F: '/Model name/ {$1=""; sub(/^ +/, ""); print; exit}' || true)"
if [[ -z "$cpu_model" ]]; then
    cpu_model="$(awk -F: '/model name/ {gsub(/^[ \t]+/, "", $2); print $2; exit}' /proc/cpuinfo 2>/dev/null || true)"
fi
commit="$(git rev-parse --short HEAD 2>/dev/null || true)"
commit="${commit:-unknown}"

for source in "${filtered_sources[@]}"; do
    if [[ ! -f "$source" ]]; then
        echo "Benchmark source not found: $source"
        exit 1
    fi

    suite="$(basename "$source" .cpp)"
    binary="$bin_dir/$suite"

    echo "::group::Build $suite"
    "$CXX" $CXXFLAGS -Iinclude "$source" -o "$binary"
    echo "::endgroup::"

    for ((repetition = 1; repetition <= PGL_BENCHMARK_REPETITIONS; ++repetition)); do
        report="$PGL_BENCHMARK_REPORT_DIR/$suite.run$repetition.txt"

        echo "::group::Run $suite ($repetition/$PGL_BENCHMARK_REPETITIONS)"
        {
            echo "# suite: $suite"
            echo "# source: $source"
            echo "# repetition: $repetition"
            echo "# compiler: $CXX"
            echo "# cxxflags: $CXXFLAGS"
            echo "# commit: $commit"
            if [[ -n "$cpu_model" ]]; then
                echo "# cpu: $cpu_model"
            fi
            "$binary"
        } | tee "$report"
        echo "::endgroup::"
    done
done

python3 benchmark/support/benchmark_to_json.py \
    --reports "$PGL_BENCHMARK_REPORT_DIR" \
    --output "$PGL_BENCHMARK_JSON" \
    --summary "$PGL_BENCHMARK_SUMMARY"

echo "Benchmark JSON written to $PGL_BENCHMARK_JSON"
echo "Benchmark summary written to $PGL_BENCHMARK_SUMMARY"
