#!/usr/bin/env python3
"""run_asymptotic.py — runner for the asymptotic (whole-algorithm) benchmarks.

Each program under tests/benchmark/asymptotic/ measures one benchmark category:
a small cube of dataset x problem x algorithm x number type, swept over a fixed
list of input sizes (asymptotic/sizes.hpp). Every driver prints the same
tab-separated table

    Category  Dataset  Problem  Algorithm  Number  Size  Result  Time(µs)
    Triangulation  points  build  Delaunay  int  3125  6180  4021.3

where Result is a numeric signature of the computed answer — an intersection
count, a vertex count, a graph's edge count. It doubles as a correctness
cross-check (the int and ERational runs see the identical input, so their
signatures must agree, as must two algorithms solving the same problem) and, in
the output-sensitive categories, as the record of how fast the output itself
grows.

This script compiles each driver with -O2 -DNDEBUG, runs it (optionally several
times, keeping the median time per cell), and writes a snapshot JSON consumed by
to_history.py.

Drivers are compiled in parallel but run one at a time: they are being timed.

Usage (from repo root):
    python3 tests/benchmark/run_asymptotic.py [category ...] [options]

Positional arguments select drivers by basename (e.g. triangulation); with none,
every asymptotic/*.cpp is built and run.

Options:
    --output FILE       JSON output (default: build/tests/benchmark/asymptotic/asymptotic.json)
    --build-dir DIR     Build root  (default: build/tests/benchmark/asymptotic)
    --cxx CXX           Compiler    (default: $CXX or c++)
    --cxxflags FLAGS    Flags       (default: $CXXFLAGS or -std=c++23 -O2 -DNDEBUG)
    --repetitions N     Runs per program; median time kept (default: 1)
    --jobs N            Parallel compile jobs (default: os.cpu_count())
    --baseline          Also build and run asymptotic/baseline/*.cpp (CGAL) and
                        write a separate, non-historical snapshot
    --baseline-only     Run only the CGAL baseline
    --baseline-output FILE
                        Baseline JSON (default: <build-dir>/baseline.json)
    --sizes LIST        Pass --sizes through to every driver. For calibration
                        only: a recorded run must sweep the checked-in sizes.
"""
from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timezone
from pathlib import Path

TIME_HEADER_RE = re.compile(r"Time\(([^)]+)\)")
# "us" is what the drivers printed before the symbol was spelled properly, and
# what the older recorded runs carry. One spelling reaches the dashboard.
MICROSECONDS = "µs"


def canonical_unit(unit: str) -> str:
    return MICROSECONDS if unit in ("us", "\u00b5s", "\u03bcs") else unit


COLUMNS = ("category", "dataset", "problem", "algorithm", "number", "size", "result")


def detect_cpu() -> str:
    try:
        out = subprocess.run(
            ["lscpu"], capture_output=True, text=True,
            env={**os.environ, "LC_ALL": "C"},
        ).stdout
        for line in out.splitlines():
            if "Model name" in line:
                return line.split(":", 1)[1].strip()
    except FileNotFoundError:
        pass
    try:
        with open("/proc/cpuinfo") as f:
            for line in f:
                if line.startswith("model name"):
                    return line.split(":", 1)[1].strip()
    except OSError:
        pass
    return ""


def parse_table(raw: str) -> tuple[str, list[dict]]:
    """Parse a driver's output into (unit, rows).

    Rows are exactly the eight columns the drivers print. Anything that does not
    have eight fields, or whose last field is not a number, is not a data row —
    a driver is free to print progress or warnings alongside its table.
    """
    unit = MICROSECONDS
    rows: list[dict] = []
    for line in raw.splitlines():
        header = TIME_HEADER_RE.search(line)
        if header:
            unit = canonical_unit(header.group(1))
            continue
        parts = line.rstrip("\n").split("\t")
        if len(parts) != len(COLUMNS) + 1:
            continue
        try:
            time = float(parts[-1])
            size = int(parts[5])
        except ValueError:
            continue
        row = dict(zip(COLUMNS, parts[:-1]))
        row["size"] = size
        row["time"] = time
        rows.append(row)
    return unit, rows


def compile_all(sources, cxx, cxxflags, include_dir, bench_dir, bin_dir, jobs, extra_flags):
    """Compile every source; return {stem: binary} for the ones that built."""
    def build(source: Path):
        binary = bin_dir / source.stem
        cmd = [cxx, *cxxflags, f"-I{include_dir}", f"-I{bench_dir}",
               *extra_flags, str(source), "-o", str(binary)]
        done = subprocess.run(cmd, capture_output=True, text=True)
        return source, binary, done

    built: dict[str, Path] = {}
    with ThreadPoolExecutor(max_workers=max(1, jobs)) as pool:
        for source, binary, done in pool.map(build, sources):
            print(f"::group::Build {source.stem}", flush=True)
            if done.returncode != 0:
                print(f"  compile FAILED: {source.stem}\n{done.stderr}", file=sys.stderr)
            else:
                built[source.stem] = binary
            print("::endgroup::", flush=True)
    return built


