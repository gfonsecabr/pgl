// @desc: Range counting and nearest-neighbour queries against a kd-tree
// (ShapeTree) over n random points.
#include "harness.hpp"
#include "datasets.hpp"
#include "sizes.hpp"

#include <optional>
#include <vector>

namespace {

constexpr const char* kCategory  = "Point search";
constexpr const char* kDataset   = "points";
constexpr const char* kAlgorithm = "ShapeTree";

template <class Number>
void run(const bench::Options& opt) {
    using Point     = pgl::Point<Number>;
    using Rectangle = pgl::Rectangle<Point>;
    using Triangle  = pgl::Triangle<Point>;
    const char* number = bench::numberName<Number>;

    const auto rectangles = bench::convert<Rectangle>(bench::queryRectangles(bench::kQueryBatch));
    const auto triangles  = bench::convert<Triangle>(bench::queryTriangles(bench::kQueryBatch));
    const auto queries    = bench::convert<Point>(bench::queryPoints(bench::kQueryBatch));

    for (const int n : bench::sweep(bench::kPointSearch, opt)) {
        const auto points = bench::convert<Point>(bench::points(n));
        long long result = 0;

        std::optional<pgl::ShapeTree<Point>> tree;
        const double buildUs = bench::timeOnce(result, [&] {
            tree.emplace(points);
            return tree->size();
        });
        if (bench::matches(opt.problem, "build")) {
            bench::emit(kCategory, kDataset, "build", kAlgorithm, number, n, result, buildUs);
        }

        // The signature is the total count over the batch, so it cross-checks
        // the int run against the exact one on the identical input.
        const auto countIn = [&](const auto& shapes) {
            return [&] {
                std::size_t total = 0;
                for (const auto& q : shapes) {
                    total += tree->countIntersecting(q);
                }
                return total;
            };
        };

        if (bench::matches(opt.problem, "count in Rectangle")) {
            const double us = bench::timeOnce(result, countIn(rectangles));
            bench::emit(kCategory, kDataset, "count in Rectangle", kAlgorithm,
                        number, n, result, us / bench::kQueryBatch);
        }
        if (bench::matches(opt.problem, "count in Triangle")) {
            const double us = bench::timeOnce(result, countIn(triangles));
            bench::emit(kCategory, kDataset, "count in Triangle", kAlgorithm,
                        number, n, result, us / bench::kQueryBatch);
        }
        if (bench::matches(opt.problem, "nearest neighbor")) {
            const double us = bench::timeOnce(result, [&] {
                double sum = 0;
                for (const auto& q : queries) {
                    sum += static_cast<double>(tree->nearestNeighbor(q).x());
                }
                return sum;
            });
            bench::emit(kCategory, kDataset, "nearest neighbor", kAlgorithm,
                        number, n, result, us / bench::kQueryBatch);
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
