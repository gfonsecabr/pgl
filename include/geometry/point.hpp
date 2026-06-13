#pragma once

/**
 * @file point.hpp
 * @brief Public declaration of pgl::Point and point-label helpers.
 *
 * Point is the zero-dimensional primitive at the center of the library. Other
 * shapes, predicates, and algorithms are all expressed in terms of points.
 */

#include <cassert>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <functional>
#include <optional>
#include <ostream>
#include <type_traits>
#include <utility>
#include <array>
#include <vector>

#include "../pgl.hpp"

namespace pgl {

/**
 * @brief Sentinel type used when a point carries no extra label.
 */
struct NoLabel {};

template <class Number = int, class Label = NoLabel>
struct Point;

Point() -> Point<int>;

template <class Number>
Point(Number, Number) -> Point<Number>;

template <class Number, class Label>
Point(Number, Number, Label) -> Point<Number, std::decay_t<Label>>;

namespace detail {
    /**
     * @brief Tells whether a label type stores real data or is just NoLabel.
     *
     * @tparam Label Label type to inspect.
     */
    template <class Label>
    inline constexpr bool has_label_v = !std::same_as<Label, NoLabel>;

    template <class T>
    struct is_point : std::false_type {};

    template <class Number, class Label>
    struct is_point<Point<Number, Label>> : std::true_type {};

    template <class T>
    inline constexpr bool is_point_v = is_point<std::remove_cvref_t<T>>::value;

    /**
     * @brief Checks whether a target label can be built from a source one.
     *
     * @tparam TargetLabel Label type of the destination point.
     * @tparam SourceLabel Label type of the source point.
     */
    template <class TargetLabel, class SourceLabel>
    inline constexpr bool can_copy_label_v =
        !has_label_v<TargetLabel> ||
        (has_label_v<SourceLabel> && std::constructible_from<TargetLabel, const SourceLabel&>) ||
        (!has_label_v<SourceLabel> && std::default_initializable<TargetLabel>);

    /**
     * @brief Copies, converts, or synthesizes a label during point conversion.
     *
     * @tparam TargetLabel Label type of the destination point.
     * @tparam OtherNumber Coordinate type of the source point.
     * @tparam OtherPoint::LabelType Label type of the source point.
     * @param other Source point.
     * @return The label value to store in the destination point.
     */
    template <class TargetLabel, class OtherPoint>
    constexpr auto copyLabel(const OtherPoint& other) {
        if constexpr (!has_label_v<TargetLabel>) {
            return NoLabel{};
        } else if constexpr (!has_label_v<typename OtherPoint::LabelType>) {
            return TargetLabel{};
        } else {
            return TargetLabel(other.label());
        }
    }

} //detail

template <class T>
concept PointConcept = detail::is_point_v<T>;


/**
 * @brief 2D point with optional label storage.
 *
 * @tparam Number Coordinate type.
 * @tparam Label Optional label or metadata type. The label is ignored when comparing points.
 */
template <class TNumber, class TLabel>
struct Point {
    /** Type of the point coordinates. */
    using NumberType = TNumber;
    /** Type of the point label. */
    using LabelType = TLabel;

    /**
     * @brief Creates the origin point `(0, 0)`.
     */
    constexpr Point() = default;

    /**
     * @brief Creates a point from its x and y coordinates.
     *
     * @param x X coordinate.
     * @param y Y coordinate.
     */
    constexpr Point(NumberType x, NumberType y)
        : coords_{x, y} {}

    /**
     * @brief Creates a point from its coordinates and label.
     *
     * Coordinates are converted to @ref NumberType with an explicit cast, so
     * explicit-only conversions (e.g. widening a @ref Rational) are accepted.
     * When @ref LabelType is @ref NoLabel the @p label argument must itself be a
     * `NoLabel` and is discarded; otherwise it constructs the stored label.
     *
     * @tparam X Type of the x coordinate expression.
     * @tparam Y Type of the y coordinate expression.
     * @tparam A Type used to construct the label.
     * @param x X coordinate.
     * @param y Y coordinate.
     * @param label Label value.
     */
    template <class X, class Y, class A>
        requires(std::constructible_from<NumberType, const X&> &&
                 std::constructible_from<NumberType, const Y&> &&
                 std::constructible_from<LabelType, A&&>)
    constexpr Point(const X& x, const Y& y, A&& label)
        : coords_{static_cast<NumberType>(x), static_cast<NumberType>(y)},
          label_(std::forward<A>(label)) {}

