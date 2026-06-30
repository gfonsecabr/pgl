# Integration test coverage plan (`shape1.method(shape2)`)

Goal: every real binary method of the form `shape1.method(shape2)` is tested, and
the test lives in the canonically-named file `tests/integration/shape1_shape2.cpp`.

## Conventions

- **Canonical shape order** (used for the `shape1_shape2` filename — `shape1` is the
  one that comes first in this list):

  `point, segment, orientedsegment, line, orientedline, ray, halfplane, rectangle,
  triangle, convex, polygon, disk`

  Example: `Triangle.separates(Disk)` ⇒ file `rectangle`/`triangle` before `disk`
  ⇒ `triangle_disk.cpp`.

- **Binary method families in scope** (non-stub declarations only; the inline
  `{ return false; }` placeholders and symmetry forwarders are excluded):
  `contains`, `boundaryContains`, `interiorContains`, `intersects`,
  `interiorsIntersect`, `separates`, `crosses`, `intersection`.

- **A pair's file covers both directions**: `shape1_shape2.cpp` tests both
  `s1.m(s2)` and `s2.m(s1)`.

- **Self-pairs** (`segment.contains(segment)`, …) live in `unit/<shape>.cpp`
  (e.g. `unit/segment.cpp`), NOT in an integration `shape1_shape1.cpp` file.
  Integration files are for **distinct** shape pairs only.
  Exception to fix: `tests/integration/segment_segment.cpp` is misplaced under this
  rule — its content should move into `unit/segment.cpp`.

- Test scaffolding mirrors existing integration files: `#include "pgl.hpp"` first
  (Windows/doctest macro ordering), then `#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN`
  and `#include "doctest.h"`. Use `CHECK_MESSAGE` / `CHECK_FALSE_MESSAGE` with the
  operands streamed into the message, as in `point_segment.cpp`.

- **No degenerate inputs**: degenerate geometry is UB in pgl — do not assert on it.

- Each new/edited test must compile clean under **both** `g++` and `clang++` with
  `-std=c++20 -Wall -Wextra -pedantic`, and run green via `sh tests/run_tests.sh <name>`.

## Audit summary (current state)

- 78 pairs have real binary methods: 66 distinct-shape pairs (→ integration files)
  + 12 self-pairs (→ `unit/<shape>.cpp`).
- Only ~20 distinct-shape pairs have an `integration/shape1_shape2.cpp` file.
- Self-pairs and most distinct pairs are currently exercised in `unit/<shape>.cpp`.
- Mis-located/mis-named files: `segment_segment.cpp` (self-pair, belongs in
  `unit/segment.cpp`); `separates_disk.cpp` (operation-named; covers `triangle_disk`
  + `rectangle_disk` separates); `boundingbox.cpp`, `shapetree.cpp` (not pair-based).

## Not implemented yet — EXCLUDED from coverage (they `throw` at runtime)

These declarations compile but `throw std::runtime_error("...not implemented yet")`
when instantiated, so they are NOT testable and are out of scope until implemented.
The audit's "real method" count was inflated by these; treat them as library TODOs.

Direct throws:
- `Disk::separates(Polygon)`
- `Polygon::intersects(Disk)`, `Polygon::interiorsIntersect(Disk)`,
  `Polygon::interiorContains(Disk)`, `Polygon::crosses(Disk)`
- `Polygon::separates(Rectangle)`, `Polygon::separates(Triangle)`,
  `Polygon::separates(Convex)`, `Polygon::separates(Polygon)`,
  `Polygon::separates(Disk)`

Transitively broken (delegate to a throwing `Polygon::separates`):
- `Polygon::crosses(Rectangle)`, `Polygon::crosses(Triangle)`,
  `Polygon::crosses(Convex)`, `Polygon::crosses(Polygon)`
- Symmetric forwarders that bounce into the above also throw, e.g.
  `Rectangle::crosses(Polygon)`, `Disk::intersects(Polygon)`.

Consequence: `Polygon` has NO working `separates`/`crosses` against any area shape
(rectangle/triangle/convex/polygon/disk), and `Polygon`↔`Disk` is essentially
unimplemented except for the disk-as-container containment predicates.

