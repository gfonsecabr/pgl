#pragma once

#include "pgl.hpp"

/**
 * @file sortaround.hpp
 * @brief Angular sorting of points around a center.
 *
 * Algorithm headers sit above the shape API and express reusable geometry
 * procedures in terms of the public primitives.
 */

#include <algorithm>
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
 * simple, star-shaped polygon whose kernel contains @p p.
 *
 * The comparison relies only on the exact @ref orientationSign predicate and
 * squared distances, so it stays exact for integer coordinates.
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

    // The smallest point defines the reference direction (angle zero).
    const auto reference = *std::min_element(points.begin(), points.end());

    // Classifies a point into the first angular half [0, pi) -> 0 or the
    // second half [pi, 2pi) -> 1, measured counterclockwise from the
    // reference direction. The half is the primary sort key, so a point at
    // angle zero (the reference direction) always precedes a point at angle pi.
    const auto half = [&reference, &p](const auto& q) -> int {
        const auto side = orientationSign(p, reference, q);
        if (side > 0)
            return 0;
        if (side < 0)
            return 1;
        // Collinear with the reference ray: split angle 0 from angle pi by the
        // sign of the dot product between the two directions.
        return (reference - p) * (q - p) >= 0 ? 0 : 1;
    };

    std::sort(points.begin(), points.end(),
              [&](const auto& a, const auto& b) {
        const int halfA = half(a);
        const int halfB = half(b);
        if (halfA != halfB)
            return halfA < halfB;

        // Same angular half: order by counterclockwise angle around p.
        const auto turn = orientationSign(p, a, b);
        if (turn > 0)
            return true;   // b is counterclockwise of a, so a has the smaller angle.
        if (turn < 0)
            return false;

        // Same direction from p: the farther point comes first.
        return p.squaredDistance(a) > p.squaredDistance(b);
    });
}

}  // namespace pgl
