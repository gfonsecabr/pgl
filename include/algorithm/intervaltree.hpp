#pragma once

#include "algorithm/convexhull.hpp"

/**
 * @file intervaltree.hpp
 * @brief Mutable one-dimensional interval tree over projected bounded shapes.
 */

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>


namespace pgl {

/** @brief Axis used to project a shape's bounding box into an interval. */
enum class ProjectionAxis { x, y };

namespace detail {

// Calls a visitor and reports whether it requested traversal to stop. A bool
// result stops on true; a void result always continues.
template <class Fn, class Arg>
[[nodiscard]] bool invokeIntervalTreeVisitor(Fn& fn, const Arg& arg) {
    if constexpr (std::is_same_v<std::invoke_result_t<Fn&, const Arg&>, bool>) {
        return fn(arg);
    } else {
        fn(arg);
        return false;
    }
}

}  // namespace detail

/**
 * @brief Mutable interval tree over the projection of bounded shapes.
 *
 * Each stored shape owns one closed interval: the x or y extent of its
 * bounding box, selected by @p Axis. Queries apply the same projection to
 * their argument, so this is intentionally a one-dimensional index: matching
 * does not imply that the original two-dimensional shapes meet or contain one
 * another.
 *
 * Nodes form a red-black tree ordered by `(low endpoint, high endpoint, node
 * ID)`. The ID keeps equal projected intervals distinct.
 * Every node caches the extrema of both endpoints in its subtree. In
 * particular, `maxHigh` is the standard augmented interval-tree value used to
 * prune subtrees lying completely before an intersection query. Query fields
 * are stored separately from insertion-only parent/color state, and 32-bit
 * node IDs keep the hot representation compact. Consequently, one tree can
 * hold at most `2^32 - 1` shapes.
 *
 * @tparam S Shape type exposing a finite `bbox()`.
 * @tparam Axis Coordinate used for the one-dimensional projection.
 */
template <class S, ProjectionAxis Axis = ProjectionAxis::x>
class IntervalTree {
  public:
    using ShapeType = S;
    using BoxType = std::remove_cvref_t<decltype(std::declval<const S&>().bbox())>;
    using NumberType = std::remove_cvref_t<decltype(std::declval<const BoxType&>().min().x())>;

    using value_type = ShapeType;
    using size_type = std::size_t;
    using const_iterator = typename std::vector<ShapeType>::const_iterator;
    using const_reference = const ShapeType&;

  private:
    using NodeId = std::uint32_t;
    static constexpr NodeId invalidNode = std::numeric_limits<NodeId>::max();

    enum class Color : unsigned char { red, black };

    // Fields touched by range queries stay together in a compact array. Node
    // IDs also index elements_, so no element index or insertion serial is
    // stored per node.
    struct QueryNode {
        NumberType low{};
        NumberType high{};
        NumberType minLow{};
        NumberType maxLow{};
        NumberType minHigh{};
        NumberType maxHigh{};
        std::uint32_t count = 0;
        NodeId left = invalidNode;
        NodeId right = invalidNode;
    };

    // Insertion-only state is kept cold so queries do not pull it into cache.
    struct MutationNode {
        NodeId parent = invalidNode;
        Color color = Color::black;
    };

    std::vector<ShapeType> elements_;
    std::vector<QueryNode> nodes_;
    std::vector<MutationNode> mutationNodes_;
    NodeId root_ = invalidNode;

    template <class T>
    [[nodiscard]] static bool less(const T& a, const T& b) {
        return a < b;
    }

    template <class A, class B>
    [[nodiscard]] static bool equivalent(const A& a, const B& b) {
        return !(a < b) && !(b < a);
    }

    template <class A, class B>
    [[nodiscard]] static const auto& minimum(const A& a, const B& b) {
        return b < a ? b : a;
    }

    template <class A, class B>
    [[nodiscard]] static const auto& maximum(const A& a, const B& b) {
        return a < b ? b : a;
    }

