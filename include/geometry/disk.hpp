#ifndef PGL_HPP_INCLUDED
// Entered out of order (before pgl.hpp): defer to the umbrella header,
// which re-includes this file at the correct layer.
#include "pgl.hpp"
#else
#ifndef PGL_GEOMETRY_DISK_HPP
#define PGL_GEOMETRY_DISK_HPP

/**
 * @file disk.hpp
 * @brief Declaration of pgl::Disk.
 *
 * A disk is a closed circle and its interior, stored by three points on its
 * boundary circle rather than by a center and radius. This keeps the
 * representation exact for integral coordinates (the center and radius of an
 * integer-coordinate circle are generally rational), at the cost of computing
 * the center/radius on demand via division.
 */

#include <array>
#include <cassert>
#include <cmath>
#include <compare>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <numbers>
#include <ostream>
#include <type_traits>
#include <utility>
#include <stdexcept>


namespace pgl {

template <class PointType = Point<>, class Label = NoLabel>
struct Disk;

/// @brief Deduces a default disk with `Point<>` boundary points and no label.
Disk() -> Disk<Point<>, NoLabel>;

/// @brief Deduces a disk from a center point and a radius.
template <class PointType>
Disk(PointType, typename PointType::NumberType) -> Disk<PointType, NoLabel>;

/// @brief Deduces a disk from three boundary points.
template <PointConcept PointType>
Disk(PointType, PointType, PointType) -> Disk<PointType, NoLabel>;

/// @brief Deduces a disk from center coordinates and a radius.
template <class Number>
Disk(Number, Number, Number) -> Disk<Point<Number>, NoLabel>;

/**
 * @brief Closed disk represented by three canonicalized boundary points.
 *
 * The three stored points are kept in a canonical form so that two disks
 * describing the same circle compare equal regardless of how they were built:
 * the points are sorted lexicographically and, when not collinear, reordered to
 * counterclockwise winding (see @ref canonicalizePoints). The center/radius
 * constructors are conveniences; the stored representation is always three
 * boundary points, matching @ref Triangle.
 *
 * @tparam PointType_ Boundary point type; must be a `pgl::Point`.
 * @tparam TLabel Optional label type carried with the disk (`NoLabel` to omit).
 */
template <class PointType_, class TLabel>
struct Disk {
    using PointType = PointType_;
    using NumberType = PointType::NumberType;
    using PointLabelType = PointType::LabelType;
    using LabelType = TLabel;

    static_assert(detail::is_point_v<PointType>, "Disk requires pgl::Point boundary points");

    /**
     * @brief Creates a disk with all three boundary points at the origin.
     */
    constexpr Disk() = default;

    /**
     * @brief Creates the disk through three boundary points.
     *
     * The points are stored in canonical order, so argument order does not
     * matter.
     */
    constexpr Disk(PointType first, PointType second, PointType third)
        requires PointConcept<PointType>
        : points_(canonicalizePoints(std::move(first), std::move(second), std::move(third))) {}

    /**
     * @brief Creates a disk from three boundary points and stores a label.
     * @tparam A Type convertible to @ref LabelType.
     * @param first,second,third Boundary points (canonicalized on storage).
     * @param label Disk label, forwarded into the stored label.
     */
    template <class A>
        requires(detail::has_label_v<LabelType> && std::constructible_from<LabelType, A&&>)
    constexpr Disk(PointType first, PointType second, PointType third, A&& label)
        requires PointConcept<PointType>
        : Disk(std::move(first), std::move(second), std::move(third)) {
        label_ = std::forward<A>(label);
    }

    /**
     * @brief Creates a disk from its center and radius.
     *
     * The leftmost, rightmost and topmost points of the circle are stored as the
     * three boundary points, which is exact for any number type (no division).
     * A negative @p radius is treated as its absolute value. The center's point
     * label, if any, is copied onto the three boundary points.
     */
    constexpr Disk(PointType center, NumberType radius)
        requires PointConcept<PointType> {
        const NumberType zero{};
        const NumberType r = radius < zero ? -radius : radius;
        const NumberType cx = center.x();
        const NumberType cy = center.y();
        const auto point_label = detail::copyLabel<PointLabelType>(center);

        points_ = canonicalizePoints(
            Point<NumberType, PointLabelType>(cx - r, cy,     point_label),
            Point<NumberType, PointLabelType>(cx + r, cy,     point_label),
            Point<NumberType, PointLabelType>(cx,     cy + r, point_label)
        );
    }
    /** @brief Same as above, and stores a disk label. */
    template <class A>
        requires(detail::has_label_v<LabelType> && std::constructible_from<LabelType, A&&>)
    constexpr Disk(PointType center, NumberType radius, A&& label)
        requires PointConcept<PointType> : Disk(std::move(center), radius) {
        label_ = std::forward<A>(label);
    }

    /**
     * @brief Creates a disk from center coordinates and a radius.
     */
    constexpr Disk(NumberType x, NumberType y, NumberType radius)
        : Disk(PointType(x, y), radius) {}

