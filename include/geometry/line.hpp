#pragma once

#ifndef PGL_HPP_INCLUDED
#error "Do not include this Pangolin header directly; include \"pgl.hpp\" instead."
#endif

/**
 * @file line.hpp
 * @brief Public declaration of pgl::Line.
 *
 * Line is the unoriented infinite 1D primitive used by predicates, duality, and
 * intersection routines throughout the library.
 */

#include <array>
#include <cassert>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <functional>
#include <optional>
#include <ostream>
#include <type_traits>
#include <utility>
#include <variant>


namespace pgl {

template <class PointType = Point<>>
struct Line;

Line() -> Line<Point<>>;

template <class PointType>
Line(PointType, PointType) -> Line<PointType>;

template <class Number>
Line(Number, Number, Number, Number) -> Line<Point<Number>>;

/**
 * @brief Infinite unoriented straight line.
 *
 * Two non-degenerate lines compare equal whenever they contain exactly the
 * same points, regardless of which two defining points were used to build
 * them. The stored defining points are kept in increasing lexicographic order.
 *
 * @tparam PointType Defining point type.
 */
template <class PointType_>
struct Line {
    using PointType = PointType_;
    using NumberType = PointType::NumberType;
    // using LabelType = detail::point_label_t<PointType>;
    using CoordinateType = detail::promoted_number_t<NumberType>;

    static_assert(detail::is_point_v<PointType>, "Line requires pgl::Point defining points");

    /**
     * @brief Creates the degenerate line `(0,0)--(0,0)`.
     */
    constexpr Line() = default;

    /**
     * @brief Creates a line from two defining points.
     *
     * The stored points are reordered so that `min() <= max()`.
     *
     * @param first First defining point.
     * @param second Second defining point.
     */
    constexpr Line(PointType first, PointType second) {
        if (second < first) {
            std::swap(first, second);
        }
        points_[0] = std::move(first);
        points_[1] = std::move(second);
    }

    /**
     * @brief Creates a line from four coordinates.
     *
     * @param x1 X coordinate of the first defining point.
     * @param y1 Y coordinate of the first defining point.
     * @param x2 X coordinate of the second defining point.
     * @param y2 Y coordinate of the second defining point.
     */
    constexpr Line(NumberType x1, NumberType y1, NumberType x2, NumberType y2)
        : Line(PointType(x1, y1), PointType(x2, y2)) {}

    template<PointConcept OtherPointType>
        requires(std::constructible_from<PointType, const OtherPointType&>)
    constexpr Line(const Line<OtherPointType>& other)
        : Line(PointType(other.min()), PointType(other.max())) {}

    template<PointConcept OtherPointType>
        requires(std::constructible_from<PointType, const OtherPointType&>)
    constexpr Line& operator=(const Line<OtherPointType>& other) {
        points_[0] = PointType(other.min());
        points_[1] = PointType(other.max());
        return *this;
    }

    /**
     * @brief Returns defining point `0` or `1`.
     *
     * @param index Defining-point index.
     * @return Reference to the selected defining point.
     */
    constexpr const PointType& operator[](std::size_t index) const {
        assert(index < size());
        return points_[index];
    }

    /**
     * @brief Returns the number of defining points (always 2).
     */
    static constexpr std::size_t size() {
        return 2;
    }

    /**
     * @brief Cyclic access: same as @ref operator[] but `index` is taken
     * modulo @ref size(); negative indices wrap from the end.
     */
    constexpr const PointType& get(std::ptrdiff_t index) const {
        const std::ptrdiff_t n = static_cast<std::ptrdiff_t>(size());
        return (*this)[static_cast<std::size_t>(((index % n) + n) % n)];
    }

    /**
     * @brief Returns the smallest index `i` with `(*this)[i] == point`, or
     * `-1` if no defining point equals `point`.
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
     * @brief Returns the smallest stored defining point.
     *
     * @return Reference to the first defining point.
     */
    constexpr const PointType& min() const {
        return points_[0];
    }

