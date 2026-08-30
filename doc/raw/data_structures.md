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

- `nearestNeighbor(q)` returns the nearest stored shape by reference. `kNearestNeighbors(q, k)` returns up to `k` nearest shapes as nearest-first copies.

- Other methods:

Sending a tree to a [Canvas](canvas.md) with `canvas << tree` draws all node bounding boxes. Is is possible to insert a new element with `insert`, but no rebalancing is performed.

<p align="center">
  <img src="../../examples/figures/example_shapetree_triangles.svg" alt="Shape tree range query over random triangles" width="50%"/>
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
  An insertion takes $O(\log n)$ time. A removal instead tombstones the node
  owning the shape, which stays in the tree, keeps it balanced, and matches
  nothing from then on; the index is rebuilt only once tombstones outnumber the
  live nodes, so a removal costs $O(\log n + k)$ amortized, where $k$ is the
  number of stored intervals sharing the projected endpoints of the removed
  shape. The stored shapes stay compact: only their order may change.

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
and keep query data separate from insertion-only state, and an identifier is
also the index of the shape the node owns, tombstones being exactly the
identifiers past the last stored shape. A tree therefore holds at most
`2^32 - 2` nodes, shapes and tombstones together, so at least `2^31 - 1` shapes
always fit.


### Triangulation

`Triangulation` stores a mutable triangulation of either a polygon or a point set: vertex coordinates never move once added, but new vertices can be inserted (`insert`, `insertDelaunay`) and the connectivity changes through flips.

It may be constructed from a Polygon (constrained Delaunay triangulation), a container of points (Delaunay triangulation), a container of points plus a container of non-crossing segments (conforming constrained Delaunay: the segments become constrained edges and nothing is carved away), segments forming a complete triangulation, or triangles, always keeping labels. The polygon constructor optionally takes a container of extra interior points (added as vertices) and/or a container of interior segments (added as vertices and constrained edges); either may be omitted, and both are assumed to lie inside the polygon (not checked). Attention, the segments or triangles must define a valid triangulation (of the convex hull or any polygon), otherwise the behavior is undefined.

- `locate(p)` returns a triangle containing point `p`, or none if `p` is outside, via a randomized visibility walk. The walk starts where the previous query ended, which is fast for queries that follow one another. `buildPointLocation()` gives it a better start triangle using a randomized trapezoidal map over a coarsening of the Delaunay triangulation. The point location structure is not updated when the triangulation is modified, but the quality of the start triangle deteriorates slowly under changes.

- Navigation: `otherTriangle`, `edgeAdjacentTriangles`, `vertexAdjacentTriangles`, `incidentTriangles` (of an edge or of a vertex), the `visitTriangles`/`visitEdges` visitors, and the sorted `triangles()`/`edges()`.

- Range searching: `trianglesIntersecting(s)` return the triangles that satisfy `triangle.intersects(s)`. The function has several variantions `visitTrianglesIntersecting(s,f)` calls the function `f` on these triangles and stops early if `f` returns `true`. If `s` is an oriented segment, oriented line, or ray, the triangles are visited in order. A polyline or a monotone chain is also traced in order, edge by edge, each triangle reported the first time the chain meets it (so a chain may leave the triangulated region and come back). The edge variations `edgesIntersecting` and `visitEdgesIntersecting` list the edges instead of the triangles. The `…InteriorIntersecting` variantions filter with `interiorIntersects(s)`.

- Predicates against the domain — the region the triangulation covers, which is the polygon for the polygon constructors and the convex hull otherwise. `contains(s)`, `interiorContains(s)`, `intersects(s)` and `interiorsIntersect(s)` give exactly the answers the shape predicates of the same name give for that region as a `Polygon`, boundary and all: a segment running along a polygon edge is contained and met, but neither interior-contained nor interior-intersecting. They work on the mesh, so the cost is proportional to the triangles `s` meets rather than to the size of the boundary. Every shape type is accepted: an unbounded one (line, oriented line, ray, half-plane) is never contained in the bounded domain, and the empty shape is contained by it but meets nothing. They answer a different question from `has(t)` and `has(s)`, which ask whether a triangle or a segment is a *cell* of the mesh rather than how the domain covers it geometrically.

- `flip(e)` replaces the diagonal shared by two triangles. It returns the new edge obtained or none if the flip cannot be performed (non-convex quadrilateral or the edge is constrained). `flippable(e)` simply returns if the flip can be performed without changing the triangulation. If we pass a container with edges in interior-disjoint quadrilaterals, the functions use parallel flips.

