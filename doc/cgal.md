<!-- AUTO-GENERATED from doc/raw/cgal.md by doc/raw/doxylink.py — do not edit; edit the raw version and regenerate. -->

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

## Comparison with CGAL

[CGAL](https://www.cgal.org/) is the reference implementation of computational
geometry in C++: decades of work, arbitrary dimensions, many kernels, and a
feature set pgl does not approach. It is the yardstick this page measures
against: what each library covers, how each one is built, how fast they run on
the problems both solve, and how much code a task takes to write.

### Scope

CGAL does far more than pgl, in more dimensions and over more kinds of
geometry. The overlap is the classical plane toolkit, and it is what the
[speed](#speed) tables measure.

#### What both have

Both libraries are header-only, templated on the number type, and exact when
that type is.

- **Primitives**: points, segments, lines, rays, triangles, axis-aligned
  rectangles, circles or disks, simple polygons, polygons with holes, and sets
  of regions.
- **Predicates and measurements** on them: the orientation and in-circle signs,
  point in polygon, simplicity, convexity, area, centroid, distances.
- **Convex hull** — [`convexHull`](https://gfonsecabr.github.io/pgl/namespacepgl.html#a3999bfdf73609b7ec708a4882fcaea2f "Computes the convex hull of a point container.") / `convex_hull_2`.
- **Delaunay and constrained Delaunay triangulation**, with point location —
  [`Triangulation`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangulation.html "Triangulation whose connectivity may change and whose vertex set may grow.") / `Delaunay_triangulation_2`,
  `Constrained_Delaunay_triangulation_2`.
- **Voronoi diagram of a point set** — `voronoiDiagram` / `Voronoi_diagram_2`.
- **Segment intersection by sweep, and the arrangement it builds**, with point
  location — [`findIntersections`](https://gfonsecabr.github.io/pgl/namespacepgl.html#adcd493466342b027a48fe7bf0718434b "Finds all intersecting segment pairs with Bentley-Ottmann.") and [`Arrangement`](https://gfonsecabr.github.io/pgl/classpgl_1_1Arrangement.html "The planar subdivision induced by a set of one-dimensional shapes.") / `compute_intersection_points`
  and `Arrangement_2`.
- **Regularized Boolean operations on polygonal regions** — `regularizedUnion`
  and its siblings / `General_polygon_set_2`.
- **Minkowski sum of polygons** — `minkowskiSum` / `minkowski_sum_2`.
- **Visibility by triangular expansion** — `regularizedVisiblePolygon` /
  `Triangular_expansion_visibility_2`.
- **Convex partition of a polygon** — `convexPartition`, within a factor of
  four of the fewest pieces / `Partition_2`, which also has the optimal and the
  y-monotone partitions.
- **Smallest enclosing disk, rectangle and slab** — [`smallestEnclosingDisk`](https://gfonsecabr.github.io/pgl/namespacepgl.html#ac8734297c4d99750062ee028e743d0d0 "Computes the smallest closed disk containing a set of points."),
  [`c.smallestEnclosingRectangle()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html#afde56d828ed9a8588eafdce8c1f89213 "Returns the smallest-area rectangle containing the convex polygon.") and [`c.smallestEnclosingSlab()`](https://gfonsecabr.github.io/pgl/structpgl_1_1Convex.html#a94b8fb9210ed90a5b355fd85a80f94e5 "Returns the narrowest slab containing the convex polygon.") /
  `Min_circle_2`, `min_rectangle_2` and `min_strip_2`.
- **Spatial search structures** — [`ShapeTree`](https://gfonsecabr.github.io/pgl/classpgl_1_1ShapeTree.html "Static shape tree of bounded shapes.") and [`IntervalTree`](https://gfonsecabr.github.io/pgl/classpgl_1_1IntervalTree.html "Mutable interval tree over the projection of bounded shapes.") / `Kd_tree`,
  `AABB_tree`, `Range_tree_2` and `Segment_tree`.

#### What CGAL has and pgl has not

- **Dimensions.** pgl is a plane library and nothing else. CGAL has 3D and
  arbitrary-dimensional kernels, triangulations, convex hulls, meshes,
  polyhedra and Boolean operations on them.
- **Curved geometry.** CGAL's circular kernel, `Arr_circle_segment_traits_2`,
  `Arr_conic_traits_2`, `Arr_Bezier_curve_traits_2` and
  `Arr_algebraic_segment_traits_2` put circular arcs, conics, Bézier curves and
  algebraic curves into arrangements and Boolean set operations. pgl's only
  curved shape is [`Disk`](https://gfonsecabr.github.io/pgl/structpgl_1_1Disk.html "Closed Euclidean disk stored by boundary points plus optional disk label."), which takes part in predicates, distances and
  Minkowski sums, but never in an arrangement or a Boolean operation.
- **Meshing and refinement.** `Mesh_2` refines a constrained Delaunay
  triangulation to a shape and size criterion, with 3D mesh generation and
  remeshing beside it. pgl triangulates and never refines.
- **Diagrams beyond the point Voronoi diagram.** Segment Delaunay graphs,
  Apollonius (additively weighted) diagrams, alpha shapes, lower and upper
  envelopes, periodic and hyperbolic triangulations.
- **Straight skeletons and offsets**, exact or to a requested error, which is
  what polygon rounding and inward offsetting go through.
- **Optimization.** Minimum enclosing ellipse, annulus and parallelogram, a QP
  solver, PCA, optimal bounding boxes. pgl stops at the disk, the rectangle and
  the slab above.
- **Point-set and shape processing.** Polyline simplification, snap rounding,
  polygon repair, Fréchet distance, natural-neighbor interpolation, barycentric
  coordinates, shape detection, classification, surface reconstruction.
- **Kernel engineering.** Filtered kernels with interval arithmetic, homogeneous
  kernels, algebraic number types, `Sqrt_extension`, and the freedom to plug a
  number type into a kernel and keep every algorithm. pgl's number types are a
  fixed short list and there is no filtering layer.
- **File formats.** WKT, DXF, OFF, STL, PLY, VTK and more, plus `CGAL::draw`
  viewers.

#### What pgl has and CGAL has not

Much shorter, and most of it is about how the plane case is packaged rather
than about reach.

- **One predicate vocabulary over every shape pair.** `contains`,
  `boundaryContains`, `interiorContains`, `intersects`, `interiorsIntersect`,
  `separates` and `crosses` are defined pair by pair across the seventeen shape
  types, with a documented set-theoretic meaning each. CGAL has `do_intersect`
  over kernel objects, `bounded_side` and `oriented_side` on `Polygon_2` for
  *points*, and `Boolean_set_operations_2` for 2D regions; there is no single
  containment-and-crossing vocabulary spanning shapes of mixed dimension.
- **[`Shape`](https://gfonsecabr.github.io/pgl/structpgl_1_1Shape.html "Runtime variant wrapper over the supported primitive shapes."), a runtime-polymorphic shape** that stores any alternative and
  answers the same predicates on it.
- **Labels carried by default.** Any point or shape can carry a label that
  survives through the algorithms that select shapes. CGAL does this per
  structure, with `Triangulation_vertex_base_with_info_2` and its relatives.
- **Minkowski erosion** by an arbitrary shape, [`a.minkowskiErosion(b)`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html#abfa245c0c11d3a95e899c2f1c60adc5a "Returns the Minkowski erosion of this shape by another (A ⊖ B)."), dual to
  the sum and defined for the same pairs. CGAL has the Minkowski sum; shrinking
  a polygon there goes through the straight skeleton, which is a different
  operation.
- **Visibility graphs.** `visibilityGraph`, `clearVisibilityGraph` and
  `reducedVisibilityGraph` on a polygon, a region or a triangulation. CGAL's
  `Visibility_2` computes visibility *regions*; its visibility graph lives
  inside `Partition_2` and is not part of the documented API.
- **[`HalfplaneIntersection`](https://gfonsecabr.github.io/pgl/structpgl_1_1HalfplaneIntersection.html "Intersection of closed half-planes; convex but possibly unbounded or empty.") as a shape**, so an unbounded convex region — a
  half-plane, a slab, a cone, the whole plane, the empty set — is a value that
  the predicates, the intersections and the Minkowski operations accept.
- **Graph algorithms in the library.** [`Graph`](https://gfonsecabr.github.io/pgl/classpgl_1_1Graph.html "Undirected simple graph stored as adjacency sets.") is keyed by whatever the
  geometry produced — points, vertex handles — and carries `spanningTree`
  (Prim), `shortestPath` (Dijkstra and A*), connected and biconnected
  components. [`Triangulation`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangulation.html "Triangulation whose connectivity may change and whose vertex set may grow."), [`Arrangement`](https://gfonsecabr.github.io/pgl/classpgl_1_1Arrangement.html "The planar subdivision induced by a set of one-dimensional shapes.") and the visibility graphs hand one
  back directly. CGAL adapts its structures to Boost.Graph instead, and the
  algorithms come from there.
- **Integer-lattice work.** `latticePoints`, [`BitMatrix`](https://gfonsecabr.github.io/pgl/classpgl_1_1BitMatrix.html "A bit per cell of a rectangular window of the integer grid.") rasterization, and a
  [`polyominoes`](https://gfonsecabr.github.io/pgl/namespacepgl.html#a9008f6bc68cdaae01e41b0e572127a43 "Enumerates the free polyominoes of a given size as polygons.") generator.
- **[`Canvas`](https://gfonsecabr.github.io/pgl/classpgl_1_1Canvas.html "Stores drawable objects and exports them as an SVG image.")**, which composes shapes of any types into one figure and writes it
  as SVG, PDF or Ipe with no GUI toolkit in the build.
- **`int` coordinates as the exact default**, with the promotion rules and
  overflow bounds documented in [types](types.md). CGAL reaches exact integer
  geometry through `Homogeneous<Exact_integer>` or a filtered kernel, and its
  filtered kernels get there faster than pgl does; what differs is which one you
  land on by writing nothing.

### Design differences

Neither library is a rearrangement of the other. The choices below run through
everything above.

- **Shapes are geometric concepts, not representations.** A pgl [`Triangle`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangle.html "Closed triangle stored by three vertices.") is
  the same triangle whatever order its vertices arrive in, and most shapes are
  unoriented; [`OrientedSegment`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedSegment.html "Directed segment preserving source-to-target order plus optional segment label.") and [`OrientedLine`](https://gfonsecabr.github.io/pgl/structpgl_1_1OrientedLine.html "Directed infinite line with left/right side semantics plus optional line label.") are the opt-in. CGAL's
  objects carry their orientation as part of the value.
- **Two points instead of an equation.** pgl stores a line, a ray or a
  half-plane as two points, and a [`Disk`](https://gfonsecabr.github.io/pgl/structpgl_1_1Disk.html "Closed Euclidean disk stored by boundary points plus optional disk label.") as three boundary points, so anything
  through integer points is exact in `int` with no rational coefficients.
  Equality and hashing canonicalize, so two lines defined by different pairs of
  points compare equal. CGAL stores the coefficients, which is what its exact
  constructions and its filtered kernels are for.
- **Number types per shape, and converted implicitly.** A shape carries its own
  number type and mixes with another in one expression, which is what makes it
  practical to reach for `ERational` only where it is needed. CGAL fixes the
  number type in the kernel, and the conversion between two kernels is the
  user's to write.
- **One [`Point`](https://gfonsecabr.github.io/pgl/structpgl_1_1Point.html "Two-dimensional point with optional label payload.").** pgl does not distinguish points from vectors and directions;
  CGAL's `Point_2`, `Vector_2` and `Direction_2` keep the affine distinction in
  the type system.
- **Predicates answer yes or no.** Where CGAL returns one of three sides from
  `bounded_side` or `oriented_side`, pgl gives the boundary and the interior
  their own named predicates and each returns a `bool`.
- **Values, not handles.** Every shape is comparable and hashable and goes
  straight into a `std::set` or `std::unordered_set`, and a [`Triangulation`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangulation.html "Triangulation whose connectivity may change and whose vertex set may grow.") is
  addressed by the points themselves rather than by handles into it. CGAL is
  navigated with handles and circulators throughout.
- **Monolithic, not modular.** One header and one set of conventions, against
  dozens of packages each with its own traits and concepts: less to learn, and
  much less to swap out.

### Practical differences

| | pgl | CGAL |
| --- | --- | --- |
| Dependencies | none | Boost headers; most builds also link GMP and MPFR |
| Building | `-Iinclude/` | header-only, usually driven through CMake |
| Standard | C++20 | C++17 |
| License | MIT | LGPLv3+ for the kernel and support packages, GPLv3+ for most algorithm packages, commercial licenses from GeometryFactory |
| Maturity | work in progress, with bugs and gaps | decades of releases, an editorial board, a large test suite and a user community |
| Bindings | [`pypgl`](https://github.com/gfonsecabr/pypgl) for Python, `ERational` constructions only | `cgal-swig-bindings` for Python and Java, over a subset of the packages |

Neither column is the better one in the abstract. A GPLv3+ package is free
software and a commercial license exists for those who need it; a dependency on
Boost and GMP is what buys CGAL its filtered exact arithmetic.

### Speed

The numbers come from the [asymptotic
benchmarks](https://gfonsecabr.github.io/pgl/benchmarks/asymptotic.html), where
the curves behind every ratio can be read directly. CGAL is the faster of the
two on most of the comparisons below, under either number type.

#### How the comparison is set up

- Both libraries are handed the identical input: the benchmark generates every
  dataset once, with `int` coordinates, and converts. The CGAL drivers live in
  `tests/benchmark/asymptotic/baseline/` beside the pgl ones.
- Each ratio is pgl time divided by CGAL time, taking the best algorithm each
  library offers at that size. **Below 1 means pgl is faster.**. The tables give
  the median over the 32 sizes of the sweep and, in parentheses, the full range
  — a single size can sit well outside the median, and where a ratio moves
  steadily with size that is called out under the table. A row covering more
  than one dataset averages the per-dataset medians, and its range spans them
  all.
- One run, one machine (Ryzen 9 9900X, `c++ -std=c++23 -O2 -DNDEBUG`, CGAL
  6.1.2), three repetitions per point. Treat a median under about 1.3 as a tie.
- Each pgl number type is measured against the CGAL kernel that matches it.
  Both of CGAL's are *filtered*: on integer input the interval arithmetic
  almost always settles a sign, so a predicate runs at close to `double` speed
  and falls back only when it must. What separates them is constructions.
  - `ERational` is measured against EPECK, exact predicates **and** exact
    constructions. That is the like-for-like pairing: both keep every
    constructed coordinate exact.
  - `int` is measured against EPICK, exact predicates with constructions
    rounded to `double`, wherever CGAL's side of the problem never constructs a
    point it later *tests* — convex hulls, Delaunay triangulations, kd-trees,
    AABB trees, and triangular-expansion visibility, whose window endpoints
    leave in the returned region but never re-enter a predicate. On this dataset
    EPICK decides every one of those predicates exactly (coordinates are
    integers under 10⁴, so the determinants stay far inside `double`'s exact
    range), so it is the same answer for less work, and it is the kernel a CGAL
    user would actually reach for. Measuring pgl's `int` tree against EPECK's
    `Lazy_exact_nt` coordinates — an interval and a DAG node per point — was
    charging CGAL for exactness the row never asked for.
  - Where a constructed point *does* feed back into CGAL's own decisions —
    arrangements and the segment sweep, which re-insert their intersection
    points, and Minkowski sums and Boolean unions, which arrange constructed
    curves — EPICK is not an option: it makes the algorithm inconsistent with
    itself. Measured, not assumed: run under EPICK the segment sweep disagrees
    with its own exact answer on 23 of the 96 cells of that category, by up to
    129 points in 63,338. EPECK is the baseline for both pgl columns there.
  - The claim is checked rather than trusted. The two kernels are separate cells
    of the benchmark cube and must report identical result signatures at
    identical sizes, so a category classified wrongly says so as soon as it is
    recorded.

  Neither pairing is perfect even so: pgl has no filter, so `ERational` pays for
  exact arithmetic unconditionally where EPECK usually avoids it, and `int`
  skips it because the coordinates make it unnecessary. Both comparisons are
  below.

#### Same arithmetic: `ERational` against EPECK

This is the like-for-like pairing, and CGAL wins most of it.

CGAL ahead:

| Problem | pgl / CGAL | pgl | CGAL |
| --- | --- | --- | --- |
| Segment search, count in Rectangle | 40× (30–44) | [`IntervalTree`](https://gfonsecabr.github.io/pgl/classpgl_1_1IntervalTree.html "Mutable interval tree over the projection of bounded shapes.") | `AABB_tree` |
| Segment search, count in Triangle | 24× (14–24) | [`ShapeTree`](https://gfonsecabr.github.io/pgl/classpgl_1_1ShapeTree.html "Static shape tree of bounded shapes.") | `AABB_tree` |
| Segment intersection | 9.8× (3.9–18) | [`findIntersections(v)`](https://gfonsecabr.github.io/pgl/namespacepgl.html#adcd493466342b027a48fe7bf0718434b "Finds all intersecting segment pairs with Bentley-Ottmann.") | `compute_intersection_points` |
| Delaunay triangulation | 8.1× (6.6–9.2) | [`Triangulation`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangulation.html "Triangulation whose connectivity may change and whose vertex set may grow.") | `Delaunay_triangulation_2` |
| Minkowski sum | 5.4× (2.2–14) | [`a.minkowskiSum(b)`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html#afe1664a4092da89cf4055a68eedf08ab "Returns the regularized Minkowski sum of the two shapes (A ⊕ B).") | `minkowski_sum_by_reduced_convolution_2` |
| Convex hull | 5.2× (4.2–5.3) | [`convexHull(v)`](https://gfonsecabr.github.io/pgl/namespacepgl.html#a3999bfdf73609b7ec708a4882fcaea2f "Computes the convex hull of a point container.") | `convex_hull_2` |
| Point search, count in Triangle | 4.8× (4.4–5.3) | [`ShapeTree`](https://gfonsecabr.github.io/pgl/classpgl_1_1ShapeTree.html "Static shape tree of bounded shapes.") | `Kd_tree::search` |
| kd-tree build | 3.4× (2.5–3.8) | [`ShapeTree`](https://gfonsecabr.github.io/pgl/classpgl_1_1ShapeTree.html "Static shape tree of bounded shapes.") | `Kd_tree` |
| Arrangement build | 2.9× (2.2–3.3) | [`Arrangement`](https://gfonsecabr.github.io/pgl/classpgl_1_1Arrangement.html "The planar subdivision induced by a set of one-dimensional shapes.") | `Arrangement_2` |
| Triangulation point location | 2.8× (1.7–3.1) | [`t.locate(p)`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangulation.html#a29b96c32ebb52fddc7fd10eaeee4dbd8 "Finds the triangle containing the query point by walking the mesh.") | `Triangulation_hierarchy_2::locate` |
| Regularized union, large + large | 2.7× (2.4–2.8) | [`a.regularizedUnion(b)`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html#a3066fa00a91b8fa642125c56b8a2d5b7 "Returns the regularized union of the two shapes (A ∪ B).") | `General_polygon_set_2::join` |

Roughly level:

| Problem | pgl / CGAL | pgl | CGAL |
| --- | --- | --- | --- |
| Segment search build | 1.1× (0.86–1.2) | [`IntervalTree`](https://gfonsecabr.github.io/pgl/classpgl_1_1IntervalTree.html "Mutable interval tree over the projection of bounded shapes.") | `AABB_tree` |
| Visibility, visible vertices | 0.9× (0.71–1.1) | [`t.visibleVertices(p)`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangulation.html#a3ec93b7700354398247e96c0ee9ba4db "The mesh vertices visible from query.") | `Triangular_expansion_visibility_2` |
| Arrangement point-location build | 0.88× (0.7–1.2) | [`a.buildPointLocation()`](https://gfonsecabr.github.io/pgl/classpgl_1_1Arrangement.html#af73b4d7888dfb82faaabaf0156b208a5 "Builds the randomized trapezoidal point-location index.") | `Arr_trapezoid_ric_point_location` |

pgl ahead:

| Problem | pgl / CGAL | pgl | CGAL |
| --- | --- | --- | --- |
| Arrangement point location query | 0.65× (0.38–1.1) | [`a.locateFace(p)`](https://gfonsecabr.github.io/pgl/classpgl_1_1Arrangement.html#a95d36d4248271458151cc640bf838704 "Returns the face containing a point.") | `Arr_trapezoid_ric_point_location::locate` |
| Nearest neighbor query | 0.55× (0.48–0.58) | [`t.nearestNeighbor(p)`](https://gfonsecabr.github.io/pgl/classpgl_1_1ShapeTree.html#ab474ac9db17611ead980ea1ff63f3446 "Returns the stored shape nearest to a query shape.") | `Orthogonal_k_neighbor_search` |
| Regularized union, triangles | 0.24× (0.22–0.75) | [`regularizedUnionOf(v)`](https://gfonsecabr.github.io/pgl/namespacepgl.html#ae72efa38504e74942758d2d4fb78ffcd "The regularized union of arbitrarily many shapes, as a set of regions.") | `General_polygon_set_2::join` |


#### pgl's default: `int` coordinates, against EPICK

pgl's predicates are exact for `int` — the orientation test and everything
built on it evaluate a determinant that fits, with no rounding and no fallback
— so integer input gives up nothing. Where the driver runs an `int` sweep, that
is the configuration a pgl user would actually reach for, and it costs roughly
5–10× less than `ERational`. Six of the categories have one; the Arrangement,
Minkowski sum and Regularized union categories do not, because their output is
constructed points that are not integers.

Dropping to `int` therefore buys pgl most of an order of magnitude — but it buys
CGAL a comparable amount, because the kernel this column is measured against
drops with it. EPICK stores plain `double` coordinates where EPECK stores a lazy
exact number per coordinate, and on the structures that only ever compare and
never construct, that is worth 12× on a kd-tree or an AABB tree build and over
40× on a nearest-neighbour query. So the gap narrows against EPECK's numbers
without reversing: CGAL is still ahead on most of these rows.

Two rows below are measured against EPECK rather than EPICK, and so are
unchanged from the pairing above — Segment intersection and Visibility, whose
CGAL side constructs geometry and so has no EPICK variant to offer. They are
the two places where pgl's `int` column races the exact kernel because there is
nothing else to race.

CGAL ahead:

| Problem | pgl / CGAL | pgl | CGAL |
| --- | --- | --- | --- |
| Delaunay triangulation | 4.3× (2.6–4.5) | [`Triangulation`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangulation.html "Triangulation whose connectivity may change and whose vertex set may grow.") | `Delaunay_triangulation_2` |
| kd-tree build | 3.7× (3.2–4.1) | [`ShapeTree`](https://gfonsecabr.github.io/pgl/classpgl_1_1ShapeTree.html "Static shape tree of bounded shapes.") | `Kd_tree` |
| Segment search build | 1.8× (1.5–2) | [`IntervalTree`](https://gfonsecabr.github.io/pgl/classpgl_1_1IntervalTree.html "Mutable interval tree over the projection of bounded shapes.") | `AABB_tree` |
| Convex hull | 1.7× (1.5–1.7) | [`convexHull(v)`](https://gfonsecabr.github.io/pgl/namespacepgl.html#a3999bfdf73609b7ec708a4882fcaea2f "Computes the convex hull of a point container.") | `convex_hull_2` |
| Segment intersection | 1.6× (1.2–2.1) | [`findIntersections(v)`](https://gfonsecabr.github.io/pgl/namespacepgl.html#adcd493466342b027a48fe7bf0718434b "Finds all intersecting segment pairs with Bentley-Ottmann.") | `compute_intersection_points` |
| Triangulation point location | 1.5× (1.2–2.1) | [`t.locate(p)`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangulation.html#a29b96c32ebb52fddc7fd10eaeee4dbd8 "Finds the triangle containing the query point by walking the mesh.") | `Triangulation_hierarchy_2::locate` |
| Nearest neighbor query | 1.5× (1.2–1.5) | [`t.nearestNeighbor(p)`](https://gfonsecabr.github.io/pgl/classpgl_1_1ShapeTree.html#ab474ac9db17611ead980ea1ff63f3446 "Returns the stored shape nearest to a query shape.") | `Orthogonal_k_neighbor_search` |

Roughly level:

| Problem | pgl / CGAL | pgl | CGAL |
| --- | --- | --- | --- |
| Segment search, count in Rectangle | 1.3× (0.83–1.4) | [`ShapeTree`](https://gfonsecabr.github.io/pgl/classpgl_1_1ShapeTree.html "Static shape tree of bounded shapes.") | `AABB_tree` |
| Segment search, count in Triangle | 0.81× (0.53–0.86) | [`ShapeTree`](https://gfonsecabr.github.io/pgl/classpgl_1_1ShapeTree.html "Static shape tree of bounded shapes.") | `AABB_tree` |

pgl ahead:

| Problem | pgl / CGAL | pgl | CGAL |
| --- | --- | --- | --- |
| Visibility, visible vertices | 0.34× (0.23–0.42) | [`t.visibleVertices(p)`](https://gfonsecabr.github.io/pgl/structpgl_1_1Triangulation.html#a3ec93b7700354398247e96c0ee9ba4db "The mesh vertices visible from query.") | `Triangular_expansion_visibility_2` |
| Point search, count in Triangle | 0.32× (0.27–0.35) | [`ShapeTree`](https://gfonsecabr.github.io/pgl/classpgl_1_1ShapeTree.html "Static shape tree of bounded shapes.") | `Kd_tree::search` |

#### What the numbers do not say

- **A pgl win over a specialized structure is usually specialization.** pgl's
  [`ShapeTree`](https://gfonsecabr.github.io/pgl/classpgl_1_1ShapeTree.html "Static shape tree of bounded shapes.") is a 2D `int` tree; CGAL's `Kd_tree` is dimension-generic over an
  arbitrary kernel, and `AABB_tree` is built for arbitrary primitives in 2D and
  3D. They are not attempting the same thing.
- **Some comparisons favor pgl by construction.** CGAL ships no triangle query
  item for `Kd_tree`, so the baseline wrote one — the "count in Triangle" rows
  race pgl against benchmark code driving CGAL, not against a CGAL facility.
- **Orthogonal range counting over points is deliberately absent.** pgl looks
  very good on it, but CGAL ships `Range_tree_2` for exactly that query and the
  benchmark does not run it; racing a kd-tree instead would prove nothing.

### How much code a task takes

Two libraries can both answer a question and still ask very different amounts of
code for it. The examples below are complete and were compiled and run against
pgl and CGAL 6.1.2; they cut in both directions.

#### Is a segment inside a polygon?

In pgl the pair is in the predicate table, so the question is the code:

```c++
#include "pgl.hpp"
#include <iostream>

int main() {
    pgl::Polygon<> room({0,0, 8,0, 8,3, 5,3, 5,6, 8,6, 8,9, 0,9});
    pgl::Segment<> a = {1,1, 1,8}, b = {1,1, 7,8};
    std::cout << room.contains(a) << room.contains(b);   // 10, exact in int
}
```

CGAL has every piece needed for the same answer, and each piece is exact and
more general than pgl's, but the predicate itself is not among them:
`Polygon_2` tests *points* against its boundary, and
`Boolean_set_operations_2` works on regions, which a segment is not. So the
test is assembled — cut the segment where the boundary crosses it, then ask
which side each piece is on:

```c++
#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <CGAL/Polygon_2.h>
#include <CGAL/intersections.h>
#include <algorithm>
#include <iostream>
#include <vector>

using K           = CGAL::Exact_predicates_exact_constructions_kernel;
using Point       = K::Point_2;
using Segment     = K::Segment_2;
using PolygonType = CGAL::Polygon_2<K>;

bool contains(const PolygonType &poly, const Segment &s) {
    std::vector<Point> cuts{s.source(), s.target()};
    for (auto e = poly.edges_begin(); e != poly.edges_end(); ++e) {
        auto obj = CGAL::intersection(s, *e);
        if (!obj) continue;
        if (const Point *p = std::get_if<Point>(&*obj))
            cuts.push_back(*p);
        else {                                    // the edge overlaps s
            const Segment &o = std::get<Segment>(*obj);
            cuts.push_back(o.source());
            cuts.push_back(o.target());
        }
    }
    auto along = [&](const Point &p) { return CGAL::squared_distance(s.source(), p); };
    std::sort(cuts.begin(), cuts.end(),
              [&](const Point &a, const Point &b) { return along(a) < along(b); });
    cuts.erase(std::unique(cuts.begin(), cuts.end()), cuts.end());
    for (std::size_t i = 0; i + 1 < cuts.size(); ++i)
        if (poly.bounded_side(CGAL::midpoint(cuts[i], cuts[i + 1]))
            == CGAL::ON_UNBOUNDED_SIDE)
            return false;
    return true;
}

int main() {
    PolygonType room;
    for (auto [x, y] : {std::pair{0,0},{8,0},{8,3},{5,3},{5,6},{8,6},{8,9},{0,9}})
        room.push_back(Point(x, y));
    Segment a(Point(1,1), Point(1,8)), b(Point(1,1), Point(7,8));
    std::cout << contains(room, a) << contains(room, b);   // 10, as above
}
```

The two agree on every case tried — the shared edge, the segment along a
boundary edge, the touched reflex vertex and the chord across the notch
included. The difference is not correctness or exactness — EPECK is exact
throughout — but that the CGAL user writes the overlap branch, the ordering
along the segment and the duplicate removal, and owns the edge cases in them.
The same holds for the neighboring questions: [`room.crosses(s)`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html#a2b23c4c3144eb5a06d76effe28e000e2 "Tests whether the two shapes mutually separate each other (each disconnects the other)."),
[`room.interiorContains(s)`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html#a28a4948401349722af11f2d72259c08f "Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B)."), [`room.intersection(s)`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html#a88086145b09c8647d5b195ab22999439 "Returns the intersection of the two shapes (A ∩ B), re-dispatching through the wrapper's own intersection."),
[`room.squaredDistance(s)`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html#a00f39592bd5e6febf12346cf4538ceb1 "Returns the squared Euclidean distance to the given shape.") and [`room.contains(disk)`](https://gfonsecabr.github.io/pgl/structpgl_1_1Polygon.html#a3a46d062f49f67213b208549e06dd23c "Tests whether this shape contains the other shape (A ⊇ B).") are single
calls in pgl, each its own assembly job in CGAL — `CGAL::intersection` has no
overload for a segment against a polygon either.

#### Rounding a polygon's corners

Here the sizes reverse. CGAL offsets a polygon by a radius, with a guaranteed
error bound, as a library call:

```c++
#include <CGAL/Lazy_exact_nt.h>
#include <CGAL/Gmpq.h>
#include <CGAL/Cartesian.h>
#include <CGAL/Polygon_2.h>
#include <CGAL/approximated_offset_2.h>
#include <iostream>

using K = CGAL::Cartesian<CGAL::Lazy_exact_nt<CGAL::Gmpq>>;

int main() {
    CGAL::Polygon_2<K> p;
    for (auto [x, y] : {std::pair{0,0},{4,0},{4,3},{0,3}})
        p.push_back(K::Point_2(x, y));
    auto out = CGAL::approximated_offset_2(p, 1, 0.0001);   // within 1e-4
    std::cout << out.outer_boundary().size() << " vertices\n";
}
```

`CGAL::offset_polygon_2` gives the same region exactly, as a general polygon
whose edges are genuine circular arcs. pgl has neither. The nearest thing is a
Minkowski sum with a polygon the caller draws in place of the disk, which
leaves the approximation and its error to the caller:

```c++
#include "pgl.hpp"
#include <cmath>
#include <iostream>

int main() {
    using P = pgl::Point<double>;
    pgl::Polygon<P> p({0,0, 4,0, 4,3, 0,3});
    std::vector<P> ring;                       // a disk of radius 1, by hand
    for (int i = 0; i < 32; ++i)
        ring.emplace_back(std::cos(2*M_PI*i/32), std::sin(2*M_PI*i/32));
    auto out = p.minkowskiSum(pgl::Convex<P>(ring));
    std::cout << out.outer().size() << " vertices\n";
}
```

Anything genuinely curved reads this way: pgl can approximate, and the
approximation is the caller's to justify.

#### Getting a picture out

Debug output is one call in pgl, takes as many shapes as the figure needs
whatever their types, and needs no toolkit:

```c++
pgl::Canvas canvas;
canvas << pgl::stroke("royalblue") << room
       << pgl::stroke("crimson")   << s << disk;
canvas.writeSVG("room.svg");
canvas.writeIPE("room.ipe");
```

CGAL's `CGAL::draw(obj)` shows one object in an interactive Qt6 window. Putting
several in the same picture goes through a `CGAL::Graphics_scene`:
`add_to_graphics_scene` for each object whose package defines the overload —
polygons, polygon sets, triangulations, arrangements, Voronoi diagrams, meshes,
point sets, and about a dozen more — then `draw_graphics_scene`. The
kernel primitives are not among them, so a segment or a triangle is added to
the scene element by element with `add_point`, `add_segment`, `add_ray`,
`add_line` or a `face_begin`/`face_end` block, and a disk has no primitive at
all to add. It is also a window rather than a file, and it needs the Qt6
package: without `CGAL_USE_BASIC_VIEWER` every draw call compiles to a line on
`stderr` saying it cannot draw. Writing a figure instead means a separate tool
or an Ipelet.

Against that, CGAL reads and writes WKT, DXF, OFF, STL, PLY and VTK, where pgl
parses no geometry at all and writes those three vector formats.

