#!/usr/bin/env python3
"""cgal_ratios.py — recompute the speed table in doc/raw/cgal.md.

That table is one number per problem: pgl's time divided by CGAL's, so below 1
means pgl is faster. Both sides come from the recorded history rather than a
fresh run — the pgl times from history/asymptotic/<driver>.jsonl at one commit,
the CGAL times from each machine's history/asymptotic-baseline/<machine>.json,
a reference overwritten by `record.sh baseline` and not tied to any commit. So
this script measures nothing; it only reduces what record.sh already stored.

The reduction, matching the methodology the page documents:

  * pgl times are only ever divided by CGAL times from the same machine (CPU and
    compiler family, see bench_machine.py). Every machine holding both yields
    its own row, and the table averages those rows' numbers over the machines;
    the range spans every ratio of every machine.
  * a cell is one (driver, dataset, problem, number type); within it, each
    library keeps the one algorithm that is fastest on average over the sweep
    (smallest geometric-mean time), not the fastest at each size, so a method
    that wins only part of the range is not raced there. `algorithms=` pins
    pgl's side where the page is deliberately not claiming the best (searches
    are always ShapeTree, segment intersection is always findIntersections).
  * a cell's ratio list is one entry per size of the sweep; its median is the
    cell's number.
  * a row covering several datasets averages their medians, and its range
    spans every size of every dataset it covers.

Each driver is read at its own newest commit on each machine, because runs are routinely
partial: `record.sh asymptotic --drivers triangulation` refreshes one driver and
leaves the rest of the page's rows measured where they last were. The commit
behind every row is printed on stderr, so a table mixing two runs says so.
Pass --commit to pin every row to one commit instead, which is the way to check
this script against a page written from a single full run.
Machines are listed on stderr the same way.

Usage (from the repo root):

    python3 tests/benchmark/cgal_ratios.py              # markdown rows
    python3 tests/benchmark/cgal_ratios.py --commit c20f09d
    python3 tests/benchmark/cgal_ratios.py --check      # diff against doc/raw/cgal.md
"""

import argparse
import collections
import json
import math
import pathlib
import re
import statistics
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tests/benchmark"))
from bench_machine import read_baselines, record_machine  # noqa: E402

# ── The table ────────────────────────────────────────────────────────────────
# One entry per row of the page, in the page's column order: the label, the
# cells averaged into it, and the two code spans naming what was raced. A cell
# is (driver, dataset, problem) plus an optional pgl algorithm whitelist.
Cell = collections.namedtuple("Cell", "driver dataset problem algorithms")


def cell(driver, dataset, problem, algorithms=None):
    return Cell(driver, dataset, problem, algorithms)


# The Salzburg Database polygons, swept by every category over a polygon
# except the segment intersections, and by the union without fpg.
SBPD_DATASETS = ("fpg", "spg", "fpg-holes")
SEGMENT_DATASETS = ("small", "large", "sheared", "polygon edges")
ARRANGEMENT_DATASETS = ("small segments", "large segments", "mixed", "voronoi")