- `insert(p)` adds point `p` as a new vertex, subdividing the triangle or edge containing it; a point strictly outside the convex hull grows the hull, joining `p` to every hull edge it sees (a constrained hull edge stays constrained and becomes interior). It returns `false` — leaving the triangulation unchanged — only if `p` is already a vertex. For a triangulation built from a polygon, `p` is assumed to lie in the closed polygon, like the constructor's extra points (not checked). `insertDelaunay(p)` additionally restores the constrained Delaunay property around the new vertex by Lawson flips (never flipping constrained edges): a triangulation that was constrained Delaunay stays constrained Delaunay.

- `asGraph()` returns the 1-skeleton of the mesh as a `Graph<PointType>`: its vertices are the `numVertices()` stored points and its edges are the `numEdges()` edges of the visible mesh. A point identifies a vertex of a triangulation, so the graph is keyed by the points themselves rather than by handles, and edge labels, which a graph has no place for, are dropped. A vertex left without any in-domain edge — one duplicated or collinear with every other point, which carries no triangle — comes back as an isolated graph vertex. The ghost vertex closing the mesh at infinity is internal and is not one of them.

- `voronoiDiagram()` returns the unbounded Arrangement dual to the current triangulation. The triangulation must be nonempty and its current real triangles must form a Delaunay triangulation of all stored vertices. The result uses exact rational vertices by default for integral input. Each face is labeled with the point that generated its Voronoi cell, so `diagram.label(diagram.locateFace(q))` returns the nearest site. At a Voronoi edge or vertex, `locateFace` selects one tied site by its usual infinitesimal-perturbation rule; use `locateCell`, then inspect the incident faces, to recover every tied site.

- `convexPartition()` returns a set of interior disjoint convex polygons that covers the domain, where each convex polygon is the union of one or more triangles. The result is guaranteed within a factor of four of the fewest convex pieces possible. A constrained edge is never deleted, so a partition can be shaped by the constraints the triangulation was built with. `Polygon::convexPartition()` and `PolygonWithHoles::convexPartition()` are shorthands for `triangulation().convexPartition()`. A convex polygon comes back as a single piece.

- `convexCovering()` grows one convex candidate independently from every triangle, then greedily selects candidates whose triangle sets cover the domain and removes redundant selections. The resulting convexes may overlap, are not guaranteed minimum, and never cross a constrained edge. `PolygonWithHoles::convexCovering()` is a shorthand for this method. `Polygon::convexCovering()` instead internally builds the SoCG 2023 full-visibility subgraph by a dual-graph BFS, applies a vertex clique cover, and takes the convex hull of each clique; that specialized graph construction is not part of the public `Triangulation` API.

- Low level: `TriId` and `VertexId` handle a triangle and a vertex of the mesh. They are opaque strong types belonging to the triangulation specialization (`pgl::Triangulation<>::TriId`), comparable and hashable, so they key a set or a map. `getId(t)` and `getId(p)` turn a triangle or a point into its handle — the invalid handle when it is not a cell of the mesh — and `getShape(id)`, also spelled `operator[]`, turns a handle back into the triangle or the point. `locateId(p)` is `locate(p)` answering with a handle, which is how a query point that is no vertex enters the handle world without a lookup. The navigation methods have handle overloads that answer with handles instead of shapes: `has(id)`, `vertices(id)` (the three vertices of a triangle, in the order `getShape(id)` lists them), `otherTriangle(t,a,b)` (the shared edge given by its two endpoint handles rather than as a segment), `edgeAdjacentTriangles(id)`, `vertexAdjacentTriangles(id)`, `incidentTriangles(v)`, and `label(id)`. They read the connectivity arrays directly, so walking the mesh costs no hash lookup per step — only the single `getId` that entered the handle world. `triangleIds()` and `vertexIds()` enumerate the mesh as handles, and every triangle visitor — `visitTriangles(f)`, `visitTrianglesIntersecting(s,f)` and `visitTrianglesInteriorIntersecting(s,f)` — hands handles to an `f` that takes a `TriId` (a callable taking a triangle, or a generic one, still gets values), so a global pass or a range search starts in the handle world instead of entering it per triangle. An edge of a triangle is named by its side index: side `i` of `t` runs from `vertices(t)[i]` to `vertices(t)[(i+1)%3]` — the edge `getShape(t).edges()[i]` — and `otherTriangle(t,i)` crosses it, `isConstrained(t,i)` and `setConstrained(t,i,value)` read and write its constrained flag. That flag is what the polygon and region constructors mark their boundary with, so a walk recognizes the boundary of the shape it came from without keeping a set of its edges. `triangleIndexBound()` and `vertexIndexBound()` bound the handles' `index()`, which lets a side table be a `std::vector` rather than a map: triangle indices are dense over `[0, triangleIndexBound())` — a range that also covers the triangles a polygon or region domain carved away, so it exceeds `numTriangles()` by exactly those — and vertex indices are dense over `[1, vertexIndexBound())`, slot 0 belonging to the internal ghost vertex, so `index() - 1` is the tight slot of a table of `numVertices()` entries. A handle is a position in the triangulation's own storage: it stays valid as long as the triangulation is only read, and `insert` or `flip` may reuse it for another cell.

