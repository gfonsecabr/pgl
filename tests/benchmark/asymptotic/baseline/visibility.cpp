// @desc: CGAL reference for the Visibility category: the visibility region of
// the same interior query points, by triangular expansion.
//
// It answers the prepared row, not the per-query one. CGAL::Triangular_-
// expansion_visibility_2 attaches to an arrangement built once and then answers
// queries against it, which is what pgl's prepared triangulation does; timing
// it against a row that rebuilds its triangulation every call would compare a
// query with a preprocessing pass.
//
// The two libraries do not return the same thing. pgl reports the polygon
// vertices visible from the query; CGAL reports the visibility *region*, whose
// boundary carries those vertices plus a window endpoint on an edge wherever
// the view is cut off. The signature counts the region's vertices that are
// vertices of the input polygon, which is exactly pgl's answer — so the counts
// are comparable, while the times are not quite: CGAL's row includes building
// the region that pgl never materializes.
//
// Swept under both kernels, which is not obvious for an algorithm that
// constructs points and deserves saying why. The window endpoints it builds go
// *outward*, into the region it returns; they are never fed back into a
// predicate. Everything the expansion decides on -- which triangle to cross,
// which vertex blocks the view -- is an orientation of the query point against
// two input vertices, because the triangulation it walks holds input vertices
// only: CGAL declares that structure with
// `No_constraint_intersection_requiring_constructions_tag`, which is the
// library saying in its own code that no vertex here is constructed. So EPICK
// decides every one of those predicates exactly on integer input, and it is the
// reference for pgl's `int` column. Measured over the checked-in sweep, the two
// kernels agree on every query -- both the visible-vertex count and the whole
// region's size -- at all 32 sizes.
//
// That agreement does not carry over to the SBPD polygons. There EPICK's count
// differs from EPECK's, and from pgl's, at 23 of fpg's 32 sizes, 1 of spg's 13
// and 2 of fpg-holes' 32, and it throws CGAL::Bad_object_cast at the other 7
// sizes of fpg and 30 of fpg-holes. The size of the coordinates cannot be the
// reason, since EPICK's predicates fall back to exact arithmetic whenever their
// filter cannot settle a sign; so the expansion does decide something on a
// point it constructed, and the agreement above only says that nothing went
// wrong on the random polygon. A throw ends the whole driver, and EPECK agrees
// with pgl at every size of all three, so the SBPD datasets are swept under
// EPECK alone.
#include "cgal.hpp"
#include "../sizes.hpp"

#include <CGAL/Arr_segment_traits_2.h>
#include <CGAL/Arrangement_2.h>
#include <CGAL/Triangular_expansion_visibility_2.h>

#include <set>
#include <type_traits>
#include <vector>

namespace {

template <class K, class Generate>
void sweepDataset(const char* dataset, const std::vector<int>& sizes, Generate generate) {
    const char* number = bench::cgal::numberName<K>;

    using Point       = typename K::Point_2;
    using Traits      = CGAL::Arr_segment_traits_2<K>;
    using Arrangement = CGAL::Arrangement_2<Traits>;
    using Visibility  = CGAL::Triangular_expansion_visibility_2<Arrangement>;

    for (const int n : sizes) {
        const auto polygon = generate(n);
        const auto queries = bench::cgal::points<K>(
            bench::interiorPoints(polygon, bench::kVisibilityQueries));

        // Setup, untimed on both sides: the boundary as an arrangement, and the
        // set of its own vertices for the signature below.
        std::vector<typename Traits::X_monotone_curve_2> edges;
        std::set<Point> corners;
        for (const auto& e : polygon.edges()) {
            const auto a = bench::cgal::point<K>(e[0]);
            const auto b = bench::cgal::point<K>(e[1]);
            edges.emplace_back(a, b);
            corners.insert(a);
            corners.insert(b);
        }
        Arrangement boundary;
        CGAL::insert(boundary, edges.begin(), edges.end());

        // A simple polygon's boundary splits the plane in two, so its interior
        // is the one bounded face. A region's holes are bounded faces too, and
        // its interior is the bounded face that has them as holes, the only one
        // with any.
        auto interior = boundary.faces_end();
        for (auto f = boundary.faces_begin(); f != boundary.faces_end(); ++f) {
            if (!f->is_unbounded() &&
                (interior == boundary.faces_end() ||
                 f->number_of_holes() > interior->number_of_holes())) {
                interior = f;
            }
        }
        bench::require(interior != boundary.faces_end(),
                       "the polygon's boundary has no bounded face");

        Visibility visibility(boundary);
        long long result = 0;
        const double us = bench::timeOnce(result, [&] {
            std::size_t total = 0;
            for (const auto& q : queries) {
                Arrangement region;
                visibility.compute_visibility(
                    q, typename Arrangement::Face_const_handle(interior), region);
                for (auto v = region.vertices_begin(); v != region.vertices_end(); ++v) {
                    total += corners.count(v->point());
                }
            }
            return total;
        });
        bench::emit("Visibility", dataset, "visible vertices",
                    "CGAL::Triangular_expansion_visibility_2", number,
                    n, result, us / bench::kVisibilityQueries);
    }
}

template <class K>
void run(const bench::Options& opt) {
    if (!bench::cgal::selected<K>(opt)) return;
    if (bench::matches(opt.dataset, "polygon")) {
        sweepDataset<K>("polygon", bench::sweep(bench::kVisibility, opt),
                        [](int n) { return bench::randomPolygon(n); });
    }
    if (!std::is_same_v<K, bench::cgal::Kernel>) return;
    for (const char* dataset : bench::kSbpdDatasets) {
        if (!bench::matches(opt.dataset, dataset)) continue;
        sweepDataset<K>(dataset, bench::sbpdSizes(dataset, bench::sweep(bench::kVisibility, opt)),
                        [dataset](int n) { return bench::sbpdPolygon(dataset, n); });
    }
    for (const char* dataset : bench::kSbpdRegionDatasets) {
        if (!bench::matches(opt.dataset, dataset)) continue;
        sweepDataset<K>(dataset, bench::sbpdSizes(dataset, bench::sweep(bench::kVisibility, opt)),
                        [dataset](int n) { return bench::sbpdRegion(dataset, n); });
    }
}

}  // namespace

int main(int argc, char** argv) {
    const auto opt = bench::parseOptions(argc, argv);
    bench::header();
    if (!bench::matches(opt.problem, "visible vertices")) return 0;
    run<bench::cgal::Inexact>(opt);
    run<bench::cgal::Kernel>(opt);
    return 0;
}
