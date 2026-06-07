#!/usr/bin/env bash

set -Eeuo pipefail
shopt -s nullglob

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
            echo "Supported values: int, double, rational, rational60"
            exit 1
        fi
    done
fi

cpu_model="$(lscpu 2>/dev/null | awk -F: '/Model name/ {$1=""; sub(/^ +/, ""); print; exit}' || true)"
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
