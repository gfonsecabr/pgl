#!/usr/bin/env bash
#
# Record a benchmark run into the versioned history (tests/benchmark/history/)
# and push it to GitHub, where the Pages workflow rebuilds the dashboard.
#
#   bash tests/benchmark/record.sh                     # full cube + all asymptotic
#   bash tests/benchmark/record.sh --pairs-only        # skip the asymptotic benchmarks
#   bash tests/benchmark/record.sh --asymptotic-only   # only the asymptotic benchmarks
#   bash tests/benchmark/record.sh --asymptotic-only --asymptotic=triangulation --baseline
#   bash tests/benchmark/record.sh --shapes Segment,Triangle --methods intersects
#   bash tests/benchmark/record.sh --focus Polygon     # Polygon vs everything (row+col)
#   bash tests/benchmark/record.sh --rev 3fbc199       # an old library, today's benchmarks
#
# Refuses to run with uncommitted changes to tracked files, so every measurement
# maps to a real commit — the dashboard's x-axis is the commit date, not the run
# date. Untracked files (scratch sources, build output) are allowed and ignored.
#
# Pass-through filters for the shape-pair cube: --shapes, --focus, --sizes,
# --sizes-a, --sizes-b (independent size lists per operand, overriding --sizes),
# --methods, --types (comma lists; see run_shapepairs.py). Other options:
#   --fraction N/D                 run only method-group N of D, split by
#                                 estimated cost (compile + measured run time)
#                                 so D roughly-equal overnight runs cover the
#                                 full cube between them. Run 1/D first, then
#                                 2/D, ... D/D in order (any commits in between
#                                 are fine); each grouping is cached in
#                                 build/tests/benchmark/fraction_cache.json so
#                                 the D runs partition the methods exactly,
#                                 even though history — and so the estimated
#                                 costs — changes between them. Re-running 1/D
#                                 starts a fresh cycle. Cannot be combined with
#                                 --methods. See split_methods.py for the cost
#                                 model. Has no effect with --asymptotic-only.
#   --pairs-only / --asymptotic-only
#                                 run only one half
#   --asymptotic NAMES            limit the asymptotic run to a comma-separated
#                                 list of driver names (for example,
#                                 triangulation,arrangement). Also limits the
#                                 CGAL baseline when --baseline is present;
#                                 existing baseline categories are retained.
#   --repetitions N               samples per program; median kept (default: 3)
#   --baseline                    also build and run the CGAL reference drivers
#                                 (tests/benchmark/asymptotic/baseline/). Off by
#                                 default: CGAL is not on every dev machine or
#                                 CI box, and a baseline is a reference point
#                                 rather than a measurement of this commit — it
#                                 overwrites history/asymptotic-baseline.json
#                                 instead of being appended to a history.
#   --no-push                     commit locally but do not push
#   --rev COMMIT                  measure an older library version. The commit is
#                                 checked out into a throwaway worktree, today's
#                                 benchmarks are run against its headers, and the
#                                 results are recorded under that commit — so a
#                                 backfilled point is comparable with the ones
#                                 recorded from here on, which a run of the old
#                                 benchmarks would not be. This worktree is never
#                                 modified. Cells the old headers cannot compile
#                                 are reported and skipped, so filling in a very
#                                 old commit may leave holes.
# Override the compiler/flags as usual:  CXX=g++ CXXFLAGS="-std=c++23 -O2" ...

set -Eeuo pipefail

root="$(git rev-parse --show-toplevel)"
cd "$root"

