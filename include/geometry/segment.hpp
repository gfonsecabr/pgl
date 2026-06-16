#pragma once

#include "pgl.hpp"

/**
 * @file segment.hpp
 * @brief Public declaration of pgl::Segment.
 *
 * Segment is the basic finite 1D primitive. It stores two endpoints in sorted
 * order and exposes the public contract for segment operations.
 */

#include <array>
#include <cassert>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <functional>
#include <ostream>
#include <type_traits>
#include <utility>
#include <variant>
#include <optional>


namespace pgl {

template <class PointType = Point<>, class Label>
struct Segment;

Segment() -> Segment<Point<>, NoLabel>;

template <class PointType>
Segment(PointType, PointType) -> Segment<PointType, NoLabel>;

template <class PointType, class A>
Segment(PointType, PointType, A) -> Segment<PointType, std::decay_t<A>>;

template <class Number>
Segment(Number, Number, Number, Number) -> Segment<Point<Number>, NoLabel>;

/**
 * @brief Unoriented segment connecting two endpoints.
 *
 * The endpoints are always sorted.
 * The interior of the segment is the segment excluding the endpoints.
 *
 * A segment may carry an optional @ref LabelType value, independent of the
 * endpoint point label. The label is metadata: it is ignored by equality,
 * ordering and hashing, but it is copied across conversions and preserved by
 * in-place transformations.
 *
 * @tparam PointType Endpoint point type.
 * @tparam TLabel Optional label type carried with the segment (`NoLabel` to omit).
 */
template <class TPoint, class TLabel>
struct Segment {
    using PointType = TPoint;
    using NumberType = PointType::NumberType;
    using LabelType = TLabel;

    static_assert(detail::is_point_v<PointType>, "Segment requires pgl::Point endpoints");

    /**
     * @brief Creates the degenerate segment `(0,0)--(0,0)`.
     */
    constexpr Segment() = default;

    /**
     * @brief Creates a segment from two endpoints.
     *
     * The stored endpoints are reordered so that `min() <= max()`.
     *
     * @param first First endpoint.
     * @param second Second endpoint.
     */
    constexpr Segment(PointType first, PointType second) {
        if (second < first) {
            std::swap(first, second);
        }
        points_[0] = std::move(first);
        points_[1] = std::move(second);
    }

    /**
     * @brief Creates a segment from four coordinates.
     *
     * @param x1 X coordinate of the first endpoint.
     * @param y1 Y coordinate of the first endpoint.
     * @param x2 X coordinate of the second endpoint.
     * @param y2 Y coordinate of the second endpoint.
     */
    constexpr Segment(NumberType x1, NumberType y1, NumberType x2, NumberType y2)
        : Segment(PointType(x1, y1), PointType(x2, y2)) {}

    /**
     * @brief Creates a segment from two endpoints and stores a label.
     *
     * The stored endpoints are reordered so that `min() <= max()`.
     *
     * @tparam A Type convertible to @ref LabelType.
     * @param first First endpoint.
     * @param second Second endpoint.
     * @param label Segment label, forwarded into the stored label.
     */
    template <class A>
        requires(detail::has_label_v<LabelType> && std::constructible_from<LabelType, A&&>)
    constexpr Segment(PointType first, PointType second, A&& label)
        : Segment(std::move(first), std::move(second)) {
        label_ = std::forward<A>(label);
    }

    /** @brief Same as the four-coordinate constructor, and stores a label. */
    template <class A>
        requires(detail::has_label_v<LabelType> && std::constructible_from<LabelType, A&&>)
    constexpr Segment(NumberType x1, NumberType y1, NumberType x2, NumberType y2, A&& label)
        : Segment(PointType(x1, y1), PointType(x2, y2), std::forward<A>(label)) {}

    /**
     * @brief Converts a segment with a different point and/or label type.
     *
     * The endpoints are converted to @ref PointType and re-sorted, and the label
     * is copied when both sides carry one.
     */
    template<PointConcept OtherPointType, class OtherLabelType>
        requires(std::constructible_from<PointType, const OtherPointType&>)
    constexpr Segment(const Segment<OtherPointType, OtherLabelType>& other)
        : Segment(PointType(other.min()), PointType(other.max())) {
        label_ = detail::copyLabel<LabelType>(other);
    }

    /**
     * @brief Assigns from a segment with compatible point and label types.
     */
    template<PointConcept OtherPointType, class OtherLabelType>
        requires(std::constructible_from<PointType, const OtherPointType&>)
    constexpr Segment& operator=(const Segment<OtherPointType, OtherLabelType>& other) {
        points_[0] = PointType(other.min());
        points_[1] = PointType(other.max());
        label_ = detail::copyLabel<LabelType>(other);
        return *this;
    }

    /**
     * @brief Returns endpoint `0` or `1`.
     *
     * @param index Endpoint index.
     * @return Reference to the selected endpoint.
     */
    constexpr const PointType& operator[](std::size_t index) const {
        assert(index < size());
        return points_[index];
    }

    /**
     * @brief Returns the number of endpoints (always 2).
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
     * `-1` if no endpoint equals `point`.
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
     * @brief Returns the smallest stored endpoint.
     *
     * @return Reference to the first endpoint.
     */
    constexpr const PointType& min() const {
        return points_[0];
    }