    /** @brief Same as above, and stores a disk label. */
    template <class A>
        requires(detail::has_label_v<LabelType> && std::constructible_from<LabelType, A&&>)
    constexpr Disk(NumberType x, NumberType y, NumberType radius, A&& label)
        : Disk(PointType(x, y), radius, std::forward<A>(label)) {}

    /**
     * @brief Converts a disk with a different point and/or label type.
     *
     * Each boundary point is converted to @ref PointType and re-canonicalized
     * (the canonical order may differ across coordinate types), and the label is
     * copied when both sides carry one.
     */
    template <PointConcept OtherPointType, class OtherLabelType>
        requires(std::constructible_from<PointType, const OtherPointType&>)
    constexpr Disk(const Disk<OtherPointType, OtherLabelType>& other)
        : points_(canonicalizePoints(PointType(other.a()), PointType(other.b()), PointType(other.c()))),
          label_(detail::copyLabel<LabelType>(other)) {}

    /**
     * @brief Assigns from a disk with compatible point and label types.
     * @tparam OtherPointType Source boundary point type.
     * @tparam OtherLabelType Source label type.
     * @param other Disk to copy from; its points are re-canonicalized.
     * @return Reference to this disk.
     */
    template <PointConcept OtherPointType, class OtherLabelType>
        requires(std::constructible_from<PointType, const OtherPointType&>)
    constexpr Disk& operator=(const Disk<OtherPointType, OtherLabelType>& other) {
        points_ = canonicalizePoints(PointType(other.a()), PointType(other.b()), PointType(other.c()));
        label_ = detail::copyLabel<LabelType>(other);
        return *this;
    }

    /**
     * @brief Returns the disk label.
     *
     * The label is mutable even through a const disk: it is metadata that
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
     * @brief Returns boundary point @p index in canonical order.
     * @param index Position in `[0, 3)`; out-of-range is undefined (asserted in debug).
     */
    constexpr const PointType& operator[](std::size_t index) const {
        assert(index < size());
        return points_[index];
    }

