#pragma once

#include "algorithm/closestpair.hpp"

/**
 * @file shapetree.hpp
 * @brief Static 2D shape tree over any bounded shape (one exposing `bbox()`).
 *
 * The tree stores shapes by value and is built once from a container. Space is
 * split by a coordinate value. At each node the axis and split value are chosen
 * adaptively to minimize `maxChild + straddlers`, balancing the children while
 * keeping few elements stuck at the node; a set of long horizontal segments is
 * thus split horizontally rather than uselessly along their length. Elements
 * lying strictly on one side of the split descend into that
 * child; elements whose bounding box straddles the split value stay at the node
 * itself, so every element is stored exactly once. Each node caches the union
 * bounding box of its whole subtree, allowing queries to prune subtrees with
 * exact integer rectangle predicates. Over an arbitrary-precision coordinate
 * type, where that box test is itself a cross multiplication of big integers,
 * every box is shadowed by an outward-rounded `double` one and the cheap test
 * runs first; a shadow that misses the query proves the exact box does too.
 */

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>
#include <vector>


namespace pgl {

namespace detail {

// Default weight: an empty type that is its own additive identity, so the
// per-node weight sum occupies no storage (via [[no_unique_address]]) and costs
// nothing unless a real weight function is supplied.
struct EmptyWeight {
    constexpr EmptyWeight operator+(const EmptyWeight&) const { return {}; }
};
struct EmptyWeightFn {
    template <class Shape>
    constexpr EmptyWeight operator()(const Shape&) const { return {}; }
};

// Invokes a visitor on one element and reports whether traversal should stop.
// A visitor returning bool stops the traversal as soon as it returns true; a
// visitor returning void never stops. This lets the visit* methods accept both
// plain side-effecting callbacks and early-exit callbacks.
template <class Fn, class Arg>
[[nodiscard]] bool invokeVisitor(Fn& fn, const Arg& arg) {
    if constexpr (std::is_same_v<std::invoke_result_t<Fn&, const Arg&>, bool>) {
        return fn(arg);
    } else {
        fn(arg);
        return false;
    }
}

// Squared distance from q to other, for use in the nearest-neighbor
// branch-and-bound. The requested result type is passed through to exact
// shape pairs. A Disk-related overload may necessarily compute in double; its
// result is then converted to the tree's comparison type.
template <class ResultNumber, class Q, class Other>
[[nodiscard]] ResultNumber nearestSquaredDistance(const Q& q, const Other& other) {
    if constexpr (requires { q.template squaredDistance<ResultNumber>(other); }) {
        return static_cast<ResultNumber>(q.template squaredDistance<ResultNumber>(other));
    } else {
        return static_cast<ResultNumber>(q.squaredDistance(other));
    }
}

// Same fallback dance as nearestSquaredDistance, for the L1 (Manhattan) metric.
template <class ResultNumber, class Q, class Other>
[[nodiscard]] ResultNumber nearestDistanceL1(const Q& q, const Other& other) {
    if constexpr (requires { q.template distanceL1<ResultNumber>(other); }) {
        return static_cast<ResultNumber>(q.template distanceL1<ResultNumber>(other));
    } else {
        return static_cast<ResultNumber>(q.distanceL1(other));
    }
}

// Same fallback dance as nearestSquaredDistance, for the LInf (Chebyshev) metric.
template <class ResultNumber, class Q, class Other>
[[nodiscard]] ResultNumber nearestDistanceLInf(const Q& q, const Other& other) {
    if constexpr (requires { q.template distanceLInf<ResultNumber>(other); }) {
        return static_cast<ResultNumber>(q.template distanceLInf<ResultNumber>(other));
    } else {
        return static_cast<ResultNumber>(q.distanceLInf(other));
    }
}

// Metric tags selecting which detail::nearest*Distance is used by Node::nearest,
// so the branch-and-bound traversal is written once and shared by squared-L2,
// L1 and LInf nearest-neighbor queries.
struct SquaredMetric {
    template <class ResultNumber, class Q, class Other>
    [[nodiscard]] static ResultNumber distance(const Q& q, const Other& other) {
        return nearestSquaredDistance<ResultNumber>(q, other);
    }
};
struct L1Metric {
    template <class ResultNumber, class Q, class Other>
    [[nodiscard]] static ResultNumber distance(const Q& q, const Other& other) {
        return nearestDistanceL1<ResultNumber>(q, other);
    }
};
struct LInfMetric {
    template <class ResultNumber, class Q, class Other>
    [[nodiscard]] static ResultNumber distance(const Q& q, const Other& other) {
        return nearestDistanceLInf<ResultNumber>(q, other);
    }
};

}  // namespace detail

/**
 * @brief Static shape tree of bounded shapes.
 *
 * @tparam S Any shape type exposing `bbox()` (Point, Segment, Triangle,
 *         Rectangle, Convex, Polygon, ...). Infinite shapes such as Line, Ray
 *         and Halfplane have no finite bounding box and are not supported.
 * @tparam WeightFn Callable mapping a `ShapeType` to a weight (any type with
 *         `operator+` whose value-initialization is the additive identity).
 *         Defaults to a no-op returning an empty type, so weights are ignored
 *         unless a real function is supplied.
 */
template <class S, class WeightFn = detail::EmptyWeightFn>
class ShapeTree {
  public:
    using ShapeType = S;
    using WeightFunction = WeightFn;
    using Rect = std::remove_cvref_t<decltype(std::declval<const S&>().bbox())>;
    using PointType = typename Rect::PointType;
    using NumberType = typename PointType::NumberType;
    using WeightType = std::remove_cvref_t<std::invoke_result_t<const WeightFn&, const ShapeType&>>;

    // Container-like aliases so a ShapeTree can be iterated and passed where a
    // container of shapes is expected.
    using value_type = ShapeType;
    using size_type = std::size_t;
    using const_iterator = typename std::vector<ShapeType>::const_iterator;
    using const_reference = const ShapeType&;

  private:
    // ----------------------------------------------------------------------
    // Cheap box tests
    //
    // Every traversal below prunes with bounding boxes before it reaches an
    // exact predicate. Over arbitrary-precision coordinates the box test is
    // itself expensive -- comparing two rationals cross-multiplies big integers
    // -- so each exact box is shadowed by an outward-rounded `double` one,
    // cached beside the node or element it belongs to. A `double` box that
    // misses the query proves the exact box does too, rounding having only
    // grown it; anything else falls through to the exact test unchanged.
    // Fixed-width coordinates compare in a few machine instructions and carry
    // no shadow.
    // ----------------------------------------------------------------------

    using FilterBox = Rectangle<Point<double>>;

    static constexpr bool usesFilter = detail::arbitraryPrecision<NumberType>;

    // Shapes that *are* their own bounding box. A box test in front of an exact
    // predicate against one of these would just run the same test twice.
    template <class T>
    static constexpr bool boxShaped = PointConcept<T> || RectangleConcept<T>;

    // Query shapes whose `bbox()` is defined for every value. An unbounded
    // convex region has no finite box (@ref HalfplaneIntersection::bbox throws
    // for one), and a runtime @ref Shape forwards to whatever it holds, so
    // neither is filtered.
    template <class T>
    static constexpr bool hasTotalBoundingBox =
        requires(const T& t) { t.bbox(); } &&
        !UnboundedConvexConcept<T> && !ShapeConcept<T>;

    // The boxes one query is filtered through, computed once per query.
    template <class QueryRect>
    struct QueryBoxes {
        QueryRect box;
        FilterBox filter;
    };

    // Stands in for @ref QueryBoxes when the query has no box to filter
    // through; every test then goes straight to the exact predicate.
    struct NoQueryBoxes {};

    // One coordinate of a filter box: `below` picks which end of the interval
    // the exact value is known to lie in. That interval is the one
    // @ref detail::approximate charges for reaching a double, so a coordinate
    // costs one conversion and a few flops. @ref Rectangle::fbox would give a
    // *tight* bound instead, at two long-double divisions and an exact
    // comparison against the rational apiece -- accuracy a filter cannot spend
    // on the box it is only trying to reject.
    template <class Coordinate>
    static double filterBound(const Coordinate& value, bool below) {
        const detail::Approximate a = detail::approximate(value);
        const double slack = a.error * detail::approximateMargin + 0x1p-1000;
        return below ? a.value - slack : a.value + slack;
    }

    // The outward-rounded `double` box of `r`. A coordinate too large to reach
    // a finite double leaves a NaN behind, and a NaN bound would answer
    // "disjoint" to everything and prune a subtree that does meet the query;
    // such a box is widened to the whole plane instead, which prunes nothing.
    template <class OtherRect>
    static FilterBox filterBoxOf(const OtherRect& r) {
        const double xmin = filterBound(r.min().x(), true);
        const double ymin = filterBound(r.min().y(), true);
        const double xmax = filterBound(r.max().x(), false);
        const double ymax = filterBound(r.max().y(), false);
        if (std::isnan(xmin) || std::isnan(ymin) || std::isnan(xmax) || std::isnan(ymax)) {
            const double lo = -detail::numeric_limits<double>::infinity();
            const double hi = detail::numeric_limits<double>::infinity();
            return FilterBox(lo, lo, hi, hi, true);
        }
        return FilterBox(xmin, ymin, xmax, ymax, true);
    }

