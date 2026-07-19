#!/usr/bin/env python3
"""Auto-link API mentions in the reference pages to the doxygen site.

The editable source pages live in doc/raw/*.md (no links). This script reads
those, rewrites inline-code API mentions such as `s.midpoint()` or
`visitTrianglesIntersecting(s,f)` into links to the generated reference, and
writes the result to doc/*.md (the copies GitHub renders). Always edit the raw
versions; doc/*.md is generated and carries a "do not edit" banner.

The script first runs doxygen (see the Doxyfile) to (re)generate the tag file
(build/doc/pgl.tag, see GENERATE_TAGFILE) and the XML output it reads, so the
links always reflect the current headers. Pass --no-doxygen to reuse whatever is
already in build/doc. The tag file maps every member's signature to its html page
+ anchor.

Resolution is *context-aware* and *fail-closed*:

  * The current class context comes from the nearest section heading whose text
    names a class (`### Segment` -> pgl::Segment, `### Oriented Segment` ->
    pgl::OrientedSegment, `### Shape Tree` -> pgl::ShapeTree, ...). A heading
    whose full text names no class still sets the context when it mentions one
    in backticks (`### Polymorphism with `Shape`` -> pgl::Shape), so a prose
    heading can name its class without being reduced to the bare class name.
    Method names
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
  python3 doc/raw/doxylink.py                    # regenerate tags, report on doc/raw/*.md
  python3 doc/raw/doxylink.py --write            # regenerate tags, write doc/*.md
  python3 doc/raw/doxylink.py --no-doxygen       # reuse the existing build/doc tag/xml
  python3 doc/raw/doxylink.py --base URL ...     # override the site base url
  python3 doc/raw/doxylink.py doc/raw/foo.md     # specific source files
"""

import argparse
import glob
import os
import re
import subprocess
import sys
import xml.etree.ElementTree as ET
from collections import defaultdict

DEFAULT_BASE = "https://gfonsecabr.github.io/pgl/"
DEFAULT_TAG = "build/doc/pgl.tag"
DEFAULT_XML = "build/doc/xml"
DEFAULT_HTML = "build/doc/html"
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


def heading_class(text, class_by_norm):
    """Return the class a heading puts us in context of, or None.

    The whole heading text wins when it names a class (`### Shape Tree`), so
    existing headings keep resolving as before. Otherwise a class named in
    backticks decides (`### Polymorphism with `Shape``), which lets a prose
    heading carry a context; the first such mention that resolves wins.
    """
    cls = class_by_norm.get(norm(text))
    if cls is not None:
        return cls
    for span in re.findall(r"`([^`]+)`", text):
        cls = class_by_norm.get(norm(span))
        if cls is not None:
            return cls
    return None


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


def load_briefs(xml_dir):
    """Return (anchor_brief, page_brief) from the doxygen XML output.

    anchor_brief: html anchor -> @brief text (members + free functions).
    page_brief:   html page    -> @brief text (classes/namespaces, for class links).
    The tag file has no descriptions, so the XML is the source of @brief text.
    """
    anchor_brief, page_brief = {}, {}
    if not xml_dir or not os.path.isdir(xml_dir):
        return anchor_brief, page_brief

    def text(el):
        if el is None:
            return ""
        return " ".join("".join(el.itertext()).split())

    for path in glob.glob(os.path.join(xml_dir, "*.xml")):
        if path.endswith("index.xml"):
            continue
        try:
            root = ET.parse(path).getroot()
        except ET.ParseError:
            continue
        for comp in root.findall("compounddef"):
            if comp.get("kind") not in ("struct", "class", "namespace"):
                continue
            cid = comp.get("id")
            b = text(comp.find("briefdescription"))
            if b:
                page_brief.setdefault(f"{cid}.html", b)
            for md in comp.iter("memberdef"):
                if md.get("kind") != "function":
                    continue
                mid = md.get("id") or ""
                # member id is "<compound-id>_1<anchor>"
                if mid.startswith(cid + "_1"):
                    anchor = mid[len(cid) + 2:]
                    mb = text(md.find("briefdescription"))
                    if mb:
                        anchor_brief.setdefault(anchor, mb)
    return anchor_brief, page_brief


def link_md(text, rel_url, base, briefs):
    """Render a markdown link, adding the @brief as a hover title when known."""
    anchor_brief, page_brief = briefs
    if "#" in rel_url:
        b = anchor_brief.get(rel_url.split("#", 1)[1])
    else:
        b = page_brief.get(rel_url)
    if b:
        return f'[`{text}`]({base}{rel_url} "{b.replace(chr(34), chr(39))}")'
    return f"[`{text}`]({base}{rel_url})"


def _member_url(anchors):
    """URL for a member. Overloads share no common anchor, so point at the first
    one; doxygen lists the rest consecutively right below it."""
    af, an = anchors[0]
    return f"{af}#{an}"


