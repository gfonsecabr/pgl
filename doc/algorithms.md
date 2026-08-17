<!-- AUTO-GENERATED from doc/raw/algorithms.md by doc/raw/doxylink.py — do not edit; edit the raw version and regenerate. -->

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

- [`findIntersections(V)`](https://gfonsecabr.github.io/pgl/namespacepgl.html#adcd493466342b027a48fe7bf0718434b "Finds all intersecting segment pairs with Bentley-Ottmann.") returns all intersecting pairs of segments from the container `V` using the Bentley-Ottmann sweep-line algorithm. It runs in $O((n+k) \log n)$ time where $n$ is the input size and $k$ is the output size.

- [`findCrossings(V)`](https://gfonsecabr.github.io/pgl/namespacepgl.html#abf691f267558aaeea543045e723d292e "Finds all proper crossing segment pairs with Bentley-Ottmann.") returns all crossing pairs of segments from the container `V` using the Bentley-Ottmann sweep-line algorithm. It runs in $O((n+k) \log n)$ time where $n$ is the input size and $k$ is the output size.

- [`bruteForceIntersections(V)`](https://gfonsecabr.github.io/pgl/namespacepgl.html#a390afa6b90488531f4702bc242322d46 "Finds all intersecting segment pairs by brute force.") returns all intersecting pairs of segments from the container `V` using the naive brute force solution that verifies each pair. It takes $O(n^2)$ time but is faster when there are many intersections.

- [`bruteForceCrossings(V)`](https://gfonsecabr.github.io/pgl/namespacepgl.html#aac3ae3a0e91834aef9525388948417c4 "Finds all crossing segment pairs by brute force.") returns all crossing pairs of segments from the container `V` using the naive brute force solution that verifies each pair. It takes $O(n^2)$ time but is faster when there are many crossings.

- [`detectIntersections(V)`](https://gfonsecabr.github.io/pgl/namespacepgl.html#adea0ebb84e7d7ada3ae27ae23ea116bc "Detects whether any two segments intersect.") returns true if there are two intersecting segments in the container `V` using the Bentley-Ottmann sweep-line algorithm. It runs in $O(n \log n)$ time.

- [`detectCrossings(V)`](https://gfonsecabr.github.io/pgl/namespacepgl.html#ade9af54c89044d728daf207e9c534759 "Detects whether any two segments properly cross.") returns true if there are two crossing segments in the container `V` using the Bentley-Ottmann sweep-line algorithm. It runs in $O(n \log n)$ time.

These functions use the same predicate conventions documented in
[Predicates](shape_methods.md#predicates).

### Convex hull

- [`convexHull(V)`](https://gfonsecabr.github.io/pgl/namespacepgl.html#a3999bfdf73609b7ec708a4882fcaea2f "Computes the convex hull of a point container.") returns the convex hull vertices in ccw order starting from the smallest (leftmost) point. Complexity $O(n \log n)$ for $n$ input points.

- [`convexHullExtended(C)`](https://gfonsecabr.github.io/pgl/namespacepgl.html#ace788332cf5ee8db888decfb08383cda "Computes the convex hull of a point container.") returns the convex hull points in ccw order including vertices and points on edge interiors, starting from the smallest (leftmost) point. Complexity $O(n \log n)$ for $n$ input points.

### Smallest enclosing disk

- [`smallestEnclosingDisk(V)`](https://gfonsecabr.github.io/pgl/namespacepgl.html#ac8734297c4d99750062ee028e743d0d0 "Computes the smallest closed disk containing a set of points.") returns the unique smallest closed disk containing every point in the non-empty container `V`. It uses a randomized incremental algorithm with expected $O(n)$ time. Constructing a disk supported by two points divides coordinates by two, so all coordinates should be even when an integral type is used; otherwise integer division can truncate the result. An overload accepts a random-bit generator as its second argument when the caller needs control over the randomized order.

### Closest pair of points

- [`closestPair(V)`](https://gfonsecabr.github.io/pgl/namespacepgl.html#a9d03d057595b9229d8cc245045ce4df4 "Computes a closest pair of points by divide and conquer.") returns a [`Segment`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html "Unoriented closed segment between two endpoints plus optional segment label.") joining two points of the container `V` at minimum distance from each other, using the classical divide and conquer algorithm. Complexity $O(n \log n)$ time on inputs whose strips stay short, which is the ordinary case, degrading to $O(n \log^2 n)$ when the points are so clustered along one line that a strip keeps holding a constant fraction of the range. The container must hold at least two points; fewer is undefined behavior. Only squared distances are compared, in the promoted coordinate type, so the result is exact for integer coordinates. The returned segment keeps the input point type, labels included, and ties are broken arbitrarily.

### Sorting points

- [`sortAround(points, p)`](https://gfonsecabr.github.io/pgl/namespacepgl.html#aab7826153f78fb8c4468ad851564fd8f "Sorts points counterclockwise around a center point.") reorders `points` in place counterclockwise around the center `p`, starting from the lexicographically smallest point and breaking ties by putting farther points first. Connecting the result in order traces a simple star-shaped polygon whose kernel contains `p`. Relies only on exact orientation and squared-distance comparisons. Complexity $O(n \log n)$ for $n$ points.

- [`hilbertSort(points)`](https://gfonsecabr.github.io/pgl/namespacepgl.html#a57def78cd131e9c518e478cafe93e137 "Sorts points along a Hilbert space-filling curve.") reorders `points` in place along a Hilbert space-filling curve, so points close in the plane stay close in the sequence — a useful preprocessing step for incremental algorithms. Uses only coordinate comparisons (exact for integer coordinates). Complexity $O(n \log n)$ for $n$ points.

### Polyominoes

- `polyominoes<T>(size)` returns one `Polygon<Point<T>>` per free polyomino of `size` cells (counted up to translation, rotation, and reflection). Each polygon traces the polyomino boundary with small non-negative integer coordinates and is normalized like any other [`Polygon`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html "Closed simple polygon stored by its vertices."). Polyominoes that enclose a hole (possible from seven cells onward) are omitted, since their boundary is not a simple polygon; [`polyominoRegions`](https://gfonsecabr.github.io/pgl/namespacepgl.html#ac6b73b7ed31a9544846662b7726f1fb3 "Enumerates the free polyominoes of a given size as regions.") below keeps them. `T` defaults to `int`.

- `polyominoes<T>(n1, n2)` returns the free polyominoes of every size in `[n1, n2]`, smallest first.

- `polyominoesUpTo<T>(n)` returns the free polyominoes of every size from `1` to `n`, smallest first.

- `polyominoRegions<T>(size)` returns one `PolygonWithHoles<Point<T>>` per free polyomino of `size` cells, omitting **none** of them: a region represents an enclosed hole, so the counts are the full free-polyomino sequence (108 at size seven, where [`polyominoes`](https://gfonsecabr.github.io/pgl/namespacepgl.html#a9008f6bc68cdaae01e41b0e572127a43 "Enumerates the free polyominoes of a given size as polygons.") returns 107, and 369 at size eight against 363). Each region has small non-negative integer coordinates, canonical rings, and area equal to the cell count.

  A hole may touch the outer boundary at a single point — two diagonally opposite cells pinch the hole shut against the outside, as in the smallest holed polyomino — which `PolygonWithHoles::isValid` accepts. Such a point is in the region but has no region interior around it.

- `polyominoRegions<T>(n1, n2)` and `polyominoRegionsUpTo<T>(n)` mirror the two [`polyominoes`](https://gfonsecabr.github.io/pgl/namespacepgl.html#a9008f6bc68cdaae01e41b0e572127a43 "Enumerates the free polyominoes of a given size as polygons.") range overloads.

### Visibility graphs

[`Polygon`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html "Closed simple polygon stored by its vertices."), [`PolygonWithHoles`](https://gfonsecabr.github.io/pgl/structpgl_1_1PolygonWithHoles.html "Closed region bounded by one outer simple polygon minus disjoint polygonal holes.") and [`Triangulation`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangulation.html "Triangulation whose connectivity may change and whose vertex set may grow.") each has 3 methods to compute visibility graphs, all returning a `Graph<PointType>` with the same vertex set. Sight is stopped by the boundary of the domain and, on a [`Triangulation`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangulation.html "Triangulation whose connectivity may change and whose vertex set may grow."), by every constrained edge as well, so [`poly.triangulation(walls).visibilityGraph()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html#a55c6ba6ed18502faa4cebbe7e15c60ae "Builds the constrained Delaunay triangulation of this polygon.") is visibility inside `poly` among the segment obstacles `walls`.

- [`visibilityGraph()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html#a2486c5f87a836c247d28fc5260e673be "Returns the visibility graph of the polygon vertices.") joins two vertices $a,b$ when the segment $ab$ is contained in the domain, possibly touching the boundary multiple times.

- [`clearVisibilityGraph()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html#a31ffed2f5e3df9aa9a682e410201b69c "Returns the clear visibility graph of the polygon vertices.") is the strict reading: the segment $ab$ must not intersect the boundary of the domain except at $a$ and $b$. Always a subgraph of `visibilityGraph()`. A degenerate polygon has no interior, so its vertices come back with no edges.

- [`reducedVisibilityGraph()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html#add9e8b8ce20f931b5e84eb54f74e1e34 "Returns the reduced visibility graph of the polygon vertices.") is the subgraph of [`visibilityGraph()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html#a2486c5f87a836c247d28fc5260e673be "Returns the visibility graph of the polygon vertices.") that a shortest path can use: the edges tangent to the obstacles at both ends. An edge $uv$ is tangent at $u$ when the walls incident to $u$ all lie in one closed half-plane of the line $uv$, which is what lets a taut path bend there; a wall running along that line counts for either side, which keeps the walls themselves in the graph. What survives is the boundary edges and the bitangents between reflex corners — much sparser than the full graph, while still containing a geodesic shortest path between any two of its vertices. A vertex with no incident wall, such as a free point inside the domain, bends no path and comes back isolated. A shortest path to or from a point that is not a vertex needs that point's own visibility edges added back.

All three are computed by *triangular expansion*: the domain is triangulated once, then each vertex runs a single traversal of the mesh carrying a cone of still unobstructed directions that every crossed diagonal clips, at a cost proportional to the part of the domain that vertex actually sees.

### Boolean Operations and Minkowski Sum

Boolean operations are documented in [`shape_methods.md`](shape_methods.md#boolean-operations). There is also the free function [`regularizedUnionOf`](https://gfonsecabr.github.io/pgl/namespacepgl.html#ae72efa38504e74942758d2d4fb78ffcd "The regularized union of arbitrarily many shapes, as a set of regions.") to compute the regularized union of multiple shapes at once.
Minkowski sum is documented in [`shape_methods.md`](shape_methods.md#minkowski-sum).
