#pragma once

#include "algorithm/intervaltree.hpp"

/**
 * @file sortpoints.hpp
 * @brief Angular sorting of points around a center.
 *
 * Algorithm headers sit above the shape API and express reusable geometry
 * procedures in terms of the public primitives.
 */

#include <algorithm>
#include <type_traits>
#include <vector>


namespace pgl {

/**
 * @brief Sorts points counterclockwise around a center point.
 *
 * The angular order starts from the lexicographically smallest point in
 * @p points (its direction from @p p defines angle zero) and proceeds
 * counterclockwise around @p p. Points that share an angular direction are
 * tied; the tie is broken by putting the points that are farther from @p p
 * first. With this convention, connecting the sorted points in order traces a
 * simple, star-shaped polygon whose kernel contains @p p. Points equal to @p p
 * have no direction to sort by and end up last.
 *
 * The comparison relies only on the exact @ref orientationSign predicate and
 * squared distances, so it stays exact for integer coordinates.
 *
 * The points are first split by the horizontal line through @p p, which costs
 * two coordinate comparisons each. Each part then spans half a turn, where the
 * orientation sign alone already orders the directions, so the sort spends one
 * orientation predicate per comparison instead of the three that comparing
 * angles measured from the reference direction would take.
 *
 * @tparam Number Coordinate type of the points being sorted.
 * @tparam Label Label type of the points being sorted.
 * @tparam CenterNumber Coordinate type of the center point.
 * @tparam CenterLabel Label type of the center point.
 * @param points Points to reorder in place.
 * @param p Center the points are sorted around.
 */
template <class Number, class Label, class CenterNumber, class CenterLabel>
void sortAround(std::vector<Point<Number, Label>>& points,
                const Point<CenterNumber, CenterLabel>& p) {
    if (points.size() < 2)
        return;

    // A point equal to p has no direction around it, and would compare tied
    // with every other point. Park those at the end and sort the rest.
    const auto first = points.begin();
    const auto last = std::partition(first, points.end(),
                                     [&p](const auto& q) { return q != p; });
    if (last - first < 2)
        return;

    // The smallest point defines the reference direction (angle zero).
    const auto reference = *std::min_element(first, last);

    // Splits off the directions in the half turn [0, pi) measured
    // counterclockwise from the +x direction, that is the points above the
    // horizontal line through p, ties on it broken by the side of p they fall
    // on. The coordinates are promoted the way the orientation predicate
    // promotes them, so the split and the sort agree on every direction.
    using Compare = std::common_type_t<Number, CenterNumber>;
    const auto firstHalf = [&p](const auto& q) {
        const auto vertical = detail::strongOrder(detail::asNumber<Compare>(q.y()),
                                                  detail::asNumber<Compare>(p.y()));
        if (vertical != 0)
            return vertical > 0;
        return detail::strongOrder(detail::asNumber<Compare>(q.x()),
                                   detail::asNumber<Compare>(p.x())) > 0;
    };
    const auto middle = std::partition(first, last, firstHalf);

    // Neither part spans more than half a turn, so within one of them the
    // orientation sign is a consistent order on the directions.
    const auto less = [&p](const auto& a, const auto& b) {
        const auto turn = orientationSign(p, a, b);
        if (turn > 0)
            return true;   // b is counterclockwise of a, so a has the smaller angle.
        if (turn < 0)
            return false;

        // Same direction from p: the farther point comes first. The distance
        // is taken in the promoted type, so points carrying more precision
        // than the center still compare exactly.
        return p.template squaredDistance<Compare>(a) >
               p.template squaredDistance<Compare>(b);
    };
    std::sort(first, middle, less);
    std::sort(middle, last, less);

    // The points now run counterclockwise from the +x direction: rotate the
    // block sharing the reference direction to the front. The search compares
    // directions alone, so it lands on the first point of that block rather
    // than on the reference itself, which the distance tie-break may have put
    // behind others pointing the same way.
    const auto above = firstHalf(reference);
    const auto start = std::lower_bound(
        above ? first : middle, above ? middle : last, reference,
        [&p](const auto& a, const auto& b) { return orientationSign(p, a, b) > 0; });
    std::rotate(first, start, last);
}

namespace detail {

// Recursive median Hilbert sort (CGAL's median policy): split the range into the
// four quadrants the Hilbert curve visits, using nested medians, then recurse
// into each quadrant with the rotated/reflected curve state. `xAxis` selects the
// primary split axis; `upX`/`upY` give the curve's direction along each axis.
// Only the two coordinate comparators are used, so it is exact for any numeric
// type and never constructs intermediate coordinates.
template <class RandomIt, class LessX, class LessY>
void hilbertSortMedian(RandomIt begin, RandomIt end, bool xAxis, bool upX, bool upY,
                       const LessX& lessX, const LessY& lessY) {
    if (end - begin <= 1) {
        return;
    }

    const RandomIt m0 = begin, m4 = end;
    const RandomIt m2 = m0 + (m4 - m0) / 2;  // primary-axis median of the whole range
    const RandomIt m1 = m0 + (m2 - m0) / 2;  // secondary-axis median of the lower half
    const RandomIt m3 = m2 + (m4 - m2) / 2;  // secondary-axis median of the upper half

    // Rearranges [first,last) so *mid is its median under `less`, ascending when
    // `up`, descending otherwise.
    const auto split = [](RandomIt first, RandomIt mid, RandomIt last, const auto& less,
                          bool up) {
        if (up) {
            std::nth_element(first, mid, last, less);
        } else {
            std::nth_element(first, mid, last,
                             [&less](const auto& a, const auto& b) { return less(b, a); });
        }
    };

    if (xAxis) {
        split(m0, m2, m4, lessX, upX);
        split(m0, m1, m2, lessY, upY);
        split(m2, m3, m4, lessY, !upY);
    } else {
        split(m0, m2, m4, lessY, upY);
        split(m0, m1, m2, lessX, upX);
        split(m2, m3, m4, lessX, !upX);
    }

    hilbertSortMedian(m0, m1, !xAxis, upY, upX, lessX, lessY);
    hilbertSortMedian(m1, m2, xAxis, upX, upY, lessX, lessY);
    hilbertSortMedian(m2, m3, xAxis, upX, upY, lessX, lessY);
    hilbertSortMedian(m3, m4, !xAxis, !upY, !upX, lessX, lessY);
}

}  // namespace detail

/**
 * @brief Sorts points along a Hilbert space-filling curve.
 *
 * Reorders @p points in place so that points close together in the plane are
 * close together in the sequence. This spatial coherence makes the order a
 * useful preprocessing step for incremental algorithms — for example, inserting
 * points in Hilbert order keeps each point-location walk short.
 *
 * The order is produced by the median policy: the set is recursively split into
 * four quadrants by nested medians, following the Hilbert curve's recursive
 * structure. Only coordinate comparisons are used, so the result is exact for
 * integer coordinates and well defined for any numeric type.
 *
 * @tparam Number Coordinate type of the points.
 * @tparam Label Label type of the points.
 * @param points Points to reorder in place.
 */
template <class Number, class Label>
void hilbertSort(std::vector<Point<Number, Label>>& points) {
    const auto lessX = [](const Point<Number, Label>& a, const Point<Number, Label>& b) {
        return a.x() < b.x();
    };
    const auto lessY = [](const Point<Number, Label>& a, const Point<Number, Label>& b) {
        return a.y() < b.y();
    };
    detail::hilbertSortMedian(points.begin(), points.end(), true, false, false, lessX, lessY);
}

}  // namespace pgl
