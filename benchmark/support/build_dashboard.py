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
import re
import shutil
import subprocess

TYPE_ORDER = ["int", "int128", "double", "rational", "rational60", "rationalbigint", "rationalbigint60"]

DESC_RE = re.compile(r"//\s*@desc:\s*(.*)")


def type_sort_key(t: str):
    return (TYPE_ORDER.index(t) if t in TYPE_ORDER else len(TYPE_ORDER), t)


def parse_desc(path: str) -> str:
    """Read the `// @desc:` comment block from a benchmark source.

    Captures the text after the tag plus any immediately following `//` comment
    lines (wrapping), stopping at a blank comment, a non-comment line, or another
    `@tag`.
    """
    lines: list[str] = []
    capturing = False
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
    return " ".join(x for x in lines if x).strip()


def benchmark_meta(benchmarks_dir: str) -> dict[str, dict]:
    """Map each suite basename to its source path and parsed description."""
    meta: dict[str, dict] = {}
    for path in sorted(glob.glob(os.path.join(benchmarks_dir, "*", "*.cpp"))):
        if os.path.basename(os.path.dirname(path)) == "support":
            continue
        suite = os.path.splitext(os.path.basename(path))[0]
        meta[suite] = {
            "source": path.replace(os.sep, "/"),
            "description": parse_desc(path),
        }
    return meta


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


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--history", default="benchmark/history")
    ap.add_argument("--dashboard", default="benchmark/dashboard")
    ap.add_argument("--benchmarks", default="benchmark",
                    help="root dir holding the benchmark <category>/<suite>.cpp sources")
    ap.add_argument("--repo-url", default="",
                    help="base URL for source links, e.g. "
                         "https://github.com/owner/repo/blob/<ref>/ "
                         "(default: derived from the git origin, pointing at main)")
    ap.add_argument("--logo", default="doc/figures/logo.png")
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    meta = benchmark_meta(args.benchmarks)
    repo_base = args.repo_url or default_repo_base()
    if repo_base and not repo_base.endswith("/"):
        repo_base += "/"

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
        m = meta.get(suite)
        if m:
            if m["description"]:
                out_suites[suite]["description"] = m["description"]
            if repo_base and m["source"]:
                out_suites[suite]["source_url"] = repo_base + m["source"]

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
