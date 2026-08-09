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

## Data Structures


### Shape Tree

`ShapeTree<Shape>` is a container for bounded shapes. The tree is built once and answers range queries against an arbitrary query shape `q`. If the tree stores $n$ points, then it is a kd-tree, with $O(\sqrt{n})$ query time for orthogonal range counting and $O(\log n)$ height. For large intersecting shapes, the tree will be similar to storing the shapes in a vector and examining all of them, but with a much larger construction time.

- `ShapeTree<Shape>(V)` builds the tree over the shapes in container `V`. An optional second argument sets the leaf size (default 8): the maximum number of shapes kept at a leaf.

The query methods come in two families. The *intersecting* family matches stored shapes `s` with `s.intersects(q)`; the *contained* family matches stored shapes `s` with `q.contains(s)`. Each family offers the same five operations:

- `countIntersecting(q)` / `countContainedIn(q)` return the number of matching stored shapes.

- `reportIntersecting(q)` / `reportContainedIn(q)` return a vector with a copy of each matching stored shape.

- `visitIntersecting(q, f)` / `visitContainedIn(q, f)` call `f(s)` on each matching stored shape `s` as it is found. If `f` returns `true` the visit stops.

- `emptyIntersecting(q)` / `emptyContainedIn(q)` return true if no stored shape matches.

- `sumIntersecting(q)` / `sumContainedIn(q)` return the sum of a weight over the matching stored shapes. The weight is given by an optional `WeightFn` template parameter mapping a shape to any type with `operator+` (`ShapeTree<Shape, WeightFn>`); the weight function is passed to the constructor and ignored by default.

- Other methods:

Sending a tree to a [Canvas](canvas.md) with `canvas << tree` draws all node bounding boxes. Is is possible to insert a new element with `insert`, but no rebalancing is performed.

<p align="center">
  <img src="figures/example_shapetree_triangles.svg" alt="Shape tree range query over random triangles" width="50%"/>
  <br/>
  <em>A shape tree over 100 random triangles: the query triangle with the triangles it contains and intersects, plus the node bounding boxes.</em>
</p>


### Triangulation

`Triangulation` stores a mutable triangulation of either a polygon or a point set: vertex coordinates never move once added, but new vertices can be inserted (`insert`, `insertDelaunay`) and the connectivity changes through flips.

It may be constructed from a Polygon (constrained Delaunay triangulation), a container of points (Delaunay triangulation), a container of points plus a container of non-crossing segments (conforming constrained Delaunay: the segments become constrained edges and nothing is carved away), segments forming a complete triangulation, or triangles, always keeping labels. The polygon constructor optionally takes a container of extra interior points (added as vertices) and/or a container of interior segments (added as vertices and constrained edges); either may be omitted, and both are assumed to lie inside the polygon (not checked). Attention, the segments or triangles must define a valid triangulation (of the convex hull or any polygon), otherwise the behavior is undefined.

- `locate(p)` returns a triangle containing point `p`, or none if `p` is outside, via a randomized visibility walk.

- Navigation: `otherTriangle`, `edgeAdjacentTriangles`, `vertexAdjacentTriangles`, `incidentTriangles` (of an edge or of a vertex), the `visitTriangles`/`visitEdges` visitors, and the sorted `triangles()`/`edges()`.

- Range searching: `trianglesIntersecting(s)` return the triangles that satisfy `triangle.intersects(s)`. The function has several variantions `visitTrianglesIntersecting(s,f)` calls the function `f` on these triangles and stops early if `f` returns `true`. If `s` is an oriented segment, oriented line, or ray, the triangles are visited in order. A polyline or a monotone chain is also traced in order, edge by edge, each triangle reported the first time the chain meets it (so a chain may leave the triangulated region and come back). The edge variations `edgesIntersecting` and `visitEdgesIntersecting` list the edges instead of the triangles. The `…InteriorIntersecting` variantions filter with `interiorIntersects(s)`.

