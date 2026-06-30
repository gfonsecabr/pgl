#!/usr/bin/env python3
"""
run_benchmarks.py — pgl shape-method benchmark runner.

For every combination of shape pair × size pair × number type × method, generates
a small C++ program that creates 100 random shapes of each kind (small or large),
times every pair-wise call to the method, and prints the elapsed time together
with an aggregate result (e.g. count of true returns, or count of zero-distance
pairs).

The script compiles and runs each program.  For each (shape1, size1, shape2,
size2, method) quintuple, the results across all number types are compared against
the ERational (exact) baseline.  Any type whose aggregate result disagrees with
ERational is marked as discarded in the JSON output.  All timing data are written
to a JSON file.

Usage (from repo root):
    python3 tests/benchmark/run_shapepairs.py [options]

Options:
    --shapes  S,...    Shapes to include (default: all 8)
    --sizes   S,...    Size variants: small, large, or both (default: small,large)
    --methods M,...    Methods to include (default: all 11)
    --types   T,...    Number types (default: all 5)
    --output  FILE     JSON output path (default: build/tests/benchmark/benchmarks.json)
    --build-dir DIR    Build root  (default: build/tests/benchmark)
    --cxx     CXX      Compiler    (default: $CXX or c++)
    --cxxflags FLAGS   Flags       (default: $CXXFLAGS or -std=c++23 -O3 -DNDEBUG)
    --jobs    N        Parallel compile jobs (default: 1)
    --dry-run          Write C++ sources but do not compile or run
"""

from __future__ import annotations
import argparse
import itertools
import json
import os
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor, as_completed
from datetime import datetime, timezone
from pathlib import Path


# ─── Benchmark dimensions ────────────────────────────────────────────────────

ALL_SHAPES = [
    "Segment",
    "OrientedSegment",
    "Line",
    "OrientedLine",
    "Rectangle",
    "Triangle",
    "Disk",
    "Convex",
]

ALL_SIZES = ["small", "large"]

# (benchmark key, C++ type used in generated source)
ALL_NUMBER_TYPES: list[tuple[str, str]] = [
    ("int",       "int"),
    ("double",    "double"),
    ("BigInt",    "pgl::BigInt"),
    ("Rational",  "pgl::Rational<>"),
    ("ERational", "pgl::ERational"),
]

ALL_METHODS = [
    "contains",
    "interiorContains",
    "boundaryContains",
    "intersects",
    "interiorsIntersect",
    "separates",
    "crosses",
    "collinear",
    "parallel",
    "intersection",
    "squaredDistance",
]

# Ground-truth type for cross-type comparison
GROUND_TRUTH_TYPE = "ERational"

# Number of random shapes generated per type per benchmark
N_SHAPES = 100

# Shape-kind categories (drives which randomXxx helper is called)
_BISHAPES  = {"Segment", "OrientedSegment", "Line", "OrientedLine", "Rectangle"}
_TRISHAPES = {"Triangle", "Disk"}
# "Convex" gets its own generator call


# ─── C++ source generation ───────────────────────────────────────────────────

def _cpp_make_shapes_for(shape: str, size: str, alias: str, var: str) -> str:
    """C++ statement that fills 'var' with N_SHAPES random shapes of kind 'shape'."""
    n = N_SHAPES
    prefix = "randomSmall" if size == "small" else "randomLarge"
    if shape in _BISHAPES:
        return f"auto {var} = {prefix}Bishape<{alias}>({n});"
    if shape in _TRISHAPES:
        return f"auto {var} = {prefix}Trishape<{alias}>({n});"
    return f"auto {var} = {prefix}Convexes<N>({n}, 8);"


def _cpp_accumulate(method: str) -> str:
    """C++ inner-loop body that increments 'count' for one (a, b) pair."""
    if method in {"contains", "interiorContains", "boundaryContains",
                  "intersects", "interiorsIntersect", "separates", "crosses",
                  "collinear", "parallel"}:
        return f"count += a.{method}(b) ? 1 : 0;"
    if method == "intersection":
        return "count += a.template intersection<N>(b).has_value() ? 1 : 0;"
    if method == "squaredDistance":
        return ("{ const auto d = a.squaredDistance(b);"
                    " const decltype(d) zero{}; count += (d == zero) ? 1 : 0; }")
    raise ValueError(f"Unknown method: {method!r}")