    /**
     * @brief Returns the largest stored endpoint.
     *
     * @return Reference to the second endpoint.
     */
    constexpr const PointType& max() const {
        return points_[1];
    }

    /**
     * @brief Returns an iterator to the first endpoint.
     *
     * @return Pointer to the first endpoint.
     */
    constexpr auto begin() const {
        return points_.cbegin();
    }

    /**
     * @brief Returns an iterator to the first endpoint.
     *
     * @return Pointer to the first endpoint.
     */
    constexpr auto cbegin() const {
        return points_.cbegin();
    }

    /**
     * @brief Returns an iterator past the last endpoint.
     *
     * @return Pointer past the second endpoint.
     */
    constexpr auto end() const {
        return points_.cend();
    }

    /**
     * @brief Returns an iterator past the last endpoint.
     *
     * @return Pointer past the second endpoint.
     */
    constexpr auto cend() const {
        return points_.cend();
    }

    /**
     * @brief Compares two segments by their endpoints; the label is ignored.
     *
     * @param other Segment to compare with.
     * @return `true` if both segments have the same endpoints.
     */
    constexpr bool operator==(const Segment& other) const {
        return points_ == other.points_;
    }

    /**
     * @brief Provides lexicographic ordering on `(x1,y1),(x2,y2)`.
     *
     * The label is ignored, mirroring @ref Point and @ref Disk.
     *
     * @param other Segment to compare with.
     * @return -1, 0, or 1.
     */
    constexpr auto operator<=>(const Segment& other) const {
        return points_ <=> other.points_;
    }

    /**
     * @brief Returns the segment label (read-only).
     * @return Const reference to the stored label.
     */
    template <class A = LabelType>
        requires(detail::has_label_v<A>)
    constexpr const A& label() const {
        return label_;
    }

    /**
     * @brief Returns the segment label.
     * @return Reference to the stored label.
     */
    template <class A = LabelType>
        requires(detail::has_label_v<A>)
    constexpr A& label() {
        return label_;
    }

    /**
     * @brief Converts to the supporting unoriented line.
     *
     * Direct initialization and explicit casts are allowed once
     * `Line` is available.
     *
     * @return Supporting line through both endpoints.
     */
    [[nodiscard]] constexpr explicit operator Line<PointType>() const;

    /**
     * @brief Returns the supporting line.
     *
     * @return Line containing the segment.
     */
    [[nodiscard]] constexpr Line<PointType> asLine() const {
        return static_cast<Line<PointType>>(*this);
    }

    /**
     * @brief Returns the segment rotated by 90k degrees around the origin.
     *
     * Endpoints are re-normalized after rotation.
     *
     * @param k Number of 90-degree CCW rotations (may be negative).
     * @return Rotated segment.
     */
    [[nodiscard]] constexpr Segment rotated90(int k = 1) const;

    /**
     * @brief Rotates the segment by 90k degrees around the origin in place.
     *
     * Endpoints are re-normalized after rotation.
     *
     * @param k Number of 90-degree CCW rotations (may be negative).
     */
    constexpr void rotate90(int k = 1);

    /** @brief Returns the segment with its x-coordinates multiplied by a factor. */
    template <class OtherNumber>
    [[nodiscard]] constexpr Segment scaledUpX(const OtherNumber scalar) const;

    /** @brief Multiplies the segment's x-coordinates by a factor in place. */
    template <class OtherNumber>
    constexpr void scaleUpX(const OtherNumber scalar);

    /** @brief Returns the segment with its y-coordinates multiplied by a factor. */
    template <class OtherNumber>
    [[nodiscard]] constexpr Segment scaledUpY(const OtherNumber scalar) const;

    /** @brief Multiplies the segment's y-coordinates by a factor in place. */
    template <class OtherNumber>
    constexpr void scaleUpY(const OtherNumber scalar);

    /** @brief Returns the segment with its x-coordinates divided by a divisor. */
    template <class OtherNumber>
    [[nodiscard]] constexpr Segment scaledDownX(const OtherNumber scalar) const;

    /** @brief Divides the segment's x-coordinates by a divisor in place. */
    template <class OtherNumber>
    constexpr void scaleDownX(const OtherNumber scalar);

    /** @brief Returns the segment with its y-coordinates divided by a divisor. */
    template <class OtherNumber>
    [[nodiscard]] constexpr Segment scaledDownY(const OtherNumber scalar) const;

    /** @brief Divides the segment's y-coordinates by a divisor in place. */
    template <class OtherNumber>
    constexpr void scaleDownY(const OtherNumber scalar);


    /**
     * @brief Returns whether both endpoints coincide.
     *
     * @return `true` if the segment is a single point.
     */
    [[nodiscard]] constexpr bool isDegenerate() const;

    /**
     * @brief Returns whether the segment is vertical.
     *
     * @return `true` if both endpoints share the same x-coordinate.
     */
    [[nodiscard]] constexpr bool isVertical() const;

