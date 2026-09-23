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
// The SBPD datasets sum a polygon with its own quarter turn, and there too the
// reference is reduced convolution: summed over each dataset's sweep it beats
// Hertel–Mehlhorn pieces of both operands by about a third on fpg and a
// twentieth on spg, though the pieces are level on fpg and ahead on spg at the
// top size, 200. On fpg-holes, which Hertel–Mehlhorn cannot decompose, it
// beats the decompositions that take holes, vertical and triangulation, about
// 25 times over.
//
// It reports the total number of boundary vertices, which is what pgl's driver
// reports, and must agree with pgl at every size.
#include "cgal.hpp"
#include "../sizes.hpp"

#include <CGAL/Polygon_convex_decomposition_2.h>
#include <CGAL/Polygon_with_holes_2.h>
#include <CGAL/minkowski_sum_2.h>

#include <vector>

namespace {

using bench::cgal::PolygonType;
using ConvexPieces = CGAL::Hertel_Mehlhorn_convex_decomposition_2<bench::cgal::Kernel>;

template <class First, class Second, class Sum>
void sweepDataset(const bench::Options& opt, const char* dataset,
                  const std::vector<int>& sizes, const char* algorithm,
                  First first, Second second, Sum sum) {
    if (!bench::matches(opt.dataset, dataset)) return;
    if (!bench::matches(opt.problem, "Minkowski sum")) return;

    for (const int n : sizes) {
        const auto source = first(n);
        const auto a = bench::cgal::toCgal(source);
        const auto b = bench::cgal::toCgal(second(n, source));

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

template <class Operand>
auto byReducedConvolution(const Operand& a, const Operand& b) {
    return CGAL::minkowski_sum_by_reduced_convolution_2(a, b);
}

}  // namespace

int main(int argc, char** argv) {
    const auto opt = bench::parseOptions(argc, argv);
    bench::header();
    const auto sizes = bench::sweep(bench::kMinkowski, opt);
    const auto random = [](int n) { return bench::randomPolygon(n, 1); };
    sweepDataset(opt, "large + large", sizes,
                 "CGAL::minkowski_sum_2 (Hertel_Mehlhorn)", random,
                 [](int n, const auto&) { return bench::randomPolygon(n, 2); }, byPieces);
    sweepDataset(opt, "large + small", sizes,
                 "CGAL::minkowski_sum_2 (Hertel_Mehlhorn)", random,
                 [](int n, const auto&) { return bench::randomSmallPolygon(n, 2); }, byPieces);
    const bench::IntPolygon convex = bench::smallConvex().asPolygon();
    sweepDataset(opt, "large + convex", sizes,
                 "CGAL::minkowski_sum_2 (reduced convolution)", random,
                 [&](int, const auto&) { return convex; }, byReducedConvolution<PolygonType>);
    for (const char* dataset : bench::kSbpdDatasets) {
        if (!bench::matches(opt.dataset, dataset)) continue;
        sweepDataset(opt, dataset, bench::sbpdSizes(dataset, sizes),
                     "CGAL::minkowski_sum_2 (reduced convolution)",
                     [dataset](int n) { return bench::sbpdPolygon(dataset, n); },
                     [](int, const bench::IntPolygon& a) { return bench::quarterTurn(a); },
                     byReducedConvolution<PolygonType>);
    }
    for (const char* dataset : bench::kSbpdRegionDatasets) {
        if (!bench::matches(opt.dataset, dataset)) continue;
        sweepDataset(opt, dataset, bench::sbpdSizes(dataset, sizes),
                     "CGAL::minkowski_sum_2 (reduced convolution)",
                     [dataset](int n) { return bench::sbpdRegion(dataset, n); },
                     [](int, const bench::IntRegion& a) { return bench::quarterTurn(a); },
                     byReducedConvolution<bench::cgal::RegionType>);
    }
    return 0;
}
