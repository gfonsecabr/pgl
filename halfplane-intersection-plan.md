# Plan: `pgl::HalfplaneIntersection` (shapeRank 130)

> **Status:** Phase 1 implemented (core shape, insert/redundancy/emptiness,
> predicates + intersection constructions vs Point/Segment/OrientedSegment/
> Line/OrientedLine/Ray/Halfplane, bbox/fbox/asConvex, transformations, io,
> hash, unit tests, docs). Phases 2–4 pending.
> Implementation notes vs the plan: the `sideOfBoundaryIntersection` predicate
> landed as `detail::vertexSide` / `detail::boundaryLinesDeterminant` in
> `shape/halfplaneintersection.hpp` (not `orientation.hpp`); the clip primitive
> is `clipLine` + `exitConstraint` (valley binary search over the angular arc);
> `separates(Halfplane)` uses the contact-component analysis validated against
> an exact rational oracle. Two pre-existing bugs were fixed along the way:
> `Line::operator<=>` (vertical lines compared non-reflexively) and
> `OrientedLine::operator<=>` (orientation ordering was asymmetric), plus
> `Halfplane::interiorContains(Halfplane)` now mirrors the fixed
> `contains(Halfplane)` for parallel nested half-planes.

A new shape representing the intersection of a finite set of closed halfplanes:
a convex region that — unlike `Convex` — may be **unbounded** (wedge, strip,
halfplane, whole plane), may be **empty**, and may have **non-integer vertices**
even when every stored halfplane has integer coordinates. Halfplanes can be
inserted at any time; redundant ones are discarded.

Decisions already made (with the user):

- **Empty is tracked, degenerate is allowed.** `insert` detects infeasibility and
  sets a sticky empty state (`isEmpty()`); predicates then behave like
  `EmptyShape`. Lower-dimensional regions (a line, ray, segment, or point arising
  from opposite halfplanes) remain representable and predicates stay correct.
- **Staged rollout, core first** (mirroring the Polyline rollout): Phase 1 is the
  data structure plus predicates against the linear shapes; area shapes, the
  `Shape` variant, canvas, and benchmarks come in later phases.

---

## 1. Representation and invariants

`include/shape/halfplaneintersection.hpp`

```c++
template <class PointType_ = Point<>, class TLabel = NoLabel>
struct HalfplaneIntersection {
    using PointType = PointType_;
    using NumberType = PointType::NumberType;
    using LabelType = TLabel;
    using HalfplaneType = Halfplane<PointType>;
  private:
    std::vector<HalfplaneType> halfplanes_;
    bool empty_ = false;              // sticky: set once insert() detects infeasibility
    [[no_unique_address]] mutable LabelType label_{};
    mutable std::size_t hash_ = hashUnset_;   // same memoized-hash scheme as Convex
};
```

Invariants enforced by every constructor and by `insert`:

1. **Sorted by boundary direction.** `halfplanes_` is sorted by the pseudo-angle
   of the oriented boundary direction `target() - source()`, CCW, starting from
   the canonical angular origin (the smallest direction in pseudo-angle order,
   i.e. the first direction ≥ +x axis). Comparison is exact via sign tests
   (`y`-halfplane split + cross product), no trigonometry.
2. **No redundant halfplane.** Every stored halfplane contributes an edge to the
   region (or to its lower-dimensional relative boundary).
3. **At most one halfplane per direction.** Parallel same-direction halfplanes
   keep only the more restrictive one.
4. **Empty state is canonical.** When `empty_` becomes true, `halfplanes_` is
   cleared, so all empty regions compare equal and hash equally.

Consequences:

- Consecutive halfplanes (cyclically, when the region is bounded) define the
  region's vertices in CCW order — vertices are *implicit* and generally
  rational.
- `n == 0 && !empty_` is the **whole plane** (the intersection of an empty
  family). The default constructor produces this; the docs must say so loudly,
  since `Convex{}` is the opposite (the empty set). An `isPlane()` /
  `isEmpty()` pair keeps the two special states queryable.
- The region is **bounded** iff `n >= 3` and every cyclic angular gap between
  consecutive boundary directions is `< π`. Boundedness is O(n) to compute
  from scratch, but gaps only change locally on insert, so track it (or the
  largest-gap candidate) incrementally; expose `isBounded()`.
- `isDegenerate()` = region has empty interior (line/ray/segment/point cases,
  detectable from antiparallel boundary pairs with zero separation).

