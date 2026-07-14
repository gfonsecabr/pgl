#pragma once

#include "algorithm/convexhull.hpp"

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
 * exact integer rectangle predicates.
 */

#include <algorithm>
#include <cstddef>
#include <type_traits>
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
// branch-and-bound. Prefers the templated `squaredDistance<ResultNumber>`
// overload; falls back to the untemplated, double-returning overload used
// whenever a Disk is involved (a disk's exterior distance is irrational, so it
// is never exact and thus never templated on a result type).
template <class ResultNumber, class Q, class Other>
[[nodiscard]] ResultNumber nearestSquaredDistance(const Q& q, const Other& other) {
    if constexpr (requires { q.template squaredDistance<ResultNumber>(other); }) {
        return q.template squaredDistance<ResultNumber>(other);
    } else {
        return static_cast<ResultNumber>(q.squaredDistance(other));
    }
}

// Same fallback dance as nearestSquaredDistance, for the L1 (Manhattan) metric.
template <class ResultNumber, class Q, class Other>
[[nodiscard]] ResultNumber nearestDistanceL1(const Q& q, const Other& other) {
    if constexpr (requires { q.template distanceL1<ResultNumber>(other); }) {
        return q.template distanceL1<ResultNumber>(other);
    } else {
        return static_cast<ResultNumber>(q.distanceL1(other));
    }
}

