// @desc: Closest pair of points by divide and conquer. Datasets: 100k distinct
//        points spread over a disk of radius 5000, where the strip around each
//        split line is a handful of points; 20k points in a 3-column vertical
//        band, where a strip instead keeps holding a large fraction of the range
//        and the per-node sorts bite, approaching the O(n log^2 n) worst case
//        (*Band); and a 1k subset of the disk where the O(n^2) brute force is
//        still affordable (*Small vs closestPairBF, same input). The *NoCutoff
//        rows recurse all the way down to three points instead of stopping at the
//        tuned per-type brute-force threshold, so that pair of rows shows what
//        the cutoff buys. Result is the squared distance of the pair found — it
//        must agree across every method on a given dataset.
#include <cstddef>
#include <iostream>
#include <set>
#include <vector>

#include "pgl.hpp"
#include "../plf_nanotimer.h"
#include "../randomshapes.hpp"


#ifndef PGL_CLOSESTPAIR_POINTS
#  define PGL_CLOSESTPAIR_POINTS 100000
#endif

#ifndef PGL_CLOSESTPAIR_BAND_POINTS
#  define PGL_CLOSESTPAIR_BAND_POINTS 20000
#endif

#ifndef PGL_CLOSESTPAIR_BRUTEFORCE_POINTS
#  define PGL_CLOSESTPAIR_BRUTEFORCE_POINTS 1000
#endif

constexpr int kPoints = PGL_CLOSESTPAIR_POINTS;
constexpr int kBandPoints = PGL_CLOSESTPAIR_BAND_POINTS;
constexpr int kBrutePoints = PGL_CLOSESTPAIR_BRUTEFORCE_POINTS;

// A tall, narrow band of distinct points: three columns of a tall integer grid.
// The closest distance such a grid induces is 1, so the strip around the split
// line is one whole column — a third of the range, and all of it once the
// recursion is inside a single column. That is the regime where the per-node
// sort costs a full n log n instead of next to nothing, which is what turns the
// algorithm's O(n log n) into its O(n log^2 n) worst case. The columns also make
// the strip arrive in x-major order rather than accidentally sorted by y, which
// a single column of points would have been.
//
// The height is kept under 2^15 so that a squared ordinate difference stays
// inside a 32-bit integer: `pgl::Rational<int>` is deliberately never promoted,
// so it is the one coordinate type that has to hold the products itself.
constexpr int kBandWidth = 2;
constexpr int kBandHeight = 30000;

template <class Number>
std::vector<pgl::Point<Number>> bandPoints(int n) {
    using Point = pgl::Point<Number>;
    Rng rng{20240816};
    std::vector<Point> v;
    std::set<Point> seen;
    v.reserve(static_cast<std::size_t>(n));
    while (static_cast<int>(v.size()) < n) {
        const Point p(Number(rng.range(kBandWidth)), Number(rng.range(kBandHeight)));
        if (seen.insert(p).second) {
            v.emplace_back(p);
        }
    }
    return v;
}

// Reference implementation: every pair, compared in the same promoted type the
// divide and conquer uses, so all three report the exact same squared distance.
template <class Point>
pgl::Segment<Point> bruteForceClosestPair(const std::vector<Point>& points) {
    using Coordinate = pgl::detail::promoted_number_t<typename Point::NumberType>;
    std::size_t bestI = 0, bestJ = 1;
    auto best = points[0].template squaredDistance<Coordinate>(points[1]);
    for (std::size_t i = 0; i < points.size(); ++i) {
        for (std::size_t j = i + 1; j < points.size(); ++j) {
            const auto squared = points[i].template squaredDistance<Coordinate>(points[j]);
            if (squared < best) {
                best = squared;
                bestI = i;
                bestJ = j;
            }
        }
    }
    return pgl::Segment<Point>(points[bestI], points[bestJ]);
}

template <class Point>
auto squaredLength(const pgl::Segment<Point>& segment) {
    using Coordinate = pgl::detail::promoted_number_t<typename Point::NumberType>;
    return segment.min().template squaredDistance<Coordinate>(segment.max());
}

// Times one closest-pair computation and prints its row. The squared distance
// doubles as the result checksum, so a variant that disagrees is visible in the
// table rather than hidden in the timings.
//
// Each row is preceded by a discarded warm-up run. Several of these rows exist
// only to be compared against the row above them, and without the warm-up the
// one that happens to run first in the program pays for cold caches and a cold
// allocator — a systematic bias of a few percent, which is the same order as
// the differences being measured.
template <class Compute>
void report(const char* operation, const char* label, Compute compute) {
    compute();

    plf::nanotimer timer;
    timer.start();
    const auto found = compute();
    const double elapsed = timer.get_elapsed_ms();
    std::cout << operation << "\t" << label << "\t" << squaredLength(found) << "\t" << elapsed
              << std::endl;
}

template <class Number>
void run(const char* label) {
    using Point = pgl::Point<Number>;

    const auto points = randomPoints<Number>(kPoints);
    const std::vector<Point> small(points.begin(), points.begin() + kBrutePoints);
    const auto band = bandPoints<Number>(kBandPoints);

    report("closestPair", label, [&] { return pgl::closestPair(points); });

    // The same run with the recursion carried all the way down to three points,
    // which is what the tuned brute-force cutoff is worth on this input.
    report("closestPairNoCutoff", label, [&] { return pgl::detail::closestPairDriver<3>(points); });

    report("closestPairBand", label, [&] { return pgl::closestPair(band); });

    report("closestPairSmall", label, [&] { return pgl::closestPair(small); });
    report("closestPairSmallNoCutoff", label,
           [&] { return pgl::detail::closestPairDriver<3>(small); });
    report("closestPairBF", label, [&] { return bruteForceClosestPair(small); });
}

int main() {
    std::cout << "Operation\tNumber\tResult\tTime(ms)" << std::endl;

    run<int>("int");
    run<double>("double");
    run<pgl::Rational<>>("Rational");
    run<pgl::ERational>("ERational");

    return 0;
}
