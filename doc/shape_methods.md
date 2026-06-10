<img align="left" src="figures/logo.png" width="23%"/>

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="figures/logotextdark.svg"/>
  <img alt="Pangoling: Plane Geometry Library" src="figures/logotext.svg" width="65%"/>
</picture>

[![Tests](https://github.com/gfonsecabr/pgl/actions/workflows/tests.yml/badge.svg)](https://github.com/gfonsecabr/pgl/actions/workflows/tests.yml)
[![Standard](https://img.shields.io/badge/C%2B%2B-20/23/26-rgb(10,66,158).svg)](https://en.wikipedia.org/wiki/C%2B%2B#Standardization)
[![License](https://img.shields.io/badge/license-MIT-rgb(216,134,42).svg)](https://opensource.org/licenses/MIT)

<br/>

> ⚠️ **Work in Progress**: This library is still under construction and contains **bugs and missing features**. Use in production environments is not recommended.

## Methods Common to Most Shapes

### Predicates

Any two shapes `A`,`B` support the following [predicates](#predicates), where $\partial A$ denotes the manifold boundary of $A$. Notice that the boundary of a one-dimensional shape is defined as its endpoints (see also [shapes](shapes.md)).

- `A.contains(B)` Does `A` contain `B`, that is $A \supseteq B$?
- `A.boundaryContains(B)` Does the boundary of `A` contain `B`, that is $\partial A \supseteq B$?
- `A.interiorContains(B)` Does `A` contain `B`, that is $(A \setminus \partial A) \supseteq B$?
- `A.intersects(B)` Do the two shapes intersect, that is $A \cap B \neq \emptyset$?
- `A.interiorsIntersect(B)` Do the interior of the two shapes intersect, that is $(A \setminus \partial A) \cap (B \setminus \partial B) \neq \emptyset$? Attention: for segments and rays, the boundary $\partial S$ is defined as the endpoint(s).
- `A.separates(B)` Does $B \setminus A$ contain two or more connected components?
- `A.crosses(B)` Do both $A \setminus B$ and $B \setminus A$ contain two or more connected components?

The following table illustrate the result of the predicates for a triangle and a line segment.

| Predicate | <img width="100%" src="figures/predicate1.svg"/> | <img width="100%" src="figures/predicate2.svg"/> | <img width="100%" src="figures/predicate3.svg"/> | <img width="100%" src="figures/predicate4.svg"/> | <img width="100%" src="figures/predicate5.svg"/> | <img width="100%" src="figures/predicate6.svg"/> | <img width="100%" src="figures/predicate7.svg"/> | <img width="100%" src="figures/predicate8.svg"/> |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `A.contains(B)`           | ❌ | ❌ | ❌ | ❌ | ❌ | ✅ | ✅ | ✅ |
| `B.contains(A)`           | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ |
| `A.boundaryContains(B)`   | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ✅ |
| `B.boundaryContains(A)`   | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ |
| `A.interiorContains(B)`   | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ✅ | ❌ |
| `B.interiorContains(A)`   | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ |
| `A.intersects(B)`         | ❌ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| `A.interiorsIntersect(B)` | ❌ | ❌ | ✅ | ✅ | ✅ | ✅ | ✅ | ❌ |
| `A.separates(B)`          | ❌ | ❌ | ✅ | ❌ | ❌ | ❌ | ❌ | ❌ |
| `B.separates(A)`          | ❌ | ❌ | ✅ | ✅ | ❌ | ❌ | ❌ | ❌ |
| `A.crosses(B)`            | ❌ | ❌ | ✅ | ❌ | ❌ | ❌ | ❌ | ❌ |

All predicates are calculated exactly for integers (except for possible overflows detailed in [types](types.md).


### Operators

Shapes are translated by adding or subtracting a point. The point coordinates
are added to, or subtracted from, every defining point of the shape.

```c++
pgl::Point p = {2,3}, q = {4,5};
pgl::Segment s = {p, q},    //  s = (2,3)--(4,5)
             t1 = p + s,    // t1 = (4,6)--(6,8)
             t2 = s - p;    // t2 = (0,0)--(2,2)
```

In-place translations use `+=` and `-=`.
Scaling around the origin uses the operator `*` or `*=` with a scalar.

```c++
pgl::Segment s = {2, 3, 4, 5};    //  s = (2,3)--(4,5)
s += pgl::Point(1,2);             //  s = (3,5)--(5,7)
s *= 10;                          //  s = (30,50)--(50,70)
```

If we want to scale around a particular point `p`, we can use a combination of the previous operators:

```c++
pgl::Segment s = {2,3,4,5};   // s = (2,3)--(4,5)
pgl::Point p = s.midpoint();  // p = (3,4)
pgl::Segment t = 3*(s-p) + p; // t = (0,1)--(6,7)
```

### Intersection

The intersection of any two shapes may be calculated as follows. Note that the intersection of any two shapes is always an [`std::optional`](https://en.cppreference.com/w/cpp/utility/optional.html) since the two shapes may not intersect. Since the intersection may have different types that depend on the two shapes, we sometimes use an
[`std::variant`](https://en.cppreference.com/w/cpp/utility/variant.html). For example, the intersection of two segments may be a point or a segment. Furthermore, some shapes such as simple polygons may have disconnected intersections. In such cases, an [`std::vector`](https://en.cppreference.com/w/cpp/container/vector.html) with several objects is returned.

```c++
pgl::Segment s = {0,0,5,5}, t = {0,3,5,3};
auto isec = s.intersection(t);
// The type of isec here is std::optional<std::variant<pgl::Point,pgl::Segment>>
pgl::Point p = std::get<0>(*isec);
// p = (3,3)
```


### Other Methods for Shapes

- `rotated90(int k = 1)`: Returns the shape rotated by `90k` degrees around the
  origin.

- `rotate90(int k = 1)`: Rotates the shape by `90k` degrees around the origin.

- `scaledUpX(Number)`: Returns the shape with the x-coordinate multiplied by a
  number.

- `scaleUpX(Number)`: Multiplies the x-coordinate by a number.

- `scaledUpY(Number)`: Returns the shape with the y-coordinate multiplied by a
  number.

- `scaleUpY(Number)`: Multiplies the y-coordinate by a number.

- `scaledDownX(Number)`: Returns the shape with the x-coordinate divided by a
  number.

- `scaleDownX(Number)`: Divides the x-coordinate by a number.

- `scaledDownY(Number)`: Returns the shape with the y-coordinate divided by a
  number.

- `scaleDownY(Number)`: Divides the y-coordinate by a number.

- `squaredDistance(Shape)`: Returns the squared distance.

- `squaredHausdorffDistance(Shape)`: Returns the squared Hausdorff distance.

- `bbox()`: Returns the minimum bounding box of the shape.

- `fbox<T>()`: Returns a bounding box of the shape using floating point coordinates of type `T`. The bounding box may not be minimun but must contain the entire shape. The `min` coordinates are rounded down and the `max` are rounded up to the nearest floating point. If `!s1.fbox().intersects(s2.fbox()))` then `!s1.bbox().intersects(s2.bbox()))`. Also, if `s1.fbox().crosses(s2.fbox()))` then `s1.bbox().crosses(s2.bbox()))`.

- `area()`: Returns the area.

- `twiceArea()`: Returns two times the area.

- `diameter()`: Returns a segment that defines the diameter.

- `pointInside()`: Returns a point strictly in the interior of the shape. Uses
  only division by a power of 2.

- `verticesContain(p)`: Returns true if there exists a value `i` such that `s[i] == p` for the shape `s`. Notice that two shapes (for example lines) may be equal (according to `==`) but still behave differently for verticesContain if they are defined by different points.

## Iterating

There are several methods to iterate through vertices, edges, or oriented
edges. An [`std::array`](https://en.cppreference.com/w/cpp/container/array.html)
is used for shapes of constant size and an
[`std::vector`](https://en.cppreference.com/w/cpp/container/vector.html) is
used otherwise.

- `vertices()`: Returns an `std::array` or an `std::vector` of `Point` that are
  the vertices. 

- `edges()`: Returns an `std::array` or an `std::vector` of `Segment` that are
  the edges.

- `orientedEdges()`: Returns an `std::array` or an `std::vector` of
  `OrientedSegment` that are the edges in counterclockwise order. Not defined
  for `Disk`.

- `begin()`, `end()`, `edgesBegin()`, `edgesEnd()`, `orientedEdgesBegin()`,
  `orientedEdgesEnd()`: Same as `vertices()`, `edges()`, and
  `orientedEdges()` above, but for iterators that take `O(1)` time per element
  visited.

### Indexed access

Every shape exposes a uniform indexed-access interface over its defining
points (or, for `Point`, its two coordinates):

- `size()`: Returns the number of indexable elements.

- `s[i]`: Returns the `i`-th element.

- `s.get(i)`: Same as `s[i]` but `i` is taken modulo `s.size()`, so negative
  values wrap from the end.

- `s.index(p)`: Returns the smallest index `i` such that `s[i] == p`, or -1
  if no such index exists.

```c++
pgl::Convex c({{0,0},{4,0},{4,3},{0,3}});
c[2];           // (4,3)
c.get(-1);      // (0,3) same as c[3]
c.get(5);       // (4,0) same as c[1]
c.index({4,3}); // 2 since c[2] == {4,3}
```


The runtime `Shape` wrapper exposes `size()`, `operator[]`, and `get()` that
dispatch to the wrapped alternative. Because `Point`'s
indexed access yields a coordinate rather than a `Point`, `Shape::operator[]`
and `Shape::get` throw `std::logic_error` if the wrapped value is a `Point`.