### Exact primitive predicates (new `detail::` helpers)

- `angularLess(h1, h2)` — pseudo-angle comparator on boundary directions,
  integer-exact. Used by construction sort, `insert` binary search, and all
  angular range queries.
- `sideOfBoundaryIntersection(h1, h2, h3)` — sign of the position of
  `∂h1 ∩ ∂h2` relative to `∂h3`, **without constructing the rational vertex**:
  the sign of the 3×3 determinant of the three boundary-line coefficients,
  corrected by the sign of `cross(dir(h1), dir(h2))`. (This is the dual of
  `orientation` — three concurrent lines ⟺ collinear dual points; ties into
  `duality.hpp`.) Coefficients are degree ≤ 2 in coordinates, the determinant
  degree ~5, so use the `detail::promoted_number_t` promotion rules and add a
  `@warning` mirroring `Halfplane::operator<=>`'s single-promotion caveat.

These two predicates carry essentially the whole shape: redundancy tests,
emptiness tests, point location and clipping all reduce to them.

## 2. Core algorithms

### 2.1 `insert(halfplane)` — amortized O(log n)

Returns `bool`: `true` if the region changed, `false` if the halfplane was
discarded as redundant.

1. If `empty_`: discard (region stays empty), return `false`.
2. **Classify via two support queries** (§2.2) in the outward normal direction
   `a` of the new halfplane `h = {p : a·p ≤ b}`:
   - `sup_{p∈K} a·p ≤ b` (or attained at a boundary tie) → `h` is **redundant**;
     discard, return `false`.
   - `inf_{p∈K} a·p > b` → `K ∩ h = ∅`; set `empty_`, clear the vector,
     return `true`.
   - Unbounded in the queried direction short-circuits the respective case
     (never redundant / never empties on that side).
