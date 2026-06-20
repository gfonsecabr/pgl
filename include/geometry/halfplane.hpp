#ifndef PGL_HPP_INCLUDED
// Entered out of order (before pgl.hpp): defer to the umbrella header,
// which re-includes this file at the correct layer.
#include "pgl.hpp"
#else
#ifndef PGL_GEOMETRY_HALFPLANE_HPP
#define PGL_GEOMETRY_HALFPLANE_HPP

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


namespace pgl {

template <class PointType = Point<>, class Label>
struct Halfplane;

Halfplane() -> Halfplane<Point<>, NoLabel>;

template <class PointType>
Halfplane(PointType, PointType) -> Halfplane<PointType, NoLabel>;

template <class PointType, class A>
Halfplane(PointType, PointType, A) -> Halfplane<PointType, std::decay_t<A>>;

template <class Number>
Halfplane(Number, Number, Number, Number) -> Halfplane<Point<Number>, NoLabel>;

/**
 * @brief Closed half-plane stored by an oriented boundary line.
 *
 * The half-plane contains all points on the left side of the oriented line
 * from @ref source to @ref target, including the boundary itself.
 *
 * @tparam PointType Defining point type.
 */
template <class PointType_, class TLabel>
struct Halfplane {
    using PointType = PointType_;
    using NumberType = PointType::NumberType;
    using LabelType = TLabel;
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

    /**
     * @brief Creates a half-plane from an oriented boundary and stores a label.
     *
     * The point order is preserved exactly as provided.
     *
     * @tparam A Type convertible to @ref LabelType.
     */
    template <class A>
        requires(detail::has_label_v<LabelType> && std::constructible_from<LabelType, A&&>)
    constexpr Halfplane(PointType source, PointType target, A&& label)
        : points_{std::move(source), std::move(target)}, label_(std::forward<A>(label)) {}

    /** @brief Same as the four-coordinate constructor, and stores a label. */
    template <class A>
        requires(detail::has_label_v<LabelType> && std::constructible_from<LabelType, A&&>)
    constexpr Halfplane(NumberType x1, NumberType y1, NumberType x2, NumberType y2, A&& label)
        : Halfplane(PointType(x1, y1), PointType(x2, y2), std::forward<A>(label)) {}

    /**
     * @brief Converts a half-plane with a different point and/or label type.
     *
     * The boundary points are converted to @ref PointType, preserving their
     * order, and the label is copied when both sides carry one.
     */
    template<PointConcept OtherPointType, class OtherLabelType>
        requires(std::constructible_from<PointType, const OtherPointType&>)
    constexpr Halfplane(const Halfplane<OtherPointType, OtherLabelType>& other)
        : Halfplane(PointType(other.source()), PointType(other.target())) {
        label_ = detail::copyLabel<LabelType>(other);
    }