- Other methods:

<p align="center">
  <img src="../../examples/figures/example_polygon_triangulation.svg" alt="Triangulation with a segment traversal highlighted" width="50%"/>
  <br/>
  <em>The constrained Delaunay triangulation of a polygon with points inside. Highlighting the triangles a segment meets and those whose interior it actually intersects.</em>
</p>


### Arrangement

`Arrangement<PointType, Label>` is the subdivision of the plane induced by segments, rays and lines. Its finite *vertices* are finite input endpoints, isolated input points, crossings and the ends of overlaps; its *edges* are the atomic segment, ray or line pieces between them; and its *faces* are the connected components of the complement. Every finite point of the plane belongs to exactly one cell.

The `Arrangement` class template represents an arrangement whose topology is fixed after construction. It is represented as a doubly connected edge list. Each edge is a pair of twin halfedges, and the face of a halfedge is always the one on its **left**, so a bounded face is enclosed by a counterclockwise cycle and the outer boundary of a connected piece of the input runs clockwise. All unbounded edge ends meet at one symbolic vertex at infinity, ordered by their exact escape direction and transverse carrier order; no clipping frame or fictitious edge is introduced. Face 0 is an unbounded face, but a line or a pair of rays can create several unbounded faces. The empty arrangement has only face 0. The handle types belong to the arrangement specialization: `pgl::Arrangement<>::VertexId`, `pgl::Arrangement<>::HalfedgeId` and `pgl::Arrangement<>::FaceId`.

Construction is exact, but the vertices must be representable: two carriers with integral defining points can cross at a rational point, so the default vertex type is `EPoint`, and an integral one is adequate only when every crossing has integer coordinates (orthogonal or interior-disjoint segment input, for example). A 32-bit `int` number type is roughly 20 times faster than `ERational` and can safely contain coordinates up to $10^8$ without an overflow.

- `Arrangement<>(V)` builds the arrangement of the shapes in container `V`. It accepts points, segments and oriented segments, polylines and monotone chains, boundaries of triangles, rectangles, convexes, polygons, polygons with holes, lines, oriented lines, rays, and a `Shape` variant holding any of them. The input may cross, overlap collinearly, repeat, share endpoints or dangle with a free end. Overlapping stretches of segments, rays and lines are merged into atomic edges, each of which remembers every input shape that covers it.

- `Arrangement<>(V, P)` additionally makes every point of container `P` a vertex, wherever it falls: a point on a shape splits it there, and a point on nothing becomes a vertex incident to no edge, in the interior of the face holding it. A point in the shape range does the same thing; passing the points apart is a convenience for the callers whose points and shapes come from different places.

- Cells: `vertexCount()`, `edgeCount()` and `faceCount()` report the numbers of finite vertices, geometric edges and faces, respectively; every unbounded face is included. `vertexCount()` therefore always equals `vertices().size()`, and the finite vertex handle indices are `[0, vertexCount())`. When present, the symbolic infinity vertex is not included in either count or vector. Its handle index is `vertexCount()` and `isFictitious(v)` identifies it; `isUnbounded()` tells whether that infinity vertex exists, equivalently whether the arrangement contains any unbounded edge. The method `edges()` returns one `EdgeType` (`variant<SegmentType, LineType, RayType>`) per edge, in edge-index order. `boundedEdges()` returns only the `SegmentType` alternatives.

- Handles: Indexing with a handle gives the cell's geometry. `a[v]` is the position of a finite vertex and throws `std::logic_error` for the infinity vertex. `a[h]` is a `HalfedgeType` (`variant<OrientedSegmentType, OrientedLineType, RayType>`) carrying the edge label. The two halfedges of a segment or line return opposite orientations. A ray has only one finite source, so both halfedges return the same `RayType`, although they remain distinct oppositely directed topological halfedges.

- Edge and vertex incidence: `isUnbounded(h)` tells whether the edge reaches the symbolic infinity vertex and has the same value for both of its halfedges. A ray has one finite endpoint and one infinity endpoint; both endpoints of a line are infinity. `outgoingHalfedges(v)` returns every halfedge leaving a vertex in clockwise order and is empty for an isolated vertex. It also accepts the infinity handle, returning the angularly ordered fan of unbounded ends. `degree(v)` returns how many there are without building the vector, which is one halfedge per incident edge end: a vertex where `k` lines cross has degree `2k`.

