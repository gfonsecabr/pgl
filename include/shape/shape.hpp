#pragma once

#include "shape/polygon.hpp"

/**
 * @file shape.hpp
 * @brief Runtime variant wrapper over the currently implemented shape types.
 *
 * Most of Pangolin is static and template-based; Shape exists for places where
 * runtime dispatch over heterogeneous shapes is more convenient.
 */

#include <compare>
#include <concepts>
#include <functional>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>


namespace pgl {

namespace detail {

template <class PointType, class T>
struct is_shape_alternative : std::false_type {};

template <class PointType>
struct is_shape_alternative<PointType, EmptyShape<PointType>> : std::true_type {};

template <class PointType, class Number, class Label>
struct is_shape_alternative<PointType, Point<Number, Label>> : std::bool_constant<std::same_as<PointType, Point<Number, Label>>> {};

template <class PointType, class Label>
struct is_shape_alternative<PointType, Segment<PointType, Label>> : std::true_type {};

template <class PointType>
struct is_shape_alternative<PointType, OrientedSegment<PointType>> : std::true_type {};

template <class PointType>
struct is_shape_alternative<PointType, Line<PointType>> : std::true_type {};

template <class PointType>
struct is_shape_alternative<PointType, OrientedLine<PointType>> : std::true_type {};

template <class PointType>
struct is_shape_alternative<PointType, Ray<PointType>> : std::true_type {};

template <class PointType>
struct is_shape_alternative<PointType, Halfplane<PointType>> : std::true_type {};

template <class PointType>
struct is_shape_alternative<PointType, Rectangle<PointType>> : std::true_type {};

template <class PointType>
struct is_shape_alternative<PointType, Triangle<PointType>> : std::true_type {};

template <class PointType>
struct is_shape_alternative<PointType, Convex<PointType>> : std::true_type {};

template <class PointType, class Label>
struct is_shape_alternative<PointType, Disk<PointType, Label>> : std::true_type {};

template <class PointType>
struct is_shape_alternative<PointType, Polygon<PointType>> : std::true_type {};

template <class PointType, class T>
inline constexpr bool is_shape_alternative_v = is_shape_alternative<PointType, std::remove_cvref_t<T>>::value;

template <class PointType, class T>
concept ShapeAlternative = is_shape_alternative_v<PointType, T>;

// Minimal shape detectors for the standard wrappers an intersection may return.
template <class T>
struct is_std_optional : std::false_type {};
template <class T>
struct is_std_optional<std::optional<T>> : std::true_type {};

template <class T>
struct is_std_vector : std::false_type {};
template <class T, class A>
struct is_std_vector<std::vector<T, A>> : std::true_type {};

template <class T>
struct is_std_variant : std::false_type {};
template <class... Ts>
struct is_std_variant<std::variant<Ts...>> : std::true_type {};

// True iff T is a std::variant whose every alternative is a supported shape for
// PointType. Gates the variant-unwrapping Shape constructor so a variant that
// could hold a non-shape is rejected at compile time.
template <class PointType, class T>
struct is_shape_variant : std::false_type {};

template <class PointType, class... Ts>
struct is_shape_variant<PointType, std::variant<Ts...>>
    : std::bool_constant<(is_shape_alternative_v<PointType, Ts> && ...)> {};

template <class PointType, class T>
inline constexpr bool is_shape_variant_v = is_shape_variant<PointType, std::remove_cvref_t<T>>::value;

// True iff T is a std::optional wrapping such a shape variant.
template <class PointType, class T>
struct is_shape_optional_variant : std::false_type {};

template <class PointType, class V>
struct is_shape_optional_variant<PointType, std::optional<V>>
    : std::bool_constant<is_shape_variant_v<PointType, V>> {};

template <class PointType, class T>
inline constexpr bool is_shape_optional_variant_v =
    is_shape_optional_variant<PointType, std::remove_cvref_t<T>>::value;

// Point type carried by a shape alternative: a Point is its own point type;
// every other alternative exposes it as a nested PointType. Used by the Shape
// deduction guides to recover the wrapper's point type from a result variant.
template <class T>
struct shape_point_type {
    using type = typename T::PointType;
};

template <class Number, class Label>
struct shape_point_type<Point<Number, Label>> {
    using type = Point<Number, Label>;
};

template <class T>
using shape_point_type_t = typename shape_point_type<T>::type;

}  // namespace detail

/**
 * @brief Type-erased wrapper over the finite set of supported primitive shapes.
 *
 * `Shape<PointType>` stores one geometry in a variant and forwards common
 * predicates through visitation.
 *
 * @tparam PointType Point type shared by every stored alternative.
 */
template <class PointType = Point<>>
struct Shape {
    /** Point type shared by all alternatives. */
    using PointType_ = PointType;
    /** Coordinate type of the stored point type. */
    using NumberType = PointType::NumberType;
    /** Label type of the stored point type. */
    using LabelType = PointType::LabelType;
    /** Variant type used for storage and visitation. The leading
     * `EmptyShape` is the empty state of a default-constructed `Shape`. */
    using Variant = std::variant<
        EmptyShape<PointType>,
        PointType,
        Segment<PointType>,
        OrientedSegment<PointType>,
        Line<PointType>,
        OrientedLine<PointType>,
        Ray<PointType>,
        Halfplane<PointType>,
        Rectangle<PointType>,
        Triangle<PointType>,
        Disk<PointType>,
        Convex<PointType>,
        Polygon<PointType>>;