def generate_source(
    shape1: str, size1: str,
    shape2: str, size2: str,
    method: str, cpp_type: str,
) -> str:
    """Return a complete, self-contained C++ benchmark source."""
    make1 = _cpp_make_shapes_for(shape1, size1, "S1", "shapes1")
    make2 = _cpp_make_shapes_for(shape2, size2, "S2", "shapes2")
    accum = _cpp_accumulate(method)

    lines = [
        f"// Benchmark: {shape1}({size1}) × {shape2}({size2}) :: {method}  [{cpp_type}]",
        "#include <iostream>",
        '#include "pgl.hpp"',
        '#include "randomshapes.hpp"',
        '#include "plf_nanotimer.h"',
        "",
        f"using N  = {cpp_type};",
        f"using S1 = pgl::{shape1}<pgl::Point<N>>;",
        f"using S2 = pgl::{shape2}<pgl::Point<N>>;",
        "",
        "int main() {",
        f"    {make1}",
        f"    {make2}",
        "    plf::nanotimer timer;",
        "    timer.start();",
        "    long long count = 0;",
        "    for (const auto& a : shapes1)",
        "        for (const auto& b : shapes2)",
        f"            {accum}",
        "    double ns = timer.get_elapsed_ns()"
        " / (double)(shapes1.size() * shapes2.size());",
        '    std::cout << count << "\\t" << ns << "\\n";',
        "    return 0;",
        "}",
        "",
    ]
    return "\n".join(lines)


# ─── Compile / run helpers ───────────────────────────────────────────────────

def compile_source(src: Path, binary: Path,
                   cxx: str, cxxflags: list[str],
                   include_dir: Path, bench_dir: Path) -> tuple[bool, str]:
    """Compile src → binary.  Returns (success, stderr)."""
    cmd = [
        cxx, *cxxflags,
        f"-I{include_dir}",
        f"-I{bench_dir}",   # randomshapes.hpp and plf_nanotimer.h
        str(src), "-o", str(binary),
    ]
    r = subprocess.run(cmd, capture_output=True, text=True)
    return r.returncode == 0, r.stderr


