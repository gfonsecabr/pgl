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
- [`Polygon`](#polygon) Simple polygon.
- [`Convex`](#convex) Convex polygon.
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
Shapes may be degenerate, for example when some of their defining points are equal. The behavior of geometric operations on degenerate shapes is undefined. However, degenerate shapes may safely be constructed and are often constructed by the default constructor that sets all points to the origin.

Shapes are grouped into a polymorphic class `Shape` that use `std::variant` for polymorphism.

```C++
pgl::Shape p = pgl::Point(3,7);
pgl::Shape s = pgl::Segment(1,4,2,9);
pgl::Shape r = pgl::Rectangle(1,4,2,9);
if (r.contains(p))
    std::cout << r << " contains " << p << std::endl;
if (r.intersects(s))
    std::cout << r << " intersects " << s << std::endl;
```

All shapes contain their boundaries (that is, they are closed in the topological sense). The boundary of a shape is the *manifold boundary*, that is:

- A point has no boundary.
- The boundary of a 1-dimensional shape is the set of (at most two) extreme points of the curve. The boundary of a segment are its two vertices. The boundary of a ray is its one vertex. A line has no boundary.
- The boundary of a 2-dimensional shape is defined in the usual way. The boundary of a triangle is its perimeter, the boundary of a halfplane is the line that defines it.


### Point

The `Point` class template defines a point with x and y coordinates. A point may optionally have a [label](types.md#point-label). A point has no boundary and has the point itself as the interior.

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
- `p.dual()`: Returns the dual line $y = ax - b$ for a point $(a,b)$.
- `p.polar()`: Returns the polar line $ax + by = 1$ for a point $(a,b)$. Undefined for the origin.

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

- `s.midpoint()`: Returns the midpoint. Uses division by 2, so make sure that the coordinates are even or a non-integer type is used. Notice that floating point handles divisions by powers of 2 exactly.
- `s.length()`: Returns `s[0].distance(s[1])`.
- `s.squaredLength()`: Returns `s[0].squaredDistance(s[1])`.
- `s.isDegenerate()`: Returns `s.length() == 0`.
- `s.isVertical()`: Returns `s[0].x() == s[1].x()`.
- `s.isHorizontal()`: Returns `s[0].y() == s[1].y()`.
- `s.containsEndpoint(p)`: Returns `s[0] == p || s[1] == p`
- `s.collinear(t)`: Returns whether `s` and `t` are on the same line, where `t` may be a point or another segment.
- `s.slope()`: Returns `(s[1].y()-s[0].y()) / (s[1].x()-s[0].x())`.
- `s.parallel(t)`: Returns whether `s` and `t` have the same slope, but without using division. Here, `t` may be a segment, oriented segment, line, ray, or oriented line.
- `s.yAtX(x)`: Returns an `std::optional` with the value of the segment y coordinate at the given coordinate `x`.
- `s.xAtY(y)`: Returns an `std::optional` with the value of the segment x coordinate at the given coordinate `y`.

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

- `s.midpoint()`: Returns the midpoint. Uses division by 2, so make sure that the coordinates are even or a non-integer type is used. Notice that floating point handles divisions by powers of 2 exactly.
- `s.length()`: Returns `s[0].distance(s[1])`.
- `s.squaredLength()`: Returns `s[0].squaredDistance(s[1])`.
- `s.isDegenerate()`: Returns `s.length() == 0`.
- `s.isVertical()`: Returns `s[0].x() == s[1].x()`.
- `s.isHorizontal()`: Returns `s[0].y() == s[1].y()`.
- `s.containsEndpoint(p)`: Returns `s[0] == p || s[1] == p`
- `s.collinear(t)`: Returns whether `s` and `t` are on the same line, where `t` may be a point or another segment.
- `s.slope()`: Returns `(s[1].y()-s[0].y()) / (s[1].x()-s[0].x())`.
- `s.parallel(t)`: Returns whether `s` and `t` have the same slope, but without using division. Here, `t` may be a segment, oriented segment, line, ray, or oriented line.
- `s.yAtX(x)`: Returns an `std::optional` with the value of the segment y coordinate at the given coordinate `x`.
- `s.xAtY(y)`: Returns an `std::optional` with the value of the segment x coordinate at the given coordinate `y`.

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
- `l.isVertical()`: Returns `l[0].x() == l[1].x()`.
- `l.isHorizontal()`: Returns `l[0].y() == l[1].y()`.
- `l.slope()`: Returns `(l[1].y()-l[0].y()) / (l[1].x()-l[0].x())`.
- `l.parallel(t)`: Returns whether `l` and `t` have the same slope, but without using division. Here, `t` may be a segment, oriented segment, line, ray, or oriented line.
- `l.halfplaneAbove()`: Returns the half-plane defined by all points `p` that are above the line (larger y-coordinate). If the line is vertical, then it returns the half-plane with smaller x-coordinate. In other words, it returns the half-plane defined by all points `p` such that `pgl::OrientedSegment(l[0],l[1]).orientation(p) >= 0`, noticing that `l[0] < l[1]`.
- `l.halfplaneBelow()`: Returns the half-plane containing `l` and not `halfplaneAbove`.
- `l.dual()`: Returns the point $(a,b)$ such that `l` is defined by $y = ax - b$. Undefined behavior for vertical lines.
- `l.polar()`: Returns the point $(a,b)$ such that `l` is defined by $ax + by = 1$. Undefined behavior for lines that contain the origin.
- `l.yAtX(x)`: Returns the value of the line y coordinate at the given coordinate `x`.
- `l.xAtY(y)`: Returns the value of the line x coordinate at the given coordinate `y`.

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
- `l.isVertical()`: Returns `l[0].x() == l[1].x()`.
- `l.isHorizontal()`: Returns `l[0].y() == l[1].y()`.
- `l.opposite()`: Returns the oriented line with source and target interchanged.
- `l.slope()`: Returns `(l[1].y()-l[0].y()) / (l[1].x()-l[0].x())`, possibly negative.
- `l.parallel(t)`: Returns whether `l` and `t` have the same slope, but without using division. Here, `t` may be a segment, oriented segment, line, ray, or oriented line.
- `l.halfplaneAbove()`: Returns the half-plane defined by all points `p` that are above the line (larger y-coordinate). If the line is vertical, then it returns the half-plane with smaller x-coordinate. In other words, it returns the half-plane defined by all points `p` such that `pgl::OrientedSegment(l[0],l[1]).orientation(p) <= 0`, noticing that `l[0] < l[1]`.
- `l.halfplaneBelow()`: Returns the half-plane containing `l` and not `halfplaneAbove`.
- `l.orientation(p)`: Given a point `p`, returns the orientation sign of `l[0],l[1],p`: null when they are collinear, negative when `l` sees `p` to its right, and positive when `l` sees `p` to its left.
- `l.rightHalfplane()`: Returns the half-plane defined by all points `p` such that `l.orientation(p) <= 0`.
- `l.leftHalfplane()`: Returns the half-plane defined by all points `p` such that `l.orientation(p) >= 0`.
- `l.yAtX(x)`: Returns the value of the line y coordinate at the given coordinate `x`.
- `l.xAtY(y)`: Returns the value of the line x coordinate at the given coordinate `y`.

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
- `l.isVertical()`: Returns `l[0].x() == l[1].x()`.
- `l.isHorizontal()`: Returns `l[0].y() == l[1].y()`.
- `l.opposite()`: Returns the ray with source and target interchanged.
- `l.slope()`: Returns `(l[1].y()-l[0].y()) / (l[1].x()-l[0].x())`, possibly negative.
- `l.parallel(t)`: Returns whether `l` and `t` have the same slope, but without using division. Here, `t` may be a segment, oriented segment, line, ray, or oriented line.
- `l.halfplaneAbove()`: Returns the half-plane defined by all points `p` that are above the line (larger y-coordinate). If the line is vertical, then it returns the half-plane with smaller x-coordinate. In other words, it returns the half-plane defined by all points `p` such that `pgl::OrientedSegment(l[0],l[1]).orientation(p) <= 0`, noticing that `l[0] < l[1]`.
- `l.halfplaneBelow()`: Returns the half-plane containing `l` and not `halfplaneAbove`.
- `l.orientation(p)`: Given a point `p`, returns the orientation sign of `l[0],l[1],p`: null when they are collinear, negative when `l` sees `p` to its right, and positive when `l` sees `p` to its left.
- `l.rightHalfplane()`: Returns the half-plane defined by all points `p` such that `l.orientation(p) <= 0`.
- `l.leftHalfplane()`: Returns the half-plane defined by all points `p` such that `l.orientation(p) >= 0`.
- `l.yAtX(x)`: Returns an `std::optional` with the value of the ray y coordinate at the given coordinate `x`.
- `l.xAtY(y)`: Returns an `std::optional` with the value of the ray x coordinate at the given coordinate `y`.

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
- `h.isVertical()`: Returns `h[0].x() == h[1].x()`.
- `h.isHorizontal()`: Returns `h[0].y() == h[1].y()`.
- `h.opposite()`: Returns the half-plane with source and target interchanged.
- `h.slope()`: Returns `(h[1].y()-h[0].y()) / (h[1].x()-h[0].x())`, possibly negative.
- `h.intersection(h2)`: Intersecting with another half-plane returns a [`HalfplaneIntersection`](#halfplane-intersection) — exact and division-free, whether the result is a wedge, a strip, a nested half-plane, a line, or the empty set.

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
- `t.centroid()`: Returns the centroid.
- `t.circumcircle()`: Returns the circumcircle.
- `t.isRectangle()`: Returns whether one angle is 90 degrees.
- `t.isObtuse()`: Returns whether one angle is greater than 90 degrees.
- `t.isIsosceles()`: Returns whether two sides have the same length.

It knows how to convert itself with an explicit cast to:
- `(pgl::Polygon) t` or `t.asPolygon()`: Returns the polygon representation of the triangle.
- `(pgl::Convex) t` or `t.asConvex()`: Returns the convex polygon representation of the triangle.

- Other methods:


### Rectangle

The class template `Rectangle` represents an axis-aligned rectangle. While it is stored internally as only two vertices (minimum and maximum x and y coordinates), it behaves as a polygon with four vertices. It can be constructed for any number of points in a container and will construct the bounding box rectangle. If only two points are given, the container is optional. If the two points are respectively the minimum x and y and the maximum x and y, then an optional argument set to true avoids the bounding box calculation.

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

- `r.isDegenerate()`: Returns true if the rectangle has null area.
- `r.centroid()`: Returns the centroid.
- `r.circumcircle()`: Returns the circumcircle.
- `r.insert(s)`: Enlarges the rectangle in order to contain a finite shape `s`. The shape must expose `bbox()`.
- `r.insert(points)`: Enlarges the rectangle in order to contain every point in the input range.

It knows how to convert itself with an explicit cast to:
- `(pgl::Polygon) r` or `r.asPolygon()`: Returns the polygon representation of the rectangle.
- `(pgl::Convex) r` or `r.asConvex()`: Returns the convex polygon representation of the rectangle.

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
- `d.radius()`: Returns the radius length.
- `d.squaredRadius()`: Returns the squared radius.
- `d.center()`: Returns the center point.
- `d.diameter()`: As always returns a diameter `Segment`, but for disks the segment is always horizontal.

- Other methods:


### Monotone Chain

The class template `MonotoneChain` represents an x-monotone polyline: a polyline whose vertices are strictly increasing in the lexicographic order (smaller x first, breaking ties by smaller y). A chain with $n$ vertices has $n-1$ edges and no closing edge, so it is an open curve and is automatically simple. Its boundary is its two extreme vertices and its interior is everything else.

A chain may be constructed from any container of points, which will be sorted automatically and duplicates removed. The input is treated as a point set, not as a pre-linked chain, so any permutation of the same points yields the same chain. If the points are already sorted and unique, a second parameter true can be given to avoid sorting the points again.

We use the term above to refer to larger y coordinates and below to refer to smaller y coordinates. A chain `P` with $n$ vertices has methods such as:

- `P.isDegenerate()`: Returns true if the chain has fewer than two vertices, and hence no edge.
- `P.isStrictlyMonotone()`: Returns true if no two vertices share an x-coordinate, so the chain is the graph of a function of x. Takes $O(n)$ time.
- `P.insert(p)`: Extends the chain in order to contain another point `p` as a vertex.
- `P.insert(points)`: Extends the chain in order to contain all the given points as vertices.
- `P.indexAtX(x)`: Returns an `std::optional<size_t>` that is engaged if the chain contains a point of x-coordinate `x`. The returned value is the smallest index `i` such that `P[i].x() == x`, or the unique `i` with `P[i].x() < x < P[i+1].x()`. Takes $O(\log n)$ time.
- `P.yAtX(x)`: Returns an `std::optional` with the value of the y coordinate at the given coordinate `x` (at a vertical edge, the y of the edge's bottom vertex). Takes $O(\log n)$ time. **Warning:** interpolation divides, so request a floating-point or `Rational` result type for an accurate value.
- `P.isBelow(p)`: Returns an `std::optional<size_t>` that is engaged if a ray shot down from `p` intersects `P`; the value is the index `indexAtX` returns for `p.x()`. Takes $O(\log n)$ time, exactly.
- `P.isAbove(p)`: The same for a ray shot up from `p`. Note that `isBelow` and `isAbove` are not complementary: both are engaged when `p` lies on the chain.
- `P.length()`, `P.lengthL1()`, `P.lengthLInf()`: Return the Euclidean, Manhattan, and Chebyshev lengths of the chain.

The monotone structure speeds up several predicates and constructions:

- `P.contains(s)` takes $O(\log n)$ time if `s` is a point, and $O(\log n + k)$ if `s` is a segment whose x-range spans $k$ vertices (a chain contains a segment exactly when the segment is a straight sub-path of the chain).
- `P.intersects(s)` takes $O(\log n + k)$ time for a segment overlapping $k$ edges of the chain.
- `P.intersects(P2)` and `P.intersection(P2)` take $O(n+m)$ time if `P2` is a chain with $m$ vertices, via a merge sweep over the two sorted vertex sequences. `P.intersection(s)` returns an `std::vector` of points and segments sorted by the lexicographic order, with collinear overlaps coalesced; the same form is returned for segments, lines, rays, halfplanes, rectangles, triangles, and convex polygons.
- `P.edgesCross(P2)`: Returns true if `P` has a point strictly above `P2` and a point strictly below it, i.e. every sufficiently small perturbation of the vertices of `P` and `P2` still yields intersecting chains. Unlike `P.crosses(P2)`, a touch that does not swap sides never counts. The x-extents of `P` and `P2` must overlap in more than a single point, or the result is false outright — a shared x that is only one chain's own extreme vertex (e.g. a chain that is a single vertical edge) is not robust to perturbation. Takes $O(n \log m + m \log n)$ time if `P2` has $m$ vertices.


### Polyline

The class template `Polyline` represents a polyline, also called a polygonal chain, polygonal curve, polygonal path, or piecewise linear curve. Unlike `MonotoneChain`, the order of the vertices matter and the polyline is allowed to self-intersect. A polyline with $n$ vertices has $n-1$ edges and no closing edge. Its boundary is its two extreme vertices and its interior is everything else.

A polyline can be constructed from any container of points, or from a flat list of coordinates. The only normalization is the direction: the constructor the smallest extreme at index 0. If the sequence is already in canonical direction, a second parameter true skips the check.

A polyline `P` with $n$ vertices has methods such as:

- `P.isDegenerate()`: Returns true if all vertices are equal (in particular for an empty or single-vertex polyline).
- `P.isSimple()`: Returns true if the edges only intersect at the shared endpoints of consecutive edges. In an open chain the first and last edges are not consecutive, so a closed polyline (first vertex equal to the last) is not simple. Takes $O(n \log n)$ time for exact coordinate types (and $O(n^2)$ for floating point).
- `P.length()`, `P.lengthL1()`, `P.lengthLInf()`: Return the Euclidean, Manhattan, and Chebyshev lengths of the polyline. A self-overlapping polyline counts every traversal of a repeated part.

Since a self-intersecting polyline has no monotone structure to exploit, the predicates scan the edges: they take $O(n)$ time against a point or a segment (segment containment takes $O(n \log n)$, as a segment may be covered by several non-consecutive collinear edges) and $O(nm)$ time against another polyline with $m$ vertices. The cut predicates use set semantics: `separates` joins the surviving pieces through self-crossings and revisited vertices, so removing one point never disconnects a closed polyline. Against a two-dimensional region (halfplane, rectangle, triangle, disk, convex, or polygon), `separates` searches the polyline's self-intersection arrangement (built exactly, in $O(n^2)$ pairwise edge intersections) for a cycle through the region's interior, so it also detects a pocket sealed by a loop that never touches the region's boundary.

`P.intersection(s)` returns an `std::vector` of points and segments sorted by the lexicographic order, in the same form as `MonotoneChain`, for segments, lines, rays, halfplanes, rectangles, triangles, convex polygons, monotone chains, and other polylines. Pieces are maximal even though a self-intersecting polyline may report them out of traversal order: collinear touching overlaps are merged (also when they come from non-consecutive edges) and points covered by a reported segment are dropped. It takes $O(n)$ segment intersections against a single shape and $O(nm)$ against a chain or polyline with $m$ vertices, plus a coalescing step quadratic in the number of pieces.

The predicates, distances, and `intersection` are implemented against every shape that supports them; membership in `Shape` is planned.

- Other methods:


### Polygon

The class template `Polygon` represents a simple polygon. It can be constructed for any number of points in a container that must be given in the order they appear on the polygon. The vertices are accessed in counterclockwise order starting from the minimum vertex (minimum x, breaking ties by minimum y).

A polygon `P` has methods such as:

- `P.isDegenerate()`: Returns true if the polygon has null area.
- `P.isSimple()`: Returns true if the edges only intersect at the endpoints of consecutive edges. Takes $O(n \log n)$ time for $n$ edges.
- `P.isConvex()`: Returns true if the polygon is convex, possibly with vertices subdividing convex hull edges. Takes $O(n)$ time.
- `P.untangle()`: Makes the polygon simple in place. Edges that cross are flipped and when a flip is blocked by collinearity (collinear vertices) the offending vertex is removed. On return `P.isSimple()` holds. Worst-case complexity is high.

- Other methods:


### Convex

The class template `Convex` represents a convex polygon. It can be constructed for any number of points in a container and will construct the convex hull. The vertices are stored in counterclockwise order starting from the minimum vertex (minimum x, breaking ties by minimum y). If the container already has the vertices in order, a second constructor parameter can be set to true to avoid computing the convex hull.

A convex polygon `c` has methods such as:

- `c.isDegenerate()`: Returns true if the convex polygon has null area.
- `c.centroid()`: Returns the centroid.
- `c.insert(s)`: Enlarges the convex polygon in order to contain a finite shape `s`. The shape must expose its vertices.
- `c.insert(points)`: Enlarges the convex polygon in order to contain every point in the input range.
- `c.upperHull()`: Returns the upper monotone chain.
- `c.lowerHull()`: Returns the lower monotone chain.

It knows how to convert itself with an explicit cast to:
- `(pgl::Polygon) c` or `c.asPolygon()`: Returns the polygon representation of the convex polygon.

If the convex polygon `c` has $n$ vertices, then:

- `c.diameter()` takes $O(n)$ time.
- `c.intersects(s)` takes $O(\log n)$ time if `s` is a shape with $O(1)$ vertices (not including Disk).
- `s.intersects(c)` takes $O(\log n)$ time if `s` is a shape with $O(1)$ vertices (not including Disk).
- `c.intersects(c2)` takes $O(\min(n+m) \log(n+m))$ time if `c2` is a convex polygon with $m$ vertices.
- Other predicates take the same time as `intersects`.
- `c.intersection(c2)` takes $O((n+m) log (n+m))$ time if `c2` is a convex polygon with $m$ vertices.

- Other methods:

### Halfplane Intersection

The class template `HalfplaneIntersection` represents the intersection of a finite set of closed half-planes: a convex region that, unlike `Convex`, may be unbounded (a wedge, a strip, a half-plane, or the whole plane) and may be empty. Its vertices are generally not representable in the coordinate type of the defining half-planes: integer half-planes routinely bound regions with rational vertices, so the constructive accessors take a result-type template parameter in the usual way (`k.vertex<pgl::Rational<int64_t>>(i)` is exact).

The half-planes are stored sorted counterclockwise by boundary direction, with no redundant half-plane and at most one half-plane per direction. A default-constructed `HalfplaneIntersection` is the **whole plane** (the intersection of no half-planes) — the opposite convention of `Convex()`, which is the empty set. It can also be constructed from a range of half-planes, or from a `Halfplane`, `Rectangle`, `Triangle`, or `Convex`.

A half-plane intersection `k` has methods such as:

- `k.insert(h)`: Intersects the region with one more half-plane. The half-plane is discarded (returning false) when it is redundant; when it empties the region, the region switches to a sticky empty state; otherwise it is stored and the stored half-planes it makes redundant are removed. Amortized $O(\log n)$ comparisons.
- `k.isEmpty()`, `k.isPlane()`, `k.isBounded()`, `k.isDegenerate()`: State queries. A degenerate region has empty interior (a line, ray, segment, or point built from touching constraints); it remains fully supported by the predicates.
- `k.vertex<R>(i)`, `k.vertices<R>()`, `k.vertexCount()`: The implicit vertices, counterclockwise for bounded regions.
- `k.edge<R>(i)`: The boundary contribution of half-plane `i` as a `std::variant` of `Segment`, `Ray`, or `Line`.
- `k.bbox<R>()`, `k.fbox()`: Bounding box; throws `std::logic_error` when the region is empty or unbounded. With an integer result type the box is rounded outward so it always encloses the region.
- `k.asConvex<R>()`: The region as a `Convex`; throws when unbounded.
- `k.intersection(h)`: Intersecting with a `Halfplane`, `Rectangle`, `Triangle`, `Convex`, or another `HalfplaneIntersection` returns another `HalfplaneIntersection`, so the type is closed under these operations and the result is exact (no coordinate divisions).

If the region has $n$ half-planes, then:

- `k.contains(p)`, `k.intersects(s)`, and the other predicates against points, segments, lines, rays, and half-planes take $O(\log n)$ time (`separates` against a half-plane takes $O(n)$).
- `k.insert(h)` takes $O(\log n)$ amortized comparisons (plus vector element moves).
- `k.isBounded()` and `k.vertexCount()` take $O(n)$ time.

Equality compares the stored half-planes: for full-dimensional regions the non-redundant half-planes are a canonical function of the point set, so this is geometric equality; for lower-dimensional (degenerate) regions the representation is not unique and equality is representational.

`HalfplaneIntersection` is an alternative of the polymorphic `Shape` class and can be drawn on a [canvas](canvas.md), which clips the region to the visible viewport and strokes only its real boundary edges. Note that `Shape::get`, `Shape::operator[]`, and `Shape::index` throw for this alternative, since its indexable elements are half-planes rather than points.

