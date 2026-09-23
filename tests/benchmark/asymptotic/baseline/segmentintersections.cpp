// @desc: CGAL reference for the Intersection of line segments category:
// CGAL's surface sweep over the same seven datasets.
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
// On the edges of the SBPD polygons it errs the other way, reporting crossings
// where two edges only share a vertex: 1 to 18 of them at every size of fpg,
// and one at 8 of spg's 13 sizes. The non-caching traits happen to get those
// counts right under EPICK, but are no faster there -- level on fpg, about 5%
// slower on spg -- so the traits stay chosen for speed, as below. On fpg-holes
// they are also the faster, and there the caching traits' count would be off
// by hundreds (724 false crossings at n = 9,993).
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

#include <CGAL/Arr_non_caching_segment_traits_2.h>
#include <CGAL/Surface_sweep_2_algorithms.h>

#include <span>
#include <type_traits>
#include <vector>

namespace {

// The sweep's traits, chosen per dataset for speed. The caching segment traits
// are CGAL's default and 4-8x faster wherever segments cross. The polygon edges
// barely meet, and there the non-caching traits are about 20% faster under
// EPECK; under EPICK they gain nothing there and crash on the crossing datasets.
// The SBPD polygons' edges are the same kind of input and take the same traits,
// except that on fpg-holes the non-caching traits are the faster under EPICK
// too, by about 7%, and take both kernels; under EPECK they are twice as fast
// there.
template <class K, class Traits = CGAL::Arr_segment_traits_2<K>, class Generate>
void sweepDataset(const bench::Options& opt, const char* dataset,
                  const std::vector<int>& sizes, Generate generate) {
    using Curve  = typename Traits::Curve_2;
    const Traits traits;
    const char* number = bench::cgal::numberName<K>;

    for (const int n : sizes) {
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
                                                  std::back_inserter(hits), true, traits);
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
                                                  std::back_inserter(hits), false, traits);
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
    const auto crossing = [&](const char* dataset, std::span<const int> sizes,
                              std::vector<bench::IntSegment> (*generate)(int)) {
        if (!bench::matches(opt.dataset, dataset)) return;
        sweepDataset<K>(opt, dataset, bench::sweep(sizes, opt), generate);
    };
    crossing("small",   bench::kSegmentsSmall, bench::smallSegments);
    crossing("sheared", bench::kSegmentsSmall, bench::shearedSegments);
    crossing("large",   bench::kSegmentsLarge, bench::largeSegments);

    // Polygon edges, which barely meet. `everywhere` takes the non-caching
    // traits under EPICK too.
    const auto edges = [&](const char* dataset, const std::vector<int>& sizes,
                           auto generate, bool everywhere = false) {
        if (everywhere || std::is_same_v<K, bench::cgal::Kernel>) {
            sweepDataset<K, CGAL::Arr_non_caching_segment_traits_2<K>>(
                opt, dataset, sizes, generate);
        } else {
            sweepDataset<K>(opt, dataset, sizes, generate);
        }
    };
    if (bench::matches(opt.dataset, "polygon edges")) {
        edges("polygon edges", bench::sweep(bench::kSegmentsPolygon, opt), bench::polygonEdges);
    }
    for (const char* dataset : bench::kSbpdDatasets) {
        if (!bench::matches(opt.dataset, dataset)) continue;
        edges(dataset, bench::sbpdSizes(dataset, bench::sweep(bench::kSegmentsPolygon, opt)),
              [dataset](int n) { return bench::edgesOf(bench::sbpdPolygon(dataset, n)); });
    }
    for (const char* dataset : bench::kSbpdRegionDatasets) {
        if (!bench::matches(opt.dataset, dataset)) continue;
        edges(dataset, bench::sbpdSizes(dataset, bench::sweep(bench::kSegmentsPolygon, opt)),
              [dataset](int n) { return bench::edgesOf(bench::sbpdRegion(dataset, n)); },
              true);
    }
}

}  // namespace

int main(int argc, char** argv) {
    const auto opt = bench::parseOptions(argc, argv);
    bench::header();
    run<bench::cgal::Inexact>(opt);
    run<bench::cgal::Kernel>(opt);
    return 0;
}