def run_binary(binary: Path, repetitions: int = 1) -> tuple[bool, str]:
    """Run binary 'repetitions' times; return the run with the *median* time.

    The aggregate result count is deterministic across runs, so only the timing
    varies; taking the median ns damps scheduler noise. Returns
    (success, stdout-or-stderr) where stdout is the chosen 'count<TAB>ns' line.
    """
    samples: list[tuple[int, float, str]] = []
    for _ in range(max(1, repetitions)):
        try:
            r = subprocess.run([str(binary)], capture_output=True, text=True, timeout=300)
        except subprocess.TimeoutExpired:
            return False, "timeout"
        if r.returncode != 0:
            return False, r.stderr
        parsed = parse_output(r.stdout)
        if parsed is None:
            return False, r.stdout
        samples.append((parsed[0], parsed[1], r.stdout))
    if not samples:
        return False, ""
    samples.sort(key=lambda s: s[1])
    return True, samples[len(samples) // 2][2]


def parse_output(raw: str) -> tuple[int, float] | None:
    """Parse 'count<TAB>ns' output; returns None on malformed output."""
    lines = raw.strip().splitlines()
    if not lines:
        return None
    parts = lines[0].split("\t")
    if len(parts) != 2:
        return None
    try:
        return int(parts[0]), float(parts[1])
    except ValueError:
        return None


# ─── Main ────────────────────────────────────────────────────────────────────

def main() -> None:
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    ap.add_argument("--shapes",    default=",".join(ALL_SHAPES))
    ap.add_argument("--sizes",     default=",".join(ALL_SIZES))
    ap.add_argument("--methods",   default=",".join(ALL_METHODS))
    ap.add_argument("--types",     default=",".join(k for k, _ in ALL_NUMBER_TYPES))
    ap.add_argument("--output",    default=None)
    ap.add_argument("--build-dir", dest="build_dir", default=None)
    ap.add_argument("--cxx",       default=os.environ.get("CXX", "c++"))
    ap.add_argument("--cxxflags",  default=os.environ.get("CXXFLAGS", "-std=c++23 -O3 -DNDEBUG"))
    ap.add_argument("--jobs",      type=int, default=1)
    ap.add_argument("--repetitions", type=int, default=1,
                    help="run each binary N times and keep the median time")
    ap.add_argument("--dry-run",   dest="dry_run", action="store_true")
    args = ap.parse_args()

    script_dir   = Path(__file__).parent.resolve()
    project_root = script_dir.parent.parent
    include_dir  = project_root / "include"
    bench_dir    = script_dir   # contains randomshapes.hpp, plf_nanotimer.h

    build_dir = (
        Path(args.build_dir).resolve()
        if args.build_dir
        else project_root / "build" / "tests" / "benchmark"
    )
    src_dir    = build_dir / "src"
    bin_dir    = build_dir / "bin"
    output_path = (
        Path(args.output).resolve()
        if args.output
        else build_dir / "benchmarks.json"
    )

    src_dir.mkdir(parents=True, exist_ok=True)
    bin_dir.mkdir(parents=True, exist_ok=True)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    cxx      = args.cxx
    cxxflags = args.cxxflags.split()

    valid_shapes    = set(ALL_SHAPES)
    valid_sizes     = set(ALL_SIZES)
    valid_methods   = set(ALL_METHODS)
    valid_type_keys = {k for k, _ in ALL_NUMBER_TYPES}

    shapes  = [s.strip() for s in args.shapes.split(",")  if s.strip() in valid_shapes]
    sizes   = [s.strip() for s in args.sizes.split(",")   if s.strip() in valid_sizes]
    methods = [m.strip() for m in args.methods.split(",") if m.strip() in valid_methods]

    requested_keys = {k.strip() for k in args.types.split(",") if k.strip() in valid_type_keys}
    requested_keys.add(GROUND_TRUTH_TYPE)  # always include for comparison
    types = [(k, t) for k, t in ALL_NUMBER_TYPES if k in requested_keys]

    if not shapes:
        sys.exit("No valid shapes specified.")
    if not sizes:
        sys.exit("No valid sizes specified (use 'small', 'large', or both).")
    if not methods:
        sys.exit("No valid methods specified.")

    shape_size_variants = list(itertools.product(shapes, sizes))
    pairs  = list(itertools.product(shape_size_variants, repeat=2))
    combos = [
        (s1, sz1, s2, sz2, m, k, cpp_t)
        for ((s1, sz1), (s2, sz2)), m, (k, cpp_t)
        in itertools.product(pairs, methods, types)
    ]

    total = len(combos)
    n_pairs = len(pairs)
    print(
        f"pgl benchmark: {n_pairs} shape×size pairs × {len(methods)} methods"
        f" × {len(types)} types = {total} programs"
    )
    print(f"  shapes:  {shapes}")
    print(f"  sizes:   {sizes}")
    print(f"  methods: {methods}")
    print(f"  types:   {[k for k, _ in types]}")

    # ── Collect environment info ─────────────────────────────────────────────
    commit = subprocess.run(
        ["git", "rev-parse", "--short", "HEAD"],
        capture_output=True, text=True, cwd=project_root,
    ).stdout.strip() or "unknown"

    cpu = ""
    try:
        lscpu = subprocess.run(
            ["lscpu"], capture_output=True, text=True, env={**os.environ, "LC_ALL": "C"},
        ).stdout
        for line in lscpu.splitlines():
            if "Model name" in line:
                cpu = line.split(":", 1)[1].strip()
                break
    except FileNotFoundError:
        pass
    if not cpu:
        try:
            with open("/proc/cpuinfo") as f:
                for line in f:
                    if line.startswith("model name"):
                        cpu = line.split(":", 1)[1].strip()
                        break
        except OSError:
            pass

    # ── Step 1: generate all C++ sources ────────────────────────────────────
    srcs: dict[tuple, Path] = {}
    bins: dict[tuple, Path] = {}
    for shape1, size1, shape2, size2, method, type_key, cpp_type in combos:
        tag    = f"{shape1}_{size1}_x_{shape2}_{size2}__{method}__{type_key}"
        src    = src_dir / f"{tag}.cpp"
        binary = bin_dir / tag
        key    = (shape1, size1, shape2, size2, method, type_key)
        srcs[key] = src
        bins[key] = binary
        src.write_text(generate_source(shape1, size1, shape2, size2, method, cpp_type))

    if args.dry_run:
        print(f"Dry-run: {total} sources written to {src_dir}")
        return

    # ── Step 2: compile ─────────────────────────────────────────────────────
    compile_ok:  dict[tuple, bool] = {}
    compile_err: dict[tuple, str]  = {}

    def _compile(key: tuple) -> tuple[tuple, bool, str]:
        ok, err = compile_source(srcs[key], bins[key], cxx, cxxflags, include_dir, bench_dir)
        return key, ok, err

    n_ok = n_fail = 0
    with ThreadPoolExecutor(max_workers=max(1, args.jobs)) as pool:
        futures = {pool.submit(_compile, k): k for k in srcs}
        for i, fut in enumerate(as_completed(futures), 1):
            key, ok, err = fut.result()
            compile_ok[key]  = ok
            compile_err[key] = err
            if ok:
                n_ok += 1
            else:
                n_fail += 1
            shape1, size1, shape2, size2, method, type_key = key
            status = "ok" if ok else "FAIL"
            print(
                f"  [{i:>6}/{total}] compile"
                f" {shape1}({size1}) × {shape2}({size2})"
                f" {method} [{type_key}] {status}",
                flush=True,
            )

    print(f"Compilation: {n_ok} ok, {n_fail} failed.")

    # ── Step 3: run all successful binaries ──────────────────────────────────
    run_results: dict[tuple, tuple[int, float] | None] = {}

    successful_keys = [k for k in srcs if compile_ok.get(k)]
    for i, key in enumerate(successful_keys, 1):
        shape1, size1, shape2, size2, method, type_key = key
        ok, raw = run_binary(bins[key], args.repetitions)
        parsed = parse_output(raw) if ok else None
        run_results[key] = parsed
        result_str = f"{parsed[0]}\t{parsed[1]:.2f}ns" if parsed else "ERROR"
        print(
            f"  [{i:>6}/{len(successful_keys)}] run"
            f" {shape1}({size1}) × {shape2}({size2})"
            f" {method} [{type_key}] {result_str}",
            flush=True,
        )

    # ── Step 4: assemble results, compare against ground truth ───────────────
    # Group by (shape1, size1, shape2, size2, method)
    groups: dict[tuple, dict[str, tuple[int, float] | None]] = {}
    for (shape1, size1, shape2, size2, method, type_key), parsed in run_results.items():
        groups.setdefault((shape1, size1, shape2, size2, method), {})[type_key] = parsed
    for (shape1, size1, shape2, size2, method, type_key) in srcs:
        if not compile_ok.get((shape1, size1, shape2, size2, method, type_key)):
            groups.setdefault((shape1, size1, shape2, size2, method), {})[type_key] = None

    output_entries: list[dict] = []

    for (shape1, size1, shape2, size2, method), type_data in groups.items():
        gt = type_data.get(GROUND_TRUTH_TYPE)
        gt_count = gt[0] if gt is not None else None

        type_entries: dict[str, dict] = {}
        for type_key, _ in types:
            parsed = type_data.get(type_key)
            if not compile_ok.get((shape1, size1, shape2, size2, method, type_key), False):
                type_entries[type_key] = {"status": "compile_error"}
                continue
            if parsed is None:
                type_entries[type_key] = {"status": "run_error"}
                continue
            count, ns = parsed
            matches = (gt_count is not None and count == gt_count)
            type_entries[type_key] = {
                "status":          "ok",
                "result":          count,
                "time_ns":         ns,
                "match_erational": matches,
            }

        output_entries.append({
            "shape1":  shape1,
            "size1":   size1,
            "shape2":  shape2,
            "size2":   size2,
            "method":  method,
            "types":   type_entries,
        })

    # ── Step 5: write JSON ───────────────────────────────────────────────────
    payload = {
        "meta": {
            "timestamp":    datetime.now(timezone.utc).isoformat(),
            "commit":       commit,
            "cpu":          cpu or None,
            "compiler":     cxx,
            "cxxflags":     args.cxxflags,
            "repetitions":  args.repetitions,
            "n_shapes":     N_SHAPES,
            "n_pairs":      N_SHAPES * N_SHAPES,
            "shapes":       shapes,
            "sizes":        sizes,
            "methods":      methods,
            "number_types": [k for k, _ in types],
            "ground_truth": GROUND_TRUTH_TYPE,
        },
        "results": output_entries,
    }
    output_path.write_text(json.dumps(payload, indent=2))

    n_entries = len(output_entries)
    ok_count  = sum(
        1 for e in output_entries
        if any(v.get("status") == "ok" for v in e["types"].values())
    )
    print(f"\n{ok_count}/{n_entries} (shape×size, method) combinations had at least one successful run.")
    print(f"Results → {output_path}")


if __name__ == "__main__":
    main()
