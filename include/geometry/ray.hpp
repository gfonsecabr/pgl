#pragma once

/**
 * @file ray.hpp
 * @brief Public declaration of pgl::Ray.
 *
 * Ray represents a half-infinite 1D primitive with a source point and a
 * direction inherited from its defining points.
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

#include "../pgl.hpp"

namespace pgl {

template <class PointType = Point<>>
struct Ray;

Ray() -> Ray<Point<>>;

template <class PointType>
Ray(PointType, PointType) -> Ray<PointType>;

template <class Number>
Ray(Number, Number, Number, Number) -> Ray<Point<Number>>;

/**
 * @brief Half-line starting at a source point and extending through a target.
 *
 * The stored point order is preserved exactly as provided. Two non-degenerate
 * rays compare equal whenever they share the same source and direction.
 *
 * @tparam PointType Defining point type.
 */
template <class PointType_>
struct Ray {
    using PointType = PointType_;
    using NumberType = PointType::NumberType;
    // using LabelType = detail::point_label_t<PointType>;
    using CoordinateType = detail::promoted_number_t<NumberType>;

    static_assert(detail::is_point_v<PointType>, "Ray requires pgl::Point defining points");

    /**
     * @brief Creates the degenerate ray `(0,0)--(0,0)->`.
     */
    constexpr Ray() = default;

    /**
     * @brief Creates a ray from a source and a second point on the ray.
     *
     * The point order is preserved exactly as provided.
     *
     * @param source Source point of the ray.
     * @param target Any other point on the ray direction.
     */
    constexpr Ray(PointType source, PointType target)
        : points_{std::move(source), std::move(target)} {}

    /**
     * @brief Creates a ray from four coordinates.
     *
     * @param x1 X coordinate of the source.
     * @param y1 Y coordinate of the source.
     * @param x2 X coordinate of the target.
     * @param y2 Y coordinate of the target.
     */
    constexpr Ray(NumberType x1, NumberType y1, NumberType x2, NumberType y2)
        : Ray(PointType(x1, y1), PointType(x2, y2)) {}

    template<PointConcept OtherPointType>
        requires(std::constructible_from<PointType, const OtherPointType&>)
    constexpr Ray(const Ray<OtherPointType>& other)
        : Ray(PointType(other.source()), PointType(other.target())) {}

