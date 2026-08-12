#!/bin/sh

# Builds and runs the property harness. Not wired into tests/run_tests.sh and not
# part of CI: a randomized search takes as long as you give it and its findings
# depend on the seed, neither of which belongs in the path of every commit. See
# tests/property/README.md.
#
# Every argument is passed through to the binary, so:
#
#   sh tests/property/run.sh                          # default run, checked against the baseline
#   sh tests/property/run.sh --cases 20000            # longer search
#   sh tests/property/run.sh --group predicates       # one group
#   sh tests/property/run.sh --property crosses       # one property
#   sh tests/property/run.sh --seed 12345 --verbose   # another seed, with per-property counts
#   sh tests/property/run.sh --update-baseline        # accept the current failures as known
#   sh tests/property/run.sh --list                   # what exists

set -eu

CXX="${CXX:-c++}"
# -O2 because the boolean identities compute in Rational<BigInt>, which is some
# fifty times slower than int; assertions stay on deliberately, since a reachable
# one is a finding (the harness catches the abort and reports it).
CXXFLAGS="${CXXFLAGS:--std=c++20 -O2 -Wall -Wextra -Werror -pedantic}"

root="$(CDPATH='' cd -- "$(dirname -- "$0")/../.." && pwd)"
binary="$root/build/property/property"
baseline="$root/tests/property/known_failures.txt"

mkdir -p "$root/build/property"

# The harness is header-only apart from main.cpp, so this is the whole build. The
# Windows identifier traps are force-included for the same reason the unit suite
# does it: the harness may one day move under tests/unit, and the names it would
# have to rename are cheaper to avoid now than to find later.
if ! $CXX $CXXFLAGS -I"$root/include" -I"$root/tests/property" -I"$root/tests/unit" \
        -include windows_traps.hpp \
        "$root/tests/property/main.cpp" -o "$binary"; then
    echo "Build failed." >&2
    exit 1
fi

# Check against the recorded failures unless the caller says otherwise, so a run
# with no arguments answers the useful question: is anything failing that was not
# already known to fail?
case " $* " in
    *" --baseline "*) exec "$binary" "$@" ;;
    *) exec "$binary" --baseline "$baseline" "$@" ;;
esac
