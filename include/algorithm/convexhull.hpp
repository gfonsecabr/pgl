#ifndef PGL_HPP_INCLUDED
// Entered out of order (before pgl.hpp): defer to the umbrella header,
// which re-includes this file at the correct layer.
#include "pgl.hpp"
#else
#ifndef PGL_ALGORITHM_CONVEXHULL_HPP
#define PGL_ALGORITHM_CONVEXHULL_HPP

/**
 * @file convexhull.hpp
 * @brief Convex hull algorithms built from Pangolin point predicates.
 *
 * Algorithm headers sit above the shape API and express reusable geometry
 * procedures in terms of the public primitives.
 */

#include <vector>



namespace pgl {

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
    using Point = std::remove_cvref_t<decltype(*std::begin(points_))>;
    std::vector<Point> points(points_.begin(), points_.end());
    std::vector<Point> hull;

    if (points.empty())
        return hull;

    std::sort(points.begin(), points.end());

    // Build lower hull
    for (const auto& p : points) {
        while (hull.size() >= 2 &&
               pgl::orientationSign(hull[hull.size() - 2], hull[hull.size() - 1], p) <= 0) {
            hull.pop_back();
        }
        hull.push_back(p);
    }

    // Build upper hull
    size_t lower_size = hull.size();
    for (size_t i = points.size() - 1; i-- != 0;) {
        const auto& p = points[i];
        while (hull.size() > lower_size &&
               pgl::orientationSign(hull[hull.size() - 2], hull[hull.size() - 1], p) <= 0) {
            hull.pop_back();
        }
        hull.push_back(p);
    }

    if (hull.size() >= 2)
       hull.pop_back();

    return hull;
}

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
auto grahamScanExtended(const Container &points_) {
    using Point = std::remove_cvref_t<decltype(*std::begin(points_))>;
    std::vector<Point> points(points_.begin(), points_.end());
    std::vector<Point> hull;

    if (points.empty())
        return hull;

    std::sort(points.begin(), points.end());

    // Build lower hull
    for (const auto& p : points) {
        while (hull.size() >= 2 &&
               pgl::orientationSign(hull[hull.size() - 2], hull[hull.size() - 1], p) < 0) {
            hull.pop_back();
        }
        hull.push_back(p);
    }

    // Build upper hull
    size_t lower_size = hull.size();
    for (size_t i = points.size() - 1; i-- != 0;) {
        const auto& p = points[i];
        while (hull.size() > lower_size &&
               pgl::orientationSign(hull[hull.size() - 2], hull[hull.size() - 1], p) < 0) {
            hull.pop_back();
        }
        hull.push_back(p);
    }

    if (hull.size() >= 2)
       hull.pop_back();

    return hull;
}


/**
 * @brief Computes the convex hull of a point container.
 *
 * Collinear points on hull edges are kept.
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
 * Collinear points on hull edges are kept.
 *
 * @tparam Container Container whose value type is a pgl point type.
 * @param points_ Input points.
 * @return Hull vertices in boundary order.
 */
template<class Container>
auto convexHullExtended (const Container &points_) {
    return grahamScan(points_);
}

} // namespace pgl

#endif // PGL_ALGORITHM_CONVEXHULL_HPP
#endif // PGL_HPP_INCLUDED
