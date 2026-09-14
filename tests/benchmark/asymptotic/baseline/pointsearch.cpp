// @desc: CGAL reference for the Point search category: a Kd_tree over the same
// points, answering the same rectangle, triangle and nearest-neighbour
// queries pgl's ShapeTree answers.
//
// Four things need saying about how the rows below are made comparable.
//
// CGAL's kd-tree builds lazily -- the constructor only stores the points, and
// the hierarchy appears on the first query. `build()` is therefore called
// inside the timed region, so the build row measures what pgl's constructor
// measures rather than a copy of a vector.
//
// CGAL ships a query item for an axis-aligned box (Fuzzy_iso_box, with zero
// fuzz here, which makes it the closed box pgl's Rectangle::contains is) but
// none for a triangle. TriangleQuery below is that missing item, and it is
// written to descend the way ShapeTree descends: prune a node whose rectangle
// misses the triangle, take a whole subtree whose rectangle lies inside it,
// test the rest point by point. Both tests are exact.
//
// The tree is not CGAL's default one. Each query runs on the splitter and
// bucket size that answer it fastest, a tree of its own, and the build row
// reports the fastest build among those trees; see kdtree.hpp.
//
// The sweep runs under both kernels -- EPICK as the reference for pgl's `int`
// column, EPECK for `ERational`. The counting rows are exact under either: a
// node's splitting value is a construction, but it is only ever compared
// against, so rounding it changes which points land in which cell and never
// which points the query returns; the per-point containment test that settles
// the answer reads the input coordinates. Nearest neighbour is exact too --
// the squared distances it orders stay under 10^9 on the random points and
// under 2 * 10^10 on euro-night, both far inside double's exact range, and they
// are what the row's signature sums.
#include "cgal.hpp"
#include "kdtree.hpp"
#include "../sizes.hpp"

#include <CGAL/Fuzzy_iso_box.h>
#include <CGAL/Kd_tree_rectangle.h>
#include <CGAL/Orthogonal_k_neighbor_search.h>
#include <CGAL/intersections.h>

#include <algorithm>
#include <cstddef>
#include <deque>
#include <iterator>
#include <limits>
#include <type_traits>
#include <vector>

namespace {

/**
 * A triangle as a kd-tree query item, exactly the way Fuzzy_iso_box is one for
 * a box. Zero fuzz: every test is exact, so the count it produces is the count
 * pgl's countIntersecting produces on the same operands.
 */
template <class K>
class TriangleQuery {
  public:
    using Traits  = CGAL::Search_traits_2<K>;
    using D       = typename Traits::Dimension;
    using Point_d = typename K::Point_2;
    using TreeRect = CGAL::Kd_tree_rectangle<typename K::FT, D>;

    explicit TriangleQuery(const bench::IntTriangle& t)
        : triangle_(bench::cgal::point<K>(t[0]), bench::cgal::point<K>(t[1]),
                    bench::cgal::point<K>(t[2])) {}

    /** Closed containment, as pgl's Triangle::contains is. */
    bool contains(const Point_d& p) const {
        return !triangle_.has_on_unbounded_side(p);
    }

    /** Does this node's cell meet the triangle at all? If not, prune it. */
    bool inner_range_intersects(const TreeRect& r) const {
        return CGAL::do_intersect(triangle_, box(r));
    }

    /** Is this node's cell wholly inside? If so, take the subtree whole. */
    bool outer_range_contains(const TreeRect& r) const {
        // The triangle is convex, so it holds the cell exactly when it holds
        // all four corners.
        return contains(Point_d(r.min_coord(0), r.min_coord(1))) &&
               contains(Point_d(r.min_coord(0), r.max_coord(1))) &&
               contains(Point_d(r.max_coord(0), r.min_coord(1))) &&
               contains(Point_d(r.max_coord(0), r.max_coord(1)));
    }

  private:
    static typename K::Iso_rectangle_2 box(const TreeRect& r) {
        return typename K::Iso_rectangle_2(
            Point_d(r.min_coord(0), r.min_coord(1)),
            Point_d(r.max_coord(0), r.max_coord(1)));
    }

    typename K::Triangle_2 triangle_;
};

/**
 * The output iterator Kd_tree::search writes its answers to, counting them
 * instead of storing them. A plain lambda will not do: the search assigns the
 * iterator to itself as it descends, and a capturing closure is not
 * assignable.
 */
class CountingIterator {
  public:
    using iterator_category = std::output_iterator_tag;
    using value_type        = void;
    using difference_type   = std::ptrdiff_t;
    using pointer           = void;
    using reference         = void;

    explicit CountingIterator(std::size_t& counter) : counter_(&counter) {}

    // Templated on the point type, so one iterator serves both kernels.
    template <class P>
    CountingIterator& operator=(const P&)  { ++*counter_; return *this; }
    CountingIterator& operator*()          { return *this; }
    CountingIterator& operator++()         { return *this; }
    CountingIterator  operator++(int)      { return *this; }

