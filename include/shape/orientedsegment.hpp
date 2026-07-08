#pragma once

#include "shape/segment.hpp"

/**
 * @file orientedsegment.hpp
 * @brief Public declaration of pgl::OrientedSegment.
 *
 * This is the directed counterpart to Segment. It preserves source-to-target
 * order for orientation-sensitive operations.
 */


namespace pgl {

template <class PointType = Point<>, class Label>
struct OrientedSegment;

OrientedSegment() -> OrientedSegment<Point<>, NoLabel>;

template <class PointType>
OrientedSegment(PointType, PointType) -> OrientedSegment<PointType, NoLabel>;

template <class PointType, class A>
OrientedSegment(PointType, PointType, A) -> OrientedSegment<PointType, std::decay_t<A>>;

template <class Number>
OrientedSegment(Number, Number, Number, Number) -> OrientedSegment<Point<Number>, NoLabel>;

/**
 * @brief Oriented segment connecting two endpoints in source-to-target order.
 *
 * Unlike @ref Segment, the stored endpoint order is preserved.
 *
 * Like @ref Segment, an oriented segment may carry an optional @ref LabelType
 * value, independent of the endpoint point label. The label is metadata: it is
 * ignored by equality, ordering and hashing, but it is copied across
 * conversions and preserved by in-place transformations.
 *
 * @tparam PointType Endpoint point type.
 * @tparam TLabel Optional label type carried with the segment (`NoLabel` to omit).
 */
template <class PointType_, class TLabel>
struct OrientedSegment {
    using PointType = PointType_;
    using NumberType = PointType::NumberType;
    using LabelType = TLabel;

    static_assert(detail::is_point_v<PointType>, "OrientedSegment requires pgl::Point endpoints");

    /**
     * @brief Creates the degenerate oriented segment `(0,0)->(0,0)`.
     */
    constexpr OrientedSegment() = default;

    /**
     * @brief Creates an oriented segment from a source and target.
     *
     * The endpoint order is preserved exactly as provided.
     *
     * @param source Source endpoint.
     * @param target Target endpoint.
     */
    constexpr OrientedSegment(PointType source, PointType target)
        : points_{std::move(source), std::move(target)} {}

    /**
     * @brief Creates an oriented segment from four coordinates.
     *
     * @param x1 X coordinate of the source.
     * @param y1 Y coordinate of the source.
     * @param x2 X coordinate of the target.
     * @param y2 Y coordinate of the target.
     */
    constexpr OrientedSegment(NumberType x1, NumberType y1, NumberType x2, NumberType y2)
        : OrientedSegment(PointType(x1, y1), PointType(x2, y2)) {}

    /**
     * @brief Creates an oriented segment from a source and target and stores a label.
     *
     * The endpoint order is preserved exactly as provided.
     *
     * @tparam A Type convertible to @ref LabelType.
     * @param source Source endpoint.
     * @param target Target endpoint.
     * @param label Segment label, forwarded into the stored label.
     */
    template <class A>
        requires(detail::has_label_v<LabelType> && std::constructible_from<LabelType, A&&>)
    constexpr OrientedSegment(PointType source, PointType target, A&& label)
        : points_{std::move(source), std::move(target)}, label_(std::forward<A>(label)) {}

    /** @brief Same as the four-coordinate constructor, and stores a label. */
    template <class A>
        requires(detail::has_label_v<LabelType> && std::constructible_from<LabelType, A&&>)
    constexpr OrientedSegment(NumberType x1, NumberType y1, NumberType x2, NumberType y2, A&& label)
        : OrientedSegment(PointType(x1, y1), PointType(x2, y2), std::forward<A>(label)) {}

    /**
     * @brief Converts an oriented segment with a different point and/or label type.
     *
     * The endpoints are converted to @ref PointType, preserving their order, and
     * the label is copied when both sides carry one.
     */
    template<PointConcept OtherPointType, class OtherLabelType>
        requires(std::constructible_from<PointType, const OtherPointType&>)
    constexpr OrientedSegment(const OrientedSegment<OtherPointType, OtherLabelType>& other)
        : OrientedSegment(PointType(other.source()), PointType(other.target())) {
        label_ = detail::copyLabel<LabelType>(other);
    }

    /**
     * @brief Assigns from an oriented segment with compatible point and label types.
     */
    template<PointConcept OtherPointType, class OtherLabelType>
        requires(std::constructible_from<PointType, const OtherPointType&>)
    constexpr OrientedSegment& operator=(const OrientedSegment<OtherPointType, OtherLabelType>& other) {
        points_[0] = PointType(other.source());
        points_[1] = PointType(other.target());
        label_ = detail::copyLabel<LabelType>(other);
        return *this;
    }

