#pragma once

#include "algorithm/sortpoints.hpp"

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
 * @return Indices into `points` of the hull, in boundary order, each at most
 *         once. When every point is collinear and `keepCollinear` is set, that
 *         is all of them in lexicographic order.
 *
 * Each point is approximated once and the sign predicate reads that
 * approximation, rather than the predicate re-approximating all three of its
 * operands on every comparison -- which over an exact coordinate type is most
 * of what the scan costs.
 */
template <class Point>
std::vector<std::size_t> grahamScanIndices(const std::vector<Point> &points, bool keepCollinear) {
    using Number = typename Point::NumberType;
    std::vector<std::size_t> stack;
    if (points.empty()) {
        return stack;
    }

    std::vector<decltype(filtered<Number>(points[0]))> approximations;
    approximations.reserve(points.size());
    for (const Point &p : points) {
        approximations.push_back(filtered<Number>(p));
    }

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
    // Every point on both chains means they are all collinear, and the upper
    // chain only walks back over the lower one.
    if (keepCollinear && points.size() >= 2 && stack.size() == 2 * points.size() - 2) {
        stack.resize(points.size());
    }
    return stack;
}

/**
 * @brief The hull points @ref grahamScanIndices picks, in boundary order.
 */
template <class Point>
std::vector<Point> grahamScanOf(const std::vector<Point> &points, bool keepCollinear) {
    std::vector<Point> hull;
    const std::vector<std::size_t> indices = grahamScanIndices(points, keepCollinear);
    hull.reserve(indices.size());
    for (std::size_t i : indices) {
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

    // Coincident inputs are never hull vertices and would otherwise survive as
    // degenerate (zero-length) hull edges, so they go before the scan, not in it.
    sortDistinctPoints(points);

    return detail::grahamScanOf(points, false);
}

/**
 * @brief Computes the convex hull of a point container using Graham's scan.
 *
 * Collinear points on hull edges are kept, so the returned hull holds every
 * input point on its boundary, not only the extreme vertices. When every input
 * point is collinear, each is listed once, in lexicographic order.
 *
 * @tparam Container Container whose value type is a pgl point type.
 * @param points_ Input points.
 * @return Hull points in boundary order.
 */
template<class Container>
auto grahamScanExtended(const Container &points_) {
    std::vector points = detail::hullCandidates(points_);

    // Coincident inputs are never hull vertices and would otherwise survive as
    // degenerate (zero-length) hull edges, so they go before the scan, not in it.
    sortDistinctPoints(points);

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
 * When every input point is collinear, each is listed once, in lexicographic
 * order.
 *
 * @tparam Container Container whose value type is a pgl point type.
 * @param points_ Input points.
 * @return Hull points in boundary order.
 */
template<class Container>
auto convexHullExtended (const Container &points_) {
    return grahamScanExtended(points_);
}

/**
 * @brief Computes the convex layers of a point container.
 *
 * The first layer is every input point on the boundary of the convex hull,
 * vertices and points on edge interiors alike; each later layer is the same
 * for the points left once the layers before it are removed. Each layer is in
 * counterclockwise order starting from its lexicographically smallest point,
 * as @ref convexHullExtended returns it. Coincident input points count once.
 *
 * The points are sorted once; every layer is then a scan over the points
 * still left, which removing a layer keeps in sorted order.
 *
 * Complexity: O(n log n + n L) for n input points and L layers.
 *
 * @tparam Container Container whose value type is a pgl point type.
 * @param points_ Input points.
 * @return The layers, outermost first.
 */
template<class Container>
auto convexLayers(const Container &points_) {
    using Point = std::remove_cvref_t<decltype(*std::begin(points_))>;
    std::vector<Point> points(std::begin(points_), std::end(points_));
    sortDistinctPoints(points);

    std::vector<std::vector<Point>> layers;
    std::vector<bool> onLayer;
    while (!points.empty()) {
        const std::vector<std::size_t> indices = detail::grahamScanIndices(points, /*keepCollinear=*/true);
        onLayer.assign(points.size(), false);
        std::vector<Point> layer;
        layer.reserve(indices.size());
        for (std::size_t i : indices) {
            onLayer[i] = true;
            layer.push_back(points[i]);
        }
        layers.push_back(std::move(layer));

        std::size_t kept = 0;
        for (std::size_t i = 0; i < points.size(); ++i) {
            if (!onLayer[i]) {
                points[kept++] = std::move(points[i]);
            }
        }
        points.erase(points.begin() + static_cast<std::ptrdiff_t>(kept), points.end());
    }
    return layers;
}

namespace detail {

/**
 * @brief The hull @ref grahamScan returns, for a cyclic sequence whose
 *        x-coordinates are cyclically bitonic, plus a few loose points.
 *
 * A cyclic sequence is x-bitonic when, cut at a smallest and at a largest
 * x-coordinate, both arcs between the cuts run weakly monotone in x. The
 * vertices of a convex polygon in boundary order are, and so is any
 * subsequence of them, and both stay so under any map that is weakly monotone
 * in x -- a scaling, a truncating division, or the rounding of a conversion.
 * The two arcs then merge into x-order in linear time, and of the points
 * sharing an x-coordinate only the lowest and the highest can be hull
 * vertices, which leaves the input of the scan sorted and distinct without a
 * sort.
 *
 * The bitonicity is checked, not assumed: a sequence that fails it is hulled by
 * @ref grahamScan, so the result is that function's either way.
 *
 * Complexity: O(n + e log e) for n ring points and e loose points when the
 * ring is x-bitonic, and that of @ref grahamScan over the n + e points
 * otherwise.
 *
 * @param ring Points in cyclic order.
 * @param loose Points in no particular order.
 * @return The hull vertices, lexicographically smallest first and
 *         counterclockwise, exactly as @ref grahamScan returns them.
 */
template <class Point>
std::vector<Point> hullOfXBitonicRing(const std::vector<Point>& ring, std::vector<Point> loose) {
    const std::size_t n = ring.size();
    const auto fallback = [&] {
        std::vector<Point> all(ring);
        all.insert(all.end(), loose.begin(), loose.end());
        return grahamScan(all);
    };

    std::size_t lowest = 0, highest = 0;
    for (std::size_t i = 1; i < n; ++i) {
        if (ring[i].x() < ring[lowest].x()) {
            lowest = i;
        }
        if (ring[highest].x() < ring[i].x()) {
            highest = i;
        }
    }
    // Arc A runs forward from `lowest` to `highest`, arc B backward; both hold
    // the two cut points, and when they coincide B is the whole cycle.
    const std::size_t lengthA = n == 0 ? 0 : (highest + n - lowest) % n + 1;
    const std::size_t lengthB = n == 0 ? 0 : n + 2 - lengthA;
    const auto arcA = [&](std::size_t k) -> const Point& { return ring[(lowest + k) % n]; };
    const auto arcB = [&](std::size_t k) -> const Point& { return ring[(lowest + n - k % n) % n]; };
    for (std::size_t k = 1; k < lengthA; ++k) {
        if (arcA(k).x() < arcA(k - 1).x()) {
            return fallback();
        }
    }
    for (std::size_t k = 1; k < lengthB; ++k) {
        if (arcB(k).x() < arcB(k - 1).x()) {
            return fallback();
        }
    }
    std::sort(loose.begin(), loose.end(), [](const Point& p, const Point& q) { return p.x() < q.x(); });

    std::vector<Point> sorted;
    sorted.reserve(lengthA + lengthB + loose.size());
    const Point* low = nullptr;
    const Point* high = nullptr;
    const auto flush = [&] {
        if (low != nullptr) {
            sorted.push_back(*low);
            if (low->y() < high->y()) {
                sorted.push_back(*high);
            }
        }
    };
    const auto take = [&](const Point& p) {
        if (low != nullptr && p.x() == low->x()) {
            if (p.y() < low->y()) {
                low = &p;
            }
            if (high->y() < p.y()) {
                high = &p;
            }
        } else {
            flush();
            low = high = &p;
        }
    };

    std::size_t a = 0, b = 0, l = 0;
    while (a < lengthA || b < lengthB || l < loose.size()) {
        const Point* next = nullptr;
        int from = 0;
        if (a < lengthA) {
            next = &arcA(a);
            from = 0;
        }
        if (b < lengthB && (next == nullptr || arcB(b).x() < next->x())) {
            next = &arcB(b);
            from = 1;
        }
        if (l < loose.size() && (next == nullptr || loose[l].x() < next->x())) {
            next = &loose[l];
            from = 2;
        }
        take(*next);
        (from == 0 ? a : from == 1 ? b : l) += 1;
    }
    flush();

    return grahamScanOf(sorted, false);
}

}  // namespace detail

} // namespace pgl
