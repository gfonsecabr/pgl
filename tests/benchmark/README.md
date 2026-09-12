# Benchmarks

Pangolin's performance benchmarks live here. There are two kinds:

1. **The shape-pair cube** — `run_shapepairs.py` *generates* a small C++ program
   for every combination of

   ```
   shape1 × size1 × shape2 × size2 × method × number-type
   ```

   times it over 100×100 random shape pairs — 30×30 when either operand carries
   more than six vertices, since those calls cost microseconds to milliseconds —
   and compares the aggregate result against the exact `ERational` baseline so a
   type that disagrees can be flagged.
   No hand-written suite files — the shapes, sizes, methods and types are just
   lists at the top of the script.

2. **Asymptotic benchmarks** (`asymptotic/*.cpp`) — whole algorithms measured
   against growing input, for the things that don't fit the pair model
   (triangulation, Voronoi diagrams, arrangements, segment sweeps, spatial
   search, visibility, Minkowski sums, unions). One driver per **category**,
   each a small cube of

   ```
   dataset × problem × algorithm × number-type
   ```

   swept over ~32 fixed input sizes. The chart is time against size, so the
   thing being read off it is the *shape* of the curve. `run_asymptotic.py`
   compiles, runs and parses them.

   Three things are load-bearing about how they are set up:

   * **The sizes are fixed constants** (`asymptotic/sizes.hpp`), never probed at
     runtime. Two runs of the same commit have to measure the same x values or
     overlaying their curves means nothing.
   * **Every dataset is generated in `int` and converted.** The `int` and
     `ERational` runs then see the identical input, which is what makes
     comparing their result signatures a correctness check rather than a
     coincidence.
   * **Every row carries a numeric result signature**, not just a time. It
     cross-checks number types and algorithms against each other, it is what the
     CGAL baseline compares, and in the output-sensitive categories it is the
     record of how fast the output itself grows.

The interactive dashboard (filter by any dimension, auto-pivoting tables, per-commit
trend charts) is published at
<https://gfonsecabr.github.io/pgl/benchmarks/index.html>.
Its **Asymptotic** button opens the size-sweep page.

### The CGAL baseline

`asymptotic/baseline/*.cpp` measure the same problems on the same operands with
CGAL — one file per category, covering every problem CGAL has a direct analogue
for. A few do not have one and are left without a reference rather than given a
misleading one: closest pair, sort by angle, the visibility graph, and the
Voronoi diagrams — CGAL's `Voronoi_diagram_2` stores no subdivision, and its
`lower_envelope_3` builds one by lifting the sites into 3D, so neither races
what pgl does; nor is there an order-k or a farthest-point diagram in CGAL to
put against orders 2 and 4 or the farthest one. The rest exist to check pgl's
answers against something that is not more pgl, and to put a reference curve on the chart. A cell may carry more than one: CGAL solves the
Minkowski sum both by decomposition, the strategy pgl uses, and by reduced
convolution, which pgl has no counterpart for — the first says how pgl's
implementation of an idea compares, the second says what the other idea costs.
The chart draws every reference a cell has, one dash pattern each.

A cell may also carry a reference per *kernel*, and the chart picks the one the
selected number type is entitled to: EPICK against pgl's `int` column, EPECK
against `ERational`. Only the categories whose CGAL side never constructs a
point it later *tests* offer both — Point constructions, Point search, Segment
search, Triangulation and Visibility — where EPICK decides every predicate
exactly on this integer dataset and is what a CGAL user would actually reach
for. A category whose constructed points feed back into its own decisions
(Arrangement, Minkowski sum, Regularized union) has no honest EPICK curve to
record and stays on EPECK for both columns.

Segment intersections records both anyway, and labels the EPICK curve *inexact*
wherever it is drawn: that kernel drops intersection points at the larger sizes,
but pgl's `int` sweep is exact and has no inexact curve to put against it, so the
alternative was charging machine-word fractions against a lazy-exact kernel.
`baseline/cgal.hpp` carries the full rule and the evidence for it.

Opt-in only (`--baseline`), because CGAL is not on every dev machine or CI box.
Never appended to the history either: a baseline is a reference point rather
than a measurement of this repo at this commit, so it overwrites a single
`asymptotic-baseline.json` in the history.

## Where the history lives

