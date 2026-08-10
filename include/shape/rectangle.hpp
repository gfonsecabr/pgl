#pragma once

#include "shape/ray.hpp"

/**
 * @file rectangle.hpp
 * @brief Public declaration of pgl::Rectangle.
 *
 * Rectangles are axis-aligned finite 2D boxes used both as shapes and as exact
 * bounding boxes for other finite geometry.
 */

#include <array>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <ostream>
#include <ranges>
#include <stdexcept>
#include <type_traits>
#include <optional>
#include <utility>
#include <variant>


namespace pgl {

namespace detail {

template <std::floating_point ResultNumber, class Value>
constexpr ResultNumber lowerFloatingBound(const Value& value) {
    if constexpr (requires { value.template lowerBound<ResultNumber>(); }) {
        return value.template lowerBound<ResultNumber>();
    } else {
        return static_cast<ResultNumber>(value);
    }
}

template <std::floating_point ResultNumber, class Value>
constexpr ResultNumber upperFloatingBound(const Value& value) {
    if constexpr (requires { value.template upperBound<ResultNumber>(); }) {
        return value.template upperBound<ResultNumber>();
    } else {
        return static_cast<ResultNumber>(value);
    }
}

}  // namespace detail

template <class PointType = Point<>, class Label>
struct Rectangle;

Rectangle() -> Rectangle<Point<>, NoLabel>;

template <class PointType>
Rectangle(PointType, PointType) -> Rectangle<PointType, NoLabel>;

template <class Number>
Rectangle(Number, Number, Number, Number) -> Rectangle<Point<Number>, NoLabel>;

template <std::ranges::input_range Range>
    requires detail::is_point_v<std::ranges::range_value_t<Range>>
Rectangle(Range&&) -> Rectangle<std::remove_cvref_t<std::ranges::range_value_t<Range>>, NoLabel>;

/**
 * @brief Axis-aligned rectangle stored by its minimum and maximum corners.
 *
 * The stored corners are always `(min x, min y)` and `(max x, max y)`.
 *
 * @tparam PointType Corner point type.
 */
template <class PointType_, class TLabel>
struct Rectangle {
    using PointType = PointType_;
    using NumberType = PointType::NumberType;
    using LabelType = TLabel;

    static_assert(detail::is_point_v<PointType>, "Rectangle requires pgl::Point corners");

    /**
     * @brief Selects unordered or oriented boundary segments.
     *
     * @tparam Oriented When `true`, uses @ref OrientedSegment.
     */
    template <bool Oriented>
    using BoundaryType = std::conditional_t<Oriented, OrientedSegment<PointType>, Segment<PointType>>;

    /**
     * @brief Forward iterator over the four rectangle boundary edges.
     *
     * @tparam Oriented When `true`, iterates oriented boundary edges.
     */
    template <bool Oriented>
    class BoundaryIterator;

    /** Forward iterator over the four corner vertices. */
    class CornerIterator;

    /** Standard range/container typedefs over the vertex sequence. */
    using value_type = PointType;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference = PointType;
    using const_reference = PointType;
    using iterator = CornerIterator;
    using const_iterator = CornerIterator;

    using EdgeIterator = BoundaryIterator<false>;
    using OrientedEdgeIterator = BoundaryIterator<true>;

    /**
     * @brief Creates the empty rectangle `[(0,0),(-1,-1)]`.
     *
     * The maximum corner falls below the minimum one, which no pair of opposite
     * corners normalizes to, so the rectangle covers no point at all and
     * behaves as @ref EmptyShape everywhere. See @ref isEmpty.
     */
    constexpr Rectangle() : points_(emptyCorners()) {}

    /**
     * @brief Creates an axis-aligned rectangle from two opposite corners.
     *
     * When the normalized minimum and maximum corners coincide with the input
     * points, their labels are preserved. Otherwise synthesized corners use
     * default-constructed labels.
     *
     * Passing @p minmax stores the corners as given, which is also the only way
     * to build an empty rectangle other than @ref Rectangle(): corners that
     * invert on either axis are not swapped back but read as the empty set, and
     * normalized to its one canonical representation so that all empty
     * rectangles compare equal.
     *
     * @param first First opposite corner.
     * @param second Second opposite corner.
     * @param minmax True if we know that first.x() < second.x() and first.y() < second.y()
     */
    constexpr Rectangle(PointType first, PointType second, bool minmax = false) {
        if (minmax) {
            if (second.x() < first.x() || second.y() < first.y()) {
                points_ = emptyCorners();
                return;
            }
            points_[0] = std::move(first);
            points_[1] = std::move(second);
            return;
        }

        const NumberType min_x = second.x() < first.x() ? second.x() : first.x();
        const NumberType min_y = second.y() < first.y() ? second.y() : first.y();
        const NumberType max_x = first.x() < second.x() ? second.x() : first.x();
        const NumberType max_y = first.y() < second.y() ? second.y() : first.y();

        const PointType normalized_min = makeCorner(min_x, min_y);
        const PointType normalized_max = makeCorner(max_x, max_y);

        if (first == normalized_min && second == normalized_max) {
            points_[0] = std::move(first);
            points_[1] = std::move(second);
            return;
        }

        if (second == normalized_min && first == normalized_max) {
            points_[0] = std::move(second);
            points_[1] = std::move(first);
            return;
        }

        points_[0] = normalized_min;
        points_[1] = normalized_max;
    }

    /**
     * @brief Creates an axis-aligned rectangle from four coordinates.
     *
     * @param x1 X coordinate of the first corner.
     * @param y1 Y coordinate of the first corner.
     * @param x2 X coordinate of the second corner.
     * @param y2 Y coordinate of the second corner.
     * @param minmax True if we know that x1 < x2 and y1 < y2
     */
    constexpr Rectangle(NumberType x1, NumberType y1, NumberType x2, NumberType y2, bool minmax = false)
        : Rectangle(PointType(x1, y1), PointType(x2, y2), minmax) {}

    template<PointConcept OtherPointType, class OtherLabelType>
        requires(std::constructible_from<PointType, const OtherPointType&>)
    constexpr Rectangle(const Rectangle<OtherPointType, OtherLabelType>& other)
        : Rectangle(PointType(other.min()), PointType(other.max()), true) {
        label_ = detail::copyLabel<LabelType>(other);
    }

    /** @brief Assigns from a rectangle with compatible point and label types. */
    template<PointConcept OtherPointType, class OtherLabelType>
        requires(std::constructible_from<PointType, const OtherPointType&>)
    constexpr Rectangle& operator=(const Rectangle<OtherPointType, OtherLabelType>& other) {
        points_[0] = PointType(other.min());
        points_[1] = PointType(other.max());
        label_ = detail::copyLabel<LabelType>(other);
        return *this;
    }

    /**
     * @brief Creates the bounding box of a range of points.
     *
     * The rectangle corners are the componentwise minimum and maximum points
     * found in the range. An empty range encloses nothing, so it gives the
     * empty rectangle.
     *
     * @tparam Range Input range whose elements can be converted to @ref PointType.
     * @param points Range of points to enclose.
     */
    template<std::ranges::input_range Range = std::initializer_list<PointType>>
    requires std::ranges::common_range<Range> &&
             std::convertible_to<std::ranges::range_value_t<Range>, PointType>
    constexpr explicit Rectangle(Range&& points) : Rectangle() {
        for(const auto &p : points) {
            insert(p);
        }
    }


    /**
     * @brief Creates the bounding box of a range of bounded shapes.
     *
     * The rectangle corners are the componentwise minimum and maximum points
     * found in the range. An empty range, or one whose shapes are all empty,
     * encloses nothing and gives the empty rectangle.
     *
     * @tparam Range Input range whose elements can be converted to @ref PointType.
     * @param points Range of points to enclose.
     */
    template <std::ranges::input_range Range>
        requires(!detail::is_point_v<typename std::ranges::range_value_t<Range>> && requires(const typename std::ranges::range_value_t<Range>& shape) { shape.bbox(); })
    constexpr explicit Rectangle(Range&& shapes) : Rectangle() {
        for(const auto &s : shapes) {
            insert(s);
        }
    }

    /**
     * @brief Returns corner `index` for `index` in `[0, 4)`.
     *
     * Corners are returned in counterclockwise order starting from the
     * minimum corner: `min`, `bottomRight`, `max`, `topLeft`. Two of the
     * four corners are synthesized from the stored `min`/`max`, so the
     * result is returned by value.
     *
     * The empty rectangle has no corners, so @ref size is `0` and every index
     * is out of range; the assertion is the only check.
     *
     * @param index Corner index.
     * @return The selected corner.
     */
    constexpr PointType operator[](std::size_t index) const {
        assert(index < size());
        switch (index) {
            case 0: return min();
            case 1: return bottomRight();
            case 2: return max();
            default: return topLeft();
        }
    }

    /**
     * @brief Returns whether the rectangle is the empty set of points.
     *
     * A rectangle is empty when its stored maximum corner falls below its
     * minimum one. Normalizing two opposite corners never produces that state,
     * so it is reached only by @ref Rectangle(), by the `minmax` constructor,
     * and by the operations that answer with a rectangle covering nothing --
     * all of which store the one canonical empty pair `(0,0),(-1,-1)`. An empty
     * rectangle behaves as @ref EmptyShape: it has no vertices, no area, and
     * every predicate reads it as the empty set.
     *
     * Because the empty set has exactly that one representation, and because
     * every rectangle covering a point has `min x <= max x`, the x axis alone
     * decides the question and the y axis need not be read. The assertion is
     * what holds a future corner-writing operation to that invariant.
     *
     * Complexity: O(1).
     *
     * @return `true` if the rectangle covers no point.
     */
    [[nodiscard]] constexpr bool isEmpty() const {
        assert(!(points_[1].y() < points_[0].y()) || points_[1].x() < points_[0].x());
        return points_[1].x() < points_[0].x();
    }

