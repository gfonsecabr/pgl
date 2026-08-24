#!/usr/bin/env python3
"""build_dashboard.py — assemble the static benchmark dashboard from history.

Reads the JSONL history under --history (pair records at the top level,
asymptotic records under history/asymptotic/, plus the single overwritten CGAL
baseline snapshot), copies the dashboard template (index.html / asymptotic.html
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
                    [ {commit, date, points:[{size,time,min,max,result}, ...]}, ... ] } },
        # The CGAL reference, if one was recorded. Keyed on dataset|problem
        # only: CGAL is not one of the cube's selectable values, so its curve is
        # drawn whatever algorithm and number type happen to be selected.
        "baseline": { "dataset|problem": {algorithm, number, points:[...]} },
        "source_url"?, "description"?
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


# The microsecond symbol. Runs recorded before it was spelled properly carry
# "us" in the history, and the page should not show two spellings of one unit.
MICROSECONDS = "\u00b5s"


def canonical_unit(unit: str) -> str:
    return MICROSECONDS if unit in ("", "us", "\u00b5s", "\u03bcs") else unit


def order_key(order: list[str]):
    return lambda v: (order.index(v) if v in order else len(order), v)


def parse_desc(path: str) -> str:
    """Read the `// @desc:` comment block from a benchmark source."""
    lines: list[str] = []
    capturing = False
    try:
        with open(path, encoding="utf-8") as f:
            for raw in f:
                s = raw.strip()
                if not capturing:
                    m = DESC_RE.match(s)
                    if m:
                        lines.append(m.group(1).strip())
                        capturing = True
                    continue
                if s.startswith("//"):
                    cont = s[2:].strip()
                    if not cont or cont.startswith("@"):
                        break
                    lines.append(cont)
                else:
                    break
    except OSError:
        return ""
    return " ".join(x for x in lines if x).strip()


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
            machine = r["machine"]
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


# Display order for the asymptotic page's own dimensions. Anything not listed
# sorts after, alphabetically — a new dataset or problem shows up without
# needing this table edited, just not in a hand-chosen position.
CATEGORY_ORDER = ["Triangulation", "Arrangement", "Segment intersections",
                  "Point constructions", "Point search", "Segment search",
                  "Visibility", "Minkowski sum", "Regularized union"]
DATASET_ORDER = ["points", "small segments", "large segments", "polygon edges",
                 "polygon", "large + large", "large + small", "triangles"]
PROBLEM_ORDER = ["build", "buildPointLocation", "locate", "locateFace",
                 "closest pair", "convex hull", "sort by angle", "Delaunay",
                 "kd-tree", "intersections", "crossings",
                 "count in Rectangle", "count in Triangle", "nearest neighbor",
                 "visibility graph", "visible vertices",
                 "Minkowski sum", "union"]


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
            machine = r["machine"]
            machines.add(machine)
            entry = raw.setdefault(category, {
                "dims": {d: set() for d in ("dataset", "problem", "algorithm", "type")},
                "machines": set(), "unit": canonical_unit(r.get("unit", "")),
                "drivers": set(), "_data": {},
            })
            for d in entry["dims"]:
                entry["dims"][d].add(r[d])
            entry["machines"].add(machine)
            entry["drivers"].add(r.get("driver", ""))
            key = "|".join((r["dataset"], r["problem"], r["algorithm"], r["type"]))
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
                "result": r.get("result"),
            }

    baseline = read_baseline(history)

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
                  "algorithm": [], "type": TYPE_ORDER}
        result = {
            "dimensions": {d: sorted(vals, key=order_key(orders[d]))
                           for d, vals in entry["dims"].items()},
            "unit": entry["unit"],
            "machines": sorted(entry["machines"]),
            "data": data,
        }
        if category in baseline:
            result["baseline"] = baseline[category]
        # One driver per category, so the description and the source link come
        # from that file's `// @desc:` block.
        for driver in sorted(d for d in entry["drivers"] if d):
            source = os.path.join(bench_root, "asymptotic", f"{driver}.cpp")
            if not os.path.exists(source):
                continue
            desc = parse_desc(source)
            if desc:
                result["description"] = desc
            if repo_base:
                result["source_url"] = repo_base + source.replace(os.sep, "/")
            break
        out[category] = result

    ordered = {c: out[c] for c in sorted(out, key=order_key(CATEGORY_ORDER))}
    return ordered, machines


def read_baseline(history: str):
    """The CGAL reference snapshot, category -> "dataset|problem" -> [curve].

    A list per key, not one curve: a category may have more than one reference
    for the same cell — CGAL's Minkowski sum is measured both by decomposition
    and by reduced convolution — and the chart draws all of them.

    A single overwritten JSON rather than a history: see to_history.py. Missing
    is the normal case — it is only ever written on a machine with CGAL.
    """
    path = os.path.join(history, "asymptotic-baseline.json")
    if not os.path.exists(path):
        return {}
    try:
        with open(path, encoding="utf-8") as f:
            snapshot = json.load(f)
    except (OSError, json.JSONDecodeError):
        return {}

    grouped: dict[str, dict] = {}
    for r in snapshot.get("results", []):
        key = "|".join((r["dataset"], r["problem"]))
        curves = grouped.setdefault(r["category"], {}).setdefault(key, {})
        curve = curves.setdefault(r["algorithm"], {
            "algorithm": r["algorithm"], "number": r["number"], "_points": {},
        })
        curve["_points"][r["size"]] = {
            "size": r["size"], "time": r["time"],
            "min": r.get("time_min", r["time"]), "max": r.get("time_max", r["time"]),
            "result": r.get("result"),
        }
    for keys in grouped.values():
        for key, curves in keys.items():
            listed = []
            for curve in curves.values():
                points = curve.pop("_points")
                curve["points"] = [points[s] for s in sorted(points)]
                listed.append(curve)
            keys[key] = listed
    return grouped


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--history", default="tests/benchmark/history")
    ap.add_argument("--dashboard", default="tests/benchmark/dashboard")
    ap.add_argument("--bench-root", default="tests/benchmark",
                    help="root holding asymptotic/<driver>.cpp sources, for source links")
    ap.add_argument("--repo-url", default="",
                    help="base URL for source links (default: derived from git origin)")
    ap.add_argument("--logo", default="doc/figures/logo.png")
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    repo_base = args.repo_url or default_repo_base()
    if repo_base and not repo_base.endswith("/"):
        repo_base += "/"

    pairs, dimensions, pair_machines = build_pairs(args.history)
    asymptotic, asym_machines = build_asymptotic(args.history, repo_base, args.bench_root)
    machines = sorted(pair_machines | asym_machines)

    generated = datetime.datetime.now(datetime.timezone.utc).isoformat()
    pairs_payload = {
        "generated": generated,
        "machines": sorted(pair_machines),
        "unit": "ns",
        "dimensions": dimensions,
        "pairs": pairs,
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

    os.makedirs(args.out, exist_ok=True)
    for fname in ("index.html", "asymptotic.html", "app.js", "style.css"):
        src = os.path.join(args.dashboard, fname)
        if os.path.exists(src):
            shutil.copy(src, os.path.join(args.out, fname))
    if os.path.exists(args.logo):
        shutil.copy(args.logo, os.path.join(args.out, "logo.png"))
    for fname, payload in (("pairs.json", pairs_payload),
                           ("asymptotic.json", asymptotic_payload)):
        with open(os.path.join(args.out, fname), "w", encoding="utf-8") as f:
            json.dump(payload, f, ensure_ascii=False, separators=(",", ":"))

    print(f"dashboard -> {args.out}  "
          f"({len(pairs)} machines of pairs, {len(asymptotic)} asymptotic categories, "
          f"{len(machines)} machines)")


if __name__ == "__main__":
    main()
