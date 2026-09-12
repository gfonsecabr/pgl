#pragma once

#include "shape/polygonset.hpp"

/**
 * @file shape.hpp
 * @brief Runtime variant wrapper over the currently implemented shape types.
 *
 * Most of Pangolin is static and template-based; Shape exists for places where
 * runtime dispatch over heterogeneous shapes is more convenient.
 */

#include <array>
#include <compare>
#include <concepts>
#include <functional>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>


namespace pgl {

namespace detail {

// The alternatives of Shape<PointType>, in storage order. The leading
// EmptyShape is the state of a default-constructed Shape. The accessor table
// PGL_SHAPE_ALTERNATIVES below lists the same types and is checked against
// this one.
template <class PointType>
using ShapeVariant = std::variant<
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
    MonotoneChain<PointType>,
    Polyline<PointType>,
    Polygon<PointType>,
    HalfplaneIntersection<PointType>,
    PolygonWithHoles<PointType>,
    PolygonSet<PointType>>;

template <class T, class Variant>
struct variant_has : std::false_type {};
template <class T, class... Ts>
struct variant_has<T, std::variant<Ts...>> : std::bool_constant<(std::same_as<T, Ts> || ...)> {};

// True iff T is exactly one of the alternatives of Shape<PointType>.
template <class PointType, class T>
concept ShapeAlternative = variant_has<std::remove_cvref_t<T>, ShapeVariant<PointType>>::value;

// True iff T is a std::variant whose every alternative is an alternative of
// Shape<PointType>. Gates the variant-unwrapping Shape constructor so a variant
// that could hold a non-shape is rejected at compile time.
template <class PointType, class T>
struct is_shape_variant : std::false_type {};

template <class PointType, class... Ts>
struct is_shape_variant<PointType, std::variant<Ts...>>
    : std::bool_constant<(ShapeAlternative<PointType, Ts> && ...)> {};

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

// Point type carried by a shape: a Point is its own point type; every other
// shape, Shape included, exposes it as a nested PointType.
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

// The alternative of a variant that is the same kind of shape as T, i.e. has
// its shapeRank; void when there is none.
template <int Rank, class Variant>
struct alternative_with_rank {
    using type = void;
};

template <int Rank, class T, class... Ts>
struct alternative_with_rank<Rank, std::variant<T, Ts...>> {
    using type = std::conditional_t<shapeRank<T> == Rank, T,
                                    typename alternative_with_rank<Rank, std::variant<Ts...>>::type>;
};

template <class PointType, class T>
using shape_alternative_of_kind_t =
    typename alternative_with_rank<shapeRank<std::remove_cvref_t<T>>, ShapeVariant<PointType>>::type;

// True iff a concrete shape T of another point or label type converts to the
// alternative of Shape<PointType> of its kind.
template <class PointType, class T>
concept ConvertibleToShapeAlternative =
    !ShapeConcept<T> && !ShapeAlternative<PointType, T> && shapeRank<std::remove_cvref_t<T>> >= 0 &&
    (EmptyShapeConcept<T> || std::is_constructible_v<shape_alternative_of_kind_t<PointType, T>, const T&>);

// Converts a concrete shape to the alternative of Shape<PointType> of its kind.
template <class PointType, class T>
constexpr auto toShapeAlternative(const T& value) {
    using Alternative = shape_alternative_of_kind_t<PointType, T>;
    if constexpr (EmptyShapeConcept<T>) {
        return Alternative{};
    } else {
        return Alternative(value);
    }
}

// Converts an exactly computed point to Point<ResultNumber, Label>, throwing
// when ResultNumber is not closed under division and the point is off its
// lattice. This is how a Shape over an integral point type reports a
// HalfplaneIntersection vertex it cannot hold, instead of truncating it.
template <class ResultNumber, class Label, class ExactPoint>
constexpr Point<ResultNumber, Label> narrowPoint(const ExactPoint& point, std::string_view operation) {
    if constexpr (!std::same_as<division_result_t<ResultNumber>, ResultNumber> &&
                  requires { point.x().isInteger(); }) {
        if (!point.x().isInteger() || !point.y().isInteger()) {
            throw unsupported_operation(operation, "HalfplaneIntersection");
        }
    }
    return Point<ResultNumber, Label>(point);
}

}  // namespace detail

// The accessor table: every alternative of Shape<PointType> as a (Name, Type)
// pair, in storage order. The holdsX / getIfHoldsX / asX shorthands are
// generated from it, and it is checked against detail::ShapeVariant; error
// messages name the alternatives through detail::shapeName.
#define PGL_SHAPE_ALTERNATIVES(X)                                  \
    X(EmptyShape, EmptyShape<PointType>)                           \
    X(Point, PointType)                                            \
    X(Segment, Segment<PointType>)                                 \
    X(OrientedSegment, OrientedSegment<PointType>)                 \
    X(Line, Line<PointType>)                                       \
    X(OrientedLine, OrientedLine<PointType>)                       \
    X(Ray, Ray<PointType>)                                         \
    X(Halfplane, Halfplane<PointType>)                             \
    X(Rectangle, Rectangle<PointType>)                             \
    X(Triangle, Triangle<PointType>)                               \
    X(Disk, Disk<PointType>)                                       \
    X(Convex, Convex<PointType>)                                   \
    X(MonotoneChain, MonotoneChain<PointType>)                     \
    X(Polyline, Polyline<PointType>)                               \
    X(Polygon, Polygon<PointType>)                                 \
    X(HalfplaneIntersection, HalfplaneIntersection<PointType>)     \
    X(PolygonWithHoles, PolygonWithHoles<PointType>)               \
    X(PolygonSet, PolygonSet<PointType>)

/**
 * @brief Type-erased wrapper over the finite set of supported primitive shapes.
 *
 * `Shape<PointType>` stores one geometry in a variant. It does what the object
 * it holds does whenever that is possible, and throws
 * @ref unsupported_operation otherwise: nothing is forbidden at compile time by
 * the set of alternatives, and nothing answers in place of an implementation
 * that does not exist.
 *
 * Two vocabularies never share a word. `is…`, `getIf…` and `as…` are geometric
 * and answer as the held object does (`isPoint()`: is the point set a single
 * point); `holds…`, `getIfHolds…` and `asHeld…` are about storage
 * (`holdsPoint()`: is the stored alternative a `Point`).
 *
 * @tparam PointType_ Point type shared by every stored alternative.
 */
template <class PointType_ = Point<>>
struct Shape {
    /** Point type shared by all alternatives. */
    using PointType = PointType_;
    /** Coordinate type of the stored point type. */
    using NumberType = typename PointType::NumberType;
    /** Label type of the stored point type. */
    using LabelType = typename PointType::LabelType;
    /** Variant type used for storage and visitation. The leading
     * `EmptyShape` is the empty state of a default-constructed `Shape`. */
    using Variant = detail::ShapeVariant<PointType>;

#define PGL_SHAPE_COUNT_ALTERNATIVE(Name, Type) +1
#define PGL_SHAPE_CHECK_ALTERNATIVE(Name, Type) \
    static_assert(detail::variant_has<Type, Variant>::value, "PGL_SHAPE_ALTERNATIVES lists " #Name " but the variant does not hold it");
    static_assert(std::variant_size_v<Variant> == 0 PGL_SHAPE_ALTERNATIVES(PGL_SHAPE_COUNT_ALTERNATIVE),
                  "PGL_SHAPE_ALTERNATIVES and detail::ShapeVariant disagree");
    PGL_SHAPE_ALTERNATIVES(PGL_SHAPE_CHECK_ALTERNATIVE)
#undef PGL_SHAPE_CHECK_ALTERNATIVE
#undef PGL_SHAPE_COUNT_ALTERNATIVE

    // -------------------------------------------------------------------------
    // Construction

    /**
     * @brief Creates the empty shape, holding an `EmptyShape`.
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
     * @brief Converts a concrete shape of another point or label type into the
     *        alternative of its kind.
     *
     * `Shape<Point<double>>(segment)` stores `Segment<Point<double>>(segment)`
     * for a `Segment` over any point type, and likewise for every other kind.
     *
     * @tparam T Concrete shape type that is not itself an alternative.
     * @param value Shape to convert.
     */
    template <class T>
        requires(detail::ConvertibleToShapeAlternative<PointType, T>)
    constexpr explicit Shape(const T& value)
        : value_(detail::toShapeAlternative<PointType>(value)) {}

    /**
     * @brief Converts a shape over another point type, alternative by alternative.
     *
     * @throws unsupported_operation if the stored alternative has no conversion
     *   to this point type.
     */
    template <class OtherPoint>
        requires(!std::same_as<OtherPoint, PointType>)
    constexpr explicit Shape(const Shape<OtherPoint>& other)
        : value_(other.visit([](const auto& alternative) -> Variant {
              using S = std::remove_cvref_t<decltype(alternative)>;
              if constexpr (EmptyShapeConcept<S> ||
                            std::is_constructible_v<detail::shape_alternative_of_kind_t<PointType, S>, const S&>) {
                  return Variant(detail::toShapeAlternative<PointType>(alternative));
              } else {
                  throw unsupported_operation("Shape conversion", detail::shapeName<S>);
              }
          })) {}

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

    // -------------------------------------------------------------------------
    // Storage access

    /**
     * @brief Calls @p f with the stored alternative.
     *
     * @return Whatever @p f returns, which must be the same type for every alternative.
     */
    template <class F>
    constexpr decltype(auto) visit(F&& f) const {
        return std::visit(std::forward<F>(f), value_);
    }

    /** @copydoc visit(F&&) const */
    template <class F>
    constexpr decltype(auto) visit(F&& f) {
        return std::visit(std::forward<F>(f), value_);
    }

    /**
     * @brief Returns the underlying variant, for code that needs it as a variant.
     *
     * @return Const reference to the stored variant.
     */
    constexpr const Variant& variant() const {
        return value_;
    }

    /** @copydoc variant() const */
    constexpr Variant& variant() {
        return value_;
    }

    /**
     * @brief Tests whether the stored alternative is `T`.
     *
     * A question about storage, not geometry: a `Shape` holding a `Triangle`
     * collapsed to a point holds a `Triangle` and not a `Point`.
     */
    template <class T>
        requires(detail::ShapeAlternative<PointType, T>)
    [[nodiscard]] constexpr bool holds() const {
        return std::holds_alternative<std::remove_cvref_t<T>>(value_);
    }

    /**
     * @brief Returns a pointer to the stored alternative when it is `T`,
     *        `nullptr` otherwise.
     */
    template <class T>
        requires(detail::ShapeAlternative<PointType, T>)
    [[nodiscard]] constexpr const std::remove_cvref_t<T>* getIfHolds() const {
        return std::get_if<std::remove_cvref_t<T>>(&value_);
    }

    /** @copydoc getIfHolds() const */
    template <class T>
        requires(detail::ShapeAlternative<PointType, T>)
    [[nodiscard]] constexpr std::remove_cvref_t<T>* getIfHolds() {
        return std::get_if<std::remove_cvref_t<T>>(&value_);
    }

    /**
     * @brief Returns the stored alternative, which must be `T`.
     *
     * @throws std::bad_variant_access if another alternative is stored.
     */
    template <class T>
        requires(detail::ShapeAlternative<PointType, T>)
    [[nodiscard]] constexpr const std::remove_cvref_t<T>& asHeld() const {
        return std::get<std::remove_cvref_t<T>>(value_);
    }

    /** @copydoc asHeld() const */
    template <class T>
        requires(detail::ShapeAlternative<PointType, T>)
    [[nodiscard]] constexpr std::remove_cvref_t<T>& asHeld() {
        return std::get<std::remove_cvref_t<T>>(value_);
    }

    /**
     * @brief Converts to the stored alternative, so an alternative can be
     *        recovered by naming its type: `Point cross(shape);`.
     *
     * The same value @ref asHeld returns, spelled as a construction or a
     * `static_cast`. Explicit, so it never fires on its own -- a `Shape` is
     * never silently taken for one of its alternatives.
     *
     * @tparam T Alternative type to extract.
     * @return A copy of the stored value.
     * @throws std::bad_variant_access if another alternative is stored.
     */
    template <class T>
        requires(detail::ShapeAlternative<PointType, T>)
    constexpr explicit operator T() const {
        return std::get<std::remove_cvref_t<T>>(value_);
    }

    /**
     * @name Per-alternative storage accessors
     *
     * Named shorthands for @ref holds, @ref getIfHolds and @ref asHeld, one
     * family per stored alternative: `holdsPoint()` / `getIfHoldsPoint()` /
     * `asHeldPoint()`, `holdsSegment()` / `getIfHoldsSegment()` /
     * `asHeldSegment()`, and so on through every alternative of @ref Variant,
     * `EmptyShape` included. The plain `getIfPoint()`, `getIfSegment()` and
     * `asPolygonWithHoles()` are geometric, as on every concrete shape.
     */
    ///@{
// The doc comments below are part of the expansion on purpose: a description on
// the enclosing @name group documents the group, not its members, which would
// leave every generated method without a @brief of their own.
#define PGL_SHAPE_STORAGE_ACCESSORS(Name, Type)                          \
    /** @brief Tests whether the stored alternative is Name. */          \
    [[nodiscard]] constexpr bool holds##Name() const {                   \
        return std::holds_alternative<Type>(value_);                     \
    }                                                                    \
    /** @brief Returns a pointer to the stored Name, or `nullptr`         \
        when another alternative is stored. */                           \
    [[nodiscard]] constexpr const Type* getIfHolds##Name() const {       \
        return std::get_if<Type>(&value_);                               \
    }                                                                    \
    /** @brief Returns a pointer to the stored Name, or `nullptr`         \
        when another alternative is stored. */                           \
    [[nodiscard]] constexpr Type* getIfHolds##Name() {                   \
        return std::get_if<Type>(&value_);                               \
    }                                                                    \
    /** @brief Returns the stored Name; throws std::bad_variant_access    \
        when another alternative is stored. */                           \
    [[nodiscard]] constexpr const Type& asHeld##Name() const {           \
        return std::get<Type>(value_);                                   \
    }                                                                    \
    /** @brief Returns the stored Name; throws std::bad_variant_access    \
        when another alternative is stored. */                           \
    [[nodiscard]] constexpr Type& asHeld##Name() {                       \
        return std::get<Type>(value_);                                   \
    }

    PGL_SHAPE_ALTERNATIVES(PGL_SHAPE_STORAGE_ACCESSORS)

