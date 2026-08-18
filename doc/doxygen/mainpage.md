# Pangolin {#mainpage}

<img align="left" src="https://raw.githubusercontent.com/gfonsecabr/pgl/main/doc/figures/logo.png" width="23%"/>

<img class="pgl-wordmark-light" alt="Pangolin: Plane Geometry Library" src="https://raw.githubusercontent.com/gfonsecabr/pgl/main/doc/figures/logotext.svg" width="65%"/>
<img class="pgl-wordmark-dark" alt="Pangolin: Plane Geometry Library" src="https://raw.githubusercontent.com/gfonsecabr/pgl/main/doc/figures/logotextdark.svg" width="65%"/>

[![GitHub](https://img.shields.io/badge/GitHub-gfonsecabr/pgl-rgb(40,40,40).svg?logo=github)](https://github.com/gfonsecabr/pgl)
[![Tests](https://github.com/gfonsecabr/pgl/actions/workflows/tests.yml/badge.svg)](https://github.com/gfonsecabr/pgl/actions/workflows/tests.yml)
[![Standard](https://img.shields.io/badge/C%2B%2B-20/23/26-rgb(10,66,158).svg)](https://en.wikipedia.org/wiki/C%2B%2B#Standardization)
[![License](https://img.shields.io/badge/license-MIT-rgb(216,134,42).svg)](https://opensource.org/licenses/MIT)
[![Benchmarks](https://img.shields.io/badge/benchmarks-online-rgb(21,153,135).svg)](benchmarks/index.html)

<br/>

> ⚠️ **Work in Progress**: This library is still under construction and contains **bugs and missing features**. Use in production environments is not recommended.

Pangolin (or `pgl`) is a header-only C++ library for computational geometry in the plane. It is designed to be pleasant to use, exact when needed, and easy to combine with standard C++ containers and algorithms. A [python binding](https://github.com/gfonsecabr/pypgl/tree/main) called `pypgl` is also available.

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

There are [many more illustrated examples](https://github.com/gfonsecabr/pgl/tree/main/examples) that give a good overview of the library's features and syntax.

## Shapes and Predicates

| Family | Shapes |
| --- | --- |
| 0-dimensional | [`Point`](https://github.com/gfonsecabr/pgl/blob/main/doc/shapes.md#point), [`EmptyShape`](https://github.com/gfonsecabr/pgl/blob/main/doc/shapes.md#emptyshape) |
| 1-dimensional | [`Segment`](https://github.com/gfonsecabr/pgl/blob/main/doc/shapes.md#segment), [`OrientedSegment`](https://github.com/gfonsecabr/pgl/blob/main/doc/shapes.md#oriented-segment), [`Line`](https://github.com/gfonsecabr/pgl/blob/main/doc/shapes.md#line), [`OrientedLine`](https://github.com/gfonsecabr/pgl/blob/main/doc/shapes.md#oriented-line), [`Ray`](https://github.com/gfonsecabr/pgl/blob/main/doc/shapes.md#ray), [`MonotoneChain`](https://github.com/gfonsecabr/pgl/blob/main/doc/shapes.md#monotone-chain), [`Polyline`](https://github.com/gfonsecabr/pgl/blob/main/doc/shapes.md#polyline) |
| 2-dimensional | [`Halfplane`](https://github.com/gfonsecabr/pgl/blob/main/doc/shapes.md#half-plane), [`Triangle`](https://github.com/gfonsecabr/pgl/blob/main/doc/shapes.md#triangle), [`Rectangle`](https://github.com/gfonsecabr/pgl/blob/main/doc/shapes.md#rectangle), [`Disk`](https://github.com/gfonsecabr/pgl/blob/main/doc/shapes.md#disk), [`Convex`](https://github.com/gfonsecabr/pgl/blob/main/doc/shapes.md#convex), [`Polygon`](https://github.com/gfonsecabr/pgl/blob/main/doc/shapes.md#polygon), [`PolygonWithHoles`](https://github.com/gfonsecabr/pgl/blob/main/doc/shapes.md#polygon-with-holes), [`PolygonSet`](https://github.com/gfonsecabr/pgl/blob/main/doc/shapes.md#polygon-set), [`HalfplaneIntersection`](https://github.com/gfonsecabr/pgl/blob/main/doc/shapes.md#halfplane-intersection) |
| Polymorphism | [`Shape`](https://github.com/gfonsecabr/pgl/blob/main/doc/shapes.md#shape) |

The following [predicates](https://github.com/gfonsecabr/pgl/blob/main/doc/shape_methods.md#predicates) are implemented as methods of all shapes.

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

Predicates among integer coordinates are implemented with exact integer arithmetic. When a construction requires non-integer coordinates, it will return exact rational types of arbitrary precision by default.

```c++
pgl::Segment s = {1, 0, 4, 7};
pgl::EPoint midpoint = s.midpoint();
std::cout << "The midpoint of " << s << " is " << midpoint << std::endl;
// Output: The midpoint of (1,0)--(4,7) is (5/2,7/2)
```

It is possible to choose rational types with fewer digits manually:

```c++
pgl::Point<pgl::Rational<int>> midpointi = s.midpoint<pgl::Rational<int>>();
```

Notice that sometimes it is possible to obtain integral results with scaling:

```c++
pgl::Segment s = {1, 0, 4, 7};
pgl::Point midpoint2 = (2*s).midpoint<int>();
std::cout << "The midpoint of " << 2*s << " is " << midpoint2 << std::endl;
// Output: The midpoint of (2,0)--(8,14) is (5,7)
```

If performance is not critical, you may use arbitrary precision rational numbers everywhere with `ERational`, `EPoint`, `ESegment`, etc. If performance is important, the library allows you to fine-tune number types accordingly. See [types.md](https://github.com/gfonsecabr/pgl/blob/main/doc/types.md) for more information.

## Other Methods

Several [other methods](https://github.com/gfonsecabr/pgl/blob/main/doc/shape_methods.md) are supported by the shapes.

```c++
pgl::Convex c{0, 0, 1, 0, 1, 2, 0, 1};
pgl::Segment s = c.diameter();
std::cout << "The diameter of " << c;
std::cout << " is defined by " << s;
std::cout << " and has length " << s.length() << std::endl;
// Output: The diameter of Convex[(0,0),(1,0),(1,2),(0,1)] is defined by (0,0)--(1,2) and has length 2.23607
```

## Visualization

A `Canvas` class is provided for [visualization](https://github.com/gfonsecabr/pgl/blob/main/doc/canvas.md). It includes support to export to `svg`, `pdf`, and [ipe](https://github.com/otfried/ipe) files.

<img align="right" src="https://raw.githubusercontent.com/gfonsecabr/pgl/main/examples/figures/example2.svg" width="200"/>

```c++
pgl::Canvas canvas;
canvas << pgl::Point(0,0);

pgl::Triangle tri = {-1, -1, 0, 2, 1, -2};
canvas << pgl::stroke("green") << tri;
canvas << pgl::stroke("blue") << 2*tri;
canvas.writeSVG("example2.svg");
canvas.writePDF("example2.pdf");
canvas.writeIPE("example2.ipe");
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

<img align="right" src="https://raw.githubusercontent.com/gfonsecabr/pgl/main/doc/figures/algds.svg" width="180"/>

Pangolin includes [fundamental algorithms](https://github.com/gfonsecabr/pgl/blob/main/doc/algorithms.md):

- **Convex hull** computed with Graham scan.
- Line segment intersection: **Bentley-Ottmann sweep line** using rational numbers.
- **Minkowski sum** and **boolean operations**.
- **Visibility** graph and visibility polygon.
- Find the **closest pair** of points using divide and conquer.
- Smallest **enclosing disk and rectangle**.
- Sort points by angle or Hilbert order.

 and [data structures](https://github.com/gfonsecabr/pgl/blob/main/doc/data_structures.md):

- **Kd-tree** for points and a generalization for other bounded shapes.
- **Interval tree** to use 1-dimensional queries on projections.
- **Triangulation** including **Delaunay** and **constrained Delaunay** triangulations for points and polygons.
- **Arrangement** of lines, line segments, and rays with a **trapezoidal map** for fast point location.
- Graph class for combinatorial algorithms like **Djikstra** and **Prim** that can be used to compute Euclidean minimum spanning trees and shortest paths among obstacles.

## Comparison to CGAL

There are several architectural differences between Pangolin and [CGAL](https://www.cgal.org/), we summarize some of them:

| Feature | Pangolin | CGAL |
| --- | --- | --- |
| Dependency-free | ✓ | ✗ |
| Learning | Easy | Hard |
| Architecture | Monolithic | Modular |
| Geometry | Plane only | 2d, 3d, hyperbolic... |
| Maturity | Very low | High |
| Number types | Per-shape | Per-kernel |
| Type conversion | Implicit | Explicit |
| Shapes | Mostly non-oriented | Oriented |
| License | MIT | LGPL, GPL, and commercial |

- Pangolin defines the shapes as their *geometric concepts*, instead of their *computational representation*. For example, a `Triangle` is the same regardless of the order of its 3 vertices (in contrast to CGAL's oriented triangles).
- Pangolin stores lines and halfplanes as *2 points* (instead of an *equation*), so rational numbers are not needed to exactly represent a line passing through any two integer points. Notice that the comparison operators (and hash function) take care of testing if two lines are equal even if they are defined by different points. Similarly, disks are represented by 3 boundary points.
- Pangolin implicitly converts shapes that use different number types, so it is easy to use rational numbers or larger numbers only when needed.
- Pangolin does not distinguish between points, vectors, and directions.
- Pangolin predicates return `true` or `false`, instead of some CGAL predicates that return 3 possible values for inside, outside, and on the boundary. Boundaries and interiors are distinguished by different predicates such as `contains`, `boundaryContains`, and `interiorContains`.
- Even simple queries often require composing several CGAL primitives. For example, checking whether a segment lies inside a polygon has no direct predicate, and `CGAL::intersection` has no overload for a segment against a polygon: you must combine endpoint side-tests with per-edge intersection checks, or build a 2D arrangement. In Pangolin these are `polygon.contains(segment)` and `polygon.intersection(segment)`.
- It is hard to compare the performance against CGAL, as many algorithms are not available in one or the other. Overall CGAL has faster more complex implementations. For example, pgl's decomposition-based Minkowski sum is much slower than CGAL's convolution-based Minkowski sum and a little slower than CGAL's decomposition-based Minkowski sum. Surprisingly, pgl's trapezoidal map point location is significantly faster than CGAL's in our benchmarks.

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

## Acknowledgments

Pangolin is developed by [Guilherme D. da Fonseca](https://pageperso.lis-lab.fr/guilherme.fonseca/), with many contributions from the undergraduate student Djebril El Feddi.

The library itself is dependency-free, but a few third-party components are bundled to
support testing, benchmarking, and PDF export. We are grateful to their authors:

- [doctest](https://github.com/doctest/doctest) by Viktor Kirilov — the unit-testing framework (MIT).
- [PDFGen](https://github.com/AndreRenaud/PDFGen) by Andre Renaud — a trimmed port powers the `Canvas` PDF export (public domain / The Unlicense).
- [plf_nanotimer](https://github.com/mattreecebentley/plf_nanotimer) by Matt Bentley — timing in the benchmark suite (zlib-style license).
- Many AI have been used to write the code, including Claude, ChatGPT, and GitHub Copilot.

## More Information

- For a brief description, check the documents at the [doc folder](https://github.com/gfonsecabr/pgl/tree/main/doc).
- For some simple examples, check the files at the [examples folder](https://github.com/gfonsecabr/pgl/tree/main/examples).
- For the **benchmarks**, check the [benchmark dashboard](benchmarks/).
- A [python binding](https://github.com/gfonsecabr/pypgl/tree/main) called `pypgl` is also available, but it only supports `ERational` constructions.
