#pragma once

#include "pgl.hpp"

/**
 * @file duality.hpp
 * @brief Projective duality and polar-transform helpers.
 *
 * These operations connect points and lines through the dual geometry used by
 * several exact constructions in the library.
 */


namespace pgl {

// -----------------------------------------------------------------------------
// Point

/**
 * @brief Returns the projective dual line of a point.
 *
 * @tparam ResultNumber Coordinate type of the dual line.
 * @return Dual line associated with this point.
 */
template <class Number, class Label>
template <class ResultNumber>
constexpr Line<Point<ResultNumber, Label>> Point<Number, Label>::dual() const {
    return Line<Point<ResultNumber, Label>>(0, -y(), 1, x() - y());
}

/**
 * @brief Returns the polar line of a point with respect to the unit parabola model used here.
 *
 * The origin has no polar in this construction.
 *
 * @tparam ResultNumber Coordinate type of the polar line.
 * @return Polar line of this point.
 */
template <class Number, class Label>
template <class ResultNumber>
constexpr Line<Point<ResultNumber, Label>> Point<Number, Label>::polar() const {
    assert(x() != 0 || y() != 0);
    // Build two lines through this point that do not contain the origin.
    Point<ResultNumber, Label> p{x(), y()};
    Point<ResultNumber, Label> q1{x() + 1, y() + 1};
    if (p.x() * q1.y() - p.y() * q1.x() == 0) {
        q1.y() = y() - 1;
    }
    Point<ResultNumber, Label> q2{x() + 1, y()};
    if (p.x() * q2.y() - p.y() * q2.x() == 0) {
        q2.y() = y() - 1;
    }
    Line<Point<ResultNumber, Label>> l1{p, q1}, l2{p, q2};
    return Line<Point<ResultNumber, Label>>(l1.polar(), l2.polar());
}

// -----------------------------------------------------------------------------
// Line

/**
 * @brief Returns normalized dual-line coordinates for the supporting line.
 *
 * @tparam ResultNumber Numeric type of the returned tuple.
 * @return Tuple `(a_num, b_num, den)` representing `y = (a_num/den)x - b_num/den`.
 */
template <class PointType>
template <class ResultNumber>
constexpr auto Line<PointType>::dualCoordinates() const {
    ResultNumber qx = static_cast<ResultNumber>(min().x());
    ResultNumber qy = static_cast<ResultNumber>(min().y());
    ResultNumber px = static_cast<ResultNumber>(max().x());
    ResultNumber py = static_cast<ResultNumber>(max().y());

    ResultNumber den = px - qx;
    ResultNumber anum = py - qy;
    ResultNumber bnum = anum * px - py * den;
    return std::make_tuple(anum, bnum, den);
}

/**
 * @brief Returns normalized polar-line coordinates for the supporting line.
 *
 * @tparam ResultNumber Numeric type of the returned tuple.
 * @return Tuple `(a_num, b_num, den)` representing `(a_num/den)x + (b_num/den)y = 1`.
 */
template <class PointType>
template <class ResultNumber>
constexpr auto Line<PointType>::polarCoordinates() const {
    ResultNumber qx = static_cast<ResultNumber>(min().x());
    ResultNumber qy = static_cast<ResultNumber>(min().y());
    ResultNumber px = static_cast<ResultNumber>(max().x());
    ResultNumber py = static_cast<ResultNumber>(max().y());

    ResultNumber den = px * qy - py * qx;
    ResultNumber anum = qy - py;
    ResultNumber bnum = px - qx;
    return std::make_tuple(anum, bnum, den);
}

/**
 * @brief Returns the projective dual point of a non-vertical line.
 *
 * @tparam ResultNumber Coordinate type of the dual point.
 * @return Dual point of this line.
 */
template <class PointType>
template <class ResultNumber>
constexpr Point<ResultNumber, typename PointType::LabelType> Line<PointType>::dual() const {
    assert(!isVertical());

    // Line equation: y = anum/den * x - bnum/den.
    const auto [anum, bnum, den] = dualCoordinates<ResultNumber>();
    return Point<ResultNumber, typename PointType::LabelType>(anum / den, bnum / den);
}

/**
 * @brief Returns the polar point of a line in the chosen dual model.
 *
 * @tparam ResultNumber Coordinate type of the polar point.
 * @return Polar point of this line.
 */
template <class PointType>
template <class ResultNumber>
constexpr Point<ResultNumber, typename PointType::LabelType> Line<PointType>::polar() const {
    // Line equation: anum/den * x + bnum/den y = 1.
    const auto [anum, bnum, den] = polarCoordinates<ResultNumber>();
    assert(den != 0);
    return Point<ResultNumber, typename PointType::LabelType>(anum / den, bnum / den);
}

}  // namespace pgl
