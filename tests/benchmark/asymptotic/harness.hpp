#pragma once
//
// Shared plumbing for the asymptotic benchmark drivers.
//
// Every driver sweeps a fixed list of input sizes (see sizes.hpp) and prints
// one tab-separated row per measured cell:
//
//   Category  Dataset  Problem  Algorithm  Number  Size  Result  Time(µs)
//
// `Result` is a numeric signature of the computed answer — an intersection
// count, a vertex count, a graph's edge count. It serves three purposes at
// once: it cross-checks the `int` run against the exact `ERational` run on the
// same input, it is what the CGAL baseline compares against, and for the
// output-sensitive categories it *is* the output size, so a curve that looks
// quadratic can be read as quadratic from the data rather than inferred from
// its shape.
//
// Times are microseconds. A construction row reports the whole construction; a
// query row reports the *mean over a fixed batch* of queries, so a query curve
// is per-query cost against n rather than batch cost against n. Which batch,
// and why it is the size it is, is below.
//
#include "pgl.hpp"
#include "../plf_nanotimer.h"

#include <cmath>
#include <concepts>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#if defined(__GLIBC__)
#include <malloc.h>
#endif

namespace bench {

// Queries per query-problem measurement.
//
// Fixed constants, like the sizes: never chosen from a runtime clock, so a
// given (problem, size) cell always averages over the same queries on every
// machine. A query row reports the mean over its batch, so the batch size does
// not change what the number means — only how finely it resolves and how long
// the sweep takes. `kQueryBatch` is the default, big enough that a
// sub-microsecond query still registers on the timer; `kSlowQueryBatch` is for
// problems whose *slowest* algorithm costs milliseconds a query, where a
// thousand of them would dominate the whole category's runtime for no extra
// precision.
//
// Every algorithm of one problem must use the same batch: they are compared
// against each other, and their result signatures cross-check only while they
// answer the same queries. Batches are always a prefix of the same query list,
// so the smaller batch is a subset of the larger.
constexpr int kQueryBatch = 1000;
constexpr int kSlowQueryBatch = 100;

// Queries per visibility measurement. Shorter still: one visibility query
// against exact coordinates costs milliseconds at the top of that sweep, so
// even the slow batch would make the one problem cost more than every other row
// of its category put together.
constexpr int kVisibilityQueries = 20;

// ---------------------------------------------------------------------------
// The heap
//
// glibc hands the free memory at the top of the heap back to the system once
// it passes a threshold, and it does so inside whichever free() crosses it. A
// sweep's large cells leave tens of megabytes free there, so the next cell to
// free a big enough block pays for returning all of it: one small-size cell of
// a segment sweep measured 2.6 times its neighbours that way. The heap is
// therefore never trimmed, and every cell is timed against a heap that only
// grows. Turning trimming off also freezes glibc's adaptive mmap threshold at
// its 128 KiB start, which would map and unmap every large buffer afresh, so
// that threshold is pinned where the adaptive one tops out on 64-bit.
//
// Every driver, the CGAL baseline's included, starts here through
// parseOptions, so both libraries are timed against the same heap.
// ---------------------------------------------------------------------------
inline void keepHeap() {
#if defined(__GLIBC__)
    mallopt(M_TRIM_THRESHOLD, std::numeric_limits<int>::max());
    mallopt(M_MMAP_THRESHOLD, 32 * 1024 * 1024);
#endif
}

// ---------------------------------------------------------------------------
// Command line
//
// The defaults are the whole point of the benchmark — a driver run with no
// arguments always measures exactly the sizes checked into sizes.hpp. The
// options exist for calibration and for narrowing a development loop; a
// recorded run never passes them.
// ---------------------------------------------------------------------------
struct Options {
    std::vector<int> sizes;   // --sizes 100,200,...  (overrides sizes.hpp)
    std::string problem;      // --problem SUBSTR
    std::string dataset;      // --dataset SUBSTR
    std::string type;         // --type int|ERational
};

inline std::vector<int> parseIntList(std::string_view s) {
    std::vector<int> out;
    while (!s.empty()) {
        const std::size_t comma = s.find(',');
        const std::string_view head = s.substr(0, comma);
        if (!head.empty()) {
            out.push_back(std::atoi(std::string(head).c_str()));
        }
        if (comma == std::string_view::npos) break;
        s.remove_prefix(comma + 1);
    }
    return out;
}

inline Options parseOptions(int argc, char** argv) {
    keepHeap();
    Options o;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        const auto value = [&]() -> std::string {
            if (i + 1 >= argc) {
                std::cerr << "missing value for " << arg << "\n";
                std::exit(2);
            }
            return argv[++i];
        };
        if (arg == "--sizes") o.sizes = parseIntList(value());
        else if (arg == "--problem") o.problem = value();
        else if (arg == "--dataset") o.dataset = value();
        else if (arg == "--type") o.type = value();
        else {
            std::cerr << "unknown option: " << arg << "\n"
                      << "usage: [--sizes N,N,...] [--dataset SUBSTR] "
                         "[--problem SUBSTR] [--type NAME]\n";
            std::exit(2);
        }
    }
    return o;
}