    template<PointConcept OtherPointType, class OtherLabelType>
        requires(std::constructible_from<PointType, const OtherPointType&>)
    constexpr Halfplane& operator=(const Halfplane<OtherPointType, OtherLabelType>& other) {
        points_[0] = PointType(other.source());
        points_[1] = PointType(other.target());
        label_ = detail::copyLabel<LabelType>(other);
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
     * @brief Returns the half-plane label.
     *
     * The label is mutable even through a const half-plane: it is metadata that
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

    // The boundary of a half-plane is its edge line. A 1D shape lies on the
    // boundary iff its defining points do; an area must additionally be
    // degenerate (collapsed onto that line). See the implementations.
    template<SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool boundaryContains(const OtherSegment& other) const;
    template<LineConcept OtherLine>
    [[nodiscard]] constexpr bool boundaryContains(const OtherLine& other) const;
    template<OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool boundaryContains(const OtherOrientedSegment& other) const;
    template<OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool boundaryContains(const OtherOrientedLine& other) const;
    template<RayConcept OtherRay>
    [[nodiscard]] constexpr bool boundaryContains(const OtherRay& other) const;
    template<RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool boundaryContains(const OtherRectangle& other) const;
    template<TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr bool boundaryContains(const OtherTriangle& other) const;
    template<HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool boundaryContains(const OtherHalfplane& other) const;
    template<ConvexConcept OtherConvex>
    [[nodiscard]] constexpr bool boundaryContains(const OtherConvex&) const { return false; }
    template<PolygonConcept OtherPolygon>
    [[nodiscard]] constexpr bool boundaryContains(const OtherPolygon&) const { return false; }
    template<DiskConcept OtherDisk>
    [[nodiscard]] constexpr bool boundaryContains(const OtherDisk&) const { return false; }

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool contains(const OtherPoint& point) const;

    template<LineConcept OtherLine>
    [[nodiscard]] constexpr bool contains(const OtherLine& other) const;

    template<OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool contains(const OtherOrientedLine& other) const;

    template<SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool contains(const OtherSegment& other) const;

    template<OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool contains(const OtherOrientedSegment& other) const;

    template<RayConcept OtherRay>
    [[nodiscard]] constexpr bool contains(const OtherRay& other) const;

    template<RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool contains(const OtherRectangle& other) const;

    template<HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool contains(const OtherHalfplane& other) const;

    template<TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr bool contains(const OtherTriangle& other) const;

    template<ConvexConcept OtherConvex>
    [[nodiscard]] constexpr bool contains(const OtherConvex& other) const;

    /** @brief Polygon overload; see the Convex overload. */
    template<PolygonConcept OtherPolygon>
    [[nodiscard]] constexpr bool contains(const OtherPolygon& other) const;

    template<DiskConcept OtherDisk>
    [[nodiscard]] constexpr bool contains(const OtherDisk& other) const;

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

    template<LineConcept OtherLine>
    [[nodiscard]] constexpr bool interiorContains(const OtherLine& other) const;

    template<OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool interiorContains(const OtherOrientedLine& other) const;

    template<SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool interiorContains(const OtherSegment& other) const;

    template<OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool interiorContains(const OtherOrientedSegment& other) const;

    template<RayConcept OtherRay>
    [[nodiscard]] constexpr bool interiorContains(const OtherRay& other) const;

    template<RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool interiorContains(const OtherRectangle& other) const;

    template<HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool interiorContains(const OtherHalfplane& other) const;

    template<TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr bool interiorContains(const OtherTriangle& other) const;

    template<ConvexConcept OtherConvex>
    [[nodiscard]] constexpr bool interiorContains(const OtherConvex& other) const;

    template<DiskConcept OtherDisk>
    [[nodiscard]] constexpr bool interiorContains(const OtherDisk& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool intersects(const OtherPoint& other) const;

    template<LineConcept OtherLine>
    [[nodiscard]] constexpr bool intersects(const OtherLine& other) const;

    template<OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool intersects(const OtherOrientedLine& other) const;

    template<SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool intersects(const OtherSegment& other) const;

    template<OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool intersects(const OtherOrientedSegment& other) const;

    template<RayConcept OtherRay>
    [[nodiscard]] constexpr bool intersects(const OtherRay& other) const;

    template<HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool intersects(const OtherHalfplane& other) const;

    [[nodiscard]] constexpr bool intersects(const Shape<PointType>& other) const;

    template<typename OtherShape>
        requires (!PointConcept<OtherShape> && detail::shapeRank<OtherShape> > detail::shapeRank<Halfplane>)
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

    template<LineConcept OtherLine>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherLine& other) const;

    template<OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherOrientedLine& other) const;

    template<SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherSegment& other) const;

