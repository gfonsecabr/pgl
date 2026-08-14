#!/usr/bin/env python3
"""
run_benchmarks.py — pgl shape-method benchmark runner.

For every combination of shape pair × size pair × number type × method, generates
a small C++ program that creates 100 random shapes of each kind (small or large),
times every pair-wise call to the method, and prints the elapsed time together
with an aggregate result (e.g. count of true returns, or count of zero-distance
pairs).

Each program stops early once it has spent --time-budget seconds in its measured
loop, and averages over the pairs it actually completed. Cost per call spans six
orders of magnitude across the cube, so this is what keeps a handful of cells —
crosses on two large PolygonWithHoles runs about 30 ms per call, against tens of
ns for a segment predicate — from setting the length of the whole run.

The script compiles and runs each program.  For each (shape1, size1, shape2,
size2, method) quintuple, the results across all number types are compared against
the ERational (exact) baseline.  Any type whose aggregate result disagrees with
ERational is marked as discarded in the JSON output.  Comparing aggregates only
means something when both sides summed over the same pairs, so where ERational
stops early the other types of that cell are capped at the same pair count, and
a capped program runs that prefix out whatever it costs — the time budget binds
only the uncapped ERational run that sets the cap.  Every type of a cell whose
baseline ran therefore has a verdict.  Such a cell is measured once whatever
--repetitions says, since a median over runs that each cost a full budget and
each covered different pairs is not worth what it costs.  All timing data are
written to a JSON file.

Usage (from repo root):
    python3 tests/benchmark/run_shapepairs.py [options]

Options:
    --shapes  S,...    Shapes to include (default: all 19, plus Point as shape B).
                       Point only appears as the second operand and is
                       size-agnostic (one variant, ignoring --sizes).
    --focus   S,...    Run these shapes against everything: keep only pairs where
                       at least one operand is a focus shape (its row AND column).
                       Focus shapes are added to --shapes automatically.
    --sizes   S,...    Size variants for both operands: small, large, or both
                       (default: small,large)
    --sizes-a S,...    Size variants for shape A only, overriding --sizes
    --sizes-b S,...    Size variants for shape B only, overriding --sizes
                       (e.g. --sizes-a large --sizes-b small runs only the
                       large-A × small-B corner instead of the full cube)
    --methods M,...    Methods to include (default: all 21)
    --types   T,...    Number types (default: all 5)
    --output  FILE     JSON output path (default: build/tests/benchmark/benchmarks.json)
    --build-dir DIR    Build root  (default: build/tests/benchmark)
    --cxx     CXX      Compiler    (default: $CXX or c++)
    --cxxflags FLAGS   Flags       (default: $CXXFLAGS or -std=c++23 -O3 -DNDEBUG)
    --jobs    N        Parallel compile jobs (default: all available CPUs)
    --time-budget S    Seconds an uncapped program may spend in its measured
                       loop before stopping early (default: 15; 0 disables the
                       limit). Does not apply to the capped runs that replay
                       the baseline's prefix.
    --keep-build       Keep the generated sources and compiled binaries. A run
                       that completes normally deletes them by default; one that
                       fails or is interrupted keeps them either way.
    --dry-run          Write C++ sources but do not compile or run
"""

from __future__ import annotations
import argparse
import itertools
import json
import os
import shutil
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
    "Polygon",
    "PolygonWithHoles",
    "Polyline",
    "MonotoneChain",
    "HalfplaneIntersection",
    # Same geometry as an existing shape, but stored as a more general type so the
    # cube exercises the storage type's code paths on that geometry.
    "TriangleAsPolygon",
    "TriangleAsConvex",
    "ConvexAsPolygon",
    "PolygonAsPWH",
    # Same geometry as Polygon, held as its constrained Delaunay triangulation, so
    # a predicate scanning the polygon can be compared against the same predicate
    # walking its mesh. Building the mesh is setup; only the queries are timed.
    "PolygonAsTriangulation",
]

