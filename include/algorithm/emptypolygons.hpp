#pragma once

#include "implementation/minkowskierosion.hpp"

/**
 * @file emptypolygons.hpp
 * @brief Empty triangles and quadrilaterals of a point set, and its largest
 *        empty convex polygon.
 *
 * A polygon is *empty* with respect to a point set when its vertices belong to
 * the set and no other point of the set lies in the closed polygon: a point on
 * an edge blocks it as much as a point inside. Only non-degenerate polygons
 * count: three collinear points never make an empty triangle, and every vertex
 * of an empty quadrilateral is a proper corner, never a straight angle. A
 * quadrilateral must be simple, and need not be convex unless the function
 * says so.
 *
 * Every function takes the points as any container whose value type is a pgl
 * point type, and treats it as a set: coincident points count once.
 */

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
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

// The distinct points of a container, lexicographically sorted.
template <class Container>
auto distinctPointsOf(const Container& points) {
    std::vector<empty_polygons_point_t<Container>> out(std::begin(points), std::end(points));
    sortDistinctPoints(out);
    return out;
}

// The distinct points of a container other than p.
template <class Container>
auto distinctPointsOtherThan(const Container& points,
                             const empty_polygons_point_t<Container>& p) {
    std::vector<empty_polygons_point_t<Container>> out;
    for (const auto& q : points) {
        if (q != p)
            out.push_back(q);
    }
    sortDistinctPoints(out);
    return out;
}

/**
 * @brief The points around a center, in the order the fan scan reads them.
 *
 * Only the nearest point of each ray from the center can be a vertex of an
 * empty triangle there, since it lies on the edge of any triangle through a
 * farther one, and it blocks everything a farther one would; the farther ones
 * are dropped. The rest run counterclockwise.
 *
 * When they leave a gap of half a turn or more, no triangle spans it and the
 * order starts right after it. Otherwise the center is inside their hull, and
 * the scan reads them twice around (`cyclic`): index `i` and `i + m` are the
 * same point.
 */
template <class PointType>
struct EmptyFan {
    std::vector<PointType> around;
    std::size_t m = 0;
    bool cyclic = false;
    // Whether a pair of indices can reach half a turn at the center: past the
    // first lap, or across a gap of exactly half a turn.
    bool spanCheck = false;

    std::size_t length() const { return cyclic ? 2 * m : m; }
    const PointType& at(std::size_t i) const { return around[i < m ? i : i - m]; }
};

// Prepares the fan of p over around, which holds distinct points none equal to p.
template <class PointType>
EmptyFan<PointType> emptyFanOf(const PointType& p, std::vector<PointType> around) {
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

    EmptyFan<PointType> fan;
    fan.m = around.size();
    fan.around = std::move(around);
    const std::size_t m = fan.m;
    if (m < 2)
        return fan;

    std::size_t start = m;
    for (std::size_t i = 0; i < m; ++i) {
        if (!(orientationSign(p, fan.around[i], fan.around[(i + 1) % m]) > 0)) {
            start = (i + 1) % m;
            break;
        }
    }
    fan.cyclic = start == m;
    if (!fan.cyclic)
        std::rotate(fan.around.begin(), fan.around.begin() + static_cast<std::ptrdiff_t>(start),
                    fan.around.end());
    fan.spanCheck =
        fan.cyclic || !(orientationSign(p, fan.around.front(), fan.around.back()) > 0);
    return fan;
}

/**
 * @brief Calls `onPair(i, j)` for every empty triangle `p at(i) at(j)`, `i < j`.
 *
 * This is the fan scan of Dobkin, Edelsbrunner and Overmars ("Searching for
 * empty convex polygons", 1990). The triangle `p p_i p_j` is empty exactly when
 * every point sorted between `p_i` and `p_j` lies strictly beyond the segment
 * `p_i p_j`. Such a pair either is consecutive or has some `p_k` between it
 * with `p p_i p_k` and `p p_k p_j` both empty and a strict left turn
 * `p_i p_k p_j` -- take the `p_k` nearest to the line `p_i p_j`. The scan
 * builds the pairs in increasing order of their second index from that rule
 * alone. Each index keeps the pairs ending at it in a queue, in increasing
 * order of their first index, which is the order their angle at it turns, so
 * the ones that extend to a new second index are a prefix of it, and each pair
 * is dequeued once.
 *
 * On a cyclic fan, every pair `i < j < 2m` is reported, both laps included.
 * The pairs ending at an index are reported in increasing order of their first
 * index, and those starting at an index in increasing order of their second.
 *
 * @return `true` if @p onPair stopped the scan.
 */
template <class PointType, class OnPair>
bool scanEmptyFan(const PointType& p, const EmptyFan<PointType>& fan, OnPair& onPair) {
    const std::size_t length = fan.length();
    if (fan.m < 2)
        return false;

    std::vector<std::vector<std::size_t>> incoming(length);
    std::vector<std::size_t> head(length, 0);
    std::vector<std::pair<std::size_t, std::size_t>> pending;
    for (std::size_t first = 0; first + 1 < length; ++first) {
        if (!(orientationSign(p, fan.at(first), fan.at(first + 1)) > 0))
            continue;
        pending.emplace_back(first, first + 1);
        while (!pending.empty()) {
            const auto [i, j] = pending.back();
            const std::vector<std::size_t>& queue = incoming[i];
            bool extended = false;
            while (head[i] < queue.size()) {
                const std::size_t k = queue[head[i]];
                if (fan.spanCheck && !(orientationSign(p, fan.at(k), fan.at(j)) > 0)) {
                    // Every later second index widens the pair further.
                    ++head[i];
                    continue;
                }
                if (!(orientationSign(fan.at(k), fan.at(i), fan.at(j)) > 0))
                    break;
                ++head[i];
                pending.emplace_back(k, j);
                extended = true;
                break;
            }
            if (extended)
                continue;
            pending.pop_back();
            if (onPair(i, j))
                return true;
            incoming[j].push_back(i);
        }
    }
    return false;
}

// Visits every empty triangle p q r with q, r in around, as scanEmptyFan.
template <class PointType, class Fn>
bool visitEmptyFan(const PointType& p, std::vector<PointType> around, Fn& fn) {
    const EmptyFan<PointType> fan = emptyFanOf(p, std::move(around));
    auto report = [&](std::size_t i, std::size_t j) {
        return i < fan.m && invokeVisitor(fn, Triangle<PointType>(p, fan.at(i), fan.at(j)));
    };
    return scanEmptyFan(p, fan, report);
}

/**
 * @brief Every empty triangle of a fan, as lists of neighbors.
 *
 * For the point at index `x < m`, `before(x)` lists the points `q` of the
 * empty triangles `p q at(x)` and `after(x)` the points `r` of the empty
 * triangles `p at(x) r`, both as fan indices in increasing order, which is the
 * counterclockwise order around `p`. Read as `at(index)`.
 */
struct EmptyFanAdjacency {
    std::vector<std::vector<std::size_t>> in;
    std::vector<std::vector<std::size_t>> out;
    std::size_t m = 0;
    bool cyclic = false;

    const std::vector<std::size_t>& before(std::size_t x) const { return in[cyclic ? x + m : x]; }
    const std::vector<std::size_t>& after(std::size_t x) const { return out[x]; }
};

template <class PointType>
EmptyFanAdjacency emptyFanAdjacency(const PointType& p, const EmptyFan<PointType>& fan) {
    EmptyFanAdjacency adjacency;
    adjacency.m = fan.m;
    adjacency.cyclic = fan.cyclic;
    adjacency.in.resize(fan.length());
    adjacency.out.resize(fan.length());
    auto record = [&adjacency](std::size_t i, std::size_t j) {
        adjacency.in[j].push_back(i);
        adjacency.out[i].push_back(j);
        return false;
    };
    (void)scanEmptyFan(p, fan, record);
    return adjacency;
}

