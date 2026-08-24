// @desc: CGAL reference for the Arrangement category: Arrangement_2 over the
// same two segment datasets, with Arr_trapezoid_ric_point_location for the
// queries. The build's signature is the arrangement's vertex count, directly
// comparable with pgl's.
#include "cgal.hpp"
#include "../sizes.hpp"

#include <CGAL/Arr_segment_traits_2.h>
#include <CGAL/Arr_trapezoid_ric_point_location.h>
#include <CGAL/Arrangement_2.h>

#include <span>
#include <variant>
#include <vector>

namespace {

using Traits       = CGAL::Arr_segment_traits_2<bench::cgal::Kernel>;
using Arrangement  = CGAL::Arrangement_2<Traits>;
using Locator      = CGAL::Arr_trapezoid_ric_point_location<Arrangement>;
using Curve        = Traits::X_monotone_curve_2;

void sweepDataset(const bench::Options& opt, const char* dataset,
                  std::span<const int> sizes,
                  std::vector<bench::IntSegment> (*generate)(int)) {
    if (!bench::matches(opt.dataset, dataset)) return;

    const auto queries = bench::cgal::points(bench::queryPoints(bench::kQueryBatch));

    for (const int n : bench::sweep(sizes, opt)) {
        const auto raw = generate(n);
        std::vector<Curve> curves;
        curves.reserve(raw.size());
        for (const auto& s : raw) {
            curves.emplace_back(bench::cgal::point(s[0]), bench::cgal::point(s[1]));
        }

        long long result = 0;
        Arrangement arrangement;
        const double buildUs = bench::timeOnce(result, [&] {
            CGAL::insert(arrangement, curves.begin(), curves.end());
            return arrangement.number_of_vertices();
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
                return arrangement.number_of_vertices();
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
                    const auto* face = std::get_if<Arrangement::Face_const_handle>(&located);
                    bounded += (face && !(*face)->is_unbounded()) ? 1u : 0u;
                }
                return bounded;
            });
            bench::emit("Arrangement", dataset, "locateFace",
                        "CGAL::Arr_trapezoid_ric_point_location::locate",
                        bench::cgal::kNumber, n, result,
                        locateUs / bench::kSlowQueryBatch);
        }
    }
}

}  // namespace

int main(int argc, char** argv) {
    const auto opt = bench::parseOptions(argc, argv);
    bench::header();
    sweepDataset(opt, "small segments", bench::kArrangement, bench::smallSegments);
    sweepDataset(opt, "large segments", bench::kArrangementLarge, bench::largeSegments);
    return 0;
}
