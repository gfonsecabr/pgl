# Property harness

A randomized, shrinking property harness for pgl. It generates shapes, asserts
relations that must hold **for every shape pair**, and reduces any violation to a
minimal witness.

It exists because of an arithmetic problem. pgl has 18 shape alternatives and
seven predicates, each predicate defined per shape pair in its own file — so the
predicate layer alone is several hundred definitions that must all agree with
each other, and the pair matrix is 324 ordered pairs before degeneracies are
considered. No hand-written suite closes that; `sandbox/todo.md` records 61
mismatches found by hand-probing collapsed area shapes against twelve operands,
and hand-probing is exactly what does not scale to the rest.

The unit suite in `tests/unit` checks *what a case should answer*. This checks
*that the answers are consistent with each other*, which needs no expected value
and therefore costs nothing per case.

## Not part of CI

`tests/run_tests.sh` globs `tests/unit/*.cpp` and never looks here, and nothing
in this directory is wired into a GitHub workflow. That is deliberate: a
randomized search runs as long as you give it and finds what the seed leads it
to, and neither belongs in the path of every commit. Run it on demand — before a
release, after touching a predicate or the boolean engine, or when a bug smells
like it has siblings.

## Running it

```bash
sh tests/property/run.sh                        # default: seed 1, 2000 cases, checked against the baseline
sh tests/property/run.sh --cases 50000          # longer search (about 45s; the build dominates a short run)
sh tests/property/run.sh --seed 7 --cases 20000 # a different corner of the space
sh tests/property/run.sh --group predicates     # one group
sh tests/property/run.sh --property crosses     # one property, by substring
sh tests/property/run.sh --generator Disk       # only draws that involve a Disk
sh tests/property/run.sh --list                 # every property and generator
sh tests/property/run.sh --verbose              # per-property held/skipped/violated counts
CXX=clang++ sh tests/property/run.sh            # both compilers work; the harness is C++20
```

Everything after `run.sh` goes to the binary; `--help` lists the rest. The build
lands in `build/property/property`, so you can re-run it directly and skip the
one-time compile.

**Exit status** is 0 when nothing failed that the baseline did not already know
about, and 1 otherwise. That is the whole point of the baseline — see below.

**Reproducing a witness.** Every reported failure prints the recipe for its
operands and a `reproduce:` line. Re-run with `--no-shrink --no-catch-crashes` to
get the case as drawn and let an assertion abort where you can attach a debugger.

## How a case works

Everything the harness feeds a property comes from one `std::vector<Point>` of
small lattice coordinates plus the index of the **generator** that consumes it:

```
Operand{ generator = "Polygon.ring", points = {(0,0), (0,-1), (1,0)} }
```

That indirection is what makes shrinking possible. A witness is not an opaque
shape but the handful of integers it was built from, so the shrinker perturbs
*coordinates*, rebuilds through the generator, and asks the property again —
knowing nothing about the shape family involved. It accepts any reduction that
still fails and strictly decreases `(point count, coordinate magnitude, distinct
values)`, which is bounded below, so it always terminates. Witnesses come out at
two or three vertices on a line, which is usually enough to see the bug by
inspection.

Coordinates are drawn from a small box (`--grid`, default ±6), and 30% of
operands from a box a third that size so that one shape lands inside the other
often enough to exercise the containment properties. Small coordinates are the
point, not a limitation: on a 13×13 grid, collinear triples, coincident vertices,
shared edges and touching boundaries all arrive within a few hundred draws, and
those are the cases where the contracts are subtle enough to get wrong.

**Degenerate versus undefined.** Generators reject `isUndefined()` shapes, the
one state in pgl that carries no contract — asserting anything about it would be
asserting about unspecified behaviour. Merely *degenerate* shapes are generated
on purpose and never rejected: a collinear `Triangle`, a zero-radius `Disk`, a
zero-length `Segment`, a one-vertex `MonotoneChain`, a collapsed `Rectangle`.
They are defined, they have contracted limit-case behaviour, and they are where
most of what follows was found.