    /**
     * @brief Creates a point-valued default shape.
     */
    constexpr Shape() = default;

    /**
     * @brief Constructs a shape from one supported alternative.
     *
     * @tparam T Alternative type.
     * @param value Shape value to store.
     */
    template <class T>
        requires(detail::ShapeAlternative<PointType, T>)
    constexpr Shape(T&& value)
        : value_(std::forward<T>(value)) {}

    /**
     * @brief Constructs a shape from a variant over supported alternatives.
     *
     * Accepts a `std::variant` whose alternatives are all supported shape types,
     * or such a variant wrapped in a `std::optional`. The active alternative is
     * forwarded to the single-alternative constructor; a valueless `std::optional`
     * yields the empty shape. The variant's alternatives are checked at compile
     * time, so a variant that could hold a non-shape is rejected.
     *
     * @tparam Result `std::variant` of alternatives, or `std::optional` thereof.
     * @param result Variant (optionally absent) to unwrap.
     */
    template <class Result>
        requires(detail::is_shape_variant_v<PointType, Result> ||
                 detail::is_shape_optional_variant_v<PointType, Result>)
    constexpr Shape(const Result& result) {
        if constexpr (detail::is_shape_optional_variant_v<PointType, Result>) {
            if (result) {
                *this = Shape(*result);
            }
        } else {
            std::visit(
                [this](const auto& alternative) { *this = Shape(alternative); }, result);
        }
    }

    /**
     * @brief Replaces the stored alternative.
     *
     * @tparam T Alternative type.
     * @param value New shape value.
     * @return This wrapper.
     */
    template <class T>
        requires(detail::ShapeAlternative<PointType, T>)
    constexpr Shape& operator=(T&& value) {
        value_ = std::forward<T>(value);
        return *this;
    }

    /**
     * @brief Compares wrapped values.
     */
    constexpr bool operator==(const Shape&) const = default;

    /**
     * @brief Orders wrapped values by the underlying variant ordering.
     */
    constexpr auto operator<=>(const Shape&) const = default;

    /**
     * @brief Returns the underlying variant.
     *
     * @return Const reference to the stored variant.
     */
    constexpr const Variant& variant() const {
        return value_;
    }

    /**
     * @brief Returns the underlying variant.
     *
     * @return Mutable reference to the stored variant.
     */
    constexpr Variant& variant() {
        return value_;
    }

    /**
     * @brief Tests whether the wrapper holds the empty shape.
     *
     * @return `true` for a default-constructed (empty) shape.
     */
    [[nodiscard]] constexpr bool empty() const {
        return std::holds_alternative<EmptyShape<PointType>>(value_);
    }

    /**
     * @brief Tests whether the wrapped shape is degenerate.
     *
     * @return Result of dispatching `isDegenerate` when available.
     */
    [[nodiscard]] constexpr bool isDegenerate() const {
        return std::visit(
            [](const auto& value) {
                if constexpr (requires { value.isDegenerate(); }) {
                    return value.isDegenerate();
                } else {
                    return false;
                }
            },
            value_);
    }

    /**
     * @brief Returns the wrapped shape's axis-aligned bounding box.
     *
     * Dispatches to the alternative's `bbox()`.
     *
     * @throws std::logic_error if the wrapped alternative is unbounded and
     * therefore has no `bbox()` — the `EmptyShape`, `Line`, `OrientedLine`,
     * `Ray`, and `Halfplane` alternatives.
     */
    [[nodiscard]] constexpr Rectangle<PointType> bbox() const {
        return std::visit(
            [](const auto& value) -> Rectangle<PointType> {
                if constexpr (requires { value.bbox(); }) {
                    return value.bbox();
                } else {
                    throw std::logic_error("Shape::bbox is not defined for this unbounded alternative");
                }
            },
            value_);
    }

    /**
     * @brief Returns the number of indexable elements of the wrapped shape.
     *
     * Dispatches to the alternative's `size()` so the result matches the
     * valid range of its `operator[]`.
     */
    [[nodiscard]] constexpr std::size_t size() const {
        return std::visit(
            [](const auto& value) -> std::size_t {
                return value.size();
            },
            value_);
    }

