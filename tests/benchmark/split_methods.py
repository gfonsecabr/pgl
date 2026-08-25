#!/usr/bin/env python3
"""split_methods.py — split the method dimension of the shape-pair cube into
'd' time-balanced groups and print the methods belonging to group 'n'.

Used by record.sh's --fraction=n/d to spread the ~24h full cube over several
overnight runs: run --fraction=1/3 tonight, 2/3 tomorrow night, 3/3 the night
after, on whatever commit HEAD happens to be each time. Each method appears in
exactly one group, so the three runs together cover the same ground as one
full run would.

Balancing is by estimated wall-clock cost, not method count: a run's cost is
dominated first by however many generated C++ programs it has to compile
(one per shape1 x shape2 x type, whatever the method) and second by the time
actually spent calling the method. The former is approximated as a constant
--compile-seconds of CPU time per program (empirically ~5s on this repo's
headers with -O2 — recalibrate by timing a small run_shapepairs.py slice and
dividing total user-CPU time by program count), divided by --jobs since
run_shapepairs.py compiles with a thread pool of that size; the latter is
read from the recorded history in tests/benchmark/history/*.jsonl (the only
place that timing lives) and is NOT divided by --jobs, since run_shapepairs.py
runs the compiled binaries one at a time regardless of --jobs. A method with
no history yet (never benchmarked) gets an all-zero weight and is spread
round-robin across the groups.

Grouping is deterministic across a whole n=1..d cycle, not recomputed fresh
every call: --fraction=1/d always recomputes the split from current history
and caches it (default build/tests/benchmark/fraction_cache.json); n=2..d
reuse that cached split. Recomputing on every call would risk a method
drifting across the group boundary between nights as fresh history comes in
from the runs already done, which would leave a gap or a duplicate between
two nights' method lists. Re-running --fraction=1/d intentionally starts a
new cycle (e.g. after a code change that shifts which methods are expensive).

Usage:
    python3 tests/benchmark/split_methods.py --fraction 2/3
    python3 tests/benchmark/split_methods.py --fraction 1/3 --shapes Polygon,Triangle --types int,ERational

Options:
    --fraction N/D        required; print the methods of group N of D
    --shapes   S,...      scope the history used for weighting to these shapes
                          (mirrors run_shapepairs.py's --shapes; matches both
                          operands, Point always allowed as operand B). Passed
                          through unchanged if you're also passing --shapes to
                          record.sh, so the weighting matches what actually runs.
    --types    T,...      scope the history used for weighting to these types
    --repetitions N       repetitions record.sh will use (default 3); scales
                          the weight of methods whose runs are not truncated
                          by the per-program time budget, since those actually
                          run N times
    --compile-seconds S   estimated compile time per generated program in
                          CPU-seconds (default 5.0; see module docstring)
    --jobs     N          parallel compile jobs record.sh will use
                          (default: all available CPUs, matching record.sh)
    --history  DIR        history root (default: see bench_paths.py)
    --cache    FILE       cache path (default build/tests/benchmark/fraction_cache.json)
"""
from __future__ import annotations

import argparse
import json
import os
import re
import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from bench_paths import default_history  # noqa: E402
from run_shapepairs import ALL_METHODS, GROUND_TRUTH_TYPE  # noqa: E402


def compute_weights(history_dir: Path,
                     shapes_filter: set[str] | None,
                     types_filter: set[str] | None,
                     repetitions: int,
                     compile_seconds: float,
                     jobs: int) -> dict[str, float]:
    """Estimated wall-clock cost of each method, in seconds.

    Reads the most recent recorded entry for every (shape1, size1, shape2,
    size2, method, type) cell in history, then per method sums:
      - compile_seconds x number of distinct (shape1, shape2, type) programs
        that cell's method needed (a program covers every size combo of its
        pair, so size is not part of this count — see generate_source in
        run_shapepairs.py), divided by 'jobs'. compile_seconds is CPU time,
        but run_shapepairs.py compiles with a 'jobs'-way thread pool, so wall
        time is roughly that CPU time shared over 'jobs' workers — unlike the
        run phase below, which run_shapepairs.py does not parallelize at all,
        one binary at a time, so it stays plain wall-clock seconds.
      - the recorded run time x however many times record.sh would actually
        run it: once for a cell truncated by the per-program time budget
        (every type of a truncated cell is capped to the baseline's prefix
        and measured once regardless of --repetitions — see run_shapepairs.py),
        'repetitions' times otherwise.
    """
    latest: dict[tuple, dict] = {}
    for path in sorted(history_dir.glob("*.jsonl")):
        with path.open() as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue
                e = json.loads(line)
                if e.get("kind") != "pair":
                    continue
                if shapes_filter is not None and (
                    e["shape1"] not in shapes_filter or e["shape2"] not in shapes_filter
                ):
                    continue
                if types_filter is not None and e["type"] not in types_filter:
                    continue
                key = (e["shape1"], e["size1"], e["shape2"], e["size2"], e["method"], e["type"])
                prev = latest.get(key)
                if prev is None or e["date"] > prev["date"]:
                    latest[key] = e

    programs: dict[str, set[tuple]] = defaultdict(set)
    run_seconds: dict[str, float] = defaultdict(float)
    for e in latest.values():
        m = e["method"]
        if m not in ALL_METHODS:
            continue  # stale/renamed method name from old history
        programs[m].add((e["shape1"], e["shape2"], e["type"]))
        calls = e.get("calls") or 0
        time_ns = e.get("time") or 0.0
        mult = 1 if e.get("truncated") else repetitions
        run_seconds[m] += time_ns * calls / 1e9 * mult

    return {
        m: compile_seconds * len(programs[m]) / jobs + run_seconds[m]
        for m in ALL_METHODS
    }


