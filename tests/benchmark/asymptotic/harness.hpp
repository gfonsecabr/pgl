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
#include <span>
#include <string>
#include <string_view>
#include <vector>

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
    std::cout << "Category\tDataset\tProblem\tAlgorithm\tNumber\tSize\tResult\tTime(µs)\n";
}

inline void emit(std::string_view category, std::string_view dataset,
                 std::string_view problem, std::string_view algorithm,
                 std::string_view number, int size, long long result,
                 double microseconds) {
    std::cout << category << '\t' << dataset << '\t' << problem << '\t'
              << algorithm << '\t' << number << '\t' << size << '\t'
              << result << '\t' << microseconds << std::endl;
}

// ---------------------------------------------------------------------------
// Region signatures
//
// For the categories whose answer is a region rather than a count — Minkowski
// sum, union — the signature is twice the region's area, not a vertex or
// component count. A vertex count is a property of how a boundary is
// *represented*: pgl keeps collinear boundary vertices where CGAL merges them,
// so two correct answers to the same question disagree on it. The area is a
// property of the region itself, so it is what the CGAL baseline can actually
// check, and it catches a wrong answer that a count would not.
//
// Twice the area, because the shoelace sum is halved to get an area and the
// doubling keeps whole coordinates on an integer footing. Both categories'
// results still have some genuinely rational vertices — a Minkowski sum's
// piece-sums have integer vertices but the union of those pieces puts vertices
// at crossings, and a union of two polygons does the same — so the reported
// signature is a *rounded* area. Read agreement as agreement to about eight
// significant digits: a difference of one in the last unit is the two doubles
// straddling a half, not a wrong answer.
//
// The shoelace sum is taken in double, over vertices converted one at a time,
// rather than by asking the shape for its exact twiceArea. pgl's rationals do
// not reduce as they add, so an exactly accumulated ring area is a fraction
// whose numerator and denominator each outgrow what a double can hold long
// before the division happens: the conversion gives inf/inf, and the signature
// comes out NaN. Converting each vertex first keeps every term small — a
// crossing's coordinate is a ratio of small polynomials in the inputs — and
// costs nothing. Measured on the union of 200 large triangles: the exact area
// takes 1.2 s and returns NaN where this returns 8.37e7 in under a
// millisecond, and on the inputs where the exact one does work (the union of
// two 3,200-vertex polygons, a 160-vertex Minkowski sum) the two agree to ten
// significant digits.
// ---------------------------------------------------------------------------
inline double doubledRingArea(const pgl::EPolygon& ring) {
    const auto& v = ring.vertices();
    double sum = 0;
    for (std::size_t i = 0; i < v.size(); ++i) {
        const auto& p = v[i];
        const auto& q = v[(i + 1) % v.size()];
        sum += static_cast<double>(p.x()) * static_cast<double>(q.y()) -
               static_cast<double>(q.x()) * static_cast<double>(p.y());
    }
    return std::abs(sum);
}

inline double doubledArea(const pgl::EPolygonWithHoles& region) {
    double total = doubledRingArea(region.outer());
    for (const auto& hole : region.holes()) {
        total -= doubledRingArea(hole);
    }
    return total;
}

inline double doubledArea(const pgl::EPolygonSet& set) {
    double total = 0;
    for (const auto& component : set.components()) {
        total += doubledArea(component);
    }
    return total;
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