    /**
     * @brief Returns the largest stored defining point.
     *
     * @return Reference to the second defining point.
     */
    constexpr const PointType& max() const {
        return points_[1];
    }

    /**
     * @brief Returns an iterator to the first defining point.
     *
     * @return Pointer to the first defining point.
     */
    constexpr auto begin() const {
        return points_.cbegin();
    }

    /**
     * @brief Returns an iterator to the first defining point.
     *
     * @return Pointer to the first defining point.
     */
    constexpr auto cbegin() const {
        return points_.cbegin();
    }

    /**
     * @brief Returns an iterator past the last defining point.
     *
     * @return Pointer past the second defining point.
     */
    constexpr auto end() const {
        return points_.cend();
    }

    /**
     * @brief Returns an iterator past the last defining point.
     *
     * @return Pointer past the second defining point.
     */
    constexpr auto cend() const {
        return points_.cend();
    }

    /**
     * @brief Tests geometric equality of two lines.
     *
     * Lines compare using the contains predicate.
     *
     * @param other Line to compare with.
     * @return `true` if both lines represent the same geometric set.
     */
    [[nodiscard]] constexpr bool operator==(const Line& other) const;

    /**
     * @brief Provides an ordering compatible with geometric equality.
     *
     * @param other Line to compare with.
     * @return Comparison result.
     * @warning Coordinates are cubed but a single promotion is used.
     */
    [[nodiscard]] constexpr auto operator<=>(const Line& other) const;

    /**
     * @brief Returns the dual point.
     *
     * @tparam ResultNumber Coordinate type of the returned point.
     * @return Point (a,b) such that the line is y = ax - b.
     * @warning Uses division.
     */
    template<class ResultNumber=NumberType>
    [[nodiscard]] constexpr Point<ResultNumber, typename PointType::LabelType> dual() const;

    /**
     * @brief Returns the polar point.
     *
     * @tparam ResultNumber Coordinate type of the returned point.
     * @return Point (a,b) such that the line is ax + by = 1.
     * @warning Uses division.
     */
    template<class ResultNumber=NumberType>
    [[nodiscard]] constexpr Point<ResultNumber, typename PointType::LabelType> polar() const;

    /**
     * @brief Returns the area of the line.
     *
     * A line is one-dimensional, so its area is always zero.
     *
     * @return Zero.
     */
    [[nodiscard]] constexpr NumberType area() const;

    /**
     * @brief Returns twice the area of the line.
     *
     * A line is one-dimensional, so this is always zero.
     *
     * @return Zero.
     */
    [[nodiscard]] constexpr NumberType twiceArea() const;

    /**
     * @brief Returns the line rotated by 90k degrees around the origin.
     *
     * @param k Number of 90-degree CCW rotations (may be negative).
     * @return Rotated line.
     */
    [[nodiscard]] constexpr Line rotated90(int k = 1) const;

    /**
     * @brief Rotates the line by 90k degrees around the origin in place.
     *
     * @param k Number of 90-degree CCW rotations (may be negative).
     */
    constexpr void rotate90(int k = 1);

    /** @brief Returns the line with its x-coordinates multiplied by a factor. */
    template <class OtherNumber>
    [[nodiscard]] constexpr Line scaledUpX(const OtherNumber scalar) const;

    /** @brief Multiplies the line's x-coordinates by a factor in place. */
    template <class OtherNumber>
    constexpr void scaleUpX(const OtherNumber scalar);

    /** @brief Returns the line with its y-coordinates multiplied by a factor. */
    template <class OtherNumber>
    [[nodiscard]] constexpr Line scaledUpY(const OtherNumber scalar) const;

    /** @brief Multiplies the line's y-coordinates by a factor in place. */
    template <class OtherNumber>
    constexpr void scaleUpY(const OtherNumber scalar);

    /** @brief Returns the line with its x-coordinates divided by a divisor. */
    template <class OtherNumber>
    [[nodiscard]] constexpr Line scaledDownX(const OtherNumber scalar) const;

