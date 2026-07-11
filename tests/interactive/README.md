# Interactive predicate explorer

A small Qt desktop app for poking at Pangolin's shape predicates by hand. Pick a
type for each of two shapes **A** and **B**, draw and drag them on a snapping
grid, and watch — updated on every edit:

- whether each shape is **degenerate** (or cannot be built from the current
  points at all),
- every predicate in **both directions** — `A.predicate(B)` and
  `B.predicate(A)` — for `contains`, `boundaryContains`, `interiorContains`,
  `intersects`, `interiorsIntersect`, `separates`, and `crosses`,
- the highlighted **`A.intersection(B)`**, with its result type named.

All geometry is computed with exact `pgl::ERational` (`Rational<BigInt>`)
coordinates. Doubles appear only when mapping to screen pixels, so the reported
predicate and intersection results are exact.

Supported types: Point, Segment, OrientedSegment, Line, OrientedLine, Ray,
Halfplane, Rectangle, Triangle, Disk, Convex, MonotoneChain, Polyline, Polygon.

## Build & run

Needs Qt 6 (falls back to Qt 5) and a C++20 compiler. Not part of
`tests/run_tests.sh` — it is a standalone, manually built program, like the
other `tests/graphical/` tools.

```bash
cd tests/interactive
cmake -S . -B build
cmake --build build
./build/pgl_interactive
```

## Controls

| Action | Effect |
| --- | --- |
| Drag a handle | Move that vertex (snaps to the grid) |
| Shift + drag | Translate the **active** shape (snaps to the grid) |
| Double-click | Add a vertex to the active shape (Convex / MonotoneChain / Polyline / Polygon) |
| Right-click a handle | Remove that vertex (variable-vertex shapes) |
| Drag empty space | Pan the view |
| Mouse wheel | Zoom |
| Click a shape box | Make that shape active |
| `a` / `b` | Make shape A / B active |
| `Ctrl+a` / `Ctrl+b` | Cycle that shape to the next type |
| `Ctrl+c` / `Ctrl+v` | Copy / paste the active shape (see below) |

The **active shape** — the one that receives new vertices and translations — is
shown by a highlighted box border matching its on-canvas color; click a shape's
box (or press `a` / `b`) to activate it. Handles of either shape can be dragged
at any time. For a Rectangle the two handles are opposite corners; for a Disk
the three handles are points on its boundary circle.

## Copy / paste as pgl code

Each shape box has a 📋 (copy) and 📥 (paste) button beside its type selector
(or use `Ctrl+c` / `Ctrl+v` on the active shape).
**Copy** puts the shape on the clipboard as a line of pgl C++ you can drop
straight into code, e.g.

```c++
ETriangle a{{-4, -3}, {4, -3}, {0, 4}};
ESegment b{{1, 2}, {3, 4}};
EPoint a{0, 0};
```

**Paste** parses such a line from the clipboard and sets that box's shape from
it — the leading type name selects the shape kind and the coordinates become its
control points. Parsing is lenient about the variable name, extra whitespace,
and the trailing semicolon; coordinates are rounded to the integer grid.

## Note on degenerate input

Pangolin treats degenerate geometric input (a collinear triangle, a
self-touching polygon, …) as undefined behavior guarded by asserts, so this
target is built with `NDEBUG` to keep those asserts from aborting the editor.
The app still reports `isDegenerate()`, but predicate/intersection output for a
shape flagged **degenerate** should not be trusted. Configurations that cannot
be constructed at all (e.g. a non-simple Polygon) are reported as *invalid*.
