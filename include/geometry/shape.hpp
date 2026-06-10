#pragma once

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
#include <ostream>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>

#include "../pgl.hpp"

namespace pgl {

namespace detail {

template <class PointType, class T>
struct is_shape_alternative : std::false_type {};

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

template <class T>
struct is_shape : std::false_type {};

template <class PointType>
struct is_shape<Shape<PointType>> : std::true_type {};

template <class T>
inline constexpr bool is_shape_v = is_shape<std::remove_cvref_t<T>>::value;

template <class Left, class Right>
constexpr bool dispatchContains(const Left& left, const Right& right) {
    if constexpr (requires { left.contains(right); }) {
        return left.contains(right);
    } else {
        return false;
    }
}

template <class Left, class Right>
constexpr bool dispatchBoundaryContains(const Left& left, const Right& right) {
    if constexpr (requires { left.boundaryContains(right); }) {
        return left.boundaryContains(right);
    } else {
        return false;
    }
}

template <class Left, class Right>
constexpr bool dispatchInteriorContains(const Left& left, const Right& right) {
    if constexpr (requires { left.interiorContains(right); }) {
        return left.interiorContains(right);
    } else {
        return false;
    }
}

template <class Left, class Right>
constexpr bool dispatchIntersects(const Left& left, const Right& right) {
    if constexpr (requires { left.intersects(right); }) {
        return left.intersects(right);
    } else if constexpr (requires { right.intersects(left); }) {
        return right.intersects(left);
    } else if constexpr (is_point_v<Left> && requires { right.contains(left); }) {
        return right.contains(left);
    } else if constexpr (is_point_v<Right> && requires { left.contains(right); }) {
        return left.contains(right);
    } else {
        return false;
    }
}

template <class Left, class Right>
constexpr bool dispatchInteriorsIntersect(const Left& left, const Right& right) {
    if constexpr (requires { left.interiorsIntersect(right); }) {
        return left.interiorsIntersect(right);
    } else if constexpr (requires { right.interiorsIntersect(left); }) {
        return right.interiorsIntersect(left);
    } else {
        return false;
    }
}

template <class Left, class Right>
constexpr bool dispatchSeparates(const Left& left, const Right& right) {
    if constexpr (requires { left.separates(right); }) {
        return left.separates(right);
    } else {
        return false;
    }
}

template <class Left, class Right>
constexpr bool dispatchCrosses(const Left& left, const Right& right) {
    if constexpr (requires { left.crosses(right); }) {
        return left.crosses(right);
    } else if constexpr (requires { right.crosses(left); }) {
        return right.crosses(left);
    } else {
        return false;
    }
}

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
    /** Variant type used for storage and visitation. */
    using Variant = std::variant<
        PointType,
        Segment<PointType>,
        OrientedSegment<PointType>,
        Line<PointType>,
        OrientedLine<PointType>,
        Ray<PointType>,
        Halfplane<PointType>,
        Rectangle<PointType>,
        Triangle<PointType>,
        Convex<PointType>,
        Disk<PointType>,
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
     * @brief Tests geometric containment between two wrapped shapes.
     *
     * @param other Shape to test.
     * @return Result of dispatching `contains`.
     */
    constexpr bool contains(const Shape& other) const {
        return std::visit(
            [](const auto& left, const auto& right) {
                return detail::dispatchContains(left, right);
            },
            value_,
            other.value_);
    }

    /**
     * @brief Tests whether the wrapped shape contains one concrete alternative.
     *
     * @tparam T Alternative type.
     * @param other Shape to test.
     * @return Result of dispatching `contains`.
     */
    template <class T>
        requires(detail::ShapeAlternative<PointType, T>)
    constexpr bool contains(const T& other) const {
        return std::visit(
            [&other](const auto& self) {
                return detail::dispatchContains(self, other);
            },
            value_);
    }

    /**
     * @brief Tests boundary containment between two wrapped shapes.
     *
     * @param other Shape to test.
     * @return Result of dispatching `boundaryContains`.
     */
    constexpr bool boundaryContains(const Shape& other) const {
        return std::visit(
            [](const auto& left, const auto& right) {
                return detail::dispatchBoundaryContains(left, right);
            },
            value_,
            other.value_);
    }

    /**
     * @brief Tests whether the wrapped shape boundary contains one concrete alternative.
     *
     * @tparam T Alternative type.
     * @param other Shape to test.
     * @return Result of dispatching `boundaryContains`.
     */
    template <class T>
        requires(detail::ShapeAlternative<PointType, T>)
    constexpr bool boundaryContains(const T& other) const {
        return std::visit(
            [&other](const auto& self) {
                return detail::dispatchBoundaryContains(self, other);
            },
            value_);
    }

