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

## Shapes Not Yet Implemented

- [`Polyline`](#polyline) Polyline, also called a polygonal chain, possibly having self-intersections.

The x-monotone polyline (formerly sketched here as `PolyFunction`) is implemented as [`MonotoneChain`](shapes.md#monotone-chain).


### Polyline

The class template `Polyline` represents a polyline, also called a polygonal chain, polygonal curve, polygonal path, or piecewise linear curve. It can be constructed for any number of points in a container that must be given in the order they appear on the polyline. The vertices are accessed in order starting from the minimum extreme vertex (minimum x, breaking ties by minimum y). Internally, the polyline is stored as multiple x-monotone polylines for improved performance, to be built on the implemented [`MonotoneChain`](shapes.md#monotone-chain).

A polyline `P` has methods such as:

- `P.isDegenerate()`: Returns true if all vertices are equal.
- `P.isSimple()`: Returns true if the edges only intersect at the endpoints of consecutive edges. Takes $O(n \log n)$ time for $n$ edges.

## Partially Implemented Features

### L1 / LInf distance to and from Disk

`distanceL1` / `distanceLInf` (and `hausdorffDistanceL1` / `hausdorffDistanceLInf`) are implemented for every pair among Point, Segment, OrientedSegment, Line, OrientedLine, Ray, Halfplane, Rectangle, Triangle, Convex, MonotoneChain, and Polygon, plus `Disk`-to-`Point`. The remaining `Disk` pairs (`Disk` vs. `Segment`/`OrientedSegment`/`Line`/`OrientedLine`/`Ray`/`Halfplane`/`Rectangle`/`Triangle`/`Convex`/`Polygon`, and `Disk`-`Disk`) are not yet implemented: unlike the Euclidean case, there is no closed form for the L1/LInf distance from a point to a circle, so these require a numeric optimization (the point-to-disk primitives already use a coarse-scan-plus-golden-section search) extended to bracket the minimum over an unbounded or two-dimensional domain. That bracketing needs more careful derivation and testing than fit in the initial pass.

## Data Structures Not Yet Implemented

### Grid

### Arrangment

### Graph