#undef PGL_SHAPE_STORAGE_ACCESSORS
    ///@}

    // -------------------------------------------------------------------------
    // Geometry queries

    /**
     * @brief Tests whether the wrapped shape covers no point at all.
     *
     * True for the `EmptyShape` alternative and for any alternative in its own
     * empty state, such as an empty `Rectangle`; `holdsEmptyShape()` asks the
     * storage question instead.
     */
    [[nodiscard]] constexpr bool empty() const {
        return visit([](const auto& value) { return detail::coversNoPoint(value); });
    }

    /**
     * @brief Tests whether the point set is a single point.
     *
     * A stored `Point` is one; every other alternative answers its own
     * `isPoint()`, and those that have none -- `EmptyShape`, `Line`,
     * `OrientedLine`, `Ray`, `Halfplane` -- are never a point.
     */
    [[nodiscard]] constexpr bool isPoint() const {
        return visit([](const auto& value) { return isPointOf(value); });
    }

    /**
     * @brief Tests whether the point set is a segment of positive length.
     *
     * A stored `Segment` or `OrientedSegment` is one unless it has collapsed to
     * a point; every other alternative answers its own `isSegment()`, and those
     * that have none are never a segment.
     */
    [[nodiscard]] constexpr bool isSegment() const {
        return visit([](const auto& value) -> bool {
            using S = std::remove_cvref_t<decltype(value)>;
            if constexpr (SegmentConcept<S> || OrientedSegmentConcept<S>) {
                return !value.isPoint();
            } else if constexpr (requires { value.isSegment(); }) {
                return value.isSegment();
            } else {
                return false;
            }
        });
    }

    /**
     * @brief Returns the point the point set collapses to, if it is one.
     *
     * The geometric counterpart of @ref isPoint, answered as the held object's
     * own `getIfPoint()`; @ref getIfHoldsPoint asks the storage question.
     *
     * @tparam ResultNumber Coordinate type of the returned point.
     * @throws unsupported_operation for a `HalfplaneIntersection` that collapses
     *   to a point off the lattice of an integral @p ResultNumber, which cannot
     *   hold it.
     */
    template <class ResultNumber = NumberType>
    [[nodiscard]] constexpr std::optional<Point<ResultNumber, LabelType>> getIfPoint() const {
        using Result = std::optional<Point<ResultNumber, LabelType>>;
        return visit([](const auto& value) -> Result {
            using S = std::remove_cvref_t<decltype(value)>;
            if constexpr (PointConcept<S>) {
                return Point<ResultNumber, LabelType>(value);
            } else if constexpr (HalfplaneIntersectionConcept<S>) {
                const auto exact = value.getIfPoint();
                if (!exact) {
                    return Result{};
                }
                return detail::narrowPoint<ResultNumber, LabelType>(*exact, "getIfPoint");
            } else if constexpr (requires { value.getIfPoint(); }) {
                const auto point = value.getIfPoint();
                return point ? Result(Point<ResultNumber, LabelType>(*point)) : Result{};
            } else {
                return Result{};
            }
        });
    }

    /**
     * @brief Returns the segment the point set collapses to, if it is one.
     *
     * The geometric counterpart of @ref isSegment; @ref getIfHoldsSegment asks
     * the storage question.
     *
     * @tparam ResultNumber Coordinate type of the returned endpoints.
     * @throws unsupported_operation for a `HalfplaneIntersection` that collapses
     *   to a segment with an endpoint off the lattice of an integral
     *   @p ResultNumber.
     */
    template <class ResultNumber = NumberType>
    [[nodiscard]] constexpr std::optional<Segment<Point<ResultNumber, LabelType>>> getIfSegment() const {
        using ResultPoint = Point<ResultNumber, LabelType>;
        using ResultSegment = Segment<ResultPoint>;
        using Result = std::optional<ResultSegment>;
        return visit([](const auto& value) -> Result {
            using S = std::remove_cvref_t<decltype(value)>;
            if constexpr (SegmentConcept<S> || OrientedSegmentConcept<S>) {
                if (value.isPoint()) {
                    return Result{};
                }
                return ResultSegment(ResultPoint(value[0]), ResultPoint(value[1]));
            } else if constexpr (HalfplaneIntersectionConcept<S>) {
                const auto exact = value.getIfSegment();
                if (!exact) {
                    return Result{};
                }
                return ResultSegment(detail::narrowPoint<ResultNumber, LabelType>((*exact)[0], "getIfSegment"),
                                     detail::narrowPoint<ResultNumber, LabelType>((*exact)[1], "getIfSegment"));
            } else if constexpr (requires { value.getIfSegment(); }) {
                const auto segment = value.getIfSegment();
                if (!segment) {
                    return Result{};
                }
                return ResultSegment(ResultPoint((*segment)[0]), ResultPoint((*segment)[1]));
            } else {
                return Result{};
            }
        });
    }

    /**
     * @brief Tests whether the wrapped shape is degenerate, as the held object
     *        defines it.
     */
    [[nodiscard]] constexpr bool isDegenerate() const {
        return visit([](const auto& value) -> bool {
            if constexpr (requires { value.isDegenerate(); }) {
                return value.isDegenerate();
            } else {
                throw unsupported_operation("isDegenerate", detail::shapeName<std::remove_cvref_t<decltype(value)>>);
            }
        });
    }

    /**
     * @brief Tests whether the wrapped shape's defining data describes no
     *        shape, as the held object defines it.
     */
    [[nodiscard]] constexpr bool isUndefined() const {
        return visit([](const auto& value) -> bool {
            if constexpr (requires { value.isUndefined(); }) {
                return value.isUndefined();
            } else {
                throw unsupported_operation("isUndefined", detail::shapeName<std::remove_cvref_t<decltype(value)>>);
            }
        });
    }

    /**
     * @brief Returns the dimension of the point set: `-1` when empty, `0` for a
     *        point, `1` for a curve, `2` for a region.
     *
     * This is the dimension of what the stored alternative covers, not of the
     * alternative: a `Triangle` with collinear vertices is `1`, a collapsed one
     * `0`.
     */
    [[nodiscard]] constexpr int dimension() const {
        return visit([](const auto& value) -> int {
            using S = std::remove_cvref_t<decltype(value)>;
            if (detail::coversNoPoint(value)) {
                return -1;
            }
            if (isPointOf(value)) {
                return 0;
            }
            if constexpr (SegmentConcept<S> || OrientedSegmentConcept<S> || LineConcept<S> ||
                          OrientedLineConcept<S> || RayConcept<S> || MonotoneChainConcept<S> ||
                          PolylineConcept<S>) {
                return 1;
            } else if constexpr (DiskConcept<S> || HalfplaneConcept<S>) {
                return 2;
            } else {
                return value.isDegenerate() ? 1 : 2;
            }
        });
    }

    /**
     * @brief Tests whether the point set is bounded.
     *
     * `false` for a `Line`, `OrientedLine`, `Ray`, `Halfplane`, and a
     * `HalfplaneIntersection` that is unbounded; `true` otherwise, including for
     * the empty shape.
     */
    [[nodiscard]] constexpr bool isBounded() const {
        return visit([](const auto& value) -> bool {
            using S = std::remove_cvref_t<decltype(value)>;
            if constexpr (requires { value.isBounded(); }) {
                return value.isBounded();
            } else {
                return !(LineConcept<S> || OrientedLineConcept<S> || RayConcept<S> || HalfplaneConcept<S>);
            }
        });
    }

    /**
     * @brief Returns the wrapped shape's axis-aligned bounding box.
     *
     * @throws unsupported_operation when the held object has no bounding box:
     *   the `EmptyShape`, `Line`, `OrientedLine`, `Ray` and `Halfplane`
     *   alternatives, and an empty or unbounded `HalfplaneIntersection`.
     */
    [[nodiscard]] constexpr Rectangle<PointType> bbox() const {
        return visit([](const auto& value) -> Rectangle<PointType> {
            using S = std::remove_cvref_t<decltype(value)>;
            if constexpr (HalfplaneIntersectionConcept<S>) {
                if (value.empty() || !value.isBounded()) {
                    throw unsupported_operation("bbox", detail::shapeName<S>);
                }
            }
            if constexpr (requires { value.template bbox<NumberType>(); }) {
                return value.template bbox<NumberType>();
            } else if constexpr (requires { value.bbox(); }) {
                return value.bbox();
            } else {
                throw unsupported_operation("bbox", detail::shapeName<S>);
            }
        });
    }

    /**
     * @brief Returns a floating-point bounding box of the wrapped shape.
     *
     * The held object's own `fbox()`, so a `Disk` computes its box from its
     * floating-point center and radius and the stored shapes round their exact
     * coordinates outward.
     *
     * @tparam ResultNumber Floating-point coordinate type of the box.
     * @throws unsupported_operation for an alternative with no `fbox()`: the
     *   `EmptyShape`, `Line`, `OrientedLine`, `Ray` and `Halfplane`
     *   alternatives.
     */
    template <class ResultNumber = double>
    [[nodiscard]] constexpr Rectangle<Point<ResultNumber>> fbox() const {
        using Result = Rectangle<Point<ResultNumber>>;
        return visit([](const auto& value) -> Result {
            if constexpr (requires { value.template fbox<ResultNumber>(); }) {
                return value.template fbox<ResultNumber>();
            } else {
                throw unsupported_operation("fbox", detail::shapeName<std::remove_cvref_t<decltype(value)>>);
            }
        });
    }

    /**
     * @name Queries about the defining data
     *
     * Each answers as the held object's method of the same name, and throws
     * @ref unsupported_operation for an alternative that has none. Unlike the
     * queries above, these are about how the shape is written down rather than
     * about its point set, so only the alternatives that store that kind of
     * data answer: the linear shapes have a slope and the shapes built from a
     * ring know whether it is simple.
     */
    ///@{