**Crashes are findings.** A failed `assert` inside the library calls `abort()`,
which would end the run at the first one — so each evaluation runs under a
`sigsetjmp` target and `SIGABRT`/`SIGSEGV`/`SIGFPE`/`SIGBUS` are caught,
reported like any other failure, and shrunk to a minimal crashing witness. See
`crash.hpp` for what that costs (leaked allocations from the abandoned frame, and
why that is safe here). `--no-catch-crashes` turns it off.

## What is checked

`--list` prints the current set: 82 properties in sixteen groups, drawn from 26
generators.

| Group | What it asserts |
| --- | --- |
| `predicates` | The algebra of the seven predicates, from their set-theoretic definitions: symmetry of `intersects` / `interiorsIntersect` / `crosses`; `crosses` is exactly mutual `separates`; `interiorContains` and `boundaryContains` imply `contains`; `contains` implies `intersects` and forbids `separates`; boundary and interior are exclusive; separation implies intersection for a connected operand; the empty-set clause of all seven; and the self-pair, including `A ⊆ ∂A` exactly when `A` has no interior. |
| `metric` | The distance families against the predicates and each other: all three distances vanish exactly when `intersects` is true; symmetry; `L∞ ≤ L2 ≤ L1 ≤ 2L∞`; containment forces distance zero; the Hausdorff distance is symmetric, dominates the nearest-point distance, and vanishes exactly on mutual containment. |
| `value` | Value semantics: a copy is equal, orders equal and hashes equal; equal shapes hash equally; `<`, `==`, `>` are a total order. Plus **normalization** — rebuilding a shape from its own defining points in a different order gives an equal shape, for each class that documents one (`Segment`, `Line`, `Rectangle`, all six vertex permutations of `Triangle` and of a three-point `Disk`, reordered input for `Convex` and `MonotoneChain`, any ring rotation for `Polygon`). |
| `invariance` | All seven predicates survive translation, quarter turns, integer scaling, negation and an integer shear; squared distance survives isometries and scales as `s²`. Any *one* correct answer becomes a test of a whole orbit, which is what finds axis-aligned shortcuts that do not generalize. |
| `bounding` | A shape lies in its own bbox; intersecting shapes have intersecting bboxes; containment carries over to tight bboxes. |
| `intersection` | `intersection` is non-empty exactly when `intersects` is true, and its result lies inside both operands. |
| `minkowski` | The Minkowski sum of two convex shapes contains every vertex sum and does not depend on the operand order; and eroding the sum by what dilated it gives back at least the original, `(A ⊕ B) ⊖ B ⊇ A`. |
| `boolean` | Exact area identities over the regularized operations: `\|A∪B\| + \|A∩B\| = \|A\| + \|B\|`, `\|A∖B\| + \|A∩B\| = \|A\|`, `\|A△B\| = \|A∪B\| − \|A∩B\|`; `A△B = (A∖B) ∪ (B∖A)` as sets; commutativity; the lattice order; `A∖B` shares no interior with `B`; and the self-operations collapse. Computed in `Rational<BigInt>`, so these are equations, not tolerances. Plus a **pointwise** oracle: over the lattice of the two bounding boxes, a point interior to both operands is in `A∩B` and `A∪B`, a point interior to `A∩B` is in both, a point interior to `A` and outside `B` is in `A∖B` and `A△B`, and a point interior to `A∖B` is in `A` and not interior to `B`. The area identities are sums and forgive two cells swapped between results; membership does not, and names the cell. |
| `measure` | Area, length and centroid: area is never negative, `twiceArea` is twice `area`, containment orders area, area survives an isometry and scales as `s²`, length scales as `s`, `L∞ ≤ L2 ≤ L1 ≤ 2L∞` on a curve, a convex shape's centroid lies in it, and the centroid commutes with a translation. The only group that asks the library for a *number* about one shape. |
| `closest` | The `closestPoints` / `closestSegments` witnesses against the distance they witness: the pair exists exactly when the shapes are disjoint, each point lies on the shape it came from, the two points are exactly `squaredDistance` apart, and the named elements lie on the operands and carry the points. |
| `curve` | `yAtX` and `xAtY` as an equivalence: what comes back lies on the curve, and nothing comes back only when the curve meets that vertical or horizontal line nowhere. A one-sided check would pass for an implementation that answered `nullopt` everywhere. |
| `hull`, `sweep`, `triangulation`, `arrangement`, `sorting` | Whole algorithms against their oracles: the hull contains its input, is convex, is idempotent and invents no vertices; Bentley–Ottmann matches brute force; a triangulated polygon's triangle areas sum to its own and number `n−2`; the arrangement's DCEL satisfies `twin∘twin = id`, head-to-tail `next`, and one face per cycle; `hilbertSort` permutes; `sortAround` traces a simple ring. The `bounding` group also asks that a shape's convex hull encloses it and adds nothing for a shape that is already convex. |

