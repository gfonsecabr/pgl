<img align="left" src="doc/figures/logo.png" width="23%"/>

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="doc/figures/logotextdark.svg"/>
  <img alt="Pangolin: Plane Geometry Library" src="doc/figures/logotext.svg" width="65%"/>
</picture>

[![Tests](https://github.com/gfonsecabr/pgl/actions/workflows/tests.yml/badge.svg)](https://github.com/gfonsecabr/pgl/actions/workflows/tests.yml)
[![Standard](https://img.shields.io/badge/C%2B%2B-20/23/26-rgb(10,66,158).svg)](https://en.wikipedia.org/wiki/C%2B%2B#Standardization)
[![License](https://img.shields.io/badge/license-MIT-rgb(216,134,42).svg)](https://opensource.org/licenses/MIT)
[![Benchmarks](https://img.shields.io/badge/benchmarks-online-rgb(21,153,135).svg)](https://gfonsecabr.github.io/pgl/benchmarks/index.html)

<br/>

> ⚠️ **Work in Progress**: This library is still under construction and contains **bugs and missing features**. Use in production environments is not recommended.

Pangolin (or `pgl`) is a header-only C++ library for computational geometry in the plane. It is designed to be pleasant to use, exact when needed, and easy to combine with standard C++ containers and algorithms.

```c++
#include <iostream>
#include "pgl.hpp"

int main() {
    pgl::Point p = {1, 0}, q = {4, 7};
    pgl::Segment s = {p, q}, t = {0, 8, 2, 1};
    if (s.intersects(t))
        std::cout << s << " intersects " << t << std::endl;

    return 0;
} // Output: (1,0)--(4,7) intersects (0,8)--(2,1)
```

## Shapes and Predicates

| Family | Shapes |
| --- | --- |
| 0-dimensional | [`Point`](doc/shapes.md#point), [`EmptyShape`](doc/shapes.md#emptyshape) |
| 1-dimensional | [`Segment`](doc/shapes.md#segment), [`OrientedSegment`](doc/shapes.md#oriented-segment), [`Line`](doc/shapes.md#line), [`OrientedLine`](doc/shapes.md#oriented-line), [`Ray`](doc/shapes.md#ray), ~~[`Polyline`](doc/shapes.md#polyline), [`PolyFunction`](doc/shapes.md#monotone-polyline)~~ |
| 2-dimensional | [`Halfplane`](doc/shapes.md#half-plane), [`Triangle`](doc/shapes.md#triangle), [`Rectangle`](doc/shapes.md#rectangle), [`Disk`](doc/shapes.md#disk), [`Convex`](doc/shapes.md#convex), [`Polygon`](doc/shapes.md#polygon) |
| Polymorphism | [`Shape`](doc/shapes.md#shape) |

The following [predicates](doc/shape_methods.md#predicates) are implemented as methods of all shapes.

- `contains(Shape)` Does it contain the other shape?
- `boundaryContains(Shape)` Does its boundary contain the other shape?
- `interiorContains(Shape)` Does it contain the other shape in the interior?
- `intersects(Shape)` Do the two shapes intersect?
- `interiorsIntersect(Shape)` Do the interiors of the two shapes intersect?
- `separates(Shape)` Does one shape cut the other into two (or more) components?
- `crosses(Shape)` Do both shapes separate each other?

```c++
pgl::Point o;      // Point (0,0)
pgl::Disk d(o,10); // Disk of radius 10 centered at (0,0)
if (d.contains(o))
    std::cout << "Disk contains " << o << std::endl;
pgl::Segment diam = d.diameter();
if (d.contains(diam))
    std::cout << "Disk contains the diameter" << std::endl;
if (!d.interiorContains(diam))
    std::cout << "Disk's interior does not contain the diameter" << std::endl;
```

## Exact Constructions

Predicates are implemented with exact integer arithmetic. When a construction requires non-integer coordinates, it is often convenient to request a rational or floating point result only where needed.

```c++
pgl::Segment s = {1, 0, 4, 7};
pgl::Point<pgl::Rational<int>> midpoint = s.midpoint<pgl::Rational<int>>();
std::cout << "The midpoint of " << s << " is " << midpoint << std::endl;
// Output: The midpoint of (1,0)--(4,7) is (5/2,7/2)
```

Notice that sometimes it is possible to obtain integral results with scaling:

```c++
pgl::Segment s = {1, 0, 4, 7};
pgl::Point midpoint2 = (2*s).midpoint();
std::cout << "The midpoint of " << 2*s << " is " << midpoint2 << std::endl;
// Output: The midpoint of (2,0)--(8,14) is (5,7)
```

See [types.md](doc/types.md) for more information.

## Other Methods

Several [other methods](doc/shape_methods.md) are supported by the shapes.

```c++
pgl::Convex c{0, 0, 1, 0, 1, 2, 0, 1};
pgl::Segment s = c.diameter();
std::cout << "The diameter of " << c;
std::cout << " is defined by " << s;
std::cout << " and has length " << s.length() << std::endl;
// Output: The diameter of Convex[(0,0),(1,0),(1,2),(0,1)] is defined by (0,0)--(1,2) and has length 2.23607
```

## Visualization

A `Canvas` class is provided for [SVG visualization](doc/canvas.md):

<img align="right" src="doc/figures/example2.svg" width="200"/>

```c++
    pgl::Canvas canvas;
    canvas << pgl::Point(0,0);

    pgl::Triangle tri = {-1, -1, 0, 2, 1, -2};
    canvas << pgl::stroke("green") << tri;
    canvas << pgl::stroke("blue") << 2*tri;
    canvas.writeSVG("example2.svg");
```

## Comparison and Hashing

All geometry types are comparable and hashable, so they can be stored in standard containers:

```c++
pgl::Segment s = {1, 0, 4, 7};
std::set<decltype(s)> set;
set.insert(s);
std::unordered_set<decltype(s)> uset;
uset.insert(s);
```

## Algorithms and Data Structures

<img align="right" src="doc/figures/example_triangulation.svg" width="200"/>

PGL includes [fundamental algorithms](doc/algorithms.md) and [data structures](doc/data_structures.md) such as:

- Convex hull: computed with Graham scan.
- Line segment intersection: Bentley-Ottmann sweep line using rational numbers.
- Sort points: by angle or Hilbert order.
- Kd-tree: for points and a generalization for other bounded shapes.
- Triangulation: including Delaunay and constrained Delaunay triangulations for points and polygons.

## Comparison to CGAL

There are several architectural differences between PGL and [CGAL](https://www.cgal.org/), we summarize some of them:

| Feature | PGL | CGAL |
| --- | --- | --- |
| Dependency-free | ✓ | ✗ |
| Learning curve | Low | High |
| Geometry | Plane only | 2d, 3d, hyperbolic... |
| Data structures | Limited | Extensive |
| Maturity | Very low | High |
| License | MIT | LGPL, GPL, and commercial |
| Number types | Per-shape | Per-kernel |
| Type conversion | Implicit | Explicit |
| Shapes | Mostly non-oriented | Oriented |

- Pangolin defines the shapes as their *geometric concepts*, instead of their *computational representation*. For example, a `Triangle` is the same regardless of the order of its 3 vertices (in contrast to CGAL's oriented triangles).
- Pangolin stores lines and halfplanes as *2 points* (instead of an *equation*), so rational numbers are not needed to exactly represent a line passing through any two integer points. Notice that the comparison operators (and hash function) take care of testing if two lines are equal even if they are defined by different points.
- Pangolin does not distinguish between points, vectors, and directions.
- Pangolin predicates return `true` or `false`, instead of some CGAL predicates that return 3 possible values for inside, outside, and on the boundary. Boundaries and interiors are distinguished by different predicates such as `contains`, `boundaryContains`, and `interiorContains`.
- Even simple queries often require composing several CGAL primitives. For example, checking whether a segment lies inside a polygon has no direct predicate, and `CGAL::intersection` has no overload for a segment against a polygon: you must combine endpoint side-tests with per-edge intersection checks, or build a 2D arrangement. In Pangolin these are `polygon.contains(segment)` and `polygon.intersection(segment)`.


## Build

As a header-only library with no dependency, you can clone the repository and then compile code directly with `g++` or `clang++`:

```bash
g++ -std=c++23 -Iinclude/ -o example examples/example1.cpp
clang++ -std=c++23 -Iinclude/ -o example examples/example1.cpp
```

If you want cmake to automatically download the library, you can include this snippet in your `CMakeLists.txt`:

```cmake
include(FetchContent)

FetchContent_Declare(
  pgl
  GIT_REPOSITORY https://github.com/gfonsecabr/pgl
  GIT_TAG main
)

FetchContent_MakeAvailable(pgl)

target_include_directories(your_target PRIVATE ${pgl_SOURCE_DIR}/include)
```

## More Information

- For a brief description, check the documents at the [doc folder](doc/).
- For some simple examples, check the files at the [examples folder](examples/).
- For the **doxygen reference** and **benchmarks**, check the [library website on github.io](https://gfonsecabr.github.io/pgl/index.html).
 
