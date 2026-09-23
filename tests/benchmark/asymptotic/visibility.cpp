// @desc: The visibility graph of a simple polygon of n vertices, and the
// vertices visible from a random point inside it.
// @dataset polygon: The polygon's vertices have integer coordinates drawn
// uniformly from a disk of diameter 5,000; joined in the order drawn, the ring
// is untangled into a simple polygon by flipping crossing edges, dropping the
// rare vertex that only touches another edge.
// @dataset fpg: The polygon with n vertices of the Salzburg Database of
// Polygonal Data's fpg set (triangulation perturbation). Its coordinates, in
// [-1500, 1500]², are scaled by 1,000 and rounded to integers. The database has
// polygons of only some sizes; the sweep takes the nearest.
// @dataset spg: The polygon with n vertices of the Salzburg Database of
// Polygonal Data's spg set (line sweep and 2-opt moves on random points). Its
// coordinates, in the unit square with six decimals, are scaled by 1,000,000.
// The database has polygons of only some sizes; the sweep takes the nearest.
// @dataset fpg-holes: A polygon with holes with n vertices in all, from the
// Salzburg Database of Polygonal Data's fpg set with holes (triangulation
// perturbation).
// The database has several polygons whose outer ring has a given number of
// vertices, with different numbers of holes; this is the one with the most.
// Its coordinates, in [-1500, 1500]², are scaled by 100,000 and rounded to
// integers. The database has polygons of only some sizes; the sweep takes the
// nearest.
#include "harness.hpp"
#include "datasets.hpp"
#include "sizes.hpp"

#include <type_traits>
#include <vector>

namespace {

constexpr const char* kCategory = "Visibility";

template <class Number, class Generate>
void sweepDataset(const bench::Options& opt, const char* dataset,
                  const std::vector<int>& sizes, Generate generate) {
    using Point = pgl::Point<Number>;
    const char* number = bench::numberName<Number>;

    for (const int n : sizes) {
        const auto source = generate(n);
        const bench::retyped_t<std::remove_cvref_t<decltype(source)>, Number> polygon(source);
        long long result = 0;

        if (bench::matches(opt.problem, "visibility graph")) {
            const double us = bench::timeOnce(result,
                [&] { return polygon.visibilityGraph().edgeCount(); });
            bench::emit(kCategory, dataset, "visibility graph", "triangulation",
                        number, n, result, us);
        }

        if (bench::matches(opt.problem, "visible vertices")) {
            const auto queries = bench::convert<Point>(
                bench::interiorPoints(source, bench::kVisibilityQueries));

            // Through the polygon: a triangulation per query.
            const double perQueryUs = bench::timeOnce(result, [&] {
                std::size_t total = 0;
                for (const auto& q : queries) {
                    total += polygon.visibleVertices(q).size();
                }
                return total;
            });
            bench::emit(kCategory, dataset, "visible vertices", "triangulate per query",
                        number, n, result, perQueryUs / bench::kVisibilityQueries);

            // Through a triangulation built once. Same queries, same answers —
            // the two rows' signatures cross-check each other at every size.
            const auto prepared = polygon.triangulation();
            long long preparedResult = 0;
            const double preparedUs = bench::timeOnce(preparedResult, [&] {
                std::size_t total = 0;
                for (const auto& q : queries) {
                    total += prepared.visibleVertices(q).size();
                }
                return total;
            });
            bench::require(preparedResult == result,
                           "the two visible-vertices paths disagree");
            bench::emit(kCategory, dataset, "visible vertices", "prepared triangulation",
                        number, n, preparedResult, preparedUs / bench::kVisibilityQueries);
        }
    }
}

template <class Number>
void run(const bench::Options& opt) {
    if (bench::matches(opt.dataset, "polygon")) {
        sweepDataset<Number>(opt, "polygon", bench::sweep(bench::kVisibility, opt),
                             [](int n) { return bench::randomPolygon(n); });
    }
    for (const char* dataset : bench::kSbpdDatasets) {
        if (!bench::matches(opt.dataset, dataset)) continue;
        sweepDataset<Number>(opt, dataset,
                             bench::sbpdSizes(dataset, bench::sweep(bench::kVisibility, opt)),
                             [dataset](int n) { return bench::sbpdPolygon(dataset, n); });
    }
    for (const char* dataset : bench::kSbpdRegionDatasets) {
        if (!bench::matches(opt.dataset, dataset)) continue;
        sweepDataset<Number>(opt, dataset,
                             bench::sbpdSizes(dataset, bench::sweep(bench::kVisibility, opt)),
                             [dataset](int n) { return bench::sbpdRegion(dataset, n); });
    }
}

}  // namespace

int main(int argc, char** argv) {
    const auto opt = bench::parseOptions(argc, argv);
    bench::header();
    if (bench::matches(opt.type, "int"))       run<int>(opt);
    if (bench::matches(opt.type, "ERational")) run<pgl::ERational>(opt);
    return 0;
}