// The doc comments are part of the expansion on purpose; see the storage
// accessors above.
#define PGL_SHAPE_FORWARD_QUERY(Name, Supported)                                          \
    /** @brief Name of the wrapped shape. @throws unsupported_operation for an             \
        alternative other than Supported. */                                              \
    [[nodiscard]] constexpr bool Name() const {                                           \
        return visit([](const auto& value) -> bool {                                       \
            if constexpr (requires { value.Name(); }) {                                    \
                return value.Name();                                                      \
            } else {                                                                       \
                throw unsupported_operation(#Name,                                        \
                                            detail::shapeName<std::remove_cvref_t<decltype(value)>>); \
            }                                                                             \
        });                                                                               \
    }

    PGL_SHAPE_FORWARD_QUERY(isVertical, Segment OrientedSegment Line OrientedLine Ray Halfplane)
    PGL_SHAPE_FORWARD_QUERY(isHorizontal, Segment OrientedSegment Line OrientedLine Ray Halfplane)
    PGL_SHAPE_FORWARD_QUERY(isSimple, Polyline Polygon PolygonWithHoles PolygonSet)

#undef PGL_SHAPE_FORWARD_QUERY
    ///@}

    /**
     * @brief Returns the slope of the wrapped linear shape.
     *
     * @tparam ResultNumber Type of the slope, which is a ratio of coordinates.
     * @throws unsupported_operation for an alternative other than `Segment`,
     *   `OrientedSegment`, `Line`, `OrientedLine`, `Ray` and `Halfplane`.
     */
    template <class ResultNumber = division_result_t<NumberType>>
    [[nodiscard]] constexpr ResultNumber slope() const {
        return visit([](const auto& value) -> ResultNumber {
            if constexpr (requires { value.template slope<ResultNumber>(); }) {
                return value.template slope<ResultNumber>();
            } else {
                throw unsupported_operation("slope", detail::shapeName<std::remove_cvref_t<decltype(value)>>);
            }
        });
    }

    /**
     * @name Defining points
     *
     * The points a shape is written down with, by the names the concrete shapes
     * use: @ref min and @ref max for the lexicographically ordered pair of a
     * `Segment`, a `Line` or a `Halfplane` and the corners of a `Rectangle`,
     * @ref source and @ref target for the ordered pair of an oriented shape.
     * Each returns a copy rather than the reference the concrete shape returns,
     * since the stored alternative is reached through a visit.
     */
    ///@{
#define PGL_SHAPE_FORWARD_POINT(Name, Supported)                                          \
    /** @brief The wrapped shape's Name point. @throws unsupported_operation for an        \
        alternative other than Supported. */                                              \
    [[nodiscard]] constexpr PointType Name() const {                                      \
        return visit([](const auto& value) -> PointType {                                 \
            if constexpr (requires { value.Name(); }) {                                    \
                return value.Name();                                                      \
            } else {                                                                       \
                throw unsupported_operation(#Name,                                        \
                                            detail::shapeName<std::remove_cvref_t<decltype(value)>>); \
            }                                                                             \
        });                                                                               \
    }

    PGL_SHAPE_FORWARD_POINT(min, Segment OrientedSegment Line OrientedLine Ray Halfplane Rectangle)
    PGL_SHAPE_FORWARD_POINT(max, Segment OrientedSegment Line OrientedLine Ray Halfplane Rectangle)
    PGL_SHAPE_FORWARD_POINT(source, OrientedSegment OrientedLine Ray Halfplane)
    PGL_SHAPE_FORWARD_POINT(target, OrientedSegment OrientedLine Ray Halfplane)

