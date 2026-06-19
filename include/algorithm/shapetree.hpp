#pragma once

#include "pgl.hpp"

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

        // --- nearest neighbor: branch-and-bound on squared distance ---

        // Refines (bestDist, bestIndex) with the closest element in this subtree.
        // The subtree box gives a lower bound on the distance from q to anything
        // it stores, so a subtree whose box is no nearer than the current best is
        // pruned entirely. Children are descended nearest-box-first so the bound
        // tightens before the farther child is considered.
        template <class ResultNumber, class Q>
        void nearest(const ShapeTree& tree, const Q& q, ResultNumber& bestDist,
                     std::ptrdiff_t& bestIndex) const {
            if (bestIndex != -1) {
                const ResultNumber lowerBound = q.template squaredDistance<ResultNumber>(box);
                if (!(lowerBound < bestDist)) {
                    return;  // Nothing here can beat the current best.
                }
            }

            for (std::size_t i : elementIndices) {
                const ResultNumber d = q.template squaredDistance<ResultNumber>(tree.elements_[i]);
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
                    tree.nodes_[right].nearest(tree, q, bestDist, bestIndex);
                }
                return;
            }
            if (right == -1) {
                tree.nodes_[left].nearest(tree, q, bestDist, bestIndex);
                return;
            }

            const ResultNumber leftBound = q.template squaredDistance<ResultNumber>(tree.nodes_[left].box);
            const ResultNumber rightBound = q.template squaredDistance<ResultNumber>(tree.nodes_[right].box);
            const std::ptrdiff_t near = leftBound <= rightBound ? left : right;
            const std::ptrdiff_t far = leftBound <= rightBound ? right : left;
            tree.nodes_[near].nearest(tree, q, bestDist, bestIndex);
            tree.nodes_[far].nearest(tree, q, bestDist, bestIndex);
        }
    };

    static constexpr std::size_t defaultLeafSize = 6;

    std::vector<ShapeType> elements_;
    std::vector<Node> nodes_;
    std::ptrdiff_t root_ = -1;
    std::size_t leafSize_ = defaultLeafSize;
    [[no_unique_address]] WeightFn weight_{};

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
        const std::ptrdiff_t id = static_cast<std::ptrdiff_t>(nodes_.size());
        nodes_.push_back(Node{});
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

    // Sends the subtree bounding boxes to the canvas in pre-order.
    void drawSubtree(Canvas& canvas, std::ptrdiff_t id) const {
        if (id == -1) {
            return;
        }
        canvas << nodes_[id].box;
        drawSubtree(canvas, nodes_[id].left);
        drawSubtree(canvas, nodes_[id].right);
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
        const std::size_t i = elements_.size();
        elements_.push_back(shape);
        const Rect eb = Rect(elements_[i].bbox());

        if (root_ == -1) {
            root_ = static_cast<std::ptrdiff_t>(nodes_.size());
            nodes_.push_back(Node{});
            nodes_[root_].box = eb;
            nodes_[root_].count = 1;
            nodes_[root_].weightSum = weight_(elements_[i]);
            nodes_[root_].elementIndices.push_back(i);
            return;
        }
        insertInto(root_, i, eb, 0);
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
        if (root_ == -1) {
            static const ShapeType empty{};
            return empty;
        }
        ResultNumber bestDist{};
        std::ptrdiff_t bestIndex = -1;
        nodes_[root_].template nearest<ResultNumber>(*this, q, bestDist, bestIndex);
        return elements_[static_cast<std::size_t>(bestIndex)];
    }

    /**
     * @brief Draws every node's subtree bounding box to a canvas in pre-order.
     *
     * Visits the root, then the left subtree, then the right subtree, sending
     * each node's cached bounding box to the canvas with its current style.
     *
     * @param canvas Destination canvas.
     * @param tree Tree whose node boxes are drawn.
     * @return The canvas.
     */
    friend Canvas& operator<<(Canvas& canvas, const ShapeTree& tree) {
        tree.drawSubtree(canvas, tree.root_);
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