The `invariance` group additionally checks that two maps applied in turn equal
their composite, and that the identity map moves nothing — the composite is one
matrix multiplication while the stepwise form rebuilds the shape in between, and
rebuilding is where each class re-establishes its normalization.

Three design notes worth knowing before adding to this:

- **Skips are tracked separately from passes.** Most of these relations are
  conditional, and a property that quietly answers "held" for inputs it cannot
  judge is indistinguishable from one that works. The runner reports any property
  that skipped *every* case under `VACUOUS`, which is the failure mode a property
  harness is most prone to. Keep that section empty.
- **A starved tag steers the draw, it does not only filter it.** `requiredTags`
  is a filter: both operands are drawn from the generator table and a property
  runs on the draws that satisfy its mask. For a mask carried by one generator in
  twenty-six that is 1/676 of the pairs, and the `minkowski` group was running
  four cases in three thousand — counted as passing, testing nothing. So a mask
  whose generators are under a quarter of the table gets a *pool*, and a quarter
  of the cases draw both operands from one. Only starved masks, and only a
  quarter of the cases, because every draw spent on a pool is one not spent
  exploring the whole matrix, and the alternatives carrying the fewest tags
  (`Disk`, `MonotoneChain`) are the ones a pool is least likely to contain.
- **Domain and value are separate questions.** Several operations are partial over
  the pair matrix and say so by throwing `std::logic_error`. The value properties
  skip those pairs; the *domains* are checked by their own properties
  (`boolean/operations-share-a-domain`,
  `metric/distance-families-share-a-domain`), which compare sibling operations
  and report a pair that one supports and another does not. Without that split,
  one missing overload shows up as six unrelated failures.

## The baseline

`known_failures.txt` is the line between "already understood" and "new". A
matched entry is still reported, but does not fail the run; an unmatched one
does. Without it the harness is unusable on a library with open bugs, because
every run drowns in the same known findings.

An entry is `group/property [AlternativeA,AlternativeB]`, or `group/property [*]`
to accept every pair of that property. The wildcard is for a root cause whose
reach is an accident of the draw — one missing overload fails the same property
across dozens of pairs, and a fresh seed finds pairs an earlier one missed. With
exact pairs only, every new seed reported a handful of "new" signatures that were
not new problems, and still 1–7 new per seed. Collapsing the twelve properties
whose failures were all traced to one cause brings the shipped list to 41 entries
over 193 observed signatures, and fresh seeds that contributed nothing to it come
out clean at 20000 cases each. That is what makes the exit code mean something.
The tail is long, though: roughly one seed in five still reaches a new *pair* of an
already-known cause, so triage means comparing the witness against the list above
before assuming it is new.

```bash
sh tests/property/run.sh --cases 20000 --update-baseline   # rewrite from one run
```

That writes exact pairs only, so the wildcards are a maintainer's judgement to
reapply by hand. The shipped list is the union over seeds 1–24 at 20000 cases.

## What it currently finds

Every item below was reduced by the shrinker and then re-confirmed with a
standalone program, so each is a real reachable case and not a harness artefact.

### Fixed

Eight of the original findings have been fixed in the library. The harness went
from 590 signatures to 193, and the baseline from 81 entries to 42. The properties
that caught them are still in place, so a regression comes back as a failure rather
than as a memory.

The three below were found on 2026-09-16 by the groups added that day
(`measure`, `closest`, `curve`, the pointwise boolean oracle, the convex-hull and
Minkowski-erosion properties, and transformation composition). Because this
harness is out of CI, each one's regression is held by a unit test instead, and
each of those was verified to fail against the unfixed library:

- **`Convex::centroid` dropped a deferred translation for a one-vertex hull.**
  `Convex` does not move its vertices when translated; it accumulates the offset
  in `translation_` and every branch of `centroid` adds it back — except the
  `points_.size() == 1` branch, which returned the stored vertex bare. So a
  degenerate hull reported the centroid it had *before* the move:
  `Convex[(-1,0)] += (3,-5)` printed as `Convex[(2,-5)]` and answered `(-1,0)`.
  `pointInside`, the sibling directly below it in the same file, carries the
  offset in its own one-vertex branch and was right all along. Caught by
  `measure/centroid-follows-translation`; regression in `tests/unit/convex.cpp`.

- **`PolygonWithHoles::interiorsIntersect(Point)` was a constant `false`.** Every
  other shape in the library spells this `return interiorContains(other);` under
  the comment "a point's interior is the point itself" — `Rectangle`, `Triangle`,
  `Convex`, `Polygon`, `Disk`, `HalfplaneIntersection`, and `Point` itself.
  `PolygonWithHoles` alone returned `false`, justified by a comment claiming it
  matched `Polygon`, which it did not; `PolygonSet` inherited the wrong answer by
  delegating to its components. `doc/raw/shape_methods.md` settles which reading
  is meant: it defines `A°` as the *relative* interior, and a point is a manifold
  with empty boundary, so `A° = A` for it. The bug also broke
  `interiorContains ⟹ interiorsIntersect`, which the existing property checks
  only for `kRegion` pairs — a `Point` operand was never asked. Caught by
  `predicates/interior-witness-meets-interiors`; the two unit tests that asserted
  the old answer (`polygonwithholes_point.cpp`, `polygonset_point.cpp`) now
  assert the documented one.

- **`separates` read a degenerate operand as two pieces of itself.**
  `detail::cellSeparates` appended *both* operands' cut segments before asking
  whether there was anything to cut, so its `cuts.empty()` shortcut — "a target
  with no extent holds at most one component" — never fired when the target had
  no extent but the remover did. The arrangement then got built out of the
  region's edges alone, and its cells describe the plane rather than the target,
  so a one-vertex `Polyline` or `MonotoneChain` sitting in a pinched hole was
  read as two components of itself. `B ∖ A` for a single point is a point or
  nothing, so `separates` must be false; the bare `Point` overload answered
  correctly, and the answer flipped with the orientation of the axes. The
  emptiness test now runs on the target's cuts alone. Caught by
  `invariance/rotation-preserves-predicates` and `shear-preserves-predicates`;
  regression in `tests/unit/polygonwithholes_separates.cpp`.

The 2026-09-16 pair below is fixed too, and because this harness is out of CI the
regression is held by `tests/unit/ray.cpp` and `tests/unit/line_ray.cpp` instead —
both verified to fail against the unfixed library.

- **A `Ray` was read as the segment between its two defining points.** Three
  overloads asked only where `source()` and `target()` fall, though a ray carries
  on past its target forever. `Ray::contains(Ray)` was
  `contains(other.source()) && contains(other.target())`, so two *opposite* rays
  on one line each contained the other while sharing only the segment between
  them; `Ray::interiorContains(Ray)` repeated it and was masked until `contains`
  was right. `Line::separates(Ray)` compared the sides of the two defining points
  and so missed every crossing that lies beyond the target — for
  `Ray((0,0),(5,0))` the lines `x=1..5` separated and `x=6,7` did not, though all
  of them cut the ray in two. `Ray::separates(Ray)` made the same comparison in
  both halves of its symmetric test, so one fixed pair of crossing rays answered
  differently depending on how far out each through-point had been placed: of
  twenty spellings of a single crossing, ten said `separates=crosses=false`.
  `Line::separates(Ray)` now reads `intersects(other) && !contains(other.source())`
  like every other `separates(Ray)` overload in the file — `Segment`, `Triangle`
  and `Rectangle` were already written that way, and `Ray::separates(Segment)`
  even carries the comment "anywhere from the source onward, not just up to the
  target". `contains` now asks for a shared supporting line and a shared
  direction, spelled as the same lexicographic order `containsCollinear` walks.
  This was also the root of the unbounded-`HalfplaneIntersection` witness that
  `invariance/negation-preserves-predicates` reported: two opposite half-planes
  pin such a region to a line and a third cuts it to a ray, and `crosses` then
  came out false as drawn and true after negating the same geometry.