    /**
     * @brief Returns the i-th vertex (modulo @ref size()) of the wrapped shape.
     *
     * Dispatches via `std::visit` to the alternative's `get(index)`. Only
     * defined for alternatives whose `get` yields a `PointType_` — i.e.,
     * every alternative except `Point` itself, whose `get` yields a
     * coordinate. Throws `std::logic_error` if the wrapped value is a
     * `Point`.
     */
    [[nodiscard]] constexpr PointType_ get(std::ptrdiff_t index) const {
        return std::visit(
            [index](const auto& value) -> PointType_ {
                using S = std::decay_t<decltype(value)>;
                if constexpr (std::same_as<S, PointType_>) {
                    throw std::logic_error("Shape::get is not defined for the Point alternative");
                } else {
                    return value.get(index);
                }
            },
            value_);
    }

    /**
     * @brief Returns the vertex at `index` of the wrapped shape.
     *
     * Dispatches via `std::visit` to the alternative's `operator[](index)`.
     * Only defined for alternatives whose `operator[]` yields a
     * `PointType_` — i.e., every alternative except `Point` itself, whose
     * `operator[]` yields a coordinate. Throws `std::logic_error` if the
     * wrapped value is a `Point`.
     */
    [[nodiscard]] constexpr PointType_ operator[](std::size_t index) const {
        return std::visit(
            [index](const auto& value) -> PointType_ {
                using S = std::decay_t<decltype(value)>;
                if constexpr (std::same_as<S, PointType_>) {
                    throw std::logic_error("Shape::operator[] is not defined for the Point alternative");
                } else {
                    return value[index];
                }
            },
            value_);
    }

    /**
     * @brief Returns the smallest index `i` with `(*this)[i] == point`, or
     * `-1` if no vertex of the wrapped shape equals `point`.
     *
     * Dispatches via `std::visit` to the alternative's `index(point)`. The
     * argument type selects this overload for every alternative except `Point`
     * — whose `operator[]` yields a coordinate, handled by the
     * @ref index(const NumberType&) overload. Throws `std::logic_error` if the
     * wrapped value is a `Point`.
     */
    [[nodiscard]] constexpr std::ptrdiff_t index(const PointType_& point) const {
        return std::visit(
            [&point](const auto& value) -> std::ptrdiff_t {
                using S = std::decay_t<decltype(value)>;
                if constexpr (std::same_as<S, PointType_>) {
                    throw std::logic_error("Shape::index(Point) is not defined for the Point alternative");
                } else {
                    return value.index(point);
                }
            },
            value_);
    }

    /**
     * @brief Returns the smallest index `i` with `(*this)[i] == value`, or
     * `-1` if no coordinate equals `value`.
     *
     * Dispatches via `std::visit` to `Point::index(value)`. The argument type
     * selects this overload only for the `Point` alternative, whose
     * `operator[]` yields a coordinate; every other alternative indexes points
     * via the @ref index(const PointType_&) overload. Throws
     * `std::logic_error` if the wrapped value is not a `Point`.
     */
    [[nodiscard]] constexpr std::ptrdiff_t index(const NumberType& value) const {
        return std::visit(
            [&value](const auto& shape) -> std::ptrdiff_t {
                using S = std::decay_t<decltype(shape)>;
                if constexpr (std::same_as<S, PointType_>) {
                    return shape.index(value);
                } else {
                    throw std::logic_error("Shape::index(NumberType) is only defined for the Point alternative");
                }
            },
            value_);
    }

    /**
     * @brief Tests whether the wrapper currently stores a given alternative.
     *
     * @tparam T Alternative type.
     * @return `true` if the stored value has that type.
     */
    template <class T>
        requires(detail::ShapeAlternative<PointType, T>)
    constexpr bool holdsAlternative() const {
        return std::holds_alternative<std::remove_cvref_t<T>>(value_);
    }

    /**
     * @brief Returns a pointer to the stored alternative when it matches `T`.
     *
     * @tparam T Alternative type.
     * @return Pointer to the stored value or `nullptr`.
     */
    template <class T>
        requires(detail::ShapeAlternative<PointType, T>)
    constexpr const std::remove_cvref_t<T>* getIf() const {
        return std::get_if<std::remove_cvref_t<T>>(&value_);
    }

    /**
     * @brief Returns a mutable pointer to the stored alternative when it matches `T`.
     *
     * @tparam T Alternative type.
     * @return Pointer to the stored value or `nullptr`.
     */
    template <class T>
        requires(detail::ShapeAlternative<PointType, T>)
    constexpr std::remove_cvref_t<T>* getIf() {
        return std::get_if<std::remove_cvref_t<T>>(&value_);
    }

