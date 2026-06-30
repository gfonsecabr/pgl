#!/bin/sh

set -eu

CXX="${CXX:-c++}"
CXXFLAGS="${CXXFLAGS:--std=c++20 -Wall -Wextra -pedantic}"

mkdir -p build/tests/bin build/tests/reports build/tests/output

# Records the source path of every test that fails. Written by run_one_test
# (possibly from several parallel processes via the --run-one entry point) and
# read once at the end to print a summary when not running under CI.
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

    echo "::group::Build $test_name"
    if ! $CXX $CXXFLAGS -Iinclude -Itests/unit "$source" -o "$binary"; then
        echo "::endgroup::"
        printf '%s\n' "$source" >> "$failures_file"
        return 1
    fi
    echo "::endgroup::"

    echo "::group::Run $test_name"
    if ! "./$binary" --order-by=name --duration=true --no-breaks=true; then
        status=1
    fi
    echo "::endgroup::"

    if ! "./$binary" --reporters=junit --out="$report" >/dev/null 2>&1; then
        status=1
    fi

    if [ "$status" -ne 0 ]; then
        printf '%s\n' "$source" >> "$failures_file"
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

# Outside CI, end with a plain list of the test files that failed. The
# GITHUB_ACTIONS guard keeps the Action's log output byte-for-byte unchanged.
if [ "${GITHUB_ACTIONS:-}" != "true" ]; then
    if [ -s "$failures_file" ]; then
        echo
        echo "Failed test files:"
        sort -u "$failures_file" | while IFS= read -r failed_source; do
            printf '  %s\n' "$failed_source"
        done
    fi
fi

exit "$overall_status"
