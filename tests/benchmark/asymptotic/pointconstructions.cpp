// @desc: Five one-shot constructions over n random points: closest pair,
// convex hull, sorting by angle about a centre, the Delaunay triangulation, and
// building a kd-tree.
#include "harness.hpp"
#include "datasets.hpp"
#include "sizes.hpp"

#include <span>
#include <vector>

namespace {

constexpr const char* kCategory = "Point constructions";
constexpr const char* kDataset  = "points";

template <class Number>
void run(const bench::Options& opt) {
    using Point = pgl::Point<Number>;
    const char* number = bench::numberName<Number>;

    // Each problem sweeps its own list, so the datasets are built per problem
    // rather than once for the category. `measure` returns the construction's
    // numeric signature, computed inside the timed region so the work cannot be
    // optimized away.
    const auto forEach = [&](const char* problem, const char* algorithm,
                             std::span<const int> sizes, auto&& measure) {
        if (!bench::matches(opt.problem, problem)) return;
        for (const int n : bench::sweep(sizes, opt)) {
            const auto points = bench::convert<Point>(bench::points(n));
            long long result = 0;
            const double us = bench::timeOnce(result, [&] { return measure(points); });
            bench::emit(kCategory, kDataset, problem, algorithm, number, n, result, us);
        }
    };

    forEach("closest pair", "divide and conquer", bench::kClosestPair,
            [](const std::vector<Point>& points) {
                return pgl::closestPair(points).squaredLength();
            });
    forEach("convex hull", "Graham scan", bench::kConvexHull,
            [](const std::vector<Point>& points) {
                return pgl::convexHull(points).size();
            });
    // The signature compares the sorted order's first and last points: cheap,
    // but it cannot be computed without the whole sort having happened.
    forEach("sort by angle", "comparison sort", bench::kSortAround,
            [](const std::vector<Point>& points) {
                auto copy = points;
                pgl::sortAround(copy, Point(0, 0));
                return copy.front() == copy.back() ? 1 : 0;
            });
    forEach("Delaunay", "incremental", bench::kDelaunayBuild,
            [](const std::vector<Point>& points) {
                return pgl::Triangulation<pgl::Triangle<Point>>(points).triangles().size();
            });
    forEach("kd-tree", "ShapeTree", bench::kPointTree,
            [](const std::vector<Point>& points) {
                return pgl::ShapeTree<Point>(points).size();
            });
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
