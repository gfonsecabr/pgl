#pragma once

#include "implementation/minkowskierosion.hpp"

/**
 * @file emptypolygons.hpp
 * @brief Empty triangles of a point set.
 *
 * A triangle is *empty* with respect to a point set when its three vertices
 * belong to the set and no other point of the set lies in the closed triangle:
 * a point on an edge blocks it as much as a point inside. Only non-degenerate
 * triangles count; three collinear points never make an empty triangle.
 *
 * Every function takes the points as any container whose value type is a pgl
 * point type, and treats it as a set: coincident points count once.
 */

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <type_traits>
#include <utility>
#include <vector>

namespace pgl {

namespace detail {

template <class Container>
using empty_polygons_point_t =
    std::remove_cvref_t<decltype(*std::begin(std::declval<const Container&>()))>;

// A container the empty-polygon functions accept: a range of pgl points.
template <class Container>
concept EmptyPolygonsPointRange =
    requires(const Container& points) {
        std::begin(points);
        std::end(points);
    } && is_point_v<empty_polygons_point_t<Container>>;

// Whether a and b, both collinear with p and different from it, leave p in the
// same direction. Along a line the lexicographic order is monotone, so that is
// whether they fall on the same side of p in it; no arithmetic is needed.
template <class PointType>
bool onSameRay(const PointType& p, const PointType& a, const PointType& b) {
    return orientationSign(p, a, b) == 0 && ((p < a) == (p < b));
}

/**
 * @brief Visits every empty triangle `p q r` with `q` and `r` in @p around.
 *
 * Emptiness is with respect to @p around together with @p p, which must hold
 * distinct points none equal to @p p; the vector is reordered and shrunk.
 *
 * This is the fan scan of Dobkin, Edelsbrunner and Overmars ("Searching for
 * empty convex polygons", 1990). Sorted around @p p, the points `p_0 .. p_m-1`
 * make a fan, and `p p_i p_j` is empty exactly when every point sorted between
 * them lies strictly beyond the segment `p_i p_j`. Such a pair either is
 * consecutive or has some `p_k` between it with `p p_i p_k` and `p p_k p_j`
 * both empty and a strict left turn `p_i p_k p_j` -- take the `p_k` nearest to
 * the line `p_i p_j`. The scan builds the pairs in increasing order of their
 * second index from that rule alone. Each index keeps the pairs ending at it in
 * a queue, in the order their angle at it turns, so the ones that extend to a
 * new second index are a prefix of it, and each pair is dequeued once.
 *
 * Only the nearest point of each ray from @p p can be a vertex, since it lies
 * on the edge of any triangle through a farther one, and it blocks everything
 * a farther one would. The farther ones are dropped first.
 *
 * When the fan leaves a gap of half a turn or more, no triangle spans it and
 * the scan starts right after it. Otherwise @p p is inside the hull of the
 * rest, and the scan runs twice around, reporting a pair from the first lap
 * only, and skipping the pairs whose angle at @p p has reached half a turn.
 *
 * @return `true` if @p fn stopped the visit.
 */
template <class PointType, class Fn>
bool visitEmptyFan(const PointType& p, std::vector<PointType>& around, Fn& fn) {
    sortAround(around, p);

    // sortAround puts the farther points of a ray first, so its nearest one is
    // the last of each run.
    std::size_t kept = 0;
    for (std::size_t i = 0; i < around.size(); ++i) {
        if (i + 1 < around.size() && onSameRay(p, around[i], around[i + 1]))
            continue;
        if (kept != i)
            around[kept] = std::move(around[i]);
        ++kept;
    }
    around.resize(kept);

    const std::size_t m = around.size();
    if (m < 2)
        return false;

    std::size_t start = m;
    for (std::size_t i = 0; i < m; ++i) {
        if (!(orientationSign(p, around[i], around[(i + 1) % m]) > 0)) {
            start = (i + 1) % m;
            break;
        }
    }
    const bool cyclic = start == m;
    if (!cyclic)
        std::rotate(around.begin(), around.begin() + static_cast<std::ptrdiff_t>(start),
                    around.end());

    const std::size_t length = cyclic ? 2 * m : m;
    const auto at = [&around, m](std::size_t i) -> const PointType& {
        return around[i < m ? i : i - m];
    };
    // Past the first lap, or across a gap of exactly half a turn, a pair can
    // reach half a turn at p and stop being a triangle. Inside a fan narrower
    // than that no pair can, and the test is skipped.
    const bool spanCheck = cyclic || !(orientationSign(p, around.front(), around.back()) > 0);

    std::vector<std::vector<std::size_t>> incoming(length);
    std::vector<std::size_t> head(length, 0);
    std::vector<std::pair<std::size_t, std::size_t>> pending;
    for (std::size_t first = 0; first + 1 < length; ++first) {
        if (!(orientationSign(p, at(first), at(first + 1)) > 0))
            continue;
        pending.emplace_back(first, first + 1);
        while (!pending.empty()) {
            const auto [i, j] = pending.back();
            const std::vector<std::size_t>& queue = incoming[i];
            bool extended = false;
            while (head[i] < queue.size()) {
                const std::size_t k = queue[head[i]];
                if (spanCheck && !(orientationSign(p, at(k), at(j)) > 0)) {
                    // Every later second index widens the pair further.
                    ++head[i];
                    continue;
                }
                if (!(orientationSign(at(k), at(i), at(j)) > 0))
                    break;
                ++head[i];
                pending.emplace_back(k, j);
                extended = true;
                break;
            }
            if (extended)
                continue;
            pending.pop_back();
            if (i < m && invokeVisitor(fn, Triangle<PointType>(p, at(i), at(j))))
                return true;
            incoming[j].push_back(i);
        }
    }
    return false;
}

/**
 * @brief Visits every empty triangle `a b r` with `r` in @p side.
 *
 * Every point of @p side lies strictly left of the line from @p a to @p b;
 * the vector is reordered. A point `s` is in the closed triangle `a b r` when
 * its angle at @p a is at most that of `r` and so is its angle at @p b, so the
 * empty ones are the minima of the two angles. The points go in order of the
 * angle at @p a, nearer first on a ray, and each one is a minimum when its
 * angle at @p b is strictly below all before it.
 *
 * @return `true` if @p fn stopped the visit.
 */
template <class PointType, class Fn>
bool visitEmptyTrianglesLeftOf(const PointType& a, const PointType& b,
                               std::vector<PointType>& side, Fn& fn) {
    // All in one open half-plane of a, so the orientation sign orders them.
    std::sort(side.begin(), side.end(), [&a](const PointType& s, const PointType& r) {
        const auto turn = orientationSign(a, s, r);
        if (turn != 0)
            return turn > 0;
        return (a < r) ? (s < r) : (r < s);
    });

    const PointType* best = nullptr;
    for (const PointType& r : side) {
        if (best != nullptr && !(orientationSign(b, *best, r) > 0))
            continue;
        best = &r;
        if (invokeVisitor(fn, Triangle<PointType>(a, b, r)))
            return true;
    }
    return false;
}

}  // namespace detail

/**
 * @brief Visits every empty triangle of @p points with @p p as a vertex.
 *
 * @p p need not belong to @p points; it is treated as one of them. If @p fn
 * returns `bool`, the visit stops as soon as it returns `true`; a
 * `void`-returning @p fn visits every triangle. Each triangle is visited once.
 *
 * @complexity O(n log n + k) time and O(n + k) space for `n` points and
 * `k` empty triangles with vertex @p p.
 *
 * @param points Container of points.
 * @param p Vertex shared by the triangles.
 * @param fn Called as `fn(Triangle)`.
 * @return `true` if @p fn stopped the visit, `false` otherwise.
 */
template <class Container, class Fn>
    requires detail::EmptyPolygonsPointRange<Container>
bool visitEmptyTriangles(const Container& points,
                         const detail::empty_polygons_point_t<Container>& p, Fn fn) {
    using PointType = detail::empty_polygons_point_t<Container>;
    std::vector<PointType> around;
    for (const auto& q : points) {
        if (q != p)
            around.push_back(q);
    }
    sortDistinctPoints(around);
    return detail::visitEmptyFan(p, around, fn);
}

/**
 * @brief Visits every empty triangle of @p points with the edge @p e.
 *
 * The endpoints of @p e need not belong to @p points; they are treated as
 * points of it. A point of @p points inside @p e blocks every triangle, and a
 * zero-length @p e has none. If @p fn returns `bool`, the visit stops as soon
 * as it returns `true`; a `void`-returning @p fn visits every triangle. Each
 * triangle is visited once.
 *
 * @complexity O(n log n) time and O(n) space for `n` points.
 *
 * @param points Container of points.
 * @param e `Segment` or `OrientedSegment` shared by the triangles.
 * @param fn Called as `fn(Triangle)`.
 * @return `true` if @p fn stopped the visit, `false` otherwise.
 */
template <class Container, class Edge, class Fn>
    requires detail::EmptyPolygonsPointRange<Container> &&
             (SegmentConcept<Edge> || OrientedSegmentConcept<Edge>)
bool visitEmptyTriangles(const Container& points, const Edge& e, Fn fn) {
    using PointType = detail::empty_polygons_point_t<Container>;
    const PointType a(e[0]);
    const PointType b(e[1]);
    if (a == b)
        return false;

    std::vector<PointType> left;
    std::vector<PointType> right;
    for (const auto& q : points) {
        if (q == a || q == b)
            continue;
        const auto side = orientationSign(a, b, q);
        if (side > 0) {
            left.push_back(q);
        } else if (side < 0) {
            right.push_back(q);
        } else if ((a < q) == (q < b)) {
            return false;  // On the line and between a and b, so inside e.
        }
    }
    return detail::visitEmptyTrianglesLeftOf(a, b, left, fn) ||
           detail::visitEmptyTrianglesLeftOf(b, a, right, fn);
}

/**
 * @brief Visits every empty triangle of @p points.
 *
 * If @p fn returns `bool`, the visit stops as soon as it returns `true`; a
 * `void`-returning @p fn visits every triangle. Each triangle is visited once.
 *
 * @complexity O(n^2 log n + k) time and O(n + k) space for `n` points and
 * `k` empty triangles.
 *
 * @param points Container of points.
 * @param fn Called as `fn(Triangle)`.
 * @return `true` if @p fn stopped the visit, `false` otherwise.
 */
template <class Container, class Fn>
    requires detail::EmptyPolygonsPointRange<Container>
bool visitEmptyTriangles(const Container& points, Fn fn) {
    using PointType = detail::empty_polygons_point_t<Container>;
    std::vector<PointType> sorted(std::begin(points), std::end(points));
    sortDistinctPoints(sorted);

    // The lexicographically smallest point of a triangle is a vertex, and no
    // point below it in that order is in the triangle. Scanning each point
    // against the points above it therefore finds every triangle exactly once,
    // from its smallest vertex.
    std::vector<PointType> around;
    for (std::size_t i = 0; i < sorted.size(); ++i) {
        around.assign(sorted.begin() + static_cast<std::ptrdiff_t>(i) + 1, sorted.end());
        if (detail::visitEmptyFan(sorted[i], around, fn))
            return true;
    }
    return false;
}

/**
 * @brief Returns every empty triangle of @p points with @p p as a vertex.
 *
 * As @ref visitEmptyTriangles, collected in a vector.
 */
template <class Container>
    requires detail::EmptyPolygonsPointRange<Container>
auto findEmptyTriangles(const Container& points,
                        const detail::empty_polygons_point_t<Container>& p) {
    std::vector<Triangle<detail::empty_polygons_point_t<Container>>> out;
    visitEmptyTriangles(points, p, [&out](const auto& t) { out.push_back(t); });
    return out;
}

/**
 * @brief Returns every empty triangle of @p points with the edge @p e.
 *
 * As @ref visitEmptyTriangles, collected in a vector.
 */
template <class Container, class Edge>
    requires detail::EmptyPolygonsPointRange<Container> &&
             (SegmentConcept<Edge> || OrientedSegmentConcept<Edge>)
auto findEmptyTriangles(const Container& points, const Edge& e) {
    std::vector<Triangle<detail::empty_polygons_point_t<Container>>> out;
    visitEmptyTriangles(points, e, [&out](const auto& t) { out.push_back(t); });
    return out;
}

/**
 * @brief Returns every empty triangle of @p points.
 *
 * As @ref visitEmptyTriangles, collected in a vector.
 */
template <class Container>
    requires detail::EmptyPolygonsPointRange<Container>
auto findEmptyTriangles(const Container& points) {
    std::vector<Triangle<detail::empty_polygons_point_t<Container>>> out;
    visitEmptyTriangles(points, [&out](const auto& t) { out.push_back(t); });
    return out;
}

}  // namespace pgl