    /**
     * @brief Returns the number of stored boundary points (always 3).
     */
    static constexpr std::size_t size() {
        return 3;
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
     * `-1` if no boundary point equals `point`.
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
     * @brief Returns the first boundary point (lexicographically smallest).
     */
    constexpr const PointType& a() const {
        return points_[0];
    }

    /**
     * @brief Returns the second boundary point in canonical order.
     */
    constexpr const PointType& b() const {
        return points_[1];
    }

    /**
     * @brief Returns the third boundary point in canonical order.
     *
     * `a()`, `b()`, `c()` wind counterclockwise.
     */
    constexpr const PointType& c() const {
        return points_[2];
    }

    /**
     * @brief Returns an iterator to the first boundary point.
     */
    constexpr auto begin() const {
        return PointIterator(this, 0);
    }

    /**
     * @brief Returns a const iterator to the first boundary point.
     */
    constexpr auto cbegin() const {
        return PointIterator(this, 0);
    }

    /**
     * @brief Returns an iterator past the last boundary point.
     */
    constexpr auto end() const {
        return PointIterator(this, size());
    }

    /**
     * @brief Returns a const iterator past the last boundary point.
     */
    constexpr auto cend() const {
        return PointIterator(this, size());
    }

    /**
     * @brief Returns the center (circumcenter of the three boundary points) in
     *        an explicitly chosen coordinate type.
     *
     * @tparam ResultNumber Coordinate type of the result.
     * @warning Computes the center via division by twice the signed area unless the disk has been created by center and radius.
     */
    template <class ResultNumber = NumberType>
    [[nodiscard]] constexpr Point<ResultNumber, PointLabelType> center() const {
        if (a().y() == b().y()) {
            NumberType r = c().y() - a().y();
            NumberType center_x = a().x() + r;
            if (b().x() == center_x + r && c().x() == center_x && c().y() == a().y() + r && c().x() == center_x) {
                return Point<ResultNumber, PointLabelType>(center_x, a().y(), PointLabelType{});
            }
        }

        if (isDegenerate()) {
            return Point<ResultNumber, PointLabelType>(a().x(),a().y(), PointLabelType{});
        }

        using Coordinate = detail::promoted_number_t<std::common_type_t<NumberType, ResultNumber>>;
        const Coordinate ax = static_cast<Coordinate>(a().x()), ay = static_cast<Coordinate>(a().y());
        const Coordinate bx = static_cast<Coordinate>(b().x()), by = static_cast<Coordinate>(b().y());
        const Coordinate cx = static_cast<Coordinate>(c().x()), cy = static_cast<Coordinate>(c().y());

        const Coordinate aa = ax * ax + ay * ay;
        const Coordinate bb = bx * bx + by * by;
        const Coordinate cc = cx * cx + cy * cy;
        const Coordinate two = static_cast<Coordinate>(2);

        const Coordinate denominator = two * (ax * (by - cy) + bx * (cy - ay) + cx * (ay - by));

        return Point<ResultNumber, PointLabelType>(
            (aa * (by - cy) + bb * (cy - ay) + cc * (ay - by)) / denominator,
            (aa * (cx - bx) + bb * (ax - cx) + cc * (bx - ax)) / denominator,
            PointLabelType{}
        );
    }

    // /**
    //  * @brief Returns the center in this disk's exact coordinate type.
    //  *
    //  * For integral coordinates the circumcenter is generally rational, so the
    //  * result is `Rational<promoted NumberType>`; for floating-point coordinates
    //  * it is the promoted floating-point type.
    //  */
    // [[nodiscard]] constexpr auto center() const {
    //     using ResultNumber = std::conditional_t<
    //         detail::extended_integral<NumberType>,
    //         Rational<detail::promoted_number_t<NumberType>>,
    //         detail::promoted_number_t<NumberType>>;
    //     return center<ResultNumber>();
    // }

    /**
     * @brief Returns the radius.
     * @tparam ResultNumber Result type.
     * @warning If the disk has not been defined by center and radius, it takes a square root of @ref squaredRadius, which uses division.
     */
    template <class ResultNumber = NumberType>
    [[nodiscard]] constexpr ResultNumber radius() const {
        if (a().y() == b().y()) {
            NumberType r = c().y() - a().y();
            NumberType center_x = a().x() + r;
            if (b().x() == center_x + r && c().x() == center_x && c().y() == a().y() + r && c().x() == center_x) {
                return static_cast<ResultNumber>(r);;
            }
        }
        
        if constexpr (!requires(ResultNumber v) { std::sqrt(v); }) {
            throw std::runtime_error("std::sqrt is not available for the requested ResultNumber type");
        } else {
            return std::sqrt(squaredRadius<ResultNumber>());
        }
    }

    /**
     * @brief Returns whether the three boundary points are collinear.
     */
    constexpr bool isDegenerate() const {
        return orientationSign(a(), b(), c()) == std::partial_ordering::equivalent;
    }

    /**
     * @brief Returns the squared radius in an explicitly chosen result type.
     *
     * @tparam ResultNumber Result type.
     * @warning If the disk has not been defined by center and radius, it uses the circumradius formula, which divides by the squared area.
     */
    template <class ResultNumber = NumberType>
    [[nodiscard]] constexpr ResultNumber squaredRadius() const {
        if (a().y() == b().y()) {
            NumberType r = c().y() - a().y();
            NumberType center_x = a().x() + r;
            if (b().x() == center_x + r && c().x() == center_x && c().y() == a().y() + r && c().x() == center_x) {
                return static_cast<ResultNumber>(r*r);
            }
        }

        if (isDegenerate()) {
            return ResultNumber{};
        }

        // Circumradius formula:
        // R^2 = |AB|^2 |BC|^2 |CA|^2 / (4 * det(A,B,C)^2),
        // where det(A,B,C) is twice the signed area of triangle ABC
        const ResultNumber ab2 = static_cast<ResultNumber>(a().squaredDistance(b()));
        const ResultNumber bc2 = static_cast<ResultNumber>(b().squaredDistance(c()));
        const ResultNumber ca2 = static_cast<ResultNumber>(c().squaredDistance(a()));
        const ResultNumber determinant = static_cast<ResultNumber>(orientationDeterminant(a(), b(), c()));
        const ResultNumber four = static_cast<ResultNumber>(4);

        return (ab2 * bc2 * ca2) / (four * determinant * determinant);
    }

    // /**
    //  * @brief Returns the squared radius in this disk's exact result type.
    //  *
    //  * Exact for integral coordinates, where the result is
    //  * `Rational<promoted NumberType>`; for floating-point coordinates it is the
    //  * promoted floating-point type.
    //  */
    // [[nodiscard]] constexpr auto squaredRadius() const {
    //     using ResultNumber = std::conditional_t<
    //         detail::extended_integral<NumberType>,
    //         Rational<detail::promoted_number_t<NumberType>>,
    //         detail::promoted_number_t<NumberType>>;
    //     return squaredRadius<ResultNumber>();
    // }

    /**
     * @brief Returns the area `pi * R^2` of the closed disk.
     * @tparam ResultNumber Floating-point result type.
     * @warning Computes @ref squaredRadius, which uses division unless the disk has been defined by center and radius.
     */
    template <std::floating_point ResultNumber = double>
    [[nodiscard]] constexpr ResultNumber area() const {
        return std::numbers::pi_v<ResultNumber> * squaredRadius<ResultNumber>();
    }

    /**
     * @brief Returns the exact axis-aligned bounding box in the coordinate type.
     * @warning Not implemented yet: currently throws, so it cannot be used in a
     *          constant expression. Use @ref fbox for a floating-point box.
     */
    [[nodiscard]] constexpr Rectangle<PointType> bbox() const {
        throw "Disk::bbox() is not implemented yet";
    }

    /**
     * @brief Returns a floating-point bounding box of the disk.
     *
     * The box spans `[cx - r, cy - r]` to `[cx + r, cy + r]`; it is tight up to
     * floating-point rounding of the center and radius.
     *
     * @tparam ResultNumber Floating-point coordinate type of the box.
     * @warning Computes @ref center and @ref radius, which use division.
     */
    template <std::floating_point ResultNumber = double>
    [[nodiscard]] constexpr Rectangle<Point<ResultNumber>> fbox() const {
        const ResultNumber r = radius<ResultNumber>();
        const auto center_point = center<ResultNumber>();
        const ResultNumber cx = center_point.x();
        const ResultNumber cy = center_point.y();
        return Rectangle<Point<ResultNumber>>(
            Point<ResultNumber>(cx - r, cy - r),
            Point<ResultNumber>(cx + r, cy + r));
    }

    /**
     * @brief Returns the midpoint of the first two boundary points.
     *
     * Being the midpoint of a chord, it lies strictly inside the disk.
     *
     * @tparam ResultNumber Coordinate type of the result; defaults to @ref NumberType.
     * @warning Divides by 2; with an integral @p ResultNumber the truncated
     *            result may land on or outside the boundary.
     */
    template <class ResultNumber = NumberType>
    [[nodiscard]] constexpr Point<ResultNumber, PointLabelType> pointInside() const {
        const ResultNumber two = static_cast<ResultNumber>(2);
        return Point<ResultNumber, PointLabelType>(
            (static_cast<ResultNumber>(a().x()) + static_cast<ResultNumber>(b().x())) / two,
            (static_cast<ResultNumber>(a().y()) + static_cast<ResultNumber>(b().y())) / two,
            PointLabelType{});
    }

    /**
     * @brief Returns the horizontal diameter as a segment through the center.
     *
     * The endpoints are the leftmost and rightmost points of the circle,
     * `(cx - r, cy)` and `(cx + r, cy)`.
     *
     * @tparam ResultNumber Floating-point coordinate type of the endpoints.
     * @warning Computes @ref center and @ref radius, which use division.
     */
    template <std::floating_point ResultNumber = double>
    [[nodiscard]] constexpr Segment<Point<ResultNumber, PointLabelType>> diameter() const {
        const ResultNumber r = radius<ResultNumber>();
        const auto center_point = center<ResultNumber>();
        const ResultNumber cx = center_point.x();
        const ResultNumber cy = center_point.y();

        return Segment<Point<ResultNumber, PointLabelType>>(
            Point<ResultNumber, PointLabelType>(cx - r, cy, PointLabelType{}),
            Point<ResultNumber, PointLabelType>(cx + r, cy, PointLabelType{})
        );
    }

    /**
     * @brief Tests whether the point @p other lies in the closed disk.
     *
     * Containment is boundary-inclusive: a point exactly on the circle counts as
     * contained. Because the disk is convex, the shape overloads below reduce to
     * this test on a finite set of points.
     *
     * @tparam OtherPoint Point type to test.
     * @return `true` if @p other is inside or on the boundary.
     */
    template <PointConcept OtherPoint>
    [[nodiscard]] constexpr bool contains(const OtherPoint& other) const;

    /** @brief Tests whether both endpoints of a segment lie in the closed disk. */
    template <SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool contains(const OtherSegment& other) const;

    /** @brief Tests whether both endpoints of an oriented segment lie in the closed disk. */
    template <OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool contains(const OtherOrientedSegment& other) const;

    template <LineConcept OtherLine>
    [[nodiscard]] constexpr bool contains(const OtherLine& other) const;

    template <OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool contains(const OtherOrientedLine& other) const;

    template <RayConcept OtherRay>
    [[nodiscard]] constexpr bool contains(const OtherRay& other) const;

    template <HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool contains(const OtherHalfplane& other) const;

    /** @brief Tests whether every triangle vertex lies in the closed disk. */
    template <TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr bool contains(const OtherTriangle& other) const;

    /** @brief Tests whether every rectangle vertex lies in the closed disk. */
    template <RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool contains(const OtherRectangle& other) const;

    /** @brief Tests whether every vertex of a convex polygon lies in the closed disk. */
    template <ConvexConcept OtherConvex>
    [[nodiscard]] constexpr bool contains(const OtherConvex& other) const;

    /** @brief Polygon overload; see the Convex overload. */
    template<PolygonConcept OtherPolygon>
    [[nodiscard]] constexpr bool contains(const OtherPolygon& other) const;

    /** @brief Tests whether this disk encloses the other disk's boundary circle. */
    template <DiskConcept OtherDisk>
    [[nodiscard]] constexpr bool contains(const OtherDisk& other) const;

    /** @brief Dispatches to the matching overload for the runtime type of @p other. */
    [[nodiscard]] constexpr bool contains(const Shape<PointType>& other) const;

    // The empty set is a subset of every shape, so its containment relations are
    // true; the symmetric intersection/crossing predicates reach the empty set
    // through Disk's existing generic OtherShape fallbacks.
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

    /**
     * @brief Tests whether the point @p other lies in the open disk.
     *
     * Strict version of @ref contains: a point exactly on the boundary circle is
     * not interior-contained. Because the open disk is convex, the shape
     * overloads below reduce to this test on a finite set of vertices.
     */
    template <PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorContains(const OtherPoint& other) const;

    /** @brief Tests whether both endpoints of a segment lie strictly inside the disk. */
    template <SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool interiorContains(const OtherSegment& other) const;

    /** @brief Tests whether both endpoints of an oriented segment lie strictly inside the disk. */
    template <OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool interiorContains(const OtherOrientedSegment& other) const;

    template <LineConcept OtherLine>
    [[nodiscard]] constexpr bool interiorContains(const OtherLine& other) const;

    template <OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool interiorContains(const OtherOrientedLine& other) const;

    template <RayConcept OtherRay>
    [[nodiscard]] constexpr bool interiorContains(const OtherRay& other) const;

    template <HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool interiorContains(const OtherHalfplane& other) const;

    /** @brief Tests whether every triangle vertex lies strictly inside the disk. */
    template <TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr bool interiorContains(const OtherTriangle& other) const;

    /** @brief Tests whether every rectangle vertex lies strictly inside the disk. */
    template <RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool interiorContains(const OtherRectangle& other) const;

    /** @brief Tests whether every vertex of a convex polygon lies strictly inside the disk. */
    template <ConvexConcept OtherConvex>
    [[nodiscard]] constexpr bool interiorContains(const OtherConvex& other) const;

    /** @brief Tests whether this disk strictly encloses the other disk's boundary circle. */
    template <DiskConcept OtherDisk>
    [[nodiscard]] constexpr bool interiorContains(const OtherDisk& other) const;

    /**
     * @brief Tests whether the closed segment shares at least one point with the
     *        closed disk (touching counts as intersecting).
     */
    template <SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool intersects(const OtherSegment& other) const;

    /**
     * @brief Tests whether the two closed disks share at least one point.
     *
     * True when the distance between the centers is at most the sum of the radii
     * (touching disks count as intersecting).
     */
    template <DiskConcept OtherDisk>
    [[nodiscard]] constexpr bool intersects(const OtherDisk& other) const;

    /**
     * @brief Tests whether the point @p other lies exactly on the boundary circle.
     *
     * Unlike @ref contains, this is the one-dimensional circle, not the filled
     * region.
     *
     * @tparam OtherPoint Point type to test.
     * @return `true` if @p other lies on the circle (exact in-circle test).
     */
    template <PointConcept OtherPoint>
    [[nodiscard]] constexpr bool boundaryContains(const OtherPoint& other) const;

    template <SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool boundaryContains(const OtherSegment& other) const;

    template <OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool boundaryContains(const OtherOrientedSegment& other) const;

    template <LineConcept OtherLine>
    [[nodiscard]] constexpr bool boundaryContains(const OtherLine& other) const;

    template <OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool boundaryContains(const OtherOrientedLine& other) const;

    template <RayConcept OtherRay>
    [[nodiscard]] constexpr bool boundaryContains(const OtherRay& other) const;

    template <HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool boundaryContains(const OtherHalfplane& other) const;

    template <TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr bool boundaryContains(const OtherTriangle& other) const;

    template <RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool boundaryContains(const OtherRectangle& other) const;

    template <ConvexConcept OtherConvex>
    [[nodiscard]] constexpr bool boundaryContains(const OtherConvex& other) const;

    /** @brief Polygon overload; see the Convex overload. */
    template<PolygonConcept OtherPolygon>
    [[nodiscard]] constexpr bool boundaryContains(const OtherPolygon& other) const;

    /** @brief Tests whether the other disk's boundary circle coincides exactly with this one's. */
    template <DiskConcept OtherDisk>
    [[nodiscard]] constexpr bool boundaryContains(const OtherDisk& other) const;

    /** @brief Dispatches to the matching overload for the runtime type of @p other. */
    [[nodiscard]] constexpr bool boundaryContains(const Shape<PointType>& other) const;

    // --- not-yet-implemented predicate pairs (throw); see implementation ---
    template<PolygonConcept OtherPolygon>
    [[nodiscard]] constexpr bool interiorContains(const OtherPolygon& other) const;

    template<PolygonConcept OtherPolygon>
    [[nodiscard]] constexpr bool separates(const OtherPolygon& other) const;


    /** @brief A disk never separates an isolated point. */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool separates(const OtherPoint& other) const;

    /**
     * @brief Tests whether removing the disk from the segment splits it into
     *        two pieces (the disk crosses the segment's interior).
     */
    template <SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool separates(const OtherSegment& other) const;

    /** @brief Same as the segment overload, ignoring the segment's orientation. */
    template <OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool separates(const OtherOrientedSegment& other) const;

    /** @brief Tests whether the disk meets the line, cutting it into two rays. */
    template <LineConcept OtherLine>
    [[nodiscard]] constexpr bool separates(const OtherLine& other) const;

    /** @brief Same as the line overload, ignoring the line's orientation. */
    template <OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool separates(const OtherOrientedLine& other) const;

    /** @brief Tests whether removing the disk from the convex polygon splits it. */
    template <ConvexConcept OtherConvex>
    [[nodiscard]] constexpr bool separates(const OtherConvex& other) const;

    /** @brief Tests whether removing the disk from the ray splits it. */
    template<RayConcept OtherRay>
    [[nodiscard]] constexpr bool separates(const OtherRay& other) const;

    /** @brief A disk never separates a halfplane. */
    template<HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool separates(const OtherHalfplane& other) const;

    /** @brief Tests whether removing the disk from the rectangle splits it. */
    template<RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool separates(const OtherRectangle& other) const;

    /** @brief Tests whether removing the disk from the triangle splits it. */
    template<TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr bool separates(const OtherTriangle& other) const;

    /** @brief A disk never separates another disk. */
    template<DiskConcept OtherDisk>
    [[nodiscard]] constexpr bool separates(const OtherDisk& other) const;

    // --- Symmetric predicate dispatch: intersects / interiorsIntersect / crosses ---
    // For a symmetric predicate a.method(b) == b.method(a). The canonical
    // implementor of each pair is the shape that appears later in pgl.hpp; the
    // earlier shape forwards to it. Disk is therefore canonical against every
    // shape up to and including Disk, and forwards to Convex/Polygon (which come
    // later). Pairs whose disk-side geometry is not yet implemented throw, in the
    // style of the other "not implemented yet" stubs.

    /** @brief intersects: not-yet-implemented disk pairs (throw); see implementation. */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool intersects(const OtherPoint& other) const;
    template<OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool intersects(const OtherOrientedSegment& other) const;
    template<LineConcept OtherLine>
    [[nodiscard]] constexpr bool intersects(const OtherLine& other) const;
    template<OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool intersects(const OtherOrientedLine& other) const;
    template<RayConcept OtherRay>
    [[nodiscard]] constexpr bool intersects(const OtherRay& other) const;
    template<HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool intersects(const OtherHalfplane& other) const;
    template<RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool intersects(const OtherRectangle& other) const;
    template<TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr bool intersects(const OtherTriangle& other) const;

    /** @brief interiorsIntersect: not-yet-implemented disk pairs (throw); see implementation. */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherPoint& other) const;
    template<SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherSegment& other) const;
    template<OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherOrientedSegment& other) const;
    template<LineConcept OtherLine>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherLine& other) const;
    template<OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherOrientedLine& other) const;
    template<RayConcept OtherRay>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherRay& other) const;
    template<HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherHalfplane& other) const;
    template<RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherRectangle& other) const;
    template<TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherTriangle& other) const;
    template<DiskConcept OtherDisk>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherDisk& other) const;

    /** @brief crosses: not-yet-implemented disk pairs (throw); see implementation. */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool crosses(const OtherPoint& other) const;
    template<SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool crosses(const OtherSegment& other) const;
    template<OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool crosses(const OtherOrientedSegment& other) const;
    template<LineConcept OtherLine>
    [[nodiscard]] constexpr bool crosses(const OtherLine& other) const;
    template<OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool crosses(const OtherOrientedLine& other) const;
    template<RayConcept OtherRay>
    [[nodiscard]] constexpr bool crosses(const OtherRay& other) const;
    template<HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool crosses(const OtherHalfplane& other) const;
    template<RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool crosses(const OtherRectangle& other) const;
    template<TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr bool crosses(const OtherTriangle& other) const;
    template<DiskConcept OtherDisk>
    [[nodiscard]] constexpr bool crosses(const OtherDisk& other) const;

    /** @brief Dispatches to the matching overload for the runtime type of @p other. */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool intersects(const Shape<OtherPoint>& other) const;
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorsIntersect(const Shape<OtherPoint>& other) const;
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool crosses(const Shape<OtherPoint>& other) const;

    /** @brief Forwards to a higher-ranked shape, the canonical implementor of the symmetric pair. */
    template<typename OtherShape>
        requires (!PointConcept<OtherShape> && detail::shapeRank<OtherShape> > detail::shapeRank<Disk>)
    [[nodiscard]] constexpr bool intersects(const OtherShape& other) const {
        return other.intersects(*this);
    }

    /** @brief The empty set never meets another shape. */
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool intersects(const EmptyShape<EmptyPoint>&) const {
        return false;
    }
    template<typename OtherShape>
        requires (!PointConcept<OtherShape> && detail::shapeRank<OtherShape> > detail::shapeRank<Disk>)
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherShape& other) const {
        return other.interiorsIntersect(*this);
    }

    /** @brief The empty set never meets another shape. */
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool interiorsIntersect(const EmptyShape<EmptyPoint>&) const {
        return false;
    }
    template<typename OtherShape>
        requires (!PointConcept<OtherShape> && detail::shapeRank<OtherShape> > detail::shapeRank<Disk>)
    [[nodiscard]] constexpr bool crosses(const OtherShape& other) const {
        return other.crosses(*this);
    }

    /** @brief The empty set never meets another shape. */
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool crosses(const EmptyShape<EmptyPoint>&) const {
        return false;
    }

    /**
     * @brief Divides every boundary point by @p scalar in place.
     * @warning Re-canonicalizes the points; integer division truncates coordinates.
     * @return Reference to this disk.
     */
    template <class Scalar>
        requires(!detail::is_point_v<Scalar>)
    constexpr Disk& operator/=(const Scalar& scalar) {
        for (auto& point : points_) {
            point /= scalar;
        }
        points_ = canonicalizePoints(points_[0], points_[1], points_[2]);
        return *this;
    }

    /**
     * @brief Returns whether two disks describe the same closed region.
     *
     * Two circles compare equal when @p other's three boundary points all lie
     * on this disk's boundary (exact in-circle test).
     */
    constexpr bool operator==(const Disk& other) const {
        // Degenerate == non-degenerate: false
        // Degenerate == degenerate with same points: true
        // Degenerate == degenerate with different points: false
        if (isDegenerate() || other.isDegenerate()) {
            if (isDegenerate() != other.isDegenerate()) {
                return false;
            }

            return a() == other.a() &&
                   b() == other.b() &&
                   c() == other.c();
        }

        // For true circles, three non-collinear boundary points define the
        // circle uniquely. So the other disk is the same if its three stored points lies on the disk boundary.
        return inCircleSign(a(), b(), c(), other.a()) == std::partial_ordering::equivalent &&
               inCircleSign(a(), b(), c(), other.b()) == std::partial_ordering::equivalent &&
               inCircleSign(a(), b(), c(), other.c()) == std::partial_ordering::equivalent;
    }

    /**
     * @brief Orders disks by increasing squared radius, then by center.
     * @warning Uses division through squaredRadius() and center().
     */
    constexpr std::partial_ordering operator<=>(const Disk& other) const {
        // Keep degenerate disks in a separate ordering class from true circles.
        if (isDegenerate() != other.isDegenerate()) {
            return isDegenerate() ? std::partial_ordering::less : std::partial_ordering::greater;
        }

        // Order true circles from smaller to larger, then by center.
        if (!isDegenerate()) {
            const auto radius_order = pgl::detail::threeWay(squaredRadius(), other.squaredRadius());
            if (radius_order != 0) {
                return radius_order;
            }
            return center() <=> other.center();
        }

        // Degenerate disks fall back to their canonical point representation.
        const auto first_point_order = a() <=> other.a();
        if (first_point_order != 0) {
            return first_point_order;
        }

        const auto second_point_order = b() <=> other.b();
        if (second_point_order != 0) {
            return second_point_order;
        }

        return c() <=> other.c();
    }

    /**
     * @brief Returns the disk rotated by 90k degrees around the origin.
     *
     * @param k Number of 90-degree CCW rotations (may be negative).
     * @return Rotated disk.
     */
    [[nodiscard]] constexpr Disk rotated90(int k = 1) const;

    /**
     * @brief Rotates the disk by 90k degrees around the origin in place.
     *
     * @param k Number of 90-degree CCW rotations (may be negative).
     */
    constexpr void rotate90(int k = 1);

    /**
     * @brief Translates the disk by @p translation in place.
     * @return Reference to this disk.
     */
    template <PointConcept OtherPoint>
    constexpr Disk& operator+=(const OtherPoint& translation) {
        for (auto& point : points_) {
            point += translation;
        }
        return *this;
    }

    /**
     * @brief Translates the disk by `-translation` in place.
     * @return Reference to this disk.
     */
    template <PointConcept OtherPoint>
    constexpr Disk& operator-=(const OtherPoint& translation) {
        for (auto& point : points_) {
            point -= translation;
        }
        return *this;
    }

    /**
     * @brief Scales every boundary point by @p scalar in place.
     * @warning Re-canonicalizes the points (a negative scalar flips orientation).
     * @return Reference to this disk.
     */
    template <class Scalar>
        requires(!detail::is_point_v<Scalar>)
    constexpr Disk& operator*=(const Scalar& scalar) {
        for (auto& point : points_) {
            point *= scalar;
        }
        points_ = canonicalizePoints(points_[0], points_[1], points_[2]);
        return *this;
    }

    /**
     * @brief Forward iterator over the three boundary points.
     */
    class PointIterator {
      public:
        using iterator_category = std::forward_iterator_tag;
        using iterator_concept = std::forward_iterator_tag;
        using value_type = PointType;
        using difference_type = std::ptrdiff_t;
        using reference = const PointType&;

        /** @brief Creates a singular (past-the-end style) iterator. */
        constexpr PointIterator() = default;

        /** @brief Returns the boundary point at the current position. */
        constexpr reference operator*() const {
            assert(disk != nullptr);
            return (*disk)[index];
        }

        /** @brief Advances to the next boundary point (pre-increment). */
        constexpr PointIterator& operator++() {
            ++index;
            return *this;
        }

        /** @brief Advances to the next boundary point (post-increment). */
        constexpr PointIterator operator++(int) {
            PointIterator copy(*this);
            ++(*this);
            return copy;
        }

        /** @brief Returns whether both iterators refer to the same position. */
        constexpr bool operator==(const PointIterator& other) const = default;

      private:
        friend struct Disk;

        /** @brief Creates an iterator over @p disk_arg starting at @p index_arg. */
        constexpr PointIterator(const Disk* disk_arg, std::size_t index_arg)
            : disk(disk_arg), index(index_arg) {}

        const Disk* disk = nullptr;  ///< Disk being iterated, or null when singular.
        std::size_t index = 0;       ///< Current boundary point index in `[0, size()]`.
    };

  private:
    // Lets Convex::interiorsIntersect(Disk) reuse the disk-interior witness below.
    template <class P, class L>
    friend struct Convex;

    /**
     * @brief Tests whether some point strictly inside this disk lies in the
     *        strict interior of @p shape.
     *
     * The witness used by the @ref interiorsIntersect overloads for the case
     * where the disk lies inside a convex shape (no boundary of @p shape crosses
     * the open disk). @ref pointInside is the chord midpoint — a single division
     * by 2 — so for integral coordinates the truncated midpoint may round onto or
     * outside the boundary. When that happens (the rounded point is not strictly
     * inside the disk) scaling the disk and @p shape by 2 makes the midpoint
     * exact, leaving the containment relation unchanged.
     */
    template <class OtherShape>
    [[nodiscard]] constexpr bool pointInsideInteriorContained(const OtherShape& shape) const {
        const auto p = pointInside();
        if (interiorContains(p)) {
            return shape.interiorContains(p);
        }
        return (shape * 2).interiorContains((*this * 2).pointInside());
    }

    /**
     * @brief Returns the three points in canonical order.
     *
     * Points are sorted lexicographically and reordered to counterclockwise
     * orientation so that geometrically equal disks share the same stored
     * representation.
     */
    static constexpr std::array<PointType, 3> canonicalizePoints(PointType first, PointType second, PointType third) {
        std::array<PointType, 3> points{
            std::move(first),
            std::move(second),
            std::move(third),
        };

        // Sort the three stored boundary points lexicographically
        if (points[1] < points[0]) {
            std::swap(points[0], points[1]);
        }
        if (points[2] < points[1]) {
            std::swap(points[1], points[2]);
        }
        if (points[1] < points[0]) {
            std::swap(points[0], points[1]);
        }

        // Keep non-degenerate disks in counterclockwise order
        if (orientationSign(points[0], points[1], points[2]) == std::partial_ordering::less) {
            std::swap(points[1], points[2]);
        }

        return points;
    }

    std::array<PointType, 3> points_{};        ///< Canonicalized boundary points.
    [[no_unique_address]] mutable LabelType label_{};  ///< Optional disk label.
};

