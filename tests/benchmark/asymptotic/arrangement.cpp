// @desc: Arrangement of n random segments, and locating the face containing a
// point two ways: by scanning the edges, and through the trapezoidal DAG that
// buildPointLocation constructs.
#include "harness.hpp"
#include "datasets.hpp"
#include "sizes.hpp"

#include <optional>
#include <span>
#include <vector>

namespace {

constexpr const char* kCategory = "Arrangement";

// Exact arithmetic only: an arrangement of intersecting segments has rational
// vertices, so there is no meaningful `int` cell to compare against.
void sweepDataset(const bench::Options& opt, const char* dataset,
                  std::span<const int> sizes,
                  std::vector<bench::IntSegment> (*generate)(int)) {
    using Point = pgl::EPoint;
    const char* number = bench::numberName<pgl::ERational>;
    if (!bench::matches(opt.dataset, dataset)) return;

    const auto queries = bench::convert<Point>(bench::queryPoints(bench::kQueryBatch));

    for (const int n : bench::sweep(sizes, opt)) {
        const auto segments = bench::convert<pgl::ESegment>(generate(n));
        long long result = 0;

        std::optional<pgl::Arrangement<Point>> arrangement;
        const double buildUs = bench::timeOnce(result, [&] {
            arrangement.emplace(segments);
            return arrangement->vertices().size();
        });
        if (bench::matches(opt.problem, "build")) {
            bench::emit(kCategory, dataset, "build", "sweep", number, n, result, buildUs);
        }

        // The signature is how many queries landed in a bounded face. Face ids
        // themselves would be a sharper signature but a library-private one;
        // bounded-ness is a property of the plane, so the CGAL baseline can
        // compute the same number and the cross-check reaches outside pgl.
        //
        // Without the index locateFace is a linear scan over the edges, which
        // reaches milliseconds a query at the top of the sweep — hence the
        // short batch, shared with the indexed algorithm so the two stay
        // comparable.
        const auto locate = [&] {
            std::size_t bounded = 0;
            for (int i = 0; i < bench::kSlowQueryBatch; ++i) {
                const auto face = arrangement->locateFace(queries[static_cast<std::size_t>(i)]);
                bounded += arrangement->isUnbounded(face) ? 0u : 1u;
            }
            return bounded;
        };

        // Without the index, locateFace scans the edges.
        if (bench::matches(opt.problem, "locateFace")) {
            const double scanUs = bench::timeOnce(result, locate);
            bench::emit(kCategory, dataset, "locateFace", "edge scan", number, n,
                        result, scanUs / bench::kSlowQueryBatch);
        }

        if (bench::matches(opt.problem, "buildPointLocation")) {
            const double indexUs = bench::timeOnce(result, [&] {
                arrangement->buildPointLocation();
                return arrangement->vertices().size();
            });
            bench::emit(kCategory, dataset, "buildPointLocation", "trapezoidal DAG",
                        number, n, result, indexUs);
        } else {
            arrangement->buildPointLocation();
        }

        bench::require(arrangement->hasPointLocation(),
                       "the point-location index is not in place");
        if (bench::matches(opt.problem, "locateFace")) {
            const double indexedUs = bench::timeOnce(result, locate);
            bench::emit(kCategory, dataset, "locateFace", "trapezoidal DAG", number, n,
                        result, indexedUs / bench::kSlowQueryBatch);
        }
    }
}

}  // namespace

int main(int argc, char** argv) {
    const auto opt = bench::parseOptions(argc, argv);
    bench::header();
    if (bench::matches(opt.type, "ERational")) {
        sweepDataset(opt, "small segments", bench::kArrangement, bench::smallSegments);
        sweepDataset(opt, "large segments", bench::kArrangementLarge, bench::largeSegments);
    }
    return 0;
}