    [[nodiscard]] const QueryNode& node(NodeId id) const {
        return nodes_[static_cast<std::size_t>(id)];
    }

    [[nodiscard]] QueryNode& node(NodeId id) {
        return nodes_[static_cast<std::size_t>(id)];
    }

    [[nodiscard]] const MutationNode& mutationNode(NodeId id) const {
        return mutationNodes_[static_cast<std::size_t>(id)];
    }

    [[nodiscard]] MutationNode& mutationNode(NodeId id) {
        return mutationNodes_[static_cast<std::size_t>(id)];
    }

    [[nodiscard]] static Color colorOf(const IntervalTree& tree, NodeId id) {
        return id == invalidNode ? Color::black : tree.mutationNode(id).color;
    }

    template <class Shape>
    [[nodiscard]] static auto project(const Shape& shape) {
        const auto box = shape.bbox();
        if constexpr (Axis == ProjectionAxis::x) {
            return std::pair{box.min().x(), box.max().x()};
        } else {
            return std::pair{box.min().y(), box.max().y()};
        }
    }

    [[nodiscard]] static bool intervalLess(const NumberType& lowA, const NumberType& highA,
                                           const NumberType& lowB, const NumberType& highB) {
        if (less(lowA, lowB)) {
            return true;
        }
        if (less(lowB, lowA)) {
            return false;
        }
        return less(highA, highB);
    }

    [[nodiscard]] bool keyLess(const NumberType& low, const NumberType& high,
                               NodeId id, NodeId otherId) const {
        const QueryNode& other = node(otherId);
        if (intervalLess(low, high, other.low, other.high)) {
            return true;
        }
        if (intervalLess(other.low, other.high, low, high)) {
            return false;
        }
        return id < otherId;
    }

    void update(NodeId id) {
        if (id == invalidNode) {
            return;
        }
        QueryNode& n = node(id);
        n.minLow = n.maxLow = n.low;
        n.minHigh = n.maxHigh = n.high;
        n.count = 1;
        for (const NodeId child : {n.left, n.right}) {
            if (child == invalidNode) {
                continue;
            }
            const QueryNode& c = node(child);
            n.minLow = minimum(n.minLow, c.minLow);
            n.maxLow = maximum(n.maxLow, c.maxLow);
            n.minHigh = minimum(n.minHigh, c.minHigh);
            n.maxHigh = maximum(n.maxHigh, c.maxHigh);
            n.count += c.count;
        }
    }

    void updateUpward(NodeId id) {
        while (id != invalidNode) {
            update(id);
            id = mutationNode(id).parent;
        }
    }

    [[nodiscard]] NodeId allocateNode(const NumberType& low, const NumberType& high) {
        if (nodes_.size() >= static_cast<std::size_t>(invalidNode)) {
            throw std::length_error("IntervalTree exceeds its 32-bit node capacity");
        }

        QueryNode fresh;
        fresh.low = fresh.minLow = fresh.maxLow = low;
        fresh.high = fresh.minHigh = fresh.maxHigh = high;
        fresh.count = 1;
        nodes_.push_back(std::move(fresh));
        try {
            mutationNodes_.push_back(MutationNode{invalidNode, Color::red});
        } catch (...) {
            nodes_.pop_back();
            throw;
        }
        return static_cast<NodeId>(nodes_.size() - 1);
    }

    void rotateLeft(NodeId x) {
        const NodeId y = node(x).right;
        node(x).right = node(y).left;
        if (node(y).left != invalidNode) {
            mutationNode(node(y).left).parent = x;
        }
        mutationNode(y).parent = mutationNode(x).parent;
        if (mutationNode(x).parent == invalidNode) {
            root_ = y;
        } else if (x == node(mutationNode(x).parent).left) {
            node(mutationNode(x).parent).left = y;
        } else {
            node(mutationNode(x).parent).right = y;
        }
        node(y).left = x;
        mutationNode(x).parent = y;
        update(x);
        update(y);
    }

