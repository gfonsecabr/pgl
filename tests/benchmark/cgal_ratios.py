#!/usr/bin/env python3
"""cgal_ratios.py — recompute the speed table in doc/raw/cgal.md.

That table is one number per problem: pgl's time divided by CGAL's, so below 1
means pgl is faster. Both sides come from the recorded history rather than a
fresh run — the pgl times from history/asymptotic/<driver>.jsonl at one commit,
the CGAL times from history/asymptotic-baseline.json, which is a single shared
reference overwritten by `record.sh baseline` and not tied to any commit. So
this script measures nothing; it only reduces what record.sh already stored.

The reduction, matching the methodology the page documents:

  * a cell is one (driver, dataset, problem, number type); within it, each
    library keeps its best algorithm at every size, so the ratio races the
    faster of what each offers rather than a fixed pair. `algorithms=` pins
    pgl's side where the page is deliberately not claiming the best (searches
    are always ShapeTree, segment intersection is always findIntersections).
  * a cell's ratio list is one entry per size of the sweep; its median is the
    cell's number.
  * a row covering several datasets averages their medians, and its range
    spans every size of every dataset it covers.

Each driver is read at its own newest commit, because runs are routinely
partial: `record.sh asymptotic --drivers triangulation` refreshes one driver and
leaves the rest of the page's rows measured where they last were. The commit
behind every row is printed on stderr, so a table mixing two runs says so.
Pass --commit to pin every row to one commit instead, which is the way to check
this script against a page written from a single full run.

Usage (from the repo root):

    python3 tests/benchmark/cgal_ratios.py              # markdown rows
    python3 tests/benchmark/cgal_ratios.py --commit c20f09d
    python3 tests/benchmark/cgal_ratios.py --check      # diff against doc/raw/cgal.md
"""

import argparse
import collections
import json
import pathlib
import re
import statistics
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]

# ── The table ────────────────────────────────────────────────────────────────
# One entry per row of the page, in the page's column order: the label, the
# cells averaged into it, and the two code spans naming what was raced. A cell
# is (driver, dataset, problem) plus an optional pgl algorithm whitelist.
Cell = collections.namedtuple("Cell", "driver dataset problem algorithms")


def cell(driver, dataset, problem, algorithms=None):
    return Cell(driver, dataset, problem, algorithms)


SEGMENT_DATASETS = ("small segments", "large segments", "polygon edges")