- `asGraph()` returns the vertex-edge incidence structure as a `Graph<VertexId>`: every vertex is a graph vertex, isolated ones included, and every edge joins the handles of its two endpoints. The symbolic infinity vertex is one of them whenever the arrangement has it, so a ray reaches it there just as it does in the halfedge structure. The graph is keyed by handles rather than points because the infinity vertex has no point; `a[v]` recovers the position of a finite one. A `Graph` is simple, so a line, whose two ends are that same infinity vertex, contributes a self-loop and hence no graph edge, and two edges sharing both of their endpoints — two rays leaving one vertex, meeting again at infinity — coalesce into one.

- `witness(v)`, `witness(h)` and `witness(f)` return a point of a cell, one name for the three families: the vertex itself, an interior point of an edge, and a point strictly inside a bounded face. `witness(v)` throws for the infinity vertex. The face witness is the cheap one — a diagonal midpoint or an ear's interior point, dividing coordinates by two or four — whenever the boundary is a single simple ring, and otherwise the more expensive ratio obtained by leaving a boundary edge along the inward normal.

- Faces: `isUnbounded(f)` identifies every unbounded face. `outerBoundaryOf(f)` returns the halfedges of a bounded face's counterclockwise outer boundary and is empty for an unbounded face. `innerBoundariesOf(f)` returns one vector per clockwise inner boundary; for an unbounded face, a boundary that reaches infinity is represented here too. `boundaryOf(f)` concatenates the outer boundary when present and then every inner boundary, preserving traversal order; it is empty for the empty arrangement's sole face. `hasSimpleBoundary(f)` says whether the face has neither a hole nor an edge with the face on both sides.

- `polygonWithHoles(f)` returns the closure of a bounded face as a `PolygonWithHoles` and throws `std::logic_error` for an unbounded or invalid face handle. The result is *regularized*: a dangling edge sticking into the face is dropped, and a boundary cycle that pinches shut at a vertex is cut there into one ring per side. The remaining vertices are kept as they are, so a vertex in the middle of a straight stretch of boundary stays in the ring.

- `halfplaneIntersection(f)` returns the intersection of the supporting half-planes of the face's outer boundary, ignoring holes and two-sided slit or dangling edges. Its optional `ResultNumber` template argument selects the defining-point coordinate type. The method accepts bounded and unbounded faces and throws `std::logic_error` only for an invalid handle. For an unbounded face it uses boundary walks that reach infinity and ignores bounded inner boundaries; the empty arrangement and the exterior of an isolated bounded component therefore produce the whole plane. The result equals the face with its holes filled when the outer boundary is convex. Its half-planes use the defining coordinates already stored for each arrangement edge, so requesting an integral `ResultNumber` can truncate fractional split coordinates.

- Labels and history: `label(h)` is the label an edge inherited from the input shape that produced it and `label(f)` the label of a face, which starts default-constructed since nothing in the input is a face; both have a mutable overload, so a caller can write back whatever classification it ran per cell. `originsOf(h)` lists the positions, in the range the arrangement was built from, of every input shape that produced an edge — more than one exactly when input shapes overlap along it, sorted and without repetition. `originsOf(v)` lists the same positions for every input shape passing through a vertex, the union over its incident edges, again sorted and without repetition; an isolated vertex is incident to no edge and has no origins, even when an input point put it there.

- `locateCell(p)` returns a `CellId`, which is a `variant<VertexId, HalfedgeId, FaceId>` identifying the cell that actually contains the point, while `locateFace(p)` returns the face containing a point (a point on an edge or a vertex belongs to no face, and the answer is then the face an infinitesimal displacement of the query). The two methods scans the edges one by one in linear time, unless `buildPointLocation()` has been called previously. After that it uses an exact randomized trapezoidal map and search DAG with expected logarithmic query time. `hasPointLocation()` reports if the map has been built and `clearPointLocation()` releases this arrangement's reference to the map. Building takes expected `O(E log E)` time and `O(E)` space, and has an overload accepting a random-bit generator.