    template <class Q>
    static auto queryBoxesOf(const Q& q) {
        if constexpr (hasTotalBoundingBox<Q>) {
            auto box = q.bbox();
            FilterBox filter{};
            if constexpr (usesFilter) {
                filter = filterBoxOf(box);
            }
            return QueryBoxes<decltype(box)>{std::move(box), filter};
        } else {
            return NoQueryBoxes{};
        }
    }

    struct Node {
        Rect box;  // Union bounding box of the whole subtree.
        std::ptrdiff_t left = -1, right = -1;
        std::size_t count = 0;  // Number of elements in the whole subtree.
        [[no_unique_address]] WeightType weightSum{};  // Sum of subtree weights.
        std::vector<std::size_t> elementIndices;  // Elements owned by this node.

        // This node's index in `tree.nodes_`, which is what addresses its cached
        // filter box. Both are elements of that one array, so the difference is
        // exact; carrying the index down every traversal instead would put a
        // parameter on each of them for it.
        [[nodiscard]] std::size_t index(const ShapeTree& tree) const {
            return static_cast<std::size_t>(this - tree.nodes_.data());
        }

        // Whether the cheap box tests alone already prove this subtree misses
        // `q`. False means undecided, not that the subtree meets the query.
        template <class Q, class QB>
        [[nodiscard]] bool boxMisses(const ShapeTree& tree, const Q&, const QB& qb) const {
            if constexpr (std::is_same_v<QB, NoQueryBoxes> || boxShaped<Q>) {
                return false;  // The exact node test that follows *is* this test.
            } else {
                if constexpr (usesFilter) {
                    if (!qb.filter.intersects(tree.nodeFilterBoxes_[index(tree)])) {
                        return true;
                    }
                }
                return !qb.box.intersects(box);
            }
        }

        // Whether the cheap box tests alone already prove element `i` misses
        // `q` -- and so that it neither meets `q` nor lies inside it.
        template <class Q, class QB>
        [[nodiscard]] bool elementBoxMisses(const ShapeTree& tree, const Q&, const QB& qb,
                                            std::size_t i) const {
            if constexpr (std::is_same_v<QB, NoQueryBoxes> ||
                          (boxShaped<Q> && boxShaped<ShapeType>)) {
                return false;  // The exact element test that follows *is* this test.
            } else if constexpr (usesFilter) {
                return !qb.filter.intersects(tree.filterBoxes_[i]);
            } else {
                return !qb.box.intersects(tree.elements_[i].bbox());
            }
        }

        template <class Q, class QB>
        [[nodiscard]] std::size_t countIntersecting(const ShapeTree& tree, const Q& q,
                                                    const QB& qb) const {
            if (boxMisses(tree, q, qb) || !q.intersects(box)) {
                return 0;
            }
            if (q.contains(box)) {
                // The whole subtree lies inside q, so every element intersects it.
                return count;
            }
            std::size_t ret = 0;
            for (std::size_t i : elementIndices) {
                if (!elementBoxMisses(tree, q, qb, i) && tree.elements_[i].intersects(q)) {
                    ret++;
                }
            }
            if (left != -1) {
                ret += tree.nodes_[left].countIntersecting(tree, q, qb);
            }
            if (right != -1) {
                ret += tree.nodes_[right].countIntersecting(tree, q, qb);
            }
            return ret;
        }

        template <class Q, class QB>
        [[nodiscard]] WeightType sumIntersecting(const ShapeTree& tree, const Q& q,
                                                 const QB& qb) const {
            if (boxMisses(tree, q, qb) || !q.intersects(box)) {
                return WeightType{};
            }
            if (q.contains(box)) {
                // The whole subtree lies inside q, so it contributes its full sum.
                return weightSum;
            }
            WeightType ret{};
            for (std::size_t i : elementIndices) {
                if (!elementBoxMisses(tree, q, qb, i) && tree.elements_[i].intersects(q)) {
                    ret = ret + tree.weight_(tree.elements_[i]);
                }
            }
            if (left != -1) {
                ret = ret + tree.nodes_[left].sumIntersecting(tree, q, qb);
            }
            if (right != -1) {
                ret = ret + tree.nodes_[right].sumIntersecting(tree, q, qb);
            }
            return ret;
        }

        // Appends every element in this subtree, with no intersection test.
        void collectAll(const ShapeTree& tree, std::vector<ShapeType>& out) const {
            for (std::size_t i : elementIndices) {
                out.push_back(tree.elements_[i]);
            }
            if (left != -1) {
                tree.nodes_[left].collectAll(tree, out);
            }
            if (right != -1) {
                tree.nodes_[right].collectAll(tree, out);
            }
        }

        template <class Q, class QB>
        void reportIntersecting(const ShapeTree& tree, const Q& q, const QB& qb,
                                std::vector<ShapeType>& out) const {
            if (boxMisses(tree, q, qb) || !q.intersects(box)) {
                return;
            }
            if (q.contains(box)) {
                // The whole subtree lies inside q, so every element intersects it.
                collectAll(tree, out);
                return;
            }
            for (std::size_t i : elementIndices) {
                if (!elementBoxMisses(tree, q, qb, i) && tree.elements_[i].intersects(q)) {
                    out.push_back(tree.elements_[i]);
                }
            }
            if (left != -1) {
                tree.nodes_[left].reportIntersecting(tree, q, qb, out);
            }
            if (right != -1) {
                tree.nodes_[right].reportIntersecting(tree, q, qb, out);
            }
        }

        // Calls fn on every element in this subtree, with no intersection test.
        // Returns true as soon as fn requests a stop (see detail::invokeVisitor).
        template <class Fn>
        [[nodiscard]] bool visitAll(const ShapeTree& tree, Fn& fn) const {
            for (std::size_t i : elementIndices) {
                if (detail::invokeVisitor(fn, tree.elements_[i])) {
                    return true;
                }
            }
            if (left != -1 && tree.nodes_[left].visitAll(tree, fn)) {
                return true;
            }
            if (right != -1 && tree.nodes_[right].visitAll(tree, fn)) {
                return true;
            }
            return false;
        }

        template <class Q, class QB, class Fn>
        [[nodiscard]] bool visitIntersecting(const ShapeTree& tree, const Q& q, const QB& qb,
                                             Fn& fn) const {
            if (boxMisses(tree, q, qb) || !q.intersects(box)) {
                return false;
            }
            if (q.contains(box)) {
                // The whole subtree lies inside q, so every element intersects it.
                return visitAll(tree, fn);
            }
            for (std::size_t i : elementIndices) {
                if (!elementBoxMisses(tree, q, qb, i) && tree.elements_[i].intersects(q) &&
                    detail::invokeVisitor(fn, tree.elements_[i])) {
                    return true;
                }
            }
            if (left != -1 && tree.nodes_[left].visitIntersecting(tree, q, qb, fn)) {
                return true;
            }
            if (right != -1 && tree.nodes_[right].visitIntersecting(tree, q, qb, fn)) {
                return true;
            }
            return false;
        }

        template <class Q, class QB>
        [[nodiscard]] bool anyIntersecting(const ShapeTree& tree, const Q& q,
                                           const QB& qb) const {
            if (boxMisses(tree, q, qb) || !q.intersects(box)) {
                return false;
            }
            if (q.contains(box)) {
                // The whole (non-empty) subtree lies inside q.
                return true;
            }
            for (std::size_t i : elementIndices) {
                if (!elementBoxMisses(tree, q, qb, i) && tree.elements_[i].intersects(q)) {
                    return true;
                }
            }
            if (left != -1 && tree.nodes_[left].anyIntersecting(tree, q, qb)) {
                return true;
            }
            if (right != -1 && tree.nodes_[right].anyIntersecting(tree, q, qb)) {
                return true;
            }
            return false;
        }

        // --- containment: stored element contained in the query (element ⊆ q) ---

        template <class Q, class QB>
        [[nodiscard]] std::size_t countContainedIn(const ShapeTree& tree, const Q& q,
                                                   const QB& qb) const {
            if (boxMisses(tree, q, qb) || !q.intersects(box)) {
                return 0;
            }
            if (q.contains(box)) {
                // Every element ⊆ box ⊆ q.
                return count;
            }
            std::size_t ret = 0;
            for (std::size_t i : elementIndices) {
                if (!elementBoxMisses(tree, q, qb, i) && q.contains(tree.elements_[i])) {
                    ret++;
                }
            }
            if (left != -1) {
                ret += tree.nodes_[left].countContainedIn(tree, q, qb);
            }
            if (right != -1) {
                ret += tree.nodes_[right].countContainedIn(tree, q, qb);
            }
            return ret;
        }

        template <class Q, class QB>
        [[nodiscard]] WeightType sumContainedIn(const ShapeTree& tree, const Q& q,
                                                const QB& qb) const {
            if (boxMisses(tree, q, qb) || !q.intersects(box)) {
                return WeightType{};
            }
            if (q.contains(box)) {
                return weightSum;
            }
            WeightType ret{};
            for (std::size_t i : elementIndices) {
                if (!elementBoxMisses(tree, q, qb, i) && q.contains(tree.elements_[i])) {
                    ret = ret + tree.weight_(tree.elements_[i]);
                }
            }
            if (left != -1) {
                ret = ret + tree.nodes_[left].sumContainedIn(tree, q, qb);
            }
            if (right != -1) {
                ret = ret + tree.nodes_[right].sumContainedIn(tree, q, qb);
            }
            return ret;
        }