/// @brief Returns a copy of @p disk translated by @p translation.
template <class PointType, class LabelType, class TranslationNumber, class TranslationLabel>
constexpr auto operator+(const Disk<PointType, LabelType>& disk, const Point<TranslationNumber, TranslationLabel>& translation) {
    auto result = disk;
    result += translation;
    if constexpr (detail::has_label_v<LabelType>) {
        result.label() = LabelType{};
    }
    return result;
}

/// @brief Returns a copy of @p disk translated by @p translation.
template <class TranslationNumber, class TranslationLabel, class PointType, class LabelType>
constexpr auto operator+(const Point<TranslationNumber, TranslationLabel>& translation, const Disk<PointType, LabelType>& disk) {
    return disk + translation;
}

/// @brief Returns a copy of @p disk translated by `-translation`.
template <class PointType, class LabelType, class TranslationNumber, class TranslationLabel>
constexpr auto operator-(const Disk<PointType, LabelType>& disk, const Point<TranslationNumber, TranslationLabel>& translation) {
    auto result = disk;
    result -= translation;
    if constexpr (detail::has_label_v<LabelType>) {
        result.label() = LabelType{};
    }
    return result;
}

/// @brief Returns a copy of @p disk with every boundary point scaled by @p scalar.
template <class PointType, class LabelType, class Scalar>
    requires(!detail::is_point_v<Scalar>)