    /**
     * @brief Returns the number of corners: `4`, or `0` when @ref isEmpty.
     */
    [[nodiscard]] constexpr std::size_t size() const {
        return isEmpty() ? 0 : 4;
    }

    /**
     * @brief Cyclic access: same as @ref operator[] but `index` is taken
     * modulo @ref size(); negative indices wrap from the end.
     *
     * The empty rectangle has no corners to wrap around, so calling this on one
     * is a precondition violation.
     */
    constexpr PointType get(std::ptrdiff_t index) const {
        assert(!isEmpty());
        const std::ptrdiff_t n = static_cast<std::ptrdiff_t>(size());
        return (*this)[static_cast<std::size_t>(((index % n) + n) % n)];
    }

    /**
     * @brief Returns the smallest index `i` with `(*this)[i] == point`, or
     * `-1` if no corner equals `point`.
     *
     * The empty rectangle has no corners, so it always answers `-1`.
     */
    constexpr std::ptrdiff_t index(const PointType& point) const {
        for (std::ptrdiff_t i = 0; i < static_cast<std::ptrdiff_t>(size()); ++i) {
            if ((*this)[static_cast<std::size_t>(i)] == point) {
                return i;
            }
        }
        return -1;
    }

    /**
     * @brief Returns the minimum corner `(min x, min y)`.
     *
     * The corners of an empty rectangle are inverted placeholders, not points
     * the rectangle covers; see @ref isEmpty.
     *
     * @return Reference to the minimum corner.
     */
    constexpr const PointType& min() const {
        return points_[0];
    }

    /**
     * @brief Returns the maximum corner `(max x, max y)`.
     *
     * The corners of an empty rectangle are inverted placeholders, not points
     * the rectangle covers; see @ref isEmpty.
     *
     * @return Reference to the maximum corner.
     */
    constexpr const PointType& max() const {
        return points_[1];
    }

    /**
     * @brief Returns the rectangle width.
     *
     * @return `max x - min x`, or `0` when @ref isEmpty.
     */
    [[nodiscard]] constexpr auto width() const {
        using Result = decltype(max().x() - min().x());
        return isEmpty() ? Result(0) : Result(max().x() - min().x());
    }

    /**
     * @brief Returns the rectangle height.
     *
     * @return `max y - min y`, or `0` when @ref isEmpty.
     */
    [[nodiscard]] constexpr auto height() const {
        using Result = decltype(max().y() - min().y());
        return isEmpty() ? Result(0) : Result(max().y() - min().y());
    }

    /**
     * @brief Forward iterator over the four corners in counterclockwise order.
     *
     * Dereferences via `Rectangle::operator[]`, so two of the four corners
     * are synthesized on the fly and yielded by value.
     */
    class CornerIterator {
      public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = PointType;
        using difference_type = std::ptrdiff_t;
        using pointer = void;
        using reference = PointType;

        constexpr CornerIterator() = default;
        constexpr CornerIterator(const Rectangle* rect, std::size_t index)
            : rect_(rect), index_(index) {}

        constexpr PointType operator*() const {
            return (*rect_)[index_];
        }
        constexpr CornerIterator& operator++() {
            ++index_;
            return *this;
        }
        constexpr CornerIterator operator++(int) {
            CornerIterator tmp = *this;
            ++index_;
            return tmp;
        }
        constexpr bool operator==(const CornerIterator&) const = default;

      private:
        const Rectangle* rect_ = nullptr;
        std::size_t index_ = 0;
    };

    /**
     * @brief Returns an iterator to the minimum corner.
     */
    constexpr CornerIterator begin() const {
        return CornerIterator(this, 0);
    }

    /**
     * @brief Returns an iterator to the minimum corner.
     */
    constexpr CornerIterator cbegin() const {
        return CornerIterator(this, 0);
    }

    /**
     * @brief Returns an iterator past the last corner.
     */
    constexpr CornerIterator end() const {
        return CornerIterator(this, size());
    }

    /**
     * @brief Returns an iterator past the last corner.
     */
    constexpr CornerIterator cend() const {
        return CornerIterator(this, size());
    }

    /**
     * @brief Returns an iterator to the first edge.
     *
     * Edges are visited in the same order as @ref edges().
     *
     * @return Iterator to the bottom edge.
     */
    constexpr EdgeIterator edgesBegin() const {
        return EdgeIterator(this, 0);
    }

    /**
     * @brief Returns an iterator past the last edge.
     *
     * @return Sentinel iterator for @ref edgesBegin().
     */
    constexpr EdgeIterator edgesEnd() const {
        return EdgeIterator(this, size());
    }

    /**
     * @brief Returns an iterator to the first oriented edge.
     *
     * Oriented edges are visited in the same order as @ref orientedEdges().
     *
     * @return Iterator to the bottom oriented edge.
     */
    constexpr OrientedEdgeIterator orientedEdgesBegin() const {
        return OrientedEdgeIterator(this, 0);
    }

    /**
     * @brief Returns an iterator past the last oriented edge.
     *
     * @return Sentinel iterator for @ref orientedEdgesBegin().
     */
    constexpr OrientedEdgeIterator orientedEdgesEnd() const {
        return OrientedEdgeIterator(this, size());
    }

    /**
     * @brief Provides lexicographic ordering on `(min, max)`.
     *
     * @param other Rectangle to compare with.
     * @return -1, 0, or 1.
     */
    constexpr bool operator==(const Rectangle& other) const {
        return points_ == other.points_;
    }

    /** @brief Orders rectangles lexicographically by their `(min, max)` corners, ignoring the label. */
    constexpr auto operator<=>(const Rectangle& other) const {
        return points_ <=> other.points_;
    }

    /**
     * @brief Returns the rectangle label.
     *
     * The label is mutable even through a const rectangle: it is metadata that
     * does not participate in equality, hashing, or geometric predicates.
     *
     * @return Reference to the stored label.
     */
    template <class A = LabelType>
        requires(detail::has_label_v<A>)
    constexpr A& label() const {
        return label_;
    }

    /**
     * @brief Returns the rectangle area.
     *
     * @tparam ResultNumber Result type (default: NumberType).
     * @return `width * height`, which is `0` when @ref isEmpty.
     */
    template <class ResultNumber = NumberType>
    [[nodiscard]] constexpr ResultNumber area() const;

    /**
     * @brief Returns twice the rectangle area.
     *
     * @return `2 * area`.
     */
    [[nodiscard]] constexpr auto twiceArea() const;

    /**
     * @brief Returns whether the rectangle has empty interior.
     *
     * The empty rectangle has no area either, so it is degenerate.
     *
     * @return `true` when width or height is zero, or when @ref isEmpty.
     */
    [[nodiscard]] constexpr bool isDegenerate() const;

    /**
     * @brief Returns whether the rectangle collapses to a single point.
     *
     * Complexity: O(1).
     *
     * @return `true` if width and height are both zero.
     */
    [[nodiscard]] constexpr bool isPoint() const;

    /**
     * @brief Returns the point the rectangle collapses to, if it does.
     *
     * Complexity: O(1).
     *
     * @return The corner if @ref isPoint, `std::nullopt` otherwise.
     */
    [[nodiscard]] constexpr std::optional<PointType> getIfPoint() const;

    /**
     * @brief Returns whether the rectangle collapses to a non-degenerate segment.
     *
     * True when exactly one of width and height is zero, so the rectangle is a
     * horizontal or vertical segment.
     *
     * Complexity: O(1).
     *
     * @return `true` if the rectangle is a segment of positive length.
     */
    [[nodiscard]] constexpr bool isSegment() const;

    /**
     * @brief Returns the segment the rectangle collapses to, if it does.
     *
     * Complexity: O(1).
     *
     * @return The segment from @ref min to @ref max if @ref isSegment,
     *         `std::nullopt` otherwise.
     */
    [[nodiscard]] constexpr std::optional<BoundaryType<false>> getIfSegment() const;

    /**
     * @brief Returns whether the rectangle is degenerate without collapsing to
     * a point or to a segment.
     *
     * A rectangle is never undefined: a degenerate one is always empty, a
     * point, or a segment, so this always returns `false`. Provided for
     * uniformity with the other shapes.
     *
     * Complexity: O(1).
     *
     * @return `false`.
     */
    [[nodiscard]] constexpr bool isUndefined() const;

    /**
     * @brief Returns the bounding box of the rectangle.
     *
     * @return This rectangle.
     */
    [[nodiscard]] constexpr Rectangle bbox() const;

    /**
     * @brief Returns a bounding box of the rectangle with floating point coordinates.
     *
     * @tparam ResultNumber Floating point type.
     * @return A rectangle that contains the rectangle, empty when @ref isEmpty.
     */
    template <std::floating_point ResultNumber = double>
    [[nodiscard]] constexpr Rectangle<Point<ResultNumber>> fbox() const;

    /**
     * @brief Returns the four vertices in counterclockwise order.
     *
     * The first vertex is the minimum corner. The empty rectangle has no
     * vertices, so calling this on one is a precondition violation; iterate
     * with @ref begin / @ref end to handle it.
     *
     * @return Vertices `(xmin,ymin)`, `(xmax,ymin)`, `(xmax,ymax)`, `(xmin,ymax)`.
     */
    [[nodiscard]] constexpr std::array<PointType, 4> vertices() const;

    /**
     * @brief Returns the four edges as unordered segments.
     *
     * The empty rectangle has no edges, so calling this on one is a
     * precondition violation; iterate with @ref edgesBegin / @ref edgesEnd to
     * handle it.
     *
     * @return Bottom, right, top, and left edges.
     */
    [[nodiscard]] constexpr std::array<Segment<PointType>, 4> edges() const;

