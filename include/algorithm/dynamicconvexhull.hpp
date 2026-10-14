#pragma once

#include "algorithm/convexhull.hpp"

/**
 * @file dynamicconvexhull.hpp
 * @brief A set of points that keeps its convex hull under insertion and erasure.
 *
 * The structure is the one of Overmars and van Leeuwen ("Maintenance of
 * configurations in the plane", 1981) with bridges only. The points are the
 * leaves of a balanced binary tree, in lexicographic order. Every internal node
 * stores, for the upper and for the lower hull of the points below it, only its
 * bridge: the edge joining a vertex of the left child's hull to one of the
 * right child's. That node's hull is the left child's up to the bridge, the
 * bridge, and the right child's after it, so a node's bridge is found from its
 * children's by descending both at once, and an update recomputes the bridges
 * on one root path.
 *
 * An update stops recomputing bridges on its way up as soon as the point it
 * adds or removes is not a vertex of the hull below the current node: no hull
 * above it can change either. Whether the point is such a vertex follows from
 * the child's answer and the node's bridge. Rotated nodes are recomputed
 * regardless.
 *
 * Every node also counts how many vertices of each of its hulls come from each
 * child, which makes the hull indexable. The counts are accumulated along the
 * same descent that finds the bridge.
 *
 * Ordering points lexicographically stands in for ordering them by abscissa
 * after an infinitesimal shear, which is what lets vertical edges and equal
 * abscissae go through the same code as everything else.
 */

#include <algorithm>
#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <ranges>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>


namespace pgl {

namespace detail {

/**
 * @brief Number type the crossing placement below is evaluated in.
 *
 * Its terms are of degree three, so a machine-integer coordinate is promoted
 * twice, as in the circumcentre of a disk; a rational coordinate promotes once,
 * its storage growing as needed.
 */
template <class Number>
using crossing_coordinate_t = std::conditional_t<
    extended_integral<Number>,
    promoted_number_t<promoted_number_t<Number>>,
    promoted_number_t<Number>>;

/**
 * @brief Whether the lines `ab` and `cd`, which must cross, cross at a point
 *        lexicographically at most `m`.
 *
 * The crossing is `a + (b - a) * k / det`, with `det = (b - a) x (d - c)` and
 * `k = (c - a) x (d - c)`, so its abscissa is past `m`'s exactly when
 * `(a.x - m.x) * det + k * (b.x - a.x)` has the sign of `det`; the same over
 * the ordinates breaks a tie. Nothing is divided.
 */
template <class PointType>
constexpr bool crossingNotPast(const PointType& a, const PointType& b,
                               const PointType& c, const PointType& d, const PointType& m) {
    using W = crossing_coordinate_t<typename PointType::NumberType>;
    const W ax = asNumber<W>(a.x()), ay = asNumber<W>(a.y());
    const W ux = asNumber<W>(b.x()) - ax, uy = asNumber<W>(b.y()) - ay;
    const W vx = asNumber<W>(d.x()) - asNumber<W>(c.x());
    const W vy = asNumber<W>(d.y()) - asNumber<W>(c.y());
    const W wx = asNumber<W>(c.x()) - ax, wy = asNumber<W>(c.y()) - ay;
    const W det = ux * vy - uy * vx;
    const W k = wx * vy - wy * vx;
    const bool negative = det < W{};
    const W dx = (ax - asNumber<W>(m.x())) * det + k * ux;
    if (dx != W{}) {
        return (dx < W{}) != negative;
    }
    const W dy = (ay - asNumber<W>(m.y())) * det + k * uy;
    return dy == W{} || ((dy < W{}) != negative);
}

}  // namespace detail

/**
 * @brief A set of points that keeps its convex hull under insertion and
 *        erasure.
 *
 * The stored points form a set: inserting a point equal to a stored one, label
 * aside, changes nothing. The hull's vertices are numbered as those of a
 * @ref Convex, counterclockwise from the lexicographically smallest, and the
 * hull is strictly convex: a point in the interior of a hull edge is not a
 * vertex. All points collinear give a hull of their two extreme points.
 *
 * Updates take `O(log^2 n)` time and most queries `O(log n)`, for `n` stored
 * points. A hull of `h` vertices is listed in `O(h log n)`. A structure holds
 * at most `2^31 - 1` points.
 *
 * @tparam PointType_ Point type of the stored points.
 */
template <class PointType_ = Point<int>>
class DynamicConvexHull {
    static_assert(detail::is_point_v<PointType_>, "DynamicConvexHull requires pgl::Point elements");

  public:
    using PointType = PointType_;
    using NumberType = typename PointType::NumberType;

    // Container-like aliases, so the stored points can be iterated and passed
    // where a container of points is expected.
    using value_type = PointType;
    using size_type = std::size_t;
    using const_reference = const PointType&;
    class const_iterator;

  private:
    using Index = std::uint32_t;
    static constexpr Index nil = std::numeric_limits<Index>::max();

    // One hull of an internal node: its bridge, and how many of its vertices
    // come from each child (the bridge's endpoints included).
    struct Chain {
        PointType a, b;
        Index left = 0, right = 0;
    };

    struct Node {
        // A leaf's point. An internal node's router: at least every point below
        // its left child and less than every point below its right one.
        PointType point;
        Chain chain[2];        // internal nodes: [0] upper hull, [1] lower hull
        Index child[2] = {nil, nil};  // a leaf: previous and next leaf
        std::int32_t height = 0;      // 0 at a leaf
    };

    std::vector<Node> nodes_;
    std::vector<Index> free_;
    Index root_ = nil;
    Index first_ = nil, last_ = nil;  // the lexicographic extremes' leaves
    std::size_t size_ = 0;

  public:
    /**
     * @brief Forward iterator over the stored points, in lexicographic order.
     */
    class const_iterator {
      public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = PointType;
        using difference_type = std::ptrdiff_t;
        using pointer = const PointType*;
        using reference = const PointType&;

        const_iterator() = default;
        reference operator*() const { return (*nodes_)[at_].point; }
        pointer operator->() const { return &(*nodes_)[at_].point; }
        const_iterator& operator++() {
            at_ = (*nodes_)[at_].child[1];
            return *this;
        }
        const_iterator operator++(int) {
            const_iterator before = *this;
            ++*this;
            return before;
        }
        bool operator==(const const_iterator&) const = default;

      private:
        friend class DynamicConvexHull;
        const_iterator(const std::vector<Node>* nodes, Index at) : nodes_(nodes), at_(at) {}
        const std::vector<Node>* nodes_ = nullptr;
        Index at_ = nil;
    };

    /** @brief Creates an empty set. */
    DynamicConvexHull() = default;

    /**
     * @brief Builds the set of the points in a container.
     *
     * Complexity: `O(n log n)` for `n` points.
     *
     * @tparam Container Range whose value type is convertible to @ref PointType.
     * @param points Points to store; repeated points are stored once.
     */
    template <class Container>
        requires std::ranges::input_range<const Container&> &&
                 std::convertible_to<std::ranges::range_value_t<const Container&>, PointType>
    explicit DynamicConvexHull(const Container& points) {
        std::vector<PointType> sorted;
        for (const auto& p : points) {
            sorted.push_back(static_cast<PointType>(p));
        }
        build(std::move(sorted));
    }

    /**
     * @brief Builds the set of the listed points.
     *
     * Complexity: `O(n log n)` for `n` points.
     *
     * @param points Points to store; repeated points are stored once.
     */
    DynamicConvexHull(std::initializer_list<PointType> points) {
        build(std::vector<PointType>(points));
    }

    // ---- the stored points --------------------------------------------------

    /** @brief Returns the number of stored points. */
    [[nodiscard]] std::size_t size() const { return size_; }

    /** @brief Returns whether no point is stored. */
    [[nodiscard]] bool empty() const { return size_ == 0; }

    /** @brief Returns an iterator to the lexicographically smallest stored point. */
    [[nodiscard]] const_iterator begin() const { return {&nodes_, first_}; }
    /** @brief Returns an iterator past the lexicographically largest stored point. */
    [[nodiscard]] const_iterator end() const { return {&nodes_, nil}; }
    /** @brief Returns an iterator to the lexicographically smallest stored point. */
    [[nodiscard]] const_iterator cbegin() const { return begin(); }
    /** @brief Returns an iterator past the lexicographically largest stored point. */
    [[nodiscard]] const_iterator cend() const { return end(); }