    /**
     * @brief Returns whether the segment is horizontal.
     *
     * @return `true` if both endpoints share the same y-coordinate.
     */
    [[nodiscard]] constexpr bool isHorizontal() const;

    /**
     * @brief Returns the area of the segment.
     *
     * A segment is one-dimensional, so its area is always zero.
     *
     * @return Zero.
     */
    [[nodiscard]] constexpr NumberType area() const;

    /**
     * @brief Returns twice the area of the segment.
     *
     * A segment is one-dimensional, so this is always zero.
     *
     * @return Zero.
     */
    [[nodiscard]] constexpr NumberType twiceArea() const;

    /**
     * @brief Returns the squared Euclidean length.
     *
     * @return Squared Euclidean length.
     */
    [[nodiscard]] constexpr auto squaredLength() const;

    /**
     * @brief Returns the Euclidean length.
     *
     * @tparam ApproximateNumber Floating-point return type.
     * @return Euclidean length.
     */
    template <class ApproximateNumber = double>
    [[nodiscard]] ApproximateNumber length() const;

    /**
     * @brief Returns the Manhattan length.
     *
     * @return L1 length.
     */
    [[nodiscard]] constexpr auto lengthL1() const;

    /**
     * @brief Returns the Chebyshev length.
     *
     * @return L infinity length.
     */
    [[nodiscard]] constexpr auto lengthLInf() const;

    /**
     * @brief Returns whether one endpoint equals the given point.
     *
     * @tparam OtherPoint Type of the point.
     *
     * @param point Point to test.
     * @return `true` if the point is an endpoint of the segment.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool verticesContain(const OtherPoint& point) const;

    /**
     * @brief Returns whether the given point is one endpoint.
     *
     * This is a segment-specific alias for @ref verticesContain.
     *
     * @tparam OtherPoint Type of the point.
     *
     * @param point Point to test.
     * @return `true` if the point equals `min()` or `max()`.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool containsEndpoint(const OtherPoint& point) const;

    /**
     * @brief Returns whether the segment boundary contains the given point.
     *
     * For segments, the boundary is exactly the two endpoints.
     *
     * @tparam OtherPoint Type of the point.
     *
     * @param point Point to test.
     * @return `true` if the point is one of the endpoints.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool boundaryContains(const OtherPoint& point) const;

    /**
     * @brief Checks if the segment contains the given Shape on its boundary.
     *
     * For segments, the boundary is exactly the two endpoints, so this is
     * `true` iff all vertices of `other` are endpoints of this segment.
     *
     * @tparam OtherPoint Type of the Shape defining points.
     * @param other Shape to check.
     * @return The result of the shape-specific boundaryContains method.
     */
    // The boundary of a segment is exactly its two endpoints, a finite point
    // set, so it contains no positive-length or two-dimensional shape.
    template<SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool boundaryContains(const OtherSegment&) const { return false; }
    template<OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool boundaryContains(const OtherOrientedSegment&) const { return false; }
    template<LineConcept OtherLine>
    [[nodiscard]] constexpr bool boundaryContains(const OtherLine&) const { return false; }
    template<OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool boundaryContains(const OtherOrientedLine&) const { return false; }
    template<RayConcept OtherRay>
    [[nodiscard]] constexpr bool boundaryContains(const OtherRay&) const { return false; }
    template<HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool boundaryContains(const OtherHalfplane&) const { return false; }
    template<RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool boundaryContains(const OtherRectangle&) const { return false; }
    template<TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr bool boundaryContains(const OtherTriangle&) const { return false; }
    template<ConvexConcept OtherConvex>
    [[nodiscard]] constexpr bool boundaryContains(const OtherConvex&) const { return false; }
    template<PolygonConcept OtherPolygon>
    [[nodiscard]] constexpr bool boundaryContains(const OtherPolygon&) const { return false; }
    template<DiskConcept OtherDisk>
    [[nodiscard]] constexpr bool boundaryContains(const OtherDisk&) const { return false; }

    /**
     * @brief Returns whether the segment contains the given point that is collinear with the segment.
     *
     * Endpoints are included. Undefined behavior if the point is not collinear with the segment.
     *
     * @tparam OtherPoint Type of the point.
     *
     * @param point Point to test.
     * @return `true` if the point lies on the segment.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool containsCollinear(const OtherPoint& point) const;

    /**
     * @brief Returns whether the segment contains the given point.
     *
     * Endpoints are included.
     *
     * @tparam OtherPoint Type of the point.
     *
     * @param point Point to test.
     * @return `true` if the point lies on the segment.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool contains(const OtherPoint& point) const;

    /**
     * @brief Returns whether the segment contains another segment.
     *
     * @tparam OtherPoint Type of the other segment endpoints.
     * @param other Other segment.
     * @return `true` if both endpoints of `other` lie on this segment.
     */
    template<SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool contains(const OtherSegment& other) const;

    /**
     * @brief Returns whether the segment contains an oriented segment.
     *
     * @tparam OtherPoint Type of the other segment endpoints.
     * @param other Other oriented segment.
     * @return `true` if both endpoints of `other` lie on this segment.
     */
    template<OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool contains(const OtherOrientedSegment& other) const;

