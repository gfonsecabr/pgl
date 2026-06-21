<!-- AUTO-GENERATED from doc/raw/README.md by doc/raw/doxylink.py — do not edit; edit the raw version and regenerate. -->

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

Pangolin (or pgl) is a header-only C++ library for computational geometry algorithms in the plane. It is intended to be easy to use and efficient.

The [`Point`](https://gfonsecabr.github.io/pgl/structpgl_1_1Point.html "Two-dimensional point with optional label payload.") and [`Segment`](https://gfonsecabr.github.io/pgl/structpgl_1_1Segment.html "Unoriented closed segment between two endpoints plus optional segment label.") classes are two of [several shapes](shapes.md).

The coordinates are of type `int` by default, but [other types](types.md) including rational and floating point numbers may be used instead.

All shapes support [many predicates](shape_methods.md#predicates) and several other [methods and operators](shape_methods.md). Fundamental [algorithms](algorithms.md) and [data structures](data_structures.md) are also provided.

A [`Canvas`](https://gfonsecabr.github.io/pgl/classpgl_1_1Canvas.html "Stores drawable objects and exports them as an SVG image.") class is provided for [easy visualization](canvas.md).

As a header-only library with no dependency, you can compile your code directly with `g++` or `clang++` (the standard needs to be at least c++20):

```bash
g++ -std=c++23 -Iinclude/ -o example1 examples/example1.cpp
clang++ -std=c++23 -Iinclude/ -o example1 examples/example1.cpp
```