    /**
     * @brief Returns whether a point equal to `point` is stored.
     *
     * Complexity: `O(log n)`.
     */
    template <PointConcept OtherPoint>
    [[nodiscard]] bool has(const OtherPoint& point) const {
        if (root_ == nil) {
            return false;
        }
        Index v = root_;
        while (!isLeaf(v)) {
            v = select(v, nodes_[v].point < point);
        }
        return nodes_[v].point == point;
    }

    /**
     * @brief Stores a point.
     *
     * Complexity: `O(log^2 n)`.
     *
     * @return Whether the point was added: false if an equal point was stored.
     */
    bool insert(const PointType& point) {
        if (root_ == nil) {
            root_ = makeLeaf(point);
            first_ = last_ = root_;
            size_ = 1;
            return true;
        }
        PGL_ASSERT(size_ < (std::size_t(1) << 31) - 1);
        bool added = false, onUpper = false, onLower = false;
        root_ = insertBelow(root_, point, added, onUpper, onLower);
        size_ += added;
        return added;
    }

    /**
     * @brief Removes the stored point equal to `point`.
     *
     * Complexity: `O(log^2 n)`.
     *
     * @return Whether a point was removed: false if none was equal to `point`.
     */
    template <PointConcept OtherPoint>
    bool erase(const OtherPoint& point) {
        if (root_ == nil) {
            return false;
        }
        bool removed = false, onUpper = false, onLower = false;
        root_ = eraseBelow(root_, point, removed, onUpper, onLower);
        size_ -= removed;
        if (size_ == 0) {
            clear();
        }
        return removed;
    }

    /** @brief Removes every point. */
    void clear() {
        nodes_.clear();
        free_.clear();
        root_ = first_ = last_ = nil;
        size_ = 0;
    }

    // ---- the hull -----------------------------------------------------------

    /**
     * @brief Returns the number of hull vertices.
     *
     * Complexity: `O(1)`.
     */
    [[nodiscard]] std::size_t hullSize() const {
        if (size_ <= 1) {
            return size_;
        }
        return total(root_, 0) + total(root_, 1) - 2;
    }

    /**
     * @brief Returns a hull vertex by index, counterclockwise from the
     *        lexicographically smallest.
     *
     * Cyclic, as @ref Convex::get: `index` is taken modulo @ref hullSize(), and
     * negative indices wrap from the end. The set must not be empty.
     *
     * Complexity: `O(log n)`.
     */
    [[nodiscard]] PointType vertex(std::ptrdiff_t index) const {
        PGL_ASSERT(!empty());
        const std::ptrdiff_t h = static_cast<std::ptrdiff_t>(hullSize());
        const std::size_t i = static_cast<std::size_t>(((index % h) + h) % h);
        const std::size_t lower = total(root_, 1);
        if (i < lower) {
            return chainVertex(1, i);
        }
        return chainVertex(0, total(root_, 0) - 2 - (i - lower));
    }

    /**
     * @brief Returns the index of `point` among the hull vertices, as numbered
     *        by @ref vertex, or `-1` if it is not a hull vertex.
     *
     * Complexity: `O(log n)`.
     */
    template <PointConcept OtherPoint>
    [[nodiscard]] std::ptrdiff_t index(const OtherPoint& point) const {
        if (root_ == nil) {
            return -1;
        }
        if (const std::ptrdiff_t lower = chainRank(1, point); lower >= 0) {
            return lower;
        }
        const std::ptrdiff_t upper = chainRank(0, point);
        if (upper < 0) {
            return -1;
        }
        return static_cast<std::ptrdiff_t>(total(root_, 1) + total(root_, 0) - 2) - upper;
    }

    /**
     * @brief Returns whether `point` is a hull vertex.
     *
     * Complexity: `O(log n)`.
     */
    template <PointConcept OtherPoint>
    [[nodiscard]] bool verticesContain(const OtherPoint& point) const {
        return index(point) >= 0;
    }

    /**
     * @brief Returns the hull vertex maximizing the dot product with
     *        `direction`, the lexicographically smallest one among ties.
     *
     * The set must not be empty, and `direction` must not be zero.
     *
     * Complexity: `O(log n)`.
     */
    [[nodiscard]] PointType extremeVertex(const PointType& direction) const {
        PGL_ASSERT(!empty());
        const PointType origin(NumberType(0), NumberType(0));
        PGL_ASSERT(!(direction == origin));
        // The chain along which the dot product rises, then falls. A
        // horizontal direction is the tie: the upper chain may start with a
        // vertical edge, along which (1, 0) does not gain, and the lower chain
        // may end with one, along which (-1, 0) does not.
        const int k = (direction.y() > NumberType(0) ||
                       (direction.y() == NumberType(0) && direction.x() < NumberType(0))) ? 0 : 1;
        Index v = root_;
        while (!isLeaf(v)) {
            const Chain& c = nodes_[v].chain[k];
            v = select(v, dotSign(c.a, c.b, origin, direction) > 0);
        }
        return nodes_[v].point;
    }

    /**
     * @brief Returns the bounding box of the stored points.
     *
     * The set must not be empty.
     *
     * Complexity: `O(log n)`.
     */
    [[nodiscard]] Rectangle<PointType> bbox() const {
        PGL_ASSERT(!empty());
        const NumberType zero(0), one(1);
        const NumberType low = extremeVertex(PointType(zero, -one)).y();
        const NumberType high = extremeVertex(PointType(zero, one)).y();
        return Rectangle<PointType>(PointType(nodes_[first_].point.x(), low),
                                    PointType(nodes_[last_].point.x(), high));
    }

    /**
     * @brief Returns the hull vertices, counterclockwise from the
     *        lexicographically smallest.
     *
     * Complexity: `O(h log n)` for `h` hull vertices.
     */
    [[nodiscard]] std::vector<PointType> vertices() const {
        std::vector<PointType> out, upper;
        if (root_ == nil) {
            return out;
        }
        collect(1, root_, nullptr, nullptr, out);
        collect(0, root_, nullptr, nullptr, upper);
        for (std::size_t i = upper.size() - 1; i-- > 1;) {
            out.push_back(upper[i]);
        }
        return out;
    }

    /**
     * @brief Returns the lower hull, from the lexicographically smallest point
     *        to the largest; see @ref Convex::lowerHull.
     *
     * Complexity: `O(h log n)`.
     */
    [[nodiscard]] MonotoneChain<PointType> lowerHull() const {
        std::vector<PointType> out;
        if (root_ != nil) {
            collect(1, root_, nullptr, nullptr, out);
        }
        return MonotoneChain<PointType>(out, pgl::trusted);
    }

    /**
     * @brief Returns the upper hull, from the lexicographically smallest point
     *        to the largest; see @ref Convex::upperHull.
     *
     * Complexity: `O(h log n)`.
     */
    [[nodiscard]] MonotoneChain<PointType> upperHull() const {
        std::vector<PointType> out;
        if (root_ != nil) {
            collect(0, root_, nullptr, nullptr, out);
        }
        return MonotoneChain<PointType>(out, pgl::trusted);
    }

    /**
     * @brief Returns the hull as a @ref Convex.
     *
     * Complexity: `O(h log n)`.
     */
    [[nodiscard]] Convex<PointType> convexHull() const {
        return Convex<PointType>(vertices(), pgl::trusted);
    }

    // ---- predicates ---------------------------------------------------------
    //
    // Every predicate answers as the same predicate on convexHull(). A hull of
    // at most two vertices is a point or a segment, built in O(log n), so the
    // overloads whose geometry needs a proper polygon hand such a hull over to
    // convexHull(). The overloads documented as answering through convexHull()
    // build it whatever its size.

    /**
     * @brief Returns whether the closed hull contains `point`.
     *
     * Complexity: `O(log n)`.
     */
    template <PointConcept OtherPoint>
    [[nodiscard]] bool contains(const OtherPoint& point) const {
        const auto [upper, lower] = sides(point);
        return upper <= 0 && lower <= 0;
    }

    /**
     * @brief Returns whether the closed hull contains `other`.
     *
     * Complexity: `O(log n)`.
     */
    template <class OtherSegment>
        requires SegmentConcept<OtherSegment> || OrientedSegmentConcept<OtherSegment>
    [[nodiscard]] bool contains(const OtherSegment& other) const {
        return contains(other[0]) && contains(other[1]);
    }