- `visitIntersecting(r, fn)` walks the cells a directed curve meets, in order along the curve, calling `fn` with an `IntersectionId` — a `variant<HalfedgeId, VertexId>` naming a vertex or an edge. The query may be an `OrientedSegment`, an `OrientedLine`, a `Ray`, a `MonotoneChain` or a `Polyline`. Where the curve meets an edge only at one of its endpoints, the vertex there stands for the contact and the edge is not reported for it; a straight piece meeting that same edge away from its endpoints still reports the edge. An edge is named by one of its two twin halfedges, and a chain meeting the same cell more than once reports it at its first encounter. A visitor returning `bool` stops the walk by returning `true`, one returning `void` never stops it, and the method returns whether the visitor stopped early.

- `reportIntersecting(r)` collects the same cells into a `vector<IntersectionId>`, `firstIntersecting(r)` returns only the first as an `optional<IntersectionId>`, and `emptyIntersecting(r)` reports whether the curve meets no cell at all.

- Without a point-location index the walk scans every vertex and edge. After `buildPointLocation()`, it searches the trapezoidal map earliest-first, so a visitor that stops early stops the search with it: the cost is proportional to the part of the query lying before the last cell reported, rather than to the whole query. `firstIntersecting(r)` on a long or unbounded curve therefore pays only for its beginning. The work is output-sensitive rather than bounded by `E`, since a query can cross many trapezoids without meeting a cell.

The following methods expose the representation directly. They are useful when implementing algorithms on the subdivision, but ordinary face extraction and point location do not require them.

- `halfedgeCount()` is twice `edgeCount()`. Halfedges have consecutive handles and twin pairs are adjacent, so they can be enumerated without allocating a vector:

- `twin(h)` returns the halfedge of the same edge in the opposite topological direction. `next(h)` returns the following halfedge along the boundary of the face on the left. Repeatedly following `next` traverses one boundary cycle.

- `source(h)` and `target(h)` return the endpoint handles in traversal order, and `face(h)` returns the face on the left. For a ray, one endpoint is the symbolic infinity vertex; for a line, both endpoint handles name that same vertex.

- `outgoing(v)` returns one halfedge leaving a vertex, or the invalid handle when the vertex is isolated. It is the starting halfedge returned first by `outgoingHalfedges(v)`.

- `outerCycle(f)` returns one halfedge of a bounded face's counterclockwise outer cycle and the invalid handle for an unbounded face. `innerCycles(f)` returns one starting halfedge per clockwise inner cycle; an unbounded face's boundary walks through infinity are represented as inner cycles. Prefer `outerBoundaryOf(f)` and `innerBoundariesOf(f)` when the complete vectors are wanted.

There are no fictitious halfedges: every halfedge represents part of an input segment, ray or line.


### Graph

`Graph<Vertex>` is an undirected simple graph stored as adjacency sets, where `Vertex` is any hashable and equality-comparable type. It is the combinatorial companion of the geometric structures: visibility graph, triangulation, and arrangement.

- `Graph<Vertex>()` builds an empty graph and `Graph<Vertex>(E)` takes a vector of `std::array<Vertex, 2>` endpoint pairs. `addVertex(v)` adds an isolated vertex and `addEdge(u, v)` adds an edge along with any endpoint still missing. `removeEdge(u, v)` leaves the endpoints in place, `removeVertex(v)` deletes every incident edge with the vertex, and `clear()` empties the graph.

- `containsVertex(v)`, `containsEdge(u, v)`, `vertexCount()`, `edgeCount()` and `maxDegree()` inspect the graph, while `degree(v)` returns the number of neighbors of a vertex or `-1` when it is absent. `vertices()` returns a lazy view of the vertices, which are the keys of the adjacency map and come out as const references in unspecified order, `neighbors(v)` returns the adjacency set by const reference and throws `std::out_of_range` for an absent vertex, and `closedNeighbors(v)` returns a copy of that set including `v` itself. Iterating over a graph with its forward iterators visits every vertex as a const reference, since modifying one in place would invalidate the hash table.

- `edges()` returns a lazy view of the edges, each as a `std::array<Vertex, 2>` holding its two endpoints in increasing order, which requires a totally ordered `Vertex` unlike the rest of the graph. The view allocates nothing and walks the adjacency sets in place, dropping the direction of each edge that would come out decreasing, so every edge is visited exactly once but in unspecified order. Like `vertices()`, it refers to the graph, and modifying the graph invalidates it. Iterating it all costs $O(n + m)$ for a graph with $n$ vertices and $m$ edges, which is also what reaching the first edge may cost when many vertices are isolated.

- `bfs(v)` returns the connected component of `v` in breadth-first order and `bfs(v, k)` stops after `k` vertices; both return nothing when `v` is absent. `components()` returns the connected components, largest first, an isolated vertex forming a one-vertex component.

