// @desc: The Minkowski sum of a simple polygon with n vertices and a second
// operand — another simple polygon with n vertices, or a fixed small convex
// polygon; the result is the total number of boundary vertices.
// @dataset large + large: Two independent random simple polygons. Each one's
// vertices have integer coordinates drawn uniformly from a disk of diameter
// 5,000; joined in the order drawn, its ring is untangled into a simple polygon
// by flipping crossing edges, dropping the rare vertex that only touches
// another edge.
// @dataset large + small: Two independent random simple polygons, one in a disk
// of diameter 5,000 and one in a disk of diameter 1,000. Each one's vertices
// have integer coordinates drawn uniformly from its disk; joined in the order
// drawn, its ring is untangled into a simple polygon by flipping crossing
// edges, dropping the rare vertex that only touches another edge.
// @dataset large + convex: A random simple polygon, generated as in large +
// large, and one fixed convex polygon with 40 vertices, the same at every n: the
// convex hull of 1,000 points with integer coordinates drawn uniformly from a
// disk of diameter 1,000.
#include "harness.hpp"
#include "datasets.hpp"
#include "sizes.hpp"

#include <span>
#include <vector>

namespace {

constexpr const char* kCategory = "Minkowski sum";

void sweepPolygons(const bench::Options& opt, const char* dataset,
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
            [&] { return static_cast<long long>(a.minkowskiSum(b).vertexCount()); });
        bench::emit(kCategory, dataset, "Minkowski sum", "convex decomposition",
                    number, n, result, us);
    }
}

void sweepConvex(const bench::Options& opt, const char* dataset,
                 std::span<const int> sizes) {
    const char* number = bench::numberName<pgl::ERational>;
    if (!bench::matches(opt.dataset, dataset)) return;
    if (!bench::matches(opt.problem, "Minkowski sum")) return;

    const pgl::EConvex b(bench::smallConvex());
    for (const int n : bench::sweep(sizes, opt)) {
        const pgl::EPolygon a(bench::randomPolygon(n, 1));
        long long result = 0;
        const double us = bench::timeOnce(result,
            [&] { return static_cast<long long>(a.minkowskiSum(b).vertexCount()); });
        bench::emit(kCategory, dataset, "Minkowski sum", "convolution",
                    number, n, result, us);
    }
}

}  // namespace

int main(int argc, char** argv) {
    const auto opt = bench::parseOptions(argc, argv);
    bench::header();
    if (bench::matches(opt.type, "ERational")) {
        sweepPolygons(opt, "large + large", bench::kMinkowski, true);
        sweepPolygons(opt, "large + small", bench::kMinkowski, false);
        sweepConvex(opt, "large + convex", bench::kMinkowski);
    }
    return 0;
}