    /** @brief Divides the line's x-coordinates by a divisor in place. */
    template <class OtherNumber>
    constexpr void scaleDownX(const OtherNumber scalar);

    /** @brief Returns the line with its y-coordinates divided by a divisor. */
    template <class OtherNumber>
    [[nodiscard]] constexpr Line scaledDownY(const OtherNumber scalar) const;

    /** @brief Divides the line's y-coordinates by a divisor in place. */
    template <class OtherNumber>
    constexpr void scaleDownY(const OtherNumber scalar);

    /**
     * @brief Returns whether the defining points coincide.
     *
     * @return `true` if both defining points are equal.
     */
    [[nodiscard]] constexpr bool isDegenerate() const;

    /**
     * @brief Returns whether the line is vertical.
     *
     * @return `true` if both defining points have the same x-coordinate.
     */
    [[nodiscard]] constexpr bool isVertical() const;

    /**
     * @brief Returns whether the line is horizontal.
     *
     * @return `true` if both defining points have the same y-coordinate.
     */
    [[nodiscard]] constexpr bool isHorizontal() const;

    /**
     * @brief Returns whether the given point is one of the stored defining points.
     *
     * @tparam OtherPoint Type of the point.
     *
     * @param point Point to test.
     * @return `true` if the point equals one stored defining point.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool verticesContain(const OtherPoint& point) const;

    /**
     * @brief Returns whether the line boundary contains the given point.
     *
     * The interior of a line is the whole line, so its boundary is empty.
     *
     * @tparam OtherPoint Type of the point.
     *
     * @param point Point to test.
     * @return Always `false`.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool boundaryContains(const OtherPoint& point) const;

    // A line has empty boundary, so it boundary-contains no non-point shape.
    template<PointConcept OtherPoint, class OtherLabel>
    [[nodiscard]] constexpr bool boundaryContains(const Segment<OtherPoint, OtherLabel>&) const { return false; }
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool boundaryContains(const OrientedSegment<OtherPoint>&) const { return false; }
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool boundaryContains(const Line<OtherPoint>&) const { return false; }
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool boundaryContains(const OrientedLine<OtherPoint>&) const { return false; }
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool boundaryContains(const Ray<OtherPoint>&) const { return false; }
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool boundaryContains(const Halfplane<OtherPoint>&) const { return false; }
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool boundaryContains(const Rectangle<OtherPoint>&) const { return false; }
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool boundaryContains(const Triangle<OtherPoint>&) const { return false; }
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool boundaryContains(const Convex<OtherPoint>&) const { return false; }
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool boundaryContains(const Polygon<OtherPoint>&) const { return false; }
    template<PointConcept OtherPoint, class OtherLabel>
    [[nodiscard]] constexpr bool boundaryContains(const Disk<OtherPoint, OtherLabel>&) const { return false; }

    /**
     * @brief Returns whether the line contains the given point.
     *
     * For degenerate lines, this reduces to point equality with the stored
     * defining point.
     *
     * @tparam OtherPoint Type of the point.
     *
     * @param point Point to test.
     * @return `true` if the point lies on the line.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool contains(const OtherPoint& point) const;

    /**
     * @brief Returns whether the line contains another line.
     *
     * @tparam OtherNumber Coordinate type of the other line defining points.
     * @tparam OtherPoint::LabelType Label type of the other line defining points.
     * @param other Other line.
     * @return `true` if both lines are equal.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool contains(const Line<OtherPoint>& other) const;

    /**
     * @brief Returns whether the line contains a segment.
     *
     * @tparam OtherNumber Coordinate type of the segment endpoints.
     * @tparam OtherPoint::LabelType Label type of the segment endpoints.
     * @param other Other segment.
     * @return `true` if both endpoints of `other` lie on the line.
     */
    template<PointConcept OtherPoint, class OtherLabel>
    [[nodiscard]] constexpr bool contains(const Segment<OtherPoint, OtherLabel>& other) const;

