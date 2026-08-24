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

The following methods apply to many shapes and fall into several groups:

- [Operators](#operators) translate and scale shapes with arithmetic syntax.
- [Transformations](#transformations) apply and compose affine maps.
- [Predicates](#predicates) test geometric relationships between shapes.
- [Intersection](#intersection) constructs literal point-set intersections.
- [Boolean Operations](#boolean-operations) construct regularized operations on regions.
- [Minkowski Sum](#minkowski-sum) adds every point of one shape to every point of another.
- [Minkowski Erosion](#minkowski-erosion) keeps the placements of one shape that stay inside another.
- [Other Methods for Shapes](#other-methods-for-shapes) covers measurements, bounding boxes, rotations, and related helpers.
- [Iterating](#iterating) traverses vertices and edges, including through [indexed access](#indexed-access).

### Operators

Shapes are translated by adding or subtracting a point. The point coordinates are added to, or subtracted from, every defining point of the shape.

```c++
pgl::Point p = {2,3}, q = {4,5};
pgl::Segment s = {p, q},    //  s = (2,3)--(4,5)
             t1 = p + s,    // t1 = (4,6)--(6,8)
             t2 = s - p;    // t2 = (0,0)--(2,2)
```

Adding a point is the special case of adding two shapes, which is their [Minkowski sum](#minkowski-sum), described later.

In-place translations use `+=` and `-=`. Scaling around the origin uses the operator `*` or `*=` with a scalar.

```c++
pgl::Segment s = {2, 3, 4, 5};    //  s = (2,3)--(4,5)
s += pgl::Point(1,2);             //  s = (3,5)--(5,7)
s *= 10;                          //  s = (30,50)--(50,70)
```

If we want to scale around a particular point `p`, we can use a combination of the previous operators:

```c++
pgl::Segment s = {2,3,4,5};        // s = (2,3)--(4,5)
auto exactMidpoint = s.midpoint(); // Point<ERational>(3,4) by default
pgl::Point p = s.midpoint<int>();  // safe here because this midpoint is integral
pgl::Segment t = 3*(s-p) + p;      // t = (0,1)--(6,7)
```

### Transformations

`pgl::Transformation<Number>` stores a general affine map as a 2x3 matrix containing a 2x2 linear part and a translation. The operator `*` applies it to a shape or composes it with another transformation, so `t1 * t2 * shape` applies the right-hand transformation first.

```c++
pgl::Segment s = {0,0,5,5};
auto t = pgl::Transformation<int>::rotation90(1) * pgl::Transformation<int>::translation(2,0);
auto rotated = t * s;
std::cout << rotated; // Prints (-5,7)--(0,2)
```

Factories cover the common exact cases: `identity()`, `translation(dx,dy)`, `scaling(sx,sy=sx)`, `rotation90(k=1)` (exact multiples of 90 degrees), `shearX(k)`, `shearY(k)`, `reflectionX()`, and `reflectionY()`. An arbitrary-angle `rotation<ResultNumber=double>(radians)` is also available but, unlike `rotation90`, returns a floating-point transformation since a general angle is generally irrational.

`determinant()` is negative exactly when the transformation reverses orientation, as a reflection does. Shapes with a winding or normalization invariant (`Triangle`, `Convex`, `MonotoneChain`, and `Polygon`) renormalize automatically through their own constructors, and `Halfplane` swaps its source and target to keep the transformed interior on the correct side.

`inverse<ResultNumber>()` returns the inverse transformation. Integral transformations therefore return an exact `Transformation<ERational>` by default; floating-point and rational matrix types retain their own number type. An explicitly requested integral `ResultNumber` can still truncate the division by `determinant()`.

`Transformation` is applied to every concrete shape except `Rectangle` and `Disk`: a general affine map turns a rectangle into a parallelogram and a disk into an ellipse, and neither class can represent that. Applying a transformation to a runtime `Shape` that holds either unsupported alternative throws `std::logic_error`.

### Predicates

Many pairs of shapes `A` and `B` support the following predicates, where $\partial A$ denotes the manifold boundary of $A$, and $A^\circ = A \setminus \partial A$ is the relative interior. The boundary of a one-dimensional shape consists of its endpoints (see also [shapes](shapes.md)).

| Predicate | Definition | Question |
| --------- | ---------- | --------- |
| `A.samePointSet(B)` | $A = B$ | Do `A` and `B` define the same point set? |
| `A.contains(B)` | $A \supseteq B$ | Does `A` contain `B`? |
| `A.boundaryContains(B)` | $\partial A \supseteq B$ | Does the boundary of `A` contain `B`? |
| `A.interiorContains(B)` | $A^\circ \supseteq B$ | Does the interior of `A` contain `B`? |
| `A.intersects(B)` | $A \cap B \neq \emptyset$ | Do `A` and `B` intersect? |
| `A.interiorsIntersect(B)` | $A^\circ \cap B^\circ \neq \emptyset$ | Do the interiors of `A` and `B` intersect? |
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

All predicates are calculated exactly for integers, except for possible overflows detailed in [types](types.md). Notice that `A.samePointSet(B)` is equivalent to `A.contains(B) && B.contains(A)` but often faster and has the same behavior of `==` in some situations. However, `==` can only compare the same shape and is not always the same as point set equality. For example, a `Polygon` may have vertices along edges that make `==` return `false` while preserving the same point set.

### Intersection

The `intersection` method computes the intersection of two shapes. The return type depends on the exact pair, but it can be sent directly to a [`Canvas`](canvas.md). A `pgl::Shape` can also be constructed directly from a non-range result. The return type is typically an `std::optional` containing an `std::variant` when the result is guaranteed to be connected, and an `std::vector` of `std::variant` when the result may have multiple components. The intersection of integer shapes often has non-integer coordinates, so rational coordinates are commonly returned.

```c++
pgl::Segment s = {0,0,4,3}, t = {1,3,3,0};
std::optional<std::variant<pgl::EPoint,pgl::ESegment>> isec = s.intersection(t);
pgl::EPoint p = std::get<0>(*isec);        // p = (2,3/2)
pgl::EShape p_shape(s.intersection(t));    // A Shape storing (2,3/2)
```

The intersection of two `pgl::Polygon` objects may produce multiple pieces of different dimensions. One-dimensional components are returned as `Polyline` objects, including when a component is a single segment.

<table>
  <tr>
    <td valign="top" width="60%">

```c++
pgl::Rectangle<> a({0,0, 12,2});
pgl::Polygon<> b({1,1, 3,1, 3,5, 5,5, 6,2, 7,5, 9,5, 9,2, 11,2, 11,6, 1,6});
auto pieces = a.intersection(b);
for (pgl::EShape piece : pieces)
    std::cout << piece << '\n';

// Output (order is unspecified):
// (6,2)
// Polyline[(9,2),(11,2)]
// Polygon[(1,1),(3,1),(3,2),(1,2)]
```

  </td>
    <td valign="top" width="40%">
      <img src="figures/polygon_intersection_components.svg" alt="Two polygons whose intersection contains a point, a polyline, and a polygon" width="100%"/>
    </td>
  </tr>
</table>

The object returned as the intersection depends on the two operands and is summarized below.

| Code | Return type |
|---|---|
| oP | opt&lt;Point&gt; |
| oPS, oPL, oPR, oPLR | opt&lt;var&lt;Point, Segment&gt;&gt;, opt&lt;var&lt;Point, Line&gt;&gt;, opt&lt;var&lt;Point, Ray&gt;&gt;, opt&lt;var&lt;Point, Line, Ray&gt;&gt; |
| oPSR, oPSRL, oPSC, oRect | opt&lt;var&lt;Point, Segment, Ray&gt;&gt;, opt&lt;var&lt;Point, Segment, Ray, Line&gt;&gt;, opt&lt;var&lt;Point, Segment, Convex&gt;&gt;, opt&lt;Rectangle&gt; |
| vPS, vPSP | vec&lt;var&lt;Point, Segment&gt;&gt;, vec&lt;var&lt;Point, Segment, Polygon&gt;&gt; |
| vPPLP, vPPLW | vec&lt;var&lt;Point, Polyline, Polygon&gt;&gt;, vec&lt;var&lt;Point, Polyline, PolygonWithHoles&gt;&gt; |
| I | HalfplaneIntersection |
| — | No overload is available |

| operands | Seg.<br>OSeg. | Line<br>O.Line | Ray | Hp. | Rect. | Tri.<br>Conv. | MCh.<br>P.line | Poly. | HpI. | PWH. | P.Set |
|---|---|---|---|---|---|---|---|---|---|---|---|
| Segment<br>OrientedSegment | <abbr title="opt&lt;var&lt;Point, Segment&gt;&gt;">oPS</abbr> | <abbr title="opt&lt;var&lt;Point, Segment&gt;&gt;">oPS</abbr> | <abbr title="opt&lt;var&lt;Point, Segment&gt;&gt;">oPS</abbr> | <abbr title="opt&lt;var&lt;Point, Segment&gt;&gt;">oPS</abbr> | <abbr title="opt&lt;var&lt;Point, Segment&gt;&gt;">oPS</abbr> | <abbr title="opt&lt;var&lt;Point, Segment&gt;&gt;">oPS</abbr> | <abbr title="vec&lt;var&lt;Point, Segment&gt;&gt;">vPS</abbr> | <abbr title="vec&lt;var&lt;Point, Segment&gt;&gt;">vPS</abbr> | <abbr title="opt&lt;var&lt;Point, Segment&gt;&gt;">oPS</abbr> | <abbr title="vec&lt;var&lt;Point, Segment&gt;&gt;">vPS</abbr> | <abbr title="No overload is available">-</abbr> |
| Line<br>OrientedLine | <abbr title="opt&lt;var&lt;Point, Segment&gt;&gt;">oPS</abbr> | <abbr title="opt&lt;var&lt;Point, Line&gt;&gt;">oPL</abbr> | <abbr title="opt&lt;var&lt;Point, Ray&gt;&gt;">oPR</abbr> | <abbr title="opt&lt;var&lt;Point, Line, Ray&gt;&gt;">oPLR</abbr> | <abbr title="opt&lt;var&lt;Point, Segment&gt;&gt;">oPS</abbr> | <abbr title="opt&lt;var&lt;Point, Segment&gt;&gt;">oPS</abbr> | <abbr title="vec&lt;var&lt;Point, Segment&gt;&gt;">vPS</abbr> | <abbr title="vec&lt;var&lt;Point, Segment&gt;&gt;">vPS</abbr> | <abbr title="opt&lt;var&lt;Point, Segment, Ray, Line&gt;&gt;">oPSRL</abbr> | <abbr title="vec&lt;var&lt;Point, Segment&gt;&gt;">vPS</abbr> | <abbr title="No overload is available">-</abbr> |
| Ray | <abbr title="opt&lt;var&lt;Point, Segment&gt;&gt;">oPS</abbr> | <abbr title="opt&lt;var&lt;Point, Ray&gt;&gt;">oPR</abbr> | <abbr title="opt&lt;var&lt;Point, Segment, Ray&gt;&gt;">oPSR</abbr> | <abbr title="opt&lt;var&lt;Point, Segment, Ray&gt;&gt;">oPSR</abbr> | <abbr title="opt&lt;var&lt;Point, Segment&gt;&gt;">oPS</abbr> | <abbr title="opt&lt;var&lt;Point, Segment&gt;&gt;">oPS</abbr> | <abbr title="vec&lt;var&lt;Point, Segment&gt;&gt;">vPS</abbr> | <abbr title="vec&lt;var&lt;Point, Segment&gt;&gt;">vPS</abbr> | <abbr title="opt&lt;var&lt;Point, Segment, Ray&gt;&gt;">oPSR</abbr> | <abbr title="vec&lt;var&lt;Point, Segment&gt;&gt;">vPS</abbr> | <abbr title="No overload is available">-</abbr> |
| Halfplane | <abbr title="opt&lt;var&lt;Point, Segment&gt;&gt;">oPS</abbr> | <abbr title="opt&lt;var&lt;Point, Line, Ray&gt;&gt;">oPLR</abbr> | <abbr title="opt&lt;var&lt;Point, Segment, Ray&gt;&gt;">oPSR</abbr> | <abbr title="HalfplaneIntersection">I</abbr> | <abbr title="opt&lt;var&lt;Point, Segment, Convex&gt;&gt;">oPSC</abbr> | <abbr title="opt&lt;var&lt;Point, Segment, Convex&gt;&gt;">oPSC</abbr> | <abbr title="vec&lt;var&lt;Point, Segment&gt;&gt;">vPS</abbr> | <abbr title="vec&lt;var&lt;Point, Segment, Polygon&gt;&gt;">vPSP</abbr> | <abbr title="HalfplaneIntersection">I</abbr> | <abbr title="vec&lt;var&lt;Point, Polyline, PolygonWithHoles&gt;&gt;">vPPLW</abbr> | <abbr title="vec&lt;var&lt;Point, Polyline, PolygonWithHoles&gt;&gt;">vPPLW</abbr> |
| Rectangle | <abbr title="opt&lt;var&lt;Point, Segment&gt;&gt;">oPS</abbr> | <abbr title="opt&lt;var&lt;Point, Segment&gt;&gt;">oPS</abbr> | <abbr title="opt&lt;var&lt;Point, Segment&gt;&gt;">oPS</abbr> | <abbr title="opt&lt;var&lt;Point, Segment, Convex&gt;&gt;">oPSC</abbr> | <abbr title="opt&lt;Rectangle&gt;">oRect</abbr> | <abbr title="opt&lt;var&lt;Point, Segment, Convex&gt;&gt;">oPSC</abbr> | <abbr title="vec&lt;var&lt;Point, Segment&gt;&gt;">vPS</abbr> | <abbr title="vec&lt;var&lt;Point, Polyline, Polygon&gt;&gt;">vPPLP</abbr> | <abbr title="HalfplaneIntersection">I</abbr> | <abbr title="vec&lt;var&lt;Point, Polyline, PolygonWithHoles&gt;&gt;">vPPLW</abbr> | <abbr title="vec&lt;var&lt;Point, Polyline, PolygonWithHoles&gt;&gt;">vPPLW</abbr> |
| Triangle<br>Convex | <abbr title="opt&lt;var&lt;Point, Segment&gt;&gt;">oPS</abbr> | <abbr title="opt&lt;var&lt;Point, Segment&gt;&gt;">oPS</abbr> | <abbr title="opt&lt;var&lt;Point, Segment&gt;&gt;">oPS</abbr> | <abbr title="opt&lt;var&lt;Point, Segment, Convex&gt;&gt;">oPSC</abbr> | <abbr title="opt&lt;var&lt;Point, Segment, Convex&gt;&gt;">oPSC</abbr> | <abbr title="opt&lt;var&lt;Point, Segment, Convex&gt;&gt;">oPSC</abbr> | <abbr title="vec&lt;var&lt;Point, Segment&gt;&gt;">vPS</abbr> | <abbr title="vec&lt;var&lt;Point, Polyline, Polygon&gt;&gt;">vPPLP</abbr> | <abbr title="HalfplaneIntersection">I</abbr> | <abbr title="vec&lt;var&lt;Point, Polyline, PolygonWithHoles&gt;&gt;">vPPLW</abbr> | <abbr title="vec&lt;var&lt;Point, Polyline, PolygonWithHoles&gt;&gt;">vPPLW</abbr> |
| MonotoneChain<br>Polyline | <abbr title="vec&lt;var&lt;Point, Segment&gt;&gt;">vPS</abbr> | <abbr title="vec&lt;var&lt;Point, Segment&gt;&gt;">vPS</abbr> | <abbr title="vec&lt;var&lt;Point, Segment&gt;&gt;">vPS</abbr> | <abbr title="vec&lt;var&lt;Point, Segment&gt;&gt;">vPS</abbr> | <abbr title="vec&lt;var&lt;Point, Segment&gt;&gt;">vPS</abbr> | <abbr title="vec&lt;var&lt;Point, Segment&gt;&gt;">vPS</abbr> | <abbr title="vec&lt;var&lt;Point, Segment&gt;&gt;">vPS</abbr> | <abbr title="vec&lt;var&lt;Point, Segment&gt;&gt;">vPS</abbr> | <abbr title="No overload is available">-</abbr> | <abbr title="vec&lt;var&lt;Point, Segment&gt;&gt;">vPS</abbr> | <abbr title="No overload is available">-</abbr> |
| Polygon | <abbr title="vec&lt;var&lt;Point, Segment&gt;&gt;">vPS</abbr> | <abbr title="vec&lt;var&lt;Point, Segment&gt;&gt;">vPS</abbr> | <abbr title="vec&lt;var&lt;Point, Segment&gt;&gt;">vPS</abbr> | <abbr title="vec&lt;var&lt;Point, Segment, Polygon&gt;&gt;">vPSP</abbr> | <abbr title="vec&lt;var&lt;Point, Polyline, Polygon&gt;&gt;">vPPLP</abbr> | <abbr title="vec&lt;var&lt;Point, Polyline, Polygon&gt;&gt;">vPPLP</abbr> | <abbr title="vec&lt;var&lt;Point, Segment&gt;&gt;">vPS</abbr> | <abbr title="vec&lt;var&lt;Point, Polyline, Polygon&gt;&gt;">vPPLP</abbr> | <abbr title="vec&lt;var&lt;Point, Polyline, Polygon&gt;&gt;">vPPLP</abbr> | <abbr title="vec&lt;var&lt;Point, Polyline, PolygonWithHoles&gt;&gt;">vPPLW</abbr> | <abbr title="vec&lt;var&lt;Point, Polyline, PolygonWithHoles&gt;&gt;">vPPLW</abbr> |
| HalfplaneIntersec. | <abbr title="opt&lt;var&lt;Point, Segment&gt;&gt;">oPS</abbr> | <abbr title="opt&lt;var&lt;Point, Segment, Ray, Line&gt;&gt;">oPSRL</abbr> | <abbr title="opt&lt;var&lt;Point, Segment, Ray&gt;&gt;">oPSR</abbr> | <abbr title="HalfplaneIntersection">I</abbr> | <abbr title="HalfplaneIntersection">I</abbr> | <abbr title="HalfplaneIntersection">I</abbr> | <abbr title="No overload is available">-</abbr> | <abbr title="vec&lt;var&lt;Point, Polyline, Polygon&gt;&gt;">vPPLP</abbr> | <abbr title="HalfplaneIntersection">I</abbr> | <abbr title="vec&lt;var&lt;Point, Polyline, PolygonWithHoles&gt;&gt;">vPPLW</abbr> | <abbr title="vec&lt;var&lt;Point, Polyline, PolygonWithHoles&gt;&gt;">vPPLW</abbr> |
| PolygonWithHoles | <abbr title="vec&lt;var&lt;Point, Segment&gt;&gt;">vPS</abbr> | <abbr title="vec&lt;var&lt;Point, Segment&gt;&gt;">vPS</abbr> | <abbr title="vec&lt;var&lt;Point, Segment&gt;&gt;">vPS</abbr> | <abbr title="vec&lt;var&lt;Point, Polyline, PolygonWithHoles&gt;&gt;">vPPLW</abbr> | <abbr title="vec&lt;var&lt;Point, Polyline, PolygonWithHoles&gt;&gt;">vPPLW</abbr> | <abbr title="vec&lt;var&lt;Point, Polyline, PolygonWithHoles&gt;&gt;">vPPLW</abbr> | <abbr title="vec&lt;var&lt;Point, Segment&gt;&gt;">vPS</abbr> | <abbr title="vec&lt;var&lt;Point, Polyline, PolygonWithHoles&gt;&gt;">vPPLW</abbr> | <abbr title="vec&lt;var&lt;Point, Polyline, PolygonWithHoles&gt;&gt;">vPPLW</abbr> | <abbr title="vec&lt;var&lt;Point, Polyline, PolygonWithHoles&gt;&gt;">vPPLW</abbr> | <abbr title="vec&lt;var&lt;Point, Polyline, PolygonWithHoles&gt;&gt;">vPPLW</abbr> |
| PolygonSet | <abbr title="No overload is available">-</abbr> | <abbr title="No overload is available">-</abbr> | <abbr title="No overload is available">-</abbr> | <abbr title="vec&lt;var&lt;Point, Polyline, PolygonWithHoles&gt;&gt;">vPPLW</abbr> | <abbr title="vec&lt;var&lt;Point, Polyline, PolygonWithHoles&gt;&gt;">vPPLW</abbr> | <abbr title="vec&lt;var&lt;Point, Polyline, PolygonWithHoles&gt;&gt;">vPPLW</abbr> | <abbr title="No overload is available">-</abbr> | <abbr title="vec&lt;var&lt;Point, Polyline, PolygonWithHoles&gt;&gt;">vPPLW</abbr> | <abbr title="vec&lt;var&lt;Point, Polyline, PolygonWithHoles&gt;&gt;">vPPLW</abbr> | <abbr title="vec&lt;var&lt;Point, Polyline, PolygonWithHoles&gt;&gt;">vPPLW</abbr> | <abbr title="vec&lt;var&lt;Point, Polyline, PolygonWithHoles&gt;&gt;">vPPLW</abbr> |



### Boolean Operations

The `intersection` method above may produce pieces of different dimensions. The following methods operate on two-dimensional polygonal regions and always return a [`PolygonSet`](shapes.md#polygon-set). They regularize their results by retaining the appropriate two-dimensional cells and taking their closure, thereby removing lower-dimensional pieces:

| call | result |
|---|---|
| `a.regularizedIntersection(b)` | $\mathrm{closure}(A^\circ \cap B^\circ)$, the area both cover |
| `a.regularizedUnion(b)` | $\mathrm{closure}(A^\circ \cup B^\circ)$, the area either covers |
| `a.difference(b)` | $\mathrm{closure}(A^\circ \setminus B)$, the area of `a` that `b` does not cover |
| `a.symmetricDifference(b)` | $\mathrm{closure}((A^\circ \setminus B) \cup (B^\circ \setminus A))$, the area exactly one covers |

```c++
pgl::Polygon<> square({0,0, 10,0, 10,10, 0,10});
pgl::EPolygonSet holed  = square.difference(pgl::Rectangle(3,3,7,7));
pgl::EPolygonSet again  = holed.difference(pgl::Rectangle(0,0,2,2));
pgl::EPolygonSet merged = again.regularizedUnion(holed);
```

To unite a range of regions in one arrangement, use `regularizedUnionOf` and
specify the result point type. The range may hold any one of the six bounded
region types:

```c++
std::vector<pgl::Polygon<>> polygons = /* ... */;
auto merged = pgl::regularizedUnionOf<pgl::EPoint>(polygons);

std::vector<pgl::Triangle<>> triangles = /* ... */;
auto covered = pgl::regularizedUnionOf<pgl::EPoint>(triangles);
```

The six bounded region types are `Rectangle`, `Triangle`, `Convex`, `Polygon`, `PolygonWithHoles`, and `PolygonSet`. `regularizedUnion` and `symmetricDifference` are defined for every pair among them. `difference` requires one of those six as its receiver and accepts any of the six, a `Halfplane`, or a `HalfplaneIntersection` as its argument. `regularizedIntersection` is available when a `PolygonWithHoles` or `PolygonSet` participates; the other operand may be any of the six bounded region types, a `Halfplane`, or a `HalfplaneIntersection`. These last two operations can involve an unbounded operand because both $A \setminus B$ and $A \cap B$ are bounded when $A$ is bounded; for the nonsymmetric difference, the unbounded operand must be the argument.

Every pair outside those grids throws `std::logic_error`, and the one gap worth knowing is that the four operations are **not** interchangeable: `regularizedIntersection` is the only one undefined for a pair drawn just from `Rectangle`, `Triangle`, `Convex` and `Polygon`, so `rectangle.regularizedIntersection(triangle)` throws where `regularizedUnion`, `difference` and `symmetricDifference` all answer. Calling [`asPolygonWithHoles`](shapes.md#polygon-with-holes) on either operand first reaches it:

```c++
pgl::Rectangle<> rect(0,0, 4,4);
pgl::Triangle<> tri(0,0, 6,0, 0,6);
auto area = rect.asPolygonWithHoles().regularizedIntersection(tri);  // rect.regularizedIntersection(tri) throws
```

For the literal point set, including its lower-dimensional pieces, `intersection` is defined for that pair as it stands.

### Minkowski Sum

The Minkowski sum of two shapes is the set of all sums of a point of the first and a point of the second, $A \oplus B = \\{a + b : a \in A, b \in B\\}$. It is written `a.minkowskiSum(b)`, or `a + b` when there is no template parameter (because the construction is exact for integers). Adding a `Point` is a translation.

The Minkowski sum of two bounded, polygonal convex shapes is convex. Every vertex of the result is a sum of two input vertices, so the construction is exact for integers.

<table>
  <tr>
    <td valign="top" width="75%">

```c++
pgl::Segment s = {0,0,2,0}, t = {0,0,0,3};
pgl::Convex box = s + t;
// box = Convex[(0,0),(2,0),(2,3),(0,3)]
pgl::Triangle tri = {0,0,3,0,0,3};
pgl::Convex convex = tri + box;
```

  </td>
    <td valign="top" width="25%">
      <img src="figures/minkowski_convex.svg" alt="A rectangle and triangle with their convex Minkowski sum" width="100%"/>
    </td>
  </tr>
</table>

The shape returned by the Minkowski sum depends on the two operands, and is summarized below.

| Code | Return type |
|---|---|
| Cvx, Rect, Poly, PWH, PSet | Convex, Rectangle, Polygon, PolygonWithHoles, PolygonSet |
| H, I | Halfplane, HalfplaneIntersection |
| K* | Rectangle for Rectangle + Rectangle; otherwise Convex |
| — | No overload is available |

| operands| Seg.<br>O.Seg. | Line<br>O.Line<br>Ray | Halfp. | Rect.<br>Tri.<br>Conv. | M.Chain | P.line | Poly.<br>PWH. | HpI. | P.Set |
|---|---|---|---|---|---|---|---|---|---|
| Segment<br>OrientedSegment | <abbr title="Convex">Cvx</abbr> | <abbr title="HalfplaneIntersection">I</abbr> | <abbr title="Halfplane">H</abbr> | <abbr title="Convex">Cvx</abbr> | <abbr title="PolygonSet">PSet</abbr> | <abbr title="PolygonSet">PSet</abbr> | <abbr title="PolygonWithHoles">PWH</abbr> | <abbr title="HalfplaneIntersection">I</abbr> | <abbr title="PolygonSet">PSet</abbr> |
| Line<br>OrientedLine<br>Ray | <abbr title="HalfplaneIntersection">I</abbr> | <abbr title="HalfplaneIntersection">I</abbr> | <abbr title="HalfplaneIntersection">I</abbr> | <abbr title="HalfplaneIntersection">I</abbr> | <abbr title="No overload is available">-</abbr> | <abbr title="No overload is available">-</abbr> | <abbr title="No overload is available">-</abbr> | <abbr title="HalfplaneIntersection">I</abbr> | <abbr title="No overload is available">-</abbr> |
| Halfplane | <abbr title="Halfplane">H</abbr> | <abbr title="HalfplaneIntersection">I</abbr> | <abbr title="HalfplaneIntersection">I</abbr> | <abbr title="Halfplane">H</abbr> | <abbr title="Halfplane">H</abbr> | <abbr title="Halfplane">H</abbr> | <abbr title="Halfplane">H</abbr> | <abbr title="HalfplaneIntersection">I</abbr> | <abbr title="Halfplane">H</abbr> |
| Rectangle<br>Triangle<br>Convex | <abbr title="Convex">Cvx</abbr> | <abbr title="HalfplaneIntersection">I</abbr> | <abbr title="Halfplane">H</abbr> | <abbr title="Rectangle for Rectangle + Rectangle; otherwise Convex">K*</abbr> | <abbr title="Polygon">Poly</abbr> | <abbr title="PolygonWithHoles">PWH</abbr> | <abbr title="PolygonWithHoles">PWH</abbr> | <abbr title="HalfplaneIntersection">I</abbr> | <abbr title="PolygonSet">PSet</abbr> |
| MonotoneChain | <abbr title="PolygonSet">PSet</abbr> | <abbr title="No overload is available">-</abbr> | <abbr title="Halfplane">H</abbr> | <abbr title="Polygon">Poly</abbr> | <abbr title="PolygonSet">PSet</abbr> | <abbr title="PolygonSet">PSet</abbr> | <abbr title="PolygonWithHoles">PWH</abbr> | <abbr title="No overload is available">-</abbr> | <abbr title="PolygonSet">PSet</abbr> |
| Polyline | <abbr title="PolygonSet">PSet</abbr> | <abbr title="No overload is available">-</abbr> | <abbr title="Halfplane">H</abbr> | <abbr title="PolygonWithHoles">PWH</abbr> | <abbr title="PolygonSet">PSet</abbr> | <abbr title="PolygonSet">PSet</abbr> | <abbr title="PolygonWithHoles">PWH</abbr> | <abbr title="No overload is available">-</abbr> | <abbr title="PolygonSet">PSet</abbr> |
| Polygon<br>PolygonWithHoles | <abbr title="PolygonWithHoles">PWH</abbr> | <abbr title="No overload is available">-</abbr> | <abbr title="Halfplane">H</abbr> | <abbr title="PolygonWithHoles">PWH</abbr> | <abbr title="PolygonWithHoles">PWH</abbr> | <abbr title="PolygonWithHoles">PWH</abbr> | <abbr title="PolygonWithHoles">PWH</abbr> | <abbr title="No overload is available">-</abbr> | <abbr title="PolygonSet">PSet</abbr> |
| HalfplaneIntersection | <abbr title="HalfplaneIntersection">I</abbr> | <abbr title="HalfplaneIntersection">I</abbr> | <abbr title="HalfplaneIntersection">I</abbr> | <abbr title="HalfplaneIntersection">I</abbr> | <abbr title="No overload is available">-</abbr> | <abbr title="No overload is available">-</abbr> | <abbr title="No overload is available">-</abbr> | <abbr title="HalfplaneIntersection">I</abbr> | <abbr title="No overload is available">-</abbr> |
| PolygonSet | <abbr title="PolygonSet">PSet</abbr> | <abbr title="No overload is available">-</abbr> | <abbr title="Halfplane">H</abbr> | <abbr title="PolygonSet">PSet</abbr> | <abbr title="PolygonSet">PSet</abbr> | <abbr title="PolygonSet">PSet</abbr> | <abbr title="PolygonSet">PSet</abbr> | <abbr title="No overload is available">-</abbr> | <abbr title="PolygonSet">PSet</abbr> |


### Minkowski Erosion

The Minkowski erosion of a shape by another is the set of translations of the second that keep it inside the first, $A \ominus B = \{x : x \oplus B \subseteq A\} = \bigcap_{b \in B} (A - b)$. It is written `a.minkowskiErosion(b)` and is defined for exactly the pairs [`minkowskiSum`](#minkowski-sum) is defined for. Unlike the sum it is **not commutative**: `a.minkowskiErosion(b)` and `b.minkowskiErosion(a)` are different questions.

A **convex receiver** is an intersection of half-planes, and eroding it moves each of them in by the operand's support point in that direction — one clamp per constraint, in time linear in the two sizes. The half-planes stay on the operands' lattice, but their crossings need not, so the result is a [`HalfplaneIntersection`](shapes.md#halfplane-intersection), which represents a two-dimensional region, a segment, a point, the empty set and the whole plane alike. Ask it for `asConvex<ResultNumber>()` to get the vertices.

```c++
pgl::Triangle<> tri(0,0, 2,0, 0,3);
auto shrunk = tri.minkowskiErosion(pgl::Segment(0,0, 0,1));
shrunk.asConvex<pgl::ERational>();                    // Convex[(0,0),(4/3,0),(0,2)]
```

Because only the operand's *support function* is read, and a support function sees no further than the convex hull, a convex erodes by a non-convex operand at no extra cost and with the same answer its hull gives. That is why a convex shape keeps the pairs whose sum it forwards to a `Polygon` or a region.

```c++
pgl::Rectangle<> box(0,0, 10,10);
pgl::Polygon<> ell({{0,0},{4,0},{4,1},{1,1},{1,4},{0,4}});
box.minkowskiErosion(ell) == box.minkowskiErosion(ell.convexHull());   // true
```

Two pairs have a tighter answer. Two rectangles erode to a `Rectangle` — the minima and the maxima subtract — and a `Halfplane` eroded by anything bounded is that same half-plane moved in.

```c++
pgl::Rectangle(0,0, 10,10).minkowskiErosion(pgl::Rectangle(0,0, 3,2));  // [(0,0),(7,8)]
pgl::Halfplane up = {0,0, 1,0};                       // y >= 0
up.minkowskiErosion(pgl::Rectangle(2,3, 5,7));        // y >= -3
```

An **unbounded operand** fits inside no bounded receiver, and the clamp says so: the support in a direction the operand recedes through is infinite, so that constraint admits nothing and the erosion is empty. An unbounded *receiver* erodes like any other: a line survives only an operand parallel to it, and a half-plane survives a line, a ray or a half-plane parallel to its own boundary.

A **non-convex receiver** — a `Polygon`, a `PolygonWithHoles`, a `PolygonSet`, a `Polyline` or a `MonotoneChain` — returns a [`PolygonSet`](shapes.md#polygon-set), because an erosion disconnects what it shrinks: a dumbbell eroded by anything taller than its handle is two regions, for operands that are in no way degenerate. This is where the sum's single-region guarantee has no counterpart. The result is *regularized*, `closure((A ⊖ B)°)`, as the [boolean operations](#boolean-operations) are, so material an erosion thins to a curve — a corridor exactly as wide as its operand — is dropped rather than represented, and a receiver with no area erodes to the empty set. As everywhere else, the coordinate type is the caller's, defaulting to `division_result_t`.

```c++
pgl::Polygon<> u({{0,0},{6,0},{6,6},{4,6},{4,2},{2,2},{2,6},{0,6}});   // a U
auto eroded = u.minkowskiErosion(pgl::Rectangle(0,0, 1,1));            // a PolygonSet
eroded.component(0).outer();     // [(0,0),(5,0),(5,5),(4,5),(4,1),(1,1),(1,5),(0,5)]
u.minkowskiErosion<int>(pgl::Rectangle(0,0, 1,1));                     // integer coordinates
```

Two `Disk`s erode to a `Disk` — the centers subtract and so do the radii — reported as a `std::optional` that is empty when the operand is the wider disk, since a disk has no empty state of its own. A `Halfplane` eroded by a `Disk` slides in by the radius along its own normal, where the sum slides it out. Both take a square root unless the disks were built from a center and a radius, so `ResultNumber` defaults to `double` as it does for the disk sum.

```c++
pgl::Disk a(pgl::Point(0,0), 5), b(pgl::Point(4,1), 2);
a.minkowskiErosion(b);                 // center (-4,-1), radius 3
b.minkowskiErosion(a);                 // std::nullopt
```


### Other Methods for Shapes

Methods that construct coordinates or return numeric measurements use one of three result-number defaults:

- `ResultNumber = NumberType` when the operation needs no division, such as rectangle area or Point–Point squared, L1, and LInf distances;
- `ResultNumber = division_result_t<NumberType>` when the operation may divide. Integral and `BigInt` receivers widen to `ERational`, while floating-point and already-rational receivers retain their coordinate type; and
- `ResultNumber = double` when a supported result may be irrational, such as a disk radius or a distance involving a disk.

The policy is receiver-only: mixed-coordinate calls use the receiver's default. An explicit result template argument overrides it. Explicit integral results can truncate an operation that divides. Disk distance operations fall back to `double` for non-floating requests, while other disk operations may reject an exact type when they require a square root.

- `rotated90(int k = 1)`: Returns the shape rotated by `90k` degrees around the origin.

- `rotate90(int k = 1)`: Rotates the shape by `90k` degrees around the origin.

- `scaledUpX(Number)`: Returns the shape with the x-coordinate multiplied by a number.

- `scaleUpX(Number)`: Multiplies the x-coordinate by a number.

- `scaledUpY(Number)`: Returns the shape with the y-coordinate multiplied by a number.

- `scaleUpY(Number)`: Multiplies the y-coordinate by a number.

- `scaledDownX(Number)`: Returns the shape with the x-coordinate divided by a number.

- `scaleDownX(Number)`: Divides the x-coordinate by a number.

- `scaledDownY(Number)`: Returns the shape with the y-coordinate divided by a number.

- `scaleDownY(Number)`: Divides the y-coordinate by a number.

- `squaredDistance<ResultNumber>(Shape)`: Returns the squared distance using the result policy selected by `ResultNumber`. Point–Point and the axis-aligned rectangle cases default to `NumberType`; pairs that may project onto an edge default to `division_result_t<NumberType>`; pairs involving `Disk`, and calls through a runtime `Shape`, default to `double`. An explicitly integral result truncates any projection division, while a disk pair falls back to `double` for a non-floating request.

- `squaredHausdorffDistance<ResultNumber>(Shape)`: Returns the squared Hausdorff distance. Pair-specific defaults are native when the extrema only reuse stored vertices and `division_result_t<NumberType>` when an edge projection may be needed; runtime `Shape` uses the latter. Defined for every pair among `Point`, `Segment`, `OrientedSegment`, `Rectangle`, `Triangle`, and `Convex` — all bounded, convex shapes, so the directed distance in either direction is always attained at a vertex. Not defined for `Line`, `OrientedLine`, `Ray`, `Halfplane`, or `HalfplaneIntersection` (unbounded, or possibly unbounded, so the Hausdorff distance to or from them is generally infinite), nor yet for `Disk`, `MonotoneChain`, or `Polygon`.

- `distanceL1<ResultNumber>(Shape)` / `distanceLInf<ResultNumber>(Shape)`: Return the Manhattan (L1) or Chebyshev (LInf) distance to the given shape. Point–Point and the axis-aligned rectangle cases default to `NumberType`; pairs that may project onto an edge default to `division_result_t<NumberType>`. Point–Point is templated too, so mixed-coordinate callers can explicitly choose the arithmetic type. Defined for every pair among `Point`, `Segment`, `OrientedSegment`, `Line`, `OrientedLine`, `Ray`, `Halfplane`, `Rectangle`, `Triangle`, `Convex`, `MonotoneChain`, `Polygon`, and `HalfplaneIntersection`, plus Disk–Point. Disk distances default to `double` because the implementation uses a numeric search; an explicitly requested floating-point type is preserved. The remaining `Disk` pairs (`Disk` against any shape other than `Point`, and Disk–Disk) are not yet implemented.

- `hausdorffDistanceL1(Shape)` / `hausdorffDistanceLInf(Shape)`: Return the L1 or LInf Hausdorff distance, with the same `ResultNumber` convention as `distanceL1` / `distanceLInf`. Defined for the same pairs as `squaredHausdorffDistance`: `Point`, `Segment`, `OrientedSegment`, `Rectangle`, `Triangle`, and `Convex`.

- `bbox()`: Returns an axis-aligned bounding box in the stored point type that contains the bounded shape. It is tight for the polygonal shapes; a disk built from three boundary points may return a larger exact-coordinate box. Runtime `Shape` dispatches to this method and throws `std::logic_error` for an unbounded alternative, `EmptyShape`, or an empty `HalfplaneIntersection`. A bounded, nonempty `HalfplaneIntersection` instead exposes `bbox<ResultNumber = division_result_t<NumberType>>()` because its implicit vertices may be fractional.

- `fbox<T>()`: Returns a floating-point bounding box with coordinates of type `T`. For shapes whose box comes from stored exact coordinates, conversions are rounded outward when the coordinate type supplies directed bounds. A disk's box is computed from its floating-point center and radius and is tight only up to floating-point rounding.

- `convexHull()`{Polygon}: Returns the smallest convex polygon that contains the shape.

- `area<ResultNumber>()`: Returns the area. Rectangle and zero- or one-dimensional shapes default to `NumberType`; polygonal shapes and `HalfplaneIntersection` default to `division_result_t<NumberType>` because they may divide by two or construct fractional vertices; `Disk` defaults to `double` because its area contains π.

- `twiceArea()`: Returns two times the area in native arithmetic for stored shapes. `HalfplaneIntersection` uses `twiceArea<ResultNumber = division_result_t<NumberType>>()`, because even its implicit vertices may require division.

- `diameter()`: Returns a segment that defines the diameter in native coordinates. `Disk` instead exposes `diameter<ResultNumber = division_result_t<NumberType>>()`, since finding the center of a three-point disk may divide.

- `pointInside<ResultNumber>()`: Returns a point in the relative interior of the shape. Forms that divide by a power of two default to `division_result_t<NumberType>`; forms that simply select a stored point default to `NumberType`.

- `pointInsideInteriorContainedIn(other)`: Returns true if the `pointInside()` witness from this shape's relative interior lies in the strict interior of `other`. It scales both shapes when necessary to keep the witness exact instead of letting integer truncation round it onto the boundary.

- `verticesContain(p)`: Returns true if there is an index `i` such that `s[i] == p` for the shape `s`. Two shapes, such as lines, may be equal according to `==` but behave differently for `verticesContain` when they are defined by different points.

- `convexPartition()` (`Polygon` and `PolygonWithHoles`): Returns the shape cut into `Convex` pieces with pairwise disjoint interiors whose union is the shape, using at most four times the fewest possible pieces. It is shorthand for `triangulation().convexPartition()`; see [Triangulation](data_structures.md#triangulation). A convex shape comes back as a single piece. On a region, the pieces cover only what has area, so the holes are where there is no piece, and a slit—having no area—appears in none of them.

- `convexCovering()` (`Polygon` and `PolygonWithHoles`): Returns an irredundant covering by `Convex` pieces. The pieces may overlap, and the covering is not necessarily minimum. For a `Polygon`, the constrained Delaunay triangles form a full-visibility subgraph using the paper's dual-graph BFS, a DSATUR vertex clique cover groups them, and every clique becomes one convex hull. For `PolygonWithHoles`, the method remains shorthand for `triangulation().convexCovering()` because clique hulls can surround holes and require an additional splitting step. On a region, the covering leaves holes and slits uncovered.


## Iterating

Several methods iterate through vertices, edges, or oriented edges. An [`std::array`](https://en.cppreference.com/w/cpp/container/array.html) is used for shapes of constant size, and an [`std::vector`](https://en.cppreference.com/w/cpp/container/vector.html) is used otherwise.

- `vertices()`: Returns an `std::array` or an `std::vector` of the stored `Point` type for ordinary shapes. `HalfplaneIntersection` exposes `vertices<ResultNumber>()` because its vertices are derived from line intersections.

- `edges()`: Returns an `std::array` or an `std::vector` of `Segment` objects representing the edges.

- `orientedEdges()`: Returns an `std::array` or an `std::vector` of `OrientedSegment` objects. Boundary shapes return them in counterclockwise order. This method is not defined for `Disk`.

- `begin()`, `end()`, `edgesBegin()`, `edgesEnd()`, `orientedEdgesBegin()`, and `orientedEdgesEnd()`: Provide iterator access corresponding to `vertices()`, `edges()`, and `orientedEdges()` above, with `O(1)` work per element visited.

### Indexed access

Most concrete shapes expose indexed access over their defining points. `Point` instead indexes its two coordinates, and `HalfplaneIntersection` indexes its stored halfplanes. `PolygonWithHoles` and `PolygonSet` deliberately have no single indexed sequence.

- `size()`: Returns the number of indexable elements.

- `s[i]`: Returns the `i`-th element.

- `s.get(i)`: Same as `s[i]`, but `i` is taken modulo `s.size()`, so negative values wrap from the end.

- `s.index(p)`: Returns the smallest index `i` such that `s[i] == p`, or -1 if no such index exists.

```c++
pgl::Convex c({{0,0},{4,0},{4,3},{0,3}});
c[2];           // (4,3)
c.get(-1);      // (0,3) same as c[3]
c.get(5);       // (4,0) same as c[1]
c.index({4,3}); // 2 since c[2] == {4,3}
```


The runtime `Shape` wrapper exposes `size()`, `operator[]`, `get()`, and `index()` by dispatching to the wrapped alternative. `size()` throws `std::logic_error` for `PolygonWithHoles` and `PolygonSet`. Point-valued access through `operator[]`, `get()`, or `index(Point)` also throws for `Point`, whose elements are coordinates; for `HalfplaneIntersection`, whose elements are halfplanes; and for `PolygonWithHoles` and `PolygonSet`, which have no single indexable sequence. The `Point` alternative instead supports `index(NumberType)`.