ROWS = [
    ("Segment intersection",
     [cell("segmentintersections", d, "intersections", {"Bentley-Ottmann"})
      for d in SEGMENT_DATASETS],
     "`findIntersections(v)`",
     "<code>compute_<wbr>intersection_points</code>"),
    ("Minkowski sum",
     [cell("minkowskisum", d, "Minkowski sum") for d in ("large + large", "large + small")],
     "`a.minkowskiSum(b)`{Polygon}",
     "<code>minkowski_sum_by_<wbr>reduced_convolution_2</code>"),
    ("Delaunay triangulation",
     [cell("triangulation", "points", "build"),
      cell("pointconstructions", "points", "Delaunay")],
     "`Triangulation`", "`Delaunay_triangulation_2`"),
    ("kd-tree build",
     [cell("pointconstructions", "points", "kd-tree"),
      cell("pointsearch", "points", "build")],
     "`ShapeTree`", "`Kd_tree`"),
    ("Arrangement build",
     [cell("arrangement", d, "build") for d in ("small segments", "large segments")],
     "`Arrangement`", "`Arrangement_2`"),
    ("Regularized union, large + large",
     [cell("regularizedunion", "large + large", "union")],
     "`a.regularizedUnion(b)`{Polygon}",
     "<code>General_polygon_set_2<wbr>::join</code>"),
    ("Triangulation point location",
     [cell("triangulation", "points", "locate")],
     "`t.locate(p)`{Triangulation}",
     "<code>Triangulation_hierarchy_2<wbr>::locate</code>"),
    ("Segment search build",
     [cell("segmentsearch", "small segments", "build", {"ShapeTree"})],
     "`ShapeTree`", "`AABB_tree`"),
    ("Convex hull",
     [cell("pointconstructions", "points", "convex hull")],
     "`convexHull(v)`", "`convex_hull_2`"),
    ("Point search, count in Triangle",
     [cell("pointsearch", "points", "count in Triangle")],
     "`ShapeTree`", "`Kd_tree::search`"),
    ("Segment search, count in Triangle",
     [cell("segmentsearch", "small segments", "count in Triangle", {"ShapeTree"})],
     "`ShapeTree`", "`AABB_tree`"),
    ("Triangulation point-location build",
     [cell("triangulation", "points", "buildPointLocation")],
     "`t.buildPointLocation()`{Triangulation}",
     "<code>Triangulation_<wbr>hierarchy_2</code>"),
    ("Segment search, count in Rectangle",
     [cell("segmentsearch", "small segments", "count in Rectangle", {"ShapeTree"})],
     "`ShapeTree`", "`AABB_tree`"),
    ("Arrangement point-location build",
     [cell("arrangement", d, "buildPointLocation") for d in ("small segments", "large segments")],
     "`a.buildPointLocation()`{Arrangement}",
     "<code>Arr_trapezoid_ric_<wbr>point_location</code>"),
    ("Visibility, visible vertices",
     [cell("visibility", "polygon", "visible vertices")],
     "`t.visibleVertices(p)`{Triangulation}",
     "<code>Triangular_expansion_<wbr>visibility_2</code>"),
    ("Arrangement point location query",
     [cell("arrangement", d, "locateFace") for d in ("small segments", "large segments")],
     "`a.locateFace(p)`{Arrangement}",
     "<code>Arr_trapezoid_ric_<wbr>point_location<wbr>::locate</code>"),
    ("Nearest neighbor query",
     [cell("pointsearch", "points", "nearest neighbor")],
     "`t.nearestNeighbor(p)`{ShapeTree}",
     "<code>Orthogonal_k_<wbr>neighbor_search</code>"),
    ("Regularized union, triangles",
     [cell("regularizedunion", "triangles", "union")],
     "`regularizedUnionOf(v)`",
     "<code>General_polygon_set_2<wbr>::join</code>"),
]

# Each pgl number type races the CGAL kernel of the same strength; the two
# columns are independent measurements, not one scaled by the other.
COLUMNS = (("ERational", "EPECK"), ("int", "EPICK"))


# ── Loading ──────────────────────────────────────────────────────────────────
def history_dir():
    out = subprocess.run([sys.executable, str(ROOT / "tests/benchmark/bench_paths.py")],
                         capture_output=True, text=True, check=True)
    return pathlib.Path(out.stdout.strip())


def load(history):
    """The pgl records per driver, and the CGAL baseline records."""
    pgl = {}
    for path in sorted((history / "asymptotic").glob("*.jsonl")):
        pgl[path.stem] = [json.loads(line) for line in path.open() if line.strip()]
    baseline = json.loads((history / "asymptotic-baseline.json").read_text())
    return pgl, baseline["results"], baseline.get("meta", {})


def fastest_per_size(records):
    """The best time each size was achieved in, across the algorithms present."""
    best = {}
    for r in records:
        size = r["size"]
        if size not in best or r["time"] < best[size]:
            best[size] = r["time"]
    return best


def newest_commit(records):
    """The commit of the most recently dated record in a driver's history."""
    return max(records, key=lambda r: (r["date"], r["commit"]))["commit"]


def cell_ratios(c, commit, pgl, baseline, pgl_number, cgal_number):
    driver = pgl.get(c.driver, [])
    at = commit or (newest_commit(driver) if driver else None)
    mine = [r for r in driver
            if r["commit"] == at and r["dataset"] == c.dataset
            and r["problem"] == c.problem and r["type_label"] == pgl_number
            and (c.algorithms is None or r["algorithm"] in c.algorithms)]
    theirs = [r for r in baseline
              if r["driver"] == c.driver and r["dataset"] == c.dataset
              and r["problem"] == c.problem and r["number"] == cgal_number]
    if not mine or not theirs:
        return None
    ours, ref = fastest_per_size(mine), fastest_per_size(theirs)
    sizes = sorted(set(ours) & set(ref))
    if not sizes:
        return None
    return at, [ours[s] / ref[s] for s in sizes]


def row_ratio(cells, commit, pgl, baseline, pgl_number, cgal_number):
    """(average of the per-dataset medians, smallest ratio, largest ratio)."""
    measured = [cell_ratios(c, commit, pgl, baseline, pgl_number, cgal_number)
                for c in cells]
    measured = [m for m in measured if m]
    if not measured:
        return None
    commits = sorted({at for at, _ in measured})
    per_cell = [r for _, r in measured]
    medians = [statistics.median(r) for r in per_cell]
    every = [x for r in per_cell for x in r]
    return sum(medians) / len(medians), min(every), max(every), commits


# ── Rendering ────────────────────────────────────────────────────────────────
def sig2(x):
    """Two significant digits, the precision the page quotes."""
    if x >= 10:
        return f"{x:.0f}"
    if x >= 1:
        return f"{x:.1f}"
    return f"{x:.2f}"


def render(value):
    if value is None:
        return "—"
    median, low, high, _ = value
    return f"{sig2(median)}× ({sig2(low)}–{sig2(high)})"


def table(commit, pgl, baseline):
    """The rows that can be recomputed, sorted as the page sorts them."""
    rows, missing = [], []
    for label, cells, ours, theirs in ROWS:
        values = [row_ratio(cells, commit, pgl, baseline, p, c) for p, c in COLUMNS]
        if values[0] is None and values[1] is None:
            missing.append(label)
            continue
        commits = sorted({c for v in values if v for c in v[3]})
        rows.append((label, values, ours, theirs, commits))
    # Ordered by the like-for-like ERational column, the one every row has.
    rows.sort(key=lambda r: -r[1][0][0])
    return rows, missing


def markdown(rows):
    out = ["| Problem | `ERational` / EPECK | `int` / EPICK | pgl | CGAL |",
           "| --- | --- | --- | --- | --- |"]
    for label, values, ours, theirs, _ in rows:
        out.append(f"| {label} | {render(values[0])} | {render(values[1])} "
                   f"| {ours} | {theirs} |")
    return "\n".join(out)


# ── Checking the page ────────────────────────────────────────────────────────
PAGE = ROOT / "doc/raw/cgal.md"


def page_rows():
    """The label → (ERational cell, int cell) the page currently states."""
    stated = {}
    for line in PAGE.read_text().splitlines():
        m = re.match(r"^\| (\w[^|]*?) \| ([^|]+?) \| ([^|]+?) \| .* \| .* \|$", line)
        if m and "---" not in line:
            stated[m.group(1)] = (m.group(2), m.group(3))
    return stated


def check(rows):
    stated = page_rows()
    stale = 0
    for label, values, _, _, _ in rows:
        want = (render(values[0]), render(values[1]))
        have = stated.get(label)
        if have is None:
            print(f"  not on the page: {label}")
            stale += 1
        elif have != want:
            print(f"  {label}\n      page: {have[0]:22s} | {have[1]}"
                  f"\n      data: {want[0]:22s} | {want[1]}")
            stale += 1
    return stale


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--commit", help="library commit to read (default: HEAD)")
    ap.add_argument("--check", action="store_true",
                    help="report where doc/raw/cgal.md disagrees with the data")
    args = ap.parse_args()

    commit = args.commit
    history = history_dir()
    pgl, baseline, meta = load(history)
    rows, missing = table(commit, pgl, baseline)
    if not rows:
        sys.exit(f"no pgl records in {history}/asymptotic/")

    print(f"pgl {commit or 'at each driver\'s newest commit'} against the "
          f"CGAL baseline recorded for "
          f"{meta.get('commit', '?')} on {meta.get('timestamp', '?')[:10]}\n",
          file=sys.stderr)
    if missing:
        print("No records at all:", file=sys.stderr)
        for label in missing:
            print(f"  {label}", file=sys.stderr)
        print(file=sys.stderr)

    for label, _, _, _, commits in rows:
        print(f"  {','.join(commits):20s} {label}", file=sys.stderr)
    print(file=sys.stderr)

    if args.check:
        stale = check(rows)
        print(f"\n{stale} of {len(rows)} recomputable rows are stale."
              if stale else f"\nAll {len(rows)} recomputable rows match the page.")
        sys.exit(1 if stale else 0)

    print(markdown(rows))


if __name__ == "__main__":
    main()
