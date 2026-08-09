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


### Interval Tree

`IntervalTree<Shape, Axis>` is a mutable one-dimensional index over bounded
shapes. It stores the closed interval obtained by projecting each shape's
bounding box onto `Axis`, which is `ProjectionAxis::x` by default or
`ProjectionAxis::y` when selected explicitly. The stored shapes themselves are
preserved. Its projection-prefixed query family makes decisions only from those
one-dimensional intervals; its unprefixed family uses the projection to prune
candidates, then applies the corresponding exact two-dimensional predicate.

- `IntervalTree<Shape>(V)` inserts every shape in container `V`; `insert(s)`
  and `erase(s)` add and remove one equal stored shape while retaining
  red-black-tree balance. Equal projected intervals are stored independently.

- `countProjectionsIntersecting(q)`, `reportProjectionsIntersecting(q)`,
  `visitProjectionsIntersecting(q, f)`, and `emptyProjectionsIntersecting(q)`
  match shapes whose projected closed interval meets that of `q`; touching at
  an endpoint counts as intersection.

- `countProjectionsContainedIn(q)`, `reportProjectionsContainedIn(q)`,
  `visitProjectionsContainedIn(q, f)`, and `emptyProjectionsContainedIn(q)`
  match shapes whose entire projected closed interval is within that of `q`,
  including shared endpoints.

- `countIntersecting(q)`, `reportIntersecting(q)`, `visitIntersecting(q, f)`,
  and `emptyIntersecting(q)` first prune by the selected projection and then
  test `shape.intersects(q)`. `countContainedIn(q)`, `reportContainedIn(q)`,
  `visitContainedIn(q, f)`, and `emptyContainedIn(q)` similarly test
  `q.contains(shape)`. These unprefixed methods return the same results as the
  corresponding `ShapeTree` methods over the same stored shapes.

Like `ShapeTree`, report methods return copies of the stored shapes, visitors
receive them by const reference and may stop early by returning `true`, and
`has`, `size`, `empty`, `shapes`, and const iterators provide container-like
access. The tree is augmented with its subtree endpoint extrema, so irrelevant
subtrees are pruned during both query families. Nodes use 32-bit identifiers
and keep query data separate from insertion-only state; a tree can therefore
hold at most `2^32 - 1` shapes.


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

`Arrangement<PointType, Label>` is the subdivision of the plane induced by segments, rays and lines. Its finite *vertices* are finite input endpoints, isolated input points, crossings and the ends of overlaps; its *edges* are the atomic segment, ray or line pieces between them; and its *faces* are the connected components of the complement. Every finite point of the plane belongs to exactly one cell.

The `Arrangement` class template represents an arrangement whose topology is fixed after construction. It is represented as a doubly connected edge list. Each edge is a pair of twin halfedges, and the face of a halfedge is always the one on its **left**, so a bounded face is enclosed by a counterclockwise cycle and the outer boundary of a connected piece of the input runs clockwise. All unbounded edge ends meet at one symbolic vertex at infinity, ordered by their exact escape direction and transverse carrier order; no clipping frame or fictitious edge is introduced. Face 0 is an unbounded face, but a line or a pair of rays can create several unbounded faces. The empty arrangement has only face 0. The handle types belong to the arrangement specialization: `pgl::Arrangement<>::VertexId`, `pgl::Arrangement<>::HalfedgeId` and `pgl::Arrangement<>::FaceId`.

Construction is exact, but the vertices must be representable: two carriers with integral defining points can cross at a rational point, so the default vertex type is `EPoint`, and an integral one is adequate only when every crossing has integer coordinates (orthogonal or interior-disjoint segment input, for example).

- `Arrangement<>(V)` builds the arrangement of the shapes in container `V`. It accepts points, segments and oriented segments, polylines and monotone chains, boundaries of triangles, rectangles, convexes, polygons, polygons with holes, lines, oriented lines, rays, and a `Shape` variant holding any of them. The input may cross, overlap collinearly, repeat, share endpoints or dangle with a free end. Overlapping stretches of segments, rays and lines are merged into atomic edges, each of which remembers every input shape that covers it.

- `Arrangement<>(V, P)` additionally makes every point of container `P` a vertex, wherever it falls: a point on a shape splits it there, and a point on nothing becomes a vertex incident to no edge, in the interior of the face holding it. A point in the shape range does the same thing; passing the points apart is a convenience for the callers whose points and shapes come from different places.

- Cells: `vertexCount()`, `edgeCount()` and `faceCount()` report the numbers of finite vertices, geometric edges and faces, respectively; every unbounded face is included. `vertexCount()` therefore always equals `vertices().size()`, and the finite vertex handle indices are `[0, vertexCount())`. When present, the symbolic infinity vertex is not included in either count or vector. Its handle index is `vertexCount()` and `isFictitious(v)` identifies it; `isUnbounded()` tells whether that infinity vertex exists, equivalently whether the arrangement contains any unbounded edge. The method `edges()` returns one `EdgeType` (`variant<SegmentType, LineType, RayType>`) per edge, in edge-index order. `boundedEdges()` returns only the `SegmentType` alternatives.