    /**
     * @brief Converts to the currently stored alternative.
     *
     * Lets an unwrapped alternative be recovered directly, e.g.
     * `Point cross = shape;` when @p shape holds a `Point`.
     *
     * @tparam T Alternative type to extract.
     * @return A copy of the stored value.
     * @throws std::bad_variant_access if the wrapper holds a different alternative.
     */
    template <class T>
        requires(detail::ShapeAlternative<PointType, T>)
    constexpr explicit operator T() const {
        return std::get<std::remove_cvref_t<T>>(value_);
    }

    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     *
     * @tparam Other `Shape` or a supported alternative type.
     * @param other Operand to test.
     * @return Result of dispatching `contains`.
     */
    template <class Other>
        requires(std::same_as<std::remove_cvref_t<Other>, Shape> || detail::ShapeAlternative<PointType, Other>)
    constexpr bool contains(const Other& other) const {
        return applyPredicate(
            [](const auto& left, const auto& right) {
                if constexpr (requires { left.contains(right); }) {
                    return left.contains(right);
                } else {
                    return false;
                }
            },
            other);
    }

    /**
     * @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B).
     *
     * @tparam Other `Shape` or a supported alternative type.
     * @param other Operand to test.
     * @return Result of dispatching `boundaryContains`.
     */
    template <class Other>
        requires(std::same_as<std::remove_cvref_t<Other>, Shape> || detail::ShapeAlternative<PointType, Other>)
    constexpr bool boundaryContains(const Other& other) const {
        return applyPredicate(
            [](const auto& left, const auto& right) {
                if constexpr (requires { left.boundaryContains(right); }) {
                    return left.boundaryContains(right);
                } else {
                    return false;
                }
            },
            other);
    }

    /**
     * @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B).
     *
     * @tparam Other `Shape` or a supported alternative type.
     * @param other Operand to test.
     * @return Result of dispatching `interiorContains`.
     */
    template <class Other>
        requires(std::same_as<std::remove_cvref_t<Other>, Shape> || detail::ShapeAlternative<PointType, Other>)
    constexpr bool interiorContains(const Other& other) const {
        return applyPredicate(
            [](const auto& left, const auto& right) {
                if constexpr (requires { left.interiorContains(right); }) {
                    return left.interiorContains(right);
                } else {
                    return false;
                }
            },
            other);
    }

    /**
     * @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅).
     *
     * @tparam Other `Shape` or a supported alternative type.
     * @param other Operand to test.
     * @return Result of dispatching `intersects`.
     */
    template <class Other>
        requires(std::same_as<std::remove_cvref_t<Other>, Shape> || detail::ShapeAlternative<PointType, Other>)
    constexpr bool intersects(const Other& other) const {
        return applyPredicate(
            [](const auto& left, const auto& right) {
                if constexpr (requires { left.intersects(right); }) {
                    return left.intersects(right);
                } else {
                    return false;
                }
            },
            other);
    }

    /**
     * @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅).
     *
     * @tparam Other `Shape` or a supported alternative type.
     * @param other Operand to test.
     * @return Result of dispatching `interiorsIntersect`.
     */
    template <class Other>
        requires(std::same_as<std::remove_cvref_t<Other>, Shape> || detail::ShapeAlternative<PointType, Other>)
    constexpr bool interiorsIntersect(const Other& other) const {
        return applyPredicate(
            [](const auto& left, const auto& right) {
                if constexpr (requires { left.interiorsIntersect(right); }) {
                    return left.interiorsIntersect(right);
                } else {
                    return false;
                }
            },
            other);
    }

    /**
     * @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected).
     *
     * @tparam Other `Shape` or a supported alternative type.
     * @param other Operand to test.
     * @return Result of dispatching `separates` when available.
     */
    template <class Other>
        requires(std::same_as<std::remove_cvref_t<Other>, Shape> || detail::ShapeAlternative<PointType, Other>)
    constexpr bool separates(const Other& other) const {
        return applyPredicate(
            [](const auto& left, const auto& right) {
                if constexpr (requires { left.separates(right); }) {
                    return left.separates(right);
                } else {
                    return false;
                }
            },
            other);
    }

    /**
     * @brief Tests whether the two shapes mutually separate each other (each disconnects the other).
     *
     * @tparam Other `Shape` or a supported alternative type.
     * @param other Operand to test.
     * @return Result of dispatching `crosses` when available.
     */
    template <class Other>
        requires(std::same_as<std::remove_cvref_t<Other>, Shape> || detail::ShapeAlternative<PointType, Other>)
    constexpr bool crosses(const Other& other) const {
        return applyPredicate(
            [](const auto& left, const auto& right) {
                if constexpr (requires { left.crosses(right); }) {
                    return left.crosses(right);
                } else {
                    return false;
                }
            },
            other);
    }

