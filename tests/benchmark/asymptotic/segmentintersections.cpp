// @desc: All intersections and all crossings among n segments, by
// findIntersections / findCrossings (which choose their own method), by
// Bentley–Ottmann alone and by the xy-sweep.
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
