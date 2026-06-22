#pragma once

#include "shape/point.hpp"

/**
 * @file emptyshape.hpp
 * @brief Public declaration of pgl::EmptyShape.
 *
 * EmptyShape models the empty set of points in the plane. It is a full-fledged
 * shape alternative so it can be stored in @ref pgl::Shape, but every predicate
 * is vacuously `false`, it has no vertices, and any intersection with it is
 * again empty. Because it answers every relation generically, it can be added
 * to the Shape variant without special-casing it anywhere else.
 */

#include <compare>
#include <cstddef>
#include <ostream>
#include <stdexcept>
#include <type_traits>
#include <utility>


namespace pgl {

/**
 * @brief The empty set of points in the plane.
 *
 * @tparam PointType_ Point type the shape is nominally parameterized on; only
 * its associated types are used, since an empty shape stores no coordinates.
 */
template <class PointType_ = Point<>>
struct EmptyShape {
    using PointType = PointType_;
    using NumberType = typename PointType::NumberType;
    using LabelType = typename PointType::LabelType;

    /**
     * @brief Creates the (unique) empty shape.
     */
    constexpr EmptyShape() = default;

    /**
     * @brief All empty shapes are equal and unordered among themselves.
     */
    [[nodiscard]] constexpr bool operator==(const EmptyShape&) const = default;
    /** @brief Three-way comparison: all empty shapes are equivalent. */
    [[nodiscard]] constexpr auto operator<=>(const EmptyShape&) const = default;

    /**
     * @brief Returns the number of vertices, always `0`.
     */
    [[nodiscard]] static constexpr std::size_t size() {
        return 0;
    }

    /**
     * @brief The empty shape is never degenerate.
     */
    [[nodiscard]] constexpr bool isDegenerate() const {
        return false;
    }

    /**
     * @brief Always throws: the empty shape has no vertices.
     */
    [[nodiscard]] PointType get(std::ptrdiff_t) const {
        throw std::logic_error("EmptyShape::get: the empty shape has no vertices");
    }

    /**
     * @brief Always throws: the empty shape has no vertices.
     */
    [[nodiscard]] PointType operator[](std::size_t) const {
        throw std::logic_error("EmptyShape::operator[]: the empty shape has no vertices");
    }

    /**
     * @brief Always throws: the empty shape has no vertices.
     */
    template <class T>
    [[nodiscard]] std::ptrdiff_t index(const T&) const {
        throw std::logic_error("EmptyShape::index: the empty shape has no vertices");
    }

    // The empty set is a subset of every shape, so it contains only the empty
    // set itself: containment of any non-empty shape is false, while containment
    // of another empty shape is true.

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template <class T>
    [[nodiscard]] constexpr bool contains(const T&) const {
        return false;
    }
    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template <PointConcept OtherPoint>
    [[nodiscard]] constexpr bool contains(const EmptyShape<OtherPoint>&) const {
        return true;
    }

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template <class T>
    [[nodiscard]] constexpr bool boundaryContains(const T&) const {
        return false;
    }
    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template <PointConcept OtherPoint>
    [[nodiscard]] constexpr bool boundaryContains(const EmptyShape<OtherPoint>&) const {
        return true;
    }

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template <class T>
    [[nodiscard]] constexpr bool interiorContains(const T&) const {
        return false;
    }
    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template <PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorContains(const EmptyShape<OtherPoint>&) const {
        return true;
    }

    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template <class T>
    [[nodiscard]] constexpr bool intersects(const T&) const {
        return false;
    }

    /** @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅). */
    template <class T>
    [[nodiscard]] constexpr bool interiorsIntersect(const T&) const {
        return false;
    }

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template <class T>
    [[nodiscard]] constexpr bool separates(const T&) const {
        return false;
    }

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template <class T>
    [[nodiscard]] constexpr bool crosses(const T&) const {
        return false;
    }

    /** @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint. */
    template <class ResultNumber = NumberType, class T>
    [[nodiscard]] constexpr EmptyShape intersection(const T&) const {
        return EmptyShape{};
    }

    /**
     * @brief Translates the empty shape in place; a no-op.
     *
     * The empty set has no points to move, so translation leaves it unchanged.
     *
     * @return This shape, unmodified.
     */
    template <PointConcept OtherPoint>
    constexpr EmptyShape& operator+=(const OtherPoint&) {
        return *this;
    }

    /**
     * @brief Translates the empty shape in place by a negated point; a no-op.
     *
     * The empty set has no points to move, so translation leaves it unchanged.
     *
     * @return This shape, unmodified.
     */
    template <PointConcept OtherPoint>
    constexpr EmptyShape& operator-=(const OtherPoint&) {
        return *this;
    }

    /**
     * @brief Scales the empty shape in place around the origin; a no-op.
     *
     * The empty set has no points to scale, so it is left unchanged.
     *
     * @return This shape, unmodified.
     */
    template <class Scalar>
        requires(!detail::is_point_v<Scalar>)
    constexpr EmptyShape& operator*=(const Scalar&) {
        return *this;
    }

    /**
     * @brief Divides the empty shape in place around the origin; a no-op.
     *
     * The empty set has no points to scale, so it is left unchanged.
     *
     * @return This shape, unmodified.
     */
    template <class Scalar>
        requires(!detail::is_point_v<Scalar>)
    constexpr EmptyShape& operator/=(const Scalar&) {
        return *this;
    }

