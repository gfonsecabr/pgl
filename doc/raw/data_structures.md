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

- Other methods:

<p align="center">
  <img src="figures/example_triangulation2.svg" alt="Triangulation with a segment traversal highlighted" width="50%"/>
  <br/>
  <em>The constrained Delaunay triangulation of a polygon with points inside. Highlighting the triangles a segment meets and those whose interior it actually intersects.</em>
</p>



