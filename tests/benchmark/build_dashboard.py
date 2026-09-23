#!/usr/bin/env python3
"""build_dashboard.py — assemble the static benchmark dashboard from history.

Reads the JSONL history under --history (pair records at the top level,
asymptotic records under history/asymptotic/, plus each machine's overwritten
CGAL baseline snapshot), copies the dashboard template (index.html / asymptotic.html
/ app.js / style.css) into --out, and emits separate pair and asymptotic
payloads so each page only downloads the data it displays. Pure transformation,
no network, so it runs identically locally and in CI.

pairs.json shape:
  {
    "generated": <iso>,
    "machines":  [<machine>, ...],
    "dimensions": {shape1:[...], size1:[...], shape2:[...], size2:[...],
                   method:[...], type:[...]},   # each value list in display order
    "pairs": { <machine>: { "s1|sz1|s2|sz2|method|type":
                            [ {commit,date,time,result,match}, ... ] } },
    "asymptotic": {}
  }

The initial pairs payload contains the opening dashboard view (large first
operand, small second operand, and int). The remaining cell histories are
written as pairs-deferred.json and fetched after the page becomes idle.

asymptotic.json shape. The cube's four dimensions are carried flat rather than
pre-grouped by a fixed axis: the page lets the reader pick which one is the
multi-select "compare" axis at render time, so it has to be able to pivot on any
of them.
  {
    "generated": <iso>,
    "machines": [<machine>, ...],
    "pairs": {},
    "asymptotic": {
      <category>: {
        "dimensions": {dataset:[...], problem:[...], algorithm:[...], type:[...]},
        "unit": "µs",
        "machines": [...],
        # One entry per cube cell, holding the runs that measured it, oldest
        # first. A run is a whole curve: the sizes it swept and the time and
        # result signature at each. The page's history-depth control just takes
        # the last N runs of each series.
        "data": { <machine>: { "dataset|problem|algorithm|type":
                    [ {commit, date, points:[{size,time,min,max,output}, ...]}, ... ] } },
        # The CGAL reference, per machine that recorded one: a machine's pgl
        # curves are only ever drawn against its own. Keyed on dataset|problem;
        # `number` is the kernel that produced the curve (EPICK is shown against
        # pgl's int column, EPECK against ERational), `rank` selects the dash
        # pattern, and a curve may additionally name the pgl algorithm it
        # compares against.
        "baseline": { <machine>: { "dataset|problem":
                      [ {algorithm, number, rank, for_algorithm?, points:[...]} ] } },
        "source_url"?, "description"?,
        # How each dataset is produced, from the driver's `// @dataset` blocks.
        "datasets"?: { <dataset>: <text> }
      }
    }
  }
"""
from __future__ import annotations

import argparse
import datetime
import glob
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from bench_machine import read_baselines, record_machine  # noqa: E402
from bench_paths import default_history, missing_history_message  # noqa: E402

# Display orders mirroring run_shapepairs.py so the dashboard axes read naturally.
SHAPE_ORDER = ["Point", "Segment", "OrientedSegment", "Line", "OrientedLine",
               "Rectangle", "Triangle", "TriangleAsConvex", "TriangleAsPolygon",
               "Disk", "Convex", "ConvexAsPolygon", "HalfplaneIntersection",
               "Polygon", "PolygonAsPWH", "PolygonWithHoles", "PolygonSet",
               "PolygonAsTriangulation", "MonotoneChain", "Polyline"]
SIZE_ORDER = ["small", "large", "n/a"]
METHOD_ORDER = ["contains", "interiorContains", "boundaryContains",
                "pointInsideInteriorContainedIn",
                "intersects", "interiorsIntersect", "separates", "crosses",
                "collinear", "parallel", "intersection", "regularizedIntersection",
                "regularizedUnion", "difference", "symmetricDifference", "minkowskiSum",
                "squaredDistance", "distanceL1", "distanceLInf",
                "squaredHausdorffDistance", "hausdorffDistanceL1",
                "hausdorffDistanceLInf"]
