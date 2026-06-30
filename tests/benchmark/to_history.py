#!/usr/bin/env python3
"""to_history.py — append benchmark snapshots into the versioned history.

Reads the two snapshot JSONs produced by the runners:

  * the shape-pair cube from run_shapepairs.py  (default build/.../benchmarks.json)
  * the whole-algorithm "extra" benchmarks from run_extra.py (default .../extra.json)

and appends one record per data point into tests/benchmark/history/, tagged with
the commit (and its commit date — the dashboard's x-axis is the commit date, not
the run date) and the machine (CPU + compiler).

Layout, append-only and committed to the repo:
  * pairs → history/<shape1>_<shape2>.jsonl   (kind:"pair")
  * extra → history/extra/<suite>.jsonl        (kind:"extra")

Usage (from repo root):
    python3 tests/benchmark/to_history.py [options]

Options:
    --pairs FILE     pair snapshot   (default: build/tests/benchmark/benchmarks.json)
    --extra FILE     extra snapshot  (default: build/tests/benchmark/extra/extra.json)
    --history DIR    history root    (default: tests/benchmark/history)
    --skip-pairs     do not read the pair snapshot
    --skip-extra     do not read the extra snapshot
"""
from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path

# Canonical number-type keys, matching run_shapepairs.py's ALL_NUMBER_TYPES keys
# so the dashboard's "type" dimension is shared between pair and extra records.
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
    if not commit or commit == "unknown":
        return ""
    try:
        return subprocess.check_output(
            ["git", "show", "-s", "--format=%cI", commit],
            text=True, stderr=subprocess.DEVNULL).strip()
    except Exception:
        return ""


def main() -> int:
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    ap.add_argument("--pairs", default="build/tests/benchmark/benchmarks.json")
    ap.add_argument("--extra", default="build/tests/benchmark/extra/extra.json")
    ap.add_argument("--history", default="tests/benchmark/history")
    ap.add_argument("--skip-pairs", action="store_true")
    ap.add_argument("--skip-extra", action="store_true")
    args = ap.parse_args()

    history_dir = Path(args.history)
    (history_dir / "extra").mkdir(parents=True, exist_ok=True)

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
                        "unit":       "ns",
                        "result":     str(td.get("result")),
                        "match_truth": bool(td.get("match_erational")),
                        "commit":     commit,
                        "date":       date,
                        "machine":    machine,
                        "cpu":        cpu, "cxx": cxx, "flags": flags,
                    })
                    total += 1

    # ── extra ────────────────────────────────────────────────────────────────
    if not args.skip_extra:
        extra_path = Path(args.extra)
        if not extra_path.exists():
            print(f"extra snapshot not found: {extra_path}", file=sys.stderr)
        else:
            data = json.loads(extra_path.read_text())
            meta = data.get("meta", {})
            commit = meta.get("commit", "unknown")
            cpu    = meta.get("cpu") or "unknown"
            cxx    = meta.get("compiler", "unknown")
            flags  = meta.get("cxxflags", "")
            machine = f"{cpu} · {cxx}"
            date = date_of(commit)
            for entry in data.get("results", []):
                suite = entry["suite"]
                fname = f"extra/{suite}.jsonl"
                buckets.setdefault(fname, []).append({
                    "kind":       "extra",
                    "suite":      suite,
                    "op":         entry["op"],
                    "type":       canon_type(entry["number"]),
                    "type_label": entry["number"],
                    "time":       entry["time"],
                    "unit":       entry.get("unit", "ns"),
                    "result":     str(entry.get("result")),
                    "commit":     commit,
                    "date":       date,
                    "machine":    machine,
                    "cpu":        cpu, "cxx": cxx, "flags": flags,
                })
                total += 1

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
