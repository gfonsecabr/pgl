// @desc: CGAL reference for the Minkowski sum category, two of them, over the
// same operands.
//
// The first is minkowski_sum_2 with a triangulation-based decomposition: the
// same strategy pgl uses, so that row compares two implementations of one idea
// rather than two ideas. The second is the reduced convolution method, which
// pgl has no counterpart for — it builds the convolution cycle of the two
// boundaries and extracts the sum from its arrangement, never decomposing
// either operand — and it is there to say what the other idea costs. Reading
// them together separates "pgl's decomposition is slower than CGAL's" from
// "decomposition is the slower approach".
//
// Both report twice the sum's area, which is what pgl's driver reports — a
// vertex count would not do, since the libraries keep collinear boundary
// vertices differently while the region they describe is the same. The two
// CGAL rows must agree with each other and with pgl's at every size.
#include "cgal.hpp"
#include "../sizes.hpp"

#include <CGAL/Polygon_triangulation_decomposition_2.h>
#include <CGAL/Polygon_with_holes_2.h>
#include <CGAL/minkowski_sum_2.h>

#include <span>

namespace {

using Decomposition = CGAL::Polygon_triangulation_decomposition_2<bench::cgal::Kernel>;

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
            return bench::cgal::doubledArea(
                CGAL::minkowski_sum_2(a, b, decomposition));
        });
        bench::emit("Minkowski sum", dataset, "Minkowski sum",
                    "CGAL::minkowski_sum_2", bench::cgal::kNumber, n, decomposed,
                    decomposedUs);

        long long convolved = 0;
        const double convolvedUs = bench::timeOnce(convolved, [&] {
            return bench::cgal::doubledArea(
                CGAL::minkowski_sum_by_reduced_convolution_2(a, b));
        });
        bench::require(convolved == decomposed,
                       "CGAL's two Minkowski sums disagree on the area");
        bench::emit("Minkowski sum", dataset, "Minkowski sum",
                    "CGAL::minkowski_sum_by_reduced_convolution_2",
                    bench::cgal::kNumber, n, convolved, convolvedUs);
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
