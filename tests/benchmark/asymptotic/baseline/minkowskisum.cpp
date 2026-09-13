// @desc: CGAL reference for the Minkowski sum category, three of them, over the
// same operands.
//
// pgl mixes the two ideas CGAL keeps apart. It cuts one operand into
// Hertel–Mehlhorn convex pieces and leaves the other whole, sums the whole
// operand against each piece by their convolution, and unites those regions a
// few at a time. CGAL decomposes both operands and unites every pairwise convex
// sum, or convolves the two whole boundaries and decomposes neither.
//
// The first row is minkowski_sum_2 with a triangulation-based decomposition of
// both operands, the plainest form of the decomposition idea. The second is the
// reduced convolution method, which builds the convolution cycle of the two
// whole boundaries and extracts the sum from its arrangement; pgl convolves only
// against convex pieces, where the winding number alone decides the sum. The
// third is minkowski_sum_2 with Hertel–Mehlhorn convex pieces, CGAL's fastest
// decomposition here and the closest to pgl, whose pieces are the same ones.
//
// The reference races CGAL at its best, which is whichever row is fastest at a
// size: reduced convolution up to roughly n = 150 on these datasets, and the
// Hertel–Mehlhorn decomposition above that.
//
// All report the total number of boundary vertices, which is what pgl's driver
// reports. The CGAL rows must agree with each other and with pgl's at every
// size.
#include "cgal.hpp"
#include "../sizes.hpp"

#include <CGAL/Polygon_convex_decomposition_2.h>
#include <CGAL/Polygon_triangulation_decomposition_2.h>
#include <CGAL/Polygon_with_holes_2.h>
#include <CGAL/minkowski_sum_2.h>

#include <span>

namespace {

using Decomposition = CGAL::Polygon_triangulation_decomposition_2<bench::cgal::Kernel>;
using ConvexPieces  = CGAL::Hertel_Mehlhorn_convex_decomposition_2<bench::cgal::Kernel>;

void sweepDataset(const bench::Options& opt, const char* dataset,
                  std::span<const int> sizes, bool bothSwept) {
    if (!bench::matches(opt.dataset, dataset)) return;
    if (!bench::matches(opt.problem, "Minkowski sum")) return;

    for (const int n : bench::sweep(sizes, opt)) {
        const auto a = bench::cgal::polygon(bench::randomPolygon(n, 1));
        const auto b = bench::cgal::polygon(
            bothSwept ? bench::randomPolygon(n, 2)
                      : bench::randomSmallPolygon(n, 2));

        long long decomposed = 0;
        const double decomposedUs = bench::timeOnce(decomposed, [&] {
            Decomposition decomposition;
            return bench::cgal::vertexCount(
                CGAL::minkowski_sum_2(a, b, decomposition));
        });
        bench::emit("Minkowski sum", dataset, "Minkowski sum",
                    "CGAL::minkowski_sum_2", bench::cgal::kNumber, n, decomposed,
                    decomposedUs);

        long long convolved = 0;
        const double convolvedUs = bench::timeOnce(convolved, [&] {
            return bench::cgal::vertexCount(
                CGAL::minkowski_sum_by_reduced_convolution_2(a, b));
        });
        bench::require(convolved == decomposed,
                       "CGAL's Minkowski sums disagree on the vertex count");
        bench::emit("Minkowski sum", dataset, "Minkowski sum",
                    "CGAL::minkowski_sum_by_reduced_convolution_2",
                    bench::cgal::kNumber, n, convolved, convolvedUs);

        long long convex = 0;
        const double convexUs = bench::timeOnce(convex, [&] {
            ConvexPieces pieces;
            return bench::cgal::vertexCount(CGAL::minkowski_sum_2(a, b, pieces));
        });
        bench::require(convex == decomposed,
                       "CGAL's Minkowski sums disagree on the vertex count");
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