def partition(weights: dict[str, float], d: int) -> list[list[str]]:
    """Greedy longest-processing-time bin packing into d groups.

    Methods are placed heaviest-first, each into whichever group currently
    has the smallest running total. Simple and, for a single dominant outlier
    like minkowskiSum (roughly half the cube's measured time by itself), it's
    also about as good as an exact partition gets.
    """
    bins: list[list[str]] = [[] for _ in range(d)]
    totals = [0.0] * d
    for method, _ in sorted(weights.items(), key=lambda kv: -kv[1]):
        i = min(range(d), key=lambda i: totals[i])
        bins[i].append(method)
        totals[i] += weights[method]
    return bins


def main() -> int:
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    ap.add_argument("--fraction", required=True)
    ap.add_argument("--shapes", default=None)
    ap.add_argument("--types", default=None)
    ap.add_argument("--repetitions", type=int, default=3)
    ap.add_argument("--compile-seconds", dest="compile_seconds", type=float, default=5.0)
    ap.add_argument("--jobs", type=int, default=max(1, os.cpu_count() or 1))
    ap.add_argument("--history", default=None)
    ap.add_argument("--cache", default=None)
    args = ap.parse_args()

    m = re.fullmatch(r"(\d+)/(\d+)", args.fraction.strip())
    if not m:
        sys.exit(f"--fraction must look like n/d (e.g. 2/3), got {args.fraction!r}")
    n, d = int(m.group(1)), int(m.group(2))
    if d < 1:
        sys.exit("--fraction denominator must be >= 1")
    if not (1 <= n <= d):
        sys.exit(f"--fraction numerator must be between 1 and {d}, got {n}")
    if d > len(ALL_METHODS):
        sys.exit(f"--fraction denominator ({d}) exceeds the number of methods ({len(ALL_METHODS)})")

    script_dir = Path(__file__).parent.resolve()
    project_root = script_dir.parent.parent
    history_dir = Path(args.history) if args.history else default_history()
    cache_path = (Path(args.cache) if args.cache
                  else project_root / "build/tests/benchmark/fraction_cache.json")

    shapes_filter = {s.strip() for s in args.shapes.split(",") if s.strip()} if args.shapes else None
    types_filter = ({t.strip() for t in args.types.split(",") if t.strip()} | {GROUND_TRUTH_TYPE}
                    if args.types else None)

    scope_key = json.dumps({
        "shapes": sorted(shapes_filter) if shapes_filter else None,
        "types": sorted(types_filter) if types_filter else None,
        "repetitions": args.repetitions,
        "compile_seconds": args.compile_seconds,
        "jobs": args.jobs,
    }, sort_keys=True)

    weights = compute_weights(history_dir, shapes_filter, types_filter,
                              args.repetitions, args.compile_seconds, args.jobs)

    if n == 1:
        bins = partition(weights, d)
        cache_path.parent.mkdir(parents=True, exist_ok=True)
        cache_path.write_text(json.dumps({"d": d, "scope_key": scope_key, "bins": bins}, indent=2))
    else:
        if not cache_path.exists():
            sys.exit(f"No cached split at {cache_path}.\n"
                     f"Run --fraction=1/{d} first to start this {d}-night cycle.")
        cache = json.loads(cache_path.read_text())
        if cache.get("d") != d or cache.get("scope_key") != scope_key:
            sys.exit(f"Cached split at {cache_path} doesn't match --fraction=?/{d}"
                     " with these --shapes/--types/--repetitions.\n"
                     f"Run --fraction=1/{d} first to (re)start this cycle.")
        bins = cache["bins"]

    chosen = bins[n - 1]
    if not chosen:
        print(f"warning: group {n}/{d} came out empty (more groups than methods"
              " with distinguishable weight)", file=sys.stderr)

    totals = [sum(weights.get(m, 0.0) for m in group) for group in bins]
    print(f"Fraction {n}/{d}: {len(chosen)} methods, "
          f"~{totals[n-1]/3600:.2f}h estimated (cache-recorded assignment)",
          file=sys.stderr)
    print("Estimated hours per group: "
          + ", ".join(f"{i+1}/{d}={t/3600:.2f}h" for i, t in enumerate(totals)),
          file=sys.stderr)

    print(",".join(chosen))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
