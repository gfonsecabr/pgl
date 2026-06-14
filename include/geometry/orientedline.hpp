#pragma once

#ifndef PGL_HPP_INCLUDED
#error "Do not include this Pangolin header directly; include \"pgl.hpp\" instead."
#endif

/**
 * @file orientedline.hpp
 * @brief Public declaration of pgl::OrientedLine.
 *
 * Oriented lines add left/right side semantics, which feed halfplanes,
 * orientation tests, and directed geometric constructions.
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
struct OrientedLine;

OrientedLine() -> OrientedLine<Point<>>;

template <class PointType>
OrientedLine(PointType, PointType) -> OrientedLine<PointType>;

template <class Number>
OrientedLine(Number, Number, Number, Number) -> OrientedLine<Point<Number>>;

/**
 * @brief Infinite oriented straight line.
 *
 * Two non-degenerate oriented lines compare equal whenever they contain the
 * same points and point in the same direction. The stored defining points are
 * preserved exactly as provided.
 *
 * @tparam PointType Defining point type.
 */
template <class PointType_>
struct OrientedLine {
    using PointType = PointType_;
    using NumberType = PointType::NumberType;
    // using LabelType = detail::point_label_t<PointType>;
    using CoordinateType = detail::promoted_number_t<NumberType>;

    static_assert(detail::is_point_v<PointType>, "OrientedLine requires pgl::Point defining points");

    /**
     * @brief Creates the degenerate oriented line `(0,0)--(0,0)`.
     */
    constexpr OrientedLine() = default;

    /**
     * @brief Creates an oriented line from two defining points.
     *
     * The point order is preserved exactly as provided.
     *
     * @param source Source defining point.
     * @param target Target defining point.
     */
    constexpr OrientedLine(PointType source, PointType target)
        : points_{std::move(source), std::move(target)} {}

    /**
     * @brief Creates an oriented line from four coordinates.
     *
     * @param x1 X coordinate of the source.
     * @param y1 Y coordinate of the source.
     * @param x2 X coordinate of the target.
     * @param y2 Y coordinate of the target.
     */
    constexpr OrientedLine(NumberType x1, NumberType y1, NumberType x2, NumberType y2)
        : OrientedLine(PointType(x1, y1), PointType(x2, y2)) {}

    template<PointConcept OtherPointType>
        requires(std::constructible_from<PointType, const OtherPointType&>)
    constexpr OrientedLine(const OrientedLine<OtherPointType>& other)
        : OrientedLine(PointType(other.source()), PointType(other.target())) {}

    template<PointConcept OtherPointType>
        requires(std::constructible_from<PointType, const OtherPointType&>)
    constexpr OrientedLine& operator=(const OrientedLine<OtherPointType>& other) {
        points_[0] = PointType(other.source());
        points_[1] = PointType(other.target());
        return *this;
    }

