#!/usr/bin/env python3
"""run_extra.py — runner for the whole-algorithm "extra" benchmarks.

The programs under tests/benchmark/extra/ (bentleyottmann, shapetree,
triangulation, …) don't fit the shape-pair cube produced by run_shapepairs.py:
each is a self-contained driver that prints a small tab-separated table

    Operation<TAB>Number<TAB>Result<TAB>Time(<unit>)
    intersections   ERational   12345   3.21

This script compiles each one with -O3 -DNDEBUG, runs it (optionally several
times, keeping the median time per operation), parses that table, and writes a
snapshot JSON consumed by to_history.py.

Usage (from repo root):
    python3 tests/benchmark/run_extra.py [suite ...] [options]

Positional 'suite' arguments select programs by basename (e.g. bentleyottmann);
with none, every extra/*.cpp is built and run.

Options:
    --output FILE       JSON output (default: build/tests/benchmark/extra.json)
    --build-dir DIR     Build root  (default: build/tests/benchmark/extra)
    --cxx CXX           Compiler    (default: $CXX or c++)
    --cxxflags FLAGS    Flags       (default: $CXXFLAGS or -std=c++23 -O3 -DNDEBUG)
    --repetitions N     Runs per program; median time kept (default: 1)
"""
from __future__ import annotations

import argparse
import json
import os
import re
import statistics
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

TIME_HEADER_RE = re.compile(r"Time\(([^)]+)\)")


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


def parse_table(raw: str) -> tuple[str, list[tuple[str, str, str, float]]]:
    """Parse an extra program's output.

    Returns (unit, rows) where each row is (operation, number, result, time).
    Tabs are the column separator; programs pad with runs of tabs for alignment,
    so empty fields are dropped — this keeps labels that contain spaces intact.
    """
    unit = "ns"
    rows: list[tuple[str, str, str, float]] = []
    for line in raw.splitlines():
        m = TIME_HEADER_RE.search(line)
        if m:
            unit = m.group(1)
        stripped = line.strip()
        if not stripped or stripped.startswith("Operation"):
            continue
        parts = [field for field in stripped.split("\t") if field]
        if len(parts) < 4:
            continue
        try:
            value = float(parts[-1])
        except ValueError:
            continue
        rows.append((parts[0], " ".join(parts[1:-2]), parts[-2], value))
    return unit, rows


def main() -> int:
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    ap.add_argument("suites", nargs="*")
    ap.add_argument("--output", default=None)
    ap.add_argument("--build-dir", dest="build_dir", default=None)
    ap.add_argument("--cxx", default=os.environ.get("CXX", "c++"))
    ap.add_argument("--cxxflags",
                    default=os.environ.get("CXXFLAGS", "-std=c++23 -O3 -DNDEBUG"))
    ap.add_argument("--repetitions", type=int, default=1)
    args = ap.parse_args()

    script_dir   = Path(__file__).parent.resolve()
    project_root = script_dir.parent.parent
    include_dir  = project_root / "include"
    extra_dir    = script_dir / "extra"

    build_dir = (
        Path(args.build_dir).resolve()
        if args.build_dir
        else project_root / "build" / "tests" / "benchmark" / "extra"
    )
    bin_dir = build_dir / "bin"
    bin_dir.mkdir(parents=True, exist_ok=True)
    output_path = (
        Path(args.output).resolve()
        if args.output
        else build_dir / "extra.json"
    )
    output_path.parent.mkdir(parents=True, exist_ok=True)

    all_sources = sorted(extra_dir.glob("*.cpp"))
    if args.suites:
        wanted = set(args.suites)
        sources = [s for s in all_sources if s.stem in wanted]
        missing = wanted - {s.stem for s in sources}
        if missing:
            sys.exit(f"no extra benchmark named: {', '.join(sorted(missing))}")
    else:
        sources = all_sources
    if not sources:
        sys.exit(f"no extra benchmark sources found in {extra_dir}")

    cxx      = args.cxx
    cxxflags = args.cxxflags.split()

    commit = subprocess.run(
        ["git", "rev-parse", "--short", "HEAD"],
        capture_output=True, text=True, cwd=project_root,
    ).stdout.strip() or "unknown"
    cpu = detect_cpu()

    results: list[dict] = []
    for source in sources:
        suite = source.stem
        binary = bin_dir / suite

        print(f"::group::Build {suite}", flush=True)
        cmd = [cxx, *cxxflags, f"-I{include_dir}", str(source), "-o", str(binary)]
        compile = subprocess.run(cmd, capture_output=True, text=True)
        if compile.returncode != 0:
            print(f"  compile FAILED: {suite}\n{compile.stderr}", file=sys.stderr)
            print("::endgroup::", flush=True)
            continue
        print("::endgroup::", flush=True)

        # Group repeated runs by operation, keep the median time per operation.
        per_op: dict[str, list[tuple[str, str, float]]] = {}
        unit = "ns"
        ok = True
        for _ in range(max(1, args.repetitions)):
            try:
                run = subprocess.run([str(binary)], capture_output=True,
                                     text=True, timeout=600)
            except subprocess.TimeoutExpired:
                print(f"  run TIMEOUT: {suite}", file=sys.stderr)
                ok = False
                break
            if run.returncode != 0:
                print(f"  run FAILED: {suite}\n{run.stderr}", file=sys.stderr)
                ok = False
                break
            unit, rows = parse_table(run.stdout)
            for op, number, result, value in rows:
                per_op.setdefault((op, number), []).append((result, value))
        if not ok:
            continue

        for (op, number), samples in per_op.items():
            samples.sort(key=lambda s: s[1])
            result, value = samples[len(samples) // 2]
            results.append({
                "suite":    suite,
                "op":       op,
                "number":   number,
                "result":   result,
                "time":     round(value, 6),
                "time_min": round(samples[0][1], 6),
                "time_max": round(samples[-1][1], 6),
                "unit":     unit,
            })
            print(f"  {suite}: {op} [{number}] {value:.4f}{unit}", flush=True)

    payload = {
        "meta": {
            "timestamp":   datetime.now(timezone.utc).isoformat(),
            "commit":      commit,
            "cpu":         cpu or None,
            "compiler":    cxx,
            "cxxflags":    args.cxxflags,
            "repetitions": args.repetitions,
        },
        "results": results,
    }
    output_path.write_text(json.dumps(payload, indent=2))
    print(f"\nExtra benchmark JSON → {output_path}  ({len(results)} rows)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
