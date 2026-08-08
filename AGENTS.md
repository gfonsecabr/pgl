# Repository Guidelines

## Project Structure & Module Organization

Pangolin is a header-only C++ geometry library. Under `include/`, numeric foundations are in `core/`, shape declarations in `shape/`, cross-shape operations in `implementation/`, higher-level routines in `algorithm/`, and drawing support in `visualization/`. Change interactions such as Segment–Triangle containment in the relevant operation header (for example, `implementation/contains.hpp`), not a shape header. `include/pgl.hpp` is the umbrella header; preserve its dependency-aware include order.

Use `examples/` for small consumer programs and `doc/` for user-facing documentation. Automated doctest cases are in `tests/unit/`; `tests/graphical/`, `tests/interactive/`, and `sandbox/` are manual or exploratory. Benchmarks live in `tests/benchmark/`. Generated artifacts belong under `build/`.

## Build, Test, and Development Commands

- `make -C examples` builds every example with C++23; append a target such as `example1` to build one.
- `g++ -std=c++23 -Iinclude -o example examples/example1.cpp` compiles a consumer directly.
- `sh tests/run_tests.sh` compiles and runs the complete suite, matching CI behavior.
- `sh tests/run_tests.sh point` runs one test by basename; `--list` shows available tests.
- `CXX=clang++ sh tests/run_tests.sh` checks another compiler. CI covers GCC, Clang, and MSVC.
- `python3 tests/benchmark/run_extra.py [name]` runs all or one whole-algorithm benchmark.
- `doxygen Doxyfile` generates API documentation under `build/doc/`.

## Coding Style & Naming Conventions

Follow the surrounding modern C++20/23 style: four-space indentation, same-line braces, `#pragma once`, and standard-library facilities. Types use PascalCase (`PolygonWithHoles`); functions and variables use camelCase (`boundaryContains`); headers and tests use lowercase names such as `halfplane_polygon.cpp`. Keep predicates exact—do not introduce floating-point fallbacks—and preserve normalization, labels, equality, and hashing invariants. Represent possibly empty results with `std::optional` and shape-varying results with `std::variant`. There is no formatter or lint command; match adjacent code and compile with `-Wall -Wextra -pedantic`.

## Testing Guidelines

Add doctest cases to `tests/unit/<feature>.cpp`, using descriptive `TEST_CASE` or `TEST_CASE_TEMPLATE` names. Each source builds as its own binary; results go to `build/tests/bin/` and JUnit reports to `build/tests/reports/`. Cover normal, degenerate, boundary, and mixed-number-type behavior. No coverage threshold is enforced; behavioral changes should include a focused regression test. Run the targeted test, then the full suite.

## Commit & Pull Request Guidelines

Recent commits use concise, imperative, sentence-style subjects without prefixes (for example, `Remove degenerate elements earlier`). Keep each commit focused. Pull requests should explain the geometric contract affected, summarize the implementation and tests, link relevant issues, and include before/after images for visualization changes. Call out public API or generated-documentation changes explicitly; edit sources in `doc/raw/` when applicable rather than only generated `doc/*.md` files.