    void rotateRight(NodeId x) {
        const NodeId y = node(x).left;
        node(x).left = node(y).right;
        if (node(y).right != invalidNode) {
            mutationNode(node(y).right).parent = x;
        }
        mutationNode(y).parent = mutationNode(x).parent;
        if (mutationNode(x).parent == invalidNode) {
            root_ = y;
        } else if (x == node(mutationNode(x).parent).right) {
            node(mutationNode(x).parent).right = y;
        } else {
            node(mutationNode(x).parent).left = y;
        }
        node(y).right = x;
        mutationNode(x).parent = y;
        update(x);
        update(y);
    }

    void insertFixup(NodeId z) {
        while (z != root_ && colorOf(*this, mutationNode(z).parent) == Color::red) {
            const NodeId parent = mutationNode(z).parent;
            const NodeId grandparent = mutationNode(parent).parent;
            if (parent == node(grandparent).left) {
                NodeId uncle = node(grandparent).right;
                if (colorOf(*this, uncle) == Color::red) {
                    mutationNode(parent).color = Color::black;
                    mutationNode(uncle).color = Color::black;
                    mutationNode(grandparent).color = Color::red;
                    z = grandparent;
                } else {
                    if (z == node(parent).right) {
                        z = parent;
                        rotateLeft(z);
                    }
                    mutationNode(mutationNode(z).parent).color = Color::black;
                    mutationNode(mutationNode(mutationNode(z).parent).parent).color = Color::red;
                    rotateRight(mutationNode(mutationNode(z).parent).parent);
                }
            } else {
                NodeId uncle = node(grandparent).left;
                if (colorOf(*this, uncle) == Color::red) {
                    mutationNode(parent).color = Color::black;
                    mutationNode(uncle).color = Color::black;
                    mutationNode(grandparent).color = Color::red;
                    z = grandparent;
                } else {
                    if (z == node(parent).left) {
                        z = parent;
                        rotateRight(z);
                    }
                    mutationNode(mutationNode(z).parent).color = Color::black;
                    mutationNode(mutationNode(mutationNode(z).parent).parent).color = Color::red;
                    rotateLeft(mutationNode(mutationNode(z).parent).parent);
                }
            }
        }
        mutationNode(root_).color = Color::black;
    }

    void insertExisting(const NumberType& low, const NumberType& high) {
        const NodeId z = allocateNode(low, high);

        NodeId parent = invalidNode;
        NodeId current = root_;
        while (current != invalidNode) {
            parent = current;
            if (keyLess(low, high, z, current)) {
                current = node(current).left;
            } else {
                current = node(current).right;
            }
        }
        mutationNode(z).parent = parent;
        if (parent == invalidNode) {
            root_ = z;
        } else if (keyLess(low, high, z, parent)) {
            node(parent).left = z;
        } else {
            node(parent).right = z;
        }
        updateUpward(z);
        insertFixup(z);
        updateUpward(z);
    }

    void rebuildFromElements() {
        nodes_.clear();
        mutationNodes_.clear();
        nodes_.reserve(elements_.size());
        mutationNodes_.reserve(elements_.size());
        root_ = invalidNode;
        for (std::size_t i = 0; i < elements_.size(); ++i) {
            const auto [low, high] = project(elements_[i]);
            insertExisting(low, high);
        }
    }

    [[nodiscard]] NodeId minimumNode(NodeId id) const {
        while (node(id).left != invalidNode) {
            id = node(id).left;
        }
        return id;
    }

    [[nodiscard]] NodeId successor(NodeId id) const {
        if (node(id).right != invalidNode) {
            return minimumNode(node(id).right);
        }
        NodeId parent = mutationNode(id).parent;
        while (parent != invalidNode && id == node(parent).right) {
            id = parent;
            parent = mutationNode(parent).parent;
        }
        return parent;
    }

