#pragma once

/**
 * @file halfplane.hpp
 * @brief Public declaration of pgl::Halfplane.
 *
 * Halfplane models one side of an oriented line and is the main 2D infinite
 * region used for clipping and side-based predicates.
 */

#include <array>
#include <cassert>
#include <compare>
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
struct Halfplane;

Halfplane() -> Halfplane<Point<>>;

template <class PointType>
Halfplane(PointType, PointType) -> Halfplane<PointType>;

template <class Number>
Halfplane(Number, Number, Number, Number) -> Halfplane<Point<Number>>;

/**
 * @brief Closed half-plane stored by an oriented boundary line.
 *
 * The half-plane contains all points on the left side of the oriented line
 * from @ref source to @ref target, including the boundary itself.
 *
 * @tparam PointType Defining point type.
 */
template <class PointType_>
struct Halfplane {
    using PointType = PointType_;
    using NumberType = PointType::NumberType;
    // using LabelType = detail::point_label_t<PointType>;
    using CoordinateType = detail::promoted_number_t<NumberType>;

    static_assert(detail::is_point_v<PointType>, "Halfplane requires pgl::Point defining points");

    /**
     * @brief Creates the degenerate half-plane `(0,0)->(0,0)`.
     */
    constexpr Halfplane() = default;

    /**
     * @brief Creates a half-plane from an oriented boundary line.
     *
     * The point order is preserved exactly as provided.
     *
     * @param source Source boundary point.
     * @param target Target boundary point.
     */
    constexpr Halfplane(PointType source, PointType target)
        : points_{std::move(source), std::move(target)} {}

    /**
     * @brief Creates a half-plane from four coordinates.
     *
     * @param x1 X coordinate of the source boundary point.
     * @param y1 Y coordinate of the source boundary point.
     * @param x2 X coordinate of the target boundary point.
     * @param y2 Y coordinate of the target boundary point.
     */
    constexpr Halfplane(NumberType x1, NumberType y1, NumberType x2, NumberType y2)
        : Halfplane(PointType(x1, y1), PointType(x2, y2)) {}

    template<PointConcept OtherPointType>
        requires(std::constructible_from<PointType, const OtherPointType&>)
    constexpr Halfplane(const Halfplane<OtherPointType>& other)
        : Halfplane(PointType(other.source()), PointType(other.target())) {}