        template <class Q, class QB>
        void reportContainedIn(const ShapeTree& tree, const Q& q, const QB& qb,
                               std::vector<ShapeType>& out) const {
            if (boxMisses(tree, q, qb) || !q.intersects(box)) {
                return;
            }
            if (q.contains(box)) {
                collectAll(tree, out);
                return;
            }
            for (std::size_t i : elementIndices) {
                if (!elementBoxMisses(tree, q, qb, i) && q.contains(tree.elements_[i])) {
                    out.push_back(tree.elements_[i]);
                }
            }
            if (left != -1) {
                tree.nodes_[left].reportContainedIn(tree, q, qb, out);
            }
            if (right != -1) {
                tree.nodes_[right].reportContainedIn(tree, q, qb, out);
            }
        }

        template <class Q, class QB, class Fn>
        [[nodiscard]] bool visitContainedIn(const ShapeTree& tree, const Q& q, const QB& qb,
                                            Fn& fn) const {
            if (boxMisses(tree, q, qb) || !q.intersects(box)) {
                return false;
            }
            if (q.contains(box)) {
                return visitAll(tree, fn);
            }
            for (std::size_t i : elementIndices) {
                if (!elementBoxMisses(tree, q, qb, i) && q.contains(tree.elements_[i]) &&
                    detail::invokeVisitor(fn, tree.elements_[i])) {
                    return true;
                }
            }
            if (left != -1 && tree.nodes_[left].visitContainedIn(tree, q, qb, fn)) {
                return true;
            }
            if (right != -1 && tree.nodes_[right].visitContainedIn(tree, q, qb, fn)) {
                return true;
            }
            return false;
        }

        template <class Q, class QB>
        [[nodiscard]] bool anyContainedIn(const ShapeTree& tree, const Q& q,
                                          const QB& qb) const {
            if (boxMisses(tree, q, qb) || !q.intersects(box)) {
                return false;
            }
            if (q.contains(box)) {
                return true;
            }
            for (std::size_t i : elementIndices) {
                if (!elementBoxMisses(tree, q, qb, i) && q.contains(tree.elements_[i])) {
                    return true;
                }
            }
            if (left != -1 && tree.nodes_[left].anyContainedIn(tree, q, qb)) {
                return true;
            }
            if (right != -1 && tree.nodes_[right].anyContainedIn(tree, q, qb)) {
                return true;
            }
            return false;
        }

        // --- membership: a stored element equal to a given shape ---

        // Searches for a stored element equal to `shape`, pruning any subtree
        // whose box does not contain the shape's box `sb` (an equal element has
        // exactly that box, so it cannot lie in such a subtree).
        [[nodiscard]] bool containsShape(const ShapeTree& tree, const ShapeType& shape,
                                         const Rect& sb) const {
            if (!box.contains(sb)) {
                return false;
            }
            for (std::size_t i : elementIndices) {
                if (tree.elements_[i] == shape) {
                    return true;
                }
            }
            if (left != -1 && tree.nodes_[left].containsShape(tree, shape, sb)) {
                return true;
            }
            if (right != -1 && tree.nodes_[right].containsShape(tree, shape, sb)) {
                return true;
            }
            return false;
        }

        // --- nearest neighbor: branch-and-bound on squared distance ---

        // Refines (bestDist, bestIndex) with the closest element in this subtree,
        // under the distance metric selected by `Metric` (squared-L2, L1 or LInf;
        // see detail::SquaredMetric / L1Metric / LInfMetric). The subtree box
        // gives a lower bound on the distance from q to anything it stores, so a
        // subtree whose box is no nearer than the current best is pruned
        // entirely. Children are descended nearest-box-first so the bound
        // tightens before the farther child is considered.
        template <class ResultNumber, class Metric, class Q>
        void nearest(const ShapeTree& tree, const Q& q, ResultNumber& bestDist,
                     std::ptrdiff_t& bestIndex) const {
            if (bestIndex != -1) {
                const ResultNumber lowerBound = Metric::template distance<ResultNumber>(q, box);
                if (!(lowerBound < bestDist)) {
                    return;  // Nothing here can beat the current best.
                }
            }

            for (std::size_t i : elementIndices) {
                const ResultNumber d = Metric::template distance<ResultNumber>(q, tree.elements_[i]);
                if (bestIndex == -1 || d < bestDist) {
                    bestDist = d;
                    bestIndex = static_cast<std::ptrdiff_t>(i);
                    if (d == 0) {
                        return;  // Exact hit: nothing can be nearer than zero.
                    }
                }
            }

            if (left == -1) {
                if (right != -1) {
                    tree.nodes_[right].template nearest<ResultNumber, Metric>(tree, q, bestDist, bestIndex);
                }
                return;
            }
            if (right == -1) {
                tree.nodes_[left].template nearest<ResultNumber, Metric>(tree, q, bestDist, bestIndex);
                return;
            }

            const ResultNumber leftBound = Metric::template distance<ResultNumber>(q, tree.nodes_[left].box);
            const ResultNumber rightBound = Metric::template distance<ResultNumber>(q, tree.nodes_[right].box);
            const std::ptrdiff_t nearChild = leftBound <= rightBound ? left : right;
            const std::ptrdiff_t farChild = leftBound <= rightBound ? right : left;
            tree.nodes_[nearChild].template nearest<ResultNumber, Metric>(tree, q, bestDist, bestIndex);
            tree.nodes_[farChild].template nearest<ResultNumber, Metric>(tree, q, bestDist, bestIndex);
        }

        // Maintains a max-heap containing the k closest elements seen so far.
        // Once the heap is full, its front is the current kth-smallest distance
        // and therefore supplies the pruning bound for whole subtrees.
        template <class ResultNumber, class Metric, class Q>
        void nearest(const ShapeTree& tree, const Q& q, std::size_t k,
                     std::vector<std::pair<ResultNumber, std::size_t>>& best) const {
            const auto nearer = [](const auto& a, const auto& b) {
                return a.first < b.first;
            };

            if (best.size() == k) {
                const ResultNumber lowerBound = Metric::template distance<ResultNumber>(q, box);
                if (!(lowerBound < best.front().first)) {
                    return;  // Nothing here can enter the current top k.
                }
            }

            for (std::size_t i : elementIndices) {
                const ResultNumber d = Metric::template distance<ResultNumber>(q, tree.elements_[i]);
                if (best.size() < k) {
                    best.emplace_back(d, i);
                    std::push_heap(best.begin(), best.end(), nearer);
                } else if (d < best.front().first) {
                    std::pop_heap(best.begin(), best.end(), nearer);
                    best.back() = {d, i};
                    std::push_heap(best.begin(), best.end(), nearer);
                }
            }

            if (left == -1) {
                if (right != -1) {
                    tree.nodes_[right].template nearest<ResultNumber, Metric>(tree, q, k, best);
                }
                return;
            }
            if (right == -1) {
                tree.nodes_[left].template nearest<ResultNumber, Metric>(tree, q, k, best);
                return;
            }

            const ResultNumber leftBound = Metric::template distance<ResultNumber>(q, tree.nodes_[left].box);
            const ResultNumber rightBound = Metric::template distance<ResultNumber>(q, tree.nodes_[right].box);
            const std::ptrdiff_t nearChild = leftBound <= rightBound ? left : right;
            const std::ptrdiff_t farChild = leftBound <= rightBound ? right : left;
            tree.nodes_[nearChild].template nearest<ResultNumber, Metric>(tree, q, k, best);
            tree.nodes_[farChild].template nearest<ResultNumber, Metric>(tree, q, k, best);
        }
    };

    static constexpr std::size_t defaultLeafSize = 6;

    std::vector<ShapeType> elements_;
    std::vector<Node> nodes_;
    // Parallel to elements_ and to nodes_, and empty unless usesFilter.
    std::vector<FilterBox> filterBoxes_;
    std::vector<FilterBox> nodeFilterBoxes_;
    std::ptrdiff_t root_ = -1;
    std::size_t leafSize_ = defaultLeafSize;
    [[no_unique_address]] WeightFn weight_{};

    // Appends a fresh node and returns its index, centralizing the "the index is
    // the current size, then push" pattern shared by build and insert.
    std::ptrdiff_t allocNode() {
        const std::ptrdiff_t id = static_cast<std::ptrdiff_t>(nodes_.size());
        nodes_.push_back(Node{});
        if constexpr (usesFilter) {
            nodeFilterBoxes_.emplace_back();
        }
        return id;
    }

    // Records the filter box of node `id`, after its box was set or changed.
    void refreshNodeFilterBox(std::ptrdiff_t id) {
        if constexpr (usesFilter) {
            nodeFilterBoxes_[static_cast<std::size_t>(id)] = filterBoxOf(nodes_[id].box);
        }
    }

    // The best split found on a single axis.
    struct Split {
        bool found = false;
        int axis = 0;
        NumberType value{};
        std::size_t straddlers = 0;
        std::size_t maxChild = 0;
        std::size_t score = 0;  // maxChild + straddlers; lower is better.
    };

    // One end of one element's bounding box on one axis, carrying the element
    // it came from. The scan below wants these in ascending order and wants
    // them contiguous, which is why the coordinate is copied here rather than
    // reached through the element every time.
    struct EndPoint {
        NumberType value;
        std::size_t index;
    };

    // A subtree's box ends: per axis, every element's lower end and every
    // element's upper end, each list ascending. Splitting a node preserves the
    // relative order of whatever it hands to a child, so a child's lists are
    // its parent's with the elements that went elsewhere dropped -- one linear
    // pass, no comparison. Inheriting them is what keeps the whole build to one
    // sort per axis instead of one per node.
    struct SortedEnds {
        std::vector<EndPoint> lo[2], hi[2];
    };