TYPE_ORDER = ["int", "int128", "double", "BigInt", "Rational",
              "RationalBigInt", "ERational"]

DESC_RE = re.compile(r"//\s*@desc:\s*(.*)")
DATASET_RE = re.compile(r"//\s*@dataset\s+([^:]+):\s*(.*)")


# The microsecond symbol. Runs recorded before it was spelled properly carry
# "us" in the history, and the page should not show two spellings of one unit.
MICROSECONDS = "\u00b5s"

# The opening dashboard slice. Point operands carry the size-agnostic ``n/a``
# sentinel, so they belong to this slice regardless of the other operand's
# selected size.
INITIAL_PAIRS = {"size1": "large", "size2": "small", "type": "int"}
NO_SIZE = "n/a"


def canonical_unit(unit: str) -> str:
    return MICROSECONDS if unit in ("", "us", "\u00b5s", "\u03bcs") else unit


def order_key(order: list[str]):
    return lambda v: (order.index(v) if v in order else len(order), v)


def parse_comment_blocks(path: str) -> tuple[str, dict[str, str]]:
    """Read a benchmark source's `// @desc:` and `// @dataset <name>:` blocks.

    Each block runs from its tag line over the `//` lines that follow it, up to
    an empty comment line, the next `@` tag or the first line of code. Returns
    the description and a dataset name -> how-it-is-produced map.
    """
    desc: list[str] = []
    datasets: dict[str, list[str]] = {}
    current: list[str] | None = None
    try:
        with open(path, encoding="utf-8") as f:
            for raw in f:
                s = raw.strip()
                m = DESC_RE.match(s)
                d = DATASET_RE.match(s)
                if m:
                    current = desc
                    current.append(m.group(1).strip())
                elif d:
                    current = datasets.setdefault(d.group(1).strip(), [])
                    current.append(d.group(2).strip())
                elif current is not None and s.startswith("//"):
                    cont = s[2:].strip()
                    if not cont or cont.startswith("@"):
                        current = None
                    else:
                        current.append(cont)
                elif current is not None or desc or datasets:
                    if not s.startswith("//"):
                        break
    except OSError:
        return "", {}
    join = lambda lines: " ".join(x for x in lines if x).strip()
    return join(desc), {name: join(lines) for name, lines in datasets.items()}


def default_repo_base() -> str:
    """Derive `https://github.com/<owner>/<repo>/blob/main/` from origin, or ''."""
    try:
        url = subprocess.check_output(
            ["git", "remote", "get-url", "origin"],
            text=True, stderr=subprocess.DEVNULL).strip()
    except Exception:
        return ""
    m = re.search(r"github\.com[:/]+([^/]+)/(.+?)(?:\.git)?$", url)
    if not m:
        return ""
    return f"https://github.com/{m.group(1)}/{m.group(2)}/blob/main/"


def read_jsonl(path: str):
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                yield json.loads(line)
            except json.JSONDecodeError:
                continue


def build_pairs(history: str):
    """Return (pairs, dimensions, machines) for the shape-pair cube."""
    # machine -> key -> commit -> point  (last record for a commit wins)
    raw: dict[str, dict[str, dict[str, dict]]] = {}
    dims = {k: set() for k in ("shape1", "size1", "shape2", "size2", "method", "type")}
    machines: set[str] = set()

    for path in sorted(glob.glob(os.path.join(history, "*.jsonl"))):
        for r in read_jsonl(path):
            if r.get("kind") != "pair":
                continue
            machine = record_machine(r)
            machines.add(machine)
            for d in dims:
                dims[d].add(r[d])
            key = "|".join((r["shape1"], r["size1"], r["shape2"], r["size2"],
                            r["method"], r["type"]))
            series = raw.setdefault(machine, {}).setdefault(key, {})
            series[r["commit"]] = {
                "commit": r["commit"],
                "date":   r.get("date", ""),
                "time":   r["time"],
                "min":    r.get("time_min", r["time"]),
                "max":    r.get("time_max", r["time"]),
                "result": r.get("result"),
                "match":  r.get("match_truth", True),
            }

    pairs = {
        machine: {
            key: sorted(by_commit.values(), key=lambda p: p["date"])
            for key, by_commit in keys.items()
        }
        for machine, keys in raw.items()
    }
    orders = {"shape1": SHAPE_ORDER, "size1": SIZE_ORDER, "shape2": SHAPE_ORDER,
              "size2": SIZE_ORDER, "method": METHOD_ORDER, "type": TYPE_ORDER}
    dimensions = {d: sorted(vals, key=order_key(orders[d])) for d, vals in dims.items()}
    return pairs, dimensions, machines


