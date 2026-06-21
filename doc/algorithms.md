<!-- AUTO-GENERATED from doc/raw/algorithms.md by doc/raw/doxylink.py — do not edit; edit the raw version and regenerate. -->

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

- [`findIntersections(V)`](https://gfonsecabr.github.io/pgl/namespacepgl.html#adcd493466342b027a48fe7bf0718434b) returns all intersecting pairs of segments from the container `V` using the Bentley-Ottmann sweep-line algorithm. It runs in $O((n+k) \log n)$ time where $n$ is the input size and $k$ is the output size.

- [`findCrossings(V)`](https://gfonsecabr.github.io/pgl/namespacepgl.html#abf691f267558aaeea543045e723d292e) returns all crossing pairs of segments from the container `V` using the Bentley-Ottmann sweep-line algorithm. It runs in $O((n+k) \log n)$ time where $n$ is the input size and $k$ is the output size.

- [`bruteForceIntersections(V)`](https://gfonsecabr.github.io/pgl/namespacepgl.html#a390afa6b90488531f4702bc242322d46) returns all intersecting pairs of segments from the container `V` using the naive brute force solution that verifies each pair. It takes $O(n^2)$ time but is faster when there are many intersections.

- [`bruteForceCrossings(V)`](https://gfonsecabr.github.io/pgl/namespacepgl.html#aac3ae3a0e91834aef9525388948417c4) returns all crossing pairs of segments from the container `V` using the naive brute force solution that verifies each pair. It takes $O(n^2)$ time but is faster when there are many crossings.

- [`detectIntersections(V)`](https://gfonsecabr.github.io/pgl/namespacepgl.html#adea0ebb84e7d7ada3ae27ae23ea116bc) returns true if there are two intersecting segments in the container `V` using the Bentley-Ottmann sweep-line algorithm. It runs in $O(n \log n)$ time.

- [`detectCrossings(V)`](https://gfonsecabr.github.io/pgl/namespacepgl.html#ade9af54c89044d728daf207e9c534759) returns true if there are two crossing segments in the container `V` using the Bentley-Ottmann sweep-line algorithm. It runs in $O(n \log n)$ time.

These functions use the same predicate conventions documented in
[Predicates](shape_methods.md#predicates).

### Convex hull

- [`convexHull(V)`](https://gfonsecabr.github.io/pgl/namespacepgl.html#a3999bfdf73609b7ec708a4882fcaea2f) returns the convex hull vertices in ccw order starting from the smallest (leftmost) point. Complexity $O(n \log n)$ for $n$ input points.

- [`convexHullExtended(C)`](https://gfonsecabr.github.io/pgl/namespacepgl.html#ace788332cf5ee8db888decfb08383cda) returns the convex hull points in ccw order including vertices and points on edge interiors, starting from the smallest (leftmost) point. Complexity $O(n \log n)$ for $n$ input points.

### Sorting points

- [`sortAround(points, p)`](https://gfonsecabr.github.io/pgl/namespacepgl.html#aab7826153f78fb8c4468ad851564fd8f) reorders `points` in place counterclockwise around the center `p`, starting from the lexicographically smallest point and breaking ties by putting farther points first. Connecting the result in order traces a simple star-shaped polygon whose kernel contains `p`. Relies only on exact orientation and squared-distance comparisons. Complexity $O(n \log n)$ for $n$ points.

- [`hilbertSort(points)`](https://gfonsecabr.github.io/pgl/namespacepgl.html#a57def78cd131e9c518e478cafe93e137) reorders `points` in place along a Hilbert space-filling curve, so points close in the plane stay close in the sequence — a useful preprocessing step for incremental algorithms. Uses only coordinate comparisons (exact for integer coordinates). Complexity $O(n \log n)$ for $n$ points.

### Polyominoes

- `polyominoes<T>(size)` returns one `Polygon<Point<T>>` per free polyomino of `size` cells (counted up to translation, rotation, and reflection). Each polygon traces the polyomino boundary with small non-negative integer coordinates and is normalized like any other `Polygon`. Polyominoes that enclose a hole (possible from seven cells onward) are omitted, since their boundary is not a simple polygon. `T` defaults to `int`.

- `polyominoes<T>(n1, n2)` returns the free polyominoes of every size in `[n1, n2]`, smallest first.

- `polyominoesUpTo<T>(n)` returns the free polyominoes of every size from `1` to `n`, smallest first.