    template<PointConcept OtherPointType>
        requires(std::constructible_from<PointType, const OtherPointType&>)
    constexpr Ray& operator=(const Ray<OtherPointType>& other) {
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
     * @brief Returns the source point of the ray.
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
     * @brief Returns the second stored point defining the direction.
     *
     * @return Reference to the target point.
     */
    constexpr const PointType& target() const {
        return points_[1];
    }
    constexpr PointType& target() {
        return points_[1];
    }

    /**
     * @brief Returns the lexicographically smallest stored defining point.
     *
     * @return Reference to the smaller stored point.
     */
    constexpr const PointType& min() const {
        return target() < source() ? target() : source();
    }

    /**
     * @brief Returns the lexicographically largest stored defining point.
     *
     * @return Reference to the larger stored point.
     */
    constexpr const PointType& max() const {
        return source() < target() ? target() : source();
    }

    /**
     * @brief Returns the ray obtained by swapping the two stored defining points.
     *
     * @return Ray with exchanged source and target.
     */
    constexpr Ray opposite() const {
        return Ray(target(), source());
    }

    /**
     * @brief Returns an iterator to the source point.
     *
     * @return Pointer to the source point.
     */
    constexpr auto begin() const {
        return points_.cbegin();
    }
    constexpr auto begin() {
        return points_.begin();
    }

    /**
     * @brief Returns an iterator to the source point.
     *
     * @return Pointer to the source point.
     */
    constexpr auto cbegin() const {
        return points_.cbegin();
    }

    /**
     * @brief Returns an iterator past the target point.
     *
     * @return Pointer past the target point.
     */
    constexpr auto end() const {
        return points_.cend();
    }
    constexpr auto end() {
        return points_.end();
    }

    /**
     * @brief Returns an iterator past the target point.
     *
     * @return Pointer past the target point.
     */
    constexpr auto cend() const {
        return points_.cend();
    }

    /**
     * @brief Tests equality of the represented ray.
     *
     * Degenerate rays compare by their unique point. Non-degenerate rays
     * compare by their source and normalized direction.
     *
     * @param other Ray to compare with.
     * @return `true` if both rays represent the same geometric set.
     */
    [[nodiscard]] constexpr bool operator==(const Ray& other) const;

    /**
     * @brief Provides an ordering compatible with ray equality.
     *
     * @param other Ray to compare with.
     * @return Comparison result.
     * @warning Coordinates are cubed but a single promotion is used.
     */
    [[nodiscard]] constexpr auto operator<=>(const Ray& other) const;

    /**
     * @brief Converts to the unoriented supporting line.
     *
     * @return Line containing the ray.
     */
    [[nodiscard]] constexpr explicit operator Line<PointType>() const;

    /**
     * @brief Returns the supporting line without orientation.
     *
     * @return Line containing the ray.
     */
    [[nodiscard]] constexpr Line<PointType> asLine() const {
        return static_cast<Line<PointType>>(*this);
    }

    /**
     * @brief Converts to the oriented supporting line.
     *
     * @return Oriented line containing the ray.
     */
    [[nodiscard]] constexpr explicit operator OrientedLine<PointType>() const;

    /**
     * @brief Returns the oriented supporting line.
     *
     * @return Oriented line containing the ray.
     */
    [[nodiscard]] constexpr OrientedLine<PointType> asOrientedLine() const {
        return static_cast<OrientedLine<PointType>>(*this);
    }

    /**
     * @brief Returns the ray rotated by 90k degrees around the origin.
     *
     * @param k Number of 90-degree CCW rotations (may be negative).
     * @return Rotated ray.
     */
    [[nodiscard]] constexpr Ray rotated90(int k = 1) const;

    /**
     * @brief Rotates the ray by 90k degrees around the origin in place.
     *
     * @param k Number of 90-degree CCW rotations (may be negative).
     */
    constexpr void rotate90(int k = 1);

    /** @brief Returns the ray with its x-coordinates multiplied by a factor. */
    template <class OtherNumber>
    [[nodiscard]] constexpr Ray scaledUpX(const OtherNumber scalar) const;

    /** @brief Multiplies the ray's x-coordinates by a factor in place. */
    template <class OtherNumber>
    constexpr void scaleUpX(const OtherNumber scalar);

    /** @brief Returns the ray with its y-coordinates multiplied by a factor. */
    template <class OtherNumber>
    [[nodiscard]] constexpr Ray scaledUpY(const OtherNumber scalar) const;

    /** @brief Multiplies the ray's y-coordinates by a factor in place. */
    template <class OtherNumber>
    constexpr void scaleUpY(const OtherNumber scalar);

    /** @brief Returns the ray with its x-coordinates divided by a divisor. */
    template <class OtherNumber>
    [[nodiscard]] constexpr Ray scaledDownX(const OtherNumber scalar) const;

    /** @brief Divides the ray's x-coordinates by a divisor in place. */
    template <class OtherNumber>
    constexpr void scaleDownX(const OtherNumber scalar);

    /** @brief Returns the ray with its y-coordinates divided by a divisor. */
    template <class OtherNumber>
    [[nodiscard]] constexpr Ray scaledDownY(const OtherNumber scalar) const;

    /** @brief Divides the ray's y-coordinates by a divisor in place. */
    template <class OtherNumber>
    constexpr void scaleDownY(const OtherNumber scalar);

    /**
     * @brief Returns the area of the ray.
     *
     * A ray is one-dimensional, so its area is always zero.
     *
     * @return Zero.
     */
    [[nodiscard]] constexpr NumberType area() const;

    /**
     * @brief Returns twice the area of the ray.
     *
     * A ray is one-dimensional, so this is always zero.
     *
     * @return Zero.
     */
    [[nodiscard]] constexpr NumberType twiceArea() const;

    /**
     * @brief Returns whether the defining points coincide.
     *
     * @return `true` if the source and target are equal.
     */
    [[nodiscard]] constexpr bool isDegenerate() const;

    /**
     * @brief Returns whether the ray is vertical.
     *
     * @return `true` if the defining points have the same x-coordinate.
     */
    [[nodiscard]] constexpr bool isVertical() const;

    /**
     * @brief Returns whether the ray is horizontal.
     *
     * @return `true` if the defining points have the same y-coordinate.
     */
    [[nodiscard]] constexpr bool isHorizontal() const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool verticesContain(const OtherPoint& point) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool boundaryContains(const OtherPoint& point) const;

    template<class S>
        requires(!detail::is_point_v<S>)
    [[nodiscard]] constexpr bool boundaryContains(const S& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool containsCollinear(const OtherPoint& point) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool contains(const OtherPoint& point) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool contains(const Line<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool contains(const OrientedLine<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool contains(const Segment<OtherPoint>& other) const;

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

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorContains(const OtherPoint& point) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorContains(const Line<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorContains(const OrientedLine<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorContains(const Segment<OtherPoint>& other) const;

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

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool collinear(const Segment<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool collinear(const OrientedSegment<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool collinear(const Ray<OtherPoint>& other) const;

    /**
     * @brief Returns the orientation sign of a point with respect to the ray.
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
     * @brief Returns the signed slope of the supporting line.
     *
     * Undefined behavior for vertical rays.
     *
     * @tparam ResultNumber Coordinate type of the returned slope.
     * @return Signed slope.
     */
    template <class ResultNumber = NumberType>
    [[nodiscard]] constexpr ResultNumber slope() const;

    /**
     * @brief Returns the half-plane geometrically above the supporting line.
     *
     * This ignores the stored ray direction and depends only on the supporting
     * geometric line.
     *
     * @return Closed half-plane above the supporting line.
     */
    [[nodiscard]] constexpr Halfplane<PointType> halfplaneAbove() const;

    /**
     * @brief Returns the half-plane geometrically below the supporting line.
     *
     * This ignores the stored ray direction and depends only on the supporting
     * geometric line.
     *
     * @return Closed half-plane below the supporting line.
     */
    [[nodiscard]] constexpr Halfplane<PointType> halfplaneBelow() const;

    /**
     * @brief Returns the half-plane on the right of the ray direction.
     *
     * @return Closed right half-plane.
     */
    [[nodiscard]] constexpr Halfplane<PointType> rightHalfplane() const;

    /**
     * @brief Returns the half-plane on the left of the ray direction.
     *
     * @return Closed left half-plane.
     */
    [[nodiscard]] constexpr Halfplane<PointType> leftHalfplane() const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool parallel(const Line<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool parallel(const OrientedLine<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool parallel(const Segment<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool parallel(const OrientedSegment<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool parallel(const Ray<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool intersects(const OtherPoint& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool intersects(const Line<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool intersects(const OrientedLine<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool intersects(const Segment<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool intersects(const OrientedSegment<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool intersects(const Ray<OtherPoint>& other) const;

    [[nodiscard]] constexpr bool intersects(const Shape<PointType>& other) const;

    template<typename OtherShape>
        requires (!PointConcept<OtherShape>)
    [[nodiscard]] constexpr bool intersects(const OtherShape& other) const {
        return other.intersects(*this);
    }

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherPoint& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorsIntersect(const Line<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorsIntersect(const OrientedLine<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorsIntersect(const Segment<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorsIntersect(const OrientedSegment<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorsIntersect(const Ray<OtherPoint>& other) const;

    template<typename OtherShape>
        requires (!PointConcept<OtherShape>)
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherShape& other) const {
        return other.interiorsIntersect(*this);
    }

    [[nodiscard]] constexpr bool interiorsIntersect(const Shape<PointType>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool crosses(const Line<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool crosses(const OrientedLine<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool crosses(const Segment<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool crosses(const OrientedSegment<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool crosses(const Ray<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool crosses(const OtherPoint& other) const;

    template<typename OtherShape>
        requires (!PointConcept<OtherShape>)
    [[nodiscard]] constexpr bool crosses(const OtherShape& other) const {
        return other.crosses(*this);
    }

    [[nodiscard]] constexpr bool crosses(const Shape<PointType>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool separates(const OtherPoint& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool separates(const Segment<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool separates(const OrientedSegment<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool separates(const Line<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool separates(const OrientedLine<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool separates(const Ray<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool separates(const Halfplane<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool separates(const Rectangle<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool separates(const Triangle<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool separates(const Convex<OtherPoint>& other) const;

    /**
     * @brief Returns whether removing this ray disconnects the polygon.
     *
     * Like the segment overload, but a ray contributes only its source as a
     * finite end; its far end runs to infinity, always outside the bounded
     * polygon, so only the source can lie strictly inside.
     *
     * Complexity: O(n) for n polygon vertices.
     */
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


    template <class ResultNumber = NumberType, class OtherPoint>
    [[nodiscard]] constexpr std::optional<Point<ResultNumber, typename PointType::LabelType>>
    intersection(const OtherPoint& other) const;

    template <class ResultNumber = NumberType, class OtherPoint>
    [[nodiscard]] constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Ray<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const Line<OtherPoint>& other) const;

    template <class ResultNumber = NumberType, class OtherPoint>
    [[nodiscard]] constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Ray<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OrientedLine<OtherPoint>& other) const;

    template <class ResultNumber = NumberType, class OtherPoint>
    [[nodiscard]] constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const Segment<OtherPoint>& other) const;

    template <class ResultNumber = NumberType, class OtherPoint>
    [[nodiscard]] constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OrientedSegment<OtherPoint>& other) const;

    template <class ResultNumber = NumberType, class OtherPoint>
    [[nodiscard]] constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>, Ray<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const Ray<OtherPoint>& other) const;

    template <class ResultNumber = NumberType, typename OtherShape>
        requires (!PointConcept<OtherShape>)
    [[nodiscard]] constexpr auto intersection(const OtherShape& other) const {
        return other.template intersection<ResultNumber>(*this);
    }

    /**
     * @brief Returns the value of the y coordinate for a given x, if it exists.
     *
     * Delegates to the supporting line, then keeps the result only if the
     * resulting point lies on the ray.
     *
     * @tparam ResultNumber Number type of the return value.
     * @tparam OtherNumber Coordinate type of the x coordinate.
     * @param x Given x coordinate.
     * @return An std::optional of ResultNumber corresponding to the y coordinate.
     * @warning Divides coordinates after casting to ResultNumber.
     */
    template <class ResultNumber = NumberType, class OtherNumber>
    [[nodiscard]] constexpr std::optional<ResultNumber>
    yAtX(const OtherNumber &x) const;

    /**
     * @brief Returns the value of the x coordinate for a given y, if it exists.
     *
     * Delegates to the supporting line, then keeps the result only if the
     * resulting point lies on the ray.
     *
     * @tparam ResultNumber Number type of the return value.
     * @tparam OtherNumber Coordinate type of the y coordinate.
     * @param y Given y coordinate.
     * @return An std::optional of ResultNumber corresponding to the x coordinate.
     * @warning Divides coordinates after casting to ResultNumber.
     */
    template <class ResultNumber = NumberType, class OtherNumber>
    [[nodiscard]] constexpr std::optional<ResultNumber>
    xAtY(const OtherNumber &y) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr auto squaredDistance(const OtherPoint& point) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr auto squaredDistance(const Line<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr auto squaredDistance(const OrientedLine<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr auto squaredDistance(const Segment<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr auto squaredDistance(const OrientedSegment<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr auto squaredDistance(const Ray<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    constexpr Ray& operator+=(const OtherPoint& translation);

    template<PointConcept OtherPoint>
    constexpr Ray& operator-=(const OtherPoint& translation);

    template <class Scalar>
        requires(!detail::is_point_v<Scalar>)
    constexpr Ray& operator*=(const Scalar& scalar);

    template <class Scalar>
        requires(!detail::is_point_v<Scalar>)
    constexpr Ray& operator/=(const Scalar& scalar);

    /**
     * @brief Returns a point inside the ray.
     *
     * For a ray, this is the target point.
     *
     * @return The target point.
     */
    template <class ResultNumber = NumberType>
    [[nodiscard]] constexpr Point<ResultNumber> pointInside() const;

private:
    template <class OtherNumber>
    using promoted_number_t = std::common_type_t<CoordinateType, detail::promoted_number_t<OtherNumber>>;

    template<PointConcept OtherPoint>
    constexpr auto directionCross(const OtherPoint& first, const OtherPoint& second) const;

    std::array<PointType, 2> points_{};
};

template <class PointType, class TranslationNumber, class TranslationLabel>
constexpr auto operator+(const Ray<PointType>& ray, const Point<TranslationNumber, TranslationLabel>& translation);

template <class TranslationNumber, class TranslationLabel, class PointType>
constexpr auto operator+(const Point<TranslationNumber, TranslationLabel>& translation, const Ray<PointType>& ray);

template <class PointType, class TranslationNumber, class TranslationLabel>
constexpr auto operator-(const Ray<PointType>& ray, const Point<TranslationNumber, TranslationLabel>& translation);

template <class PointType, class Scalar>
    requires(!detail::is_point_v<Scalar>)
constexpr auto operator*(const Ray<PointType>& ray, const Scalar& scalar);

template <class Scalar, class PointType>
    requires(!detail::is_point_v<Scalar>)
constexpr auto operator*(const Scalar& scalar, const Ray<PointType>& ray);

template <class PointType, class Scalar>
    requires(!detail::is_point_v<Scalar>)
constexpr auto operator/(const Ray<PointType>& ray, const Scalar& scalar);

template <class PointType>
std::ostream& operator<<(std::ostream& stream, const Ray<PointType>& ray);

}  // namespace pgl
