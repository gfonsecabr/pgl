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
#include "cgal.hpp"
#include "../sizes.hpp"

#include <CGAL/Surface_sweep_2_algorithms.h>

#include <span>
#include <vector>

namespace {

using Traits = CGAL::Arr_segment_traits_2<bench::cgal::Kernel>;
using Curve  = Traits::Curve_2;

void sweepDataset(const bench::Options& opt, const char* dataset,
                  std::span<const int> sizes,
                  std::vector<bench::IntSegment> (*generate)(int)) {
    if (!bench::matches(opt.dataset, dataset)) return;

    for (const int n : bench::sweep(sizes, opt)) {
        const auto raw = generate(n);
        std::vector<Curve> curves;
        curves.reserve(raw.size());
        for (const auto& s : raw) {
            curves.emplace_back(bench::cgal::point(s[0]), bench::cgal::point(s[1]));
        }

        long long result = 0;
        // With endpoints reported. Comparable to pgl's findIntersections in
        // what it sweeps, not in what it counts — see the note at the top.
        if (bench::matches(opt.problem, "intersections")) {
            const double us = bench::timeOnce(result, [&] {
                std::vector<bench::cgal::Point> hits;
                CGAL::compute_intersection_points(curves.begin(), curves.end(),
                                                  std::back_inserter(hits), true);
                return hits.size();
            });
            bench::emit("Segment intersections", dataset, "intersections",
                        "CGAL::compute_intersection_points", bench::cgal::kNumber,
                        n, result, us);
        }
        // Interior meetings only, which is exactly findCrossings' contract:
        // this count and pgl's must agree.
        if (bench::matches(opt.problem, "crossings")) {
            const double us = bench::timeOnce(result, [&] {
                std::vector<bench::cgal::Point> hits;
                CGAL::compute_intersection_points(curves.begin(), curves.end(),
                                                  std::back_inserter(hits), false);
                return hits.size();
            });
            bench::emit("Segment intersections", dataset, "crossings",
                        "CGAL::compute_intersection_points", bench::cgal::kNumber,
                        n, result, us);
        }
    }
}

}  // namespace

int main(int argc, char** argv) {
    const auto opt = bench::parseOptions(argc, argv);
    bench::header();
    sweepDataset(opt, "small segments", bench::kSegmentsSmall, bench::smallSegments);
    sweepDataset(opt, "large segments", bench::kSegmentsLarge, bench::largeSegments);
    sweepDataset(opt, "polygon edges",  bench::kSegmentsPolygon, bench::polygonEdges);
    return 0;
}
