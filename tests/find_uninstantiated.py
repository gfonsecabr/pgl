#!/usr/bin/env python3
"""Flag template functions/methods that no test ever instantiates.

Runtime coverage (gcov) cannot see these: an uninstantiated template emits no
code, so there is nothing to mark as 0%. The only reliable signal is the Sema
AST, queried here through clang-query.

Method
------
Every template definition in include/ is a "declared" candidate (set D). Every
template *instantiation* the test build produces is "used" (set I). An
instantiation records the source location of the *name* of the pattern it came
from, which is identical to the pattern's own name location -- so name location
(file:line:col) is the join key. The answer is D - I.

Note: name location, not the node's range start. Out-of-line member templates
carry a two-line template head, so the range starts above the name; only the
name loc lines up between a pattern and its instantiations.

Caveat: D is restricted to template entities. Non-template functions (BigInt,
Canvas, plain free functions) are never "instantiated" and belong to gcov's
domain, not this one; they are excluded so they don't show up as false hits.
"""

import os
import re
import subprocess
import sys
import tempfile
from signal import signal, SIGPIPE, SIG_DFL

signal(SIGPIPE, SIG_DFL)  # let `| head` close the pipe quietly

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CXXFLAGS = ["-std=c++20", "-Iinclude", "-Itests/doctest"]
# Anchor the in-library filter to this repo's include/. Library headers are
# spelled relative ("include/shape/..."), while system STL headers are absolute
# ("/usr/.../include/c++/..."), so a leading-anchored "^include/" keeps only ours.
INCLUDE_RE = "^include/"

# A dumped root node line looks like:
#   CXXMethodDecl 0x.. [parent 0x.. prev 0x..] <FILE:BL:BC, ...> NAMELOC ... name 'type'
# We need FILE (from the range start) and NAMELOC (the loc right after '>').
RANGE_RE = re.compile(r"<(?P<file>[^,>]*?):(?P<bl>\d+):(?P<bc>\d+)[^>]*>\s*(?P<rest>.*)")
NAMELOC_RE = re.compile(
    r"^(?:(?P<file>[^ :]+):)?(?:line:(?P<line>\d+):(?P<col>\d+)|col:(?P<colonly>\d+))"
)
# Constructors/destructors print with template args (e.g. "Disk<PointType, Label>"),
# so allow an optional "<...>" between the identifier and the ' type literal.
NAME_RE = re.compile(r"(?P<name>operator\S+|~?\w+)(?:<[^']*>)?\s+'")


def run_query(matcher, source):
    """Yield (key, label) for each matched root node in `source`.

    key   = "file:line:col" of the matched function's name (the join key)
    label = human-readable "qualname 'signature'" for reporting
    """
    cmd = [
        "clang-query",
        "-c=set output dump",
        f"-c=match {matcher}",
        source,
        "--",
        *CXXFLAGS,
    ]
    proc = subprocess.Popen(
        cmd, cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True
    )
    expect_root = False
    last_file = ""
    for line in proc.stdout:
        if line.startswith('Binding for "root":'):
            expect_root = True
            continue
        if not expect_root:
            continue
        # First non-empty line after the binding header is the root node.
        if not line.strip():
            continue
        expect_root = False
        rm = RANGE_RE.search(line)
        if not rm:
            continue
        file = rm.group("file") or last_file
        last_file = file
        rest = rm.group("rest")
        nm = NAMELOC_RE.match(rest)
        if not nm:
            continue
        nfile = nm.group("file") or file
        if nm.group("line"):
            nline, ncol = nm.group("line"), nm.group("col")
        else:  # name loc printed as col-only -> same line as range start
            nline, ncol = rm.group("bl"), nm.group("colonly")
        key = f"{nfile}:{nline}:{ncol}"
        name_m = NAME_RE.search(rest)
        label = name_m.group("name") if name_m else "?"
        yield key, f"{nfile}:{nline}  {label}  {rest.split(chr(39))[1] if chr(39) in rest else ''}"
    proc.wait()


def collect(matcher, source, store_labels=None):
    keys = set()
    for key, label in run_query(matcher, source):
        keys.add(key)
        if store_labels is not None:
            store_labels.setdefault(key, label)
    return keys


def main(argv):
    tests = argv[1:]
    if not tests:
        for d in ("tests/unit", "tests/integration"):
            full = os.path.join(ROOT, d)
            if os.path.isdir(full):
                tests += [
                    os.path.join(d, f)
                    for f in sorted(os.listdir(full))
                    if f.endswith(".cpp")
                ]

    # D: every template definition in include/, restricted to template entities.
    labels = {}
    with tempfile.NamedTemporaryFile(
        mode="w", suffix=".cpp", dir=ROOT, delete=False
    ) as tu:
        tu.write('#include "pgl.hpp"\n')
        decls_tu = tu.name
    try:
        print("Collecting declared template definitions...", file=sys.stderr)
        declared = collect(
            f'functionDecl(isExpansionInFileMatching("{INCLUDE_RE}"), isDefinition(), '
            "unless(isTemplateInstantiation()), "
            # Drop generic-lambda operator() bodies and other helpers defined
            # inside a closure: they are internal implementation detail, not API.
            "unless(hasAncestor(cxxRecordDecl(isLambda()))), "
            "anyOf(hasParent(functionTemplateDecl()), "
            "hasAncestor(classTemplateDecl())))",
            decls_tu,
            labels,
        )
    finally:
        os.unlink(decls_tu)
    print(f"  {len(declared)} candidate template definitions", file=sys.stderr)

    # I: every instantiation any test produces.
    instantiated = set()
    for i, t in enumerate(tests, 1):
        print(f"[{i}/{len(tests)}] {t}", file=sys.stderr)
        instantiated |= collect(
            f'functionDecl(isExpansionInFileMatching("{INCLUDE_RE}"), '
            "isTemplateInstantiation())",
            t,
        )

    never = sorted(declared - instantiated)
    print(f"\n{len(never)} template definitions never instantiated by any test:\n")
    for key in never:
        print(f"  {labels.get(key, key)}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
