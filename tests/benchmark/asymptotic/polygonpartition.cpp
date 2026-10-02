// @desc: A simple polygon, or a polygon with holes, of n vertices cut into
// pieces: triangles by its constrained Delaunay triangulation, convex pieces
// with disjoint interiors, and the fewest convex pieces whose corners are all
// corners of the polygon. The result is the number of pieces.
// @dataset large: The polygon's vertices have integer coordinates drawn
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
// nearest. The optimal convex partition is defined for simple polygons only,
// so this dataset has no such row.
#include "harness.hpp"
#include "datasets.hpp"
#include "sizes.hpp"

#include <map>
#include <type_traits>
#include <vector>

namespace {

constexpr const char* kCategory = "Polygon partition";

// The random polygon of n vertices, generated once per process: both number
// types sweep the same sizes over it, and untangling costs far more than any
// problem measured here.
const bench::IntPolygon& largePolygon(int n) {
    static std::map<int, bench::IntPolygon> cache;
    auto it = cache.find(n);
    if (it == cache.end()) {
        it = cache.emplace(n, bench::randomPolygon(n)).first;
    }
    return it->second;
}

template <class Number, class Generate>
void sweepDataset(const bench::Options& opt, const char* dataset,
                  const std::vector<int>& sizes, Generate generate) {
    if (!bench::matches(opt.dataset, dataset)) return;
    const char* number = bench::numberName<Number>;

    for (const int n : sizes) {
        const auto& source = generate(n);
        using Source = std::remove_cvref_t<decltype(source)>;
        const bench::retyped_t<Source, Number> polygon(source);
        long long result = 0;

        if (bench::matches(opt.problem, "triangulation")) {
            const double us = bench::timeOnce(result,
                [&] { return polygon.triangulation().numTriangles(); });
            bench::emit(kCategory, dataset, "triangulation", "triangulation",
                        number, n, result, us);
        }

        if (bench::matches(opt.problem, "convex partition")) {
            const double us = bench::timeOnce(result,
                [&] { return polygon.convexPartition().size(); });
            bench::emit(kCategory, dataset, "convex partition", "convexPartition",
                        number, n, result, us);
        }

        // Defined for simple polygons only.
        if constexpr (std::is_same_v<Source, bench::IntPolygon>) {
            if (bench::matches(opt.problem, "optimal convex partition")) {
                const double us = bench::timeOnce(result,
                    [&] { return polygon.optimalConvexPartition().size(); });
                bench::emit(kCategory, dataset, "optimal convex partition",
                            "optimalConvexPartition", number, n, result, us);
            }
        }
    }
}

template <class Number>
void run(const bench::Options& opt) {
    sweepDataset<Number>(opt, "large", bench::sweep(bench::kPartition, opt), largePolygon);
    for (const char* dataset : bench::kSbpdDatasets) {
        sweepDataset<Number>(opt, dataset,
                             bench::sbpdSizes(dataset, bench::sweep(bench::kPartition, opt)),
                             [dataset](int n) { return bench::sbpdPolygon(dataset, n); });
    }
    for (const char* dataset : bench::kSbpdRegionDatasets) {
        sweepDataset<Number>(opt, dataset,
                             bench::sbpdSizes(dataset, bench::sweep(bench::kPartition, opt)),
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
