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

[README](../README.md) | [User Guide](userguide.md) | [Shapes](shapes.md) | [Predicates](shape_methods.md#predicates) | [Shape Methods](shape_methods.md) | [Types](types.md) | [Algorithms and Data Structures](data_structures.md) | [Canvas](canvas.md)

# Project structure

- [Design principles](#design-principles)
- [Top-level map](#top-level-map)
- [Layered architecture](#layered-architecture)
- [How pgl.hpp is assembled](#how-pglhpp-is-assembled)
- [Geometry layer](#geometry-layer)
- [Implementation layer](#implementation-layer)
- [Tests and examples](#tests-and-examples)

Pangolin is a header-only plane geometry library. The public API is assembled by
`include/pgl.hpp`, while the implementation is distributed by mathematical
responsibility: geometry declarations live in one layer, cross-shape operations
in another, algorithms above them, and visualization beside them.

This page is meant to answer two practical questions:

1. Where does a given geometric concept live in the repository?
2. If I want to understand or change one mathematical behavior, which files
   actually control it?

---

## Design principles

- Exact geometry first: if the input coordinate type supports exact arithmetic,
  orientation, containment, intersection, and distance logic should not silently
  fall back to floating-point approximations.
- Dimension matters: 0-dimensional, 1-dimensional, and 2-dimensional objects
  are modeled explicitly, and the API tries to preserve those distinctions in
  predicates and return types.
- Topology is part of the contract: the difference between boundary,
  interior, separation, and crossing is not decorative documentation but a
  structural part of the code organization.
- Value semantics everywhere: shapes are small, comparable, hashable objects
  that can be copied, stored, sorted, and returned directly.
- Geometric ambiguity is typed, not hidden: if an intersection may be empty, a
  point, or a segment, the API uses `std::optional` and `std::variant`.
- Shared operations are grouped by family: instead of re-implementing
  containment or intersection logic independently inside each shape header,
  Pangolin centralizes those families in dedicated implementation headers.

## Top-level map

| Path | Role |
| --- | --- |
| `include/pgl.hpp` | Umbrella header that assembles the standard public Pangolin surface. |
| `include/core/` | Low-level numeric and type infrastructure: forward declarations, promotions, hashing support, and rational arithmetic. |
| `include/geometry/` | Public shape declarations and their contracts. |
| `include/implementation/` | Cross-shape template definitions grouped by mathematical family. |
| `include/algorithm/` | Geometry algorithms built on top of the public primitives. |
| `include/visualization/` | SVG-oriented drawing support through `Canvas`. |
| `tests/unit/` | Unit tests focused on one shape or one operation family at a time. |
| `tests/integration/` | Cross-shape tests through the public API. |
| `tests/graphical/` | Small visual programs for manually inspecting geometry and rendering. |
| `benchmark/` | Performance experiments and measurement support. |
| `sandbox/` | Exploratory programs that are not part of the public contract. |
| `doc/` | User-facing documentation. |
| `audit/` | Maintainer-facing audit and alignment material. |

## Layered architecture

At a high level, Pangolin is organized as the following dependency stack:

1. `core/`: number theory, promotion rules, rational arithmetic, hashing
   helpers, forward declarations.
2. `geometry/`: mathematical objects such as points, segments, lines, rays,
   halfplanes, triangles, rectangles, and the runtime `Shape` wrapper.
3. `implementation/`: operations on those objects, grouped by family:
   orientation, containment, topological predicates, coordinate evaluation,
   intersections, distances, measurements, transformations, duality, I/O, and
   bounding.
4. `algorithm/`: higher-level procedures such as Graham scan and Bentley-Ottmann.
5. `visualization/`: `Canvas` and SVG emission.

The important architectural point is that `geometry/` declares *what a shape is*
and `implementation/` defines *what families of operations mean on all shapes*.

## How `pgl.hpp` is assembled

`include/pgl.hpp` includes the public layers in a deliberate order:

1. numeric foundations:
   - `core/forward.hpp`
   - `core/numeric.hpp`
   - `core/rational.hpp`
2. primitive and finite shape declarations:
   - `geometry/*.hpp`
3. shared operation families:
   - `implementation/*.hpp`
4. optional but public auxiliary layers:
   - `visualization/canvas.hpp`
   - `core/hash.hpp`
   - `algorithm/*.hpp`

This order is not cosmetic. Many geometric formulas are expressed first in terms
of primitive orientations and collinearity, then lifted into predicates and
intersections, then reused by algorithms.

## Geometry layer

The `include/geometry/` directory contains the declarations of the currently
available shapes.

Each geometry header typically contains:

- the primary class template;
- type aliases such as `PointType`, `NumberType`, `EdgeType`;
- constructors and conversion constructors;
- accessors for defining points;
- declarations of member functions belonging to the public contract.

Mathematically, the shape headers answer questions such as:

- What data determines the object?
- What normalization invariant does it satisfy?
- What is the dimension and boundary convention of the object?
- Which other shapes can it convert to?

Examples:

- `Segment` is normalized by ordering endpoints lexicographically.
- `OrientedSegment` preserves source-target order.
- `Line` is unoriented and stores two defining points, but equality is
  geometric rather than purely representational.
- `Triangle` is stored by three canonical vertices, with the first vertex
  lexicographically minimal and the order chosen counterclockwise for
  non-degenerate triangles.
- `Rectangle` stores min/max corners but exposes polygonal structure.

## Implementation layer

The `include/implementation/` directory is the real operational core of the
library. Each header corresponds to a mathematical family of operations rather
than to one shape.

### Orientation and local geometry

| Header | Mathematical role |
| --- | --- |
| `orientation.hpp` | Orientation tests and sign logic for triples of points. This is the atomic predicate behind many planar decisions. |
| `atxy.hpp` | Coordinate evaluation on linear primitives, for example `yAtX` and `xAtY`. |

This layer is where local affine geometry begins: sidedness, collinearity,
vertical or horizontal evaluation, and directional comparison along a line.

### Set-theoretic and topological predicates

| Header | Mathematical role |
| --- | --- |
| `contains.hpp` | Closed-set containment. |
| `boundarycontains.hpp` | Boundary containment. |
| `interiorcontains.hpp` | Strict interior containment. |
| `intersects.hpp` | Non-empty intersection. |
| `interiorsintersect.hpp` | Interior intersection. |
| `separates.hpp` | Topological separation. |
| `crosses.hpp` | Mutual topological crossing. |
| `predicates_helpers.hpp` | Shared helper logic used by predicate families. |
| `predicates.hpp` | Aggregation point for the predicate family. |

These headers encode the distinction between:

- set inclusion versus interior inclusion;
- intersection versus interior intersection;
- mere contact versus topological crossing;
- geometric coincidence versus separation of connected components.

### Constructive geometry

| Header | Mathematical role |
| --- | --- |
| `intersection.hpp` | Typed intersection results: empty, point, segment, ray, line, rectangle, and so on, depending on the pair of shapes. |
| `distance.hpp` | Euclidean and derived distances between supported shapes. |
| `bounding.hpp` | Exact and floating bounding boxes, edge and vertex collections. |
| `measures.hpp` | Lengths, areas, centroids, diameters, and representative interior points. |
| `duality.hpp` | Projective dual and polar transforms. |

This is where the library turns predicates into actual constructed objects.

### Transformations and representation

| Header | Mathematical role |
| --- | --- |
| `transformations.hpp` | Translation, scaling, sign changes, and explicit conversions between related primitives. |
| `io.hpp` | String and stream representations of shapes. |

These files control how geometry moves and how it is presented.

## Tests and examples

The `tests/` directory is split by intent.

### Unit tests

`tests/unit/` contains one-file test targets such as:

- `point.cpp`
- `segment.cpp`
- `orientedsegment.cpp`
- `line.cpp`
- `orientedline.cpp`
- `ray.cpp`
...

These are the fastest way to answer:

- how one shape is expected to behave;
- how one operation family is expected to behave on that shape;
- which edge cases already have regression coverage.

### Integration tests

`tests/integration/` contains public-API scenarios across several shapes, for
example:

- `point_segment.cpp`
- `segment_segment.cpp`
- `boundingbox.cpp`

These files are useful when a behavior depends on more than one family at once,
for example normalization plus containment plus intersection.

### Graphical tests

`tests/graphical/` contains small rendering-oriented programs such as:

- `segment_segment.cpp`
- `segment_rectangle.cpp`
- `rectangle_segment.cpp`
- `duality.cpp`
- `bentleyottmann.cpp`

These are especially helpful for debugging geometric intuition when a predicate
is formally correct but visually surprising.
