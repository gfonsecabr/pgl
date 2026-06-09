#!/usr/bin/env python3
"""Compile, run and format the number-type benchmark tables for doc/types.md.

Two Markdown tables in doc/types.md are produced from two programs:

  * the "Benchmark" table        <- benchmark/support/doctable.cpp
        run twice: with type promotion (the default) and with promotion
        disabled (-DPGL_DISABLE_PROMOTION). A single run already reports both
        the integer rows and the "/ 60" rational rows.

  * the "Boost Number Types" table <- benchmark/support/boosttable.cpp
        run once (it always disables promotion). The integer rows come first,
        then the "/ 60" rows after the "Dividing coordinates by 60" marker.

This script compiles each program with the same flags the docs use
(-Ofast -std=c++23, g++ by default), runs them, and prints the two tables to
stdout ready to copy and paste. A short banner for each table is written to
stderr, so `... > tables.md` captures only clean Markdown.

Usage:
    python3 benchmark/support/make_type_tables.py            # both tables
    python3 benchmark/support/make_type_tables.py --doc      # first table only
    python3 benchmark/support/make_type_tables.py --boost    # second table only
    python3 benchmark/support/make_type_tables.py --runs 3   # best of 3 runs
    CXX=clang++ python3 benchmark/support/make_type_tables.py # different compiler

Notes:
    * Values are nanoseconds per crosses() call (lower is better).
    * The Boost table is slow: boost::cpp_int rationals can take several minutes
      per run. Use --boost on its own if you only need the first table.
"""

import argparse
import os
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]          # benchmark/support/ -> repo root
SUPPORT = ROOT / "benchmark" / "support"
CXX = os.environ.get("CXX", "g++")
BASE_FLAGS = ["-Ofast", "-std=c++23", f"-I{ROOT / 'include'}"]


def run_program(src: Path, extra_flags=(), runs: int = 1):
    """Compile `src`, run it `runs` times, and return one list of (label, ns)
    records keeping the fastest time seen for each row (positions are stable
    across runs, so this also works when labels repeat between sections)."""
    with tempfile.TemporaryDirectory() as tmp:
        exe = Path(tmp) / "bench"
        cmd = [CXX, *BASE_FLAGS, *extra_flags, str(src), "-o", str(exe)]
        print(f"  compiling {src.name} {' '.join(extra_flags)}".rstrip(),
              file=sys.stderr)
        subprocess.run(cmd, check=True)

        best = None       # list of [label, ns] across runs (min ns per position)
        divide_at = None
        for r in range(runs):
            print(f"  running {src.name} ({r + 1}/{runs})", file=sys.stderr)
            out = subprocess.run([str(exe)], check=True,
                                 capture_output=True, text=True).stdout
            records, divide_at = parse(out)
            if best is None:
                best = [[lbl, ns] for lbl, ns in records]
            else:
                for i, (_, ns) in enumerate(records):
                    best[i][1] = min(best[i][1], ns)
    return best, divide_at


def parse(output: str):
    """Parse program stdout into ((label, ns), ...) records and the record index
    at which the 'Dividing coordinates by 60' marker appeared (or None)."""
    records = []
    divide_at = None
    for line in output.splitlines():
        if "Dividing coordinates by 60" in line:
            divide_at = len(records)
            continue
        fields = [f.strip() for f in line.split("\t") if f.strip()]
        if len(fields) < 2 or fields[0] != "crosses":
            continue                       # header / "Promotion disabled!" / blank
        records.append((fields[1], float(fields[-1])))
    return records, divide_at


def md_table(headers, rows, aligns):
    """Render a padded Markdown table. aligns is 'l' or 'r' per column."""
    widths = [len(h) for h in headers]
    for row in rows:
        for c, cell in enumerate(row):
            widths[c] = max(widths[c], len(cell))

    def render(cells):
        out = []
        for c, cell in enumerate(cells):
            out.append(cell.rjust(widths[c]) if aligns[c] == "r"
                       else cell.ljust(widths[c]))
        return "| " + " | ".join(out) + " |"

    sep = []
    for c, w in enumerate(widths):
        sep.append("-" * (w - 1) + ":" if aligns[c] == "r" else "-" * w)

    lines = [render(headers), "| " + " | ".join(sep) + " |"]
    lines += [render(r) for r in rows]
    return "\n".join(lines)