    // Fills `lo` and `hi` with the elements' box ends on `axis`, ascending.
    void sortEndsOnAxis(const std::vector<std::size_t>& indices, std::size_t axis,
                        std::vector<EndPoint>& lo, std::vector<EndPoint>& hi) const {
        lo.reserve(indices.size());
        hi.reserve(indices.size());
        for (std::size_t i : indices) {
            const auto box = elements_[i].bbox();
            lo.push_back(EndPoint{box.min()[axis], i});
            hi.push_back(EndPoint{box.max()[axis], i});
        }
        const auto byValue = [](const EndPoint& a, const EndPoint& b) { return a.value < b.value; };
        std::sort(lo.begin(), lo.end(), byValue);
        std::sort(hi.begin(), hi.end(), byValue);
    }

    // Finds the split on `axis` minimizing maxChild + straddlers, which balances
    // the children while keeping few elements stuck at the node; ties are broken
    // toward fewer straddlers. The two lists are that axis's box ends, ascending.
    Split bestSplitOnEnds(const std::vector<EndPoint>& los, const std::vector<EndPoint>& his,
                          std::size_t axis) const {
        const std::size_t n = los.size();
        Split best;
        best.axis = static_cast<int>(axis);
        std::size_t loPos = 0;
        std::size_t hiPos = 0;
        while (loPos < n || hiPos < n) {
            // Merge the distinct lo/hi coordinates in order. Advancing both
            // cursors through v gives the two side counts directly, avoiding a
            // third sort and two binary searches per candidate coordinate.
            const NumberType& v =
                hiPos == n || (loPos < n && los[loPos].value < his[hiPos].value)
                    ? los[loPos].value
                    : his[hiPos].value;
            while (loPos < n && !(v < los[loPos].value)) {
                ++loPos;
            }
            while (hiPos < n && !(v < his[hiPos].value)) {
                ++hiPos;
            }

            // left: boxes entirely <= v (hi <= v); right: entirely > v (lo > v).
            const std::size_t leftCount = hiPos;
            const std::size_t rightCount = n - loPos;
            if (leftCount >= n || rightCount >= n || leftCount + rightCount == 0) {
                continue;  // No progress: a child would hold every element.
            }
            const std::size_t straddlers = n - leftCount - rightCount;
            const std::size_t maxChild = std::max(leftCount, rightCount);
            const std::size_t score = maxChild + straddlers;
            if (!best.found || score < best.score ||
                (score == best.score && straddlers < best.straddlers)) {
                best.found = true;
                best.value = v;
                best.straddlers = straddlers;
                best.maxChild = maxChild;
                best.score = score;
            }
        }
        return best;
    }

    // @ref bestSplitOnEnds for a node whose ends were not inherited: the
    // incremental path splits one overflowing leaf, whose elements are few, so
    // it sorts them here rather than carrying lists through every insertion.
    Split bestSplitOnAxis(const std::vector<std::size_t>& indices, std::size_t axis) const {
        // Points have no straddlers: an optimum is attained immediately before
        // or after the coordinate group containing the median. Selecting that
        // group is linear on average and avoids the endpoint sorts needed for
        // shapes with non-degenerate bounding boxes.
        if constexpr (PointConcept<ShapeType>) {
            const std::size_t n = indices.size();
            std::vector<std::size_t> ordered = indices;
            const auto middle = ordered.begin() + static_cast<std::ptrdiff_t>(n / 2);
            std::nth_element(
                ordered.begin(), middle, ordered.end(),
                [&](std::size_t a, std::size_t b) {
                    return elements_[a][axis] < elements_[b][axis];
                });
            const NumberType& median = elements_[*middle][axis];

            std::size_t less = 0;
            std::size_t equal = 0;
            std::size_t predecessor = 0;
            bool hasPredecessor = false;
            for (std::size_t i : indices) {
                const NumberType& coordinate = elements_[i][axis];
                if (coordinate < median) {
                    ++less;
                    if (!hasPredecessor || elements_[predecessor][axis] < coordinate) {
                        predecessor = i;
                        hasPredecessor = true;
                    }
                } else if (!(median < coordinate)) {
                    ++equal;
                }
            }

            Split best;
            best.axis = static_cast<int>(axis);
            const auto consider = [&](const NumberType& value, std::size_t leftCount) {
                const std::size_t rightCount = n - leftCount;
                if (leftCount >= n || rightCount >= n) {
                    return;
                }
                const std::size_t score = std::max(leftCount, rightCount);
                if (!best.found || score < best.score) {
                    best.found = true;
                    best.value = value;
                    best.maxChild = score;
                    best.score = score;
                }
            };
            if (hasPredecessor) {
                consider(elements_[predecessor][axis], less);
            }
            consider(median, less + equal);
            return best;
        }

        std::vector<EndPoint> los, his;
        sortEndsOnAxis(indices, axis, los, his);
        return bestSplitOnEnds(los, his, axis);
    }

    // Chooses the best split over `n` elements, trying the depth-parity axis
    // first so equal-scoring splits alternate direction. `splitOnAxis` answers
    // for one axis; where it reads the coordinates from is the caller's.
    template <class SplitOnAxis>
    static Split chooseSplitOverAxes(std::size_t n, int level, SplitOnAxis&& splitOnAxis) {
        Split best;
        for (int k = 0; k < 2; ++k) {
            const std::size_t axis = static_cast<std::size_t>((level + k) % 2);
            const Split candidate = splitOnAxis(axis);
            if (!candidate.found) {
                continue;
            }
            if (!best.found || candidate.score < best.score ||
                (candidate.score == best.score && candidate.straddlers < best.straddlers)) {
                best = candidate;
            }
            // This is the absolute lower bound: the elements are split evenly
            // and none stays at the node, so the other axis cannot improve it.
            if (candidate.straddlers == 0 && candidate.score == (n + 1) / 2) {
                return candidate;
            }
        }
        return best;
    }

    // Chooses the best split over `indices`, sorting each axis's ends here.
    Split chooseSplit(const std::vector<std::size_t>& indices, int level) const {
        return chooseSplitOverAxes(indices.size(), level, [&](std::size_t axis) {
            return bestSplitOnAxis(indices, axis);
        });
    }

    // Chooses the best split over a node whose ends came down from its parent.
    Split chooseSplit(const SortedEnds& ends, int level) const {
        return chooseSplitOverAxes(ends.lo[0].size(), level, [&](std::size_t axis) {
            return bestSplitOnEnds(ends.lo[axis], ends.hi[axis], axis);
        });
    }

    // Partitions indices by a split: strictly-left (hi <= value), strictly-right
    // (lo > value), and straddlers (kept at the node).
    void partitionBySplit(const std::vector<std::size_t>& indices, const Split& split,
                          std::vector<std::size_t>& leftIndices,
                          std::vector<std::size_t>& rightIndices,
                          std::vector<std::size_t>& straddlers) const {
        const auto a = static_cast<std::size_t>(split.axis);
        for (std::size_t i : indices) {
            const NumberType lo = elements_[i].bbox().min()[a];
            const NumberType hi = elements_[i].bbox().max()[a];
            if (hi <= split.value) {
                leftIndices.push_back(i);
            } else if (lo > split.value) {
                rightIndices.push_back(i);
            } else {
                straddlers.push_back(i);
            }
        }
    }

    // Appends the node covering `indices`, with its bounding box, its element
    // count and its weight sum, and returns its index. Whether it keeps them as
    // a leaf or splits them further is the caller's to decide.
    std::ptrdiff_t makeNode(const std::vector<std::size_t>& indices) {
        Rect box = Rect(elements_[indices[0]].bbox());
        WeightType weightSum = weight_(elements_[indices[0]]);
        // The subtree's filter box is unioned from the elements' rather than
        // converted from `box` once it is known: a union of outward boxes is
        // outward too, and it rides the loop already running instead of paying
        // eight directed conversions out of the exact coordinate type.
        FilterBox filter{};
        if constexpr (usesFilter) {
            filter = filterBoxes_[indices[0]];
        }
        for (std::size_t k = 1; k < indices.size(); ++k) {
            box.insert(elements_[indices[k]].bbox());
            weightSum = weightSum + weight_(elements_[indices[k]]);
            if constexpr (usesFilter) {
                filter.insert(filterBoxes_[indices[k]]);
            }
        }

        // Reserve this node's slot now; recursion may reallocate nodes_, so the
        // node is always addressed by index, never by a dangling reference.
        const std::ptrdiff_t id = allocNode();
        nodes_[id].box = box;
        if constexpr (usesFilter) {
            nodeFilterBoxes_[static_cast<std::size_t>(id)] = filter;
        }
        nodes_[id].count = indices.size();
        nodes_[id].weightSum = weightSum;
        return id;
    }

