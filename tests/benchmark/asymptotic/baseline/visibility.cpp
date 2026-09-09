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
#include "cgal.hpp"
#include "../sizes.hpp"

#include <CGAL/Arr_segment_traits_2.h>
#include <CGAL/Arrangement_2.h>
#include <CGAL/Triangular_expansion_visibility_2.h>

#include <set>
#include <vector>

namespace {

template <class K>
void run(const bench::Options& opt) {
    if (!bench::cgal::selected<K>(opt)) return;
    const char* number = bench::cgal::numberName<K>;

    using Point       = typename K::Point_2;
    using Traits      = CGAL::Arr_segment_traits_2<K>;
    using Arrangement = CGAL::Arrangement_2<Traits>;
    using Visibility  = CGAL::Triangular_expansion_visibility_2<Arrangement>;

    for (const int n : bench::sweep(bench::kVisibility, opt)) {
        const auto polygon = bench::randomPolygon(n);
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
        // is the one bounded face.
        auto interior = boundary.faces_begin();
        while (interior != boundary.faces_end() && interior->is_unbounded()) {
            ++interior;
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
        bench::emit("Visibility", "polygon", "visible vertices",
                    "CGAL::Triangular_expansion_visibility_2", number,
                    n, result, us / bench::kVisibilityQueries);
    }
}

}  // namespace

int main(int argc, char** argv) {
    const auto opt = bench::parseOptions(argc, argv);
    bench::header();
    if (!bench::matches(opt.dataset, "polygon")) return 0;
    if (!bench::matches(opt.problem, "visible vertices")) return 0;
    run<bench::cgal::Inexact>(opt);
    run<bench::cgal::Kernel>(opt);
    return 0;
}
