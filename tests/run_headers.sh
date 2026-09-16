#!/bin/sh

# Enforces the self-containment promise made at the top of include/pgl.hpp: a
# user may include any single header directly and get the same result as
# including pgl.hpp. Two independent things have to hold for that, and each one
# catches breakages the other cannot see:
#
#   chain   Every header includes its immediate predecessor in pgl.hpp's order,
#           so its transitive closure covers every header listed before it.
#   compile Every header parses on its own, in a translation unit that includes
#           nothing else.
#
# The compile sweep alone is not enough. A header that reaches back into the
# order too early still compiles standalone -- everything it names is declared
# -- while silently dropping the headers it skipped. That is how
# implementation/closest.hpp lost the Segment-Triangle distanceL1 overloads and
# algorithm/arrangement.hpp lost Triangulation::visibilityGraph: both compiled
# clean and failed only when a user called the missing member. The chain check
# is what sees those.

set -eu

CXX="${CXX:-c++}"
CXXFLAGS="${CXXFLAGS:--std=c++20 -Wall -Wextra -Werror -pedantic}"

jobs="${PGL_JOBS:-$(nproc 2>/dev/null || echo 4)}"

work="build/tests/headers"
failures_file="$work/failures.txt"

usage() {
    echo "Usage: $0 [--list] [--chain-only] [--compile-only] [header ...]"
    echo
    echo "Examples:"
    echo "  $0"
    echo "  $0 shape/point.hpp"
    echo "  $0 --chain-only"
    echo "  CXX=clang++ $0"
}

# Every header the promise covers. third_party/ is vendored and excluded: it is
# not part of the layered order and does not carry the promise.
list_headers() {
    find include -name '*.hpp' ! -path 'include/third_party/*' | sort
}

# Reads the include order straight out of pgl.hpp rather than a second list kept
# here, so adding a header to the umbrella is all it takes to have it enforced.
#
# Covers fewer headers than the compile sweep, and should: the per-relation
# predicate files (contains.hpp, separates.hpp, ...) are pulled in by their
# aggregator predicates.hpp and are deliberately not umbrella-order entries, so
# they have no predecessor to check. They still have to compile standalone.
check_chain() {
    python3 - <<'PY'
import re
import pathlib
import sys

root = pathlib.Path("include")
order = [
    line.split('"')[1]
    for line in (root / "pgl.hpp").read_text().splitlines()
    if line.startswith('#include "')
]

def includes(header):
    path = root / header
    if not path.exists():
        return []
    return re.findall(r'^#include "([^"]+)"', path.read_text(), re.M)

def closure(header):
    seen = set()
    stack = list(includes(header))
    while stack:
        current = stack.pop()
        if current in seen:
            continue
        seen.add(current)
        stack.extend(includes(current))
    return seen

broken = 0
for index, header in enumerate(order):
    missing = [p for p in order[:index] if p not in closure(header)]
    if not missing:
        continue
    broken += 1
    expected = order[index - 1]
    print(f"CHAIN FAIL {header}")
    print(f"    does not reach {len(missing)} header(s) listed before it in pgl.hpp:")
    for name in missing:
        print(f"        {name}")
    print(f"    fix: include \"{expected}\" (its immediate predecessor)")

if broken:
    print(f"\n{broken} header(s) break the include chain.")
    sys.exit(1)

print(f"chain: {len(order)} headers, every predecessor reachable")
PY
}

# Compiles a generated translation unit per header rather than the header
# itself: `$CXX -x c++ some_header.hpp` trips -Wpragma-once-outside-header,
# which the default -Werror turns into a failure for every header in the repo.
# A generated TU is also what a user actually writes.
compile_one() {
    header="$1"
    relative="${header#include/}"
    stem=$(printf '%s' "$relative" | tr '/.' '__')
    source="$work/tu/$stem.cpp"
    log="$work/log/$stem.txt"

    printf '#include "%s"\nint main() { return 0; }\n' "$relative" > "$source"

    if $CXX $CXXFLAGS -Iinclude -fsyntax-only "$source" > "$log" 2>&1; then
        printf 'ok   %s\n' "$relative"
    else
        printf 'FAIL %s\n' "$relative"
        sed 's/^/    /' "$log"
        printf '%s\n' "$relative" >> "$failures_file"
    fi
}

# Re-entry point for the parallel sweep below; keeps compile_one in one place
# instead of duplicating it into the xargs child.
if [ "${1:-}" = "--compile-one" ]; then
    shift
    mkdir -p "$work/tu" "$work/log"
    compile_one "$1"
    exit 0
fi

run_chain=1
run_compile=1
selected=""

while [ $# -gt 0 ]; do
    case "$1" in
        -h|--help) usage; exit 0 ;;
        --list) list_headers; exit 0 ;;
        --chain-only) run_compile=0 ;;
        --compile-only) run_chain=0 ;;
        -*) echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
        *) selected="$selected $1" ;;
    esac
    shift
done

mkdir -p "$work/tu" "$work/log"
: > "$failures_file"

status=0

if [ "$run_chain" = "1" ] && [ -z "$selected" ]; then
    echo "== include chain =="
    check_chain || status=1
    echo
fi

if [ "$run_compile" = "1" ]; then
    echo "== standalone compile ($CXX) =="

    if [ -n "$selected" ]; then
        headers=""
        for name in $selected; do
            # Accept both `shape/point.hpp` and `include/shape/point.hpp`.
            candidate="include/${name#include/}"
            if [ ! -f "$candidate" ]; then
                echo "No such header: $name" >&2
                exit 2
            fi
            headers="$headers $candidate"
        done
        printf '%s\n' $headers | xargs -P "$jobs" -I{} "$0" --compile-one {}
    else
        list_headers | xargs -P "$jobs" -I{} "$0" --compile-one {}
    fi

    count=$(list_headers | wc -l | tr -d ' ')
    if [ -s "$failures_file" ]; then
        failed=$(wc -l < "$failures_file" | tr -d ' ')
        echo
        echo "$failed header(s) failed to compile standalone:"
        sed 's/^/    /' "$failures_file"
        status=1
    elif [ -z "$selected" ]; then
        echo
        echo "compile: all $count headers parse standalone"
    fi
fi

exit $status
