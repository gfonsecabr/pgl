<!-- AUTO-GENERATED from doc/raw/shapes.md by doc/raw/doxylink.py — do not edit; edit the raw version and regenerate. -->

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

##### 2-dimensional shapes:
- [`Halfplane`](#half-plane) A straight line and all points on one side of it.
- [`Triangle`](#triangle) Unoriented triangle.
- [`Rectangle`](#rectangle) Axis-aligned rectangle.
- [`Disk`](#disk) A circle with its interior.
- [`Polygon`](#polygon) Simple polygon.
- [`Convex`](#convex) Convex polygon.

All shapes are template classes with a parameter that is a [`Point`](https://gfonsecabr.github.io/pgl/structpgl_1_1Point.html) type, with `pgl::Point<int>` as default:

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

Shapes are grouped into a polymorphic class [`Shape`](https://gfonsecabr.github.io/pgl/structpgl_1_1Shape.html) that use `std::variant` for polymorphism.

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

The [`Point`](https://gfonsecabr.github.io/pgl/structpgl_1_1Point.html) class template defines a point with x and y coordinates. A point may optionally have a [label](types.md#point-label). A point has no boundary and has the point itself as the interior.

```C++
pgl::Point p = {7,9};
pgl::Point<double> q = {3.5,2.25};
pgl::Point<int,std::string> c = {3,5,"center"};
```

You can read and change the coordinates of a point `p` as `p[0]` and `p[1]` or [`p.x()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Point.html) and [`p.y()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Point.html). You can also iterate through the coordinates.

```C++
pgl::Point p;
p.x() = 7;
p[1] = 9;
for(int coord : p) std::cout << coord << ' ';
std::cout << p << std::endl;
// Output: 7 9 (7,9)
```

A point has methods:
- [`p.swapped()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Point.html#a313ef4ae28a7d9f0405b6dd455775120): Returns the point with x and y coordinates swapped.
- [`p.dual()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Point.html): Returns the dual line $y = ax - b$ for a point $(a,b)$.
- [`p.polar()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Point.html): Returns the polar line $ax + by = 1$ for a point $(a,b)$. Undefined for the origin.

### Segment

The [`Segment`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html) class template defines an unoriented straight line segment. The segment always stores the endpoints in increasing order. 

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

- [`s.midpoint()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html#a6ba4f70a48483f85bb36651d9fa16275): Returns the midpoint. Uses division by 2, so make sure that the coordinates are even or a non-integer type is used. Notice that floating point handles divisions by powers of 2 exactly.
- [`s.length()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html#a8c2640b1515b891d3458b89e2bf852ac): Returns `s[0].distance(s[1])`.
- [`s.squaredLength()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html#ae7e06f6bf41a97e0b9a2e2af80eb442d): Returns `s[0].squaredDistance(s[1])`.
- [`s.isDegenerate()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html#a2d653f0f6dfd0664e8afe91816e262c1): Returns `s.length() == 0`.
- [`s.isVertical()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html#ad93d1387983c7189e9e201382111c87c): Returns `s[0].x() == s[1].x()`.
- [`s.isHorizontal()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html#a6903b76ac4686b9335f6f116c53f322d): Returns `s[0].y() == s[1].y()`.
- [`s.containsEndpoint(p)`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html#a90315b916a47d2356ff1793db49f9d63): Returns `s[0] == p || s[1] == p`
- [`s.collinear(t)`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html): Returns whether `s` and `t` are on the same line, where `t` may be a point or another segment.
- [`s.slope()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html#a6104dac8ebf1ae689aad41031b1c853e): Returns [`abs((s[1].y()-s[0].y()) / (s[1].x()-s[0].x()))`](https://gfonsecabr.github.io/pgl/namespacepgl.html#a2befeba538966a5420f472d6c842ad1a).
- [`s.parallel(t)`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html): Returns whether `s` and `t` have the same slope absolute value, but without using division. Here, `t` may be a segment, oriented segment, line, ray, or oriented line.
- [`s.yAtX(x)`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html#af4487113eb0a0b25129415b203d39b27): Returns an `std::optional` with the value of the segment y coordinate at the given coordinate `x`.
- [`s.xAtY(y)`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html#afc0fd6524c9dd5b450e9ca6393df79d9): Returns an `std::optional` with the value of the segment x coordinate at the given coordinate `y`.

It knows how to convert itself with an explicit cast to:
- `(pgl::Line) s` or [`s.asLine()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html#a17e749637dd68e87c00a49b75e80f958): Returns the line that contains `s`.


### Oriented Segment

The [`OrientedSegment`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html) class template defines an oriented straight line segment. The user chooses the order of the two endpoints, which are named [`source`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html) and [`target`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html), respectively.

```C++
pgl::OrientedSegment s(1,2,3,4), t(3,4,1,2);
if (s != t)
    std::cout << s << " != " << t << std::endl;
// Output: (1,2)->(3,4) != (3,4)->(1,2)
```

You can read the two endpoints of a segment `s` as `s[0]` and `s[1]` or [`s.source()`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html) and [`s.target()`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html). You can directly change the endpoints. You can also iterate through the endpoints.

```C++
pgl::OrientedSegment s(1,2,3,4);
s[0][0] = 5;
s.target().x() = 7;
std::cout << s << std::endl;
// Output: (5,2)->(7,4)
```

An oriented segment `s` has all methods of the [`Segment`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html) class, with the only difference being for the slope, which may be negative:

- [`s.midpoint()`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html#a9fcc978c0e1b9d9053f2182da496db20): Returns the midpoint. Uses division by 2, so make sure that the coordinates are even or a non-integer type is used. Notice that floating point handles divisions by powers of 2 exactly.
- [`s.length()`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html#a003b0ff77fdeaff80dfcd5591f884a8f): Returns `s[0].distance(s[1])`.
- [`s.squaredLength()`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html#a9314e8f0decc3ea85b2ec04a6bb845d8): Returns `s[0].squaredDistance(s[1])`.
- [`s.isDegenerate()`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html#a4dc0c036962ec7a1a9142296de15e64f): Returns `s.length() == 0`.
- [`s.isVertical()`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html#a7193013018c0600c23b9719e9a631247): Returns `s[0].x() == s[1].x()`.
- [`s.isHorizontal()`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html#ac8af96509cf5232e92b04a02cedcfa46): Returns `s[0].y() == s[1].y()`.
- [`s.containsEndpoint(p)`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html#a270cc8f80403c643fd94bc36e9ecff50): Returns `s[0] == p || s[1] == p`
- [`s.collinear(t)`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html): Returns whether `s` and `t` are on the same line, where `t` may be a point or another segment.
- [`s.slope()`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html#a2e104f74709a9c280784291df2e868d0): Returns `(s[1].y()-s[0].y()) / (s[1].x()-s[0].x())`.
- [`s.parallel(t)`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html): Returns whether `s` and `t` have the same slope absolute value, but without using division. Here, `t` may be a segment, oriented segment, line, ray, or oriented line.
- [`s.yAtX(x)`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html#a8f66de4ed57d7708e522a0bb1f77b566): Returns an `std::optional` with the value of the segment y coordinate at the given coordinate `x`.
- [`s.xAtY(y)`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html#ae08fd372179c6d2b5d146435c2e3abc8): Returns an `std::optional` with the value of the segment x coordinate at the given coordinate `y`.

It also has:

- [`s.opposite()`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html#abd0d595b6568119fd0a07a4ef5f4250b): Returns the segment with source and target interchanged.
- [`s.orientation(p)`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html#a531355c630b3b171d3e7ab41f6842149): Given a point `p`, returns the orientation sign of `s[0],s[1],p`: null when they are collinear, negative when `s` sees `p` to its right, and positive when `s` sees `p` to its left.
- [`s.rightHalfplane()`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html#a16de5b4095b9e58bbd2caa405fe44a7c): Returns the half-plane defined by all points `p` such that `s.orientation(p) <= 0`.
- [`s.leftHalfplane()`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html#a848ba0978471a2ad0df5e246ecd2d077): Returns the half-plane defined by all points `p` such that `s.orientation(p) >= 0`.

It knows how to convert itself with an explicit cast to:
- `(pgl::OrientedLine) s` or [`s.asOrientedLine()`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html#a32abd5495bfe740ffcfaead86078214b): Returns the line that contains `s` and has the same orientation.
- `(pgl::Ray) s`  or [`s.asRay()`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html#ab9b6246fac158c6af6fc77f20b713a04): Returns the half-line that contains `s` and has the same source.

### EmptyShape

Represents the empty set. Its [`size()`](https://gfonsecabr.github.io/pgl/structpgl_1_1EmptyShape.html#a5bd45b3506aa274a0cd9f12ca257ab8f) is 0, it intersects nothing, and it is contained in everything.

### Line

The class template [`Line`](https://gfonsecabr.github.io/pgl/structpgl_1_1Line.html) represents an infinite unoriented straight line. A line is stored as any two points it contains, but two lines defined by two distinct collinear points always compare equal. The two points are stored in increasing order. 

```C++
pgl::Line l1(1,2,3,4), l2(2,3,1,2);
if (l1 == l2)
    std::cout << l1 << " == " << l2 << std::endl;
// Output: -(1,2)--(3,4)- == -(1,2)--(2,3)-
```

The defining points may be accessed as in a segment and may not be changed directly. The interior of a line is the whole line, so [`contains`](https://gfonsecabr.github.io/pgl/structpgl_1_1Line.html) and [`interiorContains`](https://gfonsecabr.github.io/pgl/structpgl_1_1Line.html) are equivalent.

A line `l` has some additional methods such as:

- [`l.isDegenerate()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Line.html#aea48143a2889d294c15306a18bbc382f): Returns `l[0] == l[1]`.
- [`l.isVertical()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Line.html#a4b206c7339d9e8dd2c13c41de28f2d12): Returns `l[0].x() == l[1].x()`.
- [`l.isHorizontal()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Line.html#a066e7201c620f1ce7311ca130b6bd672): Returns `l[0].y() == l[1].y()`.
- [`l.slope()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Line.html#a65b09754cb121f239f3133a082fbdaeb): Returns [`abs((l[1].y()-l[0].y()) / (l[1].x()-l[0].x()))`](https://gfonsecabr.github.io/pgl/namespacepgl.html#a2befeba538966a5420f472d6c842ad1a).
- [`l.parallel(t)`](https://gfonsecabr.github.io/pgl/structpgl_1_1Line.html): Returns whether `l` and `t` have the same slope absolute value, but without using division. Here, `t` may be a segment, oriented segment, line, ray, or oriented line.
- [`l.halfplaneAbove()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Line.html#a68570dbd34b227416c066766c61249ef): Returns the half-plane defined by all points `p` that are above the line (larger y-coordinate). If the line is vertical, then it returns the half-plane with smaller x-coordinate. In other words, it returns the half-plane defined by all points `p` such that `pgl::OrientedSegment(l[0],l[1]).orientation(p) <= 0`, noticing that `l[0] < l[1]`.
- [`l.halfplaneBelow()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Line.html#aecad497c330e3cfdeac70d0b54bebf3a): Returns the half-plane containing `l` and not [`halfplaneAbove`](https://gfonsecabr.github.io/pgl/structpgl_1_1Line.html#a68570dbd34b227416c066766c61249ef).
- [`l.dual()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Line.html#aa72029be2b77416b240bb2511a8e6e57): Returns the point $(a,b)$ such that `l` is defined by $y = ax - b$. Undefined behavior for vertical lines.
- [`l.polar()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Line.html#a9a9a9d07c0d3aafcc6b273bc70dfbb0a): Returns the point $(a,b)$ such that `l` is defined by $ax + by = 1$. Undefined behavior for lines that contain the origin.
- [`l.yAtX(x)`](https://gfonsecabr.github.io/pgl/structpgl_1_1Line.html#a7788ca87e20897f7a5a4efe74fd7055c): Returns the value of the line y coordinate at the given coordinate `x`.
- [`l.xAtY(y)`](https://gfonsecabr.github.io/pgl/structpgl_1_1Line.html#a70a94d1e1a7c7bd1108ae00c273fde85): Returns the value of the line x coordinate at the given coordinate `y`.


### Oriented Line

The class template [`OrientedLine`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedLine.html) represents an infinite oriented straight line. An oriented line is stored as any two points it contains but the order matters as the line is oriented from the source to the target point. Two lines defined by two distinct collinear points compare equal if the points are in the same lexicographical order.

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

The defining points may be accessed as in an oriented segment and may be changed directly. The interior of an oriented line is the whole oriented line, so [`contains`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedLine.html) and [`interiorContains`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedLine.html) are equivalent.

An oriented line `l` has methods such as:

- [`l.isDegenerate()`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedLine.html#ab0a5530262e75a3f25423ea3f5bf14cd): Returns `l[0] == l[1]`.
- [`l.isVertical()`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedLine.html#add58c7c5ce15d56d0d142e7bc145d581): Returns `l[0].x() == l[1].x()`.
- [`l.isHorizontal()`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedLine.html#aed8a8d6c00d0366a910bc72a1f646040): Returns `l[0].y() == l[1].y()`.
- [`l.opposite()`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedLine.html#a9bf9b108074f26a0943b5d4cc34d8a67): Returns the oriented line with source and target interchanged.
- [`l.slope()`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedLine.html#a31868e82121d74b3016766b5482ebff6): Returns `(l[1].y()-l[0].y()) / (l[1].x()-l[0].x())`, possibly negative.
- [`l.parallel(t)`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedLine.html): Returns whether `l` and `t` have the same slope absolute value, but without using division. Here, `t` may be a segment, oriented segment, line, ray, or oriented line.
- [`l.halfplaneAbove()`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedLine.html#aeda2dc46e413a0d34b5cb2a127a29ba5): Returns the half-plane defined by all points `p` that are above the line (larger y-coordinate). If the line is vertical, then it returns the half-plane with smaller x-coordinate. In other words, it returns the half-plane defined by all points `p` such that `pgl::OrientedSegment(l[0],l[1]).orientation(p) <= 0`, noticing that `l[0] < l[1]`.
- [`l.halfplaneBelow()`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedLine.html#ad35665c61b605d8b0057ae774367d7d6): Returns the half-plane containing `l` and not [`halfplaneAbove`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedLine.html#aeda2dc46e413a0d34b5cb2a127a29ba5).
- [`l.orientation(p)`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedLine.html#aabcb4586ac2a8cc5c655eaa9f2208bca): Given a point `p`, returns the orientation sign of `l[0],l[1],p`: null when they are collinear, negative when `l` sees `p` to its right, and positive when `l` sees `p` to its left.
- [`l.rightHalfplane()`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedLine.html#a5b450be0b6c1ebcee8d26b805ba000fb): Returns the half-plane defined by all points `p` such that `l.orientation(p) <= 0`.
- [`l.leftHalfplane()`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedLine.html#a3d80b03525b963c2255216fb6769ddd0): Returns the half-plane defined by all points `p` such that `l.orientation(p) >= 0`.
- [`l.yAtX(x)`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedLine.html#ad7299b16efd66c8a5e620684c240a96e): Returns the value of the line y coordinate at the given coordinate `x`.
- [`l.xAtY(y)`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedLine.html#a9066c3e2a39cb45346775fe4f985ef72): Returns the value of the line x coordinate at the given coordinate `y`.

It knows how to convert itself with an explicit cast to:
- `(pgl::Line) l` or [`l.asLine()`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedLine.html#abb932f7bbe6ceb52cc031c12d8c4558b): Returns the line without the orientation.


### Ray

The class template [`Ray`](https://gfonsecabr.github.io/pgl/structpgl_1_1Ray.html) represents a half-line. A ray is stored as its source endpoint and any other point it contains. Two rays `l1`,`l2` are equal if they have the same source and the other defining point of `l1` is contained in `l2`.

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

- [`l.isDegenerate()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Ray.html#a68014d94d2e314877bf4a09aaf39ad39): Returns `l[0] == l[1]`.
- [`l.isVertical()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Ray.html#a05e1dd210bf5c5c3ec92adfceb6d5fa5): Returns `l[0].x() == l[1].x()`.
- [`l.isHorizontal()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Ray.html#aa75a93acd72c880836337a5b2bae3b9f): Returns `l[0].y() == l[1].y()`.
- [`l.opposite()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Ray.html#a749a6d7a25334d7d452c06c939a1ef24): Returns the ray with source and target interchanged.
- [`l.slope()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Ray.html#a93ea798d1e9270ced1a7450346fda9b1): Returns `(l[1].y()-l[0].y()) / (l[1].x()-l[0].x())`, possibly negative.
- [`l.parallel(t)`](https://gfonsecabr.github.io/pgl/structpgl_1_1Ray.html): Returns whether `l` and `t` have the same slope absolute value, but without using division. Here, `t` may be a segment, oriented segment, line, ray, or oriented line.
- [`l.halfplaneAbove()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Ray.html#a00aedab9d7a8015114914fd415645177): Returns the half-plane defined by all points `p` that are above the line (larger y-coordinate). If the line is vertical, then it returns the half-plane with smaller x-coordinate. In other words, it returns the half-plane defined by all points `p` such that `pgl::OrientedSegment(l[0],l[1]).orientation(p) <= 0`, noticing that `l[0] < l[1]`.
- [`l.halfplaneBelow()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Ray.html#aa8ccb101750ad8b384d3637e9ff74a76): Returns the half-plane containing `l` and not [`halfplaneAbove`](https://gfonsecabr.github.io/pgl/structpgl_1_1Ray.html#a00aedab9d7a8015114914fd415645177).
- [`l.orientation(p)`](https://gfonsecabr.github.io/pgl/structpgl_1_1Ray.html#a0cb4c142550a83fb2a46205e3c566bd4): Given a point `p`, returns the orientation sign of `l[0],l[1],p`: null when they are collinear, negative when `l` sees `p` to its right, and positive when `l` sees `p` to its left.
- [`l.rightHalfplane()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Ray.html#a0a101de661262dab7e4bc1e48dffd724): Returns the half-plane defined by all points `p` such that `l.orientation(p) <= 0`.
- [`l.leftHalfplane()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Ray.html#a01083ffe650f8b9ffaa393f7574f6005): Returns the half-plane defined by all points `p` such that `l.orientation(p) >= 0`.
- [`l.yAtX(x)`](https://gfonsecabr.github.io/pgl/structpgl_1_1Ray.html#aca8c3ed33fcaafeb3a212cc37b1c51b7): Returns an `std::optional` with the value of the ray y coordinate at the given coordinate `x`.
- [`l.xAtY(y)`](https://gfonsecabr.github.io/pgl/structpgl_1_1Ray.html#a1da16a6f7958862040effa675515dc07): Returns an `std::optional` with the value of the ray x coordinate at the given coordinate `y`.

It knows how to convert itself with an explicit cast to:
- `(pgl::Line) l` or [`l.asLine()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Ray.html#ae47f5294685f87b606737e49cf1982f9): Returns the line containing the ray.
- `(pgl::OrientedLine) l` or [`l.asOrientedLine()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Ray.html#a714c4d79f71316abc554f9b5f832b197): Returns the oriented line containing the ray and the same orientation.


### Half-Plane

The class template [`Halfplane`](https://gfonsecabr.github.io/pgl/structpgl_1_1Halfplane.html) is stored as an oriented line, but represents a completely different geometric object that contains all points on its left half-plane. The boundary of the half-plane is the line that defines it. Two half-planes are equal if the corresponding oriented lines are equal:

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

Halfplane does not have an [`intersection`](https://gfonsecabr.github.io/pgl/structpgl_1_1Halfplane.html) method. The defining points may be accessed as in an oriented segment and may be changed directly.

A half-plane `h` has methods such as:

- [`h.isDegenerate()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Halfplane.html#ae962754a791826b988b39273a9e0da7e): Returns `h[0] == h[1]`.
- [`h.isVertical()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Halfplane.html#a86a553f8847f65c8e89fe1a612ffcfe3): Returns `h[0].x() == h[1].x()`.
- [`h.isHorizontal()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Halfplane.html#ae4b2bac2a079079c00870916a7c93b0f): Returns `h[0].y() == h[1].y()`.
- [`h.opposite()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Halfplane.html#a4bcdbe04bd9d5f266d6c1bdf0bc2ad9e): Returns the half-plane with source and target interchanged.
- [`h.slope()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Halfplane.html#aea8e2495979f8da401683ed157578993): Returns `(h[1].y()-h[0].y()) / (h[1].x()-h[0].x())`, possibly negative.

It knows how to convert itself with an explicit cast to:
- `(pgl::Line) l` or [`l.asLine()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Halfplane.html#ade5884b09f45d35c46a09f9cf90c32b6): Returns the line bounding the half-plane.
- `(pgl::OrientedLine) l` or [`l.asOrientedLine()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Halfplane.html#ade8b1566d820ead8ec0d5865c2d46d3c): Returns the oriented line bounding the half-plane.


### Triangle

The class template [`Triangle`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html) is stored as three points, called vertices, which are kept in the following order. The first vertex is the smallest lexicographically and the other two vertices are ordered such that the triangle is oriented counterclockwise (positive orientation test). Two triangles are equal if they have the same vertices.

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

- [`t.isDegenerate()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html#ab159614e587f6dc4a75789f98fb7a848): Returns true if there are equal vertices or all vertices are collinear.
- [`t.centroid()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html#af601a145a531947ed2b4475ab0102f78): Returns the centroid.
- [`t.circumcircle()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html#a0b9e50f4c10a634c4706884ff2894fb7): Returns the circumcircle.
- [`t.isRectangle()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html#a4cfb7d8fffa11293343c535bc4825e3f): Returns whether one angle is 90 degrees.
- [`t.isObtuse()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html#a7848d637384e479a97395489bf9a61fb): Returns whether one angle is greater than 90 degrees.
- [`t.isIsosceles()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html#a3cc30d928e1cb21c99eae9765f38e3d4): Returns whether two sides have the same length.

It knows how to convert itself with an explicit cast to:
- `(pgl::Polygon) t` or [`t.asPolygon()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html#aaa078dd16798d8d58bb3cb46f64aba3b): Returns the polygon representation of the triangle.
- `(pgl::Convex) t` or [`t.asConvex()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html#ab01b4c98a0859edd28f1961012ff1447): Returns the convex polygon representation of the triangle.


### Rectangle

The class template [`Rectangle`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html) represents an axis-aligned rectangle. While it is stored internally as only two vertices (minimum and maximum x and y coordinates), it behaves as a polygon with four vertices. It can be constructed for any number of points in a container and will construct the bounding box rectangle. If only two points are given, the container is optional. If the two points are respectively the minimum x and y and the maximum x and y, then an optional argument set to true avoids the bounding box calculation.

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

- [`r.isDegenerate()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html#a380388f13b2cd9a5ad8f4f806e31be84): Returns true if the rectangle has null area.
- [`r.centroid()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html#a553634d0d486ae85ec22c6a036855f0e): Returns the centroid.
- [`r.circumcircle()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html#a96f720c89b9116686118e6085374a7ab): Returns the circumcircle.
- [`r.insert(s)`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html): Enlarges the rectangle in order to contain a finite shape `s`. The shape must expose [`bbox()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html#ae88798364c0bf20cd4d14c259fa33dcd).
- [`r.insert(points)`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html): Enlarges the rectangle in order to contain every point in the input range.

It knows how to convert itself with an explicit cast to:
- `(pgl::Polygon) r` or [`r.asPolygon()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html#af850885808c25c90aaa04506cc1f1537): Returns the polygon representation of the rectangle.
- `(pgl::Convex) r` or [`r.asConvex()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html#a9c20bb08763579e119ea85d68a907ab3): Returns the convex polygon representation of the rectangle.


### Disk

The class template [`Disk`](https://gfonsecabr.github.io/pgl/structpgl_1_1Disk.html) represents a circle with its interior. Disks are stored internally as three boundary points, in the same way as a [`Triangle`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html). This choice may be surprising, as the standard representation for disks is a center point and a radius. The main motivation is that the circumcircle of a triangle may be represented exactly for integers. Nevertheless, the constructor accepts both forms:

```C++
pgl::Disk d1({1,1}, {2,5}, {4,3}); // Disk from 3 points
pgl::Disk d2({2,3}, 4);            // Disk from a point and a radius
std::cout << d2 << std::endl;
// Output: Disk((-2,3)(6,3)(2,7))  // Output always uses 3 points
```

Disk does not have the `intersection` method and cannot be scaled on a single axis. A disk `d` has methods such as:

- [`d.isDegenerate()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Disk.html#a19e32c8b7656bfe02982c8a40ea0ff57): Returns true if the points are collinear or equal.
- [`d.radius()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Disk.html#a69765084e501902a81583032fdb7c816): Returns the radius length.
- [`d.squaredRadius()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Disk.html#a0027c4ab0d85b7ff3e5b0d5b42b1745f): Returns the squared radius.
- [`d.center()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Disk.html#a01f21dd5b971164474843df4ad71bfc1): Returns the center point.
- [`d.diameter()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Disk.html#ad7c86ba7778cf349bcb6c653e0470ba5): As always returns a diameter [`Segment`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html), but for disks the segment is always horizontal.


### Polygon

The class template [`Polygon`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html) represents a simple polygon. It can be constructed for any number of points in a container that must be given in the order they appear on the polygon. The vertices are accessed in counterclockwise order starting from the minimum vertex (minimum x, breaking ties by minimum y). ~~Internally, the polygon is stored as multiple x-monotone polylines for improved performance.~~

A polygon `P` has methods such as:

- [`P.isDegenerate()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html#af1d297b10aee661458543c2f5fa00645): Returns true if the polygon has null area.
- [`P.isSimple()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html#a5e182cf106f6be5ed3fb49c07daf80c6): Returns true if the edges only intersect at the endpoints of consecutive edges. Takes $O(n \log n)$ time for $n$ edges.


### Convex

The class template [`Convex`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html) represents a convex polygon. It can be constructed for any number of points in a container and will construct the convex hull. The vertices are stored in counterclockwise order starting from the minimum vertex (minimum x, breaking ties by minimum y). If the container already has the vertices in order, a second constructor parameter can be set to true to avoid computing the convex hull.

A convex polygon `c` has methods such as:

- [`c.isDegenerate()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html#a460c2cbe73f75e60d0153be2acb1662d): Returns true if the convex polygon has null area.
- [`c.centroid()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html#ad4000c6649317b13e2695a209d9102e8): Returns the centroid.
- `c.insert(s)`: Enlarges the convex polygon in order to contain a finite shape `s`. The shape must expose its vertices.
- `c.insert(points)`: Enlarges the convex polygon in order to contain every point in the input range.
- `c.upperHull()`: Returns the upper monotone polyline.
- `c.lowerHull()`: Returns the lower monotone polyline.

It knows how to convert itself with an explicit cast to:
- `(pgl::Polygon) c` or [`c.asPolygon()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html#ab08ccc569a03b8056db708b1f4e26e2f): Returns the polygon representation of the convex polygon.

If the convex polygon `c` has $n$ vertices, then:

- [`c.diameter()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html#ad44a3c0e434930328c723fcae6c4c7c9) takes $O(n)$ time.
- [`c.intersects(s)`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html) takes $O(\log n)$ time if `s` is a shape with $O(1)$ vertices (not including Disk).
- [`s.intersects(c)`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html) takes $O(\log n)$ time if `s` is a shape with $O(1)$ vertices (not including Disk).
- [`c.intersects(c2)`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html) takes $O(\min(n+m) \log(n+m))$ time if `c2` is a convex polygon with $m$ vertices.
- Other predicates take the same time as [`intersects`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html).
- [`c.intersection(c2)`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html) takes $O((n+m) log (n+m))$ time if `c2` is a convex polygon with $m$ vertices.

