#!/usr/bin/env python3
"""Which machine a measurement came from, and where its CGAL baseline lives.

A machine is identified by its CPU model and its compiler *family* (g++,
clang++), never by the compiler's version. The version is recorded next to each
measurement (`compiler_version`), but keying on it would start every curve over
after a compiler update, which is exactly the kind of change the history exists
to show.

Every reader of the history derives the machine through record_machine() or
meta_machine() rather than trusting a stored label, so records written before
the family was recorded land on the same machine as the ones written after.

The CGAL baseline is one snapshot per machine, history/asymptotic-baseline/
<slug>.json: CGAL's times are only a reference for pgl times measured on the
same hardware with the same compiler. history/asymptotic-baseline.json is the
single file that predates the split; it is still read, as the baseline of the
machine its meta names, until that machine records a baseline again.
"""
from __future__ import annotations

import json
import os
import re
import subprocess
from pathlib import Path

BASELINE_DIR = "asymptotic-baseline"
LEGACY_BASELINE = "asymptotic-baseline.json"


def detect_cpu() -> str:
    try:
        out = subprocess.run(
            ["lscpu"], capture_output=True, text=True,
            env={**os.environ, "LC_ALL": "C"},
        ).stdout
        for line in out.splitlines():
            if "Model name" in line:
                return line.split(":", 1)[1].strip()
    except FileNotFoundError:
        pass
    try:
        with open("/proc/cpuinfo") as f:
            for line in f:
                if line.startswith("model name"):
                    return line.split(":", 1)[1].strip()
    except OSError:
        pass
    return ""


def detect_compiler_version(cxx: str) -> str:
    """The first line of `cxx --version`, or '' when it cannot be run."""
    try:
        out = subprocess.run([cxx, "--version"], capture_output=True, text=True,
                             env={**os.environ, "LC_ALL": "C"}).stdout
    except OSError:
        return ""
    return out.splitlines()[0].strip() if out.strip() else ""


def compiler_family(cxx: str, version: str = "") -> str:
    """g++ or clang++ (or the command's own name for anything else).

    Decided from the version banner when there is one, since `c++` names either
    compiler depending on the system. Without one — records written before the
    banner was kept — only the command name is left, and a bare `c++` counts as
    g++: every such record was measured on the one machine that recorded
    history then, whose `c++` is GCC.
    """
    banner = version.lower()
    if "clang" in banner:
        return "clang++"
    if "(gcc)" in banner or "free software foundation" in banner or banner.startswith("g++"):
        return "g++"
    name = Path(cxx.split()[-1] if cxx.strip() else "").name
    name = re.sub(r"-\d+(\.\d+)*$", "", name)  # g++-14, clang++-18
    if "clang" in name:
        return "clang++"
    if name in ("g++", "gcc", "c++", "cc"):
        return "g++"
    return name or "unknown"


def machine_label(cpu: str, family: str) -> str:
    return f"{cpu or 'unknown'} · {family}"


def record_machine(record: dict) -> str:
    """The machine a history record was measured on."""
    family = record.get("compiler_family") or compiler_family(
        record.get("cxx", ""), record.get("compiler_version", ""))
    return machine_label(record.get("cpu", ""), family)


def meta_machine(meta: dict) -> str:
    """The machine a runner snapshot (its `meta` block) was measured on."""
    family = meta.get("compiler_family") or compiler_family(
        meta.get("compiler", ""), meta.get("compiler_version", ""))
    return machine_label(meta.get("cpu") or "", family)


def baseline_path(history: Path, machine: str) -> Path:
    slug = re.sub(r"[^a-z0-9+]+", "-", machine.lower()).strip("-")
    return Path(history) / BASELINE_DIR / f"{slug}.json"


def read_baselines(history: Path) -> dict[str, dict]:
    """machine -> baseline snapshot ({meta, results}) for every recorded machine."""
    history = Path(history)
    snapshots: dict[str, dict] = {}
    for path in sorted((history / BASELINE_DIR).glob("*.json")):
        try:
            snapshot = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            continue
        snapshots[meta_machine(snapshot.get("meta", {}))] = snapshot
    legacy = history / LEGACY_BASELINE
    if legacy.exists():
        try:
            snapshot = json.loads(legacy.read_text(encoding="utf-8"))
            snapshots.setdefault(meta_machine(snapshot.get("meta", {})), snapshot)
        except (OSError, json.JSONDecodeError):
            pass
    return snapshots
