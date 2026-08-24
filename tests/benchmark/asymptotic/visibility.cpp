// @desc: The visibility graph of a simple polygon of n vertices, and the
// vertices visible from a random point inside it.
#include "harness.hpp"
#include "datasets.hpp"
#include "sizes.hpp"

#include <vector>

namespace {

constexpr const char* kCategory = "Visibility";
constexpr const char* kDataset  = "polygon";

template <class Number>
void run(const bench::Options& opt) {
    using Point   = pgl::Point<Number>;
    using Polygon = pgl::Polygon<Point>;
    const char* number = bench::numberName<Number>;

    for (const int n : bench::sweep(bench::kVisibility, opt)) {
        const auto source = bench::randomPolygon(n);
        const Polygon polygon(source);
        long long result = 0;

        if (bench::matches(opt.problem, "visibility graph")) {
            const double us = bench::timeOnce(result,
                [&] { return polygon.visibilityGraph().edgeCount(); });
            bench::emit(kCategory, kDataset, "visibility graph", "triangulation",
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
            bench::emit(kCategory, kDataset, "visible vertices", "triangulate per query",
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
            bench::emit(kCategory, kDataset, "visible vertices", "prepared triangulation",
                        number, n, preparedResult, preparedUs / bench::kVisibilityQueries);
        }
    }
}

}  // namespace

int main(int argc, char** argv) {
    const auto opt = bench::parseOptions(argc, argv);
    bench::header();
    if (bench::matches(opt.dataset, kDataset)) {
        if (bench::matches(opt.type, "int"))       run<int>(opt);
        if (bench::matches(opt.type, "ERational")) run<pgl::ERational>(opt);
    }
    return 0;
}