#undef PGL_SHAPE_FORWARD_POINT
    ///@}

    // -------------------------------------------------------------------------
    // Sequences

    /**
     * @brief Returns the vertices of the wrapped shape.
     *
     * The held object's own `vertices()`, as a vector. A stored `Point` is its
     * one vertex and a `Ray` its source; the `EmptyShape`, `Line`,
     * `OrientedLine` and `Halfplane` alternatives have none.
     *
     * @tparam ResultNumber Coordinate type of the returned vertices.
     * @throws unsupported_operation for a `Disk`, which has no vertices, and for
     *   a `HalfplaneIntersection` with a vertex off the lattice of an integral
     *   @p ResultNumber.
     */
    template <class ResultNumber = NumberType>
    [[nodiscard]] constexpr std::vector<Point<ResultNumber, LabelType>> vertices() const {
        using ResultPoint = Point<ResultNumber, LabelType>;
        using Result = std::vector<ResultPoint>;
        return visit([](const auto& value) -> Result {
            using S = std::remove_cvref_t<decltype(value)>;
            if constexpr (EmptyShapeConcept<S> || LineConcept<S> || OrientedLineConcept<S> ||
                          HalfplaneConcept<S>) {
                return Result{};
            } else if constexpr (PointConcept<S>) {
                return Result{ResultPoint(value)};
            } else if constexpr (RayConcept<S>) {
                return Result{ResultPoint(value.source())};
            } else if constexpr (HalfplaneIntersectionConcept<S>) {
                Result result;
                for (const auto& vertex : value.vertices()) {
                    result.push_back(detail::narrowPoint<ResultNumber, LabelType>(vertex, "vertices"));
                }
                return result;
            } else if constexpr (requires { value.vertices(); }) {
                const auto& vertices = value.vertices();
                return Result(vertices.begin(), vertices.end());
            } else {
                throw unsupported_operation("vertices", detail::shapeName<S>);
            }
        });
    }

    /**
     * @brief Returns the edges of the wrapped shape, as the held object's own
     *        `edges()`.
     *
     * A stored `Point` and the empty shape have none.
     *
     * @throws unsupported_operation for an alternative with no `edges()`: a
     *   `Line`, `OrientedLine`, `Ray`, `Halfplane`, `Disk` or
     *   `HalfplaneIntersection`.
     */
    [[nodiscard]] constexpr std::vector<Segment<PointType>> edges() const {
        using Result = std::vector<Segment<PointType>>;
        return visit([](const auto& value) -> Result {
            using S = std::remove_cvref_t<decltype(value)>;
            if constexpr (EmptyShapeConcept<S>) {
                return Result{};
            } else if constexpr (requires { value.edges(); }) {
                const auto& edges = value.edges();
                return Result(edges.begin(), edges.end());
            } else {
                throw unsupported_operation("edges", detail::shapeName<S>);
            }
        });
    }

    /**
     * @brief Returns the oriented boundary edges of the wrapped shape, as the
     *        held object's own `orientedEdges()`.
     *
     * @throws unsupported_operation for an alternative with no
     *   `orientedEdges()`, the same ones as for @ref edges.
     */
    [[nodiscard]] constexpr std::vector<OrientedSegment<PointType>> orientedEdges() const {
        using Result = std::vector<OrientedSegment<PointType>>;
        return visit([](const auto& value) -> Result {
            using S = std::remove_cvref_t<decltype(value)>;
            if constexpr (EmptyShapeConcept<S>) {
                return Result{};
            } else if constexpr (requires { value.orientedEdges(); }) {
                const auto& edges = value.orientedEdges();
                return Result(edges.begin(), edges.end());
            } else {
                throw unsupported_operation("orientedEdges", detail::shapeName<S>);
            }
        });
    }

    /**
     * @brief Returns the integer points the wrapped shape contains.
     *
     * The held object's own `latticePoints()`: increasing, each point once, the
     * boundary included.
     *
     * @tparam ResultNumber Integer type of the answer.
     * @throws unsupported_operation for an alternative with no
     *   `latticePoints()`, which is every unbounded one -- `Line`,
     *   `OrientedLine`, `Ray` and `Halfplane` -- and the `EmptyShape` and
     *   `Point` alternatives.
     * @throws std::logic_error for an unbounded `HalfplaneIntersection`, or for
     *   a point that does not fit @p ResultNumber, as the concrete shapes do.
     */
    template <class ResultNumber = grid_number_t<NumberType>>
        requires(detail::extended_integral<ResultNumber> || std::same_as<ResultNumber, BigInt>)
    [[nodiscard]] std::vector<Point<ResultNumber, LabelType>> latticePoints() const {
        using Result = std::vector<Point<ResultNumber, LabelType>>;
        return visit([](const auto& value) -> Result {
            if constexpr (requires { value.template latticePoints<ResultNumber>(); }) {
                const auto points = value.template latticePoints<ResultNumber>();
                return Result(points.begin(), points.end());
            } else {
                throw unsupported_operation("latticePoints",
                                           detail::shapeName<std::remove_cvref_t<decltype(value)>>);
            }
        });
    }

    /**
     * @brief Returns the number of vertices of the wrapped shape.
     *
     * The held object's own `vertexCount()`, which the alternatives whose
     * vertices are spread over several rings or components expose in place of
     * @ref size.
     *
     * @throws unsupported_operation for an alternative other than
     *   `HalfplaneIntersection`, `PolygonWithHoles` and `PolygonSet`. Every
     *   other alternative counts its vertices with @ref size.
     */
    [[nodiscard]] constexpr std::size_t vertexCount() const {
        return visit([](const auto& value) -> std::size_t {
            if constexpr (requires { value.vertexCount(); }) {
                return value.vertexCount();
            } else {
                throw unsupported_operation("vertexCount",
                                           detail::shapeName<std::remove_cvref_t<decltype(value)>>);
            }
        });
    }

    /**
     * @brief Returns the number of indexable elements of the wrapped shape.
     *
     * Dispatches to the alternative's `size()` so the result matches the
     * valid range of its `operator[]`.
     *
     * @throws unsupported_operation for the `PolygonWithHoles` and `PolygonSet`
     *   alternatives, neither of which has a single indexable sequence. Use
     *   @ref vertices, or reach through with `getIfHoldsPolygonWithHoles()` or
     *   `getIfHoldsPolygonSet()`.
     */
    [[nodiscard]] constexpr std::size_t size() const {
        return visit([](const auto& value) -> std::size_t {
            using S = std::remove_cvref_t<decltype(value)>;
            if constexpr (PolygonWithHolesConcept<S> || PolygonSetConcept<S>) {
                throw unsupported_operation("size", detail::shapeName<S>);
            } else {
                return value.size();
            }
        });
    }

    /**
     * @brief Returns the i-th vertex (modulo @ref size()) of the wrapped shape.
     *
     * @throws unsupported_operation for the alternatives whose elements are not
     *   vertices: a `Point`, whose elements are coordinates, a
     *   `HalfplaneIntersection`, whose elements are half-planes, and a
     *   `PolygonWithHoles` or `PolygonSet`, which have no single sequence.
     */
    [[nodiscard]] constexpr PointType get(std::ptrdiff_t index) const {
        return visit([index](const auto& value) -> PointType {
            using S = std::remove_cvref_t<decltype(value)>;
            if constexpr (!indexesPoints<S>()) {
                throw unsupported_operation("get", detail::shapeName<S>);
            } else {
                return value.get(index);
            }
        });
    }

    /**
     * @brief Returns the vertex at `index` of the wrapped shape.
     *
     * @throws unsupported_operation for the same alternatives as @ref get.
     */
    [[nodiscard]] constexpr PointType operator[](std::size_t index) const {
        return visit([index](const auto& value) -> PointType {
            using S = std::remove_cvref_t<decltype(value)>;
            if constexpr (!indexesPoints<S>()) {
                throw unsupported_operation("operator[]", detail::shapeName<S>);
            } else {
                return value[index];
            }
        });
    }

    /**
     * @brief Returns the smallest index `i` with `(*this)[i] == point`, or
     * `-1` if no vertex of the wrapped shape equals `point`.
     *
     * @throws unsupported_operation for the same alternatives as @ref get.
     */
    [[nodiscard]] constexpr std::ptrdiff_t index(const PointType& point) const {
        return visit([&point](const auto& value) -> std::ptrdiff_t {
            using S = std::remove_cvref_t<decltype(value)>;
            if constexpr (!indexesPoints<S>()) {
                throw unsupported_operation("index", detail::shapeName<S>);
            } else {
                return value.index(point);
            }
        });
    }

    // -------------------------------------------------------------------------
    // Measures

    /**
     * @brief Returns the area of the wrapped shape, as the held object's own
     *        `area()`.
     *
     * A `Disk` computes in floating point and converts to @p ResultNumber.
     *
     * @throws unsupported_operation for an alternative with no `area()`.
     */
    template <class ResultNumber = division_result_t<NumberType>>
    [[nodiscard]] constexpr ResultNumber area() const {
        return visit([](const auto& value) -> ResultNumber {
            if constexpr (requires { value.template area<ResultNumber>(); }) {
                return static_cast<ResultNumber>(value.template area<ResultNumber>());
            } else if constexpr (requires { value.area(); }) {
                return static_cast<ResultNumber>(value.area());
            } else {
                throw unsupported_operation("area", detail::shapeName<std::remove_cvref_t<decltype(value)>>);
            }
        });
    }

    /**
     * @brief Returns the Euclidean length of the wrapped curve, as the held
     *        object's own `length()`.
     *
     * @throws unsupported_operation for an alternative with no `length()`.
     */
    template <class ResultNumber = double>
    [[nodiscard]] ResultNumber length() const {
        return visit([](const auto& value) -> ResultNumber {
            if constexpr (requires { value.template length<ResultNumber>(); }) {
                return value.template length<ResultNumber>();
            } else {
                throw unsupported_operation("length", detail::shapeName<std::remove_cvref_t<decltype(value)>>);
            }
        });
    }

    /**
     * @brief Returns the centroid of the wrapped region, as the held object's
     *        own `centroid()`.
     *
     * @throws unsupported_operation for an alternative with no `centroid()`.
     * @warning Divides coordinates after casting to ResultNumber.
     */
    template <class ResultNumber = division_result_t<NumberType>>
    [[nodiscard]] constexpr Point<ResultNumber, LabelType> centroid() const {
        return visit([](const auto& value) -> Point<ResultNumber, LabelType> {
            if constexpr (requires { value.template centroid<ResultNumber>(); }) {
                return Point<ResultNumber, LabelType>(value.template centroid<ResultNumber>());
            } else {
                throw unsupported_operation("centroid", detail::shapeName<std::remove_cvref_t<decltype(value)>>);
            }
        });
    }

    /**
     * @brief Returns twice the area of the wrapped shape.
     *
     * Like @ref area, and unlike the concrete shapes, this defaults to
     * @ref division_result_t rather than to the native coordinate type: which
     * alternative is stored is not known until run time, and a
     * `HalfplaneIntersection` needs division even for twice its area. An
     * integral coordinate type therefore comes back as an exact `Rational`
     * holding a whole number; ask for `twiceArea<NumberType>()` for the
     * concrete shapes' own type.
     *
     * @throws unsupported_operation for an alternative with no `twiceArea()`,
     *   which is the `EmptyShape`, `Point`, `Halfplane`, `Disk`,
     *   `MonotoneChain` and `Polyline` alternatives.
     */
    template <class ResultNumber = division_result_t<NumberType>>
    [[nodiscard]] constexpr ResultNumber twiceArea() const {
        return visit([](const auto& value) -> ResultNumber {
            if constexpr (requires { value.template twiceArea<ResultNumber>(); }) {
                return static_cast<ResultNumber>(value.template twiceArea<ResultNumber>());
            } else if constexpr (requires { value.twiceArea(); }) {
                return static_cast<ResultNumber>(value.twiceArea());
            } else {
                throw unsupported_operation("twiceArea",
                                           detail::shapeName<std::remove_cvref_t<decltype(value)>>);
            }
        });
    }

    /**
     * @brief Returns the L1 length of the wrapped curve.
     *
     * @throws unsupported_operation for an alternative other than `Segment`,
     *   `OrientedSegment`, `MonotoneChain` and `Polyline`.
     */
    template <class ResultNumber = NumberType>
    [[nodiscard]] constexpr ResultNumber lengthL1() const {
        return visit([](const auto& value) -> ResultNumber {
            if constexpr (requires { value.lengthL1(); }) {
                return static_cast<ResultNumber>(value.lengthL1());
            } else {
                throw unsupported_operation("lengthL1",
                                           detail::shapeName<std::remove_cvref_t<decltype(value)>>);
            }
        });
    }

    /**
     * @brief Returns the LInf length of the wrapped curve.
     *
     * @throws unsupported_operation for the same alternatives as @ref lengthL1.
     */
    template <class ResultNumber = NumberType>
    [[nodiscard]] constexpr ResultNumber lengthLInf() const {
        return visit([](const auto& value) -> ResultNumber {
            if constexpr (requires { value.lengthLInf(); }) {
                return static_cast<ResultNumber>(value.lengthLInf());
            } else {
                throw unsupported_operation("lengthLInf",
                                           detail::shapeName<std::remove_cvref_t<decltype(value)>>);
            }
        });
    }

    /**
     * @brief Returns the squared Euclidean length of the wrapped segment.
     *
     * @throws unsupported_operation for an alternative other than `Segment` and
     *   `OrientedSegment`, the two whose length is squared exactly.
     */
    template <class ResultNumber = NumberType>
    [[nodiscard]] constexpr ResultNumber squaredLength() const {
        return visit([](const auto& value) -> ResultNumber {
            if constexpr (requires { value.squaredLength(); }) {
                return static_cast<ResultNumber>(value.squaredLength());
            } else {
                throw unsupported_operation("squaredLength",
                                           detail::shapeName<std::remove_cvref_t<decltype(value)>>);
            }
        });
    }

    /**
     * @name Constructed points
     *
     * Each returns the point the held object's method of the same name returns,
     * re-expressed over @p ResultNumber, and throws
     * @ref unsupported_operation for an alternative that has none.
     */
    ///@{