    /**
     * @brief Checks if the segment contains the given line.
     *
     * @tparam OtherPoint Type of the line defining points.
     * @param other Line to check.
     * @return `true` if the line is degenerate and its unique point lies on this segment.
     */
    template<LineConcept OtherLine>
    [[nodiscard]] constexpr bool contains(const OtherLine& other) const;

    /**
     * @brief Checks if the segment contains the given oriented line.
     *
     * @tparam OtherPoint Type of the oriented line defining points.
     * @param other Oriented line to check.
     * @return `true` if the oriented line is degenerate and its unique point lies on this segment.
     */
    template<OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool contains(const OtherOrientedLine& other) const;

    /**
     * @brief Checks if the segment contains the given ray.
     *
     * @tparam OtherPoint Type of the ray defining points.
     * @param other Ray to check.
     * @return `true` if the ray is degenerate and its unique point lies on this segment.
     */
    template<RayConcept OtherRay>
    [[nodiscard]] constexpr bool contains(const OtherRay& other) const;

    /**
     * @brief Checks if the segment contains the given halfplane.
     *
     * @tparam OtherPoint Type of the halfplane defining points.
     * @param other Halfplane to check.
     * @return `true` if the halfplane is degenerate and its unique point lies on this segment.
     */
    template<HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool contains(const OtherHalfplane& other) const;

    /**
     * @brief Checks if the segment contains the given rectangle.
     *
     * @tparam OtherPoint Type of the rectangle defining points.
     * @param other Rectangle to check.
     * @return `true` if all vertices of `other` lie on this segment.
     */
    template<RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool contains(const OtherRectangle& other) const;

    /**
     * @brief Checks if the segment contains the given triangle.
     *
     * @tparam OtherPoint Type of the triangle defining points.
     * @param other Triangle to check.
     * @return `true` if all vertices of `other` lie on this segment.
     */
    template<TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr bool contains(const OtherTriangle& other) const;

    /**
     * @brief Checks if the segment contains the given convex polygon.
     *
     * Complexity: O(log n) for a convex polygon with n vertices.
     *    
     * @tparam OtherPoint Type of the convex polygon defining points.
     * @param other Convex polygon to check.
     * @return `true` if all vertices of `other` lie on this segment.
     */
    template<ConvexConcept OtherConvex>
    [[nodiscard]] constexpr bool contains(const OtherConvex& other) const;

    /** @brief Polygon overload; see the Convex overload. */
    template<PolygonConcept OtherPolygon>
    [[nodiscard]] constexpr bool contains(const OtherPolygon& other) const;

    /**
     * @brief Checks if the segment contains the given disk.
     *
     * @tparam OtherPoint Type of the disk boundary points.
     * @tparam OtherLabel Label type of the disk.
     * @param other Disk to check.
     * @return `true` if all three boundary points of `other` are equal and the segment contains that point.
     */
    template<DiskConcept OtherDisk>
    [[nodiscard]] constexpr bool contains(const OtherDisk& other) const;

    /**
     * @brief Checks if the segment contains the given Shape.
     *
     * @tparam OtherPoint Type of the Shape defining points.
     * @param other Shape to check.
     * @return The result of the shape-specific contains method.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool contains(const Shape<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool boundaryContains(const Shape<OtherPoint>& other) const;

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

    /**
     * @brief Checks if the segment contains the given point in its interior.
     *
     * @tparam OtherPoint Type of the point.
     * @param point Point to check.
     * @return `true` if the point lies in the interior of this segment.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorContains(const OtherPoint& point) const;

    /**
     * @brief Checks if the segment contains the given segment in its interior.
     *
     * @tparam OtherPoint Type of the other segment endpoints.
     * @param other Other segment.
     * @return `true` if all points of `other` lie in the interior of this segment.
     */
    template<SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool interiorContains(const OtherSegment& other) const;

    /**
     * @brief Checks if the segment contains the given oriented segment in its interior.
     *
     * @tparam OtherPoint Type of the other oriented segment endpoints.
     * @param other Other oriented segment.
     * @return `true` if all points of `other` lie in the interior of this segment.
     */
    template<OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool interiorContains(const OtherOrientedSegment& other) const;

    /**
     * @brief Checks if the segment contains the given triangle in its interior.
     *
     * @tparam OtherPoint Type of the triangle defining points.
     * @param other Triangle to check.
     * @return `true` if all vertices of `other` lie in the interior of this segment.
     */
    template<TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr bool interiorContains(const OtherTriangle& other) const;

    // A segment is one-dimensional: its interior is the open segment, which
    // cannot contain any unbounded or two-dimensional shape.
    template<LineConcept OtherLine>
    [[nodiscard]] constexpr bool interiorContains(const OtherLine&) const { return false; }
    template<OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool interiorContains(const OtherOrientedLine&) const { return false; }
    template<RayConcept OtherRay>
    [[nodiscard]] constexpr bool interiorContains(const OtherRay&) const { return false; }
    template<HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool interiorContains(const OtherHalfplane&) const { return false; }
    template<RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool interiorContains(const OtherRectangle&) const { return false; }
    template<ConvexConcept OtherConvex>
    [[nodiscard]] constexpr bool interiorContains(const OtherConvex&) const { return false; }
    template<PolygonConcept OtherPolygon>
    [[nodiscard]] constexpr bool interiorContains(const OtherPolygon&) const { return false; }
    template<DiskConcept OtherDisk>
    [[nodiscard]] constexpr bool interiorContains(const OtherDisk&) const { return false; }

