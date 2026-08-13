#pragma once

#include "shape/emptyshape.hpp"

/**
 * @file orientation.hpp
 * @brief Exact low-level orientation and incircle predicates.
 *
 * These are the primitive signed tests that the rest of the library builds on
 * for collinearity, side tests, and robust ordering decisions.
 */

#include <compare>
#include <type_traits>


namespace pgl {

namespace detail {

/**
 * @brief Promoted coordinate type the sign predicates evaluate in.
 *
 * One promotion above the common type of the operands, which is what makes the
 * degree-two products below exact for every coordinate the operands can hold.
 * Callers that have *already* promoted must not route through these predicates
 * with the promoted type: a second promotion turns an `int64_t` sweep into
 * @ref pgl::BigInt arithmetic. Pass the original points and let the predicate
 * promote once.
 */
template <class... Numbers>
using sign_coordinate_t = detail::promoted_number_t<std::common_type_t<Numbers...>>;

template <class AX, class BX, class CX>
using orientation_coordinate_t = sign_coordinate_t<AX, BX, CX>;
template <class AX, class BX>
using dot_coordinate_t = sign_coordinate_t<AX, BX>;

/**
 * @brief The sign of a three-way comparison result, as `-1`, `0` or `1`.
 *
 * `unordered` — which the sign predicates only ever return for a NaN
 * coordinate — collapses to `0`, matching the hand-written sign ladders this
 * replaces.
 */
template <class Ordering>
constexpr int signOf(const Ordering& order) {
    return order > 0 ? 1 : (order < 0 ? -1 : 0);
}
}  // namespace detail

/**
 * @brief Returns the signed orientation determinant of three points.
 *
 * @param a First point.
 * @param b Second point.
 * @param c Third point.
 * @return Determinant of vectors `ab` and `ac`.
 */
template <class ANumber, class ALabel, class BNumber, class BLabel, class CNumber, class CLabel>
constexpr auto orientationDeterminant(
    const Point<ANumber, ALabel>& a,
    const Point<BNumber, BLabel>& b,
    const Point<CNumber, CLabel>& c) {
    using Coordinate = detail::orientation_coordinate_t<ANumber, BNumber, CNumber>;

    const auto abx = static_cast<Coordinate>(b.x()) - static_cast<Coordinate>(a.x());
    const auto aby = static_cast<Coordinate>(b.y()) - static_cast<Coordinate>(a.y());
    const auto acx = static_cast<Coordinate>(c.x()) - static_cast<Coordinate>(a.x());
    const auto acy = static_cast<Coordinate>(c.y()) - static_cast<Coordinate>(a.y());

    return abx * acy - aby * acx;
}

/**
 * @brief Classifies the orientation of three points.
 *
 * Returns negative for clockwise order, positive for counterclockwise order,
 * and equivalence when the points are collinear.
 *
 * @param a First point.
 * @param b Second point.
 * @param c Third point.
 * @return Orientation sign of `(a,b,c)`.
 */
template <class ANumber, class ALabel, class BNumber, class BLabel, class CNumber, class CLabel>
constexpr std::partial_ordering orientationSign(
    const Point<ANumber, ALabel> &a,
    const Point<BNumber, BLabel> &b,
    const Point<CNumber, CLabel> &c) {
    using Coordinate = detail::orientation_coordinate_t<ANumber, BNumber, CNumber>;

    const auto abx = static_cast<Coordinate>(b.x()) - static_cast<Coordinate>(a.x());
    const auto aby = static_cast<Coordinate>(b.y()) - static_cast<Coordinate>(a.y());
    const auto acx = static_cast<Coordinate>(c.x()) - static_cast<Coordinate>(a.x());
    const auto acy = static_cast<Coordinate>(c.y()) - static_cast<Coordinate>(a.y());

    return detail::threeWay(abx * acy, aby * acx);
}

/**
 * @brief Classifies the turn from one vector to another.
 *
 * The sign of the 2D cross product `u x v`: positive when @p v lies
 * counterclockwise from @p u within half a turn, negative when it lies
 * clockwise, and equivalence when the two are parallel or one is zero. This is
 * @ref orientationSign in vector form — `orientationSign(a, b, c)` is
 * `crossSign(b - a, c - a)` — and is what the many call sites that already hold
 * *directions* rather than point triples need.
 *
 * @param u First vector.
 * @param v Second vector.
 * @return Cross-product sign of `(u, v)`.
 */
template <class UNumber, class ULabel, class VNumber, class VLabel>
constexpr std::partial_ordering crossSign(
    const Point<UNumber, ULabel>& u,
    const Point<VNumber, VLabel>& v) {
    using Coordinate = detail::sign_coordinate_t<UNumber, VNumber>;

    return detail::threeWay(static_cast<Coordinate>(u.x()) * static_cast<Coordinate>(v.y()),
                            static_cast<Coordinate>(u.y()) * static_cast<Coordinate>(v.x()));
}

/**
 * @brief Classifies the turn from the direction `a -> b` to the direction `p -> q`.
 *
 * The sign of `(b - a) x (q - p)`, formed with a single promotion so that a
 * caller holding two directions as four points never has to build — and
 * possibly overflow — the difference vectors itself.
 *
 * @param a Tail of the first direction.
 * @param b Head of the first direction.
 * @param p Tail of the second direction.
 * @param q Head of the second direction.
 * @return Cross-product sign of the two directions.
 */
template <class ANumber, class ALabel, class BNumber, class BLabel,
          class PNumber, class PLabel, class QNumber, class QLabel>
constexpr std::partial_ordering crossSign(
    const Point<ANumber, ALabel>& a,
    const Point<BNumber, BLabel>& b,
    const Point<PNumber, PLabel>& p,
    const Point<QNumber, QLabel>& q) {
    using Coordinate = detail::sign_coordinate_t<ANumber, BNumber, PNumber, QNumber>;

    const auto abx = static_cast<Coordinate>(b.x()) - static_cast<Coordinate>(a.x());
    const auto aby = static_cast<Coordinate>(b.y()) - static_cast<Coordinate>(a.y());
    const auto pqx = static_cast<Coordinate>(q.x()) - static_cast<Coordinate>(p.x());
    const auto pqy = static_cast<Coordinate>(q.y()) - static_cast<Coordinate>(p.y());

    return detail::threeWay(abx * pqy, aby * pqx);
}

/**
 * @brief Tests whether three points are collinear.
 *
 * @param a First point.
 * @param b Second point.
 * @param c Third point.
 * @return `true` if the orientation determinant vanishes.
 */
template <class ANumber, class ALabel, class BNumber, class BLabel, class CNumber, class CLabel>
constexpr bool collinear(
    const Point<ANumber, ALabel>& a,
    const Point<BNumber, BLabel>& b,
    const Point<CNumber, CLabel>& c) {
    return orientationSign(a, b, c) == 0;
}

/**
 * @brief Tests whether the directions `a1 -> a2` and `b1 -> b2` are parallel.
 *
 * Compares the 2D cross product of the two direction vectors against zero, so
 * it reports true for parallel and anti-parallel directions alike (and when
 * either direction is degenerate). The two endpoints of each direction must
 * share a point type; the two directions may use different point types.
 *
 * @param a1 Tail of the first direction.
 * @param a2 Head of the first direction.
 * @param b1 Tail of the second direction.
 * @param b2 Head of the second direction.
 * @return `true` if the two direction vectors are parallel.
 */
template <class ANumber, class ALabel, class BNumber, class BLabel>
constexpr bool sameDirection(
    const Point<ANumber, ALabel>& a1,
    const Point<ANumber, ALabel>& a2,
    const Point<BNumber, BLabel>& b1,
    const Point<BNumber, BLabel>& b2) {
    return crossSign(a1, a2, b1, b2) == 0;
}

/**
 * @brief Tells if the angle between two vectors is acute, right, or obtuse.
 * @param a First vector.
 * @param b Second vector.
 * @return Negative for obtuse angle, positive for acute angle, and equivalence for right angle.
 */
template <class ANumber, class ALabel, class BNumber, class BLabel>
constexpr std::partial_ordering dotSign(
    const Point<ANumber, ALabel>& a,
    const Point<BNumber, BLabel>& b) {
    using Coordinate = detail::sign_coordinate_t<ANumber, BNumber>;

    const auto x = static_cast<Coordinate>(a.x()) * static_cast<Coordinate>(b.x());
    const auto y = static_cast<Coordinate>(a.y()) * static_cast<Coordinate>(b.y());

    return detail::threeWay(x, -y);
}

/**
 * @brief Tells if the angle between the directions `a -> b` and `p -> q` is
 *        acute, right, or obtuse.
 *
 * The sign of `(b - a) . (q - p)`, formed with a single promotion. Ordering
 * points along a direction is the common use: `t` comes before `u` along `d`
 * exactly when `dotSign(t, u, tail, head) > 0` for a direction `tail -> head`.
 *
 * @param a Tail of the first direction.
 * @param b Head of the first direction.
 * @param p Tail of the second direction.
 * @param q Head of the second direction.
 * @return Negative for obtuse, positive for acute, equivalence for right.
 */
template <class ANumber, class ALabel, class BNumber, class BLabel,
          class PNumber, class PLabel, class QNumber, class QLabel>
constexpr std::partial_ordering dotSign(
    const Point<ANumber, ALabel>& a,
    const Point<BNumber, BLabel>& b,
    const Point<PNumber, PLabel>& p,
    const Point<QNumber, QLabel>& q) {
    using Coordinate = detail::sign_coordinate_t<ANumber, BNumber, PNumber, QNumber>;

    const auto abx = static_cast<Coordinate>(b.x()) - static_cast<Coordinate>(a.x());
    const auto aby = static_cast<Coordinate>(b.y()) - static_cast<Coordinate>(a.y());
    const auto pqx = static_cast<Coordinate>(q.x()) - static_cast<Coordinate>(p.x());
    const auto pqy = static_cast<Coordinate>(q.y()) - static_cast<Coordinate>(p.y());

    return detail::threeWay(abx * pqx, -(aby * pqy));
}

namespace detail {

/**
 * @brief A `double` approximation of an exact quantity, with an error bound.
 *
 * `value` is whatever double arithmetic produced; `error` bounds how far that
 * can be from the exact result, so the exact value lies in
 * `[value - error, value + error]`. Chaining the operators below propagates
 * both, which turns a floating-point evaluation of a polynomial into a proof
 * of its sign whenever `|value| > error`.
 */
struct Approximate {
    double value = 0.0;  ///< The floating-point evaluation.
    double error = 0.0;  ///< Bound on `|value - exact|`.
};

/**
 * @brief Absolute value of a double, usable in a constant expression.
 *
 * `std::abs` only became `constexpr` in C++23 and these predicates must stay
 * evaluable at compile time under C++20.
 */
constexpr double approximateAbs(double value) { return value < 0.0 ? -value : value; }

/**
 * @brief Relative slack charged for the rounding of one double operation.
 *
 * A correctly rounded double operation lands within 2^-53 of the exact result;
 * the constant is one binade looser so that the rounding committed while
 * *computing* the error terms — they are ordinary double arithmetic too, and
 * every one of them is a sum of non-negative quantities, so each can only be
 * short by a relative 2^-53 — is absorbed along the way.
 */
inline constexpr double approximateRoundoff = 0x1p-52;

/**
 * @brief Slack for a single implementation-defined narrowing to double.
 *
 * A conversion from a wide integer picks one of the two doubles adjacent to the
 * exact value, i.e. lands within one ulp.
 */
inline constexpr double approximateNarrowing = 0x1p-52;

/**
 * @brief Slack for a conversion to double that may round more than once.
 *
 * @ref BigInt reaches double by accumulating its base-2^62 limbs in
 * `long double` and narrowing the total, and a @ref Rational divides two such
 * conversions, so the rounding compounds: once per limb, once per narrowing,
 * once for the quotient. Charging 2^-45 leaves room for some 250 limbs, and
 * that on the pessimistic assumption that `long double` is no wider than
 * `double` — a magnitude around 2^15000, well past any coordinate a geometric
 * computation arrives at. Spending less slack than this buys nothing: the gap
 * only starts to matter for near-degenerate inputs at coordinate magnitudes
 * above 2^36 or so, and even there it decides all but a few percent.
 */
inline constexpr double approximateConversion = 0x1p-45;

/**
 * @brief Final safety factor applied to an accumulated error bound.
 */
inline constexpr double approximateMargin = 1.0 + 0x1p-20;

/**
 * @brief Sum of two approximations, widening the bound by the rounding it costs.
 */
constexpr Approximate operator+(const Approximate& a, const Approximate& b) {
    const double value = a.value + b.value;
    return {value, a.error + b.error + approximateRoundoff * approximateAbs(value)};
}

/**
 * @brief Difference of two approximations, widening the bound by the rounding it costs.
 */
constexpr Approximate operator-(const Approximate& a, const Approximate& b) {
    const double value = a.value - b.value;
    return {value, a.error + b.error + approximateRoundoff * approximateAbs(value)};
}

/**
 * @brief Product of two approximations, widening the bound by the rounding it costs.
 *
 * `(x + dx)(y + dy) - xy` is bounded by `|x| |dy| + |y| |dx| + |dx| |dy|`, to
 * which the product's own rounding is added.
 */
constexpr Approximate operator*(const Approximate& a, const Approximate& b) {
    const double value = a.value * b.value;
    return {value,
            approximateAbs(a.value) * b.error + approximateAbs(b.value) * a.error +
                a.error * b.error + approximateRoundoff * approximateAbs(value)};
}

/**
 * @brief Approximates an exact rational coordinate by a double.
 *
 * The single division `Rational` narrows to is deliberately preferred over the
 * tighter bracket @ref Rational::lowerBound and @ref Rational::upperBound would
 * give: those cost two long-double divisions apiece, which on a Delaunay build
 * over `ERational` coordinates was a fifth of everything the filter saved,
 * while the bracket they buy makes no difference to how often it can decide.
 *
 * A fraction whose stored parts overflow double — easy to reach, since the
 * reduction is deferred — yields a non-finite quotient or a NaN here, which
 * propagates to an error bound the comparisons cannot satisfy. The filter then
 * abstains rather than answering from a value it never computed.
 */
template <class Int>
constexpr Approximate approximate(const pgl::Rational<Int>& value) {
    const double quotient = static_cast<double>(value);
    return {quotient, approximateConversion * approximateAbs(quotient)};
}

/**
 * @brief Approximates an exact coordinate by a double, bounding the conversion.
 */
template <class Number>
constexpr Approximate approximate(const Number& value) {
    if constexpr (std::is_floating_point_v<Number>) {
        if constexpr (numeric_limits<Number>::digits <= numeric_limits<double>::digits &&
                      numeric_limits<Number>::max_exponent <= numeric_limits<double>::max_exponent) {
            return {static_cast<double>(value), 0.0};  // float, double: exact
        } else {
            const double narrowed = static_cast<double>(value);  // long double
            return {narrowed, approximateNarrowing * approximateAbs(narrowed)};
        }
    } else if constexpr (numeric_limits<Number>::is_integer &&
                         numeric_limits<Number>::digits <= numeric_limits<double>::digits) {
        return {static_cast<double>(value), 0.0};  // int and narrower: exact
    } else if constexpr (extended_integral<Number>) {
        const double narrowed = static_cast<double>(value);  // int64_t, int128
        return {narrowed, approximateNarrowing * approximateAbs(narrowed)};
    } else {
        const double narrowed = static_cast<double>(value);  // BigInt and the like
        return {narrowed, approximateConversion * approximateAbs(narrowed)};
    }
}

/**
 * @brief Whether @ref inCircleSign should try a floating-point filter first.
 *
 * The filter is not free: it doubles the arithmetic of a plain double
 * evaluation to carry the error bound, and the near-degenerate inputs it cannot
 * settle pay for the exact determinant on top. That only comes out ahead when
 * the exact fallback is arbitrary precision — measured on a 10k-point Delaunay
 * build, `ERational` halves while a 128-bit determinant (`int` coordinates) and
 * a fixed-width `Rational` both lose 10-30%, their exact arithmetic being a few
 * machine instructions to begin with.
 *
 * @tparam Coordinate The type the exact determinant is evaluated in.
 */
template <class Coordinate>
inline constexpr bool filtersInCircle = arbitraryPrecision<Coordinate>;

/**
 * @brief Floating-point filter for @ref inCircleSign.
 *
 * Evaluates the in-circle determinant in double while carrying an error bound,
 * and reports the sign only when the bound proves it. `unordered` means the
 * filter abstained — the determinant is too close to zero to call, a coordinate
 * was NaN, or an intermediate overflowed to infinity (which poisons its own
 * error bound, so the comparisons below fail and the caller falls back). Every
 * answer it does give equals the sign of the exact determinant.
 *
 * The one gap in the reasoning is underflow: a product that lands in the
 * subnormal range breaks the relative bound the operators charge. The absolute
 * error such a product can hide is 2^-1074, and nothing downstream can amplify
 * it past the 2^-1000 floor the final comparison adds, so a determinant the
 * filter accepts is never decided by underflowed noise.
 *
 * @param a First circle point.
 * @param b Second circle point.
 * @param c Third circle point.
 * @param d Query point.
 * @return The sign of the exact determinant, or `unordered` when undecided.
 */
template <class ANumber, class ALabel, class BNumber, class BLabel, class CNumber, class CLabel, class DNumber, class DLabel>
constexpr std::partial_ordering inCircleFilter(
    const Point<ANumber, ALabel>& a,
    const Point<BNumber, BLabel>& b,
    const Point<CNumber, CLabel>& c,
    const Point<DNumber, DLabel>& d) {
    const Approximate dx = approximate(d.x());
    const Approximate dy = approximate(d.y());
    const Approximate adx = approximate(a.x()) - dx;
    const Approximate ady = approximate(a.y()) - dy;
    const Approximate bdx = approximate(b.x()) - dx;
    const Approximate bdy = approximate(b.y()) - dy;
    const Approximate cdx = approximate(c.x()) - dx;
    const Approximate cdy = approximate(c.y()) - dy;
    const Approximate abdet = adx * bdy - bdx * ady;
    const Approximate bcdet = bdx * cdy - cdx * bdy;
    const Approximate cadet = cdx * ady - adx * cdy;
    const Approximate alift = adx * adx + ady * ady;
    const Approximate blift = bdx * bdx + bdy * bdy;
    const Approximate clift = cdx * cdx + cdy * cdy;
    const Approximate det = alift * bcdet + blift * cadet + clift * abdet;

    const double bound = det.error * approximateMargin + 0x1p-1000;
    if (det.value > bound) {
        return std::partial_ordering::greater;
    }
    if (det.value < -bound) {
        return std::partial_ordering::less;
    }
    return std::partial_ordering::unordered;
}

}  // namespace detail

/**
 * @brief Returns the signed in-circle determinant of a query point.
 *
 * Points `a`, `b`, and `c` define the circumcircle; the returned value is the
 * 3x3 determinant whose rows are `(p - d)` augmented with `|p - d|^2` for
 * `p` in `{a, b, c}`. It equals `-2 * signedArea(a,b,c) * power(d)`, where
 * `power(d) = |d - center|^2 - radius^2`, so its sign classifies `d` against
 * the circle (like @ref inCircleSign) while its magnitude is the exact, scaled
 * power of `d` — the basis for division-free circle/segment predicates.
 *
 * @param a First circle point.
 * @param b Second circle point.
 * @param c Third circle point.
 * @param d Query point.
 * @return The in-circle determinant in a promoted coordinate type.
 */
template <class ANumber, class ALabel, class BNumber, class BLabel, class CNumber, class CLabel, class DNumber, class DLabel>
constexpr auto inCircleDeterminant(
    const Point<ANumber, ALabel>& a,
    const Point<BNumber, BLabel>& b,
    const Point<CNumber, CLabel>& c,
    const Point<DNumber, DLabel>& d) {
    using Coordinate = detail::promoted_number_t<detail::promoted_number_t<std::common_type_t<ANumber, BNumber, CNumber, DNumber>>>;

    const auto adx = static_cast<Coordinate>(a.x()) - static_cast<Coordinate>(d.x());
    const auto ady = static_cast<Coordinate>(a.y()) - static_cast<Coordinate>(d.y());
    const auto bdx = static_cast<Coordinate>(b.x()) - static_cast<Coordinate>(d.x());
    const auto bdy = static_cast<Coordinate>(b.y()) - static_cast<Coordinate>(d.y());
    const auto cdx = static_cast<Coordinate>(c.x()) - static_cast<Coordinate>(d.x());
    const auto cdy = static_cast<Coordinate>(c.y()) - static_cast<Coordinate>(d.y());
    const auto abdet = adx * bdy - bdx * ady;
    const auto bcdet = bdx * cdy - cdx * bdy;
    const auto cadet = cdx * ady - adx * cdy;
    const auto alift = adx * adx + ady * ady;
    const auto blift = bdx * bdx + bdy * bdy;
    const auto clift = cdx * cdx + cdy * cdy;
    return alift * bcdet + blift * cadet + clift * abdet;
}

/**
 * @brief Classifies a point with respect to the circumcircle of three others.
 *
 * Points `a`, `b`, and `c` define the circumcircle. The sign convention assumes
 * counterclockwise orientation of `(a,b,c)` and flips when that orientation is
 * reversed.
 *
 * @param a First circle point.
 * @param b Second circle point.
 * @param c Third circle point.
 * @param d Query point.
 * @return For counterclockwise `(a,b,c)`: greater inside, equivalent on the
 * boundary, less outside. The order is reversed for clockwise `(a,b,c)`.
 */
template <class ANumber, class ALabel, class BNumber, class BLabel, class CNumber, class CLabel, class DNumber, class DLabel>
constexpr std::partial_ordering inCircleSign(
    const Point<ANumber, ALabel>& a,
    const Point<BNumber, BLabel>& b,
    const Point<CNumber, CLabel>& c,
    const Point<DNumber, DLabel>& d) {
    using Coordinate = detail::promoted_number_t<detail::promoted_number_t<std::common_type_t<ANumber, BNumber, CNumber, DNumber>>>;

    // A double evaluation that carries its own error bound settles the sign for
    // all but the near-degenerate inputs, sparing the exact arithmetic below.
    // It only reports a sign it has proved, so this is a shortcut, not an
    // approximation; see @ref detail::inCircleFilter.
    if constexpr (detail::filtersInCircle<Coordinate>) {
        const std::partial_ordering filtered = detail::inCircleFilter(a, b, c, d);
        if (filtered != std::partial_ordering::unordered) {
            return filtered;
        }
    }

    const auto adx = static_cast<Coordinate>(a.x()) - static_cast<Coordinate>(d.x());
    const auto ady = static_cast<Coordinate>(a.y()) - static_cast<Coordinate>(d.y());
    const auto bdx = static_cast<Coordinate>(b.x()) - static_cast<Coordinate>(d.x());
    const auto bdy = static_cast<Coordinate>(b.y()) - static_cast<Coordinate>(d.y());
    const auto cdx = static_cast<Coordinate>(c.x()) - static_cast<Coordinate>(d.x());
    const auto cdy = static_cast<Coordinate>(c.y()) - static_cast<Coordinate>(d.y());
    const auto abdet = adx * bdy - bdx * ady;
    const auto bcdet = bdx * cdy - cdx * bdy;
    const auto cadet = cdx * ady - adx * cdy;
    const auto alift = adx * adx + ady * ady;
    const auto blift = bdx * bdx + bdy * bdy;
    const auto clift = cdx * cdx + cdy * cdy;
    return detail::threeWay(alift * bcdet + blift * cadet, -clift * abdet);
}

}  // namespace pgl
