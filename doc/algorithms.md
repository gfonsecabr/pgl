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

## Algorithms

### Intersection of Line Segments

Given a container of $n$ line segments, we provide several functions to compute their intersections and crossings.

- `findIntersections(V)` returns all intersecting pairs of segments from the container `V` using the Bentley-Ottmann sweep-line algorithm. It runs in $O((n+k) \log n)$ time where $n$ is the input size and $k$ is the output size.

- `findCrossings(V)` returns all crossing pairs of segments from the container `V` using the Bentley-Ottmann sweep-line algorithm. It runs in $O((n+k) \log n)$ time where $n$ is the input size and $k$ is the output size.

- `bruteForceIntersections(V)` returns all intersecting pairs of segments from the container `V` using the naive brute force solution that verifies each pair. It takes $O(n^2)$ time but is faster when there are many intersections.

- `bruteForceCrossings(V)` returns all crossing pairs of segments from the container `V` using the naive brute force solution that verifies each pair. It takes $O(n^2)$ time but is faster when there are many crossings.

- `detectIntersections(V)` returns true if there are two intersecting segments in the container `V` using the Bentley-Ottmann sweep-line algorithm. It runs in $O(n \log n)$ time.

- `detectCrossings(V)` returns true if there are two crossing segments in the container `V` using the Bentley-Ottmann sweep-line algorithm. It runs in $O(n \log n)$ time.

These functions use the same predicate conventions documented in
[Predicates](shape_methods.md#predicates).

### Convex hull

- `convexHull(V)` returns the convex hull vertices in ccw order starting from the smallest (leftmost) point. Complexity $O(n \log n)$ for $n$ input points.

- `convexHullExtended(C)` returns the convex hull points in ccw order including vertices and points on edge interiors, starting from the smallest (leftmost) point. Complexity $O(n \log n)$ for $n$ input points.