# A bare Point has no extent, so it only ever appears as the second operand
# (shape B) and has a single, size-agnostic variant — "point small" and "point
# large" would be identical. Its variants use POINT_SIZE instead of small/large.
SHAPES_B_EXTRA = ["Point"]
POINT_SIZE = "n/a"

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
    "pointInsideInteriorContainedIn",
    "intersects",
    "interiorsIntersect",
    "separates",
    "crosses",
    "collinear",
    "parallel",
    "intersection",
    "regularizedIntersection",
    "regularizedUnion",
    "difference",
    "symmetricDifference",
    "minkowskiSum",
    "squaredDistance",
    "distanceL1",
    "distanceLInf",
    "squaredHausdorffDistance",
    "hausdorffDistanceL1",
    "hausdorffDistanceLInf",
]

# Ground-truth type for cross-type comparison
GROUND_TRUTH_TYPE = "ERational"

# Number of random shapes generated per type per benchmark
N_SHAPES = 100

# The regularized boolean operations build a whole arrangement per call — 2 to
# 5 ms for a pair of 32-gons, against nanoseconds for a predicate — so a full
# 100×100 cell would take the best part of a minute. They run on a smaller
# sample instead. The generators are deterministic, so this is the first
# N_CONSTRUCTION_SHAPES of the same shapes every other method sees.
N_CONSTRUCTION_SHAPES = 20
_CONSTRUCTION_METHODS = {"regularizedIntersection", "regularizedUnion", "difference",
                         "symmetricDifference"}

# Wall-clock ceiling on the measured loop of a single generated program. The
# cost per call spans six orders of magnitude across the cube — tens of ns for
# a segment predicate against tens of ms for crosses on a pair of large
# PolygonWithHoles — so without a ceiling the handful of expensive cells decide
# how long a full run takes. A truncated cell still reports a mean over the
# pairs it did complete; see generate_source.
TIME_BUDGET_S = 15.0

# Wall-clock ceiling on a whole benchmark process, the backstop against one that
# hangs rather than a second opinion on how long a run may take. What bounds a
# legitimate run is the time budget above when it is uncapped, and the baseline's
# prefix when it is capped; this only has to sit clear of both.
RUN_TIMEOUT_S = 600.0


def _n_shapes_for(method: str) -> int:
    """How many random shapes per operand a method is measured on."""
    return N_CONSTRUCTION_SHAPES if method in _CONSTRUCTION_METHODS else N_SHAPES


# Shape-kind categories (drives which randomXxx helper is called)
_BISHAPES  = {"Segment", "OrientedSegment", "Line", "OrientedLine", "Rectangle"}
_TRISHAPES = {"Triangle", "Disk"}
# "Convex" gets its own generator call


# ─── C++ source generation ───────────────────────────────────────────────────

def _cpp_shape_type(shape: str) -> str:
    """C++ type for a shape kind, parameterised on the number type N."""
    if shape == "Point":
        return "pgl::Point<N>"
    # "As-other-type" shapes are generated as one shape but stored as another.
    if shape in ("TriangleAsPolygon", "ConvexAsPolygon"):
        return "pgl::Polygon<pgl::Point<N>>"
    if shape == "TriangleAsConvex":
        return "pgl::Convex<pgl::Point<N>>"
    if shape == "PolygonAsPWH":
        return "pgl::PolygonWithHoles<pgl::Point<N>>"
    if shape == "PolygonAsTriangulation":
        return "pgl::Triangulation<pgl::Triangle<pgl::Point<N>>>"
    return f"pgl::{shape}<pgl::Point<N>>"