- `biconnectedComponents()` returns the vertex sets of the vertex-biconnected blocks, largest first, using an iterative version of Tarjan's depth-first search whose memory does not grow with recursion depth. A bridge comes back as a two-vertex block, an articulation vertex belongs to more than one block, and an isolated vertex, defining no edge, belongs to none.

- `cliqueCover()` partitions the vertices into cliques, largest first, by running the DSATUR coloring heuristic on the complement graph: vertices sharing a color are pairwise adjacent here. Every vertex appears in exactly one clique, but the number of cliques is not guaranteed minimum.

- `spanningTree(w)` returns a minimum spanning tree as another graph, where the edge weight function `w(u, v)` chooses its own number type: any copyable type ordered by `<`, exact rationals included. It should be symmetric, otherwise which of the two values is used for an edge is unspecified. Prim's algorithm grows the tree from a lazy binary heap (`std::priority_queue`) of frontier edges, in $O(m \log m)$ time with one weight evaluation per edge for a graph with $m$ edges. Ties between equally heavy edges are broken arbitrarily. A disconnected graph produces one minimum spanning tree per connected component, so the result always has the same vertices and the same components as the graph it comes from, isolated vertices included.

- `shortestPath(s, t, w)` returns the vertices of a shortest path from `s` to `t`, both included, under the same kind of edge weight function as `spanningTree(w)` with the added requirement that weights are non-negative and add up with `+`. Neither condition is checked, and the path is unspecified when either fails. This overload uses Dijkstra's algorithm and stops as soon as `t` is settled, in $O(m \log m)$ time with $O(m)$ weight evaluations for a graph with $m$ edges. The overload `shortestPath(s, t, w, h)` instead uses A*, where `h(v, t)` returns a lower bound on the remaining distance in the same weight type: it must be non-negative, no greater than the shortest distance from `v` to `t`, and zero at `t`. These requirements are also unchecked. A consistent lower bound gives the same $O(m \log m)$ bound and can avoid exploring vertices that cannot improve the route; an admissible but inconsistent one remains correct because vertices are reopened when a shorter route reaches them, although this can require more work. For either overload, the path from a vertex to itself is that vertex alone, and an empty result means there is none, whether because `s` and `t` lie in different components or because one of them is absent. Ties between equally long paths are broken arbitrarily.

- Other methods:

### Bit Matrix

`BitMatrix<PointType>` represents a digital geometry shape: the union of the unit squares $[x, x+1] \times [y, y+1]$ for a set of $(x,y)$ points in a rectangular window. It stores one bit per cell of a rectangular window of the integer grid, packed into 64-bit words. Cell `(x, y)` is the unit square $[x, x+1] \times [y, y+1]$, named by its lower-left corner. The window is fixed at construction and never grows: writing outside it is a no-op and reading outside it gives `false`, so sizing it is the caller's job. It is the cheap representation for rectilinear work that the exact polygonal one makes expensive — set algebra, Minkowski sums, connectivity — at the price of only representing what the grid can.

A matrix wears two hats. As a **region of the plane**, the union of its cells as unit squares, for the predicates and for `area()`, `perimeter()`, `centroid()`, `bbox()`, `convexHull()`, `asPolygonWithHoles()` and `asPolygonSet()`. As a **set of lattice points**, each cell standing for its lower-left corner, for the `lattice`-prefixed morphology and the reflections: the sum of cells `a` and `b` is the single cell `a + b`, which is what makes a structuring element behave. `minkowskiSum(b)` without the prefix wears the region hat instead. Translation is the same either way.

- `BitMatrix(origin, width, height)` covers `width` by `height` cells from `origin`, all clear; a non-positive extent leaves the window empty. `BitMatrix(box)` takes the window as the rectangle its cells cover, the inverse of `window()`. `BitMatrix(region)` rasterizes a `PolygonWithHoles` whose every edge is axis-parallel, over its own bounding box, filling the cells the region covers; it throws `std::logic_error` for any other region. The region may carry any coordinate type, but every coordinate must be a whole number this grid can hold, checked rather than rounded. `BitMatrix(polygon)` and `BitMatrix(set)` do the same for a `Polygon` and for a `PolygonSet`, the set over the bounding box of all its components; each of the three shapes spells its own constructor `asBitMatrix()`. `BitMatrix(points)` instead takes a range of points as the cells themselves, over the smallest window holding them, so it returns its own `trimmed()`; it reads the same coordinate types the same way. A shape is not such a range even where it iterates over its vertices, and neither is another matrix, which carries a window this would trim: pass `lattice()` or `latticeView()` to ask for that reading of one anyway.

