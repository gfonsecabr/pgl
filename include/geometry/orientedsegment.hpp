#pragma once

/**
 * @file orientedsegment.hpp
 * @brief Public declaration of pgl::OrientedSegment.
 *
 * This is the directed counterpart to Segment. It preserves source-to-target
 * order for orientation-sensitive operations.
 */

#include "../pgl.hpp"

namespace pgl {

template <class PointType = Point<>>
struct OrientedSegment;

OrientedSegment() -> OrientedSegment<Point<>>;

template <class PointType>
OrientedSegment(PointType, PointType) -> OrientedSegment<PointType>;

template <class Number>
OrientedSegment(Number, Number, Number, Number) -> OrientedSegment<Point<Number>>;

/**
 * @brief Oriented segment connecting two endpoints in source-to-target order.
 *
 * Unlike @ref Segment, the stored endpoint order is preserved.
 *
 * @tparam PointType Endpoint point type.
 */
template <class PointType_>
struct OrientedSegment {
    using PointType = PointType_;
    using NumberType = PointType::NumberType;
    // using LabelType = detail::point_label_t<PointType>;

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

    template<PointConcept OtherPointType>
        requires(std::constructible_from<PointType, const OtherPointType&>)
    constexpr OrientedSegment(const OrientedSegment<OtherPointType>& other)
        : OrientedSegment(PointType(other.source()), PointType(other.target())) {}

