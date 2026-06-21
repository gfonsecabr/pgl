#!/usr/bin/env python3
"""Auto-link API mentions in the reference pages to the doxygen site.

The editable source pages live in doc/raw/*.md (no links). This script reads
those, rewrites inline-code API mentions such as `s.midpoint()` or
`visitTrianglesIntersecting(s,f)` into links to the generated reference, and
writes the result to doc/*.md (the copies GitHub renders). Always edit the raw
versions; doc/*.md is generated and carries a "do not edit" banner.

The doxygen run emits a tag file (build/doc/pgl.tag, see GENERATE_TAGFILE in the
Doxyfile) mapping every member's signature to its html page + anchor.

Resolution is *context-aware* and *fail-closed*:

  * The current class context comes from the nearest section heading whose text
    names a class (`### Segment` -> pgl::Segment, `### Oriented Segment` ->
    pgl::OrientedSegment, `### Shape Tree` -> pgl::ShapeTree, ...). Method names
    collide heavily across classes (`contains` lives on 15 classes), so the
    receiver token (`s`, `l`, `t`) is ignored -- only the heading decides.
  * A mention is linked only when its method name resolves to exactly one member
    of the context class. If the name is not a member of that class (e.g. a
    cross-type `t.collinear(point)` under `### Segment`), it is left untouched
    and reported -- never mis-linked.
  * An overloaded name (several members, several anchors) links to the class page
    with no anchor, landing the reader on the right class.

A line consisting only of `- Other methods:` in a shape section is a placeholder:
it is filled with links to every method of that shape that did not otherwise get
a link in the section (so a method merely mentioned inside another method's
description, which never links, still shows up here).

Default mode is report-only. Pass --write to (re)generate doc/*.md from doc/raw.

Override syntax: append {ClassName} immediately after the code span to force the
context class for that one mention, e.g.  `t.collinear()`{Point}

Usage:
  python3 doc/raw/doxylink.py                    # report on doc/raw/*.md
  python3 doc/raw/doxylink.py --write            # generate doc/*.md from doc/raw
  python3 doc/raw/doxylink.py --base URL ...     # override the site base url
  python3 doc/raw/doxylink.py doc/raw/foo.md     # specific source files
"""

import argparse
import glob
import os
import re
import sys
import xml.etree.ElementTree as ET
from collections import defaultdict

DEFAULT_BASE = "https://gfonsecabr.github.io/pgl/"
DEFAULT_TAG = "build/doc/pgl.tag"
DEFAULT_RAW = "doc/raw"
DEFAULT_OUT = "doc"
BANNER = ("<!-- AUTO-GENERATED from {src} by doc/raw/doxylink.py — do not edit; "
          "edit the raw version and regenerate. -->")

# A code span is an API mention if it is, optionally, a `pgl::` qualifier and/or
# a `receiver.`, then a method name, then optional parentheses. Groups:
#   1 = "pgl::" qualifier (explicit free-function reference), 2 = receiver,
#   3 = method name, 4 = parentheses.
MENTION_RE = re.compile(
    r"^(pgl::)?(?:([A-Za-z_]\w*)\.)?([A-Za-z_]\w*)(\(.*\))?$")

# Inline code spans not already wrapped in a markdown link.
SPAN_RE = re.compile(r"(?<!\[)`([^`]+)`(?:\{([A-Za-z_]\w*)\})?")

HEADING_RE = re.compile(r"^(#{1,6})\s+(.*?)\s*#*\s*$")

# An "Other methods:" placeholder the author leaves in the raw markdown; the
# script fills it with the section's shape methods that got no link above.
PLACEHOLDER_RE = re.compile(r"^(\s*[-*]\s*Other methods:?)\s*$")


def norm(s):
    return re.sub(r"[^a-z0-9]", "", s.lower())


