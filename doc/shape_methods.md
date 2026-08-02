<!-- AUTO-GENERATED from doc/raw/shape_methods.md by doc/raw/doxylink.py — do not edit; edit the raw version and regenerate. -->

<img align="left" src="figures/logo.png" width="23%"/>

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="figures/logotextdark.svg"/>
  <img alt="Pangolin: Plane Geometry Library" src="figures/logotext.svg" width="65%"/>
</picture>

[![Tests](https://github.com/gfonsecabr/pgl/actions/workflows/tests.yml/badge.svg)](https://github.com/gfonsecabr/pgl/actions/workflows/tests.yml)
[![Standard](https://img.shields.io/badge/C%2B%2B-20/23/26-rgb(10,66,158).svg)](https://en.wikipedia.org/wiki/C%2B%2B#Standardization)
[![License](https://img.shields.io/badge/license-MIT-rgb(216,134,42).svg)](https://opensource.org/licenses/MIT)
[![Benchmarks](https://img.shields.io/badge/benchmarks-online-rgb(21,153,135).svg)](https://gfonsecabr.github.io/pgl/benchmarks/index.html)

<br/>

> ⚠️ **Work in Progress**: This library is still under construction and contains **bugs and missing features**. Use in production environments is not recommended.

## Methods Common to Most Shapes

### Predicates

Any two shapes `A`,`B` support the following [predicates](#predicates), where $\partial A$ denotes the manifold boundary of $A$. Notice that the boundary of a one-dimensional shape is defined as its endpoints (see also [shapes](shapes.md)).

| Predicate | Definition | Question |
| --------- | ---------- | --------- |
| `A.contains(B)` | $A \supseteq B$ | Does `A` contain `B`? |
| `A.boundaryContains(B)` | $\partial A \supseteq B$ | Does the boundary of `A` contain `B`? |
| `A.interiorContains(B)` | $(A \setminus \partial A) \supseteq B$ | Does the interior of `A` contain `B`? |
| `A.intersects(B)` | $A \cap B \neq \emptyset$ | Do `A` and `B` intersect? |
| `A.interiorsIntersect(B)` | $(A \setminus \partial A) \cap (B \setminus \partial B) \neq \emptyset$ | Do the interiors of `A` and `B` intersect? |
| `A.separates(B)` | $B \setminus A$ disconnected | Does the removal of `A` separate `B`? |
| `A.crosses(B)` | $A \setminus B$ and $B \setminus A$ disconnected | Does the removal of each of `A` and `B` separate the other? |

The following table illustrates the result of the predicates for a triangle and a line segment.

| Predicate | <img width="100%" src="figures/predicate1.svg"/> | <img width="100%" src="figures/predicate2.svg"/> | <img width="100%" src="figures/predicate3.svg"/> | <img width="100%" src="figures/predicate4.svg"/> | <img width="100%" src="figures/predicate5.svg"/> | <img width="100%" src="figures/predicate6.svg"/> | <img width="100%" src="figures/predicate7.svg"/> | <img width="100%" src="figures/predicate8.svg"/> |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `A.contains(B)`           | ❌ | ❌ | ❌ | ❌ | ❌ | ✅ | ✅ | ✅ |
| `B.contains(A)`           | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ |
| `A.boundaryContains(B)`   | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ✅ |
| `B.boundaryContains(A)`   | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ |
| `A.interiorContains(B)`   | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ✅ | ❌ |
| `B.interiorContains(A)`   | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ |
| `A.intersects(B)`         | ❌ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| `A.interiorsIntersect(B)` | ❌ | ❌ | ✅ | ✅ | ✅ | ✅ | ✅ | ❌ |
| `A.separates(B)`          | ❌ | ❌ | ✅ | ❌ | ❌ | ❌ | ❌ | ❌ |
| `B.separates(A)`          | ❌ | ❌ | ✅ | ✅ | ❌ | ❌ | ❌ | ❌ |
| `A.crosses(B)`            | ❌ | ❌ | ✅ | ❌ | ❌ | ❌ | ❌ | ❌ |

All predicates are calculated exactly for integers (except for possible overflows detailed in [types](types.md)).


### Operators

Shapes are translated by adding or subtracting a point. The point coordinates
are added to, or subtracted from, every defining point of the shape.

```c++
pgl::Point p = {2,3}, q = {4,5};
pgl::Segment s = {p, q},    //  s = (2,3)--(4,5)
             t1 = p + s,    // t1 = (4,6)--(6,8)
             t2 = s - p;    // t2 = (0,0)--(2,2)
```

Adding a point is the special case of adding two shapes, which is their
[Minkowski sum](#minkowski-sum).

In-place translations use `+=` and `-=`.
Scaling around the origin uses the operator `*` or `*=` with a scalar.

```c++
pgl::Segment s = {2, 3, 4, 5};    //  s = (2,3)--(4,5)
s += pgl::Point(1,2);             //  s = (3,5)--(5,7)
s *= 10;                          //  s = (30,50)--(50,70)
```

If we want to scale around a particular point `p`, we can use a combination of the previous operators:

```c++
pgl::Segment s = {2,3,4,5};   // s = (2,3)--(4,5)
pgl::Point p = s.midpoint();  // p = (3,4)
pgl::Segment t = 3*(s-p) + p; // t = (0,1)--(6,7)
```

### Transformations

`pgl::Transformation<Number>` stores a general affine map — a 2x2 linear part
plus a translation — as a 2x3 matrix. It is applied to a point or shape, and
composed with another transformation, with the same operator `*`, so
`t1 * t2 * shape` both composes and applies left to right (applying the
right-hand transformation first).

```c++
pgl::Segment s = {0,0,5,5};
auto t = pgl::Transformation<int>::rotation90(1) * pgl::Transformation<int>::translation(2,0);
auto rotated = t * s;
```

Factories cover the common exact cases: `identity()`, `translation(dx,dy)`,
`scaling(sx,sy=sx)`, `rotation90(k=1)` (exact multiples of 90 degrees),
`shearX(k)`, `shearY(k)`, `reflectionX()`, `reflectionY()`. An arbitrary-angle
`rotation<ResultNumber=double>(radians)` is also available but, unlike
`rotation90`, requires an explicit floating-point `ResultNumber` since a
general angle is generally irrational.

`determinant()` is negative exactly when the transformation reverses
orientation (a reflection, or an odd number of shears/reflections composed
together). Shapes with a winding or normalization invariant ([`Triangle`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html "Closed triangle stored by three vertices."),
[`Convex`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html "Closed convex polygon stored by its vertices."), [`MonotoneChain`](https://gfonsecabr.github.io/pgl/structpgl_1_1MonotoneChain.html "Weakly x-monotone polyline stored by lexicographically sorted vertices."), [`Polygon`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html "Closed simple polygon stored by its vertices.")) renormalize automatically through their
own constructors, and [`Halfplane`](https://gfonsecabr.github.io/pgl/structpgl_1_1Halfplane.html "Closed half-plane defined by an oriented boundary line.") swaps its source and target to keep the same
interior, mirroring the existing negative-scalar handling already used by
`scaledUpX`.

`inverse<ResultNumber = Number>()` returns the inverse transformation.
**Warning:** this divides by `determinant()`, so for an integral `Number` it
is inexact unless `ResultNumber` is a type such as `Rational<Number>` that
represents the division exactly.

[`Transformation`](https://gfonsecabr.github.io/pgl/structpgl_1_1Transformation.html "Affine transformation stored as a 2x3 matrix.") is applied to every shape except [`Rectangle`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html "Axis-aligned rectangle stored by minimum and maximum corners.") and [`Disk`](https://gfonsecabr.github.io/pgl/structpgl_1_1Disk.html "Closed Euclidean disk stored by boundary points plus optional disk label."): a
general affine map turns a rectangle into a parallelogram and a disk into an
ellipse, and neither class can represent that, so there is no such overload —
applying one is a compile error.

### Intersection

The intersection of any two shapes may be calculated as follows. Note that the intersection of any two shapes is always an [`std::optional`](https://en.cppreference.com/w/cpp/utility/optional.html) since the two shapes may not intersect. Since the intersection may have different types that depend on the two shapes, we sometimes use an
[`std::variant`](https://en.cppreference.com/w/cpp/utility/variant.html). For example, the intersection of two segments may be a point or a segment. Furthermore, some shapes such as simple polygons may have disconnected intersections. In such cases, an [`std::vector`](https://en.cppreference.com/w/cpp/container/vector.html) with several objects is returned.

```c++
pgl::Segment s = {0,0,5,5}, t = {0,3,5,3};
auto isec(s.intersection(t));
// The type of isec here is std::optional<std::variant<pgl::Point,pgl::Segment>>
pgl::Point p = std::get<0>(*isec);
// p = (3,3)
```

When the intersection can be represented as a [`Shape`](https://gfonsecabr.github.io/pgl/structpgl_1_1Shape.html "Runtime variant wrapper over the supported primitive shapes."), you can convert directly:

```c++
pgl::Segment s = {0,0,5,5}, t = {0,3,5,3};
pgl::Shape isec(s.intersection(t));
pgl::Point<> p(isec);
// p = (3,3)
```

[`PolygonWithHoles`](https://gfonsecabr.github.io/pgl/structpgl_1_1PolygonWithHoles.html "Closed region bounded by one outer simple polygon minus disjoint polygonal holes.") carries a second `intersection` alongside this one, returning
regions rather than polygons, because it is the one shape whose intersections
can have holes. See [Boolean Operations](#boolean-operations).

### Boolean Operations

The four boolean set operations on shapes with area all return a
`std::vector<PolygonWithHoles>`:

| call | result |
|---|---|
| `a.difference(b)` | $A \setminus B$, the part of `a` that `b` does not cover |
| `a.unionWith(b)` | $A \cup B$, the part either covers |
| `a.symmetricDifference(b)` | $A \mathbin{\triangle} B$, the part exactly one covers |
| `a.intersection(b)` | $A \cap B$, the part both cover — on [`PolygonWithHoles`](https://gfonsecabr.github.io/pgl/structpgl_1_1PolygonWithHoles.html "Closed region bounded by one outer simple polygon minus disjoint polygonal holes.") only, see below |

`difference`, `unionWith` and `symmetricDifference` are defined on [`Polygon`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html "Closed simple polygon stored by its vertices.") and
[`PolygonWithHoles`](https://gfonsecabr.github.io/pgl/structpgl_1_1PolygonWithHoles.html "Closed region bounded by one outer simple polygon minus disjoint polygonal holes."), against [`Polygon`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html "Closed simple polygon stored by its vertices."), [`PolygonWithHoles`](https://gfonsecabr.github.io/pgl/structpgl_1_1PolygonWithHoles.html "Closed region bounded by one outer simple polygon minus disjoint polygonal holes."), [`Convex`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html "Closed convex polygon stored by its vertices."), [`Triangle`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html "Closed triangle stored by three vertices.")
and [`Rectangle`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html "Axis-aligned rectangle stored by minimum and maximum corners.") — the bounded shapes with area. The union is a keyword in C++,
hence `unionWith`.

Three of the four are symmetric in their operands, and may be written in either
order. A [`Convex`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html "Closed convex polygon stored by its vertices."), [`Triangle`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html "Closed triangle stored by three vertices.") or [`Rectangle`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html "Axis-aligned rectangle stored by minimum and maximum corners.") receiver takes `unionWith`,
`symmetricDifference` and `intersection` by forwarding them to the other
operand, so `triangle.unionWith(polygon)` and `polygon.unionWith(triangle)` are
the same call — each unordered pair is implemented once, on the shape that can
represent the answer. `difference` is not symmetric and forwards nowhere: it
stays on the two receivers above.

```c++
pgl::Polygon<> square({0,0, 10,0, 10,10, 0,10});
auto pieces = square.difference(pgl::Rectangle(3,3,7,7));
// pieces.size() == 1, one region whose outer ring is the square
// and whose single hole is the rectangle
```

This is the family [`PolygonWithHoles`](https://gfonsecabr.github.io/pgl/structpgl_1_1PolygonWithHoles.html "Closed region bounded by one outer simple polygon minus disjoint polygonal holes.") exists for: removing a shape from the
middle of another one leaves a hole, and no other shape can say so. A union
creates one out of nothing just as readily — a `U` united with the bar that caps
it encloses a hole neither operand has — and a symmetric difference, being the
union of two differences, inherits holes from both.

Every one of them returns the **regularized** result, the closure of the
operation applied to the *interiors*: $\mathrm{closure}(A^\circ \setminus B)$,
$\mathrm{closure}(A^\circ \cup B^\circ)$, and so on. Lower-dimensional leftovers
are dropped — a stretch of boundary the operands share without either covering
it, an isolated contact point, and a slit, which has no area to begin with.
Without that, the answer would not be a set of regions at all. It also means
material with no area never *joins* anything: two shapes meeting at a single
point come back as two pieces, since a region may not have a self-touching outer
ring.

The pieces have pairwise disjoint interiors and their union is the result. They
are **not** nested: an island stranded inside a hole of the result comes back as
a piece of its own, since this library has no `PolygonSet`.

The boundaries can cross at non-integral points, so all four take the usual
`ResultNumber` parameter: `a.difference<pgl::ERational>(b)`. The arrangement
itself is always built over exact rationals and converted only at the end, so an
integral result type is exact whenever the crossings are integral — a
rectilinear operation on rectilinear input needs nothing special.

#### Why `intersection` is different

`intersection` appears twice in the library, and the two are not the same
method. The general one, described [above](#intersection), is defined for every
pair of shapes and returns polygons, polylines and points through an
`std::optional` and an `std::variant`. The region-valued one described here
exists **only on [`PolygonWithHoles`](https://gfonsecabr.github.io/pgl/structpgl_1_1PolygonWithHoles.html "Closed region bounded by one outer simple polygon minus disjoint polygonal holes.")**.

That is not an oversight. No component of the intersection of two *polygons* can
have a hole: a closed curve inside a closed set with a connected complement
bounds a disk inside it, so a curve in $A \cap B$ bounds a disk in each operand
and hence in the intersection. Every shape in the library has a connected
complement — except a region with holes, whose hole interiors are components of
their own. So `Polygon::intersection` loses nothing by returning plain polygons,
and a region's intersection genuinely needs a region:

```c++
pgl::Polygon<> hole({4,4, 8,4, 8,8, 4,8});
pgl::PolygonWithHoles<> annulus(square, std::vector{hole});
auto pieces = annulus.intersection(pgl::Rectangle(-5,-5, 20,20));
// pieces.size() == 1, and pieces[0] == annulus — hole and all
```

Which of the two answers a call is decided by the operands, not by the receiver:
a [`PolygonWithHoles`](https://gfonsecabr.github.io/pgl/structpgl_1_1PolygonWithHoles.html "Closed region bounded by one outer simple polygon minus disjoint polygonal holes.") operand pulls in the region-valued one whichever side it is
written on, so `polygon.intersection(region)` forwards to
`region.intersection(polygon)` and gives back regions rather than components.

One further difference is worth knowing: `Polygon::intersection` computes in the
result type, so an integral one truncates every crossing, while the region
operations above never do.

### Minkowski Sum

The Minkowski sum of two shapes is the set of all sums of a point of the first
and a point of the second, $A \oplus B = \\{a + b : a \in A, b \in B\\}$. It is
written `a.minkowskiSum(b)`, or `a + b`.

```c++
pgl::Segment s = {0,0,2,0}, t = {0,0,0,3};
pgl::Convex box = s + t;
// box = Convex[(0,0),(2,0),(2,3),(0,3)]
```

Adding a [`Point`](https://gfonsecabr.github.io/pgl/structpgl_1_1Point.html "Two-dimensional point with optional label payload.") is a translation, so it returns the other operand's own type
and is defined for every shape — that is the reading `shape + point` has always
had. Two bounded convex shapes ([`Point`](https://gfonsecabr.github.io/pgl/structpgl_1_1Point.html "Two-dimensional point with optional label payload."), [`Segment`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html "Unoriented closed segment between two endpoints plus optional segment label."), [`OrientedSegment`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html "Directed segment preserving source-to-target order plus optional segment label."),
[`Rectangle`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html "Axis-aligned rectangle stored by minimum and maximum corners."), [`Triangle`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html "Closed triangle stored by three vertices."), [`Convex`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html "Closed convex polygon stored by its vertices.")) sum to a [`Convex`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html "Closed convex polygon stored by its vertices."), computed in linear time by
merging the two boundaries' edge directions. Two rectangles are the one
non-trivial pair closed under the sum and give back a [`Rectangle`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html "Axis-aligned rectangle stored by minimum and maximum corners.").

```c++
pgl::Triangle t = {0,0,3,0,0,3};
pgl::Convex hexagon = t + pgl::Triangle(0,0,-1,0,0,-1);   // 6 vertices
pgl::Rectangle r = pgl::Rectangle(1,2,4,6) + pgl::Rectangle(-1,0,2,1);
// r = (0,2)--(6,7)
```

Every vertex of the result is a sum of two input vertices, so the construction
is exact: integer coordinates in, integer coordinates out. A result that drops
below two dimensions is reported the usual way, through the returned [`Convex`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html "Closed convex polygon stored by its vertices."):
summing two parallel segments gives a [`Convex`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html "Closed convex polygon stored by its vertices.") satisfying `isSegment()`. The
empty shape absorbs, and an empty [`Convex`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html "Closed convex polygon stored by its vertices.") operand gives an empty [`Convex`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html "Closed convex polygon stored by its vertices.").

#### Non-convex operands

A non-convex operand is where the sum needs a region: sliding a shape around the
inside of a `C` sweeps out material that closes over a hole neither operand has.
[`Polygon`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html "Closed simple polygon stored by its vertices.") and [`PolygonWithHoles`](https://gfonsecabr.github.io/pgl/structpgl_1_1PolygonWithHoles.html "Closed region bounded by one outer simple polygon minus disjoint polygonal holes.") therefore carry a second `minkowskiSum`, against
[`Polygon`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html "Closed simple polygon stored by its vertices."), [`PolygonWithHoles`](https://gfonsecabr.github.io/pgl/structpgl_1_1PolygonWithHoles.html "Closed region bounded by one outer simple polygon minus disjoint polygonal holes."), [`Convex`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html "Closed convex polygon stored by its vertices."), [`Triangle`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html "Closed triangle stored by three vertices."), [`Rectangle`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html "Axis-aligned rectangle stored by minimum and maximum corners."), [`Segment`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html "Unoriented closed segment between two endpoints plus optional segment label.") and
[`OrientedSegment`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html "Directed segment preserving source-to-target order plus optional segment label."), returning a `std::vector<PolygonWithHoles>` like the boolean
operations above; so does [`Polyline`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polyline.html "Open polygonal chain stored in traversal order; may self-intersect."), whose own operands are below.

```c++
// The square annulus, cut open through its right wall over y in [3,5].
pgl::Polygon<> c({0,0, 8,0, 8,3, 6,3, 6,2, 2,2, 2,6, 6,6, 6,5, 8,5, 8,8, 0,8});
auto plugged = c.minkowskiSum(pgl::Rectangle(0,0, 2,2));
// plugged.size() == 1; its outer ring is (0,0)--(10,10) and it has one hole,
// (4,4)--(6,6) — the cavity, stranded once the two-unit cut is closed.
```

The two overload sets never overlap: the pairs whose sum fits in a single shape
are exactly the pairs listed above, and these take the rest. Which one answers is
again a question about the pair and not about the receiver — a [`Segment`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html "Unoriented closed segment between two endpoints plus optional segment label."),
[`OrientedSegment`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html "Directed segment preserving source-to-target order plus optional segment label."), [`Convex`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html "Closed convex polygon stored by its vertices."), [`Triangle`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html "Closed triangle stored by three vertices.") or [`Rectangle`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html "Axis-aligned rectangle stored by minimum and maximum corners.") written on the left of a
non-convex operand forwards to it, so `rectangle.minkowskiSum(polygon)` is
`polygon.minkowskiSum(rectangle)` and `segment.minkowskiSum(polyline)` is
`polyline.minkowskiSum(segment)`, while `rectangle.minkowskiSum(triangle)` and
`segment.minkowskiSum(segment)` are still the single-shape sum. The result is a
*set* of regions because $A \oplus B$ is connected whenever both operands are, so
it is one region unless its boundary pinches shut — which no single region may
do. Like the boolean operations it is **regularized**, so a flat operand's sum
keeps only what has area, and it takes the same `ResultNumber` parameter. Every
vertex of every convex piece sum is a sum of two input vertices, so those are
exact; only where two of them cross can a vertex land off the lattice, and that
arrangement is built over exact rationals and converted once at the end.

That last point is worth more attention here than it gets in the boolean
operations, because it bites sooner. There the crossings that land off the
lattice are crossings of the *operands'* boundaries, which rectilinear integer
input never has; here they are crossings between two piece sums, which two
perfectly ordinary integer operands can produce on their own:

```c++
pgl::Polygon<> u({0,0, 6,0, 6,6, 4,6, 4,2, 2,2, 2,6, 0,6});    // a U
u.minkowskiSum(pgl::Triangle(-2,-1, 2,0, 0,2));                // notch tip truncated
u.minkowskiSum<pgl::ERational>(pgl::Triangle(-2,-1, 2,0, 0,2)); // tip at (16/5, 34/5)
```

Both operands are convex-edged and integral, and the answer still is not. Ask for
an exact `ResultNumber` unless you know the sum lands on the lattice.

A region operand needs nothing special for its holes — they are simply where the
decomposition has no piece — but its **slits** do sweep out area, so they are part
of the decomposition too.

A [`Segment`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html "Unoriented closed segment between two endpoints plus optional segment label.") is the thinnest operand of the set, and the one that shows plainest
that it is the *receiver's* concavity, not the summand's size, that calls for a
region. It has no area at all, and dragging a non-convex shape along one sweeps a
band that closes a cut exactly as a wider summand does:

```c++
pgl::Polygon<> c({0,0, 8,0, 8,3, 6,3, 6,2, 2,2, 2,6, 6,6, 6,5, 8,5, 8,8, 0,8});
auto plugged = c.minkowskiSum(pgl::Segment(0,0, 0,2));
// plugged.size() == 1; outer ring (0,0)--(8,10), one hole, (2,4)--(6,6)
```

It is also the cheapest: a segment is one convex piece, so the sum costs one
convex merge per piece of the receiver's decomposition. An [`OrientedSegment`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html "Directed segment preserving source-to-target order plus optional segment label.")
answers identically — an orientation is not part of a point set.

A [`Polyline`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polyline.html "Open polygonal chain stored in traversal order; may self-intersect.") carries the same second `minkowskiSum`, against the same seven
operands: [`Polygon`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html "Closed simple polygon stored by its vertices."), [`PolygonWithHoles`](https://gfonsecabr.github.io/pgl/structpgl_1_1PolygonWithHoles.html "Closed region bounded by one outer simple polygon minus disjoint polygonal holes."), [`Convex`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html "Closed convex polygon stored by its vertices."), [`Triangle`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html "Closed triangle stored by three vertices.") and [`Rectangle`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html "Axis-aligned rectangle stored by minimum and maximum corners."),
every bounded shape with area to sweep, plus [`Segment`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html "Unoriented closed segment between two endpoints plus optional segment label.") and [`OrientedSegment`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html "Directed segment preserving source-to-target order plus optional segment label."),
which have none. The chain has none of its own either, and the sum still needs a
region: dragging a shape along a chain that comes back on itself closes the swept
material over a hole, and a closed chain is the plainest example there is.

```c++
pgl::Polyline<> square({0,0, 8,0, 8,8, 0,8, 0,0});   // the boundary, traced once
auto frame = square.minkowskiSum(pgl::Rectangle(0,0, 1,1));
// frame.size() == 1; its outer ring is (0,0)--(9,9) and it has one hole,
// (1,1)--(8,8) — the cavity the chain encloses, eroded by the summand.
```

The two non-convex operands are where *both* sides may be concave, so either one's
concavity can strand a cavity; either order spells the same call, since [`Polygon`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html "Closed simple polygon stored by its vertices.")
and [`PolygonWithHoles`](https://gfonsecabr.github.io/pgl/structpgl_1_1PolygonWithHoles.html "Closed region bounded by one outer simple polygon minus disjoint polygonal holes.") carry the mirror overload:

```c++
pgl::Polygon<> u({0,0, 6,0, 6,6, 4,6, 4,2, 2,2, 2,6, 0,6});   // a U
auto swept = square.minkowskiSum(u);            // == u.minkowskiSum(square)
// swept.size() == 1; outer ring (0,0)--(14,14), one hole, (6,6)--(8,8)
```

A region operand behaves here as it does on the receivers above — its holes are
where its decomposition has no piece, and its slits sweep out area along the chain
like anything else. That is where the regularization becomes visible, because a
chain can drag a slit *along its own direction*: the sweep is then a segment, part
of $A \oplus B$ that no region may keep, so the answer is smaller than the point
set. The same contract shows up more plainly still in a summand with no area at
all, which leaves nothing to keep:
`polyline.minkowskiSum(pgl::Rectangle(3,3, 3,3))` comes back **empty** rather than
as the translated chain, which is what the single-shape `polyline + point` is for.

A [`Segment`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html "Unoriented closed segment between two endpoints plus optional segment label.") summand is where that regularization is easiest to trip over, since
the chain's own edges are what sweep: an edge *parallel* to the segment sweeps a
segment, which is dropped. A closed square chain summed with a vertical segment
therefore comes back as **two** disjoint bands — the sum of two connected shapes
is connected, but $\mathrm{closure}((A \oplus B)^\circ)$ need not be, which is the
other reason this entry point returns a set of regions rather than one.

```c++
pgl::Polyline<> square({0,0, 8,0, 8,8, 0,8, 0,0});
square.minkowskiSum(pgl::Segment(0,0, 2,1));   // one region, one hole
square.minkowskiSum(pgl::Segment(0,0, 0,3));   // two regions, (0,0)--(8,3) and (0,8)--(8,11)
```

Those seven are the whole list. `polyline + polyline` is not a pair — sum the
edges of one against the other if you want it — and neither is a [`MonotoneChain`](https://gfonsecabr.github.io/pgl/structpgl_1_1MonotoneChain.html "Weakly x-monotone polyline stored by lexicographically sorted vertices.")
receiver, which `asPolyline()` converts when its sum is wanted.

The remaining pairs are a compile error rather than an approximation: [`Disk`](https://gfonsecabr.github.io/pgl/structpgl_1_1Disk.html "Closed Euclidean disk stored by boundary points plus optional disk label.") sums
to a rounded shape, and an unbounded operand ([`Line`](https://gfonsecabr.github.io/pgl/structpgl_1_1Line.html "Unoriented infinite line."), [`Ray`](https://gfonsecabr.github.io/pgl/structpgl_1_1Ray.html "Half-infinite line starting from one source point plus optional ray label."), [`Halfplane`](https://gfonsecabr.github.io/pgl/structpgl_1_1Halfplane.html "Closed half-plane defined by an oriented boundary line."),
[`HalfplaneIntersection`](https://gfonsecabr.github.io/pgl/structpgl_1_1HalfplaneIntersection.html "Intersection of closed half-planes; convex but possibly unbounded or empty.")) to an unbounded region, neither of which is
representable. Since $\mathrm{hull}(A \oplus B) = \mathrm{hull}(A) \oplus
\mathrm{hull}(B)$, a caller who wants the convex approximation can ask for it
explicitly by summing the hulls. On the polymorphic [`Shape`](https://gfonsecabr.github.io/pgl/structpgl_1_1Shape.html "Runtime variant wrapper over the supported primitive shapes.") the operand pair is
only known at run time, so an unsupported pair throws `std::logic_error`
instead — and so does a pair whose sum is a set of regions, which no single
[`Shape`](https://gfonsecabr.github.io/pgl/structpgl_1_1Shape.html "Runtime variant wrapper over the supported primitive shapes.") alternative can hold.

### Other Methods for Shapes

- `rotated90(int k = 1)`: Returns the shape rotated by `90k` degrees around the
  origin.

- `rotate90(int k = 1)`: Rotates the shape by `90k` degrees around the origin.

- `scaledUpX(Number)`: Returns the shape with the x-coordinate multiplied by a
  number.

- `scaleUpX(Number)`: Multiplies the x-coordinate by a number.

- `scaledUpY(Number)`: Returns the shape with the y-coordinate multiplied by a
  number.

- `scaleUpY(Number)`: Multiplies the y-coordinate by a number.

- `scaledDownX(Number)`: Returns the shape with the x-coordinate divided by a
  number.

- `scaleDownX(Number)`: Divides the x-coordinate by a number.

- `scaledDownY(Number)`: Returns the shape with the y-coordinate divided by a
  number.

- `scaleDownY(Number)`: Divides the y-coordinate by a number.

- `squaredDistance<ResultNumber = NumberType>(Shape)`: Returns the squared
  distance, computed in `ResultNumber` (default: the shape's coordinate type),
  mirroring `intersection`. **Warning:** distances to a line, segment or ray
  divide by a squared length, so with an integer `ResultNumber` the result is
  truncated; request a floating-point or [`Rational`](https://gfonsecabr.github.io/pgl/classpgl_1_1Rational.html "Exact rational number class template.") type, e.g.
  `a.squaredDistance<double>(b)`, for an accurate value. Distances between
  points and between axis-aligned rectangles use no division and are exact.

- `squaredHausdorffDistance<ResultNumber = NumberType>(Shape)`: Returns the
  squared Hausdorff distance, with the same `ResultNumber` convention and
  truncation warning as `squaredDistance`. Defined for every pair among
  [`Point`](https://gfonsecabr.github.io/pgl/structpgl_1_1Point.html "Two-dimensional point with optional label payload."), [`Segment`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html "Unoriented closed segment between two endpoints plus optional segment label."), [`OrientedSegment`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html "Directed segment preserving source-to-target order plus optional segment label."), [`Rectangle`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html "Axis-aligned rectangle stored by minimum and maximum corners."), [`Triangle`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html "Closed triangle stored by three vertices."), and [`Convex`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html "Closed convex polygon stored by its vertices.")
  — all bounded, convex shapes, so the directed distance in either direction
  is always attained at a vertex. Not defined for [`Line`](https://gfonsecabr.github.io/pgl/structpgl_1_1Line.html "Unoriented infinite line."), [`OrientedLine`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedLine.html "Directed infinite line with left/right side semantics plus optional line label."),
  [`Ray`](https://gfonsecabr.github.io/pgl/structpgl_1_1Ray.html "Half-infinite line starting from one source point plus optional ray label."), [`Halfplane`](https://gfonsecabr.github.io/pgl/structpgl_1_1Halfplane.html "Closed half-plane defined by an oriented boundary line."), or [`HalfplaneIntersection`](https://gfonsecabr.github.io/pgl/structpgl_1_1HalfplaneIntersection.html "Intersection of closed half-planes; convex but possibly unbounded or empty.") (unbounded, or possibly
  unbounded, so the Hausdorff distance to or from them is generally infinite),
  nor yet for [`Disk`](https://gfonsecabr.github.io/pgl/structpgl_1_1Disk.html "Closed Euclidean disk stored by boundary points plus optional disk label."), [`MonotoneChain`](https://gfonsecabr.github.io/pgl/structpgl_1_1MonotoneChain.html "Weakly x-monotone polyline stored by lexicographically sorted vertices."), or [`Polygon`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html "Closed simple polygon stored by its vertices.").

- `distanceL1(Shape)` / `distanceLInf(Shape)`: Return the Manhattan (L1) or
  Chebyshev (LInf) distance to the given shape. Neither metric needs
  squaring to stay exact, so [`Point`](https://gfonsecabr.github.io/pgl/structpgl_1_1Point.html "Two-dimensional point with optional label payload.")-to-[`Point`](https://gfonsecabr.github.io/pgl/structpgl_1_1Point.html "Two-dimensional point with optional label payload.") returns an exact value with
  no `ResultNumber` template. Every other pair is
  `distanceL1<ResultNumber = NumberType>(Shape)` /
  `distanceLInf<ResultNumber = NumberType>(Shape)`, with the same
  `ResultNumber` convention and truncation warning as `squaredDistance` (a
  non-axis-aligned segment, ray, or line generally has a fractional exact
  distance). Defined for every pair among [`Point`](https://gfonsecabr.github.io/pgl/structpgl_1_1Point.html "Two-dimensional point with optional label payload."), [`Segment`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html "Unoriented closed segment between two endpoints plus optional segment label."),
  [`OrientedSegment`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html "Directed segment preserving source-to-target order plus optional segment label."), [`Line`](https://gfonsecabr.github.io/pgl/structpgl_1_1Line.html "Unoriented infinite line."), [`OrientedLine`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedLine.html "Directed infinite line with left/right side semantics plus optional line label."), [`Ray`](https://gfonsecabr.github.io/pgl/structpgl_1_1Ray.html "Half-infinite line starting from one source point plus optional ray label."), [`Halfplane`](https://gfonsecabr.github.io/pgl/structpgl_1_1Halfplane.html "Closed half-plane defined by an oriented boundary line."), [`Rectangle`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html "Axis-aligned rectangle stored by minimum and maximum corners."),
  [`Triangle`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html "Closed triangle stored by three vertices."), [`Convex`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html "Closed convex polygon stored by its vertices."), [`MonotoneChain`](https://gfonsecabr.github.io/pgl/structpgl_1_1MonotoneChain.html "Weakly x-monotone polyline stored by lexicographically sorted vertices."), [`Polygon`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html "Closed simple polygon stored by its vertices."), and
  [`HalfplaneIntersection`](https://gfonsecabr.github.io/pgl/structpgl_1_1HalfplaneIntersection.html "Intersection of closed half-planes; convex but possibly unbounded or empty."), plus [`Disk`](https://gfonsecabr.github.io/pgl/structpgl_1_1Disk.html "Closed Euclidean disk stored by boundary points plus optional disk label.")-[`Point`](https://gfonsecabr.github.io/pgl/structpgl_1_1Point.html "Two-dimensional point with optional label payload."): like [`Disk`](https://gfonsecabr.github.io/pgl/structpgl_1_1Disk.html "Closed Euclidean disk stored by boundary points plus optional disk label.")'s
  other overloads this always returns `double`, since there is no closed
  form for the distance from a point to a circle under either metric and it
  is instead found with a numeric search. The remaining [`Disk`](https://gfonsecabr.github.io/pgl/structpgl_1_1Disk.html "Closed Euclidean disk stored by boundary points plus optional disk label.") pairs ([`Disk`](https://gfonsecabr.github.io/pgl/structpgl_1_1Disk.html "Closed Euclidean disk stored by boundary points plus optional disk label.")
  against any shape other than [`Point`](https://gfonsecabr.github.io/pgl/structpgl_1_1Point.html "Two-dimensional point with optional label payload."), and [`Disk`](https://gfonsecabr.github.io/pgl/structpgl_1_1Disk.html "Closed Euclidean disk stored by boundary points plus optional disk label.")-[`Disk`](https://gfonsecabr.github.io/pgl/structpgl_1_1Disk.html "Closed Euclidean disk stored by boundary points plus optional disk label.")) are not yet
  implemented — see [todo](todo.md).

- `hausdorffDistanceL1(Shape)` / `hausdorffDistanceLInf(Shape)`: Return the
  L1 or LInf Hausdorff distance, with the same `ResultNumber` convention as
  `distanceL1` / `distanceLInf`. Defined for the same pairs as
  `squaredHausdorffDistance`: [`Point`](https://gfonsecabr.github.io/pgl/structpgl_1_1Point.html "Two-dimensional point with optional label payload."), [`Segment`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html "Unoriented closed segment between two endpoints plus optional segment label."), [`OrientedSegment`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html "Directed segment preserving source-to-target order plus optional segment label."),
  [`Rectangle`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html "Axis-aligned rectangle stored by minimum and maximum corners."), [`Triangle`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html "Closed triangle stored by three vertices."), and [`Convex`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html "Closed convex polygon stored by its vertices.").

- `bbox()`: Returns the minimum bounding box of the shape.

- `fbox<T>()`: Returns a bounding box of the shape using floating point coordinates of type `T`. The bounding box may not be minimum but must contain the entire shape. The `min` coordinates are rounded down and the `max` are rounded up to the nearest floating point. If `!s1.fbox().intersects(s2.fbox()))` then `!s1.bbox().intersects(s2.bbox()))`. Also, if `s1.fbox().crosses(s2.fbox()))` then `s1.bbox().crosses(s2.bbox()))`.

- `area()`: Returns the area.

- `twiceArea()`: Returns two times the area.

- `diameter()`: Returns a segment that defines the diameter.

- `pointInside()`: Returns a point strictly in the interior of the shape. Uses
  only division by a power of 2.

- `pointInsideInteriorContainedIn(other)`: Returns true if some point in this
  shape's relative interior lies in the strict interior of the argument `other`.
  It uses the `pointInside()` witness, scaling both shapes to keep the witness
  exact when integer truncation would round it onto the boundary.

- `verticesContain(p)`: Returns true if there exists a value `i` such that `s[i] == p` for the shape `s`. Notice that two shapes (for example lines) may be equal (according to `==`) but still behave differently for verticesContain if they are defined by different points.

## Iterating

There are several methods to iterate through vertices, edges, or oriented
edges. An [`std::array`](https://en.cppreference.com/w/cpp/container/array.html)
is used for shapes of constant size and an
[`std::vector`](https://en.cppreference.com/w/cpp/container/vector.html) is
used otherwise.

- `vertices()`: Returns an `std::array` or an `std::vector` of [`Point`](https://gfonsecabr.github.io/pgl/structpgl_1_1Point.html "Two-dimensional point with optional label payload.") that are
  the vertices. 

- `edges()`: Returns an `std::array` or an `std::vector` of [`Segment`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html "Unoriented closed segment between two endpoints plus optional segment label.") that are
  the edges.

- `orientedEdges()`: Returns an `std::array` or an `std::vector` of
  [`OrientedSegment`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html "Directed segment preserving source-to-target order plus optional segment label.") that are the edges in counterclockwise order. Not defined
  for [`Disk`](https://gfonsecabr.github.io/pgl/structpgl_1_1Disk.html "Closed Euclidean disk stored by boundary points plus optional disk label.").

- `begin()`, `end()`, `edgesBegin()`, `edgesEnd()`, `orientedEdgesBegin()`,
  `orientedEdgesEnd()`: Same as `vertices()`, `edges()`, and
  `orientedEdges()` above, but for iterators that take `O(1)` time per element
  visited.

### Indexed access

Every shape exposes a uniform indexed-access interface over its defining
points (or, for [`Point`](https://gfonsecabr.github.io/pgl/structpgl_1_1Point.html "Two-dimensional point with optional label payload."), its two coordinates):

- `size()`: Returns the number of indexable elements.

- `s[i]`: Returns the `i`-th element.

- `s.get(i)`: Same as `s[i]` but `i` is taken modulo `s.size()`, so negative
  values wrap from the end.

- `s.index(p)`: Returns the smallest index `i` such that `s[i] == p`, or -1
  if no such index exists.

```c++
pgl::Convex c({{0,0},{4,0},{4,3},{0,3}});
c[2];           // (4,3)
c.get(-1);      // (0,3) same as c[3]
c.get(5);       // (4,0) same as c[1]
c.index({4,3}); // 2 since c[2] == {4,3}
```


The runtime [`Shape`](https://gfonsecabr.github.io/pgl/structpgl_1_1Shape.html "Runtime variant wrapper over the supported primitive shapes.") wrapper exposes `size()`, `operator[]`, and `get()` that
dispatch to the wrapped alternative. Because [`Point`](https://gfonsecabr.github.io/pgl/structpgl_1_1Point.html "Two-dimensional point with optional label payload.")'s
indexed access yields a coordinate rather than a [`Point`](https://gfonsecabr.github.io/pgl/structpgl_1_1Point.html "Two-dimensional point with optional label payload."), `Shape::operator[]`
and `Shape::get` throw `std::logic_error` if the wrapped value is a [`Point`](https://gfonsecabr.github.io/pgl/structpgl_1_1Point.html "Two-dimensional point with optional label payload.").




