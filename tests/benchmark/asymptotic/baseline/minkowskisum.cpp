// @desc: CGAL reference for the Minkowski sum category, over the same operands.
//
// pgl cuts one operand into Hertel–Mehlhorn convex pieces and leaves the other
// whole, sums the whole operand against each piece by their convolution, and
// unites those regions a few at a time. The reference is minkowski_sum_2 with
// Hertel–Mehlhorn convex pieces of both operands, CGAL's fastest decomposition
// here and the closest to pgl, whose pieces are the same ones.
//
// Against the fixed small convex operand pgl skips the decomposition and
// arranges the two boundaries' convolution directly. There the reference is
// CGAL's reduced convolution, the fastest at every size of the sweep against
// full convolution, Hertel–Mehlhorn pieces of both operands, and decomposing
// only the polygon (Hertel–Mehlhorn, small-side angle bisector, vertical,
// triangulation, each with Polygon_nop_decomposition_2 for the convex operand).
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

using bench::cgal::PolygonType;
using ConvexPieces = CGAL::Hertel_Mehlhorn_convex_decomposition_2<bench::cgal::Kernel>;

template <class Operand, class Sum>
void sweepDataset(const bench::Options& opt, const char* dataset,
                  std::span<const int> sizes, const char* algorithm,
                  Operand operand, Sum sum) {
    if (!bench::matches(opt.dataset, dataset)) return;
    if (!bench::matches(opt.problem, "Minkowski sum")) return;

    for (const int n : bench::sweep(sizes, opt)) {
        const auto a = bench::cgal::polygon(bench::randomPolygon(n, 1));
        const auto b = bench::cgal::polygon(operand(n));

        long long result = 0;
        const double us = bench::timeOnce(result,
            [&] { return bench::cgal::vertexCount(sum(a, b)); });
        bench::emit("Minkowski sum", dataset, "Minkowski sum", algorithm,
                    bench::cgal::kNumber, n, result, us);
    }
}

auto byPieces(const PolygonType& a, const PolygonType& b) {
    ConvexPieces pieces;
    return CGAL::minkowski_sum_2(a, b, pieces);
}

auto byReducedConvolution(const PolygonType& a, const PolygonType& b) {
    return CGAL::minkowski_sum_by_reduced_convolution_2(a, b);
}

}  // namespace

int main(int argc, char** argv) {
    const auto opt = bench::parseOptions(argc, argv);
    bench::header();
    sweepDataset(opt, "large + large", bench::kMinkowski,
                 "CGAL::minkowski_sum_2 (Hertel_Mehlhorn)",
                 [](int n) { return bench::randomPolygon(n, 2); }, byPieces);
    sweepDataset(opt, "large + small", bench::kMinkowski,
                 "CGAL::minkowski_sum_2 (Hertel_Mehlhorn)",
                 [](int n) { return bench::randomSmallPolygon(n, 2); }, byPieces);
    const bench::IntPolygon convex = bench::smallConvex().asPolygon();
    sweepDataset(opt, "large + convex", bench::kMinkowski,
                 "CGAL::minkowski_sum_2 (reduced convolution)",
                 [&](int) { return convex; }, byReducedConvolution);
    return 0;
}