    /**
     * @brief Returns the four boundary edges in counterclockwise order.
     *
     * The empty rectangle has no edges, so calling this on one is a
     * precondition violation; iterate with @ref orientedEdgesBegin /
     * @ref orientedEdgesEnd to handle it.
     *
     * @return Bottom, right, top, and left oriented edges.
     */
    [[nodiscard]] constexpr std::array<OrientedSegment<PointType>, 4> orientedEdges() const;

    /**
     * @brief Converts the rectangle to a convex polygon.
     *
     * The four corners already follow the canonical convex order
     * (counterclockwise, lexicographically smallest first), and degenerate
     * rectangles collapse to their hull. An empty rectangle gives the empty
     * convex polygon, which has no vertices.
     *
     * @return Convex polygon with the same corners.
     */
    [[nodiscard]] constexpr explicit operator Convex<PointType>() const {
        if (isEmpty()) {
            return Convex<PointType>();
        }
        return Convex<PointType>(*this, !isDegenerate());
    }

    /**
     * @brief Returns the rectangle as a convex polygon.
     *
     * @return Convex polygon with the same corners.
     */
    [[nodiscard]] constexpr Convex<PointType> asConvex() const {
        return static_cast<Convex<PointType>>(*this);
    }

    /**
     * @brief Returns the rectangle as a half-plane intersection.
     *
     * The region is the intersection of the four edge half-planes. A degenerate
     * rectangle produces the corresponding degenerate region (a segment or a
     * point), and an empty one the empty region.
     *
     * @return Half-plane intersection whose point set is this rectangle.
     */
    [[nodiscard]] constexpr HalfplaneIntersection<PointType> asHalfplaneIntersection() const {
        return HalfplaneIntersection<PointType>(*this);
    }

    /**
     * @brief Converts the rectangle to a simple polygon.
     *
     * The four corners already follow the canonical polygon order
     * (counterclockwise, lexicographically smallest first). An empty rectangle
     * gives the empty polygon, which has no vertices.
     *
     * @return Polygon with the same corners.
     */
    [[nodiscard]] constexpr explicit operator Polygon<PointType>() const {
        if (isEmpty()) {
            return Polygon<PointType>();
        }
        return Polygon<PointType>(*this, !isDegenerate());
    }

    /**
     * @brief Returns the rectangle as a simple polygon.
     *
     * @return Polygon with the same corners.
     */
    [[nodiscard]] constexpr Polygon<PointType> asPolygon() const {
        return static_cast<Polygon<PointType>>(*this);
    }

    /**
     * @brief Returns the rectangle as a hole-free region.
     *
     * @return PolygonWithHoles whose outer boundary is the rectangle and which
     *         has no holes.
     */
    [[nodiscard]] constexpr PolygonWithHoles<PointType> asPolygonWithHoles() const {
        return PolygonWithHoles<PointType>(asPolygon());
    }

    /**
     * @brief Returns the rectangle as a one-component set of regions.
     *
     * A rectangle with no area covers nothing that survives regularization, so
     * it gives back the empty set rather than a component without area.
     *
     * @return PolygonSet whose only component is the rectangle as a region.
     */
    [[nodiscard]] constexpr PolygonSet<PointType> asPolygonSet() const {
        return PolygonSet<PointType>(asPolygonWithHoles());
    }

