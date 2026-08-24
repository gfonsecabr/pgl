// @desc: CGAL reference for the Union category: General_polygon_set_2 over the
// same operands. The signature is twice the result's total area, which is what
// pgl's driver reports — a vertex or component count would not do, since the
// two libraries keep collinear boundary vertices differently while the region
// they describe is the same.
#include "cgal.hpp"
#include "../sizes.hpp"

#include <CGAL/Boolean_set_operations_2.h>
#include <CGAL/General_polygon_set_2.h>
#include <CGAL/Polygon_with_holes_2.h>

#include <vector>

namespace {

using Region       = CGAL::Polygon_with_holes_2<bench::cgal::Kernel>;
using PolygonSet   = CGAL::General_polygon_set_2<
    CGAL::Gps_segment_traits_2<bench::cgal::Kernel>>;

double doubledArea(const PolygonSet& set) {
    std::vector<Region> components;
    set.polygons_with_holes(std::back_inserter(components));
    double total = 0;
    for (const auto& component : components) {
        if (component.is_unbounded()) continue;
        total += bench::cgal::doubledArea(component);
    }
    return total;
}

}  // namespace

int main(int argc, char** argv) {
    const auto opt = bench::parseOptions(argc, argv);
    bench::header();

    if (bench::matches(opt.dataset, "large + large") &&
        bench::matches(opt.problem, "union")) {
        for (const int n : bench::sweep(bench::kUnionPair, opt)) {
            const auto a = bench::cgal::polygon(bench::randomPolygon(n, 1));
            const auto b = bench::cgal::polygon(bench::randomPolygon(n, 2));
            long long result = 0;
            const double us = bench::timeOnce(result, [&] {
                PolygonSet set(a);
                set.join(b);
                return doubledArea(set);
            });
            bench::emit("Regularized union", "large + large", "union",
                        "CGAL::General_polygon_set_2::join", bench::cgal::kNumber,
                        n, result, us);
        }
    }

    // The same n triangles, straight from the shared generator: integer
    // vertices, so nothing is converted through pgl on the way in and the two
    // libraries union the identical set.
    if (bench::matches(opt.dataset, "triangles") &&
        bench::matches(opt.problem, "union")) {
        for (const int n : bench::sweep(bench::kUnionTriangles, opt)) {
            std::vector<bench::cgal::PolygonType> pieces;
            for (const auto& t : bench::largeTriangles(n)) {
                bench::cgal::PolygonType converted;
                for (const auto& v : t.vertices()) {
                    converted.push_back(bench::cgal::point(v));
                }
                if (converted.is_clockwise_oriented()) {
                    converted.reverse_orientation();
                }
                pieces.push_back(std::move(converted));
            }
            long long result = 0;
            const double us = bench::timeOnce(result, [&] {
                PolygonSet set;
                set.join(pieces.begin(), pieces.end());
                return doubledArea(set);
            });
            bench::emit("Regularized union", "triangles", "union",
                        "CGAL::General_polygon_set_2::join", bench::cgal::kNumber,
                        n, result, us);
        }
    }
    return 0;
}
