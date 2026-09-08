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

// The size of the thing built: every cell of the subdivision, which is what the
// DCEL stores and so what the build had to produce. The vertex count alone --
// what this reported before -- is not a stand-in for it. The two datasets do not
// even agree on the ratio: over small segments an arrangement comes out with
// E ~ V and F ~ V/13, over large ones with E ~ 2V and F ~ V, because the first
// stays in hundreds of components and the second is one. Counting all three also
// gives the signature something to catch -- a wrong edge or face count is
// invisible in V -- and Euler's V - E + F = 1 + components ties them together.
template <class ArrangementType>
long long outputSize(const ArrangementType& arrangement) {
    return static_cast<long long>(arrangement.vertexCount() + arrangement.edgeCount() +
                                  arrangement.faceCount());
}

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
            return outputSize(*arrangement);
        });
        if (bench::matches(opt.problem, "build")) {
            bench::emit(kCategory, dataset, "build", "sweep", number, n, result, buildUs);
        }

        // The signature is how many queries landed in a bounded face. Face ids
        // themselves would be a sharper signature but a library-private one;
        // bounded-ness is a property of the plane, so the CGAL baseline can
        // compute the same number and the cross-check reaches outside pgl.
        //
        // It is a signature and nothing else: a locate returns one face however
        // large the subdivision is, so what grows under these rows is the
        // arrangement they query, and that is what they report as their output.
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
                        result, outputSize(*arrangement),
                        scanUs / bench::kSlowQueryBatch);
        }

        if (bench::matches(opt.problem, "buildPointLocation")) {
            const double indexUs = bench::timeOnce(result, [&] {
                arrangement->buildPointLocation();
                return outputSize(*arrangement);
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
                        result, outputSize(*arrangement),
                        indexedUs / bench::kSlowQueryBatch);
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
