# Examples

Complete programs using `pgl`, each one self-contained and buildable on its own.

```sh
make                 # build every example in this folder
make example_voronoi # build a single one
make clean           # remove the binaries
```

Every example writes its figures into the current directory. [`example1.cpp`](example1.cpp) produces no figure.

## Gallery

<table>
<tr>
<td width="50%" valign="top" align="center">
<a href="example2.cpp"><img src="figures/example2.svg" width="100%"/></a>
</td>
<td width="50%" valign="top" align="center">
<a href="example_canvas.cpp"><img src="figures/canvas_gallery.svg" width="100%"/></a>
</td>
</tr>
<tr>
<td valign="top"><a href="example2.cpp"><code>example2.cpp</code></a> — Streams shapes into a <a href="../doc/canvas.md"><code>Canvas</code></a> with <code>&lt;&lt;</code>, styling them with <code>pgl::stroke</code> and scaling a triangle by an integer factor.</td>
<td valign="top"><a href="example_canvas.cpp"><code>example_canvas.cpp</code></a> — Draws every bounded shape the library offers on one grid, and exports the same canvas as <a href="figures/canvas_gallery.svg">SVG</a>, <a href="figures/canvas_gallery.pdf">PDF</a> and <a href="figures/canvas_gallery.ipe">IPE</a>.</td>
</tr>

<tr>
<td width="50%" valign="top" align="center">
<a href="example_helloworld.cpp"><img src="figures/example_helloworld.svg" width="100%"/></a>
</td>
<td width="50%" valign="top" align="center">
<a href="example_enclosing.cpp"><img src="figures/example_enclosing.svg" width="100%"/></a>
</td>
</tr>
<tr>
<td valign="top"><a href="example_helloworld.cpp"><code>example_helloworld.cpp</code></a> — Letters built from segments, rectangles, polylines, disks, convex polygons and a half-plane intersection, mixing integer and exact-rational coordinates on a single canvas.</td>
<td valign="top"><a href="example_enclosing.cpp"><code>example_enclosing.cpp</code></a> — <a href="../doc/algorithms.md"><code>smallestEnclosingDisk</code></a> and <a href="../doc/shapes.md#convex"><code>Convex::smallestEnclosingRectangle</code></a> compute the minimum disk and smallest-area rectangle enclosing a set of points.</td>
</tr>

<tr>
<td width="50%" valign="top" align="center">
<a href="example_convex.cpp"><img src="figures/midpoint_polygon.svg" width="100%"/></a>
</td>
<td width="50%" valign="top" align="center">
<a href="example_minkowskisum.cpp"><img src="figures/example_minkowskisum.svg" width="100%"/></a>
</td>
</tr>
<tr>
<td valign="top"><a href="example_convex.cpp"><code>example_convex.cpp</code></a> — Iterates the midpoint map on a <a href="../doc/shapes.md#convex"><code>Convex</code></a> hull 100 times using <code>Rational&lt;BigInt&gt;</code> coordinates, so every edge midpoint stays exact.</td>
<td valign="top"><a href="example_minkowskisum.cpp"><code>example_minkowskisum.cpp</code></a> — <code>Polygon::minkowskiSum</code> grows a nonconvex polygon by a convex one, returning a <a href="../doc/shapes.md#polygon-with-holes"><code>PolygonWithHoles</code></a>.</td>
</tr>

<tr>
<td width="50%" valign="top" align="center">
<a href="example_triangulation.cpp"><img src="figures/example_triangulation.svg" width="100%"/></a>
</td>
<td width="50%" valign="top" align="center">
<a href="example_polygon_triangulation.cpp"><img src="figures/example_polygon_triangulation.svg" width="100%"/></a>
</td>
</tr>
<tr>
<td valign="top"><a href="example_triangulation.cpp"><code>example_triangulation.cpp</code></a> — Delaunay <a href="../doc/data_structures.md"><code>Triangulation</code></a> of random points, queried for the triangles a segment intersects and those it meets in the interior.</td>
<td valign="top"><a href="example_polygon_triangulation.cpp"><code>example_polygon_triangulation.cpp</code></a> — Constrained Delaunay triangulation of a spiral polygon with interior points, with the same intersection queries run against a convex window.</td>
</tr>

<tr>
<td width="50%" valign="top" align="center">
<a href="example_voronoi.cpp"><img src="figures/example_voronoi.svg" width="100%"/></a>
</td>
<td width="50%" valign="top" align="center">
<a href="example_mst.cpp"><img src="figures/example_mst.svg" width="100%"/></a>
</td>
</tr>
<tr>
<td valign="top"><a href="example_voronoi.cpp"><code>example_voronoi.cpp</code></a> — Takes the Voronoi dual of a Delaunay triangulation and answers nearest-site queries with <code>Arrangement::locateFace</code>, reading the site off the face label.</td>
<td valign="top"><a href="example_mst.cpp"><code>example_mst.cpp</code></a> — Hands a Delaunay triangulation over as a <a href="../doc/data_structures.md"><code>Graph</code></a> and grows the Euclidean minimum spanning tree with <code>spanningTree</code>.</td>
</tr>

<tr>
<td width="50%" valign="top" align="center">
<a href="example_visibility.cpp"><img src="figures/example_visibility.svg" width="100%"/></a>
</td>
<td width="50%" valign="top" align="center">
<a href="example_arrangement.cpp"><img src="figures/example_arrangement.svg" width="100%"/></a>
</td>
</tr>
<tr>
<td valign="top"><a href="example_visibility.cpp"><code>example_visibility.cpp</code></a> — <code>PolygonWithHoles::visibilityGraph</code> plus <code>Graph::shortestPath</code> give the shortest obstacle-avoiding path across a polygonal room.</td>
<td valign="top"><a href="example_arrangement.cpp"><code>example_arrangement.cpp</code></a> — Builds the <a href="../doc/data_structures.md"><code>Arrangement</code></a> of segments and lines, whose crossings are stored as exact rationals, and walks the boundary of its largest bounded face.</td>
</tr>

<tr>
<td width="50%" valign="top" align="center">
<a href="example_dual_arrangement.cpp"><img src="figures/example_dual_arrangement_dual.svg" width="100%"/></a>
</td>
<td width="50%" valign="top" align="center">
<a href="example_shapetree.cpp"><img src="figures/example_shapetree_triangles.svg" width="100%"/></a>
</td>
</tr>
<tr>
<td valign="top"><a href="example_dual_arrangement.cpp"><code>example_dual_arrangement.cpp</code></a> — Point-line <a href="../doc/shape_methods.md"><code>dual</code></a> turns "which points are collinear?" into a question about the arrangement of the dual lines, where a vertex of degree greater than four is exactly a line through three or more of the input points.</td>
<td valign="top"><a href="example_shapetree.cpp"><code>example_shapetree.cpp</code></a> — A <a href="../doc/data_structures.md"><code>ShapeTree</code></a> indexes random triangles, distinguishing those contained in a query triangle from those merely intersecting it.</td>
</tr>
</table>