    /**
     * @brief Returns defining point `0` for the source and `1` for the target.
     *
     * @param index Defining-point index.
     * @return Reference to the selected defining point.
     */
    constexpr const PointType& operator[](std::size_t index) const {
        assert(index < size());
        return points_[index];
    }
    constexpr PointType& operator[](std::size_t index) {
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
    constexpr PointType& get(std::ptrdiff_t index) {
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
     * @brief Returns the source defining point.
     *
     * @return Reference to the source.
     */
    constexpr const PointType& source() const {
        return points_[0];
    }
    constexpr PointType& source() {
        return points_[0];
    }

    /**
     * @brief Returns the target defining point.
     *
     * @return Reference to the target.
     */
    constexpr const PointType& target() const {
        return points_[1];
    }
    constexpr PointType& target() {
        return points_[1];
    }

    /**
     * @brief Returns the lexicographically smallest defining point.
     *
     * @return Reference to the smaller stored point.
     */
    constexpr const PointType& min() const {
        return target() < source() ? target() : source();
    }

    /**
     * @brief Returns the lexicographically largest defining point.
     *
     * @return Reference to the larger stored point.
     */
    constexpr const PointType& max() const {
        return source() < target() ? target() : source();
    }

    /**
     * @brief Returns the opposite orientation of the same geometric line.
     *
     * @return Reversed oriented line.
     */
    constexpr OrientedLine opposite() const {
        return OrientedLine(target(), source());
    }

    /**
     * @brief Returns an iterator to the source defining point.
     *
     * @return Pointer to the source.
     */
    constexpr auto begin() const {
        return points_.cbegin();
    }
    constexpr auto begin() {
        return points_.begin();
    }

    /**
     * @brief Returns an iterator to the source defining point.
     *
     * @return Pointer to the source.
     */
    constexpr auto cbegin() const {
        return points_.cbegin();
    }

    /**
     * @brief Returns an iterator past the target defining point.
     *
     * @return Pointer past the target.
     */
    constexpr auto end() const {
        return points_.cend();
    }
    constexpr auto end() {
        return points_.end();
    }

    /**
     * @brief Returns an iterator past the target defining point.
     *
     * @return Pointer past the target.
     */
    constexpr auto cend() const {
        return points_.cend();
    }

    /**
     * @brief Tests equality of the represented oriented line.
     *
     * Non-degenerate lines compare by their oriented normalized implicit
     * equation. Degenerate lines compare by their unique defining point.
     *
     * @param other Oriented line to compare with.
     * @return `true` if both lines represent the same oriented geometric set.
     */
    [[nodiscard]] constexpr bool operator==(const OrientedLine& other) const;

    /**
     * @brief Provides an ordering compatible with oriented-line equality.
     *
     * @param other Oriented line to compare with.
     * @return Comparison result.
     * @warning Coordinates are cubed but a single promotion is used.
     */
    [[nodiscard]] constexpr auto operator<=>(const OrientedLine& other) const;

    /**
     * @brief Converts to the unoriented line with the same supporting set.
     *
     * @return Unoriented supporting line.
     */
    [[nodiscard]] constexpr explicit operator Line<PointType>() const;

    /**
     * @brief Returns the line without orientation.
     *
     * @return Unoriented supporting line.
     */
    [[nodiscard]] constexpr Line<PointType> asLine() const {
        return static_cast<Line<PointType>>(*this);
    }

    /**
     * @brief Returns the oriented line rotated by 90k degrees around the origin.
     *
     * @param k Number of 90-degree CCW rotations (may be negative).
     * @return Rotated oriented line.
     */
    [[nodiscard]] constexpr OrientedLine rotated90(int k = 1) const;

    /**
     * @brief Rotates the oriented line by 90k degrees around the origin in place.
     *
     * @param k Number of 90-degree CCW rotations (may be negative).
     */
    constexpr void rotate90(int k = 1);

    /** @brief Returns the line with its x-coordinates multiplied by a factor. */
    template <class OtherNumber>
    [[nodiscard]] constexpr OrientedLine scaledUpX(const OtherNumber scalar) const;

    /** @brief Multiplies the line's x-coordinates by a factor in place. */
    template <class OtherNumber>
    constexpr void scaleUpX(const OtherNumber scalar);

    /** @brief Returns the line with its y-coordinates multiplied by a factor. */
    template <class OtherNumber>
    [[nodiscard]] constexpr OrientedLine scaledUpY(const OtherNumber scalar) const;

    /** @brief Multiplies the line's y-coordinates by a factor in place. */
    template <class OtherNumber>
    constexpr void scaleUpY(const OtherNumber scalar);

    /** @brief Returns the line with its x-coordinates divided by a divisor. */
    template <class OtherNumber>
    [[nodiscard]] constexpr OrientedLine scaledDownX(const OtherNumber scalar) const;

    /** @brief Divides the line's x-coordinates by a divisor in place. */
    template <class OtherNumber>
    constexpr void scaleDownX(const OtherNumber scalar);

    /** @brief Returns the line with its y-coordinates divided by a divisor. */
    template <class OtherNumber>
    [[nodiscard]] constexpr OrientedLine scaledDownY(const OtherNumber scalar) const;

    /** @brief Divides the line's y-coordinates by a divisor in place. */
    template <class OtherNumber>
    constexpr void scaleDownY(const OtherNumber scalar);

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

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool verticesContain(const OtherPoint& point) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool boundaryContains(const OtherPoint& point) const;

    // An oriented line has empty boundary, so it boundary-contains no non-point shape.
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

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool contains(const OtherPoint& point) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool contains(const Line<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool contains(const OrientedLine<OtherPoint>& other) const;

    template<PointConcept OtherPoint, class OtherLabel>
    [[nodiscard]] constexpr bool contains(const Segment<OtherPoint, OtherLabel>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool contains(const OrientedSegment<OtherPoint>& other) const;

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

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorContains(const OrientedLine<OtherPoint>& other) const;

    template<PointConcept OtherPoint, class OtherLabel>
    [[nodiscard]] constexpr bool interiorContains(const Segment<OtherPoint, OtherLabel>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorContains(const OrientedSegment<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorContains(const Ray<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorContains(const Halfplane<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorContains(const Rectangle<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorContains(const Triangle<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool collinear(const OtherPoint& point) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool collinear(const Line<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool collinear(const OrientedLine<OtherPoint>& other) const;

    template<PointConcept OtherPoint, class OtherLabel>
    [[nodiscard]] constexpr bool collinear(const Segment<OtherPoint, OtherLabel>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool collinear(const OrientedSegment<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool collinear(const Ray<OtherPoint>& other) const;

    /**
     * @brief Returns the orientation sign of a point with respect to the line.
     *
     * Negative means the point lies on the right, positive on the left, and
     * equivalent to zero when the point is collinear.
     *
     * @tparam OtherPoint Type of the point.
     *
     * @param point Point to classify.
     * @return Orientation sign of `(source, target, point)`.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr std::partial_ordering orientation(const OtherPoint& point) const;

    /**
     * @brief Returns the signed slope of the line.
     *
     * Undefined behavior for vertical lines.
     *
     * @tparam ResultNumber Coordinate type of the returned slope.
     * @return Signed slope.
     */
    template <class ResultNumber = NumberType>
    [[nodiscard]] constexpr ResultNumber slope() const;

    /**
     * @brief Returns the y-coordinate of the supporting line at a given x-coordinate, if defined.
     *
     * This ignores the stored orientation and delegates to the underlying
     * geometric line.
     *
     * @tparam ResultNumber Coordinate type of the returned value.
     * @tparam OtherNumber Coordinate type of the query x-coordinate.
     * @param x Query x-coordinate.
     * @return Corresponding y-coordinate, or empty when undefined.
     * @warning Uses division after casting to ResultNumber.
     */
    template <class ResultNumber = NumberType, class OtherNumber>
    [[nodiscard]] constexpr std::optional<ResultNumber> yAtX(const OtherNumber& x) const;

    /**
     * @brief Returns the x-coordinate of the supporting line at a given y-coordinate, if defined.
     *
     * This ignores the stored orientation and delegates to the underlying
     * geometric line.
     *
     * @tparam ResultNumber Coordinate type of the returned value.
     * @tparam OtherNumber Coordinate type of the query y-coordinate.
     * @param y Query y-coordinate.
     * @return Corresponding x-coordinate, or empty when undefined.
     * @warning Uses division after casting to ResultNumber.
     */
    template <class ResultNumber = NumberType, class OtherNumber>
    [[nodiscard]] constexpr std::optional<ResultNumber> xAtY(const OtherNumber& y) const;

    /**
     * @brief Returns the half-plane geometrically above the supporting line.
     *
     * This ignores the stored orientation and depends only on the underlying
     * geometric line.
     *
     * @return Closed half-plane above the supporting line.
     */
    [[nodiscard]] constexpr Halfplane<PointType> halfplaneAbove() const;

    /**
     * @brief Returns the half-plane geometrically below the supporting line.
     *
     * This ignores the stored orientation and depends only on the underlying
     * geometric line.
     *
     * @return Closed half-plane below the supporting line.
     */
    [[nodiscard]] constexpr Halfplane<PointType> halfplaneBelow() const;

    /**
     * @brief Returns the half-plane on the right of the oriented line.
     *
     * @return Closed right half-plane.
     */
    [[nodiscard]] constexpr Halfplane<PointType> rightHalfplane() const;

    /**
     * @brief Returns the half-plane on the left of the oriented line.
     *
     * @return Closed left half-plane.
     */
    [[nodiscard]] constexpr Halfplane<PointType> leftHalfplane() const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool parallel(const Line<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool parallel(const OrientedLine<OtherPoint>& other) const;

    template<PointConcept OtherPoint, class OtherLabel>
    [[nodiscard]] constexpr bool parallel(const Segment<OtherPoint, OtherLabel>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool parallel(const OrientedSegment<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool intersects(const OtherPoint& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool intersects(const Line<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool intersects(const OrientedLine<OtherPoint>& other) const;

    template<PointConcept OtherPoint, class OtherLabel>
    [[nodiscard]] constexpr bool intersects(const Segment<OtherPoint, OtherLabel>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool intersects(const OrientedSegment<OtherPoint>& other) const;

    [[nodiscard]] constexpr bool intersects(const Shape<PointType>& other) const;

    template<typename OtherShape>
        requires (!PointConcept<OtherShape> && detail::shapeRank<OtherShape> > detail::shapeRank<OrientedLine>)
    [[nodiscard]] constexpr bool intersects(const OtherShape& other) const {
        return other.intersects(*this);
    }

    /** @brief The empty set never meets another shape. */
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool intersects(const EmptyShape<EmptyPoint>&) const {
        return false;
    }

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherPoint& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorsIntersect(const Line<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorsIntersect(const OrientedLine<OtherPoint>& other) const;

    template<PointConcept OtherPoint, class OtherLabel>
    [[nodiscard]] constexpr bool interiorsIntersect(const Segment<OtherPoint, OtherLabel>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorsIntersect(const OrientedSegment<OtherPoint>& other) const;

    template<typename OtherShape>
        requires (!PointConcept<OtherShape> && detail::shapeRank<OtherShape> > detail::shapeRank<OrientedLine>)
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherShape& other) const {
        return other.interiorsIntersect(*this);
    }

    /** @brief The empty set never meets another shape. */
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool interiorsIntersect(const EmptyShape<EmptyPoint>&) const {
        return false;
    }

    [[nodiscard]] constexpr bool interiorsIntersect(const Shape<PointType>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool crosses(const Line<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool crosses(const OrientedLine<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool crosses(const OtherPoint& other) const;

    template<PointConcept OtherPoint, class OtherLabel>
    [[nodiscard]] constexpr bool crosses(const Segment<OtherPoint, OtherLabel>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool crosses(const OrientedSegment<OtherPoint>& other) const;

    template<typename OtherShape>
        requires (!PointConcept<OtherShape> && detail::shapeRank<OtherShape> > detail::shapeRank<OrientedLine>)
    [[nodiscard]] constexpr bool crosses(const OtherShape& other) const {
        return other.crosses(*this);
    }

    /** @brief The empty set never meets another shape. */
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool crosses(const EmptyShape<EmptyPoint>&) const {
        return false;
    }

    [[nodiscard]] constexpr bool crosses(const Shape<PointType>& other) const;

    /**
     * @brief Orders two lines by where they cross this oriented line.
     *
     * Following this oriented line's orientation, returns `less` if @p first
     * crosses before @p second, `greater` if after, `equivalent` if both cross
     * at the same point, and `unordered` if either line is parallel to this one.
     *
     * Uses only the orientation of the defining points; no crossing point is
     * ever constructed.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr std::partial_ordering
    crossingOrder(const Line<OtherPoint>& first, const Line<OtherPoint>& second) const;

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


    template <class ResultNumber = NumberType, PointConcept OtherPoint>
    [[nodiscard]] constexpr std::optional<Point<ResultNumber, typename PointType::LabelType>>
    intersection(const OtherPoint& other) const;

    template <class ResultNumber = NumberType, PointConcept OtherPoint>
    [[nodiscard]] constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Line<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const Line<OtherPoint>& other) const;

    template <class ResultNumber = NumberType, PointConcept OtherPoint>
    [[nodiscard]] constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Line<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OrientedLine<OtherPoint>& other) const;

    template <class ResultNumber = NumberType, PointConcept OtherPoint, class OtherLabel>
    [[nodiscard]] constexpr auto intersection(const Segment<OtherPoint, OtherLabel>& other) const;

    template <class ResultNumber = NumberType, PointConcept OtherPoint>
    [[nodiscard]] constexpr auto intersection(const OrientedSegment<OtherPoint>& other) const;

    template <class ResultNumber = NumberType, typename OtherShape>
        requires (!PointConcept<OtherShape>
                  && (detail::shapeRank<OtherShape> > detail::shapeRank<OrientedLine>)
                  && requires(const OtherShape& o, const OrientedLine& self) {
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

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr auto squaredDistance(const OtherPoint& point) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr auto squaredDistance(const Line<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr auto squaredDistance(const OrientedLine<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    constexpr OrientedLine& operator+=(const OtherPoint& translation);

    template<PointConcept OtherPoint>
    constexpr OrientedLine& operator-=(const OtherPoint& translation);

    template <class Scalar>
        requires(!detail::is_point_v<Scalar>)
    constexpr OrientedLine& operator*=(const Scalar& scalar);

    template <class Scalar>
        requires(!detail::is_point_v<Scalar>)
    constexpr OrientedLine& operator/=(const Scalar& scalar);

    /**
     * @brief Returns a point inside the oriented line.
     *
     * For a line, this is the source point.
     *
     * @return The source point.
     */
    template <class ResultNumber = NumberType>
    [[nodiscard]] constexpr Point<ResultNumber> pointInside() const;

    private:
    template <class OtherNumber>
    using promoted_number_t = std::common_type_t<CoordinateType, detail::promoted_number_t<OtherNumber>>;

    std::array<PointType, 2> points_{};
};

template <class PointType, class TranslationNumber, class TranslationLabel>
constexpr auto operator+(const OrientedLine<PointType>& line, const Point<TranslationNumber, TranslationLabel>& translation);

template <class TranslationNumber, class TranslationLabel, class PointType>
constexpr auto operator+(const Point<TranslationNumber, TranslationLabel>& translation, const OrientedLine<PointType>& line);

template <class PointType, class TranslationNumber, class TranslationLabel>
constexpr auto operator-(const OrientedLine<PointType>& line, const Point<TranslationNumber, TranslationLabel>& translation);

template <class PointType, class Scalar>
    requires(!detail::is_point_v<Scalar>)
constexpr auto operator*(const OrientedLine<PointType>& line, const Scalar& scalar);

template <class Scalar, class PointType>
    requires(!detail::is_point_v<Scalar>)
constexpr auto operator*(const Scalar& scalar, const OrientedLine<PointType>& line);

template <class PointType, class Scalar>
    requires(!detail::is_point_v<Scalar>)
constexpr auto operator/(const OrientedLine<PointType>& line, const Scalar& scalar);

template <class PointType>
std::ostream& operator<<(std::ostream& stream, const OrientedLine<PointType>& line);

}  // namespace pgl