# ── Parse options ────────────────────────────────────────────────────────────
pairs=1
asymptotic=1
baseline=0
push=1
repetitions=3
rev=""
fraction=""
asymptotic_filter=0
asymptotic_names=""
shapes_opt=""
types_opt=""
methods_given=0
pair_args=()
asymptotic_drivers=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        --pairs-only) asymptotic=0; shift ;;
        --asymptotic-only) pairs=0; shift ;;
        --asymptotic)
            asymptotic_filter=1
            asymptotic_names="$2"
            shift 2
            ;;
        --asymptotic=*)
            asymptotic_filter=1
            asymptotic_names="${1#*=}"
            shift
            ;;
        --baseline)   baseline=1; shift ;;
        --no-push)    push=0;  shift ;;
        --repetitions) repetitions="$2"; shift 2 ;;
        --repetitions=*) repetitions="${1#*=}"; shift ;;
        --rev)        rev="$2"; shift 2 ;;
        --rev=*)      rev="${1#*=}"; shift ;;
        --fraction)   fraction="$2"; shift 2 ;;
        --fraction=*) fraction="${1#*=}"; shift ;;
        --shapes)     shapes_opt="$2"; pair_args+=("$1" "$2"); shift 2 ;;
        --shapes=*)   shapes_opt="${1#*=}"; pair_args+=("$1"); shift ;;
        --types)      types_opt="$2"; pair_args+=("$1" "$2"); shift 2 ;;
        --types=*)    types_opt="${1#*=}"; pair_args+=("$1"); shift ;;
        --methods)    methods_given=1; pair_args+=("$1" "$2"); shift 2 ;;
        --methods=*)  methods_given=1; pair_args+=("$1"); shift ;;
        --focus|--sizes|--sizes-a|--sizes-b)
            pair_args+=("$1" "$2"); shift 2 ;;
        --focus=*|--sizes=*|--sizes-a=*|--sizes-b=*)
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

if [[ "$asymptotic_filter" -eq 1 ]]; then
    if [[ -z "$asymptotic_names" ]]; then
        echo "error: --asymptotic needs at least one driver name." >&2
        exit 2
    fi
    IFS=',' read -r -a asymptotic_drivers <<< "$asymptotic_names"
    for driver in "${asymptotic_drivers[@]}"; do
        if [[ -z "$driver" ]]; then
            echo "error: --asymptotic contains an empty driver name." >&2
            exit 2
        fi
    done
    if [[ "$asymptotic" -eq 0 ]]; then
        echo "error: --asymptotic cannot be combined with --pairs-only." >&2
        exit 2
    fi
fi

if [[ -n "$fraction" ]]; then
    if [[ "$methods_given" -eq 1 ]]; then
        echo "error: --fraction and --methods are mutually exclusive (--fraction picks methods itself)." >&2
        exit 2
    fi
    if [[ "$pairs" -eq 0 ]]; then
        echo "error: --fraction has no effect with --asymptotic-only (it splits the shape-pair cube's methods)." >&2
        exit 2
    fi
fi

if [[ -n "$(git status --porcelain --untracked-files=no)" ]]; then
    echo "error: tracked files have uncommitted changes." >&2
    echo "Commit them first — benchmarks are tagged to the current commit." >&2
    exit 1
fi

CXX="${CXX:-c++}"
CXXFLAGS="${CXXFLAGS:--std=c++23 -O2 -DNDEBUG}"
jobs="$(nproc 2>/dev/null || echo 1)"
export CXX CXXFLAGS

pairs_json="build/tests/benchmark/benchmarks.json"
asymptotic_json="build/tests/benchmark/asymptotic/asymptotic.json"
baseline_json="build/tests/benchmark/asymptotic/baseline.json"

history_args=()