    [[nodiscard]] NodeId lowerBoundInterval(const NumberType& low,
                                            const NumberType& high) const {
        NodeId id = root_;
        NodeId result = invalidNode;
        while (id != invalidNode) {
            const QueryNode& n = node(id);
            if (intervalLess(n.low, n.high, low, high)) {
                id = n.right;
            } else {
                result = id;
                id = n.left;
            }
        }
        return result;
    }

    [[nodiscard]] NodeId findEqualNode(const ShapeType& shape, const NumberType& low,
                                      const NumberType& high) const {
        for (NodeId id = lowerBoundInterval(low, high); id != invalidNode; id = successor(id)) {
            const QueryNode& n = node(id);
            if (!equivalent(n.low, low) || !equivalent(n.high, high)) {
                break;
            }
            if (elements_[static_cast<std::size_t>(id)] == shape) {
                return id;
            }
        }
        return invalidNode;
    }

    template <class Low, class High>
    [[nodiscard]] static bool intersects(const QueryNode& n, const Low& low, const High& high) {
        return !(n.high < low) && !(high < n.low);
    }

    template <class Low, class High>
    [[nodiscard]] static bool mayIntersect(const QueryNode& n, const Low& low, const High& high) {
        return !(n.maxHigh < low) && !(high < n.minLow);
    }

    template <class Low, class High>
    [[nodiscard]] static bool allIntersect(const QueryNode& n, const Low& low, const High& high) {
        return !(high < n.maxLow) && !(n.minHigh < low);
    }

    template <class Low, class High>
    [[nodiscard]] static bool containedIn(const QueryNode& n, const Low& low, const High& high) {
        return !(n.low < low) && !(high < n.low) && !(high < n.high);
    }

    template <class Low, class High>
    [[nodiscard]] static bool mayContain(const QueryNode& n, const Low& low, const High& high) {
        return !(n.maxLow < low) && !(high < n.minLow) && !(high < n.minHigh);
    }

    template <class Low, class High>
    [[nodiscard]] static bool allContainedIn(const QueryNode& n, const Low& low, const High& high) {
        return !(n.minLow < low) && !(high < n.maxLow) && !(high < n.maxHigh);
    }

    template <class Fn>
    [[nodiscard]] bool visitAll(NodeId id, Fn& fn) const {
        if (id == invalidNode) {
            return false;
        }
        const QueryNode& n = node(id);
        if (detail::invokeIntervalTreeVisitor(fn, elements_[static_cast<std::size_t>(id)])) {
            return true;
        }
        return visitAll(n.left, fn) || visitAll(n.right, fn);
    }

    template <class Low, class High, class Fn>
    [[nodiscard]] bool visitIntersecting(NodeId id, const Low& low, const High& high,
                                         Fn& fn) const {
        if (id == invalidNode) {
            return false;
        }
        const QueryNode& n = node(id);
        if (!mayIntersect(n, low, high)) {
            return false;
        }
        if (allIntersect(n, low, high)) {
            return visitAll(id, fn);
        }
        if (intersects(n, low, high) &&
            detail::invokeIntervalTreeVisitor(fn, elements_[static_cast<std::size_t>(id)])) {
            return true;
        }
        return visitIntersecting(n.left, low, high, fn) ||
               visitIntersecting(n.right, low, high, fn);
    }

    template <class Low, class High, class Fn>
    [[nodiscard]] bool visitContainedIn(NodeId id, const Low& low, const High& high,
                                        Fn& fn) const {
        if (id == invalidNode) {
            return false;
        }
        const QueryNode& n = node(id);
        if (!mayContain(n, low, high)) {
            return false;
        }
        if (allContainedIn(n, low, high)) {
            return visitAll(id, fn);
        }
        if (containedIn(n, low, high) &&
            detail::invokeIntervalTreeVisitor(fn, elements_[static_cast<std::size_t>(id)])) {
            return true;
        }
        return visitContainedIn(n.left, low, high, fn) ||
               visitContainedIn(n.right, low, high, fn);
    }