    // Builds a subtree from the given element indices and returns its node index.
    // `level` is the depth, used only to break ties between equally good axes so
    // the split direction alternates (e.g. for points, where both axes always
    // score the same).
    std::ptrdiff_t build(const std::vector<std::size_t>& indices, int level) {
        const std::ptrdiff_t id = makeNode(indices);

        if (indices.size() <= leafSize_) {
            nodes_[id].elementIndices = indices;
            return id;
        }

        const Split best = chooseSplit(indices, level);
        if (!best.found) {
            // No axis can separate the elements (e.g. many identical boxes):
            // keep them all here as a leaf.
            nodes_[id].elementIndices = indices;
            return id;
        }

        std::vector<std::size_t> leftIndices, rightIndices, straddlers;
        partitionBySplit(indices, best, leftIndices, rightIndices, straddlers);

        const std::ptrdiff_t leftChild = leftIndices.empty() ? -1 : build(leftIndices, level + 1);
        const std::ptrdiff_t rightChild = rightIndices.empty() ? -1 : build(rightIndices, level + 1);
        nodes_[id].left = leftChild;
        nodes_[id].right = rightChild;
        nodes_[id].elementIndices = std::move(straddlers);
        return id;
    }

    // @ref build for a node whose box ends came down from its parent already in
    // order. It picks the same split the sorting path would and so produces the
    // same tree; what it saves is the sort, which the other path pays at every
    // node and on both axes. `side` is scratch indexed by element, sized once
    // by the caller and reused down the whole recursion.
    std::ptrdiff_t buildFromEnds(SortedEnds& ends, int level, std::vector<std::uint8_t>& side) {
        // The elements this subtree holds, in the order the first axis's lower
        // ends put them. A node's own list is read in whatever order it is
        // stored, and nothing depends on which order that is: it decides only
        // which of several equally good answers a query gives back -- which
        // stored shape is returned when two are the same distance away.
        std::vector<std::size_t> indices;
        indices.reserve(ends.lo[0].size());
        for (const EndPoint& end : ends.lo[0]) {
            indices.push_back(end.index);
        }

        const std::ptrdiff_t id = makeNode(indices);

        if (indices.size() <= leafSize_) {
            nodes_[id].elementIndices = std::move(indices);
            return id;
        }

        const Split best = chooseSplit(ends, level);
        if (!best.found) {
            nodes_[id].elementIndices = std::move(indices);
            return id;
        }

        // Tag each element with the side it goes to, then deal every end list
        // out by the tags. A filtered subsequence of an ordered list is ordered,
        // so the children get their lists sorted without a comparison.
        static constexpr std::uint8_t toLeft = 0, toRight = 1, stays = 2;
        const auto a = static_cast<std::size_t>(best.axis);
        std::vector<std::size_t> straddlers;
        std::size_t leftCount = 0, rightCount = 0;
        for (std::size_t i : indices) {
            const auto box = elements_[i].bbox();
            const std::uint8_t which = box.max()[a] <= best.value  ? toLeft
                                       : box.min()[a] > best.value ? toRight
                                                                   : stays;
            side[i] = which;
            if (which == toLeft) {
                ++leftCount;
            } else if (which == toRight) {
                ++rightCount;
            } else {
                straddlers.push_back(i);
            }
        }

        SortedEnds left, right;
        const auto deal = [&](std::vector<EndPoint>& source, std::vector<EndPoint>& toTheLeft,
                              std::vector<EndPoint>& toTheRight) {
            toTheLeft.reserve(leftCount);
            toTheRight.reserve(rightCount);
            for (const EndPoint& end : source) {
                if (side[end.index] == toLeft) {
                    toTheLeft.push_back(end);
                } else if (side[end.index] == toRight) {
                    toTheRight.push_back(end);
                }
            }
            // The parent's copy is dead the moment its children have theirs;
            // releasing it here is what keeps the live lists linear in total
            // rather than linear per level of the recursion.
            source.clear();
            source.shrink_to_fit();
        };
        for (int axis = 0; axis < 2; ++axis) {
            deal(ends.lo[axis], left.lo[axis], right.lo[axis]);
            deal(ends.hi[axis], left.hi[axis], right.hi[axis]);
        }

        const std::ptrdiff_t leftChild =
            leftCount == 0 ? -1 : buildFromEnds(left, level + 1, side);
        const std::ptrdiff_t rightChild =
            rightCount == 0 ? -1 : buildFromEnds(right, level + 1, side);
        nodes_[id].left = leftChild;
        nodes_[id].right = rightChild;
        nodes_[id].elementIndices = std::move(straddlers);
        return id;
    }

    // Splits an overflowing leaf in place with the best split. Its box, count
    // and weight sum are unchanged since the same elements stay in the subtree.
    void splitNode(std::ptrdiff_t id, int level) {
        std::vector<std::size_t> indices = std::move(nodes_[id].elementIndices);
        nodes_[id].elementIndices.clear();

        const Split best = chooseSplit(indices, level);
        if (!best.found) {
            // Cannot separate (e.g. identical boxes): stays an oversized leaf.
            nodes_[id].elementIndices = std::move(indices);
            return;
        }

        std::vector<std::size_t> leftIndices, rightIndices, straddlers;
        partitionBySplit(indices, best, leftIndices, rightIndices, straddlers);

        const std::ptrdiff_t leftChild = leftIndices.empty() ? -1 : build(leftIndices, level + 1);
        const std::ptrdiff_t rightChild = rightIndices.empty() ? -1 : build(rightIndices, level + 1);
        nodes_[id].left = leftChild;
        nodes_[id].right = rightChild;
        nodes_[id].elementIndices = std::move(straddlers);
    }

    // Area increase of `box` when grown to also include `other`.
    static auto enlargement(const Rect& box, const Rect& other) {
        Rect grown = box;
        grown.insert(other);
        return grown.area() - box.area();
    }

    // Routes element i (bounding box eb) down from node id, maintaining every
    // visited node's box, count and weight sum, and keeping sibling boxes
    // disjoint.
    void insertInto(std::ptrdiff_t id, std::size_t i, const Rect& eb, int level) {
        nodes_[id].box.insert(eb);
        refreshNodeFilterBox(id);
        nodes_[id].count += 1;
        nodes_[id].weightSum = nodes_[id].weightSum + weight_(elements_[i]);

        if (nodes_[id].left == -1 && nodes_[id].right == -1) {
            nodes_[id].elementIndices.push_back(i);
            if (nodes_[id].elementIndices.size() > leafSize_) {
                splitNode(id, level);
            }
            return;
        }

        const std::ptrdiff_t L = nodes_[id].left;
        const std::ptrdiff_t R = nodes_[id].right;

        // A child may take the element only if its grown box stays disjoint from
        // its sibling's box.
        bool leftOk = false;
        bool rightOk = false;
        if (L != -1) {
            Rect grown = nodes_[L].box;
            grown.insert(eb);
            leftOk = (R == -1) || !grown.intersects(nodes_[R].box);
        }
        if (R != -1) {
            Rect grown = nodes_[R].box;
            grown.insert(eb);
            rightOk = (L == -1) || !grown.intersects(nodes_[L].box);
        }

        std::ptrdiff_t target = -1;
        if (leftOk && rightOk) {
            // Both keep disjointness: descend into the one enlarged least.
            target = enlargement(nodes_[L].box, eb) <= enlargement(nodes_[R].box, eb) ? L : R;
        } else if (leftOk) {
            target = L;
        } else if (rightOk) {
            target = R;
        }

        if (target == -1) {
            // Neither child can stay disjoint: keep the element at this node.
            nodes_[id].elementIndices.push_back(i);
            return;
        }
        insertInto(target, i, eb, level + 1);
    }

    // True when the weight function actually produces weights (i.e. the user
    // supplied one), so weight bookkeeping can be skipped entirely otherwise.
    static constexpr bool hasWeight = !std::is_same_v<WeightType, detail::EmptyWeight>;

    // A node holds nothing once it has neither elements nor children; such a
    // node is detached from its parent by erase.
    static bool nodeIsEmpty(const Node& node) {
        return node.elementIndices.empty() && node.left == -1 && node.right == -1;
    }

    // Recomputes node id's box as the union of its children boxes and its own
    // elements' boxes, and returns whether the box actually changed. The node
    // must be non-empty (so the union is well defined).
    bool recomputeBox(std::ptrdiff_t id) {
        Node& node = nodes_[id];
        Rect newBox{};
        bool init = false;
        for (std::size_t i : node.elementIndices) {
            const Rect eb = Rect(elements_[i].bbox());
            if (!init) {
                newBox = eb;
                init = true;
            } else {
                newBox.insert(eb);
            }
        }
        if (node.left != -1) {
            if (!init) {
                newBox = nodes_[node.left].box;
                init = true;
            } else {
                newBox.insert(nodes_[node.left].box);
            }
        }
        if (node.right != -1) {
            if (!init) {
                newBox = nodes_[node.right].box;
                init = true;
            } else {
                newBox.insert(nodes_[node.right].box);
            }
        }
        const bool changed = !(newBox == node.box);
        node.box = newBox;
        refreshNodeFilterBox(id);
        return changed;
    }

