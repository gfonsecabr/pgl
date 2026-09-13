// @desc: All intersections and all crossings among n segments, by
// findIntersections / findCrossings (which choose their own method), by
// Bentley–Ottmann alone and by the xy-sweep.
// @dataset small segments: Distinct segments with integer coordinates. One
// endpoint is drawn uniformly from a disk of diameter 10,000 and the other is
// offset from it by a vector drawn uniformly from a disk of diameter 1,000, so
// most pairs are far apart.
// @dataset sheared: The small segments under the shear (x, y) -> (x, 10x + y).
// The same pairs meet, but the segments turn nearly vertical, so pairs whose
// bounding boxes overlap outnumber the intersecting pairs many times over.
// @dataset large segments: Distinct segments with integer coordinates. One
// endpoint is drawn uniformly from a disk of diameter 5,000 and the other is
// offset from it by a vector drawn uniformly from a disk of the same size, so
// most pairs meet.
// @dataset polygon edges: The edges of a random simple polygon with n vertices,
// as separate segments, so no two of them cross. The vertices have integer
// coordinates drawn uniformly from a disk of diameter 5,000; joined in the
// order drawn, the ring is untangled into a simple polygon by flipping crossing
// edges, dropping the rare vertex that only touches another edge.
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

template <class Number>
void sweepDataset(const bench::Options& opt, const char* dataset,
                  std::span<const int> sizes,
                  std::vector<bench::IntSegment> (*generate)(int)) {
    using Segment = pgl::Segment<pgl::Point<Number>>;
    const char* number = bench::numberName<Number>;
    if (!bench::matches(opt.dataset, dataset)) return;

    for (const int n : bench::sweep(sizes, opt)) {
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
    sweepDataset<Number>(opt, "small segments", bench::kSegmentsSmall, bench::smallSegments);
    sweepDataset<Number>(opt, "sheared",        bench::kSegmentsSmall, bench::shearedSegments);
    sweepDataset<Number>(opt, "large segments", bench::kSegmentsLarge, bench::largeSegments);
    sweepDataset<Number>(opt, "polygon edges",  bench::kSegmentsPolygon, bench::polygonEdges);
}

}  // namespace

int main(int argc, char** argv) {
    const auto opt = bench::parseOptions(argc, argv);
    bench::header();
    if (bench::matches(opt.type, "int"))       run<int>(opt);
    if (bench::matches(opt.type, "ERational")) run<pgl::ERational>(opt);
    return 0;
}