    // The unqualified public query family uses the interval tree only as a
    // necessary-condition filter, then applies the same exact shape predicate
    // as ShapeTree. A subtree cannot be accepted wholesale here: matching
    // projections do not imply that the original shapes match.
    template <class Low, class High, class Q, class Fn>
    [[nodiscard]] bool visitShapeIntersecting(NodeId id, const Low& low, const High& high,
                                              const Q& q, Fn& fn) const {
        if (id == invalidNode) {
            return false;
        }
        const QueryNode& n = node(id);
        if (!mayIntersect(n, low, high)) {
            return false;
        }
        const ShapeType& shape = elements_[static_cast<std::size_t>(id)];
        if (shape.intersects(q) && detail::invokeIntervalTreeVisitor(fn, shape)) {
            return true;
        }
        return visitShapeIntersecting(n.left, low, high, q, fn) ||
               visitShapeIntersecting(n.right, low, high, q, fn);
    }

    template <class Low, class High, class Q, class Fn>
    [[nodiscard]] bool visitShapeContainedIn(NodeId id, const Low& low, const High& high,
                                             const Q& q, Fn& fn) const {
        if (id == invalidNode) {
            return false;
        }
        const QueryNode& n = node(id);
        if (!mayContain(n, low, high)) {
            return false;
        }
        const ShapeType& shape = elements_[static_cast<std::size_t>(id)];
        if (q.contains(shape) && detail::invokeIntervalTreeVisitor(fn, shape)) {
            return true;
        }
        return visitShapeContainedIn(n.left, low, high, q, fn) ||
               visitShapeContainedIn(n.right, low, high, q, fn);
    }

    template <class Low, class High>
    [[nodiscard]] std::size_t countIntersecting(NodeId id, const Low& low,
                                                const High& high) const {
        if (id == invalidNode) {
            return 0;
        }
        const QueryNode& n = node(id);
        if (!mayIntersect(n, low, high)) {
            return 0;
        }
        if (allIntersect(n, low, high)) {
            return n.count;
        }
        return (intersects(n, low, high) ? 1 : 0) + countIntersecting(n.left, low, high) +
               countIntersecting(n.right, low, high);
    }

    template <class Low, class High>
    [[nodiscard]] std::size_t countContainedIn(NodeId id, const Low& low,
                                               const High& high) const {
        if (id == invalidNode) {
            return 0;
        }
        const QueryNode& n = node(id);
        if (!mayContain(n, low, high)) {
            return 0;
        }
        if (allContainedIn(n, low, high)) {
            return n.count;
        }
        return (containedIn(n, low, high) ? 1 : 0) + countContainedIn(n.left, low, high) +
               countContainedIn(n.right, low, high);
    }

  public:
    IntervalTree() = default;

    /** @brief Builds a tree by inserting every shape in @p shapes. */
    template <class Container>
    explicit IntervalTree(const Container& shapes) {
        if constexpr (requires { shapes.size(); }) {
            const std::size_t count = static_cast<std::size_t>(shapes.size());
            if (count > static_cast<std::size_t>(invalidNode)) {
                throw std::length_error("IntervalTree exceeds its 32-bit node capacity");
            }
            elements_.reserve(count);
            nodes_.reserve(count);
            mutationNodes_.reserve(count);
        }
        for (const auto& shape : shapes) {
            insert(shape);
        }
    }

    /** @brief Returns the number of stored shapes. */
    [[nodiscard]] std::size_t size() const {
        return elements_.size();
    }

    /** @brief Returns whether no shape is stored. */
    [[nodiscard]] bool empty() const {
        return elements_.empty();
    }

    /** @brief Returns the stored shapes in internal storage order. */
    [[nodiscard]] const std::vector<ShapeType>& shapes() const {
        return elements_;
    }