    /**
     * @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint.
     *
     * Visits the stored alternative (and @p other when it is itself a `Shape`),
     * delegates to the concrete `intersection` requesting @p ResultNumber
     * coordinates, and re-wraps the result. An empty intersection becomes an
     * `EmptyShape`. The returned wrapper is parameterized on the result point
     * type, which may differ from this wrapper's.
     *
     * @tparam ResultNumber Coordinate type of the result (defaults to this
     *   wrapper's `NumberType`).
     * @tparam Other `Shape` or a supported alternative type.
     * @param other Shape to intersect with.
     * @return The intersection wrapped in a `Shape<Point<ResultNumber, LabelType>>`.
     * @throws std::logic_error when the result cannot be represented by a single
     *   `Shape` — i.e. a disconnected (multi-component) intersection, or a pair
     *   whose intersection is unsupported (anything against a `Disk`).
     * @warning Divides coordinates after casting to ResultNumber.
     */
    template <class ResultNumber = NumberType, class Other>
        requires(std::same_as<std::remove_cvref_t<Other>, Shape> || detail::ShapeAlternative<PointType, Other>)
    constexpr Shape<Point<ResultNumber, LabelType>> intersection(const Other& other) const {
        if constexpr (detail::is_shape_v<Other>) {
            return std::visit(
                [](const auto& left, const auto& right) {
                    return intersectionOf<ResultNumber>(left, right);
                },
                value_,
                other.variant());
        } else {
            return std::visit(
                [&other](const auto& left) {
                    return intersectionOf<ResultNumber>(left, other);
                },
                value_);
        }
    }

    /**
     * @brief Returns the squared Euclidean distance to the given shape.
     *
     * Visits the stored alternative (and @p other when it is itself a `Shape`)
     * and delegates to the concrete `squaredDistance` requesting @p ResultNumber
     * coordinates.
     *
     * @tparam ResultNumber Coordinate type of the result (defaults to this
     *   wrapper's `NumberType`).
     * @tparam Other `Shape` or a supported alternative type.
     * @param other Shape to measure the distance to.
     * @return The squared Euclidean distance as @p ResultNumber.
     * @throws std::logic_error when `squaredDistance` is undefined for the pair
     *   selected at runtime — anything involving an `EmptyShape`.
     *
     * @warning `Disk::squaredDistance` (and the `Disk` overloads on `Convex` and
     *   `Polygon`) always compute in `double` rather than being templated on
     *   @p ResultNumber; for a pair involving a `Disk`, the `double` result is
     *   `static_cast` to @p ResultNumber rather than computed exactly.
     *
     * @warning With an integer @p ResultNumber the exact squared distance is
     *   generally a fraction, so the internal division truncates and the result
     *   is inexact. Request a floating-point or pgl::Rational result type for an
     *   accurate value.
     */
    template <class ResultNumber = NumberType, class Other>
        requires(std::same_as<std::remove_cvref_t<Other>, Shape> || detail::ShapeAlternative<PointType, Other>)
    constexpr ResultNumber squaredDistance(const Other& other) const {
        if constexpr (detail::is_shape_v<Other>) {
            return std::visit(
                [](const auto& left, const auto& right) {
                    return squaredDistanceOf<ResultNumber>(left, right);
                },
                value_,
                other.variant());
        } else {
            return std::visit(
                [&other](const auto& left) {
                    return squaredDistanceOf<ResultNumber>(left, other);
                },
                value_);
        }
    }

    /**
     * @brief Translates the stored shape in place.
     *
     * Visits the active alternative and translates it by @p translation; the
     * empty shape is left unchanged. The stored alternative type is preserved.
     *
     * @tparam OtherPoint Translation point type.
     * @param translation Translation vector.
     * @return This wrapper.
     */
    template <PointConcept OtherPoint>
    constexpr Shape& operator+=(const OtherPoint& translation) {
        std::visit([&translation](auto& alternative) { alternative += translation; }, value_);
        return *this;
    }

    /**
     * @brief Translates the stored shape in place by a negated point.
     *
     * Visits the active alternative and translates it by @p translation; the
     * empty shape is left unchanged. The stored alternative type is preserved.
     *
     * @tparam OtherPoint Translation point type.
     * @param translation Translation vector.
     * @return This wrapper.
     */
    template <PointConcept OtherPoint>
    constexpr Shape& operator-=(const OtherPoint& translation) {
        std::visit([&translation](auto& alternative) { alternative -= translation; }, value_);
        return *this;
    }

    /**
     * @brief Scales the stored shape in place around the origin.
     *
     * Visits the active alternative and scales it by @p scalar; the empty shape
     * is left unchanged. The stored alternative type is preserved.
     *
     * @tparam Scalar Scaling factor type.
     * @param scalar Scaling factor.
     * @return This wrapper.
     */
    template <class Scalar>
        requires(!detail::is_point_v<Scalar>)
    constexpr Shape& operator*=(const Scalar& scalar) {
        std::visit([&scalar](auto& alternative) { alternative *= scalar; }, value_);
        return *this;
    }

    /**
     * @brief Divides the stored shape in place around the origin.
     *
     * Visits the active alternative and divides it by @p scalar; the empty shape
     * is left unchanged. The stored alternative type is preserved.
     *
     * @tparam Scalar Scaling factor type.
     * @param scalar Scaling factor.
     * @return This wrapper.
     */
    template <class Scalar>
        requires(!detail::is_point_v<Scalar>)
    constexpr Shape& operator/=(const Scalar& scalar) {
        std::visit([&scalar](auto& alternative) { alternative /= scalar; }, value_);
        return *this;
    }

