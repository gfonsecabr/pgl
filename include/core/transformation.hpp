#pragma once

#include "core/rational.hpp"

/**
 * @file transformation.hpp
 * @brief Public declaration of pgl::Transformation, an affine transformation.
 *
 * Transformation stores a 2x3 matrix mapping (x,y) to (a*x+b*y+tx, c*x+d*y+ty).
 * It is applied to a point or shape with `operator*` and composed with another
 * transformation with the same operator, so `t1 * t2 * shape` both composes
 * and applies left to right. Application to every shape but `Rectangle` and
 * `Disk` is defined in implementation/transformations.hpp, once every shape is
 * visible: a general affine map turns a rectangle into a parallelogram and a
 * disk into an ellipse, neither of which those two classes can represent, so
 * no such overload exists and applying one is a compile error.
 */

#include <array>
#include <cmath>
#include <concepts>
#include <type_traits>

namespace pgl {

/**
 * @brief Affine transformation of the plane, stored as a 2x3 matrix.
 *
 * @tparam Number Matrix entry / coordinate type.
 */
template <class Number>
struct Transformation {
    /** Type of the matrix entries. */
    using NumberType = Number;

    /**
     * @brief Creates the identity transformation.
     */
    constexpr Transformation() = default;

    /**
     * @brief Creates a transformation from its matrix entries.
     *
     * @param a Row 0, column 0 of the linear part.
     * @param b Row 0, column 1 of the linear part.
     * @param c Row 1, column 0 of the linear part.
     * @param d Row 1, column 1 of the linear part.
     * @param tx Translation added to the first coordinate.
     * @param ty Translation added to the second coordinate.
     */
    constexpr Transformation(Number a, Number b, Number c, Number d,
                              Number tx = Number{}, Number ty = Number{})
        : m_{{{a, b, tx}, {c, d, ty}}} {}

    /**
     * @brief Converts a transformation with a different entry type.
     *
     * @tparam OtherNumber Entry type of the source transformation.
     * @param other Source transformation.
     */
    template <class OtherNumber>
    constexpr Transformation(const Transformation<OtherNumber>& other)
        : m_{{
              {static_cast<Number>(other.a()), static_cast<Number>(other.b()), static_cast<Number>(other.tx())},
              {static_cast<Number>(other.c()), static_cast<Number>(other.d()), static_cast<Number>(other.ty())},
          }} {}

    /**
     * @brief Returns the identity transformation.
     */
    static constexpr Transformation identity() {
        return Transformation();
    }

    /**
     * @brief Returns a translation by (dx, dy).
     */
    static constexpr Transformation translation(Number dx, Number dy) {
        return Transformation(Number{1}, Number{}, Number{}, Number{1}, dx, dy);
    }

    /**
     * @brief Returns a non-uniform scaling around the origin.
     *
     * @param sx Factor applied to the x coordinate.
     * @param sy Factor applied to the y coordinate.
     */
    static constexpr Transformation scaling(Number sx, Number sy) {
        return Transformation(sx, Number{}, Number{}, sy);
    }

    /**
     * @brief Returns a uniform scaling around the origin.
     *
     * @param s Factor applied to both coordinates.
     */
    static constexpr Transformation scaling(Number s) {
        return scaling(s, s);
    }

    /**
     * @brief Returns a rotation around the origin by `90k` degrees.
     *
     * Exact for every @ref NumberType since the matrix entries are 0, 1, or -1.
     *
     * @param k Number of 90-degree CCW rotations (may be negative).
     */
    static constexpr Transformation rotation90(int k = 1) {
        switch (((k % 4) + 4) % 4) {
            case 1:  return Transformation(Number{}, -Number{1}, Number{1}, Number{});
            case 2:  return Transformation(-Number{1}, Number{}, Number{}, -Number{1});
            case 3:  return Transformation(Number{}, Number{1}, -Number{1}, Number{});
            default: return identity();
        }
    }

    /**
     * @brief Returns a horizontal shear: `(x, y) -> (x + k*y, y)`.
     */
    static constexpr Transformation shearX(Number k) {
        return Transformation(Number{1}, k, Number{}, Number{1});
    }

    /**
     * @brief Returns a vertical shear: `(x, y) -> (x, y + k*x)`.
     */
    static constexpr Transformation shearY(Number k) {
        return Transformation(Number{1}, Number{}, k, Number{1});
    }

    /**
     * @brief Returns a reflection across the x-axis.
     */
    static constexpr Transformation reflectionX() {
        return Transformation(Number{1}, Number{}, Number{}, -Number{1});
    }

    /**
     * @brief Returns a reflection across the y-axis.
     */
    static constexpr Transformation reflectionY() {
        return Transformation(-Number{1}, Number{}, Number{}, Number{1});
    }