- **`distanceL1` / `distanceLInf` aborted on a one-vertex `MonotoneChain` or
  `Polyline`.** `edgeMinDistanceL1` asserted `size() >= 2`, but a one-vertex chain
  is *defined* — `isUndefined()` is `empty()` alone, and `isPoint()` is true — so
  the assertion was reachable from the public API, and under `NDEBUG` it read
  `boundaryAt<false>(0)` on a chain with no edge. `MonotoneChain`'s *squared*
  distance already had the single-vertex branch; the L1 and LInf siblings and all
  three `Polyline` versions now carry the same one.
- **`EmptyShape::contains(X)` was false for an `X` that is empty but is not an
  `EmptyShape`** — a default `Rectangle`, an empty `HalfplaneIntersection`, and so
  on, where `∅ ⊇ ∅` must hold. The generic overloads now ask `X.empty()` where the
  type has it; the shapes that can never be empty keep the constant-false branch.
- **`intersection` with an empty operand returned the other operand.** The empty
  rectangle carries no edge, so `HalfplaneIntersection::intersection(Rectangle)`
  inserted no constraint and handed back the whole region. It now forces emptiness
  the way the sibling `Convex` and half-plane-intersection overloads already did.
- **`difference` and `symmetricDifference` against a zero-area operand collapsed
  to empty.** The deeper of the six: a zero-area operand contributes no cut
  segment, so it does not subdivide the arrangement's cells, yet `contains` still
  answers true on the points it covers — so a cell whose witness happened to land
  on that operand was classified, in whole, by a single point of a set that covers
  no area. `A ∖ point` and `A △ point` came back empty instead of `A` exactly when
  the arrangement picked that point as the witness for A's interior.
  `regularizedBoolean` now reads an operand without area as the empty set, which
  is what regularization means and what keeps the cell classification sound.
- **`Convex::operator*=` did not restore the canonical rotation.** A negative
  factor is a point reflection: it preserves the counterclockwise cycle but
  reverses the lexicographic order, so the lex-min-first rotation no longer starts
  where it must — and the convex predicates binary-search that cycle, so the value
  answered `contains` incorrectly. `Triangle`, `Rectangle`, `Polygon`,
  `PolygonSet` and `MonotoneChain` all renormalize in their scalar operators;
  `Convex` was the sole outlier and now rebuilds through the normalizing
  constructor, as `rotate90` and the four `scale*` mutators beside it already did.
- **`Disk`'s ordering was not total.** `operator<=>` compared the radius and centre
  in `NumberType`, but both are fractions of the boundary coordinates, so an
  integral `NumberType` truncated them and two different circles could compare
  *equivalent* while `operator==` correctly reported them unequal — leaving `<`,
  `==` and `>` all false and breaking `std::set<Disk>`. It now computes both in
  `division_result_t`, where equivalence means the same thing as `operator==`.
- **`contains` rejected a radius-zero `Disk` on the receiver's boundary.** The area
  shapes decide disk containment with two tests that stay in integer arithmetic and
  so avoid the disk's rational centre and radius: no edge passes through the *open*
  disk, and a point strictly inside the disk is *interior* to the receiver. A
  radius-zero disk has an empty interior, so `pointInsideInteriorContainedIn` had
  only the disk's own point to offer and the interior test then rejected it on the
  boundary — `Rectangle[(0,0),(10,10)].contains(Disk(P(0,4),0))` was false although
  the rectangle contains `Point(0,4)`. It broke `boundaryContains ⟹ contains` too,
  since `boundaryContains` answered true for that pair.
  `Halfplane::interiorContains(Disk)` already carried a `getIfPoint()` guard for
  exactly this reason; `contains` now carries it as well, in `Rectangle`,
  `Triangle`, `Convex`, `Halfplane` and `HalfplaneIntersection` (`Polygon` and
  `PolygonWithHoles` had one already). `Convex` and `Triangle` answer through their
  own `contains(Point)` rather than through the edge half-planes, because a
  degenerate hull has no opposing pair of bounding half-planes to cut its carrier
  line back down to the point or segment it actually is.
- **`Disk::boundaryContains(itself)` was true at positive radius.** It tested that
  the other disk's three defining points lie on this circle, which answers *the two
  disks share a boundary* — not *the filled disk lies on the boundary*. A disk of
  positive radius covers area and a circle covers none, so only a collapsed disk
  can lie on one; that collapsed reading was already right and is what the overload
  now keeps.

### Still open

These are triaged but unfixed. The first four are all about shapes that are
degenerate but defined, which is where most of what this harness finds lives.

1. **A degenerate `Segment` contains an unbounded region.**
   `Segment((3,3),(3,3)).contains(HalfplaneIntersection[...])` is true; a point
   cannot contain a quarter-plane. `intersects` correctly answers false.

2. **`intersection` invents a point for a zero-length operand.**
   `OrientedSegment (0,1)->(0,1)` against `Segment (0,0)--(1,1)` returns the point
   `(0,1)`, which lies on neither operand — `intersects` correctly answers false.
   Same for a two-equal-vertex `Polyline`, and a degenerate `Triangle` returns a
   whole segment that is not inside the other operand.

3. **`crosses` is true where both `separates` are false, for degenerate area
   shapes.** `Convex[(-1,0),(1,0)].crosses(Rectangle[(0,-1),(0,1)])` is true while
   `separates` is false both ways — violating the documented
   `crosses == A.separates(B) && B.separates(A)`. The identical geometry as two
   `Segment`s answers `crosses=1, separates=1, separates=1`. This is the far side
   of the "a degenerate area shape never separates" entry already in
   `sandbox/todo.md`: the guards make `separates` too strict, and `crosses` does
   not go through `separates` for these pairs, so the two disagree.

4. **Bentley–Ottmann double-reports with a zero-length segment.** For
   `{(0,0)--(0,0), (0,-1)--(0,0)}`, `findIntersections` returns 2 pairs and
   `detail::bruteForceIntersections` returns 1. A zero-length segment is defined
   (`isUndefined()` is false), so it is legitimate input.

### Unbounded regions

5. **Predicates throw on an unbounded `HalfplaneIntersection`.** `separates` and
    `crosses` against a `Polyline` throw
    `HalfplaneIntersection::bbox is only defined for a nonempty bounded region`.
    A predicate is documented to answer, not to throw.

6. **`squaredDistance` returns 0 for a non-intersecting pair when instantiated at
    `double`.** For `HalfplaneIntersection[^-(0,-2)--(-2,-3)-^,^-(0,0)--(1,-1)-^]`
    against `Line -(-1,-1)--(1,0)-`, the `double` instantiation answers 0 while
    the default exact one answers 9/5, and `intersects` answers false. Translating
    the pair by `(3,-5)` makes the `double` answer 1.8 — which is how the
    invariance property caught it.

### API coverage asymmetries

These may be deliberate, but the sibling operations disagree about their domain:

7. **`regularizedIntersection` throws for all 16 ordered pairs among
    `Rectangle`, `Triangle`, `Convex` and `Polygon`**, while `regularizedUnion`,
    `difference` and `symmetricDifference` all work for exactly those pairs. It
    succeeds only when an operand is a `PolygonWithHoles` or a `PolygonSet`. The
    concrete overload is constrained on `shapeRank<Other> > shapeRank<Self>` with
    no same-rank or lower-rank case, so `Rectangle ∩ Rectangle` does not even
    compile. This contradicts `Shape::intersection`'s own documentation, which
    sends the caller to `regularizedIntersection` on the grounds that it "answers
    with a `PolygonSet` and so never has to throw".

8. **`distanceL1` and `distanceLInf` are undefined for every pair involving a
    `Disk`**, while `squaredDistance` is defined for all of them.

### A documented guarantee that does not hold

9. **`sortAround` does not always trace a simple polygon.** Its documentation
    promises that "connecting the sorted points in order traces a simple,
    star-shaped polygon whose kernel contains `p`". With centre `(0,1)` and points
    `(-1,0), (0,-1), (0,0), (1,0)` it returns the ring
    `(-1,0), (0,-1), (0,0), (1,0)`, whose closing edge `(1,0)–(-1,0)` passes
    through the vertex `(0,0)` — not simple by pgl's own `isSimple`. Three of the
    points being collinear is what does it. Either the guarantee needs a
    non-collinearity precondition or it should say *weakly* simple.