    /**
     * @brief Returns endpoint `0` for the source and `1` for the target.
     *
     * @param index Endpoint index.
     * @return Reference to the selected endpoint.
     */
    constexpr const PointType& operator[](std::size_t index) const {
        assert(index < size());
        return points_[index];
    }
    /** @copydoc operator[](std::size_t) const */
    constexpr PointType& operator[](std::size_t index) {
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
    constexpr PointType& get(std::ptrdiff_t index) {
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
     * @brief Returns the source endpoint.
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
     * @brief Returns the target endpoint.
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
     * @brief Returns the lexicographically smallest endpoint.
     *
     * @return Reference to the smaller endpoint.
     */
    constexpr const PointType& min() const {
        return target() < source() ? target() : source();
    }

    /**
     * @brief Returns the lexicographically largest endpoint.
     *
     * @return Reference to the larger endpoint.
     */
    constexpr const PointType& max() const {
        return source() < target() ? target() : source();
    }

    /**
     * @brief Returns the opposite orientation of the same geometric segment.
     *
     * @return Reversed oriented segment.
     */
    constexpr OrientedSegment opposite() const {
        return OrientedSegment(target(), source());
    }

    /**
     * @brief Returns an iterator to the source endpoint.
     *
     * @return Pointer to the source endpoint.
     */
    constexpr auto begin() const {
        return points_.cbegin();
    }
    constexpr auto begin() {
        return points_.begin();
    }

    /**
     * @brief Returns an iterator to the source endpoint.
     *
     * @return Pointer to the source endpoint.
     */
    constexpr auto cbegin() const {
        return points_.cbegin();
    }

    /**
     * @brief Returns an iterator past the target endpoint.
     *
     * @return Pointer past the target endpoint.
     */
    constexpr auto end() const {
        return points_.cend();
    }
    constexpr auto end() {
        return points_.end();
    }

    /**
     * @brief Returns an iterator past the target endpoint.
     *
     * @return Pointer past the target endpoint.
     */
    constexpr auto cend() const {
        return points_.cend();
    }

    /**
     * @brief Compares two oriented segments by their endpoints; the label is ignored.
     *
     * @param other Oriented segment to compare with.
     * @return `true` if both segments have the same ordered endpoints.
     */
    constexpr bool operator==(const OrientedSegment& other) const {
        return points_ == other.points_;
    }

    /**
     * @brief Provides lexicographic ordering on `(source, target)`.
     *
     * The label is ignored, mirroring @ref Point and @ref Segment.
     *
     * @param other Oriented segment to compare with.
     * @return -1, 0, or 1.
     */
    constexpr auto operator<=>(const OrientedSegment& other) const {
        return points_ <=> other.points_;
    }

    /**
     * @brief Returns the segment label.
     *
     * The label is mutable even through a const segment: it is metadata that
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
     * @brief Converts to the unordered geometric segment with the same endpoints.
     *
     * Direct initialization and explicit casts are allowed.
     *
     * @return Unordered segment spanning the same endpoints.
     */
    [[nodiscard]] constexpr explicit operator Segment<PointType>() const {
        return Segment(source(), target());
    }

    /**
     * @brief Returns the segment without orientation.
     *
     * @return Unordered segment spanning the same endpoints.
     */
    [[nodiscard]] constexpr Segment<PointType> asSegment() const {
        return static_cast<Segment<PointType>>(*this);
    }

    /**
     * @brief Converts to the oriented supporting line.
     *
     * @return Oriented line containing the segment and preserving its direction.
     */
    [[nodiscard]] constexpr explicit operator OrientedLine<PointType>() const;

    /**
     * @brief Returns the oriented supporting line.
     *
     * @return Oriented line containing the segment and preserving its direction.
     */
    [[nodiscard]] constexpr OrientedLine<PointType> asOrientedLine() const {
        return static_cast<OrientedLine<PointType>>(*this);
    }

    /**
     * @brief Returns the supporting line without orientation.
     *
     * @return Line containing the segment.
     */
    [[nodiscard]] constexpr Line<PointType> asLine() const {
        return Line<PointType>(source(), target());
    }

    /**
     * @brief Converts to the ray with the same source and direction.
     *
     * @return Ray starting at @ref source and going through @ref target.
     */
    [[nodiscard]] constexpr explicit operator Ray<PointType>() const;

    /**
     * @brief Returns the ray with the same source and direction.
     *
     * @return Ray starting at @ref source and going through @ref target.
     */
    [[nodiscard]] constexpr Ray<PointType> asRay() const {
        return static_cast<Ray<PointType>>(*this);
    }

    /**
     * @brief Returns the segment rotated by 90k degrees around the origin.
     *
     * @param k Number of 90-degree CCW rotations (may be negative).
     * @return Rotated segment.
     */
    [[nodiscard]] constexpr OrientedSegment rotated90(int k = 1) const;

    /**
     * @brief Rotates the segment by 90k degrees around the origin in place.
     *
     * @param k Number of 90-degree CCW rotations (may be negative).
     */
    constexpr void rotate90(int k = 1);

    /** @brief Returns the segment with its x-coordinates multiplied by a factor. */
    template <class OtherNumber>
    [[nodiscard]] constexpr OrientedSegment scaledUpX(const OtherNumber scalar) const;

    /** @brief Multiplies the segment's x-coordinates by a factor in place. */
    template <class OtherNumber>
    constexpr void scaleUpX(const OtherNumber scalar);

    /** @brief Returns the segment with its y-coordinates multiplied by a factor. */
    template <class OtherNumber>
    [[nodiscard]] constexpr OrientedSegment scaledUpY(const OtherNumber scalar) const;

    /** @brief Multiplies the segment's y-coordinates by a factor in place. */
    template <class OtherNumber>
    constexpr void scaleUpY(const OtherNumber scalar);

    /** @brief Returns the segment with its x-coordinates divided by a divisor. */
    template <class OtherNumber>
    [[nodiscard]] constexpr OrientedSegment scaledDownX(const OtherNumber scalar) const;

    /** @brief Divides the segment's x-coordinates by a divisor in place. */
    template <class OtherNumber>
    constexpr void scaleDownX(const OtherNumber scalar);

    /** @brief Returns the segment with its y-coordinates divided by a divisor. */
    template <class OtherNumber>
    [[nodiscard]] constexpr OrientedSegment scaledDownY(const OtherNumber scalar) const;

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
     * @return `true` if the point equals @ref source or @ref target.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool containsEndpoint(const OtherPoint& point) const;

    /**
     * @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B).
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

    // The boundary of an oriented segment is its two endpoints, a finite point
    // set, so it contains no positive-length or two-dimensional shape.
    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool boundaryContains(const OtherSegment&) const { return false; }
    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool boundaryContains(const OtherOrientedSegment&) const { return false; }
    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<LineConcept OtherLine>
    [[nodiscard]] constexpr bool boundaryContains(const OtherLine&) const { return false; }
    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool boundaryContains(const OtherOrientedLine&) const { return false; }
    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<RayConcept OtherRay>
    [[nodiscard]] constexpr bool boundaryContains(const OtherRay&) const { return false; }
    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool boundaryContains(const OtherHalfplane&) const { return false; }
    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool boundaryContains(const OtherRectangle&) const { return false; }
    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr bool boundaryContains(const OtherTriangle&) const { return false; }
    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<ConvexConcept OtherConvex>
    [[nodiscard]] constexpr bool boundaryContains(const OtherConvex&) const { return false; }
    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<PolygonConcept OtherPolygon>
    [[nodiscard]] constexpr bool boundaryContains(const OtherPolygon&) const { return false; }
    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
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
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
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
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     *
     * @tparam OtherNumber Coordinate type of the other segment endpoints.
     * @tparam OtherPoint::LabelType Label type of the other segment endpoints.
     * @param other Other segment.
     * @return `true` if both endpoints of `other` lie on this segment.
     */
    template<SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool contains(const OtherSegment& other) const;

    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     *
     * @tparam OtherNumber Coordinate type of the other segment endpoints.
     * @tparam OtherPoint::LabelType Label type of the other segment endpoints.
     * @param other Other oriented segment.
     * @return `true` if both endpoints of `other` lie on this segment.
     */
    template<OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool contains(const OtherOrientedSegment& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template<LineConcept OtherLine>
    [[nodiscard]] constexpr bool contains(const OtherLine& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template<OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool contains(const OtherOrientedLine& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template<RayConcept OtherRay>
    [[nodiscard]] constexpr bool contains(const OtherRay& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template<HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool contains(const OtherHalfplane& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
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
    template<SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool interiorContains(const OtherSegment& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template<OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool interiorContains(const OtherOrientedSegment& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template<LineConcept OtherLine>
    [[nodiscard]] constexpr bool interiorContains(const OtherLine& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template<OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool interiorContains(const OtherOrientedLine& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template<RayConcept OtherRay>
    [[nodiscard]] constexpr bool interiorContains(const OtherRay& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template<HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool interiorContains(const OtherHalfplane& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template<RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool interiorContains(const OtherRectangle& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template<TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr bool interiorContains(const OtherTriangle& other) const;

    /** @brief Returns whether the given point is collinear with the oriented segment. */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool collinear(const OtherPoint& point) const;

    /** @brief Returns whether the given segment is collinear with the oriented segment. */
    template<SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool collinear(const OtherSegment& other) const;

    /** @brief Returns whether another oriented segment is collinear with this oriented segment. */
    template<OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool collinear(const OtherOrientedSegment& other) const;

    /** @brief Returns whether the given line is collinear with the oriented segment. */
    template<LineConcept OtherLine>
    [[nodiscard]] constexpr bool collinear(const OtherLine& other) const;

    /** @brief Returns whether the given oriented line is collinear with the oriented segment. */
    template<OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool collinear(const OtherOrientedLine& other) const;

    /** @brief Returns whether the given ray is collinear with the oriented segment. */
    template<RayConcept OtherRay>
    [[nodiscard]] constexpr bool collinear(const OtherRay& other) const;

    /**
     * @brief Returns the orientation sign of a point with respect to the segment.
     *
     * Negative means the point lies on the right of the segment direction,
     * positive on the left, and equivalent to zero when the point is collinear.
     *
     * @tparam OtherPoint Type of the point.
     *
     * @param point Point to classify.
     * @return Orientation sign of `(source, target, point)`.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr std::partial_ordering orientation(const OtherPoint& point) const;

    /**
     * @brief Returns the slope of the segment.
     *
     * Undefined behavior for vertical segments.
     *
     * @tparam ResultNumber Coordinate type of the returned slope.
     * @return Slope.
     */
    template <class ResultNumber = NumberType>
    [[nodiscard]] constexpr ResultNumber slope() const;

    /**
     * @brief Returns the half-plane on the right of the segment direction.
     *
     * @return Closed right half-plane.
     */
    [[nodiscard]] constexpr Halfplane<PointType> rightHalfplane() const;

    /**
     * @brief Returns the half-plane on the left of the segment direction.
     *
     * @return Closed left half-plane.
     */
    [[nodiscard]] constexpr Halfplane<PointType> leftHalfplane() const;

    /** @brief Returns whether the given segment is parallel to the oriented segment. */
    template<SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool parallel(const OtherSegment& other) const;

    /** @brief Returns whether another oriented segment is parallel to this oriented segment. */
    template<OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool parallel(const OtherOrientedSegment& other) const;

    /** @brief Returns whether the given line is parallel to the oriented segment. */
    template<LineConcept OtherLine>
    [[nodiscard]] constexpr bool parallel(const OtherLine& other) const;

    /** @brief Returns whether the given oriented line is parallel to the oriented segment. */
    template<OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool parallel(const OtherOrientedLine& other) const;

    /** @brief Returns whether the given ray is parallel to the oriented segment. */
    template<RayConcept OtherRay>
    [[nodiscard]] constexpr bool parallel(const OtherRay& other) const;

    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool intersects(const OtherPoint& other) const;

    /**
     * @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅).
     *
     * @tparam OtherNumber Coordinate type of the other segment endpoints.
     * @tparam OtherPoint::LabelType Label type of the other segment endpoints.
     * @param other Other segment.
     * @return `true` if the segments share at least one point.
     */
    template<SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool intersects(const OtherSegment& other) const;

    /**
     * @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅).
     *
     * @tparam OtherNumber Coordinate type of the other segment endpoints.
     * @tparam OtherPoint::LabelType Label type of the other segment endpoints.
     * @param other Other oriented segment.
     * @return `true` if the segments share at least one point.
     */
    template<OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool intersects(const OtherOrientedSegment& other) const;

    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    [[nodiscard]] constexpr bool intersects(const Shape<PointType>& other) const;

    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template<typename OtherShape>
        requires (!PointConcept<OtherShape> && detail::shapeRank<OtherShape> > detail::shapeRank<OrientedSegment>)
    [[nodiscard]] constexpr bool intersects(const OtherShape& other) const {
        return other.intersects(*this);
    }

    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool intersects(const EmptyShape<EmptyPoint>&) const {
        return false;
    }

    /**
     * @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint.
     *
     * @tparam ResultNumber Coordinate type of the returned point or segment.
     * @tparam OtherNumber Coordinate type of the other segment endpoints.
     * @tparam OtherPoint::LabelType Label type of the other segment endpoints.
     * @param other Other segment.
     * @return Empty if disjoint, otherwise a point or an unordered segment.
     */
    template <class ResultNumber = NumberType, PointConcept OtherPoint>
    [[nodiscard]] constexpr std::optional<Point<ResultNumber, typename PointType::LabelType>>
    intersection(const OtherPoint& other) const;

    /** @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint. @warning Divides coordinates after casting to ResultNumber. */
    template <class ResultNumber = NumberType, SegmentConcept OtherSegment>
    [[nodiscard]] constexpr auto intersection(const OtherSegment& other) const;

    /**
     * @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint.
     *
     * @tparam ResultNumber Coordinate type of the returned point or segment.
     * @tparam OtherNumber Coordinate type of the other segment endpoints.
     * @tparam OtherPoint::LabelType Label type of the other segment endpoints.
     * @param other Other oriented segment.
     * @return Empty if disjoint, otherwise a point or an unordered segment.
     * @warning Divides coordinates after casting to ResultNumber.
     */
    template <class ResultNumber = NumberType, OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr auto intersection(const OtherOrientedSegment& other) const;

    /** @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint. @warning Divides coordinates after casting to ResultNumber. */
    template <class ResultNumber = NumberType, typename OtherShape>
        requires (!PointConcept<OtherShape>
                  && (detail::shapeRank<OtherShape> > detail::shapeRank<OrientedSegment>)
                  && requires(const OtherShape& o, const OrientedSegment& self) {
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
     * @brief Returns the value of the y coordinate for a given x, if it exists.
     *
     * Delegates to the unordered segment with the same endpoints.
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
     * Delegates to the unordered segment with the same endpoints.
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
     * @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected).
     *
     * @tparam OtherNumber Coordinate type of the other segment endpoints.
     * @tparam OtherPoint::LabelType Label type of the other segment endpoints.
     * @param other Other segment.
     * @return `true` if removing this segment splits the other one.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool separates(const OtherPoint& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template<SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool separates(const OtherSegment& other) const;

    /**
     * @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected).
     *
     * @tparam OtherNumber Coordinate type of the other segment endpoints.
     * @tparam OtherPoint::LabelType Label type of the other segment endpoints.
     * @param other Other oriented segment.
     * @return `true` if removing this segment splits the other one.
     */
    template<OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool separates(const OtherOrientedSegment& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template<LineConcept OtherLine>
    [[nodiscard]] constexpr bool separates(const OtherLine& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template<OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool separates(const OtherOrientedLine& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template<RayConcept OtherRay>
    [[nodiscard]] constexpr bool separates(const OtherRay& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template<RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool separates(const OtherRectangle& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template<TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr bool separates(const OtherTriangle& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template<HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool separates(const OtherHalfplane& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template<ConvexConcept OtherConvex>
    [[nodiscard]] constexpr bool separates(const OtherConvex& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template<PolygonConcept OtherPolygon>
    [[nodiscard]] constexpr bool separates(const OtherPolygon& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template<MonotoneChainConcept OtherChain>
    [[nodiscard]] constexpr bool contains(const OtherChain& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<MonotoneChainConcept OtherChain>
    [[nodiscard]] constexpr bool boundaryContains(const OtherChain&) const { return false; }

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template<MonotoneChainConcept OtherChain>
    [[nodiscard]] constexpr bool interiorContains(const OtherChain& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template<MonotoneChainConcept OtherChain>
    [[nodiscard]] constexpr bool separates(const OtherChain& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template<DiskConcept OtherDisk>
    [[nodiscard]] constexpr bool separates(const OtherDisk& other) const;

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
     * @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅).
     *
     * @tparam OtherNumber Coordinate type of the other segment endpoints.
     * @tparam OtherPoint::LabelType Label type of the other segment endpoints.
     * @param other Other segment.
     * @return `true` if the segments share at least one point interior to both.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherPoint& other) const;

    /** @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅). */
    template<SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherSegment& other) const;

    /**
     * @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅).
     *
     * @tparam OtherNumber Coordinate type of the other segment endpoints.
     * @tparam OtherPoint::LabelType Label type of the other segment endpoints.
     * @param other Other oriented segment.
     * @return `true` if the segments share at least one point interior to both.
     */
    template<OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherOrientedSegment& other) const;

    /** @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅). */
    template<typename OtherShape>
        requires (!PointConcept<OtherShape> && detail::shapeRank<OtherShape> > detail::shapeRank<OrientedSegment>)
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

    /**
     * @brief Tests whether the two shapes mutually separate each other (each disconnects the other).
     *
     * @tparam OtherNumber Coordinate type of the other segment endpoints.
     * @tparam OtherPoint::LabelType Label type of the other segment endpoints.
     * @param other Other segment.
     * @return `true` if the segments cross through their interiors.
     */
    template<SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool crosses(const OtherSegment& other) const;

    /**
     * @brief Tests whether the two shapes mutually separate each other (each disconnects the other).
     *
     * @tparam OtherNumber Coordinate type of the other segment endpoints.
     * @tparam OtherPoint::LabelType Label type of the other segment endpoints.
     * @param other Other oriented segment.
     * @return `true` if the segments cross through their interiors.
     */
    template<OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool crosses(const OtherOrientedSegment& other) const;

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool crosses(const OtherPoint& other) const;

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template<typename OtherShape>
        requires (!PointConcept<OtherShape> && detail::shapeRank<OtherShape> > detail::shapeRank<OrientedSegment>)
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

    /**
     * @brief Returns the squared Euclidean distance to the given shape.
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
     * @brief Returns the squared Euclidean distance to the given shape.
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
     * @brief Returns the squared Euclidean distance to the given shape.
     *
     * @tparam ResultNumber Coordinate type of the returned distance (default: NumberType).
     * @tparam OtherNumber Coordinate type of the other segment endpoints.
     * @tparam OtherPoint::LabelType Label type of the other segment endpoints.
     * @param other Other oriented segment.
     * @return Squared Euclidean distance.
     *
     * @warning With an integer @p ResultNumber the exact squared distance is
     *          generally a fraction, so the internal division truncates and the
     *          result is inexact. Request a floating-point or pgl::Rational
     *          result type, e.g. `squaredDistance<double>(other)`, for an
     *          accurate value.
     */
    template <class ResultNumber = NumberType, OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr auto squaredDistance(const OtherOrientedSegment& other) const;

    /**
     * @brief Returns the squared Euclidean distance to the given shape.
     *
     * Forwards to the other shape's implementation so that each unordered pair
     * needs `squaredDistance` defined only once, on the higher-ranked shape.
     */
    template <class ResultNumber = NumberType, typename OtherShape>
        requires ((detail::shapeRank<OtherShape> > detail::shapeRank<OrientedSegment>)
                  && requires(const OtherShape& o, const OrientedSegment& self) {
                         o.template squaredDistance<ResultNumber>(self);
                     })
    [[nodiscard]] constexpr auto squaredDistance(const OtherShape& other) const {
        return other.template squaredDistance<ResultNumber>(*this);
    }

    /**
     * @brief Returns the squared Euclidean distance to a disk.
     *
     * Forwards to @ref Disk::squaredDistance, which is not templated on a result
     * type: a disk's exterior distance is irrational, so it always returns
     * `double`. The generic forwarder above does not apply because it requires
     * the templated `squaredDistance<ResultNumber>` form.
     */
    template <class DiskPointType, class DiskLabel>
    [[nodiscard]] double squaredDistance(const Disk<DiskPointType, DiskLabel>& disk) const {
        return disk.squaredDistance(*this);
    }

    /** @brief Returns the Manhattan (L1) distance to the given shape. */
    template <class ResultNumber = NumberType, PointConcept OtherPoint>
    [[nodiscard]] constexpr auto distanceL1(const OtherPoint& point) const;

    /** @copydoc distanceL1(const OtherPoint&) const */
    template <class ResultNumber = NumberType, SegmentConcept OtherSegment>
    [[nodiscard]] constexpr auto distanceL1(const OtherSegment& other) const;

    /** @copydoc distanceL1(const OtherPoint&) const */
    template <class ResultNumber = NumberType, OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr auto distanceL1(const OtherOrientedSegment& other) const;

    /**
     * @brief Returns the Manhattan (L1) distance to the given shape.
     *
     * Forwards to the other shape's implementation so that each unordered pair
     * needs `distanceL1` defined only once, on the higher-ranked shape.
     */
    template <class ResultNumber = NumberType, typename OtherShape>
        requires ((detail::shapeRank<OtherShape> > detail::shapeRank<OrientedSegment>)
                  && requires(const OtherShape& o, const OrientedSegment& self) {
                         o.template distanceL1<ResultNumber>(self);
                     })
    [[nodiscard]] constexpr auto distanceL1(const OtherShape& other) const {
        return other.template distanceL1<ResultNumber>(*this);
    }

    /**
     * @brief Returns the distance to the given shape, using symmetry to
     * re-dispatch through the wrapper's own `distanceL1`.
     *
     * Distance is symmetric, so this just calls @p other's own `distanceL1`,
     * which visits its wrapped alternative and throws if the pair is
     * unsupported.
     */
    template <class ResultNumber = NumberType, PointConcept OtherPoint>
    [[nodiscard]] constexpr auto distanceL1(const Shape<OtherPoint>& other) const {
        return other.template distanceL1<ResultNumber>(*this);
    }

    /** @brief Returns the Chebyshev (LInf) distance to the given shape. */
    template <class ResultNumber = NumberType, PointConcept OtherPoint>
    [[nodiscard]] constexpr auto distanceLInf(const OtherPoint& point) const;

    /** @copydoc distanceLInf(const OtherPoint&) const */
    template <class ResultNumber = NumberType, SegmentConcept OtherSegment>
    [[nodiscard]] constexpr auto distanceLInf(const OtherSegment& other) const;

    /** @copydoc distanceLInf(const OtherPoint&) const */
    template <class ResultNumber = NumberType, OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr auto distanceLInf(const OtherOrientedSegment& other) const;

    /**
     * @brief Returns the Chebyshev (LInf) distance to the given shape.
     *
     * Forwards to the other shape's implementation so that each unordered pair
     * needs `distanceLInf` defined only once, on the higher-ranked shape.
     */
    template <class ResultNumber = NumberType, typename OtherShape>
        requires ((detail::shapeRank<OtherShape> > detail::shapeRank<OrientedSegment>)
                  && requires(const OtherShape& o, const OrientedSegment& self) {
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
    template <class ResultNumber = NumberType, PointConcept OtherPoint>
    [[nodiscard]] constexpr auto distanceLInf(const Shape<OtherPoint>& other) const {
        return other.template distanceLInf<ResultNumber>(*this);
    }

    /** @brief Returns the Manhattan (L1) Hausdorff distance to the given shape. */
    template <class ResultNumber = NumberType, SegmentConcept OtherSegment>
    [[nodiscard]] constexpr auto hausdorffDistanceL1(const OtherSegment& other) const;

    /** @copydoc hausdorffDistanceL1(const OtherSegment&) const */
    template <class ResultNumber = NumberType, OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr auto hausdorffDistanceL1(const OtherOrientedSegment& other) const;

    /** @copydoc hausdorffDistanceL1(const OtherSegment&) const */
    template <class ResultNumber = NumberType, PointConcept OtherPoint>
    [[nodiscard]] constexpr auto hausdorffDistanceL1(const OtherPoint& point) const;

    /**
     * @brief Returns the Manhattan (L1) Hausdorff distance to the given shape.
     *
     * Forwards to the other shape's implementation so that each unordered pair
     * needs `hausdorffDistanceL1` defined only once, on the higher-ranked shape.
     */
    template <class ResultNumber = NumberType, typename OtherShape>
        requires ((detail::shapeRank<OtherShape> > detail::shapeRank<OrientedSegment>)
                  && requires(const OtherShape& o, const OrientedSegment& self) {
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
    template <class ResultNumber = NumberType, PointConcept OtherPoint>
    [[nodiscard]] constexpr auto hausdorffDistanceL1(const Shape<OtherPoint>& other) const {
        return other.template hausdorffDistanceL1<ResultNumber>(*this);
    }

    /** @brief Returns the Chebyshev (LInf) Hausdorff distance to the given shape. */
    template <class ResultNumber = NumberType, SegmentConcept OtherSegment>
    [[nodiscard]] constexpr auto hausdorffDistanceLInf(const OtherSegment& other) const;

    /** @copydoc hausdorffDistanceLInf(const OtherSegment&) const */
    template <class ResultNumber = NumberType, OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr auto hausdorffDistanceLInf(const OtherOrientedSegment& other) const;

    /** @copydoc hausdorffDistanceLInf(const OtherSegment&) const */
    template <class ResultNumber = NumberType, PointConcept OtherPoint>
    [[nodiscard]] constexpr auto hausdorffDistanceLInf(const OtherPoint& point) const;

    /**
     * @brief Returns the Chebyshev (LInf) Hausdorff distance to the given shape.
     *
     * Forwards to the other shape's implementation so that each unordered pair
     * needs `hausdorffDistanceLInf` defined only once, on the higher-ranked shape.
     */
    template <class ResultNumber = NumberType, typename OtherShape>
        requires ((detail::shapeRank<OtherShape> > detail::shapeRank<OrientedSegment>)
                  && requires(const OtherShape& o, const OrientedSegment& self) {
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
    template <class ResultNumber = NumberType, PointConcept OtherPoint>
    [[nodiscard]] constexpr auto hausdorffDistanceLInf(const Shape<OtherPoint>& other) const {
        return other.template hausdorffDistanceLInf<ResultNumber>(*this);
    }

    /**
     * @brief Returns the squared Hausdorff distance to another unordered segment.
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
     * @brief Returns the squared Hausdorff distance to another oriented segment.
     *
     * @tparam ResultNumber Coordinate type of the returned distance (default: NumberType).
     * @tparam OtherNumber Coordinate type of the other segment endpoints.
     * @tparam OtherPoint::LabelType Label type of the other segment endpoints.
     * @param other Other oriented segment.
     * @return Squared Hausdorff distance.
     *
     * @warning With an integer @p ResultNumber the exact squared distance is
     *          generally a fraction, so the internal division truncates and the
     *          result is inexact. Request a floating-point or pgl::Rational
     *          result type, e.g. `squaredHausdorffDistance<double>(other)`, for
     *          an accurate value.
     */
    template <class ResultNumber = NumberType, OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr auto squaredHausdorffDistance(const OtherOrientedSegment& other) const;

    /**
     * @brief Returns the squared Hausdorff distance to a point.
     *
     * @tparam ResultNumber Coordinate type of the returned distance (default: NumberType).
     * @tparam OtherPoint Type of the point.
     * @param point Point to measure from.
     * @return Squared Hausdorff distance.
     *
     * @warning With an integer @p ResultNumber the exact squared distance is
     *          generally a fraction, so the internal division truncates and the
     *          result is inexact. Request a floating-point or pgl::Rational
     *          result type, e.g. `squaredHausdorffDistance<double>(point)`, for
     *          an accurate value.
     */
    template <class ResultNumber = NumberType, PointConcept OtherPoint>
    [[nodiscard]] constexpr auto squaredHausdorffDistance(const OtherPoint& point) const;

    /**
     * @brief Returns the squared Hausdorff distance to the given shape.
     *
     * Forwards to the other shape's implementation so that each unordered pair
     * needs `squaredHausdorffDistance` defined only once, on the higher-ranked shape.
     */
    template <class ResultNumber = NumberType, typename OtherShape>
        requires ((detail::shapeRank<OtherShape> > detail::shapeRank<OrientedSegment>)
                  && requires(const OtherShape& o, const OrientedSegment& self) {
                         o.template squaredHausdorffDistance<ResultNumber>(self);
                     })
    [[nodiscard]] constexpr auto squaredHausdorffDistance(const OtherShape& other) const {
        return other.template squaredHausdorffDistance<ResultNumber>(*this);
    }

    /**
     * @brief Returns an unordered segment defining the diameter.
     *
     * @return Unordered segment spanning the same endpoints.
     */
    [[nodiscard]] constexpr Segment<PointType> diameter() const;

    /**
     * @brief Returns the bounding box of the oriented segment.
     *
     * @return The minimum axis-aligned rectangle that contains the segment.
     */
    [[nodiscard]] constexpr Rectangle<PointType> bbox() const;

    /**
     * @brief Returns a bounding box of the oriented segment with floating point coordinates.
     *
     * @tparam ResultNumber Floating point type.
     * @return A rectangle that contains the segment.
     */
    template <std::floating_point ResultNumber = double>
    [[nodiscard]] constexpr Rectangle<Point<ResultNumber>> fbox() const;

    /**
     * @brief Returns the two endpoints in source-to-target order.
     *
     * @return Array `{source(), target()}`.
     */
    [[nodiscard]] constexpr std::array<PointType, 2> vertices() const;

    /**
     * @brief Returns the unique geometric edge of the oriented segment.
     *
     * @return Array containing the underlying unoriented segment.
     */
    [[nodiscard]] constexpr std::array<Segment<PointType>, 1> edges() const;

    /**
     * @brief Returns the unique oriented edge of the oriented segment.
     *
     * @return Array containing this segment.
     */
    [[nodiscard]] constexpr std::array<OrientedSegment, 1> orientedEdges() const;

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
     * @brief Tests whether some point in this shape's relative interior lies in
     *        the strict interior of @p shape.
     *
     * Uses @ref pointInside as the witness. When integer truncation rounds that
     * witness onto or outside the boundary, this shape and @p shape are scaled
     * so the witness is exact, leaving the containment relation unchanged.
     */
    template <class OtherShape>
    [[nodiscard]] constexpr bool pointInsideInteriorContainedIn(const OtherShape& shape) const;

    /** @brief Translates the oriented segment by the given point in place. */
    template<PointConcept OtherPoint>
    constexpr OrientedSegment& operator+=(const OtherPoint& translation);

    /** @brief Translates the oriented segment by the negation of the given point in place. */
    template<PointConcept OtherPoint>
    constexpr OrientedSegment& operator-=(const OtherPoint& translation);

    /** @brief Scales the oriented segment around the origin by a scalar in place. */
    template <class Scalar>
        requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
    constexpr OrientedSegment& operator*=(const Scalar& scalar);

    /** @brief Divides the oriented segment coordinates by a scalar in place. */
    template <class Scalar>
        requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
    constexpr OrientedSegment& operator/=(const Scalar& scalar);

  public:
    std::array<PointType, 2> points_{};
    [[no_unique_address]] mutable LabelType label_{};
};

/**
 * @brief Translates an oriented segment by a point.
 *
 * @tparam Number Coordinate type of the segment endpoints.
 * @tparam Label Label type of the segment endpoints.
 * @tparam TranslationNumber Coordinate type of the translation point.
 * @tparam TranslationLabel Label type of the translation point.
 * @param segment Oriented segment to translate.
 * @param translation Translation vector.
 * @return Translated oriented segment.
 */
template <class PointType, class LabelType, class TranslationNumber, class TranslationLabel>
constexpr auto operator+(const OrientedSegment<PointType, LabelType>& segment, const Point<TranslationNumber, TranslationLabel>& translation);

/**
 * @brief Translates an oriented segment by a point written on the left.
 *
 * @tparam TranslationNumber Coordinate type of the translation point.
 * @tparam TranslationLabel Label type of the translation point.
 * @tparam Number Coordinate type of the segment endpoints.
 * @tparam Label Label type of the segment endpoints.
 * @param translation Translation vector.
 * @param segment Oriented segment to translate.
 * @return Translated oriented segment.
 */
template <class TranslationNumber, class TranslationLabel, class PointType, class LabelType>
constexpr auto operator+(const Point<TranslationNumber, TranslationLabel>& translation, const OrientedSegment<PointType, LabelType>& segment);

/**
 * @brief Translates an oriented segment by the opposite of a point.
 *
 * @tparam Number Coordinate type of the segment endpoints.
 * @tparam Label Label type of the segment endpoints.
 * @tparam TranslationNumber Coordinate type of the translation point.
 * @tparam TranslationLabel Label type of the translation point.
 * @param segment Oriented segment to translate.
 * @param translation Translation vector to subtract.
 * @return Translated oriented segment.
 */
template <class PointType, class LabelType, class TranslationNumber, class TranslationLabel>
constexpr auto operator-(const OrientedSegment<PointType, LabelType>& segment, const Point<TranslationNumber, TranslationLabel>& translation);

/**
 * @brief Scales an oriented segment by a scalar.
 *
 * @tparam Number Coordinate type of the segment endpoints.
 * @tparam Label Label type of the segment endpoints.
 * @tparam Scalar Scalar type.
 * @param segment Oriented segment to scale.
 * @param scalar Scale factor.
 * @return Scaled oriented segment.
 */
template <class PointType, class LabelType, class Scalar>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr auto operator*(const OrientedSegment<PointType, LabelType>& segment, const Scalar& scalar);

/**
 * @brief Scales an oriented segment by a scalar written on the left.
 *
 * @tparam Scalar Scalar type.
 * @tparam Number Coordinate type of the segment endpoints.
 * @tparam Label Label type of the segment endpoints.
 * @param scalar Scale factor.
 * @param segment Oriented segment to scale.
 * @return Scaled oriented segment.
 */
template <class Scalar, class PointType, class LabelType>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr auto operator*(const Scalar& scalar, const OrientedSegment<PointType, LabelType>& segment);

/**
 * @brief Divides both endpoints by a scalar.
 *
 * @tparam Number Coordinate type of the segment endpoints.
 * @tparam Label Label type of the segment endpoints.
 * @tparam Scalar Scalar type.
 * @param segment Oriented segment to divide.
 * @param scalar Divisor.
 * @return Scaled oriented segment.
 */
template <class PointType, class LabelType, class Scalar>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr auto operator/(const OrientedSegment<PointType, LabelType>& segment, const Scalar& scalar);

/**
 * @brief Streams an oriented segment as `p->q`.
 *
 * @tparam Number Coordinate type of the segment endpoints.
 * @tparam Label Label type of the segment endpoints.
 * @param stream Output stream.
 * @param segment Oriented segment to print.
 * @return The output stream.
 */
template <class PointType, class LabelType>
std::ostream& operator<<(std::ostream& stream, const OrientedSegment<PointType, LabelType>& segment);

}  // namespace pgl

#include "halfplane.hpp"
