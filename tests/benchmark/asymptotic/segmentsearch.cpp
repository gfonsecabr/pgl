// @desc: Counting the segments a query shape meets, through a kd-tree
// (ShapeTree) and through an interval tree, over n independent small random
// segments.
#include "harness.hpp"
#include "datasets.hpp"
#include "sizes.hpp"

#include <optional>
#include <span>
#include <vector>

namespace {

constexpr const char* kCategory = "Segment search";

template <class Number>
void sweepDataset(const bench::Options& opt, const char* dataset,
                  std::span<const int> sizes,
                  std::vector<bench::IntSegment> (*generate)(int)) {
    using Point     = pgl::Point<Number>;
    using Segment   = pgl::Segment<Point>;
    using Rectangle = pgl::Rectangle<Point>;
    using Triangle  = pgl::Triangle<Point>;
    const char* number = bench::numberName<Number>;
    if (!bench::matches(opt.dataset, dataset)) return;

    const auto rectangles = bench::convert<Rectangle>(bench::queryRectangles(bench::kQueryBatch));
    const auto triangles  = bench::convert<Triangle>(bench::queryTriangles(bench::kQueryBatch));

    for (const int n : bench::sweep(sizes, opt)) {
        const auto segments = bench::convert<Segment>(generate(n));
        long long result = 0;

        std::optional<pgl::ShapeTree<Segment>> shapeTree;
        std::optional<pgl::IntervalTree<Segment>> intervalTree;

        const double shapeBuildUs = bench::timeOnce(result, [&] {
            shapeTree.emplace(segments);
            return shapeTree->size();
        });
        if (bench::matches(opt.problem, "build")) {
            bench::emit(kCategory, dataset, "build", "ShapeTree", number, n, result,
                        shapeBuildUs);
        }
        const double intervalBuildUs = bench::timeOnce(result, [&] {
            intervalTree.emplace(segments);
            return intervalTree->size();
        });
        if (bench::matches(opt.problem, "build")) {
            bench::emit(kCategory, dataset, "build", "IntervalTree", number, n, result,
                        intervalBuildUs);
        }

        // The signature is the total count over the batch. The two structures
        // answer the same question, so their rows must agree at every size —
        // as must the int and ERational runs, which see the identical input.
        // A count against exact coordinates reaches milliseconds a query at the
        // top of the sweep, so the batch is the short one — shared by both
        // structures, which is what lets their signatures cross-check.
        const auto countIn = [](const auto& tree, const auto& shapes) {
            return [&] {
                std::size_t total = 0;
                for (int i = 0; i < bench::kSlowQueryBatch; ++i) {
                    total += tree->countIntersecting(shapes[static_cast<std::size_t>(i)]);
                }
                return total;
            };
        };
        const auto measure = [&](const char* problem, const auto& shapes) {
            if (!bench::matches(opt.problem, problem)) return;
            double us = bench::timeOnce(result, countIn(shapeTree, shapes));
            bench::emit(kCategory, dataset, problem, "ShapeTree", number, n, result,
                        us / bench::kSlowQueryBatch);
            us = bench::timeOnce(result, countIn(intervalTree, shapes));
            bench::emit(kCategory, dataset, problem, "IntervalTree", number, n, result,
                        us / bench::kSlowQueryBatch);
        };
        measure("count in Rectangle", rectangles);
        measure("count in Triangle", triangles);
    }
}

template <class Number>
void run(const bench::Options& opt) {
    sweepDataset<Number>(opt, "small segments", bench::kSegmentSearch,
                         bench::smallSegments);
}

}  // namespace

int main(int argc, char** argv) {
    const auto opt = bench::parseOptions(argc, argv);
    bench::header();
    if (bench::matches(opt.type, "int"))       run<int>(opt);
    if (bench::matches(opt.type, "ERational")) run<pgl::ERational>(opt);
    return 0;
}