### Where the harness was wrong

Recorded because they are easy to re-introduce as false findings:

- **The quarter turn in `distances-follow-the-map` was asking a `MonotoneChain`
  to be something it is not.** The property bundled translation, a quarter turn
  and scaling into one check registered at `kNoTag`, so the rotation ran on chain
  operands — which, per the entry below, cannot represent their own rotation. All
  33 pairs under the `invariance/distances-follow-the-map [*]` wildcard were that
  one mistake. The rotation now lives in its own `distances-survive-a-quarter-turn`
  at `kAxisFree`, both halves come out clean across seeds, and the wildcard is
  gone from the baseline. A reminder that a `[*]` entry can hide a harness bug as
  easily as a library one: bundling three maps into one property meant the two
  correct ones could never be seen passing separately.
- **`MonotoneChain` is not closed under rotation or shear.** Its value *is* the
  lexicographic order of its vertices, and the quarter turn of an x-monotone
  chain is generally not x-monotone, so the class cannot represent its own image:
  rotating gives the chain *through the rotated points*, a different curve. That
  is inherent to the type, so those two properties require the `kAxisFree` tag.
  Translation, positive scaling and negation are fine (negation reverses the
  x-order, which re-sorts to the reversed traversal — the same curve).
- **A `Disk`'s integer bbox must round outward**, its radius being irrational, so
  a disk inside a rectangle can have the larger box. `contains ⟹ bbox contains
  bbox` therefore holds only for tight boxes, and skips `Disk` operands.
- **Two coordinate draws must not be two arguments of one call.** Argument
  evaluation order is unspecified, and g++ and clang++ choose differently, so
  `emplace_back(rng.inRange(…), rng.inRange(…))` silently swapped x and y between
  the two builds: the same seed explored a different sequence, and a witness found
  by one build did not reproduce under the other. Both draw sites now use named
  locals, and the fix is verified by comparing the full signature set of a seed
  across both compilers.
- **`empty.separates(A)` is not false for every `A`.** `X.separates(Y)` asks
  whether `Y∖X` is disconnected, so with `X` empty it asks whether `A` itself is
  — which a multi-component `PolygonSet` already is, correctly, before anything
  is removed. `emptyOperandIsDegenerateCase` now guards that one clause with the
  same `isConnected` helper the rest of the `separates` reasoning uses.
- **A `Disk`'s area is not exact either.** `area` is `pi r^2`, computed in
  floating point and converted, so an exact result type holds a rounded value and
  the `measure` identities hold only to a tolerance there. The `measure`
  properties skip a `Disk` for the exact comparisons, the same way the metric
  ones do for a `Disk` pair's distances.
- **A shape's convex hull must be held at the exact vertex type.** Wrapping a
  `HalfplaneIntersection`'s hull in `Shape<Point<int>>` compiles — `Shape`'s
  converting constructors are explicit but present — and rounds the rational
  vertices, so the property then tests a different polygon and reports the
  original as not contained in it. Use `ExactShape`, not `AnyShape`, for anything
  a construction computed in `Exact`.
- **An empty operand violates a precondition, which is undefined behaviour
  whether or not anything checks it.** Passing an empty shape to a measure or a
  distance is UB; `Rectangle::midpoint` and `Rectangle::squaredDistance` happen
  to `assert(!empty())` because that check is far cheaper than what they compute,
  and an abort there is the library working, not a crash to report. Whether a
  precondition carries an assert is a cost decision, so the *absence* of one
  elsewhere is not evidence that an empty operand is supported — and `Convex` and
  `HalfplaneIntersection` answering the origin for an empty centroid is not a
  disagreement to reconcile, since under UB any value will do. The `measure`,
  `metric` and `closest` groups skip an empty operand for the same reason the
  generators skip an `isUndefined()` shape: there is no contract to assert about.
  Note this is a precondition of the *operation*, not a state of the shape — an
  empty `Rectangle` is still `isUndefined() == false`, and the predicates answer
  for it normally.