#define PGL_SHAPE_FORWARD_CONSTRUCTED_POINT(Name, Supported)                              \
    /** @brief The wrapped shape's Name. @throws unsupported_operation for an              \
        alternative other than Supported. */                                              \
    template <class ResultNumber = division_result_t<NumberType>>                          \
    [[nodiscard]] constexpr Point<ResultNumber, LabelType> Name() const {                 \
        using Result = Point<ResultNumber, LabelType>;                                     \
        return visit([](const auto& value) -> Result {                                     \
            if constexpr (requires { value.template Name<ResultNumber>(); }) {             \
                return Result(value.template Name<ResultNumber>());                       \
            } else {                                                                       \
                throw unsupported_operation(#Name,                                        \
                                            detail::shapeName<std::remove_cvref_t<decltype(value)>>); \
            }                                                                             \
        });                                                                               \
    }

    PGL_SHAPE_FORWARD_CONSTRUCTED_POINT(pointInside, every alternative but EmptyShape)
    PGL_SHAPE_FORWARD_CONSTRUCTED_POINT(midpoint, Segment OrientedSegment Rectangle)
    PGL_SHAPE_FORWARD_CONSTRUCTED_POINT(center, Rectangle Disk)
    PGL_SHAPE_FORWARD_CONSTRUCTED_POINT(verticesCentroid, Convex Polygon PolygonWithHoles PolygonSet)