def split_initial_pairs(pairs: dict):
    """Split the first-view pair cells from the deferred dashboard payload."""
    initial: dict = {}
    deferred: dict = {}
    for machine, cells in pairs.items():
        initial[machine] = {}
        deferred[machine] = {}
        for key, history in cells.items():
            _, size1, _, size2, _, number = key.split("|")
            is_initial = (
                size1 in (INITIAL_PAIRS["size1"], NO_SIZE)
                and size2 in (INITIAL_PAIRS["size2"], NO_SIZE)
                and number == INITIAL_PAIRS["type"]
            )
            (initial if is_initial else deferred)[machine][key] = history
    return initial, deferred


# Display order for the asymptotic page's own dimensions. Anything not listed
# sorts after, alphabetically — a new dataset or problem shows up without
# needing this table edited, just not in a hand-chosen position. Categories
# themselves have no table: the page lists them alphabetically, so a reader can
# find one by name.
DATASET_ORDER = ["random", "euro-night", "small segments", "small", "sheared", "large segments", "large",
                 "mixed", "voronoi", "polygon edges", "polygon", "large + large", "large + small",
                 "large + convex", "triangles", "fpg", "spg", "fpg-holes"]
PROBLEM_ORDER = ["build", "buildPointLocation", "locate", "locateFace",
                 "closest pair", "convex hull", "sort by angle", "Delaunay",
                 "kd-tree", "order 1", "order 2", "order 4", "farthest",
                 "intersections", "crossings",
                 "count in Rectangle", "count in Triangle", "nearest neighbor",
                 "visibility graph", "visible vertices",
                 "Minkowski sum", "union"]
# The public entry point of a category comes first, so it is the algorithm a
# problem opens on; the named algorithms it chooses between follow.
ALGORITHM_ORDER = ["findIntersections", "findCrossings"]

# A CGAL entry may be the independent reference for one specific pgl algorithm,
# rather than for every algorithm that solves the same problem.
BASELINE_FOR_ALGORITHM = {
    ("Triangulation", "locate", "CGAL::Delaunay_triangulation_2::locate"): "walk",
    ("Triangulation", "locate", "CGAL::Triangulation_hierarchy_2::locate"):
        "preprocessed",
}

# A (category, number) whose curve does not answer the question the pgl curves
# beside it answer, and must say so wherever it is drawn. The sweep's EPICK
# curve is the only one: rounding an intersection point that is re-inserted
# into the event structure loses points, so it is a timing reference for what
# CGAL's `int`-strength kernel costs, not a second opinion on the answer.
# Recorded rather than dropped because pgl's `int` sweep is exact and has no
# inexact curve of its own; see asymptotic/baseline/cgal.hpp.
BASELINE_INEXACT = {
    ("Segment intersections", "EPICK"),
}


