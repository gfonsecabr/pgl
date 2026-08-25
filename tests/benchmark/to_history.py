#!/usr/bin/env python3
"""to_history.py — append benchmark snapshots into the versioned history.

Reads the snapshot JSONs produced by the runners:

  * the shape-pair cube from run_shapepairs.py (default build/.../benchmarks.json)
  * the asymptotic benchmarks from run_asymptotic.py (default .../asymptotic.json)
  * optionally the CGAL baseline, also from run_asymptotic.py

and appends one record per data point into tests/benchmark/history/, tagged with
the commit (and its commit date — the pairs dashboard's x-axis is the commit
date, not the run date) and the machine (CPU + compiler).

Layout, append-only and committed to the repo:
  * pairs      → history/<shape1>_<shape2>.jsonl   (kind:"pair")
  * asymptotic → history/asymptotic/<driver>.jsonl (kind:"asymptotic")

The CGAL baseline is the exception: it is *not* appended. It goes to a single
history/asymptotic-baseline.json, overwritten each time. A baseline is a
reference point, not a measurement of this repo at this commit, so there is
nothing to track over time; and it only exists at all on a machine that has
CGAL, so an append-only baseline would be a ragged log of whoever last ran one.

Usage (from repo root):
    python3 tests/benchmark/to_history.py [options]

Options:
    --pairs FILE         pair snapshot       (default: build/tests/benchmark/benchmarks.json)
    --asymptotic FILE    asymptotic snapshot (default: build/tests/benchmark/asymptotic/asymptotic.json)
    --baseline FILE      CGAL baseline snapshot; only read when it exists
                         (default: build/tests/benchmark/asymptotic/baseline.json)
    --history DIR        history root        (default: tests/benchmark/history)
    --skip-pairs         do not read the pair snapshot
    --skip-asymptotic    do not read the asymptotic snapshot
    --merge-baseline     replace only the categories present in the baseline
                         snapshot, preserving the other stored categories
"""
from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from bench_paths import default_history  # noqa: E402

# The microsecond symbol, and the "us" older runs spelled it with.
MICROSECONDS = "\u00b5s"


def canonical_unit(unit: str) -> str:
    return MICROSECONDS if unit in ("", "us", "\u00b5s", "\u03bcs") else unit


# Canonical number-type keys, matching run_shapepairs.py's ALL_NUMBER_TYPES keys
# so the dashboard's "type" dimension means the same thing on both pages.
def canon_type(label: str) -> str:
    s = label.strip().lower()
    if "erational" in s:
        return "ERational"
    if "rational" in s and "bigint" in s:
        return "RationalBigInt"
    if "rational" in s:
        return "Rational"
    if "bigint" in s:
        return "BigInt"
    if "double" in s:
        return "double"
    if "128" in s:
        return "int128"
    if s == "int":
        return "int"
    return label.strip()


def commit_date(commit: str) -> str:
    """The commit date in UTC, strict ISO 8601.

    In UTC rather than the committer's own zone so that every record carries the
    same offset: the dashboard orders a series by comparing these as plain
    strings, which is chronological only while the offset is shared. `%cI` would
    give local time, so ask for `%cd` — the one that honours --date — with the
    -local suffix, and pin that "local" to UTC through the environment.
    """
    if not commit or commit == "unknown":
        return ""
    try:
        return subprocess.check_output(
            ["git", "show", "-s", "--format=%cd", "--date=iso-strict-local", commit],
            text=True, stderr=subprocess.DEVNULL,
            env={**os.environ, "TZ": "UTC"}).strip()
    except Exception:
        return ""


