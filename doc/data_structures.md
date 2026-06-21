<!-- AUTO-GENERATED from doc/raw/data_structures.md by doc/raw/doxylink.py — do not edit; edit the raw version and regenerate. -->

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

- [`countIntersecting(q)`](https://gfonsecabr.github.io/pgl/classpgl_1_1ShapeTree.html#ab9a97f8962f28bea8a93e59c7e959014) / [`countContainedIn(q)`](https://gfonsecabr.github.io/pgl/classpgl_1_1ShapeTree.html#aae23593e5f3cee436125f3287da6b4d5) return the number of matching stored shapes.

- [`reportIntersecting(q)`](https://gfonsecabr.github.io/pgl/classpgl_1_1ShapeTree.html#acfd0b343eb6267a71f22ef7f5adf6ac7) / [`reportContainedIn(q)`](https://gfonsecabr.github.io/pgl/classpgl_1_1ShapeTree.html#a74aaaaabb391d4828371fdd2a74b9215) return a vector with a copy of each matching stored shape.

- [`visitIntersecting(q, f)`](https://gfonsecabr.github.io/pgl/classpgl_1_1ShapeTree.html#a63c5008738d8300bd823a4288c421e50) / [`visitContainedIn(q, f)`](https://gfonsecabr.github.io/pgl/classpgl_1_1ShapeTree.html#a8c0868aea15e190a8e54895f7c0753fe) call `f(s)` on each matching stored shape `s` as it is found. If `f` returns `true` the visit stops.

- [`emptyIntersecting(q)`](https://gfonsecabr.github.io/pgl/classpgl_1_1ShapeTree.html#a8dce1815d2e4ac57b72dcc11ba22a898) / [`emptyContainedIn(q)`](https://gfonsecabr.github.io/pgl/classpgl_1_1ShapeTree.html#aa6e8876bbef51debe667a37b5086009a) return true if no stored shape matches.

- [`sumIntersecting(q)`](https://gfonsecabr.github.io/pgl/classpgl_1_1ShapeTree.html#ae05cf1c45fe7e9c0db0979e9c0fef5d6) / [`sumContainedIn(q)`](https://gfonsecabr.github.io/pgl/classpgl_1_1ShapeTree.html#a8809bb7ad02988ac7dcae36e71fb4d50) return the sum of a weight over the matching stored shapes. The weight is given by an optional `WeightFn` template parameter mapping a shape to any type with `operator+` (`ShapeTree<Shape, WeightFn>`); the weight function is passed to the constructor and ignored by default.

Sending a tree to a [Canvas](canvas.md) with `canvas << tree` draws all node bounding boxes. Is is possible to insert a new element with [`insert`](https://gfonsecabr.github.io/pgl/classpgl_1_1ShapeTree.html#a0ccc82dec95517590154bd038214445d), but no rebalancing is performed.

<p align="center">
  <img src="figures/example_shapetree_triangles.svg" alt="Shape tree range query over random triangles" width="50%"/>
  <br/>
  <em>A shape tree over 100 random triangles: the query triangle with the triangles it contains and intersects, plus the node bounding boxes.</em>
</p>


### Triangulation

[`Triangulation`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangulation.html) stores a mutable triangulation of either a fixed polygon or a fixed point set: the vertex coordinates are fixed at construction, only the connectivity changes. It may be constructed from a Polygon (constrained Delaunay triangulation), a container of points (Delaunay triangulation), segments, or triangles, always keeping labels. The polygon constructor optionally takes a container of extra interior points (added as vertices) and/or a container of interior segments (added as vertices and constrained edges); either may be omitted, and both are assumed to lie inside the polygon (not checked). Attention, the segments or triangles must define a valid triangulation (of the convex hull or any polygon), otherwise the behavior is undefined.

Construction and predicates are exact. For a polygon, the triangles between it and its convex hull are marked out-of-domain, so the public view — sizes, [`triangles`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangulation.html#adf13f4f217147acfad64a1c60294bbe0), [`edges`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangulation.html#a4d9f06c16b448a6094f121d3ff64675f), [`locate`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangulation.html#a29b96c32ebb52fddc7fd10eaeee4dbd8), … — describes exactly the polygon, including non-convex ones. The interface speaks only in value types ([`Point`](https://gfonsecabr.github.io/pgl/structpgl_1_1Point.html), [`Segment`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html), [`Triangle`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html)).

- [`locate(p)`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangulation.html#a29b96c32ebb52fddc7fd10eaeee4dbd8) returns a triangle containing point `p`, or none if `p` is outside, via a randomized visibility walk.

- Navigation: [`otherTriangle`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangulation.html#aa625a8ae9abbcedae55b710f7d14f256), [`adjacentTriangles`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangulation.html#a41527ab6a3bfcb23c964420b86e2d3d5), [`incidentTriangles`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangulation.html#a6a46217a54fcaf96c7bce5d3e9625eaa), the [`visitTriangles`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangulation.html#ae66b9330eca83b9768236c30f7fe064f)/[`visitEdges`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangulation.html#a199bd230951e914c4c169ab3283eaad0) visitors, and the sorted [`triangles()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangulation.html#adf13f4f217147acfad64a1c60294bbe0)/[`edges()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangulation.html#a4d9f06c16b448a6094f121d3ff64675f).

- Range searching: [`trianglesIntersecting(s)`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangulation.html#a0951e0ae30e494c3a336e99b1c214f20) return the triangles that satisfy `triangle.intersects(s)`. The function has several variantions [`visitTrianglesIntersecting(s,f)`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangulation.html#a7edb5aad94b2d0b8bebbe14b30708403) calls the function `f` on these triangles and stops early if `f` returns `true`. If `s` is an oriented segment, oriented line, or ray, the triangles are visited in order. The edge variations [`edgesIntersecting`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangulation.html#add516a5fb9be6e490e90d04334d8a95f) and [`visitEdgesIntersecting`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangulation.html#a0e42fd474c4661c8f7eaed01a7a13767) list the edges instead of the triangles. The `…InteriorIntersecting` variantions filter with `interiorIntersects(s)`.

- [`flip(e)`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangulation.html#ae3a9971db0a8fa7716b554650d67921d) replaces the diagonal shared by two triangles. It returns the new edge obtained or none if the flip cannot be performed (non-convex quadrilateral or the edge is constrained). [`flippable(e)`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangulation.html#ae91e0a2cede91a1c5e9c57c86128b935) simply returns if the flip can be performed without changing the triangulation. If we pass a container with edges in interior-disjoint quadrilaterals, the functions use parallel flips. [`flip`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangulation.html#ae3a9971db0a8fa7716b554650d67921d) is the only function that modifies the triangulation.

<p align="center">
  <img src="figures/example_triangulation2.svg" alt="Triangulation with a segment traversal highlighted" width="50%"/>
  <br/>
  <em>The constrained Delaunay triangulation of a polygon with points inside. Highlighting the triangles a segment meets and those whose interior it actually intersects.</em>
</p>