def _cpp_make_shapes_for(shape: str, size: str, alias: str, var: str, n: int) -> str:
    """C++ statement that fills 'var' with n random shapes of kind 'shape'."""
    if shape == "Point":
        return f"auto {var} = randomPoints<N>({n});"
    prefix = "randomSmall" if size == "small" else "randomLarge"
    if shape in _BISHAPES:
        return f"auto {var} = {prefix}Bishape<{alias}>({n});"
    if shape in _TRISHAPES:
        return f"auto {var} = {prefix}Trishape<{alias}>({n});"
    if shape == "Polygon":
        return f"auto {var} = {prefix}Polygons<N>({n}, 32);"
    if shape == "PolygonWithHoles":
        # A 32-gon punched with 6 holes of 6 vertices each.
        return f"auto {var} = {prefix}PolygonsWithHoles<N>({n}, 32, 6, 6);"
    if shape == "Polyline":
        return f"auto {var} = {prefix}Polylines<N>({n}, 32);"
    if shape == "MonotoneChain":
        return f"auto {var} = {prefix}MonotoneChains<N>({n}, 32);"
    if shape == "HalfplaneIntersection":
        return f"auto {var} = {prefix}HalfplaneIntersections<N>({n}, 1000);"
    if shape == "TriangleAsPolygon":
        return f"auto {var} = {prefix}TriangleAsPolygon<N>({n});"
    if shape == "TriangleAsConvex":
        return f"auto {var} = {prefix}TriangleAsConvex<N>({n});"
    if shape == "ConvexAsPolygon":
        return f"auto {var} = {prefix}ConvexAsPolygon<N>({n}, 1000);"
    if shape == "PolygonAsPWH":
        return f"auto {var} = {prefix}PolygonAsPWH<N>({n}, 32);"
    if shape == "PolygonAsTriangulation":
        return f"auto {var} = {prefix}PolygonAsTriangulation<N>({n}, 32);"
    return f"auto {var} = {prefix}Convexes<N>({n}, 1000);"


def _cpp_accumulate(method: str) -> str:
    """C++ inner-loop body that increments 'count' for one (a, b) pair."""
    if method in {"contains", "interiorContains", "boundaryContains",
                  "pointInsideInteriorContainedIn",
                  "intersects", "interiorsIntersect", "separates", "crosses",
                  "collinear", "parallel"}:
        return f"count += a.{method}(b) ? 1 : 0;"
    if method == "intersection":
        # intersection returns std::optional for most shape pairs,
        # std::vector for the ones whose result can have several disjoint
        # pieces (Polygon/Polyline/MonotoneChain/PolygonWithHoles/PolygonSet
        # vs a non-Point operand), and a
        # bare HalfplaneIntersection for the pairs closed under intersection
        # (HalfplaneIntersection vs Rectangle/Triangle/Convex/Halfplane/region).
        # Count a non-empty result the same way for all three: the shape and
        # the vector both answer `empty()`, so only the optional needs a case
        # of its own. A generic lambda makes the argument dependent so
        # `if constexpr` actually discards the branch that doesn't apply (a
        # plain `if constexpr` in the non-template main() would still
        # type-check both).
        return ("count += [](const auto& r) {"
                " if constexpr (requires { r.has_value(); }) return r.has_value() ? 1 : 0;"
                " else return r.empty() ? 0 : 1;"
                " }(a.template intersection<N>(b));")
    if method in {"regularizedIntersection", "regularizedUnion", "difference",
                  "symmetricDifference"}:
        # The regularized boolean operations. Each returns the pieces of the
        # result as a PolygonSet. `regularizedUnion` is defined for every ordered pair
        # of the six shapes that are bounded polygonal regions, each pair on the
        # higher-ranked of its operands with the lower-ranked one forwarding.
        # `difference` is not symmetric, so it has no forwarders at all and only
        # appears with a polygon, a region or a set as operand A;
        # `symmetricDifference` forwards but is not defined among the three
        # bounded convex shapes. `regularizedIntersection` needs a
        # PolygonWithHoles or a PolygonSet on one side, and takes an unbounded
        # operand — a Halfplane or a HalfplaneIntersection — on the other.
        #
        # Counting non-empty results the way `intersection` does would be
        # useless here: A ∪ B is never empty, so the aggregate would be the same
        # constant for every type. The piece count plus the total vertex count
        # is a real digest of the arrangement instead — it disagrees as soon as
        # a type merges or splits a piece differently, or rounds a crossing onto
        # a different vertex — and it has to materialize the whole result, so
        # the compiler cannot delete the construction.
        return ("count += [](const auto& pieces) {"
                " return (long long)pieces.componentCount()"
                "      + (long long)pieces.vertexCount();"
                f" }}(a.template {method}<N>(b));")
    if method == "minkowskiSum":
        # The result type follows the pair: a `PolygonSet` when the sum can fall
        # into several pieces, a `PolygonWithHoles` when it is one region that may
        # have holes, and otherwise a single shape stored by its defining points —
        # a translated operand, a bounded or unbounded convex shape, a `Disk`.
        # Digest the first two the way the boolean operations do, and retain a
        # coordinate read for the third: a fixed-size translated shape would
        # otherwise let the compiler erase the whole construction. `componentCount`
        # is what tells a set from a region, and `holeCount` a region from the
        # rest — `vertexCount` does not, since `HalfplaneIntersection` has one too.
        # A generic lambda makes the branches dependent, so `if constexpr` discards
        # the inapplicable result representations.
        return ("count += [](const auto& s) {"
                " if constexpr (requires { s.componentCount(); }) {"
                "   return (long long)s.componentCount() + (long long)s.vertexCount();"
                " } else if constexpr (requires { s.holeCount(); }) {"
                "   return (long long)s.holeCount() + (long long)s.vertexCount();"
                " } else {"
                "   const auto element = s.get(0);"
                "   const auto vertex = [](const auto& e) {"
                "     if constexpr (requires { e.source(); }) return e.source();"
                "     else return e; }(element);"
                "   const typename std::remove_cvref_t<decltype(vertex)>::NumberType zero{};"
                "   return (long long)s.size() + ((vertex.x() < zero) ? 1 : 0);"
                " }"
                " }(a.minkowskiSum(b));")
    if method in {"squaredDistance", "distanceL1", "distanceLInf",
                  "squaredHausdorffDistance", "hausdorffDistanceL1",
                  "hausdorffDistanceLInf"}:
        return (f"{{ const auto d = a.{method}(b);"
                    " const decltype(d) zero{}; count += (d == zero) ? 1 : 0; }")
    raise ValueError(f"Unknown method: {method!r}")


def generate_source(
    shape1: str, size1: str,
    shape2: str, size2: str,
    method: str, cpp_type: str,
    time_budget_s: float = TIME_BUDGET_S,
) -> str:
    """Return a complete, self-contained C++ benchmark source.

    The generated program stops as soon as it has spent 'time_budget_s' in the
    measured loop, and divides by the pairs it actually got through. A cell that
    finishes the full n² pairs is unaffected; one that does not reports a timing
    over a prefix of them, plus the size of that prefix so the caller can tell
    the two apart. Passing a pair count as argv[1] caps the loop there and turns
    the budget off, which is how the runner replays one type's prefix on another
    and gets back something it can actually compare.
    """
    n = _n_shapes_for(method)
    # A non-positive budget means "run to completion". Rather than generating a
    # second shape of loop for that case, push the deadline out of reach: the
    # clock is then read once per 64 pairs and never trips, which is under a
    # tenth of a percent of even the cheapest cell's per-call cost.
    budget_ns = time_budget_s * 1e9 if time_budget_s > 0 else 1e18
    make1 = _cpp_make_shapes_for(shape1, size1, "S1", "shapes1", n)
    make2 = _cpp_make_shapes_for(shape2, size2, "S2", "shapes2", n)
    accum = _cpp_accumulate(method)

    lines = [
        f"// Benchmark: {shape1}({size1}) × {shape2}({size2}) :: {method}  [{cpp_type}]",
        "#include <cstdlib>",
        "#include <iostream>",
        "#include <limits>",
        '#include "pgl.hpp"',
        '#include "randomshapes.hpp"',
        '#include "plf_nanotimer.h"',
        "",
        f"using N  = {cpp_type};",
        f"using S1 = {_cpp_shape_type(shape1)};",
        f"using S2 = {_cpp_shape_type(shape2)};",
        "",
        "int main(int argc, char** argv) {",
        "    // argv[1], when given, caps the number of pairs. The runner uses it",
        "    // to hold every number type of a cell to the prefix the exact",
        "    // baseline managed, so their aggregates stay comparable.",
        "    const long long arg_pairs = (argc > 1) ? std::atoll(argv[1]) : 0;",
        "    const bool capped = arg_pairs > 0;",
        "    const long long max_pairs =",
        "        capped ? arg_pairs : std::numeric_limits<long long>::max();",
        "    // A capped run has to finish the prefix it was given. It exists to be",
        "    // compared against the run that set the cap, and one that stopped",
        "    // somewhere short of it compares against nothing — so the time budget",
        "    // applies only to an uncapped run, which has nothing to line up with.",
        "    const double deadline_ns =",
        f"        capped ? std::numeric_limits<double>::infinity() : {budget_ns:.1f};",
        f"    {make1}",
        f"    {make2}",
        "    plf::nanotimer timer;",
        "    timer.start();",
        "    long long count = 0;",
        "    long long calls = 0;",
        "    bool stop = false;",
        "    for (const auto& a : shapes1) {",
        "        for (const auto& b : shapes2) {",
        f"            {accum}",
        "            ++calls;",
        "            if (calls >= max_pairs) { stop = true; break; }",
        "            // Reading the clock costs on the order of the cheapest call",
        "            // being measured, so amortize it over 64 pairs. The overshoot",
        "            // that buys is 63 calls past the budget, which only matters",
        "            // for the cells that are already far over it.",
        "            if ((calls & 63) == 0 && timer.get_elapsed_ns() >= deadline_ns) {",
        "                stop = true;",
        "                break;",
        "            }",
        "        }",
        "        if (stop) break;",
        "    }",
        "    double ns = timer.get_elapsed_ns() / (double)(calls > 0 ? calls : 1);",
        '    std::cout << count << "\\t" << ns << "\\t" << calls << "\\n";',
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


def run_binary(binary: Path,
               repetitions: int = 1,
               max_pairs: int = 0,
               timeout_s: float = RUN_TIMEOUT_S,
               full_pairs: int = 0) -> tuple[bool, tuple[int, float, float, float, int] | str]:
    """Run binary 'repetitions' times; aggregate the timings.

    'max_pairs' caps the pairs the program may work through; 0 leaves it to run
    until its own time budget stops it. A capped program ignores that budget —
    it has a prefix to finish — so 'timeout_s' is the only thing bounding it,
    and killing it there would throw away the very comparison the cap was for.

    'full_pairs' is the pair count of an untruncated run. Given it, repetitions
    stop as soon as one comes back short of it: a run that stopped on the time
    budget spent the whole budget getting there, and where it stopped moves with
    the load on the machine, so repeating costs a budget apiece to take a median
    over measurements that did not cover the same work.

    Returns (True, (count, median_ns, min_ns, max_ns, calls)) — the median damps
    scheduler noise while min/max record the observed spread. On failure returns
    (False, message).

    A cell that completes all its pairs reports the same count every run. One
    that stops on the time budget does not, so the reported count and calls are
    taken from the same run that supplied the median time, keeping the triple
    internally consistent.
    """
    argv = [str(binary)] + ([str(max_pairs)] if max_pairs > 0 else [])
    runs: list[tuple[float, int, int]] = []   # (ns, count, calls)
    for _ in range(max(1, repetitions)):
        try:
            r = subprocess.run(argv, capture_output=True, text=True, timeout=timeout_s)
        except subprocess.TimeoutExpired:
            return False, "timeout"
        if r.returncode != 0:
            return False, r.stderr
        parsed = parse_output(r.stdout)
        if parsed is None:
            return False, r.stdout
        count, ns, calls = parsed
        runs.append((ns, count, calls))
        if full_pairs and calls < full_pairs:
            break
    if not runs:
        return False, ""
    runs.sort()
    median_ns, median_count, median_calls = runs[len(runs) // 2]
    return True, (median_count, median_ns, runs[0][0], runs[-1][0], median_calls)


def parse_output(raw: str) -> tuple[int, float, int] | None:
    """Parse 'count<TAB>ns<TAB>calls' output; returns None on malformed output.

    A two-field line is accepted as the pre-time-budget format, where the loop
    always ran to completion; calls comes back as 0, meaning "unknown".
    """
    lines = raw.strip().splitlines()
    if not lines:
        return None
    parts = lines[0].split("\t")
    if len(parts) not in (2, 3):
        return None
    try:
        return (int(parts[0]), float(parts[1]),
                int(parts[2]) if len(parts) == 3 else 0)
    except ValueError:
        return None


def remove_build_artifacts(src_dir: Path, bin_dir: Path,
                           keep: Path) -> tuple[int, int]:
    """Delete the generated sources and binaries; return (files, bytes) freed.

    The full cube is tens of thousands of files and several GB, and once the
    snapshot JSON is written none of it is worth keeping — the next run
    regenerates every source anyway. Only called after a run that finished, so
    a crash or a Ctrl-C leaves the sources on disk, which is what you want when
    chasing the compile error that caused it.
    """
    n_files = n_bytes = 0
    for d in (src_dir, bin_dir):
        # Refuse to take the results with the artifacts, in case --output put
        # the JSON inside one of these directories.
        if not d.is_dir() or keep.is_relative_to(d):
            continue
        for p in d.rglob("*"):
            if p.is_file():
                n_files += 1
                n_bytes += p.stat().st_size
        shutil.rmtree(d)
    return n_files, n_bytes


# ─── Main ────────────────────────────────────────────────────────────────────

def main() -> None:
    default_jobs = max(1, os.cpu_count() or 1)

    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    ap.add_argument("--shapes",    default=",".join(ALL_SHAPES + SHAPES_B_EXTRA))
    ap.add_argument("--focus",     default="",
                    help="Run these shapes against everything (both operand A and B)")
    ap.add_argument("--sizes",     default=",".join(ALL_SIZES))
    ap.add_argument("--sizes-a",   dest="sizes_a", default=None,
                    help="size variants for shape A, overriding --sizes")
    ap.add_argument("--sizes-b",   dest="sizes_b", default=None,
                    help="size variants for shape B, overriding --sizes")
    ap.add_argument("--methods",   default=",".join(ALL_METHODS))
    ap.add_argument("--types",     default=",".join(k for k, _ in ALL_NUMBER_TYPES))
    ap.add_argument("--output",    default=None)
    ap.add_argument("--build-dir", dest="build_dir", default=None)
    ap.add_argument("--cxx",       default=os.environ.get("CXX", "c++"))
    ap.add_argument("--cxxflags",  default=os.environ.get("CXXFLAGS", "-std=c++23 -O3 -DNDEBUG"))
    ap.add_argument("--jobs",      type=int, default=default_jobs)
    ap.add_argument("--time-budget", dest="time_budget", type=float,
                    default=TIME_BUDGET_S,
                    help="seconds a single benchmark program may spend in its "
                         "measured loop before stopping early (0 = no limit)")
    ap.add_argument("--repetitions", type=int, default=1,
                    help="run each binary N times and keep the median time; "
                         "cells the exact baseline could not finish inside the "
                         "time budget are run once regardless")
    ap.add_argument("--keep-build", dest="keep_build", action="store_true",
                    help="keep the generated sources and compiled binaries; "
                         "by default a run that completes deletes them")
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

    valid_shapes_a  = set(ALL_SHAPES)                    # operand a
    valid_shapes_b  = set(ALL_SHAPES) | set(SHAPES_B_EXTRA)  # operand b (adds Point)
    valid_sizes     = set(ALL_SIZES)
    valid_methods   = set(ALL_METHODS)
    valid_type_keys = {k for k, _ in ALL_NUMBER_TYPES}

    requested_shapes = [s.strip() for s in args.shapes.split(",") if s.strip()]
    focus = [s.strip() for s in args.focus.split(",") if s.strip()]

    valid_all = valid_shapes_a | valid_shapes_b
    bad_focus = [s for s in focus if s not in valid_all]
    if bad_focus:
        sys.exit(f"Unknown focus shape(s): {bad_focus}")
    # Focus shapes must be present in the operand universe to appear at all.
    for s in focus:
        if s not in requested_shapes:
            requested_shapes.append(s)

    shapes1 = [s for s in requested_shapes if s in valid_shapes_a]
    shapes2 = [s for s in requested_shapes if s in valid_shapes_b]
    sizes   = [s.strip() for s in args.sizes.split(",")   if s.strip() in valid_sizes]
    sizes_a = ([s.strip() for s in args.sizes_a.split(",") if s.strip() in valid_sizes]
               if args.sizes_a is not None else sizes)
    sizes_b = ([s.strip() for s in args.sizes_b.split(",") if s.strip() in valid_sizes]
               if args.sizes_b is not None else sizes)
    methods = [m.strip() for m in args.methods.split(",") if m.strip() in valid_methods]

    requested_keys = {k.strip() for k in args.types.split(",") if k.strip() in valid_type_keys}
    requested_keys.add(GROUND_TRUTH_TYPE)  # always include for comparison
    types = [(k, t) for k, t in ALL_NUMBER_TYPES if k in requested_keys]

    if not shapes1:
        sys.exit("No valid shape A specified.")
    if not shapes2:
        sys.exit("No valid shape B specified.")
    if not sizes_a:
        sys.exit("No valid sizes specified for shape A (use 'small', 'large', or both).")
    if not sizes_b:
        sys.exit("No valid sizes specified for shape B (use 'small', 'large', or both).")
    if not methods:
        sys.exit("No valid methods specified.")

    # A bare Point is size-agnostic: a single variant, independent of --sizes.
    def variants_of(shape: str, sizes_for: list[str]) -> list[tuple[str, str]]:
        return [(shape, POINT_SIZE)] if shape == "Point" else [(shape, sz) for sz in sizes_for]

    variants1 = [v for s in shapes1 for v in variants_of(s, sizes_a)]
    variants2 = [v for s in shapes2 for v in variants_of(s, sizes_b)]
    pairs = list(itertools.product(variants1, variants2))
    if focus:
        focus_set = set(focus)
        pairs = [(a, b) for a, b in pairs if a[0] in focus_set or b[0] in focus_set]
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
    print(f"  shape A: {shapes1}")
    print(f"  shape B: {shapes2}")
    if focus:
        print(f"  focus:   {focus} (kept pairs touching a focus shape)")
    if sizes_a == sizes_b:
        print(f"  sizes:   {sizes_a}")
    else:
        print(f"  sizes A: {sizes_a}")
        print(f"  sizes B: {sizes_b}")
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
        # Slashes (e.g. the "n/a" Point size) would break the path; sanitise.
        tag    = (f"{shape1}_{size1}_x_{shape2}_{size2}__{method}__{type_key}"
                  .replace("/", "-"))
        src    = src_dir / f"{tag}.cpp"
        binary = bin_dir / tag
        key    = (shape1, size1, shape2, size2, method, type_key)
        srcs[key] = src
        bins[key] = binary
        src.write_text(generate_source(shape1, size1, shape2, size2, method,
                                       cpp_type, args.time_budget))

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
    # parsed = (count, median_ns, min_ns, max_ns, calls)
    run_results: dict[tuple, tuple[int, float, float, float, int] | None] = {}

    # Run cell by cell with the exact baseline first. Where the baseline stops
    # on the time budget, every other type of that cell is held to the same
    # prefix of pairs, so the aggregates they aggregate over are the same set
    # and stay comparable. The baseline is the slowest type on almost every
    # cell, so this mostly costs the faster types nothing but the pairs they
    # would have spent going further than the comparison could follow.
    successful_keys = [k for k in srcs if compile_ok.get(k)]
    cells: dict[tuple, list[tuple]] = {}
    for key in successful_keys:
        cells.setdefault(key[:5], []).append(key)
    ordered_keys = [
        key
        for cell_keys in cells.values()
        for key in sorted(cell_keys, key=lambda k: k[5] != GROUND_TRUTH_TYPE)
    ]

    cap_for_cell: dict[tuple, int] = {}
    truncated_cell: dict[tuple, bool] = {}
    for i, key in enumerate(ordered_keys, 1):
        shape1, size1, shape2, size2, method, type_key = key
        full_pairs = _n_shapes_for(method) ** 2
        if type_key == GROUND_TRUTH_TYPE:
            # Uncapped, and told what an untruncated run looks like so it can
            # give up repeating the moment the budget starts cutting runs short.
            cap, reps, stop_early = 0, args.repetitions, full_pairs
        else:
            # A cell whose baseline was truncated is one of the expensive ones,
            # and every type of it is measured over a prefix rather than the
            # whole thing. Repeating that is minutes spent per cell to average
            # samples the baseline could not afford to take itself.
            cap = cap_for_cell.get(key[:5], 0)
            reps = 1 if truncated_cell.get(key[:5]) else args.repetitions
            stop_early = 0
        ok, res = run_binary(bins[key], reps, cap, full_pairs=stop_early)
        parsed = res if ok else None
        run_results[key] = parsed
        if type_key == GROUND_TRUTH_TYPE and parsed:
            cap_for_cell[key[:5]] = parsed[4]
            truncated_cell[key[:5]] = parsed[4] < full_pairs
        if parsed:
            cut = ("" if parsed[4] in (0, full_pairs)
                   else f"\t({parsed[4]}/{full_pairs} pairs)")
            result_str = f"{parsed[0]}\t{parsed[1]:.2f}ns{cut}"
        else:
            result_str = "ERROR"
        print(
            f"  [{i:>6}/{len(successful_keys)}] run"
            f" {shape1}({size1}) × {shape2}({size2})"
            f" {method} [{type_key}] {result_str}",
            flush=True,
        )

    # ── Step 4: assemble results, compare against ground truth ───────────────
    # Group by (shape1, size1, shape2, size2, method)
    groups: dict[tuple, dict[str, tuple[int, float, float, float, int] | None]] = {}
    for (shape1, size1, shape2, size2, method, type_key), parsed in run_results.items():
        groups.setdefault((shape1, size1, shape2, size2, method), {})[type_key] = parsed
    for (shape1, size1, shape2, size2, method, type_key) in srcs:
        if not compile_ok.get((shape1, size1, shape2, size2, method, type_key)):
            groups.setdefault((shape1, size1, shape2, size2, method), {})[type_key] = None

    output_entries: list[dict] = []

    for (shape1, size1, shape2, size2, method), type_data in groups.items():
        gt = type_data.get(GROUND_TRUTH_TYPE)
        gt_count = gt[0] if gt is not None else None
        gt_calls   = gt[4] if gt is not None else None
        full_pairs = _n_shapes_for(method) ** 2

        type_entries: dict[str, dict] = {}
        for type_key, _ in types:
            parsed = type_data.get(type_key)
            if not compile_ok.get((shape1, size1, shape2, size2, method, type_key), False):
                type_entries[type_key] = {"status": "compile_error"}
                continue
            if parsed is None:
                type_entries[type_key] = {"status": "run_error"}
                continue
            count, ns, lo, hi, calls = parsed
            # The aggregate only means the same thing on both sides when both
            # sides summed it over the same pairs. Capping to the baseline's
            # prefix is what normally makes that true; this catches the case it
            # cannot, a type slower than the baseline that ran out of budget
            # before reaching the cap. Not comparable is not the same as
            # disagreeing — hence None rather than False, which would read as
            # "this type got it wrong".
            if gt_count is None or calls != gt_calls:
                matches = None
            else:
                matches = (count == gt_count)
            type_entries[type_key] = {
                "status":          "ok",
                "result":          count,
                "time_ns":         ns,
                "time_min_ns":     lo,
                "time_max_ns":     hi,
                "calls":           calls,
                "truncated":       calls not in (0, full_pairs),
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
            "time_budget_s": args.time_budget or None,
            "n_shapes":     N_SHAPES,
            "n_pairs":      N_SHAPES * N_SHAPES,
            # The constructive boolean methods run on a smaller sample; see
            # _n_shapes_for. Only these methods depart from n_shapes above.
            "n_shapes_by_method": {m: _n_shapes_for(m) for m in methods},
            "shapes_a":     shapes1,
            "shapes_b":     shapes2,
            "focus":        focus or None,
            "sizes":        sizes_a if sizes_a == sizes_b else None,
            "sizes_a":      sizes_a,
            "sizes_b":      sizes_b,
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

    if args.keep_build:
        print(f"Build artifacts kept in {build_dir}")
    else:
        n_files, n_bytes = remove_build_artifacts(src_dir, bin_dir, output_path)
        print(f"Removed {n_files} generated files"
              f" ({n_bytes / 1e9:.2f} GB) from {build_dir}")


if __name__ == "__main__":
    main()