def main() -> int:
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    ap.add_argument("--pairs", default="build/tests/benchmark/benchmarks.json")
    ap.add_argument("--asymptotic",
                    default="build/tests/benchmark/asymptotic/asymptotic.json")
    ap.add_argument("--baseline",
                    default="build/tests/benchmark/asymptotic/baseline.json")
    ap.add_argument("--history", default=None,
                    help="history root (default: see bench_paths.py)")
    ap.add_argument("--skip-pairs", action="store_true")
    ap.add_argument("--skip-asymptotic", action="store_true")
    ap.add_argument("--merge-baseline", action="store_true")
    args = ap.parse_args()

    history_dir = Path(args.history) if args.history else default_history()
    (history_dir / "asymptotic").mkdir(parents=True, exist_ok=True)

    # buckets: relative-path -> list[record]
    buckets: dict[str, list[dict]] = {}
    date_cache: dict[str, str] = {}

    def date_of(commit: str) -> str:
        if commit not in date_cache:
            date_cache[commit] = commit_date(commit)
        return date_cache[commit]

    total = 0

    # ── pairs ────────────────────────────────────────────────────────────────
    if not args.skip_pairs:
        pairs_path = Path(args.pairs)
        if not pairs_path.exists():
            print(f"pair snapshot not found: {pairs_path}", file=sys.stderr)
        else:
            data = json.loads(pairs_path.read_text())
            meta = data.get("meta", {})
            commit = meta.get("commit", "unknown")
            cpu    = meta.get("cpu") or "unknown"
            cxx    = meta.get("compiler", "unknown")
            flags  = meta.get("cxxflags", "")
            machine = f"{cpu} · {cxx}"
            date = date_of(commit)
            for entry in data.get("results", []):
                s1, sz1 = entry["shape1"], entry["size1"]
                s2, sz2 = entry["shape2"], entry["size2"]
                method  = entry["method"]
                fname = f"{s1.lower()}_{s2.lower()}.jsonl"
                for type_key, td in entry.get("types", {}).items():
                    if td.get("status") != "ok":
                        continue
                    buckets.setdefault(fname, []).append({
                        "kind":       "pair",
                        "shape1":     s1, "size1": sz1,
                        "shape2":     s2, "size2": sz2,
                        "method":     method,
                        "type":       type_key,
                        "type_label": type_key,
                        "time":       td["time_ns"],
                        "time_min":   td.get("time_min_ns", td["time_ns"]),
                        "time_max":   td.get("time_max_ns", td["time_ns"]),
                        "unit":       "ns",
                        "result":     str(td.get("result")),
                        # Tri-state: True agrees with the exact baseline, False
                        # disagrees, None means the two runs stopped on the time
                        # budget at different points and summed their aggregates
                        # over different pairs, so there is nothing to compare.
                        "match_truth": (None if td.get("match_erational") is None
                                        else bool(td["match_erational"])),
                        "calls":      td.get("calls") or None,
                        "truncated":  bool(td.get("truncated")),
                        "commit":     commit,
                        "date":       date,
                        "machine":    machine,
                        "cpu":        cpu, "cxx": cxx, "flags": flags,
                    })
                    total += 1

    # ── asymptotic ───────────────────────────────────────────────────────────
    if not args.skip_asymptotic:
        path = Path(args.asymptotic)
        if not path.exists():
            print(f"asymptotic snapshot not found: {path}", file=sys.stderr)
        else:
            data = json.loads(path.read_text())
            meta = data.get("meta", {})
            if meta.get("sizes_override"):
                # A run whose sizes came from the command line is a calibration
                # run: its points sit at x values no other run measured, so
                # recording them would put stray dots on every chart.
                print("asymptotic snapshot used --sizes; not recording it.",
                      file=sys.stderr)
            else:
                commit = meta.get("commit", "unknown")
                cpu    = meta.get("cpu") or "unknown"
                cxx    = meta.get("compiler", "unknown")
                flags  = meta.get("cxxflags", "")
                machine = f"{cpu} · {cxx}"
                date = date_of(commit)
                for entry in data.get("results", []):
                    fname = f"asymptotic/{entry['driver']}.jsonl"
                    buckets.setdefault(fname, []).append({
                        "kind":       "asymptotic",
                        "driver":     entry["driver"],
                        "category":   entry["category"],
                        "dataset":    entry["dataset"],
                        "problem":    entry["problem"],
                        "algorithm":  entry["algorithm"],
                        "type":       canon_type(entry["number"]),
                        "type_label": entry["number"],
                        "size":       entry["size"],
                        "time":       entry["time"],
                        "time_min":   entry.get("time_min", entry["time"]),
                        "time_max":   entry.get("time_max", entry["time"]),
                        "unit":       canonical_unit(entry.get("unit", "")),
                        "result":     str(entry.get("result")),
                        "commit":     commit,
                        "date":       date,
                        "machine":    machine,
                        "cpu":        cpu, "cxx": cxx, "flags": flags,
                    })
                    total += 1

    # ── CGAL baseline: overwritten, never appended ───────────────────────────
    baseline_path = Path(args.baseline)
    if baseline_path.exists():
        data = json.loads(baseline_path.read_text())
        if data.get("meta", {}).get("sizes_override"):
            print("baseline snapshot used --sizes; not recording it.", file=sys.stderr)
        else:
            target = history_dir / "asymptotic-baseline.json"
            if args.merge_baseline and target.exists():
                try:
                    existing = json.loads(target.read_text())
                    categories = {r["category"] for r in data.get("results", [])}
                    preserved = [r for r in existing.get("results", [])
                                 if r.get("category") not in categories]
                    data = {**data, "results": preserved + data.get("results", [])}
                except (OSError, json.JSONDecodeError):
                    pass
            target.write_text(json.dumps(data, ensure_ascii=False, indent=2))
            print(f"  asymptotic-baseline.json: {len(data.get('results', []))} rows "
                  f"({'merged' if args.merge_baseline else 'overwritten'})")

    if not buckets:
        print("no records to append.", file=sys.stderr)
        return 1

    for fname, records in sorted(buckets.items()):
        path = history_dir / fname
        path.parent.mkdir(parents=True, exist_ok=True)
        with path.open("a", encoding="utf-8") as f:
            for record in records:
                f.write(json.dumps(record, ensure_ascii=False) + "\n")
        print(f"  {fname}: +{len(records)} records")

    print(f"appended {total} records to {history_dir}/")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