    // Removes one stored element equal to `shape` from the subtree rooted at
    // `id`. On success the element is removed from its owning node, every node on
    // the path has its count decremented and its weight sum reduced by the
    // removed weight, boxes are recomputed from the removal point upward and stop
    // propagating once a box is unchanged, and any node left empty is detached
    // from its parent. `removedIdx`/`removedWeight` receive the removed element's
    // index and weight; `boxChanged` reports whether this node's box changed, so
    // the parent only recomputes when it has to.
    bool eraseFrom(std::ptrdiff_t id, const ShapeType& shape, const Rect& sb,
                   std::size_t& removedIdx, WeightType& removedWeight, bool& boxChanged,
                   std::vector<std::ptrdiff_t>& dead) {
        boxChanged = false;
        if (!nodes_[id].box.contains(sb)) {
            return false;  // An equal element has box sb, so it cannot be here.
        }

        bool removed = false;
        bool needBoxRecompute = false;

        // Look among this node's own elements first.
        auto& elems = nodes_[id].elementIndices;
        for (std::size_t k = 0; k < elems.size(); ++k) {
            if (elements_[elems[k]] == shape) {
                removedIdx = elems[k];
                if constexpr (hasWeight) {
                    removedWeight = weight_(elements_[removedIdx]);
                }
                elems.erase(elems.begin() + static_cast<std::ptrdiff_t>(k));
                removed = true;
                needBoxRecompute = true;
                break;
            }
        }

        // Otherwise descend into the children, detaching one that empties out.
        for (std::ptrdiff_t side = 0; !removed && side < 2; ++side) {
            std::ptrdiff_t& child = side == 0 ? nodes_[id].left : nodes_[id].right;
            if (child == -1) {
                continue;
            }
            bool childBoxChanged = false;
            if (eraseFrom(child, shape, sb, removedIdx, removedWeight, childBoxChanged, dead)) {
                removed = true;
                if (nodeIsEmpty(nodes_[child])) {
                    dead.push_back(child);  // Reclaimed after the recursion unwinds.
                    child = -1;
                    needBoxRecompute = true;
                } else if (childBoxChanged) {
                    needBoxRecompute = true;
                }
            }
        }

        if (!removed) {
            return false;
        }

        nodes_[id].count -= 1;
        if constexpr (hasWeight) {
            nodes_[id].weightSum = nodes_[id].weightSum - removedWeight;
        }
        if (nodeIsEmpty(nodes_[id])) {
            boxChanged = true;  // The whole subtree is gone; the parent detaches it.
            return true;
        }
        if (needBoxRecompute) {
            boxChanged = recomputeBox(id);
        }
        return true;
    }

    // After an element is swap-removed in elements_, the element formerly at
    // `oldIdx` now lives at `newIdx`; this fixes the single node reference to it.
    // The element is stored in exactly one node, whose box (and every ancestor's)
    // contains the element box `eb`; since sibling boxes are disjoint, at most one
    // child can contain `eb`, so the owning node is reached on a single path down.
    void remapElementIndex(std::size_t oldIdx, std::size_t newIdx, const Rect& eb) {
        for (std::ptrdiff_t id = root_; id != -1;) {
            for (std::size_t& ref : nodes_[id].elementIndices) {
                if (ref == oldIdx) {
                    ref = newIdx;
                    return;
                }
            }
            const std::ptrdiff_t left = nodes_[id].left;
            // Descend into the unique child whose box contains eb (the right one
            // when the left does not), since the element lies in one subtree.
            id = (left != -1 && nodes_[left].box.contains(eb)) ? left : nodes_[id].right;
        }
    }

    // Repoints the single reference to node `oldId` (a parent's child link, or
    // the root) to `newId`, used when that node is relocated within nodes_. `b`
    // is the relocated node's box; its parent is reached on a single path down,
    // descending into the unique child whose box contains `b` (sibling boxes are
    // disjoint, so only one can).
    void repointNodeRef(std::ptrdiff_t oldId, std::ptrdiff_t newId, const Rect& b) {
        if (root_ == oldId) {
            root_ = newId;
            return;
        }
        for (std::ptrdiff_t id = root_; id != -1;) {
            if (nodes_[id].left == oldId) {
                nodes_[id].left = newId;
                return;
            }
            if (nodes_[id].right == oldId) {
                nodes_[id].right = newId;
                return;
            }
            const std::ptrdiff_t left = nodes_[id].left;
            id = (left != -1 && nodes_[left].box.contains(b)) ? left : nodes_[id].right;
        }
    }

    // Removes the detached node slots in `dead` from nodes_ by swap-removing each
    // with the last node, keeping the array compact so interleaved insert/erase
    // does not grow it without bound. Processing the dead indices in descending
    // order guarantees the last slot is always a live node (every index past the
    // largest remaining dead index is live), so the node moved into the hole is
    // real and its one inbound reference can be repointed.
    void compactNodes(std::vector<std::ptrdiff_t>& dead) {
        std::sort(dead.begin(), dead.end());
        for (auto it = dead.rbegin(); it != dead.rend(); ++it) {
            const std::ptrdiff_t hole = *it;
            const std::ptrdiff_t last = static_cast<std::ptrdiff_t>(nodes_.size()) - 1;
            if (hole != last) {
                nodes_[hole] = std::move(nodes_[last]);
                repointNodeRef(last, hole, nodes_[hole].box);
                if constexpr (usesFilter) {
                    nodeFilterBoxes_[static_cast<std::size_t>(hole)] =
                        nodeFilterBoxes_[static_cast<std::size_t>(last)];
                }
            }
            nodes_.pop_back();
            if constexpr (usesFilter) {
                nodeFilterBoxes_.pop_back();
            }
        }
    }

    // Shared implementation behind nearestNeighbor/nearestNeighborL1/
    // nearestNeighborLInf: runs the branch-and-bound traversal under `Metric`
    // and returns the winning element (or a default-constructed one when empty).
    template <class Metric, class ResultNumber, class Q>
    [[nodiscard]] const ShapeType& nearestNeighborByMetric(const Q& q) const {
        if (root_ == -1) {
            static const ShapeType empty{};
            return empty;
        }
        ResultNumber bestDist{};
        std::ptrdiff_t bestIndex = -1;
        nodes_[root_].template nearest<ResultNumber, Metric>(*this, q, bestDist, bestIndex);
        return elements_[static_cast<std::size_t>(bestIndex)];
    }

    // Shared implementation for the k-nearest-neighbor overloads. The heap is
    // sorted before its indices are mapped back to copies of the stored shapes.
    template <class Metric, class ResultNumber, class Q>
    [[nodiscard]] std::vector<ShapeType> nearestNeighborsByMetric(const Q& q, int k) const {
        if (root_ == -1 || k <= 0) {
            return {};
        }
        const std::size_t count = std::min(static_cast<std::size_t>(k), elements_.size());
        std::vector<std::pair<ResultNumber, std::size_t>> best;
        best.reserve(count);
        nodes_[root_].template nearest<ResultNumber, Metric>(*this, q, count, best);
        std::sort(best.begin(), best.end(), [](const auto& a, const auto& b) {
            return a.first < b.first;
        });

        std::vector<ShapeType> result;
        result.reserve(best.size());
        for (const auto& [distance, index] : best) {
            (void)distance;
            result.push_back(elements_[index]);
        }
        return result;
    }

    // Discards the current node structure and rebuilds it from elements_.
    void buildFromElements() {
        nodes_.clear();
        nodeFilterBoxes_.clear();
        if constexpr (usesFilter) {
            filterBoxes_.clear();
            filterBoxes_.reserve(elements_.size());
            for (const ShapeType& e : elements_) {
                filterBoxes_.push_back(filterBoxOf(e.bbox()));
            }
        }
        root_ = -1;
        if (elements_.empty()) {
            return;
        }
        std::vector<std::size_t> indices(elements_.size());
        for (std::size_t i = 0; i < indices.size(); ++i) {
            indices[i] = i;
        }
        nodes_.reserve(2 * elements_.size() / leafSize_ + 1);
        if constexpr (PointConcept<ShapeType>) {
            // A point's split needs no ordered ends: the median it splits at is
            // selected in linear time, so there is nothing to inherit.
            root_ = build(indices, 0);
        } else {
            SortedEnds ends;
            for (int axis = 0; axis < 2; ++axis) {
                sortEndsOnAxis(indices, static_cast<std::size_t>(axis), ends.lo[axis],
                               ends.hi[axis]);
            }
            std::vector<std::uint8_t> side(elements_.size());
            root_ = buildFromEnds(ends, 0, side);
        }
    }

    // Appends the subtree bounding boxes to `out` in pre-order.
    void collectBoundingBoxes(std::ptrdiff_t id, std::vector<Rect>& out) const {
        if (id == -1) {
            return;
        }
        out.push_back(nodes_[id].box);
        collectBoundingBoxes(nodes_[id].left, out);
        collectBoundingBoxes(nodes_[id].right, out);
    }

  public:
    ShapeTree() = default;

    /**
     * @brief Builds the tree from a container of shapes.
     *
     * @tparam Container Range whose value type is convertible to @ref ShapeType.
     * @param shapes Shapes to store.
     * @param leafSize Maximum elements kept at a leaf before it is split.
     * @param weight Weight function applied to each shape (defaults to a no-op).
     */
    template <class Container>
    explicit ShapeTree(const Container& shapes, std::size_t leafSize = defaultLeafSize,
                       WeightFn weight = WeightFn{})
        : leafSize_(leafSize > 0 ? leafSize : 1), weight_(std::move(weight)) {
        for (const auto& s : shapes) {
            elements_.push_back(s);
        }
        buildFromElements();
    }

    /**
     * @brief Builds the tree from a container of shapes with a weight function.
     *
     * Uses the default leaf size.
     *
     * @tparam Container Range whose value type is convertible to @ref ShapeType.
     * @param shapes Shapes to store.
     * @param weight Weight function applied to each shape.
     */
    template <class Container>
    explicit ShapeTree(const Container& shapes, WeightFn weight)
        : ShapeTree(shapes, defaultLeafSize, std::move(weight)) {}

    /** @brief Returns the number of stored shapes. */
    [[nodiscard]] std::size_t size() const {
        return elements_.size();
    }