def build_asymptotic(history: str, repo_base: str, bench_root: str):
    """Return (asymptotic, machines) for the size-swept benchmarks."""
    # category -> machine -> cell key -> commit -> run
    raw: dict[str, dict] = {}
    machines: set[str] = set()

    for path in sorted(glob.glob(os.path.join(history, "asymptotic", "*.jsonl"))):
        for r in read_jsonl(path):
            if r.get("kind") != "asymptotic":
                continue
            category = r["category"]
            machine = record_machine(r)
            machines.add(machine)
            entry = raw.setdefault(category, {
                "dims": {d: set() for d in ("dataset", "problem", "algorithm", "type")},
                "machines": set(), "unit": canonical_unit(r.get("unit", "")),
                "drivers": set(), "_data": {},
            })
            dataset = r["dataset"]
            for d in entry["dims"]:
                entry["dims"][d].add(dataset if d == "dataset" else r[d])
            entry["machines"].add(machine)
            entry["drivers"].add(r.get("driver", ""))
            key = "|".join((dataset, r["problem"], r["algorithm"], r["type"]))
            runs = entry["_data"].setdefault(machine, {}).setdefault(key, {})
            # One run per commit; a re-run of the same commit replaces it, and a
            # size measured twice within a run keeps the later reading.
            run = runs.setdefault(r["commit"], {
                "commit": r["commit"], "date": r.get("date", ""), "_points": {},
            })
            run["_points"][r["size"]] = {
                "size":   r["size"],
                "time":   r["time"],
                "min":    r.get("time_min", r["time"]),
                "max":    r.get("time_max", r["time"]),
                # The output size, and only that: `result` is the verification
                # signature, which is compared but never displayed. Records
                # written before the two were told apart carry one number that
                # served as both, so they fall back to it.
                "output": r.get("output", r.get("result")),
            }

    baselines = {machine: group_baseline(snapshot)
                 for machine, snapshot in read_baselines(history).items()}

    out: dict[str, dict] = {}
    for category, entry in raw.items():
        data = {
            machine: {
                key: [
                    {"commit": run["commit"], "date": run["date"],
                     "points": [run["_points"][s] for s in sorted(run["_points"])]}
                    for run in sorted(runs.values(), key=lambda x: (x["date"], x["commit"]))
                ]
                for key, runs in keys.items()
            }
            for machine, keys in entry["_data"].items()
        }
        orders = {"dataset": DATASET_ORDER, "problem": PROBLEM_ORDER,
                  "algorithm": ALGORITHM_ORDER, "type": TYPE_ORDER}
        result = {
            "dimensions": {d: sorted(vals, key=order_key(orders[d]))
                           for d, vals in entry["dims"].items()},
            "unit": entry["unit"],
            "machines": sorted(entry["machines"]),
            "data": data,
        }
        references = {machine: grouped[category]
                      for machine, grouped in sorted(baselines.items())
                      if category in grouped}
        if references:
            result["baseline"] = references
        # One driver per category, so the description, the dataset notes and the
        # source link come from that file's `// @desc:` and `// @dataset` blocks.
        for driver in sorted(d for d in entry["drivers"] if d):
            source = os.path.join(bench_root, "asymptotic", f"{driver}.cpp")
            if not os.path.exists(source):
                continue
            desc, datasets = parse_comment_blocks(source)
            if desc:
                result["description"] = desc
            if datasets:
                result["datasets"] = datasets
            if repo_base:
                result["source_url"] = repo_base + source.replace(os.sep, "/")
            break
        out[category] = result

    ordered = {c: out[c] for c in sorted(out, key=lambda c: (c.casefold(), c))}
    return ordered, machines


