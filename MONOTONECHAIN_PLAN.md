# Implementation Plan: `pgl::MonotoneChain` — an x-monotone polygonal line

Status: **Phases 1, 2, 3 and the Phase 4 separates/crosses follow-up COMPLETE (see §0) — remaining work is optional depth (see "Remaining")**
Target: header-only, C++20, exact-arithmetic-first, following the existing shape conventions.

---

## 0. Progress log (read this first when resuming on another machine)

This section is the single source of truth for continuation; the conversation
that produced each phase is not available across machines.

### Phase 1 — DONE (2026-07-06, not yet committed to git at time of writing)

All Phase 1 items from §6 are implemented and green. Files touched:

| file | what landed |
|---|---|
| `include/core/forward.hpp` | fwd decl, `shapeRank = 130`, `detail::is_monotone_chain_v`, `MonotoneChainConcept` |
| `include/shape/monotonechain.hpp` | **new** — class, deduction guides, ctors (range / flat coords / converting, `trusted` flag), sort+dedup `normalize()`, `operator[]`, `index()` (O(log n)), vertex `Iterator`, non-cyclic `BoundaryIterator` (n−1 edges), `edges()/orientedEdges()`, `insert(point)`/`insert(range)`, `isDegenerate` (`size()<2`), `isStrictlyMonotone`, `diameter`, `<=>`/`==`, `+=`/`-=`/`*=`/`/=` and free `+`/`-`/`*`/`/`, memoized-hash plumbing (mirrors Polygon's `hash_`/`hashUnset_`), declarations of everything defined in implementation files |
| `include/pgl.hpp` | include between `polygon.hpp` and `shape.hpp`; `EMonotoneChain` alias |
| `include/shape/shape.hpp` | now includes `monotonechain.hpp` (umbrella predecessor-chain rule); Shape variant **unchanged** (Phase 3) |
| `include/implementation/atxy.hpp` | `indexAtX`, `yAtX` (bottom-vertex rule at vertical runs), `isBelow`, `isAbove` (top of run via second `upper_bound`), all O(log n); predicates division-free via `orientationSign` |
| `include/implementation/bounding.hpp` | `bbox()` (cached) / `fbox()` |
| `include/implementation/measures.hpp` | `length<ApproximateNumber>()`, `lengthL1()`, `lengthLInf()`; no `area` on purpose |
| `include/implementation/transformations.hpp` | `rotated90/rotate90`, `scaledUp{X,Y}`/`scaleUp{X,Y}`, `scaledDown{X,Y}`/`scaleDown{X,Y}` (all renormalize via the untrusted ctor), plus a `MonotoneChainConcept` branch in the generic `Transformation * shape` |
| `include/implementation/io.hpp` | `operator<<` printing `MonotoneChain[(x,y),…]` |
| `include/core/hash.hpp` | `std::hash<MonotoneChain>` with the Polygon-style hash memo |
| `include/visualization/canvas.hpp` | `operator<<(MonotoneChain)` — decomposes into edge Segments (single vertex → Point) because the canvas element store is the `Shape` variant, which the chain has not joined yet; upgrade to a first-class `<polyline>` element in Phase 3 |
| `tests/unit/monotonechain.cpp` | **new** — 8 test cases / 127 assertions covering §5 item 1 |

Verified: `sh tests/run_tests.sh` fully green under **g++ and clang++**
(90/90 binaries, zero warnings from the new code). Quick re-check:
`sh tests/run_tests.sh monotonechain`.

Decisions taken during implementation (beyond what §1–§4 already said):

- Constructors `assert` the canonical invariant even on the `trusted` path
  (debug builds only).
- `insert` subtracts the active `translation_` so it composes with `+=`.
- Doc-promised semantics pinned by tests: `indexAtX`/`isBelow`/`isAbove`
  return the *same* index (smallest at the queried x); on-chain points engage
  both `isBelow` and `isAbove`; integer `yAtX` truncates (request `Rational`
  or `double` for exact/precise values).
- Discovered: `Polyline` today is only a stub alias `std::vector<PointType>`
  (`include/shape/point.hpp` ~line 1196), used by `Polygon::intersection`
  return types. The future real `Polyline` class must reconcile with that
  alias — noted for the eventual Polyline work, no impact on MonotoneChain.

### Phase 2 — DONE (2026-07-06, not yet committed to git at time of writing)

All Phase 2 items from §6 are implemented and green (§7's open questions were
already answered inline in this plan: vertical edges kept,
`vector<variant<Point, Segment>>` return, `Label = NoLabel` parameter). Files
touched:

| file | what landed |
|---|---|
| `include/shape/monotonechain.hpp` | declarations of the Phase 2 predicate surface: `contains` × {Point, Segment, OrientedSegment, Line, OrientedLine, Ray, Halfplane, Rectangle, Triangle, Convex, Polygon, Disk, EmptyShape}, `boundaryContains` (Point real; all other shapes inline `false`, EmptyShape `true`), `interiorContains` (Point/Segment/OrientedSegment real; Line/OrientedLine/Ray/Halfplane/Triangle degenerate reductions; Rectangle/Convex/Polygon/Disk inline `false`; EmptyShape `true`), `intersects` × {Point, Segment, OrientedSegment, MonotoneChain, EmptyShape}, `interiorsIntersect` × {Point, Segment, OrientedSegment, EmptyShape}, `intersection(MonotoneChain)`; private `edgeWindow` helper |
| `include/implementation/contains.hpp` | new MonotoneChain section. `contains(Point)` = `isBelow && isAbove` (the slice of the chain at any x is one vertical segment), O(log n). `contains(Segment)` = both endpoints on chain + every lex-between vertex collinear (exits at first bend), O(log n + k). Degenerate reductions mirror `Segment`'s (`isDegenerate()` + point/segment); `contains(Polygon)` is an **edge fold**, not `Segment`'s vertex fold — a bent chain can pass through all vertices without containing the edges |
| `include/implementation/boundarycontains.hpp` | `boundaryContains(Point)` = equals front or back vertex, O(1) |
| `include/implementation/interiorcontains.hpp` | Point = `!boundaryContains && contains`; Segment = `contains && !boundaryContains(endpoints)` (extreme vertices have degree 1, §1.5); degenerate reductions for Line/OrientedLine/Ray/Halfplane/Triangle |
| `include/implementation/intersects.hpp` | `intersects(Point)` = contains; `intersects(Segment)` = edge fold over `edgeWindow` (O(log n + k)); `intersects(MonotoneChain)` = merge sweep advancing by lex-smaller edge right endpoint, ties advance both (skipped pairs can only meet at the shared right endpoint, already found by the tested pair), plus O(1) disjoint-x-extent early-out |
| `include/implementation/interiorsintersect.hpp` | Segment case = open-edge fold over the window **plus** a non-extreme-vertex pass (`other.interiorContains(vertex)`) — a crossing exactly at a non-extreme chain vertex touches no open edge |
| `include/implementation/intersection.hpp` | `intersection<ResultNumber>(MonotoneChain)` merge sweep delegating to `Segment::intersection<ResultNumber>`; post-pass sorts pieces by lex-min (segments before points at ties, so points get absorbed), coalesces collinear touching segments, dedups repeated touch points. Overlaps across a bend deliberately stay two segments |
| `include/implementation/atxy.hpp` | private `edgeWindow(xlo, xhi)` (conservative-by-one on the left so touch-at-a-vertex cases stay inside; clamps the right end to the last edge). **Fix to Phase 1 code**: `indexAtX`/`yAtX`/`isBelow`/`isAbove` compared raw mixed coordinates, which breaks for `int` vs `Rational` (Rational only compares against Rational); all comparisons now cast both sides to `std::common_type_t`, the same pattern as `Point::operator<=>` |
| `tests/unit/monotonechain_point.cpp` | §5 file 2: vertex/edge-interior/vertical-edge containment, boundary-vs-interior split verified pointwise on a grid sweep, single-vertex chain (point is boundary, interior empty), 1e9-coordinate orientation exactness, Rational-point-on-int-chain, Rational and double chains, `p.intersects(chain)` forwarder symmetry |
| `tests/unit/monotonechain_segment.cpp` | §5 file 3: straight sub-path vs bend-spanning chords, collinear runs across multiple edges, vertical-edge sub-segments, windowed intersects incl. extreme-x boundary cases, interiorsIntersect incl. the crossing-at-non-extreme-vertex rule and endpoint-resting cases, mixed numeric types |
| `tests/unit/monotonechain_monotonechain.cpp` | §5 file 4: sweep intersects (crossing/touching/vertical-edge stacks/disjoint early-out), intersection pieces (lattice + Rational crossings, 4-edge vertex touch deduped, coalescing across collinear vertices, bend keeps two pieces, sorted multi-crossing through the vertical edge), and a 300-round randomized brute-force cross-check (boolean agreement both directions, soundness `a.contains(piece) && b.contains(piece)`, completeness of every brute piece via coverage, sortedness) |

Verified: `sh tests/run_tests.sh` fully green under **g++ and clang++**
(93/93 binaries; zero warnings from the new code).

Notes for Phase 3:

- The rank-based forwarders on the existing shapes already route
  `segment.intersects(chain)`, `segment.interiorsIntersect(chain)` and
  `point.intersects(chain)` to the chain's members (verified by tests) — the
  Phase 3 "symmetric overloads" chore only concerns the predicate families
  where lower shapes lack a generic rank forwarder.
- `chain.contains(chain)` / `interiorContains(chain)` / chain-vs-chain
  `interiorsIntersect` were deliberately left out of Phase 2 (plan scope);
  they slot into the Phase 3 roster sweep (contains(chain) is an edge fold
  over the other chain using `contains(Segment)`).
- `separates`/`crosses` and the `Shape` variant work remain untouched, per §6.

### Phase 3 — DONE (2026-07-07, not yet committed to git at time of writing)

**Placement change requested by the maintainer**: MonotoneChain now sits
**before Polygon**, not after — `shapeRank = 115` (between Convex 110 and
Polygon 120), pgl.hpp include order `convex → monotonechain → polygon → shape`,
umbrella chain updated (`monotonechain.hpp` includes `convex.hpp`,
`polygon.hpp` includes `monotonechain.hpp`, `shape.hpp` includes
`polygon.hpp`), and the `Shape` variant alternative list has MonotoneChain
between Convex and Polygon. Consequence: **Polygon owns the Polygon×chain
symmetric pairs**; the chain gained the standard rank forwarders
(intersects / interiorsIntersect / crosses / intersection / squaredDistance /
distanceL1 / distanceLInf, gated on `shapeRank > 115`) which route
`chain.op(polygon)` to Polygon's explicit chain overloads. All other shapes
reach the chain through their existing rank forwarders — verified by tests.

What landed, by file:

| area | details |
|---|---|
| `shape/monotonechain.hpp` | `get()` (Euclidean-mod vertex access, required by `Shape`); full predicate surface: `contains`/`interiorContains`/`boundaryContains` × chain; `intersects`/`interiorsIntersect` × Line/OrientedLine/Ray/Halfplane/Rectangle/Triangle/Convex/Disk/chain; `separates` (Point=false; Segment/OSeg/Ray via exact division-free component walk `separatesOneDimensional`; Line/OLine = intersects; **throw stubs** for Halfplane/Rectangle/Triangle/Disk/Convex/Polygon/chain — *all filled in Phase 4*); `crosses` × roster (`separates && other.separates`, throwing through staged separates — *now complete, Phase 4*); `Shape<OtherPoint>` visitor overloads for all predicate families; distances `squaredDistance`/`distanceL1`/`distanceLInf` × 11 shapes + self (edge-min folds, intersects → 0) + `double squaredDistance(Disk)` + `Shape`-arg L1/LInf re-dispatch (ResultNumber leading, per the overload-hazard rule); `intersection` × Point (optional) and Segment/OSeg/Line/OLine/Ray/Halfplane/Rectangle/Triangle/Convex (edge fold + shared `coalescePieces`, `vector<variant<Point,Segment>>` sorted and coalesced); private `edgeMinSquaredDistance/L1/LInf`, `edgeFoldIntersection`, `coalescePieces`, `separatesOneDimensional` |
| the 11 lower shape headers | four declarations each — `contains(chain)` (vertex fold: every shape below the chain is convex), `boundaryContains(chain)` (inline false for Segment/OSeg/Line/OLine/Ray; real for Point(false)/Halfplane(boundary-line vertex fold)/Rectangle/Triangle(asConvex)/Convex(**edge fold** — a chain may run along the hull through collinear vertices, so vertex counting à la the Polygon overloads is wrong)/Disk(degenerate reduction)), `interiorContains(chain)` (convex-interior vertex folds, matching each shape's Polygon overload shape), `separates(chain)` (all via new `detail::separatesChain` in predicates_helpers.hpp — exact component walk valid for any **convex** remover; Point uses `interiorContains`; the non-convex `Polygon::separates(chain)` was added in Phase 4) |
| `shape/polygon.hpp` + impls | Polygon×chain: `contains`/`boundaryContains`/`interiorContains` (**edge folds** over the chain, since a polygon is not convex — a vertex fold would wrongly accept a chain cutting across a notch, pinned by test), `intersects`, `interiorsIntersect` (via shared `detail::chainInteriorsIntersect`), `separates` (**throw stub** in Phase 3 — *implemented in Phase 4* via the arc-cut state machine, since the criterion is remover-agnostic), `crosses`, `squaredDistance`/`distanceL1`/`distanceLInf` (edgeMin folds) |
| `implementation/interiorsintersect.hpp` | new `detail::chainInteriorsIntersect(chain, other)` (open-edge fold + non-extreme-vertex pass) shared by the chain's 2-D/1-D overloads and Polygon's; chain×chain interiors via merge sweep + both vertex passes |
| `implementation/separates.hpp` | chain section + all cross-shape `separates(chain)` definitions; throw stubs carried an unreachable `return false;` because clang rejects throw-only constexpr bodies (*stubs removed in Phase 4*) |
| `core/forward.hpp`, `pgl.hpp`, `shape/shape.hpp` | rank 115, include reorder, variant membership + `is_shape_alternative` |
| `visualization/canvas.hpp` | first-class element: chain pushed as a `Shape` alternative, rendered as SVG `<polyline>` (never closed/filled; single vertex renders as a point circle); Phase 1's decompose-into-edges overload removed |
| tests | new pair files `monotonechain_{line,ray,halfplane,rectangle,triangle,convex,disk,polygon}.cpp`; `monotonechain_segment.cpp` extended with separates/crosses (incl. the crossing-exactly-at-a-shared-vertex case both directions) and segment/point intersection pieces; `shape.cpp` +2 cases (variant wrap, dispatch incl. hausdorff-throws, intersection re-wrap, transformations); `canvas.cpp` polyline case; `monotonechain.cpp` `get()` case |
| docs | `doc/raw/shapes.md` new "Monotone Chain" section (before Polygon) + 1-D index entry; `doc/raw/shape_methods.md` chain added to renormalizing shapes, L1/LInf pair list, hausdorff not-defined list; `doc/raw/todo.md` Monotone Polyline section removed (points to shapes.md), Polyline annotated "to be built on MonotoneChain", new "Partially Implemented" entry for the staged chain separates/crosses; regenerated `doc/*.md` via `python3 doc/raw/doxylink.py --write` |

Verified: `sh tests/run_tests.sh` fully green under **g++ and clang++**
(101/101 binaries; zero warnings from the new code).

Discoveries / notes:

- **Pre-existing, unrelated**: `Shape::intersection(const Shape&)` (both
  operands wrapped) does not compile for any point type, because the double
  visit instantiates `Rectangle::intersection(Polygon)`, whose result variant
  contains the `Polyline` stub alias (`std::vector<PointType>`), which
  `Shape` cannot wrap. Single-visit calls (concrete right operand) are fine
  and are what the tests use. Worth fixing when the real `Polyline` lands.
- Hausdorff distances deliberately skipped for the chain (exact parity with
  Polygon); `Shape` throws for those pairs, tested.
- `separates` where the chain is the remover of a 2-D region (or another
  chain) throws `std::logic_error` (documented in headers and todo.md): a
  chain genuinely can disconnect a region, but that needs region-side
  component analysis, out of scope here. `crosses` throws through it for the
  same pairs. **(Resolved in Phase 4 below — no longer throws.)**

### Phase 4 — separates/crosses follow-up — DONE (2026-07-07, not yet committed)

The staged `separates`/`crosses` throw stubs are all implemented; **no chain
`separates`/`crosses` pair throws any more**. Two directions, all exact and
integer-only:

| Direction | Method | Notes |
| --- | --- | --- |
| chain removes 2-D region | `MonotoneChain::separates(Halfplane / Rectangle / Triangle / Disk / Convex / Polygon)` | `separatesTwoDimensional` crosscut scan: an edge separates the region by itself, or the chain runs outside–interior–outside (scanned at vertices). A `bool OtherIsConvex` template flag; the `Polygon` overload passes `false`, adding a per-edge "excursion" test (`!interiorContains(edge)`) so an edge that leaves a non-convex interior between two interior vertices still counts. Halfplane has no fast edge term, so the vertex scan alone handles its pocket case. Rectangle/Triangle/Disk/Convex/Polygon gated by a `bbox` fast path. |
| chain removes chain | `MonotoneChain::separates(MonotoneChain)` | Arc-cut criterion: a chain's arc order **is** its lexicographic vertex order, so removal cuts the arc iff some ordered `a < b < c` on the target has `b` covered and `a`, `c` uncovered. Edge-scan state machine over `separates(edge)` / `contains(edge)` / `contains(vertex)` / `intersects(edge)`. |
| any shape removes chain | `Segment/OSeg/Line/OLine/Ray/Halfplane/Rectangle/Triangle/Disk/Convex::separates(chain)` (was `detail::separatesChain`, already convex-remover-complete) and **`Polygon::separates(chain)`** | The `Polygon` overload is the same arc-cut state machine as chain-removes-chain (the criterion is remover-agnostic; it only needs `Polygon`'s `separates(Segment)`/`contains`/`intersects`, which all exist). This was the last throw stub. |

`crosses` composes the two directions and is now complete for every pair
(including chain↔Polygon).

Verification: two independent oracles (an `intersection`-based coalesce and a
`contains`-only dense sampler, both separate code paths from the state
machine) agreed with the implementation over ~1.5 M random cases spanning
convex and non-convex polygons (L, comb, U, bracket, plus-sign, spiral,
S-shape), including the `bbox` fast path firing on every case.

Files touched: `implementation/separates.hpp` (region + chain + Polygon
definitions, `separatesTwoDimensional` gains the `OtherIsConvex` flag),
`shape/monotonechain.hpp` and `shape/polygon.hpp` (doc comments, throw stubs
removed), tests `monotonechain_{halfplane,rectangle,triangle,convex,disk,
polygon,monotonechain}.cpp` (throw-expectations replaced with real
separates/crosses checks + a randomized oracle cross-check), `doc/raw/{shapes,
todo}.md` regenerated.

### Remaining

- **Hausdorff distances for the chain** (`squaredHausdorffDistance` /
  `hausdorffDistance` / `…L1` / `…LInf`): deliberately skipped for exact parity
  with `Polygon`; `Shape` throws for those pairs (tested). Land them together
  with the `Polygon` versions.
- **`L1`/`LInf` distance between a chain and a `Disk`**: absent, matching the
  library-wide `Disk` L1/LInf gap (no closed form for point-to-circle in those
  metrics — see todo.md). `squaredDistance(Disk)` (Euclidean, `double`) works.
- **chain ∩ `Polygon` intersection**: not declared — blocked on `Polygon`
  having no `intersection` overload at all, not on the chain.
- **`Shape::intersection(const Shape&)` with both operands wrapped**:
  pre-existing, unrelated (see the note above); double-visit instantiates
  `Rectangle::intersection(Polygon)`, whose result variant holds the
  `Polyline` stub alias that `Shape` cannot wrap.
- **The real (non-monotone) `Polyline` class** built on `MonotoneChain`
  (see todo.md) — a later class, out of scope here.

This plan implements the class already promised in `doc/todo.md` under
*"Shapes Not Yet Implemented → Monotone Polyline"*: a polyline whose vertices
are strictly increasing in lexicographic `(x, y)` order, i.e. the graph of a
piecewise-linear (partial) function of `x`, possibly with vertical steps.
The general (non-monotone) `Polyline` from the same doc section is **out of
scope** here; it is a later class that will be *built on top of* this one
("stored as multiple x-monotone polylines"), which is a strong reason to get
`MonotoneChain` right first.

---

## 1. Semantics and design decisions

### 1.1 Definition

A `MonotoneChain<PointType, Label>` is the polygonal chain through vertices
`v0 < v1 < … < v(n-1)` where `<` is the lexicographic point order already used
throughout the library (compare `x`, tie-break on `y`). Consequences:

- **No duplicate vertices** (strict order).
- **Vertical edges are allowed** (consecutive vertices may share `x` with
  increasing `y`). This is exactly what `doc/todo.md` specifies ("if vertex u
  is before vertex v, then u compares inferior to v") and is what makes the
  class usable as a building block for `Polyline` and for upper/lower convex
  chains. So the object is *weakly* x-monotone: at one `x` value the chain may
  cover a vertical segment of `y` values.
- The chain is automatically **simple** (edges meet only at shared endpoints)
  — this is a theorem given the vertex order, not something to check at
  runtime. Document it.
- **Degenerate cases**: 0 vertices (empty chain — mirror `Polygon`, which
  permits an empty vertex set), 1 vertex (a point). `isDegenerate()` is true
  when `size() < 2` (all vertices equal is impossible by strictness, so the
  doc's "all vertices equal" collapses to `size() <= 1`).

### 1.2 Naming

Use **`MonotoneChain`** (decided) — the standard computational-geometry term
for an x-monotone polygonal chain, and honest about the weak-monotonicity
contract (vertical edges allowed). The name `PolyFunction` used in
`doc/todo.md` is superseded: it reads as "polynomial function" and
overpromises single-valuedness; update the name in `doc/raw/todo.md` when the
docs land (§6). Header: `include/shape/monotonechain.hpp`. Concept:
`MonotoneChainConcept`. Exact alias: `EMonotoneChain`. Trait:
`detail::is_monotone_chain_v`.

### 1.3 Normalization invariant (constructor contract)

Mirroring `Segment` / `Triangle` / `Polygon` normalization:

- The constructor accepts vertices in **any order**, sorts them
  lexicographically, and **removes consecutive duplicates**. This matches the
  doc: "constructed from any container of points, which will be sorted
  automatically".
- A `trusted = true` flag (same convention as `Convex` / `Polygon`) skips
  sorting/deduping when the caller guarantees canonical form. With
  `NDEBUG` off, `assert` the invariant in the trusted path.
- Because the canonical form is unique, `operator==` / `operator<=>` /
  `std::hash` are plain sequence comparisons — no orientation or rotation
  ambiguity, which makes this *simpler* than `Polygon`.

Note one subtlety the plan explicitly embraces: sorting means the input
`{(0,0), (2,0), (1,5)}` produces the chain `(0,0)-(1,5)-(2,0)`, i.e. the
constructor treats the input as a **point set**, not as a pre-linked chain.
That is the documented contract; a user who wants "reject non-monotone input"
can compare `size()` or use the trusted path. Document this loudly.

### 1.4 Storage

Follow the `Convex` / `Polygon` layout exactly:

```cpp
std::vector<PointType> points_;         // canonical (sorted, unique)
PointType translation_{};               // applied lazily on access
[[no_unique_address]] mutable detail::LabelStorage<LabelType> label_;
mutable std::optional<Rectangle<PointType>> bbox_;  // cached, reset on mutation
```

- The lazy `translation_` gives O(1) exact translation, like the other
  vector-backed shapes, and translation preserves the sort invariant.
- The x-extent of the bbox is free (`points_.front().x()`, `points_.back().x()`);
  only the y-extent needs the O(n) scan. Cache the full `Rectangle` the way
  `Polygon::bbox()` does.

### 1.5 What `MonotoneChain` is topologically

It is a **1-dimensional manifold with boundary**, and pgl already fixes the
convention on `Segment`: `Segment::boundaryContains(point)` tests the two
*endpoints* (`verticesContain`), and `interiorContains` is
`contains && !boundaryContains` (see `implementation/boundarycontains.hpp`
and `implementation/interiorcontains.hpp`). `MonotoneChain` follows the same
model:

- **Boundary** = the two extreme vertices, `points_.front()` and
  `points_.back()` (translated). `boundaryContains(Point)` is an O(1)
  two-point comparison; `boundaryContains(Segment)` is true only for a
  degenerate segment equal to an extreme vertex, etc.
- **Relative interior** = the chain minus the two extreme vertices.
  `interiorContains(Point)` = `contains(point) && point != front() && point
  != back()` — still O(log n). `interiorContains(Segment)` = `contains(other)
  && interiorContains(other.min()) && interiorContains(other.max())`: unlike
  `Segment`'s endpoint-only shortcut, keep the full `contains` term; the
  endpoint test alone is still sufficient once containment holds (an extreme
  vertex has degree 1, so a contained segment cannot pass *through* it), but
  containment itself is not implied by the endpoints for a bent chain.
- Degenerate cases mirror degenerate `Segment`: a single-vertex chain has that
  point as boundary and empty interior; the empty chain contains only
  `EmptyShape`.
- `interiorsIntersect(X)` for 2-D `X` = does the chain's relative interior
  meet the *open* region of `X`; for 1-D `X` it is the transversal/overlap
  test — mirror the `Segment`-vs-`X` implementations edge-by-edge, minding
  that chain vertices *between* edges are interior points of the chain (a
  crossing exactly at a non-extreme vertex counts).
- It cannot contain any 2-D shape or any unbounded shape; those overloads are
  constant `false`.
- `separates` follows the same "does removing A disconnect B" contract as
  other 1-D shapes; a chain *can* separate a segment/line/ray/rectangle etc.
  (it can cut through them), so these are meaningful but can be staged later
  (see Phasing).

---

## 2. Public API

### 2.1 Class skeleton (`include/shape/monotonechain.hpp`)

Declarations only, per the "shape headers declare, implementation headers
define" rule. Sketch:

```cpp
template <class PointType_ = Point<>, class TLabel = NoLabel>
struct MonotoneChain {
    using PointType  = PointType_;
    using NumberType = PointType::NumberType;
    using LabelType  = TLabel;

    // Constructors
    constexpr MonotoneChain() = default;                       // empty chain
    template <std::ranges::input_range Range> requires ...
    constexpr explicit MonotoneChain(Range&& pts, bool trusted = false);
    constexpr explicit MonotoneChain(std::initializer_list<NumberType> coords,
                                    bool trusted = false);    // (x0,y0,x1,y1,…)
    template <PointConcept OtherPointType, class OtherLabel> requires ...
    constexpr MonotoneChain(const MonotoneChain<OtherPointType, OtherLabel>&);

    // Deduction guides mirroring Polygon's (range, flat coord list).

    // Vertex access — same surface as Polygon minus cyclic get():
    constexpr const PointType operator[](std::size_t) const; // translation applied
    constexpr std::size_t size() const;
    constexpr bool empty() const;
    constexpr std::vector<PointType> vertices() const;
    constexpr auto begin/cbegin/end/cend() const;             // translated iterator
    constexpr std::ptrdiff_t index(const PointType&) const;   // O(log n) here!

    // Edges: n-1 edges, NOT cyclic (no wraparound edge) — this is the key
    // structural difference from Polygon's edge iterators.
    constexpr std::vector<Segment<PointType>> edges() const;
    constexpr std::vector<OrientedSegment<PointType>> orientedEdges() const;
    constexpr EdgeIterator edgesBegin()/edgesEnd() const;     // size()-1 edges
    constexpr OrientedEdgeIterator orientedEdgesBegin()/…End() const;

    // Structure queries
    constexpr bool isDegenerate() const;      // size() < 2
    constexpr bool isStrictlyMonotone() const;// no vertical edge, true function
    constexpr Segment<PointType> diameter() const;  // via Convex, like Polygon

    // Mutation (doc-mandated)
    constexpr void insert(const PointType& p);            // keep sorted; no-op if present
    template <std::ranges::input_range Range>
    constexpr void insert(Range&& pts);                   // bulk: append+inplace_merge
    // Both reset bbox_ cache.

    // Monotone queries (the reason this class exists)
    template <class OtherNumber>
    constexpr std::optional<std::size_t> indexAtX(const OtherNumber& x) const;
    template <class ResultNumber = NumberType, class OtherNumber>
    constexpr std::optional<ResultNumber> yAtX(const OtherNumber& x) const;
    template <PointConcept OtherPoint>
    constexpr std::optional<std::size_t> isBelow(const OtherPoint& p) const;
    template <PointConcept OtherPoint>
    constexpr std::optional<std::size_t> isAbove(const OtherPoint& p) const;

    // Measures
    template <std::floating_point ResultNumber = double>
    ResultNumber length() const;                          // sum of edge lengths
    constexpr auto squaredLength() const = delete;        // not additive; omit
    constexpr const Rectangle<PointType>& bbox() const;   // cached
    template <std::floating_point ResultNumber = double>
    constexpr Rectangle<Point<ResultNumber>> fbox() const;

    // Predicates (declared here, defined in implementation/ — see §3)
    // contains / boundaryContains / interiorContains / intersects /
    // interiorsIntersect / crosses / separates × the shape roster,
    // plus EmptyShape constants and the shapeRank-based crosses fallback.

    // Distances (Phase 3): squaredDistance / distanceL1 / distanceLInf ×
    // Point, Segment, Line, Ray, … — each is min over edges, mirroring
    // Polygon's edge-fold implementations.

    // Comparison / value semantics
    constexpr auto operator<=>(const MonotoneChain&) const;  // sequence compare
    constexpr bool operator==(const MonotoneChain&) const;
    template <class A = LabelType> requires(detail::has_label_v<A>)
    constexpr A& label() const;
};
```

### 2.2 Contracts for the monotone queries (exact semantics, worth pinning down now)

All four run in **O(log n)** via `std::ranges::lower_bound` on `x` (compare
against stored `points_[i].x() + translation_.x()` — search untranslated and
adjust, to avoid per-comparison additions).

- `indexAtX(x) -> optional<size_t>`: empty iff `x < front().x() || x > back().x()`
  (or the chain is empty). Otherwise, per the doc: the smallest index `i` such
  that `points[i].x() == x`, or, when no vertex has that x, the unique `i` with
  `points[i].x() < x < points[i+1].x()`. For a vertical edge at `x` this yields
  the **bottom vertex** of that edge (smallest index rule) — deterministic.
- `yAtX<Result>(x) -> optional<Result>`: empty when `x` is outside the
  x-extent. When `x` lands strictly inside edge `(v_i, v_{i+1})`, delegate to
  `Segment::yAtX<Result>` (already exists in `implementation/atxy.hpp`, handles
  endpoints exactly and does the one division). When `x` hits a **vertical
  edge**, return the y of the *lower* endpoint and document that
  `isStrictlyMonotone()` is the precondition for `yAtX` to be the full
  function value; the pair (`yAtX`, vertical-edge-bottom rule) keeps the
  result well-defined without a variant return. Same `ResultNumber`-template
  pattern as `Segment::yAtX` (division ⇒ user opts into `Rational`/`double`).
- `isBelow(p)`: "a ray shot **down** from `p` hits the chain". Implement
  exactly: find the edge/vertex at `p.x()` via `indexAtX`; empty optional if
  none; otherwise compare using `orientationSign(v_i, v_{i+1}, p)` — **no
  division**, stays exact for `int`. Returns the index per the doc contract.
  Vertical edge at `p.x()`: the ray down from `p` hits it iff
  `p.y() >= bottom.y()` — handle explicitly.
- `isAbove(p)`: symmetric (ray up), same index contract.

Note `isBelow`/`isAbove` are *not* complementary: both are true when the point
is **on** the chain, both false when `p.x()` is outside the x-extent. State
this in the doc comments and test it.

### 2.3 `insert`

- Single point: binary-search position; if a vertex with the same `(x,y)`
  exists, no-op. Insert into `points_` (subtracting `translation_`), reset
  `bbox_`. O(n) worst case (vector shift), O(log n) compare cost — document.
  Appending at either end (the common "extend the function" use) is amortized
  O(1) comparisons + amortized O(1)/O(n) vector mechanics; fine.
- Range: append all, `std::sort` the tail, `std::inplace_merge`, `std::unique`.
  O((n+k)·log-ish) and simple; document complexity.
- Note `insert` changes indices — any cached index a caller holds is
  invalidated. (No iterator stability promises; value semantics as usual.)

---

## 3. Integration into the layered architecture (file-by-file)

Ordered exactly as the work should land. `pgl.hpp` include order matters:
`shape/monotonechain.hpp` must be included **after `shape/polygon.hpp`** (it
refers to segments and points, and its rank sits above Polygon's) and before
`shape/shape.hpp` if/when it joins the variant.

### 3.1 `include/core/forward.hpp`

- Forward-declare `template <class PointType, class Label = NoLabel> struct MonotoneChain;`
  with a doxygen brief ("Weakly x-monotone polyline stored by sorted vertices…").
- `detail::shapeRank<MonotoneChain<…>> = 130` (next free slot after
  `Polygon = 120`). This gives the `crosses`/`intersection` delegation a
  direction: **MonotoneChain owns every mixed overload** against existing
  shapes, and existing shape headers need only their `rank`-based generic
  fallback (they already have it), so *no existing shape header changes* are
  required for the symmetric calls to resolve.
- `detail::is_monotone_chain` / `is_monotone_chain_v` trait +
  `MonotoneChainConcept`, placed with the others.

### 3.2 `include/shape/monotonechain.hpp` (new)

The class skeleton of §2.1: declarations, deduction guides, inline trivial
members (accessors, comparison, size, isDegenerate, insert, iterators —
mirror which members `Polygon` defines inline vs. defers). Include
`"shape/polygon.hpp"` at the top to slot into the umbrella chain
(each header includes its predecessor in `pgl.hpp` order).

### 3.3 `include/pgl.hpp`

- Add `#include "shape/monotonechain.hpp"` between `shape/polygon.hpp` and
  `shape/shape.hpp`.
- Add `using EMonotoneChain = MonotoneChain<EPoint>;` to the exact-alias block.

### 3.4 `include/implementation/atxy.hpp`

Definitions of `yAtX` and `indexAtX` (the file's charter is exactly this
operation family). `xAtY` is **not** provided — the chain is generally not
y-monotone, so `xAtY` is multi-valued; omitting it is a deliberate contract
(the doc never promised it).

### 3.5 Predicates (`implementation/contains.hpp`, `boundarycontains.hpp`, `interiorcontains.hpp`, `intersects.hpp`, `interiorsintersect.hpp`, `separates.hpp`, `crosses.hpp`)

All defined as `MonotoneChain` members in the per-relation files, one section
per file, following the existing section-per-shape-pair layout. The payoff of
monotonicity is that the *point and segment* cases beat the generic O(n):

| operation | algorithm | complexity |
|---|---|---|
| `contains(Point)` | `indexAtX` + `orientationSign(v_i, v_{i+1}, p) == 0` (+ range check on the edge) | O(log n) |
| `contains(Segment)` | both endpoints on chain, on the **same or adjacent collinear** edges: locate both ends with `indexAtX`, then verify every intervening vertex is collinear with the segment (a chain contains a segment iff the segment is a sub-path with no bends). Bends bound the scan: exit at first non-collinear vertex. | O(log n) typical, O(k) for k spanned vertices worst case — document as O(log n + k) |
| `contains(OrientedSegment)` | delegate to the Segment logic | same |
| `contains(<any 2-D or unbounded shape>)`, `contains(Line/Ray/…)` | `false` constants (a bounded 1-D set contains no unbounded/2-D set); `contains(EmptyShape)` = `true` | O(1) |
| `boundaryContains(Point)` | equals `front()` or `back()` (the two extreme vertices), like `Segment::verticesContain` | O(1) |
| `boundaryContains(Segment/other 1-D)` | only degenerate shapes equal to an extreme vertex qualify; mirror `Segment`'s overload set | O(1) |
| `interiorContains(Point)` | `contains(point) && !boundaryContains(point)`, same formula as `Segment` | O(log n) |
| `interiorContains(Segment)` | `contains(other)` plus both segment endpoints not extreme vertices (§1.5) | O(log n + k) |
| `interiorContains(2-D / unbounded shape)` | `false`; `true` for `EmptyShape` and for degenerate shapes reducing to an interior point (mirror `OrientedSegment`'s degenerate-Line/Ray handling) | O(1)–O(log n) |
| `intersects(Point)` | = `contains(Point)` | O(log n) |
| `intersects(Segment/OrientedSegment)` | clip the segment's x-range onto the chain's, locate the two boundary indices by binary search, then test only the edges in that x-window | O(log n + k), k = edges overlapping the x-window; worst case O(n) |
| `intersects(Line/OrientedLine/Ray/Halfplane)` | fold over edges (`edge.intersects(other)`), early-out | O(n); Halfplane can early-out via bbox corner test |
| `intersects(Rectangle/Triangle/Convex/Polygon/Disk)` | fold over edges against the other shape + one `other.contains(vertex)` check is NOT needed (chain has no interior, but chain fully inside the other shape still intersects it — the edge-fold covers this since edges intersect the containing shape). | O(n · cost(edge op)) |
| `intersects(MonotoneChain)` | **merge sweep** over the two sorted vertex sequences: advance the pointer with the smaller next x, test only edge pairs whose x-ranges overlap | O(n + m) per the doc |
| `interiorsIntersect(X)` | relative interior of the chain vs. interior of `X`: edge-by-edge like `Segment` vs. `X`, treating non-extreme chain vertices as interior points (§1.5) | Phase 2 for Point/Segment, Phase 3 for the rest |
| `crosses` / `separates` | Phase 3 (see §6); the rank-130 generic fallback + explicit `throw` stubs in the interim, exactly like `Polygon`'s "not-yet-implemented predicate pairs (throw)" block | — |

Add the `Shape<PointType>` visitor overloads (`contains(const Shape&)` etc.)
only in the phase where `MonotoneChain` joins the variant (§3.9); until then the
`Shape`-facing overloads that *other* shapes route through their variant
visitor will simply not name `MonotoneChain`, which is fine.

### 3.6 `implementation/intersection.hpp`

- `MonotoneChain ∩ MonotoneChain` → the doc says `std::vector` of points via the
  O(n+m) merge sweep. **Refine the contract** (ambiguity must be typed):
  return `std::vector<std::variant<PointType, Segment<PointType>>>`, sorted by
  x, because two chains can overlap along collinear sub-segments. During the
  merge sweep, each overlapping edge pair delegates to the existing
  `Segment ∩ Segment` (already returns point-or-segment variant); coalesce
  adjacent collinear overlaps. Intersection coordinates require division ⇒
  template on `ResultNumber` following the established
  `intersection<pgl::Rational<…>>` pattern used by segment intersection.
- `MonotoneChain ∩ Segment/Line/Ray/…`: Phase 3. Declare-and-throw or simply
  omit until then (omission is cleaner: `Shape::intersection` detects support
  via `requires`).

### 3.7 Representation & misc.

- `implementation/io.hpp`: `operator<<` printing
  `MonotoneChain[(x1, y1),(x2, y2),…]`, following the `Polygon[…]` format.
- `implementation/transformations.hpp`: member set mirroring `Polygon`
  (`operator+`/`+=` point translation via `translation_`; `scaledUpX/Y`,
  `rotated90`, mirrored variants as applicable). **Key invariant care**:
  - translation and positive x/y scaling preserve monotonicity → trusted path;
  - `rotated90(k)` for odd `k` yields a y-monotone chain → re-normalize
    through the untrusted constructor (result is the chain on the same point
    set — document that odd rotations re-sort, or restrict to even `k`;
    recommendation: allow and re-sort, it is still a meaningful point-set op
    and matches the constructor's point-set semantics);
  - negative scale factors reverse order → re-normalize (reverse is O(n));
  - `Transformation * MonotoneChain` (in `core/transformation.hpp`'s apply
    machinery): map vertices, construct **untrusted** (general affine maps
    break monotonicity).
- `implementation/measures.hpp`: `length()` (sum of `edge.length()`,
  floating-point result like other lengths), centroid-of-vertices if desired
  (`verticesCentroid` only; there is no area centroid). No `area()` — do not
  define it (absence communicates better than zero).
- `implementation/bounding.hpp`: `bbox()` / `fbox()` with the x-extent
  shortcut + cached rectangle (§1.4).
- `core/hash.hpp`: `std::hash<pgl::MonotoneChain<PointType, Label>>` hashing the
  translated vertex sequence, copying the `Polygon` specialization.
- `visualization/canvas.hpp`: an `add(const MonotoneChain&)` overload drawing
  the open polyline (SVG `<polyline>`, not `<polygon>`); check how Canvas
  draws `Polygon` and reuse with fill disabled.

### 3.8 What deliberately does NOT change

- `duality.hpp` — no natural dual for a chain in the current framework; skip.
- Existing shape headers — no new member declarations needed thanks to the
  rank-based delegation (§3.1). Existing shapes' `intersects(MonotoneChain)`
  etc. resolve through *MonotoneChain's* members when users call
  `poly.intersects(x)`; the symmetric direction `x.intersects(poly)` is
  **Phase 3** (it requires adding overloads to every shape header, exactly the
  chore the rank system was built to sequence — verify how `Polygon` handled
  being called from `Triangle` and follow the same route).

### 3.9 `shape/shape.hpp` variant membership — **defer to Phase 3**

Adding `MonotoneChain<PointType>` to the `Shape` variant touches the
alternative list, `is_shape_alternative`, every visitor in `shape.hpp`, and
the throwing-pair matrices. It is mechanical but wide. Land the concrete class
first; variant membership is a separate PR with its own test sweep
(`tests/unit/shape.cpp` etc.).

---

## 4. Algorithms worth writing down before coding

### 4.1 Point location (`indexAtX`) — the shared primitive

```
if empty or x < front.x or x > back.x: return nullopt
i = lower_bound over points_ by x        // first index with points_[i].x >= x
if points_[i].x == x: return i           // smallest such index (lower_bound gives it)
else: return i - 1                       // interior of edge (i-1, i)
```
Everything else (`yAtX`, `isBelow`, `isAbove`, `contains(Point)`,
`intersects(Segment)` windowing) is a thin exact-arithmetic layer over this.
One subtle case: `x == back.x()` with a vertical last edge — lower_bound gives
the *bottom* vertex of that edge; contract says smallest index; consistent.

### 4.2 Chain–chain merge sweep (`intersects` / `intersection`)

Two pointers `i`, `j` over edges of `P` (n−1 edges) and `Q` (m−1 edges):

```
while i, j valid:
    if P.edge(i) and Q.edge(j) x-ranges overlap: test the pair (exact Segment ops)
    advance the edge whose right endpoint has smaller (x, y)   // lex tie-break
```
Correct because both edge sequences are sorted by x-interval; each pair tested
at most once; O(n+m) tests. For `intersection`, collect results in x-order and
coalesce adjacent collinear overlaps into single segments before returning.
Watch the vertical-edge tie-breaking (multiple edges of one chain at the same
x): advance by lexicographic right endpoint, and when x-ranges are degenerate
(vertical vs vertical at same x) test them all — bounded because vertices are
strictly ordered.

### 4.3 Exactness rules (non-negotiable, per CLAUDE.md)

- All predicates use `orientationSign` / integer comparisons only — no
  division anywhere in §3.5.
- Divisions appear only in `yAtX` and `intersection`, both templated on an
  explicit `ResultNumber` the caller chooses (`Rational`, `double`), mirroring
  `Segment::yAtX` and segment intersection.
- Overflow discipline: predicates on `int` coordinates go through the same
  promoted types `orientationSign` already uses; nothing new to invent.

---

## 5. Tests (`tests/unit/`)

Follow the existing naming scheme (per-shape file + pair files). New files:

1. **`monotonechain.cpp`** — construction & invariants:
   - sorting + dedup from shuffled input; `trusted` path; initializer-list of
     flat coords; deduction guides; empty & single-vertex chains;
   - `operator==`/`<=>`/hash consistency incl. translated-equal chains
     (`p + t == q` semantics like Polygon's translation-consistent equality);
   - `insert` (front/middle/back/duplicate/bulk), bbox cache invalidation;
   - `indexAtX`/`yAtX`/`isBelow`/`isAbove`: interior of edge, exactly at
     vertex, at a vertical edge (bottom-vertex rule), outside extent, on the
     chain (both isBelow and isAbove true), `yAtX<Rational<int>>` exactness;
   - `isDegenerate`, `isStrictlyMonotone`, `length`, `bbox`/`fbox`, io format;
   - transformations: translation exactness, odd `rotated90` re-sorts,
     negative scale re-normalizes, `Transformation * MonotoneChain`;
   - numeric types: `int`, `double`, `Rational<int>`, `Rational<BigInt>` (at
     minimum the construction+query smoke tests per type, like sibling files).
2. **`monotonechain_point.cpp`** — contains/intersects/boundaryContains/
   interiorContains vs points on vertices, edge interiors, vertical edges,
   just-off-chain (orientation ±1 at huge coordinates to catch overflow).
   Boundary/interior split per §1.5: extreme vertices are boundary (not
   interior); non-extreme vertices and edge interiors are interior; verify
   `contains == boundaryContains || interiorContains` pointwise, and the
   single-vertex chain (point is boundary, interior empty).
3. **`monotonechain_segment.cpp`** — contains (sub-path with/without bends,
   collinear-spanning-multiple-edges), intersects (windowed), degenerate
   segments, vertical edges vs vertical segments.
4. **`monotonechain_monotonechain.cpp`** — merge-sweep intersects/intersection:
   crossing chains, touching at a vertex, collinear overlap runs (coalescing),
   interleaved vertical edges, disjoint x-ranges (O(1) early-out), and a
   randomized cross-check: brute-force all-pairs edge intersection vs sweep
   result on random monotone chains (exact `Rational` coordinates).
5. Extend nothing else initially; pair files against Line/Ray/Convex/… arrive
   with Phase 3.

All tests must pass `sh tests/run_tests.sh` under both `g++` and `clang++`
with the default `-std=c++20 -Wall -Wextra -pedantic`.

Optionally add `tests/graphical/monotonechain.cpp` (manual build) drawing a
chain, a query point with its isBelow/isAbove verdicts, and a chain–chain
intersection — the repo convention for visual sanity checks.

---

## 6. Phasing

**Phase 1 — the class and monotone queries** — **DONE, see §0**
(self-contained, highest value):
forward.hpp entries, `shape/monotonechain.hpp`, pgl.hpp wiring, constructors +
normalization, accessors/iterators/edges, comparison/hash, `insert`,
`indexAtX`/`yAtX`/`isBelow`/`isAbove`, bbox/fbox, io, transformations, length,
canvas. Tests file 1. *Deliverable compiles and tests green with zero edits to
other shapes.*

**Phase 2 — predicates & chain–chain** (the doc-promised O(·) operations):
contains/boundaryContains/interiorContains/intersects/interiorsIntersect for
Point, Segment, OrientedSegment + constants for the trivial cases;
MonotoneChain×MonotoneChain intersects + intersection (merge sweep). Tests 2–4.

**Phase 3 — full roster integration** (mechanical breadth): remaining
intersects overloads (Line/Ray/Halfplane/Rectangle/Triangle/Convex/Polygon/
Disk), separates/crosses where meaningful (+ documented `throw` stubs like
Polygon's), symmetric overloads on the other shape headers, `Shape` variant
membership + visitors, distances (`squaredDistance`/`distanceL1`/`distanceLInf`
as edge-folds), Hausdorff variants if applicable. Each sub-block gets its pair
test file.

**Docs, updated in the phase that lands the feature** (edit `doc/raw/*.md`,
regenerate with `doc/raw/doxylink.py` — the top-level `doc/*.md` are
generated):
- `doc/raw/shapes.md`: new MonotoneChain section (definition, invariant,
  vertical-edge and point-set-constructor caveats);
- `doc/raw/shape_methods.md`: method matrix rows/columns;
- `doc/raw/todo.md`: move Monotone Polyline out of "Not Yet Implemented"
  (leave `Polyline` there, now annotated "to be built on MonotoneChain");
- Doxygen comments throughout, matching the house style (complexity notes,
  `@warning` on division-based members).

---


## 7. Open questions for the maintainer (answered before Phase 1)

1. **Vertical edges**: the doc's lexicographic contract admits them (this plan
   keeps them, with the bottom-vertex rule for `yAtX`).
2. **`intersection` return type**: doc says vector of points; but we will use
   `vector<variant<Point, Segment>>` for typed collinear overlaps.
3. **Label parameter**: yes (`Label = NoLabel`), matching every other
   shape.