#undef PGL_SHAPE_FORWARD_CONSTRUCTED_POINT
    ///@}

    /**
     * @brief Returns a segment realizing the diameter of the wrapped shape.
     *
     * Like @ref twiceArea, this defaults to @ref division_result_t where the
     * concrete shapes default to the native coordinate type, because a `Disk`
     * built from three boundary points finds its endpoints by division.
     *
     * @throws unsupported_operation for an alternative with no `diameter()`,
     *   which is the `EmptyShape` and the unbounded ones -- `Line`,
     *   `OrientedLine`, `Ray`, `Halfplane` and `HalfplaneIntersection`.
     */
    template <class ResultNumber = division_result_t<NumberType>>
    [[nodiscard]] constexpr Segment<Point<ResultNumber, LabelType>> diameter() const {
        using Result = Segment<Point<ResultNumber, LabelType>>;
        return visit([](const auto& value) -> Result {
            if constexpr (requires { value.template diameter<ResultNumber>(); }) {
                return Result(value.template diameter<ResultNumber>());
            } else if constexpr (requires { value.diameter(); }) {
                return Result(value.diameter());
            } else {
                throw unsupported_operation("diameter",
                                           detail::shapeName<std::remove_cvref_t<decltype(value)>>);
            }
        });
    }

    /**
     * @brief Returns the squared Euclidean distance to the given shape.
     *
     * @throws unsupported_operation when the pair has no `squaredDistance`, such
     *   as anything against an `EmptyShape`.
     * @warning A pair involving a @ref Disk computes in `double` and converts to
     *   @p ResultNumber, so an exact @p ResultNumber holds a rounded value there.
     */
    template <class ResultNumber = division_result_t<NumberType>, AnyShapeConcept Other>
    [[nodiscard]] constexpr ResultNumber squaredDistance(const Other& other) const {
        return detail::squaredDistanceAny<ResultNumber>(*this, other);
    }

    /**
     * @brief Returns the Manhattan (L1) distance to the given shape.
     *
     * @throws unsupported_operation when the pair has no `distanceL1`.
     * @warning A pair involving a @ref Disk computes in `double`, as for
     *   @ref squaredDistance.
     */
    template <class ResultNumber = division_result_t<NumberType>, AnyShapeConcept Other>
    [[nodiscard]] constexpr ResultNumber distanceL1(const Other& other) const {
        return detail::distanceL1Any<ResultNumber>(*this, other);
    }

    /**
     * @brief Returns the Chebyshev (LInf) distance to the given shape.
     *
     * @copydetails distanceL1
     */
    template <class ResultNumber = division_result_t<NumberType>, AnyShapeConcept Other>
    [[nodiscard]] constexpr ResultNumber distanceLInf(const Other& other) const {
        return detail::distanceLInfAny<ResultNumber>(*this, other);
    }

    /**
     * @brief Returns the squared Euclidean Hausdorff distance to the given shape.
     *
     * @throws unsupported_operation when the pair has no
     *   `squaredHausdorffDistance`.
     */
    template <class ResultNumber = division_result_t<NumberType>, AnyShapeConcept Other>
    [[nodiscard]] constexpr ResultNumber squaredHausdorffDistance(const Other& other) const {
        return detail::squaredHausdorffDistanceAny<ResultNumber>(*this, other);
    }

    /**
     * @brief Returns the Manhattan (L1) Hausdorff distance to the given shape.
     *
     * @throws unsupported_operation when the pair has no `hausdorffDistanceL1`.
     */
    template <class ResultNumber = division_result_t<NumberType>, AnyShapeConcept Other>
    [[nodiscard]] constexpr ResultNumber hausdorffDistanceL1(const Other& other) const {
        return detail::hausdorffDistanceL1Any<ResultNumber>(*this, other);
    }

    /**
     * @brief Returns the Chebyshev (LInf) Hausdorff distance to the given shape.
     *
     * @throws unsupported_operation when the pair has no `hausdorffDistanceLInf`.
     */
    template <class ResultNumber = division_result_t<NumberType>, AnyShapeConcept Other>
    [[nodiscard]] constexpr ResultNumber hausdorffDistanceLInf(const Other& other) const {
        return detail::hausdorffDistanceLInfAny<ResultNumber>(*this, other);
    }

    // -------------------------------------------------------------------------
    // Predicates
    //
    // Each answers as the held object answers against @p other (or against the
    // object @p other holds), and throws unsupported_operation for a pair with no
    // implementation.

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template <AnyShapeConcept Other>
    [[nodiscard]] constexpr bool contains(const Other& other) const {
        return detail::containsAny(*this, other);
    }

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template <AnyShapeConcept Other>
    [[nodiscard]] constexpr bool boundaryContains(const Other& other) const {
        return detail::boundaryContainsAny(*this, other);
    }

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template <AnyShapeConcept Other>
    [[nodiscard]] constexpr bool interiorContains(const Other& other) const {
        return detail::interiorContainsAny(*this, other);
    }

    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template <AnyShapeConcept Other>
    [[nodiscard]] constexpr bool intersects(const Other& other) const {
        return detail::intersectsAny(*this, other);
    }

    /** @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅). */
    template <AnyShapeConcept Other>
    [[nodiscard]] constexpr bool interiorsIntersect(const Other& other) const {
        return detail::interiorsIntersectAny(*this, other);
    }

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template <AnyShapeConcept Other>
    [[nodiscard]] constexpr bool separates(const Other& other) const {
        return detail::separatesAny(*this, other);
    }

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template <AnyShapeConcept Other>
    [[nodiscard]] constexpr bool crosses(const Other& other) const {
        return detail::crossesAny(*this, other);
    }

    /** @brief Tests whether another shape defines exactly the same point set. */
    template <AnyShapeConcept OtherShape>
    [[nodiscard]] constexpr bool samePointSet(const OtherShape& other) const;

    /**
     * @brief Tests whether the interior of this shape contains the interior of
     *        the other shape, which must be a segment.
     *
     * @throws unsupported_operation unless the stored alternative is `Polygon`,
     *   `PolygonWithHoles` or `PolygonSet` and @p other is or holds a `Segment`
     *   or an `OrientedSegment`.
     */
    template <AnyShapeConcept Other>
    [[nodiscard]] constexpr bool interiorContainsInterior(const Other& other) const {
        return detail::interiorContainsInteriorAny(*this, other);
    }

    /**
     * @brief Tests whether this shape's `pointInside()` witness lies in the
     *        strict interior of the other shape.
     *
     * @throws unsupported_operation for an alternative with no
     *   `pointInsideInteriorContainedIn`, which is the `EmptyShape`, `Polyline`
     *   and `PolygonSet` alternatives.
     */
    template <AnyShapeConcept Other>
    [[nodiscard]] constexpr bool pointInsideInteriorContainedIn(const Other& other) const {
        return detail::pointInsideInteriorContainedInAny(*this, other);
    }

    /**
     * @brief Tests whether some defining point of this shape equals the given
     *        point.
     *
     * Two shapes that compare equal may still differ here when they are written
     * down with different points, as two equal lines can be.
     *
     * @throws unsupported_operation for an alternative with no
     *   `verticesContain`, and for an @p other that is not, and does not hold,
     *   a `Point`.
     */
    template <AnyShapeConcept Other>
    [[nodiscard]] constexpr bool verticesContain(const Other& other) const {
        return detail::verticesContainAny(*this, other);
    }

    /**
     * @brief Tests whether the given point, already known to be collinear with
     *        this shape, lies on it.
     *
     * @throws unsupported_operation unless the stored alternative is `Segment`,
     *   `OrientedSegment` or `Ray` and @p other is or holds a `Point`.
     */
    template <AnyShapeConcept Other>
    [[nodiscard]] constexpr bool containsCollinear(const Other& other) const {
        return detail::containsCollinearAny(*this, other);
    }

    /**
     * @brief Tests whether the given point is an endpoint of this shape.
     *
     * @throws unsupported_operation unless the stored alternative is `Segment`
     *   or `OrientedSegment` and @p other is or holds a `Point`.
     */
    template <AnyShapeConcept Other>
    [[nodiscard]] constexpr bool containsEndpoint(const Other& other) const {
        return detail::containsEndpointAny(*this, other);
    }

    /**
     * @brief Tests whether this shape and the other are parallel.
     *
     * @throws unsupported_operation unless both shapes are linear -- `Segment`,
     *   `OrientedSegment`, `Line`, `OrientedLine` or `Ray` on either side.
     */
    template <AnyShapeConcept Other>
    [[nodiscard]] constexpr bool parallel(const Other& other) const {
        return detail::parallelAny(*this, other);
    }

    /**
     * @brief Tests whether this shape and the other are collinear.
     *
     * @throws unsupported_operation unless this shape is linear -- `Segment`,
     *   `OrientedSegment`, `Line`, `OrientedLine` or `Ray` -- and @p other is
     *   linear or a `Point`.
     */
    template <AnyShapeConcept Other>
    [[nodiscard]] constexpr bool collinear(const Other& other) const {
        return detail::collinearAny(*this, other);
    }

    /**
     * @brief Returns which side of this oriented shape the given point lies on.
     *
     * `std::partial_ordering::greater` to the left, `less` to the right,
     * `equivalent` on the line through it, and `unordered` when this shape is
     * degenerate.
     *
     * @throws unsupported_operation unless the stored alternative is
     *   `OrientedSegment`, `OrientedLine` or `Ray` and @p other is or holds a
     *   `Point`.
     */
    template <AnyShapeConcept Other>
    [[nodiscard]] constexpr std::partial_ordering orientation(const Other& other) const {
        return detail::orientationAny(*this, other);
    }

    // -------------------------------------------------------------------------
    // Constructions

    /**
     * @brief Returns the connected pieces of the intersection of the two shapes
     *        (A ∩ B).
     *
     * The concrete `intersection` of the pair, split by @ref pieces: empty for a
     * disjoint pair, one element for a connected intersection, one per
     * component otherwise.
     *
     * @tparam ResultNumber Coordinate type of the result (defaults to
     *   @ref division_result_t for this wrapper's coordinate type).
     * @throws unsupported_operation when the pair has no `intersection`, such
     *   as anything against a `Disk`.
     * @warning Divides coordinates after casting to ResultNumber.
     */
    template <class ResultNumber = division_result_t<NumberType>, AnyShapeConcept Other>
    [[nodiscard]] constexpr std::vector<Shape<Point<ResultNumber, LabelType>>> intersection(const Other& other) const {
        return detail::intersectionAny<ResultNumber>(*this, other);
    }

    /**
     * @brief Returns the regularized intersection `closure(A° ∩ B°)` of two
     *        region-valued shapes.
     *
     * @throws unsupported_operation when the pair has no
     *   `regularizedIntersection`: one operand must be a `PolygonWithHoles` or
     *   a `PolygonSet`, the other a region, a `Halfplane` or a
     *   `HalfplaneIntersection`.
     */
    template <class ResultNumber = division_result_t<NumberType>, AnyShapeConcept Other>
    [[nodiscard]] PolygonSet<Point<ResultNumber, LabelType>> regularizedIntersection(const Other& other) const {
        return detail::regularizedIntersectionAny<ResultNumber>(*this, other);
    }

    /**
     * @brief Returns the regularized union `closure(A° ∪ B°)`.
     *
     * @throws unsupported_operation unless both alternatives are
     *   @ref PolygonalRegionConcept.
     */
    template <class ResultNumber = division_result_t<NumberType>, AnyShapeConcept Other>
    [[nodiscard]] PolygonSet<Point<ResultNumber, LabelType>> regularizedUnion(const Other& other) const {
        return detail::regularizedUnionAny<ResultNumber>(*this, other);
    }

    /**
     * @brief Returns the regularized set difference of the two shapes (A ∖ B).
     *
     * @throws unsupported_operation unless the left alternative is
     *   @ref PolygonalRegionConcept and the right one is too, or is a
     *   `Halfplane` or `HalfplaneIntersection`.
     */
    template <class ResultNumber = division_result_t<NumberType>, AnyShapeConcept Other>
    [[nodiscard]] PolygonSet<Point<ResultNumber, LabelType>> difference(const Other& other) const {
        return detail::differenceAny<ResultNumber>(*this, other);
    }

    /**
     * @brief Returns the regularized symmetric difference of the two shapes
     *        (A △ B).
     *
     * @throws unsupported_operation on the same pairs as @ref regularizedUnion.
     */
    template <class ResultNumber = division_result_t<NumberType>, AnyShapeConcept Other>
    [[nodiscard]] PolygonSet<Point<ResultNumber, LabelType>> symmetricDifference(const Other& other) const {
        return detail::symmetricDifferenceAny<ResultNumber>(*this, other);
    }

    /**
     * @brief Returns the Minkowski sum of this shape and another (A ⊕ B).
     *
     * The sum is the point set `{a + b : a ∈ A, b ∈ B}`. Summing with a
     * `Point` is a translation, so it returns this shape's own type; two
     * bounded convex shapes sum to a @ref Convex, or to a @ref Rectangle when
     * both are rectangles. See @ref MinkowskiSummableConcept for the pairs a
     * Minkowski sum is defined for.
     *
     * @tparam OtherShape Type of the other shape.
     * @param other Shape to sum with.
     * @return The Minkowski sum, in the tightest type that represents it.
     */
    template <class OtherShape>
        requires MinkowskiSummableConcept<Shape<PointType_>, OtherShape>
    [[nodiscard]] constexpr auto minkowskiSum(const OtherShape& other) const;

    /**
     * @brief Returns the Minkowski erosion of this shape by another (A ⊖ B).
     *
     * The erosion is the point set `{x : x ⊕ B ⊆ A}`, the translations of
     * @p other that keep it inside this shape -- equivalently
     * `⋂ {A - b : b ∈ B}`. It is the morphological dual of
     * @ref minkowskiSum and is defined for the same pairs, but it is **not**
     * commutative.
     *
     * Eroding by a `Point` is the translation by its negation, so it returns
     * this shape's own type; the other pairs come back as the convex region
     * they are, a @ref HalfplaneIntersection, which holds a lower-dimensional
     * erosion and the empty one as readily as a two-dimensional one.
     * The pair of stored alternatives decides, and only at run time.
     *
     * Eroding by a shape that covers no point is the whole plane, which a
     * @ref HalfplaneIntersection returns and the tighter result types cannot.
     *
     * @tparam OtherShape Type of the shape to erode by.
     * @param other Shape to erode by.
     * @return The erosion, in the tightest type that represents it.
     */
    template <class OtherShape>
        requires MinkowskiSummableConcept<Shape<PointType_>, OtherShape>
    [[nodiscard]] constexpr auto minkowskiErosion(const OtherShape& other) const;

    /**
     * @brief Returns the pair of points realizing the distance, nothing when the
     *        shapes meet.
     *
     * The first point lies on this shape and the second on @p other.
     *
     * @throws unsupported_operation when the pair has no `closestPoints`.
     */
    template <class ResultNumber = division_result_t<NumberType>, AnyShapeConcept Other>
    [[nodiscard]] constexpr std::optional<std::array<Point<ResultNumber, LabelType>, 2>>
    closestPoints(const Other& other) const {
        return detail::closestPointsAny<ResultNumber>(*this, other);
    }

    /**
     * @brief Returns the pair of elements realizing the distance, nothing when
     *        the shapes meet.
     *
     * @throws unsupported_operation when the pair has no `closestSegments`.
     */
    template <class ResultNumber = NumberType, AnyShapeConcept Other>
    [[nodiscard]] constexpr std::optional<std::array<Segment<Point<ResultNumber, LabelType>>, 2>>
    closestSegments(const Other& other) const {
        return detail::closestSegmentsAny<ResultNumber>(*this, other);
    }

    /**
     * @brief Returns the convex hull of the wrapped shape, as the held object's
     *        own `convexHull()`.
     *
     * @throws unsupported_operation for an alternative with no convex hull --
     *   the empty shape, the unbounded alternatives and a `Disk` -- and for a
     *   `HalfplaneIntersection` with a vertex off the lattice of an integral
     *   @p ResultNumber.
     */
    template <class ResultNumber = NumberType>
    [[nodiscard]] Convex<Point<ResultNumber, LabelType>> convexHull() const {
        using Result = Convex<Point<ResultNumber, LabelType>>;
        return visit([this](const auto& value) -> Result {
            using S = std::remove_cvref_t<decltype(value)>;
            if constexpr (HalfplaneIntersectionConcept<S>) {
                if (value.empty() || !value.isBounded()) {
                    throw unsupported_operation("convexHull", detail::shapeName<S>);
                }
                return Result(vertices<ResultNumber>());
            } else if constexpr (requires { value.convexHull(); }) {
                return Result(value.convexHull());
            } else {
                throw unsupported_operation("convexHull", detail::shapeName<S>);
            }
        });
    }

    /**
     * @brief Returns the wrapped region as a `PolygonWithHoles`.
     *
     * A stored `PolygonWithHoles` is returned as it is; every other alternative
     * answers its own `asPolygonWithHoles()`.
     *
     * @throws unsupported_operation for an alternative with no such
     *   conversion.
     */
    [[nodiscard]] PolygonWithHoles<PointType> asPolygonWithHoles() const {
        return visit([](const auto& value) -> PolygonWithHoles<PointType> {
            using S = std::remove_cvref_t<decltype(value)>;
            if constexpr (PolygonWithHolesConcept<S>) {
                return value;
            } else if constexpr (requires { value.asPolygonWithHoles(); }) {
                return value.asPolygonWithHoles();
            } else {
                throw unsupported_operation("asPolygonWithHoles", detail::shapeName<S>);
            }
        });
    }

    /**
     * @name Conversions to another shape
     *
     * Each answers the held object's conversion of the same name, joining
     * @ref asPolygonWithHoles. A stored alternative that is already the target
     * type is returned as it is.
     */
    ///@{