// Apexes r of the empty triangles a b r with r in side, all strictly left of
// the line from a to b. The side is reordered.
//
// A point s is in the closed triangle a b r when its angle at a is at most that
// of r and so is its angle at b, so the apexes are the minima of the two
// angles. The points go in order of the angle at a, nearer first on a ray, and
// each one is a minimum when its angle at b is strictly below all before it.
// The apexes come out in increasing order of the angle at a, which is
// decreasing order of the angle at b.
template <class PointType>
std::vector<PointType> emptyApexesLeftOf(const PointType& a, const PointType& b,
                                         std::vector<PointType>& side) {
    // All in one open half-plane of a, so the orientation sign orders them.
    std::sort(side.begin(), side.end(), [&a](const PointType& s, const PointType& r) {
        const auto turn = orientationSign(a, s, r);
        if (turn != 0)
            return turn > 0;
        return (a < r) ? (s < r) : (r < s);
    });

    std::vector<PointType> apexes;
    for (const PointType& r : side) {
        if (!apexes.empty() && !(orientationSign(b, apexes.back(), r) > 0))
            continue;
        apexes.push_back(r);
    }
    return apexes;
}

// The empty-triangle apexes on the two sides of the edge from a to b: left of
// it in increasing order of the angle at a, right of it in decreasing order.
// Both empty when a point of the set lies inside the edge or a == b.
template <class Container, class PointType>
std::pair<std::vector<PointType>, std::vector<PointType>>
emptyApexesAcross(const Container& points, const PointType& a, const PointType& b) {
    std::vector<PointType> left;
    std::vector<PointType> right;
    if (a == b)
        return {};
    for (const auto& q : points) {
        if (q == a || q == b)
            continue;
        const auto side = orientationSign(a, b, q);
        if (side > 0) {
            left.push_back(q);
        } else if (side < 0) {
            right.push_back(q);
        } else if ((a < q) == (q < b)) {
            return {};  // On the line and between a and b, so inside the edge.
        }
    }
    return {emptyApexesLeftOf(a, b, left), emptyApexesLeftOf(b, a, right)};
}

// The quadrilateral with counterclockwise boundary a q b r, in canonical form.
template <class Result, class PointType>
Result quadrilateralOf(const PointType& a, const PointType& q, const PointType& b,
                      const PointType& r) {
    std::array<PointType, 4> ring{a, q, b, r};
    std::rotate(ring.begin(), std::min_element(ring.begin(), ring.end()), ring.end());
    return Result(ring, trusted);
}

/**
 * @brief Calls `fn(q, r, convexAtA, convexAtB)` for the quadrilaterals `a q b r`
 *        built on the diagonal `a b`.
 *
 * @p before holds the apexes `q` of the empty triangles `a q b`, right of the
 * line from `a` to `b`, and @p after the apexes `r` of the empty triangles
 * `a b r`, left of it, both in counterclockwise order around `a`. The union of
 * two such triangles is an empty quadrilateral unless its angle at `a` or at
 * `b` is straight, and those pairs are skipped: at most one `r` per `q` at
 * each of `a` and `b`, since the `r` leave both in distinct directions.
 *
 * @return `true` if @p fn stopped the visit.
 */
template <class PointType, class Before, class After, class Fn>
bool visitQuadrilateralPairs(const PointType& a, const PointType& b, const Before& before,
                             const After& after, Fn&& fn) {
    for (const PointType& q : before) {
        for (const PointType& r : after) {
            const auto atA = orientationSign(a, q, r);
            if (atA == 0)
                continue;
            const auto atB = orientationSign(q, b, r);
            if (atB == 0)
                continue;
            if (fn(q, r, atA > 0, atB > 0))
                return true;
        }
    }
    return false;
}

/**
 * @brief Calls `fn(q, r)` for the convex quadrilaterals `a q b r` built on the
 *        diagonal `a b`, with @p before and @p after as in
 *        @ref visitQuadrilateralPairs.
 *
 * Along @p before the angle `q a b` shrinks and the angle `q b a` grows; along
 * @p after the angle `b a r` grows and the angle `a b r` shrinks. So for each
 * `q` the `r` convex at `a` are a prefix of @p after and those convex at `b` a
 * suffix, and both ends only move forward as `q` does.
 *
 * @return `true` if @p fn stopped the visit.
 */
template <class PointType, class Before, class After, class Fn>
bool visitConvexQuadrilateralPairs(const PointType& a, const PointType& b, const Before& before,
                                   const After& after, Fn&& fn) {
    const std::size_t count = after.size();
    std::size_t convexAtA = 0;  // after[0, convexAtA) is convex at a.
    std::size_t convexAtB = 0;  // after[convexAtB, count) is convex at b.
    for (const PointType& q : before) {
        while (convexAtA < count && orientationSign(a, q, after[convexAtA]) > 0)
            ++convexAtA;
        while (convexAtB < count && !(orientationSign(q, b, after[convexAtB]) > 0))
            ++convexAtB;
        for (std::size_t i = convexAtB; i < convexAtA; ++i) {
            if (fn(q, after[i]))
                return true;
        }
    }
    return false;
}

// A list of fan indices read as the points they stand for.
template <class PointType>
struct FanPoints {
    const EmptyFan<PointType>* fan;
    const std::vector<std::size_t>* indices;

    struct iterator {
        const EmptyFan<PointType>* fan;
        const std::size_t* index;
        const PointType& operator*() const { return fan->at(*index); }
        iterator& operator++() {
            ++index;
            return *this;
        }
        bool operator!=(const iterator& other) const { return index != other.index; }
    };

    std::size_t size() const { return indices->size(); }
    const PointType& operator[](std::size_t i) const { return fan->at((*indices)[i]); }
    iterator begin() const { return {fan, indices->data()}; }
    iterator end() const { return {fan, indices->data() + indices->size()}; }
};

template <class PointType>
FanPoints<PointType> fanPoints(const EmptyFan<PointType>& fan,
                               const std::vector<std::size_t>& indices) {
    return {&fan, &indices};
}

/**
 * @brief Visits the quadrilaterals with a diagonal at the center of a fan.
 *
 * Calls `fn(q, b, r, convexAtCenter, convexAtB)` for every empty
 * quadrilateral `p q b r` (counterclockwise) whose diagonal `p b` lies inside
 * it; with @p convexOnly, only the convex ones.
 */
template <bool convexOnly, class PointType, class Fn>
bool visitQuadrilateralsAtCenter(const PointType& p, const EmptyFan<PointType>& fan,
                                 const EmptyFanAdjacency& adjacency, Fn& fn) {
    for (std::size_t x = 0; x < fan.m; ++x) {
        const PointType& b = fan.at(x);
        const auto before = fanPoints(fan, adjacency.before(x));
        const auto after = fanPoints(fan, adjacency.after(x));
        if (before.size() == 0 || after.size() == 0)
            continue;
        bool stopped;
        if constexpr (convexOnly) {
            stopped = visitConvexQuadrilateralPairs(
                p, b, before, after,
                [&](const PointType& q, const PointType& r) { return fn(q, b, r, true, true); });
        } else {
            stopped = visitQuadrilateralPairs(
                p, b, before, after,
                [&](const PointType& q, const PointType& r, bool atP, bool atB) {
                    return fn(q, b, r, atP, atB);
                });
        }
        if (stopped)
            return true;
    }
    return false;
}

}  // namespace detail

