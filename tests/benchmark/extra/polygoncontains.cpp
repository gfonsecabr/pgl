// @desc: Polygon-vs-polygon boundary predicates, chain decomposition vs red-blue
//        sweep. Star polygons of n vertices with a controlled share of inward
//        spikes, so the boundary's lexicographically monotone chain count sweeps
//        from 2 (convex) to ~n (maximally jagged) at a fixed vertex count. Three
//        placements of the second ring exercise the three shapes the predicates
//        take: nested with clear boundaries, nested but snug, and crossing.
//        Times are milliseconds per call.
//
// Every predicate here dispatches between the two strategies through
// pgl::preferSweep. Rebuilding with -DPGL_BOUNDARY_STRATEGY=1 forces the chain
// tests and =2 forces the sweep, which is how the dispatch's threshold was
// fitted: the row labels carry the strategy, so the three tables merge.
#include <cmath>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "pgl.hpp"
#include "../plf_nanotimer.h"

namespace {

using Point = pgl::Point<int>;
using PolygonShape = pgl::Polygon<Point>;

constexpr double kPi = 3.14159265358979323846;
constexpr double kRadius = 1000000.0;

#if PGL_BOUNDARY_STRATEGY == 1
constexpr const char* kStrategy = "chain";
#elif PGL_BOUNDARY_STRATEGY == 2
constexpr const char* kStrategy = "sweep";
#else
constexpr const char* kStrategy = "auto";
#endif

// A star-shaped ring of n vertices, every `period`-th one pulled deep inward.
// Each notch adds a pair of lexicographic direction reversals, so `spikePercent`
// drives the chain count without touching the vertex count.
PolygonShape starRing(int n, int spikePercent, double scale, double shift) {
    std::vector<Point> points;
    points.reserve(static_cast<std::size_t>(n));
    const int period = spikePercent == 0 ? 0 : std::max(2, 100 / spikePercent);
    const auto dx = static_cast<int>(std::llround(shift * kRadius));
    for (int i = 0; i < n; ++i) {
        const double angle = 2.0 * kPi * i / n;
        const bool spike = period != 0 && i % period == 0;
        const double radius = scale * kRadius * (spike ? 0.25 : 1.0);
        points.emplace_back(static_cast<int>(std::llround(radius * std::cos(angle))) + dx,
                            static_cast<int>(std::llround(radius * std::sin(angle))));
    }
    return PolygonShape(points);
}

std::string label(const char* strategy, const char* method, const char* placement, int n,
                  int spikePercent) {
    return std::string(strategy) + '.' + method + '.' + placement + ".n" + std::to_string(n) +
           ".j" + std::to_string(spikePercent);
}

// Times `call` over enough repetitions to be readable and prints one table row.
template <class Call>
void row(const std::string& name, int repetitions, Call call) {
    plf::nanotimer timer;
    bool result = false;
    timer.start();
    for (int i = 0; i < repetitions; ++i) {
        result = call();
    }
    const double elapsed = timer.get_elapsed_ms() / repetitions;
    std::cout << name << "\t\tint\t\t" << result << "\t" << elapsed << std::endl;
}

void grid(const char* placement, int n, int spikePercent, double innerScale, double shift) {
    const PolygonShape outer = starRing(n, spikePercent, 1.0, 0.0);
    const PolygonShape inner = starRing(n, spikePercent, innerScale, shift);
    const int repetitions = std::max(1, 20000 / n);

    std::cout << "# " << placement << " n=" << n << " spikes=" << spikePercent
              << "% chains=" << outer.chainCount() << "/" << inner.chainCount()
              << " sweep=" << pgl::preferSweep(outer, inner) << std::endl;

    // The four public predicates, each through the dispatch this build selects.
    row(label(kStrategy, "contains", placement, n, spikePercent), repetitions,
        [&] { return outer.contains(inner); });
    row(label(kStrategy, "intersects", placement, n, spikePercent), repetitions,
        [&] { return outer.intersects(inner); });
    row(label(kStrategy, "interiorContains", placement, n, spikePercent), repetitions,
        [&] { return outer.interiorContains(inner); });
    row(label(kStrategy, "interiorsIntersect", placement, n, spikePercent), repetitions,
        [&] { return outer.interiorsIntersect(inner); });

    // containment has a strategy-explicit entry point on each side, so the A/B
    // is available in one build and does not depend on the macro above.
    row(label("chainOnly", "contains", placement, n, spikePercent), repetitions,
        [&] { return outer.containsChainBased(inner); });
    row(label("sweepOnly", "contains", placement, n, spikePercent), repetitions,
        [&] { return pgl::sweepContains(outer, inner); });
}

}  // namespace

int main() {
    std::cout << std::setprecision(6);
    std::cout << "Operation\t\t\tNumber\t\tResult\tTime(ms)" << std::endl;

    // Nested with room to spare: contained, boundaries clear of each other, so
    // no predicate can exit early on a crossing.
    for (const int n : {64, 256, 1024, 4096}) {
        for (const int spikes : {0, 2, 5, 10, 25, 50}) {
            grid("nested", n, spikes, 0.5, 0.0);
        }
    }

    // Still nested, but at 96% of the radius the two boundaries run close
    // together across the whole plane.
    for (const int n : {256, 1024, 4096}) {
        for (const int spikes : {0, 50}) {
            grid("snug", n, spikes, 0.96, 0.0);
        }
    }

    // Overlapping: the boundaries cross, which every predicate can answer from
    // the first crossing it finds — the case the lazy chain decomposition is
    // best at and the sweep, which sorts every event up front, is worst at.
    for (const int n : {64, 256, 1024, 4096}) {
        for (const int spikes : {0, 50}) {
            grid("crossing", n, spikes, 0.9, 0.7);
        }
    }

    return 0;
}