    /**
     * @brief Converts a point with different coordinate and label types.
     *
     * @tparam OtherNumber Coordinate type of the source point.
     * @tparam OtherPoint::LabelType Label type of the source point.
     * @param other Source point.
     */
    template <PointConcept OtherPoint>
        // requires(std::convertible_to<typename OtherPoint::Number, NumberType> && detail::can_copy_label_v<LabelType, typename OtherPoint::LabelType>)
    constexpr Point(const OtherPoint& other)
        : coords_{
              detail::convertCoordinate<NumberType>(other.x()),
              detail::convertCoordinate<NumberType>(other.y()),
          },
          label_(detail::copyLabel<LabelType>(other)) {}

    /**
     * @brief Returns the x coordinate.
     *
     * @return Reference to the x coordinate.
     */
    constexpr const NumberType& x() const {
        return coords_[0];
    }
    NumberType& x() {
        return coords_[0];
    }

    /**
     * @brief Returns the y coordinate.
     *
     * @return Reference to the y coordinate.
     */
    constexpr const NumberType& y() const {
        return coords_[1];
    }
    NumberType& y() {
        return coords_[1];
    }

    /**
     * @brief Returns the stored label.
     *
     * @tparam A Label type.
     * @return Reference to the stored label.
     */
    template <class A = LabelType>
        requires(detail::has_label_v<A>)
    constexpr const A& label() const {
        return label_;
    }
    template <class A = LabelType>
        requires(detail::has_label_v<A>)
    constexpr A& label() {
        return label_;
    }

    /**
     * @brief Returns coordinate `0` for x and `1` for y.
     *
     * @param index Coordinate index.
     * @return Reference to the selected coordinate.
     */
    constexpr const NumberType& operator[](std::size_t index) const {
        assert(index < size());
        return coords_[index];
    }
    NumberType& operator[](std::size_t index) {
        assert(index < size());
        return coords_[index];
    }

    /**
     * @brief Returns the number of coordinates (always 2).
     */
    static constexpr std::size_t size() {
        return 2;
    }

    /**
     * @brief Cyclic access: same as @ref operator[] but `index` is taken
     * modulo @ref size(); negative indices wrap from the end.
     */
    constexpr const NumberType& get(std::ptrdiff_t index) const {
        const std::ptrdiff_t n = static_cast<std::ptrdiff_t>(size());
        return (*this)[static_cast<std::size_t>(((index % n) + n) % n)];
    }
    constexpr NumberType& get(std::ptrdiff_t index) {
        const std::ptrdiff_t n = static_cast<std::ptrdiff_t>(size());
        return (*this)[static_cast<std::size_t>(((index % n) + n) % n)];
    }

    /**
     * @brief Returns the smallest index `i` with `(*this)[i] == value`, or
     * `-1` if no coordinate equals `value`.
     */
    constexpr std::ptrdiff_t index(const NumberType& value) const {
        for (std::ptrdiff_t i = 0; i < static_cast<std::ptrdiff_t>(size()); ++i) {
            if ((*this)[static_cast<std::size_t>(i)] == value) {
                return i;
            }
        }
        return -1;
    }

    /**
     * @brief Returns an iterator to the first coordinate.
     *
     * @return Iterator to the first coordinate.
     */
    constexpr auto begin() {
        return coords_.begin();
    }
    constexpr auto begin() const {
        return coords_.cbegin();
    }

    /**
     * @brief Returns an iterator to the first coordinate.
     *
     * @return Const iterator to the first coordinate.
     */
    constexpr auto cbegin() const {
        return coords_.cbegin();
    }

    /**
     * @brief Returns an iterator past the last coordinate.
     *
     * @return Iterator past the last coordinate.
     */
    constexpr auto end() {
        return coords_.end();
    }
    constexpr auto end() const {
        return coords_.cend();
    }

    /**
     * @brief Returns an iterator past the last coordinate.
     *
     * @return Pointer past the last coordinate.
     */
    constexpr auto cend() const {
        return coords_.cend();
    }

    /**
     * @brief Returns the point mirrored through the origin.
     *
     * @return The negated point.
     */
    [[nodiscard]] constexpr Point operator-() const;