inline bool matches(const std::string& filter, std::string_view value) {
    return filter.empty() || value.find(filter) != std::string_view::npos;
}

// The sizes a driver should actually sweep: its own fixed list, unless the
// command line overrode it.
inline std::vector<int> sweep(std::span<const int> fixed, const Options& o) {
    if (!o.sizes.empty()) return o.sizes;
    return std::vector<int>(fixed.begin(), fixed.end());
}

// ---------------------------------------------------------------------------
// Number type names
//
// The dashboard's type axis is keyed on these, so they must match the keys
// run_shapepairs.py uses for the same types.
// ---------------------------------------------------------------------------
template <class Number> struct NumberName;
template <> struct NumberName<int>           { static constexpr const char* value = "int"; };
template <> struct NumberName<double>        { static constexpr const char* value = "double"; };
template <> struct NumberName<pgl::ERational> { static constexpr const char* value = "ERational"; };

template <class Number>
constexpr const char* numberName = NumberName<Number>::value;

// ---------------------------------------------------------------------------
// Output
// ---------------------------------------------------------------------------
inline void header() {
    std::cout << "Category\tDataset\tProblem\tAlgorithm\tNumber\tSize\tResult\tOutput"
                 "\tTime(µs)\n";
}

// A row carries two numbers, and they answer different questions.
//
// `result` is the verification signature: whatever value pins down that the
// computation happened and came out the same as every other way of doing it.
// Two algorithms of one problem must agree on it, an `int` run and an
// `ERational` run over the identical input must agree on it, and where CGAL
// solves the same problem its driver reports the same number. It is never
// shown: it exists to be compared, not read.
//
// `output` is the size of what the row is about, and it is what the dashboard
// plots when the x axis is switched off n. For most rows the two coincide --
// a count of intersections is both the answer and the size of the answer --
// and the short overload below is how a driver says so.
//
// They part company wherever the answer is not a size. A locate query returns
// one face however large the subdivision is; a closest pair returns a distance.
// A signature there has to be something else entirely -- how many queries
// landed inside, the squared length -- and reporting that as an output size
// puts a number on the chart that measures nothing. Such a row reports the size
// of the structure it is about, the same measure that structure's own build row
// reports, and keeps its check in `result`.
inline void emit(std::string_view category, std::string_view dataset,
                 std::string_view problem, std::string_view algorithm,
                 std::string_view number, int size, long long result,
                 long long output, double microseconds) {
    std::cout << category << '\t' << dataset << '\t' << problem << '\t'
              << algorithm << '\t' << number << '\t' << size << '\t'
              << result << '\t' << output << '\t' << microseconds << std::endl;
}

/** @brief A row whose answer is its own size, so the two numbers coincide. */
inline void emit(std::string_view category, std::string_view dataset,
                 std::string_view problem, std::string_view algorithm,
                 std::string_view number, int size, long long result,
                 double microseconds) {
    emit(category, dataset, problem, algorithm, number, size, result, result, microseconds);
}

// ---------------------------------------------------------------------------
// Preconditions
//
// A benchmark can measure the wrong thing silently: an index that was never
// built still answers queries, just by the slower path the row above already
// timed, and the two rows then differ by noise instead of by the thing being
// compared. Where a row's meaning depends on state the driver set up, it says
// so here rather than trusting the setup. Not an assert: these run under
// -DNDEBUG.
// ---------------------------------------------------------------------------
inline void require(bool condition, std::string_view what) {
    if (!condition) {
        std::cerr << "benchmark precondition failed: " << what << "\n";
        std::exit(3);
    }
}

// ---------------------------------------------------------------------------
// Timing
// ---------------------------------------------------------------------------

// A row's `Result` column, narrowed to an integer. Counts arrive as integers
// already; a coordinate-typed signature (a squared length, say) goes through
// double, which is lossless here because every dataset has integer coordinates
// and the signature is an integer value however it is stored.
template <class T>
    requires std::integral<T>
long long signature(const T& value) { return static_cast<long long>(value); }

template <class T>
    requires(!std::integral<T>)
long long signature(const T& value) { return std::llround(static_cast<double>(value)); }

// Runs `f` once and returns the elapsed microseconds. `f` returns the row's
// numeric signature, which is written through `result` — computing it inside
// the timed region is deliberate: it is what keeps the optimizer from eliding
// the work being measured.
template <class F>
double timeOnce(long long& result, F&& f) {
    plf::nanotimer timer;
    timer.start();
    result = signature(f());
    return timer.get_elapsed_us();
}

// ---------------------------------------------------------------------------
// Datasets
//
// Every dataset is generated with `int` coordinates and converted, never
// generated afresh per number type. Two reasons, both load-bearing: the
// generators draw integer coordinates regardless of the target type anyway, so
// generating in ERational only pays for slower arithmetic on the same numbers;
// and converting guarantees the int and ERational runs see the *identical*
// input, without which comparing their result signatures would prove nothing.
// ---------------------------------------------------------------------------
template <class Target, class Source>
std::vector<Target> convert(const std::vector<Source>& in) {
    std::vector<Target> out;
    out.reserve(in.size());
    for (const auto& s : in) {
        out.emplace_back(s);
    }
    return out;
}

}  // namespace bench
