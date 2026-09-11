// @desc: CGAL reference for the Intersection of line segments category:
// CGAL's surface sweep over the same three datasets.
//
// Read the Result columns carefully: pgl counts *pairs*, CGAL counts distinct
// *points*, and how far apart that puts them depends on the problem.
//
// **crossings** is close to a real cross-check and is worth reading as one. On
// input in general position every crossing involves exactly one pair at exactly
// one point, and the two counts agree exactly — measured, they agree at every
// size up to n = 2,499 on the small-segment dataset, and at every size of the
// large-segment and polygon-edge datasets. Above that the counts drift apart by
// a handful (10 in 43,341 at n = 10,000), and the drift is accounted for: it
// tracks the number of pairs that *meet* without crossing transversally —
// collinear overlaps and endpoint-on-segment contacts — which pgl's
// findCrossings excludes by contract while CGAL still reports points along
// them. Both pgl algorithms and both number types agree with each other on
// every one of those rows, which is what says the difference is a definition
// and not an arithmetic error.
//
// **intersections** is a timing reference only. With endpoints reported CGAL
// additionally counts every segment end, which on n segments is n more points
// before any geometry is considered, so the columns are simply not the same
// quantity and a difference between them means nothing.
//
// ---------------------------------------------------------------------------
// The EPICK curve is an approximation, and is recorded anyway
// ---------------------------------------------------------------------------
//
// This sweep is the one place in the baseline where the kernel a column is
// entitled to (see cgal.hpp) and the kernel that answers correctly are not the
// same. A surface sweep re-inserts its constructed intersection points into the
// event structure and compares them against what follows, so rounding them
// makes CGAL's own decisions inconsistent, and EPICK loses points: measured
// over the sweep's 192 cells it disagrees with EPECK on 23 of them, all on the
// dense small-segment dataset, from −1 point at n = 3,749 up to −133 of 43,351
// crossings and −129 of 63,338 intersection points at n = 10,000.
//
// It is still recorded, because the alternative is worse. pgl's `int` sweep is
// exact — sweepFractionTier picks the narrowest fraction that stays exact, not
// a float — so it has no inexact mode of its own, and leaving this category
// EPECK-only charges pgl's machine-word fractions against a lazy-exact kernel
// in the `int` column and calls that like-for-like. Recording both lets the
// page show the honest thing instead: the EPICK curve for what CGAL's own
// `int`-strength kernel costs here, labelled inexact wherever it is drawn, and
// the EPECK curve for what the same answer pgl computes actually costs.
#include "cgal.hpp"
#include "../sizes.hpp"

#include <CGAL/Surface_sweep_2_algorithms.h>

#include <span>
#include <vector>

namespace {

template <class K>
void sweepDataset(const bench::Options& opt, const char* dataset,
                  std::span<const int> sizes,
                  std::vector<bench::IntSegment> (*generate)(int)) {
    if (!bench::matches(opt.dataset, dataset)) return;

    using Traits = CGAL::Arr_segment_traits_2<K>;
    using Curve  = typename Traits::Curve_2;
    const char* number = bench::cgal::numberName<K>;

    for (const int n : bench::sweep(sizes, opt)) {
        const auto raw = generate(n);
        std::vector<Curve> curves;
        curves.reserve(raw.size());
        for (const auto& s : raw) {
            curves.emplace_back(bench::cgal::point<K>(s[0]), bench::cgal::point<K>(s[1]));
        }

        long long result = 0;
        // With endpoints reported. Comparable to pgl's findIntersections in
        // what it sweeps, not in what it counts — see the note at the top.
        if (bench::matches(opt.problem, "intersections")) {
            const double us = bench::timeOnce(result, [&] {
                std::vector<typename K::Point_2> hits;
                CGAL::compute_intersection_points(curves.begin(), curves.end(),
                                                  std::back_inserter(hits), true);
                return hits.size();
            });
            bench::emit("Segment intersections", dataset, "intersections",
                        "CGAL::compute_intersection_points", number,
                        n, result, us);
        }
        // Interior meetings only, which is exactly findCrossings' contract:
        // this count and pgl's must agree — under EPECK. Under EPICK it is the
        // count that goes wrong, not the timing; see the note at the top.
        if (bench::matches(opt.problem, "crossings")) {
            const double us = bench::timeOnce(result, [&] {
                std::vector<typename K::Point_2> hits;
                CGAL::compute_intersection_points(curves.begin(), curves.end(),
                                                  std::back_inserter(hits), false);
                return hits.size();
            });
            bench::emit("Segment intersections", dataset, "crossings",
                        "CGAL::compute_intersection_points", number,
                        n, result, us);
        }
    }
}

template <class K>
void run(const bench::Options& opt) {
    if (!bench::cgal::selected<K>(opt)) return;
    sweepDataset<K>(opt, "small segments", bench::kSegmentsSmall, bench::smallSegments);
    sweepDataset<K>(opt, "large segments", bench::kSegmentsLarge, bench::largeSegments);
    sweepDataset<K>(opt, "polygon edges",  bench::kSegmentsPolygon, bench::polygonEdges);
}

}  // namespace

int main(int argc, char** argv) {
    const auto opt = bench::parseOptions(argc, argv);
    bench::header();
    run<bench::cgal::Inexact>(opt);
    run<bench::cgal::Kernel>(opt);
    return 0;
}