    /**
     * @brief Tests coordinate equality.
     *
     * @param other Point to compare with.
     * @return `true` if both points are equal.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool operator==(const OtherPoint& other) const;

    /**
     * @brief Provides lexicographic ordering on `(x, y)`.
     *
     * @param other Point to compare with.
     * @return -1, 0, or 1.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr std::strong_ordering operator<=>(const OtherPoint& other) const;

    /**
     * @brief Returns the bounding box of the point.
     *
     * @return A degenerate rectangle containing only this point.
     */
    [[nodiscard]] constexpr Rectangle<Point> bbox() const;

    /**
     * @brief Returns a floating-point bounding box containing the point.
     *
     * @tparam ResultNumber Floating-point coordinate type.
     * @return A rectangle whose bounds contain this point.
     */
    template <std::floating_point ResultNumber = double>
    [[nodiscard]] constexpr Rectangle<Point<ResultNumber>> fbox() const;

    /**
     * @brief Returns the unique vertex of the point-shaped object.
     *
     * @return Array containing only this point.
     */
    [[nodiscard]] constexpr std::array<Point, 1> vertices() const;

    /**
     * @brief Returns the point boundary edges.
     *
     * @return Empty array because a point has no edges.
     */
    [[nodiscard]] constexpr std::array<Segment<Point>, 0> edges() const;

    /**
     * @brief Returns the oriented boundary edges.
     *
     * @return Empty array because a point has no edges.
     */
    [[nodiscard]] constexpr std::array<OrientedSegment<Point>, 0> orientedEdges() const;

    /**
     * @brief Returns whether this point contains another point.
     *
     * Point containment is coordinate equality. Labels are ignored.
     *
     * @tparam OtherNumber Coordinate type of the other point.
     * @tparam OtherPoint::LabelType Label type of the other point.
     * @param other Other point.
     * @return `true` if both points have the same coordinates.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool contains(const OtherPoint& other) const;

    template<PointConcept OtherPoint, class OtherLabel>
    [[nodiscard]] constexpr bool contains(const Segment<OtherPoint, OtherLabel>& other) const;

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

    /** @brief Checks if every vertex of the polygon equals this point. */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool contains(const Polygon<OtherPoint>& other) const;

    [[nodiscard]] constexpr bool contains(const Shape<Point<TNumber, TLabel>>& other) const;

    [[nodiscard]] constexpr bool boundaryContains(const Shape<Point<TNumber, TLabel>>& other) const;

    // The empty set is a subset of every shape, so it is contained in all of
    // them. This lets an EmptyShape flow through Shape's variant dispatch
    // without special-casing.
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
     * @brief Returns whether the point boundary contains another point.
     *
     * The documented boundary of a point is empty.
     *
     * @return Always `false`.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool boundaryContains(const OtherPoint& other) const;