ROWS = [
    ("Segment intersection",
     [cell("segmentintersections", d, "intersections", {"findIntersections"})
      for d in SEGMENT_DATASETS],
     "`findIntersections(v)`",
     "<code>compute_<wbr>intersection_points</code>"),
    ("Minkowski sum",
     [cell("minkowskisum", d, "Minkowski sum")
      for d in ("large + large", "large + small", *SBPD_DATASETS)],
     "`a.minkowskiSum(b)`{Polygon}",
     "`minkowski_sum_2`"),
    ("Delaunay triangulation",
     [cell("triangulation", "random", "build"),
      cell("pointconstructions", "random", "Delaunay")],
     "`Triangulation`", "`Delaunay_triangulation_2`"),
    ("kd-tree build",
     [cell("pointconstructions", "random", "kd-tree"),
      cell("pointsearch", "random", "build")],
     "`ShapeTree`", "`Kd_tree`"),
    ("Arrangement build",
     [cell("arrangement", d, "build") for d in ARRANGEMENT_DATASETS],
     "`Arrangement`", "`Arrangement_2`"),
    ("Regularized union, two polygons",
     [cell("regularizedunion", d, "union") for d in ("large + large", "spg", "fpg-holes")],
     "`a.regularizedUnion(b)`{Polygon}",
     "`CGAL::join`"),
    ("Triangulation point location",
     [cell("triangulation", "random", "locate")],
     "`t.locate(p)`{Triangulation}",
     "<code>Triangulation_hierarchy_2<wbr>::locate</code>"),
    ("Segment search build",
     [cell("segmentsearch", "small segments", "build", {"ShapeTree"})],
     "`ShapeTree`", "`AABB_tree`"),
    ("Convex hull",
     [cell("pointconstructions", "random", "convex hull")],
     "`convexHull(v)`", "`convex_hull_2`"),
    ("Point search, count in Triangle",
     [cell("pointsearch", "random", "count in Triangle")],
     "`ShapeTree`", "`Kd_tree::search`"),
    ("Segment search, count in Triangle",
     [cell("segmentsearch", "small segments", "count in Triangle", {"ShapeTree"})],
     "`ShapeTree`", "`AABB_tree`"),
    ("Triangulation point-location build",
     [cell("triangulation", "random", "buildPointLocation")],
     "`t.buildPointLocation()`{Triangulation}",
     "<code>Triangulation_<wbr>hierarchy_2</code>"),
    ("Segment search, count in Rectangle",
     [cell("segmentsearch", "small segments", "count in Rectangle", {"ShapeTree"})],
     "`ShapeTree`", "`AABB_tree`"),
    ("Arrangement point-location build",
     [cell("arrangement", d, "buildPointLocation") for d in ARRANGEMENT_DATASETS],
     "`a.buildPointLocation()`{Arrangement}",
     "<code>Arr_trapezoid_ric_<wbr>point_location</code>"),
    ("Visibility, visible vertices",
     [cell("visibility", d, "visible vertices") for d in ("polygon", *SBPD_DATASETS)],
     "`t.visibleVertices(p)`{Triangulation}",
     "<code>Triangular_expansion_<wbr>visibility_2</code>"),
    ("Arrangement point location query",
     [cell("arrangement", d, "locateFace") for d in ARRANGEMENT_DATASETS],
     "`a.locateFace(p)`{Arrangement}",
     "<code>Arr_trapezoid_ric_<wbr>point_location<wbr>::locate</code>"),
    ("Nearest neighbor query",
     [cell("pointsearch", "random", "nearest neighbor")],
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

# Cells the page marks with a footnote, keyed by (row label, column index). The
# ratio is measured like every other, but the two sides are not answering the
# same question and the note under the table says which. The sweep's `int`
# column: EPICK loses intersection points there, while pgl's `int` sweep is
# exact (see asymptotic/baseline/cgal.hpp). And the Minkowski sum, where the
# baseline keeps CGAL's fastest method over the whole sweep rather than the
# fastest at each size (see asymptotic/baseline/minkowskisum.cpp). The union,
# where the fastest CGAL method differs by dataset (see
# asymptotic/baseline/regularizedunion.cpp). And visibility's `int` column,
# which covers the random polygon alone: EPICK fails on the SBPD polygons, so
# the baseline runs them under EPECK only (see asymptotic/baseline/visibility.cpp).
FOOTNOTE = {("Segment intersection", 1): "\\*", ("Minkowski sum", 0): "†",
            ("Regularized union, two polygons", 0): "‡",
            ("Visibility, visible vertices", 1): "§"}

# The note the page prints under the table, and the ratio it quotes: pgl `int`
# against EPECK, the kernel that computes the answer pgl computes. That ratio is
# not a column of the table, so it is recomputed here rather than typed in by
# hand, and the sentence is emitted with it so refreshing the note is a copy.
# Keep NOTE's wording in step with doc/raw/cgal.md — it is the page's sentence,
# not this script's.
FOOTNOTE_EXACT = ("Segment intersection", "int", "EPECK")
NOTE = ("\\* CGAL's sweep line runs under EPICK here, which is not exact. "
        "pgl's `int` `findIntersections` is exact and {ratio} against EPECK.")
MINKOWSKI_NOTE = ("† CGAL runs its fastest method over the whole input range: the "
                  "Hertel–Mehlhorn decomposition on the random polygons, where its "
                  "reduced convolution is faster below about 150 vertices, and "
                  "reduced convolution on fpg, spg and fpg-holes, where the "
                  "Hertel–Mehlhorn decomposition is faster on spg at 200 vertices.")
UNION_NOTE = ("‡ CGAL's free `join` on the random polygons and fpg-holes, and "
              "<code>General_polygon_set_2<wbr>::join</code> on spg, the faster "
              "of the two on each.")
VISIBILITY_NOTE = ("§ The random polygon only. On fpg, spg and fpg-holes, EPICK "
                   "answers wrongly or throws, so CGAL runs them under EPECK alone.")


# ── Loading ──────────────────────────────────────────────────────────────────
def history_dir():
    out = subprocess.run([sys.executable, str(ROOT / "tests/benchmark/bench_paths.py")],
                         capture_output=True, text=True, check=True)
    return pathlib.Path(out.stdout.strip())


def load(history):
    """The pgl records per driver per machine, and the CGAL baseline snapshots
    per machine."""
    pgl = {}
    for path in sorted((history / "asymptotic").glob("*.jsonl")):
        for line in path.open():
            if line.strip():
                r = json.loads(line)
                pgl.setdefault(path.stem, {}).setdefault(record_machine(r), []).append(r)
    return pgl, read_baselines(history)


def fastest_overall(records):
    """size -> time of the one algorithm fastest on average over the sweep.

    Averaged in log space (a geometric mean), so every size weighs the same
    rather than the largest dominating; only the sizes every algorithm reached
    are compared, and the winner's full series is returned."""
    series = collections.defaultdict(dict)
    for r in records:
        series[r["algorithm"]][r["size"]] = r["time"]
    common = set.intersection(*(set(s) for s in series.values()))
    best = min(series, key=lambda a: sum(math.log(series[a][s]) for s in common))
    return series[best]


def newest_commit(records):
    """The commit of the most recently dated record in a driver's history."""
    return max(records, key=lambda r: (r["date"], r["commit"]))["commit"]


def cell_ratios(c, commit, machine, pgl, baseline, pgl_number, cgal_number):
    """(commit, per-size ratios) for one cell on one machine, or None."""
    driver = pgl.get(c.driver, {}).get(machine, [])
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
    ours, ref = fastest_overall(mine), fastest_overall(theirs)
    sizes = sorted(set(ours) & set(ref))
    if not sizes:
        return None
    return at, [ours[s] / ref[s] for s in sizes]


def row_ratio(cells, commit, pgl, baselines, pgl_number, cgal_number):
    """(average over machines of the average of the per-dataset medians,
    smallest ratio, largest ratio, [(machine, commit)])."""
    averages, every, runs = [], [], set()
    for machine, snapshot in sorted(baselines.items()):
        measured = [cell_ratios(c, commit, machine, pgl, snapshot["results"],
                                pgl_number, cgal_number)
                    for c in cells]
        measured = [m for m in measured if m]
        if not measured:
            continue
        runs |= {(machine, at) for at, _ in measured}
        medians = [statistics.median(r) for _, r in measured]
        averages.append(sum(medians) / len(medians))
        every += [x for _, r in measured for x in r]
    if not averages:
        return None
    return sum(averages) / len(averages), min(every), max(every), sorted(runs)


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


def table(commit, pgl, baselines):
    """The rows that can be recomputed, sorted as the page sorts them."""
    rows, missing = [], []
    for label, cells, ours, theirs in ROWS:
        values = [row_ratio(cells, commit, pgl, baselines, p, c) for p, c in COLUMNS]
        if values[0] is None and values[1] is None:
            missing.append(label)
            continue
        runs = sorted({run for v in values if v for run in v[3]})
        rows.append((label, values, ours, theirs, runs))
    # Ordered by the like-for-like ERational column, the one every row has.
    rows.sort(key=lambda r: -r[1][0][0])
    return rows, missing


# A median below FAST is drawn dark green (pgl clearly faster), above SLOW dark
# red (CGAL clearly faster), judged on the printed two-digit value so a cell
# reading 0.80 or 1.2 is never colored. GitHub strips inline styles from
# Markdown, so the color goes through its math renderer instead, and every
# ratio cell goes through it, colored or not, so all the numbers share one font.
FAST, SLOW = 0.8, 1.2
GREEN, RED = "#006400", "#8b0000"


def render_colored(value):
    if value is None:
        return "—"
    median, low, high, _ = value
    shown = sig2(median)
    number = f"\\textsf{{{shown}×}}"
    color = GREEN if float(shown) < FAST else RED if float(shown) > SLOW else None
    if color:
        number = f"{{\\color{{{color}}}{number}}}"  # braces scope the switch
    return f"${number}\\textsf{{ ({sig2(low)}–{sig2(high)})}}$"


def cells(label, values):
    """A row's two rendered ratio cells, footnote marker included."""
    return [render_colored(v) + FOOTNOTE.get((label, i), "")
            for i, v in enumerate(values)]


def markdown(rows):
    out = ["| Problem | `ERational` / EPECK | `int` / EPICK | pgl | CGAL |",
           "| --- | --- | --- | --- | --- |"]
    for label, values, ours, theirs, _ in rows:
        a, b = cells(label, values)
        out.append(f"| {label} | {a} | {b} | {ours} | {theirs} |")
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
        want = tuple(cells(label, values))
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
    pgl, baselines = load(history)
    if not baselines:
        sys.exit(f"no CGAL baseline in {history}/")
    rows, missing = table(commit, pgl, baselines)
    if not rows:
        sys.exit(f"no pgl records matching a baseline's machine in {history}/asymptotic/")

    print(f"pgl {commit or 'at each driver\'s newest commit'} against the CGAL baselines",
          file=sys.stderr)
    for machine, snapshot in sorted(baselines.items()):
        meta = snapshot.get("meta", {})
        print(f"  {machine}: recorded for {meta.get('commit', '?')} "
              f"on {meta.get('timestamp', '?')[:10]}", file=sys.stderr)
    print(file=sys.stderr)
    if missing:
        print("No records at all:", file=sys.stderr)
        for label in missing:
            print(f"  {label}", file=sys.stderr)
        print(file=sys.stderr)

    machines = sorted({m for *_, runs in rows for m, _ in runs})
    for label, _, _, _, runs in rows:
        where = ",".join(at if len(machines) == 1 else f"{at}@{machines.index(m) + 1}"
                         for m, at in runs)
        print(f"  {where:20s} {label}", file=sys.stderr)
    if len(machines) > 1:
        for i, machine in enumerate(machines, 1):
            print(f"  @{i} = {machine}", file=sys.stderr)
    print(file=sys.stderr)

    if args.check:
        stale = check(rows)
        print(f"\n{stale} of {len(rows)} recomputable rows are stale."
              if stale else f"\nAll {len(rows)} recomputable rows match the page.")
        sys.exit(1 if stale else 0)

    print(markdown(rows))

    label, pgl_number, cgal_number = FOOTNOTE_EXACT
    row = next((r for r in ROWS if r[0] == label), None)
    if row:
        exact = row_ratio(row[1], commit, pgl, baselines, pgl_number, cgal_number)
        if exact:
            print("\n" + NOTE.format(ratio=render(exact)))
    for note in (MINKOWSKI_NOTE, UNION_NOTE, VISIBILITY_NOTE):
        print("\n" + note)


if __name__ == "__main__":
    main()
