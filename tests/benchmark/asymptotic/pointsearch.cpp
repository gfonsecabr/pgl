// @desc: Range counting and nearest-neighbour queries against a kd-tree
// (ShapeTree) over n points.
// @dataset random: The points are distinct, with integer coordinates drawn
// uniformly from a disk of diameter 10,000.
// @dataset euro-night: The first n of the 100,000 distinct points of the CG:SHOP
// 2019 instance euro-night-0100000, sampled from a night-time image of Europe
// and shuffled once, with integer coordinates in [8, 102,392] x [0, 57,598].
// The queries are the random dataset's, carried onto that box by scaling x ten
// times and y five times about its centre.
#include "harness.hpp"
#include "datasets.hpp"
#include "sizes.hpp"

#include <optional>
#include <vector>

namespace {

constexpr const char* kCategory  = "Point search";
constexpr const char* kAlgorithm = "ShapeTree";

template <class Number>
void run(const bench::Options& opt, const bench::PointDataset& dataset) {
    using Point     = pgl::Point<Number>;
    using Rectangle = pgl::Rectangle<Point>;
    using Triangle  = pgl::Triangle<Point>;
    using SquaredNumber = pgl::detail::promoted_number_t<Number>;
    const char* number = bench::numberName<Number>;

    const auto rectangles = bench::convert<Rectangle>(dataset.queryRectangles(bench::kQueryBatch));
    const auto triangles  = bench::convert<Triangle>(dataset.queryTriangles(bench::kQueryBatch));
    const auto queries    = bench::convert<Point>(dataset.queryPoints(bench::kQueryBatch));

    for (const int n : bench::sweep(bench::kPointSearch, opt)) {
        const auto points = bench::convert<Point>(dataset.points(n));
        long long result = 0;

        std::optional<pgl::ShapeTree<Point>> tree;
        const double buildUs = bench::timeOnce(result, [&] {
            tree.emplace(points);
            return tree->size();
        });
        if (bench::matches(opt.problem, "build")) {
            bench::emit(kCategory, dataset.name, "build", kAlgorithm, number, n, result, buildUs);
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
            bench::emit(kCategory, dataset.name, "count in Rectangle", kAlgorithm,
                        number, n, result, us / bench::kQueryBatch);
        }
        if (bench::matches(opt.problem, "count in Triangle")) {
            const double us = bench::timeOnce(result, countIn(triangles));
            bench::emit(kCategory, dataset.name, "count in Triangle", kAlgorithm,
                        number, n, result, us / bench::kQueryBatch);
        }
        if (bench::matches(opt.problem, "nearest neighbor")) {
            const double us = bench::timeOnce(result, [&] {
                double sum = 0;
                for (const auto& q : queries) {
                    // Promoted, like the tree's own comparisons: the checksum
                    // is a squared distance, which for euro-night's coordinates
                    // does not fit an int.
                    sum += static_cast<double>(
                        q.template squaredDistance<SquaredNumber>(
                            tree->nearestNeighbor(q)));
                }
                return sum;
            });
            // The signature is a checksum over the answers' *distances*, not
            // over the answers: a query equidistant from two points has two
            // correct answers, and two implementations need not pick the same
            // one, but the distance they are both at is the same number. So
            // this row has to match to the digit like every other one. A
            // nearest neighbour is one point however large the tree is, so the
            // output column reports the tree the queries searched.
            bench::emit(kCategory, dataset.name, "nearest neighbor", kAlgorithm,
                        number, n, result, static_cast<long long>(tree->size()),
                        us / bench::kQueryBatch);
        }
    }
}

}  // namespace

int main(int argc, char** argv) {
    const auto opt = bench::parseOptions(argc, argv);
    bench::header();
    for (const auto& dataset : bench::pointDatasets()) {
        if (!bench::matches(opt.dataset, dataset.name)) continue;
        if (bench::matches(opt.type, "int"))       run<int>(opt, dataset);
        if (bench::matches(opt.type, "ERational")) run<pgl::ERational>(opt, dataset);
    }
    return 0;
}