---

## Phase 0 — Policy (DECIDED)

- [x] Convention: distinct-shape pairs go in `integration/shape1_shape2.cpp` (all real
      binary methods, both directions); **self-pairs go in `unit/<shape>.cpp`**.
- [x] **Migrate** distinct-pair cross-shape tests out of `unit/<shape>.cpp` into the
      integration files. Integration is the single source of truth for distinct pairs;
      no duplication. Self-pair blocks STAY in the unit files.
- [x] **Relocate** `separates_disk.cpp` content into `triangle_disk.cpp` /
      `rectangle_disk.cpp`, then delete `separates_disk.cpp`.
- [x] **Relocate** `segment_segment.cpp` content into `unit/segment.cpp`, then delete
      the integration file.

## Phase 1 — Fill the true gaps (no test anywhere) — HIGHEST PRIORITY

These pairs have real methods but ZERO detected coverage. Cover only the
genuinely-implemented methods (the throwing pairs above are out of scope).

- [x] `tests/integration/orientedsegment_polygon.cpp` — full set (mirror
      `segment_polygon.cpp`): polygon.{boundaryContains,contains,interiorContains,
      interiorsIntersect,intersects,intersection,separates,crosses}(orientedsegment)
      and orientedsegment.{contains,interiorContains,separates}(polygon).
- [x] `tests/integration/orientedline_polygon.cpp` — full set (mirror
      `line_polygon.cpp`).
- [x] `tests/integration/rectangle_polygon.cpp` — testable:
      rectangle.{boundaryContains,contains,interiorContains,separates,intersects,
      interiorsIntersect}(polygon) and polygon.{boundaryContains,contains,
      interiorContains,interiorsIntersect,intersects}(rectangle).
      SKIP polygon.crosses/separates(rectangle) and rectangle.crosses(polygon) — throw.
- [x] `tests/integration/triangle_polygon.cpp` — testable:
      triangle.{boundaryContains,contains,interiorContains,separates,intersects,
      interiorsIntersect}(polygon) and polygon.{boundaryContains,contains,
      interiorContains,interiorsIntersect,intersects}(triangle).
      SKIP polygon.crosses/separates(triangle) and triangle.crosses(polygon) — throw.
- [x] `tests/integration/polygon_disk.cpp` — testable ONLY:
      disk.{boundaryContains,contains,interiorContains}(polygon) and
      polygon.boundaryContains(disk). Everything else throws (document in the file).

After each: `sh tests/run_tests.sh <name>` green under g++ and clang++.

## Phase 2 — Complete the existing integration files

Add the missing method families to files that already exist.

- [x] `line_orientedline.cpp` — add ALL 7 (currently only `crossingOrder`):
      contains, crosses, interiorContains, interiorsIntersect, intersection,
      intersects, separates.
- [x] `convex_polygon.cpp` — add boundaryContains, contains, interiorContains,
      interiorsIntersect, intersects. (`separates` already covered one direction via
      `convex.separates(polygon)`; SKIP `crosses` and `polygon.separates(convex)` —
      they throw.)
- [x] `line_polygon.cpp` — add boundaryContains, contains, crosses,
      interiorContains, interiorsIntersect, intersects, separates.
- [x] `halfplane_polygon.cpp` — add boundaryContains, contains, crosses,
      interiorContains, interiorsIntersect, intersects.
- [x] `halfplane_convex.cpp` — add boundaryContains, contains, crosses,
      interiorContains, separates.
- [x] `halfplane_disk.cpp` — add boundaryContains, contains, crosses,
      interiorContains, separates.
- [x] `point_segment.cpp` — add crosses, interiorsIntersect, intersection,
      intersects, separates.
- [x] `segment_ray.cpp` — add crosses, interiorContains, interiorsIntersect,
      intersection, intersects.