    /** @brief Dispatches interior containment over the wrapped shape. */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorContains(const Shape<OtherPoint>& other) const;

    /**
     * @brief Returns whether the given point lies on the supporting line.
     *
     * @tparam OtherPoint Type of the point.
     * @param point Point to test.
     * @return `true` if the point is collinear with the segment endpoints.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool collinear(const OtherPoint& point) const;

    /**
     * @brief Returns whether another segment lies on the same supporting line.
     *
     * @tparam OtherPoint Type of the other segment endpoints.     *
     * @param other Other segment.
     * @return `true` if both endpoints of `other` are collinear with this segment.
     */
    template<SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool collinear(const OtherSegment& other) const;

    /**
     * @brief Returns whether an oriented segment lies on the same supporting line.
     *
     * @tparam OtherPoint Type of the other segment endpoints.
     * @param other Other oriented segment.
     * @return `true` if both endpoints of `other` are collinear with this segment.
     */
    template<OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool collinear(const OtherOrientedSegment& other) const;

    /**
     * @brief Returns whether a line lies on the same supporting line.
     * @return `true` if both defining points of `other` are collinear with this segment.
     */
    template<LineConcept OtherLine>
    [[nodiscard]] constexpr bool collinear(const OtherLine& other) const;

    /**
     * @brief Returns whether an oriented line lies on the same supporting line.
     * @return `true` if both defining points of `other` are collinear with this segment.
     */
    template<OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool collinear(const OtherOrientedLine& other) const;

    /**
     * @brief Returns whether a ray lies on the same supporting line.
     * @return `true` if both defining points of `other` are collinear with this segment.
     */
    template<RayConcept OtherRay>
    [[nodiscard]] constexpr bool collinear(const OtherRay& other) const;

    /**
     * @brief Returns the absolute slope of the segment.
     *
     * Undefined behavior for vertical segments.
     *
     * @tparam ResultNumber Coordinate type of the returned slope.
     * @return Absolute slope.
     */
    template <class ResultNumber = NumberType>
    [[nodiscard]] constexpr ResultNumber slope() const;

    /**
     * @brief Returns whether another segment is parallel to this one.
     *
     * @tparam OtherPoint Type of the other segment endpoints.
     * @param other Other segment.
     * @return `true` if both direction vectors are proportional.
     */
    template<SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool parallel(const OtherSegment& other) const;

    /**
     * @brief Returns whether an oriented segment is parallel to this one.
     *
     * @tparam OtherPoint Type of the other segment endpoints.
     * @param other Other oriented segment.
     * @return `true` if both direction vectors are proportional.
     */
    template<OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool parallel(const OtherOrientedSegment& other) const;

    /**
     * @brief Returns whether a line is parallel to this segment.
     *
     * @tparam OtherPoint Type of the other segment endpoints.
     * @param other Other line.
     * @return `true` if both directions are proportional.
     */
    template<LineConcept OtherLine>
    [[nodiscard]] constexpr bool parallel(const OtherLine& other) const;

    /**
     * @brief Returns whether an oriented line is parallel to this segment.
     *
     * @tparam OtherNumber Coordinate type of the line defining points.
     * @tparam OtherPoint::LabelType Label type of the line defining points.
     * @param other Other oriented line.
     * @return `true` if both directions are proportional.
     */
    template<OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool parallel(const OtherOrientedLine& other) const;

    /**
     * @brief Returns whether a ray is parallel to this segment.
     *
     * @tparam OtherNumber Coordinate type of the ray defining points.
     * @tparam OtherPoint::LabelType Label type of the ray defining points.
     * @param other Other ray.
     * @return `true` if both directions are proportional.
     */
    template<RayConcept OtherRay>
    [[nodiscard]] constexpr bool parallel(const OtherRay& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool intersects(const OtherPoint& other) const;

    /**
     * @brief Returns whether two segments intersect.
     *
     * Performs at most 4 orientation tests.
     *
     * @tparam OtherNumber Coordinate type of the other segment endpoints.
     * @tparam OtherPoint::LabelType Label type of the other segment endpoints.
     * @param other Other segment.
     * @return `true` if the segments share at least one point.
     */
    template<SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool intersects(const OtherSegment& other) const;

    [[nodiscard]] constexpr bool intersects(const Shape<PointType>& other) const;

    template<typename OtherShape>
        requires (!PointConcept<OtherShape> && detail::shapeRank<OtherShape> > detail::shapeRank<Segment>)
    [[nodiscard]] constexpr bool intersects(const OtherShape& other) const {
        return other.intersects(*this);
    }

    /** @brief The empty set never meets another shape. */
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool intersects(const EmptyShape<EmptyPoint>&) const {
        return false;
    }

    /**
     * @brief Returns the intersection of two segments.
     *
     * The intersection of two segments may be null, a point, or a segment.
     * Hence, we return an std::optional of std::variant of point and segment.
     * Division is only used if the segments cross.
     *
     * @tparam ResultNumber Number type of the return value.
     * @tparam OtherNumber Coordinate type of the other segment endpoints.
     * @tparam OtherPoint::LabelType Label type of the other segment endpoints.
     * @param other Other segment.
     * @return An std::optional of std::variant of Point and Segment representing the intersection.
     * @warning Divides coordinates after casting to ResultNumber.
     */
    template <class ResultNumber=NumberType, PointConcept OtherPoint>
    [[nodiscard]] constexpr std::optional<Point<ResultNumber, typename PointType::LabelType>>
    intersection(const OtherPoint& other) const;

    template <class ResultNumber=NumberType, SegmentConcept OtherSegment>
    [[nodiscard]] constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherSegment& other) const;

