#pragma once

#include "implementation/minkowskierosion.hpp"

/**
 * @file emptypolygons.hpp
 * @brief Empty triangles and quadrilaterals of a point set.
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

}  // namespace pgl