    /** @brief Returns false: the hull is bounded and `other` is not. */
    template <class OtherShape>
        requires LineConcept<OtherShape> || OrientedLineConcept<OtherShape> ||
                 RayConcept<OtherShape> || HalfplaneConcept<OtherShape>
    [[nodiscard]] bool contains(const OtherShape&) const {
        return false;
    }

    /**
     * @brief Returns whether the closed hull contains `other`.
     *
     * Complexity: `O(log n)`.
     */
    template <RectangleConcept OtherRectangle>
    [[nodiscard]] bool contains(const OtherRectangle& other) const {
        return other.empty() || containsAll(std::array{other[0], other[1], other[2], other[3]});
    }

    /**
     * @brief Returns whether the closed hull contains `other`.
     *
     * Complexity: `O(log n)`.
     */
    template <TriangleConcept OtherTriangle>
    [[nodiscard]] bool contains(const OtherTriangle& other) const {
        return containsAll(std::array{other[0], other[1], other[2]});
    }

    /**
     * @brief Returns whether the closed hull contains `other`.
     *
     * Complexity: `O(k log n)` for `k` vertices of `other`.
     */
    template <class OtherShape>
        requires ConvexConcept<OtherShape> || PolygonConcept<OtherShape> ||
                 PolylineConcept<OtherShape> || MonotoneChainConcept<OtherShape>
    [[nodiscard]] bool contains(const OtherShape& other) const {
        return containsAll(other);
    }

    /**
     * @brief Returns whether the closed hull contains `other`.
     *
     * Complexity: `O(k log n)` for `k` vertices on the outer boundary of `other`.
     */
    template <PolygonWithHolesConcept OtherRegion>
    [[nodiscard]] bool contains(const OtherRegion& other) const {
        return contains(other.outer());
    }

