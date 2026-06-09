#!/usr/bin/env python3
"""Assemble the static benchmark dashboard under <out>/ from the JSONL history.

Copies the dashboard template (index.html / app.js / style.css) and emits
data.json, the single payload the frontend fetches. Pure transformation — no
network, so it runs identically locally and in CI.
"""
from __future__ import annotations

import argparse
import datetime
import glob
import json
import os
import shutil

TYPE_ORDER = ["int", "int128", "double", "rational", "rational60", "rationalbigint", "rationalbigint60"]


def type_sort_key(t: str):
    return (TYPE_ORDER.index(t) if t in TYPE_ORDER else len(TYPE_ORDER), t)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--history", default="benchmark/history")
    ap.add_argument("--dashboard", default="benchmark/dashboard")
    ap.add_argument("--logo", default="doc/figures/logo.png")
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    suites: dict[str, dict] = {}
    machines: set[str] = set()
    func_order: dict[str, dict[str, int]] = {}

    for path in sorted(glob.glob(os.path.join(args.history, "*.jsonl"))):
        with open(path, encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue
                try:
                    r = json.loads(line)
                except json.JSONDecodeError:
                    continue

                suite, op, typ = r["suite"], r["op"], r["type"]
                machine = r["machine"]
                machines.add(machine)

                s = suites.setdefault(suite, {
                    "functions": [],
                    "types": set(),
                    "machines": set(),
                    "unit": r.get("unit", "ns"),
                    "_data": {},  # machine -> "op|type" -> {commit: point}
                })
                order = func_order.setdefault(suite, {})
                if op not in order:
                    order[op] = len(order)
                    s["functions"].append(op)
                s["types"].add(typ)
                s["machines"].add(machine)

                series = s["_data"].setdefault(machine, {}).setdefault(op + "|" + typ, {})
                series[r["commit"]] = {  # last record for a commit wins
                    "commit": r["commit"],
                    "date": r.get("date", ""),
                    "time": r["time"],
                    "result": r.get("result"),
                }

    out_suites: dict[str, dict] = {}
    for suite, s in suites.items():
        data: dict[str, dict] = {}
        for machine, keys in s["_data"].items():
            data[machine] = {
                key: sorted(by_commit.values(), key=lambda p: p["date"])
                for key, by_commit in keys.items()
            }
        out_suites[suite] = {
            "functions": s["functions"],
            "types": sorted(s["types"], key=type_sort_key),
            "machines": sorted(s["machines"]),
            "unit": s["unit"],
            "data": data,
        }

    payload = {
        "generated": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "machines": sorted(machines),
        "suites": out_suites,
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

    print(f"dashboard -> {args.out}  ({len(out_suites)} suites, {len(machines)} machines)")


if __name__ == "__main__":
    main()