constexpr auto operator*(const Disk<PointType, LabelType>& disk, const Scalar& scalar) {
    auto result = disk;
    result *= scalar;
    if constexpr (detail::has_label_v<LabelType>) {
        result.label() = LabelType{};
    }
    return result;
}

/// @brief Returns a copy of @p disk with every boundary point scaled by @p scalar.
template <class Scalar, class PointType, class LabelType>
    requires(!detail::is_point_v<Scalar>)
constexpr auto operator*(const Scalar& scalar, const Disk<PointType, LabelType>& disk) {
    return disk * scalar;
}

/// @brief Returns a copy of @p disk with every boundary point divided by @p scalar.
template <class PointType, class LabelType, class Scalar>
    requires(!detail::is_point_v<Scalar>)
constexpr auto operator/(const Disk<PointType, LabelType>& disk, const Scalar& scalar) {
    auto result = disk;
    result /= scalar;
    if constexpr (detail::has_label_v<LabelType>) {
        result.label() = LabelType{};
    }
    return result;
}

/// @brief Writes @p disk to @p stream as `Disk(a b c)`.
template <class PointType, class LabelType>
std::ostream& operator<<(std::ostream& stream, const Disk<PointType, LabelType>& disk);

}  // namespace pgl

#endif // PGL_GEOMETRY_DISK_HPP
#endif // PGL_HPP_INCLUDED
