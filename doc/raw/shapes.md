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


## Shapes

The following shapes are supported by Pangolin:

##### 0-dimensional shapes:
- [`Point`](#point): A point in the plane.

##### 1-dimensional shapes:
- [`Segment`](#segment): Unoriented straight line segment.
- [`OrientedSegment`](#oriented-segment): Oriented straight line segment.
- [`Line`](#line) Infinite straight line.
- [`OrientedLine`](#oriented-line) Infinite oriented straight line.
- [`Ray`](#ray) Half-line.
- [`MonotoneChain`](#monotone-chain) Weakly x-monotone polyline.
- [`Polyline`](#polyline) Polygonal chain, possibly self-intersecting.

##### 2-dimensional shapes:
- [`Halfplane`](#half-plane) A straight line and all points on one side of it.
- [`Triangle`](#triangle) Unoriented triangle.
- [`Rectangle`](#rectangle) Axis-aligned rectangle.
- [`Disk`](#disk) A circle with its interior.
- [`Convex`](#convex) Convex polygon.
- [`Polygon`](#polygon) Simple polygon.
- [`PolygonWithHoles`](#polygon-with-holes) Simple polygon minus a set of disjoint polygonal holes.
- [`PolygonSet`](#polygon-set) A set of regions with pairwise disjoint interiors.
- [`HalfplaneIntersection`](#halfplane-intersection) Intersection of half-planes; convex but possibly unbounded or empty.

All shapes are template classes with a parameter that is a `Point` type, with `pgl::Point<int>` as default:

```C++
pgl::Segment si = {1,2,3,4}; // Same as pgl::Segment<pgl::Point<int>>
pgl::Segment<pgl::Point<double>> sd = {1.0,1.5,2.0,2.5};
```

It is often convenient to define the types you use more often:

```
using Point = pgl::Point<double>;
using Segment = pgl::Segment<Point>;
using Triangle = pgl::Triangle<Point>;
```

There are many [predicates](shape_methods.md#predicates) and [other methods](shape_methods.md) supported by all shapes, such as `intersects`, `contains`, `squaredDistance`, `distanceL1`, translation, and scaling.

A method that constructs coordinates or measures takes the result type as its first template argument. It is named `ResultNumber` when the answer stays exact — integral coordinates then default to `ERational` — and `ApproximateNumber` when it does not, the default there being `double`. Both are spelled out below; the [defaults](shape_methods.md#other-methods-for-shapes) follow the receiver's coordinate type. Each entry states the contract and the complexity; the Doxygen link on the name gives the rest.

All shapes contain their boundaries (that is, they are closed in the topological sense). The boundary of a shape is the *manifold boundary*, that is:

- A point has no boundary.
- The boundary of a 1-dimensional shape is the set of (at most two) extreme points of the curve. The boundary of a segment are its two vertices. The boundary of a ray is its one vertex. A line has no boundary.
- The boundary of a 2-dimensional shape is defined in the usual way. The boundary of a triangle is its perimeter, the boundary of a halfplane is the line that defines it.

### Degeneracies

Shapes may be degenerate, for example when some of their defining points are equal. Degenerate shapes may safely be constructed, and are often constructed by the default constructor that sets all points to the origin. There are different types of degenerate shapes.

One type consists of shapes that are well defined, for example, a triangle with three collinear vertices represents a segment and a disk of radius 0 represents a point. These degeneracies are supported with the expected behavior or the limit case whenever possible and unexpensive. In particular, degenerate shapes that represent points and segments are supported by all predicates. On operations that return a shape, the return shape is one that represents the result with non-degenerate inputs, and is undefined behavior for degenerate cases.

Other degenerate shapes have no meaningful behavior and are called **undefined**. For example, a line defined by two equal points has no reasonable interpretation, and a disk defined by 3 different collinear points could represent two different halfplanes as limit cases. The `isUndefined` and `isDegenerate` methods distinguish between these two types. If `isUndefined` returns true, then every geometric operation is undefined behavior (any value may be returned, but no segmentation fault or infinite loop).

A third state is the **empty set**: `Rectangle`, `Convex`, `Polygon`, `PolygonWithHoles`, `PolygonSet`, and `HalfplaneIntersection` can each cover no point at all, which `empty` reports. An empty shape is well defined, not undefined: it behaves exactly as `EmptyShape` in every predicate, so it is contained in every shape (`contains`, `boundaryContains`, and `interiorContains` all accept it), it meets none (`intersects`, `interiorsIntersect`, and `crosses` are all `false` for it), it `separates` only a `PolygonSet` already in pieces, and it contains nothing but itself. It has no vertices, so `size()` is `0` and iteration yields nothing; it has zero area, so `isDegenerate` is `true`. The empty state is what lets these shapes answer "no points" without a `std::optional` wrapper.

A shape satisfying `isPoint` covers exactly a point and `getIfPoint()` returns the point. Similarly a shape satisfying `isSegment` covers exactly the point set of a segment that is obtained with `getIfSegment()`. Degenerate shapes that dropped below their natural dimension are **entirely boundary with empty interior**. So `boundaryContains` on a collapsed shape coincides with `contains`, while `interiorContains` and `interiorsIntersect` are always `false`. The polymorphic `Shape` answers these as the shape it holds.

### Polymorphism with `Shape`

Shapes are grouped into a polymorphic class `Shape` that uses `std::variant` for polymorphism, so a shape whose kind is only known at run time is still a value with the whole common interface. It has a page of its own: [polymorphism](polymorphism.md).

```C++
pgl::Shape p = pgl::Point(3,7);
pgl::Shape s = pgl::Segment(1,4,2,9);
pgl::Shape r = pgl::Rectangle(1,4,2,9);
if (r.contains(p))
    std::cout << r << " contains " << p << std::endl;
if (r.intersects(s))
    std::cout << r << " intersects " << s << std::endl;
```


### Point

The `Point` class template defines a point with x and y coordinates. A point may optionally have a [label](types.md#labels). A point has no boundary and has the point itself as the interior.

```C++
pgl::Point p = {7,9};
pgl::Point<double> q = {3.5,2.25};
pgl::Point<int,std::string> c = {3,5,"center"};
```

You can read and change the coordinates of a point `p` as `p[0]` and `p[1]` or `p.x()` and `p.y()`. You can also iterate through the coordinates.

```C++
pgl::Point p;
p.x() = 7;
p[1] = 9;
for(int coord : p) std::cout << coord << ' ';
std::cout << p << std::endl;
// Output: 7 9 (7,9)
```

A point has methods:
- `p.swapped()`: Returns the point with x and y coordinates swapped.
- `p.dual<ResultNumber>()`: Returns the dual line $y = ax - b$ for a point $(a,b)$.
- `p.polar<ResultNumber>()`: Returns the polar line $ax + by = 1$ for a point $(a,b)$. Undefined for the origin.

- Other methods:

### Segment

The `Segment` class template defines an unoriented straight line segment. The segment always stores the endpoints in increasing order.

```C++
pgl::Segment s(1,2,3,4), t(3,4,1,2);
if (s == t)
    std::cout << s << " == " << t << std::endl;
// Output: (1,2)--(3,4) == (1,2)--(3,4)
```

You can read the two endpoints of a segment `s` as `s[0]` and `s[1]`. You cannot directly change the endpoints. You can also iterate through the endpoints.

```C++
pgl::Segment s(3,4,1,2);
for(size_t i : {0,1}) std::cout << s[i] << ' ';
for(pgl::Point p : s) std::cout << p << ' ';
// Output: (1,2) (3,4) (1,2) (3,4)
```

The interior of a segment is all the segment except the two endpoints.
```C++
pgl::Segment s(1,0,5,0), t(2,0,2,3);
if (s.intersects(t)) std::cout << "Intersect!";
if (!s.interiorsIntersect(t)) std::cout << " Interiors do not intersect!\n";
// Output: Intersect! Interiors do not intersect!
```

A segment `s` has methods such as:

- `s.midpoint<ResultNumber>()`: Returns the midpoint. An explicitly integral result type truncates odd coordinates.
- `s.latticePoints<ResultNumber>()`: Returns the [integer points](shape_methods.md#other-methods-for-shapes) on `s` in increasing order, an endpoint among them exactly when its own coordinates are whole.
- `s.length<ApproximateNumber>()`: Returns `s[0].distance(s[1])`.
- `s.squaredLength()`: Returns `s[0].squaredDistance(s[1])`.
- `s.isDegenerate()`: Returns `s.length() == 0`.
- `s.isPoint()` / `s.getIfPoint()`: Whether the segment collapses to a single point (all defining points equal), and that point as a `std::optional<PointType>`.
- `s.isUndefined()`: Always `false`: a degenerate segment is always a point.
- `s.isVertical()`: Returns `s[0].x() == s[1].x()`.
- `s.isHorizontal()`: Returns `s[0].y() == s[1].y()`.
- `s.containsEndpoint(p)`: Returns `s[0] == p || s[1] == p`
- `s.collinear(t)`: Returns whether `s` and `t` are on the same line, where `t` may be a point or another segment.
- `s.slope<ResultNumber>()`: Returns `(s[1].y()-s[0].y()) / (s[1].x()-s[0].x())`.
- `s.parallel(t)`: Returns whether `s` and `t` have the same slope, but without using division. Here, `t` may be a segment, oriented segment, line, ray, or oriented line.
- `s.yAtX<ResultNumber>(x)`: Returns an `std::optional` with the value of the segment y coordinate at the given coordinate `x`.
- `s.xAtY<ResultNumber>(y)`: Returns an `std::optional` with the value of the segment x coordinate at the given coordinate `y`.

It knows how to convert itself with an explicit cast to:
- `(pgl::Line) s` or `s.asLine()`: Returns the line that contains `s`.

- Other methods:


### Oriented Segment

The `OrientedSegment` class template defines an oriented straight line segment. The user chooses the order of the two endpoints, which are named `source` and `target`, respectively.

```C++
pgl::OrientedSegment s(1,2,3,4), t(3,4,1,2);
if (s != t)
    std::cout << s << " != " << t << std::endl;
// Output: (1,2)->(3,4) != (3,4)->(1,2)
```

You can read the two endpoints of a segment `s` as `s[0]` and `s[1]` or `s.source()` and `s.target()`. You can directly change the endpoints. You can also iterate through the endpoints.

```C++
pgl::OrientedSegment s(1,2,3,4);
s[0][0] = 5;
s.target().x() = 7;
std::cout << s << std::endl;
// Output: (5,2)->(7,4)
```

An oriented segment `s` has all methods of the `Segment` class, with the only difference being for the slope, which may be negative:

- `s.midpoint<ResultNumber>()`: Returns the midpoint. An explicitly integral result type truncates odd coordinates.
- `s.latticePoints<ResultNumber>()`: The same integer points as the unoriented segment's, listed from `source()` to `target()` instead of in increasing order.
- `s.length<ApproximateNumber>()`: Returns `s[0].distance(s[1])`.
- `s.squaredLength()`: Returns `s[0].squaredDistance(s[1])`.
- `s.isDegenerate()`: Returns `s.length() == 0`.
- `s.isPoint()` / `s.getIfPoint()`: Whether the segment collapses to a single point (all defining points equal), and that point as a `std::optional<PointType>`.
- `s.isUndefined()`: Always `false`: a degenerate segment is always a point.
- `s.isVertical()`: Returns `s[0].x() == s[1].x()`.
- `s.isHorizontal()`: Returns `s[0].y() == s[1].y()`.
- `s.containsEndpoint(p)`: Returns `s[0] == p || s[1] == p`
- `s.collinear(t)`: Returns whether `s` and `t` are on the same line, where `t` may be a point or another segment.
- `s.slope<ResultNumber>()`: Returns `(s[1].y()-s[0].y()) / (s[1].x()-s[0].x())`.
- `s.parallel(t)`: Returns whether `s` and `t` have the same slope, but without using division. Here, `t` may be a segment, oriented segment, line, ray, or oriented line.
- `s.yAtX<ResultNumber>(x)`: Returns an `std::optional` with the value of the segment y coordinate at the given coordinate `x`.
- `s.xAtY<ResultNumber>(y)`: Returns an `std::optional` with the value of the segment x coordinate at the given coordinate `y`.

It also has:

- `s.opposite()`: Returns the segment with source and target interchanged.
- `s.orientation(p)`: Given a point `p`, returns the orientation sign of `s[0],s[1],p`: null when they are collinear, negative when `s` sees `p` to its right, and positive when `s` sees `p` to its left.
- `s.rightHalfplane()`: Returns the half-plane defined by all points `p` such that `s.orientation(p) <= 0`.
- `s.leftHalfplane()`: Returns the half-plane defined by all points `p` such that `s.orientation(p) >= 0`.

It knows how to convert itself with an explicit cast to:
- `(pgl::OrientedLine) s` or `s.asOrientedLine()`: Returns the line that contains `s` and has the same orientation.
- `(pgl::Ray) s`  or `s.asRay()`: Returns the half-line that contains `s` and has the same source.

- Other methods:

### EmptyShape

Represents the empty set. Its `size()` is 0, it intersects nothing, and it is contained in everything.

- Other methods:

### Line

The class template `Line` represents an infinite unoriented straight line. A line is stored as any two points it contains, but two lines defined by two distinct collinear points always compare equal. The two points are stored in increasing order.

```C++
pgl::Line l1(1,2,3,4), l2(2,3,1,2);
if (l1 == l2)
    std::cout << l1 << " == " << l2 << std::endl;
// Output: -(1,2)--(3,4)- == -(1,2)--(2,3)-
```

The defining points may be accessed as in a segment and may not be changed directly. The interior of a line is the whole line, so `contains` and `interiorContains` are equivalent.

A line `l` has some additional methods such as:

- `l.isDegenerate()`: Returns `l[0] == l[1]`.
- `l.isUndefined()`: Returns `l.isDegenerate()`: a line through two equal points has no direction and no reasonable interpretation.
- `l.isVertical()`: Returns `l[0].x() == l[1].x()`.
- `l.isHorizontal()`: Returns `l[0].y() == l[1].y()`.
- `l.slope<ResultNumber>()`: Returns `(l[1].y()-l[0].y()) / (l[1].x()-l[0].x())`.
- `l.parallel(t)`: Returns whether `l` and `t` have the same slope, but without using division. Here, `t` may be a segment, oriented segment, line, ray, or oriented line.
- `l.halfplaneAbove()`: Returns the half-plane defined by all points `p` that are above the line (larger y-coordinate). If the line is vertical, then it returns the half-plane with smaller x-coordinate. In other words, it returns the half-plane defined by all points `p` such that `pgl::OrientedSegment(l[0],l[1]).orientation(p) >= 0`, noticing that `l[0] < l[1]`.
- `l.halfplaneBelow()`: Returns the half-plane containing `l` and not `halfplaneAbove`.
- `l.dual<ResultNumber>()`: Returns the point $(a,b)$ such that `l` is defined by $y = ax - b$. Undefined behavior for vertical lines.
- `l.polar<ResultNumber>()`: Returns the point $(a,b)$ such that `l` is defined by $ax + by = 1$. Undefined behavior for lines that contain the origin.
- `l.yAtX<ResultNumber>(x)`: Returns the value of the line y coordinate at the given coordinate `x`.
- `l.xAtY<ResultNumber>(y)`: Returns the value of the line x coordinate at the given coordinate `y`.

- Other methods:


### Oriented Line

The class template `OrientedLine` represents an infinite oriented straight line. An oriented line is stored as any two points it contains but the order matters as the line is oriented from the source to the target point. Two lines defined by two distinct collinear points compare equal if the points are in the same lexicographical order.

```C++
pgl::OrientedLine l1(1,2,3,4), l2(2,3,1,2);
if (l1 != l2)
    std::cout << l1 << " != " << l2;
l2 = l2.opposite();
if (l1 == l2)
    std::cout << l1 << " == " << l2;
// Output: -(1,2)--(3,4)-> != -(2,3)--(1,2)->
//         -(1,2)--(3,4)-> == -(1,2)--(2,3)->
```

The defining points may be accessed as in an oriented segment and may be changed directly. The interior of an oriented line is the whole oriented line, so `contains` and `interiorContains` are equivalent.

An oriented line `l` has methods such as:

- `l.isDegenerate()`: Returns `l[0] == l[1]`.
- `l.isUndefined()`: Returns `l.isDegenerate()`: a line through two equal points has no direction and no reasonable interpretation.
- `l.isVertical()`: Returns `l[0].x() == l[1].x()`.
- `l.isHorizontal()`: Returns `l[0].y() == l[1].y()`.
- `l.opposite()`: Returns the oriented line with source and target interchanged.
- `l.slope<ResultNumber>()`: Returns `(l[1].y()-l[0].y()) / (l[1].x()-l[0].x())`, possibly negative.
- `l.parallel(t)`: Returns whether `l` and `t` have the same slope, but without using division. Here, `t` may be a segment, oriented segment, line, ray, or oriented line.
- `l.halfplaneAbove()`: Returns the half-plane defined by all points `p` that are above the line (larger y-coordinate). If the line is vertical, then it returns the half-plane with smaller x-coordinate. In other words, it returns the half-plane defined by all points `p` such that `pgl::OrientedSegment(l[0],l[1]).orientation(p) <= 0`, noticing that `l[0] < l[1]`.
- `l.halfplaneBelow()`: Returns the half-plane containing `l` and not `halfplaneAbove`.
- `l.orientation(p)`: Given a point `p`, returns the orientation sign of `l[0],l[1],p`: null when they are collinear, negative when `l` sees `p` to its right, and positive when `l` sees `p` to its left.
- `l.rightHalfplane()`: Returns the half-plane defined by all points `p` such that `l.orientation(p) <= 0`.
- `l.leftHalfplane()`: Returns the half-plane defined by all points `p` such that `l.orientation(p) >= 0`.
- `l.yAtX<ResultNumber>(x)`: Returns the value of the line y coordinate at the given coordinate `x`.
- `l.xAtY<ResultNumber>(y)`: Returns the value of the line x coordinate at the given coordinate `y`.

It knows how to convert itself with an explicit cast to:
- `(pgl::Line) l` or `l.asLine()`: Returns the line without the orientation.

- Other methods:


### Ray

The class template `Ray` represents a half-line. A ray is stored as its source endpoint and any other point it contains. Two rays `l1`,`l2` are equal if they have the same source and the other defining point of `l1` is contained in `l2`.

```C++
pgl::Ray l1(1,2,3,4), l2(2,3,1,2);
if (l1 != l2)
    std::cout << l1 << " != " << l2;
l2 = l2.opposite();
if (l1 == l2)
    std::cout << l1 << " == " << l2;
// Output: (1,2)--(3,4)-> != (2,3)--(1,2)->
//         (1,2)--(3,4)-> == (1,2)--(2,3)->
```

The defining points may be accessed as in an oriented segment and may be changed directly. The boundary of a ray is its source.

A ray `l` has methods such as:

- `l.isDegenerate()`: Returns `l[0] == l[1]`.
- `l.isUndefined()`: Returns `l.isDegenerate()`: a ray whose source and target coincide has no direction and no reasonable interpretation.
- `l.isVertical()`: Returns `l[0].x() == l[1].x()`.
- `l.isHorizontal()`: Returns `l[0].y() == l[1].y()`.
- `l.opposite()`: Returns the ray with source and target interchanged.
- `l.slope<ResultNumber>()`: Returns `(l[1].y()-l[0].y()) / (l[1].x()-l[0].x())`, possibly negative.
- `l.parallel(t)`: Returns whether `l` and `t` have the same slope, but without using division. Here, `t` may be a segment, oriented segment, line, ray, or oriented line.
- `l.halfplaneAbove()`: Returns the half-plane defined by all points `p` that are above the line (larger y-coordinate). If the line is vertical, then it returns the half-plane with smaller x-coordinate. In other words, it returns the half-plane defined by all points `p` such that `pgl::OrientedSegment(l[0],l[1]).orientation(p) <= 0`, noticing that `l[0] < l[1]`.
- `l.halfplaneBelow()`: Returns the half-plane containing `l` and not `halfplaneAbove`.
- `l.orientation(p)`: Given a point `p`, returns the orientation sign of `l[0],l[1],p`: null when they are collinear, negative when `l` sees `p` to its right, and positive when `l` sees `p` to its left.
- `l.rightHalfplane()`: Returns the half-plane defined by all points `p` such that `l.orientation(p) <= 0`.
- `l.leftHalfplane()`: Returns the half-plane defined by all points `p` such that `l.orientation(p) >= 0`.
- `l.yAtX<ResultNumber>(x)`: Returns an `std::optional` with the value of the ray y coordinate at the given coordinate `x`.
- `l.xAtY<ResultNumber>(y)`: Returns an `std::optional` with the value of the ray x coordinate at the given coordinate `y`.

It knows how to convert itself with an explicit cast to:
- `(pgl::Line) l` or `l.asLine()`: Returns the line containing the ray.
- `(pgl::OrientedLine) l` or `l.asOrientedLine()`: Returns the oriented line containing the ray and the same orientation.

- Other methods:


### Half-Plane

The class template `Halfplane` is stored as an oriented line, but represents a completely different geometric object that contains all points on its left half-plane. The boundary of the half-plane is the line that defines it. Two half-planes are equal if the corresponding oriented lines are equal:

```C++
pgl::Halfplane h1(1,2,3,4), h2(2,3,1,2);
if (h1 != h2)
    std::cout << h1 << " != " << h2;
// Output: ^-(1,2)--(3,4)-^ != ^-(2,3)--(1,2)-^

h2 = h2.opposite();
if (h1 == h2)
    std::cout << h1 << " == " << h2;
// Output: ^-(1,2)--(3,4)-^ == ^-(1,2)--(2,3)-^
```

The defining points may be accessed as in an oriented segment and may be changed directly.

A half-plane `h` has methods such as:

- `h.isDegenerate()`: Returns `h[0] == h[1]`.
- `h.isUndefined()`: Returns `h.isDegenerate()`: when the boundary collapses to a point it has no direction, so the side it bounds is undetermined.
- `h.isVertical()`: Returns `h[0].x() == h[1].x()`.
- `h.isHorizontal()`: Returns `h[0].y() == h[1].y()`.
- `h.opposite()`: Returns the half-plane with source and target interchanged.
- `h.slope<ResultNumber>()`: Returns `(h[1].y()-h[0].y()) / (h[1].x()-h[0].x())`, possibly negative.

It knows how to convert itself with an explicit cast to:
- `(pgl::Line) l` or `l.asLine()`: Returns the line bounding the half-plane.
- `(pgl::OrientedLine) l` or `l.asOrientedLine()`: Returns the oriented line bounding the half-plane.
- `(pgl::HalfplaneIntersection) h` or `h.asHalfplaneIntersection()`: Returns the half-plane as a one-constraint half-plane intersection.

- Other methods:


### Triangle

The class template `Triangle` is stored as three points, called vertices, which are kept in the following order. The first vertex is the smallest lexicographically and the other two vertices are ordered such that the triangle is oriented counterclockwise (positive orientation test). Two triangles are equal if they have the same vertices.

```C++
pgl::Triangle t(3,3,4,1,1,1);
std::cout << t << std::endl;
// Output: <(1,1)(4,1)(3,3)>
for(size_t i : {0,1,2}) std::cout << t[i] << ' ';
// Output: (1,1) (4,1) (3,3)
for(pgl::Point p : t) std::cout << p << ' ';
// Output: (1,1) (4,1) (3,3)
for(pgl::Segment s : t.edges()) std::cout << s << ' ';
// Output: (1,1)--(4,1) (3,3)--(4,1) (1,1)--(3,3)
for(pgl::OrientedSegment s : t.orientedEdges()) std::cout << s << ' ';
// Output: (1,1)->(4,1) (4,1)->(3,3) (3,3)->(1,1)
```

A triangle `t` has methods such as:

- `t.isDegenerate()`: Returns true if there are equal vertices or all vertices are collinear.
- `t.isPoint()` / `t.getIfPoint()`: Whether the triangle collapses to a single point (all defining points equal), and that point as a `std::optional<PointType>`.
- `t.isSegment()` / `t.getIfSegment()`: Whether the triangle collapses to a segment of positive length (defining points collinear but not all equal), and that segment as a `std::optional<Segment>`.
- `t.isUndefined()`: Always `false`: a degenerate triangle is always a point or a segment.
- `t.centroid<ResultNumber>()`: Returns the centroid.
- `t.circumcircle()`: Returns the circumcircle.
- `t.isRectangle()`: Returns whether one angle is 90 degrees.
- `t.isObtuse()`: Returns whether one angle is greater than 90 degrees.
- `t.isIsosceles()`: Returns whether two sides have the same length.

It knows how to convert itself to:
- `(pgl::Polygon) t` or `t.asPolygon()`: Returns the polygon representation of the triangle.
- `(pgl::Convex) t` or `t.asConvex()`: Returns the convex polygon representation of the triangle.
- `t.asPolygonWithHoles()`: Returns the triangle as a hole-free `PolygonWithHoles` region.

- Other methods:


### Rectangle

The class template `Rectangle` represents an axis-aligned rectangle. While it is stored internally as only two vertices (minimum and maximum x and y coordinates), it behaves as a polygon with four vertices. It can be constructed for any number of points in a container and will construct the bounding box rectangle. If only two points are given, the container is optional. A last argument `pgl::trusted` stores two given corners as the minimum and the maximum, so the first must not exceed the second in either coordinate; inverted corners give the empty rectangle.

A default-constructed rectangle is **empty**: it stores the corners `(0,0)` and `(-1,-1)`, so its maximum falls below its minimum and it covers no point. It is the one canonical empty value, produced by `Rectangle()`, by the `pgl::trusted` constructor given inverted corners, and by any operation that answers with a rectangle covering nothing. Inserting into an empty rectangle does not grow those placeholder corners: `r.insert(p)` makes `r` the single point `p`.

```C++
pgl::Rectangle r({{1,3},{2,4},{3,1},{5,4},{2,3}});
// Same as pgl::Rectangle r({1,1},{5,4}) or pgl::Rectangle r({1,4},{5,1});
std::cout << r << std::endl;
// Output: [(1,1),(5,4)]
std::cout << r.min() << ' ' << r.max() << std::endl;
// Output: (1,1) (5,4)
for(size_t i : {0,1,2,3}) std::cout << r[i] << ' ';
// Output: (1,1) (5,1) (5,4) (1,4)
for(pgl::Point p : r) std::cout << p << ' ';
// Output: (1,1) (5,1) (5,4) (1,4)
for(pgl::Segment s : r.edges()) std::cout << s << ' ';
// Output: (1,1)--(5,1) (5,1)--(5,4) (1,4)--(5,4) (1,1)--(1,4)
for(pgl::OrientedSegment s : r.orientedEdges()) std::cout << s << ' ';
// Output: (1,1)->(5,1) (5,1)->(5,4) (5,4)->(1,4) (1,4)->(1,1)
```

A rectangle `r` has methods such as:

- `r.empty()`: Returns true if the rectangle covers no point, that is, if its stored maximum corner falls below its minimum one on either axis. An empty rectangle behaves as `EmptyShape` everywhere: `r.size()` is `0`, iteration over its corners and edges yields nothing, `r.area()` is `0`, and reading `r[i]`, `r.vertices()`, `r.edges()`, `r.centroid()`, or `r.circumcircle()` is a precondition violation.
- `r.isDegenerate()`: Returns true if the rectangle has null area, which includes the empty one.
- `r.isPoint()` / `r.getIfPoint()`: Whether the rectangle collapses to a single point (all defining points equal), and that point as a `std::optional<PointType>`.
- `r.isSegment()` / `r.getIfSegment()`: Whether the rectangle collapses to a segment of positive length (defining points collinear but not all equal), and that segment as a `std::optional<Segment>`.
- `r.isUndefined()`: Always `false`: a degenerate rectangle is always empty, a point, or a segment.
- `r.centroid<ResultNumber>()`: Returns the centroid.
- `r.circumcircle()`: Returns the circumcircle.
- `r.insert(s)`: Enlarges the rectangle in order to contain a finite shape `s`. The shape must expose `bbox()`.
- `r.insert(points)`: Enlarges the rectangle in order to contain every point in the input range.

It knows how to convert itself to:
- `(pgl::Polygon) r` or `r.asPolygon()`: Returns the polygon representation of the rectangle; an empty rectangle gives the empty polygon.
- `(pgl::Convex) r` or `r.asConvex()`: Returns the convex polygon representation of the rectangle; an empty rectangle gives the empty convex polygon.
- `r.asPolygonWithHoles()`: Returns the rectangle as a hole-free `PolygonWithHoles` region.

- Other methods:


### Disk

The class template `Disk` represents a circle with its interior. Disks are stored internally as three boundary points, in the same way as a `Triangle`. This choice may be surprising, as the standard representation for disks is a center point and a radius. The main motivation is that the circumcircle of a triangle may be represented exactly for integers. Nevertheless, the constructor accepts both forms:

```C++
pgl::Disk d1({1,1}, {2,5}, {4,3}); // Disk from 3 points
pgl::Disk d2({2,3}, 4);            // Disk from a point and a radius
std::cout << d2 << std::endl;
// Output: Disk((-2,3)(6,3)(2,7))  // Output always uses 3 points
```

Disk does not have the `intersection` method and cannot be scaled on a single axis. A disk `d` has methods such as:

- `d.isDegenerate()`: Returns true if the points are collinear or equal.
- `d.isPoint()` / `d.getIfPoint()`: Whether the disk collapses to a single point (all defining points equal), and that point as a `std::optional<PointType>`.
- `d.isUndefined()`: True if the boundary points are collinear but not all equal, so they do not determine a circle (three distinct collinear points have no circle through them; two distinct ones have infinitely many). A disk is never a segment, so this and `isPoint` cover every degenerate disk.
- `d.latticePoints<ResultNumber>()`: The integer points of the disk, its boundary circle included. Exact: no square root is taken and nothing is rounded.
- `d.radius<ApproximateNumber>()`: Returns the radius length. A radius can be irrational, so the result is floating-point by default. Notice that when the disk is defined by center and radius, we may set `ApproximateNumber` to the same number type as the defining point.
- `d.squaredRadius<ResultNumber>()`: Returns the squared radius.
- `d.center<ResultNumber>()`: Returns the center point.
- `d.diameter<ResultNumber>()`: Returns a diameter `Segment`.
- `d.minkowskiSum<ApproximateNumber>(d2)`: Returns the [Minkowski sum](shape_methods.md#minkowski-sum) of two disks, which is a `Disk`: the centers add and so do the radii. A radius is generally a square root of what a disk stores, so the sum is exact only when both disks were built from a center and a radius and `ApproximateNumber` is given an exact type.
- `d.minkowskiErosion<ApproximateNumber>(d2)`: Returns the [Minkowski erosion](shape_methods.md#minkowski-erosion) of two disks as a `std::optional<Disk>`: the centers subtract and so do the radii, and the result is empty when `d2` is the wider disk. Exact under the same condition the sum is.
- Other methods:


### Monotone Chain

The class template `MonotoneChain` represents an x-monotone polyline: a polyline whose vertices are strictly increasing in the lexicographic order (smaller x first, breaking ties by smaller y). A chain with $n$ vertices has $n-1$ edges and no closing edge, so it is an open curve and is automatically simple. Its boundary is its two extreme vertices and its interior is everything else.

A chain may be constructed from any container of points, which will be sorted automatically and duplicates removed. The input is treated as a point set, not as a pre-linked chain, so any permutation of the same points yields the same chain. A last argument `pgl::trusted` stores the points as given, which must then already be sorted and unique.

We use the term above to refer to larger y coordinates and below to refer to smaller y coordinates. A chain `P` with $n$ vertices has methods such as:

- `P.isDegenerate()`: Returns true if the chain has fewer than two vertices, and hence no edge.
- `P.isPoint()` / `P.getIfPoint()`: Whether the chain collapses to a single point (all defining points equal), and that point as a `std::optional<PointType>`.
- `P.isSegment()` / `P.getIfSegment()`: Whether the chain collapses to a segment of positive length (defining points collinear but not all equal), and that segment as a `std::optional<Segment>`.
- `P.isUndefined()`: True only for an empty chain, which has no vertex.
- `P.isStrictlyMonotone()`: Returns true if no two vertices share an x-coordinate, so the chain is the graph of a function of x. Takes $O(n)$ time.
- `P.insert(p)`: Extends the chain in order to contain another point `p` as a vertex.
- `P.insert(points)`: Extends the chain in order to contain all the given points as vertices.
- `P.erase(p)` / `P.erase(i)`: Removes a vertex, given as a point or by its index in the lexicographic order (as in `P[i]`), the first returning whether it was a vertex and the second requiring `i` to be smaller than `P.size()`. Both take $O(n)$ time.
- `P.indexAtX(x)`: Returns an `std::optional<size_t>` that is engaged if the chain contains a point of x-coordinate `x`. The returned value is the smallest index `i` such that `P[i].x() == x`, or the unique `i` with `P[i].x() < x < P[i+1].x()`. Takes $O(\log n)$ time.
- `P.yAtX<ResultNumber>(x)`: Returns an `std::optional` with the y coordinate at `x` (at a vertical edge, the y of the edge's bottom vertex). Takes $O(\log n)$ time.
- `P.isBelow(p)`: Returns an `std::optional<size_t>` that is engaged if a ray shot down from `p` intersects `P`; the value is the index `indexAtX` returns for `p.x()`. Takes $O(\log n)$ time, exactly.
- `P.isAbove(p)`: The same for a ray shot up from `p`. Note that `isBelow` and `isAbove` are not complementary: both are engaged when `p` lies on the chain.
- `P.latticePoints<ResultNumber>()`: The integer points on `P`, in increasing order, the vertex two edges share reported once.
- `P.length<ApproximateNumber>()`, `P.lengthL1()`, `P.lengthLInf()`: Return the Euclidean, Manhattan, and Chebyshev lengths of the chain.
- `P.edgesCross(P2)`: Returns true if `P` has a point strictly above `P2` and a point strictly below it, i.e. every sufficiently small perturbation of the vertices still yields intersecting chains. Unlike `P.crosses(P2)`, a touch that does not swap sides never counts, and the x-extents must overlap in more than a single point. Takes $O(n + m)$ time if `P2` has $m$ vertices.

The monotone structure speeds up several predicates and constructions:

- `P.contains(s)` takes $O(\log n)$ time if `s` is a point, and $O(\log n + k)$ if `s` is a segment whose x-range spans $k$ vertices (a chain contains a segment exactly when the segment is a straight sub-path of the chain).
- `P.intersects(s)` takes $O(\log n + k)$ time for a segment, where $k$ is the number of edges of the chain whose x-range meets that of the segment.
- `P.intersects(P2)` takes $O(n+m)$ time if `P2` is a chain with $m$ vertices, via a merge sweep over the two sorted vertex sequences.


### Polyline

The class template `Polyline` represents a polyline, also called a polygonal chain, polygonal curve, polygonal path, or piecewise linear curve. Unlike `MonotoneChain`, the order of the vertices matter and the polyline is allowed to self-intersect. A polyline with $n$ vertices has $n-1$ edges and no implicit closing edge. An open polyline's boundary is its two extreme vertices and its interior is everything else. A closed polyline, whose first vertex equals its last, has an empty boundary and is its own interior; this includes a polyline whose vertices are all equal. Two polylines are equal when their vertex sequences agree up to reversal and, for closed polylines, up to the starting vertex.
A polyline can be constructed from any container of points, or from a flat list of coordinates.

A polyline `P` with $n$ vertices has methods such as:

- `P.isClosed()`: Returns true if the first vertex equals the last (in particular for a single-vertex polyline), and false for an empty polyline.
- `P.isDegenerate()`: Returns true if all vertices are equal (in particular for an empty or single-vertex polyline).
- `P.isPoint()` / `P.getIfPoint()`: Whether the polyline collapses to a single point (all defining points equal), and that point as a `std::optional<PointType>`.
- `P.isSegment()` / `P.getIfSegment()`: Whether the polyline collapses to a segment of positive length (defining points collinear but not all equal), and that segment as a `std::optional<Segment>`.
- `P.isUndefined()`: True only for an empty polyline, which has no vertex.
- `P.isSimple()`: Returns true if the edges only intersect at the shared endpoints of consecutive edges. The first and last edges are consecutive exactly when the polyline is closed. Takes $O(n \log n)$ time for exact coordinate types and $O(n^2 \log n)$ for floating-point ones.
- `P.latticePoints<ResultNumber>()`: The integer points on `P`, in traversal order, each of them once: a shared vertex, a crossing and a retraced part are reported where the polyline first reaches them.
- `P.length<ApproximateNumber>()`, `P.lengthL1()`, `P.lengthLInf()`: Return the Euclidean, Manhattan, and Chebyshev lengths of the polyline. A self-overlapping polyline counts every traversal of a repeated part.


- Other methods:


### Convex

The class template `Convex` represents a convex polygon. It can be constructed for any number of points in a container and will construct the convex hull. The vertices are stored in counterclockwise order starting from the minimum vertex (minimum x, breaking ties by minimum y). A last argument `pgl::trusted` stores the points as given, which must then already be the convex hull vertices in that order.

A convex polygon `c` has methods such as:

- `c.isDegenerate()`: Returns true if the convex polygon has null area.
- `c.isPoint()` / `c.getIfPoint()`: Whether the polygon collapses to a single point (all defining points equal), and that point as a `std::optional<PointType>`.
- `c.isSegment()` / `c.getIfSegment()`: Whether the polygon collapses to a segment of positive length (defining points collinear but not all equal), and that segment as a `std::optional<Segment>`.
- `c.empty()`: True only for a convex polygon with no vertex, which is the empty set of points: the default-constructed one, the hull of no points, and every convex-valued result that comes back empty.
- `c.isUndefined()`: Always `false`: a degenerate convex polygon is always empty, a point, or a segment.
- `c.centroid<ResultNumber>()`: Returns the centroid.
- `c.smallestEnclosingRectangle()`: Returns the smallest-area enclosing rectangle as a `HalfplaneIntersection` in the polygon's own number type. It is generally not axis-parallel (`c.bbox()` is) and its corners generally need fractions, so ask the region for them at the wanted precision, with `vertices<ResultNumber>()`{HalfplaneIntersection} or `asConvex<ResultNumber>()`{HalfplaneIntersection}. A convex polygon with fewer than three vertices comes back as its own region.
- `c.smallestEnclosingSlab()`: Returns the narrowest strip between two parallel supporting lines as a `HalfplaneIntersection` in the polygon's own number type. The two lines are exact while the distance between them is not, and a convex polygon with fewer than three vertices comes back as its own region.
- `c.squaredMinimumWidth<ResultNumber>()`: Returns the squared distance between those two lines. The width itself is generally irrational while its square is a fraction, so this is the form to compare exactly against a threshold or between polygons. An explicitly integral `ResultNumber` truncates it.
- `c.minimumWidth<ApproximateNumber>()`: Returns that distance as a floating-point number.
- `c.insert(s)`: Enlarges the convex polygon in order to contain a finite shape `s`. The shape must expose its vertices.
- `c.insert(points)`: Enlarges the convex polygon in order to contain every point in the input range.
- `c.upperHull()`: Returns the upper monotone chain.
- `c.lowerHull()`: Returns the lower monotone chain.
- `c.boundary()`: Returns the closed `Polyline` through the vertices in counterclockwise order, the first repeated at the end. Takes $O(n)$ time.

It knows how to convert itself to:
- `(pgl::Polygon) c` or `c.asPolygon()`: Returns the polygon representation of the convex polygon.
- `c.asPolygonWithHoles()`: Returns the convex polygon as a hole-free `PolygonWithHoles` region.

If the convex polygon `c` has $n$ vertices, then:

- `c.diameter()`, `c.smallestEnclosingRectangle()` and the three minimum-width methods take $O(n)$ time. The comparisons they make are degree six in the coordinates, so they run in `BigInt` for integral coordinates and in `ERational` for rational ones, floating point unchanged.
- `c.intersects(s)` takes $O(\log n)$ time if `s` is a shape with $O(1)$ vertices (not including Disk).
- `s.intersects(c)` takes $O(\log n)$ time if `s` is a shape with $O(1)$ vertices (not including Disk).
- `c.intersects(c2)` takes $O(\min(n,m) \log(n+m))$ time if `c2` is a convex polygon with $m$ vertices.
- Other predicates take the same time as `intersects`.

- Other methods:


### Polygon

The class template `Polygon` represents a simple polygon. It can be constructed for any number of points in a container that must be given in the order they appear on the polygon. The vertices are accessed in counterclockwise order starting from the minimum vertex (minimum x, breaking ties by minimum y). A last argument `pgl::trusted` stores the points as given, which must then already be in that order.

A polygon `P` has methods such as:

- `P.isDegenerate()`: Returns true if the polygon has null area.
- `P.isPoint()` / `P.getIfPoint()`: Whether the polygon collapses to a single point (all defining points equal), and that point as a `std::optional<PointType>`.
- `P.isSegment()` / `P.getIfSegment()`: Whether the polygon collapses to a segment of positive length (defining points collinear but not all equal), and that segment as a `std::optional<Segment>`.
- `P.empty()`: True only for a polygon with no vertex, which is the empty set of points.
- `P.isUndefined()`: True if the polygon is degenerate yet covers more than a segment, that is, when its zero area comes from a self-overlapping boundary rather than from collinear vertices. The empty polygon is *not* undefined; use `empty` for it.
- `P.isSimple()`: Returns true if the edges only intersect at the endpoints of consecutive edges. Takes $O(n \log n)$ time for $n$ edges with exact coordinate types and $O(n^2 \log n)$ with floating-point ones.
- `P.isConvex()`: Returns true if the polygon is convex, possibly with vertices subdividing convex hull edges. Takes $O(n)$ time.
- `P.isStarShaped()` / `P.getStarShapedKernel()`: Whether some point of the polygon sees all of it, and the set of such points (the kernel) as a `HalfplaneIntersection`, or `std::nullopt` when the polygon is not star-shaped. A polygon collapsed to a point or a segment is its own kernel. Only meaningful for a simple polygon. Takes $O(n \log n)$ time.
- `P.asPolygonWithHoles()`: Returns the polygon as a hole-free `PolygonWithHoles` region.
- `P.boundary()`: Returns the closed `Polyline` through the vertices in order, the first repeated at the end. Simplicity is not checked. Takes $O(n)$ time.
- `P.asBitMatrix<ResultNumber>()`: Returns the polygon [rasterized](shape_methods.md#other-methods-for-shapes) into a [`BitMatrix`](data_structures.md#bit-matrix) over its bounding box, one bit per covered cell. Every edge must be axis-parallel and every coordinate a whole number, otherwise it throws `std::logic_error`.
- `P.untangle()`: Makes the polygon simple in place, removing vertices where it must. On return `P.isSimple()` holds. No worst-case bound.
- `P.interiorContainsInterior(s)`: Returns true when the open segment lies in the polygon's strict interior; its endpoints may lie on the boundary.

- Other methods:


### Polygon with Holes

The class template `PolygonWithHoles` represents a polygon that has a set of disjoint polygonal holes. Let $P$ be the outer polygon and $H_i^\circ$ be the interior of the hole $H_i$. The polygon with holes $A$ is a closed region defined as the outer polygon minus the **interiors** of the holes:

$$A = P \setminus \bigcup_i H_i^\circ.$$

For all $i$, the hole $H_i$ must satisfy $H_i \subseteq P$. Every pair of distinct holes must be interior disjoint, but their boundaries may intersect.

The boundary of $A$ is the union of the boundary of $P$ and the boundary of every hole $H_i$. Notice that $A$ is connected but its interior may not be.

The outer boundary and every hole are ordinary [`Polygon`](#polygon) values, each in `Polygon`'s own canonical form (counterclockwise, lexicographically smallest vertex first) — holes are *not* stored reversed. Equality, ordering and hashing do not depend on the order the holes were given in. A last argument `pgl::trusted` stores the holes as given, which must then already be sorted, with no zero-area hole.

```C++
pgl::Polygon<> outer({0,0, 10,0, 10,10, 0,10});
pgl::Polygon<> hole({4,4, 6,4, 6,6, 4,6});
pgl::PolygonWithHoles<> region(outer, std::vector{hole});
std::cout << region << std::endl;
// Output: PolygonWithHoles[Polygon[(0,0),(10,0),(10,10),(0,10)],Polygon[(4,4),(6,4),(6,6),(4,6)]]
std::cout << region.area() << ' ' << region.holeCount() << ' ' << region.vertexCount();
// Output: 96 1 8
```

As with [`Polygon`](#polygon), whose constructor does not check simplicity, structural validity is a documented precondition rather than an enforced invariant: every polygon must be simple, each hole must lie inside the outer polygon — closed containment, so a hole may touch the outer ring but never pokes out through it — and hole interiors must be pairwise disjoint. The method `isValid` checks the whole contract on demand.

A region `A` with $n$ vertices in total and $k$ holes has methods such as:

- `A.outer()`: Returns the outer boundary polygon.
- `A.holeCount()` / `A.hasHoles()` / `A.hole(i)` / `A.holes()`: The holes, in canonical (sorted) order. Iterating a region iterates its holes.
- `A.addHole(h)`: Adds a hole, keeping the canonical order. A zero-area ring removes nothing and is ignored.
- `A.eraseHole(i)` / `A.eraseHole(h)`: Fills a hole back in, by its index in the canonical order or by the polygon itself, the second returning whether it found one to erase. Takes $O(k)$ time by index and $O(k + s \log k)$ for a hole of $s$ vertices.
- `A.vertexCount()`: Returns the total number of vertices over all rings, the outer boundary *and* every hole. Deliberately not named `size`, and for the same reason a region has no `operator[]`.
- `A.vertices()` / `A.edges()`: The vertices and the boundary edges of every ring, outer boundary first.
- `A.orientedEdges()`: The boundary edges directed so the region lies to the left: the outer ring counterclockwise as stored, the hole rings **reversed**, i.e. clockwise.
- `A.empty()`: Returns true if the region has no outer boundary at all.
- `A.isDegenerate()`: Returns true if the region has null area.
- `A.isPoint()` / `A.isSegment()`: Whether the region covers exactly one point, or exactly one segment of positive length. Zero-area holes are dropped at construction, so both are decided by the outer boundary alone.
- `A.isUndefined()`: True if the region is degenerate without covering a point or a segment, which includes the empty region.
- `A.isSimple()`: Returns true if every ring is simple. A per-ring check only, saying nothing about how the rings sit relative to one another. Takes $O(n \log n)$ time for exact coordinate types and $O(n^2 \log n)$ for floating-point ones.
- `A.isValid()`: Tests the whole structural contract above. Takes $O(n^2 \log n)$ time.
- `A.isRegular()`: Returns true if the region is the closure of its own interior, $A = \mathrm{closure}(A^\circ)$. Since the contract above constrains interiors only, a valid region may pinch shut along a whole stretch of edge — a **slit**, region material with no area on either side of it, as when a hole shares an edge with another hole or with the outer boundary. A slit belongs to $A$ but not to $\mathrm{closure}(A^\circ)$, so a region with area is regular exactly when it has no slit. Pinching at an isolated *point* is not a slit: the interior still reaches the point from every side, so rings meeting at a vertex leave the region regular. Takes $O(n^2)$ time.
- `A.regularized<ResultNumber>()`: Returns $\mathrm{closure}(A^\circ)$ — the region without its slits — as a `std::vector<PolygonWithHoles>`, the same regularization every [boolean operation](shape_methods.md#boolean-operations) applies to its own result. The result is a set of regions because dropping the slits can disconnect the region; one with no area comes back empty. A region that is already regular is returned unchanged, vertex for vertex; the pieces of one that is not may lose vertices that no longer sit at a corner.
- `A.twiceArea<ResultNumber>()`: Returns twice the area, `2·area(outer) − Σ 2·area(hole)`, exactly and without division.
- `A.area<ResultNumber>()`: Returns the area; the final division by two is exact by default for integral receivers.
- `A.centroid<ResultNumber>()`: Returns the area-weighted centroid, the holes entering with negative weight. When the net area is zero the region has no area-weighted centroid and the centroid of the vertex set is returned instead.
- `A.verticesCentroid<ResultNumber>()`: Returns the centroid of the vertex set over all rings.
- `A.pointInside<ResultNumber>()`: Returns a point strictly inside the region, so inside the outer boundary and outside every hole. It may divide coordinates by four and is undefined for a region with no area.
- `A.triangulation()`: Returns the constrained Delaunay [triangulation](data_structures.md#triangulation) of the region, optionally with extra interior constraint segments. Every ring becomes constrained edges and the hole interiors are left out of the domain, so the in-domain triangles cover exactly the part of the region that has area.
- `A.latticePoints<ResultNumber>()`: The integer points of the region. A hole takes away the points strictly inside it and keeps the ones on its boundary, which belong to the region as any boundary point does.
- `A.asBitMatrix<ResultNumber>()`: The region rasterized into a [`BitMatrix`](data_structures.md#bit-matrix) over its bounding box, the holes left unset, under the same requirements as on a polygon.
- `A.diameter()` / `A.bbox()`: Both are the outer polygon's, the holes being unable to contribute.
- `A.interiorContainsInterior(s)`: Returns true when the open segment lies in the polygon's strict interior. The segment interior must be strictly inside the outer ring and may not touch a hole, while either endpoint may lie on any ring.

Against a region of $n$ vertices and an operand of $m$:

- The predicates against a point take $O(n)$ time. Against a segment, a line, a ray or a half-plane, `intersects` and `interiorContains` take $O(n)$ time, and `contains`, `boundaryContains` and `interiorsIntersect` take $O(n^2)$, except `interiorsIntersect` against a half-plane, which takes $O(n^3)$.
- The distances take $O(n)$ time against a shape of constant size, $O(n \cdot m)$ against a convex polygon, a monotone chain or a polyline, and $O(n \cdot m + (n+m) \log(n+m))$ against a polygon or a region.

- Other methods:


### Polygon Set

The class template `PolygonSet` represents a set of [`PolygonWithHoles`](#polygon-with-holes) components with pairwise disjoint interiors. The point set is simply their union:

$$A = \bigcup_i A_i.$$

This is what the [boolean operations](shape_methods.md#boolean-operations) produce — a difference, union, regularized intersection or symmetric difference can come apart into several pieces — and having it as a shape rather than a `std::vector` is what makes those operations **closed**: a result can be fed straight back in, compared, hashed, drawn, transformed and measured.

```C++
pgl::Polygon<> square({0,0, 10,0, 10,10, 0,10});
pgl::PolygonSet<> holed = square.difference(pgl::Rectangle(3,3,7,7));
std::cout << holed.componentCount() << ' ' << holed.holeCount() << ' ' << holed.area();
// Output: 1 1 84
pgl::PolygonSet<> smaller = holed.difference(pgl::Rectangle(0,0,2,2));  // and again
```

The components are kept sorted by `PolygonWithHoles::operator<=>`, so equality, ordering and hashing do not depend on the order they were given in. A component with no area covers nothing that survives regularization and is dropped, and duplicates are erased. A last argument `pgl::trusted` stores the components as given, which must then already be sorted and distinct, with no zero-area component. The components are deliberately **not** nested: a component stranded inside another's hole is stored beside it, not within it, which is what the cell engine emits and what a flat set can say.

As with [`Polygon`](#polygon) and [`PolygonWithHoles`](#polygon-with-holes), structural validity is a documented precondition rather than an enforced invariant. A set is valid when every component is, when the component interiors are pairwise disjoint, and when no two components share a stretch of edge — they may meet only at finitely many points. `isValid` checks all three on demand.

That last clause is the one that earns its keep. It is what makes the interior of the set the union of the components' interiors, $A^\circ = \bigcup_i A_i^\circ$, and that identity is what lets a question about the set be answered one component at a time. Two squares glued along an edge would have interior points belonging to neither component's interior, and the componentwise answers would be wrong.

A set `A` with $k$ components and $n$ vertices in total has methods such as:

- `A.componentCount()` / `A.component(i)` / `A.components()`: The components, in canonical (sorted) order. Iterating a set iterates its components. Deliberately not `size` and `operator[]`, which count *defining points* on the other shapes.
- `A.addComponent(c)`: Adds a component, keeping the canonical order. One with no area, or one already present, is ignored.
- `A.eraseComponent(i)` / `A.eraseComponent(c)`: Drops a component, by its index in the canonical order or by the region itself, the second returning whether it found one to erase. Takes $O(k)$ time by index and $O(k + s \log k)$ for a component of $s$ vertices.
- `A.vertexCount()` / `A.vertices()` / `A.edges()` / `A.orientedEdges()`: The totals over every ring of every component, with the same meaning they have on a region.
- `A.holeCount()` / `A.hasHoles()`: The total number of holes over all components, and whether there are any.
- `A.empty()`: Returns true if the set has no components at all.
- `A.isDegenerate()` / `A.isPoint()` / `A.isSegment()` / `A.isUndefined()`: A canonical set drops its zero-area components, so a degenerate set is exactly an empty one; only a set adopted with `pgl::trusted` can answer otherwise.
- `A.isConnected()`: Returns true if the set is connected as a point set. This is the library's first shape that need not be — two components that never touch are two pieces — and it is what the [cut predicates](shape_methods.md#predicates) ask before dismissing a remover that misses the set.
- `A.isPinched()`: Returns true if two components touch each other anywhere. A set whose components stay apart is a disjoint union of closed sets at positive distance, and then every predicate folds componentwise exactly.
- `A.isValid()`: Tests the whole structural contract above.
- `A.isRegular()` / `A.regularized<ResultNumber>()`: A set is regular exactly when every component is, since no slit can run between two components. `regularized()` returns a `PolygonSet`, so the regularization is idempotent in the type system and not only in the mathematics.
- `A.twiceArea<ResultNumber>()` / `A.area<ResultNumber>()`: The sum over the components, which is the area of their union because the interiors are disjoint.
- `A.centroid<ResultNumber>()` / `A.verticesCentroid<ResultNumber>()`: The area-weighted centroid over the components, falling back on the vertex centroid when the total area is zero.
- `A.diameter()`: The diameter of the outer rings' convex hull; unlike a region's, it cannot be delegated to any one component.
- `A.bbox()`: The union of the components' boxes.
- `A.latticePoints<ResultNumber>()`: The integer points of the set, over all components at once, so a point two touching components share is reported once.
- `A.pointInside<ResultNumber>()`: A point in the first component's interior.
- `A.triangulation()`: The constrained Delaunay [triangulation](data_structures.md#triangulation) of the set, optionally with extra interior constraint segments. The hole interiors and the gaps between components are left out of the domain.
- `A.convexPartition()` / `A.convexCovering()`: As on a region, derived from the triangulation.
- `A.asBitMatrix<ResultNumber>()`: The set rasterized into a [`BitMatrix`](data_structures.md#bit-matrix) over the bounding box of the whole set, the holes and the gaps between components left unset, under the same requirements as on a region.
- `A.interiorContainsInterior(s)`: Returns true when the open segment lies strictly inside one component; boundary endpoints are allowed, but an interior passage through a component pinch is not.


- Other methods:


### Halfplane Intersection

The class template `HalfplaneIntersection` represents the intersection of a finite set of closed half-planes: a convex region that, unlike `Convex`, may be unbounded (a wedge, a strip, a half-plane, or the whole plane) and may be empty. Its vertices are generally not representable in the coordinate type of the defining half-planes: integer half-planes routinely bound regions with rational vertices, so constructive accessors return ERational coordinates for an integral receiver. An explicit result type such as `k.vertex<pgl::Rational<int64_t>>(i)` is available.

The half-planes are stored sorted counterclockwise by boundary direction, with no redundant half-plane and at most one half-plane per direction. A default-constructed `HalfplaneIntersection` is the **whole plane** (the intersection of no half-planes) — the opposite convention of `Convex()`, which is the empty set. It can also be constructed from a range of $n$ half-planes in $O(n \log n)$ time, or from a `Halfplane`, `Rectangle`, `Triangle`, or `Convex`. A last argument `pgl::trusted` stores a range as given, which must then already be sorted, non-redundant, and bound a region with nonempty interior.

A half-plane intersection `k` has methods such as:

- `k.insert(h)`: Intersects the region with one more half-plane, returning whether it was stored. A redundant or undefined half-plane is discarded, and one that empties the region leaves it in a sticky empty state.
- `k.empty()`, `k.isPlane()`, `k.isBounded()`, `k.isDegenerate()`: State queries. A degenerate region has empty interior (a line, ray, segment, or point built from touching constraints); it remains fully supported by the predicates.
- `k.isUndefined()`: Always `false`: `insert` ignores undefined half-planes, so every region — empty, degenerate, or full-dimensional — is well defined.
- `k.isHalfplane()` / `k.getIfHalfplane()`: Whether the region is exactly one closed half-plane (a single stored constraint), and that half-plane. Exact, no division.
- `k.isLine()` / `k.getIfLine()`: Whether the region is exactly one line, and that line. Exact, no division.
- `k.isPoint()` / `k.getIfPoint<ResultNumber>()`: Whether the region is a single point, and that point. The test and the default returned point are exact for an integral region, including when the point is not representable in `NumberType`.
- `k.isSegment()` / `k.getIfSegment<ResultNumber>()`: Whether the region is a segment of positive length, and that segment. The default endpoints are exact for integral constraints.
- `k.isRay()` / `k.getIfRay<ResultNumber>()`: Whether the region is a ray, and that ray. The default source is exact for integral constraints.
- Together with `empty` and `isPlane` these name every region a half-plane intersection can be, except a full-dimensional one other than a half-plane.
- `k.latticePoints<ResultNumber>()`: The integer points of the region. Throws `std::logic_error` for an unbounded region, which covers infinitely many.
- `k.vertex<ResultNumber>(i)`, `k.vertices<ResultNumber>()`, `k.vertexCount()`: The implicit vertices, counterclockwise for bounded regions.
- `k.edge<ResultNumber>(i)`: The boundary contribution of half-plane `i` as a `std::variant` of `Segment`, `Ray`, or `Line`.
- `k.bbox<ResultNumber>()`, `k.fbox<ApproximateNumber>()`: Bounding box; throws `std::logic_error` when the region is empty or unbounded. With an explicitly integral result type the box is rounded outward so it always encloses the region.
- `k.asConvex<ResultNumber>()`: The region as a `Convex`; throws when unbounded.
- `k.twiceArea<ResultNumber>()`, `k.area<ResultNumber>()`, and `k.centroid<ResultNumber>()`: Measures of a bounded region; they throw when the region is unbounded.
- `k.squaredHausdorffDistance<ResultNumber>(s)`, `k.hausdorffDistanceL1<ResultNumber>(s)`, and `k.hausdorffDistanceLInf<ResultNumber>(s)`: [Hausdorff distances](shape_methods.md#other-methods-for-shapes) to a bounded convex shape — a `Point`, `Segment`, `OrientedSegment`, `Rectangle`, `Triangle`, `Convex`, or another region. They throw `std::logic_error` when either region is unbounded, the distance then being infinite; the empty region is a precondition violation.

If the region has $n$ half-planes, then:

- `k.contains(p)`, `k.intersects(s)`, and the other predicates against points, segments, lines, rays, and half-planes take $O(\log n)$ time, except that `separates` against a half-plane takes $O(n)$, and so, when the region is degenerate, do `crosses` against a segment or a ray and the predicates of those shapes against the region.
- `k.insert(h)` takes $O(n)$ time.
- `k.isBounded()` and `k.vertexCount()` take $O(n)$ time.
- `k.hausdorffDistanceL1(s)` and `k.hausdorffDistanceLInf(s)` take $O(n \cdot m)$ time against a convex shape of $m$ vertices, and `k.squaredHausdorffDistance(s)` takes $O(n \cdot (1 + \log m) + m \cdot \log n)$.

- Other methods:

Equality compares the stored half-planes: for full-dimensional regions the non-redundant half-planes are a canonical function of the point set, so this is geometric equality; for lower-dimensional (degenerate) regions the representation is not unique and equality is representational.