- `innerRaster(shape, box)` and `outerRaster(shape, box)` rasterize **any** shape over a window, the first keeping the cells the shape contains and the second the cells it meets, so the two bracket the shape. Each costs one exact predicate per cell of the window, against the one pass per row that `BitMatrix(region)` takes for the rectilinear case. Given a bounded shape with integer coordinates, the window may be left out and is then its bounding box.

- `get(x,y)` and `set(x,y)` read and write one cell, along with `set(x,y,value)`, `reset(x,y)`, `flip(x,y)`, `setAll()` and `clear()`; each also takes a point, and `set`, `reset` and `flip` also take a range of them. `origin()`, `width()`, `height()`, `window()`, `emptyWindow()`, `inWindow(x,y)` and `sameWindow(b)` describe the window, `resized(box)` moves the cells to another one, dropping those it does not hold, and `trimmed()` moves them to the smallest window holding them.

- `count()`, `empty()` and the const iterators report the set cells, which the iterators yield as points in row-major order. The cells come out under two readings: as the lattice points every `lattice`-prefixed operation works in, and that `get` and `set` address them by, or as the unit squares they cover. `lattice()` and `cells()` materialize those into a vector of `Point` or of `Rectangle`; `latticeView()` and `cellsView()` return the same sequences as lazy forward views instead, building each element as it is reached and allocating nothing. Like `Graph`'s views they refer to the matrix and are invalidated by anything that modifies it. `rectangles()` instead merges each row's runs into as few rectangles as a row-major pass can, disjoint and covering, which is how to draw the cells as separate elements: `canvas << matrix.rectangles()`.

- `area()`, `perimeter()`, `centroid()`, `pointInside()`, `bbox()` and `fbox()` measure the covered region: the number of cells, the length of its boundary counting the edges of a cell on the window border, the average of the cell centers, the center of the first set cell, and the rectangle the cells cover. `area()` and `perimeter()` take their result type as a template parameter defaulting to the coordinate type, as every shape measure does; `count()` is the cardinality and stays a `std::size_t`. `bbox()` is exactly the box `asPolygonSet()` reports, read off the first and last set word of every row instead. `convexHull()` returns the hull of the region as a `Convex`.

- `asPolygonSet()` returns the covered region as a `PolygonSet`, one component per edge-connected group of cells, so it has no precondition and keeps two components touching only at a corner apart. It costs one pass over the words plus one over the boundary edges rather than a union of unit squares: the boundary loops are read straight off the words, which yields the outer ring and one ring per hole directly and drops the vertices in the middle of a straight stretch along the way, so a filled box comes back as four corners however many cells it holds. `asPolygonWithHoles()` returns a single `PolygonWithHoles` and throws `std::logic_error` unless the cells form one edge-connected group, which a rasterized region and its Minkowski sums are. `canvas << matrix` draws the matrix as one element by streaming its polygon set.

- The shape predicates read the cells as the closed squares they are. `samePointSet(b)` is `operator==` under its shape name. `contains(b)` holds when every cell of `b` is a cell of this matrix, and `interiorContains(b)` when none of them touches the boundary either, corners included — so it is `interior(GridAdjacency::vertex).contains(b)`. `intersects(b)` holds as soon as a cell of one matrix comes within Chebyshev distance one of a cell of the other, since two cells sharing only an edge or a corner already share points; `interiorsIntersect(b)` is the stricter question of a shared cell. `boundaryContains(b)` holds only for an empty `b`, a boundary being a curve and a nonempty matrix covering area.

- `operator&`, `operator|`, `operator^` and `difference(b)` are the set algebra, with `symmetricDifference(b)` spelling `^` out. Each returns the smallest window that provably loses no cell: the overlap of the two windows for an intersection, their hull for a union or a symmetric difference, the left window for a difference. The compound `&=`, `|=` and `^=` instead never move their window and drop whatever falls outside it, exactly as `set` does. and `andCount(b)`, `orCount(b)` and `xorCount(b)` give the three cardinalities without building the result.

- `operator~` is the complement *within the window*, not against the plane. It and `latticeMinkowskiErosion(b)`, which is built on it, are the only operations that compute from the window rather than only from the cells.

- The window is nonetheless part of a matrix's value. `operator==` and `operator<=>` compare the window along with the cells, as a shape's do for its stored representation, and `std::hash` agrees with them, so a matrix keys a `std::set`, a `std::map` and their unordered counterparts. The order is lexicographic on `(origin, width, height, bits)` and carries no geometric meaning. `samePointSet(b)` is the geometric question, and `trimmed()` is the canonical form to compare or hash by when only the region matters: `a != a.trimmed()` whenever trimming moves anything, while `a.samePointSet(a.trimmed())` always holds. Every window covering no cell has one canonical form, so all such matrices compare equal whatever origin they were built with.

