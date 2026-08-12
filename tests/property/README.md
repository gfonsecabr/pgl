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

`--list` prints the current set: 58 properties in thirteen groups, drawn from 25
generators.

| Group | What it asserts |
| --- | --- |
| `predicates` | The algebra of the seven predicates, from their set-theoretic definitions: symmetry of `intersects` / `interiorsIntersect` / `crosses`; `crosses` is exactly mutual `separates`; `interiorContains` and `boundaryContains` imply `contains`; `contains` implies `intersects` and forbids `separates`; boundary and interior are exclusive; separation implies intersection for a connected operand; the empty-set clause of all seven; and the self-pair, including `A ⊆ ∂A` exactly when `A` has no interior. |
| `metric` | The distance families against the predicates and each other: all three distances vanish exactly when `intersects` is true; symmetry; `L∞ ≤ L2 ≤ L1 ≤ 2L∞`; containment forces distance zero; the Hausdorff distance is symmetric, dominates the nearest-point distance, and vanishes exactly on mutual containment. |
| `value` | Value semantics: a copy is equal, orders equal and hashes equal; equal shapes hash equally; `<`, `==`, `>` are a total order. Plus **normalization** — rebuilding a shape from its own defining points in a different order gives an equal shape, for each class that documents one (`Segment`, `Line`, `Rectangle`, all six vertex permutations of `Triangle` and of a three-point `Disk`, reordered input for `Convex` and `MonotoneChain`, any ring rotation for `Polygon`). |
| `invariance` | All seven predicates survive translation, quarter turns, integer scaling, negation and an integer shear; squared distance survives isometries and scales as `s²`. Any *one* correct answer becomes a test of a whole orbit, which is what finds axis-aligned shortcuts that do not generalize. |
| `bounding` | A shape lies in its own bbox; intersecting shapes have intersecting bboxes; containment carries over to tight bboxes. |
| `intersection` | `intersection` is non-empty exactly when `intersects` is true, and its result lies inside both operands. |
| `minkowski` | The Minkowski sum of two convex shapes contains every vertex sum and does not depend on the operand order. |
| `boolean` | Exact area identities over the regularized operations: `\|A∪B\| + \|A∩B\| = \|A\| + \|B\|`, `\|A∖B\| + \|A∩B\| = \|A\|`, `\|A△B\| = \|A∪B\| − \|A∩B\|`; `A△B = (A∖B) ∪ (B∖A)` as sets; commutativity; the lattice order; `A∖B` shares no interior with `B`; and the self-operations collapse. Computed in `Rational<BigInt>`, so these are equations, not tolerances. |
| `hull`, `sweep`, `triangulation`, `arrangement`, `sorting` | Whole algorithms against their oracles: the hull contains its input, is convex, is idempotent and invents no vertices; Bentley–Ottmann matches brute force; a triangulated polygon's triangle areas sum to its own and number `n−2`; the arrangement's DCEL satisfies `twin∘twin = id`, head-to-tail `next`, and one face per cycle; `hilbertSort` permutes; `sortAround` traces a simple ring. |

Two design notes worth knowing before adding to this:

- **Skips are tracked separately from passes.** Most of these relations are
  conditional, and a property that quietly answers "held" for inputs it cannot
  judge is indistinguishable from one that works. The runner reports any property
  that skipped *every* case under `VACUOUS`, which is the failure mode a property
  harness is most prone to. Keep that section empty.
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
not new problems: 588 exact entries, and still 1–7 new per seed. Collapsing the
thirteen properties whose failures were all traced to one cause brings that to 81
entries, and seeds 17–22 — none of which contributed to the list — come out clean
at 20000 cases each. That is what makes the exit code mean something.

```bash
sh tests/property/run.sh --cases 20000 --update-baseline   # rewrite from one run
```

That writes exact pairs only, so the wildcards are a maintainer's judgement to
reapply by hand. The shipped list is the union over seeds 1–16 at 20000 cases.

## What it currently finds

Every item below was reduced by the shrinker and then re-confirmed with a
standalone program, so each is a real reachable case and not a harness artefact.
They are about a dozen root causes behind some 590 signatures.

### Reachable assertion failure

1. **`distanceL1` / `distanceLInf` abort on a one-vertex `MonotoneChain` or
   `Polyline`.** `edgeMinDistanceL1` asserts `size() >= 2`, but a one-vertex chain
   is *defined* — `isUndefined()` is `empty()` alone, and `isPoint()` is true — so
   the assertion is reachable from the public API. Under `NDEBUG` it reads
   `boundaryAt<false>(0)` on a chain with no edge.
   `MonotoneChain{(0,0)}.distanceL1(Line{(0,-1),(1,0)})`.

### The empty set

2. **`EmptyShape::contains(X)` is false for an `X` that is empty but is not an
   `EmptyShape`** — a default `Rectangle`, an empty `HalfplaneIntersection`.
   `∅ ⊇ ∅` should hold. Recent commits gave `Rectangle`, `Convex` and `Polygon` a
   real empty set; this looks like the operand side of that change.
   `EmptyShape<P>().contains(Rectangle<P>())` is false while
   `Rectangle<P>().empty()` is true.

3. **`intersection` with an empty operand returns the other operand.**
   `Rectangle<P>().intersection(region)` returns the whole region, while
   `intersects` correctly answers false.

4. **`symmetricDifference` with a zero-area operand returns empty.**
   `Rectangle[(2,-1),(2,-1)] △ PolygonSet[Polygon[(1,-6),(5,-6),(5,4),(1,4)]]` has
   twiceArea 0, where `|A∪B| − |A∩B|` is 80.

