// @desc: CGAL reference for the Arrangement category: Arrangement_2 over the
// same three datasets, with Arr_trapezoid_ric_point_location for the queries.
// The segment datasets use Arr_segment_traits_2; the mixed one needs rays and
// lines, so it uses Arr_linear_traits_2, whose unbounded topology counts
// vertices at infinity and fictitious edges nowhere, just as pgl does. The
// build's signature is the arrangement's size -- finite vertices, edges and
// faces, every unbounded one included -- directly comparable with pgl's.
#include "cgal.hpp"
#include "../sizes.hpp"

#include <CGAL/Arr_linear_traits_2.h>
#include <CGAL/Arr_segment_traits_2.h>
#include <CGAL/Arr_trapezoid_ric_point_location.h>
#include <CGAL/Arrangement_2.h>

#include <span>
#include <variant>
#include <vector>

namespace {

using SegmentTraits = CGAL::Arr_segment_traits_2<bench::cgal::Kernel>;
using LinearTraits  = CGAL::Arr_linear_traits_2<bench::cgal::Kernel>;

// A segment, converted for the segment traits.
SegmentTraits::X_monotone_curve_2 segmentCurve(const bench::IntSegment& s) {
    return {bench::cgal::point(s[0]), bench::cgal::point(s[1])};
}

// A segment, ray or line, converted for the linear traits. The dataset builds a
// ray from its source towards a second point, and a line through two points,
// and CGAL's constructors take exactly those.
LinearTraits::Curve_2 linearCurve(const bench::IntShape& shape) {
    using bench::IntPoint;
    if (const auto* ray = shape.getIfHolds<pgl::Ray<IntPoint>>()) {
        return bench::cgal::Kernel::Ray_2(bench::cgal::point(ray->source()),
                                          bench::cgal::point(ray->target()));
    }
    if (const auto* line = shape.getIfHolds<pgl::Line<IntPoint>>()) {
        return bench::cgal::Kernel::Line_2(bench::cgal::point((*line)[0]),
                                           bench::cgal::point((*line)[1]));
    }
    const auto& segment = shape.asHeld<bench::IntSegment>();
    return bench::cgal::Kernel::Segment_2(bench::cgal::point(segment[0]),
                                          bench::cgal::point(segment[1]));
}

// Every cell of the subdivision, counted as pgl's driver counts them. CGAL's
// number_of_faces includes every unbounded face, as pgl's faceCount does, and
// number_of_vertices leaves out the vertices at infinity, as vertexCount does.
template <class Arrangement>
long long outputSize(const Arrangement& arrangement) {
    return static_cast<long long>(arrangement.number_of_vertices() +
                                  arrangement.number_of_edges() +
                                  arrangement.number_of_faces());
}

template <class Traits, class Source, class Convert>
void sweepDataset(const bench::Options& opt, const char* dataset,
                  std::span<const int> sizes,
                  std::vector<Source> (*generate)(int), Convert convert) {
    using Arrangement = CGAL::Arrangement_2<Traits>;
    using Locator     = CGAL::Arr_trapezoid_ric_point_location<Arrangement>;

    if (!bench::matches(opt.dataset, dataset)) return;

    const auto queries = bench::cgal::points(bench::queryPoints(bench::kQueryBatch));

    for (const int n : bench::sweep(sizes, opt)) {
        const auto raw = generate(n);
        std::vector<decltype(convert(raw.front()))> curves;
        curves.reserve(raw.size());
        for (const auto& s : raw) {
            curves.push_back(convert(s));
        }

        long long result = 0;
        Arrangement arrangement;
        const double buildUs = bench::timeOnce(result, [&] {
            CGAL::insert(arrangement, curves.begin(), curves.end());
            return outputSize(arrangement);
        });
        if (bench::matches(opt.problem, "build")) {
            bench::emit("Arrangement", dataset, "build", "CGAL::Arrangement_2",
                        bench::cgal::kNumber, n, result, buildUs);
        }

        // The index build and the queries, measured the way the pgl driver
        // measures them: the same short batch, so the per-query means line up.
        if (bench::matches(opt.problem, "buildPointLocation")) {
            Locator locator;
            const double indexUs = bench::timeOnce(result, [&] {
                locator.attach(arrangement);
                return outputSize(arrangement);
            });
            bench::emit("Arrangement", dataset, "buildPointLocation",
                        "CGAL::Arr_trapezoid_ric_point_location", bench::cgal::kNumber,
                        n, result, indexUs);
        }
        if (bench::matches(opt.problem, "locateFace")) {
            Locator locator(arrangement);
            const double locateUs = bench::timeOnce(result, [&] {
                std::size_t bounded = 0;
                for (int i = 0; i < bench::kSlowQueryBatch; ++i) {
                    const auto located = locator.locate(queries[static_cast<std::size_t>(i)]);
                    const auto* face = std::get_if<typename Arrangement::Face_const_handle>(&located);
                    bounded += (face && !(*face)->is_unbounded()) ? 1u : 0u;
                }
                return bounded;
            });
            // As in pgl's driver: the bounded-face count is the signature, and
            // the size these rows report is the arrangement being queried.
            bench::emit("Arrangement", dataset, "locateFace",
                        "CGAL::Arr_trapezoid_ric_point_location::locate",
                        bench::cgal::kNumber, n, result, outputSize(arrangement),
                        locateUs / bench::kSlowQueryBatch);
        }
    }
}

}  // namespace

int main(int argc, char** argv) {
    const auto opt = bench::parseOptions(argc, argv);
    bench::header();
    sweepDataset<SegmentTraits>(opt, "small segments", bench::kArrangement,
                                bench::smallSegments, segmentCurve);
    sweepDataset<SegmentTraits>(opt, "large segments", bench::kArrangementLarge,
                                bench::largeSegments, segmentCurve);
    sweepDataset<LinearTraits>(opt, "mixed", bench::kArrangementMixed,
                               bench::mixedShapes, linearCurve);
    return 0;
}
