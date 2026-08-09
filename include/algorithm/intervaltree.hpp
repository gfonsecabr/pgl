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

/** Axis used to project a shape's bounding box into an interval. */
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
 * Nodes form a red-black tree ordered by `(low endpoint, high endpoint,
 * insertion serial)`. The serial keeps equal projected intervals distinct.
 * Every node caches the extrema of both endpoints in its subtree. In
 * particular, `maxHigh` is the standard augmented interval-tree value used to
 * prune subtrees lying completely before an intersection query.
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
    enum class Color : unsigned char { red, black };

    struct Node {
        NumberType low{};
        NumberType high{};
        NumberType minLow{};
        NumberType maxLow{};
        NumberType minHigh{};
        NumberType maxHigh{};
        std::size_t elementIndex = 0;
        std::uint64_t serial = 0;
        std::size_t count = 0;
        std::ptrdiff_t left = -1;
        std::ptrdiff_t right = -1;
        std::ptrdiff_t parent = -1;
        Color color = Color::black;
    };

    std::vector<ShapeType> elements_;
    std::vector<std::ptrdiff_t> elementNodes_;
    std::vector<Node> nodes_;
    std::ptrdiff_t root_ = -1;
    std::uint64_t nextSerial_ = 0;

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

    [[nodiscard]] const Node& node(std::ptrdiff_t id) const {
        return nodes_[static_cast<std::size_t>(id)];
    }

    [[nodiscard]] Node& node(std::ptrdiff_t id) {
        return nodes_[static_cast<std::size_t>(id)];
    }

    [[nodiscard]] static Color colorOf(const IntervalTree& tree, std::ptrdiff_t id) {
        return id == -1 ? Color::black : tree.node(id).color;
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
                               std::uint64_t serial, const Node& other) const {
        if (intervalLess(low, high, other.low, other.high)) {
            return true;
        }
        if (intervalLess(other.low, other.high, low, high)) {
            return false;
        }
        return serial < other.serial;
    }

    void update(std::ptrdiff_t id) {
        if (id == -1) {
            return;
        }
        Node& n = node(id);
        n.minLow = n.maxLow = n.low;
        n.minHigh = n.maxHigh = n.high;
        n.count = 1;
        for (const std::ptrdiff_t child : {n.left, n.right}) {
            if (child == -1) {
                continue;
            }
            const Node& c = node(child);
            n.minLow = minimum(n.minLow, c.minLow);
            n.maxLow = maximum(n.maxLow, c.maxLow);
            n.minHigh = minimum(n.minHigh, c.minHigh);
            n.maxHigh = maximum(n.maxHigh, c.maxHigh);
            n.count += c.count;
        }
    }

    void updateUpward(std::ptrdiff_t id) {
        while (id != -1) {
            update(id);
            id = node(id).parent;
        }
    }

    [[nodiscard]] std::ptrdiff_t allocateNode(std::size_t elementIndex, const NumberType& low,
                                              const NumberType& high) {
        Node fresh;
        fresh.low = fresh.minLow = fresh.maxLow = low;
        fresh.high = fresh.minHigh = fresh.maxHigh = high;
        fresh.elementIndex = elementIndex;
        fresh.serial = nextSerial_++;
        fresh.count = 1;
        fresh.color = Color::red;
        nodes_.push_back(std::move(fresh));
        return static_cast<std::ptrdiff_t>(nodes_.size() - 1);
    }

    void rotateLeft(std::ptrdiff_t x) {
        const std::ptrdiff_t y = node(x).right;
        node(x).right = node(y).left;
        if (node(y).left != -1) {
            node(node(y).left).parent = x;
        }
        node(y).parent = node(x).parent;
        if (node(x).parent == -1) {
            root_ = y;
        } else if (x == node(node(x).parent).left) {
            node(node(x).parent).left = y;
        } else {
            node(node(x).parent).right = y;
        }
        node(y).left = x;
        node(x).parent = y;
        update(x);
        update(y);
    }

    void rotateRight(std::ptrdiff_t x) {
        const std::ptrdiff_t y = node(x).left;
        node(x).left = node(y).right;
        if (node(y).right != -1) {
            node(node(y).right).parent = x;
        }
        node(y).parent = node(x).parent;
        if (node(x).parent == -1) {
            root_ = y;
        } else if (x == node(node(x).parent).right) {
            node(node(x).parent).right = y;
        } else {
            node(node(x).parent).left = y;
        }
        node(y).right = x;
        node(x).parent = y;
        update(x);
        update(y);
    }

    void insertFixup(std::ptrdiff_t z) {
        while (z != root_ && colorOf(*this, node(z).parent) == Color::red) {
            const std::ptrdiff_t parent = node(z).parent;
            const std::ptrdiff_t grandparent = node(parent).parent;
            if (parent == node(grandparent).left) {
                std::ptrdiff_t uncle = node(grandparent).right;
                if (colorOf(*this, uncle) == Color::red) {
                    node(parent).color = Color::black;
                    node(uncle).color = Color::black;
                    node(grandparent).color = Color::red;
                    z = grandparent;
                } else {
                    if (z == node(parent).right) {
                        z = parent;
                        rotateLeft(z);
                    }
                    node(node(z).parent).color = Color::black;
                    node(node(node(z).parent).parent).color = Color::red;
                    rotateRight(node(node(z).parent).parent);
                }
            } else {
                std::ptrdiff_t uncle = node(grandparent).left;
                if (colorOf(*this, uncle) == Color::red) {
                    node(parent).color = Color::black;
                    node(uncle).color = Color::black;
                    node(grandparent).color = Color::red;
                    z = grandparent;
                } else {
                    if (z == node(parent).left) {
                        z = parent;
                        rotateRight(z);
                    }
                    node(node(z).parent).color = Color::black;
                    node(node(node(z).parent).parent).color = Color::red;
                    rotateLeft(node(node(z).parent).parent);
                }
            }
        }
        node(root_).color = Color::black;
    }

    void insertExisting(std::size_t elementIndex, const NumberType& low, const NumberType& high) {
        const std::ptrdiff_t z = allocateNode(elementIndex, low, high);
        elementNodes_[elementIndex] = z;

        std::ptrdiff_t parent = -1;
        std::ptrdiff_t current = root_;
        while (current != -1) {
            parent = current;
            if (keyLess(low, high, node(z).serial, node(current))) {
                current = node(current).left;
            } else {
                current = node(current).right;
            }
        }
        node(z).parent = parent;
        if (parent == -1) {
            root_ = z;
        } else if (keyLess(low, high, node(z).serial, node(parent))) {
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
        elementNodes_.assign(elements_.size(), -1);
        root_ = -1;
        nextSerial_ = 0;
        for (std::size_t i = 0; i < elements_.size(); ++i) {
            const auto [low, high] = project(elements_[i]);
            insertExisting(i, low, high);
        }
    }

    [[nodiscard]] std::ptrdiff_t minimumNode(std::ptrdiff_t id) const {
        while (node(id).left != -1) {
            id = node(id).left;
        }
        return id;
    }

    [[nodiscard]] std::ptrdiff_t successor(std::ptrdiff_t id) const {
        if (node(id).right != -1) {
            return minimumNode(node(id).right);
        }
        std::ptrdiff_t parent = node(id).parent;
        while (parent != -1 && id == node(parent).right) {
            id = parent;
            parent = node(parent).parent;
        }
        return parent;
    }

    [[nodiscard]] std::ptrdiff_t lowerBoundInterval(const NumberType& low,
                                                    const NumberType& high) const {
        std::ptrdiff_t id = root_;
        std::ptrdiff_t result = -1;
        while (id != -1) {
            const Node& n = node(id);
            if (intervalLess(n.low, n.high, low, high)) {
                id = n.right;
            } else {
                result = id;
                id = n.left;
            }
        }
        return result;
    }

    [[nodiscard]] std::ptrdiff_t findEqualNode(const ShapeType& shape, const NumberType& low,
                                               const NumberType& high) const {
        for (std::ptrdiff_t id = lowerBoundInterval(low, high); id != -1; id = successor(id)) {
            const Node& n = node(id);
            if (!equivalent(n.low, low) || !equivalent(n.high, high)) {
                break;
            }
            if (elements_[n.elementIndex] == shape) {
                return id;
            }
        }
        return -1;
    }

    template <class Low, class High>
    [[nodiscard]] static bool intersects(const Node& n, const Low& low, const High& high) {
        return !(n.high < low) && !(high < n.low);
    }

    template <class Low, class High>
    [[nodiscard]] static bool mayIntersect(const Node& n, const Low& low, const High& high) {
        return !(n.maxHigh < low) && !(high < n.minLow);
    }

    template <class Low, class High>
    [[nodiscard]] static bool allIntersect(const Node& n, const Low& low, const High& high) {
        return !(high < n.maxLow) && !(n.minHigh < low);
    }

    template <class Low, class High>
    [[nodiscard]] static bool containedIn(const Node& n, const Low& low, const High& high) {
        return !(n.low < low) && !(high < n.low) && !(high < n.high);
    }

    template <class Low, class High>
    [[nodiscard]] static bool mayContain(const Node& n, const Low& low, const High& high) {
        return !(n.maxLow < low) && !(high < n.minLow) && !(high < n.minHigh);
    }

    template <class Low, class High>
    [[nodiscard]] static bool allContainedIn(const Node& n, const Low& low, const High& high) {
        return !(n.minLow < low) && !(high < n.maxLow) && !(high < n.maxHigh);
    }

    template <class Fn>
    [[nodiscard]] bool visitAll(std::ptrdiff_t id, Fn& fn) const {
        if (id == -1) {
            return false;
        }
        const Node& n = node(id);
        if (detail::invokeIntervalTreeVisitor(fn, elements_[n.elementIndex])) {
            return true;
        }
        return visitAll(n.left, fn) || visitAll(n.right, fn);
    }

    template <class Low, class High, class Fn>
    [[nodiscard]] bool visitIntersecting(std::ptrdiff_t id, const Low& low, const High& high,
                                         Fn& fn) const {
        if (id == -1) {
            return false;
        }
        const Node& n = node(id);
        if (!mayIntersect(n, low, high)) {
            return false;
        }
        if (allIntersect(n, low, high)) {
            return visitAll(id, fn);
        }
        if (intersects(n, low, high) && detail::invokeIntervalTreeVisitor(fn, elements_[n.elementIndex])) {
            return true;
        }
        return visitIntersecting(n.left, low, high, fn) ||
               visitIntersecting(n.right, low, high, fn);
    }

    template <class Low, class High, class Fn>
    [[nodiscard]] bool visitContainedIn(std::ptrdiff_t id, const Low& low, const High& high,
                                        Fn& fn) const {
        if (id == -1) {
            return false;
        }
        const Node& n = node(id);
        if (!mayContain(n, low, high)) {
            return false;
        }
        if (allContainedIn(n, low, high)) {
            return visitAll(id, fn);
        }
        if (containedIn(n, low, high) && detail::invokeIntervalTreeVisitor(fn, elements_[n.elementIndex])) {
            return true;
        }
        return visitContainedIn(n.left, low, high, fn) ||
               visitContainedIn(n.right, low, high, fn);
    }

    template <class Low, class High>
    [[nodiscard]] std::size_t countIntersecting(std::ptrdiff_t id, const Low& low,
                                                const High& high) const {
        if (id == -1) {
            return 0;
        }
        const Node& n = node(id);
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
    [[nodiscard]] std::size_t countContainedIn(std::ptrdiff_t id, const Low& low,
                                               const High& high) const {
        if (id == -1) {
            return 0;
        }
        const Node& n = node(id);
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

    /** Builds a tree by inserting every shape in @p shapes. */
    template <class Container>
    explicit IntervalTree(const Container& shapes) {
        for (const auto& shape : shapes) {
            insert(shape);
        }
    }

    [[nodiscard]] std::size_t size() const {
        return elements_.size();
    }

    [[nodiscard]] bool empty() const {
        return elements_.empty();
    }

    /** Returns the stored shapes in internal storage order. */
    [[nodiscard]] const std::vector<ShapeType>& shapes() const {
        return elements_;
    }

    [[nodiscard]] const_iterator begin() const { return elements_.begin(); }
    [[nodiscard]] const_iterator end() const { return elements_.end(); }
    [[nodiscard]] const_iterator cbegin() const { return elements_.cbegin(); }
    [[nodiscard]] const_iterator cend() const { return elements_.cend(); }

    /** Inserts @p shape and its selected closed bounding-box interval. */
    void insert(const ShapeType& shape) {
        const auto [low, high] = project(shape);
        if (nextSerial_ == std::numeric_limits<std::uint64_t>::max()) {
            throw std::overflow_error("IntervalTree insertion serial exhausted");
        }

        const std::size_t elementIndex = elements_.size();
        elements_.push_back(shape);
        elementNodes_.push_back(-1);
        insertExisting(elementIndex, low, high);
    }

    /**
     * @brief Removes one stored shape equal to @p shape.
     *
     * Storage remains compact, so after removal the index is rebuilt from the
     * surviving shapes. The rebuilt structure has the same red-black and
     * augmentation invariants as a freshly constructed tree.
     */
    bool erase(const ShapeType& shape) {
        if (root_ == -1) {
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

    /** Returns whether a shape equal to @p shape is stored. */
    [[nodiscard]] bool has(const ShapeType& shape) const {
        if (root_ == -1) {
            return false;
        }
        const auto [low, high] = project(shape);
        return findEqualNode(shape, low, high) != -1;
    }

    /** Counts shapes whose projected interval intersects the projection of @p q. */
    template <class Q>
    [[nodiscard]] std::size_t countIntersecting(const Q& q) const {
        if (root_ == -1) {
            return 0;
        }
        const auto [low, high] = project(q);
        return countIntersecting(root_, low, high);
    }

    /** Returns copies of shapes whose projected interval intersects that of @p q. */
    template <class Q>
    [[nodiscard]] std::vector<ShapeType> reportIntersecting(const Q& q) const {
        std::vector<ShapeType> out;
        if (root_ != -1) {
            const auto [low, high] = project(q);
            auto append = [&out](const ShapeType& shape) { out.push_back(shape); };
            (void)visitIntersecting(root_, low, high, append);
        }
        return out;
    }

    /** Visits projected-interval intersections, stopping early if @p fn returns true. */
    template <class Q, class Fn>
    bool visitIntersecting(const Q& q, Fn fn) const {
        if (root_ == -1) {
            return false;
        }
        const auto [low, high] = project(q);
        return visitIntersecting(root_, low, high, fn);
    }

    /** Returns whether no stored projected interval intersects the projection of @p q. */
    template <class Q>
    [[nodiscard]] bool emptyIntersecting(const Q& q) const {
        return visitIntersecting(q, [](const ShapeType&) { return true; }) == false;
    }

    /** Counts shapes whose projected interval is contained in the projection of @p q. */
    template <class Q>
    [[nodiscard]] std::size_t countContainedIn(const Q& q) const {
        if (root_ == -1) {
            return 0;
        }
        const auto [low, high] = project(q);
        return countContainedIn(root_, low, high);
    }

    /** Returns copies of shapes whose projected interval is contained in that of @p q. */
    template <class Q>
    [[nodiscard]] std::vector<ShapeType> reportContainedIn(const Q& q) const {
        std::vector<ShapeType> out;
        if (root_ != -1) {
            const auto [low, high] = project(q);
            auto append = [&out](const ShapeType& shape) { out.push_back(shape); };
            (void)visitContainedIn(root_, low, high, append);
        }
        return out;
    }

    /** Visits projected intervals contained in @p q, stopping early if @p fn returns true. */
    template <class Q, class Fn>
    bool visitContainedIn(const Q& q, Fn fn) const {
        if (root_ == -1) {
            return false;
        }
        const auto [low, high] = project(q);
        return visitContainedIn(root_, low, high, fn);
    }

    /** Returns whether no stored projected interval is contained in that of @p q. */
    template <class Q>
    [[nodiscard]] bool emptyContainedIn(const Q& q) const {
        return visitContainedIn(q, [](const ShapeType&) { return true; }) == false;
    }
};

template <class Container>
IntervalTree(const Container&) -> IntervalTree<typename Container::value_type>;

}  // namespace pgl
