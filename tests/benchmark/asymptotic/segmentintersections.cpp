// @desc: All intersections and all crossings among n segments, by
// findIntersections / findCrossings (which choose their own method), by
// Bentley–Ottmann alone and by the xy-sweep.
// @dataset small: Distinct segments with integer coordinates. One
// endpoint is drawn uniformly from a disk of diameter 10,000 and the other is
// offset from it by a vector drawn uniformly from a disk of diameter 1,000, so
// most pairs are far apart.
// @dataset sheared: The small segments under the shear (x, y) -> (x, 10x + y).
// The same pairs meet, but the segments turn nearly vertical, so pairs whose
// bounding boxes overlap outnumber the intersecting pairs many times over.
// @dataset large: Distinct segments with integer coordinates. One
// endpoint is drawn uniformly from a disk of diameter 5,000 and the other is
// offset from it by a vector drawn uniformly from a disk of the same size.
// Between 5.9% and 6.8% of the pairs meet, depending on n.
// @dataset polygon edges: The edges of a random simple polygon with n vertices,
// as separate segments, so no two of them cross. The vertices have integer
// coordinates drawn uniformly from a disk of diameter 5,000; joined in the
// order drawn, the ring is untangled into a simple polygon by flipping crossing
// edges, dropping the rare vertex that only touches another edge.
// @dataset fpg: The edges of the polygon with n vertices of the Salzburg
// Database of Polygonal Data's fpg set (triangulation perturbation), as
// separate segments, so no two of them cross. Its coordinates, in
// [-1500, 1500]², are scaled by 1,000 and rounded to integers. The database has
// polygons of only some sizes; the sweep takes the nearest.
// @dataset spg: The edges of the polygon with n vertices of the Salzburg
// Database of Polygonal Data's spg set (line sweep and 2-opt moves on random
// points), as separate segments, so no two of them cross. Its coordinates, in
// the unit square with six decimals, are scaled by 1,000,000. The database has
// polygons of only some sizes; the sweep takes the nearest.
// @dataset fpg-holes: The edges of every ring of a polygon with holes with n
// vertices in all, from the Salzburg Database of Polygonal Data's fpg set with
// holes (triangulation perturbation), as separate segments, so no two of them
// cross.
// The database has several polygons whose outer ring has a given number of
// vertices, with different numbers of holes; this is the one with the most.
// Its coordinates, in [-1500, 1500]², are scaled by 100,000 and rounded to
// integers. The database has polygons of only some sizes; the sweep takes the
// nearest.
#include "harness.hpp"
#include "datasets.hpp"
#include "sizes.hpp"

#include <span>
#include <vector>

namespace {

constexpr const char* kCategory = "Segment intersections";

// The Bentley–Ottmann sweep on its own. The public functions only fall back to
// it where a scan over bounding boxes would cost more, so the row that has
// always been labelled Bentley-Ottmann forces it.
template <class Segment>
auto sweepOnly(const std::vector<Segment>& segments, bool onlyCrossings) {
    using Relation = pgl::detail::SegmentPairRelation;
    return pgl::detail::findSegmentPairs<pgl::Rational<pgl::BigInt>>(
        segments, onlyCrossings ? Relation::crosses : Relation::intersects,
        pgl::detail::SegmentPairMethod::sweep);
}

template <class Number, class Generate>
void sweepDataset(const bench::Options& opt, const char* dataset,
                  const std::vector<int>& sizes, Generate generate) {
    using Segment = pgl::Segment<pgl::Point<Number>>;
    const char* number = bench::numberName<Number>;

    for (const int n : sizes) {
        const auto segments = bench::convert<Segment>(generate(n));
        long long result = 0;

        if (bench::matches(opt.problem, "intersections")) {
            const double chosen = bench::timeOnce(result,
                [&] { return pgl::findIntersections(segments).size(); });
            bench::emit(kCategory, dataset, "intersections", "findIntersections",
                        number, n, result, chosen);
            const double bo = bench::timeOnce(result,
                [&] { return sweepOnly(segments, false).size(); });
            bench::emit(kCategory, dataset, "intersections", "Bentley-Ottmann",
                        number, n, result, bo);
            const double xy = bench::timeOnce(result,
                [&] { return pgl::detail::xyIntersections(segments).size(); });
            bench::emit(kCategory, dataset, "intersections", "xy sweep",
                        number, n, result, xy);
        }

        if (bench::matches(opt.problem, "crossings")) {
            const double chosen = bench::timeOnce(result,
                [&] { return pgl::findCrossings(segments).size(); });
            bench::emit(kCategory, dataset, "crossings", "findCrossings",
                        number, n, result, chosen);
            const double bo = bench::timeOnce(result,
                [&] { return sweepOnly(segments, true).size(); });
            bench::emit(kCategory, dataset, "crossings", "Bentley-Ottmann",
                        number, n, result, bo);
            const double xy = bench::timeOnce(result,
                [&] { return pgl::detail::xyCrossings(segments).size(); });
            bench::emit(kCategory, dataset, "crossings", "xy sweep",
                        number, n, result, xy);
        }
    }
}

template <class Number>
void run(const bench::Options& opt) {
    const auto generated = [&](const char* dataset, std::span<const int> sizes,
                               std::vector<bench::IntSegment> (*generate)(int)) {
        if (!bench::matches(opt.dataset, dataset)) return;
        sweepDataset<Number>(opt, dataset, bench::sweep(sizes, opt), generate);
    };
    generated("small",         bench::kSegmentsSmall,   bench::smallSegments);
    generated("sheared",       bench::kSegmentsSmall,   bench::shearedSegments);
    generated("large",         bench::kSegmentsLarge,   bench::largeSegments);
    generated("polygon edges", bench::kSegmentsPolygon, bench::polygonEdges);
    for (const char* dataset : bench::kSbpdDatasets) {
        if (!bench::matches(opt.dataset, dataset)) continue;
        sweepDataset<Number>(opt, dataset,
                             bench::sbpdSizes(dataset, bench::sweep(bench::kSegmentsPolygon, opt)),
                             [dataset](int n) {
                                 return bench::edgesOf(bench::sbpdPolygon(dataset, n));
                             });
    }
    for (const char* dataset : bench::kSbpdRegionDatasets) {
        if (!bench::matches(opt.dataset, dataset)) continue;
        sweepDataset<Number>(opt, dataset,
                             bench::sbpdSizes(dataset, bench::sweep(bench::kSegmentsPolygon, opt)),
                             [dataset](int n) {
                                 return bench::edgesOf(bench::sbpdRegion(dataset, n));
                             });
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