### Degenerate shapes

5. **`contains` mishandles a radius-zero `Disk`.**
   `Rectangle[(0,0),(0,0)].contains(Disk(P(0,0),0))` is false although both are
   exactly the point `(0,0)`, and each contains `Point(0,0)`. Same for
   `Halfplane` and `Triangle`. `boundaryContains` answers true for the same pair,
   so it also breaks `boundaryContains ⟹ contains`.

6. **`Disk::boundaryContains(itself)` is true at positive radius.** The boundary
   is the circle and the disk is filled, so it cannot lie in it. Correct at radius
   zero. `Disk(P(0,0),1).boundaryContains(itself)`.

7. **A degenerate `Segment` contains an unbounded region.**
   `Segment((3,3),(3,3)).contains(HalfplaneIntersection[...])` is true; a point
   cannot contain a quarter-plane. `intersects` correctly answers false.

8. **`intersection` invents a point for a zero-length operand.**
   `OrientedSegment (0,1)->(0,1)` against `Segment (0,0)--(1,1)` returns the point
   `(0,1)`, which lies on neither operand — `intersects` correctly answers false.
   Same for a two-equal-vertex `Polyline`, and a degenerate `Triangle` returns a
   whole segment that is not inside the other operand.

9. **`crosses` is true where both `separates` are false, for degenerate area
   shapes.** `Convex[(-1,0),(1,0)].crosses(Rectangle[(0,-1),(0,1)])` is true while
   `separates` is false both ways — violating the documented
   `crosses == A.separates(B) && B.separates(A)`. The identical geometry as two
   `Segment`s answers `crosses=1, separates=1, separates=1`. This is the far side
   of the "a degenerate area shape never separates" entry already in
   `sandbox/todo.md`: the guards make `separates` too strict, and `crosses` does
   not go through `separates` for these pairs, so the two disagree.

10. **Bentley–Ottmann double-reports with a zero-length segment.** For
    `{(0,0)--(0,0), (0,-1)--(0,0)}`, `findIntersections` returns 2 pairs and
    `bruteForceIntersections` returns 1. A zero-length segment is defined
    (`isUndefined()` is false), so it is legitimate input.

### Normalization and ordering

11. **`Convex::operator*=(scalar)` does not restore the canonical rotation.**
    `Convex[(-1,0),(0,0),(0,1)] * -1` gives `Convex[(1,0),(0,0),(0,-1)]`, whose
    lex-min vertex is not first; `rotated90(2)` — the same geometric map —
    correctly gives `Convex[(0,-1),(1,0),(0,0)]`. The un-normalized value then
    answers `contains(Point(0,0))` false where the canonical one answers true,
    the convex predicates relying on the canonical order for their binary search.
    A negative scalar therefore corrupts a `Convex`.

12. **`Disk`'s ordering is not total.** `Disk((-1,-2)(2,-2)(0,2))` against
    `Disk((-1,2)(1,-2)(0,2))` reports `<`, `==` and `>` all false, which breaks
    `std::set<Disk>` and `std::sort`.

### Unbounded regions

13. **Predicates throw on an unbounded `HalfplaneIntersection`.** `separates` and
    `crosses` against a `Polyline` throw
    `HalfplaneIntersection::bbox is only defined for a nonempty bounded region`.
    A predicate is documented to answer, not to throw.

14. **`squaredDistance` returns 0 for a non-intersecting pair when instantiated at
    `double`.** For `HalfplaneIntersection[^-(0,-2)--(-2,-3)-^,^-(0,0)--(1,-1)-^]`
    against `Line -(-1,-1)--(1,0)-`, the `double` instantiation answers 0 while
    the default exact one answers 9/5, and `intersects` answers false. Translating
    the pair by `(3,-5)` makes the `double` answer 1.8 — which is how the
    invariance property caught it.

### API coverage asymmetries

These may be deliberate, but the sibling operations disagree about their domain:

15. **`regularizedIntersection` throws for all 16 ordered pairs among
    `Rectangle`, `Triangle`, `Convex` and `Polygon`**, while `regularizedUnion`,
    `difference` and `symmetricDifference` all work for exactly those pairs. It
    succeeds only when an operand is a `PolygonWithHoles` or a `PolygonSet`. The
    concrete overload is constrained on `shapeRank<Other> > shapeRank<Self>` with
    no same-rank or lower-rank case, so `Rectangle ∩ Rectangle` does not even
    compile. This contradicts `Shape::intersection`'s own documentation, which
    sends the caller to `regularizedIntersection` on the grounds that it "answers
    with a `PolygonSet` and so never has to throw".

16. **`distanceL1` and `distanceLInf` are undefined for every pair involving a
    `Disk`**, while `squaredDistance` is defined for all of them.

### A documented guarantee that does not hold

17. **`sortAround` does not always trace a simple polygon.** Its documentation
    promises that "connecting the sorted points in order traces a simple,
    star-shaped polygon whose kernel contains `p`". With centre `(0,1)` and points
    `(-1,0), (0,-1), (0,0), (1,0)` it returns the ring
    `(-1,0), (0,-1), (0,0), (1,0)`, whose closing edge `(1,0)–(-1,0)` passes
    through the vertex `(0,0)` — not simple by pgl's own `isSimple`. Three of the
    points being collinear is what does it. Either the guarantee needs a
    non-collinearity precondition or it should say *weakly* simple.

### Where the harness was wrong

Recorded because they are easy to re-introduce as false findings:

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
reorient, `kConvexAlternative` for convex-only). Tags *select* rather than skip,
which keeps a narrow property from going vacuous.

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