    /** @brief Returns the empty shape rotated by 90k degrees; a no-op. */
    [[nodiscard]] constexpr EmptyShape rotated90(int = 1) const {
        return *this;
    }

    /** @brief Rotates the empty shape by 90k degrees in place; a no-op. */
    constexpr void rotate90(int = 1) {}

    /** @brief Returns the empty shape with its x-coordinates scaled up; a no-op. */
    template <class OtherNumber>
    [[nodiscard]] constexpr EmptyShape scaledUpX(const OtherNumber) const {
        return *this;
    }

    /** @brief Scales the empty shape's x-coordinates up in place; a no-op. */
    template <class OtherNumber>
    constexpr void scaleUpX(const OtherNumber) {}

    /** @brief Returns the empty shape with its y-coordinates scaled up; a no-op. */
    template <class OtherNumber>
    [[nodiscard]] constexpr EmptyShape scaledUpY(const OtherNumber) const {
        return *this;
    }

    /** @brief Scales the empty shape's y-coordinates up in place; a no-op. */
    template <class OtherNumber>
    constexpr void scaleUpY(const OtherNumber) {}

    /** @brief Returns the empty shape with its x-coordinates scaled down; a no-op. */
    template <class OtherNumber>
    [[nodiscard]] constexpr EmptyShape scaledDownX(const OtherNumber) const {
        return *this;
    }

    /** @brief Scales the empty shape's x-coordinates down in place; a no-op. */
    template <class OtherNumber>
    constexpr void scaleDownX(const OtherNumber) {}

    /** @brief Returns the empty shape with its y-coordinates scaled down; a no-op. */
    template <class OtherNumber>
    [[nodiscard]] constexpr EmptyShape scaledDownY(const OtherNumber) const {
        return *this;
    }

    /** @brief Scales the empty shape's y-coordinates down in place; a no-op. */
    template <class OtherNumber>
    constexpr void scaleDownY(const OtherNumber) {}
};

/**
 * @brief Translates the empty shape by a point; a no-op.
 *
 * The empty set has no points to move, so it is returned unchanged, over the
 * promoted point type to mirror the coordinate promotion of the other shapes'
 * translation.
 *
 * @return The empty shape over the promoted point type.
 */
template <class PointType, class TranslationNumber, class TranslationLabel>
constexpr auto operator+(const EmptyShape<PointType>&,
                         const Point<TranslationNumber, TranslationLabel>&) {
    using ResultPoint = std::decay_t<decltype(std::declval<const PointType&>() +
                                              std::declval<const Point<TranslationNumber, TranslationLabel>&>())>;
    return EmptyShape<ResultPoint>{};
}

/** @copydoc operator+(const EmptyShape<PointType>&, const Point<TranslationNumber, TranslationLabel>&) */
template <class TranslationNumber, class TranslationLabel, class PointType>
constexpr auto operator+(const Point<TranslationNumber, TranslationLabel>& translation,
                         const EmptyShape<PointType>& empty) {
    return empty + translation;
}

/**
 * @brief Translates the empty shape by a negated point; a no-op.
 *
 * The empty set has no points to move, so it is returned unchanged, over the
 * promoted point type to mirror the coordinate promotion of the other shapes'
 * translation.
 *
 * @return The empty shape over the promoted point type.
 */
template <class PointType, class TranslationNumber, class TranslationLabel>
constexpr auto operator-(const EmptyShape<PointType>&,
                         const Point<TranslationNumber, TranslationLabel>&) {
    using ResultPoint = std::decay_t<decltype(std::declval<const PointType&>() -
                                              std::declval<const Point<TranslationNumber, TranslationLabel>&>())>;
    return EmptyShape<ResultPoint>{};
}

/**
 * @brief Scales the empty shape around the origin; a no-op.
 *
 * The empty set has no points to scale, so it is returned unchanged, over the
 * promoted point type to mirror the coordinate promotion of the other shapes'
 * scaling.
 *
 * @return The empty shape over the promoted point type.
 */
template <class PointType, class Scalar>
    requires(!detail::is_point_v<Scalar>)
constexpr auto operator*(const EmptyShape<PointType>&, const Scalar&) {
    using ResultPoint = std::decay_t<decltype(std::declval<const PointType&>() *
                                              std::declval<const Scalar&>())>;
    return EmptyShape<ResultPoint>{};
}

/** @copydoc operator*(const EmptyShape<PointType>&, const Scalar&) */
template <class Scalar, class PointType>
    requires(!detail::is_point_v<Scalar>)
constexpr auto operator*(const Scalar& scalar, const EmptyShape<PointType>& empty) {
    return empty * scalar;
}

/**
 * @brief Divides the empty shape around the origin; a no-op.
 *
 * The empty set has no points to scale, so it is returned unchanged, over the
 * promoted point type to mirror the coordinate promotion of the other shapes'
 * scaling.
 *
 * @return The empty shape over the promoted point type.
 */
template <class PointType, class Scalar>
    requires(!detail::is_point_v<Scalar>)
constexpr auto operator/(const EmptyShape<PointType>&, const Scalar&) {
    using ResultPoint = std::decay_t<decltype(std::declval<const PointType&>() /
                                              std::declval<const Scalar&>())>;
    return EmptyShape<ResultPoint>{};
}

/**
 * @brief Streams the empty shape.
 *
 * @param stream Output stream.
 * @return The output stream.
 */
template <class PointType>
std::ostream& operator<<(std::ostream& stream, const EmptyShape<PointType>&) {
    return stream << "EmptyShape()";
}

}  // namespace pgl