    template <class ResultNumber = NumberType, typename OtherShape>
        requires (!PointConcept<OtherShape>
                  && (detail::shapeRank<OtherShape> > detail::shapeRank<Segment>)
                  && requires(const OtherShape& o, const Segment& self) {
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
     * @brief Returns the value of the y coordinate for a given x, if it exists.
     *
     * If the segment is vertical at the given coordinate x, then
     * we return the min y coordinate.
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
     * If the segment is horizontal at the given coordinate y, then
     * we return the min x coordinate.
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

    /**
     * @brief Returns false.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool separates(const OtherPoint& other) const;

    /**
     * @brief Returns whether this segment separates the other one.
     *
     * Tests whether the this segment intersects the other segment in
     * a single point that is not an endpoint of the other segment.
     * Performs at most 4 orientation tests.
     *
     * @tparam OtherNumber Coordinate type of the other segment endpoints.
     * @tparam OtherPoint::LabelType Label type of the other segment endpoints.
     * @param other Other segment.
     * @return `true` if removing this segment splits the other one.
     */
    template<SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool separates(const OtherSegment& other) const;

    template<OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool separates(const OtherOrientedSegment& other) const;

    template<LineConcept OtherLine>
    [[nodiscard]] constexpr bool separates(const OtherLine& other) const;

    template<OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool separates(const OtherOrientedLine& other) const;

    template<RayConcept OtherRay>
    [[nodiscard]] constexpr bool separates(const OtherRay& other) const;

    template<RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool separates(const OtherRectangle& other) const;

    template<TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr bool separates(const OtherTriangle& other) const;

    template<HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool separates(const OtherHalfplane& other) const;

    template<ConvexConcept OtherConvex>
    [[nodiscard]] constexpr bool separates(const OtherConvex& other) const;

    /**
     * @brief Returns whether removing this segment disconnects the polygon.
     *
     * Walks the polygon boundary once and counts its contacts with the segment:
     * each is either a maximal run of boundary vertices lying on the segment, or
     * an edge that cuts transversally through the segment's interior. Two such
     * contacts mean the segment carries a chord clear across the polygon, so
     * removing it splits the polygon in two.
     *
     * Complexity: O(n) for n polygon vertices.
     */
    template<PolygonConcept OtherPolygon>
    [[nodiscard]] constexpr bool separates(const OtherPolygon& other) const;

    template<DiskConcept OtherDisk>
    [[nodiscard]] constexpr bool separates(const OtherDisk& other) const;

    [[nodiscard]] constexpr bool separates(const Shape<PointType>& other) const;

    /**
     * @brief Returns whether two segments intersect through their interiors.
     *
     * Performs at most 4 orientation tests.
     *
     * @tparam OtherNumber Coordinate type of the other segment endpoints.
     * @tparam OtherPoint::LabelType Label type of the other segment endpoints.
     * @param other Other segment.
     * @return `true` if the segments share at least one point interior to both.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherPoint& other) const;

    template<SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherSegment& other) const;

    template<typename OtherShape>
        requires (!PointConcept<OtherShape> && detail::shapeRank<OtherShape> > detail::shapeRank<Segment>)
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
     * @brief Returns whether two segments cross.
     *
     * Two segments cross if they intersect at a single point that is not
     * an endpoint of either segment. Equivalently, two segments `s,t` cross
     * if `s` separates `t` and `t` separates `s`.
     * Performs at most 4 orientation tests.
     *
     * @tparam OtherNumber Coordinate type of the other segment endpoints.
     * @tparam OtherPoint::LabelType Label type of the other segment endpoints.
     * @param other Other segment.
     * @return `true` if the segments cross through their interiors.
     */
    template<SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool crosses(const OtherSegment& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool crosses(const OtherPoint& other) const;

    template<typename OtherShape>
        requires (!PointConcept<OtherShape> && detail::shapeRank<OtherShape> > detail::shapeRank<Segment>)
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
     * @brief Returns the squared Euclidean distance to a point
     *
     * The computation uses a projection on the supporting line and divides by
     * the squared segment length when the closest point lies in the interior.
     *
     * @tparam ResultNumber Coordinate type of the returned distance (default: NumberType).
     * @tparam OtherPoint Type of the point.
     *
     * @param point Point to measure from.
     * @return Squared Euclidean distance.
     *
     * @warning With an integer @p ResultNumber the exact squared distance is
     *          generally a fraction, so the internal division truncates and the
     *          result is inexact. Request a floating-point or pgl::Rational
     *          result type, e.g. `squaredDistance<double>(point)`, for an
     *          accurate value.
     */
    template <class ResultNumber = NumberType, PointConcept OtherPoint>
    [[nodiscard]] constexpr auto squaredDistance(const OtherPoint& point) const;

    /**
     * @brief Returns the squared Euclidean distance to another segment.
     *
     * @tparam ResultNumber Coordinate type of the returned distance (default: NumberType).
     * @tparam OtherNumber Coordinate type of the other segment endpoints.
     * @tparam OtherPoint::LabelType Label type of the other segment endpoints.
     * @param other Other segment.
     * @return Squared Euclidean distance.
     *
     * @warning With an integer @p ResultNumber the exact squared distance is
     *          generally a fraction, so the internal division truncates and the
     *          result is inexact. Request a floating-point or pgl::Rational
     *          result type, e.g. `squaredDistance<double>(other)`, for an
     *          accurate value.
     */
    template <class ResultNumber = NumberType, SegmentConcept OtherSegment>
    [[nodiscard]] constexpr auto squaredDistance(const OtherSegment& other) const;

    /**
     * @brief Returns the squared Euclidean distance to a higher-ranked shape.
     *
     * Forwards to the other shape's implementation so that each unordered pair
     * needs `squaredDistance` defined only once, on the higher-ranked shape.
     */
    template <class ResultNumber = NumberType, typename OtherShape>
        requires ((detail::shapeRank<OtherShape> > detail::shapeRank<Segment>)
                  && requires(const OtherShape& o, const Segment& self) {
                         o.template squaredDistance<ResultNumber>(self);
                     })
    [[nodiscard]] constexpr auto squaredDistance(const OtherShape& other) const {
        return other.template squaredDistance<ResultNumber>(*this);
    }

    /**
     * @brief Returns the squared Hausdorff distance to another segment.
     *
     * Since the distance to a segment is convex along a segment, the directed
     * Hausdorff distance is attained at an endpoint.
     *
     * @tparam ResultNumber Coordinate type of the returned distance (default: NumberType).
     * @tparam OtherNumber Coordinate type of the other segment endpoints.
     * @tparam OtherPoint::LabelType Label type of the other segment endpoints.
     * @param other Other segment.
     * @return Squared Hausdorff distance.
     *
     * @warning With an integer @p ResultNumber the exact squared distance is
     *          generally a fraction, so the internal division truncates and the
     *          result is inexact. Request a floating-point or pgl::Rational
     *          result type, e.g. `squaredHausdorffDistance<double>(other)`, for
     *          an accurate value.
     */
    template <class ResultNumber = NumberType, SegmentConcept OtherSegment>
    [[nodiscard]] constexpr auto squaredHausdorffDistance(const OtherSegment& other) const;

    /**
     * @brief Returns a segment defining the diameter.
     *
     * For a segment, the diameter is the segment itself.
     *
     * @return This segment.
     */
    [[nodiscard]] constexpr Segment diameter() const;

    /**
     * @brief Returns the midpoint of the segment.
     *
     * @tparam ResultNumber Coordinate type of the midpoint.
     * @return Midpoint with no label.
     * @warning Divides coordinates by 2. Inexact for odd integer coordinates.
     */
    template <class ResultNumber = NumberType>
    [[nodiscard]] constexpr Point<ResultNumber> midpoint() const;

    /**
     * @brief Returns a point inside the segment.
     *
     * For a segment, this is its midpoint.
     *
     * @tparam ResultNumber Coordinate type of the midpoint.
     * @return Midpoint with no label.
     * @warning Divides coordinates by 2. Inexact for odd integer coordinates.
     */
    template <class ResultNumber = NumberType>
    [[nodiscard]] constexpr Point<ResultNumber> pointInside() const;

    /**
     * @brief Returns the bounding box of the segment.
     *
     * @return The minimun rectangle that contains the segment.
     */
    [[nodiscard]] constexpr Rectangle<PointType> bbox() const;

    /**
     * @brief Returns a bounding box of the segment with floating point coordinates.
     *
     * @tparam ResultNumber Floating point type
     * @return A rectangle that contains the segment.
     */
    template <std::floating_point ResultNumber = double>
    [[nodiscard]] constexpr Rectangle<Point<ResultNumber>> fbox() const;

    /**
     * @brief Returns the two endpoints in canonical order.
     *
     * @return Array `{min(), max()}`.
     */
    [[nodiscard]] constexpr std::array<PointType, 2> vertices() const;

    /**
     * @brief Returns the unique boundary edge of the segment.
     *
     * @return Array containing this segment.
     */
    [[nodiscard]] constexpr std::array<Segment, 1> edges() const;

    /**
     * @brief Returns the unique oriented boundary edge in canonical order.
     *
     * @return Array containing `min() -> max()`.
     */
    [[nodiscard]] constexpr std::array<OrientedSegment<PointType>, 1> orientedEdges() const;

    template<PointConcept OtherPoint>
    constexpr Segment& operator+=(const OtherPoint& translation);

    template<PointConcept OtherPoint>
    constexpr Segment& operator-=(const OtherPoint& translation);

    template <class Scalar>
        requires(!detail::is_point_v<Scalar>)
    constexpr Segment& operator*=(const Scalar& scalar);

    template <class Scalar>
        requires(!detail::is_point_v<Scalar>)
    constexpr Segment& operator/=(const Scalar& scalar);

   private:
    template <std::floating_point ResultNumber, class Value>
    static constexpr ResultNumber lowerCoordinateBound(const Value& value);

    template <std::floating_point ResultNumber, class Value>
    static constexpr ResultNumber upperCoordinateBound(const Value& value);

    template<SegmentConcept OtherSegment>
    constexpr bool boundingBoxesOverlap(const OtherSegment& other) const;

    // Returns 0 for no overlap, 1 for overlap but no cross, 2 for cross
    template<SegmentConcept OtherSegment>
    constexpr int boundingBoxesCross(const OtherSegment& other) const;

    std::array<PointType,2> points_{};
    [[no_unique_address]] LabelType label_{};
};

/**
 * @brief Translates a segment by a point.
 *
 * @tparam Number Coordinate type of the segment endpoints.
 * @tparam Label Label type of the segment endpoints.
 * @tparam TranslationNumber Coordinate type of the translation point.
 * @tparam TranslationLabel Label type of the translation point.
 * @param segment Segment to translate.
 * @param translation Translation vector.
 * @return Translated segment.
 */
template <class PointType, class LabelType, class TranslationNumber, class TranslationLabel>
constexpr auto operator+(const Segment<PointType, LabelType>& segment, const Point<TranslationNumber, TranslationLabel>& translation);

/**
 * @brief Translates a segment by a point written on the left.
 *
 * @tparam TranslationNumber Coordinate type of the translation point.
 * @tparam TranslationLabel Label type of the translation point.
 * @tparam Number Coordinate type of the segment endpoints.
 * @tparam Label Label type of the segment endpoints.
 * @param translation Translation vector.
 * @param segment Segment to translate.
 * @return Translated segment.
 */
template <class TranslationNumber, class TranslationLabel, class PointType, class LabelType>
constexpr auto operator+(const Point<TranslationNumber, TranslationLabel>& translation, const Segment<PointType, LabelType>& segment);

/**
 * @brief Translates a segment by the opposite of a point.
 *
 * @tparam Number Coordinate type of the segment endpoints.
 * @tparam Label Label type of the segment endpoints.
 * @tparam TranslationNumber Coordinate type of the translation point.
 * @tparam TranslationLabel Label type of the translation point.
 * @param segment Segment to translate.
 * @param translation Translation vector to subtract.
 * @return Translated segment.
 */
template <class PointType, class LabelType, class TranslationNumber, class TranslationLabel>
constexpr auto operator-(const Segment<PointType, LabelType>& segment, const Point<TranslationNumber, TranslationLabel>& translation);

/**
 * @brief Scales a segment by a scalar.
 *
 * @tparam Number Coordinate type of the segment endpoints.
 * @tparam Label Label type of the segment endpoints.
 * @tparam Scalar Scalar type.
 * @param segment Segment to scale.
 * @param scalar Scale factor.
 * @return Scaled segment.
 */
template <class PointType, class LabelType, class Scalar>
    requires(!detail::is_point_v<Scalar>)
constexpr auto operator*(const Segment<PointType, LabelType>& segment, const Scalar& scalar);

/**
 * @brief Scales a segment by a scalar written on the left.
 *
 * @tparam Scalar Scalar type.
 * @tparam Number Coordinate type of the segment endpoints.
 * @tparam Label Label type of the segment endpoints.
 * @param scalar Scale factor.
 * @param segment Segment to scale.
 * @return Scaled segment.
 */
template <class Scalar, class PointType, class LabelType>
    requires(!detail::is_point_v<Scalar>)
constexpr auto operator*(const Scalar& scalar, const Segment<PointType, LabelType>& segment);

/**
 * @brief Divides both endpoints by a scalar.
 *
 * @tparam Number Coordinate type of the segment endpoints.
 * @tparam Label Label type of the segment endpoints.
 * @tparam Scalar Scalar type.
 * @param segment Segment to divide.
 * @param scalar Divisor.
 * @return Scaled segment.
 */
template <class PointType, class LabelType, class Scalar>
    requires(!detail::is_point_v<Scalar>)
constexpr auto operator/(const Segment<PointType, LabelType>& segment, const Scalar& scalar);

/**
 * @brief Streams a segment as `p--q`.
 *
 * @tparam Number Coordinate type of the segment endpoints.
 * @tparam Label Label type of the segment endpoints.
 * @param stream Output stream.
 * @param segment Segment to print.
 * @return The output stream.
 */
template <class PointType, class LabelType>
std::ostream& operator<<(std::ostream& stream, const Segment<PointType, LabelType>& segment);

}  // pgl