    template<PointConcept OtherPointType>
        requires(std::constructible_from<PointType, const OtherPointType&>)
    constexpr Halfplane& operator=(const Halfplane<OtherPointType>& other) {
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
     * @brief Returns the source boundary point.
     *
     * @return Reference to the source point.
     */
    constexpr const PointType& source() const {
        return points_[0];
    }
    constexpr PointType& source() {
        return points_[0];
    }

    /**
     * @brief Returns the target boundary point.
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
     * @brief Returns the complementary half-plane with reversed boundary orientation.
     *
     * @return Half-plane on the opposite side of the same boundary line.
     */
    constexpr Halfplane opposite() const {
        return Halfplane(target(), source());
    }

    /**
     * @brief Returns an iterator to the source defining point.
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
     * @brief Returns an iterator to the source defining point.
     *
     * @return Pointer to the source point.
     */
    constexpr auto cbegin() const {
        return points_.cbegin();
    }

    /**
     * @brief Returns an iterator past the target defining point.
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
     * @brief Returns an iterator past the target defining point.
     *
     * @return Pointer past the target point.
     */
    constexpr auto cend() const {
        return points_.cend();
    }

    /**
     * @brief Tests equality of the represented half-plane.
     *
     * Two half-planes are equal when their oriented boundary lines are equal.
     *
     * @param other Half-plane to compare with.
     * @return `true` if both half-planes represent the same geometric set.
     */
    [[nodiscard]] constexpr bool operator==(const Halfplane& other) const;

    /**
     * @brief Provides an ordering compatible with half-plane equality.
     *
     * @param other Half-plane to compare with.
     * @return Comparison result on the oriented boundary line.
     * @warning Coordinates are cubed but a single promotion is used.
     */
    [[nodiscard]] constexpr auto operator<=>(const Halfplane& other) const;

    /**
     * @brief Converts to the unoriented boundary line.
     *
     * @return Boundary line of the half-plane.
     */
    [[nodiscard]] constexpr explicit operator Line<PointType>() const;

    /**
     * @brief Returns the boundary line without orientation.
     *
     * @return Boundary line of the half-plane.
     */
    [[nodiscard]] constexpr Line<PointType> asLine() const {
        return static_cast<Line<PointType>>(*this);
    }

    /**
     * @brief Converts to the oriented boundary line.
     *
     * @return Oriented boundary line of the half-plane.
     */
    [[nodiscard]] constexpr explicit operator OrientedLine<PointType>() const;

    /**
     * @brief Returns the oriented boundary line.
     *
     * @return Oriented boundary line of the half-plane.
     */
    [[nodiscard]] constexpr OrientedLine<PointType> asOrientedLine() const {
        return static_cast<OrientedLine<PointType>>(*this);
    }

    /**
     * @brief Returns the half-plane rotated by 90k degrees around the origin.
     *
     * @param k Number of 90-degree CCW rotations (may be negative).
     * @return Rotated half-plane.
     */
    [[nodiscard]] constexpr Halfplane rotated90(int k = 1) const;

    /**
     * @brief Rotates the half-plane by 90k degrees around the origin in place.
     *
     * @param k Number of 90-degree CCW rotations (may be negative).
     */
    constexpr void rotate90(int k = 1);

    /** @brief Returns the half-plane with its x-coordinates multiplied by a factor. */
    template <class OtherNumber>
    [[nodiscard]] constexpr Halfplane scaledUpX(const OtherNumber scalar) const;

    /** @brief Multiplies the half-plane's x-coordinates by a factor in place. */
    template <class OtherNumber>
    constexpr void scaleUpX(const OtherNumber scalar);

    /** @brief Returns the half-plane with its y-coordinates multiplied by a factor. */
    template <class OtherNumber>
    [[nodiscard]] constexpr Halfplane scaledUpY(const OtherNumber scalar) const;

    /** @brief Multiplies the half-plane's y-coordinates by a factor in place. */
    template <class OtherNumber>
    constexpr void scaleUpY(const OtherNumber scalar);

    /** @brief Returns the half-plane with its x-coordinates divided by a divisor. */
    template <class OtherNumber>
    [[nodiscard]] constexpr Halfplane scaledDownX(const OtherNumber scalar) const;

    /** @brief Divides the half-plane's x-coordinates by a divisor in place. */
    template <class OtherNumber>
    constexpr void scaleDownX(const OtherNumber scalar);

    /** @brief Returns the half-plane with its y-coordinates divided by a divisor. */
    template <class OtherNumber>
    [[nodiscard]] constexpr Halfplane scaledDownY(const OtherNumber scalar) const;

    /** @brief Divides the half-plane's y-coordinates by a divisor in place. */
    template <class OtherNumber>
    constexpr void scaleDownY(const OtherNumber scalar);

    /**
     * @brief Returns whether the defining points coincide.
     *
     * @return `true` if the boundary degenerates to a point.
     */
    [[nodiscard]] constexpr bool isDegenerate() const;

    /**
     * @brief Returns whether the boundary line is vertical.
     *
     * @return `true` if both defining points share the same x-coordinate.
     */
    [[nodiscard]] constexpr bool isVertical() const;

    /**
     * @brief Returns whether the boundary line is horizontal.
     *
     * @return `true` if both defining points share the same y-coordinate.
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
    [[nodiscard]] constexpr bool contains(const Rectangle<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool contains(const Halfplane<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool contains(const Triangle<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool contains(const Convex<OtherPoint>& other) const;

    /** @brief Polygon overload; see the Convex overload. */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool contains(const Polygon<OtherPoint>& other) const;

    template<PointConcept OtherPoint, class OtherLabel>
    [[nodiscard]] constexpr bool contains(const Disk<OtherPoint, OtherLabel>& other) const;

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
    [[nodiscard]] constexpr bool interiorContains(const Rectangle<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorContains(const Halfplane<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorContains(const Triangle<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorContains(const Convex<OtherPoint>& other) const;

    template<PointConcept OtherPoint, class OtherLabel>
    [[nodiscard]] constexpr bool interiorContains(const Disk<OtherPoint, OtherLabel>& other) const;

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

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool intersects(const Halfplane<OtherPoint>& other) const;

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

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorsIntersect(const Halfplane<OtherPoint>& other) const;

    template<typename OtherShape>
        requires (!PointConcept<OtherShape>)
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherShape& other) const {
        return other.interiorsIntersect(*this);
    }

    [[nodiscard]] constexpr bool interiorsIntersect(const Shape<PointType>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool separates(const Line<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool separates(const OtherPoint& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool separates(const OrientedLine<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool separates(const Segment<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool separates(const OrientedSegment<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool separates(const Ray<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool separates(const Rectangle<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool separates(const Halfplane<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool separates(const Triangle<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool separates(const Convex<OtherPoint>& other) const;

    /** @brief Polygon overload; see the Convex overload. */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool separates(const Polygon<OtherPoint>& other) const;

    [[nodiscard]] constexpr bool separates(const Shape<PointType>& other) const;

    // --- not-yet-implemented predicate pairs (throw); see implementation ---
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorContains(const Polygon<OtherPoint>& other) const;


    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool crosses(const Line<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool crosses(const OtherPoint& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool crosses(const OrientedLine<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool crosses(const Segment<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool crosses(const OrientedSegment<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool crosses(const Ray<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool crosses(const Halfplane<OtherPoint>& other) const;

    template<typename OtherShape>
        requires (!PointConcept<OtherShape>)
    [[nodiscard]] constexpr bool crosses(const OtherShape& other) const {
        return other.crosses(*this);
    }

    [[nodiscard]] constexpr bool crosses(const Shape<PointType>& other) const;

    template <class ResultNumber = NumberType, class OtherPoint>
    [[nodiscard]] constexpr std::optional<Point<ResultNumber, typename PointType::LabelType>>
    intersection(const OtherPoint& other) const;

    template <class ResultNumber = NumberType, class OtherPoint>
    [[nodiscard]] constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Line<Point<ResultNumber, typename PointType::LabelType>>, Ray<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const Line<OtherPoint>& other) const;

    template <class ResultNumber = NumberType, class OtherPoint>
    [[nodiscard]] constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Line<Point<ResultNumber, typename PointType::LabelType>>, Ray<Point<ResultNumber, typename PointType::LabelType>>>>
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

    // Intersecting two half-planes is intentionally unsupported. Deleting the
    // overload makes such a call a compile error rather than letting it fall
    // through to the reversing fallback below, which would recurse forever.
    template <class ResultNumber = NumberType, class OtherPoint>
    constexpr auto intersection(const Halfplane<OtherPoint>& other) const = delete;

    template <class ResultNumber = NumberType, typename OtherShape>
        requires (!PointConcept<OtherShape>)
    [[nodiscard]] constexpr auto intersection(const OtherShape& other) const {
        return other.template intersection<ResultNumber>(*this);
    }

    /**
     * @brief Returns the signed slope of the boundary line.
     *
     * Undefined behavior for vertical boundaries.
     *
     * @tparam ResultNumber Coordinate type of the returned slope.
     * @return Signed slope.
     */
    template <class ResultNumber = NumberType>
    [[nodiscard]] constexpr ResultNumber slope() const;

    template<PointConcept OtherPoint>
    constexpr Halfplane& operator+=(const OtherPoint& translation);

    template<PointConcept OtherPoint>
    constexpr Halfplane& operator-=(const OtherPoint& translation);

    template <class Scalar>
        requires(!detail::is_point_v<Scalar>)
    constexpr Halfplane& operator*=(const Scalar& scalar);

    template <class Scalar>
        requires(!detail::is_point_v<Scalar>)
    constexpr Halfplane& operator/=(const Scalar& scalar);

    /**
     * @brief Returns a point inside the halfplane.
     *
     * @return Returns the point strictly inside the halfplane.
     */
    template <class ResultNumber = NumberType>
    [[nodiscard]] constexpr Point<ResultNumber> pointInside() const;

  private:

    std::array<PointType, 2> points_{};
};

template <class PointType, class TranslationNumber, class TranslationLabel>
constexpr auto operator+(const Halfplane<PointType>& halfplane, const Point<TranslationNumber, TranslationLabel>& translation);

template <class TranslationNumber, class TranslationLabel, class PointType>
constexpr auto operator+(const Point<TranslationNumber, TranslationLabel>& translation, const Halfplane<PointType>& halfplane);

template <class PointType, class TranslationNumber, class TranslationLabel>
constexpr auto operator-(const Halfplane<PointType>& halfplane, const Point<TranslationNumber, TranslationLabel>& translation);

template <class PointType, class Scalar>
    requires(!detail::is_point_v<Scalar>)
constexpr auto operator*(const Halfplane<PointType>& halfplane, const Scalar& scalar);

template <class Scalar, class PointType>
    requires(!detail::is_point_v<Scalar>)
constexpr auto operator*(const Scalar& scalar, const Halfplane<PointType>& halfplane);

template <class PointType, class Scalar>
    requires(!detail::is_point_v<Scalar>)
constexpr auto operator/(const Halfplane<PointType>& halfplane, const Scalar& scalar);

template <class PointType>
std::ostream& operator<<(std::ostream& stream, const Halfplane<PointType>& halfplane);

}  // namespace pgl