    /** @brief Returns a constant iterator to the first stored shape. */
    [[nodiscard]] const_iterator begin() const { return elements_.begin(); }
    /** @brief Returns a constant iterator past the last stored shape. */
    [[nodiscard]] const_iterator end() const { return elements_.end(); }
    /** @brief Returns a constant iterator to the first stored shape. */
    [[nodiscard]] const_iterator cbegin() const { return elements_.cbegin(); }
    /** @brief Returns a constant iterator past the last stored shape. */
    [[nodiscard]] const_iterator cend() const { return elements_.cend(); }

    /** @brief Inserts @p shape and its selected closed bounding-box interval. */
    void insert(const ShapeType& shape) {
        const auto [low, high] = project(shape);
        if (nodes_.size() >= static_cast<std::size_t>(invalidNode)) {
            throw std::length_error("IntervalTree exceeds its 32-bit node capacity");
        }

        elements_.push_back(shape);
        try {
            insertExisting(low, high);
        } catch (...) {
            elements_.pop_back();
            throw;
        }
    }

    /**
     * @brief Removes one stored shape equal to @p shape.
     *
     * Storage remains compact, so after removal the index is rebuilt from the
     * surviving shapes. The rebuilt structure has the same red-black and
     * augmentation invariants as a freshly constructed tree.
     */
    bool erase(const ShapeType& shape) {
        if (root_ == invalidNode) {
            return false;
        }
        const auto found = std::find(elements_.begin(), elements_.end(), shape);
        if (found == elements_.end()) {
            return false;
        }

        const std::size_t removedIndex = static_cast<std::size_t>(found - elements_.begin());
        const std::size_t last = elements_.size() - 1;
        if (removedIndex != last) {
            elements_[removedIndex] = std::move(elements_[last]);
        }
        elements_.pop_back();
        rebuildFromElements();
        return true;
    }

    /** @brief Returns whether a shape equal to @p shape is stored. */
    [[nodiscard]] bool has(const ShapeType& shape) const {
        if (root_ == invalidNode) {
            return false;
        }
        const auto [low, high] = project(shape);
        return findEqualNode(shape, low, high) != invalidNode;
    }

    /** @brief Counts shapes whose projected interval intersects the projection of @p q. */
    template <class Q>
    [[nodiscard]] std::size_t countProjectionsIntersecting(const Q& q) const {
        if (root_ == invalidNode) {
            return 0;
        }
        const auto [low, high] = project(q);
        return countIntersecting(root_, low, high);
    }

    /** @brief Returns copies of shapes whose projected interval intersects that of @p q. */
    template <class Q>
    [[nodiscard]] std::vector<ShapeType> reportProjectionsIntersecting(const Q& q) const {
        std::vector<ShapeType> out;
        if (root_ != invalidNode) {
            const auto [low, high] = project(q);
            auto append = [&out](const ShapeType& shape) { out.push_back(shape); };
            (void)visitIntersecting(root_, low, high, append);
        }
        return out;
    }

    /** @brief Visits projected-interval intersections, stopping early if @p fn returns true. */
    template <class Q, class Fn>
    bool visitProjectionsIntersecting(const Q& q, Fn fn) const {
        if (root_ == invalidNode) {
            return false;
        }
        const auto [low, high] = project(q);
        return visitIntersecting(root_, low, high, fn);
    }

    /** @brief Returns whether no stored projected interval intersects the projection of @p q. */
    template <class Q>
    [[nodiscard]] bool emptyProjectionsIntersecting(const Q& q) const {
        return visitProjectionsIntersecting(q, [](const ShapeType&) { return true; }) == false;
    }

    /** @brief Counts shapes whose projected interval is contained in the projection of @p q. */
    template <class Q>
    [[nodiscard]] std::size_t countProjectionsContainedIn(const Q& q) const {
        if (root_ == invalidNode) {
            return 0;
        }
        const auto [low, high] = project(q);
        return countContainedIn(root_, low, high);
    }

