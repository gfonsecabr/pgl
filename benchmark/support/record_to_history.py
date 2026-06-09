#!/usr/bin/env python3
"""Append a benchmark run's reports into the versioned history (JSONL).

Reuses benchmark_to_json's report parser, then writes one aggregated record per
(suite, operation, type) — the median across repetitions — tagged with the
commit (and its date) and the machine (CPU + compiler). One file per suite:
benchmark/history/<suite>.jsonl, append-only and committed to the repo.
"""
from __future__ import annotations

import argparse
import json
import os
import re
import statistics
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import benchmark_to_json as bj  # noqa: E402  (local helper, same directory)


def canon_type(label: str) -> str:
    """Map a printed Number label (e.g. 'Rational i64') to a stable key."""
    s = label.strip().lower()
    if "rational" in s and "bigint" in s and "60" in s:
        return "rationalbigint60"
    if "rational" in s and "bigint" in s:
        return "rationalbigint"
    if "rational" in s and "60" in s:
        return "rational60"
    if "rational" in s:
        return "rational"
    if "128" in s:
        return "int128"
    if "double" in s:
        return "double"
    if s == "int":
        return "int"
    return "".join(c for c in s if c.isalnum())


def units_by_suite(report_dir: Path) -> dict[str, str]:
    """Read each report's 'Time(<unit>)' header so charts label the y-axis."""
    units: dict[str, str] = {}
    for path in report_dir.glob("*.txt"):
        suite, unit = None, "ns"
        for line in path.read_text(encoding="utf-8").splitlines():
            if line.startswith("# suite:"):
                suite = line.split(":", 1)[1].strip()
            match = re.search(r"Time\(([^)]+)\)", line)
            if match:
                unit = match.group(1)
        if suite:
            units[suite] = unit
    return units


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
    ap = argparse.ArgumentParser()
    ap.add_argument("--reports", default="build/benchmark/current-reports")
    ap.add_argument("--history", default="benchmark/history")
    args = ap.parse_args()

    report_dir = Path(args.reports)
    measurements = []
    for path in sorted(report_dir.glob("*.txt")):
        measurements.extend(bj.read_report(path))
    if not measurements:
        print(f"no measurements found in {report_dir}", file=sys.stderr)
        return 1

    units = units_by_suite(report_dir)

    # Group repetitions by (suite, operation, number label) and take the median.
    grouped: dict[tuple[str, str, str], list[bj.Measurement]] = {}
    for m in measurements:
        grouped.setdefault((m.suite, m.operation, m.number), []).append(m)

    history_dir = Path(args.history)
    history_dir.mkdir(parents=True, exist_ok=True)

    by_suite: dict[str, list[dict]] = {}
    for (suite, op, number), samples in grouped.items():
        first = samples[0]
        results = sorted({s.result for s in samples})
        by_suite.setdefault(suite, []).append({
            "suite": suite,
            "op": op,
            "type": canon_type(number),
            "type_label": number,
            "time": round(statistics.median(s.value for s in samples), 6),
            "unit": units.get(suite, "ns"),
            "result": results[0] if len(results) == 1 else ",".join(results),
            "commit": first.commit,
            "date": commit_date(first.commit),
            "machine": f"{first.cpu} · {first.compiler}",
            "cpu": first.cpu,
            "cxx": first.compiler,
            "flags": first.cxxflags,
        })

    total = 0
    for suite, records in sorted(by_suite.items()):
        records.sort(key=lambda r: (r["op"], r["type"]))
        with (history_dir / f"{suite}.jsonl").open("a", encoding="utf-8") as f:
            for record in records:
                f.write(json.dumps(record, ensure_ascii=False) + "\n")
                total += 1
        print(f"  {suite}: +{len(records)} records")
    print(f"appended {total} records to {history_dir}/")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