    /**
     * @brief Returns whether the closed hull contains `other`.
     *
     * Complexity: `O(k log n)` for `k` vertices on the outer boundaries of
     * `other`'s components.
     */
    template <PolygonSetConcept OtherSet>
    [[nodiscard]] bool contains(const OtherSet& other) const {
        for (const auto& component : other) {
            if (!contains(component)) {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief Returns whether the closed hull contains `other`; answered through
     *        @ref convexHull.
     *
     * Complexity: `O(h log n)` plus that of the same predicate on @ref convexHull.
     */
    template <class OtherShape>
        requires DiskConcept<OtherShape> || HalfplaneIntersectionConcept<OtherShape>
    [[nodiscard]] bool contains(const OtherShape& other) const {
        return convexHull().contains(other);
    }

    /** @brief Returns true: every set contains the empty set. */
    template <class EmptyPoint>
    [[nodiscard]] bool contains(const EmptyShape<EmptyPoint>&) const {
        return true;
    }

    /** @brief Answers for the shape `other` holds, as the overload for that shape. */
    template <ShapeConcept OtherShape>
    [[nodiscard]] bool contains(const OtherShape& other) const {
        return std::visit([this](const auto& shape) { return this->contains(shape); }, other.variant());
    }

    /**
     * @brief Returns whether the hull's interior contains `point`.
     *
     * Complexity: `O(log n)`.
     */
    template <PointConcept OtherPoint>
    [[nodiscard]] bool interiorContains(const OtherPoint& point) const {
        if (hullSize() <= 2) {
            return !empty() && convexHull().interiorContains(point);
        }
        const auto [upper, lower] = sides(point);
        return upper < 0 && lower < 0;
    }

    /**
     * @brief Returns whether the hull's interior contains `other`.
     *
     * Complexity: `O(log n)`.
     */
    template <class OtherSegment>
        requires SegmentConcept<OtherSegment> || OrientedSegmentConcept<OtherSegment>
    [[nodiscard]] bool interiorContains(const OtherSegment& other) const {
        return interiorContains(other[0]) && interiorContains(other[1]);
    }

    /** @brief Returns false: the hull is bounded and `other` is not. */
    template <class OtherShape>
        requires LineConcept<OtherShape> || OrientedLineConcept<OtherShape> ||
                 RayConcept<OtherShape> || HalfplaneConcept<OtherShape>
    [[nodiscard]] bool interiorContains(const OtherShape&) const {
        return false;
    }

    /**
     * @brief Returns whether the hull's interior contains `other`.
     *
     * Complexity: `O(log n)`.
     */
    template <RectangleConcept OtherRectangle>
    [[nodiscard]] bool interiorContains(const OtherRectangle& other) const {
        return other.empty() ||
               interiorContainsAll(std::array{other[0], other[1], other[2], other[3]});
    }

    /**
     * @brief Returns whether the hull's interior contains `other`.
     *
     * Complexity: `O(log n)`.
     */
    template <TriangleConcept OtherTriangle>
    [[nodiscard]] bool interiorContains(const OtherTriangle& other) const {
        return interiorContainsAll(std::array{other[0], other[1], other[2]});
    }

    /**
     * @brief Returns whether the hull's interior contains `other`.
     *
     * Complexity: `O(k log n)` for `k` vertices of `other`.
     */
    template <class OtherShape>
        requires ConvexConcept<OtherShape> || PolygonConcept<OtherShape> ||
                 PolylineConcept<OtherShape> || MonotoneChainConcept<OtherShape>
    [[nodiscard]] bool interiorContains(const OtherShape& other) const {
        if (hullSize() <= 2) {
            return convexHull().interiorContains(other);
        }
        return interiorContainsAll(other);
    }

    /**
     * @brief Returns whether the hull's interior contains `other`.
     *
     * Complexity: `O(k log n)` for `k` vertices on the outer boundary of `other`.
     */
    template <PolygonWithHolesConcept OtherRegion>
    [[nodiscard]] bool interiorContains(const OtherRegion& other) const {
        return interiorContains(other.outer());
    }

    /**
     * @brief Returns whether the hull's interior contains `other`.
     *
     * Complexity: `O(k log n)` for `k` vertices on the outer boundaries of
     * `other`'s components.
     */
    template <PolygonSetConcept OtherSet>
    [[nodiscard]] bool interiorContains(const OtherSet& other) const {
        for (const auto& component : other) {
            if (!interiorContains(component)) {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief Returns whether the hull's interior contains `other`; answered
     *        through @ref convexHull.
     *
     * Complexity: `O(h log n)` plus that of the same predicate on @ref convexHull.
     */
    template <class OtherShape>
        requires DiskConcept<OtherShape> || HalfplaneIntersectionConcept<OtherShape>
    [[nodiscard]] bool interiorContains(const OtherShape& other) const {
        return convexHull().interiorContains(other);
    }

    /** @brief Returns true: every set contains the empty set. */
    template <class EmptyPoint>
    [[nodiscard]] bool interiorContains(const EmptyShape<EmptyPoint>&) const {
        return true;
    }

    /** @brief Answers for the shape `other` holds, as the overload for that shape. */
    template <ShapeConcept OtherShape>
    [[nodiscard]] bool interiorContains(const OtherShape& other) const {
        return std::visit([this](const auto& shape) { return this->interiorContains(shape); }, other.variant());
    }

    /**
     * @brief Returns whether the hull's boundary contains `point`.
     *
     * Complexity: `O(log n)`.
     */
    template <PointConcept OtherPoint>
    [[nodiscard]] bool boundaryContains(const OtherPoint& point) const {
        if (hullSize() <= 2) {
            return !empty() && convexHull().boundaryContains(point);
        }
        const auto [upper, lower] = sides(point);
        return upper <= 0 && lower <= 0 && (upper == 0 || lower == 0);
    }

    /**
     * @brief Returns whether the hull's boundary contains `other`.
     *
     * Complexity: `O(log n)`.
     */
    template <class OtherSegment>
        requires SegmentConcept<OtherSegment> || OrientedSegmentConcept<OtherSegment>
    [[nodiscard]] bool boundaryContains(const OtherSegment& other) const {
        if (other[0] == other[1]) {
            return boundaryContains(other[0]);
        }
        if (hullSize() <= 2) {
            return convexHull().boundaryContains(other);
        }
        // A segment in the hull lies on its boundary exactly when its line
        // leaves the whole hull on one side.
        return contains(other[0]) && contains(other[1]) &&
               (orientationSign(other[0], other[1], farthestLeftOf(other[0], other[1])) <= 0 ||
                orientationSign(other[1], other[0], farthestLeftOf(other[1], other[0])) <= 0);
    }

    /**
     * @brief Returns whether the hull's boundary contains `other`: only a
     *        degenerate line, which is a point, may lie on it.
     *
     * Complexity: `O(log n)`.
     */
    template <LineConcept OtherLine>
    [[nodiscard]] bool boundaryContains(const OtherLine& other) const {
        return other.isDegenerate() && boundaryContains(other.min());
    }

    /**
     * @brief Returns whether the hull's boundary contains `other`: only a
     *        degenerate shape, which is a point, may lie on it.
     *
     * Complexity: `O(log n)`.
     */
    template <class OtherShape>
        requires OrientedLineConcept<OtherShape> || RayConcept<OtherShape> ||
                 HalfplaneConcept<OtherShape>
    [[nodiscard]] bool boundaryContains(const OtherShape& other) const {
        return other.isDegenerate() && boundaryContains(other.source());
    }

    /**
     * @brief Returns whether the hull's boundary contains `other`.
     *
     * Complexity: `O(log n)`.
     */
    template <RectangleConcept OtherRectangle>
    [[nodiscard]] bool boundaryContains(const OtherRectangle& other) const {
        if (other.empty()) {
            return true;
        }
        if (!other.isDegenerate()) {
            return false;
        }
        if (other.min() == other.max()) {
            return boundaryContains(other.min());
        }
        return boundaryContains(Segment<typename OtherRectangle::PointType>(other.min(), other.max()));
    }

    /**
     * @brief Returns whether the hull's boundary contains `other`.
     *
     * Complexity: `O(log n)`.
     */
    template <TriangleConcept OtherTriangle>
    [[nodiscard]] bool boundaryContains(const OtherTriangle& other) const {
        if (!other.isDegenerate()) {
            return false;
        }
        if (other.a() == other.c()) {
            return boundaryContains(other.a());
        }
        return boundaryContains(Segment<typename OtherTriangle::PointType>(other.a(), other.c()));
    }

    /**
     * @brief Returns whether the hull's boundary contains `other`: only one of
     *        at most two vertices may lie on it.
     *
     * Complexity: `O(log n)`.
     */
    template <class OtherShape>
        requires ConvexConcept<OtherShape> || PolygonConcept<OtherShape>
    [[nodiscard]] bool boundaryContains(const OtherShape& other) const {
        if (other.size() == 0) {
            return true;
        }
        if (other.size() == 1) {
            return boundaryContains(other[0]);
        }
        if (other.size() == 2) {
            return boundaryContains(Segment<typename OtherShape::PointType>(other[0], other[1]));
        }
        return false;
    }

    /**
     * @brief Returns whether the hull's boundary contains `other`.
     *
     * Complexity: `O(k log n)` for `k` vertices of `other`.
     */
    template <class OtherChain>
        requires PolylineConcept<OtherChain> || MonotoneChainConcept<OtherChain>
    [[nodiscard]] bool boundaryContains(const OtherChain& other) const {
        if (other.empty()) {
            return true;
        }
        if (other.size() == 1) {
            return boundaryContains(other[0]);
        }
        for (std::size_t i = 0; i + 1 < other.size(); ++i) {
            if (!boundaryContains(Segment<typename OtherChain::PointType>(other[i], other[i + 1]))) {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief Returns whether the hull's boundary contains `other`: only a
     *        zero-radius disk, which is a point, may lie on it.
     *
     * Complexity: `O(log n)`.
     */
    template <DiskConcept OtherDisk>
    [[nodiscard]] bool boundaryContains(const OtherDisk& other) const {
        if (other[0] == other[1] && other[0] == other[2]) {
            return boundaryContains(other[0]);
        }
        return false;
    }

    /**
     * @brief Returns whether the hull's boundary contains `other`; a degenerate
     *        region is answered through @ref convexHull.
     *
     * Complexity: `O(1)` unless `other` is degenerate and not empty, and
     * otherwise `O(h log n)` plus that of the same predicate on
     * @ref convexHull.
     */
    template <HalfplaneIntersectionConcept OtherRegion>
    [[nodiscard]] bool boundaryContains(const OtherRegion& other) const {
        if (other.empty()) {
            return true;
        }
        if (!other.isDegenerate()) {
            return false;
        }
        return convexHull().boundaryContains(other);
    }

    /**
     * @brief Returns whether the hull's boundary contains `other`; answered
     *        through @ref convexHull.
     *
     * Complexity: `O(h log n)` plus that of the same predicate on @ref convexHull.
     */
    template <class OtherShape>
        requires PolygonWithHolesConcept<OtherShape> || PolygonSetConcept<OtherShape>
    [[nodiscard]] bool boundaryContains(const OtherShape& other) const {
        return convexHull().boundaryContains(other);
    }

    /** @brief Returns true: every set contains the empty set. */
    template <class EmptyPoint>
    [[nodiscard]] bool boundaryContains(const EmptyShape<EmptyPoint>&) const {
        return true;
    }

    /** @brief Answers for the shape `other` holds, as the overload for that shape. */
    template <ShapeConcept OtherShape>
    [[nodiscard]] bool boundaryContains(const OtherShape& other) const {
        return std::visit([this](const auto& shape) { return this->boundaryContains(shape); }, other.variant());
    }

    /**
     * @brief Returns whether `point` meets the closed hull.
     *
     * Complexity: `O(log n)`.
     */
    template <PointConcept OtherPoint>
    [[nodiscard]] bool intersects(const OtherPoint& point) const {
        return contains(point);
    }

    /**
     * @brief Returns whether `other` meets the closed hull.
     *
     * Complexity: `O(log n)`.
     */
    template <class OtherSegment>
        requires SegmentConcept<OtherSegment> || OrientedSegmentConcept<OtherSegment>
    [[nodiscard]] bool intersects(const OtherSegment& other) const {
        if (hullSize() <= 2) {
            return convexHull().intersects(other);
        }
        if (contains(other[0]) || contains(other[1])) {
            return true;
        }
        return chordAcross(other[0], other[1]).intersects(other);
    }

    /**
     * @brief Returns whether `other` meets the closed hull.
     *
     * Complexity: `O(log n)`.
     */
    template <class OtherLine>
        requires LineConcept<OtherLine> || OrientedLineConcept<OtherLine>
    [[nodiscard]] bool intersects(const OtherLine& other) const {
        if (hullSize() <= 2) {
            return convexHull().intersects(other);
        }
        return orientationSign(other[0], other[1], farthestLeftOf(other[0], other[1])) >= 0 &&
               orientationSign(other[1], other[0], farthestLeftOf(other[1], other[0])) >= 0;
    }

    /**
     * @brief Returns whether `other` meets the closed hull.
     *
     * Complexity: `O(log n)`.
     */
    template <RayConcept OtherRay>
    [[nodiscard]] bool intersects(const OtherRay& other) const {
        if (hullSize() <= 2) {
            return convexHull().intersects(other);
        }
        if (contains(other.source())) {
            return true;
        }
        return chordAcross(other[0], other[1]).intersects(other);
    }

    /**
     * @brief Returns whether `other` meets the closed hull.
     *
     * Complexity: `O(log n)`.
     */
    template <HalfplaneConcept OtherHalfplane>
    [[nodiscard]] bool intersects(const OtherHalfplane& other) const {
        if (empty()) {
            return false;
        }
        if (other.isDegenerate()) {
            return contains(other.source());
        }
        return other.contains(farthestLeftOf(other.source(), other.target()));
    }

    /**
     * @brief Returns whether `other` meets the closed hull.
     *
     * Complexity: `O(log n)`.
     */
    template <RectangleConcept OtherRectangle>
    [[nodiscard]] bool intersects(const OtherRectangle& other) const {
        return !other.empty() && intersectsConvexShape(other, other.edges());
    }

    /**
     * @brief Returns whether `other` meets the closed hull.
     *
     * Complexity: `O(log n)`.
     */
    template <TriangleConcept OtherTriangle>
    [[nodiscard]] bool intersects(const OtherTriangle& other) const {
        return intersectsConvexShape(other, other.edges());
    }

    /**
     * @brief Returns whether `other` meets the closed hull.
     *
     * Complexity: `O(k log n)` for `k` vertices of `other`.
     */
    template <ConvexConcept OtherConvex>
    [[nodiscard]] bool intersects(const OtherConvex& other) const {
        return !other.empty() && intersectsConvexShape(other, other.edgesView());
    }

    /**
     * @brief Returns whether `other` meets the closed hull; answered through
     *        @ref convexHull.
     *
     * Complexity: `O(h log n)` plus that of the same predicate on @ref convexHull.
     */
    template <class OtherShape>
        requires DiskConcept<OtherShape> || PolygonConcept<OtherShape> ||
                 PolylineConcept<OtherShape> || MonotoneChainConcept<OtherShape> ||
                 HalfplaneIntersectionConcept<OtherShape> ||
                 PolygonWithHolesConcept<OtherShape> || PolygonSetConcept<OtherShape>
    [[nodiscard]] bool intersects(const OtherShape& other) const {
        return convexHull().intersects(other);
    }

    /** @brief Returns false: nothing meets the empty set. */
    template <class EmptyPoint>
    [[nodiscard]] bool intersects(const EmptyShape<EmptyPoint>&) const {
        return false;
    }

    /** @brief Answers for the shape `other` holds, as the overload for that shape. */
    template <ShapeConcept OtherShape>
    [[nodiscard]] bool intersects(const OtherShape& other) const {
        return std::visit([this](const auto& shape) { return this->intersects(shape); }, other.variant());
    }

    /**
     * @brief Returns whether `point` meets the hull's interior.
     *
     * Complexity: `O(log n)`.
     */
    template <PointConcept OtherPoint>
    [[nodiscard]] bool interiorsIntersect(const OtherPoint& point) const {
        return interiorContains(point);
    }

    /**
     * @brief Returns whether the interiors of the hull and `other` meet.
     *
     * Complexity: `O(log n)`.
     */
    template <class OtherSegment>
        requires SegmentConcept<OtherSegment> || OrientedSegmentConcept<OtherSegment>
    [[nodiscard]] bool interiorsIntersect(const OtherSegment& other) const {
        if (hullSize() <= 2) {
            return convexHull().interiorsIntersect(other);
        }
        if (other[0] == other[1]) {
            return false;
        }
        if (interiorContains(other[0]) || interiorContains(other[1])) {
            return true;
        }
        return triangleAcross(other[0], other[1]).interiorsIntersect(other);
    }

    /**
     * @brief Returns whether the interiors of the hull and `other` meet.
     *
     * Complexity: `O(log n)`.
     */
    template <class OtherLine>
        requires LineConcept<OtherLine> || OrientedLineConcept<OtherLine>
    [[nodiscard]] bool interiorsIntersect(const OtherLine& other) const {
        if (hullSize() <= 2) {
            return convexHull().interiorsIntersect(other);
        }
        if (other.isDegenerate()) {
            return interiorContains(other.min());
        }
        // Hull vertices strictly on both sides.
        return orientationSign(other[0], other[1], farthestLeftOf(other[0], other[1])) > 0 &&
               orientationSign(other[1], other[0], farthestLeftOf(other[1], other[0])) > 0;
    }

    /**
     * @brief Returns whether the interiors of the hull and `other` meet.
     *
     * Complexity: `O(log n)`.
     */
    template <RayConcept OtherRay>
    [[nodiscard]] bool interiorsIntersect(const OtherRay& other) const {
        if (hullSize() <= 2) {
            return convexHull().interiorsIntersect(other);
        }
        if (other.isDegenerate()) {
            return false;
        }
        if (interiorContains(other.source())) {
            return true;
        }
        return triangleAcross(other[0], other[1]).interiorsIntersect(other);
    }

    /**
     * @brief Returns whether the interiors of the hull and `other` meet.
     *
     * Complexity: `O(log n)`.
     */
    template <HalfplaneConcept OtherHalfplane>
    [[nodiscard]] bool interiorsIntersect(const OtherHalfplane& other) const {
        if (hullSize() <= 2) {
            return convexHull().interiorsIntersect(other);
        }
        if (other.isDegenerate()) {
            return false;
        }
        return other.interiorContains(farthestLeftOf(other.source(), other.target()));
    }

    /**
     * @brief Returns whether the interiors of the hull and `other` meet.
     *
     * Complexity: `O(log n)`.
     */
    template <RectangleConcept OtherRectangle>
    [[nodiscard]] bool interiorsIntersect(const OtherRectangle& other) const {
        return !other.empty() && interiorsIntersectConvexShape(other, other.edges());
    }

    /**
     * @brief Returns whether the interiors of the hull and `other` meet.
     *
     * Complexity: `O(log n)`.
     */
    template <TriangleConcept OtherTriangle>
    [[nodiscard]] bool interiorsIntersect(const OtherTriangle& other) const {
        return interiorsIntersectConvexShape(other, other.edges());
    }

    /**
     * @brief Returns whether the interiors of the hull and `other` meet.
     *
     * Complexity: `O(k log n)` for `k` vertices of `other`.
     */
    template <ConvexConcept OtherConvex>
    [[nodiscard]] bool interiorsIntersect(const OtherConvex& other) const {
        return interiorsIntersectConvexShape(other, other.edgesView());
    }

    /**
     * @brief Returns whether the interiors of the hull and `other` meet;
     *        answered through @ref convexHull.
     *
     * Complexity: `O(h log n)` plus that of the same predicate on @ref convexHull.
     */
    template <class OtherShape>
        requires DiskConcept<OtherShape> || PolygonConcept<OtherShape> ||
                 PolylineConcept<OtherShape> || MonotoneChainConcept<OtherShape> ||
                 HalfplaneIntersectionConcept<OtherShape> ||
                 PolygonWithHolesConcept<OtherShape> || PolygonSetConcept<OtherShape>
    [[nodiscard]] bool interiorsIntersect(const OtherShape& other) const {
        return convexHull().interiorsIntersect(other);
    }

    /** @brief Returns false: the empty set has no interior. */
    template <class EmptyPoint>
    [[nodiscard]] bool interiorsIntersect(const EmptyShape<EmptyPoint>&) const {
        return false;
    }

    /** @brief Answers for the shape `other` holds, as the overload for that shape. */
    template <ShapeConcept OtherShape>
    [[nodiscard]] bool interiorsIntersect(const OtherShape& other) const {
        return std::visit([this](const auto& shape) { return this->interiorsIntersect(shape); }, other.variant());
    }

    /** @brief Returns false: removing the hull never disconnects a point. */
    template <PointConcept OtherPoint>
    [[nodiscard]] bool separates(const OtherPoint&) const {
        return false;
    }

    /**
     * @brief Returns whether removing the hull disconnects `other`.
     *
     * Complexity: `O(log n)`.
     */
    template <class OtherSegment>
        requires SegmentConcept<OtherSegment> || OrientedSegmentConcept<OtherSegment>
    [[nodiscard]] bool separates(const OtherSegment& other) const {
        return !(other[0] == other[1]) && !contains(other[0]) && !contains(other[1]) && intersects(other);
    }

    /**
     * @brief Returns whether removing the hull disconnects `other`.
     *
     * Complexity: `O(log n)`.
     */
    template <class OtherLine>
        requires LineConcept<OtherLine> || OrientedLineConcept<OtherLine>
    [[nodiscard]] bool separates(const OtherLine& other) const {
        return !other.isDegenerate() && intersects(other);
    }

    /**
     * @brief Returns whether removing the hull disconnects `other`.
     *
     * Complexity: `O(log n)`.
     */
    template <RayConcept OtherRay>
    [[nodiscard]] bool separates(const OtherRay& other) const {
        if (hullSize() <= 2) {
            return convexHull().separates(other);
        }
        return !other.isDegenerate() && !contains(other.source()) && intersects(other);
    }

    /** @brief Returns false: a half-plane minus a bounded set stays connected. */
    template <HalfplaneConcept OtherHalfplane>
    [[nodiscard]] bool separates(const OtherHalfplane&) const {
        return false;
    }

    /**
     * @brief Returns whether removing the hull disconnects `other`.
     *
     * Complexity: `O(log n)`.
     */
    template <RectangleConcept OtherRectangle>
    [[nodiscard]] bool separates(const OtherRectangle& other) const {
        return !other.empty() && separatesConvex(other.asConvex());
    }

    /**
     * @brief Returns whether removing the hull disconnects `other`.
     *
     * Complexity: `O(log n)`.
     */
    template <TriangleConcept OtherTriangle>
    [[nodiscard]] bool separates(const OtherTriangle& other) const {
        return separatesConvex(other.asConvex());
    }

    /**
     * @brief Returns whether removing the hull disconnects `other`.
     *
     * Complexity: `O(k log n)` for `k` vertices of `other`.
     */
    template <ConvexConcept OtherConvex>
    [[nodiscard]] bool separates(const OtherConvex& other) const {
        return separatesConvex(other);
    }

    /**
     * @brief Returns whether removing the hull disconnects `other`; answered
     *        through @ref convexHull.
     *
     * Complexity: `O(h log n)` plus that of the same predicate on @ref convexHull.
     */
    template <class OtherShape>
        requires DiskConcept<OtherShape> || PolygonConcept<OtherShape> ||
                 PolylineConcept<OtherShape> || MonotoneChainConcept<OtherShape> ||
                 HalfplaneIntersectionConcept<OtherShape> ||
                 PolygonWithHolesConcept<OtherShape> || PolygonSetConcept<OtherShape>
    [[nodiscard]] bool separates(const OtherShape& other) const {
        return convexHull().separates(other);
    }

    /** @brief Returns false: the empty set has no pieces. */
    template <class EmptyPoint>
    [[nodiscard]] bool separates(const EmptyShape<EmptyPoint>&) const {
        return false;
    }

    /** @brief Answers for the shape `other` holds, as the overload for that shape. */
    template <ShapeConcept OtherShape>
    [[nodiscard]] bool separates(const OtherShape& other) const {
        return std::visit([this](const auto& shape) { return this->separates(shape); }, other.variant());
    }

    /** @brief Returns false: the hull and a point never disconnect each other. */
    template <PointConcept OtherPoint>
    [[nodiscard]] bool crosses(const OtherPoint&) const {
        return false;
    }

    /**
     * @brief Returns whether the hull and `other` disconnect each other.
     *
     * Complexity: `O(log n)`.
     */
    template <class OtherSegment>
        requires SegmentConcept<OtherSegment> || OrientedSegmentConcept<OtherSegment>
    [[nodiscard]] bool crosses(const OtherSegment& other) const {
        if (hullSize() <= 2) {
            return convexHull().crosses(other);
        }
        return separates(other) && segmentSeparates(other);
    }

    /**
     * @brief Returns whether the hull and `other` disconnect each other.
     *
     * Complexity: `O(log n)`.
     */
    template <class OtherLine>
        requires LineConcept<OtherLine> || OrientedLineConcept<OtherLine>
    [[nodiscard]] bool crosses(const OtherLine& other) const {
        if (hullSize() <= 2) {
            return convexHull().crosses(other);
        }
        return separates(other) && interiorsIntersect(other);
    }

    /**
     * @brief Returns whether the hull and `other` disconnect each other.
     *
     * Complexity: `O(log n)`.
     */
    template <RayConcept OtherRay>
    [[nodiscard]] bool crosses(const OtherRay& other) const {
        if (hullSize() <= 2) {
            return convexHull().crosses(other);
        }
        return separates(other) && !interiorContains(other.source()) && interiorsIntersect(other);
    }

    /** @brief Returns false: a half-plane minus a bounded set stays connected. */
    template <HalfplaneConcept OtherHalfplane>
    [[nodiscard]] bool crosses(const OtherHalfplane&) const {
        return false;
    }

    /**
     * @brief Returns whether the hull and `other` disconnect each other.
     *
     * Complexity: `O(log n)`.
     */
    template <RectangleConcept OtherRectangle>
    [[nodiscard]] bool crosses(const OtherRectangle& other) const {
        return !other.empty() && crossesConvex(other.asConvex());
    }

    /**
     * @brief Returns whether the hull and `other` disconnect each other.
     *
     * Complexity: `O(log n)`.
     */
    template <TriangleConcept OtherTriangle>
    [[nodiscard]] bool crosses(const OtherTriangle& other) const {
        return crossesConvex(other.asConvex());
    }

    /**
     * @brief Returns whether the hull and `other` disconnect each other.
     *
     * Complexity: `O(k log n)` for `k` vertices of `other`.
     */
    template <ConvexConcept OtherConvex>
    [[nodiscard]] bool crosses(const OtherConvex& other) const {
        return crossesConvex(other);
    }

    /**
     * @brief Returns whether the hull and `other` disconnect each other;
     *        answered through @ref convexHull.
     *
     * Complexity: `O(h log n)` plus that of the same predicate on @ref convexHull.
     */
    template <class OtherShape>
        requires DiskConcept<OtherShape> || PolygonConcept<OtherShape> ||
                 PolylineConcept<OtherShape> || MonotoneChainConcept<OtherShape> ||
                 HalfplaneIntersectionConcept<OtherShape> ||
                 PolygonWithHolesConcept<OtherShape> || PolygonSetConcept<OtherShape>
    [[nodiscard]] bool crosses(const OtherShape& other) const {
        return convexHull().crosses(other);
    }

    /** @brief Returns false: the empty set disconnects nothing. */
    template <class EmptyPoint>
    [[nodiscard]] bool crosses(const EmptyShape<EmptyPoint>&) const {
        return false;
    }

    /** @brief Answers for the shape `other` holds, as the overload for that shape. */
    template <ShapeConcept OtherShape>
    [[nodiscard]] bool crosses(const OtherShape& other) const {
        return std::visit([this](const auto& shape) { return this->crosses(shape); }, other.variant());
    }

    /** @brief Draws the hull and the stored points. */
    friend Canvas& operator<<(Canvas& canvas, const DynamicConvexHull& hull) {
        if (!hull.empty()) {
            canvas << hull.convexHull();
        }
        for (const PointType& p : hull) {
            canvas << p;
        }
        return canvas;
    }

  private:
    // ---- tree plumbing ------------------------------------------------------

    bool isLeaf(Index v) const { return nodes_[v].height == 0; }

    // The child on `right`'s side. Both children are read before the choice is
    // made, so that the load of the next node does not wait for the comparison
    // that picks it: indexing the child array by the comparison's result
    // instead made descents about twice as slow.
    Index select(Index v, bool right) const {
        const Index l = nodes_[v].child[0], r = nodes_[v].child[1];
        return right ? r : l;
    }
    std::size_t total(Index v, int k) const {
        return isLeaf(v) ? 1 : std::size_t(nodes_[v].chain[k].left) + nodes_[v].chain[k].right;
    }

    Index make() {
        if (!free_.empty()) {
            const Index v = free_.back();
            free_.pop_back();
            nodes_[v] = Node{};
            return v;
        }
        nodes_.emplace_back();
        return static_cast<Index>(nodes_.size() - 1);
    }
    Index makeLeaf(const PointType& point) {
        const Index v = make();
        nodes_[v].point = point;
        return v;
    }

    // S * orientation: negative is a convex turn of the chain, zero collinear,
    // positive bends outward. S = +1 for the upper chain (index 0), -1 for the
    // lower one (index 1).
    template <int S>
    static int turn(const PointType& a, const PointType& b, const PointType& c) {
        return S * detail::signOf(orientationSign(a, b, c));
    }

    // The bridge between the hulls of x (left) and y (right), separated by m,
    // with the number of vertices each side contributes.
    //
    // Each node on the way holds an edge of its own hull, the hull of the
    // points below it, which contains the bridge's endpoint on its side.
    // With "beyond" meaning the outer side of the chain:
    //  - a point of y's edge on or beyond the line of x's edge settles that the
    //    bridge leaves x's hull at or before that edge's first endpoint;
    //  - a point of x's edge on or beyond the line of y's edge, that it reaches
    //    y's hull at or after that edge's last endpoint;
    //  - otherwise each line passes strictly beyond the other edge, they cross
    //    between the two, and the side of m they cross on says which of the
    //    two edges lies beyond the bridge's endpoint.
    // An endpoint is the outermost point of its side on the supporting line,
    // so the hulls are strictly convex.
    template <int S>
    Chain findBridge(Index x, Index y, const PointType& m) const {
        constexpr int k = S > 0 ? 0 : 1;
        const Index y0 = y;
        // A vertex's rank in the starting hull is its rank in the current
        // node's hull plus these offsets.
        std::int64_t rx = 0, ry = 0;
        const auto down = [&](Index& v, std::int64_t& offset, bool right) {
            const Node& n = nodes_[v];
            if (right) {
                offset += std::int64_t(n.chain[k].left) + n.chain[k].right -
                          std::int64_t(total(n.child[1], k));
            }
            v = select(v, right);
        };
        for (;;) {
            const bool xLeaf = isLeaf(x), yLeaf = isLeaf(y);
            if (xLeaf && yLeaf) {
                break;
            }
            if (xLeaf) {
                const Chain& e = nodes_[y].chain[k];
                down(y, ry, turn<S>(e.a, e.b, nodes_[x].point) >= 0);
                continue;
            }
            if (yLeaf) {
                const Chain& e = nodes_[x].chain[k];
                down(x, rx, turn<S>(e.a, e.b, nodes_[y].point) < 0);
                continue;
            }
            const Chain& ex = nodes_[x].chain[k];
            const Chain& ey = nodes_[y].chain[k];
            const bool xLeft = turn<S>(ex.a, ex.b, ey.a) >= 0 || turn<S>(ex.a, ex.b, ey.b) >= 0;
            const bool yRight = turn<S>(ey.a, ey.b, ex.a) >= 0 || turn<S>(ey.a, ey.b, ex.b) >= 0;
            if (xLeft || yRight) {
                if (xLeft) {
                    down(x, rx, false);
                }
                if (yRight) {
                    down(y, ry, true);
                }
                continue;
            }
            if (detail::crossingNotPast(ex.a, ex.b, ey.a, ey.b, m)) {
                down(x, rx, true);
            } else {
                down(y, ry, false);
            }
        }
        Chain c;
        c.a = nodes_[x].point;
        c.b = nodes_[y].point;
        c.left = static_cast<Index>(rx + 1);
        c.right = static_cast<Index>(std::int64_t(total(y0, k)) - ry);
        return c;
    }

    void pull(Index v, bool upper, bool lower) {
        const Index l = nodes_[v].child[0], r = nodes_[v].child[1];
        nodes_[v].height = 1 + std::max(nodes_[l].height, nodes_[r].height);
        if (upper) {
            nodes_[v].chain[0] = findBridge<+1>(l, r, nodes_[v].point);
        }
        if (lower) {
            nodes_[v].chain[1] = findBridge<-1>(l, r, nodes_[v].point);
        }
    }

    // A rotation keeps every router valid: a router separates the same two
    // ranges of points before and after.
    Index rotate(Index v, int side) {
        const Index u = nodes_[v].child[1 - side];
        nodes_[v].child[1 - side] = nodes_[u].child[side];
        nodes_[u].child[side] = v;
        pull(v, true, true);
        pull(u, true, true);
        return u;
    }
    Index rebalance(Index v) {
        const auto h = [&](Index u) { return nodes_[u].height; };
        const Index l = nodes_[v].child[0], r = nodes_[v].child[1];
        if (h(l) > h(r) + 1) {
            if (h(nodes_[l].child[0]) < h(nodes_[l].child[1])) {
                nodes_[v].child[0] = rotate(l, 0);
            }
            return rotate(v, 1);
        }
        if (h(r) > h(l) + 1) {
            if (h(nodes_[r].child[1]) < h(nodes_[r].child[0])) {
                nodes_[v].child[1] = rotate(r, 1);
            }
            return rotate(v, 0);
        }
        return v;
    }

    // Whether point, below child `side` of v, is a vertex of v's chain k given
    // that it is one of the child's.
    bool keepsVertex(Index v, int k, int side, const PointType& point) const {
        const Chain& c = nodes_[v].chain[k];
        return side == 0 ? !(c.a < point) : !(point < c.b);
    }

    // onUpper, onLower: the point is a vertex of the returned subtree's upper,
    // lower hull. Only then can the hulls above change.
    Index insertBelow(Index v, const PointType& point, bool& added, bool& onUpper, bool& onLower) {
        if (isLeaf(v)) {
            if (nodes_[v].point == point) {
                added = false;
                return v;
            }
            added = true;
            const bool before = point < nodes_[v].point;
            const Index q = makeLeaf(point);
            const Index in = make();
            // Thread the new leaf into the leaf list.
            const Index prev = before ? nodes_[v].child[0] : v;
            const Index next = before ? v : nodes_[v].child[1];
            nodes_[q].child[0] = prev;
            nodes_[q].child[1] = next;
            if (prev != nil) nodes_[prev].child[1] = q; else first_ = q;
            if (next != nil) nodes_[next].child[0] = q; else last_ = q;
            nodes_[in].child[0] = before ? q : v;
            nodes_[in].child[1] = before ? v : q;
            nodes_[in].point = nodes_[nodes_[in].child[0]].point;
            pull(in, true, true);
            onUpper = keepsVertex(in, 0, before ? 0 : 1, point);
            onLower = keepsVertex(in, 1, before ? 0 : 1, point);
            return in;
        }
        const int side = nodes_[v].point < point ? 1 : 0;
        const Index c = insertBelow(nodes_[v].child[side], point, added, onUpper, onLower);
        nodes_[v].child[side] = c;
        if (!added) {
            return v;
        }
        pull(v, onUpper, onLower);
        onUpper = onUpper && keepsVertex(v, 0, side, point);
        onLower = onLower && keepsVertex(v, 1, side, point);
        return rebalance(v);
    }

    // onUpper, onLower: the point was a vertex of the subtree's hulls before the
    // erasure, read off the old bridges. Only then can the hulls above change.
    template <class OtherPoint>
    Index eraseBelow(Index v, const OtherPoint& point, bool& removed, bool& onUpper, bool& onLower) {
        if (isLeaf(v)) {
            if (!(nodes_[v].point == point)) {
                removed = false;
                return v;
            }
            removed = true;
            onUpper = onLower = true;
            const Index prev = nodes_[v].child[0], next = nodes_[v].child[1];
            if (prev != nil) nodes_[prev].child[1] = next; else first_ = next;
            if (next != nil) nodes_[next].child[0] = prev; else last_ = prev;
            free_.push_back(v);
            return nil;
        }
        const int side = nodes_[v].point < point ? 1 : 0;
        const Index c = eraseBelow(nodes_[v].child[side], point, removed, onUpper, onLower);
        if (!removed) {
            return v;
        }
        onUpper = onUpper && keepsVertex(v, 0, side, point);
        onLower = onLower && keepsVertex(v, 1, side, point);
        if (c == nil) {
            const Index sibling = nodes_[v].child[1 - side];
            free_.push_back(v);
            return sibling;
        }
        nodes_[v].child[side] = c;
        pull(v, onUpper, onLower);
        return rebalance(v);
    }

    // The subtree over sorted[lo, hi), balanced; its leaves are threaded in order.
    Index buildRange(const std::vector<PointType>& sorted, std::size_t lo, std::size_t hi) {
        if (hi - lo == 1) {
            const Index v = makeLeaf(sorted[lo]);
            nodes_[v].child[0] = last_;
            if (last_ != nil) nodes_[last_].child[1] = v; else first_ = v;
            last_ = v;
            return v;
        }
        const std::size_t mid = lo + (hi - lo) / 2;
        const Index l = buildRange(sorted, lo, mid);
        const Index r = buildRange(sorted, mid, hi);
        const Index v = make();
        nodes_[v].child[0] = l;
        nodes_[v].child[1] = r;
        nodes_[v].point = sorted[mid - 1];
        pull(v, true, true);
        return v;
    }

    void build(std::vector<PointType> sorted) {
        std::sort(sorted.begin(), sorted.end());
        sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());
        PGL_ASSERT(sorted.size() < (std::size_t(1) << 31));
        clear();
        if (sorted.empty()) {
            return;
        }
        nodes_.reserve(2 * sorted.size() - 1);
        root_ = buildRange(sorted, 0, sorted.size());
        size_ = sorted.size();
    }

    // ---- queries ------------------------------------------------------------

    PointType chainVertex(int k, std::size_t i) const {
        Index v = root_;
        while (!isLeaf(v)) {
            const Node& n = nodes_[v];
            if (i < n.chain[k].left) {
                v = n.child[0];
            } else {
                i = total(n.child[1], k) - n.chain[k].right + (i - n.chain[k].left);
                v = n.child[1];
            }
        }
        return nodes_[v].point;
    }

    // The rank of point in chain k, from the lexicographically smallest, or -1.
    template <class OtherPoint>
    std::ptrdiff_t chainRank(int k, const OtherPoint& point) const {
        Index v = root_;
        std::int64_t rank = 0;
        while (!isLeaf(v)) {
            const Node& n = nodes_[v];
            const Chain& c = n.chain[k];
            if (!(c.a < point)) {
                v = n.child[0];
            } else if (!(point < c.b)) {
                rank += std::int64_t(c.left) + c.right - std::int64_t(total(n.child[1], k));
                v = n.child[1];
            } else {
                return -1;
            }
        }
        return nodes_[v].point == point ? static_cast<std::ptrdiff_t>(rank) : -1;
    }

    // Where point lies against chain S over its abscissa: positive beyond it,
    // zero on it, negative on the hull's side.
    template <int S, class OtherPoint>
    int side(const OtherPoint& point) const {
        constexpr int k = S > 0 ? 0 : 1;
        Index v = root_;
        while (!isLeaf(v)) {
            const Chain& c = nodes_[v].chain[k];
            if (point < c.a) {
                v = nodes_[v].child[0];
            } else if (c.b < point) {
                v = nodes_[v].child[1];
            } else {
                return S * detail::signOf(orientationSign(c.a, c.b, point));
            }
        }
        return nodes_[v].point == point ? 0 : 1;
    }

    // The sides of point against the upper and lower chains; outside the
    // lexicographic range of the points, beyond both.
    template <class OtherPoint>
    std::pair<int, int> sides(const OtherPoint& point) const {
        if (root_ == nil || point < nodes_[first_].point || nodes_[last_].point < point) {
            return {1, 1};
        }
        return {side<+1>(point), side<-1>(point)};
    }

    // The hull vertex maximizing orientation(p, q, .), for a nonempty set: the
    // one extreme along the left normal of p -> q, found as extremeVertex finds
    // it, without forming the normal. The left normal points up, or left when
    // vertical, exactly when p precedes q.
    template <class OtherPoint>
    PointType farthestLeftOf(const OtherPoint& p, const OtherPoint& q) const {
        const int k = p < q ? 0 : 1;
        Index v = root_;
        while (!isLeaf(v)) {
            const Chain& c = nodes_[v].chain[k];
            v = select(v, crossSign(p, q, c.a, c.b) > 0);
        }
        return nodes_[v].point;
    }

    // A chord of the hull joining its farthest vertices on either side of the
    // line pq. A segment or ray along that line with no endpoint in the hull
    // meets the hull exactly when it meets this chord.
    template <class OtherPoint>
    Segment<PointType> chordAcross(const OtherPoint& p, const OtherPoint& q) const {
        return Segment<PointType>(farthestLeftOf(p, q), farthestLeftOf(q, p));
    }

    // For a hull of at least three vertices, a triangle on its vertices with
    // one strictly on each side of the line pq when any hull vertex is. A
    // segment or ray along that line with no endpoint in the hull's interior
    // meets that interior exactly when it meets the triangle's: it then spans
    // the whole of the line's open chord through the hull.
    template <class OtherPoint>
    Triangle<PointType> triangleAcross(const OtherPoint& p, const OtherPoint& q) const {
        const PointType a = farthestLeftOf(p, q);
        const PointType b = farthestLeftOf(q, p);
        const std::ptrdiff_t i = index(a);
        PointType c = vertex(i - 1);
        if (c == b) {
            c = vertex(i + 1);
        }
        return Triangle<PointType>(a, b, c);
    }

    template <class Vertices>
    bool containsAll(const Vertices& vertices) const {
        for (const auto& vertex : vertices) {
            if (!contains(vertex)) {
                return false;
            }
        }
        return true;
    }

    template <class Vertices>
    bool interiorContainsAll(const Vertices& vertices) const {
        for (const auto& vertex : vertices) {
            if (!interiorContains(vertex)) {
                return false;
            }
        }
        return true;
    }

    // A nonempty bounded convex shape meets the hull when either holds a point
    // of the other or one of its edges meets the hull.
    template <class OtherShape, class Edges>
    bool intersectsConvexShape(const OtherShape& other, Edges edges) const {
        if (empty()) {
            return false;
        }
        if (contains(other[0]) || other.contains(vertex(0))) {
            return true;
        }
        for (const auto& edge : edges) {
            if (intersects(edge)) {
                return true;
            }
        }
        return false;
    }

    // A bounded convex shape of nonempty interior meets the hull's interior
    // when one of its edges does, or else when the hull's interior lies inside
    // it, which a triangle on three hull vertices witnesses.
    template <class OtherShape, class Edges>
    bool interiorsIntersectConvexShape(const OtherShape& other, Edges edges) const {
        if (hullSize() <= 2) {
            return convexHull().interiorsIntersect(other);
        }
        if (other.isDegenerate()) {
            return false;
        }
        for (const auto& edge : edges) {
            if (interiorsIntersect(edge)) {
                return true;
            }
        }
        return other.interiorsIntersect(Triangle<PointType>(vertex(0), vertex(1), vertex(2)));
    }

    // Whether removing the segment s disconnects a hull of at least three
    // vertices: s must run across the interior from outside to outside.
    template <class OtherSegment>
    bool segmentSeparates(const OtherSegment& s) const {
        return !(s[0] == s[1]) && !interiorContains(s[0]) && !interiorContains(s[1]) &&
               interiorsIntersect(s);
    }

    // Whether removing the hull disconnects the convex polygon `other`: its
    // boundary leaves the hull along two arcs or more, as Convex counts them.
    template <class OtherConvex>
    bool separatesConvex(const OtherConvex& other) const {
        if (other.isDegenerate()) {
            if (const auto spanned = other.getIfSegment()) {
                return separates(*spanned);
            }
            return false;
        }
        if (hullSize() <= 2) {
            return convexHull().separates(other);
        }
        using OtherPoint = typename OtherConvex::PointType;
        const std::ptrdiff_t m = static_cast<std::ptrdiff_t>(other.size());
        int arcs = 0;
        bool previous = contains(other.get(m - 1));
        for (std::ptrdiff_t i = 0; i < m; ++i) {
            const bool current = contains(other.get(i));
            if (!current && (previous || intersects(Segment<OtherPoint>(other.get(i - 1), other.get(i))))) {
                if (++arcs >= 2) {
                    return true;
                }
            }
            previous = current;
        }
        return false;
    }

    // Whether removing the convex polygon `other` disconnects a hull of at
    // least three vertices, by the same count over other's boundary.
    template <class OtherConvex>
    bool convexSeparates(const OtherConvex& other) const {
        using OtherPoint = typename OtherConvex::PointType;
        if (other.isDegenerate()) {
            if (const auto spanned = other.getIfSegment()) {
                return segmentSeparates(*spanned);
            }
            return false;
        }
        const std::ptrdiff_t m = static_cast<std::ptrdiff_t>(other.size());
        int arcs = 0;
        bool previous = interiorContains(other.get(m - 1));
        for (std::ptrdiff_t i = 0; i < m; ++i) {
            const bool current = interiorContains(other.get(i));
            if (!current && (previous || segmentSeparates(Segment<OtherPoint>(other.get(i - 1), other.get(i))))) {
                if (++arcs >= 2) {
                    return true;
                }
            }
            previous = current;
        }
        return false;
    }

    template <class OtherConvex>
    bool crossesConvex(const OtherConvex& other) const {
        if (other.empty()) {
            return false;  // the empty set disconnects nothing
        }
        if (hullSize() <= 2) {
            return convexHull().crosses(other);
        }
        if (bbox().crosses(other.bbox())) {
            return true;
        }
        return separatesConvex(other) && convexSeparates(other);
    }

    // Appends the vertices of chain k below v that lie in [*lo, *hi] (a null
    // bound is open), in order.
    void collect(int k, Index v, const PointType* lo, const PointType* hi,
                 std::vector<PointType>& out) const {
        const Node& n = nodes_[v];
        if (isLeaf(v)) {
            if ((!lo || !(n.point < *lo)) && (!hi || !(*hi < n.point))) {
                out.push_back(n.point);
            }
            return;
        }
        const Chain& c = n.chain[k];
        if (!lo || !(c.a < *lo)) {
            collect(k, n.child[0], lo, (hi && *hi < c.a) ? hi : &c.a, out);
        }
        if (!hi || !(*hi < c.b)) {
            collect(k, n.child[1], (lo && c.b < *lo) ? lo : &c.b, hi, out);
        }
    }
};

template <class Container>
    requires detail::is_point_v<std::remove_cvref_t<std::ranges::range_value_t<const Container&>>>
DynamicConvexHull(const Container&)
    -> DynamicConvexHull<std::remove_cvref_t<std::ranges::range_value_t<const Container&>>>;

}  // namespace pgl