    template<PointConcept OtherPoint, class OtherLabel>
    [[nodiscard]] constexpr bool boundaryContains(const Segment<OtherPoint, OtherLabel>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool boundaryContains(const OrientedSegment<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool boundaryContains(const Line<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool boundaryContains(const OrientedLine<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool boundaryContains(const Ray<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool boundaryContains(const Halfplane<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool boundaryContains(const Rectangle<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool boundaryContains(const Triangle<OtherPoint>& other) const;

    /**
     * @brief Returns whether the point interior contains another point.
     *
     * Since the documented point boundary is empty, this matches
     * @ref contains.
     *
     * @tparam OtherNumber Coordinate type of the other point.
     * @tparam OtherPoint::LabelType Label type of the other point.
     * @param other Other point.
     * @return `true` if both points have the same coordinates.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorContains(const OtherPoint& other) const;

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
    [[nodiscard]] constexpr bool interiorContains(const Convex<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorContains(const Polygon<OtherPoint>& other) const;

    template<PointConcept OtherPoint, class OtherLabel>
    [[nodiscard]] constexpr bool interiorContains(const Disk<OtherPoint, OtherLabel>& other) const;

    // --- not-yet-implemented predicate pairs (throw); see implementation ---
    template<PointConcept OtherPoint, class OtherLabel>
    [[nodiscard]] constexpr bool boundaryContains(const Disk<OtherPoint, OtherLabel>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool boundaryContains(const Convex<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool boundaryContains(const Polygon<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool separates(const OtherPoint& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool separates(const Halfplane<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool separates(const Rectangle<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool separates(const Triangle<OtherPoint>& other) const;

    template<PointConcept OtherPoint, class OtherLabel>
    [[nodiscard]] constexpr bool separates(const Disk<OtherPoint, OtherLabel>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool separates(const Convex<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool separates(const Polygon<OtherPoint>& other) const;


    /**
     * @brief Returns whether this point intersects another point.
     *
     * @return `true` if both points have the same coordinates.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool intersects(const OtherPoint& other) const;

    [[nodiscard]] constexpr bool intersects(const Shape<Point<TNumber, TLabel>>& other) const;

    template<typename OtherShape>
        requires (!PointConcept<OtherShape>)
    [[nodiscard]] constexpr bool intersects(const OtherShape& other) const {
        return other.intersects(*this);
    }

    /** @brief A point's interior is the point itself, so this matches @ref interiorContains. */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherPoint& other) const;

    template<typename OtherShape>
        requires (!PointConcept<OtherShape>)
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherShape& other) const {
        return other.interiorsIntersect(*this);
    }

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
    [[nodiscard]] constexpr bool crosses(const OtherPoint& other) const;

    template<typename OtherShape>
        requires (!PointConcept<OtherShape>)
    [[nodiscard]] constexpr bool crosses(const OtherShape& other) const {
        return other.crosses(*this);
    }

    template <class ResultNumber = NumberType, PointConcept OtherPoint>
    [[nodiscard]] constexpr std::optional<Point<ResultNumber, LabelType>>
    intersection(const OtherPoint& other) const;

    template <class ResultNumber = NumberType, typename OtherShape>
        requires (!PointConcept<OtherShape>
                  && (detail::shapeRank<OtherShape> > detail::shapeRank<Point>)
                  && requires(const OtherShape& o, const Point& self) {
                         o.template intersection<ResultNumber>(self);
                     })
    [[nodiscard]] constexpr auto intersection(const OtherShape& other) const {
        return other.template intersection<ResultNumber>(*this);
    }

    /**
     * @brief Returns the squared Euclidean distance to another point.
     *
     * @tparam OtherNumber Coordinate type of the other point.
     * @tparam OtherPoint::LabelType Label type of the other point.
     * @param other Other point.
     * @return Squared Euclidean distance.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr auto squaredDistance(const OtherPoint& other) const;

    /**
     * @brief Returns the Euclidean distance to another point.
     *
     * @tparam ApproximateNumber Floating-point return type.
     * @tparam OtherNumber Coordinate type of the other point.
     * @tparam OtherPoint::LabelType Label type of the other point.
     * @param other Other point.
     * @return Euclidean distance.
     */
    template <class ApproximateNumber = double, class OtherPoint>
    [[nodiscard]] ApproximateNumber distance(const OtherPoint& other) const;

    /**
     * @brief Returns the Manhattan distance to another point.
     *
     * @tparam OtherNumber Coordinate type of the other point.
     * @tparam OtherPoint::LabelType Label type of the other point.
     * @param other Other point.
     * @return Manhattan distance.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr auto distanceL1(const OtherPoint& other) const;

    /**
     * @brief Returns the Chebyshev distance to another point.
     *
     * @tparam OtherNumber Coordinate type of the other point.
     * @tparam OtherPoint::LabelType Label type of the other point.
     * @param other Other point.
     * @return Chebyshev distance.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr auto distanceLInf(const OtherPoint& other) const;

    /**
     * @brief Translates a point by another point in place.
     *
     * @tparam OtherNumber Coordinate type of the other point.
     * @tparam OtherPoint::LabelType Label type of the other point.
     * @param other Other point.
     *
     * @return Translated point.
     */
    template<PointConcept OtherPoint>
    constexpr Point& operator+=(const OtherPoint& other);

    /**
     * @brief Translates a point by the opposite of another point in place.
     *
     * @tparam OtherNumber Coordinate type of the other point.
     * @tparam OtherPoint::LabelType Label type of the other point.
     * @param other Other point.
     *
     * @return Translated point.
     */
    template<PointConcept OtherPoint>
    constexpr Point& operator-=(const OtherPoint& other);

    /**
     * @brief Scales a point by a scalar in place.
     *
     * @tparam OtherNumber Scalar type.
     * @param scalar Scale factor.
     *
     * @return Scaled point.
     */
    template <class OtherNumber>
    constexpr Point& operator*=(const OtherNumber scalar);

    /**
     * @brief Scales a point by a scalar division in place.
     *
     * @tparam OtherNumber Scalar type.
     * @param scalar Scale divider.
     *
     * @return Scaled point.
     */
    template <class OtherNumber>
    constexpr Point& operator/=(const OtherNumber scalar);

    /**
     * @brief Returns the point with x and y swapped.
     *
     * @return Point with x and y swapped.
     */
    constexpr Point swapped() const;

    /**
     * @brief Returns the point rotated by 90k degrees around the origin.
     *
     * @param k Number of 90-degree CCW rotations (may be negative).
     * @return Rotated point.
     */
    [[nodiscard]] constexpr Point rotated90(int k = 1) const;

    /**
     * @brief Rotates the point by 90k degrees around the origin in place.
     *
     * @param k Number of 90-degree CCW rotations (may be negative).
     */
    constexpr void rotate90(int k = 1);

    /**
     * @brief Returns the point with its x-coordinate multiplied by a scalar.
     *
     * @tparam OtherNumber Scalar type.
     * @param scalar Scale factor.
     * @return Point with scaled x-coordinate.
     */
    template <class OtherNumber>
    [[nodiscard]] constexpr Point scaledUpX(const OtherNumber scalar) const;

    /**
     * @brief Multiplies the point's x-coordinate by a scalar in place.
     *
     * @tparam OtherNumber Scalar type.
     * @param scalar Scale factor.
     */
    template <class OtherNumber>
    constexpr void scaleUpX(const OtherNumber scalar);

    /**
     * @brief Returns the point with its y-coordinate multiplied by a scalar.
     *
     * @tparam OtherNumber Scalar type.
     * @param scalar Scale factor.
     * @return Point with scaled y-coordinate.
     */
    template <class OtherNumber>
    [[nodiscard]] constexpr Point scaledUpY(const OtherNumber scalar) const;

    /**
     * @brief Multiplies the point's y-coordinate by a scalar in place.
     *
     * @tparam OtherNumber Scalar type.
     * @param scalar Scale factor.
     */
    template <class OtherNumber>
    constexpr void scaleUpY(const OtherNumber scalar);

    /**
     * @brief Returns the point with its x-coordinate divided by a scalar.
     *
     * @tparam OtherNumber Scalar type.
     * @param scalar Scale divider.
     * @return Point with scaled x-coordinate.
     */
    template <class OtherNumber>
    [[nodiscard]] constexpr Point scaledDownX(const OtherNumber scalar) const;

    /**
     * @brief Divides the point's x-coordinate by a scalar in place.
     *
     * @tparam OtherNumber Scalar type.
     * @param scalar Scale divider.
     */
    template <class OtherNumber>
    constexpr void scaleDownX(const OtherNumber scalar);

    /**
     * @brief Returns the point with its y-coordinate divided by a scalar.
     *
     * @tparam OtherNumber Scalar type.
     * @param scalar Scale divider.
     * @return Point with scaled y-coordinate.
     */
    template <class OtherNumber>
    [[nodiscard]] constexpr Point scaledDownY(const OtherNumber scalar) const;

    /**
     * @brief Divides the point's y-coordinate by a scalar in place.
     *
     * @tparam OtherNumber Scalar type.
     * @param scalar Scale divider.
     */
    template <class OtherNumber>
    constexpr void scaleDownY(const OtherNumber scalar);

    /**
     * @brief Returns the dual Line.
     *
     * @tparam ResultNumber Coordinate type of the returned line.
     * @return Line y = ax - b for point (a,b).
     */
    template<class ResultNumber=NumberType>
    constexpr Line<Point<ResultNumber, LabelType>> dual() const;

    /**
     * @brief Returns the polar Line.
     *
     * @tparam ResultNumber Coordinate type of the returned line.
     * @return Line ax + by = 1 for point (a,b).
     * @warning Uses division.
     */
    template<class ResultNumber=NumberType>
    constexpr Line<Point<ResultNumber, LabelType>> polar() const;

  private:
    std::array<NumberType,2> coords_{};
    [[no_unique_address]] LabelType label_{};
}; // struct Point

/**
 * @brief Translates a point by another point.
 *
 * @tparam LeftNumber Coordinate type of the left operand.
 * @tparam LeftLabel Label type of the left operand.
 * @tparam RightNumber Coordinate type of the right operand.
 * @tparam RightLabel Label type of the right operand.
 * @param left Point to translate.
 * @param right Translation vector.
 * @return Translated point.
 */
template <class LeftNumber, class LeftLabel, class RightNumber, class RightLabel>
[[nodiscard]] constexpr auto operator+(const Point<LeftNumber, LeftLabel>& left, const Point<RightNumber, RightLabel>& right);

/**
 * @brief Translates a point by the opposite of another point.
 *
 * @tparam LeftNumber Coordinate type of the left operand.
 * @tparam LeftLabel Label type of the left operand.
 * @tparam RightNumber Coordinate type of the right operand.
 * @tparam RightLabel Label type of the right operand.
 * @param left Point to translate.
 * @param right Vector to subtract.
 * @return Translated point.
 */
template <class LeftNumber, class LeftLabel, class RightNumber, class RightLabel>
[[nodiscard]] constexpr auto operator-(const Point<LeftNumber, LeftLabel>& left, const Point<RightNumber, RightLabel>& right);

/**
 * @brief Scales a point by a scalar.
 *
 * @tparam Number Coordinate type of the point.
 * @tparam Label Label type of the point.
 * @tparam Scalar Scalar type.
 * @param point Point to scale.
 * @param scalar Scale factor.
 * @return Scaled point.
 */
template <class Number, class Label, class Scalar>
    requires(!detail::is_point_v<Scalar>)
[[nodiscard]] constexpr auto operator*(const Point<Number, Label>& point, const Scalar& scalar);

/**
 * @brief Scales a point by a scalar written on the left.
 *
 * @tparam Scalar Scalar type.
 * @tparam Number Coordinate type of the point.
 * @tparam Label Label type of the point.
 * @param scalar Scale factor.
 * @param point Point to scale.
 * @return Scaled point.
 */
template <class Scalar, class Number, class Label>
    requires(!detail::is_point_v<Scalar>)
[[nodiscard]] constexpr auto operator*(const Scalar& scalar, const Point<Number, Label>& point);

/**
 * @brief Returns the dot product of two points.
 *
 * @tparam LeftNumber Coordinate type of the left operand.
 * @tparam LeftLabel Label type of the left operand.
 * @tparam RightNumber Coordinate type of the right operand.
 * @tparam RightLabel Label type of the right operand.
 * @param left Left point.
 * @param right Right point.
 * @return Dot product.
 */
template <class LeftNumber, class LeftLabel, class RightNumber, class RightLabel>
[[nodiscard]] constexpr auto operator*(const Point<LeftNumber, LeftLabel>& left, const Point<RightNumber, RightLabel>& right);

/**
 * @brief Divides both coordinates by a scalar.
 *
 * @tparam Number Coordinate type of the point.
 * @tparam Label Label type of the point.
 * @tparam Scalar Scalar type.
 * @param point Point to divide.
 * @param scalar Divisor.
 * @return Scaled point.
 */
template <class Number, class Label, class Scalar>
    requires(!detail::is_point_v<Scalar>)
[[nodiscard]] constexpr auto operator/(const Point<Number, Label>& point, const Scalar& scalar);

/**
 * @brief Streams a point as `(x,y)` or `label:(x,y)`.
 *
 * @tparam Number Coordinate type of the point.
 * @tparam Label Label type of the point.
 * @param stream Output stream.
 * @param point Point to print.
 * @return The output stream.
 */
template <class Number, class Label>
std::ostream& operator<<(std::ostream& stream, const Point<Number, Label>& point);

/**
 * @brief An open polyline: an ordered sequence of points.
 *
 * @note Stub: this is a placeholder alias for a `Polyline` class that has not
 * been implemented yet. For now it is simply a `std::vector` of points, with no
 * normalization or closure invariant. It is expected to be replaced by a proper
 * shape type (with its own predicates and constructions) in the future.
 *
 * @tparam PointType The vertex point type.
 */
template <class PointType = Point<>>
using Polyline = std::vector<PointType>;

}//pgl