  private:
    std::size_t* counter_;
};

/** The number of points the query selects, without materializing them. */
template <class Tree, class Query>
std::size_t countIn(const Tree& tree, const Query& q) {
    std::size_t found = 0;
    tree.search(CountingIterator(found), q);
    return found;
}

template <class K>
void run(const bench::Options& opt, const bench::PointDataset& dataset) {
    if (!bench::cgal::selected<K>(opt)) return;
    const char* number = bench::cgal::numberName<K>;

    using Tuning        = bench::cgal::KdTreeTuning<K>;
    using Traits        = CGAL::Search_traits_2<K>;
    using RectangleTree = bench::cgal::KdTree<K, typename Tuning::RectangleSplitter>;
    using TriangleTree  = bench::cgal::KdTree<K, typename Tuning::TriangleSplitter>;
    using NearestTree   = bench::cgal::KdTree<K, typename Tuning::NearestSplitter>;
    using Box           = CGAL::Fuzzy_iso_box<Traits>;
    using Search        = CGAL::Orthogonal_k_neighbor_search<
        Traits, CGAL::Euclidean_distance<Traits>, typename Tuning::NearestSplitter,
        NearestTree>;

    static_assert(std::is_same_v<typename Search::Tree, NearestTree>,
                  "the neighbour search must query the tree the build row measured");

    // The query batches, converted once: the same shapes, in the same order,
    // that the pgl driver hands its tree.
    //
    // A deque, not a vector, and emplaced rather than pushed: Fuzzy_iso_box
    // keeps iterators into its own corner members, so copying or moving one
    // leaves it pointing into the object it came from. A deque constructs each
    // element at its final address and never relocates it.
    std::deque<Box> boxes;
    for (const auto& r : dataset.queryRectangles(bench::kQueryBatch)) {
        boxes.emplace_back(bench::cgal::point<K>(r.min()), bench::cgal::point<K>(r.max()));
    }
    std::vector<TriangleQuery<K>> triangles;
    for (const auto& t : dataset.queryTriangles(bench::kQueryBatch)) {
        triangles.emplace_back(t);
    }
    const auto queries = bench::cgal::points<K>(dataset.queryPoints(bench::kQueryBatch));

    for (const int n : bench::sweep(bench::kPointSearch, opt)) {
        const auto pts = bench::cgal::points<K>(dataset.points(n));
        long long result = 0;

        // One tree per query, and the fastest of their builds is the build row.
        // Each batch is timed once, so the state of the tree it runs on shows:
        // a tree left cold behind another tree's build pays for that in its
        // first queries. So each tree is built immediately before its own batch
        // and freed after it, and the nearest-neighbour tree first answers the
        // two counting batches untimed -- the state pgl's single tree is in when
        // its nearest-neighbour batch runs, after the counting ones.
        double buildUs = std::numeric_limits<double>::infinity();
        const auto build = [&](auto& tree) {
            buildUs = std::min(buildUs, bench::timeOnce(result, [&] {
                tree.build();
                return tree.size();
            }));
            bench::require(tree.is_built(), "the kd-tree was never built");
        };

        // As in pgl's driver, the signature is the total count over the batch,
        // so it is directly comparable with the ShapeTree rows'.
        const auto measure = [&](const char* problem, const auto& tree, const auto& shapes) {
            if (!bench::matches(opt.problem, problem)) return;
            const double us = bench::timeOnce(result, [&] {
                std::size_t total = 0;
                for (const auto& q : shapes) {
                    total += countIn(tree, q);
                }
                return total;
            });
            bench::emit("Point search", dataset.name, problem, "CGAL::Kd_tree::search",
                        number, n, result, us / bench::kQueryBatch);
        };
        {
            RectangleTree tree(pts.begin(), pts.end(),
                               typename Tuning::RectangleSplitter(Tuning::rectangleBucket));
            build(tree);
            measure("count in Rectangle", tree, boxes);
        }
        {
            TriangleTree tree(pts.begin(), pts.end(),
                              typename Tuning::TriangleSplitter(Tuning::triangleBucket));
            build(tree);
            measure("count in Triangle", tree, triangles);
        }
        {
            NearestTree tree(pts.begin(), pts.end(),
                             typename Tuning::NearestSplitter(Tuning::nearestBucket));
            build(tree);
            if (bench::matches(opt.problem, "nearest neighbor")) {
                for (const auto& q : boxes) countIn(tree, q);
                for (const auto& q : triangles) countIn(tree, q);
                const double us = bench::timeOnce(result, [&] {
                    double sum = 0;
                    for (const auto& q : queries) {
                        Search search(tree, q, 1);
                        sum += CGAL::to_double(search.begin()->second);
                    }
                    return sum;
                });
                // The same checksum over the answers' distances the pgl driver
                // takes, and the same output column: a nearest neighbour is one
                // point however large the tree is, so these rows report the
                // tree they searched.
                //
                // Summing the distance rather than the point is what makes this
                // row match to the digit like every other one. A query
                // equidistant from two points has two correct answers and the
                // two libraries need not pick the same one -- over the checked-
                // in sweep that happens to a query or two of the thousand at
                // any size -- but both answers are the same distance from the
                // query, and that is the number being summed. The search
                // already computed it, so reading it costs the baseline
                // nothing.
                bench::emit("Point search", dataset.name, "nearest neighbor",
                            "CGAL::Orthogonal_k_neighbor_search", number,
                            n, result, static_cast<long long>(tree.size()),
                            us / bench::kQueryBatch);
            }
        }
        if (bench::matches(opt.problem, "build")) {
            bench::emit("Point search", dataset.name, "build", "CGAL::Kd_tree",
                        number, n, static_cast<long long>(pts.size()), buildUs);
        }
    }
}

}  // namespace

int main(int argc, char** argv) {
    const auto opt = bench::parseOptions(argc, argv);
    bench::header();
    for (const auto& dataset : bench::pointDatasets()) {
        if (!bench::matches(opt.dataset, dataset.name)) continue;
        run<bench::cgal::Inexact>(opt, dataset);
        run<bench::cgal::Kernel>(opt, dataset);
    }
    return 0;
}
