// @desc: CGAL reference for the Polygon partition category: the constrained
// Delaunay triangulation of the same polygons, their Hertel–Mehlhorn convex
// partition, and their optimal convex partition. The signature is the number
// of pieces, as in pgl's driver.
//
// The triangulation inserts each ring as one closed polyline and keeps the
// faces mark_domain_in_triangulation puts in the domain. Inserting the vertices
// first and the edges between their handles afterwards, or the edges as index
// pairs over the vertex range, costs between one and a half and three times as
// much; the intersection tag makes no difference. Its face count must equal
// pgl's triangle count.
//
// The convex partition is approx_convex_partition_2, Hertel–Mehlhorn over a
// constrained triangulation, the algorithm pgl's convexPartition runs. CGAL's
// other approximation, greene_approx_convex_partition_2, is faster, but its
// y-monotone partition fails an assertion (partition_y_monotone_2.h, `it !=
// tree.end()`) on the random polygon at 3 of its 32 sizes, and without
// assertions it crashes there, taking the driver with it. Neither approximation
// is unique, and CGAL starts from a different triangulation than pgl, so these
// counts are not expected to equal pgl's.
//
// The optimal partition is optimal_convex_partition_2, built with the fix of
// CGAL pull request 9685 (see CGAL_PATCHES in ../../run_asymptotic.py): without
// it the answer can exceed the optimum. Every polygon is handed over starting
// at its lexicographically smallest vertex, which is what pull request 9349
// (merged, not yet released) does inside the function, since other starting
// vertices can give pieces that are not simple. Its count must equal pgl's,
// and on fpg and spg it does at every size.
//
// Not on the random polygon, whose integer vertices are collinear in thousands
// of triples, which is the case CGAL documents at the top of
// partition_optimal_convex_2.h as giving more pieces than the optimum. Over the
// checked-in sweep it answers wrongly at 14 of the 32 sizes, with or without
// pull request 9685: at 5 it returns a valid partition with one or two pieces
// more than pgl's, and at the other 9 one that convex_partition_is_valid_2
// rejects, with fewer pieces — as few as 121 where pgl's has 4,010, every one
// of pgl's checked to lie inside the polygon with interiors pairwise disjoint
// and areas summing to the polygon's. So the random polygon has no optimal
// partition row here.
//
// CGAL's partitions take simple polygons only, so fpg-holes has a
// triangulation row and nothing else.
//
// Swept under both kernels: every decision here is an orientation or a
// coordinate comparison of input vertices, and no constraint crosses another,
// so nothing is constructed. Both must report the same counts; see cgal.hpp.
#include "cgal.hpp"
#include "../sizes.hpp"

#include <CGAL/Constrained_Delaunay_triangulation_2.h>
#include <CGAL/Delaunay_mesh_face_base_2.h>
#include <CGAL/Partition_traits_2.h>
#include <CGAL/mark_domain_in_triangulation.h>
#include <CGAL/partition_2.h>

#include <algorithm>
#include <iterator>
#include <list>
#include <map>
#include <type_traits>
#include <vector>

