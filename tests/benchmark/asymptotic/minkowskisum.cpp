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
// @dataset fpg: The polygon with n vertices of the Salzburg Database of
// Polygonal Data's fpg set (triangulation perturbation), and the same polygon
// turned a quarter turn about the centre of its bounding box, since the
// database has one polygon of each size. Its coordinates, in [-1500, 1500]²,
// are scaled by 1,000 and rounded to integers. The database has polygons of
// only some sizes; the sweep takes the nearest.
// @dataset spg: The polygon with n vertices of the Salzburg Database of
// Polygonal Data's spg set (line sweep and 2-opt moves on random points), and
// the same polygon turned a quarter turn about the centre of its bounding box.
// Its coordinates, in the unit square with six decimals, are scaled by
// 1,000,000. The database has polygons of only some sizes; the sweep takes the
// nearest.
// @dataset fpg-holes: A polygon with holes with n vertices in all, from the
// Salzburg Database of Polygonal Data's fpg set with holes (triangulation
// perturbation), and the same region turned a quarter turn about the centre of
// its bounding box.
// The database has several polygons whose outer ring has a given number of
// vertices, with different numbers of holes; this is the one with the most.
// Its coordinates, in [-1500, 1500]², are scaled by 100,000 and rounded to
// integers. The database has polygons of only some sizes; the sweep takes the
// nearest.
#include "harness.hpp"
#include "datasets.hpp"
#include "sizes.hpp"

#include <span>
#include <type_traits>
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

// A polygon or region of the database against its own quarter turn.
template <class Load>
void sweepSbpd(const bench::Options& opt, const char* dataset, Load load) {
    const char* number = bench::numberName<pgl::ERational>;
    if (!bench::matches(opt.dataset, dataset)) return;
    if (!bench::matches(opt.problem, "Minkowski sum")) return;

    for (const int n : bench::sbpdSizes(dataset, bench::sweep(bench::kMinkowski, opt))) {
        const auto source = load(dataset, n);
        using Exact = bench::retyped_t<std::remove_cvref_t<decltype(source)>, pgl::ERational>;
        const Exact a(source);
        const Exact b(bench::quarterTurn(source));
        long long result = 0;
        const double us = bench::timeOnce(result,
            [&] { return static_cast<long long>(a.minkowskiSum(b).vertexCount()); });
        bench::emit(kCategory, dataset, "Minkowski sum", "convex decomposition",
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
        for (const char* dataset : bench::kSbpdDatasets) {
            sweepSbpd(opt, dataset, bench::sbpdPolygon);
        }
        for (const char* dataset : bench::kSbpdRegionDatasets) {
            sweepSbpd(opt, dataset, bench::sbpdRegion);
        }
    }
    return 0;
}
