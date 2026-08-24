// @desc: The Minkowski sum of two simple polygons with n vertices each.
#include "harness.hpp"
#include "datasets.hpp"
#include "sizes.hpp"

#include <span>
#include <vector>

namespace {

constexpr const char* kCategory  = "Minkowski sum";
constexpr const char* kAlgorithm = "convex decomposition";

void sweepDataset(const bench::Options& opt, const char* dataset,
                  std::span<const int> sizes, bool bothSwept) {
    const char* number = bench::numberName<pgl::ERational>;
    if (!bench::matches(opt.dataset, dataset)) return;
    if (!bench::matches(opt.problem, "Minkowski sum")) return;

    for (const int n : bench::sweep(sizes, opt)) {
        // Two different draws, so the operands are never the same polygon.
        const pgl::EPolygon a(bench::randomPolygon(n, 1));
        const pgl::EPolygon b(bothSwept ? bench::randomPolygon(n, 2)
                                        : bench::randomSmallPolygon(n, 2));
        long long result = 0;
        const double us = bench::timeOnce(result,
            [&] { return bench::doubledArea(a.minkowskiSum(b)); });
        bench::emit(kCategory, dataset, "Minkowski sum", kAlgorithm, number, n, result, us);
    }
}

}  // namespace

int main(int argc, char** argv) {
    const auto opt = bench::parseOptions(argc, argv);
    bench::header();
    if (bench::matches(opt.type, "ERational")) {
        sweepDataset(opt, "large + large", bench::kMinkowski, true);
        sweepDataset(opt, "large + small", bench::kMinkowski, false);
    }
    return 0;
}