    /**
     * @brief Returns the wrapped shape rotated by 90k degrees around the origin.
     *
     * @param k Number of 90-degree CCW rotations (may be negative).
     * @return Rotated shape, preserving the stored alternative type.
     */
    [[nodiscard]] constexpr Shape rotated90(int k = 1) const {
        return std::visit(
            [k](const auto& value) -> Shape { return Shape(value.rotated90(k)); },
            value_);
    }

    /**
     * @brief Rotates the wrapped shape by 90k degrees around the origin in place.
     *
     * @param k Number of 90-degree CCW rotations (may be negative).
     */
    constexpr void rotate90(int k = 1) {
        std::visit([k](auto& value) { value.rotate90(k); }, value_);
    }

    /**
     * @brief Returns the wrapped shape with its x-coordinates scaled up.
     *
     * @throws std::logic_error if the wrapped alternative cannot be scaled along
     * a single axis (the `Disk` alternative, whose result would be an ellipse).
     */
    template <class OtherNumber>
    [[nodiscard]] constexpr Shape scaledUpX(const OtherNumber scalar) const {
        return std::visit(
            [scalar](const auto& value) -> Shape {
                if constexpr (requires { value.scaledUpX(scalar); }) {
                    return Shape(value.scaledUpX(scalar));
                } else {
                    throw std::logic_error("Shape::scaledUpX is not defined for the Disk alternative");
                }
            },
            value_);
    }

    /**
     * @brief Scales the wrapped shape's x-coordinates up in place.
     *
     * @throws std::logic_error if the wrapped alternative cannot be scaled along
     * a single axis (the `Disk` alternative).
     */
    template <class OtherNumber>
    constexpr void scaleUpX(const OtherNumber scalar) {
        std::visit(
            [scalar](auto& value) {
                if constexpr (requires { value.scaleUpX(scalar); }) {
                    value.scaleUpX(scalar);
                } else {
                    throw std::logic_error("Shape::scaleUpX is not defined for the Disk alternative");
                }
            },
            value_);
    }

    /**
     * @brief Returns the wrapped shape with its y-coordinates scaled up.
     *
     * @throws std::logic_error if the wrapped alternative cannot be scaled along
     * a single axis (the `Disk` alternative, whose result would be an ellipse).
     */
    template <class OtherNumber>
    [[nodiscard]] constexpr Shape scaledUpY(const OtherNumber scalar) const {
        return std::visit(
            [scalar](const auto& value) -> Shape {
                if constexpr (requires { value.scaledUpY(scalar); }) {
                    return Shape(value.scaledUpY(scalar));
                } else {
                    throw std::logic_error("Shape::scaledUpY is not defined for the Disk alternative");
                }
            },
            value_);
    }

    /**
     * @brief Scales the wrapped shape's y-coordinates up in place.
     *
     * @throws std::logic_error if the wrapped alternative cannot be scaled along
     * a single axis (the `Disk` alternative).
     */
    template <class OtherNumber>
    constexpr void scaleUpY(const OtherNumber scalar) {
        std::visit(
            [scalar](auto& value) {
                if constexpr (requires { value.scaleUpY(scalar); }) {
                    value.scaleUpY(scalar);
                } else {
                    throw std::logic_error("Shape::scaleUpY is not defined for the Disk alternative");
                }
            },
            value_);
    }

    /**
     * @brief Returns the wrapped shape with its x-coordinates scaled down.
     *
     * @throws std::logic_error if the wrapped alternative cannot be scaled along
     * a single axis (the `Disk` alternative, whose result would be an ellipse).
     */
    template <class OtherNumber>
    [[nodiscard]] constexpr Shape scaledDownX(const OtherNumber scalar) const {
        return std::visit(
            [scalar](const auto& value) -> Shape {
                if constexpr (requires { value.scaledDownX(scalar); }) {
                    return Shape(value.scaledDownX(scalar));
                } else {
                    throw std::logic_error("Shape::scaledDownX is not defined for the Disk alternative");
                }
            },
            value_);
    }

    /**
     * @brief Scales the wrapped shape's x-coordinates down in place.
     *
     * @throws std::logic_error if the wrapped alternative cannot be scaled along
     * a single axis (the `Disk` alternative).
     */
    template <class OtherNumber>
    constexpr void scaleDownX(const OtherNumber scalar) {
        std::visit(
            [scalar](auto& value) {
                if constexpr (requires { value.scaleDownX(scalar); }) {
                    value.scaleDownX(scalar);
                } else {
                    throw std::logic_error("Shape::scaleDownX is not defined for the Disk alternative");
                }
            },
            value_);
    }