def load_tag(path):
    """Return (methods, class_by_norm, class_page, freefuncs, ns_names).

    methods:   full_class_name -> method_name -> list of (anchorfile, anchor).
    class_by_norm: normalized short class name -> full class name.
    class_page: full class name -> its html page (for linking a bare class name).
    freefuncs: free_function_name -> list of (anchorfile, anchor) for the
               namespace-level functions (pgl::stroke, pgl::convexHull, ...).
    ns_names:  every pgl:: member name (types, enums, ...) for `pgl::X` checks.
    """
    root = ET.parse(path).getroot()
    methods = defaultdict(lambda: defaultdict(list))
    class_by_norm = {}
    class_page = {}
    freefuncs = defaultdict(list)
    ns_names = set()          # every pgl:: name (types, enums, ...) for `pgl::X`
    for comp in root.findall("compound"):
        kind = comp.get("kind")
        full = comp.findtext("name")
        if kind in ("struct", "class"):
            short = full.split("::")[-1]
            # Prefer top-level classes over nested ones (Triangle vs Triangle::Iter)
            if "::" not in full or norm(short) not in class_by_norm:
                class_by_norm[norm(short)] = full
            class_page[full] = comp.findtext("filename")
            sink = methods[full]
            funcs_only = True
        elif kind == "namespace":
            sink = freefuncs
            funcs_only = False
        else:
            continue
        for m in comp.findall("member"):
            name = m.findtext("name")
            if not funcs_only and name:
                ns_names.add(name)
            if m.get("kind") != "function":
                continue
            af = m.findtext("anchorfile")
            an = m.findtext("anchor")
            if name and af and an:
                sink[name].append((af, an))
    # Drop deduction guides / constructors named after a class: a bare `Segment`
    # should mean the type, never a free function.
    for name in [n for n in freefuncs if norm(n) in class_by_norm]:
        del freefuncs[name]
    return methods, class_by_norm, class_page, freefuncs, ns_names


def _member_url(anchors):
    """URL for a member: the class page for overloads, else the member anchor."""
    if len(anchors) > 1:
        return anchors[0][0]
    af, an = anchors[0]
    return f"{af}#{an}"


def _hit(anchors, target):
    """Turn a resolved anchor list into a (status, url, detail) result."""
    if len(anchors) > 1:
        return ("overloaded", anchors[0][0], f"{target} ({len(anchors)} overloads)")
    af, an = anchors[0]
    return ("linked", f"{af}#{an}", target)


def resolve(content, override, context, methods, class_by_norm, class_page,
            freefuncs, ns_names):
    """Classify a code span. Returns (status, url_or_none, detail).

    A span is a "call form" if it carries a receiver dot or parentheses
    (`s.midpoint()`, `flip(e)`); otherwise it is a "bare" name (`edgesIntersecting`,
    `triangles`). Resolution tries, in order: a standalone class name (`OrientedLine`
    -> its class page), the heading's class context, then the pgl:: free functions
    (`convexHull`, `stroke`, ...). Call forms are reported loudly when nothing
    resolves (drift detection), while bare names that don't resolve are silently
    left alone -- a bare word is just as likely to be a variable, value, or type
    as a method.
    """
    m = MENTION_RE.match(content)
    if not m:
        return ("not-a-mention", None, None)
    qualified = m.group(1) is not None
    receiver = m.group(2)
    method = m.group(3)
    parens = m.group(4)
    is_call = qualified or receiver is not None or parens is not None

    # A token that names a class. Standing alone (optionally pgl::-qualified), link
    # it to its class page; with a receiver or arguments it is not a plain mention.
    if norm(method) in class_by_norm:
        if receiver is None and parens is None:
            full = class_by_norm[norm(method)]
            page = class_page.get(full)
            if page:
                return ("linked", page, f"{method} -> {full} (class)")
        return ("not-a-mention", None, None)

    cls = None
    if override:
        cls = class_by_norm.get(norm(override))
        if cls is None:
            return ("bad-override", None, override)
    elif not qualified:
        cls = context

    # 1. Member of the heading's class context (unless explicitly pgl::-qualified,
    #    which always means the free function).
    if cls is not None and not qualified:
        anchors = methods.get(cls, {}).get(method)
        if anchors:
            return _hit(anchors, f"{method} -> {cls}")

    # 2. A pgl:: free function (context-independent).
    anchors = freefuncs.get(method)
    if anchors:
        return _hit(anchors, f"{method} -> pgl (free)")

    # 3. Nothing resolved.
    if not is_call:
        return ("not-a-mention", None, None)
    if qualified:
        # A known pgl:: name we just don't link (a type/enum/variable) is fine;
        # an unknown pgl:: name is likely a typo worth surfacing.
        if method in ns_names:
            return ("not-a-mention", None, None)
        return ("not-in-context", None, f"{method} unknown in pgl namespace")
    if cls is None:
        return ("no-context", None, method)
    return ("not-in-context", None, f"{method} not a member of {cls}")