    template<PointConcept OtherPointType>
        requires(std::constructible_from<PointType, const OtherPointType&>)
    constexpr OrientedSegment& operator=(const OrientedSegment<OtherPointType>& other) {
        points_[0] = PointType(other.source());
        points_[1] = PointType(other.target());
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
     * @brief Provides lexicographic ordering on `(source, target)`.
     *
     * @param other Oriented segment to compare with.
     * @return -1, 0, or 1.
     */
    constexpr auto operator<=>(const OrientedSegment& other) const = default;

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

    template<class S>
        requires(!detail::is_point_v<S>)
    [[nodiscard]] constexpr bool boundaryContains(const S& other) const;

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
     * @brief Returns whether the segment contains another unordered segment.
     *
     * @tparam OtherNumber Coordinate type of the other segment endpoints.
     * @tparam OtherPoint::LabelType Label type of the other segment endpoints.
     * @param other Other segment.
     * @return `true` if both endpoints of `other` lie on this segment.
     */
    template<PointConcept OtherPoint, class OtherLabel>
    [[nodiscard]] constexpr bool contains(const Segment<OtherPoint, OtherLabel>& other) const;

    /**
     * @brief Returns whether the segment contains another oriented segment.
     *
     * @tparam OtherNumber Coordinate type of the other segment endpoints.
     * @tparam OtherPoint::LabelType Label type of the other segment endpoints.
     * @param other Other oriented segment.
     * @return `true` if both endpoints of `other` lie on this segment.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool contains(const OrientedSegment<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool contains(const Line<OtherPoint>& other) const;

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

    template<PointConcept OtherPoint, class OtherLabel>
    [[nodiscard]] constexpr bool interiorContains(const Segment<OtherPoint, OtherLabel>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorContains(const OrientedSegment<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorContains(const Line<OtherPoint>& other) const;

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

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool collinear(const OtherPoint& point) const;

    template<PointConcept OtherPoint, class OtherLabel>
    [[nodiscard]] constexpr bool collinear(const Segment<OtherPoint, OtherLabel>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool collinear(const OrientedSegment<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool collinear(const Line<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool collinear(const OrientedLine<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool collinear(const Ray<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr std::partial_ordering orientation(const OtherPoint& point) const;

    /**
     * @brief Returns the signed slope of the segment.
     *
     * Undefined behavior for vertical segments.
     *
     * @tparam ResultNumber Coordinate type of the returned slope.
     * @return Signed slope.
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

    template<PointConcept OtherPoint, class OtherLabel>
    [[nodiscard]] constexpr bool parallel(const Segment<OtherPoint, OtherLabel>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool parallel(const OrientedSegment<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool parallel(const Line<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool parallel(const OrientedLine<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool parallel(const Ray<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool intersects(const OtherPoint& other) const;

    /**
     * @brief Returns whether this segment intersects another unordered segment.
     *
     * @tparam OtherNumber Coordinate type of the other segment endpoints.
     * @tparam OtherPoint::LabelType Label type of the other segment endpoints.
     * @param other Other segment.
     * @return `true` if the segments share at least one point.
     */
    template<PointConcept OtherPoint, class OtherLabel>
    [[nodiscard]] constexpr bool intersects(const Segment<OtherPoint, OtherLabel>& other) const;

    /**
     * @brief Returns whether this segment intersects another oriented segment.
     *
     * @tparam OtherNumber Coordinate type of the other segment endpoints.
     * @tparam OtherPoint::LabelType Label type of the other segment endpoints.
     * @param other Other oriented segment.
     * @return `true` if the segments share at least one point.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool intersects(const OrientedSegment<OtherPoint>& other) const;

    [[nodiscard]] constexpr bool intersects(const Shape<PointType>& other) const;

    template<typename OtherShape>
        requires (!PointConcept<OtherShape>)
    [[nodiscard]] constexpr bool intersects(const OtherShape& other) const {
        return other.intersects(*this);
    }

    /**
     * @brief Returns the intersection with another unordered segment.
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

    template <class ResultNumber = NumberType, PointConcept OtherPoint, class OtherLabel>
    [[nodiscard]] constexpr auto intersection(const Segment<OtherPoint, OtherLabel>& other) const;

    /**
     * @brief Returns the intersection with another oriented segment.
     *
     * @tparam ResultNumber Coordinate type of the returned point or segment.
     * @tparam OtherNumber Coordinate type of the other segment endpoints.
     * @tparam OtherPoint::LabelType Label type of the other segment endpoints.
     * @param other Other oriented segment.
     * @return Empty if disjoint, otherwise a point or an unordered segment.
     */
    template <class ResultNumber = NumberType, PointConcept OtherPoint>
    [[nodiscard]] constexpr auto intersection(const OrientedSegment<OtherPoint>& other) const;

    template <class ResultNumber = NumberType, typename OtherShape>
        requires (!PointConcept<OtherShape>
                  && (detail::shapeRank<OtherShape> > detail::shapeRank<OrientedSegment>)
                  && requires(const OtherShape& o, const OrientedSegment& self) {
                         o.template intersection<ResultNumber>(self);
                     })
    [[nodiscard]] constexpr auto intersection(const OtherShape& other) const {
        return other.template intersection<ResultNumber>(*this);
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
     * @brief Returns whether this segment separates another unordered segment.
     *
     * @tparam OtherNumber Coordinate type of the other segment endpoints.
     * @tparam OtherPoint::LabelType Label type of the other segment endpoints.
     * @param other Other segment.
     * @return `true` if removing this segment splits the other one.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool separates(const OtherPoint& other) const;

    template<PointConcept OtherPoint, class OtherLabel>
    [[nodiscard]] constexpr bool separates(const Segment<OtherPoint, OtherLabel>& other) const;

    /**
     * @brief Returns whether this segment separates another oriented segment.
     *
     * @tparam OtherNumber Coordinate type of the other segment endpoints.
     * @tparam OtherPoint::LabelType Label type of the other segment endpoints.
     * @param other Other oriented segment.
     * @return `true` if removing this segment splits the other one.
     */
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
     * @brief Returns whether two segments intersect through their interiors.
     *
     * @tparam OtherNumber Coordinate type of the other segment endpoints.
     * @tparam OtherPoint::LabelType Label type of the other segment endpoints.
     * @param other Other segment.
     * @return `true` if the segments share at least one point interior to both.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherPoint& other) const;

    template<PointConcept OtherPoint, class OtherLabel>
    [[nodiscard]] constexpr bool interiorsIntersect(const Segment<OtherPoint, OtherLabel>& other) const;

    /**
     * @brief Returns whether two oriented segments intersect through their interiors.
     *
     * @tparam OtherNumber Coordinate type of the other segment endpoints.
     * @tparam OtherPoint::LabelType Label type of the other segment endpoints.
     * @param other Other oriented segment.
     * @return `true` if the segments share at least one point interior to both.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorsIntersect(const OrientedSegment<OtherPoint>& other) const;

    template<typename OtherShape>
        requires (!PointConcept<OtherShape>)
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherShape& other) const {
        return other.interiorsIntersect(*this);
    }

    [[nodiscard]] constexpr bool interiorsIntersect(const Shape<PointType>& other) const;

    /**
     * @brief Returns whether two segments cross.
     *
     * @tparam OtherNumber Coordinate type of the other segment endpoints.
     * @tparam OtherPoint::LabelType Label type of the other segment endpoints.
     * @param other Other segment.
     * @return `true` if the segments cross through their interiors.
     */
    template<PointConcept OtherPoint, class OtherLabel>
    [[nodiscard]] constexpr bool crosses(const Segment<OtherPoint, OtherLabel>& other) const;

    /**
     * @brief Returns whether two oriented segments cross.
     *
     * @tparam OtherNumber Coordinate type of the other segment endpoints.
     * @tparam OtherPoint::LabelType Label type of the other segment endpoints.
     * @param other Other oriented segment.
     * @return `true` if the segments cross through their interiors.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool crosses(const OrientedSegment<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool crosses(const OtherPoint& other) const;

    template<typename OtherShape>
        requires (!PointConcept<OtherShape>)
    [[nodiscard]] constexpr bool crosses(const OtherShape& other) const {
        return other.crosses(*this);
    }

    [[nodiscard]] constexpr bool crosses(const Shape<PointType>& other) const;

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
     * @brief Returns the squared Euclidean distance to another unordered segment.
     *
     * @tparam OtherNumber Coordinate type of the other segment endpoints.
     * @tparam OtherPoint::LabelType Label type of the other segment endpoints.
     * @param other Other segment.
     * @return Squared Euclidean distance.
     */
    template<PointConcept OtherPoint, class OtherLabel>
    [[nodiscard]] constexpr auto squaredDistance(const Segment<OtherPoint, OtherLabel>& other) const;

    /**
     * @brief Returns the squared Euclidean distance to another oriented segment.
     *
     * @tparam OtherNumber Coordinate type of the other segment endpoints.
     * @tparam OtherPoint::LabelType Label type of the other segment endpoints.
     * @param other Other oriented segment.
     * @return Squared Euclidean distance.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr auto squaredDistance(const OrientedSegment<OtherPoint>& other) const;

    /**
     * @brief Returns the squared Hausdorff distance to another unordered segment.
     *
     * @tparam OtherNumber Coordinate type of the other segment endpoints.
     * @tparam OtherPoint::LabelType Label type of the other segment endpoints.
     * @param other Other segment.
     * @return Squared Hausdorff distance.
     */
    template<PointConcept OtherPoint, class OtherLabel>
    [[nodiscard]] constexpr auto squaredHausdorffDistance(const Segment<OtherPoint, OtherLabel>& other) const;

    /**
     * @brief Returns the squared Hausdorff distance to another oriented segment.
     *
     * @tparam OtherNumber Coordinate type of the other segment endpoints.
     * @tparam OtherPoint::LabelType Label type of the other segment endpoints.
     * @param other Other oriented segment.
     * @return Squared Hausdorff distance.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr auto squaredHausdorffDistance(const OrientedSegment<OtherPoint>& other) const;

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

    template<PointConcept OtherPoint>
    constexpr OrientedSegment& operator+=(const OtherPoint& translation);

    template<PointConcept OtherPoint>
    constexpr OrientedSegment& operator-=(const OtherPoint& translation);

    template <class Scalar>
        requires(!detail::is_point_v<Scalar>)
    constexpr OrientedSegment& operator*=(const Scalar& scalar);

    template <class Scalar>
        requires(!detail::is_point_v<Scalar>)
    constexpr OrientedSegment& operator/=(const Scalar& scalar);

  public:
    std::array<PointType, 2> points_{};
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
template <class PointType, class TranslationNumber, class TranslationLabel>
constexpr auto operator+(const OrientedSegment<PointType>& segment, const Point<TranslationNumber, TranslationLabel>& translation);

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
template <class TranslationNumber, class TranslationLabel, class PointType>
constexpr auto operator+(const Point<TranslationNumber, TranslationLabel>& translation, const OrientedSegment<PointType>& segment);

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
template <class PointType, class TranslationNumber, class TranslationLabel>
constexpr auto operator-(const OrientedSegment<PointType>& segment, const Point<TranslationNumber, TranslationLabel>& translation);

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
template <class PointType, class Scalar>
    requires(!detail::is_point_v<Scalar>)
constexpr auto operator*(const OrientedSegment<PointType>& segment, const Scalar& scalar);

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
template <class Scalar, class PointType>
    requires(!detail::is_point_v<Scalar>)
constexpr auto operator*(const Scalar& scalar, const OrientedSegment<PointType>& segment);

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
template <class PointType, class Scalar>
    requires(!detail::is_point_v<Scalar>)
constexpr auto operator/(const OrientedSegment<PointType>& segment, const Scalar& scalar);

/**
 * @brief Streams an oriented segment as `p->q`.
 *
 * @tparam Number Coordinate type of the segment endpoints.
 * @tparam Label Label type of the segment endpoints.
 * @param stream Output stream.
 * @param segment Oriented segment to print.
 * @return The output stream.
 */
template <class PointType>
std::ostream& operator<<(std::ostream& stream, const OrientedSegment<PointType>& segment);

}  // namespace pgl

#include "halfplane.hpp"