    /**
     * @brief Returns a rotation around the origin by an arbitrary angle.
     *
     * Unlike @ref rotation90, an arbitrary angle is generally irrational, so
     * this requires an explicit floating-point @p ResultNumber: there is no
     * exact-by-default overload to silently fall back from.
     *
     * @tparam ResultNumber Floating-point matrix entry type of the result.
     * @param radians Rotation angle in radians.
     */
    template <std::floating_point ResultNumber = double>
    static Transformation<ResultNumber> rotation(ResultNumber radians) {
        const ResultNumber cosine = std::cos(radians);
        const ResultNumber sine = std::sin(radians);
        return Transformation<ResultNumber>(cosine, -sine, sine, cosine);
    }

    /** @brief Returns the row-0, column-0 matrix entry. */
    constexpr Number a() const { return m_[0][0]; }
    /** @brief Returns the row-0, column-1 matrix entry. */
    constexpr Number b() const { return m_[0][1]; }
    /** @brief Returns the row-1, column-0 matrix entry. */
    constexpr Number c() const { return m_[1][0]; }
    /** @brief Returns the row-1, column-1 matrix entry. */
    constexpr Number d() const { return m_[1][1]; }
    /** @brief Returns the translation added to the first coordinate. */
    constexpr Number tx() const { return m_[0][2]; }
    /** @brief Returns the translation added to the second coordinate. */
    constexpr Number ty() const { return m_[1][2]; }

    /**
     * @brief Returns the determinant of the linear (non-translation) part.
     *
     * Negative when the transformation reverses orientation (a reflection or
     * an odd number of shears/reflections composed together), zero when it
     * collapses the plane onto a line or point.
     */
    constexpr auto determinant() const {
        return a() * d() - b() * c();
    }

    /**
     * @brief Tests whether the transformation has an inverse.
     *
     * @return `true` when @ref determinant is nonzero.
     */
    constexpr bool isInvertible() const {
        return determinant() != decltype(determinant()){};
    }

    /**
     * @brief Returns the inverse transformation.
     *
     * @tparam ResultNumber Matrix entry type of the result (default: @ref NumberType).
     * @warning Divides by @ref determinant. For an integral @ref NumberType this
     * is inexact unless @p ResultNumber is a type such as `Rational<Number>`
     * that represents the division exactly.
     */
    template <class ResultNumber = Number>
    constexpr Transformation<ResultNumber> inverse() const {
        const ResultNumber det = static_cast<ResultNumber>(determinant());
        const ResultNumber ra = static_cast<ResultNumber>(a());
        const ResultNumber rb = static_cast<ResultNumber>(b());
        const ResultNumber rc = static_cast<ResultNumber>(c());
        const ResultNumber rd = static_cast<ResultNumber>(d());
        const ResultNumber rtx = static_cast<ResultNumber>(tx());
        const ResultNumber rty = static_cast<ResultNumber>(ty());
        return Transformation<ResultNumber>(
            rd / det, -rb / det,
            -rc / det, ra / det,
            (rb * rty - rd * rtx) / det,
            (rc * rtx - ra * rty) / det);
    }

    /**
     * @brief Composes two transformations: `(*this) * other` applies @p other
     * first, then `*this`.
     *
     * @tparam OtherNumber Entry type of @p other.
     */
    template <class OtherNumber>
    constexpr auto operator*(const Transformation<OtherNumber>& other) const {
        using ResultNumber = std::common_type_t<Number, OtherNumber>;
        return Transformation<ResultNumber>(
            a() * other.a() + b() * other.c(),
            a() * other.b() + b() * other.d(),
            c() * other.a() + d() * other.c(),
            c() * other.b() + d() * other.d(),
            a() * other.tx() + b() * other.ty() + tx(),
            c() * other.tx() + d() * other.ty() + ty());
    }

    /**
     * @brief Compares the matrix entries for equality.
     */
    constexpr bool operator==(const Transformation&) const = default;

  private:
    std::array<std::array<Number, 3>, 2> m_{{{Number{1}, Number{}, Number{}}, {Number{}, Number{1}, Number{}}}};
};

/**
 * @brief Applies a transformation to any supported shape.
 *
 * Defined for every shape except `Rectangle` and `Disk`: a general affine map
 * turns a rectangle into a parallelogram and a disk into an ellipse, and
 * neither class can represent that, so no overload is provided for them.
 *
 * @tparam Number Entry type of @p transformation.
 * @tparam ShapeT Shape type; must not be `Rectangle` or `Disk`.
 * @param transformation Transformation to apply.
 * @param shape Shape to transform.
 * @return The transformed shape, with a coordinate type promoted via
 * `std::common_type_t<Number, ShapeT::NumberType>`.
 */
template <class Number, class ShapeT>
    requires (detail::shapeRank<ShapeT> >= 0 && !RectangleConcept<ShapeT> && !DiskConcept<ShapeT>)
[[nodiscard]] constexpr auto operator*(const Transformation<Number>& transformation, const ShapeT& shape);

/**
 * @brief Applies a transformation to the polymorphic @ref Shape wrapper.
 *
 * @throws std::logic_error if the wrapped alternative is `Rectangle` or `Disk`.
 */
template <class Number, ShapeConcept ShapeT>
[[nodiscard]] constexpr auto operator*(const Transformation<Number>& transformation, const ShapeT& shape);

}  // namespace pgl
