// @desc: Polygon-in-polygon containment, chain decomposition vs red-blue sweep.
//        Star polygons of n vertices with a controlled share of inward spikes,
//        so the boundary's lexicographically monotone chain count sweeps from 2
//        (convex) to ~n (maximally jagged) at a fixed vertex count. The inner
//        polygon is the same ring at half the radius, so it really is contained
//        and both implementations have to look at every edge. Times are
//        milliseconds per call.
#include <cmath>
#include <iomanip>
#include <iostream>
#include <vector>

#include "pgl.hpp"
#include "../plf_nanotimer.h"

namespace {

using Point = pgl::Point<int>;
using PolygonShape = pgl::Polygon<Point>;

constexpr double kPi = 3.14159265358979323846;
constexpr double kRadius = 1000000.0;

// A star-shaped ring of n vertices, every `period`-th one pulled deep inward.
// Each notch adds a pair of lexicographic direction reversals, so `spikePercent`
// drives the chain count without touching the vertex count.
PolygonShape starRing(int n, int spikePercent, double scale) {
    std::vector<Point> points;
    points.reserve(static_cast<std::size_t>(n));
    const int period = spikePercent == 0 ? 0 : std::max(2, 100 / spikePercent);
    for (int i = 0; i < n; ++i) {
        const double angle = 2.0 * kPi * i / n;
        const bool spike = period != 0 && i % period == 0;
        const double radius = scale * kRadius * (spike ? 0.25 : 1.0);
        points.emplace_back(static_cast<int>(std::llround(radius * std::cos(angle))),
                            static_cast<int>(std::llround(radius * std::sin(angle))));
    }
    return PolygonShape(points);
}

// How many maximal lexicographically monotone chains the boundary splits into:
// one per vertex where the lexicographic direction reverses. This is exactly the
// count Polygon::BoundaryChains produces, and the quantity the chain-based
// containment test pays for quadratically.
std::size_t chainCount(const PolygonShape& poly) {
    const std::size_t n = poly.size();
    std::size_t breaks = 0;
    for (std::size_t i = 0; i < n; ++i) {
        const bool before = poly[(i + n - 1) % n] < poly[i];
        const bool after = poly[i] < poly[(i + 1) % n];
        if (before != after) {
            ++breaks;
        }
    }
    return breaks;
}

void row(const char* label, int n, int spikePercent, bool sweep, const PolygonShape& outer,
         const PolygonShape& inner, int repetitions) {
    plf::nanotimer timer;
    bool result = false;
    timer.start();
    for (int i = 0; i < repetitions; ++i) {
        result = sweep ? pgl::sweepContains(outer, inner) : outer.containsChainBased(inner);
    }
    const double elapsed = timer.get_elapsed_ms() / repetitions;

    std::cout << (sweep ? "sweep." : "chain.") << label << ".n" << n << ".j" << spikePercent
              << "\t\tint\t\t" << result << "\t" << elapsed << std::endl;
}

void grid(const char* label, int n, int spikePercent, double innerScale) {
    const PolygonShape outer = starRing(n, spikePercent, 1.0);
    const PolygonShape inner = starRing(n, spikePercent, innerScale);
    const int repetitions = std::max(1, 20000 / n);

    std::cout << "# n=" << n << " spikes=" << spikePercent << "% chains=" << chainCount(outer)
              << " contains=" << outer.contains(inner) << std::endl;
    row(label, n, spikePercent, false, outer, inner, repetitions);
    row(label, n, spikePercent, true, outer, inner, repetitions);
}

}  // namespace

int main() {
    std::cout << std::setprecision(6);
    std::cout << "Operation\t\t\tNumber\t\tResult\tTime(ms)" << std::endl;

    // The inner ring at half the radius: contained, boundaries clear of each
    // other, so neither implementation can exit early.
    for (const int n : {64, 256, 1024, 4096}) {
        for (const int spikes : {0, 2, 5, 10, 25, 50}) {
            grid("contains", n, spikes, 0.5);
        }
    }

    // The same rings at 96% of the radius: still contained, but the two
    // boundaries now run close together across the whole plane.
    for (const int n : {256, 1024, 4096}) {
        grid("snug", n, 50, 0.96);
    }

    return 0;
}