// ---------------------------------------------------------------------------
// Triangles
// ---------------------------------------------------------------------------

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
    return detail::visitEmptyFan(p, detail::distinctPointsOtherThan(points, p), fn);
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
    const auto [left, right] = detail::emptyApexesAcross(points, a, b);
    for (const PointType& r : left) {
        if (detail::invokeVisitor(fn, Triangle<PointType>(a, b, r)))
            return true;
    }
    for (const PointType& r : right) {
        if (detail::invokeVisitor(fn, Triangle<PointType>(a, b, r)))
            return true;
    }
    return false;
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
    const auto sorted = detail::distinctPointsOf(points);

    // The lexicographically smallest point of a triangle is a vertex, and no
    // point below it in that order is in the triangle. Scanning each point
    // against the points above it therefore finds every triangle exactly once,
    // from its smallest vertex.
    for (std::size_t i = 0; i < sorted.size(); ++i) {
        std::vector above(sorted.begin() + static_cast<std::ptrdiff_t>(i) + 1, sorted.end());
        if (detail::visitEmptyFan(sorted[i], std::move(above), fn))
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

// ---------------------------------------------------------------------------
// Quadrilaterals
//
// An empty quadrilateral is the union of two empty triangles on opposite sides
// of a diagonal that lies inside it: a convex one has two such diagonals, a
// non-convex one only the diagonal from its reflex vertex.
// ---------------------------------------------------------------------------

/**
 * @brief Visits every empty quadrilateral of @p points with @p p as a vertex.
 *
 * The quadrilaterals are simple, possibly non-convex, and given as `Polygon`.
 * @p p need not belong to @p points; it is treated as one of them. If @p fn
 * returns `bool`, the visit stops as soon as it returns `true`; a
 * `void`-returning @p fn visits every quadrilateral. Each quadrilateral is
 * visited once.
 *
 * @complexity O(n^2 log n + t + k) time and O(n + t) space for `n` points,
 * `t` empty triangles of the points and @p p, and `k` visited quadrilaterals.
 *
 * @param points Container of points.
 * @param p Vertex shared by the quadrilaterals.
 * @param fn Called as `fn(Polygon)`.
 * @return `true` if @p fn stopped the visit, `false` otherwise.
 */
template <class Container, class Fn>
    requires detail::EmptyPolygonsPointRange<Container>
bool visitEmptyQuadrilaterals(const Container& points,
                              const detail::empty_polygons_point_t<Container>& p, Fn fn) {
    using PointType = detail::empty_polygons_point_t<Container>;
    using Result = Polygon<PointType>;
    auto others = detail::distinctPointsOtherThan(points, p);

    // With a diagonal through p.
    const detail::EmptyFan<PointType> fan = detail::emptyFanOf(p, others);
    const detail::EmptyFanAdjacency adjacency = detail::emptyFanAdjacency(p, fan);
    auto report = [&](const PointType& q, const PointType& b, const PointType& r, bool, bool) {
        return detail::invokeVisitor(fn, detail::quadrilateralOf<Result>(p, q, b, r));
    };
    if (detail::visitQuadrilateralsAtCenter<false>(p, fan, adjacency, report))
        return true;

    // Without one: p is next to the reflex vertex d, whose diagonal d w is the
    // one inside. The triangle p d w is empty, so d is a neighbor of p in its
    // fan, and the quadrilateral is found from the fan of d.
    for (std::size_t x = 0; x < fan.m; ++x) {
        if (adjacency.before(x).empty() && adjacency.after(x).empty())
            continue;
        const PointType& d = fan.at(x);
        std::vector<PointType> aroundD;
        aroundD.reserve(others.size());
        for (const PointType& q : others) {
            if (q != d)
                aroundD.push_back(q);
        }
        aroundD.push_back(p);
        const detail::EmptyFan<PointType> fanD = detail::emptyFanOf(d, std::move(aroundD));
        std::size_t at = fanD.m;
        for (std::size_t i = 0; i < fanD.m; ++i) {
            if (fanD.at(i) == p) {
                at = i;
                break;
            }
        }
        if (at == fanD.m)
            continue;  // Not the nearest on its ray from d, so no triangle d p w is empty.
        const detail::EmptyFanAdjacency adjacencyD = detail::emptyFanAdjacency(d, fanD);

        // d p w r counterclockwise, reflex at d.
        for (std::size_t w : adjacencyD.after(at)) {
            const std::size_t wIndex = w % fanD.m;
            for (std::size_t r : adjacencyD.after(wIndex)) {
                if (orientationSign(d, p, fanD.at(r)) < 0 &&
                    detail::invokeVisitor(fn, detail::quadrilateralOf<Result>(
                                                  d, p, fanD.at(w), fanD.at(r))))
                    return true;
            }
        }
        // d q w p counterclockwise, reflex at d.
        for (std::size_t w : adjacencyD.before(at)) {
            const std::size_t wIndex = w % fanD.m;
            for (std::size_t q : adjacencyD.before(wIndex)) {
                if (orientationSign(d, fanD.at(q), p) < 0 &&
                    detail::invokeVisitor(fn, detail::quadrilateralOf<Result>(
                                                  d, fanD.at(q), fanD.at(w), p)))
                    return true;
            }
        }
    }
    return false;
}

/**
 * @brief Visits every empty quadrilateral of @p points with the diagonal @p e
 *        inside it.
 *
 * The quadrilaterals are simple, possibly non-convex, and given as `Polygon`.
 * The endpoints of @p e need not belong to @p points; they are treated as
 * points of it. A point of @p points inside @p e blocks every quadrilateral,
 * and a zero-length @p e has none. If @p fn returns `bool`, the visit stops as
 * soon as it returns `true`; a `void`-returning @p fn visits every
 * quadrilateral. Each quadrilateral is visited once.
 *
 * @complexity O(n log n + k) time and O(n) space for `n` points and `k`
 * visited quadrilaterals.
 *
 * @param points Container of points.
 * @param e `Segment` or `OrientedSegment`, a diagonal of the quadrilaterals.
 * @param fn Called as `fn(Polygon)`.
 * @return `true` if @p fn stopped the visit, `false` otherwise.
 */
template <class Container, class Edge, class Fn>
    requires detail::EmptyPolygonsPointRange<Container> &&
             (SegmentConcept<Edge> || OrientedSegmentConcept<Edge>)
bool visitEmptyQuadrilaterals(const Container& points, const Edge& e, Fn fn) {
    using PointType = detail::empty_polygons_point_t<Container>;
    const PointType a(e[0]);
    const PointType b(e[1]);
    const auto [left, right] = detail::emptyApexesAcross(points, a, b);
    return detail::visitQuadrilateralPairs(
        a, b, right, left, [&](const PointType& q, const PointType& r, bool, bool) {
            return detail::invokeVisitor(fn,
                                         detail::quadrilateralOf<Polygon<PointType>>(a, q, b, r));
        });
}

/**
 * @brief Visits every empty quadrilateral of @p points.
 *
 * The quadrilaterals are simple, possibly non-convex, and given as `Polygon`.
 * If @p fn returns `bool`, the visit stops as soon as it returns `true`; a
 * `void`-returning @p fn visits every quadrilateral. Each quadrilateral is
 * visited once.
 *
 * @complexity O(n^2 log n + t + k) time and O(n + t) space for `n` points,
 * `t` empty triangles and `k` empty quadrilaterals.
 *
 * @param points Container of points.
 * @param fn Called as `fn(Polygon)`.
 * @return `true` if @p fn stopped the visit, `false` otherwise.
 */
template <class Container, class Fn>
    requires detail::EmptyPolygonsPointRange<Container>
bool visitEmptyQuadrilaterals(const Container& points, Fn fn) {
    using PointType = detail::empty_polygons_point_t<Container>;
    using Result = Polygon<PointType>;
    const auto sorted = detail::distinctPointsOf(points);

    // Every quadrilateral turns up at both ends of each inside diagonal. A
    // non-convex one is reported from its reflex vertex, a convex one from its
    // lexicographically smallest vertex.
    for (std::size_t i = 0; i < sorted.size(); ++i) {
        const PointType& a = sorted[i];
        std::vector<PointType> others;
        others.reserve(sorted.size() - 1);
        others.insert(others.end(), sorted.begin(), sorted.begin() + static_cast<std::ptrdiff_t>(i));
        others.insert(others.end(), sorted.begin() + static_cast<std::ptrdiff_t>(i) + 1,
                      sorted.end());
        const detail::EmptyFan<PointType> fan = detail::emptyFanOf(a, std::move(others));
        const detail::EmptyFanAdjacency adjacency = detail::emptyFanAdjacency(a, fan);
        auto report = [&](const PointType& q, const PointType& b, const PointType& r,
                          bool convexAtA, bool convexAtB) {
            const bool reflexAtA = !convexAtA;
            const bool convexFromSmallest =
                convexAtA && convexAtB && a < q && a < b && a < r;
            if (!reflexAtA && !convexFromSmallest)
                return false;
            return detail::invokeVisitor(fn, detail::quadrilateralOf<Result>(a, q, b, r));
        };
        if (detail::visitQuadrilateralsAtCenter<false>(a, fan, adjacency, report))
            return true;
    }
    return false;
}

/**
 * @brief Returns every empty quadrilateral of @p points with @p p as a vertex.
 *
 * As @ref visitEmptyQuadrilaterals, collected in a vector.
 */
template <class Container>
    requires detail::EmptyPolygonsPointRange<Container>
auto findEmptyQuadrilaterals(const Container& points,
                             const detail::empty_polygons_point_t<Container>& p) {
    std::vector<Polygon<detail::empty_polygons_point_t<Container>>> out;
    visitEmptyQuadrilaterals(points, p, [&out](const auto& q) { out.push_back(q); });
    return out;
}

/**
 * @brief Returns every empty quadrilateral of @p points with the diagonal @p e
 *        inside it.
 *
 * As @ref visitEmptyQuadrilaterals, collected in a vector.
 */
template <class Container, class Edge>
    requires detail::EmptyPolygonsPointRange<Container> &&
             (SegmentConcept<Edge> || OrientedSegmentConcept<Edge>)
auto findEmptyQuadrilaterals(const Container& points, const Edge& e) {
    std::vector<Polygon<detail::empty_polygons_point_t<Container>>> out;
    visitEmptyQuadrilaterals(points, e, [&out](const auto& q) { out.push_back(q); });
    return out;
}

/**
 * @brief Returns every empty quadrilateral of @p points.
 *
 * As @ref visitEmptyQuadrilaterals, collected in a vector.
 */
template <class Container>
    requires detail::EmptyPolygonsPointRange<Container>
auto findEmptyQuadrilaterals(const Container& points) {
    std::vector<Polygon<detail::empty_polygons_point_t<Container>>> out;
    visitEmptyQuadrilaterals(points, [&out](const auto& q) { out.push_back(q); });
    return out;
}

// ---------------------------------------------------------------------------
// Convex quadrilaterals
// ---------------------------------------------------------------------------

/**
 * @brief Visits every empty convex quadrilateral of @p points with @p p as a
 *        vertex.
 *
 * The quadrilaterals are given as `Convex`. @p p need not belong to
 * @p points; it is treated as one of them. If @p fn returns `bool`, the visit
 * stops as soon as it returns `true`; a `void`-returning @p fn visits every
 * quadrilateral. Each quadrilateral is visited once.
 *
 * @complexity O(n log n + t + k) time and O(n + t) space for `n` points, `t`
 * empty triangles with vertex @p p, and `k` visited quadrilaterals.
 *
 * @param points Container of points.
 * @param p Vertex shared by the quadrilaterals.
 * @param fn Called as `fn(Convex)`.
 * @return `true` if @p fn stopped the visit, `false` otherwise.
 */
template <class Container, class Fn>
    requires detail::EmptyPolygonsPointRange<Container>
bool visitEmptyConvexQuadrilaterals(const Container& points,
                                    const detail::empty_polygons_point_t<Container>& p, Fn fn) {
    using PointType = detail::empty_polygons_point_t<Container>;
    const detail::EmptyFan<PointType> fan =
        detail::emptyFanOf(p, detail::distinctPointsOtherThan(points, p));
    const detail::EmptyFanAdjacency adjacency = detail::emptyFanAdjacency(p, fan);
    auto report = [&](const PointType& q, const PointType& b, const PointType& r, bool, bool) {
        return detail::invokeVisitor(fn, detail::quadrilateralOf<Convex<PointType>>(p, q, b, r));
    };
    return detail::visitQuadrilateralsAtCenter<true>(p, fan, adjacency, report);
}

/**
 * @brief Visits every empty convex quadrilateral of @p points with the
 *        diagonal @p e.
 *
 * The quadrilaterals are given as `Convex`. The endpoints of @p e need not
 * belong to @p points; they are treated as points of it. A point of @p points
 * inside @p e blocks every quadrilateral, and a zero-length @p e has none. If
 * @p fn returns `bool`, the visit stops as soon as it returns `true`; a
 * `void`-returning @p fn visits every quadrilateral. Each quadrilateral is
 * visited once.
 *
 * @complexity O(n log n + k) time and O(n) space for `n` points and `k`
 * visited quadrilaterals.
 *
 * @param points Container of points.
 * @param e `Segment` or `OrientedSegment`, a diagonal of the quadrilaterals.
 * @param fn Called as `fn(Convex)`.
 * @return `true` if @p fn stopped the visit, `false` otherwise.
 */
template <class Container, class Edge, class Fn>
    requires detail::EmptyPolygonsPointRange<Container> &&
             (SegmentConcept<Edge> || OrientedSegmentConcept<Edge>)
bool visitEmptyConvexQuadrilaterals(const Container& points, const Edge& e, Fn fn) {
    using PointType = detail::empty_polygons_point_t<Container>;
    const PointType a(e[0]);
    const PointType b(e[1]);
    auto [left, right] = detail::emptyApexesAcross(points, a, b);
    // The right side comes out in decreasing angle at a, the order a fan lists
    // the points before b in.
    return detail::visitConvexQuadrilateralPairs(
        a, b, right, left, [&](const PointType& q, const PointType& r) {
            return detail::invokeVisitor(fn,
                                         detail::quadrilateralOf<Convex<PointType>>(a, q, b, r));
        });
}

/**
 * @brief Visits every empty convex quadrilateral of @p points.
 *
 * The quadrilaterals are given as `Convex`. If @p fn returns `bool`, the visit
 * stops as soon as it returns `true`; a `void`-returning @p fn visits every
 * quadrilateral. Each quadrilateral is visited once.
 *
 * @complexity O(n^2 log n + t + k) time and O(n + t) space for `n` points,
 * `t` empty triangles and `k` empty convex quadrilaterals.
 *
 * @param points Container of points.
 * @param fn Called as `fn(Convex)`.
 * @return `true` if @p fn stopped the visit, `false` otherwise.
 */
template <class Container, class Fn>
    requires detail::EmptyPolygonsPointRange<Container>
bool visitEmptyConvexQuadrilaterals(const Container& points, Fn fn) {
    using PointType = detail::empty_polygons_point_t<Container>;
    const auto sorted = detail::distinctPointsOf(points);

    // As for triangles, each quadrilateral is found once, from its
    // lexicographically smallest vertex, scanning only the points above it:
    // that vertex is convex, so the diagonal from it lies inside.
    for (std::size_t i = 0; i < sorted.size(); ++i) {
        const PointType& a = sorted[i];
        const detail::EmptyFan<PointType> fan = detail::emptyFanOf(
            a, std::vector(sorted.begin() + static_cast<std::ptrdiff_t>(i) + 1, sorted.end()));
        const detail::EmptyFanAdjacency adjacency = detail::emptyFanAdjacency(a, fan);
        auto report = [&](const PointType& q, const PointType& b, const PointType& r, bool,
                          bool) {
            return detail::invokeVisitor(fn,
                                         detail::quadrilateralOf<Convex<PointType>>(a, q, b, r));
        };
        if (detail::visitQuadrilateralsAtCenter<true>(a, fan, adjacency, report))
            return true;
    }
    return false;
}

/**
 * @brief Returns every empty convex quadrilateral of @p points with @p p as a
 *        vertex.
 *
 * As @ref visitEmptyConvexQuadrilaterals, collected in a vector.
 */
template <class Container>
    requires detail::EmptyPolygonsPointRange<Container>
auto findEmptyConvexQuadrilaterals(const Container& points,
                                   const detail::empty_polygons_point_t<Container>& p) {
    std::vector<Convex<detail::empty_polygons_point_t<Container>>> out;
    visitEmptyConvexQuadrilaterals(points, p, [&out](const auto& q) { out.push_back(q); });
    return out;
}

/**
 * @brief Returns every empty convex quadrilateral of @p points with the
 *        diagonal @p e.
 *
 * As @ref visitEmptyConvexQuadrilaterals, collected in a vector.
 */
template <class Container, class Edge>
    requires detail::EmptyPolygonsPointRange<Container> &&
             (SegmentConcept<Edge> || OrientedSegmentConcept<Edge>)
auto findEmptyConvexQuadrilaterals(const Container& points, const Edge& e) {
    std::vector<Convex<detail::empty_polygons_point_t<Container>>> out;
    visitEmptyConvexQuadrilaterals(points, e, [&out](const auto& q) { out.push_back(q); });
    return out;
}

/**
 * @brief Returns every empty convex quadrilateral of @p points.
 *
 * As @ref visitEmptyConvexQuadrilaterals, collected in a vector.
 */
template <class Container>
    requires detail::EmptyPolygonsPointRange<Container>
auto findEmptyConvexQuadrilaterals(const Container& points) {
    std::vector<Convex<detail::empty_polygons_point_t<Container>>> out;
    visitEmptyConvexQuadrilaterals(points, [&out](const auto& q) { out.push_back(q); });
    return out;
}

namespace detail {

// Scores of the largest empty convex polygon search. A convex polygon is the
// fan of triangles p q_i q_{i+1} from its lexicographically smallest vertex p,
// its anchor, and both measures are a start value plus a gain per fan
// triangle: twice the area, or 2 plus one vertex per triangle.
template <class PointType>
struct EmptyConvexAreaScore {
    using Value = decltype(orientationDeterminant(std::declval<const PointType&>(),
                                                  std::declval<const PointType&>(),
                                                  std::declval<const PointType&>()));
    static Value start() { return Value(0); }
    static Value gain(const PointType& p, const PointType& a, const PointType& b) {
        return orientationDeterminant(p, a, b);
    }
};

template <class PointType>
struct EmptyConvexVertexScore {
    using Value = std::size_t;
    static Value start() { return 2; }
    static Value gain(const PointType&, const PointType&, const PointType&) { return 1; }
};

inline constexpr std::uint32_t emptyConvexNone = UINT32_MAX;

// Scratch space of the search, reused from one anchor to the next. q holds the
// fan of the anchor (the points after it, counterclockwise, the nearest of each
// ray only) and ids their indices in the sorted point set.
template <class PointType, class Value>
struct EmptyConvexWork {
    struct InPair {
        std::uint32_t first;
        std::uint32_t parent;  // first index of the best pair before, or emptyConvexNone
        Value value;
    };
    std::vector<PointType> q;
    std::vector<std::uint32_t> ids;
    std::vector<std::vector<InPair>> incoming;
    std::vector<std::uint32_t> head, bestArg;
    std::vector<Value> bestAt, pre, suf, cum;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> pending, pairs, tmp, aItems, bItems, chords;
    std::vector<std::uint32_t> stack, kpos, inner, outer, bList, count, outStart, inStart, inFill;
    std::vector<std::uint8_t> cls, aFlag, bFlag;
    std::vector<InPair> in;
    // The best pair of the last anchor run in rebuild mode, and whether the
    // anchor ran the plain scan (pairs in incoming) or the restricted path
    // (pairs in in, by inStart and inFill).
    bool scanned = false, topFound = false;
    std::uint32_t topI = 0, topJ = 0;
    Value top{};
};

// The fan of pts[a]: the points after it, counterclockwise around it (they
// span less than half a turn, since pts[a] is the smallest), keeping only the
// nearest point of each ray, which blocks the farther ones.
template <class PointType, class Value>
void emptyConvexFan(const std::vector<PointType>& pts, std::size_t a,
                    EmptyConvexWork<PointType, Value>& w) {
    const PointType& p = pts[a];
    w.ids.resize(pts.size() - a - 1);
    for (std::size_t i = 0; i < w.ids.size(); ++i)
        w.ids[i] = static_cast<std::uint32_t>(a + 1 + i);
    std::sort(w.ids.begin(), w.ids.end(), [&](std::uint32_t u, std::uint32_t v) {
        const auto turn = orientationSign(p, pts[u], pts[v]);
        if (turn != 0)
            return turn > 0;
        return u < v;  // Along a ray from p the order is lexicographic.
    });
    std::size_t kept = 0;
    for (std::size_t i = 0; i < w.ids.size(); ++i)
        if (kept == 0 || orientationSign(p, pts[w.ids[kept - 1]], pts[w.ids[i]]) != 0)
            w.ids[kept++] = w.ids[i];
    w.ids.resize(kept);
    w.q.resize(kept);
    for (std::size_t i = 0; i < kept; ++i)
        w.q[i] = pts[w.ids[i]];
}

/*
 * The fan scan of Dobkin, Edelsbrunner and Overmars (see scanEmptyFan) with
 * the dynamic program for the best polygon folded in:
 *
 *   f(i, j) = gain(p, q_i, q_j) + max(start, max { f(k, i) : turn(q_k, q_i, q_j) > 0 })
 *
 * over the empty triangles p q_i q_j. If every p q_k q_i is empty, the angle
 * q_k q_i p grows with k, and if every p q_i q_j is empty, the angle p q_i q_j
 * shrinks as j grows; so the k with a convex turn at q_i are a prefix of the
 * pairs ending at i, growing with j. Before it reports (i, j) the scan advances
 * head[i] over exactly that prefix, so the max rides along with it.
 *
 * keep(i, j) may drop pairs as long as it drops every pair derived from a
 * dropped one. onPair(i, j, f) sees every pair kept; incoming[j] then holds the
 * pairs ending at j by increasing first index, with the first index of the
 * pair before each in its best polygon.
 */
template <class Score, class PointType, class Value, class OnPair, class Keep>
void emptyConvexScan(const PointType& p, EmptyConvexWork<PointType, Value>& w, OnPair&& onPair,
                     Keep&& keep) {
    const std::vector<PointType>& q = w.q;
    const std::size_t m = q.size();
    if (w.incoming.size() < m)
        w.incoming.resize(m);
    for (std::size_t i = 0; i < m; ++i)
        w.incoming[i].clear();
    w.head.assign(m, 0);
    w.bestAt.assign(m, Score::start());
    w.bestArg.assign(m, emptyConvexNone);
    for (std::uint32_t first = 0; first + 1 < m; ++first) {
        if (keep(first, first + 1))
            w.pending.emplace_back(first, first + 1);
        while (!w.pending.empty()) {
            const auto [i, j] = w.pending.back();
            const auto& queue = w.incoming[i];
            if (w.head[i] < queue.size()) {
                const std::uint32_t k = queue[w.head[i]].first;
                if (orientationSign(q[k], q[i], q[j]) > 0) {
                    if (w.bestAt[i] < queue[w.head[i]].value) {
                        w.bestAt[i] = queue[w.head[i]].value;
                        w.bestArg[i] = k;
                    }
                    ++w.head[i];
                    if (keep(k, j))
                        w.pending.emplace_back(k, j);
                    continue;
                }
            }
            w.pending.pop_back();
            const Value v = Score::gain(p, q[i], q[j]) + w.bestAt[i];
            onPair(i, j, v);
            w.incoming[j].push_back({i, w.bestArg[i], v});
        }
    }
}

/*
 * Upper bounds on the two halves of a polygon at the anchor around a pair
 * (i, j): a polygon with the edge q_i q_j scores at most pre[i] + gain + suf[j].
 * For the area, pre[i] is twice the area of the hull of p, q_0, ..., q_i (the
 * fan triangles of Graham's stack, which holds every prefix hull in turn) and
 * suf[j] that of p, q_j, ..., q_{m-1}; for the vertex count, i + 2 and
 * m - 1 - j. If q_i lies between q_k and q_j beyond the segment q_k q_j, the
 * bound of (k, j) is at most those of (k, i) and (i, j); the scan derives
 * (k, j) from exactly such a pair, so dropping the pairs whose bound cannot
 * beat the best found drops whole subtrees and keeps the scan exact.
 */
template <class Score, class PointType, class Value>
void emptyConvexPairBounds(const PointType& p, EmptyConvexWork<PointType, Value>& w) {
    const std::vector<PointType>& q = w.q;
    const std::uint32_t m = static_cast<std::uint32_t>(q.size());
    w.pre.resize(m);
    w.suf.resize(m);
    if constexpr (std::is_same_v<Score, EmptyConvexVertexScore<PointType>>) {
        for (std::uint32_t i = 0; i < m; ++i) {
            w.pre[i] = i + 2;
            w.suf[i] = m - 1 - i;
        }
    } else {
        auto& st = w.stack;
        auto& cum = w.cum;
        st.clear();
        cum.clear();
        for (std::uint32_t i = 0; i < m; ++i) {
            while (st.size() >= 2 && !(orientationSign(q[st[st.size() - 2]], q[st.back()], q[i]) > 0)) {
                st.pop_back();
                cum.pop_back();
            }
            cum.push_back(st.empty() ? Value(0) : cum.back() + orientationDeterminant(p, q[st.back()], q[i]));
            st.push_back(i);
            w.pre[i] = cum.back();
        }
        st.clear();
        cum.clear();
        for (std::uint32_t j = m; j-- > 0;) {
            while (st.size() >= 2 && !(orientationSign(q[st[st.size() - 2]], q[st.back()], q[j]) < 0)) {
                st.pop_back();
                cum.pop_back();
            }
            cum.push_back(st.empty() ? Value(0) : cum.back() + orientationDeterminant(p, q[j], q[st.back()]));
            st.push_back(j);
            w.suf[j] = cum.back();
        }
    }
}

// Stable counting sort of index pairs by their first or second index.
template <bool BySecond>
void countingSortPairs(std::vector<std::pair<std::uint32_t, std::uint32_t>>& v,
                       std::vector<std::pair<std::uint32_t, std::uint32_t>>& tmp,
                       std::vector<std::uint32_t>& count, std::size_t m) {
    count.assign(m + 1, 0);
    for (const auto& e : v)
        ++count[(BySecond ? e.second : e.first) + 1];
    for (std::size_t i = 0; i < m; ++i)
        count[i + 1] += count[i];
    tmp.resize(v.size());
    for (const auto& e : v)
        tmp[count[BySecond ? e.second : e.first]++] = e;
    v.swap(tmp);
}

/*
 * The best polygon at one anchor, raising best (with found set) when it beats
 * it. Exact for the anchor of the optimum; elsewhere it may settle for less.
 *
 * Pocket lemma: if z a b c are consecutive vertices of an optimal polygon, no
 * point lies beyond the line ab where inserting it between a and b keeps the
 * polygon convex (turn(z, a, x) > 0 and turn(x, b, c) > 0): the one nearest
 * to the line would span an empty triangle a x b, and inserting it gives a
 * larger empty convex polygon.
 *
 * Hull theorem: let K be the hull of p and any of the fan points; every point
 * is a corner (strict vertex of K), inner (in K) or outer (outside K). If ab is
 * an edge of the optimal polygon at p with a and b corners not adjacent on K,
 * then z or c is outer. For if z is in K, the line za meets the boundary of K
 * again only past every corner from a to b, so turn(z, a, b-) > 0 for the
 * corner b- before b; and if c is in K, it lies left of the edge b- b and not
 * on it, so turn(b-, b, c) > 0; then b- is in the pocket of ab.
 *
 * So with few points off K, the dynamic program only needs the edges of K, the
 * pairs with an inner or outer endpoint, and the chords between corners next
 * to an outer vertex: for z outer before a blocking b- (the sign of
 * turn(z, a, .) along K switches once, so b is unique: a binary search), the
 * mirror case for c outer after b, and z blocking a+ while c blocks b-, cut by
 * the pair bound, which only falls as b moves along K. With many points off K
 * (random-like input), the anchor runs the plain scan.
 */
//
// In rebuild mode best is the final score: the anchor keeps the pairs whose
// bound reaches it (those of the optimal polygon among them) and records its
// own best pair in w.top*, with the parents to trace the polygon back.
template <class Score, class PointType, class Value>
void largestEmptyConvexAtAnchor(const PointType& p, EmptyConvexWork<PointType, Value>& w,
                                bool usePairBound, bool alwaysRestrict,
                                const std::vector<std::uint8_t>& isOuter, Value& best, bool& found,
                                bool rebuild = false) {
    const std::vector<PointType>& q = w.q;
    const std::uint32_t m = static_cast<std::uint32_t>(q.size());
    if (usePairBound)
        emptyConvexPairBounds<Score>(p, w);
    auto keep = [&](std::uint32_t i, std::uint32_t j) {
        if (!usePairBound)
            return true;
        const Value bound = w.pre[i] + Score::gain(p, q[i], q[j]) + w.suf[j];
        return rebuild ? !(bound < best) : best < bound;
    };
    auto record = [&](std::uint32_t i, std::uint32_t j, const Value& v) {
        if (rebuild) {
            if (!w.topFound || w.top < v) {
                w.top = v;
                w.topI = i;
                w.topJ = j;
                w.topFound = true;
            }
        } else if (!found || best < v) {
            best = v;
            found = true;
        }
    };
    w.scanned = false;
    w.topFound = false;
    auto scan = [&] {
        w.scanned = true;
        emptyConvexScan<Score>(p, w, record, keep);
    };

    // K, by Graham's scan over the fan without the outer points (p is
    // extreme): its corners in counterclockwise order are kv[1..r], kv[0]
    // standing for p. Every fan point off K costs a sweep; once more than an
    // eighth are off, give up on K (unless alwaysRestrict, for the tests).
    auto& kv = w.stack;
    auto at = [&](std::uint32_t i) -> const PointType& { return i == emptyConvexNone ? p : q[i]; };
    const std::uint32_t maxOff = alwaysRestrict ? m : m / 8;
    std::uint32_t off = 0;
    kv.clear();
    kv.push_back(emptyConvexNone);
    for (std::uint32_t i = 0; i < m; ++i) {
        if (isOuter[w.ids[i]]) {
            ++off;
            continue;
        }
        while (kv.size() >= 2 && !(orientationSign(at(kv[kv.size() - 2]), at(kv.back()), q[i]) > 0)) {
            kv.pop_back();
            ++off;
        }
        if (off > maxOff)
            return scan();
        kv.push_back(i);
    }
    const std::uint32_t r = static_cast<std::uint32_t>(kv.size()) - 1;
    if (r < 2)
        return scan();
    w.kpos.assign(m, 0);
    for (std::uint32_t t = 1; t <= r; ++t)
        w.kpos[kv[t]] = t;

    // Classes: 0 inner, 1 corner, 2 outer. A point outer for the whole set can
    // still fall inside this K.
    w.cls.assign(m, 0);
    w.inner.clear();
    w.outer.clear();
    for (std::uint32_t i = 0, t = 0; i < m; ++i) {
        if (t < r && kv[t + 1] == i) {
            ++t;
            w.cls[i] = 1;
            continue;
        }
        const bool outside = isOuter[w.ids[i]] &&
                             (t == 0 || t == r || orientationSign(q[kv[t]], q[kv[t + 1]], q[i]) < 0);
        w.cls[i] = outside ? 2 : 0;
        (outside ? w.outer : w.inner).push_back(i);
    }
    auto& innerBefore = w.count;  // inner points with fan index < i
    innerBefore.assign(m + 1, 0);
    for (std::uint32_t i = 0; i < m; ++i)
        innerBefore[i + 1] = innerBefore[i] + (w.cls[i] == 0 ? 1 : 0);

    w.pairs.clear();
    w.aItems.clear();
    w.bItems.clear();
    auto add = [&](std::uint32_t i, std::uint32_t j) {
        if (keep(i, j))
            w.pairs.emplace_back(i, j);
    };
    // Edges of K with no inner point between their ends (outer ones lie beyond).
    for (std::uint32_t t = 1; t < r; ++t)
        if (innerBefore[kv[t + 1]] == innerBefore[kv[t] + 1])
            add(kv[t], kv[t + 1]);
    // Every empty pair with an inner or outer endpoint: p q_u q_j is empty when
    // q_j is strictly left of the most counterclockwise ray q_u q_l, u < l < j
    // (and mirrored for the pairs ending at u, first endpoints on K only).
    auto sweep = [&](std::uint32_t u) {
        const bool outer = w.cls[u] == 2;
        for (std::uint32_t j = u + 1, b = u + 1; j < m; ++j) {
            if (j > u + 1 && !(orientationSign(q[u], q[b], q[j]) > 0))
                continue;
            b = j;
            if (keep(u, j)) {
                w.pairs.emplace_back(u, j);
                if (outer && w.cls[j] == 1)
                    w.aItems.emplace_back(u, j);
            }
        }
        if (u == 0)
            return;
        for (std::uint32_t k = u, b = u - 1; k-- > 0;) {
            if (k + 1 < u && !(orientationSign(q[u], q[b], q[k]) < 0))
                continue;
            b = k;
            if (w.cls[k] == 1 && keep(k, u)) {
                w.pairs.emplace_back(k, u);
                if (outer)
                    w.bItems.emplace_back(k, u);
            }
        }
    };
    for (const std::uint32_t u : w.inner)
        sweep(u);
    for (const std::uint32_t u : w.outer)
        sweep(u);

    // Each item below costs a binary search; past m^2 / log m of them that
    // would exceed the plain scan's O(m^2), so run that instead.
    std::size_t logM = 1;
    while ((std::size_t(1) << logM) < m)
        ++logM;
    if ((w.aItems.size() + w.bItems.size()) * logM > std::size_t(m) * m)
        return scan();

    if (!w.outer.empty()) {
        w.chords.clear();
        w.aFlag.assign(r + 2, 0);
        w.bFlag.assign(r + 2, 0);
        // z outer before a: the corners after a that z blocks are a prefix,
        // and b is the first corner past it.
        for (const auto& [z, a] : w.aItems) {
            const std::uint32_t ta = w.kpos[a];
            if (ta >= r || orientationSign(q[z], q[a], q[kv[ta + 1]]) > 0)
                continue;
            w.aFlag[ta] = 1;
            std::uint32_t lo = ta + 1, hi = r + 1;
            while (lo + 1 < hi) {
                const std::uint32_t mid = lo + (hi - lo) / 2;
                if (orientationSign(q[z], q[a], q[kv[mid]]) > 0)
                    hi = mid;
                else
                    lo = mid;
            }
            if (hi <= r)
                w.chords.emplace_back(ta, hi);
        }
        // c outer after b: the corners before b that c blocks are a suffix,
        // and a is the last corner before it.
        for (const auto& [b, c] : w.bItems) {
            const std::uint32_t tb = w.kpos[b];
            if (tb <= 1 || orientationSign(q[kv[tb - 1]], q[b], q[c]) > 0)
                continue;
            w.bFlag[tb] = 1;
            std::uint32_t lo = 0, hi = tb - 1;
            while (lo + 1 < hi) {
                const std::uint32_t mid = lo + (hi - lo) / 2;
                if (orientationSign(q[kv[mid]], q[b], q[c]) > 0)
                    lo = mid;
                else
                    hi = mid;
            }
            if (lo >= 1)
                w.chords.emplace_back(lo, tb);
        }
        // Both ends blocked from outside: b runs along K while the bound can win.
        w.bList.clear();
        for (std::uint32_t t = 1; t <= r; ++t)
            if (w.bFlag[t])
                w.bList.push_back(t);
        for (std::uint32_t ta = 1; ta <= r; ++ta) {
            if (!w.aFlag[ta])
                continue;
            for (auto it = std::lower_bound(w.bList.begin(), w.bList.end(), ta + 2);
                 it != w.bList.end() && keep(kv[ta], kv[*it]); ++it)
                w.chords.emplace_back(ta, *it);
        }
        // Only inner points between its ends can lie in front of a chord.
        // Chords from one corner are checked against them while that costs at
        // most m steps in all, else by the corner's row sweep, in O(m).
        countingSortPairs<true>(w.chords, w.tmp, w.outStart, r + 1);
        countingSortPairs<false>(w.chords, w.tmp, w.outStart, r + 1);
        w.chords.erase(std::unique(w.chords.begin(), w.chords.end()), w.chords.end());
        for (std::size_t g = 0; g < w.chords.size();) {
            const std::uint32_t ta = w.chords[g].first, a = kv[ta];
            std::size_t gEnd = g;
            std::size_t cost = 0;
            for (; gEnd < w.chords.size() && w.chords[gEnd].first == ta; ++gEnd)
                cost += innerBefore[kv[w.chords[gEnd].second]] - innerBefore[a + 1];
            if (cost <= m) {
                for (std::size_t e = g; e < gEnd; ++e) {
                    const std::uint32_t b = kv[w.chords[e].second];
                    if (w.chords[e].second < ta + 2 || !keep(a, b))
                        continue;
                    // inner is sorted, so innerBefore gives the range in it.
                    bool empty = true;
                    for (std::uint32_t x = innerBefore[a + 1]; empty && x < innerBefore[b]; ++x)
                        empty = orientationSign(q[a], q[b], q[w.inner[x]]) < 0;
                    if (empty)
                        w.pairs.emplace_back(a, b);
                }
            } else {
                std::size_t e = g;
                for (std::uint32_t j = a + 1, b = a + 1; j < m && e < gEnd; ++j) {
                    const bool empty = j == a + 1 || orientationSign(q[a], q[b], q[j]) > 0;
                    if (empty)
                        b = j;
                    while (e < gEnd && kv[w.chords[e].second] < j)
                        ++e;
                    if (e < gEnd && kv[w.chords[e].second] == j) {
                        if (empty && w.chords[e].second >= ta + 2 && keep(a, j))
                            w.pairs.emplace_back(a, j);
                        ++e;
                    }
                }
            }
            g = gEnd;
        }
    }

    // The dynamic program of the scan over these pairs, with explicit lists:
    // the convex-turn prefix property holds for any set of empty pairs.
    countingSortPairs<true>(w.pairs, w.tmp, w.count, m);
    countingSortPairs<false>(w.pairs, w.tmp, w.count, m);
    w.pairs.erase(std::unique(w.pairs.begin(), w.pairs.end()), w.pairs.end());
    w.outStart.assign(m + 1, 0);
    w.inStart.assign(m + 1, 0);
    for (const auto& [i, j] : w.pairs) {
        ++w.outStart[i + 1];
        ++w.inStart[j + 1];
    }
    for (std::uint32_t i = 0; i < m; ++i) {
        w.outStart[i + 1] += w.outStart[i];
        w.inStart[i + 1] += w.inStart[i];
    }
    w.inFill.assign(w.inStart.begin(), w.inStart.end() - 1);
    w.in.resize(w.pairs.size());
    for (std::uint32_t i = 0; i < m; ++i) {
        const std::uint32_t inEnd = w.inFill[i];
        std::uint32_t t = w.inStart[i];
        Value bestAt = Score::start();
        std::uint32_t bestArg = emptyConvexNone;
        for (std::uint32_t e = w.outStart[i]; e < w.outStart[i + 1]; ++e) {
            const std::uint32_t j = w.pairs[e].second;
            while (t < inEnd && orientationSign(q[w.in[t].first], q[i], q[j]) > 0) {
                if (bestAt < w.in[t].value) {
                    bestAt = w.in[t].value;
                    bestArg = w.in[t].first;
                }
                ++t;
            }
            const Value v = Score::gain(p, q[i], q[j]) + bestAt;
            record(i, j, v);
            w.in[w.inFill[j]++] = {i, bestArg, v};
        }
    }
}

// Twice the area of the hull of pts[a..n) for every a, pts sorted: adding the
// points from the right, each new point is the leftmost and extends both
// monotone chains like a stack. The chains are measured as fans from the
// rightmost point. O(n).
template <class PointType, class Value>
std::vector<Value> suffixHullTwiceAreas(const std::vector<PointType>& pts) {
    const std::size_t n = pts.size();
    std::vector<Value> out(n, Value(0));
    if (n == 0)
        return out;
    const PointType& o = pts.back();
    std::vector<std::uint32_t> lo, up;
    std::vector<Value> loSum, upSum;
    for (std::size_t a = n; a-- > 0;) {
        const auto p = static_cast<std::uint32_t>(a);
        while (lo.size() >= 2 && !(orientationSign(pts[p], pts[lo.back()], pts[lo[lo.size() - 2]]) > 0)) {
            lo.pop_back();
            loSum.pop_back();
        }
        loSum.push_back(lo.empty() ? Value(0) : loSum.back() + orientationDeterminant(o, pts[p], pts[lo.back()]));
        lo.push_back(p);
        while (up.size() >= 2 && !(orientationSign(pts[p], pts[up.back()], pts[up[up.size() - 2]]) < 0)) {
            up.pop_back();
            upSum.pop_back();
        }
        upSum.push_back(up.empty() ? Value(0) : upSum.back() + orientationDeterminant(o, pts[up.back()], pts[p]));
        up.push_back(p);
        out[a] = loSum.back() + upSum.back();
    }
    return out;
}

/*
 * Outer points: vertices of the hull that hide other points (strictly beyond
 * or on the chord between their two hull neighbours), peeled round after
 * round, biggest hiders first and never two neighbours in one round, while
 * each round grows the hull by at least as many points as it peels (random
 * input stops at once). Only the speed depends on this choice.
 */
template <class PointType>
std::vector<std::uint8_t> emptyConvexOuterPoints(const std::vector<PointType>& pts) {
    const std::size_t n = pts.size();
    std::vector<std::uint8_t> isOuter(n, 0);
    std::vector<std::uint32_t> hull, lower, upper, peeled, order, hides;
    std::vector<std::uint8_t> onHull(n), taken;
    std::size_t lastHull = 0;
    for (int round = 0; round <= 64; ++round) {
        lower.clear();
        upper.clear();
        for (std::uint32_t i = 0; i < n; ++i) {
            if (isOuter[i])
                continue;
            while (lower.size() >= 2 &&
                   !(orientationSign(pts[lower[lower.size() - 2]], pts[lower.back()], pts[i]) > 0))
                lower.pop_back();
            lower.push_back(i);
            while (upper.size() >= 2 &&
                   !(orientationSign(pts[upper[upper.size() - 2]], pts[upper.back()], pts[i]) < 0))
                upper.pop_back();
            upper.push_back(i);
        }
        if (lower.size() < 2)
            break;
        hull = lower;
        for (std::size_t t = upper.size() - 1; t-- > 1;)
            hull.push_back(upper[t]);
        const std::size_t h = hull.size();
        if (round > 0 && h < lastHull + peeled.size()) {
            for (const std::uint32_t v : peeled)
                isOuter[v] = 0;
            break;
        }
        lastHull = h;
        if (h < 4 || round == 64)
            break;
        std::fill(onHull.begin(), onHull.end(), 0);
        for (const std::uint32_t v : hull)
            onHull[v] = 1;
        // The hull vertex v whose neighbours' chord a point y is beyond is an
        // end of the hull edge that the ray from hull[0] through y leaves by,
        // or hull[0] itself.
        hides.assign(h, 0);
        auto hiddenBy = [&](std::size_t t, std::uint32_t y) {
            return !(orientationSign(pts[hull[(t + h - 1) % h]], pts[hull[(t + 1) % h]], pts[y]) > 0);
        };
        for (std::uint32_t y = 0; y < n; ++y) {
            if (isOuter[y] || onHull[y])
                continue;
            std::size_t lo = 1, hi = h - 1;  // last t with y left of hull[0] -> hull[t]
            while (lo + 1 < hi) {
                const std::size_t mid = lo + (hi - lo) / 2;
                if (orientationSign(pts[hull[0]], pts[hull[mid]], pts[y]) >= 0)
                    lo = mid;
                else
                    hi = mid;
            }
            for (const std::size_t t : {std::size_t(0), lo, lo + 1})
                if (hiddenBy(t, y))
                    ++hides[t];
        }
        order.resize(h);
        for (std::uint32_t t = 0; t < h; ++t)
            order[t] = t;
        std::sort(order.begin(), order.end(),
                  [&](std::uint32_t s, std::uint32_t t) { return hides[s] > hides[t]; });
        taken.assign(h, 0);
        peeled.clear();
        for (const std::uint32_t t : order) {
            if (hides[t] == 0 || taken[(t + h - 1) % h] || taken[(t + 1) % h])
                continue;
            taken[t] = 1;
            isOuter[hull[t]] = 1;
            peeled.push_back(hull[t]);
        }
        if (peeled.empty())
            break;
    }
    return isOuter;
}

// pts sorted and distinct. alwaysRestrict takes the restricted path at every
// anchor, however many points are off K: slower, but it lets small tests reach
// every case of it.
template <class Score, class PointType>
Convex<PointType> largestEmptyConvex(const std::vector<PointType>& pts,
                                                    bool alwaysRestrict = false) {
    using Value = typename Score::Value;
    const std::size_t n = pts.size();
    // With no triangle (fewer than three points, or all collinear), the
    // answer is the longest empty segment: the two consecutive points of the
    // sorted set farthest apart, or the single point, or nothing.
    auto noTriangle = [&pts] {
        if (pts.size() < 2)
            return Convex<PointType>(pts, trusted);
        std::size_t best = 0;
        for (std::size_t i = 1; i + 1 < pts.size(); ++i)
            if (pts[best].squaredDistance(pts[best + 1]) < pts[i].squaredDistance(pts[i + 1]))
                best = i;
        return Convex<PointType>(std::vector<PointType>{pts[best], pts[best + 1]}, trusted);
    };
    if (n < 3)
        return noTriangle();

    // Anchors in lexicographic order. Anchor a's polygons lie in the hull of
    // pts[a..n), so twice its area, or n - a vertices, bounds them; the bound
    // never grows with a, so the loop stops at the first that cannot win.
    std::vector<Value> bound;
    if constexpr (std::is_same_v<Score, EmptyConvexVertexScore<PointType>>) {
        bound.resize(n);
        for (std::size_t a = 0; a < n; ++a)
            bound[a] = n - a;
    } else {
        bound = suffixHullTwiceAreas<PointType, Value>(pts);
    }
    const std::vector<std::uint8_t> isOuter = emptyConvexOuterPoints(pts);
    EmptyConvexWork<PointType, Value> w;
    Value best = Score::start();
    bool found = false;
    std::size_t bestAnchor = n;
    for (std::size_t a = 0; a + 2 < n; ++a) {
        if (found && !(best < bound[a]))
            break;
        emptyConvexFan(pts, a, w);
        if (w.q.size() < 2)
            continue;
        // The pair bound prunes nothing while the best polygon is small next
        // to what the anchor could hold; skip its cost then.
        const bool usePairBound = found && bound[a] < best * Value(4);
        const Value before = best;
        const bool foundBefore = found;
        largestEmptyConvexAtAnchor<Score>(pts[a], w, usePairBound, alwaysRestrict, isOuter, best,
                                          found);
        if (found && (!foundBefore || before < best))
            bestAnchor = a;
    }
    if (!found)
        return noTriangle();

    // Rebuild the polygon: run the best anchor again, keeping every pair whose
    // bound reaches the best score, then follow the best predecessors back
    // from its best pair. The pairs ending at j are sorted by first index.
    emptyConvexFan(pts, bestAnchor, w);
    const PointType& p = pts[bestAnchor];
    largestEmptyConvexAtAnchor<Score>(p, w, true, alwaysRestrict, isOuter, best, found, true);
    PGL_ASSERT(w.topFound);
    auto parentOf = [&w](std::uint32_t i, std::uint32_t j) {
        auto byFirst = [](const auto& e, std::uint32_t f) { return e.first < f; };
        if (w.scanned) {
            const auto& list = w.incoming[j];
            return std::lower_bound(list.begin(), list.end(), i, byFirst)->parent;
        }
        const auto begin = w.in.begin() + w.inStart[j], end = w.in.begin() + w.inFill[j];
        return std::lower_bound(begin, end, i, byFirst)->parent;
    };
    std::vector<PointType> ring{w.q[w.topJ]};
    for (std::uint32_t i = w.topI, j = w.topJ;;) {
        ring.push_back(w.q[i]);
        const std::uint32_t k = parentOf(i, j);
        if (k == emptyConvexNone)
            break;
        j = i;
        i = k;
    }
    ring.push_back(p);
    std::reverse(ring.begin(), ring.end());
    return Convex<PointType>(ring, trusted);
}

}  // namespace detail

/**
 * @brief Returns an empty convex polygon of @p points with the largest area.
 *
 * The polygon has its vertices in @p points, no other point of @p points in
 * it or on its boundary, and no straight angle. Ties are broken arbitrarily.
 * For floating-point coordinates the areas are compared approximately.
 *
 * The search runs the dynamic program of Dobkin, Edelsbrunner and Overmars
 * ("Searching for empty convex polygons", 1990) from each lexicographically
 * smallest vertex in turn, restricted to the edges an optimal polygon can use:
 * an optimal polygon admits no point that would keep it convex, so its edges
 * between two vertices of the hull of the remaining points are hull edges
 * unless a neighbouring vertex lies outside that hull. Bounds from the hulls of
 * the points still ahead prune vertices and edges that cannot win.
 *
 * @complexity O(n^3) time and O(n^2) space for `n` points.
 *
 * @param points Container of points.
 * @return The polygon. If @p points has no three non-collinear points, the
 *         longest empty segment: two consecutive points of @p points along
 *         its line (one point if there is only one, none if @p points is
 *         empty).
 */
template <class Container>
    requires detail::EmptyPolygonsPointRange<Container>
Convex<detail::empty_polygons_point_t<Container>>
maximumAreaEmptyConvex(const Container& points) {
    using PointType = detail::empty_polygons_point_t<Container>;
    return detail::largestEmptyConvex<detail::EmptyConvexAreaScore<PointType>>(
        detail::distinctPointsOf(points));
}

/**
 * @brief Returns an empty convex polygon of @p points with the most vertices.
 *
 * As @ref maximumAreaEmptyConvex, maximizing the number of vertices instead of
 * the area.
 *
 * @complexity O(n^3) time and O(n^2) space for `n` points.
 *
 * @param points Container of points.
 * @return The polygon. If @p points has no three non-collinear points, the
 *         longest empty segment: two consecutive points of @p points along
 *         its line (one point if there is only one, none if @p points is
 *         empty).
 */
template <class Container>
    requires detail::EmptyPolygonsPointRange<Container>
Convex<detail::empty_polygons_point_t<Container>>
maximumVerticesEmptyConvex(const Container& points) {
    using PointType = detail::empty_polygons_point_t<Container>;
    return detail::largestEmptyConvex<detail::EmptyConvexVertexScore<PointType>>(
        detail::distinctPointsOf(points));
}

}  // namespace pgl