    /**
     * @brief Returns the wrapped shape with its y-coordinates scaled down.
     *
     * @throws std::logic_error if the wrapped alternative cannot be scaled along
     * a single axis (the `Disk` alternative, whose result would be an ellipse).
     */
    template <class OtherNumber>
    [[nodiscard]] constexpr Shape scaledDownY(const OtherNumber scalar) const {
        return std::visit(
            [scalar](const auto& value) -> Shape {
                if constexpr (requires { value.scaledDownY(scalar); }) {
                    return Shape(value.scaledDownY(scalar));
                } else {
                    throw std::logic_error("Shape::scaledDownY is not defined for the Disk alternative");
                }
            },
            value_);
    }

    /**
     * @brief Scales the wrapped shape's y-coordinates down in place.
     *
     * @throws std::logic_error if the wrapped alternative cannot be scaled along
     * a single axis (the `Disk` alternative).
     */
    template <class OtherNumber>
    constexpr void scaleDownY(const OtherNumber scalar) {
        std::visit(
            [scalar](auto& value) {
                if constexpr (requires { value.scaleDownY(scalar); }) {
                    value.scaleDownY(scalar);
                } else {
                    throw std::logic_error("Shape::scaleDownY is not defined for the Disk alternative");
                }
            },
            value_);
    }

  private:
    /**
     * @brief Dispatches a binary predicate against a shape or concrete alternative.
     *
     * Unwraps the stored value (and @p other when it is itself a `Shape`) and
     * forwards the operands to @p dispatch. This is the shared plumbing behind
     * every public predicate, so the `Shape` and alternative overloads need not
     * be written twice.
     *
     * @tparam Dispatch Callable taking the two unwrapped operands.
     * @tparam Other `Shape` or a supported alternative type.
     */
    template <class Dispatch, class Other>
    constexpr bool applyPredicate(Dispatch dispatch, const Other& other) const {
        if constexpr (detail::is_shape_v<Other>) {
            return std::visit(
                [&dispatch](const auto& left, const auto& right) {
                    return dispatch(left, right);
                },
                value_,
                other.variant());
        } else {
            return std::visit(
                [&dispatch, &other](const auto& self) {
                    return dispatch(self, other);
                },
                value_);
        }
    }

    // Intersect two unwrapped alternatives and wrap the result as a Shape over
    // the result point type. The empty set on either side gives the empty shape;
    // otherwise the pair is dispatched to the concrete `intersection` when one
    // exists. Thanks to the rank-constrained fallbacks, the `requires` probe is
    // SFINAE-safe and self-maintaining: a pair with no intersection (Disk, or
    // two shapes neither of which implements the other) simply takes the throw.
    template <class ResultNumber, class Left, class Right>
    static constexpr Shape<Point<ResultNumber, LabelType>> intersectionOf(const Left& left, const Right& right) {
        using ResultPoint = Point<ResultNumber, LabelType>;
        if constexpr (std::same_as<Left, EmptyShape<PointType>> ||
                      std::same_as<Right, EmptyShape<PointType>>) {
            return Shape<ResultPoint>{EmptyShape<ResultPoint>{}};
        } else if constexpr (requires { left.template intersection<ResultNumber>(right); }) {
            return resultToShape<ResultNumber>(left.template intersection<ResultNumber>(right));
        } else {
            throw std::logic_error("Shape::intersection is not defined for this shape pair");
        }
    }

    // Measure the squared distance between two unwrapped alternatives. A pair with
    // no defined squaredDistance (anything against an EmptyShape) takes the throw.
    // The requires probes are SFINAE-safe and self-maintaining: a pair gains
    // support here as soon as either side implements squaredDistance for the
    // other (directly or via forwarding). The second probe covers Disk, whose
    // squaredDistance (and the Disk overloads on Convex and Polygon) always
    // returns double rather than being templated on ResultNumber.
    template <class ResultNumber, class Left, class Right>
    static constexpr ResultNumber squaredDistanceOf(const Left& left, const Right& right) {
        if constexpr (requires { left.template squaredDistance<ResultNumber>(right); }) {
            return left.template squaredDistance<ResultNumber>(right);
        } else if constexpr (requires { left.squaredDistance(right); }) {
            return static_cast<ResultNumber>(left.squaredDistance(right));
        } else {
            throw std::logic_error("Shape::squaredDistance is not defined for this shape pair");
        }
    }

    // Unwrap a concrete intersection result (optional<T> or vector<T>, where T is
    // a single alternative or a variant over alternatives) into a Shape. An
    // absent/empty result is the empty shape; a disconnected result (more than
    // one component) cannot be a single Shape and throws. The actual wrapping of
    // a value or variant is handled by the Shape constructors.
    template <class ResultNumber, class Result>
    static constexpr Shape<Point<ResultNumber, LabelType>> resultToShape(const Result& result) {
        using ResultShape = Shape<Point<ResultNumber, LabelType>>;
        if constexpr (detail::is_std_optional<Result>::value) {
            return result ? ResultShape(*result) : ResultShape{};
        } else if constexpr (detail::is_std_vector<Result>::value) {
            if (result.size() > 1) {
                throw std::logic_error(
                    "Shape::intersection: disconnected result cannot be a single Shape");
            }
            return result.empty() ? ResultShape{} : ResultShape(result.front());
        } else {
            return ResultShape(result);
        }
    }

