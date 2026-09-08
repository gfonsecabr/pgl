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
    // `outputOf` says how big the answer was, which is not always what the
    // signature measured: a closest pair's signature is a squared distance, and
    // the sort's is a two-point spot check.
    const auto forEachSized = [&](const char* problem, const char* algorithm,
                                  std::span<const int> sizes, auto&& measure,
                                  auto&& outputOf) {
        if (!bench::matches(opt.problem, problem)) return;
        for (const int n : bench::sweep(sizes, opt)) {
            const auto points = bench::convert<Point>(bench::points(n));
            long long result = 0;
            const double us = bench::timeOnce(result, [&] { return measure(points); });
            bench::emit(kCategory, kDataset, problem, algorithm, number, n, result,
                        outputOf(points, result), us);
        }
    };

    // The ordinary case: the signature counts what came out, so it is its size.
    const auto forEach = [&](const char* problem, const char* algorithm,
                             std::span<const int> sizes, auto&& measure) {
        forEachSized(problem, algorithm, sizes, measure,
                     [](const std::vector<Point>&, long long result) { return result; });
    };

    // For the two problems whose answer has no size that grows -- a closest
    // pair is one segment, a sort is a permutation of what it was given -- the
    // point set they ran over is what the output column reports.
    const auto inputSize = [](const std::vector<Point>& points, long long) {
        return static_cast<long long>(points.size());
    };

    // The signature is a squared distance, which is not a size at all.
    forEachSized("closest pair", "divide and conquer", bench::kClosestPair,
                 [](const std::vector<Point>& points) {
                     return pgl::closestPair(points).squaredLength();
                 },
                 inputSize);
    forEach("convex hull", "Graham scan", bench::kConvexHull,
            [](const std::vector<Point>& points) {
                return pgl::convexHull(points).size();
            });
    // The signature compares the sorted order's first and last points: cheap,
    // but it cannot be computed without the whole sort having happened.
    forEachSized("sort by angle", "comparison sort", bench::kSortAround,
                 [](const std::vector<Point>& points) {
                     auto copy = points;
                     pgl::sortAround(copy, Point(0, 0));
                     return copy.front() == copy.back() ? 1 : 0;
                 },
                 inputSize);
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