    /** @brief Returns whether the tree is empty. */
    [[nodiscard]] bool empty() const {
        return elements_.empty();
    }

    /** @brief Returns the stored shapes in their internal order. */
    [[nodiscard]] const std::vector<ShapeType>& shapes() const {
        return elements_;
    }

    /** @brief Returns an iterator to the first stored shape. */
    [[nodiscard]] const_iterator begin() const {
        return elements_.begin();
    }

    /** @brief Returns an iterator past the last stored shape. */
    [[nodiscard]] const_iterator end() const {
        return elements_.end();
    }

    /** @brief Returns an iterator to the first stored shape. */
    [[nodiscard]] const_iterator cbegin() const {
        return elements_.cbegin();
    }

    /** @brief Returns an iterator past the last stored shape. */
    [[nodiscard]] const_iterator cend() const {
        return elements_.cend();
    }

    /**
     * @brief Inserts a shape without rebalancing the existing tree.
     *
     * The element is routed down the tree, keeping every visited node's box,
     * count and weight sum up to date and preserving the invariant that the two
     * child boxes of a node stay disjoint: a child takes the element only if its
     * grown box remains disjoint from its sibling's; when both qualify the one
     * enlarged least is chosen; when neither does the element is kept at the
     * node. A leaf that overflows the leaf size is split with the best split.
     *
     * Each insertion is `O(height)` and never reshapes the existing nodes, so
     * the tree quality degrades over many insertions; rebuild from @ref shapes()
     * to restore it.
     *
     * @param shape Shape to insert.
     */
    void insert(const ShapeType& shape) {
        const Rect eb = Rect(shape.bbox());
        const std::size_t i = elements_.size();
        elements_.push_back(shape);
        if constexpr (usesFilter) {
            filterBoxes_.push_back(filterBoxOf(eb));
        }

        if (root_ == -1) {
            root_ = allocNode();
            nodes_[root_].box = eb;
            refreshNodeFilterBox(root_);
            nodes_[root_].count = 1;
            nodes_[root_].weightSum = weight_(elements_[i]);
            nodes_[root_].elementIndices.push_back(i);
            return;
        }
        insertInto(root_, i, eb, 0);
    }

    /**
     * @brief Rebuilds the tree from the stored shapes, restoring its quality.
     *
     * @ref insert never reshapes existing nodes, so the structure degrades over
     * many insertions; this discards the node structure and rebuilds it from
     * scratch (the same way the constructor does) over the current shapes,
     * leaving the stored shapes and their order unchanged.
     *
     * @param leafSize Maximum elements kept at a leaf before it is split. Pass 0
     *        to keep the current leaf size.
     */
    void rebuild(std::size_t leafSize = 0) {
        if (leafSize > 0) {
            leafSize_ = leafSize;
        }
        buildFromElements();
    }

    /**
     * @brief Removes one stored shape equal to `shape`.
     *
     * Descends to the node owning an element equal to `shape` (pruning by the
     * cached boxes), removes it, and on the way back up decrements each node's
     * count, subtracts the removed weight, and recomputes bounding boxes from the
     * removal point upward until a box is unchanged. A node left empty is
     * detached from its parent and reclaimed. The element is then swap-removed
     * from storage and the moved element's reference is updated, so @ref shapes()
     * stays compact; detached node slots are likewise swap-removed from the node
     * array, so it does not grow under interleaved insert/erase.
     *
     * Like @ref insert, this does not rebalance, so the structure degrades over
     * many removals; @ref rebuild restores it. Only the element order in
     * @ref shapes() may change.
     *
     * @param shape Shape to remove.
     * @return `true` if a matching shape was found and removed, `false` otherwise.
     */
    bool erase(const ShapeType& shape) {
        if (root_ == -1) {
            return false;
        }
        const Rect sb = Rect(shape.bbox());
        std::size_t removedIdx = 0;
        WeightType removedWeight{};
        bool boxChanged = false;
        std::vector<std::ptrdiff_t> dead;
        if (!eraseFrom(root_, shape, sb, removedIdx, removedWeight, boxChanged, dead)) {
            return false;
        }
        if (nodeIsEmpty(nodes_[root_])) {
            dead.push_back(root_);
            root_ = -1;
        }

        // Swap-remove the element from storage, repointing the moved element.
        const std::size_t last = elements_.size() - 1;
        if (removedIdx != last) {
            elements_[removedIdx] = std::move(elements_[last]);
            remapElementIndex(last, removedIdx, Rect(elements_[removedIdx].bbox()));
            if constexpr (usesFilter) {
                filterBoxes_[removedIdx] = filterBoxes_[last];
            }
        }
        elements_.pop_back();
        if constexpr (usesFilter) {
            filterBoxes_.pop_back();
        }

        // Reclaim the detached node slots, keeping the node array compact.
        compactNodes(dead);
        return true;
    }

    /**
     * @brief Counts the stored shapes intersecting a query shape.
     *
     * Subtrees whose bounding box does not meet the query are pruned; surviving
     * candidates are tested with the exact `intersects` predicate.
     *
     * @tparam Q Query shape type.
     * @param q Query shape.
     * @return Number of stored shapes intersecting `q`.
     */
    template <class Q>
    [[nodiscard]] std::size_t countIntersecting(const Q& q) const {
        if (root_ == -1) {
            return 0;
        }
        return nodes_[root_].countIntersecting(*this, q, queryBoxesOf(q));
    }

    /**
     * @brief Sums the weights of the stored shapes intersecting a query shape.
     *
     * Like @ref countIntersecting, but accumulates the weight function instead of
     * counting. Subtrees fully inside the query contribute their cached weight
     * sum without descending.
     *
     * @tparam Q Query shape type.
     * @param q Query shape.
     * @return Sum of weights over the stored shapes intersecting `q`.
     */
    template <class Q>
    [[nodiscard]] WeightType sumIntersecting(const Q& q) const {
        if (root_ == -1) {
            return WeightType{};
        }
        return nodes_[root_].sumIntersecting(*this, q, queryBoxesOf(q));
    }

    /**
     * @brief Returns copies of the stored shapes intersecting a query shape.
     *
     * Subtrees whose bounding box does not meet the query are pruned; subtrees
     * fully inside the query are collected without per-element tests.
     *
     * @tparam Q Query shape type.
     * @param q Query shape.
     * @return Vector of the stored shapes intersecting `q`.
     */
    template <class Q>
    [[nodiscard]] std::vector<ShapeType> reportIntersecting(const Q& q) const {
        std::vector<ShapeType> out;
        if (root_ != -1) {
            nodes_[root_].reportIntersecting(*this, q, queryBoxesOf(q), out);
        }
        return out;
    }

    /**
     * @brief Calls `fn` on each stored shape intersecting a query shape.
     *
     * Shapes are visited as they are found during traversal. Subtrees disjoint
     * from the query are pruned; subtrees fully inside it are visited without
     * per-element tests.
     *
     * If `fn` returns `bool`, returning `true` stops the traversal immediately;
     * a `void`-returning `fn` always visits every match.
     *
     * @tparam Q Query shape type.
     * @tparam Fn Callable invocable with `const ShapeType&`, returning `void`
     *         or `bool` (return `true` to stop).
     * @param q Query shape.
     * @param fn Function to call on each intersecting shape.
     * @return `true` if `fn` requested an early stop, `false` otherwise.
     */
    template <class Q, class Fn>
    bool visitIntersecting(const Q& q, Fn fn) const {
        return root_ == -1 ? false : nodes_[root_].visitIntersecting(*this, q, queryBoxesOf(q), fn);
    }

    /**
     * @brief Returns whether no stored shape intersects a query shape.
     *
     * Stops as soon as an intersecting shape is found.
     *
     * @tparam Q Query shape type.
     * @param q Query shape.
     * @return `true` if no stored shape intersects `q`, `false` otherwise.
     */
    template <class Q>
    [[nodiscard]] bool emptyIntersecting(const Q& q) const {
        return root_ == -1 ? true : !nodes_[root_].anyIntersecting(*this, q, queryBoxesOf(q));
    }

    /**
     * @brief Counts the stored shapes contained in a query shape.
     *
     * A stored shape matches when it lies inside `q` (`q.contains(element)`).
     * Note this is directional: `q.contains(element)` is not the same as
     * `element.contains(q)`.
     *
     * @tparam Q Query shape type.
     * @param q Query shape.
     * @return Number of stored shapes contained in `q`.
     */
    template <class Q>
    [[nodiscard]] std::size_t countContainedIn(const Q& q) const {
        if (root_ == -1) {
            return 0;
        }
        return nodes_[root_].countContainedIn(*this, q, queryBoxesOf(q));
    }

    /**
     * @brief Sums the weights of the stored shapes contained in a query shape.
     *
     * @tparam Q Query shape type.
     * @param q Query shape.
     * @return Sum of weights over the stored shapes contained in `q`.
     */
    template <class Q>
    [[nodiscard]] WeightType sumContainedIn(const Q& q) const {
        if (root_ == -1) {
            return WeightType{};
        }
        return nodes_[root_].sumContainedIn(*this, q, queryBoxesOf(q));
    }

    /**
     * @brief Returns copies of the stored shapes contained in a query shape.
     *
     * @tparam Q Query shape type.
     * @param q Query shape.
     * @return Vector of the stored shapes contained in `q`.
     */
    template <class Q>
    [[nodiscard]] std::vector<ShapeType> reportContainedIn(const Q& q) const {
        std::vector<ShapeType> out;
        if (root_ != -1) {
            nodes_[root_].reportContainedIn(*this, q, queryBoxesOf(q), out);
        }
        return out;
    }