- **A `Disk` pair's distances are not exact.** `squaredDistance` is documented to
  compute in `double` and convert for any pair involving a `Disk`, so an exact
  `ResultNumber` holds a rounded value there and the isometry identities hold
  only to a tolerance. `distancesFollowTheMap` runs its exact pass on the other
  pairs and asks a `Disk` pair at `double` alone.

### Keeping it in step with the library

It is header-only against `include/`, out of CI, and therefore silently rots.
It last built and ran clean under both compilers on 2026-09-16. It had been found
broken earlier that day by three API changes it had not followed — worth knowing
as the shape of what breaks it:

- `Shape`'s storage accessors were renamed when the two vocabularies were split
  (commit 2f97444): `isPolygon()` → `holdsPolygon()`, `getIfConvex()` →
  `getIfHoldsConvex()`, and so on. The plain `isPoint()` / `isSegment()` /
  `getIfPoint()` survive but now mean the *geometric* question, so a use of one
  that wants the stored alternative compiles and answers the wrong thing.
- `Shape::intersection` returns `std::vector<Shape>` — the connected pieces —
  rather than one `Shape`. The two `intersection` properties judge each piece.
- The distance members default to `division_result_t<NumberType>`, which is
  `ERational` for the harness's `int` coordinates, where they used to default to
  `double`. The metric properties now compare exactly, `metricsAreOrdered` states
  `L∞ ≤ L2 ≤ L1 ≤ 2L∞` on the squares to avoid a root, and `nearlyEqual` /
  `nearlyAtMost` take the tolerance branch only for a floating-point
  instantiation.

Run it under both compilers after any change to the `Shape` interface: the build
is the test that it still describes the library.

## Adding to it

A property is a function returning `held()`, `skipped()` or `failure(detail)`:

```cpp
inline Result myProperty(const AnyShape& a, const AnyShape& b) {
    if (!a.contains(b)) {
        return skipped();               // outside the relation's domain
    }
    PGLPROP_CHECK(a.intersects(b),      // the detail is the whole report: name the values
                  pair(a, b) + " ; A contains B but does not intersect it");
    return held();
}
```

Register it in the same file's `register…Properties`, choosing `unary`, `binary`
or `pointSet`, and a tag mask the operands must carry (`kRegion` for the boolean
operations, `kAffine` where `Transformation` applies, `kAxisFree` for maps that
reorient, `kConvexAlternative` for convex-only). A tag filters the draw, and a
*starved* tag also steers a quarter of the cases towards it, which is what keeps
a narrow property from going vacuous — see the third design note above.

Say what the property is a consequence of. Every one here is derivable from a
documented definition, and the comment saying which is what lets the next reader
decide whether a failure is a library bug or a wrong property — the distinction
this harness lives or dies by.

A generator is a function from points to a shape, returning `false` to refuse the
input; add it to the table in `generators.hpp` with its point-count range and
tags. Prefer several routes to the same alternative — a `Polygon` arrives here as
a hull, an arbitrary simple ring and a polyomino, and the three are
systematically different shapes.

## Files

| File | |
| --- | --- |
| `run.sh` | Build and run; passes arguments through. |
| `main.cpp` | Command line, and the registry of every property. |
| `rng.hpp` | splitmix64 plus unbiased bounded draws, so a seed replays identically under any standard library. |
| `generators.hpp` | The case model, the generator table, and the shape aliases. |
| `shrink.hpp` | Witness reduction and its termination measure. |
| `crash.hpp` | Fatal signals as reported failures. |
| `framework.hpp` | Property registry, the case runner, failure aggregation, the baseline. |
| `properties_common.hpp` | Shared helpers: operand rendering, tolerances, partial-operation handling. |
| `properties_predicates.hpp` | The predicate algebra. |
| `properties_metric.hpp` | Distances against the predicates. |
| `properties_invariance.hpp` | Value semantics, normalization, affine invariance. |
| `properties_constructions.hpp` | Bounding boxes, `intersection`, the boolean identities, Minkowski sums. |
| `properties_algorithms.hpp` | Hull, sweep, triangulation, arrangement, sorting. |
| `known_failures.txt` | The baseline. |
