// @desc: CGAL reference for the Minkowski sum category, over the same operands.
//
// pgl cuts one operand into Hertel–Mehlhorn convex pieces and leaves the other
// whole, sums the whole operand against each piece by their convolution, and
// unites those regions a few at a time. The reference is minkowski_sum_2 with
// Hertel–Mehlhorn convex pieces of both operands, CGAL's fastest decomposition
// here and the closest to pgl, whose pieces are the same ones.
//
// It reports the total number of boundary vertices, which is what pgl's driver
// reports, and must agree with pgl at every size.
#include "cgal.hpp"
#include "../sizes.hpp"

#include <CGAL/Polygon_convex_decomposition_2.h>
#include <CGAL/Polygon_with_holes_2.h>
#include <CGAL/minkowski_sum_2.h>

#include <span>

namespace {

using ConvexPieces = CGAL::Hertel_Mehlhorn_convex_decomposition_2<bench::cgal::Kernel>;

void sweepDataset(const bench::Options& opt, const char* dataset,
                  std::span<const int> sizes, bool bothSwept) {
    if (!bench::matches(opt.dataset, dataset)) return;
    if (!bench::matches(opt.problem, "Minkowski sum")) return;

    for (const int n : bench::sweep(sizes, opt)) {
        const auto a = bench::cgal::polygon(bench::randomPolygon(n, 1));
        const auto b = bench::cgal::polygon(
            bothSwept ? bench::randomPolygon(n, 2)
                      : bench::randomSmallPolygon(n, 2));

        long long convex = 0;
        const double convexUs = bench::timeOnce(convex, [&] {
            ConvexPieces pieces;
            return bench::cgal::vertexCount(CGAL::minkowski_sum_2(a, b, pieces));
        });
        bench::emit("Minkowski sum", dataset, "Minkowski sum",
                    "CGAL::minkowski_sum_2 (Hertel_Mehlhorn)", bench::cgal::kNumber,
                    n, convex, convexUs);
    }
}

}  // namespace

int main(int argc, char** argv) {
    const auto opt = bench::parseOptions(argc, argv);
    bench::header();
    sweepDataset(opt, "large + large", bench::kMinkowski, true);
    sweepDataset(opt, "large + small", bench::kMinkowski, false);
    return 0;
}
