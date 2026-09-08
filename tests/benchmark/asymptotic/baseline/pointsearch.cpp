// @desc: CGAL reference for the Point search category: a Kd_tree over the same
// random points, answering the same rectangle, triangle and nearest-neighbour
// queries pgl's ShapeTree answers.
//
// Two things need saying about how the rows below are made comparable.
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
#include "cgal.hpp"
#include "../sizes.hpp"

#include <CGAL/Fuzzy_iso_box.h>
#include <CGAL/Kd_tree.h>
#include <CGAL/Kd_tree_rectangle.h>
#include <CGAL/Orthogonal_k_neighbor_search.h>
#include <CGAL/Search_traits_2.h>
#include <CGAL/intersections.h>

#include <cstddef>
#include <deque>
#include <iterator>
#include <type_traits>
#include <vector>

namespace {

using Kernel   = bench::cgal::Kernel;
using Point    = bench::cgal::Point;
using FT       = Kernel::FT;
using Traits   = CGAL::Search_traits_2<Kernel>;
using Tree     = CGAL::Kd_tree<Traits>;
using Box      = CGAL::Fuzzy_iso_box<Traits>;
using Search   = CGAL::Orthogonal_k_neighbor_search<Traits>;
using TreeRect = CGAL::Kd_tree_rectangle<FT, Traits::Dimension>;

static_assert(std::is_same_v<Search::Tree, Tree>,
              "the neighbour search must query the same tree the build row measured");

/**
 * A triangle as a kd-tree query item, exactly the way Fuzzy_iso_box is one for
 * a box. Zero fuzz: every test is exact, so the count it produces is the count
 * pgl's countIntersecting produces on the same operands.
 */
class TriangleQuery {
  public:
    using D       = Traits::Dimension;
    using Point_d = Point;

    explicit TriangleQuery(const bench::IntTriangle& t)
        : triangle_(bench::cgal::point(t[0]), bench::cgal::point(t[1]),
                    bench::cgal::point(t[2])) {}

    /** Closed containment, as pgl's Triangle::contains is. */
    bool contains(const Point& p) const {
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
        return contains(Point(r.min_coord(0), r.min_coord(1))) &&
               contains(Point(r.min_coord(0), r.max_coord(1))) &&
               contains(Point(r.max_coord(0), r.min_coord(1))) &&
               contains(Point(r.max_coord(0), r.max_coord(1)));
    }

  private:
    static Kernel::Iso_rectangle_2 box(const TreeRect& r) {
        return Kernel::Iso_rectangle_2(Point(r.min_coord(0), r.min_coord(1)),
                                       Point(r.max_coord(0), r.max_coord(1)));
    }

    Kernel::Triangle_2 triangle_;
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

    CountingIterator& operator=(const Point&) { ++*counter_; return *this; }
    CountingIterator& operator*()             { return *this; }
    CountingIterator& operator++()            { return *this; }
    CountingIterator  operator++(int)         { return *this; }

  private:
    std::size_t* counter_;
};

/** The number of points the query selects, without materializing them. */
template <class Query>
std::size_t countIn(const Tree& tree, const Query& q) {
    std::size_t found = 0;
    tree.search(CountingIterator(found), q);
    return found;
}

}  // namespace

int main(int argc, char** argv) {
    const auto opt = bench::parseOptions(argc, argv);
    bench::header();
    if (!bench::matches(opt.dataset, "points")) return 0;

    // The query batches, converted once: the same shapes, in the same order,
    // that the pgl driver hands its tree.
    //
    // A deque, not a vector, and emplaced rather than pushed: Fuzzy_iso_box
    // keeps iterators into its own corner members, so copying or moving one
    // leaves it pointing into the object it came from. A deque constructs each
    // element at its final address and never relocates it.
    std::deque<Box> boxes;
    for (const auto& r : bench::queryRectangles(bench::kQueryBatch)) {
        boxes.emplace_back(bench::cgal::point(r.min()), bench::cgal::point(r.max()));
    }
    std::vector<TriangleQuery> triangles;
    for (const auto& t : bench::queryTriangles(bench::kQueryBatch)) {
        triangles.emplace_back(t);
    }
    const auto queries = bench::cgal::points(bench::queryPoints(bench::kQueryBatch));

    for (const int n : bench::sweep(bench::kPointSearch, opt)) {
        const auto pts = bench::cgal::points(bench::points(n));
        long long result = 0;

        Tree tree(pts.begin(), pts.end());
        const double buildUs = bench::timeOnce(result, [&] {
            tree.build();
            return tree.size();
        });
        if (bench::matches(opt.problem, "build")) {
            bench::emit("Point search", "points", "build", "CGAL::Kd_tree",
                        bench::cgal::kNumber, n, result, buildUs);
        }
        bench::require(tree.is_built(), "the kd-tree was never built");

        // As in pgl's driver, the signature is the total count over the batch,
        // so it is directly comparable with the ShapeTree rows'.
        const auto measure = [&](const char* problem, const auto& shapes) {
            if (!bench::matches(opt.problem, problem)) return;
            const double us = bench::timeOnce(result, [&] {
                std::size_t total = 0;
                for (const auto& q : shapes) {
                    total += countIn(tree, q);
                }
                return total;
            });
            bench::emit("Point search", "points", problem, "CGAL::Kd_tree::search",
                        bench::cgal::kNumber, n, result, us / bench::kQueryBatch);
        };
        measure("count in Rectangle", boxes);
        measure("count in Triangle", triangles);

        if (bench::matches(opt.problem, "nearest neighbor")) {
            const double us = bench::timeOnce(result, [&] {
                double sum = 0;
                for (const auto& q : queries) {
                    Search search(tree, q, 1);
                    sum += CGAL::to_double(search.begin()->first.x());
                }
                return sum;
            });
            // The same checksum over the answers the pgl driver takes, and the
            // same output column: a nearest neighbour is one point however
            // large the tree is, so these rows report the tree they searched.
            //
            // This is the one signature in the baseline that does not have to
            // match to the digit. A query equidistant from two points has two
            // correct answers, and the two libraries need not pick the same
            // one. Measured over the checked-in sweep, that happens to at most
            // two queries of the thousand at any size, and every time it does
            // the two answers are the same distance away -- so a small
            // disagreement here is a tie, while a large one, or one on any
            // other row, is not.
            bench::emit("Point search", "points", "nearest neighbor",
                        "CGAL::Orthogonal_k_neighbor_search", bench::cgal::kNumber,
                        n, result, static_cast<long long>(tree.size()),
                        us / bench::kQueryBatch);
        }
    }
    return 0;
}