# ── Optionally measure an older library version ──────────────────────────────
# Both runners take everything from their own location: include/ is their script
# directory's project root, and the commit they tag results with is HEAD there.
# So checking the old commit out into a throwaway worktree and copying today's
# harness over its tests/benchmark measures old headers with new benchmarks,
# recorded under the old commit. Running the old harness instead would be the
# easier thing to do and the wrong one — its shapes, sample sizes and flags are
# not today's, so the point would not line up with anything recorded since.
#
# The snapshot JSONs and the history stay here: --output is resolved against the
# current directory, which is this worktree's root, and to_history.py runs from
# here too, so the throwaway worktree only ever supplies headers.
bench_root="$root"
if [[ -n "$rev" ]]; then
    if ! rev_commit="$(git rev-parse --verify --short "${rev}^{commit}" 2>/dev/null)"; then
        echo "error: '$rev' is not a commit." >&2
        exit 1
    fi
    tmp_root="$(mktemp -d "${TMPDIR:-/tmp}/pgl-bench-XXXXXX")"
    bench_root="$tmp_root/worktree"
    cleanup_worktree() {
        git worktree remove --force "$bench_root" >/dev/null 2>&1 || true
        rm -rf "$tmp_root"
    }
    trap cleanup_worktree EXIT
    git worktree add --detach --quiet "$bench_root" "$rev_commit"
    # Today's harness over the old one: the runners, the shape generators, the
    # timer header and the whole-algorithm drivers. Everything else in the
    # worktree — include/ above all — stays as that commit left it.
    mkdir -p "$bench_root/tests/benchmark/asymptotic/baseline"
    cp tests/benchmark/*.py tests/benchmark/*.hpp tests/benchmark/*.h \
       "$bench_root/tests/benchmark/"
    cp tests/benchmark/asymptotic/*.hpp tests/benchmark/asymptotic/*.cpp \
       "$bench_root/tests/benchmark/asymptotic/"
    cp tests/benchmark/asymptotic/baseline/* \
       "$bench_root/tests/benchmark/asymptotic/baseline/"
    echo "Measuring $rev_commit ($(git log -1 --format=%s "$rev_commit")) with today's benchmarks."
    echo
fi

# ── Split methods for --fraction ──────────────────────────────────────────────
# Always resolved against the real repo's history and cache, even under --rev:
# the split is about spreading tonight's compile+run cost, which has nothing to
# do with which commit's headers are being measured.
if [[ -n "$fraction" ]]; then
    split_args=(--fraction "$fraction" --repetitions "$repetitions" --jobs "$jobs")
    [[ -n "$shapes_opt" ]] && split_args+=(--shapes "$shapes_opt")
    [[ -n "$types_opt"  ]] && split_args+=(--types  "$types_opt")
    fraction_methods="$(python3 tests/benchmark/split_methods.py "${split_args[@]}")"
    pair_args+=(--methods "$fraction_methods")
    echo "Fraction $fraction methods: $fraction_methods"
    echo
fi

# ── Run the shape-pair cube ──────────────────────────────────────────────────
if [[ "$pairs" -eq 1 ]]; then
    echo "::group::Shape-pair cube"
    python3 "$bench_root/tests/benchmark/run_shapepairs.py" \
        --jobs "$jobs" --repetitions "$repetitions" \
        --output "$pairs_json" "${pair_args[@]}"
    echo "::endgroup::"
else
    history_args+=(--skip-pairs)
fi

# ── Run the asymptotic benchmarks ────────────────────────────────────────────
# Every driver measures the fixed size list checked into asymptotic/sizes.hpp;
# nothing here may pass --sizes, which is for calibration and would put the run's
# points at x values no other run measured.
if [[ "$asymptotic" -eq 1 ]]; then
    echo "::group::Asymptotic benchmarks"
    asymptotic_args=(--repetitions "$repetitions" --jobs "$jobs"
                     --output "$asymptotic_json" --baseline-output "$baseline_json")
    [[ "$baseline" -eq 1 ]] && asymptotic_args+=(--baseline)
    python3 "$bench_root/tests/benchmark/run_asymptotic.py" \
        "${asymptotic_drivers[@]}" "${asymptotic_args[@]}"
    echo "::endgroup::"
else
    history_args+=(--skip-asymptotic)
fi

# A stale baseline from an earlier run would otherwise be re-recorded as if it
# were this one's; to_history.py only reads the file when it is there.
if [[ "$baseline" -eq 0 ]]; then
    rm -f "$baseline_json"
elif [[ "$asymptotic_filter" -eq 1 ]]; then
    history_args+=(--merge-baseline)
fi

# ── Append to the versioned history ──────────────────────────────────────────
python3 tests/benchmark/to_history.py \
    --pairs "$pairs_json" --asymptotic "$asymptotic_json" --baseline "$baseline_json" \
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

git commit -m "Benchmark${fraction:+ ($fraction)}${rev:+ $rev_commit}"
if [[ "$push" -eq 1 ]]; then
    git push
    echo "History committed and pushed."
else
    echo "History committed (not pushed; --no-push)."
fi
