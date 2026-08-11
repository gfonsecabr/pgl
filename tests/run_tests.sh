#!/bin/sh

set -eu

CXX="${CXX:-c++}"
CXXFLAGS="${CXXFLAGS:--std=c++20 -Wall -Wextra -Werror -pedantic}"

# Force-included into every test build so g++ and clang reject the Windows
# identifier traps -- `far`/`near`, the wingdi.h function names -- that otherwise
# only the MSVC job can see, half an hour after a push. Kept out of CXXFLAGS on
# purpose: overriding the flags to try another standard or optimization level
# must not silently switch the check off. Set PGL_NO_WINDOWS_TRAPS=1 for that.
# See tests/unit/windows_traps.hpp.
windows_traps=""
if [ "${PGL_NO_WINDOWS_TRAPS:-0}" != "1" ]; then
    windows_traps="-include windows_traps.hpp"
fi

mkdir -p build/tests/bin build/tests/reports build/tests/output

# Records the source path of every test that fails. Written by run_one_test
# (possibly from several parallel processes via the --run-one entry point) and
# read once at the end to print a summary.
failures_file="build/tests/output/failures.txt"

usage() {
    echo "Usage: $0 [--list] [test-name-or-path ...]"
    echo
    echo "Examples:"
    echo "  $0"
    echo "  $0 rational"
    echo "  $0 segment segment_segment"
    echo "  $0 tests/unit/rational.cpp tests/unit/segment_segment.cpp"
}

list_tests() {
    for source in tests/unit/*.cpp; do
        if [ -f "$source" ]; then
            printf '%s\n' "${source#tests/}"
        fi
    done
}

run_one_test() {
    source="$1"
    status=0
    test_name="$(basename "$source" .cpp)"
    binary="build/tests/bin/$test_name"
    report="build/tests/reports/$test_name.junit.xml"
    build_output="build/tests/output/$test_name.build.log"
    run_output="build/tests/output/$test_name.run.log"

    if ! $CXX $CXXFLAGS -Iinclude -Itests/unit $windows_traps "$source" -o "$binary" >"$build_output" 2>&1; then
        printf '%s\n' "$source" >> "$failures_file"
        printf 'FAIL %s (build; see %s)\n' "$source" "$build_output"
        return 1
    fi

    if ! "./$binary" --order-by=name --duration=true --no-breaks=true >"$run_output" 2>&1; then
        status=1
    fi

    if ! "./$binary" --reporters=junit --out="$report" >/dev/null 2>&1; then
        status=1
    fi

    if [ "$status" -ne 0 ]; then
        printf '%s\n' "$source" >> "$failures_file"
        printf 'FAIL %s (run; see %s)\n' "$source" "$run_output"
    else
        printf 'PASS %s\n' "$source"
    fi

    return "$status"
}

resolve_test() {
    target="$1"

    for candidate in \
        "$target" \
        "$target.cpp" \
        "tests/$target" \
        "tests/$target.cpp" \
        "tests/unit/$target.cpp"
    do
        if [ -f "$candidate" ]; then
            printf '%s\n' "$candidate"
            return 0
        fi
    done

    matches="$(find tests/unit -maxdepth 1 -type f -name "*$target*.cpp" | sort)"
    match_count="$(printf '%s\n' "$matches" | sed '/^$/d' | wc -l)"

    if [ "$match_count" -eq 1 ]; then
        printf '%s\n' "$matches"
        return 0
    fi

    if [ "$match_count" -gt 1 ]; then
        echo "Test name '$target' is ambiguous. Matches:" >&2
        printf '%s\n' "$matches" >&2
        return 1
    fi

    echo "Unknown test '$target'." >&2
    usage >&2
    return 1
}

if [ "${1:-}" = "--help" ] || [ "${1:-}" = "-h" ]; then
    usage
    exit 0
fi

if [ "${1:-}" = "--list" ] || [ "${1:-}" = "-l" ]; then
    list_tests
    exit 0
fi

# Internal entry point: build and run a single resolved test source. Used to
# dispatch one test per job under GNU parallel; not meant for direct use.
if [ "${1:-}" = "--run-one" ]; then
    run_one_test "$2"
    exit "$?"
fi

if [ "$#" -eq 0 ]; then
    set -- tests/unit/*.cpp
else
    resolved_sources=""
    for target in "$@"; do
        resolved_sources="$resolved_sources $(resolve_test "$target")"
    done
    set -- $resolved_sources
fi

sources=""
for source in "$@"; do
    [ -f "$source" ] || continue
    sources="$sources $source"
done

if [ -z "$sources" ]; then
    echo "No .cpp files found in tests/unit/"
    exit 1
fi

overall_status=0

# Start each run with an empty failures log. Children spawned via --run-one
# only append, so this reset must happen here in the parent, never in --run-one.
: > "$failures_file"

# Count the resolved sources so a single test skips the parallel machinery.
source_count="$(printf '%s\n' $sources | wc -l)"

if [ "$source_count" -gt 1 ] && command -v parallel >/dev/null 2>&1; then
    # Build and run each test as its own parallel job. -k (--keep-order) makes
    # parallel buffer each job and emit output in input order, so the result is
    # identical to the sequential run below. parallel exits non-zero if any job
    # failed. CXX/CXXFLAGS are inherited through the environment.
    printf '%s\n' $sources | parallel -k --will-cite sh "$0" --run-one || overall_status=$?
else
    # A single test (or no GNU parallel available) runs sequentially, avoiding
    # parallel's per-job buffering and startup overhead.
    for source in $sources; do
        run_one_test "$source" || overall_status=1
    done
fi

passed_count=$((source_count - $(sort -u "$failures_file" | sed '/^$/d' | wc -l)))
failed_count="$(sort -u "$failures_file" | sed '/^$/d' | wc -l)"
failed_tests="$(sort -u "$failures_file" | sed '/^$/d' | paste -sd, -)"
if [ "$failed_count" -gt 0 ]; then
    printf 'Summary: %s passed, %s failed, %s total (failed: %s)\n' \
        "$passed_count" "$failed_count" "$source_count" "$failed_tests"
else
    printf 'Summary: %s passed, %s failed, %s total\n' \
        "$passed_count" "$failed_count" "$source_count"
fi

exit "$overall_status"