- Handles: Indexing with a handle gives the cell's geometry. `a[v]` is the position of a finite vertex and throws `std::logic_error` for the infinity vertex. `a[h]` is a `HalfedgeType` (`variant<OrientedSegmentType, OrientedLineType, RayType>`) carrying the edge label. The two halfedges of a segment or line return opposite orientations. A ray has only one finite source, so both halfedges return the same `RayType`, although they remain distinct oppositely directed topological halfedges.

- Edge and vertex incidence: `isUnbounded(h)` tells whether the edge reaches the symbolic infinity vertex and has the same value for both of its halfedges. A ray has one finite endpoint and one infinity endpoint; both endpoints of a line are infinity. `outgoingHalfedges(v)` returns every halfedge leaving a vertex in clockwise order and is empty for an isolated vertex. It also accepts the infinity handle, returning the angularly ordered fan of unbounded ends. `degree(v)` returns how many there are without building the vector, which is one halfedge per incident edge end: a vertex where `k` lines cross has degree `2k`.

- `witness(v)`, `witness(h)` and `witness(f)` return a point of a cell, one name for the three families: the vertex itself, an interior point of an edge, and a point strictly inside a bounded face. `witness(v)` throws for the infinity vertex. The face witness is the cheap one — a diagonal midpoint or an ear's interior point, dividing coordinates by two or four — whenever the boundary is a single simple ring, and otherwise the more expensive ratio obtained by leaving a boundary edge along the inward normal.

- Faces: `isUnbounded(f)` identifies every unbounded face. `outerBoundaryOf(f)` returns the halfedges of a bounded face's counterclockwise outer boundary and is empty for an unbounded face. `innerBoundariesOf(f)` returns one vector per clockwise inner boundary; for an unbounded face, a boundary that reaches infinity is represented here too. `boundaryOf(f)` concatenates the outer boundary when present and then every inner boundary, preserving traversal order; it is empty for the empty arrangement's sole face. `hasSimpleBoundary(f)` says whether the face has neither a hole nor an edge with the face on both sides.

- `polygonWithHoles(f)` returns the closure of a bounded face as a `PolygonWithHoles` and throws `std::logic_error` for an unbounded or invalid face handle. The result is *regularized*: a dangling edge sticking into the face is dropped, and a boundary cycle that pinches shut at a vertex is cut there into one ring per side. The remaining vertices are kept as they are, so a vertex in the middle of a straight stretch of boundary stays in the ring.

- `halfplaneIntersection(f)` returns the intersection of the supporting half-planes of the face's outer boundary, ignoring holes and two-sided slit or dangling edges. Its optional `ResultNumber` template argument selects the defining-point coordinate type. The method accepts bounded and unbounded faces and throws `std::logic_error` only for an invalid handle. For an unbounded face it uses boundary walks that reach infinity and ignores bounded inner boundaries; the empty arrangement and the exterior of an isolated bounded component therefore produce the whole plane. The result equals the face with its holes filled when the outer boundary is convex. Its half-planes use the defining coordinates already stored for each arrangement edge, so requesting an integral `ResultNumber` can truncate fractional split coordinates.

- Labels and history: `label(h)` is the label an edge inherited from the input shape that produced it and `label(f)` the label of a face, which starts default-constructed since nothing in the input is a face; both have a mutable overload, so a caller can write back whatever classification it ran per cell. `originsOf(h)` lists the positions, in the range the arrangement was built from, of every input shape that produced an edge — more than one exactly when input shapes overlap along it, sorted and without repetition. `originsOf(v)` lists the same positions for every input shape passing through a vertex, the union over its incident edges, again sorted and without repetition; an isolated vertex is incident to no edge and has no origins, even when an input point put it there.

- `locate(p)` returns the face containing a point. A point on an edge or a vertex belongs to no face, and the answer is then the face an infinitesimal displacement of the query towards `-x` (and, when that does not leave the boundary, towards `+y`) lands in. The search is a linear scan over the edges, which is enough for the occasional query; a query-heavy caller wants a point-location structure, which this class does not have yet.

The following methods expose the representation directly. They are useful when implementing algorithms on the subdivision, but ordinary face extraction and point location do not require them.

- `halfedgeCount()` is twice `edgeCount()`. Halfedges have consecutive handles and twin pairs are adjacent, so they can be enumerated without allocating a vector:

- `twin(h)` returns the halfedge of the same edge in the opposite topological direction. `next(h)` returns the following halfedge along the boundary of the face on the left. Repeatedly following `next` traverses one boundary cycle.

- `source(h)` and `target(h)` return the endpoint handles in traversal order, and `face(h)` returns the face on the left. For a ray, one endpoint is the symbolic infinity vertex; for a line, both endpoint handles name that same vertex.

- `outgoing(v)` returns one halfedge leaving a vertex, or the invalid handle when the vertex is isolated. It is the starting halfedge returned first by `outgoingHalfedges(v)`.

- `outerCycle(f)` returns one halfedge of a bounded face's counterclockwise outer cycle and the invalid handle for an unbounded face. `innerCycles(f)` returns one starting halfedge per clockwise inner cycle; an unbounded face's boundary walks through infinity are represented as inner cycles. Prefer `outerBoundaryOf(f)` and `innerBoundariesOf(f)` when the complete vectors are wanted.

There are no fictitious halfedges: every halfedge represents part of an input segment, ray or line.