#define PGL_SHAPE_FORWARD_CONVERSION(Name, Target, Concept, Supported)                    \
    /** @brief The wrapped shape as a Target. @throws unsupported_operation for an         \
        alternative other than Supported. */                                              \
    [[nodiscard]] constexpr Target<PointType> Name() const {                              \
        return visit([](const auto& value) -> Target<PointType> {                         \
            using S = std::remove_cvref_t<decltype(value)>;                                \
            if constexpr (Concept<S>) {                                                    \
                return value;                                                             \
            } else if constexpr (requires { value.Name(); }) {                             \
                return value.Name();                                                      \
            } else {                                                                       \
                throw unsupported_operation(#Name, detail::shapeName<S>);                 \
            }                                                                             \
        });                                                                               \
    }

    PGL_SHAPE_FORWARD_CONVERSION(asLine, Line, LineConcept,
                                 Segment OrientedSegment Line OrientedLine Ray Halfplane)
    PGL_SHAPE_FORWARD_CONVERSION(asOrientedLine, OrientedLine, OrientedLineConcept,
                                 OrientedSegment OrientedLine Ray Halfplane)
    PGL_SHAPE_FORWARD_CONVERSION(asPolyline, Polyline, PolylineConcept,
                                 Segment MonotoneChain Polyline)
    PGL_SHAPE_FORWARD_CONVERSION(asPolygon, Polygon, PolygonConcept,
                                 Rectangle Triangle Convex Polygon)
    PGL_SHAPE_FORWARD_CONVERSION(asPolygonSet, PolygonSet, PolygonSetConcept,
                                 Rectangle Triangle Convex Polygon PolygonWithHoles PolygonSet)
    PGL_SHAPE_FORWARD_CONVERSION(asHalfplaneIntersection, HalfplaneIntersection,
                                 HalfplaneIntersectionConcept,
                                 Point Segment Line Halfplane Rectangle Triangle Convex
                                 HalfplaneIntersection)

#undef PGL_SHAPE_FORWARD_CONVERSION
    ///@}

    /**
     * @brief Returns the wrapped shape as a `Convex`.
     *
     * @tparam ResultNumber Coordinate type of the result, which a
     *   `HalfplaneIntersection` needs because its vertices are implicit.
     * @throws unsupported_operation for an alternative other than `Rectangle`,
     *   `Triangle`, `Convex` and `HalfplaneIntersection`.
     */
    template <class ResultNumber = NumberType>
    [[nodiscard]] constexpr Convex<Point<ResultNumber, LabelType>> asConvex() const {
        using Result = Convex<Point<ResultNumber, LabelType>>;
        return visit([](const auto& value) -> Result {
            using S = std::remove_cvref_t<decltype(value)>;
            if constexpr (ConvexConcept<S>) {
                return Result(value);
            } else if constexpr (requires { value.template asConvex<ResultNumber>(); }) {
                return Result(value.template asConvex<ResultNumber>());
            } else if constexpr (requires { value.asConvex(); }) {
                return Result(value.asConvex());
            } else {
                throw unsupported_operation("asConvex", detail::shapeName<S>);
            }
        });
    }

    /**
     * @name Derived linear shapes
     *
     * The half-planes an oriented or unoriented linear shape bounds, as the held
     * object names them.
     */
    ///@{
#define PGL_SHAPE_FORWARD_HALFPLANE(Name, Supported)                                      \
    /** @brief The wrapped shape's Name. @throws unsupported_operation for an              \
        alternative other than Supported. */                                              \
    [[nodiscard]] constexpr Halfplane<PointType> Name() const {                            \
        return visit([](const auto& value) -> Halfplane<PointType> {                       \
            if constexpr (requires { value.Name(); }) {                                    \
                return value.Name();                                                      \
            } else {                                                                       \
                throw unsupported_operation(#Name,                                        \
                                            detail::shapeName<std::remove_cvref_t<decltype(value)>>); \
            }                                                                             \
        });                                                                               \
    }

    PGL_SHAPE_FORWARD_HALFPLANE(halfplaneAbove, Line OrientedLine Ray)
    PGL_SHAPE_FORWARD_HALFPLANE(halfplaneBelow, Line OrientedLine Ray)
    PGL_SHAPE_FORWARD_HALFPLANE(leftHalfplane, OrientedSegment OrientedLine Ray)
    PGL_SHAPE_FORWARD_HALFPLANE(rightHalfplane, OrientedSegment OrientedLine Ray)

#undef PGL_SHAPE_FORWARD_HALFPLANE
    ///@}

    /**
     * @brief Returns the wrapped shape with its orientation reversed.
     *
     * The held object's own `opposite()`, which is again its own type, so the
     * result is a `Shape` holding the same alternative.
     *
     * @throws unsupported_operation for an alternative other than
     *   `OrientedSegment`, `OrientedLine`, `Ray` and `Halfplane`, the four that
     *   carry an orientation.
     */
    [[nodiscard]] constexpr Shape opposite() const {
        return visit([](const auto& value) -> Shape {
            if constexpr (requires { value.opposite(); }) {
                return Shape(value.opposite());
            } else {
                throw unsupported_operation("opposite",
                                           detail::shapeName<std::remove_cvref_t<decltype(value)>>);
            }
        });
    }

    /**
     * @brief Returns the circumscribed disk of the wrapped shape.
     *
     * @throws unsupported_operation for an alternative other than `Rectangle`
     *   and `Triangle`.
     */
    [[nodiscard]] constexpr Disk<PointType, NoLabel> circumcircle() const {
        return visit([](const auto& value) -> Disk<PointType, NoLabel> {
            if constexpr (requires { value.circumcircle(); }) {
                return value.circumcircle();
            } else {
                throw unsupported_operation("circumcircle",
                                           detail::shapeName<std::remove_cvref_t<decltype(value)>>);
            }
        });
    }

    /**
     * @name Duality
     *
     * The dual and polar images, which swap a point for a line and a line for a
     * point. Both come back as a `Shape`, since which of the two it is depends
     * on the stored alternative, and both default to @ref division_result_t for
     * that reason: the `Line` direction divides where the `Point` direction does
     * not. See @ref duality.
     */
    ///@{
#define PGL_SHAPE_FORWARD_DUAL(Name)                                                      \
    /** @brief The wrapped shape's Name image, a line for a point and a point for a        \
        line. @throws unsupported_operation for an alternative other than Point and        \
        Line. */                                                                          \
    template <class ResultNumber = division_result_t<NumberType>>                          \
    [[nodiscard]] constexpr Shape<Point<ResultNumber, LabelType>> Name() const {           \
        using Result = Shape<Point<ResultNumber, LabelType>>;                              \
        return visit([](const auto& value) -> Result {                                     \
            if constexpr (requires { value.template Name<ResultNumber>(); }) {             \
                return Result(value.template Name<ResultNumber>());                        \
            } else {                                                                       \
                throw unsupported_operation(#Name,                                        \
                                            detail::shapeName<std::remove_cvref_t<decltype(value)>>); \
            }                                                                             \
        });                                                                               \
    }

    PGL_SHAPE_FORWARD_DUAL(dual)
    PGL_SHAPE_FORWARD_DUAL(polar)

#undef PGL_SHAPE_FORWARD_DUAL
    ///@}

    /**
     * @brief Cuts the wrapped region into `Convex` pieces with disjoint
     *        interiors.
     *
     * Defined out of line, since it builds a @ref Triangulation.
     *
     * @throws unsupported_operation for an alternative other than `Polygon`,
     *   `PolygonWithHoles` and `PolygonSet`.
     */
    [[nodiscard]] std::vector<Convex<PointType>> convexPartition() const;

    /**
     * @brief Returns an irredundant covering of the wrapped region by `Convex`
     *        pieces, which may overlap.
     *
     * @throws unsupported_operation for an alternative other than `Polygon`,
     *   `PolygonWithHoles` and `PolygonSet`.
     */
    [[nodiscard]] std::vector<Convex<PointType>> convexCovering() const;

    /**
     * @brief Returns the constrained Delaunay triangulation of the wrapped
     *        region.
     *
     * @throws unsupported_operation for an alternative other than `Polygon`,
     *   `PolygonWithHoles` and `PolygonSet`.
     */
    [[nodiscard]] auto triangulation() const;

    /**
     * @brief Returns the wrapped rectilinear region rasterized into a
     *        @ref BitMatrix.
     *
     * @tparam ResultNumber Integer type of the grid.
     * @throws unsupported_operation for an alternative other than `Polygon`,
     *   `PolygonWithHoles` and `PolygonSet`.
     * @throws std::logic_error when the region is not rectilinear, as the
     *   concrete shapes do.
     */
    template <class ResultNumber = grid_number_t<NumberType>>
        requires(std::signed_integral<ResultNumber>)
    [[nodiscard]] auto asBitMatrix() const;

    // -------------------------------------------------------------------------
    // Transformations

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
        visit([&translation](auto& alternative) { alternative += translation; });
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
        visit([&translation](auto& alternative) { alternative -= translation; });
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
        requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
    constexpr Shape& operator*=(const Scalar& scalar) {
        visit([&scalar](auto& alternative) { alternative *= scalar; });
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
        requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
    constexpr Shape& operator/=(const Scalar& scalar) {
        visit([&scalar](auto& alternative) { alternative /= scalar; });
        return *this;
    }

    /**
     * @brief Returns the wrapped shape rotated by 90k degrees around the origin.
     *
     * @param k Number of 90-degree CCW rotations (may be negative).
     * @return Rotated shape, preserving the stored alternative type.
     */
    [[nodiscard]] constexpr Shape rotated90(int k = 1) const {
        return visit([k](const auto& value) -> Shape { return Shape(value.rotated90(k)); });
    }

    /**
     * @brief Rotates the wrapped shape by 90k degrees around the origin in place.
     *
     * @param k Number of 90-degree CCW rotations (may be negative).
     */
    constexpr void rotate90(int k = 1) {
        visit([k](auto& value) { value.rotate90(k); });
    }

    /**
     * @brief Returns the wrapped shape with its x-coordinates scaled up.
     *
     * @throws unsupported_operation for the `Disk` alternative, whose result
     *   would be an ellipse.
     */
    template <class OtherNumber>
    [[nodiscard]] constexpr Shape scaledUpX(const OtherNumber scalar) const {
        return visit([scalar](const auto& value) -> Shape {
            if constexpr (requires { value.scaledUpX(scalar); }) {
                return Shape(value.scaledUpX(scalar));
            } else {
                throw unsupported_operation("scaledUpX", detail::shapeName<std::remove_cvref_t<decltype(value)>>);
            }
        });
    }

    /**
     * @brief Scales the wrapped shape's x-coordinates up in place.
     *
     * @throws unsupported_operation for the `Disk` alternative.
     */
    template <class OtherNumber>
    constexpr void scaleUpX(const OtherNumber scalar) {
        visit([scalar](auto& value) {
            if constexpr (requires { value.scaleUpX(scalar); }) {
                value.scaleUpX(scalar);
            } else {
                throw unsupported_operation("scaleUpX", detail::shapeName<std::remove_cvref_t<decltype(value)>>);
            }
        });
    }

    /**
     * @brief Returns the wrapped shape with its y-coordinates scaled up.
     *
     * @throws unsupported_operation for the `Disk` alternative, whose result
     *   would be an ellipse.
     */
    template <class OtherNumber>
    [[nodiscard]] constexpr Shape scaledUpY(const OtherNumber scalar) const {
        return visit([scalar](const auto& value) -> Shape {
            if constexpr (requires { value.scaledUpY(scalar); }) {
                return Shape(value.scaledUpY(scalar));
            } else {
                throw unsupported_operation("scaledUpY", detail::shapeName<std::remove_cvref_t<decltype(value)>>);
            }
        });
    }

    /**
     * @brief Scales the wrapped shape's y-coordinates up in place.
     *
     * @throws unsupported_operation for the `Disk` alternative.
     */
    template <class OtherNumber>
    constexpr void scaleUpY(const OtherNumber scalar) {
        visit([scalar](auto& value) {
            if constexpr (requires { value.scaleUpY(scalar); }) {
                value.scaleUpY(scalar);
            } else {
                throw unsupported_operation("scaleUpY", detail::shapeName<std::remove_cvref_t<decltype(value)>>);
            }
        });
    }

    /**
     * @brief Returns the wrapped shape with its x-coordinates scaled down.
     *
     * @throws unsupported_operation for the `Disk` alternative, whose result
     *   would be an ellipse.
     */
    template <class OtherNumber>
    [[nodiscard]] constexpr Shape scaledDownX(const OtherNumber scalar) const {
        return visit([scalar](const auto& value) -> Shape {
            if constexpr (requires { value.scaledDownX(scalar); }) {
                return Shape(value.scaledDownX(scalar));
            } else {
                throw unsupported_operation("scaledDownX", detail::shapeName<std::remove_cvref_t<decltype(value)>>);
            }
        });
    }

    /**
     * @brief Scales the wrapped shape's x-coordinates down in place.
     *
     * @throws unsupported_operation for the `Disk` alternative.
     */
    template <class OtherNumber>
    constexpr void scaleDownX(const OtherNumber scalar) {
        visit([scalar](auto& value) {
            if constexpr (requires { value.scaleDownX(scalar); }) {
                value.scaleDownX(scalar);
            } else {
                throw unsupported_operation("scaleDownX", detail::shapeName<std::remove_cvref_t<decltype(value)>>);
            }
        });
    }

    /**
     * @brief Returns the wrapped shape with its y-coordinates scaled down.
     *
     * @throws unsupported_operation for the `Disk` alternative, whose result
     *   would be an ellipse.
     */
    template <class OtherNumber>
    [[nodiscard]] constexpr Shape scaledDownY(const OtherNumber scalar) const {
        return visit([scalar](const auto& value) -> Shape {
            if constexpr (requires { value.scaledDownY(scalar); }) {
                return Shape(value.scaledDownY(scalar));
            } else {
                throw unsupported_operation("scaledDownY", detail::shapeName<std::remove_cvref_t<decltype(value)>>);
            }
        });
    }

    /**
     * @brief Scales the wrapped shape's y-coordinates down in place.
     *
     * @throws unsupported_operation for the `Disk` alternative.
     */
    template <class OtherNumber>
    constexpr void scaleDownY(const OtherNumber scalar) {
        visit([scalar](auto& value) {
            if constexpr (requires { value.scaleDownY(scalar); }) {
                value.scaleDownY(scalar);
            } else {
                throw unsupported_operation("scaleDownY", detail::shapeName<std::remove_cvref_t<decltype(value)>>);
            }
        });
    }

    // -------------------------------------------------------------------------
    // Comparison

    /**
     * @brief Compares wrapped values: equal when the same alternative holds an
     *        equal value.
     *
     * A `Rectangle` and a `Convex` with the same point set compare unequal; use
     * @ref samePointSet to compare point sets.
     */
    constexpr bool operator==(const Shape&) const = default;

    /**
     * @brief Orders wrapped values by the underlying variant ordering: by
     *        stored alternative first, then by value.
     */
    constexpr auto operator<=>(const Shape&) const = default;

  private:
    // The geometric isPoint of one alternative; see isPoint().
    template <class S>
    static constexpr bool isPointOf(const S& value) {
        if constexpr (PointConcept<S>) {
            return true;
        } else if constexpr (requires { value.isPoint(); }) {
            return value.isPoint();
        } else {
            return false;
        }
    }

    // Whether the elements get(), operator[] and index() reach are vertices.
    template <class S>
    static constexpr bool indexesPoints() {
        return !(PointConcept<S> || HalfplaneIntersectionConcept<S> || PolygonWithHolesConcept<S> ||
                 PolygonSetConcept<S>);
    }

    Variant value_{};
};

#undef PGL_SHAPE_ALTERNATIVES

// Deduce the wrapper's point type from the alternatives of a result variant (or
// an optional thereof), so `Shape s = a.intersection<N>(b);` names the right
// type for a concrete pair. The point type is taken from the variant's first
// alternative; every alternative shares it.
template <class T, class... Ts>
Shape(const std::variant<T, Ts...>&) -> Shape<detail::shape_point_type_t<T>>;

template <class T, class... Ts>
Shape(const std::optional<std::variant<T, Ts...>>&) -> Shape<detail::shape_point_type_t<T>>;

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
    return shape.visit([&translation](const auto& alternative) {
        return Shape<ResultPoint>(alternative - translation);
    });
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
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr auto operator*(const Shape<PointType>& shape, const Scalar& scalar) {
    using ResultPoint = std::decay_t<decltype(std::declval<const PointType&>() * scalar)>;
    return shape.visit([&scalar](const auto& alternative) {
        return Shape<ResultPoint>(alternative * scalar);
    });
}

/** @copydoc operator*(const Shape<PointType>&, const Scalar&) */
template <class Scalar, class PointType>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
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
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr auto operator/(const Shape<PointType>& shape, const Scalar& scalar) {
    using ResultPoint = std::decay_t<decltype(std::declval<const PointType&>() / scalar)>;
    return shape.visit([&scalar](const auto& alternative) {
        return Shape<ResultPoint>(alternative / scalar);
    });
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
    shape.visit([&stream](const auto& value) { stream << value; });
    return stream;
}

}  // namespace pgl
