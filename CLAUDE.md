# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this repo is

Pangolin (`pgl`) is a **header-only C++20+ library for plane computational geometry**. There is no build target for the library itself — consumers just `-Iinclude/ -std=c++20` (or newer) and `#include "pgl.hpp"`. Everything in `build/` is produced by the test or benchmark runner scripts.

## Common commands

```bash
# Run all unit + integration tests (CI uses this script verbatim)
sh tests/run_tests.sh

# Run one test (resolves by basename in tests/unit or tests/integration)
sh tests/run_tests.sh segment
sh tests/run_tests.sh segment_segment
sh tests/run_tests.sh tests/unit/rational.cpp   # also accepts paths

sh tests/run_tests.sh --list   # list available tests

# Override compiler / flags
CXX=clang++ sh tests/run_tests.sh
CXXFLAGS="-std=c++23 -O2 -Wall" sh tests/run_tests.sh

# Build a single example or graphical/sandbox program by hand
g++  -std=c++23 -Iinclude/ -o example1 examples/example1.cpp
clang++ -std=c++23 -Iinclude/ -o bo tests/graphical/bentleyottmann.cpp

# Run benchmarks
bash benchmark/run_benchmark.sh
PGL_BENCHMARK_SOURCES="benchmark/segment/foo.cpp" bash benchmark/run_benchmark.sh
PGL_BENCHMARK_NUMBERS="int,rational" bash benchmark/run_benchmark.sh
```

Test runner notes:
- Uses **doctest** (header bundled in `tests/doctest/doctest.h`); each `tests/{unit,integration}/*.cpp` is its own binary.
- Default flags are `-std=c++20 -Wall -Wextra -pedantic`. Tests must compile clean under both `g++` and `clang++` (CI runs both).
- Binaries land in `build/tests/bin/`; JUnit reports in `build/tests/reports/`.
- Files under `tests/graphical/` and `sandbox/` are **not** picked up by the runner — they are exploratory programs built manually.

## Architecture

The library is layered. `include/pgl.hpp` includes the layers in a deliberate order that mirrors the dependency stack; if you add a new header, place its `#include` in `pgl.hpp` at the right layer or downstream code will fail to compile.

1. **`include/core/`** — numeric infrastructure: `forward.hpp` (forward decls of all shape templates), `numeric.hpp` (type promotion rules), `rational.hpp` (exact rational arithmetic), `hash.hpp` (std::hash specializations, included late so all shapes are visible).
2. **`include/geometry/`** — one header per shape (`point.hpp`, `segment.hpp`, `line.hpp`, `triangle.hpp`, `rectangle.hpp`, `convex.hpp`, `halfplane.hpp`, …, plus `shape.hpp` which is a runtime-polymorphic `std::variant` wrapper over all shapes). Each header declares the class, type aliases (`PointType`, `NumberType`, `EdgeType`), accessors, and the **member-function signatures** of the public contract — but most cross-shape logic is *defined* in the implementation layer.
3. **`include/implementation/`** — this is where the bulk of the code lives. Files are organized **by mathematical operation family, not by shape**:
    - Predicates split into one file per relation: `contains.hpp`, `boundarycontains.hpp`, `interiorcontains.hpp`, `intersects.hpp`, `interiorsintersect.hpp`, `separates.hpp`, `crosses.hpp`, plus `predicates_helpers.hpp` and the aggregator `predicates.hpp`.
    - Constructions: `intersection.hpp` (typed results via `std::optional`/`std::variant`), `distance.hpp`, `bounding.hpp`, `measures.hpp` (length/area/centroid), `duality.hpp`.
    - Local geometry: `orientation.hpp` (the atomic sign predicate behind almost everything), `atxy.hpp` (`yAtX` / `xAtY`).
    - Representation: `transformations.hpp`, `io.hpp`.
    - `split_predicates.py` is a maintainer script that produced the per-relation split — not part of the build.
4. **`include/algorithm/`** — higher-level algorithms built on the primitives: `convexhull.hpp` (Graham scan), `intersections.hpp` (Bentley–Ottmann).
5. **`include/visualization/canvas.hpp`** — SVG emission; included via `pgl.hpp` but optional in spirit.

**Key consequence for editing:** to change how, say, "contains" behaves for `Segment` vs `Triangle`, you edit `implementation/contains.hpp`, not `geometry/segment.hpp` or `geometry/triangle.hpp`. The geometry headers only declare; the implementation headers define cross-shape operations as templates that overload on shape pairs.

### Key Design Principles

- **Exact geometry first**: integer arithmetic by default; `pgl::Rational<Int>` for fractional results. No silent floating-point fallback.
- **Topology is structural**: predicates distinguish boundary vs. interior explicitly (`Contains`, `BoundaryContains`, `InteriorContains`).
- **Value semantics**: all shapes are small, comparable, and hashable objects.
- **Geometric ambiguity is typed**: results use `std::optional` and `std::variant` — never null pointers or sentinel values.
- **Operations grouped by family**: cross-shape predicate logic lives in `include/implementation/`, not scattered across shape classes.
- **Generic numeric types**: shapes are templated on the numeric type (`int`, `double`, `Rational<int64_t>`, etc.).

### Design invariants worth knowing

- **Exact-by-default integer arithmetic.** Coordinates default to `int`; predicates must not silently downgrade to floating-point. When a construction genuinely needs a non-integer result (e.g. midpoint), the user explicitly requests the result type: `s.midpoint<pgl::Rational<int>>()`. Mirror this pattern for new constructive operations.
- **Topology is structural, not cosmetic.** `intersects` vs `interiorsIntersect`, `contains` vs `interiorContains` vs `boundaryContains`, `separates`, `crosses` — each has a distinct file and a distinct contract. Don't conflate them. `a.crosses(b)` means `a.separates(b) && b.separates(a)`.
- **Shapes have normalization invariants** that constructors enforce. Examples: `Segment` orders endpoints lexicographically; `OrientedSegment` does not; `Triangle` puts its lex-min vertex first and is CCW when non-degenerate; `Rectangle` stores min/max corners but exposes polygonal accessors. Equality and hashing assume normalization, so any new constructor must preserve it.
- **Value semantics throughout.** Shapes are small, copyable, comparable, hashable, usable directly in `std::set`/`std::unordered_set`.
- **Ambiguous results are typed**, not flagged: `std::optional` for "might be empty", `std::variant` for "could be a point or a segment or …".

### Test Structure

- `tests/unit/` — per-shape and per-operation unit tests
- `tests/integration/` — cross-shape public API tests
- `tests/graphical/` — visual debugging programs (SVG output via `include/visualization/canvas.hpp`)


### Documentation

`doc/` is the user-facing reference and is unusually thorough — consult it before guessing at API shape:
- `doc/project_structure.md` — the authoritative architectural map (this file is a condensed pointer to it).
- `doc/shapes.md`, `doc/predicates.md`, `doc/shape_methods.md`, `doc/types.md`, `doc/data_structures.md`, `doc/canvas.md`.

`olddoc/` and `olderdoc/` are archived and should be ignored.

## Untracked clutter to ignore

The working tree contains many untracked artifacts that are normal here and should not be committed unless explicitly requested: `.clangd` files in various dirs, ad-hoc `.cpp` files under `sandbox/`, generated `.svg` outputs under `tests/graphical/` and the repo root, and compiled test/sandbox binaries.