    /**
     * @brief Calls `fn` on each stored shape contained in a query shape.
     *
     * If `fn` returns `bool`, returning `true` stops the traversal immediately;
     * a `void`-returning `fn` always visits every match.
     *
     * @tparam Q Query shape type.
     * @tparam Fn Callable invocable with `const ShapeType&`, returning `void`
     *         or `bool` (return `true` to stop).
     * @param q Query shape.
     * @param fn Function to call on each contained shape.
     * @return `true` if `fn` requested an early stop, `false` otherwise.
     */
    template <class Q, class Fn>
    bool visitContainedIn(const Q& q, Fn fn) const {
        return root_ == -1 ? false : nodes_[root_].visitContainedIn(*this, q, queryBoxesOf(q), fn);
    }

    /**
     * @brief Returns whether no stored shape is contained in a query shape.
     *
     * Stops as soon as a contained shape is found.
     *
     * @tparam Q Query shape type.
     * @param q Query shape.
     * @return `true` if no stored shape lies inside `q`, `false` otherwise.
     */
    template <class Q>
    [[nodiscard]] bool emptyContainedIn(const Q& q) const {
        return root_ == -1 ? true : !nodes_[root_].anyContainedIn(*this, q, queryBoxesOf(q));
    }

    /**
     * @brief Returns whether a shape equal to `shape` is stored in the tree.
     *
     * Tests exact membership with `operator==`, pruning any subtree whose cached
     * box does not contain the shape's bounding box. This is distinct from the
     * geometric `*ContainedIn` queries: it matches a stored shape identical to
     * `shape`, not one geometrically inside a query region.
     *
     * @param shape Shape to look for.
     * @return `true` if an equal shape is stored, `false` otherwise.
     */
    [[nodiscard]] bool has(const ShapeType& shape) const {
        return root_ != -1 && nodes_[root_].containsShape(*this, shape, Rect(shape.bbox()));
    }

    /**
     * @brief Returns the stored shape nearest to a query shape.
     *
     * Finds the stored shape minimizing `squaredDistance` to `q`, using a
     * branch-and-bound traversal: each node's cached bounding box gives a lower
     * bound on the distance from `q` to anything in that subtree (via
     * `q.squaredDistance(Rectangle)`), so subtrees that cannot hold a closer
     * shape than the best found are pruned, and the nearer child box is
     * descended first to tighten the bound early.
     *
     * With no explicit result type, the concrete shape pair chooses its natural
     * type: native arithmetic when the metric uses no division, @ref
     * division_result_t when it may produce a fraction, and `double` when a
     * @ref Disk makes an irrational result possible. An explicitly requested
     * integral `ResultNumber` may truncate fractional distances; the box lower
     * bound remains conservative in that case.
     *
     * If a @ref Disk is involved (as `ShapeType` or as `Q`), that leg may be
     * irrational and is computed in `double`, then converted to the common
     * comparison type. Other legs stay exact.
     *
     * @pre The tree is non-empty. A reference to a default-constructed
     *      @ref ShapeType is returned otherwise.
     *
     * The reference points into the tree's own storage and stays valid until the
     * tree is destroyed or modified (e.g. by @ref insert).
     *
     * @tparam ResultNumber Explicit coordinate type of the squared distance.
     * @tparam Q Query shape type.
     * @param q Query shape.
     * @return The nearest stored shape.
     */
    template <class Q>
    [[nodiscard]] const ShapeType& nearestNeighbor(const Q& q) const {
        using ResultNumber = std::remove_cvref_t<decltype(
            q.squaredDistance(std::declval<const ShapeType&>()))>;
        return nearestNeighborByMetric<detail::SquaredMetric, ResultNumber>(q);
    }

    template <class ResultNumber, class Q>
    [[nodiscard]] const ShapeType& nearestNeighbor(const Q& q) const {
        return nearestNeighborByMetric<detail::SquaredMetric, ResultNumber>(q);
    }

    /**
     * @brief Returns up to `k` stored shapes nearest to a query shape.
     *
     * The result contains copies of the stored shapes in nondecreasing squared
     * distance from `q`. If `k` exceeds the tree size, every stored shape is
     * returned. A non-positive `k` or an empty tree produces an empty vector.
     *
     * @tparam ResultNumber Explicit coordinate type of the squared distance.
     * @tparam Q Query shape type.
     * @param q Query shape.
     * @param k Maximum number of neighbors to return.
     * @return Up to `k` nearest stored shapes, nearest first.
     */
    template <class Q>
    [[nodiscard]] std::vector<ShapeType> kNearestNeighbors(const Q& q, int k) const {
        using ResultNumber = std::remove_cvref_t<decltype(
            q.squaredDistance(std::declval<const ShapeType&>()))>;
        return nearestNeighborsByMetric<detail::SquaredMetric, ResultNumber>(q, k);
    }

    template <class ResultNumber, class Q>
    [[nodiscard]] std::vector<ShapeType> kNearestNeighbors(const Q& q, int k) const {
        return nearestNeighborsByMetric<detail::SquaredMetric, ResultNumber>(q, k);
    }

    /**
     * @brief Returns the stored shape nearest to a query shape under the L1
     *        (Manhattan) metric.
     *
     * Same branch-and-bound traversal as @ref nearestNeighbor, but minimizes
     * `distanceL1` instead of `squaredDistance`.
     *
     * @pre The tree is non-empty. A reference to a default-constructed
     *      @ref ShapeType is returned otherwise.
     *
     * @tparam ResultNumber Explicit coordinate type of the distance.
     * @tparam Q Query shape type.
     * @param q Query shape.
     * @return The stored shape nearest to `q` under the L1 metric.
     */
    template <class Q>
    [[nodiscard]] const ShapeType& nearestNeighborL1(const Q& q) const {
        using ResultNumber = std::remove_cvref_t<decltype(
            q.distanceL1(std::declval<const ShapeType&>()))>;
        return nearestNeighborByMetric<detail::L1Metric, ResultNumber>(q);
    }

    template <class ResultNumber, class Q>
    [[nodiscard]] const ShapeType& nearestNeighborL1(const Q& q) const {
        return nearestNeighborByMetric<detail::L1Metric, ResultNumber>(q);
    }

    /**
     * @brief Returns the stored shape nearest to a query shape under the LInf
     *        (Chebyshev) metric.
     *
     * Same branch-and-bound traversal as @ref nearestNeighbor, but minimizes
     * `distanceLInf` instead of `squaredDistance`.
     *
     * @pre The tree is non-empty. A reference to a default-constructed
     *      @ref ShapeType is returned otherwise.
     *
     * @tparam ResultNumber Explicit coordinate type of the distance.
     * @tparam Q Query shape type.
     * @param q Query shape.
     * @return The stored shape nearest to `q` under the LInf metric.
     */
    template <class Q>
    [[nodiscard]] const ShapeType& nearestNeighborLInf(const Q& q) const {
        using ResultNumber = std::remove_cvref_t<decltype(
            q.distanceLInf(std::declval<const ShapeType&>()))>;
        return nearestNeighborByMetric<detail::LInfMetric, ResultNumber>(q);
    }

    template <class ResultNumber, class Q>
    [[nodiscard]] const ShapeType& nearestNeighborLInf(const Q& q) const {
        return nearestNeighborByMetric<detail::LInfMetric, ResultNumber>(q);
    }

    /**
     * @brief Returns every node's subtree bounding box in pre-order.
     *
     * Visits the root, then the left subtree, then the right subtree, collecting
     * each node's cached bounding box. The result is empty for an empty tree.
     *
     * @return Vector of the node bounding boxes in pre-order.
     */
    [[nodiscard]] std::vector<Rect> boundingBoxes() const {
        std::vector<Rect> out;
        out.reserve(nodes_.size());
        collectBoundingBoxes(root_, out);
        return out;
    }

    /**
     * @brief Draws every node's subtree bounding box to a canvas in pre-order.
     *
     * Sends each box returned by @ref boundingBoxes to the canvas with its
     * current style.
     *
     * @param canvas Destination canvas.
     * @param tree Tree whose node boxes are drawn.
     * @return The canvas.
     */
    friend Canvas& operator<<(Canvas& canvas, const ShapeTree& tree) {
        for (const Rect& box : tree.boundingBoxes()) {
            canvas << box;
        }
        return canvas;
    }
};

// Deduction guides: the shape type S is not deducible from the templated
// constructors on their own (the constructor is templated on the container,
// not the element), so without these CTAD fails and S must always be named.
// These deduce S from the container's value type, preserving its label.
template <class Container>
ShapeTree(const Container&) -> ShapeTree<typename Container::value_type>;

template <class Container>
ShapeTree(const Container&, std::size_t) -> ShapeTree<typename Container::value_type>;

template <class Container, class WeightFn>
ShapeTree(const Container&, std::size_t, WeightFn)
    -> ShapeTree<typename Container::value_type, WeightFn>;

// A weight given without a leaf size: constrained off the integral overload so
// `ShapeTree(shapes, leafSize)` still deduces the default weight.
template <class Container, class WeightFn>
    requires(!std::is_integral_v<WeightFn>)
ShapeTree(const Container&, WeightFn)
    -> ShapeTree<typename Container::value_type, WeightFn>;

}  // namespace pgl
