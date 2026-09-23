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

    // Two polygons, joined by the free join or by General_polygon_set_2's member
    // join, whichever is faster over the dataset's sweep. The free join's
    // default UsePolylines = Tag_true is about 10% faster on the random polygons
    // and 20% on fpg-holes; the member join's segment traits are about 17% faster
    // on spg.
    const auto twoPolygons = [&](const char* dataset, const std::vector<int>& sizes,
                                 bool member, auto first, auto second) {
        if (!bench::matches(opt.dataset, dataset) || !bench::matches(opt.problem, "union")) {
            return;
        }
        for (const int n : sizes) {
            const auto source = first(n);
            const auto a = bench::cgal::toCgal(source);
            const auto b = bench::cgal::toCgal(second(n, source));
            long long result = 0;
            const double us = bench::timeOnce(result, [&] {
                if (member) {
                    PolygonSet set(a);
                    set.join(b);
                    return vertexCount(set);
                }
                Region joined;
                if (!CGAL::join(a, b, joined)) {
                    return bench::cgal::vertexCount(a) + bench::cgal::vertexCount(b);
                }
                return bench::cgal::vertexCount(joined);
            });
            bench::emit("Regularized union", dataset, "union",
                        member ? "CGAL::General_polygon_set_2::join" : "CGAL::join",
                        bench::cgal::kNumber, n, result, us);
        }
    };
    twoPolygons("large + large", bench::sweep(bench::kUnionPair, opt), false,
                [](int n) { return bench::randomPolygon(n, 1); },
                [](int n, const bench::IntPolygon&) { return bench::randomPolygon(n, 2); });
    if (bench::matches(opt.dataset, "spg")) {
        twoPolygons("spg", bench::sbpdSizes("spg", bench::sweep(bench::kUnionPair, opt)),
                    true,
                    [](int n) { return bench::sbpdPolygon("spg", n); },
                    [](int, const bench::IntPolygon& a) { return bench::quarterTurn(a); });
    }
    for (const char* dataset : bench::kSbpdRegionDatasets) {
        if (!bench::matches(opt.dataset, dataset)) continue;
        twoPolygons(dataset, bench::sbpdSizes(dataset, bench::sweep(bench::kUnionPair, opt)),
                    false,
                    [dataset](int n) { return bench::sbpdRegion(dataset, n); },
                    [](int, const bench::IntRegion& a) { return bench::quarterTurn(a); });
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