def process(src, dst, methods, class_by_norm, class_page, freefuncs, ns_names,
            base, write):
    with open(src) as fh:
        text = fh.read()
    lines = text.split("\n")
    context = None
    section = -1                       # bumps on every heading
    in_fence = False
    stats = defaultdict(int)
    details = []
    out_lines = []
    linked_here = defaultdict(set)     # section -> shape-member names that got a link
    placeholders = []                  # (out_index, section, cls, prefix)

    for lineno, line in enumerate(lines, 1):
        if line.lstrip().startswith("```"):
            in_fence = not in_fence
            out_lines.append(line)
            continue
        if in_fence:
            out_lines.append(line)
            continue

        h = HEADING_RE.match(line)
        if h:
            section += 1
            cls = class_by_norm.get(norm(h.group(2)))
            if cls is not None:
                context = cls
            out_lines.append(line)
            continue

        ph = PLACEHOLDER_RE.match(line)
        if ph and context is not None:
            placeholders.append((len(out_lines), section, context, ph.group(1)))
            out_lines.append(line)     # filled in after the whole section is seen
            continue

        def repl(mo):
            content = mo.group(1)
            override = mo.group(2)
            status, url, detail = resolve(
                content, override, context, methods, class_by_norm, class_page,
                freefuncs, ns_names
            )
            stats[status] += 1
            if status in ("linked", "overloaded", "not-in-context", "bad-override"):
                details.append((lineno, status, content, detail))
            if status in ("linked", "overloaded"):
                mm = MENTION_RE.match(content)
                if mm and context and mm.group(3) in methods.get(context, {}):
                    linked_here[section].add(mm.group(3))
                return f"[`{content}`]({base}{url})"
            return mo.group(0)

        out_lines.append(SPAN_RE.sub(repl, line))

    for idx, sec, cls, prefix in placeholders:
        short = cls.split("::")[-1]
        members = sorted(m for m in methods.get(cls, {})
                         if not m.startswith("operator") and m != short)
        others = [m for m in members if m not in linked_here[sec]]
        stats["other-filled"] += len(others)
        if others:
            items = ", ".join(f"[`{m}`]({base}{_member_url(methods[cls][m])})"
                              for m in others)
            out_lines[idx] = f"{prefix} {items}."
        else:
            out_lines[idx] = prefix

    if write:
        new_text = BANNER.format(src=src) + "\n\n" + "\n".join(out_lines)
        with open(dst, "w") as fh:
            fh.write(new_text)
    return stats, details


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("files", nargs="*",
                    help="source markdown files (default: doc/raw/*.md)")
    ap.add_argument("--tag", default=DEFAULT_TAG)
    ap.add_argument("--base", default=DEFAULT_BASE)
    ap.add_argument("--raw", default=DEFAULT_RAW, help="source dir (default: doc/raw)")
    ap.add_argument("--out", default=DEFAULT_OUT, help="output dir (default: doc)")
    ap.add_argument("--write", action="store_true",
                    help="(re)generate the output files from the raw sources")
    ap.add_argument("--verbose", action="store_true",
                    help="list every linked/skipped mention")
    args = ap.parse_args()

    base = args.base if args.base.endswith("/") else args.base + "/"

    if not os.path.exists(args.tag):
        sys.exit(f"tag file not found: {args.tag}\nrun: doxygen Doxyfile")
    methods, class_by_norm, class_page, freefuncs, ns_names = load_tag(args.tag)

    files = args.files or sorted(glob.glob(os.path.join(args.raw, "*.md")))
    totals = defaultdict(int)
    for path in files:
        dst = os.path.join(args.out, os.path.basename(path))
        stats, details = process(path, dst, methods, class_by_norm, class_page,
                                 freefuncs, ns_names, base, args.write)
        if not stats:
            continue
        summary = "  ".join(f"{k}={v}" for k, v in sorted(stats.items())
                            if k != "not-a-mention")
        if summary:
            print(f"{path}: {summary}")
        for k, v in stats.items():
            totals[k] += v
        if args.verbose:
            for lineno, status, content, detail in details:
                print(f"    {path}:{lineno} [{status}] `{content}`  {detail or ''}")
        else:
            for lineno, status, content, detail in details:
                if status in ("not-in-context", "bad-override"):
                    print(f"    {path}:{lineno} [{status}] `{content}`  {detail}")

    print("\nTOT:", "  ".join(f"{k}={v}" for k, v in sorted(totals.items())
                              if k != "not-a-mention"))
    if args.write:
        print(f"(wrote {len(files)} file(s) to {args.out}/)")
    else:
        print("(report only; pass --write to generate)")


if __name__ == "__main__":
    main()