    template<OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherOrientedSegment& other) const;

    template<RayConcept OtherRay>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherRay& other) const;

    template<HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherHalfplane& other) const;

    template<typename OtherShape>
        requires (!PointConcept<OtherShape> && detail::shapeRank<OtherShape> > detail::shapeRank<Halfplane>)
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherShape& other) const {
        return other.interiorsIntersect(*this);
    }

    /** @brief The empty set never meets another shape. */
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool interiorsIntersect(const EmptyShape<EmptyPoint>&) const {
        return false;
    }

    [[nodiscard]] constexpr bool interiorsIntersect(const Shape<PointType>& other) const;

    template<LineConcept OtherLine>
    [[nodiscard]] constexpr bool separates(const OtherLine& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool separates(const OtherPoint& other) const;

    template<OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool separates(const OtherOrientedLine& other) const;

    template<SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool separates(const OtherSegment& other) const;

    template<OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool separates(const OtherOrientedSegment& other) const;

    template<RayConcept OtherRay>
    [[nodiscard]] constexpr bool separates(const OtherRay& other) const;

    template<RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool separates(const OtherRectangle& other) const;

    template<HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool separates(const OtherHalfplane& other) const;

    template<TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr bool separates(const OtherTriangle& other) const;

    template<ConvexConcept OtherConvex>
    [[nodiscard]] constexpr bool separates(const OtherConvex& other) const;

    /** @brief Polygon overload; see the Convex overload. */
    template<PolygonConcept OtherPolygon>
    [[nodiscard]] constexpr bool separates(const OtherPolygon& other) const;

    [[nodiscard]] constexpr bool separates(const Shape<PointType>& other) const;

    // --- not-yet-implemented predicate pairs (throw); see implementation ---
    template<PolygonConcept OtherPolygon>
    [[nodiscard]] constexpr bool interiorContains(const OtherPolygon& other) const;


    template<LineConcept OtherLine>
    [[nodiscard]] constexpr bool crosses(const OtherLine& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool crosses(const OtherPoint& other) const;

    template<OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool crosses(const OtherOrientedLine& other) const;

    template<SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool crosses(const OtherSegment& other) const;

    template<OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool crosses(const OtherOrientedSegment& other) const;

    template<RayConcept OtherRay>
    [[nodiscard]] constexpr bool crosses(const OtherRay& other) const;

    template<HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool crosses(const OtherHalfplane& other) const;

    template<typename OtherShape>
        requires (!PointConcept<OtherShape> && detail::shapeRank<OtherShape> > detail::shapeRank<Halfplane>)
    [[nodiscard]] constexpr bool crosses(const OtherShape& other) const {
        return other.crosses(*this);
    }

    /** @brief The empty set never meets another shape. */
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool crosses(const EmptyShape<EmptyPoint>&) const {
        return false;
    }

    [[nodiscard]] constexpr bool crosses(const Shape<PointType>& other) const;

    template <class ResultNumber = NumberType, PointConcept OtherPoint>
    [[nodiscard]] constexpr std::optional<Point<ResultNumber, typename PointType::LabelType>>
    intersection(const OtherPoint& other) const;

    template <class ResultNumber = NumberType, LineConcept OtherLine>
    [[nodiscard]] constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Line<Point<ResultNumber, typename PointType::LabelType>>, Ray<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherLine& other) const;

    template <class ResultNumber = NumberType, OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Line<Point<ResultNumber, typename PointType::LabelType>>, Ray<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherOrientedLine& other) const;

    template <class ResultNumber = NumberType, SegmentConcept OtherSegment>
    [[nodiscard]] constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherSegment& other) const;

    template <class ResultNumber = NumberType, OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherOrientedSegment& other) const;

    template <class ResultNumber = NumberType, RayConcept OtherRay>
    [[nodiscard]] constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>, Ray<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherRay& other) const;

    // Intersecting two half-planes is intentionally unsupported. Deleting the
    // overload makes such a call a compile error rather than letting it fall
    // through to the reversing fallback below, which would recurse forever.
    template <class ResultNumber = NumberType, HalfplaneConcept OtherHalfplane>
    constexpr auto intersection(const OtherHalfplane& other) const = delete;

    template <class ResultNumber = NumberType, typename OtherShape>
        requires (!PointConcept<OtherShape>
                  && (detail::shapeRank<OtherShape> > detail::shapeRank<Halfplane>)
                  && requires(const OtherShape& o, const Halfplane& self) {
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
     * @brief Returns the squared Euclidean distance to the given shape.
     *
     * Zero when the half-plane intersects the shape; otherwise the shape lies
     * entirely outside, so its distance to the half-plane equals its distance to
     * the boundary line @ref asLine().
     *
     * @tparam ResultNumber Coordinate type of the returned distance (default: NumberType).
     *
     * @warning With an integer @p ResultNumber the exact squared distance is
     *          generally a fraction, so the internal division truncates and the
     *          result is inexact. Request a floating-point or pgl::Rational
     *          result type, e.g. `squaredDistance<double>(point)`, for an
     *          accurate value.
     */
    template <class ResultNumber = NumberType, PointConcept OtherPoint>
    [[nodiscard]] constexpr auto squaredDistance(const OtherPoint& point) const;

    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, SegmentConcept OtherSegment>
    [[nodiscard]] constexpr auto squaredDistance(const OtherSegment& other) const;

    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr auto squaredDistance(const OtherOrientedSegment& other) const;

    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, LineConcept OtherLine>
    [[nodiscard]] constexpr auto squaredDistance(const OtherLine& other) const;

    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr auto squaredDistance(const OtherOrientedLine& other) const;

    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, RayConcept OtherRay>
    [[nodiscard]] constexpr auto squaredDistance(const OtherRay& other) const;

    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr auto squaredDistance(const OtherHalfplane& other) const;

    /**
     * @brief Returns the squared Euclidean distance to a higher-ranked shape.
     *
     * Forwards to the other shape's implementation so that each unordered pair
     * needs `squaredDistance` defined only once, on the higher-ranked shape.
     */
    template <class ResultNumber = NumberType, typename OtherShape>
        requires ((detail::shapeRank<OtherShape> > detail::shapeRank<Halfplane>)
                  && requires(const OtherShape& o, const Halfplane& self) {
                         o.template squaredDistance<ResultNumber>(self);
                     })
    [[nodiscard]] constexpr auto squaredDistance(const OtherShape& other) const {
        return other.template squaredDistance<ResultNumber>(*this);
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
    [[no_unique_address]] mutable LabelType label_{};
};

template <class PointType, class LabelType, class TranslationNumber, class TranslationLabel>
constexpr auto operator+(const Halfplane<PointType, LabelType>& halfplane, const Point<TranslationNumber, TranslationLabel>& translation);

template <class TranslationNumber, class TranslationLabel, class PointType, class LabelType>
constexpr auto operator+(const Point<TranslationNumber, TranslationLabel>& translation, const Halfplane<PointType, LabelType>& halfplane);

template <class PointType, class LabelType, class TranslationNumber, class TranslationLabel>
constexpr auto operator-(const Halfplane<PointType, LabelType>& halfplane, const Point<TranslationNumber, TranslationLabel>& translation);

template <class PointType, class LabelType, class Scalar>
    requires(!detail::is_point_v<Scalar>)
constexpr auto operator*(const Halfplane<PointType, LabelType>& halfplane, const Scalar& scalar);

template <class Scalar, class PointType, class LabelType>
    requires(!detail::is_point_v<Scalar>)
constexpr auto operator*(const Scalar& scalar, const Halfplane<PointType, LabelType>& halfplane);

template <class PointType, class LabelType, class Scalar>
    requires(!detail::is_point_v<Scalar>)
constexpr auto operator/(const Halfplane<PointType, LabelType>& halfplane, const Scalar& scalar);

template <class PointType, class LabelType>
std::ostream& operator<<(std::ostream& stream, const Halfplane<PointType, LabelType>& halfplane);

}  // namespace pgl

#endif // PGL_GEOMETRY_HALFPLANE_HPP
#endif // PGL_HPP_INCLUDED