- `translated(v)` moves the cells, spelled `a + v` or `v + a` for a point `v`, with `a - v` for the opposite and `+=` and `-=` in place, as for a shape. Translation reads the same under both hats. Scaling has no operator: multiplying the coordinates and stretching each cell into a block are both defensible readings of `a * n` and neither is the obvious one.

- `reflected()` (also `operator-`), `reflectedX()`, `reflectedY()`, `transposed()`, `rotated90(k)` and its in-place `rotate90(k)` are the symmetries **of the covered region**, so they commute with the conversion: `a.rotated90(k).asPolygonSet()` and `Transformation::rotation90(k) * a.asPolygonSet()` agree. A symmetry carries the square of cell `c` onto the square of another cell, which is not the square of the image of `c` as a point: `reflected()` maps cell `c` to `-c - (1,1)`. The `lattice`-prefixed `latticeReflected()`, `latticeReflectedX()`, `latticeReflectedY()`, `latticeRotated90(k)` and `latticeRotate90(k)` map the lattice points instead, so `latticeReflected()` maps `c` to `-c`; that is the reflection `latticeMinkowskiErosion` is dual to. Each pair differs by one cell in the directions it flips. Transposition is the exception: swapping the coordinates carries cell `(x, y)` to cell `(y, x)` either way, so `transposed()` and `latticeTransposed()` are the same operation.

- `latticeMinkowskiSum(b)` is $\\{a + b : a \in A, b \in B\\}$ over the cells **as lattice points**, over a window that is exactly the bounding box of the result. It costs one shifted or-assignment of the larger operand per cell of the smaller one, so a large region and a small structuring element are cheap. `latticeMinkowskiErosion(b)` is its dual $\\{p : p + B \subseteq A\\}$, over this window shrunk by the bounding box of `b`; eroding by a matrix with no cell is vacuously true and fills the window. `latticeOpening(b)` and `latticeClosing(b)` compose the two in both orders. This is the family to reach for in morphology: a one-cell matrix is the identity of the sum, and the erosion is an exact dual of it.

- `minkowskiSum(b)`, also spelled `a + b`, is instead the sum of the two **regions**, the one the shapes compute, so it commutes with the conversion: `(a + b).asPolygonSet()` and `a.asPolygonSet() + b.asPolygonSet()` agree. The unit square is not the identity of that sum — $U \oplus U$ is the two-by-two square $[0,2]^2$ — so this is `latticeMinkowskiSum(b)` dilated by the two-by-two block of cells that extra square covers, and comes out one cell wider and one cell taller in each direction.

- `minkowskiErosion(b)` is the region erosion, regularized exactly as `PolygonSet` regularizes its own, and it commutes with the conversion too. Regularizing is what keeps it on the grid: writing $B = B_{\mathrm{lat}} \oplus U$ splits the erosion into $(A \ominus B_{\mathrm{lat}}) \ominus U$, whose first step is the lattice erosion, while a *non-integer* $p$ has $p + U$ inside a cell region exactly when the two-by-two block of cells it straddles is — so the closure of the second step is the lattice erosion by that block, and the two compose into one. What regularizing drops is the lower-dimensional part: a cell eroded by a cell is the single point $p = 0$, which the shapes report as a degenerate `HalfplaneIntersection` and this reports as empty. Eroding by a matrix with no cell is the whole plane, and fills the window as the lattice erosion does.

- `interior(adjacency)` returns the set cells all of whose neighbors are set and `boundary(adjacency)` the rest of them, a cell on the window border always belonging to the boundary. `adjacency` is `GridAdjacency::edge` (4 neighbors) by default or `GridAdjacency::vertex` (8 neighbors).

- `connectedComponents(adjacency)` returns one trimmed matrix per connected group of cells, `componentCount(adjacency)` and `isConnected(adjacency)` count them, and `fillHoles(adjacency)` adds every group of unset cells that cannot reach outside. The background is always walked with the complementary adjacency, which is what keeps a diagonal chain of cells from both being connected and letting the background leak through it. `holeCount(adjacency)` counts what `fillHoles` would add, and `eulerNumber(adjacency)` returns components minus holes, read off the sixteen patterns a two-by-two block of cells can show rather than by a flood fill.

- `fillRows()` and `fillColumns()` close the gaps of every row or column and report whether anything changed; `makeHvConvex()` alternates them to a fixed point and returns how many cells it added, giving the smallest hv-convex superset. `isRowConvex()`, `isColumnConvex()` and `isHvConvex()` ask whether that fixed point is already reached.

- Other methods:
