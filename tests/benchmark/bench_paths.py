#!/usr/bin/env python3
"""Locate the recorded benchmark history, which lives in its own repository.

The history is a growing time series — hundreds of megabytes of JSONL after a
few dozen runs, and every run rewrites the files it touches. Keeping it in the
library's own git history would make every clone of pgl pay for it, so it is
kept in a separate data repository and joined back in only where it is read:
by the dashboard build, and by the Pages workflow that publishes it.

Resolution order:
  1. $PGL_BENCH_HISTORY
  2. ../pgl-benchmarks/history, a data checkout beside the library checkout
  3. tests/benchmark/history, for a checkout predating the split

Run as a script it prints the resolved path; that is how record.sh gets it.
"""
from __future__ import annotations

import os
from pathlib import Path

# The repository holding the recorded history.
DATA_REPO = "gfonsecabr/pgl-benchmarks"
DATA_REPO_URL = f"https://github.com/{DATA_REPO}"
# Directory name it is expected to be cloned into, beside the library checkout.
SIBLING_DIR = "pgl-benchmarks"


def project_root() -> Path:
    """The pgl checkout this script belongs to."""
    return Path(__file__).resolve().parent.parent.parent


def sibling_history() -> Path:
    return project_root().parent / SIBLING_DIR / "history"


def default_history() -> Path:
    """Where to read/write history when no --history was given.

    Returns the sibling path even when nothing exists yet, so callers can
    report a concrete path; use missing_history_message() for the guidance.
    """
    # Always absolute: record.sh hands the result to `git -C <data repo>`,
    # where a path relative to the library checkout would resolve elsewhere.
    env = os.environ.get("PGL_BENCH_HISTORY")
    if env:
        return Path(env).expanduser().resolve()
    sibling = sibling_history().resolve()
    if sibling.is_dir():
        return sibling
    in_tree = project_root() / "tests" / "benchmark" / "history"
    if in_tree.is_dir():
        return in_tree
    return sibling


def missing_history_message(path: Path) -> str:
    return (
        f"benchmark history not found at {path}\n"
        f"It lives in its own repository — clone it beside this one:\n"
        f"    git clone {DATA_REPO_URL}.git {project_root().parent / SIBLING_DIR}\n"
        f"or set PGL_BENCH_HISTORY to an existing checkout's history/ directory."
    )


if __name__ == "__main__":
    import sys

    resolved = default_history()
    if not resolved.is_dir():
        sys.exit(missing_history_message(resolved))
    print(resolved)