    /**
     * @brief Returns whether a point is one of the rectangle vertices.
     *
     * @tparam OtherPoint Type of the point.
     *
     * @param point Point to test.
     * @return `true` if the point is a rectangle corner.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool verticesContain(const OtherPoint& point) const;

    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     *
     * The boundary is included.
     *
     * @tparam OtherPoint Type of the point.
     *
     * @param point Point to test.
     * @return `true` if the point lies in the rectangle.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool contains(const OtherPoint& point) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template<LineConcept OtherLine>
    [[nodiscard]] constexpr bool contains(const OtherLine& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template<OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool contains(const OtherOrientedLine& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template<SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool contains(const OtherSegment& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template<OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool contains(const OtherOrientedSegment& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template<RayConcept OtherRay>
    [[nodiscard]] constexpr bool contains(const OtherRay& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template<HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool contains(const OtherHalfplane& other) const;

    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     *
     * The boundary is included.
     *
     * @tparam OtherNumber Coordinate type of the other rectangle corners.
     * @tparam OtherPoint::LabelType Label type of the other rectangle corners.
     * @param other Other rectangle.
     * @return `true` if both corners of `other` lie in this rectangle.
     */
    template<RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool contains(const OtherRectangle& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template<TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr bool contains(const OtherTriangle& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template<ConvexConcept OtherConvex>
    [[nodiscard]] constexpr bool contains(const OtherConvex& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template<PolygonConcept OtherPolygon>
    [[nodiscard]] constexpr bool contains(const OtherPolygon& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template<DiskConcept OtherDisk>
    [[nodiscard]] constexpr bool contains(const OtherDisk& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    [[nodiscard]] constexpr bool contains(const Shape<PointType>& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    [[nodiscard]] constexpr bool boundaryContains(const Shape<PointType>& other) const;

    // The empty set is a subset of every shape (contained in all of them) and
    // disjoint from all of them, so containment is true while separation is
    // false. These overloads let an EmptyShape flow through Shape's variant
    // dispatch without special-casing.
    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool contains(const EmptyShape<EmptyPoint>&) const {
        return true;
    }
    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool boundaryContains(const EmptyShape<EmptyPoint>&) const {
        return true;
    }
    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool interiorContains(const EmptyShape<EmptyPoint>&) const {
        return true;
    }
    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool separates(const EmptyShape<EmptyPoint>&) const {
        return false;
    }

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorContains(const OtherPoint& point) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template<LineConcept OtherLine>
    [[nodiscard]] constexpr bool interiorContains(const OtherLine& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template<OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool interiorContains(const OtherOrientedLine& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template<SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool interiorContains(const OtherSegment& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template<OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool interiorContains(const OtherOrientedSegment& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template<RayConcept OtherRay>
    [[nodiscard]] constexpr bool interiorContains(const OtherRay& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template<HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool interiorContains(const OtherHalfplane& other) const;

    /**
     * @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B).
     *
     * The boundary is excluded.
     *
     * @tparam OtherNumber Coordinate type of the other rectangle corners.
     * @tparam OtherPoint::LabelType Label type of the other rectangle corners.
     * @param other Other rectangle.
     * @return `true` if the interior contains both corners of `other`.
     */
    template<RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool interiorContains(const OtherRectangle& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template<TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr bool interiorContains(const OtherTriangle& other) const;

    /**
     * @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B).
     *
     * @tparam OtherPoint Type of the point.
     *
     * @param point Point to test.
     * @return `true` if the point lies on the rectangle boundary.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool boundaryContains(const OtherPoint& point) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool boundaryContains(const OtherSegment& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool boundaryContains(const OtherOrientedSegment& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<LineConcept OtherLine>
    [[nodiscard]] constexpr bool boundaryContains(const OtherLine& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool boundaryContains(const OtherOrientedLine& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<RayConcept OtherRay>
    [[nodiscard]] constexpr bool boundaryContains(const OtherRay& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool boundaryContains(const OtherHalfplane& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool boundaryContains(const OtherRectangle& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr bool boundaryContains(const OtherTriangle& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<ConvexConcept OtherConvex>
    [[nodiscard]] constexpr bool boundaryContains(const OtherConvex& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<PolygonConcept OtherPolygon>
    [[nodiscard]] constexpr bool boundaryContains(const OtherPolygon& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<DiskConcept OtherDisk>
    [[nodiscard]] constexpr bool boundaryContains(const OtherDisk& other) const;

    /**
     * @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅).
     *
     * Boundary contact counts as intersection.
     *
     * @tparam OtherNumber Coordinate type of the other rectangle corners.
     * @tparam OtherPoint::LabelType Label type of the other rectangle corners.
     * @param other Other rectangle.
     * @return `true` if the rectangles share at least one point.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool intersects(const OtherPoint& other) const;

    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template<RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool intersects(const OtherRectangle& other) const;

    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template<LineConcept OtherLine>
    [[nodiscard]] constexpr bool intersects(const OtherLine& other) const;

    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template<OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool intersects(const OtherOrientedLine& other) const;

    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template<SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool intersects(const OtherSegment& other) const;

    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template<OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool intersects(const OtherOrientedSegment& other) const;

    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template<RayConcept OtherRay>
    [[nodiscard]] constexpr bool intersects(const OtherRay& other) const;

    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template<HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool intersects(const OtherHalfplane& other) const;

    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    [[nodiscard]] constexpr bool intersects(const Shape<PointType>& other) const;

    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template<typename OtherShape>
        requires (!PointConcept<OtherShape> && detail::shapeRank<OtherShape> > detail::shapeRank<Rectangle>)
    [[nodiscard]] constexpr bool intersects(const OtherShape& other) const {
        return other.intersects(*this);
    }

    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool intersects(const EmptyShape<EmptyPoint>&) const {
        return false;
    }

    /**
     * @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅).
     *
     * Rectangles with empty interiors never satisfy this predicate.
     *
     * @tparam OtherNumber Coordinate type of the other rectangle corners.
     * @tparam OtherPoint::LabelType Label type of the other rectangle corners.
     * @param other Other rectangle.
     * @return `true` if the interiors overlap with positive area.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherPoint& other) const;

    /** @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅). */
    template<RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherRectangle& other) const;

    /** @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅). */
    template<LineConcept OtherLine>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherLine& other) const;

    /** @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅). */
    template<OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherOrientedLine& other) const;

    /** @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅). */
    template<SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherSegment& other) const;

    /** @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅). */
    template<OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherOrientedSegment& other) const;

    /** @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅). */
    template<RayConcept OtherRay>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherRay& other) const;

    /** @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅). */
    template<HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherHalfplane& other) const;

    /** @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅). */
    template<typename OtherShape>
        requires (!PointConcept<OtherShape> && detail::shapeRank<OtherShape> > detail::shapeRank<Rectangle>)
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherShape& other) const {
        return other.interiorsIntersect(*this);
    }

    /** @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅). */
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool interiorsIntersect(const EmptyShape<EmptyPoint>&) const {
        return false;
    }

    /** @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅). */
    [[nodiscard]] constexpr bool interiorsIntersect(const Shape<PointType>& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template<RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool separates(const OtherRectangle& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool separates(const OtherPoint& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template<LineConcept OtherLine>
    [[nodiscard]] constexpr bool separates(const OtherLine& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template<OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool separates(const OtherOrientedLine& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template<SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool separates(const OtherSegment& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template<OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool separates(const OtherOrientedSegment& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template<RayConcept OtherRay>
    [[nodiscard]] constexpr bool separates(const OtherRay& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template<HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool separates(const OtherHalfplane& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template<TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr bool separates(const OtherTriangle& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template<ConvexConcept OtherConvex>
    [[nodiscard]] constexpr bool separates(const OtherConvex& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template<DiskConcept OtherDisk>
    [[nodiscard]] constexpr bool separates(const OtherDisk& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template<PolygonConcept OtherPolygon>
    [[nodiscard]] constexpr bool separates(const OtherPolygon& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template<MonotoneChainConcept OtherChain>
    [[nodiscard]] constexpr bool contains(const OtherChain& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<MonotoneChainConcept OtherChain>
    [[nodiscard]] constexpr bool boundaryContains(const OtherChain& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template<MonotoneChainConcept OtherChain>
    [[nodiscard]] constexpr bool interiorContains(const OtherChain& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template<MonotoneChainConcept OtherChain>
    [[nodiscard]] constexpr bool separates(const OtherChain& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template<PolylineConcept OtherPolyline>
    [[nodiscard]] constexpr bool contains(const OtherPolyline& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<PolylineConcept OtherPolyline>
    [[nodiscard]] constexpr bool boundaryContains(const OtherPolyline& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template<PolylineConcept OtherPolyline>
    [[nodiscard]] constexpr bool interiorContains(const OtherPolyline& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template<PolylineConcept OtherPolyline>
    [[nodiscard]] constexpr bool separates(const OtherPolyline& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template<HalfplaneIntersectionConcept OtherRegion>
    [[nodiscard]] constexpr bool contains(const OtherRegion& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<HalfplaneIntersectionConcept OtherRegion>
    [[nodiscard]] constexpr bool boundaryContains(const OtherRegion& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template<HalfplaneIntersectionConcept OtherRegion>
    [[nodiscard]] constexpr bool interiorContains(const OtherRegion& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template<HalfplaneIntersectionConcept OtherRegion>
    [[nodiscard]] constexpr bool separates(const OtherRegion& other) const;

    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     *
     * A region is contained exactly when its outer polygon is: the region holds
     * the whole outer ring whatever its holes do, and this shape has a
     * connected complement. See implementation/contains.hpp.
     */
    template<PolygonWithHolesConcept OtherRegion>
    [[nodiscard]] constexpr bool contains(const OtherRegion& other) const;

    /**
     * @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B).
     *
     * A boundary has no area, so it holds only a region with no area — which is
     * exactly the union of that region's ring edges.
     */
    template<PolygonWithHolesConcept OtherRegion>
    [[nodiscard]] constexpr bool boundaryContains(const OtherRegion& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template<PolygonWithHolesConcept OtherRegion>
    [[nodiscard]] constexpr bool interiorContains(const OtherRegion& other) const;

    /**
     * @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected).
     *
     * The region is settled by the cell engine of implementation/separates.hpp;
     * see the notes on pgl::PolygonWithHoles::separates for what a region
     * admits that a simply connected target does not.
     */
    template<PolygonWithHolesConcept OtherRegion>
    [[nodiscard]] bool separates(const OtherRegion& other) const;

    // -------------------------------------------------------------------------
    // A set of regions
    //
    // It outranks every other shape, so the symmetric relations reach it through
    // the rank-based forwarders and only the asymmetric ones are answered here.
    // A set is the union of its components, so it is contained exactly when
    // every component is — no matter what this shape is.

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template<PolygonSetConcept OtherSet>
    [[nodiscard]] constexpr bool contains(const OtherSet& other) const {
        for (const auto& component : other) {
            if (!contains(component)) {
                return false;
            }
        }
        return true;
    }

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<PolygonSetConcept OtherSet>
    [[nodiscard]] constexpr bool boundaryContains(const OtherSet& other) const {
        for (const auto& component : other) {
            if (!boundaryContains(component)) {
                return false;
            }
        }
        return true;
    }

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template<PolygonSetConcept OtherSet>
    [[nodiscard]] constexpr bool interiorContains(const OtherSet& other) const {
        for (const auto& component : other) {
            if (!interiorContains(component)) {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected).
     *
     * A set of regions is the one target that may already be in several pieces
     * before anything is removed, so this neither folds over its components nor
     * answers false for a remover that misses it. See
     * implementation/separates.hpp.
     */
    template<PolygonSetConcept OtherSet>
    [[nodiscard]] bool separates(const OtherSet& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    [[nodiscard]] constexpr bool separates(const Shape<PointType>& other) const;

    // --- not-yet-implemented predicate pairs (throw); see implementation ---
    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template<DiskConcept OtherDisk>
    [[nodiscard]] constexpr bool interiorContains(const OtherDisk& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template<ConvexConcept OtherConvex>
    [[nodiscard]] constexpr bool interiorContains(const OtherConvex& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template<PolygonConcept OtherPolygon>
    [[nodiscard]] constexpr bool interiorContains(const OtherPolygon& other) const;


    /**
     * @brief Tests whether the two shapes mutually separate each other (each disconnects the other).
     *
     * For axis-aligned rectangles, this means each one separates the other.
     *
     * @tparam OtherNumber Coordinate type of the other rectangle corners.
     * @tparam OtherPoint::LabelType Label type of the other rectangle corners.
     * @param other Other rectangle.
     * @return `true` if both rectangles split each other.
     */
    template<RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool crosses(const OtherRectangle& other) const;

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool crosses(const OtherPoint& other) const;

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template<LineConcept OtherLine>
    [[nodiscard]] constexpr bool crosses(const OtherLine& other) const;

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template<OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool crosses(const OtherOrientedLine& other) const;

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template<SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool crosses(const OtherSegment& other) const;

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template<OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool crosses(const OtherOrientedSegment& other) const;

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template<RayConcept OtherRay>
    [[nodiscard]] constexpr bool crosses(const OtherRay& other) const;

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template<HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool crosses(const OtherHalfplane& other) const;

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template<typename OtherShape>
        requires (!PointConcept<OtherShape> && detail::shapeRank<OtherShape> > detail::shapeRank<Rectangle>)
    [[nodiscard]] constexpr bool crosses(const OtherShape& other) const {
        return other.crosses(*this);
    }

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool crosses(const EmptyShape<EmptyPoint>&) const {
        return false;
    }

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    [[nodiscard]] constexpr bool crosses(const Shape<PointType>& other) const;

    /** @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint. */
    template <class ResultNumber = NumberType, PointConcept OtherPoint>
    [[nodiscard]] constexpr std::optional<Point<ResultNumber, typename PointType::LabelType>>
    intersection(const OtherPoint& other) const;

    /** @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint. */
    template <class ResultNumber = NumberType, RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr std::optional<Rectangle<Point<ResultNumber, typename PointType::LabelType>>>
    intersection(const OtherRectangle& other) const;

    /** @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint. @warning Divides coordinates after casting to ResultNumber. */
    template <class ResultNumber = division_result_t<NumberType>, LineConcept OtherLine>
    [[nodiscard]] constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherLine& other) const;

    /** @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint. @warning Divides coordinates after casting to ResultNumber. */
    template <class ResultNumber = division_result_t<NumberType>, OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherOrientedLine& other) const;

    /** @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint. @warning Divides coordinates after casting to ResultNumber. */
    template <class ResultNumber = division_result_t<NumberType>, SegmentConcept OtherSegment>
    [[nodiscard]] constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherSegment& other) const;

    /** @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint. @warning Divides coordinates after casting to ResultNumber. */
    template <class ResultNumber = division_result_t<NumberType>, OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherOrientedSegment& other) const;

    /** @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint. @warning Divides coordinates after casting to ResultNumber. */
    template <class ResultNumber = division_result_t<NumberType>, RayConcept OtherRay>
    [[nodiscard]] constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherRay& other) const;

    /** @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint. @warning Divides coordinates after casting to ResultNumber. */
    template <class ResultNumber = division_result_t<NumberType>, HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr auto intersection(const OtherHalfplane& other) const;

    /** @brief Adds this rectangle's four constraints to a half-plane intersection without deriving vertices. */
    template <class ResultNumber = NumberType, HalfplaneIntersectionConcept OtherRegion>
    [[nodiscard]] constexpr auto intersection(const OtherRegion& other) const {
        return other.template intersection<ResultNumber>(*this);
    }

    /** @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint. @warning Divides coordinates after casting to ResultNumber. */
    template <class ResultNumber = division_result_t<NumberType>, typename OtherShape>
        requires (!PointConcept<OtherShape>
                  && !HalfplaneIntersectionConcept<OtherShape>
                  && (detail::shapeRank<OtherShape> > detail::shapeRank<Rectangle>)
                  && requires(const OtherShape& o, const Rectangle& self) {
                         o.template intersection<ResultNumber>(self);
                     })
    [[nodiscard]] constexpr auto intersection(const OtherShape& other) const {
        return other.template intersection<ResultNumber>(*this);
    }

    /** @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint. */
    template <class ResultNumber = NumberType, class EmptyPoint>
    [[nodiscard]] constexpr EmptyShape<EmptyPoint> intersection(const EmptyShape<EmptyPoint>&) const {
        return {};
    }

    /**
     * @brief Returns the squared Euclidean distance to the given shape.
     *
     * The closest point of an axis-aligned rectangle has integer coordinate
     * gaps, so this overload involves no division and is exact.
     *
     * The empty rectangle has no nearest point, so no distance is defined from
     * it; this holds for every distance on this shape, Euclidean, L1, LInf, and
     * Hausdorff alike, and calling one on an empty rectangle is a precondition
     * violation.
     *
     * @tparam ResultNumber Coordinate type of the returned distance (default: NumberType).
     * @tparam OtherPoint Type of the point.
     *
     * @param point Point to measure from.
     * @return Squared Euclidean distance.
     */
    template <class ResultNumber = NumberType, PointConcept OtherPoint>
    [[nodiscard]] constexpr auto squaredDistance(const OtherPoint& point) const;

    /**
     * @brief Returns the squared Euclidean distance to the given shape.
     *
     * @tparam ResultNumber Coordinate type of the returned distance (default: @ref division_result_t).
     *
     * @warning With an integer @p ResultNumber the exact squared distance is
     *          generally a fraction, so the internal division truncates and the
     *          result is inexact. Request a floating-point or pgl::Rational
     *          result type, e.g. `squaredDistance<double>(other)`, for an
     *          accurate value.
     */
    template <class ResultNumber = division_result_t<NumberType>, LineConcept OtherLine>
    [[nodiscard]] constexpr auto squaredDistance(const OtherLine& other) const;

    /** @copydoc squaredDistance(const OtherLine&) const */
    template <class ResultNumber = division_result_t<NumberType>, OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr auto squaredDistance(const OtherOrientedLine& other) const;

    /** @copydoc squaredDistance(const OtherLine&) const */
    template <class ResultNumber = division_result_t<NumberType>, SegmentConcept OtherSegment>
    [[nodiscard]] constexpr auto squaredDistance(const OtherSegment& other) const;

    /** @copydoc squaredDistance(const OtherLine&) const */
    template <class ResultNumber = division_result_t<NumberType>, OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr auto squaredDistance(const OtherOrientedSegment& other) const;

    /** @copydoc squaredDistance(const OtherLine&) const */
    template <class ResultNumber = division_result_t<NumberType>, RayConcept OtherRay>
    [[nodiscard]] constexpr auto squaredDistance(const OtherRay& other) const;

    /** @copydoc squaredDistance(const OtherLine&) const */
    template <class ResultNumber = division_result_t<NumberType>, HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr auto squaredDistance(const OtherHalfplane& other) const;

    /**
     * @brief Returns the squared Euclidean distance to the given shape.
     *
     * Rectangle-to-rectangle distance uses axis gaps only and is exact.
     *
     * @tparam ResultNumber Coordinate type of the returned distance (default: NumberType).
     * @tparam OtherNumber Coordinate type of the other rectangle corners.
     * @tparam OtherPoint::LabelType Label type of the other rectangle corners.
     * @param other Other rectangle.
     * @return Squared Euclidean distance.
     */
    template <class ResultNumber = NumberType, RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr auto squaredDistance(const OtherRectangle& other) const;

    /**
     * @brief Returns the squared Euclidean distance to the given shape.
     *
     * Forwards to the other shape's implementation so that each unordered pair
     * needs `squaredDistance` defined only once, on the higher-ranked shape.
     */
    template <class ResultNumber = division_result_t<NumberType>, typename OtherShape>
        requires ((detail::shapeRank<OtherShape> > detail::shapeRank<Rectangle>)
                  && requires(const OtherShape& o, const Rectangle& self) {
                         o.template squaredDistance<ResultNumber>(self);
                     })
    [[nodiscard]] constexpr auto squaredDistance(const OtherShape& other) const {
        return other.template squaredDistance<ResultNumber>(*this);
    }

    /**
     * @brief Returns the squared Euclidean distance to a disk.
     *
     * Forwards to @ref Disk::squaredDistance. Reports in `detail::floating_result_t<ResultNumber>`: a distance realized on a
     * circle is generally irrational, so a floating-point `ResultNumber` is
     * honoured as asked and any other request falls back to `double`.
     */
    template <class ResultNumber = double, class DiskPointType, class DiskLabel>
    [[nodiscard]] detail::floating_result_t<ResultNumber> squaredDistance(const Disk<DiskPointType, DiskLabel>& disk) const {
        return disk.template squaredDistance<ResultNumber>(*this);
    }

    /**
     * @brief Returns the Manhattan (L1) distance to the given shape.
     *
     * An axis-aligned rectangle's closest point has integer coordinate gaps
     * against another axis-aligned shape, so the point/rectangle overloads
     * involve no division and are exact.
     */
    template <class ResultNumber = NumberType, PointConcept OtherPoint>
    [[nodiscard]] constexpr auto distanceL1(const OtherPoint& point) const;

    /** @copydoc distanceL1(const OtherPoint&) const */
    template <class ResultNumber = division_result_t<NumberType>, LineConcept OtherLine>
    [[nodiscard]] constexpr auto distanceL1(const OtherLine& other) const;

    /** @copydoc distanceL1(const OtherPoint&) const */
    template <class ResultNumber = division_result_t<NumberType>, OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr auto distanceL1(const OtherOrientedLine& other) const;

    /** @copydoc distanceL1(const OtherPoint&) const */
    template <class ResultNumber = division_result_t<NumberType>, SegmentConcept OtherSegment>
    [[nodiscard]] constexpr auto distanceL1(const OtherSegment& other) const;

    /** @copydoc distanceL1(const OtherPoint&) const */
    template <class ResultNumber = division_result_t<NumberType>, OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr auto distanceL1(const OtherOrientedSegment& other) const;

    /** @copydoc distanceL1(const OtherPoint&) const */
    template <class ResultNumber = division_result_t<NumberType>, RayConcept OtherRay>
    [[nodiscard]] constexpr auto distanceL1(const OtherRay& other) const;

    /** @copydoc distanceL1(const OtherPoint&) const */
    template <class ResultNumber = division_result_t<NumberType>, HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr auto distanceL1(const OtherHalfplane& other) const;

    /** @copydoc distanceL1(const OtherPoint&) const */
    template <class ResultNumber = NumberType, RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr auto distanceL1(const OtherRectangle& other) const;

    /**
     * @brief Returns the Manhattan (L1) distance to the given shape.
     *
     * Forwards to the other shape's implementation so that each unordered pair
     * needs `distanceL1` defined only once, on the higher-ranked shape.
     */
    template <class ResultNumber = division_result_t<NumberType>, typename OtherShape>
        requires ((detail::shapeRank<OtherShape> > detail::shapeRank<Rectangle>)
                  && requires(const OtherShape& o, const Rectangle& self) {
                         o.template distanceL1<ResultNumber>(self);
                     })
    [[nodiscard]] constexpr auto distanceL1(const OtherShape& other) const {
        return other.template distanceL1<ResultNumber>(*this);
    }

    /**
     * @brief Returns the intersection of the two shapes (A ∩ B), re-dispatching
     *        through the wrapper's own `intersection`.
     *
     * An intersection is symmetric, so this just calls @p other's own
     * `intersection`, which visits its wrapped alternative and throws if the
     * pair is unsupported.
     *
     * The point type is deduced from @p other so a plain concrete shape cannot
     * reach this overload through an implicit conversion to `Shape`.
     *
     * @return The intersection wrapped in a `Shape`, rather than the tighter
     *   type the concrete pair would answer with: which alternative @p other
     *   holds is not known until run time, so neither is the result's.
     */
    template <class ResultNumber = division_result_t<NumberType>, PointConcept OtherPoint>
    [[nodiscard]] constexpr auto intersection(const Shape<OtherPoint>& other) const {
        return other.template intersection<ResultNumber>(*this);
    }

    /**
     * @brief Returns the regularized union of the two shapes (A ∪ B),
     *        re-dispatching through the wrapper's own `unionWith`.
     *
     * A union is symmetric, so this just calls @p other's own `unionWith`, which
     * visits its wrapped alternative and throws if the pair is unsupported —
     * here, whenever @p other turns out to hold anything but a bounded polygonal
     * region. See @ref Polygon::unionWith for the contract.
     *
     * The point type is deduced from @p other so a plain concrete shape cannot
     * reach this overload through an implicit conversion to `Shape`.
     */
    template <class ResultNumber = division_result_t<NumberType>, PointConcept OtherPoint>
    [[nodiscard]] auto unionWith(const Shape<OtherPoint>& other) const {
        return other.template unionWith<ResultNumber>(*this);
    }

    /**
     * @brief Returns the distance to the given shape, using symmetry to
     * re-dispatch through the wrapper's own `distanceL1`.
     *
     * Distance is symmetric, so this just calls @p other's own `distanceL1`,
     * which visits its wrapped alternative and throws if the pair is
     * unsupported.
     */
    template <class ResultNumber = double, PointConcept OtherPoint>
    [[nodiscard]] constexpr auto distanceL1(const Shape<OtherPoint>& other) const {
        return other.template distanceL1<ResultNumber>(*this);
    }

    /**
     * @brief Returns the Chebyshev (LInf) distance to the given shape.
     *
     * An axis-aligned rectangle's closest point has integer coordinate gaps
     * against another axis-aligned shape, so the point/rectangle overloads
     * involve no division and are exact.
     */
    template <class ResultNumber = NumberType, PointConcept OtherPoint>
    [[nodiscard]] constexpr auto distanceLInf(const OtherPoint& point) const;

    /** @copydoc distanceLInf(const OtherPoint&) const */
    template <class ResultNumber = division_result_t<NumberType>, LineConcept OtherLine>
    [[nodiscard]] constexpr auto distanceLInf(const OtherLine& other) const;

    /** @copydoc distanceLInf(const OtherPoint&) const */
    template <class ResultNumber = division_result_t<NumberType>, OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr auto distanceLInf(const OtherOrientedLine& other) const;

    /** @copydoc distanceLInf(const OtherPoint&) const */
    template <class ResultNumber = division_result_t<NumberType>, SegmentConcept OtherSegment>
    [[nodiscard]] constexpr auto distanceLInf(const OtherSegment& other) const;

    /** @copydoc distanceLInf(const OtherPoint&) const */
    template <class ResultNumber = division_result_t<NumberType>, OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr auto distanceLInf(const OtherOrientedSegment& other) const;

    /** @copydoc distanceLInf(const OtherPoint&) const */
    template <class ResultNumber = division_result_t<NumberType>, RayConcept OtherRay>
    [[nodiscard]] constexpr auto distanceLInf(const OtherRay& other) const;

    /** @copydoc distanceLInf(const OtherPoint&) const */
    template <class ResultNumber = division_result_t<NumberType>, HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr auto distanceLInf(const OtherHalfplane& other) const;

    /** @copydoc distanceLInf(const OtherPoint&) const */
    template <class ResultNumber = NumberType, RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr auto distanceLInf(const OtherRectangle& other) const;

    /**
     * @brief Returns the Chebyshev (LInf) distance to the given shape.
     *
     * Forwards to the other shape's implementation so that each unordered pair
     * needs `distanceLInf` defined only once, on the higher-ranked shape.
     */
    template <class ResultNumber = division_result_t<NumberType>, typename OtherShape>
        requires ((detail::shapeRank<OtherShape> > detail::shapeRank<Rectangle>)
                  && requires(const OtherShape& o, const Rectangle& self) {
                         o.template distanceLInf<ResultNumber>(self);
                     })
    [[nodiscard]] constexpr auto distanceLInf(const OtherShape& other) const {
        return other.template distanceLInf<ResultNumber>(*this);
    }

    /**
     * @brief Returns the distance to the given shape, using symmetry to
     * re-dispatch through the wrapper's own `distanceLInf`.
     *
     * Distance is symmetric, so this just calls @p other's own `distanceLInf`,
     * which visits its wrapped alternative and throws if the pair is
     * unsupported.
     */
    template <class ResultNumber = double, PointConcept OtherPoint>
    [[nodiscard]] constexpr auto distanceLInf(const Shape<OtherPoint>& other) const {
        return other.template distanceLInf<ResultNumber>(*this);
    }

    /** @brief Returns the Manhattan (L1) Hausdorff distance to the given shape. */
    template <class ResultNumber = NumberType, RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr auto hausdorffDistanceL1(const OtherRectangle& other) const;

    /** @copydoc hausdorffDistanceL1(const OtherRectangle&) const */
    template <class ResultNumber = NumberType, PointConcept OtherPoint>
    [[nodiscard]] constexpr auto hausdorffDistanceL1(const OtherPoint& point) const;

    /** @copydoc hausdorffDistanceL1(const OtherRectangle&) const */
    template <class ResultNumber = division_result_t<NumberType>, SegmentConcept OtherSegment>
    [[nodiscard]] constexpr auto hausdorffDistanceL1(const OtherSegment& other) const;

    /** @copydoc hausdorffDistanceL1(const OtherRectangle&) const */
    template <class ResultNumber = division_result_t<NumberType>, OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr auto hausdorffDistanceL1(const OtherOrientedSegment& other) const;

    /**
     * @brief Returns the Manhattan (L1) Hausdorff distance to the given shape.
     *
     * Forwards to the other shape's implementation so that each unordered pair
     * needs `hausdorffDistanceL1` defined only once, on the higher-ranked shape.
     */
    template <class ResultNumber = division_result_t<NumberType>, typename OtherShape>
        requires ((detail::shapeRank<OtherShape> > detail::shapeRank<Rectangle>)
                  && requires(const OtherShape& o, const Rectangle& self) {
                         o.template hausdorffDistanceL1<ResultNumber>(self);
                     })
    [[nodiscard]] constexpr auto hausdorffDistanceL1(const OtherShape& other) const {
        return other.template hausdorffDistanceL1<ResultNumber>(*this);
    }

    /**
     * @brief Returns the distance to the given shape, using symmetry to
     * re-dispatch through the wrapper's own `hausdorffDistanceL1`.
     *
     * Distance is symmetric, so this just calls @p other's own `hausdorffDistanceL1`,
     * which visits its wrapped alternative and throws if the pair is
     * unsupported.
     */
    template <class ResultNumber = division_result_t<NumberType>, PointConcept OtherPoint>
    [[nodiscard]] constexpr auto hausdorffDistanceL1(const Shape<OtherPoint>& other) const {
        return other.template hausdorffDistanceL1<ResultNumber>(*this);
    }

    /** @brief Returns the Chebyshev (LInf) Hausdorff distance to the given shape. */
    template <class ResultNumber = NumberType, RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr auto hausdorffDistanceLInf(const OtherRectangle& other) const;

    /** @copydoc hausdorffDistanceLInf(const OtherRectangle&) const */
    template <class ResultNumber = NumberType, PointConcept OtherPoint>
    [[nodiscard]] constexpr auto hausdorffDistanceLInf(const OtherPoint& point) const;

    /** @copydoc hausdorffDistanceLInf(const OtherRectangle&) const */
    template <class ResultNumber = division_result_t<NumberType>, SegmentConcept OtherSegment>
    [[nodiscard]] constexpr auto hausdorffDistanceLInf(const OtherSegment& other) const;

    /** @copydoc hausdorffDistanceLInf(const OtherRectangle&) const */
    template <class ResultNumber = division_result_t<NumberType>, OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr auto hausdorffDistanceLInf(const OtherOrientedSegment& other) const;

    /**
     * @brief Returns the Chebyshev (LInf) Hausdorff distance to the given shape.
     *
     * Forwards to the other shape's implementation so that each unordered pair
     * needs `hausdorffDistanceLInf` defined only once, on the higher-ranked shape.
     */
    template <class ResultNumber = division_result_t<NumberType>, typename OtherShape>
        requires ((detail::shapeRank<OtherShape> > detail::shapeRank<Rectangle>)
                  && requires(const OtherShape& o, const Rectangle& self) {
                         o.template hausdorffDistanceLInf<ResultNumber>(self);
                     })
    [[nodiscard]] constexpr auto hausdorffDistanceLInf(const OtherShape& other) const {
        return other.template hausdorffDistanceLInf<ResultNumber>(*this);
    }

    /**
     * @brief Returns the distance to the given shape, using symmetry to
     * re-dispatch through the wrapper's own `hausdorffDistanceLInf`.
     *
     * Distance is symmetric, so this just calls @p other's own `hausdorffDistanceLInf`,
     * which visits its wrapped alternative and throws if the pair is
     * unsupported.
     */
    template <class ResultNumber = division_result_t<NumberType>, PointConcept OtherPoint>
    [[nodiscard]] constexpr auto hausdorffDistanceLInf(const Shape<OtherPoint>& other) const {
        return other.template hausdorffDistanceLInf<ResultNumber>(*this);
    }

    /**
     * @brief Returns the squared Hausdorff distance to another rectangle.
     *
     * For axis-aligned rectangles, the directed Hausdorff distance is attained
     * at a vertex. It uses only point-to-rectangle distances and is exact.
     *
     * @tparam ResultNumber Coordinate type of the returned distance (default: NumberType).
     * @tparam OtherNumber Coordinate type of the other rectangle corners.
     * @tparam OtherPoint::LabelType Label type of the other rectangle corners.
     * @param other Other rectangle.
     * @return Squared Hausdorff distance.
     */
    template <class ResultNumber = NumberType, RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr auto squaredHausdorffDistance(const OtherRectangle& other) const;

    /**
     * @brief Returns the squared Hausdorff distance to a point.
     *
     * @tparam ResultNumber Coordinate type of the returned distance (default: NumberType).
     * @tparam OtherPoint Type of the point.
     * @param point Point to measure from.
     * @return Squared Hausdorff distance.
     */
    template <class ResultNumber = NumberType, PointConcept OtherPoint>
    [[nodiscard]] constexpr auto squaredHausdorffDistance(const OtherPoint& point) const;

    /** @copydoc squaredHausdorffDistance(const OtherPoint&) const */
    template <class ResultNumber = division_result_t<NumberType>, SegmentConcept OtherSegment>
    [[nodiscard]] constexpr auto squaredHausdorffDistance(const OtherSegment& other) const;

    /** @copydoc squaredHausdorffDistance(const OtherPoint&) const */
    template <class ResultNumber = division_result_t<NumberType>, OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr auto squaredHausdorffDistance(const OtherOrientedSegment& other) const;

    /**
     * @brief Returns the squared Hausdorff distance to the given shape.
     *
     * Forwards to the other shape's implementation so that each unordered pair
     * needs `squaredHausdorffDistance` defined only once, on the higher-ranked shape.
     */
    template <class ResultNumber = division_result_t<NumberType>, typename OtherShape>
        requires ((detail::shapeRank<OtherShape> > detail::shapeRank<Rectangle>)
                  && requires(const OtherShape& o, const Rectangle& self) {
                         o.template squaredHausdorffDistance<ResultNumber>(self);
                     })
    [[nodiscard]] constexpr auto squaredHausdorffDistance(const OtherShape& other) const {
        return other.template squaredHausdorffDistance<ResultNumber>(*this);
    }

    /**
     * @brief Enlarges the rectangle so that it contains the given point.
     *
     * Existing corner labels are preserved when their coordinates do not
     * change. Newly synthesized corners use default-constructed labels.
     *
     * An empty rectangle bounds nothing and so cannot be grown: it becomes the
     * inserted point outright, rather than stretching to reach its inverted
     * placeholder corners.
     *
     * @tparam OtherPoint Type of the point.
     *
     * @param point Point to insert.
     */
    template <PointConcept OtherPoint>
    constexpr void insert(const OtherPoint& point);

    /**
     * @brief Enlarges the rectangle so that it contains another rectangle.
     *
     * Inserting an empty rectangle changes nothing; inserting into an empty one
     * makes it a copy of @p other.
     *
     * @tparam OtherNumber Coordinate type of the other rectangle.
     * @tparam OtherPoint::LabelType Label type of the other rectangle.
     * @param other Rectangle to insert.
     */
    template <RectangleConcept OtherRectangle>
    constexpr void insert(const OtherRectangle& other);

    /**
     * @brief Enlarges the rectangle so that it contains a finite shape.
     *
     * The shape must expose `bbox()`. Infinite shapes such as lines, rays, and
     * halfplanes do not have a finite bounding box and are intentionally not
     * accepted by this overload. A shape whose bounding box is empty
     * contributes nothing.
     *
     * @tparam Shape Shape type exposing `bbox()`.
     * @param shape Shape to insert.
     */
    template <class TShape>
        requires(!detail::is_point_v<TShape> && !RectangleConcept<TShape> && requires(const TShape& shape) { shape.bbox(); })
    constexpr void insert(const TShape& shape);

    /**
     * @brief Enlarges the rectangle so that it contains every point in a range.
     *
     * A single bounded shape (which exposes `bbox()`) is handled by the
     * shape overload, even though it may itself be iterable as a range.
     *
     * @tparam Range Range of points.
     * @param range Points to insert.
     */
    template<std::ranges::input_range Range = std::initializer_list<PointType>>
    requires std::ranges::common_range<Range> &&
             std::convertible_to<std::ranges::range_value_t<Range>, PointType> &&
             (!requires(const std::remove_cvref_t<Range>& shape) { shape.bbox(); })
    constexpr void insert(Range&& range) {
        // Defined inline so MSVC can match the constrained overload; the
        // overloaded out-of-line form trips MSVC's C2244.
        for (const auto& point : range) {
            insert(point);
        }
    }

    /**
     * @brief Enlarges the rectangle to contain every point in a range of shapes.
     *
     * The shape must expose `bbox()`. Infinite shapes such as lines, rays, and
     * halfplanes do not have a finite bounding box and are intentionally not
     * accepted by this overload.
     *
     * @tparam Range Range of bounded shapes.
     * @param range Range of shapes to insert.
     */
    template <std::ranges::input_range Range>
        requires(!detail::is_point_v<typename std::ranges::range_value_t<Range>> && requires(const typename std::ranges::range_value_t<Range>& shape) { shape.bbox(); })
    constexpr void insert(Range&& range) {
        // Defined inline so MSVC can match the constrained overload; the
        // overloaded out-of-line form trips MSVC's C2244.
        for (const auto& shape : range) {
            insert(shape);
        }
    }

    /**
     * @brief Returns a segment defining a diameter.
     *
     * For a rectangle, a diagonal is a diameter. The empty rectangle has no
     * diagonal, so calling this on one is a precondition violation.
     *
     * @return The diagonal from `min()` to `max()`.
     */
    [[nodiscard]] constexpr Segment<PointType> diameter() const;

    /**
     * @brief Returns the midpoint of the rectangle.
     *
     * The empty rectangle has no point to be the middle of, so calling this on
     * one is a precondition violation.
     *
     * @tparam ResultNumber Coordinate type of the midpoint.
     * @return Midpoint with no label.
     * @warning Divides coordinates by 2. Inexact for odd integer coordinates.
     */
    template <class ResultNumber = division_result_t<NumberType>>
    [[nodiscard]] constexpr Point<ResultNumber> midpoint() const;

    /**
     * @brief Returns the centroid of the rectangle.
     *
     * @tparam ResultNumber Coordinate type of the centroid.
     * @return The rectangle midpoint.
     * @warning Divides coordinates by 2. Inexact for odd integer coordinates.
     */
    template <class ResultNumber = division_result_t<NumberType>>
    [[nodiscard]] constexpr Point<ResultNumber> centroid() const;

    /**
     * @brief Returns the circumcircle of the rectangle.
     *
     * The returned disk passes through the rectangle corners. The empty
     * rectangle has none, so calling this on one is a precondition violation.
     *
     * @return Disk passing through three rectangle corners.
     */
    [[nodiscard]] constexpr Disk<PointType, NoLabel> circumcircle() const;

    /**
     * @brief Returns the center of the rectangle.
     *
     * @tparam ResultNumber Coordinate type of the center.
     * @return The rectangle midpoint.
     */
    template <class ResultNumber = division_result_t<NumberType>>
    [[nodiscard]] constexpr Point<ResultNumber> center() const;

    /**
     * @brief Returns a point inside the rectangle.
     *
     * This is the midpoint, even for degenerate rectangles. The empty rectangle
     * has no point inside it, so calling this on one is a precondition
     * violation.
     *
     * @tparam ResultNumber Coordinate type of the midpoint.
     * @return Midpoint with the rectangle coordinate type.
     * @warning Divides coordinates by 2. Inexact for odd integer coordinates.
     */
    template <class ResultNumber = division_result_t<NumberType>>
    [[nodiscard]] constexpr Point<ResultNumber> pointInside() const;

    /**
     * @brief Tests whether some point in this shape's relative interior lies in
     *        the strict interior of @p shape.
     *
     * Uses @ref pointInside as the witness. When integer truncation rounds that
     * witness onto or outside the boundary, this shape and @p shape are scaled
     * so the witness is exact, leaving the containment relation unchanged.
     */
    template <class OtherShape>
    [[nodiscard]] constexpr bool pointInsideInteriorContainedIn(const OtherShape& shape) const;

    /**
     * @brief Returns the rectangle rotated by 90k degrees around the origin.
     *
     * @param k Number of 90-degree CCW rotations (may be negative).
     * @return Rotated rectangle.
     */
    [[nodiscard]] constexpr Rectangle rotated90(int k = 1) const;

    /**
     * @brief Rotates the rectangle by 90k degrees around the origin in place.
     *
     * @param k Number of 90-degree CCW rotations (may be negative).
     */
    constexpr void rotate90(int k = 1);

    /** @brief Returns the rectangle with its x-coordinates multiplied by a factor. */
    template <class OtherNumber>
    [[nodiscard]] constexpr Rectangle scaledUpX(const OtherNumber scalar) const;

    /** @brief Multiplies the rectangle's x-coordinates by a factor in place. */
    template <class OtherNumber>
    constexpr void scaleUpX(const OtherNumber scalar);

    /** @brief Returns the rectangle with its y-coordinates multiplied by a factor. */
    template <class OtherNumber>
    [[nodiscard]] constexpr Rectangle scaledUpY(const OtherNumber scalar) const;

    /** @brief Multiplies the rectangle's y-coordinates by a factor in place. */
    template <class OtherNumber>
    constexpr void scaleUpY(const OtherNumber scalar);

    /** @brief Returns the rectangle with its x-coordinates divided by a divisor. */
    template <class OtherNumber>
    [[nodiscard]] constexpr Rectangle scaledDownX(const OtherNumber scalar) const;

    /** @brief Divides the rectangle's x-coordinates by a divisor in place. */
    template <class OtherNumber>
    constexpr void scaleDownX(const OtherNumber scalar);

    /** @brief Returns the rectangle with its y-coordinates divided by a divisor. */
    template <class OtherNumber>
    [[nodiscard]] constexpr Rectangle scaledDownY(const OtherNumber scalar) const;

    /** @brief Divides the rectangle's y-coordinates by a divisor in place. */
    template <class OtherNumber>
    constexpr void scaleDownY(const OtherNumber scalar);

    /**
     * @brief Returns the Minkowski sum of this shape and another (A ⊕ B).
     *
     * The sum is the point set `{a + b : a ∈ A, b ∈ B}`. Summing with a
     * `Point` is a translation, so it returns this shape's own type; two
     * bounded convex shapes sum to a @ref Convex, or to a @ref Rectangle when
     * both are rectangles. See @ref MinkowskiSummableConcept for the pairs a
     * Minkowski sum is defined for.
     *
     * @tparam OtherShape Type of the other shape.
     * @param other Shape to sum with.
     * @return The Minkowski sum, in the tightest type that represents it.
     */
    template <class OtherShape>
        requires MinkowskiSummableConcept<Rectangle<PointType_, TLabel>, OtherShape>
    [[nodiscard]] constexpr auto minkowskiSum(const OtherShape& other) const;

    /**
     * @brief Returns the regularized Minkowski sum of the two shapes (A ⊕ B).
     *
     * The pairs @ref MinkowskiSummableConcept rejects are exactly the ones whose
     * sum needs a region-valued result rather than one bounded convex shape;
     * they are implemented on
     * @ref Polygon and @ref PolygonWithHoles. Forwards to the other shape's
     * implementation so that each unordered pair needs the sum defined only once,
     * on the higher-ranked shape.
     */
    template <class ResultNumber = division_result_t<NumberType>, typename OtherShape>
        requires (!MinkowskiSummableConcept<Rectangle<PointType_, TLabel>, OtherShape>
                  && (detail::shapeRank<OtherShape> > detail::shapeRank<Rectangle>)
                  && requires(const OtherShape& o, const Rectangle& self) {
                         o.template minkowskiSum<ResultNumber>(self);
                     })
    [[nodiscard]] auto minkowskiSum(const OtherShape& other) const {
        return other.template minkowskiSum<ResultNumber>(*this);
    }

    /**
     * @brief Returns the regularized union of the two shapes (A ∪ B).
     *
     * Two rectangles are the one pair of @ref PolygonalRegionConcept operands a
     * rectangle owns, being the lowest-ranked of them: every other pair is
     * defined on the higher-ranked operand and reached through the forwarding
     * overload below. The union of two rectangles is a rectangle only by
     * coincidence — two that overlap in a corner make an L, and two that are
     * apart make two pieces — so it answers with a set of regions like every
     * other union. See @ref Polygon::unionWith for the contract.
     *
     * @tparam ResultNumber The number type for the result.
     * @param other The rectangle to unite with.
     * @return The pieces of the union, in canonical order.
     */
    template <class ResultNumber = division_result_t<NumberType>, RectangleConcept OtherRectangle>
    [[nodiscard]] PolygonSet<Point<ResultNumber, typename PointType::LabelType>>
    unionWith(const OtherRectangle& other) const;

    /**
     * @brief Returns the regularized union of the two shapes (A ∪ B).
     *
     * Forwards to the other shape's implementation so that each unordered pair
     * needs `unionWith` defined only once, on the higher-ranked shape. See
     * @ref Polygon::unionWith for the contract.
     */
    template <class ResultNumber = division_result_t<NumberType>, typename OtherShape>
        requires ((detail::shapeRank<OtherShape> > detail::shapeRank<Rectangle>)
                  && requires(const OtherShape& o, const Rectangle& self) {
                         o.template unionWith<ResultNumber>(self);
                     })
    [[nodiscard]] auto unionWith(const OtherShape& other) const {
        return other.template unionWith<ResultNumber>(*this);
    }

    /**
     * @brief Returns the regularized symmetric difference of the two shapes (A △ B).
     *
     * Forwards to the other shape's implementation so that each unordered pair
     * needs `symmetricDifference` defined only once, on the higher-ranked shape.
     * See @ref Polygon::symmetricDifference for the contract.
     */
    template <class ResultNumber = division_result_t<NumberType>, typename OtherShape>
        requires ((detail::shapeRank<OtherShape> > detail::shapeRank<Rectangle>)
                  && requires(const OtherShape& o, const Rectangle& self) {
                         o.template symmetricDifference<ResultNumber>(self);
                     })
    [[nodiscard]] auto symmetricDifference(const OtherShape& other) const {
        return other.template symmetricDifference<ResultNumber>(*this);
    }

    /**
     * @brief Translates both stored corners in place.
     *
     * @tparam OtherNumber Coordinate type of the translation point.
     * @tparam OtherPoint::LabelType Label type of the translation point.
     * @param translation Translation vector.
     * @return Reference to this rectangle.
     */
    template<PointConcept OtherPoint>
    constexpr Rectangle& operator+=(const OtherPoint& translation);

    /**
     * @brief Translates both stored corners by the opposite vector in place.
     *
     * @tparam OtherNumber Coordinate type of the translation point.
     * @tparam OtherPoint::LabelType Label type of the translation point.
     * @param translation Translation vector to subtract.
     * @return Reference to this rectangle.
     */
    template<PointConcept OtherPoint>
    constexpr Rectangle& operator-=(const OtherPoint& translation);

    /**
     * @brief Scales the rectangle around the origin in place.
     *
     * Negative scalars keep the stored corners normalized through the
     * non-member scaling operator.
     *
     * @tparam Scalar Scalar type.
     * @param scalar Scale factor.
     * @return Reference to this rectangle.
     */
    template <class Scalar>
        requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
    constexpr Rectangle& operator*=(const Scalar& scalar);

    /**
     * @brief Divides the rectangle coordinates by a scalar in place.
     *
     * Negative divisors keep the stored corners normalized through the
     * non-member division operator.
     *
     * @tparam Scalar Scalar type.
     * @param scalar Divisor.
     * @return Reference to this rectangle.
     */
    template <class Scalar>
        requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
    constexpr Rectangle& operator/=(const Scalar& scalar);

    template <bool Oriented>
    class BoundaryIterator {
      public:
        using iterator_category = std::forward_iterator_tag;
        using iterator_concept = std::forward_iterator_tag;
        using value_type = BoundaryType<Oriented>;
        using difference_type = std::ptrdiff_t;
        using reference = value_type;

        constexpr BoundaryIterator() = default;

        constexpr value_type operator*() const {
            assert(rectangle != nullptr);
            return rectangle->template boundaryAt<Oriented>(index);
        }

        constexpr BoundaryIterator& operator++() {
            ++index;
            return *this;
        }

        constexpr BoundaryIterator operator++(int) {
            BoundaryIterator copy(*this);
            ++(*this);
            return copy;
        }

        constexpr bool operator==(const BoundaryIterator& other) const = default;

      private:
        friend struct Rectangle;

        constexpr BoundaryIterator(const Rectangle* rectangle_arg, std::size_t index_arg)
            : rectangle(rectangle_arg), index(index_arg) {}

        const Rectangle* rectangle = nullptr;
        std::size_t index = 0;
    };

  private:
    static constexpr std::size_t edgeCount = 4;

    static constexpr PointType makeCorner(const NumberType& x, const NumberType& y) {
        return Point<NumberType, typename PointType::LabelType>(x, y, typename PointType::LabelType{});
    }

    /**
     * @brief The one representation every empty rectangle normalizes to.
     *
     * Equality, ordering, and hashing all read the stored corners, so the empty
     * set needs a single value rather than any pair that happens to invert.
     */
    static constexpr std::array<PointType, 2> emptyCorners() {
        return {makeCorner(NumberType(0), NumberType(0)),
                makeCorner(NumberType(-1), NumberType(-1))};
    }

    /**
     * @brief Returns the corner `(max x, min y)`.
     *
     * @return Bottom-right corner.
     */
    constexpr PointType bottomRight() const;

    /**
     * @brief Returns the corner `(min x, max y)`.
     *
     * @return Top-left corner.
     */
    constexpr PointType topLeft() const;

    /**
     * @brief Returns one boundary edge by index.
     *
     * Index order is bottom, right, top, left. When `Oriented` is `true`,
     * the edges are returned in counterclockwise orientation.
     *
     * @tparam Oriented Whether to return oriented edges.
     * @param index Edge index in `[0,4)`.
     * @return The selected edge.
     */
    template <bool Oriented>
    constexpr BoundaryType<Oriented> boundaryAt(std::size_t index) const;

    template <class Left, class Right>
    static constexpr bool intervalsOverlap(const Left& first_min, const Left& first_max, const Right& second_min, const Right& second_max);

    template <class Left, class Right>
    static constexpr bool intervalsOverlapStrict(const Left& first_min, const Left& first_max, const Right& second_min, const Right& second_max);

    template <class Left, class Right>
    static constexpr auto axisDistance(const Left& first_min, const Left& first_max, const Right& second_min, const Right& second_max)
        -> std::common_type_t<Left, Right>;

    // template <std::ranges::input_range Range>
    //     requires std::constructible_from<PointType, std::ranges::range_reference_t<Range>>
    // constexpr void assignBoundingBox(Range&& points);

    std::array<PointType, 2> points_{};
    [[no_unique_address]] mutable LabelType label_{};
};

/**
 * @brief Translates a rectangle by the opposite of a point.
 *
 * @tparam NumberType Coordinate type of the rectangle corners.
 * @tparam Label Label type of the rectangle corners.
 * @tparam TranslationNumber Coordinate type of the translation point.
 * @tparam TranslationLabel Label type of the translation point.
 * @param rectangle Rectangle to translate.
 * @param translation Translation vector to subtract.
 * @return Translated rectangle.
 */
template <class PointType, class LabelType, class TranslationNumber, class TranslationLabel>
constexpr auto operator-(const Rectangle<PointType, LabelType>& rectangle, const Point<TranslationNumber, TranslationLabel>& translation);

/**
 * @brief Scales a rectangle by a scalar.
 *
 * @tparam NumberType Coordinate type of the rectangle corners.
 * @tparam Label Label type of the rectangle corners.
 * @tparam Scalar Scalar type.
 * @param rectangle Rectangle to scale.
 * @param scalar Scale factor.
 * @return Scaled rectangle.
 */
template <class PointType, class LabelType, class Scalar>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr auto operator*(const Rectangle<PointType, LabelType>& rectangle, const Scalar& scalar);

/**
 * @brief Scales a rectangle by a scalar written on the left.
 *
 * @tparam Scalar Scalar type.
 * @tparam NumberType Coordinate type of the rectangle corners.
 * @tparam Label Label type of the rectangle corners.
 * @param scalar Scale factor.
 * @param rectangle Rectangle to scale.
 * @return Scaled rectangle.
 */
template <class Scalar, class PointType, class LabelType>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr auto operator*(const Scalar& scalar, const Rectangle<PointType, LabelType>& rectangle);

/**
 * @brief Divides both rectangle corners by a scalar.
 *
 * @tparam NumberType Coordinate type of the rectangle corners.
 * @tparam Label Label type of the rectangle corners.
 * @tparam Scalar Scalar type.
 * @param rectangle Rectangle to divide.
 * @param scalar Divisor.
 * @return Scaled rectangle.
 */
template <class PointType, class LabelType, class Scalar>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr auto operator/(const Rectangle<PointType, LabelType>& rectangle, const Scalar& scalar);

/**
 * @brief Streams a rectangle as `[min,max]`.
 *
 * @tparam NumberType Coordinate type of the rectangle corners.
 * @tparam Label Label type of the rectangle corners.
 * @param stream Output stream.
 * @param rectangle Rectangle to print.
 * @return The output stream.
 */
template <class PointType, class LabelType>
std::ostream& operator<<(std::ostream& stream, const Rectangle<PointType, LabelType>& rectangle);

}  // namespace pgl
