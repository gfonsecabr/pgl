// @desc: CGAL reference for the Triangulation and Point constructions
// categories: Delaunay_triangulation_2 over the same random points, and its own
// point location. The signature is the number of finite faces, which is what
// pgl's triangle count means, so the two are directly comparable.
#include "cgal.hpp"
#include "../sizes.hpp"

#include <CGAL/Delaunay_triangulation_2.h>

#include <vector>

namespace {

using Triangulation = CGAL::Delaunay_triangulation_2<bench::cgal::Kernel>;

}  // namespace

int main(int argc, char** argv) {
    const auto opt = bench::parseOptions(argc, argv);
    bench::header();

    const auto queries = bench::cgal::points(bench::queryPoints(bench::kQueryBatch));

    for (const int n : bench::sweep(bench::kTriangulation, opt)) {
        const auto pts = bench::cgal::points(bench::points(n));
        long long result = 0;

        Triangulation triangulation;
        const double buildUs = bench::timeOnce(result, [&] {
            triangulation.insert(pts.begin(), pts.end());
            return triangulation.number_of_faces();
        });
        if (bench::matches(opt.problem, "build")) {
            bench::emit("Triangulation", "points", "build",
                        "CGAL::Delaunay_triangulation_2", bench::cgal::kNumber,
                        n, result, buildUs);
        }

        if (bench::matches(opt.problem, "locate")) {
            const double locateUs = bench::timeOnce(result, [&] {
                std::size_t hits = 0;
                for (const auto& q : queries) {
                    const auto face = triangulation.locate(q);
                    hits += triangulation.is_infinite(face) ? 0u : 1u;
                }
                return hits;
            });
            bench::emit("Triangulation", "points", "locate",
                        "CGAL::Delaunay_triangulation_2::locate", bench::cgal::kNumber,
                        n, result, locateUs / bench::kQueryBatch);
        }
    }
    return 0;
}
