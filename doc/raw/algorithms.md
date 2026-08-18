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

- `xyIntersections(V)` returns all intersecting pairs of segments from the container `V`, exactly as `bruteForceIntersections` does but in the order a sweep meets them. A vertical line sweeps the bounding-box abscissas while an [interval tree](data_structures.md) over the y-extents holds the segments it currently meets, so only the pairs whose bounding boxes overlap are tested. It takes $O((n+k) \log n)$ time where $k$ is the number of such pairs. Unlike `findIntersections` it needs no exact arithmetic and accepts floating-point coordinates.

- `xyCrossings(V)` returns all crossing pairs of segments from the container `V`, exactly as `bruteForceCrossings` does but in the order the same bounding-box sweep meets them, in $O((n+k) \log n)$ time where $k$ is the number of pairs of overlapping bounding boxes. It also accepts floating-point coordinates.

- `detectIntersections(V)` returns true if there are two intersecting segments in the container `V` using the Bentley-Ottmann sweep-line algorithm. It runs in $O(n \log n)$ time.

- `detectCrossings(V)` returns true if there are two crossing segments in the container `V` using the Bentley-Ottmann sweep-line algorithm. It runs in $O(n \log n)$ time.

These functions use the same predicate conventions documented in
[Predicates](shape_methods.md#predicates).

### Convex hull

- `convexHull(V)` returns the convex hull vertices in ccw order starting from the smallest (leftmost) point. Complexity $O(n \log n)$ for $n$ input points.

- `convexHullExtended(C)` returns the convex hull points in ccw order including vertices and points on edge interiors, starting from the smallest (leftmost) point. Complexity $O(n \log n)$ for $n$ input points.

### Smallest enclosing disk

- `smallestEnclosingDisk(V)` returns the unique smallest closed disk containing every point in the non-empty container `V`. It uses a randomized incremental algorithm with expected $O(n)$ time. Constructing a disk supported by two points divides coordinates by two, so all coordinates should be even when an integral type is used; otherwise integer division can truncate the result. The insertion order comes from a fixed seed, so the same input always yields the same disk; an overload accepts a random-bit generator as its second argument when the caller wants another order.

- `Convex` offers the same computation as a method, since a disk containing the hull vertices contains the whole polygon: `c.smallestEnclosingDisk()`{Convex} returns the smallest disk containing the non-empty convex polygon `c`, and an overload takes a random-bit generator in place of the fixed default one. The disk keeps the coordinate number type and drops labels, with the same caveat about even integral coordinates.

### Closest pair of points

- `closestPair(V)` returns a `Segment` joining two points of the container `V` at minimum distance from each other, using the classical divide and conquer algorithm. Complexity $O(n \log n)$ time on inputs whose strips stay short, which is the ordinary case, degrading to $O(n \log^2 n)$ when the points are so clustered along one line that a strip keeps holding a constant fraction of the range. The container must hold at least two points; fewer is undefined behavior. Only squared distances are compared, in the promoted coordinate type, so the result is exact for integer coordinates. The returned segment keeps the input point type, labels included, and ties are broken arbitrarily.

### Sorting points

- `sortAround(points, p)` reorders `points` in place counterclockwise around the center `p`, starting from the lexicographically smallest point and breaking ties by putting farther points first. Points equal to `p` have no direction to sort by and end up last. Connecting the result in order traces a simple star-shaped polygon whose kernel contains `p`. Relies only on exact orientation and squared-distance comparisons. Splitting the points by the horizontal line through `p` leaves each part inside half a turn, where the orientation sign alone orders them, so one orientation predicate per comparison suffices. Complexity $O(n \log n)$ for $n$ points.

- `hilbertSort(points)` reorders `points` in place along a Hilbert space-filling curve, so points close in the plane stay close in the sequence — a useful preprocessing step for incremental algorithms. Uses only coordinate comparisons (exact for integer coordinates). Complexity $O(n \log n)$ for $n$ points.

### Polyominoes

- `polyominoes<T>(size)` returns one `Polygon<Point<T>>` per free polyomino of `size` cells (counted up to translation, rotation, and reflection). Each polygon traces the polyomino boundary with small non-negative integer coordinates and is normalized like any other `Polygon`. Polyominoes that enclose a hole (possible from seven cells onward) are omitted, since their boundary is not a simple polygon; `polyominoRegions` below keeps them. `T` defaults to `int`.

- `polyominoes<T>(n1, n2)` returns the free polyominoes of every size in `[n1, n2]`, smallest first.

- `polyominoesUpTo<T>(n)` returns the free polyominoes of every size from `1` to `n`, smallest first.

- `polyominoRegions<T>(size)` returns one `PolygonWithHoles<Point<T>>` per free polyomino of `size` cells, omitting **none** of them: a region represents an enclosed hole, so the counts are the full free-polyomino sequence (108 at size seven, where `polyominoes` returns 107, and 369 at size eight against 363). Each region has small non-negative integer coordinates, canonical rings, and area equal to the cell count.

  A hole may touch the outer boundary at a single point — two diagonally opposite cells pinch the hole shut against the outside, as in the smallest holed polyomino — which `PolygonWithHoles::isValid` accepts. Such a point is in the region but has no region interior around it.

- `polyominoRegions<T>(n1, n2)` and `polyominoRegionsUpTo<T>(n)` mirror the two `polyominoes` range overloads.

### Visibility

`Polygon`, `PolygonWithHoles` and `Triangulation` each has 3 methods to compute visibility graphs, all returning a `Graph<PointType>` with the same vertex set. Sight is stopped by the boundary of the domain and, on a `Triangulation`, by every constrained edge as well, so `poly.triangulation(walls).visibilityGraph()`{Polygon} is visibility inside `poly` among the segment obstacles `walls`.

- `visibilityGraph()`{Polygon} joins two vertices $a,b$ when the segment $ab$ is contained in the domain, possibly touching the boundary multiple times.

- `clearVisibilityGraph()`{Polygon} is the strict reading: the segment $ab$ must not intersect the boundary of the domain except at $a$ and $b$. Always a subgraph of `visibilityGraph()`. A degenerate polygon has no interior, so its vertices come back with no edges.

- `reducedVisibilityGraph()`{Polygon} is the subgraph of `visibilityGraph()`{Polygon} that a shortest path can bend along: the edges tangent to the obstacles at both ends. An edge $uv$ is tangent at $u$ when the walls incident to $u$ all lie in one closed half-plane of the line $uv$, which is what lets a taut path bend there; a wall running along that line counts for either side, which keeps the walls themselves in the graph. What survives is the boundary edges and the bitangents between reflex corners. Routing between two points, vertices or not, requires adding them joined to everything they see, which is what `visibleVertices(p)`{Polygon} is for.

The same three classes answer the following queries for one query point $p$ in the region (possibly a vertex, but not necessarily).

- `visibleVertices(p)`{Polygon} returns the domain's vertices visible from $p$, under the convention of `visibilityGraph()`{Polygon}. This is what joins a query point to `reducedVisibilityGraph()`{Polygon}:

  ```cpp
  auto graph = room.reducedVisibilityGraph();
  for (const Point &w : room.visibleVertices(source))
      graph.addEdge(source, w);          // and the same for the target
  auto path = graph.shortestPath(source, target,
                                 [](const Point &a, const Point &b) { return a.distance(b); });
  ```

- `clearlyVisibleVertices(p)`{Polygon} is the strict counterpart, under the convention of `clearVisibilityGraph()`{Polygon}: always a subset of `visibleVertices(p)`{Polygon}.

  Both return the vertices counterclockwise around $p$ starting from the lexicographically smallest, as `sortAround(points, p)` orders them.

- `regularizedVisiblePolygon(p)`{Polygon} returns the *region* visible from $p$ — every point reachable by a segment that stays in the domain and crosses no wall. Star-shaped about $p$, hence simply connected, so one `Polygon` holds it however many holes or walls the domain has. *Regularized* means the closure of the interior, which drops the one-dimensional spikes grazing sight would add: a sightline running along a wall, or straight through a vertex into a part beyond that has no area, contributes nothing here, and what comes back always bounds area. A $p$ on the boundary is a vertex of the result.

  Its vertices are the domain's own together with the *window* ends where a sightline past a reflex corner lands on a farther edge. Those are ray-edge intersections and need division, so the result type is requested explicitly as everywhere else in the library: `regularizedVisiblePolygon<double>(p)` for a `Polygon<Point<double>>`, the default being `division_result_t` of the domain's own coordinate type. This is the one place in the visibility code where a coordinate is constructed at all — everything above is orientation predicates on the input points.

All methods use *triangular expansion*: the domain is triangulated once, then each vertex or query point runs a single traversal of the mesh carrying a cone of still unobstructed directions that every crossed diagonal clips, at a cost proportional to the part of the domain that vertex actually sees.


### Boolean Operations and Minkowski Sum

Boolean operations are documented in [`shape_methods.md`](shape_methods.md#boolean-operations). There is also the free function `regularizedUnionOf` to compute the regularized union of multiple shapes at once.
Minkowski sum is documented in [`shape_methods.md`](shape_methods.md#minkowski-sum).
