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

- Other methods: [`bbox`](https://gfonsecabr.github.io/pgl/structpgl_1_1Point.html#aa06f32e7c4b67bca6c5496338013d099), [`begin`](https://gfonsecabr.github.io/pgl/structpgl_1_1Point.html), [`boundaryContains`](https://gfonsecabr.github.io/pgl/structpgl_1_1Point.html), [`cbegin`](https://gfonsecabr.github.io/pgl/structpgl_1_1Point.html#ad21c37bddf34cb290f5c9041727e4b46), [`cend`](https://gfonsecabr.github.io/pgl/structpgl_1_1Point.html#a677eca13b347376c4101bac37ce9d65d), [`contains`](https://gfonsecabr.github.io/pgl/structpgl_1_1Point.html), [`crosses`](https://gfonsecabr.github.io/pgl/structpgl_1_1Point.html), [`distance`](https://gfonsecabr.github.io/pgl/structpgl_1_1Point.html#a0f327863bbabf1c8603aaa16027b39fe), [`distanceL1`](https://gfonsecabr.github.io/pgl/structpgl_1_1Point.html#af8c141c3ca491737267797594a98f6bd), [`distanceLInf`](https://gfonsecabr.github.io/pgl/structpgl_1_1Point.html#acf57d15ddb1243e56313a0837a850f81), [`edges`](https://gfonsecabr.github.io/pgl/structpgl_1_1Point.html#aa74a251f19bb50c307f069a9aaf490b9), [`end`](https://gfonsecabr.github.io/pgl/structpgl_1_1Point.html), [`fbox`](https://gfonsecabr.github.io/pgl/structpgl_1_1Point.html#a8b8f70afdcb0c1998b1c80cf4eed88b9), [`get`](https://gfonsecabr.github.io/pgl/structpgl_1_1Point.html), [`index`](https://gfonsecabr.github.io/pgl/structpgl_1_1Point.html#a9a3e12195880f93bfe9134fb65e03ff0), [`interiorContains`](https://gfonsecabr.github.io/pgl/structpgl_1_1Point.html), [`interiorsIntersect`](https://gfonsecabr.github.io/pgl/structpgl_1_1Point.html), [`intersection`](https://gfonsecabr.github.io/pgl/structpgl_1_1Point.html), [`intersects`](https://gfonsecabr.github.io/pgl/structpgl_1_1Point.html), [`label`](https://gfonsecabr.github.io/pgl/structpgl_1_1Point.html#afb7bffbecf1399d7247a538fb26d4642), [`orientedEdges`](https://gfonsecabr.github.io/pgl/structpgl_1_1Point.html#aa00f6c786e95f9f6e1f48778c1a69893), [`rotate90`](https://gfonsecabr.github.io/pgl/structpgl_1_1Point.html#aa721e29e9d9a76c90f4d8b0c1d9e5362), [`rotated90`](https://gfonsecabr.github.io/pgl/structpgl_1_1Point.html#a09a86c3fc54499ce0b6e648767718670), [`scaleDownX`](https://gfonsecabr.github.io/pgl/structpgl_1_1Point.html#a1896b477b6a8aa9e96dc5ac6f1b5a9ee), [`scaleDownY`](https://gfonsecabr.github.io/pgl/structpgl_1_1Point.html#a470091d2ecf8d237c972fcb2ae62faa2), [`scaleUpX`](https://gfonsecabr.github.io/pgl/structpgl_1_1Point.html#a02141f2cbe1f539e6aa3c888eb925878), [`scaleUpY`](https://gfonsecabr.github.io/pgl/structpgl_1_1Point.html#a87b805965105230ca89e7ede8167ba7f), [`scaledDownX`](https://gfonsecabr.github.io/pgl/structpgl_1_1Point.html), [`scaledDownY`](https://gfonsecabr.github.io/pgl/structpgl_1_1Point.html), [`scaledUpX`](https://gfonsecabr.github.io/pgl/structpgl_1_1Point.html), [`scaledUpY`](https://gfonsecabr.github.io/pgl/structpgl_1_1Point.html), [`separates`](https://gfonsecabr.github.io/pgl/structpgl_1_1Point.html), [`size`](https://gfonsecabr.github.io/pgl/structpgl_1_1Point.html#a6d56fab92043ca5524419b0a5247f358), [`squaredDistance`](https://gfonsecabr.github.io/pgl/structpgl_1_1Point.html), [`vertices`](https://gfonsecabr.github.io/pgl/structpgl_1_1Point.html#a6e443118fca6e5c4c2ee501e553f6c2c).

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

- Other methods: [`area`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html#a900ec557f95500c0308138817858ec83), [`bbox`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html#a447581d0b86d603605ca84856053521a), [`begin`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html#a69fd5afc7418628582f8b61bf521d363), [`boundaryContains`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html), [`cbegin`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html#a8ff6045db2a8dd66fa7c42e89f3e505f), [`cend`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html#a14a976ca0b92bd486bfdd2ceb4f025d2), [`contains`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html), [`containsCollinear`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html#a5f2e199a9d1a4809d9b07f269d74b9c6), [`crosses`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html), [`diameter`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html#ae0ba7b79bd5c8a8f16940052f9709776), [`edges`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html#a70ef5eef6e342da444a35ed9d3a20f1b), [`end`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html#a64321c03de3ec64b038bdf947a7d4c67), [`fbox`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html#a0ebaa92c57ab30cd53dd6fc7f0f6b6b6), [`get`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html#a9603e82a2099e0d085d6f21ade77ee3c), [`index`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html#a07c1240cdbdf75152ec3bd381db6cb45), [`interiorContains`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html), [`interiorsIntersect`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html), [`intersection`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html), [`intersects`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html), [`label`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html#a17639cf154c82cdb08eed7fce639dbd1), [`lengthL1`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html#a5f85458293a2a86ac3727b39a1cb09dc), [`lengthLInf`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html#a403c0f01a4a06c9a66870d26ee3fbb91), [`max`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html#a7ed2bf58648e2eb9b29c349b4762aaca), [`min`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html#a8ca634e3fec2026ef7a18c4a2652667d), [`orientedEdges`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html#aeb3dab6fe571994ba130dc4f005deeb5), [`pointInside`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html#ac381a6cbc94661fbb3f2e602f3999c78), [`rotate90`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html#a3fd8c134074fbf2636aa9743ccbf030c), [`rotated90`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html#a9e5459b86e0eaf6f007d26681a73a240), [`scaleDownX`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html#aeb711153f0e2394cf4c7e7e84d2f5fa5), [`scaleDownY`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html#a5ba2ee3d871dedf12d5a92ecc79ad85b), [`scaleUpX`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html#ac0874880eb0fa131702a2857aa0638f4), [`scaleUpY`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html#a695297c93ef05dcaf56affb98c382ff8), [`scaledDownX`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html), [`scaledDownY`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html), [`scaledUpX`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html), [`scaledUpY`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html), [`separates`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html), [`size`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html#a889dfc74b64baa67e97bc4c7dd413a6a), [`squaredDistance`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html), [`squaredHausdorffDistance`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html#ae7bba7fbc2fefc7de46ce8a32a4f4c02), [`twiceArea`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html#add64cac458c92a18bfdfbb87363408c3), [`vertices`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html#a175b49d33518dc20795f0bdd752d2313), [`verticesContain`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html#a08097be84764754c35aac6cc6f1d5d96).


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

- Other methods: [`area`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html#a728b6cd39d9cf4e4e6aad12b1758f32e), [`asLine`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html#af7700575ec9d0865ab559d3a70b3ab94), [`asSegment`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html#a839015c7197fe69821533cda01d8136a), [`bbox`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html#a27d265989a331167765f44b9d859a6c5), [`begin`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html), [`boundaryContains`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html), [`cbegin`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html#a37f0f59c2d9e82d9c1b3a1795455332f), [`cend`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html#a916becc1b4b29308a43d5225c78a091c), [`contains`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html), [`containsCollinear`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html#ab99a981d4cfbe2087e55a97b52eab145), [`crosses`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html), [`diameter`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html#adb88cba4f4637968187c78fddaabaf16), [`edges`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html#a8b7ee1fb46bf24ee60fde12e86c33e04), [`end`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html), [`fbox`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html#a30cbae58290e89e188e6b751599384d7), [`get`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html), [`index`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html#a0e85f32bac2d7392328e751be0a1e1f3), [`interiorContains`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html), [`interiorsIntersect`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html), [`intersection`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html), [`intersects`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html), [`label`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html#a5e8087666ddcb941c3ffda6f97354933), [`lengthL1`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html#a105f79c0fb325725b753dee91a411857), [`lengthLInf`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html#a39e8b93dccbd0e2dcc60cdd19a188e78), [`max`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html#a5299844de92c345405c81bbe2d499bf9), [`min`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html#a1f7b0a29be7c37d9e6cc2070199b0562), [`orientedEdges`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html#a0b8e8afbf16178e51a8e1d497dc9ea25), [`pointInside`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html#a5a399c0606d7b4641bd10a8c11d85d80), [`rotate90`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html#a09f959e151766e01b526367d1f7b9c58), [`rotated90`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html#a37bf7b25137a0712fbfb0b0137c8b0f9), [`scaleDownX`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html#a6eff6106b2fb621dd275cfdde4ba67b7), [`scaleDownY`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html#ad38fc22e1cb565df68e14e3d163c6823), [`scaleUpX`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html#a2d95af4674752a735eef1d0472ebb3f6), [`scaleUpY`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html#ab1245c6404e254fd88b2d62d9b453616), [`scaledDownX`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html), [`scaledDownY`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html), [`scaledUpX`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html), [`scaledUpY`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html), [`separates`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html), [`size`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html#a477436b9bbddaa54cd8cc7f6263aa52f), [`squaredDistance`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html), [`squaredHausdorffDistance`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html), [`twiceArea`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html#a87076975a979b23a702d5af684488c84), [`vertices`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html#aa515dbca8dc2332613b1e0267e8516f6), [`verticesContain`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html#a247b09048136f7092ba4642d734943d6).

### EmptyShape

Represents the empty set. Its [`size()`](https://gfonsecabr.github.io/pgl/structpgl_1_1EmptyShape.html#a5bd45b3506aa274a0cd9f12ca257ab8f) is 0, it intersects nothing, and it is contained in everything.

- Other methods: [`boundaryContains`](https://gfonsecabr.github.io/pgl/structpgl_1_1EmptyShape.html), [`contains`](https://gfonsecabr.github.io/pgl/structpgl_1_1EmptyShape.html), [`crosses`](https://gfonsecabr.github.io/pgl/structpgl_1_1EmptyShape.html#af46b0a6534ef5aa5734ee3414a6067b7), [`get`](https://gfonsecabr.github.io/pgl/structpgl_1_1EmptyShape.html#a71080110672913245ba1ddbd5f31693a), [`index`](https://gfonsecabr.github.io/pgl/structpgl_1_1EmptyShape.html#a82ab1d0879b5b087a4f2f39e601aa9b9), [`interiorContains`](https://gfonsecabr.github.io/pgl/structpgl_1_1EmptyShape.html), [`interiorsIntersect`](https://gfonsecabr.github.io/pgl/structpgl_1_1EmptyShape.html#adc6cc1877378411a8d76af443d740de8), [`intersection`](https://gfonsecabr.github.io/pgl/structpgl_1_1EmptyShape.html#a03b459e2e2d907a3e8425af0a01daa6a), [`intersects`](https://gfonsecabr.github.io/pgl/structpgl_1_1EmptyShape.html#a545a5779a108a3bbb3ad673b1429dbb1), [`isDegenerate`](https://gfonsecabr.github.io/pgl/structpgl_1_1EmptyShape.html#a8324a647c03113afcc4fc6a7f37656cb), [`separates`](https://gfonsecabr.github.io/pgl/structpgl_1_1EmptyShape.html#a1ff3719fc56858cf597717f6c72e9c7b).

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

- Other methods: [`area`](https://gfonsecabr.github.io/pgl/structpgl_1_1Line.html#a852fafcdfd7c159cea73c207b334f2c3), [`begin`](https://gfonsecabr.github.io/pgl/structpgl_1_1Line.html#a4522c14c252a3093c83fcc5381ae4801), [`boundaryContains`](https://gfonsecabr.github.io/pgl/structpgl_1_1Line.html), [`cbegin`](https://gfonsecabr.github.io/pgl/structpgl_1_1Line.html#ac92427a493388e54d07c8a51671fb069), [`cend`](https://gfonsecabr.github.io/pgl/structpgl_1_1Line.html#a173527bfe987fec13c62e0402b2d2757), [`collinear`](https://gfonsecabr.github.io/pgl/structpgl_1_1Line.html), [`crosses`](https://gfonsecabr.github.io/pgl/structpgl_1_1Line.html), [`dualCoordinates`](https://gfonsecabr.github.io/pgl/structpgl_1_1Line.html#a1b4df1c4b503f41c70511df39debb3bf), [`end`](https://gfonsecabr.github.io/pgl/structpgl_1_1Line.html#aaebdb39d99bba13b42a26a3a91b679a9), [`get`](https://gfonsecabr.github.io/pgl/structpgl_1_1Line.html#aa78d70c2e1b07bee3ba4eba2083c29c7), [`index`](https://gfonsecabr.github.io/pgl/structpgl_1_1Line.html#acb7f7b3f4940841503855d09b8947c23), [`interiorsIntersect`](https://gfonsecabr.github.io/pgl/structpgl_1_1Line.html), [`intersection`](https://gfonsecabr.github.io/pgl/structpgl_1_1Line.html), [`intersects`](https://gfonsecabr.github.io/pgl/structpgl_1_1Line.html), [`label`](https://gfonsecabr.github.io/pgl/structpgl_1_1Line.html#a8f95fdfc54e852f2be721b09ee67b33c), [`max`](https://gfonsecabr.github.io/pgl/structpgl_1_1Line.html#a320576b5b0950794eb5994b2b32faa03), [`min`](https://gfonsecabr.github.io/pgl/structpgl_1_1Line.html#a5143b9b0c107fecab04a397b47203156), [`pointInside`](https://gfonsecabr.github.io/pgl/structpgl_1_1Line.html#a02a7f68895d3822c49025defca738341), [`rotate90`](https://gfonsecabr.github.io/pgl/structpgl_1_1Line.html#a2e89314127bafa4e01da38c9b74e3882), [`rotated90`](https://gfonsecabr.github.io/pgl/structpgl_1_1Line.html#a7aeca6a269be181179e5b450f59898a7), [`scaleDownX`](https://gfonsecabr.github.io/pgl/structpgl_1_1Line.html#acd5d61a9bcc4fb8ab5ca6f73b0f51e31), [`scaleDownY`](https://gfonsecabr.github.io/pgl/structpgl_1_1Line.html#aa70e7239057ff98a7d847ef57ffed83c), [`scaleUpX`](https://gfonsecabr.github.io/pgl/structpgl_1_1Line.html#a709b2751e41450732da6957219ac31ef), [`scaleUpY`](https://gfonsecabr.github.io/pgl/structpgl_1_1Line.html#af67fedd5c1d688bb6e4bae226538f95a), [`scaledDownX`](https://gfonsecabr.github.io/pgl/structpgl_1_1Line.html), [`scaledDownY`](https://gfonsecabr.github.io/pgl/structpgl_1_1Line.html), [`scaledUpX`](https://gfonsecabr.github.io/pgl/structpgl_1_1Line.html), [`scaledUpY`](https://gfonsecabr.github.io/pgl/structpgl_1_1Line.html), [`separates`](https://gfonsecabr.github.io/pgl/structpgl_1_1Line.html), [`size`](https://gfonsecabr.github.io/pgl/structpgl_1_1Line.html#a87a202d457d89f32911b6dc811be6361), [`squaredDistance`](https://gfonsecabr.github.io/pgl/structpgl_1_1Line.html), [`twiceArea`](https://gfonsecabr.github.io/pgl/structpgl_1_1Line.html#ad94a95afb14713ff5d909063f5e91865), [`verticesContain`](https://gfonsecabr.github.io/pgl/structpgl_1_1Line.html#a91b2e25a94740265188b175967c14f8c).


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

- Other methods: [`area`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedLine.html#aa3fe0ee224bd4a89975fce8d13a51bbe), [`begin`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedLine.html), [`boundaryContains`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedLine.html), [`cbegin`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedLine.html#ab22cf551a336c59cd99d224be39d695d), [`cend`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedLine.html#a9ed5d604315cfd9c910665cf75226199), [`collinear`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedLine.html), [`crosses`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedLine.html), [`crossingOrder`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedLine.html#a68183d94c20884bb21f47134b6f42766), [`end`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedLine.html), [`get`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedLine.html), [`index`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedLine.html#ab8976011bb7580e03e4811d42935209d), [`interiorsIntersect`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedLine.html), [`intersection`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedLine.html), [`intersects`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedLine.html), [`label`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedLine.html#a1eaf9ae1db110af53ec64ad04270ad30), [`max`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedLine.html#a981d03ccc8abb5b6daebcd90b61c4a45), [`min`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedLine.html#adeebd042e968ec518a76dbe1cda3f8c9), [`pointInside`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedLine.html#a3c73c7f60b7910bc45c49daec553312b), [`rotate90`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedLine.html#a6ac5046221104fe666769956443bfcae), [`rotated90`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedLine.html#abc039c9ba303040c5dd4376d542e1f84), [`scaleDownX`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedLine.html#ac1489e9dbea1c9dca5fa565ae1c81b6e), [`scaleDownY`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedLine.html#ac291d59ec09201e28b47772d206b82f7), [`scaleUpX`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedLine.html#ad553b726faede6cafae0421e59bd1d17), [`scaleUpY`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedLine.html#a31d6662a4ab3059d643c198cfa568329), [`scaledDownX`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedLine.html), [`scaledDownY`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedLine.html), [`scaledUpX`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedLine.html), [`scaledUpY`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedLine.html), [`separates`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedLine.html), [`size`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedLine.html#a7a41d4c4b6949741f4186f5916d9cf03), [`source`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedLine.html), [`squaredDistance`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedLine.html), [`target`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedLine.html), [`twiceArea`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedLine.html#a40041a3c82dd187ec31e8e4ef211e94c), [`verticesContain`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedLine.html#a7a6c854b76f6e0eadf672dee09b30091).


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

- Other methods: [`area`](https://gfonsecabr.github.io/pgl/structpgl_1_1Ray.html#accc8080e83502a2cdb1a60da7ad79fa6), [`begin`](https://gfonsecabr.github.io/pgl/structpgl_1_1Ray.html), [`boundaryContains`](https://gfonsecabr.github.io/pgl/structpgl_1_1Ray.html), [`cbegin`](https://gfonsecabr.github.io/pgl/structpgl_1_1Ray.html#a8d6a17d7cb4bc074b891181c11cd62e1), [`cend`](https://gfonsecabr.github.io/pgl/structpgl_1_1Ray.html#a05eb2aeb0e782a643f3749d80d43f0fc), [`collinear`](https://gfonsecabr.github.io/pgl/structpgl_1_1Ray.html), [`contains`](https://gfonsecabr.github.io/pgl/structpgl_1_1Ray.html), [`containsCollinear`](https://gfonsecabr.github.io/pgl/structpgl_1_1Ray.html#a3e26488ffd5e65ea1fbcf620ea155b64), [`crosses`](https://gfonsecabr.github.io/pgl/structpgl_1_1Ray.html), [`end`](https://gfonsecabr.github.io/pgl/structpgl_1_1Ray.html), [`get`](https://gfonsecabr.github.io/pgl/structpgl_1_1Ray.html), [`index`](https://gfonsecabr.github.io/pgl/structpgl_1_1Ray.html#a3e9100b021385db17bd0f7e3c010830a), [`interiorContains`](https://gfonsecabr.github.io/pgl/structpgl_1_1Ray.html), [`interiorsIntersect`](https://gfonsecabr.github.io/pgl/structpgl_1_1Ray.html), [`intersection`](https://gfonsecabr.github.io/pgl/structpgl_1_1Ray.html), [`intersects`](https://gfonsecabr.github.io/pgl/structpgl_1_1Ray.html), [`label`](https://gfonsecabr.github.io/pgl/structpgl_1_1Ray.html#a5cb84ab9187e88bbaad09dfd09877157), [`max`](https://gfonsecabr.github.io/pgl/structpgl_1_1Ray.html#a7d70e6c27e28e6f2fcc50cd4efcecb5b), [`min`](https://gfonsecabr.github.io/pgl/structpgl_1_1Ray.html#ab0cd668afb5c0cf698edcf702d700e2e), [`pointInside`](https://gfonsecabr.github.io/pgl/structpgl_1_1Ray.html#adf32c22c527e752a545bbe33573f331c), [`rotate90`](https://gfonsecabr.github.io/pgl/structpgl_1_1Ray.html#aafd6c26b8489e744a54fa896c1b40c8f), [`rotated90`](https://gfonsecabr.github.io/pgl/structpgl_1_1Ray.html#af3781a8fa9b18651c3c8639b43eef8a0), [`scaleDownX`](https://gfonsecabr.github.io/pgl/structpgl_1_1Ray.html#a307a947f066be1e00e6f8d151a434875), [`scaleDownY`](https://gfonsecabr.github.io/pgl/structpgl_1_1Ray.html#a72458854b249293dfb69aaad48889307), [`scaleUpX`](https://gfonsecabr.github.io/pgl/structpgl_1_1Ray.html#ab2ff66dafd58be13a56478bdea168223), [`scaleUpY`](https://gfonsecabr.github.io/pgl/structpgl_1_1Ray.html#a7fb33feca02bdfa3ed7a75df1c7770bf), [`scaledDownX`](https://gfonsecabr.github.io/pgl/structpgl_1_1Ray.html), [`scaledDownY`](https://gfonsecabr.github.io/pgl/structpgl_1_1Ray.html), [`scaledUpX`](https://gfonsecabr.github.io/pgl/structpgl_1_1Ray.html), [`scaledUpY`](https://gfonsecabr.github.io/pgl/structpgl_1_1Ray.html), [`separates`](https://gfonsecabr.github.io/pgl/structpgl_1_1Ray.html), [`size`](https://gfonsecabr.github.io/pgl/structpgl_1_1Ray.html#ac04b3f1567932d63175fa45e26627f86), [`source`](https://gfonsecabr.github.io/pgl/structpgl_1_1Ray.html), [`squaredDistance`](https://gfonsecabr.github.io/pgl/structpgl_1_1Ray.html), [`target`](https://gfonsecabr.github.io/pgl/structpgl_1_1Ray.html), [`twiceArea`](https://gfonsecabr.github.io/pgl/structpgl_1_1Ray.html#acd61d952b25ad571c3e88e48d3002564), [`verticesContain`](https://gfonsecabr.github.io/pgl/structpgl_1_1Ray.html#a89cc49900b6085bdb013a80a2c009023).


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

- Other methods: [`begin`](https://gfonsecabr.github.io/pgl/structpgl_1_1Halfplane.html), [`boundaryContains`](https://gfonsecabr.github.io/pgl/structpgl_1_1Halfplane.html), [`cbegin`](https://gfonsecabr.github.io/pgl/structpgl_1_1Halfplane.html#ae62f220f21bb593f931a15dbdd399960), [`cend`](https://gfonsecabr.github.io/pgl/structpgl_1_1Halfplane.html#ad364df753efb3fb088fc9bdc3f7c955f), [`contains`](https://gfonsecabr.github.io/pgl/structpgl_1_1Halfplane.html), [`crosses`](https://gfonsecabr.github.io/pgl/structpgl_1_1Halfplane.html), [`end`](https://gfonsecabr.github.io/pgl/structpgl_1_1Halfplane.html), [`get`](https://gfonsecabr.github.io/pgl/structpgl_1_1Halfplane.html), [`index`](https://gfonsecabr.github.io/pgl/structpgl_1_1Halfplane.html#a27001adb22631bfa0a96debd380e5ee0), [`interiorContains`](https://gfonsecabr.github.io/pgl/structpgl_1_1Halfplane.html), [`interiorsIntersect`](https://gfonsecabr.github.io/pgl/structpgl_1_1Halfplane.html), [`intersects`](https://gfonsecabr.github.io/pgl/structpgl_1_1Halfplane.html), [`label`](https://gfonsecabr.github.io/pgl/structpgl_1_1Halfplane.html#aa950a0eecc36647518259a38cb6ac72f), [`max`](https://gfonsecabr.github.io/pgl/structpgl_1_1Halfplane.html#a4362ec5ee1c67c386760f7c32a6c3746), [`min`](https://gfonsecabr.github.io/pgl/structpgl_1_1Halfplane.html#a547f9125924efa0946219f00fcee2a77), [`pointInside`](https://gfonsecabr.github.io/pgl/structpgl_1_1Halfplane.html#a27a12a9125ffbaa3462d19ae663382c0), [`rotate90`](https://gfonsecabr.github.io/pgl/structpgl_1_1Halfplane.html#a3ae1505df6e38ee4c5d506300a30471e), [`rotated90`](https://gfonsecabr.github.io/pgl/structpgl_1_1Halfplane.html#a02da3e0f6aa4cbe728626e123cbdcb84), [`scaleDownX`](https://gfonsecabr.github.io/pgl/structpgl_1_1Halfplane.html#a0bb72ce6f6e03ce87589c92e4ac823c8), [`scaleDownY`](https://gfonsecabr.github.io/pgl/structpgl_1_1Halfplane.html#a28fcfd6f876360f6c57799f342bb2e87), [`scaleUpX`](https://gfonsecabr.github.io/pgl/structpgl_1_1Halfplane.html#a3dc7ad02fca572893d2e52dcd4b78daf), [`scaleUpY`](https://gfonsecabr.github.io/pgl/structpgl_1_1Halfplane.html#ac7dc14a15669253eaf72edcf61d22e16), [`scaledDownX`](https://gfonsecabr.github.io/pgl/structpgl_1_1Halfplane.html), [`scaledDownY`](https://gfonsecabr.github.io/pgl/structpgl_1_1Halfplane.html), [`scaledUpX`](https://gfonsecabr.github.io/pgl/structpgl_1_1Halfplane.html), [`scaledUpY`](https://gfonsecabr.github.io/pgl/structpgl_1_1Halfplane.html), [`separates`](https://gfonsecabr.github.io/pgl/structpgl_1_1Halfplane.html), [`size`](https://gfonsecabr.github.io/pgl/structpgl_1_1Halfplane.html#a3a2b16706b1386ddf56f4d5d80a25ec4), [`source`](https://gfonsecabr.github.io/pgl/structpgl_1_1Halfplane.html), [`squaredDistance`](https://gfonsecabr.github.io/pgl/structpgl_1_1Halfplane.html), [`target`](https://gfonsecabr.github.io/pgl/structpgl_1_1Halfplane.html), [`verticesContain`](https://gfonsecabr.github.io/pgl/structpgl_1_1Halfplane.html#a7d8d6875b24c6dfacd894926105c97fa).


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

- Other methods: [`a`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html#a49e6b3ba0db01d986087b842b54d9131), [`area`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html#a7d9d7443fe0e474c2a40cc81eeda5aa3), [`b`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html#a2940bbceda1e86124340be582ae78283), [`bbox`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html#a9b3e0556b074ae291d5db07c827463c9), [`begin`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html#a3bf2365cdca283316f0deb0f3d0377fa), [`boundaryContains`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html), [`c`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html#ae3d2e34092959e6f78dbd305910f34ac), [`cbegin`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html#a642ff0f840b22d623caf3c1f1562a102), [`cend`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html#a396ed38714259a4e781de67c93d1941f), [`contains`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html), [`crosses`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html), [`diameter`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html#a523e4d5e777d1977ab0a71295a59c9b2), [`edges`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html#add3cd93bb69d662b90418d601d36f3f5), [`edgesBegin`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html#afb9f125b51f09c63bce44d68d502ca65), [`edgesEnd`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html#ab4149c9830cfb6c26ec3827f14ab169b), [`end`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html#a84d2e947c1fc166c9b291d8797de9cd3), [`fbox`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html#aaf7fbb752ca9b2a784bd298606ff965d), [`get`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html#ac1c69cde482a51d9fa1cb3701338b81d), [`index`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html#aca6b47e468bac538594b1f12432b5a03), [`interiorContains`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html), [`interiorsIntersect`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html), [`intersection`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html), [`intersects`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html), [`label`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html#a0d5baf5be961a9baf436a39ae21d8a67), [`orientedEdges`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html#af1f1ea8bef504fcef567533e1dcaab64), [`orientedEdgesBegin`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html#a73dbde059416276890c2c69d4fcac97f), [`orientedEdgesEnd`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html#a7e2ad6b78ae985a9437eb143732e7011), [`pointInside`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html#acd5f859d0052fb38ebcaed8238e5d18c), [`rotate90`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html#aa22fcc6c25a3cda78273bf9f2695514a), [`rotated90`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html#adf035881d22ca6cbe87c2af5a28e0dbd), [`scaleDownX`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html#ad3997028eb56849c01147531cc2ff697), [`scaleDownY`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html#ab441ed7b80d9af48fb18f9314bdd77c4), [`scaleUpX`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html#a656348c9d879829ce9496d95db9d90ec), [`scaleUpY`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html#a82e5a70787404160a029f8020e6f0bc5), [`scaledDownX`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html), [`scaledDownY`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html), [`scaledUpX`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html), [`scaledUpY`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html), [`separates`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html), [`size`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html#a15b6971531e1e67cdeb401480c2ea086), [`squaredDistance`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html), [`twiceArea`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html#a8ba4cd745c67787ef7209dcb18ff1d09), [`vertices`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html#a395af097b8902119b6c810f516211534), [`verticesContain`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html#a083e324686e1905ee26cd84ee373c7ef).


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

- Other methods: [`area`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html#af8e8f46b3461676c7e40a14b6c4ff86c), [`begin`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html#aae952c36436a16afb5bff4f8929cceb8), [`boundaryAt`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html#a3565ced82f92e91b79f1aa9be6df761d), [`boundaryContains`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html), [`cbegin`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html#a118731b6ddcbd04097b0cf76136958d2), [`cend`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html#ad52301729cc8376c6c9dbbcebcb769d4), [`center`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html#a7d2e656f0e136e0978ac785096178df7), [`contains`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html), [`crosses`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html), [`diameter`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html#af310096576e07b538fc1b3eb2ee2f74e), [`edges`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html#a7017ee8e082661250fdcdf9948033f71), [`edgesBegin`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html#ac9c6e134a9752582356769c2574b056c), [`edgesEnd`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html#a6ec19f5060b5641082f0cac862a00d17), [`end`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html#aeae879542bbef06613ccc988adaeec11), [`fbox`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html#a2222e82708cb4d30b46519dbc4db4601), [`get`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html#a04908c335b96990b5bd8b6f68ac06299), [`height`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html#a41e31233e3edebba8832ffc84c5e7927), [`index`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html#aa27f92baa8430b0fa73726f64f68839c), [`interiorContains`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html), [`interiorsIntersect`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html), [`intersection`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html), [`intersects`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html), [`label`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html#a96037aa87b6a66f747e4bdfd368f8734), [`max`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html#ac2c5d9b26f67491dbd62fc700954d08b), [`midpoint`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html#a36738ba8e4e131ab97cdb2e9fdbccfaf), [`min`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html#a58c52150645f5830ec93a87add23abcd), [`orientedEdges`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html#a6660cc18ff22cad17d57ce680be9b344), [`orientedEdgesBegin`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html#ad34f137ca4d8e9ce95e0f313bc26c432), [`orientedEdgesEnd`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html#a52408d94dd1b7509f9134abe8230b017), [`pointInside`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html#a3d3a71320826ad5060436195accf8916), [`rotate90`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html#a6c33271dc02da27c043d44afa040775d), [`rotated90`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html#a0f7c4ec9134c624403e63118b940a03d), [`scaleDownX`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html#a9fff37aed09e8261bbb193c686eada99), [`scaleDownY`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html#a869e60dc8eb42713cbbba387e98c0292), [`scaleUpX`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html#ac001fbbafce6b74b7d53337b14c1fa77), [`scaleUpY`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html#a1cf2a0d95cce531d55bc5494e514bf34), [`scaledDownX`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html), [`scaledDownY`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html), [`scaledUpX`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html), [`scaledUpY`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html), [`separates`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html), [`size`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html#a99388dcbdc773cc434910a7e37600346), [`squaredDistance`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html), [`squaredHausdorffDistance`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html#a15ef653c720d714779abbfbb1990ffc3), [`twiceArea`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html#a926a8acf69c73483a45f9cd60fc8ec6c), [`vertices`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html#a8e890106e82852c58aaa51ceff095275), [`verticesContain`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html#a0d982fcbd82c865bddc0ab1277aff7eb), [`width`](https://gfonsecabr.github.io/pgl/structpgl_1_1Rectangle.html#aed51c8fbbb4b4886ef1b907f6119b5b9).


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

- Other methods: [`a`](https://gfonsecabr.github.io/pgl/structpgl_1_1Disk.html#aab80a3c4810748f39fdc8c7e020eb232), [`area`](https://gfonsecabr.github.io/pgl/structpgl_1_1Disk.html#aa4f2837f14655fd73098513f769e41da), [`b`](https://gfonsecabr.github.io/pgl/structpgl_1_1Disk.html#aeb247fff089ba4d4463418f84f0f9e3d), [`bbox`](https://gfonsecabr.github.io/pgl/structpgl_1_1Disk.html#a82543add1cf92f0b0116fd798ddecced), [`begin`](https://gfonsecabr.github.io/pgl/structpgl_1_1Disk.html#aaec2b4934b1b89530f0ea126a080f4ef), [`boundaryContains`](https://gfonsecabr.github.io/pgl/structpgl_1_1Disk.html), [`c`](https://gfonsecabr.github.io/pgl/structpgl_1_1Disk.html#a20d60679286b84a64063b648c860430e), [`cbegin`](https://gfonsecabr.github.io/pgl/structpgl_1_1Disk.html#ad39c1b05f308f292112590c388575a36), [`cend`](https://gfonsecabr.github.io/pgl/structpgl_1_1Disk.html#a5c10cddfceeff5a571f4b892ba2e11ad), [`contains`](https://gfonsecabr.github.io/pgl/structpgl_1_1Disk.html), [`crosses`](https://gfonsecabr.github.io/pgl/structpgl_1_1Disk.html), [`end`](https://gfonsecabr.github.io/pgl/structpgl_1_1Disk.html#a4245360f58a74d7969b4176627797e8f), [`fbox`](https://gfonsecabr.github.io/pgl/structpgl_1_1Disk.html#aff00091cb8c1a2afd46f55e31fb4d38f), [`get`](https://gfonsecabr.github.io/pgl/structpgl_1_1Disk.html#afd129f4cb8111846762e2b4ca06db7c1), [`index`](https://gfonsecabr.github.io/pgl/structpgl_1_1Disk.html#ab6e63f207efba3178e1a665c70f60cd0), [`interiorContains`](https://gfonsecabr.github.io/pgl/structpgl_1_1Disk.html), [`interiorsIntersect`](https://gfonsecabr.github.io/pgl/structpgl_1_1Disk.html), [`intersects`](https://gfonsecabr.github.io/pgl/structpgl_1_1Disk.html), [`label`](https://gfonsecabr.github.io/pgl/structpgl_1_1Disk.html#af5f96f809e41fead49b9cafa8de11938), [`pointInside`](https://gfonsecabr.github.io/pgl/structpgl_1_1Disk.html#a78477a3bcf69c6f36cff63e5a65442e3), [`rotate90`](https://gfonsecabr.github.io/pgl/structpgl_1_1Disk.html#a8d38ec21e40e6459475c0006646d352b), [`rotated90`](https://gfonsecabr.github.io/pgl/structpgl_1_1Disk.html#a07288ad0eb805ea8a8e6a67d5b8e497c), [`separates`](https://gfonsecabr.github.io/pgl/structpgl_1_1Disk.html), [`size`](https://gfonsecabr.github.io/pgl/structpgl_1_1Disk.html#a9276993379a1097fb9a2c3464faf66ed).


### Polygon

The class template [`Polygon`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html) represents a simple polygon. It can be constructed for any number of points in a container that must be given in the order they appear on the polygon. The vertices are accessed in counterclockwise order starting from the minimum vertex (minimum x, breaking ties by minimum y). ~~Internally, the polygon is stored as multiple x-monotone polylines for improved performance.~~

A polygon `P` has methods such as:

- [`P.isDegenerate()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html#af1d297b10aee661458543c2f5fa00645): Returns true if the polygon has null area.
- [`P.isSimple()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html#a5e182cf106f6be5ed3fb49c07daf80c6): Returns true if the edges only intersect at the endpoints of consecutive edges. Takes $O(n \log n)$ time for $n$ edges.

- Other methods: [`area`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html#ad4765e3f6390f06a59f25fef4fcb219f), [`bbox`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html#a1ff93373098baf44e68ba00bb02565d6), [`begin`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html#a9411cd18c37f8f81740ce5ad1cf834aa), [`boundaryContains`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html), [`cbegin`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html#a15bd64aed079105234fc3148afb5e1be), [`cend`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html#af6668ee498e370013d29d775d0b1973b), [`centroid`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html#a66951f26f6ab36fa08fc6bd0188d3234), [`contains`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html), [`crosses`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html), [`diameter`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html#a34513cb8bcf668d052f7d15ad58cfe85), [`edges`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html#ad8456b5d296bc23059b0ffe7053dd74d), [`edgesBegin`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html#aa874eea1865d33c7d88d2820cf959cd3), [`edgesEnd`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html#a8e2ead5cd51c5412f672f17cf1cbeb68), [`end`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html#af74dcdb08aba7a122770a1de6f9aa1d9), [`fbox`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html#a6088100ec92a2d72b8732e2e12afa1b8), [`get`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html#ae81b4e6f9129d731d6732f8b20bd45db), [`index`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html#a4242053f262a712d1e38cc5ced8e301b), [`interiorContains`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html), [`interiorsIntersect`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html), [`intersection`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html), [`intersects`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html), [`label`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html#ad5fee5791a7d7f6e48a6bd1dea3f446b), [`orientedEdges`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html#ab6c6a1c32666c9f220ba03c2c4864f98), [`orientedEdgesBegin`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html#aa6a00964980269056093f52df5ca2302), [`orientedEdgesEnd`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html#ab78f1bd5cba565f6024ce5d08e41db8c), [`rotate90`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html#abae5b3272afe3c706ff1378fb6d00683), [`rotated90`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html#ae737bfc7fb10dc9fbe6f9101dad4edc6), [`scaleDownX`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html#afcc813c8d7e00a28290e5e535de205d7), [`scaleDownY`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html#a8161a2a6e3c75c8eb5f3b6ca0659e9f4), [`scaleUpX`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html#a7485d53c8396b8402102695e9f122255), [`scaleUpY`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html#abdd5508e61d4c797f02257116e2d3125), [`scaledDownX`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html), [`scaledDownY`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html), [`scaledUpX`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html), [`scaledUpY`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html), [`separates`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html), [`size`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html#a9a70b70146923bd677bebf7ee9ef38b8), [`twiceArea`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html#ae08d1b6ee40568069ed6ace905abca0f), [`vertices`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html#ac441c37b1bb195da9f14784e2f6207bf), [`verticesCentroid`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html#af868c2942503c2bafaf89f64af1fe463).


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

- Other methods: [`antipodalPairs`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html#ad8daeec7e468c1b3a12be81c96752f95), [`area`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html#a75a11db621ea33f8ea69f4946de7034e), [`bbox`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html#a06e8f55ebcb8cf814bbc5df5d4288312), [`begin`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html#ac3d6626a1da2b4dcb4723679ba2d36b4), [`boundaryContains`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html), [`cbegin`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html#af6878c6b999ac61e7e2328c71fc1a86a), [`cend`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html#a196e2ec7cfe538a2a4e39f8ed93b9a96), [`contains`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html), [`crosses`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html), [`edges`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html#a9dcf1c9ee107a02ec9893e2baa59620c), [`edgesAtX`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html#ac8afcc0efcc66b471d0c3a5f994d204b), [`edgesBegin`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html#a319a670fa0703ffd3f263268a7fddfd6), [`edgesEnd`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html#a47390af7cb9472bcde6c3c0e10290f37), [`end`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html#ab4c6a1493d20d239550d07fb87cb260f), [`fbox`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html#a129c434124d3e5fa172ae7b12993456b), [`get`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html#a20a4c5a95a53f91958478a8f40908e0e), [`index`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html#a66ca8f0c5c1fb858dc88134abe664207), [`interiorContains`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html), [`interiorsIntersect`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html), [`label`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html#a0ecd9d4676b0fb01b290edd140e4d2bb), [`maxIndex`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html#a7245d13086db65109cd90a0e11c640c4), [`orientedEdges`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html#ad265bd5b6734525d68ef1c63c3747b49), [`orientedEdgesBegin`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html#a4d01da6add5037580a76bd74465dff9c), [`orientedEdgesEnd`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html#ab4f25ed5d6230d89b1823aa97c9e63a3), [`pointInside`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html#a15719a7622fa3025b9eef141dbfceb61), [`rotate90`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html#acac576ae17ab38cf43697f793ee8ceca), [`rotated90`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html#a755f47a54e052b8f5723bb01df111cf8), [`scaleDownX`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html#a0b79d7dbe80b783215de8ed35a2ce816), [`scaleDownY`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html#a34c0e742024ce3c9c565a6f642b646a2), [`scaleUpX`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html#a4a1d8e61e86ab8fdfb1ce791e17bdeaa), [`scaleUpY`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html#a0419fa660cbd44740acfd38b3c6b7eb5), [`scaledDownX`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html), [`scaledDownY`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html), [`scaledUpX`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html), [`scaledUpY`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html), [`separates`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html), [`size`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html#af571fa35d1459bf957d1e57a7a6dab54), [`twiceArea`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html#a0cb5bbaafdd13d462ebda639e6bbc8d3), [`vertices`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html#ad5be32b0077d06c77f97954d1213792a), [`verticesCentroid`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html#a18faa3ce367b7e75a14ad67cb994148d), [`verticesContain`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html#a9017a5bd874b74b80bd70f97f4444f40).

