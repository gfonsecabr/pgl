// @desc: CGAL reference for the Triangulation and Point constructions
// categories: Delaunay_triangulation_2 over the same random points, its own
// point location, and the point-location hierarchy that CGAL builds alongside
// a second triangulation. The signature is the number of finite faces, which
// is what pgl's triangle count means, so the two are directly comparable.
#include "cgal.hpp"
#include "../sizes.hpp"

#include <CGAL/Delaunay_triangulation_2.h>
#include <CGAL/Triangulation_data_structure_2.h>
#include <CGAL/Triangulation_hierarchy_2.h>
#include <CGAL/Triangulation_vertex_base_2.h>

#include <optional>
#include <vector>

namespace {

using Triangulation = CGAL::Delaunay_triangulation_2<bench::cgal::Kernel>;
using VertexBase = CGAL::Triangulation_vertex_base_2<bench::cgal::Kernel>;
using HierarchyVertexBase = CGAL::Triangulation_hierarchy_vertex_base_2<VertexBase>;
using HierarchyTds = CGAL::Triangulation_data_structure_2<HierarchyVertexBase>;
using HierarchyBase = CGAL::Delaunay_triangulation_2<bench::cgal::Kernel, HierarchyTds>;
using Hierarchy = CGAL::Triangulation_hierarchy_2<HierarchyBase>;

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

        // CGAL's hierarchy is assembled while its triangulation is inserted,
        // rather than being added to an existing Delaunay triangulation. Its
        // construction is therefore the matching preprocessing cost.
        std::optional<Hierarchy> hierarchy;
        if (bench::matches(opt.problem, "buildPointLocation")) {
            const double hierarchyUs = bench::timeOnce(result, [&] {
                hierarchy.emplace(pts.begin(), pts.end());
                return hierarchy->number_of_faces();
            });
            bench::emit("Triangulation", "points", "buildPointLocation",
                        "CGAL::Triangulation_hierarchy_2", bench::cgal::kNumber,
                        n, result, hierarchyUs);
        } else {
            hierarchy.emplace(pts.begin(), pts.end());
        }

        if (bench::matches(opt.problem, "locate")) {
            const double hierarchyLocateUs = bench::timeOnce(result, [&] {
                std::size_t hits = 0;
                for (const auto& q : queries) {
                    const auto face = hierarchy->locate(q);
                    hits += hierarchy->is_infinite(face) ? 0u : 1u;
                }
                return hits;
            });
            bench::emit("Triangulation", "points", "locate",
                        "CGAL::Triangulation_hierarchy_2::locate", bench::cgal::kNumber,
                        n, result, hierarchyLocateUs / bench::kQueryBatch);
        }
    }
    return 0;
}
