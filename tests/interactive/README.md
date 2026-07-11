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

The **Active shape** radio selects which shape receives new vertices and
translations; handles of either shape can be dragged at any time. For a
Rectangle the two handles are opposite corners; for a Disk the three handles are
points on its boundary circle.

## Note on degenerate input

Pangolin treats degenerate geometric input (a collinear triangle, a
self-touching polygon, …) as undefined behavior guarded by asserts, so this
target is built with `NDEBUG` to keep those asserts from aborting the editor.
The app still reports `isDegenerate()`, but predicate/intersection output for a
shape flagged **degenerate** should not be trusted. Configurations that cannot
be constructed at all (e.g. a non-simple Polygon) are reported as *invalid*.
