// @desc: CGAL reference for the Union category: CGAL's Boolean set operations
// over the same operands. The signature is the total number of boundary
// vertices in CGAL's result, the same output-size measure reported by pgl's
// driver. It is not expected to match pgl exactly: the libraries canonicalize
// collinear boundary vertices differently.
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

long long vertexCount(const PolygonSet& set) {
    std::vector<Region> components;
    set.polygons_with_holes(std::back_inserter(components));
    long long total = 0;
    for (const auto& component : components) {
        if (component.is_unbounded()) continue;
        total += bench::cgal::vertexCount(component);
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
                // The free join's default UsePolylines = Tag_true: about 10%
                // faster here than the segment traits the member join uses.
                Region joined;
                if (!CGAL::join(a, b, joined)) {
                    return static_cast<long long>(a.size() + b.size());
                }
                return bench::cgal::vertexCount(joined);
            });
            bench::emit("Regularized union", "large + large", "union",
                        "CGAL::join", bench::cgal::kNumber,
                        n, result, us);
        }
    }

    // The same n / 3 triangles, straight from the shared generator: integer
    // vertices, so nothing is converted through pgl on the way in and the two
    // libraries union the identical set. Each emitted n is the input's number
    // of vertices, as it is for the two-polygon dataset.
    if (bench::matches(opt.dataset, "triangles") &&
        bench::matches(opt.problem, "union")) {
        for (const int n : bench::sweep(bench::kUnionTriangles, opt)) {
            std::vector<bench::cgal::PolygonType> pieces;
            for (const auto& t : bench::largeTriangles(n / 3)) {
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
                // Not the free join: its default UsePolylines = Tag_true is
                // about 40% slower on triangles than these segment traits. The
                // third argument is the divide-and-conquer fan-in, undocumented
                // and 5 by default; 3 is the fastest here.
                PolygonSet set;
                set.join(pieces.begin(), pieces.end(), 3);
                return vertexCount(set);
            });
            bench::emit("Regularized union", "triangles", "union",
                        "CGAL::General_polygon_set_2::join", bench::cgal::kNumber,
                        n, result, us);
        }
    }
    return 0;
}