namespace {

constexpr const char* kCategory = "Polygon partition";

// The random polygon, generated once for both kernels, as in pgl's driver.
const bench::IntPolygon& largePolygon(int n) {
    static std::map<int, bench::IntPolygon> cache;
    auto it = cache.find(n);
    if (it == cache.end()) {
        it = cache.emplace(n, bench::randomPolygon(n)).first;
    }
    return it->second;
}

template <class K>
using Triangulation = CGAL::Constrained_Delaunay_triangulation_2<
    K,
    CGAL::Triangulation_data_structure_2<CGAL::Triangulation_vertex_base_2<K>,
                                         CGAL::Delaunay_mesh_face_base_2<K>>,
    CGAL::No_constraint_intersection_tag>;

template <class K>
void insertRing(Triangulation<K>& triangulation, const CGAL::Polygon_2<K>& ring) {
    triangulation.insert_constraint(ring.vertices_begin(), ring.vertices_end(), true);
}

template <class K>
void insertRings(Triangulation<K>& triangulation, const CGAL::Polygon_2<K>& polygon) {
    insertRing(triangulation, polygon);
}

template <class K>
void insertRings(Triangulation<K>& triangulation,
                 const CGAL::Polygon_with_holes_2<K>& region) {
    insertRing(triangulation, region.outer_boundary());
    for (const auto& hole : region.holes()) {
        insertRing(triangulation, hole);
    }
}

// The polygon as the partition functions take it: counterclockwise, starting
// at its lexicographically smallest vertex.
template <class K>
std::vector<typename CGAL::Partition_traits_2<K>::Point_2>
partitionInput(const CGAL::Polygon_2<K>& polygon) {
    std::vector<typename CGAL::Partition_traits_2<K>::Point_2> vertices(
        polygon.vertices_begin(), polygon.vertices_end());
    std::rotate(vertices.begin(), std::min_element(vertices.begin(), vertices.end()),
                vertices.end());
    return vertices;
}

template <class K, class Generate>
void sweepDataset(const bench::Options& opt, const char* dataset,
                  const std::vector<int>& sizes, Generate generate, bool optimal = true) {
    if (!bench::matches(opt.dataset, dataset)) return;
    const char* number = bench::cgal::numberName<K>;
    using Traits = CGAL::Partition_traits_2<K>;

    for (const int n : sizes) {
        const auto& source = generate(n);
        const auto polygon = bench::cgal::toCgal<K>(source);
        long long result = 0;

        if (bench::matches(opt.problem, "triangulation")) {
            const double us = bench::timeOnce(result, [&] {
                Triangulation<K> triangulation;
                insertRings(triangulation, polygon);
                CGAL::mark_domain_in_triangulation(triangulation);
                std::size_t inside = 0;
                for (const auto face : triangulation.finite_face_handles()) {
                    inside += face->is_in_domain() ? 1u : 0u;
                }
                return inside;
            });
            bench::emit(kCategory, dataset, "triangulation",
                        "CGAL::Constrained_Delaunay_triangulation_2", number, n, result, us);
        }

        if constexpr (std::is_same_v<std::remove_cvref_t<decltype(source)>, bench::IntPolygon>) {
            const auto vertices = partitionInput(polygon);

            if (bench::matches(opt.problem, "convex partition")) {
                const double us = bench::timeOnce(result, [&] {
                    std::list<typename Traits::Polygon_2> pieces;
                    CGAL::approx_convex_partition_2(vertices.begin(), vertices.end(),
                                                    std::back_inserter(pieces), Traits());
                    return pieces.size();
                });
                bench::emit(kCategory, dataset, "convex partition",
                            "CGAL::approx_convex_partition_2", number, n, result, us);
            }

            if (optimal && bench::matches(opt.problem, "optimal convex partition")) {
                const double us = bench::timeOnce(result, [&] {
                    std::list<typename Traits::Polygon_2> pieces;
                    CGAL::optimal_convex_partition_2(vertices.begin(), vertices.end(),
                                                     std::back_inserter(pieces), Traits());
                    return pieces.size();
                });
                bench::emit(kCategory, dataset, "optimal convex partition",
                            "CGAL::optimal_convex_partition_2", number, n, result, us);
            }
        }
    }
}

template <class K>
void run(const bench::Options& opt) {
    if (!bench::cgal::selected<K>(opt)) return;
    sweepDataset<K>(opt, "large", bench::sweep(bench::kPartition, opt), largePolygon,
                    /*optimal=*/false);
    for (const char* dataset : bench::kSbpdDatasets) {
        sweepDataset<K>(opt, dataset,
                        bench::sbpdSizes(dataset, bench::sweep(bench::kPartition, opt)),
                        [dataset](int n) { return bench::sbpdPolygon(dataset, n); });
    }
    for (const char* dataset : bench::kSbpdRegionDatasets) {
        sweepDataset<K>(opt, dataset,
                        bench::sbpdSizes(dataset, bench::sweep(bench::kPartition, opt)),
                        [dataset](int n) { return bench::sbpdRegion(dataset, n); });
    }
}

}  // namespace

int main(int argc, char** argv) {
    const auto opt = bench::parseOptions(argc, argv);
    bench::header();
    run<bench::cgal::Inexact>(opt);
    run<bench::cgal::Kernel>(opt);
    return 0;
}
