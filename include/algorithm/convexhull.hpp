#pragma once

#include "algorithm/redbluesweep.hpp"

/**
 * @file convexhull.hpp
 * @brief Convex hull algorithms built from Pangolin point predicates.
 *
 * Algorithm headers sit above the shape API and express reusable geometry
 * procedures in terms of the public primitives.
 */

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <type_traits>
#include <vector>



namespace pgl {

namespace detail {

/**
 * @brief Drops the points that cannot be on the hull, by Akl-Toussaint.
 *
 * Four input points in counterclockwise convex position span a quadrilateral
 * inscribed in the hull, so every point strictly inside it is strictly inside
 * the hull: neither a vertex nor a point of an edge, whichever of the two hulls
 * is being asked for. Over points spread through a region that leaves few of
 * them extreme, this is most of them, and what survives is all the sort and the
 * scan that follow ever see.
 *
 * The corners are the four axis extremes, but only approximately: which points
 * they are changes how much the filter drops and nothing else, so they are
 * picked on the `double` approximations. What must hold exactly is that they
 * wind counterclockwise, and that is four orientation signs, checked before any
 * point is dropped. A degenerate quadrilateral -- collinear extremes, or a
 * coordinate too large to approximate -- fails that check, and then nothing is
 * dropped and the hull is the scan's alone to find.
 */
template <class Container>
auto hullCandidates(const Container &points_) {
    using Point = std::remove_cvref_t<decltype(*std::begin(points_))>;
    using Number = typename Point::NumberType;

    // Materialized first: a shape's vertex iterator hands out points by value,
    // and an approximation holds the point it approximates by address.
    std::vector<Point> points(std::begin(points_), std::end(points_));
    // Four distinct corners are needed before the filter can drop anything, and
    // a handful of points more before the orientation signs it spends cost less
    // than the sort they save.
    if (points.size() < 16) {
        return points;
    }

    std::vector<decltype(filtered<Number>(points[0]))> approximations;
    approximations.reserve(points.size());
    for (const Point &p : points) {
        approximations.push_back(filtered<Number>(p));
    }

    std::size_t lowest = 0, rightmost = 0, highest = 0, leftmost = 0;
    ApproximatePoint first = approximationOf(approximations[0]);
    double lowY = first.y.value, highY = first.y.value;
    double lowX = first.x.value, highX = first.x.value;
    for (std::size_t i = 1; i < approximations.size(); ++i) {
        const ApproximatePoint a = approximationOf(approximations[i]);
        if (a.y.value < lowY)  { lowY = a.y.value;  lowest = i; }
        if (a.x.value > highX) { highX = a.x.value; rightmost = i; }
        if (a.y.value > highY) { highY = a.y.value; highest = i; }
        if (a.x.value < lowX)  { lowX = a.x.value;  leftmost = i; }
    }

    const std::size_t corner[4] = {lowest, rightmost, highest, leftmost};
    for (int i = 0; i < 4; ++i) {
        const auto turn = orientationSignOf(approximations[corner[i]],
                                            approximations[corner[(i + 1) % 4]],
                                            approximations[corner[(i + 2) % 4]]).value();
        if (!(turn > 0)) {
            return points;
        }
    }

    std::vector<Point> kept;
    kept.reserve(points.size() / 8 + 16);
    for (std::size_t i = 0; i < approximations.size(); ++i) {
        bool inside = true;
        for (int e = 0; e < 4 && inside; ++e) {
            inside = orientationSignOf(approximations[corner[e]],
                                       approximations[corner[(e + 1) % 4]],
                                       approximations[i]).value() > 0;
        }
        if (!inside) {
            kept.push_back(points[i]);
        }
    }
    return kept;
}

/**
 * @brief The scan both hulls run, over points already sorted and deduplicated.
 *
 * @param points Distinct points in lexicographic order.
 * @param keepCollinear Whether a point interior to a hull edge stays on the hull.
 *
 * Each point is approximated once and the sign predicate reads that
 * approximation, rather than the predicate re-approximating all three of its
 * operands on every comparison -- which over an exact coordinate type is most
 * of what the scan costs.
 */
template <class Point>
std::vector<Point> grahamScanOf(const std::vector<Point> &points, bool keepCollinear) {
    using Number = typename Point::NumberType;
    std::vector<Point> hull;
    if (points.empty()) {
        return hull;
    }

    std::vector<decltype(filtered<Number>(points[0]))> approximations;
    approximations.reserve(points.size());
    for (const Point &p : points) {
        approximations.push_back(filtered<Number>(p));
    }

    std::vector<std::size_t> stack;
    const auto turnsBack = [&](std::size_t candidate) {
        const auto sign = orientationSignOf(approximations[stack[stack.size() - 2]],
                                            approximations[stack.back()],
                                            approximations[candidate]).value();
        return keepCollinear ? sign < 0 : sign <= 0;
    };

    // Build lower hull
    for (std::size_t i = 0; i < points.size(); ++i) {
        while (stack.size() >= 2 && turnsBack(i)) {
            stack.pop_back();
        }
        stack.push_back(i);
    }

    // Build upper hull
    const std::size_t lower_size = stack.size();
    for (std::size_t i = points.size() - 1; i-- != 0;) {
        while (stack.size() > lower_size && turnsBack(i)) {
            stack.pop_back();
        }
        stack.push_back(i);
    }

    if (stack.size() >= 2) {
        stack.pop_back();
    }

    hull.reserve(stack.size());
    for (std::size_t i : stack) {
        hull.push_back(points[i]);
    }
    return hull;
}

}  // namespace detail


/**
 * @brief Computes the convex hull of a point container using Graham's scan.
 *
 * Collinear points on hull edges are discarded, so the returned hull
 * contains only the extreme vertices.
 *
 * @tparam Container Container whose value type is a pgl point type.
 * @param points_ Input points.
 * @return Hull vertices in boundary order.
 */
template<class Container>
auto grahamScan(const Container &points_) {
    std::vector points = detail::hullCandidates(points_);

    std::sort(points.begin(), points.end());
    // Drop duplicate points: coincident inputs are never hull vertices and would
    // otherwise survive as degenerate (zero-length) hull edges.
    points.erase(std::unique(points.begin(), points.end()), points.end());

    return detail::grahamScanOf(points, false);
}

/**
 * @brief Computes the convex hull of a point container using Graham's scan.
 *
 * Collinear points on hull edges are kept, so the returned hull holds every
 * input point on its boundary, not only the extreme vertices.
 *
 * @tparam Container Container whose value type is a pgl point type.
 * @param points_ Input points.
 * @return Hull points in boundary order.
 */
template<class Container>
auto grahamScanExtended(const Container &points_) {
    std::vector points = detail::hullCandidates(points_);

    std::sort(points.begin(), points.end());
    // Drop duplicate points: coincident inputs are never hull vertices and would
    // otherwise survive as degenerate (zero-length) hull edges.
    points.erase(std::unique(points.begin(), points.end()), points.end());

    return detail::grahamScanOf(points, true);
}


/**
 * @brief Computes the convex hull of a point container.
 *
 * Collinear points on hull edges are discarded, so the returned hull contains
 * only the extreme vertices. @ref convexHullExtended keeps them.
 *
 * @tparam Container Container whose value type is a pgl point type.
 * @param points_ Input points.
 * @return Hull vertices in boundary order.
 */
template<class Container>
auto convexHull(const Container &points_) {
    return grahamScan(points_);
}

/**
 * @brief Computes the convex hull of a point container.
 *
 * Collinear points on hull edges are kept, so the returned hull holds every
 * input point on its boundary. @ref convexHull returns the vertices alone.
 *
 * @tparam Container Container whose value type is a pgl point type.
 * @param points_ Input points.
 * @return Hull points in boundary order.
 */
template<class Container>
auto convexHullExtended (const Container &points_) {
    return grahamScanExtended(points_);
}

} // namespace pgl
