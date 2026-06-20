#pragma once

#include "pgl.hpp"

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
 * @brief Promoted coordinate type used by orientation predicates.
 */
template <class AX, class BX, class CX>
using orientation_coordinate_t = detail::promoted_number_t<std::common_type_t<AX, BX, CX>>;
template <class AX, class BX>
using dot_coordinate_t = detail::promoted_number_t<std::common_type_t<AX, BX>>;
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
    using Coordinate = detail::orientation_coordinate_t<ANumber, BNumber, CNumber>;

    const auto abx = static_cast<Coordinate>(b.x()) - static_cast<Coordinate>(a.x());
    const auto aby = static_cast<Coordinate>(b.y()) - static_cast<Coordinate>(a.y());
    const auto acx = static_cast<Coordinate>(c.x()) - static_cast<Coordinate>(a.x());
    const auto acy = static_cast<Coordinate>(c.y()) - static_cast<Coordinate>(a.y());

    return abx * acy == aby * acx;
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
    using Coordinate = detail::dot_coordinate_t<ANumber, BNumber>;

    const auto dx1 = static_cast<Coordinate>(a2.x()) - static_cast<Coordinate>(a1.x());
    const auto dy1 = static_cast<Coordinate>(a2.y()) - static_cast<Coordinate>(a1.y());
    const auto dx2 = static_cast<Coordinate>(b2.x()) - static_cast<Coordinate>(b1.x());
    const auto dy2 = static_cast<Coordinate>(b2.y()) - static_cast<Coordinate>(b1.y());

    return dx1 * dy2 == dy1 * dx2;
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
    using Coordinate = detail::dot_coordinate_t<ANumber, BNumber>;

    const auto x = static_cast<Coordinate>(a.x()) * static_cast<Coordinate>(b.x());
    const auto y = static_cast<Coordinate>(a.y()) * static_cast<Coordinate>(b.y());

    return detail::threeWay(x, -y);
}

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