    /**
     * @brief Returns whether the line contains an oriented segment.
     *
     * @tparam OtherNumber Coordinate type of the segment endpoints.
     * @tparam OtherPoint::LabelType Label type of the segment endpoints.
     * @param other Other oriented segment.
     * @return `true` if both endpoints of `other` lie on the line.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool contains(const OrientedSegment<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool contains(const OrientedLine<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool contains(const Ray<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool contains(const Halfplane<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool contains(const Rectangle<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool contains(const Triangle<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool contains(const Convex<OtherPoint>& other) const;

    /** @brief Polygon overload; see the Convex overload. */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool contains(const Polygon<OtherPoint>& other) const;

    [[nodiscard]] constexpr bool contains(const Shape<PointType>& other) const;

    [[nodiscard]] constexpr bool boundaryContains(const Shape<PointType>& other) const;

    // The empty set is a subset of every shape (contained in all of them) and
    // disjoint from all of them, so containment is true while separation is
    // false. These overloads let an EmptyShape flow through Shape's variant
    // dispatch without special-casing.
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool contains(const EmptyShape<EmptyPoint>&) const {
        return true;
    }
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool boundaryContains(const EmptyShape<EmptyPoint>&) const {
        return true;
    }
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool interiorContains(const EmptyShape<EmptyPoint>&) const {
        return true;
    }
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool separates(const EmptyShape<EmptyPoint>&) const {
        return false;
    }

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorContains(const OtherPoint& point) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorContains(const Line<OtherPoint>& other) const;

