// @desc: All intersections and all crossings among n segments, by Bentley–
// Ottmann and by the xy-sweep.
#include "harness.hpp"
#include "datasets.hpp"
#include "sizes.hpp"

#include <span>
#include <vector>

namespace {

constexpr const char* kCategory = "Segment intersections";

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
            const double bo = bench::timeOnce(result,
                [&] { return pgl::findIntersections(segments).size(); });
            bench::emit(kCategory, dataset, "intersections", "Bentley-Ottmann",
                        number, n, result, bo);
            const double xy = bench::timeOnce(result,
                [&] { return pgl::xyIntersections(segments).size(); });
            bench::emit(kCategory, dataset, "intersections", "xy sweep",
                        number, n, result, xy);
        }

        if (bench::matches(opt.problem, "crossings")) {
            const double bo = bench::timeOnce(result,
                [&] { return pgl::findCrossings(segments).size(); });
            bench::emit(kCategory, dataset, "crossings", "Bentley-Ottmann",
                        number, n, result, bo);
            const double xy = bench::timeOnce(result,
                [&] { return pgl::xyCrossings(segments).size(); });
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
