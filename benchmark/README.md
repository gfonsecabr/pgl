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