def _hit(anchors, target):
    """Turn a resolved anchor list into a (status, url, detail) result."""
    if len(anchors) > 1:
        return ("overloaded", _member_url(anchors), f"{target} ({len(anchors)} overloads)")
    return ("linked", _member_url(anchors), target)


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
    # Class names are capitalized, so a lowercase token never names one: `shape` is a
    # variable, and `triangulation` is Polygon::triangulation(), not pgl::Triangulation.
    # Without this guard both would be captured here -- the first mis-linked to the
    # class page, the second swallowed as not-a-mention before method resolution runs.
    if method[:1].isupper() and norm(method) in class_by_norm:
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
            briefs, base, write):
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
            cls = heading_class(h.group(2), class_by_norm)
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
                return link_md(content, url, base, briefs)
            return mo.group(0)

        out_lines.append(SPAN_RE.sub(repl, line))

    for idx, sec, cls, prefix in placeholders:
        short = cls.split("::")[-1]
        members = sorted(m for m in methods.get(cls, {})
                         if not m.startswith("operator") and m != short)
        others = [m for m in members if m not in linked_here[sec]]
        stats["other-filled"] += len(others)
        if others:
            items = ", ".join(link_md(m, _member_url(methods[cls][m]), base, briefs)
                              for m in others)
            out_lines[idx] = f"{prefix} {items}."
        else:
            out_lines[idx] = prefix

    if write:
        new_text = BANNER.format(src=src) + "\n\n" + "\n".join(out_lines)
        with open(dst, "w") as fh:
            fh.write(new_text)
    return stats, details


def regenerate_tags(doxyfile):
    """Run doxygen so the tag file and XML reflect the current headers. Doxygen's
    output is shown only on failure to keep a normal run quiet."""
    if not os.path.exists(doxyfile):
        sys.exit(f"Doxyfile not found: {doxyfile}")
    try:
        proc = subprocess.run(["doxygen", doxyfile],
                              stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                              text=True)
    except FileNotFoundError:
        sys.exit("doxygen not found on PATH; install it or pass --no-doxygen")
    if proc.returncode != 0:
        sys.stderr.write(proc.stdout)
        sys.exit(f"doxygen failed (exit {proc.returncode})")


# Doxygen's bundled navtree.js has a sidebar-resize bug: `barWidth` (6) is
# subtracted from both the sidebar width and the persisted width cookie on every
# page load and every window-resize tick, so the left sidebar loses a few px each
# time and collapses over repeated reloads/resizes. Zeroing barWidth removes the
# drift -- the resize handle (.ui-resizable-e) is absolutely positioned and does
# not consume layout width, so 0 is safe. Note doxygen-pgl.css deliberately keeps
# #side-nav border/padding at 0; any horizontal box there would reintroduce the
# drift in the opposite direction. The CI build patches navtree.js the same way
# (see .github/workflows/pages.yml). Upstream bug:
#   https://github.com/jothepro/doxygen-awesome-css/issues/170
NAVTREE_BARWIDTH_RE = re.compile(r"(barWidth)\s*=\s*\d+")


def patch_navtree(html_dir):
    """Zero out navtree.js's buggy `barWidth` so the sidebar stops shrinking.

    navtree.js is regenerated by doxygen on every run, so this re-applies after
    each regeneration. Idempotent and silent unless it actually changes the file."""
    path = os.path.join(html_dir, "navtree.js")
    try:
        with open(path) as fh:
            src = fh.read()
    except FileNotFoundError:
        return                          # GENERATE_HTML off, or unexpected layout
    patched, n = NAVTREE_BARWIDTH_RE.subn(r"\1=0", src)
    if n and patched != src:
        with open(path, "w") as fh:
            fh.write(patched)
        print(f"(patched {path}: barWidth -> 0, sidebar-resize fix)")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("files", nargs="*",
                    help="source markdown files (default: doc/raw/*.md)")
    ap.add_argument("--tag", default=DEFAULT_TAG)
    ap.add_argument("--xml", default=DEFAULT_XML,
                    help="doxygen XML dir for @brief tooltips (default: build/doc/xml)")
    ap.add_argument("--html", default=DEFAULT_HTML,
                    help="doxygen HTML output dir to patch (default: build/doc/html)")
    ap.add_argument("--doxyfile", default="Doxyfile",
                    help="Doxyfile used to regenerate the tag/xml (default: Doxyfile)")
    ap.add_argument("--no-doxygen", action="store_true",
                    help="skip running doxygen; reuse the existing tag/xml in build/doc")
    ap.add_argument("--base", default=DEFAULT_BASE)
    ap.add_argument("--raw", default=DEFAULT_RAW, help="source dir (default: doc/raw)")
    ap.add_argument("--out", default=DEFAULT_OUT, help="output dir (default: doc)")
    ap.add_argument("--write", action="store_true",
                    help="(re)generate the output files from the raw sources")
    ap.add_argument("--verbose", action="store_true",
                    help="list every linked/skipped mention")
    args = ap.parse_args()

    base = args.base if args.base.endswith("/") else args.base + "/"

    if not args.no_doxygen:
        regenerate_tags(args.doxyfile)
        patch_navtree(args.html)       # fix the regenerated navtree.js sidebar bug

    if not os.path.exists(args.tag):
        hint = "doxygen failed to emit it" if not args.no_doxygen else \
               f"run: doxygen {args.doxyfile} (or drop --no-doxygen)"
        sys.exit(f"tag file not found: {args.tag}\n{hint}")
    methods, class_by_norm, class_page, freefuncs, ns_names = load_tag(args.tag)
    briefs = load_briefs(args.xml)

    files = args.files or sorted(glob.glob(os.path.join(args.raw, "*.md")))
    totals = defaultdict(int)
    for path in files:
        dst = os.path.join(args.out, os.path.basename(path))
        stats, details = process(path, dst, methods, class_by_norm, class_page,
                                 freefuncs, ns_names, briefs, base, args.write)
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
