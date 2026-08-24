// @desc: CGAL reference for the Constructions over a set of points category:
// convex_hull_2 over the same random points. The signature is the number of
// hull vertices, which is what pgl's convexHull returns, so the two are
// directly comparable.
//
// Only the hull. CGAL has no direct analogue of pgl's closestPair or
// sortAround as a single call, and its kd-tree is a different structure from
// pgl's ShapeTree with different build semantics, so those problems are left
// without a baseline rather than given a misleading one.
#include "cgal.hpp"
#include "../sizes.hpp"

#include <CGAL/convex_hull_2.h>

#include <iterator>
#include <vector>

int main(int argc, char** argv) {
    const auto opt = bench::parseOptions(argc, argv);
    bench::header();
    if (!bench::matches(opt.problem, "convex hull")) return 0;

    for (const int n : bench::sweep(bench::kConvexHull, opt)) {
        const auto pts = bench::cgal::points(bench::points(n));
        long long result = 0;
        const double us = bench::timeOnce(result, [&] {
            std::vector<bench::cgal::Point> hull;
            CGAL::convex_hull_2(pts.begin(), pts.end(), std::back_inserter(hull));
            return hull.size();
        });
        bench::emit("Point constructions", "points", "convex hull",
                    "CGAL::convex_hull_2", bench::cgal::kNumber, n, result, us);
    }
    return 0;
}
