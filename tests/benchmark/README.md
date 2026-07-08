# Benchmarks

Pangolin's performance benchmarks live here. There are two kinds:

1. **The shape-pair cube** — `run_shapepairs.py` *generates* a small C++ program
   for every combination of

   ```
   shape1 × size1 × shape2 × size2 × method × number-type
   ```

   times it over 100×100 random shape pairs, and compares the aggregate result
   against the exact `ERational` baseline so a type that disagrees can be flagged.
   No hand-written suite files — the shapes, sizes, methods and types are just
   lists at the top of the script.

2. **Whole-algorithm benchmarks** (`extra/*.cpp`) — self-contained drivers for
   things that don't fit the pair model (Bentley–Ottmann, triangulation, the
   shape tree, …). Each prints a tab-separated `Operation/Number/Result/Time`
   table; `run_extra.py` compiles, runs and parses them.

The interactive dashboard (filter by any dimension, auto-pivoting tables, per-commit
trend charts) is published at
<https://gfonsecabr.github.io/pgl/benchmarks/index.html>.

## Recording a run

`record.sh` runs the benchmarks, appends the results to the versioned history
under `history/`, commits and pushes. The Pages workflow then rebuilds the
dashboard from that history.

```bash
bash tests/benchmark/record.sh                 # full cube + all extra benchmarks
bash tests/benchmark/record.sh --pairs-only    # skip the extra benchmarks
bash tests/benchmark/record.sh --extra-only    # only the extra benchmarks
bash tests/benchmark/record.sh --shapes Segment,Triangle --methods intersects
bash tests/benchmark/record.sh --no-push       # commit locally, don't push
```

It refuses to run with uncommitted changes to tracked files, so every
measurement maps to a real commit (the dashboard's x-axis is the commit date).
Override the compiler/flags as usual: `CXX=g++ CXXFLAGS="-std=c++23 -O3" …`.

> The full cube is ~256 shape-size pairs × 11 methods × 5 number types — many
> thousands of programs to compile. Compilation is parallelised across all cores
> by default; narrow it with `--shapes/--sizes/--methods/--types` while iterating,
> or override with `--jobs N`.

## Running pieces by hand

```bash
# Shape-pair cube, a slice, written to a snapshot JSON (no history, no commit):
python3 tests/benchmark/run_shapepairs.py \
    --shapes Segment,Triangle --sizes small \
    --methods intersects,contains --types int,double,ERational \
    --jobs $(nproc)

# Extra benchmarks (one or all):
python3 tests/benchmark/run_extra.py bentleyottmann
python3 tests/benchmark/run_extra.py

# Append snapshots to history, then build the static site locally to preview:
python3 tests/benchmark/to_history.py
python3 tests/benchmark/build_dashboard.py --out /tmp/site && \
    python3 -m http.server -d /tmp/site
```

Run `python3 tests/benchmark/run_shapepairs.py --help` for the full option list.

## Files

| Path | Role |
| --- | --- |
| `run_shapepairs.py` | Generate/compile/run/compare the shape-pair cube → snapshot JSON |
| `run_extra.py`      | Compile/run the `extra/*.cpp` whole-algorithm benchmarks → snapshot JSON |
| `to_history.py`     | Append snapshots into `history/` (one record per data point per commit) |
| `build_dashboard.py`| History → `data.json` + copy the `dashboard/` frontend into the output dir |
| `record.sh`         | Orchestrate run → history → commit → push |
| `randomshapes.hpp`  | Deterministic random shape generators used by the generated sources |
| `dashboard/`        | Static frontend (`index.html`, `app.js`, `style.css`) |
| `history/`          | Versioned JSONL: pair records at the top level, extra under `history/extra/` |
| `extra/`            | Whole-algorithm benchmark sources |