def fmt(value):
    return f"{value:.2f}" if value is not None else ""


def doc_table(runs: int) -> str:
    prom, _ = run_program(SUPPORT / "doctable.cpp", runs=runs)
    noprom, _ = run_program(SUPPORT / "doctable.cpp",
                            extra_flags=["-DPGL_DISABLE_PROMOTION"], runs=runs)
    prom = dict(prom)
    noprom = dict(noprom)

    # (Markdown label, integer-row key, "/60"-row key or None, promotes?)
    # promotes is False for the types at the top of a promotion chain, i.e. those
    # with promoted_number_t<T> == T: BigInt, long double and Rational<BigInt>.
    # Their promotion columns are left empty (promotion is a no-op for them).
    spec = [
        ("`int16_t`",           "int16_t",          None,                 True),
        ("`int32_t`",           "int32_t",          None,                 True),
        ("`int64_t`",           "int64_t",          None,                 True),
        ("`int128`",            "pgl::int128",      None,                 True),
        ("`pgl::BigInt`",            "pgl::BigInt",      None,                 False),
        ("`float`",             "float",            None,                 True),
        ("`double`",            "double",           None,                 True),
        ("`long double`",       "long double",      None,                 False),
        ("`Rational<int32_t>`", "Rational i32",     "Rational i32/60",    False),
        ("`Rational<int64_t>`", "Rational i64",     "Rational i64/60",    False),
        ("`Rational<int128>`",  "Rational i128",    "Rational 128/60",    False),
        ("`Rational<BigInt>`",  "Rational BigInt",  "Rational BigInt/60", False),
    ]
    # No "promotion / integer / 60" column: only Rational types have a "/ 60"
    # variant, and Rational is never promoted, so that column is always empty.
    headers = ["Type",
               "promotion <br/> integer",
               "no promotion <br/> integer",
               "no promotion <br/> integer / 60"]
    rows = []
    for label, ikey, fkey, promotes in spec:
        rows.append([
            label,
            fmt(prom.get(ikey)) if promotes else "",
            fmt(noprom.get(ikey)),
            fmt(noprom.get(fkey)) if fkey else "",
        ])
    return md_table(headers, rows, ["l", "r", "r", "r"])


def boost_table(runs: int) -> str:
    records, divide_at = run_program(SUPPORT / "boosttable.cpp", runs=runs)
    if divide_at is None:
        divide_at = len(records)
    integer = dict(records[:divide_at])
    frac = dict(records[divide_at:])

    # (Markdown label, program label, has "/60" value)
    spec = [
        ("`int128`",                          "__int128_t",             False),
        ("`boost::multiprecision::int128_t`", "boost int128_t",         False),
        ("`pgl::BigInt`",                          "BigInt",                 False),
        ("`boost::cpp_int`",                  "boost cpp_int",          False),
        ("`pgl::Rational<int64_t>`",          "pgl Rational int64_t",   True),
        ("`boost::rational<int64_t>`",        "boost rational int64_t", True),
        ("`pgl::Rational<pgl::BigInt>`",           "pgl Rational BigInt",    True),
        ("`boost::rational<boost::cpp_int>`", "boost rational cpp_int", True),
    ]
    headers = ["Type", "integer", "integer / 60"]
    rows = []
    for label, key, has_frac in spec:
        rows.append([
            label,
            fmt(integer.get(key)),
            fmt(frac.get(key)) if has_frac else "",
        ])
    return md_table(headers, rows, ["l", "r", "r"])


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    group = ap.add_mutually_exclusive_group()
    group.add_argument("--doc", action="store_true",
                       help="only the Benchmark table (doctable.cpp)")
    group.add_argument("--boost", action="store_true",
                       help="only the Boost Number Types table (boosttable.cpp)")
    ap.add_argument("--runs", type=int, default=1, metavar="N",
                    help="run each program N times and keep the fastest (default 1)")
    args = ap.parse_args()

    do_doc = not args.boost
    do_boost = not args.doc

    if do_doc:
        print("=== Benchmark table (doc/types.md) ===", file=sys.stderr)
        print(doc_table(args.runs))
        if do_boost:
            print()
    if do_boost:
        print("=== Boost Number Types table (doc/types.md) ===", file=sys.stderr)
        print(boost_table(args.runs))


if __name__ == "__main__":
    main()