- Predicates against the domain — the region the triangulation covers, which is the polygon for the polygon constructors and the convex hull otherwise. `contains(s)`, `interiorContains(s)`, `intersects(s)` and `interiorsIntersect(s)` give exactly the answers the shape predicates of the same name give for that region as a `Polygon`, boundary and all: a segment running along a polygon edge is contained and met, but neither interior-contained nor interior-intersecting. They work on the mesh, so the cost is proportional to the triangles `s` meets rather than to the size of the boundary. Every shape type is accepted: an unbounded one (line, oriented line, ray, half-plane) is never contained in the bounded domain, and the empty shape is contained by it but meets nothing. They answer a different question from `has(t)` and `has(s)`, which ask whether a triangle or a segment is a *cell* of the mesh rather than how the domain covers it geometrically.

- `flip(e)` replaces the diagonal shared by two triangles. It returns the new edge obtained or none if the flip cannot be performed (non-convex quadrilateral or the edge is constrained). `flippable(e)` simply returns if the flip can be performed without changing the triangulation. If we pass a container with edges in interior-disjoint quadrilaterals, the functions use parallel flips.

- `insert(p)` adds point `p` as a new vertex, subdividing the triangle or edge containing it; a point strictly outside the convex hull grows the hull, joining `p` to every hull edge it sees (a constrained hull edge stays constrained and becomes interior). It returns `false` — leaving the triangulation unchanged — only if `p` is already a vertex. For a triangulation built from a polygon, `p` is assumed to lie in the closed polygon, like the constructor's extra points (not checked). `insertDelaunay(p)` additionally restores the constrained Delaunay property around the new vertex by Lawson flips (never flipping constrained edges): a triangulation that was constrained Delaunay stays constrained Delaunay.

- `convexPartition()` returns a set of interior disjoint convex polygons that covers the domain, where each convex polygon is the union of one or more triangles. The result is guaranteed within a factor of four of the fewest convex pieces possible. A constrained edge is never deleted, so a partition can be shaped by the constraints the triangulation was built with. `Polygon::convexPartition()` and `PolygonWithHoles::convexPartition()` are shorthands for `triangulation().convexPartition()`. A convex polygon comes back as a single piece.

- `convexCovering()` grows one convex candidate independently from every triangle, then greedily selects candidates whose triangle sets cover the domain and removes redundant selections. The resulting convexes may overlap, are not guaranteed minimum, and never cross a constrained edge. `PolygonWithHoles::convexCovering()` is a shorthand for this method. `Polygon::convexCovering()` instead internally builds the SoCG 2023 full-visibility subgraph by a dual-graph BFS, applies a vertex clique cover, and takes the convex hull of each clique; that specialized graph construction is not part of the public `Triangulation` API.

- Other methods:

<p align="center">
  <img src="figures/example_triangulation2.svg" alt="Triangulation with a segment traversal highlighted" width="50%"/>
  <br/>
  <em>The constrained Delaunay triangulation of a polygon with points inside. Highlighting the triangles a segment meets and those whose interior it actually intersects.</em>
</p>


### Arrangement

`Arrangement<PointType, Label>` is the subdivision of the plane induced by a set of segments: its *vertices* are the input endpoints as well as every crossing, its *edges* are the pieces of segments those vertices cut the input into, and its *faces* are the connected components of what is left of the plane. Every point of the plane belongs to exactly one cell.

It is represented as a doubly connected edge list. Each edge is a pair of twin halfedges, and the face of a halfedge is always the one on its **left**, so a bounded face is enclosed by a counterclockwise cycle and the outer boundary of a connected piece of the input runs clockwise. Face 0 is always the unbounded face, and it is the only face of an empty arrangement. The three cell families are named by three distinct nested handle types — `Arrangement::VertexId`, `Arrangement::HalfedgeId` and `Arrangement::FaceId`.

Construction is exact, but the vertices must be representable: two integral segments cross at a rational point, so the default vertex type is `EPoint`, and an integral one is adequate only when the input meets at integer coordinates (orthogonal or interior-disjoint segments, for example).