// Same fallback dance as nearestSquaredDistance, for the LInf (Chebyshev) metric.
template <class ResultNumber, class Q, class Other>
[[nodiscard]] ResultNumber nearestDistanceLInf(const Q& q, const Other& other) {
    if constexpr (requires { q.template distanceLInf<ResultNumber>(other); }) {
        return q.template distanceLInf<ResultNumber>(other);
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
    struct Node {
        Rect box;  // Union bounding box of the whole subtree.
        std::ptrdiff_t left = -1, right = -1;
        std::size_t count = 0;  // Number of elements in the whole subtree.
        [[no_unique_address]] WeightType weightSum{};  // Sum of subtree weights.
        std::vector<std::size_t> elementIndices;  // Elements owned by this node.

        template <class Q>
        [[nodiscard]] std::size_t countIntersecting(const ShapeTree& tree, const Q& q) const {
            if (!q.intersects(box)) {
                return 0;
            }
            if (q.contains(box)) {
                // The whole subtree lies inside q, so every element intersects it.
                return count;
            }
            std::size_t ret = 0;
            for (std::size_t i : elementIndices) {
                if (tree.elements_[i].intersects(q)) {
                    ret++;
                }
            }
            if (left != -1) {
                ret += tree.nodes_[left].countIntersecting(tree, q);
            }
            if (right != -1) {
                ret += tree.nodes_[right].countIntersecting(tree, q);
            }
            return ret;
        }

        template <class Q>
        [[nodiscard]] WeightType sumIntersecting(const ShapeTree& tree, const Q& q) const {
            if (!q.intersects(box)) {
                return WeightType{};
            }
            if (q.contains(box)) {
                // The whole subtree lies inside q, so it contributes its full sum.
                return weightSum;
            }
            WeightType ret{};
            for (std::size_t i : elementIndices) {
                if (tree.elements_[i].intersects(q)) {
                    ret = ret + tree.weight_(tree.elements_[i]);
                }
            }
            if (left != -1) {
                ret = ret + tree.nodes_[left].sumIntersecting(tree, q);
            }
            if (right != -1) {
                ret = ret + tree.nodes_[right].sumIntersecting(tree, q);
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

        template <class Q>
        void reportIntersecting(const ShapeTree& tree, const Q& q, std::vector<ShapeType>& out) const {
            if (!q.intersects(box)) {
                return;
            }
            if (q.contains(box)) {
                // The whole subtree lies inside q, so every element intersects it.
                collectAll(tree, out);
                return;
            }
            for (std::size_t i : elementIndices) {
                if (tree.elements_[i].intersects(q)) {
                    out.push_back(tree.elements_[i]);
                }
            }
            if (left != -1) {
                tree.nodes_[left].reportIntersecting(tree, q, out);
            }
            if (right != -1) {
                tree.nodes_[right].reportIntersecting(tree, q, out);
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

        template <class Q, class Fn>
        [[nodiscard]] bool visitIntersecting(const ShapeTree& tree, const Q& q, Fn& fn) const {
            if (!q.intersects(box)) {
                return false;
            }
            if (q.contains(box)) {
                // The whole subtree lies inside q, so every element intersects it.
                return visitAll(tree, fn);
            }
            for (std::size_t i : elementIndices) {
                if (tree.elements_[i].intersects(q) &&
                    detail::invokeVisitor(fn, tree.elements_[i])) {
                    return true;
                }
            }
            if (left != -1 && tree.nodes_[left].visitIntersecting(tree, q, fn)) {
                return true;
            }
            if (right != -1 && tree.nodes_[right].visitIntersecting(tree, q, fn)) {
                return true;
            }
            return false;
        }

        template <class Q>
        [[nodiscard]] bool anyIntersecting(const ShapeTree& tree, const Q& q) const {
            if (!q.intersects(box)) {
                return false;
            }
            if (q.contains(box)) {
                // The whole (non-empty) subtree lies inside q.
                return true;
            }
            for (std::size_t i : elementIndices) {
                if (tree.elements_[i].intersects(q)) {
                    return true;
                }
            }
            if (left != -1 && tree.nodes_[left].anyIntersecting(tree, q)) {
                return true;
            }
            if (right != -1 && tree.nodes_[right].anyIntersecting(tree, q)) {
                return true;
            }
            return false;
        }

        // --- containment: stored element contained in the query (element ⊆ q) ---

        template <class Q>
        [[nodiscard]] std::size_t countContainedIn(const ShapeTree& tree, const Q& q) const {
            if (!q.intersects(box)) {
                return 0;
            }
            if (q.contains(box)) {
                // Every element ⊆ box ⊆ q.
                return count;
            }
            std::size_t ret = 0;
            for (std::size_t i : elementIndices) {
                if (q.contains(tree.elements_[i])) {
                    ret++;
                }
            }
            if (left != -1) {
                ret += tree.nodes_[left].countContainedIn(tree, q);
            }
            if (right != -1) {
                ret += tree.nodes_[right].countContainedIn(tree, q);
            }
            return ret;
        }

        template <class Q>
        [[nodiscard]] WeightType sumContainedIn(const ShapeTree& tree, const Q& q) const {
            if (!q.intersects(box)) {
                return WeightType{};
            }
            if (q.contains(box)) {
                return weightSum;
            }
            WeightType ret{};
            for (std::size_t i : elementIndices) {
                if (q.contains(tree.elements_[i])) {
                    ret = ret + tree.weight_(tree.elements_[i]);
                }
            }
            if (left != -1) {
                ret = ret + tree.nodes_[left].sumContainedIn(tree, q);
            }
            if (right != -1) {
                ret = ret + tree.nodes_[right].sumContainedIn(tree, q);
            }
            return ret;
        }

        template <class Q>
        void reportContainedIn(const ShapeTree& tree, const Q& q, std::vector<ShapeType>& out) const {
            if (!q.intersects(box)) {
                return;
            }
            if (q.contains(box)) {
                collectAll(tree, out);
                return;
            }
            for (std::size_t i : elementIndices) {
                if (q.contains(tree.elements_[i])) {
                    out.push_back(tree.elements_[i]);
                }
            }
            if (left != -1) {
                tree.nodes_[left].reportContainedIn(tree, q, out);
            }
            if (right != -1) {
                tree.nodes_[right].reportContainedIn(tree, q, out);
            }
        }

        template <class Q, class Fn>
        [[nodiscard]] bool visitContainedIn(const ShapeTree& tree, const Q& q, Fn& fn) const {
            if (!q.intersects(box)) {
                return false;
            }
            if (q.contains(box)) {
                return visitAll(tree, fn);
            }
            for (std::size_t i : elementIndices) {
                if (q.contains(tree.elements_[i]) &&
                    detail::invokeVisitor(fn, tree.elements_[i])) {
                    return true;
                }
            }
            if (left != -1 && tree.nodes_[left].visitContainedIn(tree, q, fn)) {
                return true;
            }
            if (right != -1 && tree.nodes_[right].visitContainedIn(tree, q, fn)) {
                return true;
            }
            return false;
        }

        template <class Q>
        [[nodiscard]] bool anyContainedIn(const ShapeTree& tree, const Q& q) const {
            if (!q.intersects(box)) {
                return false;
            }
            if (q.contains(box)) {
                return true;
            }
            for (std::size_t i : elementIndices) {
                if (q.contains(tree.elements_[i])) {
                    return true;
                }
            }
            if (left != -1 && tree.nodes_[left].anyContainedIn(tree, q)) {
                return true;
            }
            if (right != -1 && tree.nodes_[right].anyContainedIn(tree, q)) {
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
                    if (d == ResultNumber{}) {
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
    };

    static constexpr std::size_t defaultLeafSize = 6;

    std::vector<ShapeType> elements_;
    std::vector<Node> nodes_;
    std::ptrdiff_t root_ = -1;
    std::size_t leafSize_ = defaultLeafSize;
    [[no_unique_address]] WeightFn weight_{};

    // Appends a fresh node and returns its index, centralizing the "the index is
    // the current size, then push" pattern shared by build and insert.
    std::ptrdiff_t allocNode() {
        const std::ptrdiff_t id = static_cast<std::ptrdiff_t>(nodes_.size());
        nodes_.push_back(Node{});
        return id;
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

    // Finds the split on `axis` minimizing maxChild + straddlers, which balances
    // the children while keeping few elements stuck at the node; ties are broken
    // toward fewer straddlers.
    Split bestSplitOnAxis(const std::vector<std::size_t>& indices, std::size_t axis) const {
        const std::size_t n = indices.size();

        // Sorted box extents on the axis: lo = bbox min coord, hi = max coord.
        std::vector<NumberType> los, his;
        los.reserve(n);
        his.reserve(n);
        for (std::size_t i : indices) {
            los.push_back(elements_[i].bbox().min()[axis]);
            his.push_back(elements_[i].bbox().max()[axis]);
        }
        std::sort(los.begin(), los.end());
        std::sort(his.begin(), his.end());

        // Counts on each side only change at the distinct extent coordinates.
        std::vector<NumberType> candidates = his;
        candidates.insert(candidates.end(), los.begin(), los.end());
        std::sort(candidates.begin(), candidates.end());
        candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());

        Split best;
        best.axis = axis;
        for (const NumberType& v : candidates) {
            // left: boxes entirely <= v (hi <= v); right: entirely > v (lo > v).
            const std::size_t leftCount =
                static_cast<std::size_t>(std::upper_bound(his.begin(), his.end(), v) - his.begin());
            const std::size_t rightCount =
                static_cast<std::size_t>(los.end() - std::upper_bound(los.begin(), los.end(), v));
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

    // Chooses the best split over `indices`, trying the depth-parity axis first
    // so equal-scoring splits alternate direction.
    Split chooseSplit(const std::vector<std::size_t>& indices, int level) const {
        Split best;
        for (int k = 0; k < 2; ++k) {
            const std::size_t axis = static_cast<std::size_t>((level + k) % 2);
            const Split candidate = bestSplitOnAxis(indices, axis);
            if (!candidate.found) {
                continue;
            }
            if (!best.found || candidate.score < best.score ||
                (candidate.score == best.score && candidate.straddlers < best.straddlers)) {
                best = candidate;
            }
        }
        return best;
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

    // Builds a subtree from the given element indices and returns its node index.
    // `level` is the depth, used only to break ties between equally good axes so
    // the split direction alternates (e.g. for points, where both axes always
    // score the same).
    std::ptrdiff_t build(const std::vector<std::size_t>& indices, int level) {
        Rect box = Rect(elements_[indices[0]].bbox());
        WeightType weightSum = weight_(elements_[indices[0]]);
        for (std::size_t k = 1; k < indices.size(); ++k) {
            box.insert(elements_[indices[k]].bbox());
            weightSum = weightSum + weight_(elements_[indices[k]]);
        }

        // Reserve this node's slot now; recursion may reallocate nodes_, so the
        // node is always addressed by index, never by a dangling reference.
        const std::ptrdiff_t id = allocNode();
        nodes_[id].box = box;
        nodes_[id].count = indices.size();
        nodes_[id].weightSum = weightSum;

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
            }
            nodes_.pop_back();
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

    // Discards the current node structure and rebuilds it from elements_.
    void buildFromElements() {
        nodes_.clear();
        root_ = -1;
        if (elements_.empty()) {
            return;
        }
        std::vector<std::size_t> indices(elements_.size());
        for (std::size_t i = 0; i < indices.size(); ++i) {
            indices[i] = i;
        }
        nodes_.reserve(2 * elements_.size() / leafSize_ + 1);
        root_ = build(indices, 0);
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

        if (root_ == -1) {
            root_ = allocNode();
            nodes_[root_].box = eb;
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
        }
        elements_.pop_back();

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
        return root_ == -1 ? 0 : nodes_[root_].countIntersecting(*this, q);
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
        return root_ == -1 ? WeightType{} : nodes_[root_].sumIntersecting(*this, q);
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
            nodes_[root_].reportIntersecting(*this, q, out);
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
        return root_ == -1 ? false : nodes_[root_].visitIntersecting(*this, q, fn);
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
        return root_ == -1 ? true : !nodes_[root_].anyIntersecting(*this, q);
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
        return root_ == -1 ? 0 : nodes_[root_].countContainedIn(*this, q);
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
        return root_ == -1 ? WeightType{} : nodes_[root_].sumContainedIn(*this, q);
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
            nodes_[root_].reportContainedIn(*this, q, out);
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
        return root_ == -1 ? false : nodes_[root_].visitContainedIn(*this, q, fn);
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
        return root_ == -1 ? true : !nodes_[root_].anyContainedIn(*this, q);
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
     * Like the other distance operations, the squared distance is computed with
     * the @ref NumberType coordinate type by default; request an exact
     * `ResultNumber` (a floating-point type or `pgl::Rational`) when a stored
     * shape's nearest point falls in its interior, since integer division there
     * truncates. With a truncating `ResultNumber` the box lower bound stays a
     * lower bound, so the pruning remains conservative.
     *
     * If `Disk` is involved (as `ShapeType` or as `Q`), its distance is
     * irrational and thus never templated on a result type; those legs of the
     * comparison fall back to the untemplated `double`-returning overload,
     * converted to `ResultNumber`.
     *
     * @pre The tree is non-empty. A reference to a default-constructed
     *      @ref ShapeType is returned otherwise.
     *
     * The reference points into the tree's own storage and stays valid until the
     * tree is destroyed or modified (e.g. by @ref insert).
     *
     * @tparam ResultNumber Coordinate type of the squared distance (default:
     *         @ref NumberType).
     * @tparam Q Query shape type.
     * @param q Query shape.
     * @return The nearest stored shape.
     */
    template <class ResultNumber = NumberType, class Q>
    [[nodiscard]] const ShapeType& nearestNeighbor(const Q& q) const {
        return nearestNeighborByMetric<detail::SquaredMetric, ResultNumber>(q);
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
     * @tparam ResultNumber Coordinate type of the distance (default:
     *         @ref NumberType).
     * @tparam Q Query shape type.
     * @param q Query shape.
     * @return The stored shape nearest to `q` under the L1 metric.
     */
    template <class ResultNumber = NumberType, class Q>
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
     * @tparam ResultNumber Coordinate type of the distance (default:
     *         @ref NumberType).
     * @tparam Q Query shape type.
     * @param q Query shape.
     * @return The stored shape nearest to `q` under the LInf metric.
     */
    template <class ResultNumber = NumberType, class Q>
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