    Variant value_{};
};

// Deduce the wrapper's point type from the alternatives of a result variant (or
// an optional thereof), so `Shape s = a.intersection<N>(b);` names the right
// type. The point type is taken from the variant's first alternative; every
// alternative shares it.
template <class T, class... Ts>
Shape(const std::variant<T, Ts...>&) -> Shape<detail::shape_point_type_t<T>>;

template <class T, class... Ts>
Shape(const std::optional<std::variant<T, Ts...>>&) -> Shape<detail::shape_point_type_t<T>>;

/**
 * @brief Translates a shape by a point.
 *
 * Visits the stored alternative and translates it, re-wrapping the result. The
 * coordinate type is promoted to match the translation, mirroring the per-shape
 * translation operators.
 *
 * @param shape Shape to translate.
 * @param translation Translation vector.
 * @return Translated shape over the promoted point type.
 */
template <class PointType, class TranslationNumber, class TranslationLabel>
constexpr auto operator+(const Shape<PointType>& shape,
                         const Point<TranslationNumber, TranslationLabel>& translation) {
    using ResultPoint = std::decay_t<decltype(std::declval<const PointType&>() + translation)>;
    return std::visit(
        [&translation](const auto& alternative) {
            return Shape<ResultPoint>(alternative + translation);
        },
        shape.variant());
}

/** @copydoc operator+(const Shape<PointType>&, const Point<TranslationNumber, TranslationLabel>&) */
template <class TranslationNumber, class TranslationLabel, class PointType>
constexpr auto operator+(const Point<TranslationNumber, TranslationLabel>& translation,
                         const Shape<PointType>& shape) {
    return shape + translation;
}

/**
 * @brief Translates a shape by a negated point.
 *
 * Visits the stored alternative and translates it, re-wrapping the result. The
 * coordinate type is promoted to match the translation, mirroring the per-shape
 * translation operators.
 *
 * @param shape Shape to translate.
 * @param translation Translation vector.
 * @return Translated shape over the promoted point type.
 */
template <class PointType, class TranslationNumber, class TranslationLabel>
constexpr auto operator-(const Shape<PointType>& shape,
                         const Point<TranslationNumber, TranslationLabel>& translation) {
    using ResultPoint = std::decay_t<decltype(std::declval<const PointType&>() - translation)>;
    return std::visit(
        [&translation](const auto& alternative) {
            return Shape<ResultPoint>(alternative - translation);
        },
        shape.variant());
}

/**
 * @brief Scales a shape around the origin.
 *
 * Visits the stored alternative and scales it, re-wrapping the result. The
 * coordinate type is promoted to match the scalar, mirroring the per-shape
 * scaling operators.
 *
 * @param shape Shape to scale.
 * @param scalar Scaling factor.
 * @return Scaled shape over the promoted point type.
 */
template <class PointType, class Scalar>
    requires(!detail::is_point_v<Scalar>)
constexpr auto operator*(const Shape<PointType>& shape, const Scalar& scalar) {
    using ResultPoint = std::decay_t<decltype(std::declval<const PointType&>() * scalar)>;
    return std::visit(
        [&scalar](const auto& alternative) {
            return Shape<ResultPoint>(alternative * scalar);
        },
        shape.variant());
}

/** @copydoc operator*(const Shape<PointType>&, const Scalar&) */
template <class Scalar, class PointType>
    requires(!detail::is_point_v<Scalar>)
constexpr auto operator*(const Scalar& scalar, const Shape<PointType>& shape) {
    return shape * scalar;
}

/**
 * @brief Divides a shape around the origin.
 *
 * Visits the stored alternative and divides it, re-wrapping the result. The
 * coordinate type is promoted to match the scalar, mirroring the per-shape
 * scaling operators.
 *
 * @param shape Shape to scale.
 * @param scalar Scaling factor.
 * @return Scaled shape over the promoted point type.
 */
template <class PointType, class Scalar>
    requires(!detail::is_point_v<Scalar>)
constexpr auto operator/(const Shape<PointType>& shape, const Scalar& scalar) {
    using ResultPoint = std::decay_t<decltype(std::declval<const PointType&>() / scalar)>;
    return std::visit(
        [&scalar](const auto& alternative) {
            return Shape<ResultPoint>(alternative / scalar);
        },
        shape.variant());
}

/**
 * @brief Streams the currently stored alternative.
 *
 * @tparam PointType Shared point type.
 * @param stream Output stream.
 * @param shape Shape wrapper to print.
 * @return The output stream.
 */
template <class PointType>
std::ostream& operator<<(std::ostream& stream, const Shape<PointType>& shape) {
    std::visit(
        [&stream](const auto& value) {
            stream << value;
        },
        shape.variant());
    return stream;
}

}  // namespace pgl