def group_baseline(snapshot: dict):
    """One machine's CGAL reference, category -> "dataset|problem" -> [curve].

    A list per key, not one curve, for two reasons. A category may have more
    than one reference for the same cell, and the chart draws all of them. And
    a reference measured under both of CGAL's kernels appears once per
    kernel, since the page shows EPICK against pgl's `int` column and EPECK
    against `ERational` (see baseline/cgal.hpp for which drivers may offer
    both). Curves are therefore keyed on (algorithm, number), not on algorithm
    alone, or the second kernel would overwrite the first. A curve listed in
    BASELINE_INEXACT is flagged `inexact`, which the page renders into its
    legend entry: it is a cost reference, not a second opinion on the answer.

    `rank` is the index of a curve's *algorithm* among the key's algorithms, and
    it is what the page picks a dash pattern from. Ranking the flat list instead
    would give one algorithm's two kernels two different patterns, which would
    read as two different algorithms.

    An overwritten JSON per machine rather than a history: see to_history.py.
    A machine without one is the normal case — it is only ever written on a
    machine with CGAL.
    """
    grouped: dict[str, dict] = {}
    for r in snapshot.get("results", []):
        key = "|".join((r["dataset"], r["problem"]))
        curves = grouped.setdefault(r["category"], {}).setdefault(key, {})
        for_algorithm = BASELINE_FOR_ALGORITHM.get(
            (r["category"], r["problem"], r["algorithm"]))
        curve = curves.setdefault((r["algorithm"], r["number"]), {
            "algorithm": r["algorithm"], "number": r["number"],
            "_points": {},
        })
        if for_algorithm:
            curve["for_algorithm"] = for_algorithm
        if (r["category"], r["number"]) in BASELINE_INEXACT:
            curve["inexact"] = True
        curve["_points"][r["size"]] = {
            "size": r["size"], "time": r["time"],
            "min": r.get("time_min", r["time"]), "max": r.get("time_max", r["time"]),
            "output": r.get("output", r.get("result")),
        }
    for keys in grouped.values():
        for key, curves in keys.items():
            ranks: dict[str, int] = {}
            listed = []
            for curve in curves.values():
                curve["rank"] = ranks.setdefault(curve["algorithm"], len(ranks))
                points = curve.pop("_points")
                curve["points"] = [points[s] for s in sorted(points)]
                listed.append(curve)
            keys[key] = listed
    return grouped


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--history", default=None,
                    help="history root (default: see bench_paths.py)")
    ap.add_argument("--dashboard", default="tests/benchmark/dashboard")
    ap.add_argument("--bench-root", default="tests/benchmark",
                    help="root holding asymptotic/<driver>.cpp sources, for source links")
    ap.add_argument("--repo-url", default="",
                    help="base URL for source links (default: derived from git origin)")
    ap.add_argument("--logo", default="doc/figures/logo.png")
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    history = Path(args.history) if args.history else default_history()
    if not history.is_dir():
        sys.exit(missing_history_message(history))

    repo_base = args.repo_url or default_repo_base()
    if repo_base and not repo_base.endswith("/"):
        repo_base += "/"

    pairs, dimensions, pair_machines = build_pairs(history)
    initial_pairs, deferred_pairs = split_initial_pairs(pairs)
    asymptotic, asym_machines = build_asymptotic(history, repo_base, args.bench_root)
    machines = sorted(pair_machines | asym_machines)

    generated = datetime.datetime.now(datetime.timezone.utc).isoformat()
    pairs_payload = {
        "generated": generated,
        "machines": sorted(pair_machines),
        "unit": "ns",
        "dimensions": dimensions,
        "pairs": initial_pairs,
        "asymptotic": {},
    }
    asymptotic_payload = {
        "generated": generated,
        "machines": sorted(asym_machines),
        "unit": "µs",
        "dimensions": {},
        "pairs": {},
        "asymptotic": asymptotic,
    }
    deferred_pairs_payload = {"pairs": deferred_pairs}

    os.makedirs(args.out, exist_ok=True)
    for fname in ("index.html", "asymptotic.html", "app.js", "style.css"):
        src = os.path.join(args.dashboard, fname)
        if os.path.exists(src):
            shutil.copy(src, os.path.join(args.out, fname))
    if os.path.exists(args.logo):
        shutil.copy(args.logo, os.path.join(args.out, "logo.png"))
    for fname, payload in (("pairs.json", pairs_payload),
                           ("pairs-deferred.json", deferred_pairs_payload),
                           ("asymptotic.json", asymptotic_payload)):
        with open(os.path.join(args.out, fname), "w", encoding="utf-8") as f:
            json.dump(payload, f, ensure_ascii=False, separators=(",", ":"))

    print(f"dashboard -> {args.out}  "
          f"({len(pairs)} machines of pairs, {len(asymptotic)} asymptotic categories, "
          f"{len(machines)} machines)")


if __name__ == "__main__":
    main()
