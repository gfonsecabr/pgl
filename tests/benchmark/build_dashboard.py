#!/usr/bin/env python3
"""build_dashboard.py — assemble the static benchmark dashboard from history.

Reads the JSONL history under --history (pair records at the top level, extra
whole-algorithm records under history/extra/), copies the dashboard template
(index.html / app.js / style.css) into --out, and emits data.json — the single
payload the frontend fetches. Pure transformation, no network, so it runs
identically locally and in CI.

data.json shape:
  {
    "generated": <iso>,
    "machines":  [<machine>, ...],
    "dimensions": {shape1:[...], size1:[...], shape2:[...], size2:[...],
                   method:[...], type:[...]},   # each value list in display order
    "pairs": { <machine>: { "s1|sz1|s2|sz2|method|type":
                            [ {commit,date,time,result,match}, ... ] } },
    "extra": { <suite>: { functions:[...], types:[...], unit, machines:[...],
                          data:{ <machine>: { "op|type": [ {commit,date,time,result} ] } },
                          source_url?, description? } }
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
               "Rectangle", "Triangle", "Disk", "Convex"]
SIZE_ORDER = ["small", "large", "n/a"]
METHOD_ORDER = ["contains", "interiorContains", "boundaryContains",
                "intersects", "interiorsIntersect", "separates", "crosses",
                "collinear", "parallel", "intersection", "squaredDistance"]
TYPE_ORDER = ["int", "int128", "double", "BigInt", "Rational",
              "RationalBigInt", "ERational"]

DESC_RE = re.compile(r"//\s*@desc:\s*(.*)")


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


def build_extra(history: str, repo_base: str, bench_root: str):
    """Return (extra, machines) for the whole-algorithm benchmarks."""
    suites: dict[str, dict] = {}
    machines: set[str] = set()
    func_order: dict[str, dict[str, int]] = {}

    for path in sorted(glob.glob(os.path.join(history, "extra", "*.jsonl"))):
        for r in read_jsonl(path):
            if r.get("kind") != "extra":
                continue
            suite, op, typ = r["suite"], r["op"], r["type"]
            machine = r["machine"]
            machines.add(machine)
            s = suites.setdefault(suite, {
                "functions": [], "types": set(), "machines": set(),
                "unit": r.get("unit", "ns"), "_data": {},
            })
            order = func_order.setdefault(suite, {})
            if op not in order:
                order[op] = len(order)
                s["functions"].append(op)
            s["types"].add(typ)
            s["machines"].add(machine)
            series = s["_data"].setdefault(machine, {}).setdefault(op + "|" + typ, {})
            series[r["commit"]] = {
                "commit": r["commit"], "date": r.get("date", ""),
                "time": r["time"], "min": r.get("time_min", r["time"]),
                "max": r.get("time_max", r["time"]), "result": r.get("result"),
            }

    out: dict[str, dict] = {}
    for suite, s in suites.items():
        data = {
            machine: {
                key: sorted(by_commit.values(), key=lambda p: p["date"])
                for key, by_commit in keys.items()
            }
            for machine, keys in s["_data"].items()
        }
        entry = {
            "functions": s["functions"],
            "types": sorted(s["types"], key=order_key(TYPE_ORDER)),
            "machines": sorted(s["machines"]),
            "unit": s["unit"],
            "data": data,
        }
        source = os.path.join(bench_root, "extra", f"{suite}.cpp")
        if os.path.exists(source):
            desc = parse_desc(source)
            if desc:
                entry["description"] = desc
            if repo_base:
                entry["source_url"] = repo_base + source.replace(os.sep, "/")
        out[suite] = entry
    return out, machines


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--history", default="tests/benchmark/history")
    ap.add_argument("--dashboard", default="tests/benchmark/dashboard")
    ap.add_argument("--bench-root", default="tests/benchmark",
                    help="root holding extra/<suite>.cpp sources, for source links")
    ap.add_argument("--repo-url", default="",
                    help="base URL for source links (default: derived from git origin)")
    ap.add_argument("--logo", default="doc/figures/logo.png")
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    repo_base = args.repo_url or default_repo_base()
    if repo_base and not repo_base.endswith("/"):
        repo_base += "/"

    pairs, dimensions, pair_machines = build_pairs(args.history)
    extra, extra_machines = build_extra(args.history, repo_base, args.bench_root)
    machines = sorted(pair_machines | extra_machines)

    payload = {
        "generated": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "machines": machines,
        "unit": "ns",
        "dimensions": dimensions,
        "pairs": pairs,
        "extra": extra,
    }

    os.makedirs(args.out, exist_ok=True)
    for fname in ("index.html", "app.js", "style.css"):
        src = os.path.join(args.dashboard, fname)
        if os.path.exists(src):
            shutil.copy(src, os.path.join(args.out, fname))
    if os.path.exists(args.logo):
        shutil.copy(args.logo, os.path.join(args.out, "logo.png"))
    with open(os.path.join(args.out, "data.json"), "w", encoding="utf-8") as f:
        json.dump(payload, f, ensure_ascii=False, separators=(",", ":"))

    print(f"dashboard -> {args.out}  "
          f"({len(pairs)} machines of pairs, {len(extra)} extra suites, "
          f"{len(machines)} machines)")


if __name__ == "__main__":
    main()