    template<PointConcept OtherPoint, class OtherLabel>
    [[nodiscard]] constexpr bool interiorContains(const Segment<OtherPoint, OtherLabel>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorContains(const OrientedSegment<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorContains(const OrientedLine<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorContains(const Ray<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorContains(const Halfplane<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorContains(const Rectangle<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorContains(const Triangle<OtherPoint>& other) const;

    /**
     * @brief Returns whether the line interior contains the given point.
     * @brief Returns whether the given point is collinear with the line.
     *
     * @tparam OtherPoint Type of the point.
     *
     * @param point Point to test.
     * @return `true` if the point lies on the supporting line.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool collinear(const OtherPoint& point) const;

    /**
     * @brief Returns whether the given segment is collinear with the line.
     *
     * @tparam OtherNumber Coordinate type of the segment endpoints.
     * @tparam OtherPoint::LabelType Label type of the segment endpoints.
     * @param other Other segment.
     * @return `true` if the segment lies on the line.
     */
    template<PointConcept OtherPoint, class OtherLabel>
    [[nodiscard]] constexpr bool collinear(const Segment<OtherPoint, OtherLabel>& other) const;

    /**
     * @brief Returns whether the given oriented segment is collinear with the line.
     *
     * @tparam OtherNumber Coordinate type of the segment endpoints.
     * @tparam OtherPoint::LabelType Label type of the segment endpoints.
     * @param other Other oriented segment.
     * @return `true` if the segment lies on the line.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool collinear(const OrientedSegment<OtherPoint>& other) const;

    /**
     * @brief Returns whether another line is collinear with this line.
     *
     * @tparam OtherNumber Coordinate type of the other line defining points.
     * @tparam OtherPoint::LabelType Label type of the other line defining points.
     * @param other Other line.
     * @return `true` if the two lines coincide.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool collinear(const Line<OtherPoint>& other) const;

    /**
     * @brief Returns whether an oriented line is collinear with this line.
     *
     * @tparam OtherPoint Defining-point type of the other oriented line.
     * @param other Other oriented line.
     * @return `true` if both lines coincide as point sets.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool collinear(const OrientedLine<OtherPoint>& other) const;

    /**
     * @brief Returns whether a ray is collinear with this line.
     *
     * @tparam OtherPoint Defining-point type of the ray.
     * @param other Ray to test.
     * @return `true` if the ray lies on the supporting line.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool collinear(const Ray<OtherPoint>& other) const;

    /**
     * @brief Returns the absolute slope of the line.
     *
     * Undefined behavior for vertical lines.
     *
     * @tparam ResultNumber Coordinate type of the returned slope.
     * @return Absolute slope.
     * @warning Uses division of coordinates.
     */
    template <class ResultNumber = NumberType>
    [[nodiscard]] constexpr ResultNumber slope() const;

    /**
     * @brief Returns the y-coordinate of the line at a given x-coordinate, if defined.
     *
     * For vertical lines queried at their x-coordinate, the stored `min().y()`
     * is returned to keep the API deterministic.
     *
     * @tparam ResultNumber Coordinate type of the returned value.
     * @tparam OtherNumber Coordinate type of the query x-coordinate.
     * @param x Query x-coordinate.
     * @return Corresponding y-coordinate, or empty when the line is vertical at a different x.
     * @warning Uses division after casting to ResultNumber.
     */
    template <class ResultNumber = NumberType, class OtherNumber>
    [[nodiscard]] constexpr std::optional<ResultNumber> yAtX(const OtherNumber& x) const;

    /**
     * @brief Returns the x-coordinate of the line at a given y-coordinate, if defined.
     *
     * For horizontal lines queried at their y-coordinate, the stored `min().x()`
     * is returned to keep the API deterministic.
     *
     * @tparam ResultNumber Coordinate type of the returned value.
     * @tparam OtherNumber Coordinate type of the query y-coordinate.
     * @param y Query y-coordinate.
     * @return Corresponding x-coordinate, or empty when the line is horizontal at a different y.
     * @warning Uses division after casting to ResultNumber.
     */
    template <class ResultNumber = NumberType, class OtherNumber>
    [[nodiscard]] constexpr std::optional<ResultNumber> xAtY(const OtherNumber& y) const;

    /**
     * @brief Returns the half-plane geometrically above this line.
     *
     * For vertical lines, this is the half-plane with smaller x-coordinate.
     *
     * @return Closed half-plane above the line.
     */
    [[nodiscard]] constexpr Halfplane<PointType> halfplaneAbove() const;

    /**
     * @brief Returns the half-plane geometrically below this line.
     *
     * For vertical lines, this is the half-plane with larger x-coordinate.
     *
     * @return Closed half-plane below the line.
     */
    [[nodiscard]] constexpr Halfplane<PointType> halfplaneBelow() const;

    /**
     * @brief Returns whether another line is parallel to this line.
     *
     * @tparam OtherNumber Coordinate type of the other line defining points.
     * @tparam OtherPoint::LabelType Label type of the other line defining points.
     * @param other Other line.
     * @return `true` if both directions are proportional.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool parallel(const Line<OtherPoint>& other) const;

    /**
     * @brief Returns whether a segment is parallel to this line.
     *
     * @tparam OtherNumber Coordinate type of the segment endpoints.
     * @tparam OtherPoint::LabelType Label type of the segment endpoints.
     * @param other Other segment.
     * @return `true` if both directions are proportional.
     */
    template<PointConcept OtherPoint, class OtherLabel>
    [[nodiscard]] constexpr bool parallel(const Segment<OtherPoint, OtherLabel>& other) const;

    /**
     * @brief Returns whether an oriented segment is parallel to this line.
     *
     * @tparam OtherNumber Coordinate type of the segment endpoints.
     * @tparam OtherPoint::LabelType Label type of the segment endpoints.
     * @param other Other oriented segment.
     * @return `true` if both directions are proportional.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool parallel(const OrientedSegment<OtherPoint>& other) const;

    /**
     * @brief Returns whether an oriented line is parallel to this line.
     *
     * @tparam OtherPoint Defining-point type of the other oriented line.
     * @param other Other oriented line.
     * @return `true` if both directions are proportional.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool parallel(const OrientedLine<OtherPoint>& other) const;

    /**
     * @brief Returns whether a ray is parallel to this line.
     *
     * @tparam OtherPoint Defining-point type of the ray.
     * @param other Ray to test.
     * @return `true` if both directions are proportional.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool parallel(const Ray<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool intersects(const OtherPoint& other) const;

    /**
     * @brief Returns whether two lines intersect.
     *
     * @tparam OtherNumber Coordinate type of the other line defining points.
     * @tparam OtherPoint::LabelType Label type of the other line defining points.
     * @param other Other line.
     * @return `true` if the lines share at least one point.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool intersects(const Line<OtherPoint>& other) const;

    template<PointConcept OtherPoint, class OtherLabel>
    [[nodiscard]] constexpr bool intersects(const Segment<OtherPoint, OtherLabel>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool intersects(const OrientedSegment<OtherPoint>& other) const;

    [[nodiscard]] constexpr bool intersects(const Shape<PointType>& other) const;

    template<typename OtherShape>
        requires (!PointConcept<OtherShape> && detail::shapeRank<OtherShape> > detail::shapeRank<Line>)
    [[nodiscard]] constexpr bool intersects(const OtherShape& other) const {
        return other.intersects(*this);
    }

    /** @brief The empty set never meets another shape. */
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool intersects(const EmptyShape<EmptyPoint>&) const {
        return false;
    }

    /**
     * @brief Returns whether the interiors of two lines intersect.
     *
     * For lines, this is the same as @ref intersects.
     *
     * @tparam OtherNumber Coordinate type of the other line defining points.
     * @tparam OtherPoint::LabelType Label type of the other line defining points.
     * @param other Other line.
     * @return `true` if the lines share at least one point.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherPoint& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorsIntersect(const Line<OtherPoint>& other) const;

    template<PointConcept OtherPoint, class OtherLabel>
    [[nodiscard]] constexpr bool interiorsIntersect(const Segment<OtherPoint, OtherLabel>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorsIntersect(const OrientedSegment<OtherPoint>& other) const;

    template<typename OtherShape>
        requires (!PointConcept<OtherShape> && detail::shapeRank<OtherShape> > detail::shapeRank<Line>)
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherShape& other) const {
        return other.interiorsIntersect(*this);
    }

    /** @brief The empty set never meets another shape. */
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool interiorsIntersect(const EmptyShape<EmptyPoint>&) const {
        return false;
    }

    [[nodiscard]] constexpr bool interiorsIntersect(const Shape<PointType>& other) const;

    /**
     * @brief Returns whether two lines cross at a unique point.
     *
     * @tparam OtherNumber Coordinate type of the other line defining points.
     * @tparam OtherPoint::LabelType Label type of the other line defining points.
     * @param other Other line.
     * @return `true` if the lines are distinct and not parallel.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool crosses(const Line<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool crosses(const OtherPoint& other) const;

    template<PointConcept OtherPoint, class OtherLabel>
    [[nodiscard]] constexpr bool crosses(const Segment<OtherPoint, OtherLabel>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool crosses(const OrientedSegment<OtherPoint>& other) const;

    template<typename OtherShape>
        requires (!PointConcept<OtherShape> && detail::shapeRank<OtherShape> > detail::shapeRank<Line>)
    [[nodiscard]] constexpr bool crosses(const OtherShape& other) const {
        return other.crosses(*this);
    }

    /** @brief The empty set never meets another shape. */
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool crosses(const EmptyShape<EmptyPoint>&) const {
        return false;
    }

    [[nodiscard]] constexpr bool crosses(const Shape<PointType>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool separates(const OtherPoint& other) const;

    template<PointConcept OtherPoint, class OtherLabel>
    [[nodiscard]] constexpr bool separates(const Segment<OtherPoint, OtherLabel>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool separates(const OrientedSegment<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool separates(const Line<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool separates(const OrientedLine<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool separates(const Ray<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool separates(const Rectangle<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool separates(const Triangle<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool separates(const Halfplane<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool separates(const Convex<OtherPoint>& other) const;

    /** @brief Polygon overload; see the Convex overload. */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool separates(const Polygon<OtherPoint>& other) const;

    [[nodiscard]] constexpr bool separates(const Shape<PointType>& other) const;

    // --- not-yet-implemented predicate pairs (throw); see implementation ---
    template<PointConcept OtherPoint, class OtherLabel>
    [[nodiscard]] constexpr bool interiorContains(const Disk<OtherPoint, OtherLabel>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorContains(const Convex<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorContains(const Polygon<OtherPoint>& other) const;


    /**
     * @brief Returns the intersection of two lines.
     *
     * The intersection may be empty, a point, or the entire line.
     *
     * @tparam ResultNumber Coordinate type of the returned point or line.
     * @tparam OtherNumber Coordinate type of the other line defining points.
     * @tparam OtherPoint::LabelType Label type of the other line defining points.
     * @param other Other line.
     * @return Empty if disjoint, otherwise a point or a line.
     */
    template <class ResultNumber = NumberType, PointConcept OtherPoint>
    [[nodiscard]] constexpr std::optional<Point<ResultNumber, typename PointType::LabelType>>
    intersection(const OtherPoint& other) const;

    template <class ResultNumber = NumberType, PointConcept OtherPoint>
    [[nodiscard]] constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>,Line<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const Line<OtherPoint>& other) const;

    template <class ResultNumber = NumberType, PointConcept OtherPoint, class OtherLabel>
    [[nodiscard]] constexpr auto intersection(const Segment<OtherPoint, OtherLabel>& other) const;

    template <class ResultNumber = NumberType, PointConcept OtherPoint>
    [[nodiscard]] constexpr auto intersection(const OrientedSegment<OtherPoint>& other) const;

    template <class ResultNumber = NumberType, typename OtherShape>
        requires (!PointConcept<OtherShape>
                  && (detail::shapeRank<OtherShape> > detail::shapeRank<Line>)
                  && requires(const OtherShape& o, const Line& self) {
                         o.template intersection<ResultNumber>(self);
                     })
    [[nodiscard]] constexpr auto intersection(const OtherShape& other) const {
        return other.template intersection<ResultNumber>(*this);
    }

    /** @brief Intersecting with the empty set yields the empty set. */
    template <class ResultNumber = NumberType, class EmptyPoint>
    [[nodiscard]] constexpr EmptyShape<EmptyPoint> intersection(const EmptyShape<EmptyPoint>&) const {
        return {};
    }

    /**
     * @brief Returns the squared Euclidean distance to a point.
     *
     * @tparam OtherPoint Type of the point.
     *
     * @param point Point to measure from.
     * @return Squared Euclidean distance.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr auto squaredDistance(const OtherPoint& point) const;

    /**
     * @brief Returns the squared Euclidean distance to another line.
     *
     * @tparam OtherNumber Coordinate type of the other line defining points.
     * @tparam OtherPoint::LabelType Label type of the other line defining points.
     * @param other Other line.
     * @return Squared Euclidean distance.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr auto squaredDistance(const Line<OtherPoint>& other) const;

    /**
     * @brief Translates both defining points in place.
     *
     * @tparam OtherNumber Coordinate type of the translation point.
     * @tparam OtherPoint::LabelType Label type of the translation point.
     * @param translation Translation vector.
     * @return Reference to this line.
     */
    template<PointConcept OtherPoint>
    constexpr Line& operator+=(const OtherPoint& translation);

    /**
     * @brief Translates both defining points by the opposite vector in place.
     *
     * @tparam OtherNumber Coordinate type of the translation point.
     * @tparam OtherPoint::LabelType Label type of the translation point.
     * @param translation Translation vector to subtract.
     * @return Reference to this line.
     */
    template<PointConcept OtherPoint>
    constexpr Line& operator-=(const OtherPoint& translation);

    /**
     * @brief Scales the line around the origin in place.
     *
     * @tparam Scalar Scalar type.
     * @param scalar Scale factor.
     * @return Reference to this line.
     */
    template <class Scalar>
        requires(!detail::is_point_v<Scalar>)
    constexpr Line& operator*=(const Scalar& scalar);

    /**
     * @brief Divides the line coordinates by a scalar in place.
     *
     * @tparam Scalar Scalar type.
     * @param scalar Divisor.
     * @return Reference to this line.
     */
    template <class Scalar>
        requires(!detail::is_point_v<Scalar>)
    constexpr Line& operator/=(const Scalar& scalar);

    /**
     * @brief Returns a point inside the line.
     *
     * For a line, this is the min point.
     *
     * @return Returns min()
     */
    template <class ResultNumber = NumberType>
    [[nodiscard]] constexpr Point<ResultNumber> pointInside() const;


    template<class ResultNumber>
    [[nodiscard]] constexpr auto dualCoordinates() const;

  private:
    template <class OtherNumber>
    using promoted_number_t = std::common_type_t<CoordinateType, detail::promoted_number_t<OtherNumber>>;

    template<class ResultNumber>
    [[nodiscard]] constexpr auto polarCoordinates() const;

    std::array<PointType, 2> points_{};
};

/**
 * @brief Translates a line by a point.
 *
 * @tparam PointType Defining point type of the line.
 * @tparam TranslationNumber Coordinate type of the translation point.
 * @tparam TranslationLabel Label type of the translation point.
 * @param line Line to translate.
 * @param translation Translation vector.
 * @return Translated line.
 */
template <class PointType, class TranslationNumber, class TranslationLabel>
constexpr auto operator+(const Line<PointType>& line, const Point<TranslationNumber, TranslationLabel>& translation);

/**
 * @brief Translates a line by a point written on the left.
 *
 * @tparam TranslationNumber Coordinate type of the translation point.
 * @tparam TranslationLabel Label type of the translation point.
 * @tparam PointType Defining point type of the line.
 * @param translation Translation vector.
 * @param line Line to translate.
 * @return Translated line.
 */
template <class TranslationNumber, class TranslationLabel, class PointType>
constexpr auto operator+(const Point<TranslationNumber, TranslationLabel>& translation, const Line<PointType>& line);

/**
 * @brief Translates a line by the opposite of a point.
 *
 * @tparam PointType Defining point type of the line.
 * @tparam TranslationNumber Coordinate type of the translation point.
 * @tparam TranslationLabel Label type of the translation point.
 * @param line Line to translate.
 * @param translation Translation vector to subtract.
 * @return Translated line.
 */
template <class PointType, class TranslationNumber, class TranslationLabel>
constexpr auto operator-(const Line<PointType>& line, const Point<TranslationNumber, TranslationLabel>& translation);

/**
 * @brief Scales a line by a scalar.
 *
 * @tparam PointType Defining point type of the line.
 * @tparam Scalar Scalar type.
 * @param line Line to scale.
 * @param scalar Scale factor.
 * @return Scaled line.
 */
template <class PointType, class Scalar>
    requires(!detail::is_point_v<Scalar>)
constexpr auto operator*(const Line<PointType>& line, const Scalar& scalar);

/**
 * @brief Scales a line by a scalar written on the left.
 *
 * @tparam Scalar Scalar type.
 * @tparam PointType Defining point type of the line.
 * @param scalar Scale factor.
 * @param line Line to scale.
 * @return Scaled line.
 */
template <class Scalar, class PointType>
    requires(!detail::is_point_v<Scalar>)
constexpr auto operator*(const Scalar& scalar, const Line<PointType>& line);

/**
 * @brief Divides both defining points by a scalar.
 *
 * @tparam PointType Defining point type of the line.
 * @tparam Scalar Scalar type.
 * @param line Line to divide.
 * @param scalar Divisor.
 * @return Scaled line.
 */
template <class PointType, class Scalar>
    requires(!detail::is_point_v<Scalar>)
constexpr auto operator/(const Line<PointType>& line, const Scalar& scalar);

/**
 * @brief Streams a line as `-p--q-`.
 *
 * @tparam PointType Defining point type of the line.
 * @param stream Output stream.
 * @param line Line to print.
 * @return The output stream.
 */
template <class PointType>
std::ostream& operator<<(std::ostream& stream, const Line<PointType>& line);

}  // namespace pgl