- `Arrangement<>(V)` builds the arrangement of the shapes in container `V`. The accepted shapes are the bounded ones: a point (which becomes an isolated vertex), a segment or oriented segment, a polyline or monotone chain, the boundary of a triangle, rectangle, convex, polygon or region, and a `Shape` variant holding any of those. Anything unbounded — a line, a ray, a half-plane, a disk — is rejected at compile time. The input may be as degenerate as it likes otherwise: segments may cross, overlap collinearly, repeat, share endpoints, dangle with a free end, or run vertically. Overlapping and duplicated stretches are merged into a single edge, which then remembers every input shape it came from.

- `Arrangement<>(V, P)` additionally makes every point of container `P` a vertex, wherever it falls: a point on a shape splits it there, and a point on nothing becomes a vertex incident to no edge, in the interior of the face holding it. A point in the shape range does the same thing; passing the points apart is a convenience for the callers whose points and shapes come from different places.

- Cells: `vertexCount()`, `halfedgeCount()`, `edgeCount()` (half the halfedge count) and `faceCount()` (the unbounded face included). `vertices()` and `edges()` respectively return a vector of `Point` corresponding to the vertices and a vector of `Segment` corresponding to the edges.

- Handles: Indexing with a handle gives the cell's geometry: `a[v]` is the position of a vertex, and `a[h]` is the halfedge as an `OrientedSegment` from `source(h)` to `target(h)`, carrying the edge label.

- Incidence: `twin(h)` is the halfedge running along the same edge the other way, `next(h)` the next one along the boundary of the face on the left, `source(h)` and `target(h)` the vertices it leaves and reaches, and `face(h)` the face on its left. `outgoing(v)` returns one halfedge leaving a vertex, or the invalid handle when the vertex is isolated. `outgoingHalfedges(v)` returns every halfedge leaving the vertex in clockwise order, starting with `outgoing(v)`, and is empty for an isolated vertex; each entry after the first is `next(twin(h))` of the previous one.

- `witness(v)`, `witness(h)` and `witness(f)` return a point of a cell, one name for the three families: the vertex itself, the midpoint of an edge, and a point strictly inside a bounded face. The face witness is the cheap one — a diagonal midpoint or an ear's interior point, dividing coordinates by two or four — whenever the boundary is a single simple ring, and otherwise the more expensive ratio obtained by leaving a boundary edge along the inward normal.

- Faces: `isUnbounded(f)` tells the unbounded face apart, `outerCycle(f)` returns a halfedge of the face's counterclockwise outer cycle (the invalid handle for the unbounded face), and `innerCycles(f)` one halfedge of each clockwise inner cycle — the holes of the face, which for the unbounded face are the connected components of the input. `outerBoundaryOf(f)` materializes the outer cycle's halfedges (empty for the unbounded face), while `innerBoundariesOf(f)` materializes one vector of halfedges per inner cycle. `boundaryOf(f)` materializes every boundary halfedge in traversal order: the outer cycle first when there is one, then every inner cycle; it is empty for the empty arrangement's unbounded face. `hasSimpleBoundary(f)` says whether the face has neither a hole nor an edge with the face on both sides.

- `polygonWithHoles(f)` returns the closure of a bounded face as a `PolygonWithHoles` and throws `std::logic_error` for the unbounded or an invalid face handle. The result is *regularized*: a dangling edge sticking into the face is dropped, and a boundary cycle that pinches shut at a vertex is cut there into one ring per side. The remaining vertices are kept as they are, so a vertex in the middle of a straight stretch of boundary stays in the ring.

- Labels and history: `label(h)` is the label an edge inherited from the input shape that produced it and `label(f)` the label of a face, which starts default-constructed since nothing in the input is a face; both have a mutable overload, so a caller can write back whatever classification it ran per cell. `originsOf(h)` lists the positions, in the range the arrangement was built from, of every input shape that produced an edge — more than one exactly when input shapes overlap along it, sorted and without repetition.

- `locate(p)` returns the face containing a point. A point on an edge or a vertex belongs to no face, and the answer is then the face an infinitesimal displacement of the query towards `-x` lands in. The search is a linear scan over the edges, which is enough for the occasional query; a query-heavy caller wants a point-location structure, which this class does not have yet.

Unbounded input is not accepted yet: `isFictitious(h)`, which will tell a clipping artefact from a real edge, is already part of the interface and always false, so supporting it stays an additive change.
