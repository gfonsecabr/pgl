// @desc: CGAL reference for the Constructions over a set of points category:
// convex_hull_2, Delaunay_triangulation_2 and Kd_tree over the same random
// points. Each signature is what pgl's driver reports for the same problem --
// the number of hull vertices, of triangles, of stored points -- so the rows
// are directly comparable.
//
// All three are predicate-only, so the sweep runs under both kernels: EPICK as
// the reference for pgl's `int` column, EPECK for `ERational`. The two must
// agree on every signature -- a hull has the same vertices however the
// coordinates are stored -- and cgal.hpp says why that is guaranteed rather
// than lucky.
//
// Three of the category's problems keep the sweep to themselves.
//
// closestPair and sortAround have no CGAL analogue as a single call, and
// standing one up out of the pieces would put a curve on the chart that
// measures an implementation written here rather than CGAL's.
//
// The kd-tree does have one, and takes it, but with a caveat that shapes how it
// is measured: CGAL's Kd_tree constructor only stores the points, and the
// hierarchy is built on the first query. build() is therefore called inside the
// timed region, so the row measures the work pgl's constructor does rather than
// a copy of a vector.
#include "cgal.hpp"
#include "../sizes.hpp"

#include <CGAL/Delaunay_triangulation_2.h>
#include <CGAL/Kd_tree.h>
#include <CGAL/Search_traits_2.h>
#include <CGAL/convex_hull_2.h>

#include <iterator>
#include <span>
#include <vector>

namespace {

template <class K>
void run(const bench::Options& opt) {
    if (!bench::cgal::selected<K>(opt)) return;
    const char* number = bench::cgal::numberName<K>;

    using Point         = typename K::Point_2;
    using Triangulation = CGAL::Delaunay_triangulation_2<K>;
    using SearchTraits  = CGAL::Search_traits_2<K>;
    using Tree          = CGAL::Kd_tree<SearchTraits>;

    // One problem of the category: its own size list, its own one-shot
    // construction, and a signature computed inside the timed region so the
    // work cannot be optimized away -- the shape of pgl's own driver.
    const auto forEach = [&](const char* problem, const char* algorithm,
                             std::span<const int> sizes, auto&& measure) {
        if (!bench::matches(opt.problem, problem)) return;
        for (const int n : bench::sweep(sizes, opt)) {
            const auto pts = bench::cgal::points<K>(bench::points(n));
            long long result = 0;
            const double us = bench::timeOnce(result, [&] { return measure(pts); });
            bench::emit("Point constructions", "points", problem, algorithm,
                        number, n, result, us);
        }
    };

    forEach("convex hull", "CGAL::convex_hull_2", bench::kConvexHull,
            [](const std::vector<Point>& pts) -> long long {
                std::vector<Point> hull;
                CGAL::convex_hull_2(pts.begin(), pts.end(), std::back_inserter(hull));
                return static_cast<long long>(hull.size());
            });
    // Finite faces, which is what pgl's triangle count means. The Triangulation
    // category measures the same construction; this row is the one that sweeps
    // this category's own size list.
    forEach("Delaunay", "CGAL::Delaunay_triangulation_2", bench::kDelaunayBuild,
            [](const std::vector<Point>& pts) -> long long {
                Triangulation triangulation;
                triangulation.insert(pts.begin(), pts.end());
                return static_cast<long long>(triangulation.number_of_faces());
            });
    forEach("kd-tree", "CGAL::Kd_tree", bench::kPointTree,
            [](const std::vector<Point>& pts) -> long long {
                Tree tree(pts.begin(), pts.end());
                tree.build();
                return static_cast<long long>(tree.size());
            });
}

}  // namespace

int main(int argc, char** argv) {
    const auto opt = bench::parseOptions(argc, argv);
    bench::header();
    if (!bench::matches(opt.dataset, "points")) return 0;
    run<bench::cgal::Inexact>(opt);
    run<bench::cgal::Kernel>(opt);
    return 0;
}