The recorded history is **not** in this repository. It is a growing time series
— every run rewrites the files it touches — and keeping it here would make each
clone of the library carry every measurement ever taken. It lives in
[gfonsecabr/pgl-benchmarks](https://github.com/gfonsecabr/pgl-benchmarks)
instead, and the two are joined only where the history is read: locally by
`build_dashboard.py`, and in CI by the Pages workflow, which checks the data
repository out alongside this one.

Clone it beside your pgl checkout before recording anything:

```bash
git clone https://github.com/gfonsecabr/pgl-benchmarks.git ../pgl-benchmarks
```

`bench_paths.py` resolves the directory — `$PGL_BENCH_HISTORY` if set, else
`../pgl-benchmarks/history` — and every script that reads or writes history
takes `--history` to override it. Nothing else ties the repositories together:
each record carries the short SHA of the pgl commit it measured, and so does
each commit message in the data repository.

## Recording a run

`record.sh` runs the benchmarks, appends the results to the history, commits and
pushes them to the data repository, then dispatches this repository's Pages
workflow (via `gh`) to rebuild the dashboard.

What to run is named positionally — `pairs` (the shape-pair cube), `asymptotic`
(the whole-algorithm size sweeps), `baseline` (the CGAL reference for those
sweeps) — and at least one name is required: nothing runs by default.

```bash
bash tests/benchmark/record.sh pairs asymptotic     # the usual full run
bash tests/benchmark/record.sh pairs                # only the shape-pair cube
bash tests/benchmark/record.sh asymptotic           # only the size sweeps
bash tests/benchmark/record.sh baseline             # only refresh the CGAL reference
bash tests/benchmark/record.sh asymptotic baseline  # sweeps and their reference
bash tests/benchmark/record.sh asymptotic baseline --drivers triangulation
bash tests/benchmark/record.sh baseline --drivers triangulation,arrangement
bash tests/benchmark/record.sh pairs --shapes Segment,Triangle --methods intersects
bash tests/benchmark/record.sh pairs asymptotic --no-push   # commit to the data repo, don't push
```

`--drivers` limits the asymptotic and baseline runs to named drivers; baseline
categories outside the list are retained rather than dropped.

It refuses to run with uncommitted changes to tracked files — in either
repository — so every measurement maps to a real commit (the dashboard's x-axis
is the commit date).
Override the compiler/flags as usual: `CXX=g++ CXXFLAGS="-std=c++23 -O2" …`.

> The full cube is ~256 shape-size pairs × 18 methods × 5 number types — many
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

# Asymptotic benchmarks (one category or all):
python3 tests/benchmark/run_asymptotic.py triangulation
python3 tests/benchmark/run_asymptotic.py

# The CGAL baseline, which is never part of a default run:
python3 tests/benchmark/run_asymptotic.py --baseline-only

# Calibrating a size list: drivers take --sizes (and --dataset/--problem/--type)
# so one cell can be measured at candidate maxima. A run with --sizes is refused
# by to_history.py — its points would sit at x values no other run measured.
python3 tests/benchmark/run_asymptotic.py minkowskisum --sizes 80,160,320

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
| `run_asymptotic.py` | Compile/run the `asymptotic/*.cpp` size sweeps (and, with `--baseline`, the CGAL reference) → snapshot JSON |
| `to_history.py`     | Append snapshots into `history/` (one record per data point per commit) |
| `build_dashboard.py`| History → page-specific JSON payloads + copy the `dashboard/` frontend into the output dir |
| `record.sh`         | Orchestrate run → history → commit → push → dispatch the Pages rebuild |
| `bench_paths.py`    | Resolve where the history checkout is (shared by the scripts above) |
| `randomshapes.hpp`  | Deterministic random shape generators used by the generated sources |
| `legacy_untangle.hpp` | The pre-batching `Polygon::untangle()`, pinning the pair benchmark's polygon datasets to the shapes its recorded history was measured on |
| `dashboard/`        | Static frontend (`index.html`, `asymptotic.html`, `app.js`, `style.css`) |
| `history/` *(separate repo)* | Versioned JSONL: pair records at the top level, asymptotic under `history/asymptotic/`, plus the overwritten `asymptotic-baseline.json` |
| `asymptotic/`       | Size-sweep drivers, the fixed size lists (`sizes.hpp`), the shared harness, and the CGAL `baseline/` |