    /** @brief Returns copies of shapes whose projected interval is contained in that of @p q. */
    template <class Q>
    [[nodiscard]] std::vector<ShapeType> reportProjectionsContainedIn(const Q& q) const {
        std::vector<ShapeType> out;
        if (root_ != invalidNode) {
            const auto [low, high] = project(q);
            auto append = [&out](const ShapeType& shape) { out.push_back(shape); };
            (void)visitContainedIn(root_, low, high, append);
        }
        return out;
    }

    /** @brief Visits projected intervals contained in @p q, stopping early if @p fn returns true. */
    template <class Q, class Fn>
    bool visitProjectionsContainedIn(const Q& q, Fn fn) const {
        if (root_ == invalidNode) {
            return false;
        }
        const auto [low, high] = project(q);
        return visitContainedIn(root_, low, high, fn);
    }

    /** @brief Returns whether no stored projected interval is contained in that of @p q. */
    template <class Q>
    [[nodiscard]] bool emptyProjectionsContainedIn(const Q& q) const {
        return visitProjectionsContainedIn(q, [](const ShapeType&) { return true; }) == false;
    }

    /**
     * @brief Counts stored shapes that geometrically intersect @p q.
     *
     * The selected projection prunes candidates, then each survivor is tested
     * with `shape.intersects(q)`, exactly as in @ref ShapeTree.
     */
    template <class Q>
    [[nodiscard]] std::size_t countIntersecting(const Q& q) const {
        std::size_t count = 0;
        (void)visitIntersecting(q, [&](const ShapeType&) { ++count; });
        return count;
    }

    /** @brief Returns copies of stored shapes that geometrically intersect @p q. */
    template <class Q>
    [[nodiscard]] std::vector<ShapeType> reportIntersecting(const Q& q) const {
        std::vector<ShapeType> out;
        (void)visitIntersecting(q, [&](const ShapeType& shape) { out.push_back(shape); });
        return out;
    }

    /**
     * @brief Visits stored shapes that geometrically intersect @p q.
     *
     * A bool-returning visitor stops when it returns true; a void visitor
     * examines every geometrically intersecting shape.
     */
    template <class Q, class Fn>
    bool visitIntersecting(const Q& q, Fn fn) const {
        if (root_ == invalidNode) {
            return false;
        }
        const auto [low, high] = project(q);
        return visitShapeIntersecting(root_, low, high, q, fn);
    }

    /** @brief Returns whether no stored shape geometrically intersects @p q. */
    template <class Q>
    [[nodiscard]] bool emptyIntersecting(const Q& q) const {
        return !visitIntersecting(q, [](const ShapeType&) { return true; });
    }

    /**
     * @brief Counts stored shapes geometrically contained in @p q.
     *
     * The selected projection prunes candidates, then each survivor is tested
     * with `q.contains(shape)`, exactly as in @ref ShapeTree.
     */
    template <class Q>
    [[nodiscard]] std::size_t countContainedIn(const Q& q) const {
        std::size_t count = 0;
        (void)visitContainedIn(q, [&](const ShapeType&) { ++count; });
        return count;
    }

    /** @brief Returns copies of stored shapes geometrically contained in @p q. */
    template <class Q>
    [[nodiscard]] std::vector<ShapeType> reportContainedIn(const Q& q) const {
        std::vector<ShapeType> out;
        (void)visitContainedIn(q, [&](const ShapeType& shape) { out.push_back(shape); });
        return out;
    }

    /** @brief Visits stored shapes geometrically contained in @p q. */
    template <class Q, class Fn>
    bool visitContainedIn(const Q& q, Fn fn) const {
        if (root_ == invalidNode) {
            return false;
        }
        const auto [low, high] = project(q);
        return visitShapeContainedIn(root_, low, high, q, fn);
    }

    /** @brief Returns whether no stored shape is geometrically contained in @p q. */
    template <class Q>
    [[nodiscard]] bool emptyContainedIn(const Q& q) const {
        return !visitContainedIn(q, [](const ShapeType&) { return true; });
    }
};

template <class Container>
IntervalTree(const Container&) -> IntervalTree<typename Container::value_type>;

}  // namespace pgl
