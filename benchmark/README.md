<img align="left" src="../doc/figures/logo.png" width="23%"/>

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="../doc/figures/logotextdark.svg"/>
  <img alt="Pangolin: Plane Geometry Library" src="../doc/figures/logotext.svg" width="65%"/>
</picture>

[![Tests](https://github.com/gfonsecabr/pgl/actions/workflows/tests.yml/badge.svg)](https://github.com/gfonsecabr/pgl/actions/workflows/tests.yml)
[![Standard](https://img.shields.io/badge/C%2B%2B-20/23/26-rgb(10,66,158).svg)](https://en.wikipedia.org/wiki/C%2B%2B#Standardization)
[![License](https://img.shields.io/badge/license-MIT-rgb(216,134,42).svg)](https://opensource.org/licenses/MIT)
[![Benchmarks](https://img.shields.io/badge/benchmarks-online-rgb(21,153,135).svg)](https://gfonsecabr.github.io/pgl/benchmarks/index.html)

<br/>

> ⚠️ **Work in Progress**: This library is still under construction and contains **bugs and missing features**. Use in production environments is not recommended.

# Benchmarks

PGL keeps its performance benchmarks in `benchmark/<suite>/*.cpp`. The `benchmark/support/`
directory contains helpers and is not run by default.

Run the same benchmark pipeline as CI with:

```bash
bash benchmark/run_benchmark.sh
```

The runner builds each suite with `-O3 -DNDEBUG`, writes raw reports to
`build/benchmark/current-reports/`, and converts them to
`build/benchmark/benchmark.json`. That JSON is the input consumed by
`benchmark-action/github-action-benchmark`.

Useful environment variables:

- `CXX`: compiler to use, for example `g++` or `clang++`.
- `CXXFLAGS`: complete compiler flags.
- `PGL_BENCHMARK_REPETITIONS`: number of samples per suite. CI uses `3`.
- `PGL_BENCHMARK_SOURCES`: space-separated list of benchmark source files to run.
- `PGL_BENCHMARK_NUMBERS`: comma- or space-separated number families to run.
  Supported values are `int`, `double`, `rational`, and `rational60`.

The GitHub workflow publishes the history to the `gh-pages` branch under
`dev/bench`. GitHub Pages must be enabled for the repository, with `gh-pages` as
the Pages branch, before the dashboard is visible. If the branch does not exist
yet, create an empty `gh-pages` branch once before the first benchmark run.