- [x] `ray_polygon.cpp` — add contains, crosses, interiorsIntersect, intersects.
- [x] `line_disk.cpp` — add boundaryContains, contains, crosses, separates.
- [x] `ray_disk.cpp` — add boundaryContains, crosses, separates.
- [x] `segment_disk.cpp` — add boundaryContains, crosses, separates.
- [x] `line_convex.cpp` — add boundaryContains, interiorContains.
- [x] `line_rectangle.cpp` — add boundaryContains, interiorContains.
- [x] `ray_convex.cpp` — add boundaryContains, interiorContains.
- [x] `line_triangle.cpp` — add interiorContains.
- [x] `segment_polygon.cpp` — add crosses.
- [x] `segment_triangle.cpp` — add interiorsIntersect.

Already complete (no action): `segment_convex`, `segment_line`, `segment_rectangle`,
`segment_segment`, `line_ray`.

## Phase 3 — Self-pairs in `unit/<shape>.cpp` (verify/complete in place)

Self-pairs stay in the unit file — do NOT create `shape1_shape1.cpp`. For each, verify
all real self-methods are exercised in `unit/<shape>.cpp` and fill any gaps.

- [x] `unit/point.cpp` — point self-methods (filled gaps: interiorsIntersect, separates)
- [x] `unit/segment.cpp` — segment self-methods (migrated `segment_segment.cpp`
      content here and deleted that integration file)
- [x] `unit/orientedsegment.cpp` — verified complete (contains/interiorContains,
      intersects, interiorsIntersect, separates, crosses, intersection all self-paired).
- [x] `unit/line.cpp` — filled gaps: separates, interiorContains, boundaryContains
      against another Line (rest were already covered).
- [x] `unit/orientedline.cpp` — verified complete (intersection self-pair was already
      there via `.intersection<Rational>(IntLine{...})`).
- [x] `unit/ray.cpp` — filled gap: crosses against another Ray.
- [x] `unit/halfplane.cpp` — verified complete (contains/interiorContains/
      interiorsIntersect/separates/crosses all self-paired).
- [x] `unit/rectangle.cpp` — verified complete (all families self-paired,
      incl. intersection).
- [x] `unit/triangle.cpp` — verified complete for contains/boundaryContains/
      interiorContains/intersects/interiorsIntersect/separates/crosses.
      FIXED `Triangle::intersection(Triangle)`: it was incomplete (result variant only
      {Point, Segment}; returned just the first edge-crossing piece, not the overlap).
      Now delegates to `asConvex().intersection(other.asConvex())`, so an area overlap
      returns a Convex. Added the self-pair intersection test (overlap→Convex,
      edge→Segment, contained→Convex, disjoint→empty).
      FIXED `Triangle::intersection(Rectangle)` and `Triangle::intersection(Halfplane)`
      (same incompleteness): both now delegate to the convex clip. This added a new
      `Convex::intersection(Halfplane)` (O(log n + k): find an inside vertex via
      cyclicMaxOrPositive, walk the contiguous inside arc, take the two boundary
      crossings). Covered by halfplane_convex.cpp and triangle.cpp tests.
- [x] `unit/convex.cpp` — filled gap: intersection against another Convex (polygon /
      segment / point / disjoint cases).
      FIXED degenerate-result bug: `grahamScan`/`grahamScanExtended` did not dedupe
      coincident input points, so a single-point overlap came back as a degenerate
      (zero-length) Segment. Added `std::unique` after the sort; now a single contact
      point returns a Point. Covered by new graham.cpp duplicate-input tests and the
      convex/triangle corner/vertex-touch cases. (This also corrected
      `Triangle::intersection(Triangle)` since it delegates to the convex clip.)
- [x] `unit/polygon.cpp` — currently only intersects/interiorsIntersect/intersection;
      added boundaryContains, contains, interiorContains. (crosses/separates SKIPPED:
      `Polygon::separates(Polygon)` throws and crosses delegates to it — out of scope.)
- [x] `unit/disk.cpp` — currently only boundaryContains/contains/interiorContains/
      interiorsIntersect; added crosses, intersects, separates.

## Phase 4 — Distinct pairs tested only in `unit/<shape>.cpp` (relocate/create)

Create the integration file and migrate the cross-shape assertions. Grouped by first
shape in canonical order.

