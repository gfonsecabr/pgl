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

## Algorithms


### Intersection of Line Segments

Given a container of $n$ line segments, we provide several functions to compute their intersections and crossings.

- `findIntersections(V)` returns all intersecting pairs of segments from the container `V` using the Bentley-Ottmann sweep-line algorithm. It runs in $O((n+k) \log n)$ time where $n$ is the input size and $k$ is the output size.

- `findCrossings(V)` returns all crossing pairs of segments from the container `V` using the Bentley-Ottmann sweep-line algorithm. It runs in $O((n+k) \log n)$ time where $n$ is the input size and $k$ is the output size.

- `bruteForceIntersections(V)` returns all intersecting pairs of segments from the container `V` using the naive brute force solution that verifies each pair. It takes $O(n^2)$ time but is faster when there are many intersections.

- `bruteForceCrossings(V)` returns all crossing pairs of segments from the container `V` using the naive brute force solution that verifies each pair. It takes $O(n^2)$ time but is faster when there are many crossings.

- `detectIntersections(V)` returns true if there are two intersecting segments in the container `V` using the Bentley-Ottmann sweep-line algorithm. It runs in $O(n \log n)$ time.

- `detectCrossings(V)` returns true if there are two crossing segments in the container `V` using the Bentley-Ottmann sweep-line algorithm. It runs in $O(n \log n)$ time.

These functions use the same predicate conventions documented in
[Predicates](shape_methods.md#predicates).

### Convex hull

- `convexHull(V)` returns the convex hull vertices in ccw order starting from the smallest (leftmost) point. Complexity $O(n \log n)$ for $n$ input points.

- `convexHullExtended(C)` returns the convex hull points in ccw order including vertices and points on edge interiors, starting from the smallest (leftmost) point. Complexity $O(n \log n)$ for $n$ input points.

### Smallest enclosing disk

- `smallestEnclosingDisk(V)` returns the unique smallest closed disk containing every point in the non-empty container `V`. It uses a randomized incremental algorithm with expected $O(n)$ time. Constructing a disk supported by two points divides coordinates by two, so all coordinates should be even when an integral type is used; otherwise integer division can truncate the result. An overload accepts a random-bit generator as its second argument when the caller needs control over the randomized order.

### Closest pair of points

- `closestPair(V)` returns a `Segment` joining two points of the container `V` at minimum distance from each other, using the classical divide and conquer algorithm: the points are sorted by abscissa and split by a vertical line, each side is solved recursively, and only the strip around the line can still hold a closer pair, which sorting it by ordinate and scanning it settles. The running closest pair is threaded through the whole traversal rather than combined on the way back up, so a pair found anywhere narrows every strip measured afterwards, and since the points keep their abscissa order each strip is a contiguous run found by walking out from the split — a node costs what its strip costs, not what its range costs. Complexity $O(n \log n)$ time on inputs whose strips stay short, which is the ordinary case, degrading to $O(n \log^2 n)$ when the points are so clustered along one line that a strip keeps holding a constant fraction of the range; $O(n)$ additional space for $n$ points. The container must hold at least two points; fewer is undefined behavior. Only squared distances are compared, in the promoted coordinate type — the strip half-width is applied squared, never rooted — so the result is exact for integer coordinates. The returned segment keeps the input point type, labels included, and ties are broken arbitrarily.

### Sorting points

- `sortAround(points, p)` reorders `points` in place counterclockwise around the center `p`, starting from the lexicographically smallest point and breaking ties by putting farther points first. Connecting the result in order traces a simple star-shaped polygon whose kernel contains `p`. Relies only on exact orientation and squared-distance comparisons. Complexity $O(n \log n)$ for $n$ points.

- `hilbertSort(points)` reorders `points` in place along a Hilbert space-filling curve, so points close in the plane stay close in the sequence — a useful preprocessing step for incremental algorithms. Uses only coordinate comparisons (exact for integer coordinates). Complexity $O(n \log n)$ for $n$ points.

### Polyominoes

- `polyominoes<T>(size)` returns one `Polygon<Point<T>>` per free polyomino of `size` cells (counted up to translation, rotation, and reflection). Each polygon traces the polyomino boundary with small non-negative integer coordinates and is normalized like any other `Polygon`. Polyominoes that enclose a hole (possible from seven cells onward) are omitted, since their boundary is not a simple polygon; `polyominoRegions` below keeps them. `T` defaults to `int`.

- `polyominoes<T>(n1, n2)` returns the free polyominoes of every size in `[n1, n2]`, smallest first.

- `polyominoesUpTo<T>(n)` returns the free polyominoes of every size from `1` to `n`, smallest first.

- `polyominoRegions<T>(size)` returns one `PolygonWithHoles<Point<T>>` per free polyomino of `size` cells, omitting **none** of them: a region represents an enclosed hole, so the counts are the full free-polyomino sequence (108 at size seven, where `polyominoes` returns 107, and 369 at size eight against 363). Each region has small non-negative integer coordinates, canonical rings, and area equal to the cell count.

  A hole may touch the outer boundary at a single point — two diagonally opposite cells pinch the hole shut against the outside, as in the smallest holed polyomino — which `PolygonWithHoles::isValid` accepts. Such a point is in the region but has no region interior around it.

- `polyominoRegions<T>(n1, n2)` and `polyominoRegionsUpTo<T>(n)` mirror the two `polyominoes` range overloads.

### Boolean Operations and Minkowski Sum

Boolean operations are documented in [`shape_methods.md`](shape_methods.md#boolean-operations). There is also the free function `regularizedUnionOf` to compute the regularized union of multiple shapes at once.
Minkowski sum is documented in [`shape_methods.md`](shape_methods.md#minkowski-sum).
