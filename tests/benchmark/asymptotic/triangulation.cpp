// @desc: Delaunay triangulation of n points. Location by stochastic walk, and
// through the preprocessed point location.
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

constexpr const char* kCategory = "Triangulation";

template <class Number>
void run(const bench::Options& opt, const bench::PointDataset& dataset) {
    using Point = pgl::Point<Number>;
    const char* number = bench::numberName<Number>;

    const auto queries = bench::convert<Point>(dataset.queryPoints(bench::kQueryBatch));

    for (const int n : bench::sweep(bench::kTriangulation, opt)) {
        const auto points = bench::convert<Point>(dataset.points(n));
        long long result = 0;

        // The build is measured and kept: every other problem in this category
        // queries this same triangulation.
        //
        // The signature is numTriangles(), not triangles().size(): the two
        // report the same number, but the latter materializes and sorts a
        // vector of every triangle, which at the top size costs a sixth of the
        // build again — and the CGAL row this is compared against signs itself
        // with number_of_faces(), which is a read of a counter. Timing a
        // conversion to a sorted vector on one side of that comparison and not
        // the other measures the conversion, not the build.
        std::optional<pgl::Triangulation<pgl::Triangle<Point>>> triangulation;
        const double buildUs = bench::timeOnce(result, [&] {
            triangulation.emplace(points);
            return triangulation->numTriangles();
        });
        if (bench::matches(opt.problem, "build")) {
            bench::emit(kCategory, dataset.name, "build", "incremental", number, n, result, buildUs);
        }

        // A locate answers with one triangle however large the triangulation
        // is, so the hit count below is a signature and not a size: what these
        // rows report as their output is the triangulation they search.
        const auto locate = [&] {
            std::size_t hits = 0;
            for (const auto& q : queries) {
                hits += triangulation->locate(q).has_value() ? 1u : 0u;
            }
            return hits;
        };

        // Before the index: the stochastic visibility walk.
        if (bench::matches(opt.problem, "locate")) {
            const double walkUs = bench::timeOnce(result, locate);
            bench::emit(kCategory, dataset.name, "locate", "walk", number, n, result,
                        static_cast<long long>(triangulation->numTriangles()),
                        walkUs / bench::kQueryBatch);
        }

        if (bench::matches(opt.problem, "buildPointLocation")) {
            const double indexUs = bench::timeOnce(result, [&] {
                triangulation->buildPointLocation();
                return triangulation->numEdges();
            });
            bench::emit(kCategory, dataset.name, "buildPointLocation", "preprocessed",
                        number, n, result, indexUs);
        } else {
            triangulation->buildPointLocation();
        }

        // After it: the same queries down the Kirkpatrick hierarchy. The hit
        // count is the same signature as the walk's, so the two rows
        // cross-check each other at every size.
        bench::require(triangulation->hasPointLocation(),
                       "the point-location index is not in place");
        if (bench::matches(opt.problem, "locate")) {
            const double indexedUs = bench::timeOnce(result, locate);
            bench::emit(kCategory, dataset.name, "locate", "preprocessed", number, n, result,
                        static_cast<long long>(triangulation->numTriangles()),
                        indexedUs / bench::kQueryBatch);
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
