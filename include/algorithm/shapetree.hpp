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
#include <vector>


namespace pgl {

/**
 * @brief Static shape tree of bounded shapes.
 *
 * @tparam S Any shape type exposing `bbox()` (Point, Segment, Triangle,
 *         Rectangle, Convex, Polygon, ...). Infinite shapes such as Line, Ray
 *         and Halfplane have no finite bounding box and are not supported.
 */
template <class S>
class ShapeTree {
  public:
    using ShapeType = S;
    using Rect = std::remove_cvref_t<decltype(std::declval<const S&>().bbox())>;
    using PointType = typename Rect::PointType;
    using NumberType = typename PointType::NumberType;

  private:
    struct Node {
        Rect box;  // Union bounding box of the whole subtree.
        std::ptrdiff_t left = -1, right = -1;
        std::vector<std::size_t> elementIndices;  // Elements owned by this node.

        template <class Q>
        [[nodiscard]] std::size_t countIntersects(const ShapeTree& tree, const Q& q) const {
            if (!q.intersects(box)) {
                return 0;
            }
            std::size_t ret = 0;
            for (std::size_t i : elementIndices) {
                if (tree.elements_[i].intersects(q)) {
                    ret++;
                }
            }
            if (left != -1) {
                ret += tree.nodes_[left].countIntersects(tree, q);
            }
            if (right != -1) {
                ret += tree.nodes_[right].countIntersects(tree, q);
            }
            return ret;
        }
    };

    static constexpr std::size_t defaultLeafSize = 8;

    std::vector<ShapeType> elements_;
    std::vector<Node> nodes_;
    std::ptrdiff_t root_ = -1;
    std::size_t leafSize_ = defaultLeafSize;

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

    // Builds a subtree from the given element indices and returns its node index.
    // `level` is the depth, used only to break ties between equally good axes so
    // the split direction alternates (e.g. for points, where both axes always
    // score the same).
    std::ptrdiff_t build(const std::vector<std::size_t>& indices, int level) {
        Rect box = Rect(elements_[indices[0]].bbox());
        for (std::size_t k = 1; k < indices.size(); ++k) {
            box.insert(elements_[indices[k]].bbox());
        }

        // Reserve this node's slot now; recursion may reallocate nodes_, so the
        // node is always addressed by index, never by a dangling reference.
        const std::ptrdiff_t id = static_cast<std::ptrdiff_t>(nodes_.size());
        nodes_.push_back(Node{});
        nodes_[id].box = box;

        if (indices.size() <= leafSize_) {
            nodes_[id].elementIndices = indices;
            return id;
        }

        // Choose the axis with the lower score, then fewer straddlers. Remaining
        // ties go to the first axis tried; trying the depth-parity axis first
        // makes the split direction alternate when both axes are fully equal.
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

        if (!best.found) {
            // No axis can separate the elements (e.g. many identical boxes):
            // keep them all here as a leaf.
            nodes_[id].elementIndices = indices;
            return id;
        }

        std::vector<std::size_t> leftIndices, rightIndices, straddlers;
        const auto a = static_cast<std::size_t>(best.axis);
        for (std::size_t i : indices) {
            const NumberType lo = elements_[i].bbox().min()[a];
            const NumberType hi = elements_[i].bbox().max()[a];
            if (hi <= best.value) {
                leftIndices.push_back(i);
            } else if (lo > best.value) {
                rightIndices.push_back(i);
            } else {
                straddlers.push_back(i);
            }
        }

        const std::ptrdiff_t leftChild = leftIndices.empty() ? -1 : build(leftIndices, level + 1);
        const std::ptrdiff_t rightChild = rightIndices.empty() ? -1 : build(rightIndices, level + 1);
        nodes_[id].left = leftChild;
        nodes_[id].right = rightChild;
        nodes_[id].elementIndices = std::move(straddlers);
        return id;
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
     */
    template <class Container>
    explicit ShapeTree(const Container& shapes, std::size_t leafSize = defaultLeafSize)
        : leafSize_(leafSize > 0 ? leafSize : 1) {
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

    /** @brief Returns the number of stored shapes. */
    [[nodiscard]] std::size_t size() const {
        return elements_.size();
    }

    /** @brief Returns whether the tree is empty. */
    [[nodiscard]] bool empty() const {
        return elements_.empty();
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
    [[nodiscard]] std::size_t countIntersects(const Q& q) const {
        return root_ == -1 ? 0 : nodes_[root_].countIntersects(*this, q);
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

}  // namespace pgl