    /**
     * @brief Tests interior containment between two wrapped shapes.
     *
     * @param other Shape to test.
     * @return Result of dispatching `interiorContains`.
     */
    constexpr bool interiorContains(const Shape& other) const {
        return std::visit(
            [](const auto& left, const auto& right) {
                return detail::dispatchInteriorContains(left, right);
            },
            value_,
            other.value_);
    }

    /**
     * @brief Tests whether the wrapped shape strictly contains one concrete alternative.
     *
     * @tparam T Alternative type.
     * @param other Shape to test.
     * @return Result of dispatching `interiorContains`.
     */
    template <class T>
        requires(detail::ShapeAlternative<PointType, T>)
    constexpr bool interiorContains(const T& other) const {
        return std::visit(
            [&other](const auto& self) {
                return detail::dispatchInteriorContains(self, other);
            },
            value_);
    }

    /**
     * @brief Tests intersection between two wrapped shapes.
     *
     * @param other Shape to test.
     * @return Result of dispatching `intersects`.
     */
    constexpr bool intersects(const Shape& other) const {
        return std::visit(
            [](const auto& left, const auto& right) {
                return detail::dispatchIntersects(left, right);
            },
            value_,
            other.value_);
    }

    /**
     * @brief Tests whether the wrapped shape intersects one concrete alternative.
     *
     * @tparam T Alternative type.
     * @param other Shape to test.
     * @return Result of dispatching `intersects`.
     */
    template <class T>
        requires(detail::ShapeAlternative<PointType, T>)
    constexpr bool intersects(const T& other) const {
        return std::visit(
            [&other](const auto& self) {
                return detail::dispatchIntersects(self, other);
            },
            value_);
    }

    /**
     * @brief Tests whether interiors intersect between two wrapped shapes.
     *
     * @param other Shape to test.
     * @return Result of dispatching `interiorsIntersect`.
     */
    constexpr bool interiorsIntersect(const Shape& other) const {
        return std::visit(
            [](const auto& left, const auto& right) {
                return detail::dispatchInteriorsIntersect(left, right);
            },
            value_,
            other.value_);
    }

    /**
     * @brief Tests whether the wrapped shape has interior intersection with one concrete alternative.
     *
     * @tparam T Alternative type.
     * @param other Shape to test.
     * @return Result of dispatching `interiorsIntersect`.
     */
    template <class T>
        requires(detail::ShapeAlternative<PointType, T>)
    constexpr bool interiorsIntersect(const T& other) const {
        return std::visit(
            [&other](const auto& self) {
                return detail::dispatchInteriorsIntersect(self, other);
            },
            value_);
    }

    /**
     * @brief Tests whether the wrapped shape separates another wrapped shape.
     *
     * @param other Shape to test.
     * @return Result of dispatching `separates` when available.
     */
    constexpr bool separates(const Shape& other) const {
        return std::visit(
            [](const auto& left, const auto& right) {
                return detail::dispatchSeparates(left, right);
            },
            value_,
            other.value_);
    }

    /**
     * @brief Tests whether the wrapped shape separates one concrete alternative.
     *
     * @tparam T Alternative type.
     * @param other Shape to test.
     * @return Result of dispatching `separates` when available.
     */
    template <class T>
        requires(detail::ShapeAlternative<PointType, T>)
    constexpr bool separates(const T& other) const {
        return std::visit(
            [&other](const auto& self) {
                return detail::dispatchSeparates(self, other);
            },
            value_);
    }

    /**
     * @brief Tests whether the wrapped shape crosses another wrapped shape.
     *
     * @param other Shape to test.
     * @return Result of dispatching `crosses` when available.
     */
    constexpr bool crosses(const Shape& other) const {
        return std::visit(
            [](const auto& left, const auto& right) {
                return detail::dispatchCrosses(left, right);
            },
            value_,
            other.value_);
    }

    /**
     * @brief Tests whether the wrapped shape crosses one concrete alternative.
     *
     * @tparam T Alternative type.
     * @param other Shape to test.
     * @return Result of dispatching `crosses` when available.
     */
    template <class T>
        requires(detail::ShapeAlternative<PointType, T>)
    constexpr bool crosses(const T& other) const {
        return std::visit(
            [&other](const auto& self) {
                return detail::dispatchCrosses(self, other);
            },
            value_);
    }

  private:
    Variant value_{};
};

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
