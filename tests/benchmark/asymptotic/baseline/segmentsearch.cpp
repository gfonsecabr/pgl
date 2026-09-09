// @desc: CGAL reference for the Segment search category: an AABB_tree over the
// same random segments, counting the ones a query rectangle or triangle meets.
//
// This category already compares two pgl structures against each other --
// ShapeTree and IntervalTree answer the same question and their signatures have
// to agree at every size. CGAL's bounding-volume hierarchy is a third,
// independent answer to it, so the agreement it joins is a real check rather
// than two implementations sharing one bug.
//
// AABB_tree is the right analogue for the counting rows in particular:
// number_of_intersected_primitives asks exactly what countIntersecting asks,
// and the queries it accepts include the iso rectangle and the triangle this
// category uses. Both libraries treat those queries as closed filled regions,
// so a segment lying wholly inside one counts for both.
//
// Like CGAL's kd-tree, the hierarchy is built lazily -- the first query would
// otherwise pay for it -- so build() is called inside the timed region, and the
// query rows are measured against a tree that is already standing.
//
// The sweep runs under both kernels -- EPICK as the reference for pgl's `int`
// column, EPECK for `ERational`. The counts are exact under either: the
// hierarchy's bounding boxes are already double intervals rounded outwards
// whatever the kernel, so they only ever prune conservatively, and the
// do_intersect that settles each candidate reads the input coordinates.
#include "cgal.hpp"
#include "../sizes.hpp"

#include <CGAL/AABB_segment_primitive_2.h>
#include <CGAL/AABB_traits_2.h>
#include <CGAL/AABB_tree.h>

#include <span>
#include <vector>

namespace {

template <class K>
std::vector<typename K::Segment_2> segments(const std::vector<bench::IntSegment>& in) {
    std::vector<typename K::Segment_2> out;
    out.reserve(in.size());
    for (const auto& s : in) {
        out.emplace_back(bench::cgal::point<K>(s[0]), bench::cgal::point<K>(s[1]));
    }
    return out;
}

template <class K>
void sweepDataset(const bench::Options& opt, const char* dataset,
                  std::span<const int> sizes,
                  std::vector<bench::IntSegment> (*generate)(int)) {
    if (!bench::matches(opt.dataset, dataset)) return;
    if (!bench::cgal::selected<K>(opt)) return;
    const char* number = bench::cgal::numberName<K>;

    using Segment   = typename K::Segment_2;
    using Iterator  = typename std::vector<Segment>::const_iterator;
    using Primitive = CGAL::AABB_segment_primitive_2<K, Iterator>;
    using Traits    = CGAL::AABB_traits_2<K, Primitive>;
    using Tree      = CGAL::AABB_tree<Traits>;

    // The same query shapes the pgl driver uses, in the same order, so the two
    // read the same prefix of the batch.
    std::vector<typename K::Iso_rectangle_2> rectangles;
    for (const auto& r : bench::queryRectangles(bench::kQueryBatch)) {
        rectangles.emplace_back(bench::cgal::point<K>(r.min()),
                                bench::cgal::point<K>(r.max()));
    }
    std::vector<typename K::Triangle_2> triangles;
    for (const auto& t : bench::queryTriangles(bench::kQueryBatch)) {
        triangles.emplace_back(bench::cgal::point<K>(t[0]), bench::cgal::point<K>(t[1]),
                               bench::cgal::point<K>(t[2]));
    }

    for (const int n : bench::sweep(sizes, opt)) {
        // The primitives reference the segments through iterators, so this
        // vector has to outlive the tree built over it.
        const auto segs = segments<K>(generate(n));
        long long result = 0;

        Tree tree(segs.begin(), segs.end());
        const double buildUs = bench::timeOnce(result, [&] {
            tree.build();
            return tree.size();
        });
        if (bench::matches(opt.problem, "build")) {
            bench::emit("Segment search", dataset, "build", "CGAL::AABB_tree",
                        number, n, result, buildUs);
        }
        bench::require(!tree.empty() && tree.size() == segs.size(),
                       "the AABB tree does not hold the whole dataset");

        // As in pgl's driver: the short batch, and the signature is the total
        // count over it -- the same number both pgl structures report.
        const auto measure = [&](const char* problem, const auto& shapes) {
            if (!bench::matches(opt.problem, problem)) return;
            const double us = bench::timeOnce(result, [&] {
                std::size_t total = 0;
                for (int i = 0; i < bench::kSlowQueryBatch; ++i) {
                    total += tree.number_of_intersected_primitives(
                        shapes[static_cast<std::size_t>(i)]);
                }
                return total;
            });
            bench::emit("Segment search", dataset, problem,
                        "CGAL::AABB_tree::number_of_intersected_primitives",
                        number, n, result, us / bench::kSlowQueryBatch);
        };
        measure("count in Rectangle", rectangles);
        measure("count in Triangle", triangles);
    }
}

}  // namespace

int main(int argc, char** argv) {
    const auto opt = bench::parseOptions(argc, argv);
    bench::header();
    sweepDataset<bench::cgal::Inexact>(opt, "small segments", bench::kSegmentSearch,
                                       bench::smallSegments);
    sweepDataset<bench::cgal::Kernel>(opt, "small segments", bench::kSegmentSearch,
                                      bench::smallSegments);
    return 0;
}
