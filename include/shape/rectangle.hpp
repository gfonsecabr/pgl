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
     * @brief Creates the degenerate rectangle `[(0,0),(0,0)]`.
     */
    constexpr Rectangle() = default;

    /**
     * @brief Creates an axis-aligned rectangle from two opposite corners.
     *
     * When the normalized minimum and maximum corners coincide with the input
     * points, their labels are preserved. Otherwise synthesized corners use
     * default-constructed labels.
     *
     * @param first First opposite corner.
     * @param second Second opposite corner.
     * @param minmax True if we know that first.x() < second.x() and first.y() < second.y()
     */
    constexpr Rectangle(PointType first, PointType second, bool minmax = false) {
        if (minmax) {
            points_[0] = first;
            points_[1] = second;
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

    template<PointConcept OtherPointType, class OtherLabelType>
        requires(std::constructible_from<PointType, const OtherPointType&>)
    constexpr Rectangle& operator=(const Rectangle<OtherPointType, OtherLabelType>& other) {
        points_[0] = PointType(other.min());
        points_[1] = PointType(other.max());
        label_ = detail::copyLabel<LabelType>(other);
        return *this;
    }

    /**
     * @brief Creates the bounding box of a non-empty range of points.
     *
     * The rectangle corners are the componentwise minimum and maximum points
     * found in the range.
     *
     * @tparam Range Input range whose elements can be converted to @ref PointType.
     * @param points Range of points to enclose.
     * @throws std::invalid_argument If the range is empty.
     */
    template<std::ranges::input_range Range = std::initializer_list<PointType>>
    requires std::ranges::common_range<Range> &&
             std::convertible_to<std::ranges::range_value_t<Range>, PointType>
    constexpr explicit Rectangle(Range&& points) {
        if (points.begin() == points.end()) {
            throw std::invalid_argument("Rectangle bounding box requires at least one point");
        }
        points_[0] = points_[1] = static_cast<PointType>(*points.begin());
        for(const auto &p : points) {
            insert(p);
        }
    }


    /**
     * @brief Creates the bounding box of a non-empty range of bounded shapes.
     *
     * The rectangle corners are the componentwise minimum and maximum points
     * found in the range.
     *
     * @tparam Range Input range whose elements can be converted to @ref PointType.
     * @param points Range of points to enclose.
     * @throws std::invalid_argument If the range is empty.
     */
    template <std::ranges::input_range Range>
        requires(!detail::is_point_v<typename std::ranges::range_value_t<Range>> && requires(const typename std::ranges::range_value_t<Range>& shape) { shape.bbox(); })
    constexpr explicit Rectangle(Range&& shapes) {
        if (shapes.begin() == shapes.end()) {
            throw std::invalid_argument("Rectangle bounding box requires at least one point");
        }
        points_[0] = points_[1] = static_cast<PointType>(shapes.begin()->bbox().min());
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
     * @brief Returns the number of corners (always 4).
     */
    static constexpr std::size_t size() {
        return 4;
    }

    /**
     * @brief Cyclic access: same as @ref operator[] but `index` is taken
     * modulo @ref size(); negative indices wrap from the end.
     */
    constexpr PointType get(std::ptrdiff_t index) const {
        const std::ptrdiff_t n = static_cast<std::ptrdiff_t>(size());
        return (*this)[static_cast<std::size_t>(((index % n) + n) % n)];
    }

    /**
     * @brief Returns the smallest index `i` with `(*this)[i] == point`, or
     * `-1` if no corner equals `point`.
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
     * @return Reference to the minimum corner.
     */
    constexpr const PointType& min() const {
        return points_[0];
    }

    /**
     * @brief Returns the maximum corner `(max x, max y)`.
     *
     * @return Reference to the maximum corner.
     */
    constexpr const PointType& max() const {
        return points_[1];
    }

    /**
     * @brief Returns the rectangle width.
     *
     * @return `max x - min x`.
     */
    [[nodiscard]] constexpr auto width() const {
        return max().x() - min().x();
    }

    /**
     * @brief Returns the rectangle height.
     *
     * @return `max y - min y`.
     */
    [[nodiscard]] constexpr auto height() const {
        return max().y() - min().y();
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
        return CornerIterator(this, 4);
    }

    /**
     * @brief Returns an iterator past the last corner.
     */
    constexpr CornerIterator cend() const {
        return CornerIterator(this, 4);
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
        return EdgeIterator(this, edgeCount);
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
        return OrientedEdgeIterator(this, edgeCount);
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
     * @return `width * height`.
     */
    [[nodiscard]] constexpr auto area() const;

    /**
     * @brief Returns twice the rectangle area.
     *
     * @return `2 * area`.
     */
    [[nodiscard]] constexpr auto twiceArea() const;

    /**
     * @brief Returns whether the rectangle has empty interior.
     *
     * @return `true` when width or height is zero.
     */
    [[nodiscard]] constexpr bool isDegenerate() const;

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
     * @return A rectangle that contains the rectangle.
     */
    template <std::floating_point ResultNumber = double>
    [[nodiscard]] constexpr Rectangle<Point<ResultNumber>> fbox() const;

    /**
     * @brief Returns the four vertices in counterclockwise order.
     *
     * The first vertex is the minimum corner.
     *
     * @return Vertices `(xmin,ymin)`, `(xmax,ymin)`, `(xmax,ymax)`, `(xmin,ymax)`.
     */
    [[nodiscard]] constexpr std::array<PointType, 4> vertices() const;

    /**
     * @brief Returns the four edges as unordered segments.
     *
     * @return Bottom, right, top, and left edges.
     */
    [[nodiscard]] constexpr std::array<Segment<PointType>, 4> edges() const;

    /**
     * @brief Returns the four boundary edges in counterclockwise order.
     *
     * @return Bottom, right, top, and left oriented edges.
     */
    [[nodiscard]] constexpr std::array<OrientedSegment<PointType>, 4> orientedEdges() const;

    /**
     * @brief Converts the rectangle to a convex polygon.
     *
     * The four corners already follow the canonical convex order
     * (counterclockwise, lexicographically smallest first), and degenerate
     * rectangles collapse to their hull.
     *
     * @return Convex polygon with the same corners.
     */
    [[nodiscard]] constexpr explicit operator Convex<PointType>() const {
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
     * @brief Converts the rectangle to a simple polygon.
     *
     * The four corners already follow the canonical polygon order
     * (counterclockwise, lexicographically smallest first).
     *
     * @return Polygon with the same corners.
     */
    [[nodiscard]] constexpr explicit operator Polygon<PointType>() const {
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
    template<PolygonConcept OtherPolygon>
    [[nodiscard]] constexpr bool separates(const OtherPolygon& other) const;

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
    template <class ResultNumber = NumberType, LineConcept OtherLine>
    [[nodiscard]] constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherLine& other) const;

    /** @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint. @warning Divides coordinates after casting to ResultNumber. */
    template <class ResultNumber = NumberType, OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherOrientedLine& other) const;

    /** @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint. @warning Divides coordinates after casting to ResultNumber. */
    template <class ResultNumber = NumberType, SegmentConcept OtherSegment>
    [[nodiscard]] constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherSegment& other) const;

    /** @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint. @warning Divides coordinates after casting to ResultNumber. */
    template <class ResultNumber = NumberType, OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherOrientedSegment& other) const;

    /** @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint. @warning Divides coordinates after casting to ResultNumber. */
    template <class ResultNumber = NumberType, RayConcept OtherRay>
    [[nodiscard]] constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherRay& other) const;

    /** @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint. @warning Divides coordinates after casting to ResultNumber. */
    template <class ResultNumber = NumberType, HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr auto intersection(const OtherHalfplane& other) const;

    /** @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint. @warning Divides coordinates after casting to ResultNumber. */
    template <class ResultNumber = NumberType, typename OtherShape>
        requires (!PointConcept<OtherShape>
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
     * @brief Returns the squared Euclidean distance to a point.
     *
     * The closest point of an axis-aligned rectangle has integer coordinate
     * gaps, so this overload involves no division and is exact.
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
     * @tparam ResultNumber Coordinate type of the returned distance (default: NumberType).
     *
     * @warning With an integer @p ResultNumber the exact squared distance is
     *          generally a fraction, so the internal division truncates and the
     *          result is inexact. Request a floating-point or pgl::Rational
     *          result type, e.g. `squaredDistance<double>(other)`, for an
     *          accurate value.
     */
    template <class ResultNumber = NumberType, LineConcept OtherLine>
    [[nodiscard]] constexpr auto squaredDistance(const OtherLine& other) const;

    /** @copydoc squaredDistance(const OtherLine&) const */
    template <class ResultNumber = NumberType, OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr auto squaredDistance(const OtherOrientedLine& other) const;

    /** @copydoc squaredDistance(const OtherLine&) const */
    template <class ResultNumber = NumberType, SegmentConcept OtherSegment>
    [[nodiscard]] constexpr auto squaredDistance(const OtherSegment& other) const;

    /** @copydoc squaredDistance(const OtherLine&) const */
    template <class ResultNumber = NumberType, OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr auto squaredDistance(const OtherOrientedSegment& other) const;

    /** @copydoc squaredDistance(const OtherLine&) const */
    template <class ResultNumber = NumberType, RayConcept OtherRay>
    [[nodiscard]] constexpr auto squaredDistance(const OtherRay& other) const;

    /** @copydoc squaredDistance(const OtherLine&) const */
    template <class ResultNumber = NumberType, HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr auto squaredDistance(const OtherHalfplane& other) const;

    /**
     * @brief Returns the squared Euclidean distance to another rectangle.
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
     * @brief Returns the squared Euclidean distance to a higher-ranked shape.
     *
     * Forwards to the other shape's implementation so that each unordered pair
     * needs `squaredDistance` defined only once, on the higher-ranked shape.
     */
    template <class ResultNumber = NumberType, typename OtherShape>
        requires ((detail::shapeRank<OtherShape> > detail::shapeRank<Rectangle>)
                  && requires(const OtherShape& o, const Rectangle& self) {
                         o.template squaredDistance<ResultNumber>(self);
                     })
    [[nodiscard]] constexpr auto squaredDistance(const OtherShape& other) const {
        return other.template squaredDistance<ResultNumber>(*this);
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
     * @brief Enlarges the rectangle so that it contains the given point.
     *
     * Existing corner labels are preserved when their coordinates do not
     * change. Newly synthesized corners use default-constructed labels.
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
     * accepted by this overload.
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
    constexpr void insert(Range&& range);

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
    constexpr void insert(Range&& range);

    /**
     * @brief Returns a segment defining a diameter.
     *
     * For a rectangle, a diagonal is a diameter.
     *
     * @return The diagonal from `min()` to `max()`.
     */
    [[nodiscard]] constexpr Segment<PointType> diameter() const;

    /**
     * @brief Returns the midpoint of the rectangle.
     *
     * @tparam ResultNumber Coordinate type of the midpoint.
     * @return Midpoint with no label.
     * @warning Divides coordinates by 2. Inexact for odd integer coordinates.
     */
    template <class ResultNumber = NumberType>
    [[nodiscard]] constexpr Point<ResultNumber> midpoint() const;

    /**
     * @brief Returns the centroid of the rectangle.
     *
     * @tparam ResultNumber Coordinate type of the centroid.
     * @return The rectangle midpoint.
     * @warning Divides coordinates by 2. Inexact for odd integer coordinates.
     */
    template <class ResultNumber = NumberType>
    [[nodiscard]] constexpr Point<ResultNumber> centroid() const;

    /**
     * @brief Returns the circumcircle of the rectangle.
     *
     * The returned disk passes through the rectangle corners.
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
    template <class ResultNumber = NumberType>
    [[nodiscard]] constexpr Point<ResultNumber> center() const;

    /**
     * @brief Returns a point inside the rectangle.
     *
     * This is the midpoint, even for degenerate rectangles.
     *
     * @tparam ResultNumber Coordinate type of the midpoint.
     * @return Midpoint with the rectangle coordinate type.
     * @warning Divides coordinates by 2. Inexact for odd integer coordinates.
     */
    template <class ResultNumber = NumberType>
    [[nodiscard]] constexpr Point<ResultNumber> pointInside() const;

    /**
     * @brief Translates both stored corners in place.
     *
     * @tparam OtherNumber Coordinate type of the translation point.
     * @tparam OtherPoint::LabelType Label type of the translation point.
     * @param translation Translation vector.
     * @return Reference to this rectangle.
     */
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
        requires(!detail::is_point_v<Scalar>)
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
        requires(!detail::is_point_v<Scalar>)
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

    template <class ResultNumber = NumberType, RectangleConcept OtherRectangle>
    constexpr auto maxVertexDistanceTo(const OtherRectangle& other) const;

    std::array<PointType, 2> points_{};
    [[no_unique_address]] mutable LabelType label_{};
};

/**
 * @brief Translates a rectangle by a point.
 *
 * @tparam NumberType Coordinate type of the rectangle corners.
 * @tparam Label Label type of the rectangle corners.
 * @tparam TranslationNumber Coordinate type of the translation point.
 * @tparam TranslationLabel Label type of the translation point.
 * @param rectangle Rectangle to translate.
 * @param translation Translation vector.
 * @return Translated rectangle.
 */
template <class PointType, class LabelType, class TranslationNumber, class TranslationLabel>
constexpr auto operator+(const Rectangle<PointType, LabelType>& rectangle, const Point<TranslationNumber, TranslationLabel>& translation);

/**
 * @brief Translates a rectangle by a point written on the left.
 *
 * @tparam TranslationNumber Coordinate type of the translation point.
 * @tparam TranslationLabel Label type of the translation point.
 * @tparam NumberType Coordinate type of the rectangle corners.
 * @tparam Label Label type of the rectangle corners.
 * @param translation Translation vector.
 * @param rectangle Rectangle to translate.
 * @return Translated rectangle.
 */
template <class TranslationNumber, class TranslationLabel, class PointType, class LabelType>
constexpr auto operator+(const Point<TranslationNumber, TranslationLabel>& translation, const Rectangle<PointType, LabelType>& rectangle);

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
    requires(!detail::is_point_v<Scalar>)
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
    requires(!detail::is_point_v<Scalar>)
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
    requires(!detail::is_point_v<Scalar>)
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
