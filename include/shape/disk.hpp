#pragma once

#include "shape/triangle.hpp"

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
#include <optional>
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
        if (auto cr = centerAndRadius()) {
            return Point<ResultNumber, PointLabelType>(cr->first);
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
        if (auto cr = centerAndRadius()) {
            return static_cast<ResultNumber>(cr->second);
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
     * @brief Returns whether the disk collapses to a single point.
     *
     * True when all three boundary points coincide, i.e. the disk has zero
     * radius.
     *
     * Complexity: O(1).
     */
    [[nodiscard]] constexpr bool isPoint() const {
        return a() == b() && b() == c();
    }

    /**
     * @brief Returns the point the disk collapses to, if it does.
     *
     * Complexity: O(1).
     *
     * @return The common boundary point if @ref isPoint, `std::nullopt`
     *         otherwise.
     */
    [[nodiscard]] constexpr std::optional<PointType> getIfPoint() const {
        if (!isPoint()) {
            return std::nullopt;
        }
        return a();
    }

    /**
     * @brief Returns whether the disk is degenerate without collapsing to a
     * point.
     *
     * True when the boundary points are collinear but not all equal, so they do
     * not determine a circle: three distinct collinear points have no circle
     * through them, and two distinct ones (a repeated point) have infinitely
     * many. A disk never collapses to a segment, so together with @ref isPoint
     * this covers every degenerate disk.
     *
     * Complexity: O(1).
     */
    [[nodiscard]] constexpr bool isUndefined() const {
        return !isPoint() && isDegenerate();
    }

    /**
     * @brief Returns the squared radius in an explicitly chosen result type.
     *
     * @tparam ResultNumber Result type.
     * @warning If the disk has not been defined by center and radius, it uses the circumradius formula, which divides by the squared area.
     */
    template <class ResultNumber = NumberType>
    [[nodiscard]] constexpr ResultNumber squaredRadius() const {
        if (auto cr = centerAndRadius()) {
            return static_cast<ResultNumber>(cr->second) * static_cast<ResultNumber>(cr->second);
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
     * @brief Returns an axis-aligned bounding box in the coordinate type.
     *
     * The box is guaranteed to contain the disk. When the disk was built from a
     * center and radius the box is tight; otherwise the box may be larger than
     * the disk.
     *
     * @warning The box may be larger than the disk if it is not contructed by center and radius.
     */
    [[nodiscard]] constexpr Rectangle<PointType> bbox() const {
        // Exact when built from a center and radius: center +/- (radius, radius).
        if (auto cr = centerAndRadius()) {
            const auto radius = cr->second;
            return Rectangle<PointType>(cr->first - PointType(radius, radius),
                                        cr->first + PointType(radius, radius));
        }

        // Degenerate disk (collinear boundary points): no finite circle, so the
        // bounding box of the three boundary points contains it.
        if (isDegenerate()) {
            const auto lo = [](NumberType u, NumberType v, NumberType w) { return u < v ? (u < w ? u : w) : (v < w ? v : w); };
            const auto hi = [](NumberType u, NumberType v, NumberType w) { return u > v ? (u > w ? u : w) : (v > w ? v : w); };
            return Rectangle<PointType>(
                PointType(lo(a().x(), b().x(), c().x()), lo(a().y(), b().y(), c().y())),
                PointType(hi(a().x(), b().x(), c().x()), hi(a().y(), b().y(), c().y())));
        }

        // General case: the circumcenter is the rational point (nx/d, ny/d) and
        // the radius is sqrt(s)/d, where d = 2*det(A,B,C) > 0 and
        // s = |AB|^2 |BC|^2 |CA|^2 = (radius*d)^2. The box spans
        // [cx +/- r] x [cy +/- r]; how it is rounded depends on the coordinate
        // type, but the result always contains the disk (and may be larger).
        if constexpr (std::floating_point<NumberType>) {
            // Real square root is available: center +/- radius, tight up to
            // floating-point rounding.
            const NumberType r = radius<NumberType>();
            const auto ctr = center<NumberType>();
            return Rectangle<PointType>(ctr - PointType(r, r), ctr + PointType(r, r));
        } else {
            using Wide = detail::promoted_number_t<NumberType>;
            const Wide ax = a().x(), ay = a().y();
            const Wide bx = b().x(), by = b().y();
            const Wide cx = c().x(), cy = c().y();
            const Wide aa = ax * ax + ay * ay;
            const Wide bb = bx * bx + by * by;
            const Wide cc = cx * cx + cy * cy;

            Wide d  = Wide{2} * (ax * (by - cy) + bx * (cy - ay) + cx * (ay - by));
            Wide nx = aa * (by - cy) + bb * (cy - ay) + cc * (ay - by);
            Wide ny = aa * (cx - bx) + bb * (ax - cx) + cc * (bx - ax);
            if (d < Wide{0}) {           // normalize to d > 0
                d = -d; nx = -nx; ny = -ny;
            }

            const Wide ab2 = (ax - bx) * (ax - bx) + (ay - by) * (ay - by);
            const Wide bc2 = (bx - cx) * (bx - cx) + (by - cy) * (by - cy);
            const Wide ca2 = (cx - ax) * (cx - ax) + (cy - ay) * (cy - ay);
            const Wide s = ab2 * bc2 * ca2;   // (radius * d)^2

            // ceil(sqrt(n)) for a non-negative *integer* n: floor-sqrt (std::sqrt
            // when usable, else integer Newton) followed by an upward correction.
            // The argument is always an integer, so this terminates.
            const auto ceilIntSqrt = [](auto n) {
                using Int = decltype(n);
                Int r{};
                if constexpr (requires { std::sqrt(n); }) {
                    r = static_cast<Int>(std::sqrt(n));
                } else if (n >= Int{2}) {
                    Int x = n, y = (n + Int{1}) / Int{2};
                    while (y < x) { x = y; y = (x + n / x) / Int{2}; }
                    r = x;
                } else {
                    r = n;
                }
                while (r * r < n) { ++r; }
                return r;
            };

            if constexpr (requires { s.numerator(); s.denominator(); }) {
                // sqrt(s) = sqrt(sn*sd)/sd <= ceil(sqrt(sn*sd))/sd =: u, an exact
                // rational upper bound whose only square root is over the integer
                // sn*sd -- so it never iterates on the (irrational) sqrt itself.
                using Int = std::remove_cvref_t<decltype(s.numerator())>;
                const Int sn = s.numerator();
                const Int sd = s.denominator();
                const Wide u(ceilIntSqrt(sn * sd), sd);   // u >= sqrt(s)
                return Rectangle<PointType>(
                    PointType(static_cast<NumberType>((nx - u) / d),
                              static_cast<NumberType>((ny - u) / d)),
                    PointType(static_cast<NumberType>((nx + u) / d),
                              static_cast<NumberType>((ny + u) / d)));
            } else {
                // Integer coordinates: round (nx +/- q)/d outward to integers,
                // with q = ceil(sqrt(s)) >= radius*d.
                const Wide q = ceilIntSqrt(s);
                const auto floorDiv = [](Wide p, Wide den) {   // den > 0
                    return p >= Wide{0} ? p / den : -((-p + den - Wide{1}) / den);
                };
                const auto ceilDiv = [](Wide p, Wide den) {    // den > 0
                    return p >= Wide{0} ? (p + den - Wide{1}) / den : -((-p) / den);
                };
                return Rectangle<PointType>(
                    PointType(static_cast<NumberType>(floorDiv(nx - q, d)),
                              static_cast<NumberType>(floorDiv(ny - q, d))),
                    PointType(static_cast<NumberType>(ceilDiv(nx + q, d)),
                              static_cast<NumberType>(ceilDiv(ny + q, d))));
            }
        }
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
     * @brief Returns a segment defining a diameter of the disk.
     *
     * For a genuine 3-point disk the segment uses `a()` and its reflection across
     * the exact center, so integral boundary points can still yield exact rational
     * endpoints without introducing a square root.
     *
     * @tparam ResultNumber Coordinate type of the returned endpoints.
     * @warning Uses division unless the disk is defined by center and radius.
     */
    template <class ResultNumber = NumberType>
    [[nodiscard]] constexpr Segment<Point<ResultNumber, PointLabelType>> diameter() const {
        using ResultPoint = Point<ResultNumber, PointLabelType>;

        if (auto cr = centerAndRadius()) {
            return Segment<ResultPoint>(a(),b());
        }

        if (isDegenerate()) {
            return Segment<ResultPoint>();
        }

        const ResultPoint center_point = center<ResultNumber>();
        ResultPoint anti_a = center_point + (center_point-a());
        return Segment<ResultPoint>(static_cast<ResultPoint>(a()), anti_a);
    }

    /**
     * @brief Returns the squared Euclidean distance from this disk to a point.
     *
     * Zero when the disk contains the point (boundary inclusive); otherwise the
     * square of `|point - center| - radius`, the gap between the point and the
     * nearest point of the circle.
     *
     * Always returns `double`: the distance to an exterior point is generally
     * irrational, so unlike the other shapes there is no exact result to request
     * and no `ResultNumber` template parameter.
     */
    template <PointConcept OtherPoint>
    [[nodiscard]] double squaredDistance(const OtherPoint& point) const;

    /**
     * @brief Returns the squared Euclidean distance from this disk to a shape.
     *
     * Zero when the disk and the shape intersect; otherwise the square of
     * `distance(center, shape) - radius`, the gap between the shape and the
     * nearest point of the circle. Because the disk is the set of points within
     * `radius` of the center, this gap is the disk-to-shape distance.
     *
     * Always returns `double`, like @ref squaredDistance(const OtherPoint&) const:
     * the distance to a disjoint shape is generally irrational, so there is no
     * exact result to request and no `ResultNumber` template parameter. The lower-
     * ranked shapes forward their `squaredDistance(Disk)` to this overload.
     */
    template <SegmentConcept OtherSegment>
    [[nodiscard]] double squaredDistance(const OtherSegment& other) const;

    /** @copydoc squaredDistance(const OtherSegment&) const */
    template <OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] double squaredDistance(const OtherOrientedSegment& other) const;

    /** @copydoc squaredDistance(const OtherSegment&) const */
    template <LineConcept OtherLine>
    [[nodiscard]] double squaredDistance(const OtherLine& other) const;

    /** @copydoc squaredDistance(const OtherSegment&) const */
    template <OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] double squaredDistance(const OtherOrientedLine& other) const;

    /** @copydoc squaredDistance(const OtherSegment&) const */
    template <RayConcept OtherRay>
    [[nodiscard]] double squaredDistance(const OtherRay& other) const;

    /** @copydoc squaredDistance(const OtherSegment&) const */
    template <HalfplaneConcept OtherHalfplane>
    [[nodiscard]] double squaredDistance(const OtherHalfplane& other) const;

    /** @copydoc squaredDistance(const OtherSegment&) const */
    template <RectangleConcept OtherRectangle>
    [[nodiscard]] double squaredDistance(const OtherRectangle& other) const;

    /** @copydoc squaredDistance(const OtherSegment&) const */
    template <TriangleConcept OtherTriangle>
    [[nodiscard]] double squaredDistance(const OtherTriangle& other) const;

    /**
     * @brief Returns the squared Euclidean distance between two disks.
     *
     * Zero when the disks intersect (touching counts); otherwise the square of
     * `distance(centers) - radius - other.radius`. Always returns `double`.
     */
    template <DiskConcept OtherDisk>
    [[nodiscard]] double squaredDistance(const OtherDisk& other) const;

    /**
     * @brief Returns the squared Euclidean distance to the given shape.
     *
     * Forwards to the other shape's implementation so that each unordered pair
     * needs `squaredDistance` defined only once, on the higher-ranked shape (the
     * shapes ranked above @ref Disk are @ref Convex and @ref Polygon).
     */
    template <typename OtherShape>
        requires (!PointConcept<OtherShape> && detail::shapeRank<OtherShape> > detail::shapeRank<Disk>
                  && requires(const OtherShape& o, const Disk& self) { o.squaredDistance(self); })
    [[nodiscard]] double squaredDistance(const OtherShape& other) const {
        return other.squaredDistance(*this);
    }

    /**
     * @brief Returns the Manhattan (L1) distance from this disk to a point.
     *
     * Zero when the disk contains the point. Otherwise the nearest point of
     * the disk lies on the circle, and its L1 distance to an external point
     * is a trigonometric optimization with no closed form (unlike the
     * Euclidean case's `|point - center| - radius`), so this refines a
     * coarse angular scan with a golden-section search. Always returns
     * `double`, like @ref squaredDistance(const OtherPoint&) const.
     */
    template <PointConcept OtherPoint>
    [[nodiscard]] double distanceL1(const OtherPoint& point) const;

    /**
     * @brief Returns the Chebyshev (LInf) distance from this disk to a point.
     *
     * @copydetails distanceL1(const OtherPoint&) const
     */
    template <PointConcept OtherPoint>
    [[nodiscard]] double distanceLInf(const OtherPoint& point) const;

    /**
     * @brief Returns the Manhattan (L1) distance to the given shape.
     *
     * Distance is symmetric, so this just calls @p other's own `distanceL1`
     * requesting `double` (like every other `Disk` overload), which visits
     * its wrapped alternative and throws if the pair is unsupported.
     *
     * @warning Fixed to this disk's own `Shape<PointType>` rather than a
     *          deduced point type: `Disk::distanceL1` has no `ResultNumber`
     *          template of its own (it always returns `double`), so it is a
     *          function with a single template parameter, just like the
     *          plain `distanceL1(const OtherPoint&)` above. If that single
     *          slot were a deduced point type here too, an external explicit
     *          probe such as `o.template distanceL1<ResultNumber>(self)`
     *          (used throughout this codebase's rank-forwarding machinery)
     *          would bind `ResultNumber`'s type directly to it, forcing an
     *          attempt to form `Shape<ResultNumberType>` — e.g. `Shape<int>`
     *          — which is not a valid point type and triggers a hard error
     *          deep inside `std::variant`'s instantiation, not a
     *          SFINAE-friendly failure. A fixed, already-valid parameter type
     *          has no template parameter for an explicit argument to land on,
     *          so such a probe is simply rejected as an arity mismatch.
     */
    [[nodiscard]] double distanceL1(const Shape<PointType>& other) const {
        return other.template distanceL1<double>(*this);
    }

    /** @copydoc distanceL1(const Shape<PointType>&) const */
    [[nodiscard]] double distanceLInf(const Shape<PointType>& other) const {
        return other.template distanceLInf<double>(*this);
    }

    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
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

    /**
     * @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint.
     *
     * @tparam ResultNumber Coordinate type of the returned point.
     * @tparam OtherPoint Point type.
     * @param other Point to intersect with.
     * @return The point when contained, otherwise empty.
     */
    template <class ResultNumber = NumberType, PointConcept OtherPoint>
    [[nodiscard]] constexpr std::optional<Point<ResultNumber, typename PointType::LabelType>>
    intersection(const OtherPoint& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template <SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool contains(const OtherSegment& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template <OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool contains(const OtherOrientedSegment& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template <LineConcept OtherLine>
    [[nodiscard]] constexpr bool contains(const OtherLine& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template <OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool contains(const OtherOrientedLine& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template <RayConcept OtherRay>
    [[nodiscard]] constexpr bool contains(const OtherRay& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template <HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool contains(const OtherHalfplane& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template <TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr bool contains(const OtherTriangle& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template <RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool contains(const OtherRectangle& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template <ConvexConcept OtherConvex>
    [[nodiscard]] constexpr bool contains(const OtherConvex& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template<PolygonConcept OtherPolygon>
    [[nodiscard]] constexpr bool contains(const OtherPolygon& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template <DiskConcept OtherDisk>
    [[nodiscard]] constexpr bool contains(const OtherDisk& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    [[nodiscard]] constexpr bool contains(const Shape<PointType>& other) const;

    // The empty set is a subset of every shape, so its containment relations are
    // true; the symmetric intersection/crossing predicates reach the empty set
    // through Disk's existing generic OtherShape fallbacks.
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

    /**
     * @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B).
     *
     * Strict version of @ref contains: a point exactly on the boundary circle is
     * not interior-contained. Because the open disk is convex, the shape
     * overloads below reduce to this test on a finite set of vertices.
     */
    template <PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorContains(const OtherPoint& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template <SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool interiorContains(const OtherSegment& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template <OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool interiorContains(const OtherOrientedSegment& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template <LineConcept OtherLine>
    [[nodiscard]] constexpr bool interiorContains(const OtherLine& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template <OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool interiorContains(const OtherOrientedLine& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template <RayConcept OtherRay>
    [[nodiscard]] constexpr bool interiorContains(const OtherRay& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template <HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool interiorContains(const OtherHalfplane& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template <TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr bool interiorContains(const OtherTriangle& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template <RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool interiorContains(const OtherRectangle& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template <ConvexConcept OtherConvex>
    [[nodiscard]] constexpr bool interiorContains(const OtherConvex& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template <DiskConcept OtherDisk>
    [[nodiscard]] constexpr bool interiorContains(const OtherDisk& other) const;

    /**
     * @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅).
     */
    template <SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool intersects(const OtherSegment& other) const;

    /**
     * @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅).
     *
     * True when the distance between the centers is at most the sum of the radii
     * (touching disks count as intersecting).
     */
    template <DiskConcept OtherDisk>
    [[nodiscard]] constexpr bool intersects(const OtherDisk& other) const;

    /**
     * @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B).
     *
     * Unlike @ref contains, this is the one-dimensional circle, not the filled
     * region.
     *
     * @tparam OtherPoint Point type to test.
     * @return `true` if @p other lies on the circle (exact in-circle test).
     */
    template <PointConcept OtherPoint>
    [[nodiscard]] constexpr bool boundaryContains(const OtherPoint& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template <SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool boundaryContains(const OtherSegment& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template <OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool boundaryContains(const OtherOrientedSegment& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template <LineConcept OtherLine>
    [[nodiscard]] constexpr bool boundaryContains(const OtherLine& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template <OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool boundaryContains(const OtherOrientedLine& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template <RayConcept OtherRay>
    [[nodiscard]] constexpr bool boundaryContains(const OtherRay& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template <HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool boundaryContains(const OtherHalfplane& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template <TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr bool boundaryContains(const OtherTriangle& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template <RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool boundaryContains(const OtherRectangle& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template <ConvexConcept OtherConvex>
    [[nodiscard]] constexpr bool boundaryContains(const OtherConvex& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<PolygonConcept OtherPolygon>
    [[nodiscard]] constexpr bool boundaryContains(const OtherPolygon& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template <DiskConcept OtherDisk>
    [[nodiscard]] constexpr bool boundaryContains(const OtherDisk& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    [[nodiscard]] constexpr bool boundaryContains(const Shape<PointType>& other) const;

    // --- not-yet-implemented predicate pairs (throw); see implementation ---
    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template<PolygonConcept OtherPolygon>
    [[nodiscard]] constexpr bool interiorContains(const OtherPolygon& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template<PolygonConcept OtherPolygon>
    [[nodiscard]] constexpr bool separates(const OtherPolygon& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template<MonotoneChainConcept OtherChain>
    [[nodiscard]] constexpr bool contains(const OtherChain& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<MonotoneChainConcept OtherChain>
    [[nodiscard]] constexpr bool boundaryContains(const OtherChain& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template<MonotoneChainConcept OtherChain>
    [[nodiscard]] constexpr bool interiorContains(const OtherChain& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template<MonotoneChainConcept OtherChain>
    [[nodiscard]] constexpr bool separates(const OtherChain& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template<PolylineConcept OtherPolyline>
    [[nodiscard]] constexpr bool contains(const OtherPolyline& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<PolylineConcept OtherPolyline>
    [[nodiscard]] constexpr bool boundaryContains(const OtherPolyline& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template<PolylineConcept OtherPolyline>
    [[nodiscard]] constexpr bool interiorContains(const OtherPolyline& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template<PolylineConcept OtherPolyline>
    [[nodiscard]] constexpr bool separates(const OtherPolyline& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template<HalfplaneIntersectionConcept OtherRegion>
    [[nodiscard]] constexpr bool contains(const OtherRegion& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<HalfplaneIntersectionConcept OtherRegion>
    [[nodiscard]] constexpr bool boundaryContains(const OtherRegion& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template<HalfplaneIntersectionConcept OtherRegion>
    [[nodiscard]] constexpr bool interiorContains(const OtherRegion& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template<HalfplaneIntersectionConcept OtherRegion>
    [[nodiscard]] constexpr bool separates(const OtherRegion& other) const;


    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool separates(const OtherPoint& other) const;

    /**
     * @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected).
     */
    template <SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool separates(const OtherSegment& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template <OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool separates(const OtherOrientedSegment& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template <LineConcept OtherLine>
    [[nodiscard]] constexpr bool separates(const OtherLine& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template <OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool separates(const OtherOrientedLine& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template <ConvexConcept OtherConvex>
    [[nodiscard]] constexpr bool separates(const OtherConvex& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template<RayConcept OtherRay>
    [[nodiscard]] constexpr bool separates(const OtherRay& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template<HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool separates(const OtherHalfplane& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template<RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool separates(const OtherRectangle& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template<TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr bool separates(const OtherTriangle& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template<DiskConcept OtherDisk>
    [[nodiscard]] constexpr bool separates(const OtherDisk& other) const;

    // --- Symmetric predicate dispatch: intersects / interiorsIntersect / crosses ---
    // For a symmetric predicate a.method(b) == b.method(a). The canonical
    // implementor of each pair is the shape that appears later in pgl.hpp; the
    // earlier shape forwards to it. Disk is therefore canonical against every
    // shape up to and including Disk, and forwards to Convex/Polygon (which come
    // later). Pairs whose disk-side geometry is not yet implemented throw, in the
    // style of the other "not implemented yet" stubs.

    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool intersects(const OtherPoint& other) const;
    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template<OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool intersects(const OtherOrientedSegment& other) const;
    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template<LineConcept OtherLine>
    [[nodiscard]] constexpr bool intersects(const OtherLine& other) const;
    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template<OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool intersects(const OtherOrientedLine& other) const;
    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template<RayConcept OtherRay>
    [[nodiscard]] constexpr bool intersects(const OtherRay& other) const;
    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template<HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool intersects(const OtherHalfplane& other) const;
    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template<RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool intersects(const OtherRectangle& other) const;
    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template<TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr bool intersects(const OtherTriangle& other) const;

    /** @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅). */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherPoint& other) const;
    /** @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅). */
    template<SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherSegment& other) const;
    /** @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅). */
    template<OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherOrientedSegment& other) const;
    /** @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅). */
    template<LineConcept OtherLine>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherLine& other) const;
    /** @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅). */
    template<OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherOrientedLine& other) const;
    /** @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅). */
    template<RayConcept OtherRay>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherRay& other) const;
    /** @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅). */
    template<HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherHalfplane& other) const;
    /** @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅). */
    template<RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherRectangle& other) const;
    /** @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅). */
    template<TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherTriangle& other) const;
    /** @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅). */
    template<DiskConcept OtherDisk>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherDisk& other) const;

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool crosses(const OtherPoint& other) const;
    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template<SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool crosses(const OtherSegment& other) const;
    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template<OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool crosses(const OtherOrientedSegment& other) const;
    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template<LineConcept OtherLine>
    [[nodiscard]] constexpr bool crosses(const OtherLine& other) const;
    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template<OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool crosses(const OtherOrientedLine& other) const;
    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template<RayConcept OtherRay>
    [[nodiscard]] constexpr bool crosses(const OtherRay& other) const;
    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template<HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool crosses(const OtherHalfplane& other) const;
    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template<RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool crosses(const OtherRectangle& other) const;
    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template<TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr bool crosses(const OtherTriangle& other) const;
    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template<DiskConcept OtherDisk>
    [[nodiscard]] constexpr bool crosses(const OtherDisk& other) const;

    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool intersects(const Shape<OtherPoint>& other) const;
    /** @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅). */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorsIntersect(const Shape<OtherPoint>& other) const;
    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool crosses(const Shape<OtherPoint>& other) const;

    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template<typename OtherShape>
        requires (!PointConcept<OtherShape> && detail::shapeRank<OtherShape> > detail::shapeRank<Disk>)
    [[nodiscard]] constexpr bool intersects(const OtherShape& other) const {
        return other.intersects(*this);
    }

    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool intersects(const EmptyShape<EmptyPoint>&) const {
        return false;
    }
    /** @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅). */
    template<typename OtherShape>
        requires (!PointConcept<OtherShape> && detail::shapeRank<OtherShape> > detail::shapeRank<Disk>)
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherShape& other) const {
        return other.interiorsIntersect(*this);
    }

    /** @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅). */
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool interiorsIntersect(const EmptyShape<EmptyPoint>&) const {
        return false;
    }
    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template<typename OtherShape>
        requires (!PointConcept<OtherShape> && detail::shapeRank<OtherShape> > detail::shapeRank<Disk>)
    [[nodiscard]] constexpr bool crosses(const OtherShape& other) const {
        return other.crosses(*this);
    }

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
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
        requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
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
        requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
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
    [[nodiscard]] constexpr bool pointInsideInteriorContainedIn(const OtherShape& shape) const {
        const auto p = pointInside();
        if (interiorContains(p)) {
            return shape.interiorContains(p);
        }
        return (shape * 2).interiorContains((*this * 2).pointInside());
    }

  private:
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

    /**
     * @brief Exact center and radius when the disk was built from a center
     * and radius (axis-aligned boundary points), otherwise `nullopt`.
     */
    [[nodiscard]] constexpr std::optional<std::pair<PointType, NumberType>> centerAndRadius() const {
        if (a().y() == b().y()) {
            NumberType r = c().y() - a().y();
            NumberType center_x = a().x() + r;
            if (r >= 0 && b().x() == center_x + r && c().x() == center_x) {
                return std::make_pair(PointType(center_x, a().y()), r);
            }
        }
        return std::nullopt;
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
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
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
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr auto operator*(const Scalar& scalar, const Disk<PointType, LabelType>& disk) {
    return disk * scalar;
}

/// @brief Returns a copy of @p disk with every boundary point divided by @p scalar.
template <class PointType, class LabelType, class Scalar>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
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
