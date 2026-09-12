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

> ℹ️ **Pre-release**: pgl is extensively tested, but it has not had a stable release yet and its API may still change.

## Polymorphism with `Shape`

Most of pgl is static: the pair of [shapes](shapes.md) is known at compile time and the tightest result type is chosen there. `Shape` is the runtime alternative — a `std::variant` over every shape, with the whole common interface forwarded to whichever one it holds.

```C++
pgl::Shape p = pgl::Point(3,7);
pgl::Shape s = pgl::Segment(1,4,2,9);
pgl::Shape r = pgl::Rectangle(1,4,2,9);
if (r.contains(p))
    std::cout << r << " contains " << p << std::endl;
if (r.intersects(s))
    std::cout << r << " intersects " << s << std::endl;
```

The alternatives, in storage order, are `EmptyShape`, `Point`, `Segment`, `OrientedSegment`, `Line`, `OrientedLine`, `Ray`, `Halfplane`, `Rectangle`, `Triangle`, `Disk`, `Convex`, `MonotoneChain`, `Polyline`, `Polygon`, `HalfplaneIntersection`, `PolygonWithHoles` and `PolygonSet`. A default-constructed `Shape` holds the [`EmptyShape`](shapes.md#emptyshape).

Like every other shape, `Shape` is templated on a point type, `pgl::Shape<pgl::Point<int>>` by default. All alternatives share that same point type: a `Shape<Point<int>>` can hold a `Segment<Point<int>>` but not a `Segment<Point<double>>`. The [exact type aliases](types.md#exact-type-aliases) name the common instantiations, `pgl::EShape` for `pgl::Shape<pgl::EPoint>`.

```C++
pgl::Shape<pgl::Point<double>> e;   // holds EmptyShape
e.empty();                          // true
e = pgl::Disk<pgl::Point<double>>(...);
e.empty();                          // false
```

`Shape` does what the shape it holds does, and throws `pgl::unsupported_operation` (a `std::logic_error`) when that shape cannot: a bounding box of a `Line`, the vertices of a `Disk`, the `intersection` of a `Disk` and a `Segment`. The message names the operation and the alternatives, e.g. `pgl::Shape: bbox(Line) is not supported` or `pgl::Shape: intersection(Segment, Disk) is not supported`. Nothing answers in place of a missing implementation, and no pair is listed anywhere: a pair gains `Shape` support as soon as the concrete member for it exists.

### Storage against geometry

Two vocabularies never share a word. `is…`, `getIf…` and `as…` are geometric and answer as the stored shape does; `holds…`, `getIfHolds…` and `asHeld…` ask which alternative is stored.

```C++
pgl::Shape c = pgl::Triangle(2,2,2,2,2,2);  // collapsed to a point
c.holdsTriangle();                          // true: the Triangle alternative is stored
c.holdsPoint();                             // false
c.isPoint();                                // true: the point set is a single point
c.getIfPoint();                             // std::optional holding (2,2)
c.getIfHoldsTriangle()->isPoint();          // the same answer, through the stored triangle
```

- `s.holds<T>()`, `s.getIfHolds<T>()`, `s.asHeld<T>()`: Whether the stored alternative is `T`, a pointer to it (`nullptr` otherwise), and a reference to it (throws `std::bad_variant_access` otherwise). Every alternative, `EmptyShape` included, has named shorthands: `holdsSegment()`, `getIfHoldsSegment()`, `asHeldSegment()`, and so on.
- `s.visit(f)`: Calls `f` with the stored alternative, in a `const` and a mutable overload. `s.variant()` exposes the `std::variant` itself, for `std::visit` over several shapes at once.

### Construction and conversion

- A `Shape` is constructed and assigned from any alternative implicitly.
- It is also constructible from a `std::variant` of alternatives, or a `std::optional` of one — the return types of the typed [intersection](shape_methods.md#intersection) methods. An absent optional gives the empty shape, and deduction reads the point type off the variant, so `pgl::Shape i = a.intersection(b);` names the right type for a result that is not a range.
- `pgl::pieces(result)` splits any concrete result into a `std::vector<Shape>`, one per connected piece, dropping what covers no point. It accepts a shape, an optional, a variant, a vector of either, a `PolygonSet` (one piece per component) and a `Shape`.

```C++
auto i = pgl::Shape(a.intersection(b));   // point, segment or empty
auto p = pgl::pieces(a.intersection(b));  // the same, as pieces
```

- `pgl::Shape<pgl::Point<double>>(s)` converts a `Shape` over another point type alternative by alternative, and converts a concrete shape of another point or label type into the alternative of its kind. Both conversions are explicit; the conversion between `Shape` types throws for an alternative with no such conversion.
- Naming the alternative's type extracts it — `pgl::Segment<> q(s)`, or a `static_cast` — throwing `std::bad_variant_access` on a mismatch, as `asHeld<T>()` does.

### Queries about the point set

- `s.empty()`: Whether the point set covers no point. True for the `EmptyShape` alternative and for any alternative in its own empty state, so a `Shape` holding an empty `Rectangle` is `empty()` without holding `EmptyShape`.
- `s.isPoint()`, `s.isSegment()`, `s.getIfPoint<ResultNumber>()`, `s.getIfSegment<ResultNumber>()`: The collapsed point set, as the stored shape reports it ([degeneracies](shapes.md#degeneracies)). The `getIf…` pair returns an `std::optional`, and throws for a `HalfplaneIntersection` that collapses off the lattice of an integral `ResultNumber`.
- `s.isDegenerate()`, `s.isUndefined()`: The stored shape's own answers.
- `s.dimension()`: `-1` for an empty point set, `0` for a point, `1` for a curve, `2` for a region — of the point set, so a triangle with collinear vertices is `1` and a collapsed one `0`.
- `s.isBounded()`: False for `Line`, `OrientedLine`, `Ray`, `Halfplane` and an unbounded `HalfplaneIntersection`; true otherwise, the empty shape included.
- `s.bbox()`: The bounding box, in the stored point type. Throws for the unbounded alternatives, for `EmptyShape`, and for an empty or unbounded `HalfplaneIntersection`.
- `s.fbox<ResultNumber>()`: The floating-point box, over the same alternatives `bbox()` covers. A `Disk` computes it from its center and radius, where `bbox()` rounds that out to the stored point type.

### Queries about the defining data

These ask how the shape is written down rather than what it covers, so only the alternatives that store that kind of data answer and the rest throw.

- `s.isVertical()`, `s.isHorizontal()`, `s.slope<ResultNumber>()`: For the linear alternatives — `Segment`, `OrientedSegment`, `Line`, `OrientedLine`, `Ray` and `Halfplane`.
- `s.min()`, `s.max()`: The lexicographic pair of defining points, for the linear alternatives and a `Rectangle`'s two corners. Copies, not the references the concrete shapes return.
- `s.source()`, `s.target()`: The ordered pair, for the alternatives that carry an orientation — `OrientedSegment`, `OrientedLine`, `Ray` and `Halfplane`.
- `s.isSimple()`: Whether the ring or chain does not cross itself, for `Polyline`, `Polygon`, `PolygonWithHoles` and `PolygonSet`.

### Sequences

- `s.vertices<ResultNumber>()`, `s.edges()`, `s.orientedEdges()`: The stored shape's sequences, as vectors. A `Point` is its one vertex and a `Ray` its source; `EmptyShape`, `Line`, `OrientedLine` and `Halfplane` have none. `vertices()` throws for a `Disk`, and for a `HalfplaneIntersection` vertex off the lattice of an integral `ResultNumber` — ask for a rational one, `vertices<pgl::ERational>()`. The edge sequences throw for every alternative with no edges: `Line`, `OrientedLine`, `Ray`, `Halfplane`, `Disk` and `HalfplaneIntersection`.
- `s.latticePoints<ResultNumber>()`: The integer points the stored shape contains. Throws for `EmptyShape`, `Point` and the unbounded alternatives, and for an unbounded `HalfplaneIntersection`.
- `s.vertexCount()`: The vertex count of the alternatives whose vertices span several rings or components, `HalfplaneIntersection`, `PolygonWithHoles` and `PolygonSet`. Every other alternative throws here and counts with `vertices().size()` — not with `size()`, which counts defining points, two for a `Line` that has no vertex at all.
- `s.size()`, `s.get(i)`, `s[i]`, `s.index(point)`: [Indexed access](shape_methods.md#indexed-access) to the stored shape's defining points. `size()` throws for `PolygonWithHoles` and `PolygonSet`, whose vertices are spread over several rings or components. The point-valued accessors throw for those two, for `EmptyShape`, which has no element, and for the alternatives whose elements are not vertices: `Point`, whose elements are coordinates, and `HalfplaneIntersection`, whose elements are half-planes. Reach through with `getIfHolds…` to use the concrete shape's own accessors.

### Measures and distances

- `s.area<ResultNumber>()`, `s.length<ResultNumber>()`, `s.centroid<ResultNumber>()`: Forwarded to the stored shape, and throwing for an alternative that has none. A `Disk` computes its area in floating point and converts.
- `squaredDistance`, `distanceL1`, `distanceLInf`, `squaredHausdorffDistance`, `hausdorffDistanceL1`, `hausdorffDistanceLInf`: The [distances](shape_methods.md#other-methods-for-shapes) of the concrete pair, throwing for a pair with none. They default to `division_result_t<NumberType>` rather than to the pair-specific default, and a `Disk` pair's floating answer is converted into it.
- `s.closestPoints<ResultNumber>(t)`, `s.closestSegments<ResultNumber>(t)`: The pair of points, or of elements, realizing the distance, and nothing when the shapes meet. A `Shape` accepts every pair and throws for one the concrete shapes do not define.
- `s.twiceArea<ResultNumber>()`, `s.diameter<ResultNumber>()`: Both default to `division_result_t<NumberType>` where the concrete shapes default to the native type, because a `HalfplaneIntersection` and a three-point `Disk` divide; ask for `twiceArea<NumberType>()` to get the concrete shapes' own type. `twiceArea` throws for `EmptyShape`, `Point`, `Halfplane`, `Disk`, `MonotoneChain` and `Polyline`; `diameter` throws for `EmptyShape`, `Point`, the unbounded alternatives and every `HalfplaneIntersection`.
- `s.lengthL1<ResultNumber>()`, `s.lengthLInf<ResultNumber>()`, `s.squaredLength<ResultNumber>()`: The exact lengths, for `Segment` and `OrientedSegment`, and the first two also for `MonotoneChain` and `Polyline`.
- `s.pointInside<ResultNumber>()`, `s.midpoint<ResultNumber>()`, `s.center<ResultNumber>()`, `s.verticesCentroid<ResultNumber>()`: The stored shape's constructed points. `pointInside()` answers for every alternative but `EmptyShape` and `Point`; `midpoint()` for `Segment`, `OrientedSegment` and `Rectangle`; `center()` for `Rectangle` and `Disk`; `verticesCentroid()` for `Convex`, `Polygon`, `PolygonWithHoles` and `PolygonSet`.

### Predicates

`contains`, `boundaryContains`, `interiorContains`, `intersects`, `interiorsIntersect`, `separates`, `crosses` and `samePointSet` answer as the stored shape answers against the other operand ([predicates](shape_methods.md#predicates)). Every shape defines every predicate against every alternative, so these are the operations a `Shape` never throws for.

The predicates about the defining data are defined for far fewer pairs, and throw for the rest.

- `s.verticesContain(p)`, `s.containsEndpoint(p)`, `s.containsCollinear(p)`, `s.orientation(p)`: Each needs a `Point` on the right, wrapped or concrete, and throws for anything else. `verticesContain` answers for the linear alternatives, `Rectangle`, `Triangle` and `Convex`; `containsEndpoint` for `Segment` and `OrientedSegment`; `containsCollinear` for those two and a `Ray`; `orientation` for `OrientedSegment`, `OrientedLine` and `Ray`.
- `s.parallel(t)`, `s.collinear(t)`: Both sides must be linear, except that `collinear` also takes a `Point`.
- `s.interiorContainsInterior(t)`: A `Polygon`, `PolygonWithHoles` or `PolygonSet` against a segment.
- `s.pointInsideInteriorContainedIn(t)`: Whether this shape's `pointInside()` witness lies in the strict interior of `t`. Throws for the `EmptyShape`, `Polyline` and `PolygonSet` alternatives.

### Constructions

- `s.intersection<ResultNumber>(t)`: The connected pieces of the point-set intersection, as a `std::vector<Shape>` — empty for a disjoint pair, one element when the intersection is connected. Throws for a pair with no `intersection`, such as anything against a `Disk`.
- `s.regularizedUnion<ResultNumber>(t)`, `s.difference<ResultNumber>(t)`, `s.regularizedIntersection<ResultNumber>(t)`, `s.symmetricDifference<ResultNumber>(t)`: The [boolean operations](shape_methods.md#boolean-operations), returning a `PolygonSet` rather than a `Shape`, since every pair that has one answers with a set of regions. `regularizedUnion` and `symmetricDifference` need both alternatives to be a bounded polygonal region — `Rectangle`, `Triangle`, `Convex`, `Polygon`, `PolygonWithHoles` or `PolygonSet`. `difference` needs that on the left and admits a `Halfplane` or `HalfplaneIntersection` as the subtrahend. `regularizedIntersection` needs a `PolygonWithHoles` or `PolygonSet` on one side.
- `s.minkowskiSum(t)`, `s.minkowskiErosion(t)`: The [Minkowski sum](shape_methods.md#minkowski-sum) and [erosion](shape_methods.md#minkowski-erosion), re-wrapped in a `Shape`. Both compile for every operand and throw when the stored pair has no such answer, or when it has one the wrapper's point type cannot hold — an erosion that is a rational `HalfplaneIntersection` does not fit a `Shape` over integer points.
- `s.convexHull<ResultNumber>()`: Throws for the empty shape, the unbounded alternatives and a `Disk`, and for a `HalfplaneIntersection` that is empty, unbounded, or has a vertex off the lattice of an integral `ResultNumber`.
- `s.asPolygonWithHoles()`: The stored shape as a region, for the alternatives that have such a conversion.
- `s.asLine()`, `s.asOrientedLine()`, `s.asPolyline()`, `s.asPolygon()`, `s.asPolygonSet()`, `s.asHalfplaneIntersection()`, `s.asConvex<ResultNumber>()`: The stored shape's own conversions, each throwing for an alternative that has none. An alternative that is already the target type comes back as it is. `asConvex` takes a `ResultNumber` because a `HalfplaneIntersection`'s vertices are implicit.
- `s.halfplaneAbove()`, `s.halfplaneBelow()`, `s.leftHalfplane()`, `s.rightHalfplane()`: The half-planes a linear shape bounds — the first pair for `Line`, `OrientedLine` and `Ray`, the second for `OrientedSegment`, `OrientedLine` and `Ray`.
- `s.opposite()`: The stored shape with its orientation reversed, as a `Shape` holding the same alternative. For `OrientedSegment`, `OrientedLine`, `Ray` and `Halfplane`.
- `s.circumcircle()`: The circumscribed `Disk` of a `Rectangle` or a `Triangle`.
- `s.dual<ResultNumber>()`, `s.polar<ResultNumber>()`: The [dual and polar](shape_methods.md#other-methods-for-shapes) images, as a `Shape` — a line for a `Point` and a point for a `Line`, so which alternative comes back is known only at run time. Both default to `division_result_t<NumberType>` for that reason: the `Line` direction divides.
- `s.convexPartition()`, `s.convexCovering()`, `s.triangulation()`, `s.asBitMatrix<ResultNumber>()`: The region decompositions, for `Polygon`, `PolygonWithHoles` and `PolygonSet`. `asBitMatrix` still requires a rectilinear region.

### Mixing wrapped and concrete operands

Every binary operation takes a `Shape` or a concrete shape on either side, so the two styles mix freely.

```C++
pgl::Shape r = pgl::Rectangle(1,4,2,9);
r.intersects(pgl::Segment(0,0,5,5));   // concrete argument
pgl::Segment(0,0,5,5).intersection(r); // concrete receiver, wrapped argument: a std::vector<Shape>
```

The answer comes back in the wrapper's shape, not the receiver's: `segment.intersection(shape)` is a vector of `Shape`, where `segment.intersection(otherSegment)` is the tight `std::optional<std::variant<Point, Segment>>`, because which alternative the argument holds is not known until run time.

A concrete receiver takes a `Shape` for a construction or a distance only when it has that operation against some alternative, so a call no stored alternative could answer stays a compile error instead of a call certain to throw: `segment.regularizedUnion(shape)` does not compile, while `Shape(segment).regularizedUnion(shape)` compiles and throws. The predicates need no such gate.

### Transformations

- `+=`, `-=`, `*=`, `/=`, `rotate90(k)` and the in-place axis scalings act on the stored shape and preserve both the alternative and the coordinate type; `rotated90(k)` and the `scaled…` methods return a `Shape` of the same type. The free `+`, `-`, `*` and `/` promote the point type the way the concrete shapes do.
- The axis scalings (`scaledUpX`, `scaleUpX` and their siblings) throw for a `Disk`, whose result would be an ellipse.
- `transformation * s` applies an [affine map](shape_methods.md#transformations) to the stored shape, and throws for the `Rectangle` and `Disk` alternatives, which cannot represent a parallelogram or an ellipse.

### Comparison, hashing and output

- `==` compares the stored alternative and value, so a `Rectangle` and a `Convex` with the same point set differ; `samePointSet` compares point sets instead. `<=>` orders by alternative first, then by value.
- `std::hash` is specialized, so a `Shape` goes directly into a `std::unordered_set` or `std::unordered_map`, and the ordering puts it into a `std::set`.
- `operator<<` streams the stored alternative, and a [`Canvas`](canvas.md) draws it. An [`Arrangement`](data_structures.md#arrangement) accepts a container of `Shape` as input.

- Other methods:
