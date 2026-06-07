<img align="left" src="figures/logo.png" width="23%"/>

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="figures/logotextdark.svg"/>
  <img alt="Pangoling: Plane Geometry Library" src="figures/logotext.svg" width="65%"/>
</picture>

[![Tests](https://github.com/gfonsecabr/pgl/actions/workflows/tests.yml/badge.svg)](https://github.com/gfonsecabr/pgl/actions/workflows/tests.yml)
[![Standard](https://img.shields.io/badge/C%2B%2B-20/23/26-rgb(10,66,158).svg)](https://en.wikipedia.org/wiki/C%2B%2B#Standardization)
[![License](https://img.shields.io/badge/license-MIT-rgb(216,134,42).svg)](https://opensource.org/licenses/MIT)

<br/>

> ⚠️ **Work in Progress**: This library is still under construction and contains **bugs and missing features**. Use in production environments is not recommended.

## Shapes Not Yet Implemented

- [`Polyline`](#polyline) Polyline, also called a polygonal chain, possibly having self-intersections.
- [`PolyFunction`](#monotone-polyline) An x-monotone polyline.


### Polyline

The class template `Polyline` represents a polyline,also called a polygonal chain, polygonal curve, polygonal path, or piecewise linear curve. It can be constructed for any number of points in a container that must be given in the order they appear on the polyline. The vertices are accessed in order starting from the minimum extreme vertex (minimum x, breaking ties by minimum y). Internally, the polyline is stored as multiple x-monotone polylines for improved performance.

A polyline `P` has methods such as:

- `P.isDegenerate()`: Returns true if all vertices are equal.
- `P.isSimple()`: Returns true if the edges only intersect at the endpoints of consecutive edges. Takes $O(n \log n)$ time for $n$ edges.


### Monotone Polyline

The class `PolyFunction` represents an x-monotone polyline, more precisely a polyline such that if vertex $u$ is before vertex $v$, then $u$ compares inferior to $v$. A polyline may be constructed from any container of points, which will be sorted automatically. If the points are already sorted, then a second parameter true can be given to avoid sorting the points again.

We use the term above to refer to larger y coordinates and below to refer to smaller y coordinates. A polyline `P` has methods such as:

- `P.isDegenerate()`: Returns true if all vertices are equal.
- `P.insert(P2)`: Extends the polyfunction in order to contain another point `P2` as a vertex.
- `P.insert(points)`: Extends the polyfunction in order to contain other given vertices`.
- `s.yAtX(x)`: Returns an `std::optional` with the value of the y coordinate at the given coordinate `x`.  Takes $O(\log n)$ time for $n$ vertices.
- `s.indexAtX(x)`: Returns an `std::optional<size_t>` that is true if the polyline contains a point of x-coordinate equal to x. The returned value is the smallest index `i` such that `P[i+1].x() > p.x()` or `P[i].x() = p.x()`. Takes $O(\log n)$ time for $n$ vertices. 
- `P.iSBelow(p)`: Returns an `std::optional<size_t>` that is true if a ray shot down from `p` intersects `P`. The returned value is the smallest index `i` such that `P[i+1].x() > p.x()` or `P[i].x() = p.x()`. Takes $O(\log n)$ time for $n$ vertices.
- `P.isAbove(p)`: Returns an `std::optional<size_t>` that is true if a ray shot up from `p` intersects `P`. The returned value is the smallest index `i` such that `P[i+1].x() > p.x()` or `P[i].x() = p.x()`. Takes $O(\log n)$ time for $n$ vertices.

If the polyfunction `P` has $n$ vertices, then:

- `P.contains(s)` takes $O(\log n)$ time if `s` is a point or a segment.
- `P.intersects(P2)` takes $O(n+m)$ time if `P2` is a polyfunction with $m$ vertices.
- `P.intersection(P2)` takes $O(n+m)$ time if `P2` is another polyfunction with $m$ vertices and returns an `std::vector`of points.

## Data Structures Not Yet Implemented

### Triangulation

### Grid

### Fair-Split Tree

### KD-Tree