3. **Same-direction dedup**: binary search the angular slot; if a stored
   halfplane shares the direction, keep the more restrictive (already covered
   by step 2's redundancy test on one side; replace on the other).
4. **Insert and cascade**: place `h` at its angular position, then repeatedly
   delete the successor while it is redundant w.r.t. its new neighbors
   (`sideOfBoundaryIntersection(h, succ2, succ)` test) and symmetrically the
   predecessor. Each deletion is O(1) and paid for once — amortized O(log n)
   per insert. Handle the small cases `n ≤ 2` and antiparallel neighbors
   (gap ≥ π means the "vertex" doesn't exist and the neighbor can't be
   redundant) explicitly.

Use `std::vector` insertion despite O(n) element movement — consistent with the
library's storage choices; the *predicate* work is O(log n) amortized. Document
the complexity as O(n) worst-case movement, O(log n) comparisons.

### 2.2 Support (extreme-point) query — O(log n)

`extreme(direction d)` → the maximum of `d·p` over the region: either
"unbounded in `d`", or the index pair of consecutive halfplanes whose implicit
vertex attains it. Because the outward-normal angles of the region's edges are
sorted, the attaining vertex is found with a single angular `lower_bound` of
`d` among normal directions — pure integer comparisons. Unboundedness in `d`
falls out of the local angular-gap test.

Powers: insert classification, `isBounded`, `bbox` (extremes in ±x, ±y),
`intersects(Halfplane)`, `contains`-style tests of the region inside another
shape, and `diameter`-like future queries.

### 2.3 Point location — O(log n)

`contains(point)`, `interiorContains(point)`, `boundaryContains(point)`:
split the sorted vector into its two contiguous angular arcs — the **upper
chain** (boundary directions pointing leftward: region below the lines) and
**lower chain** (directions pointing rightward), verticals at the arc seams,
exactly analogous to `Convex::upperHull/lowerHull`. Each chain's breakpoints
are x-monotone, so binary search `p.x` against implicit rational breakpoint
x-coordinates (integer cross-multiplied comparisons), then a single side test
of `p` against the active boundary line per chain. Boundary vs interior falls
out of the side-test sign.

### 2.4 Clipping a linear shape — O(log n)

`detail` clip primitive: given a line/ray/segment parametrized as
`p(t) = origin + t·dir`, compute the parameter interval `K ∩ shape` by finding
the entry and exit crossings with the two chains via binary search (side tests
of implicit vertices against the query line — `sideOfBoundaryIntersection`).
Result endpoints are rational parameters.

Powers, for Segment/OrientedSegment/Line/OrientedLine/Ray:
- `intersects` (interval nonempty), `interiorsIntersect` (open interval),
- `contains` (interval = whole parameter range; for `contains(Segment)` the
  cheaper "both endpoints inside" suffices by convexity),
- `separates` / `crosses`,
- `intersection<ResultNumber>` constructions (typed
  `optional<variant<Point, Segment, Ray, Line>>` as appropriate per pair, with
  the usual `@warning` about division after casting to `ResultNumber`).

Also needed, cheap and binary-searchable:
- `contains(Line)`: only possible when **all** stored halfplanes are parallel
  to it (≤ 2 after dedup) and contain it.
- `contains(Ray)`: source containment (§2.3) + recession-cone test: some
  outward normal has positive dot with the ray direction iff a stored normal
  direction lies in the open half-circle centered on the direction — two
  angular binary searches.
- `intersects(Halfplane h)` / `contains(Halfplane)`: support queries in ±normal
  of `h`.

## 3. Public interface (Phase 1)

Constructors and deduction guides following house style:

- `HalfplaneIntersection()` — whole plane.
- From a range of halfplanes (+ `trusted` flag like `Convex`): untrusted path
  sorts by angle then runs one monotone redundancy pass + feasibility pass
  (O(n log n)); trusted path adopts the vector as-is.
- Converting constructor from `Halfplane`, `Rectangle`, `Triangle`, `Convex`
  (their edge halfplanes, trusted).
- Cross-point-type converting constructor + assignment (house pattern).

Accessors / queries:

- `size()`, `operator[]`, `get(cyclic)`, `begin/end/cbegin/cend`,
  `halfplanes()` (vector copy, like `Convex::vertices()`), `index(halfplane)`.
- `isEmpty()`, `isPlane()`, `isBounded()`, `isDegenerate()`.
- `vertexCount()`; `vertex<ResultNumber>(i)` (implicit boundary-line
  intersection; `@warning` division after casting, request `Rational` for
  exactness — the midpoint pattern); `vertices<ResultNumber>()`.
- `edge<ResultNumber>(i)` → `std::variant<Segment, Ray, Line>` over
  `Point<ResultNumber>` (typed ambiguity, per design principles).
- `bbox<ResultNumber = NumberType>()` → `Rectangle<Point<ResultNumber>>`;
  **throws `std::logic_error`** when unbounded or empty (precedent:
  `Shape::bbox` for unbounded alternatives). With integer `ResultNumber` the
  box is rounded **outward** (floor mins, ceil maxes) so it still encloses the
  region. `fbox<double>()` convenience like `Convex`.
- `asConvex<ResultNumber>()` — bounded regions only (throws otherwise).
- `pointInside<ResultNumber>()` + `pointInsideInteriorContainedIn` (needed by
  the predicate helpers' witness pattern).
- `operator==` / `operator<=>` (empty flag first, then element-wise — canonical
  rotation + geometric `Halfplane` equality make this region equality),
  `std::hash` specialization in `core/hash.hpp` (memoized like `Convex`).
- `label()` (house pattern, mutable metadata).
- Transformations: `operator+=/-=` (translate), `*=`, `/=`, `rotated90/rotate90`,
  `scaledUpX/...` — all preserve the sorted/non-redundant invariants up to a
  possible reversal/rotation of the angular order (re-canonicalize; negative
  scale factors reverse orientation — either handle or document like siblings).
- `operator<<` in `implementation/io.hpp`.
- EmptyShape overloads (contains/boundaryContains/interiorContains true,
  separates/intersects/crosses false) inline in the header, house pattern.

**No generic catch-alls**: every predicate pair gets an explicit definition
(project rule). Since rank 130 is the highest, all *symmetric* predicates
(`intersects`, `interiorsIntersect`, `crosses`, `intersection`, distances) on
existing shapes auto-forward here through their existing rank-guarded
forwarders — no edits to other shape headers needed for those in Phase 1.
Asymmetric predicates in the *other* direction (e.g.
`halfplane.contains(hpi)`) require declarations on the other shapes' headers —
deferred to Phase 2. Mind the memory note on `Shape<>`-arg overload template
parameters when Phase 3 arrives.

## 4. File-by-file changes (Phase 1)

| File | Change |
|---|---|
| `include/core/forward.hpp` | fwd decl; `shapeRank = 130`; `is_halfplane_intersection` trait; `HalfplaneIntersectionConcept` |
| `include/shape/halfplaneintersection.hpp` | **new** — class, invariants, insert, support/clip primitives' declarations, predicate member declarations |
| `include/pgl.hpp` | include after `shape/polygon.hpp`, before `shape/shape.hpp`; add `EHalfplaneIntersection` alias |
| `include/implementation/contains.hpp` | vs Point, Segment, OrientedSegment, Line, OrientedLine, Ray, Halfplane |
| `include/implementation/boundarycontains.hpp` | same pair set |
| `include/implementation/interiorcontains.hpp` | same pair set |
| `include/implementation/intersects.hpp` | same pair set |
| `include/implementation/interiorsintersect.hpp` | same pair set |
| `include/implementation/separates.hpp` | same pair set |
| `include/implementation/crosses.hpp` | same pair set |
| `include/implementation/intersection.hpp` | clip-based constructions vs the linear shapes + Point |
| `include/implementation/bounding.hpp` | `bbox` / `fbox` |
| `include/implementation/transformations.hpp` | translate/scale/rotate90 family |
| `include/implementation/io.hpp` | `operator<<` |
| `include/core/hash.hpp` | `std::hash` specialization |
| `include/implementation/predicates_helpers.hpp` or `orientation.hpp` | `angularLess`, `sideOfBoundaryIntersection` |
| `tests/unit/halfplaneintersection.cpp` | construction, insert/redundancy/dedup/empty detection, isBounded/isPlane, vertices/edges, bbox throw + outward rounding, asConvex, equality/hash, transformations, `Rational<int64_t>` + `ERational` instantiations |
| `tests/unit/halfplaneintersection_point.cpp` | contains/boundary/interior/intersects/intersection vs Point (bounded, wedge, strip, halfplane-like, plane, empty regions) |
| `tests/unit/halfplaneintersection_segment.cpp` | Segment + OrientedSegment pairs incl. clip constructions |
| `tests/unit/halfplaneintersection_line.cpp` | Line, OrientedLine, Ray, Halfplane pairs |
| `doc/raw/shapes.md` (+ regenerate `doc/shapes.md` via `doxylink.py --write`) | new shape section |

Test discipline: **no degenerate-input assertions** (zero-length segments etc.
remain UB); empty/degenerate *regions* are supported states and are tested.
Avoid `near`/`far` identifiers (MSVC). Must compile clean under g++ and clang++
with `-std=c++20 -Wall -Wextra -pedantic` (`sh tests/run_tests.sh`).

## 5. Later phases

**Phase 2 — area shapes and metrics.**
Predicates and `intersection` vs `Rectangle`, `Triangle`, `Convex`, `Polygon`,
`MonotoneChain`, `Polyline`, `Disk`; reverse-direction asymmetric predicates
added to those shapes' headers; `squaredDistance` / `distanceL1` /
`distanceLInf` for all pairs; `measures.hpp` (`twiceArea`/`area`/`centroid`,
throwing when unbounded). Natural typed results:
`HalfplaneIntersection ∩ Convex/Rectangle/Triangle/Halfplane →
HalfplaneIntersection` (stays closed under the operation — a selling point of
the shape). Per-pair unit tests mirroring the existing `<a>_<b>.cpp` layout.

**Phase 3 — Shape variant, canvas.**
Add to `shape.hpp` variant (watch MSVC C1128 `/bigobj` — memory note), variant
plumbing in each predicate aggregator, `canvas.hpp` drawing (clip the region to
the viewport rectangle first — the clip primitive gives this for free),
`tests/unit/shape.cpp` + `canvas.cpp` additions. Mind the
`Shape<>`-argument overload template-parameter hazard (memory note).

**Phase 4 — benchmarks and docs completion.**
Add to the benchmark shape-pair cube (`tests/benchmark/run_shapepairs.py`
shapes list), record a run when asked; complete `doc/raw/shape_methods.md`
tables and regenerate.

## 6. Open items to confirm during implementation

- Whether `angularLess` / `sideOfBoundaryIntersection` belong in
  `orientation.hpp` (they are atomic sign predicates) or in a new detail block
  in the shape header — decide by matching where `cyclicMax` ended up
  (`convex.hpp`) vs the file-per-operation rule.
- Exact `intersection` result-type menu per pair (e.g. region ∩ Line can be
  Point/Segment/Ray/Line — four-way variant) — finalize against
  `intersection.hpp` house conventions.
- Whether `insert` should also accept a range and other shapes
  (`insert(Convex)` = intersect with it) in Phase 1 or 2 — leaning Phase 2.