def run_drivers(built, repetitions, driver_args, timeout) -> list[dict]:
    """Run each binary `repetitions` times, keeping the median time per cell."""
    results: list[dict] = []
    for stem, binary in sorted(built.items()):
        # cell key -> list of (result, time); the median time's own result is
        # kept, so a reported row is one real measurement rather than a blend.
        cells: dict[tuple, list[tuple[str, float]]] = {}
        order: list[tuple] = []
        unit = MICROSECONDS
        ok = True
        for _ in range(max(1, repetitions)):
            try:
                run = subprocess.run([str(binary), *driver_args], capture_output=True,
                                     text=True, timeout=timeout)
            except subprocess.TimeoutExpired:
                print(f"  run TIMEOUT: {stem}", file=sys.stderr)
                ok = False
                break
            if run.returncode != 0:
                print(f"  run FAILED: {stem}\n{run.stderr}", file=sys.stderr)
                ok = False
                break
            unit, rows = parse_table(run.stdout)
            for row in rows:
                key = tuple(row[c] for c in COLUMNS[:-1])
                if key not in cells:
                    cells[key] = []
                    order.append(key)
                cells[key].append((row["result"], row["time"]))
        if not ok:
            continue

        for key in order:
            samples = sorted(cells[key], key=lambda s: s[1])
            result, time = samples[len(samples) // 2]
            row = dict(zip(COLUMNS[:-1], key))
            row.update({
                "driver":   stem,
                "result":   result,
                "time":     round(time, 6),
                "time_min": round(samples[0][1], 6),
                "time_max": round(samples[-1][1], 6),
                "unit":     unit,
            })
            results.append(row)
        print(f"  {stem}: {len(order)} cells", flush=True)
    return results


def write_snapshot(path: Path, meta: dict, results: list[dict], label: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps({"meta": meta, "results": results}, indent=2))
    print(f"{label} JSON -> {path}  ({len(results)} rows)")


def main() -> int:
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    ap.add_argument("categories", nargs="*")
    ap.add_argument("--output", default=None)
    ap.add_argument("--build-dir", dest="build_dir", default=None)
    ap.add_argument("--cxx", default=os.environ.get("CXX", "c++"))
    ap.add_argument("--cxxflags",
                    default=os.environ.get("CXXFLAGS", "-std=c++23 -O2 -DNDEBUG"))
    ap.add_argument("--repetitions", type=int, default=1)
    ap.add_argument("--jobs", type=int, default=os.cpu_count() or 1)
    ap.add_argument("--baseline", action="store_true")
    ap.add_argument("--baseline-only", dest="baseline_only", action="store_true")
    ap.add_argument("--baseline-output", dest="baseline_output", default=None)
    ap.add_argument("--sizes", default=None)
    ap.add_argument("--timeout", type=int, default=7200)
    args = ap.parse_args()

    script_dir     = Path(__file__).parent.resolve()
    project_root   = script_dir.parent.parent
    include_dir    = project_root / "include"
    asymptotic_dir = script_dir / "asymptotic"
    baseline_dir   = asymptotic_dir / "baseline"

    build_dir = (
        Path(args.build_dir).resolve()
        if args.build_dir
        else project_root / "build" / "tests" / "benchmark" / "asymptotic"
    )
    bin_dir = build_dir / "bin"
    bin_dir.mkdir(parents=True, exist_ok=True)
    output_path = (
        Path(args.output).resolve() if args.output else build_dir / "asymptotic.json"
    )
    baseline_path = (
        Path(args.baseline_output).resolve()
        if args.baseline_output
        else build_dir / "baseline.json"
    )

    def select(directory: Path, what: str) -> list[Path]:
        sources = sorted(directory.glob("*.cpp"))
        if args.categories:
            wanted = set(args.categories)
            chosen = [s for s in sources if s.stem in wanted]
            missing = wanted - {s.stem for s in sources}
            if missing and what == "asymptotic":
                sys.exit(f"no asymptotic benchmark named: {', '.join(sorted(missing))}")
            return chosen
        return sources

    cxx      = args.cxx
    cxxflags = args.cxxflags.split()
    driver_args = ["--sizes", args.sizes] if args.sizes else []

    commit = subprocess.run(
        ["git", "rev-parse", "--short", "HEAD"],
        capture_output=True, text=True, cwd=project_root,
    ).stdout.strip() or "unknown"
    meta = {
        "timestamp":   datetime.now(timezone.utc).isoformat(),
        "commit":      commit,
        "cpu":         detect_cpu() or None,
        "compiler":    cxx,
        "cxxflags":    args.cxxflags,
        "repetitions": args.repetitions,
    }
    if args.sizes:
        # A run with overridden sizes is a calibration run: recording it into the
        # history would put points at x values no other run measured.
        meta["sizes_override"] = args.sizes

    if not args.baseline_only:
        sources = select(asymptotic_dir, "asymptotic")
        if not sources:
            sys.exit(f"no asymptotic benchmark sources found in {asymptotic_dir}")
        built = compile_all(sources, cxx, cxxflags, include_dir, script_dir,
                            bin_dir, args.jobs, [])
        results = run_drivers(built, args.repetitions, driver_args, args.timeout)
        write_snapshot(output_path, meta, results, "Asymptotic benchmark")

    # The CGAL baseline is opt-in and stores a single overwritten snapshot, never
    # a history: it exists to check pgl's answers and to sit on the chart as a
    # reference curve, not to be tracked commit over commit. CGAL is not
    # guaranteed present on a CI box or on every dev machine, so nothing here
    # runs unless it was asked for.
    if args.baseline or args.baseline_only:
        baseline_sources = select(baseline_dir, "baseline")
        if not baseline_sources:
            print(f"no CGAL baseline sources in {baseline_dir}; skipping.", file=sys.stderr)
            return 0
        baseline_bin = bin_dir / "baseline"
        baseline_bin.mkdir(parents=True, exist_ok=True)
        built = compile_all(baseline_sources, cxx, cxxflags, include_dir, script_dir,
                            baseline_bin, args.jobs, ["-lgmp", "-lmpfr"])
        results = run_drivers(built, args.repetitions, driver_args, args.timeout)
        write_snapshot(baseline_path, meta, results, "CGAL baseline")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