- [x] point_*: `point_orientedsegment`, `point_line`, `point_orientedline`,
      `point_ray`, `point_halfplane`, `point_rectangle`, `point_triangle`,
      `point_convex`, `point_polygon`, `point_disk` — created (both directions);
      `point_segment` augmented with the point-as-container case. Point-predicate
      assertions stripped from each shape's `unit/<shape>.cpp` (incl. the five
      multi-shape `unit/point.cpp` cases; point-self assertions folded into
      "Point shape predicates use coordinate equality and an empty boundary").
      Non-predicate point uses (`verticesContain`, `pointInside`, `orientation`,
      `collinear`, transform probes) and degenerate-disk/triangle blocks left in
      place. Green under g++ and clang++.
- [x] segment_*: `segment_orientedsegment`, `segment_orientedline`, `segment_halfplane`
- [x] orientedsegment_*: `orientedsegment_line`, `orientedsegment_orientedline`,
      `orientedsegment_ray`, `orientedsegment_halfplane`, `orientedsegment_rectangle`,
      `orientedsegment_triangle`, `orientedsegment_convex`, `orientedsegment_disk`
      All 8 files created; OrientedSegment assertions removed from unit/orientedsegment,
      halfplane, rectangle, triangle, convex, line, orientedline, ray. Full suite green.
- [x] line_*: `line_halfplane`
      Created tests/integration/line_halfplane.cpp; stripped Line↔Halfplane assertions
      from unit/halfplane.cpp and unit/line.cpp. Green under g++ and clang++.
- [x] orientedline_*: `orientedline_ray`, `orientedline_halfplane`,
      `orientedline_rectangle`, `orientedline_triangle`, `orientedline_convex`,
      `orientedline_disk`
      Created all 6 files; stripped OrientedLine assertions from unit/orientedline.cpp,
      unit/ray.cpp, unit/halfplane.cpp, unit/rectangle.cpp, unit/triangle.cpp,
      unit/convex.cpp, unit/disk.cpp. Green under g++ and clang++.
- [x] ray_*: `ray_halfplane`, `ray_rectangle`, `ray_triangle`
      Created all 3; stripped Ray↔Halfplane/Rectangle/Triangle assertions from
      unit/ray.cpp, unit/halfplane.cpp, unit/rectangle.cpp, unit/triangle.cpp.
- [x] halfplane_*: `halfplane_rectangle`, `halfplane_triangle`
      Created both; stripped Halfplane↔Rectangle/Triangle assertions from
      unit/halfplane.cpp, unit/rectangle.cpp, unit/triangle.cpp.
- [x] rectangle_*: `rectangle_triangle`, `rectangle_convex`, `rectangle_disk`
      Created all 3; stripped Rectangle↔Triangle/Convex/Disk assertions from
      unit/rectangle.cpp, unit/triangle.cpp, unit/convex.cpp.
- [x] triangle_*: `triangle_convex`, `triangle_disk`
      Created both; stripped Triangle↔Convex/Disk assertions from
      unit/triangle.cpp, unit/convex.cpp, unit/disk.cpp.
- [x] convex_*: `convex_disk`
      Created; stripped Convex↔Disk assertions from unit/convex.cpp, unit/disk.cpp.
      Full suite green under g++ and clang++.

## Phase 5 — Cleanup & verify

- [ ] Fold `separates_disk.cpp` into `triangle_disk.cpp` / `rectangle_disk.cpp`;
      delete `separates_disk.cpp` (or keep per Phase 0 decision).
- [ ] Move `segment_segment.cpp` content into `unit/segment.cpp`; delete the
      integration file (or keep per Phase 0 decision).
- [ ] Re-run the audit script to confirm: every distinct-pair method appears in its
      canonical integration file (both directions), and every self-pair method appears
      in its `unit/<shape>.cpp`.
- [ ] `sh tests/run_tests.sh` fully green under g++ and clang++.
- [ ] Update `doc/` (e.g. note the test-organization convention) if appropriate.

## Notes / risks

- `distance` is a construction and did not surface as a `const Other& ` binary method
  in the headers; confirm its signature and whether it belongs in this matrix before
  asserting coverage on it.
- The audit grep is name-level: it confirms a method name appears in a file, not that
  it is invoked on the specific pair. When migrating, verify each assertion really
  exercises the intended pair.
