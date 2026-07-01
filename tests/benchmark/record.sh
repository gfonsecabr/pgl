#!/usr/bin/env bash
#
# Record a benchmark run into the versioned history (tests/benchmark/history/)
# and push it to GitHub, where the Pages workflow rebuilds the dashboard.
#
#   bash tests/benchmark/record.sh                     # full cube + all extra
#   bash tests/benchmark/record.sh --pairs-only        # skip the extra benchmarks
#   bash tests/benchmark/record.sh --extra-only        # only the extra benchmarks
#   bash tests/benchmark/record.sh --shapes Segment,Triangle --methods intersects
#   bash tests/benchmark/record.sh --focus Polygon     # Polygon vs everything (row+col)
#
# Refuses to run with uncommitted changes to tracked files, so every measurement
# maps to a real commit — the dashboard's x-axis is the commit date, not the run
# date. Untracked files (scratch sources, build output) are allowed and ignored.
#
# Pass-through filters for the shape-pair cube: --shapes, --focus, --sizes,
# --methods, --types (comma lists; see run_shapepairs.py). Other options:
#   --pairs-only / --extra-only   run only one half
#   --repetitions N               samples per program; median kept (default: 3)
#   --no-push                     commit locally but do not push
# Override the compiler/flags as usual:  CXX=g++ CXXFLAGS="-std=c++23 -O3" ...

set -Eeuo pipefail

root="$(git rev-parse --show-toplevel)"
cd "$root"

# ── Parse options ────────────────────────────────────────────────────────────
pairs=1
extra=1
push=1
repetitions=3
pair_args=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        --pairs-only) extra=0; shift ;;
        --extra-only) pairs=0; shift ;;
        --no-push)    push=0;  shift ;;
        --repetitions) repetitions="$2"; shift 2 ;;
        --repetitions=*) repetitions="${1#*=}"; shift ;;
        --shapes|--focus|--sizes|--methods|--types)
            pair_args+=("$1" "$2"); shift 2 ;;
        --shapes=*|--focus=*|--sizes=*|--methods=*|--types=*)
            pair_args+=("$1"); shift ;;
        -h|--help)
            awk 'NR>1 && /^#/ {sub(/^# ?/,""); print; next} NR>1 {exit}' "$0"
            exit 0 ;;
        *)
            echo "error: unknown option '$1'" >&2
            echo "Run 'bash tests/benchmark/record.sh --help' for usage." >&2
            exit 2 ;;
    esac
done

if [[ -n "$(git status --porcelain --untracked-files=no)" ]]; then
    echo "error: tracked files have uncommitted changes." >&2
    echo "Commit them first — benchmarks are tagged to the current commit." >&2
    exit 1
fi

CXX="${CXX:-c++}"
CXXFLAGS="${CXXFLAGS:--std=c++23 -O3 -DNDEBUG}"
jobs="$(nproc 2>/dev/null || echo 1)"
export CXX CXXFLAGS

pairs_json="build/tests/benchmark/benchmarks.json"
extra_json="build/tests/benchmark/extra/extra.json"

history_args=()

# ── Run the shape-pair cube ──────────────────────────────────────────────────
if [[ "$pairs" -eq 1 ]]; then
    echo "::group::Shape-pair cube"
    python3 tests/benchmark/run_shapepairs.py \
        --jobs "$jobs" --repetitions "$repetitions" \
        --output "$pairs_json" "${pair_args[@]}"
    echo "::endgroup::"
else
    history_args+=(--skip-pairs)
fi

# ── Run the whole-algorithm extra benchmarks ─────────────────────────────────
if [[ "$extra" -eq 1 ]]; then
    echo "::group::Extra benchmarks"
    python3 tests/benchmark/run_extra.py \
        --repetitions "$repetitions" --output "$extra_json"
    echo "::endgroup::"
else
    history_args+=(--skip-extra)
fi

# ── Append to the versioned history ──────────────────────────────────────────
python3 tests/benchmark/to_history.py \
    --pairs "$pairs_json" --extra "$extra_json" \
    --history tests/benchmark/history "${history_args[@]}"

echo

# Commit the refreshed history (and push). The clean-tree guard above guarantees
# the only tracked changes are the jsonl files written just now, so staging the
# history directory can't sweep up anything unrelated.
git add tests/benchmark/history
if git diff --cached --quiet; then
    echo "History unchanged — nothing to commit."
    exit 0
fi

git commit -m "Benchmark"
if [[ "$push" -eq 1 ]]; then
    git push
    echo "History committed and pushed."
else
    echo "History committed (not pushed; --no-push)."
fi
