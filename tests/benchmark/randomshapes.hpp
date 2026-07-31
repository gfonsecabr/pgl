#pragma once
#include "pgl.hpp"

#include <algorithm>
#include <set>
#include <vector>

constexpr int largeRange = 10000;
constexpr int smallRange = 1000;

struct Rng {
    std::uint64_t state;
    std::uint64_t next() {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        return state >> 33;
    }
    int range(int hi) {
        return static_cast<int>(next() % static_cast<std::uint64_t>(hi + 1));
    }
};

template <class Number>
pgl::Point<Number> randomPoint(Rng& rng, int range) {
    auto center = pgl::Point<Number>(range/2, range/2);
    pgl::Disk<pgl::Point<Number>> disk(center, Number(range/2));
    while (true) {
        const int x = rng.range(range);
        const int y = rng.range(range);
        const auto p = pgl::Point<Number>(Number(x), Number(y));
        if (disk.contains(p)) {
            return p - center;
        }
    }
    return pgl::Point<Number>(0, 0); // unreachable
}


template <class Number>
std::vector<pgl::Point<Number>> randomPoints(int n) {
    using Point = pgl::Point<Number>;
    Rng rng{0};
    std::vector<Point> v;
    std::set<Point> seen;
    v.reserve(static_cast<std::size_t>(n));
    while (static_cast<int>(v.size()) < n) {
        const auto p = randomPoint<Number>(rng, largeRange);
        if (seen.insert(p).second) {
            v.emplace_back(p);
        }
    }
    return v;
}

template <class S>
std::vector<S> randomSmallBishape(int n) {
    using Number = typename S::NumberType;
    std::vector<S> w;
    std::set<S> seen;
    Rng rng{static_cast<std::uint64_t>(pgl::detail::shapeRank<S>)};
    while (static_cast<int>(w.size()) < n) {
        const auto p1 = randomPoint<Number>(rng, largeRange);
        const auto p2 = p1 + randomPoint<Number>(rng, smallRange);
        S s(p1, p2);
        if (!s.isDegenerate() && seen.insert(s).second) {
            w.emplace_back(s);
        }
    }
    return w;
}


template <class S>
std::vector<S> randomLargeBishape(int n) {
    using Number = typename S::NumberType;
    std::vector<S> w;
    std::set<S> seen;
    Rng rng{static_cast<std::uint64_t>(pgl::detail::shapeRank<S>)};
    while (static_cast<int>(w.size()) < n) {
        const auto p1 = randomPoint<Number>(rng, largeRange);
        const auto p2 = randomPoint<Number>(rng, largeRange);
        S s(p1, p2);
        if (!s.isDegenerate() && seen.insert(s).second) {
            w.emplace_back(s);
        }
    }
    return w;
}

template <class S>
std::vector<S> randomSmallTrishape(int n) {
    using Number = typename S::NumberType;
    std::vector<S> w;
    std::set<S> seen;
    Rng rng{static_cast<std::uint64_t>(pgl::detail::shapeRank<S>)};
    while (static_cast<int>(w.size()) < n) {
        const auto p1 = randomPoint<Number>(rng, largeRange);
        const auto p2 = p1 + randomPoint<Number>(rng, smallRange);
        const auto p3 = p1 + randomPoint<Number>(rng, smallRange);
        S s(p1, p2, p3);
        if (!s.isDegenerate() && seen.insert(s).second) {
            w.emplace_back(s);
        }
    }
    return w;
}

template <class S>
std::vector<S> randomLargeTrishape(int n) {
    using Number = typename S::NumberType;
    std::vector<S> w;
    std::set<S> seen;
    Rng rng{static_cast<std::uint64_t>(pgl::detail::shapeRank<S>)};
    while (static_cast<int>(w.size()) < n) {
        const auto p1 = randomPoint<Number>(rng, largeRange);
        const auto p2 = randomPoint<Number>(rng, largeRange);
        const auto p3 = randomPoint<Number>(rng, largeRange);
        S s(p1, p2, p3);
        if (!s.isDegenerate() && seen.insert(s).second) {
            w.emplace_back(s);
        }
    }
    return w;
}

template <class Number>
std::vector<pgl::Convex<pgl::Point<Number>>> randomSmallConvexes(int n, int m) {
    using Point = pgl::Point<Number>;
    using Convex = pgl::Convex<Point>;
    std::vector<Convex> w;
    std::set<Convex> seen;
    Rng rng{static_cast<std::uint64_t>(pgl::detail::shapeRank<pgl::Convex<Point>>)};
    while (static_cast<int>(w.size()) < n) {
        const auto p1 = randomPoint<Number>(rng, largeRange);
        std::vector<Point> points;
        for (int i = 0; i < m; ++i) {
            points.push_back(randomPoint<Number>(rng, smallRange));
        }
        Convex s(points);
        if (!s.isDegenerate()) {
            const auto shifted = p1 + s;
            if (seen.insert(shifted).second) {
                w.push_back(shifted);
            }
        }
    }
    return w;
}

template <class Number>
std::vector<pgl::Convex<pgl::Point<Number>>> randomLargeConvexes(int n, int m) {
    using Point = pgl::Point<Number>;
    using Convex = pgl::Convex<Point>;
    std::vector<Convex> w;
    std::set<Convex> seen;
    Rng rng{static_cast<std::uint64_t>(pgl::detail::shapeRank<pgl::Convex<Point>>)};
    while (static_cast<int>(w.size()) < n) {
        std::vector<Point> points;
        for (int i = 0; i < m; ++i) {
            points.push_back(randomPoint<Number>(rng, largeRange));
        }
        Convex s(points);
        if (!s.isDegenerate() && seen.insert(s).second) {
            w.push_back(s);
        }
    }
    return w;
}

// Simple polygons: m random points fed to Polygon in generation (random) order,
// then Polygon::untangle() removes crossings (2-opt) to make it simple. untangle
// may drop redundant vertices, so the result has at most m vertices.
template <class Number>
std::vector<pgl::Polygon<pgl::Point<Number>>> randomSmallPolygons(int n, int m) {
    using Point = pgl::Point<Number>;
    using Polygon = pgl::Polygon<Point>;
    std::vector<Polygon> w;
    std::set<Polygon> seen;
    Rng rng{static_cast<std::uint64_t>(pgl::detail::shapeRank<pgl::Polygon<Point>>)};
    while (static_cast<int>(w.size()) < n) {
        const auto base = randomPoint<Number>(rng, largeRange);
        std::vector<Point> points;
        for (int i = 0; i < m; ++i) {
            points.push_back(base + randomPoint<Number>(rng, smallRange));
        }
        Polygon poly(points, true);  // trusted: untangle renormalizes at the end
        poly.untangle();
        if (!poly.isDegenerate() && seen.insert(poly).second) {
            w.push_back(poly);
        }
    }
    return w;
}

template <class Number>
std::vector<pgl::Polygon<pgl::Point<Number>>> randomLargePolygons(int n, int m) {
    using Point = pgl::Point<Number>;
    using Polygon = pgl::Polygon<Point>;
    std::vector<Polygon> w;
    std::set<Polygon> seen;
    Rng rng{static_cast<std::uint64_t>(pgl::detail::shapeRank<pgl::Polygon<Point>>)};
    while (static_cast<int>(w.size()) < n) {
        std::vector<Point> points;
        for (int i = 0; i < m; ++i) {
            points.push_back(randomPoint<Number>(rng, largeRange));
        }
        Polygon poly(points, true);  // trusted: untangle renormalizes at the end
        poly.untangle();
        if (!poly.isDegenerate() && seen.insert(poly).second) {
            w.push_back(poly);
        }
    }
    return w;
}

// Holes for a simple polygon: each candidate is holeVertices random points
// drawn from a disk a fifth of the polygon's own span wide, centred anywhere in
// the polygon's own disk and untangled into a simple ring — so a hole is
// usually non-convex (about 90% of them are). A candidate is kept when it lies
// inside the polygon and its interior misses every hole kept so far, which is
// exactly the PolygonWithHoles precondition, established by construction rather
// than checked afterwards. Boundary contact is allowed, as the shape allows it.
//
// A fifth of the span is the largest hole that still fits on the first few
// tries: it takes about a tenth of the polygon's area away, so queries really
// do run into the holes, while barely any polygon has to be discarded below.
//
// Returns fewer than holeCount holes when the attempt budget runs out; a thin
// or spiky polygon may simply have no room for them, and the caller then throws
// the polygon away rather than looping forever.
template <class Number>
std::vector<pgl::Polygon<pgl::Point<Number>>>
randomHoles(const pgl::Polygon<pgl::Point<Number>>& outer, const pgl::Point<Number>& base,
            int range, Rng& rng, int holeCount, int holeVertices, int budget) {
    using Point = pgl::Point<Number>;
    using Polygon = pgl::Polygon<Point>;
    std::vector<Polygon> holes;
    for (int attempt = 0; attempt < budget && static_cast<int>(holes.size()) < holeCount; ++attempt) {
        const auto center = base + randomPoint<Number>(rng, range);
        std::vector<Point> points;
        for (int i = 0; i < holeVertices; ++i) {
            points.push_back(center + randomPoint<Number>(rng, range / 5));
        }
        Polygon hole(points, true);  // trusted: untangle renormalizes at the end
        hole.untangle();
        if (hole.isDegenerate() || !outer.contains(hole)) {
            continue;
        }
        const bool overlaps = std::any_of(holes.begin(), holes.end(),
                                          [&](const Polygon& kept) {
                                              return hole.interiorsIntersect(kept);
                                          });
        if (!overlaps) {
            holes.push_back(hole);
        }
    }
    return holes;
}

// Simple polygons with holes: a polygon generated exactly as randomXxxPolygons
// does, then punched with holeCount holes of holeVertices vertices each. The
// outer ring keeps at most m vertices and every hole at most holeVertices, so a
// region has at most m + holeCount * holeVertices vertices in total. Polygons
// with no room for all the holes are discarded, so every region really has
// holeCount of them.
template <class Number>
std::vector<pgl::PolygonWithHoles<pgl::Point<Number>>>
randomSmallPolygonsWithHoles(int n, int m, int holeCount, int holeVertices) {
    using Point = pgl::Point<Number>;
    using Polygon = pgl::Polygon<Point>;
    using PolygonWithHoles = pgl::PolygonWithHoles<Point>;
    std::vector<PolygonWithHoles> w;
    std::set<PolygonWithHoles> seen;
    Rng rng{static_cast<std::uint64_t>(pgl::detail::shapeRank<pgl::PolygonWithHoles<Point>>)};
    while (static_cast<int>(w.size()) < n) {
        const auto base = randomPoint<Number>(rng, largeRange);
        std::vector<Point> points;
        for (int i = 0; i < m; ++i) {
            points.push_back(base + randomPoint<Number>(rng, smallRange));
        }
        Polygon poly(points, true);  // trusted: untangle renormalizes at the end
        poly.untangle();
        if (poly.isDegenerate()) {
            continue;
        }
        const auto holes = randomHoles<Number>(poly, base, smallRange, rng,
                                               holeCount, holeVertices, 100 * holeCount);
        if (static_cast<int>(holes.size()) < holeCount) {
            continue;
        }
        PolygonWithHoles region(poly, holes);
        if (seen.insert(region).second) {
            w.push_back(region);
        }
    }
    return w;
}

template <class Number>
std::vector<pgl::PolygonWithHoles<pgl::Point<Number>>>
randomLargePolygonsWithHoles(int n, int m, int holeCount, int holeVertices) {
    using Point = pgl::Point<Number>;
    using Polygon = pgl::Polygon<Point>;
    using PolygonWithHoles = pgl::PolygonWithHoles<Point>;
    std::vector<PolygonWithHoles> w;
    std::set<PolygonWithHoles> seen;
    Rng rng{static_cast<std::uint64_t>(pgl::detail::shapeRank<pgl::PolygonWithHoles<Point>>)};
    while (static_cast<int>(w.size()) < n) {
        std::vector<Point> points;
        for (int i = 0; i < m; ++i) {
            points.push_back(randomPoint<Number>(rng, largeRange));
        }
        Polygon poly(points, true);  // trusted: untangle renormalizes at the end
        poly.untangle();
        if (poly.isDegenerate()) {
            continue;
        }
        const auto holes = randomHoles<Number>(poly, Point(0, 0), largeRange, rng,
                                               holeCount, holeVertices, 100 * holeCount);
        if (static_cast<int>(holes.size()) < holeCount) {
            continue;
        }
        PolygonWithHoles region(poly, holes);
        if (seen.insert(region).second) {
            w.push_back(region);
        }
    }
    return w;
}

// "As-other-type" generators: build shapes with one shape's generator, then
// store the equivalent representation of a more general storage type. This lets
// the shape-pair cube measure, e.g., Polygon's code paths when the polygon is
// actually a triangle. Generation (and thus the small/large geometry and the
// dedup seed) matches the source shape exactly; only the container type differs.

// Triangles, generated as Triangle, stored as Polygon (via Triangle::asPolygon).
template <class Number>
std::vector<pgl::Polygon<pgl::Point<Number>>> randomSmallTriangleAsPolygon(int n) {
    using Point = pgl::Point<Number>;
    auto tris = randomSmallTrishape<pgl::Triangle<Point>>(n);
    std::vector<pgl::Polygon<Point>> w;
    w.reserve(tris.size());
    for (const auto& t : tris) w.push_back(t.asPolygon());
    return w;
}

template <class Number>
std::vector<pgl::Polygon<pgl::Point<Number>>> randomLargeTriangleAsPolygon(int n) {
    using Point = pgl::Point<Number>;
    auto tris = randomLargeTrishape<pgl::Triangle<Point>>(n);
    std::vector<pgl::Polygon<Point>> w;
    w.reserve(tris.size());
    for (const auto& t : tris) w.push_back(t.asPolygon());
    return w;
}

// Triangles, generated as Triangle, stored as Convex (via Triangle::asConvex).
template <class Number>
std::vector<pgl::Convex<pgl::Point<Number>>> randomSmallTriangleAsConvex(int n) {
    using Point = pgl::Point<Number>;
    auto tris = randomSmallTrishape<pgl::Triangle<Point>>(n);
    std::vector<pgl::Convex<Point>> w;
    w.reserve(tris.size());
    for (const auto& t : tris) w.push_back(t.asConvex());
    return w;
}

template <class Number>
std::vector<pgl::Convex<pgl::Point<Number>>> randomLargeTriangleAsConvex(int n) {
    using Point = pgl::Point<Number>;
    auto tris = randomLargeTrishape<pgl::Triangle<Point>>(n);
    std::vector<pgl::Convex<Point>> w;
    w.reserve(tris.size());
    for (const auto& t : tris) w.push_back(t.asConvex());
    return w;
}

// Convexes, generated as Convex, stored as Polygon (via Convex::asPolygon).
template <class Number>
std::vector<pgl::Polygon<pgl::Point<Number>>> randomSmallConvexAsPolygon(int n, int m) {
    using Point = pgl::Point<Number>;
    auto cs = randomSmallConvexes<Number>(n, m);
    std::vector<pgl::Polygon<Point>> w;
    w.reserve(cs.size());
    for (const auto& c : cs) w.push_back(c.asPolygon());
    return w;
}

template <class Number>
std::vector<pgl::Polygon<pgl::Point<Number>>> randomLargeConvexAsPolygon(int n, int m) {
    using Point = pgl::Point<Number>;
    auto cs = randomLargeConvexes<Number>(n, m);
    std::vector<pgl::Polygon<Point>> w;
    w.reserve(cs.size());
    for (const auto& c : cs) w.push_back(c.asPolygon());
    return w;
}

// Bounded half-plane intersections: generated as Convex, then adopted as the
// intersection of that convex polygon's edge half-planes (via the Convex
// converting constructor). The geometry — and thus the small/large scale and
// the dedup seed — matches randomXxxConvexes exactly; only the representation
// differs, so the cube exercises HalfplaneIntersection's rational-vertex code
// paths on the same regions Convex is measured on. These regions are bounded,
// so the shape's unbounded states are not covered here.
template <class Number>
std::vector<pgl::HalfplaneIntersection<pgl::Point<Number>>>
randomSmallHalfplaneIntersections(int n, int m) {
    using Point = pgl::Point<Number>;
    auto cs = randomSmallConvexes<Number>(n, m);
    std::vector<pgl::HalfplaneIntersection<Point>> w;
    w.reserve(cs.size());
    for (const auto& c : cs) w.emplace_back(c);
    return w;
}

template <class Number>
std::vector<pgl::HalfplaneIntersection<pgl::Point<Number>>>
randomLargeHalfplaneIntersections(int n, int m) {
    using Point = pgl::Point<Number>;
    auto cs = randomLargeConvexes<Number>(n, m);
    std::vector<pgl::HalfplaneIntersection<Point>> w;
    w.reserve(cs.size());
    for (const auto& c : cs) w.emplace_back(c);
    return w;
}

// Simple polygons, generated as Polygon, stored as a hole-free PolygonWithHoles
// (via Polygon::asPolygonWithHoles). The region is exactly the one Polygon is
// measured on, so the cube shows what the holed shape's code paths cost when
// there is no hole to account for.
template <class Number>
std::vector<pgl::PolygonWithHoles<pgl::Point<Number>>> randomSmallPolygonAsPWH(int n, int m) {
    using Point = pgl::Point<Number>;
    auto polys = randomSmallPolygons<Number>(n, m);
    std::vector<pgl::PolygonWithHoles<Point>> w;
    w.reserve(polys.size());
    for (const auto& poly : polys) w.push_back(poly.asPolygonWithHoles());
    return w;
}

template <class Number>
std::vector<pgl::PolygonWithHoles<pgl::Point<Number>>> randomLargePolygonAsPWH(int n, int m) {
    using Point = pgl::Point<Number>;
    auto polys = randomLargePolygons<Number>(n, m);
    std::vector<pgl::PolygonWithHoles<Point>> w;
    w.reserve(polys.size());
    for (const auto& poly : polys) w.push_back(poly.asPolygonWithHoles());
    return w;
}

// Simple polygons, generated as Polygon, stored as the constrained Delaunay
// triangulation of that polygon (via Polygon::triangulation()). Unlike the other
// "as-other-type" generators this is not a re-storage of the same vertices but a
// different data structure over the same region, so the cube can compare a
// predicate answered by scanning the polygon against the same predicate answered
// by walking its mesh. Building the mesh is setup: only the queries are timed.
template <class Number>
auto randomSmallPolygonAsTriangulation(int n, int m) {
    const auto polys = randomSmallPolygons<Number>(n, m);
    std::vector<decltype(polys.front().triangulation())> w;
    w.reserve(polys.size());
    for (const auto& poly : polys) w.push_back(poly.triangulation());
    return w;
}

template <class Number>
auto randomLargePolygonAsTriangulation(int n, int m) {
    const auto polys = randomLargePolygons<Number>(n, m);
    std::vector<decltype(polys.front().triangulation())> w;
    w.reserve(polys.size());
    for (const auto& poly : polys) w.push_back(poly.triangulation());
    return w;
}

// Weakly x-monotone chains: m random points fed to MonotoneChain, whose
// constructor sorts them lexicographically and drops duplicates. The result has
// at most m vertices (fewer when x/y collisions coincide).
template <class Number>
std::vector<pgl::MonotoneChain<pgl::Point<Number>>> randomSmallMonotoneChains(int n, int m) {
    using Point = pgl::Point<Number>;
    using MonotoneChain = pgl::MonotoneChain<Point>;
    std::vector<MonotoneChain> w;
    std::set<MonotoneChain> seen;
    Rng rng{static_cast<std::uint64_t>(pgl::detail::shapeRank<pgl::MonotoneChain<Point>>)};
    while (static_cast<int>(w.size()) < n) {
        const auto base = randomPoint<Number>(rng, largeRange);
        std::vector<Point> points;
        for (int i = 0; i < m; ++i) {
            points.push_back(base + randomPoint<Number>(rng, smallRange));
        }
        MonotoneChain chain(points);
        if (!chain.isDegenerate() && seen.insert(chain).second) {
            w.push_back(chain);
        }
    }
    return w;
}

template <class Number>
std::vector<pgl::MonotoneChain<pgl::Point<Number>>> randomLargeMonotoneChains(int n, int m) {
    using Point = pgl::Point<Number>;
    using MonotoneChain = pgl::MonotoneChain<Point>;
    std::vector<MonotoneChain> w;
    std::set<MonotoneChain> seen;
    Rng rng{static_cast<std::uint64_t>(pgl::detail::shapeRank<pgl::MonotoneChain<Point>>)};
    while (static_cast<int>(w.size()) < n) {
        std::vector<Point> points;
        for (int i = 0; i < m; ++i) {
            points.push_back(randomPoint<Number>(rng, largeRange));
        }
        MonotoneChain chain(points);
        if (!chain.isDegenerate() && seen.insert(chain).second) {
            w.push_back(chain);
        }
    }
    return w;
}

// Open polylines: m random points linked in generation order. Unlike Polygon,
// the sequence is not untangled, so the chain may self-intersect — Polyline only
// canonicalizes its direction and keeps all m vertices.
template <class Number>
std::vector<pgl::Polyline<pgl::Point<Number>>> randomSmallPolylines(int n, int m) {
    using Point = pgl::Point<Number>;
    using Polyline = pgl::Polyline<Point>;
    std::vector<Polyline> w;
    std::set<Polyline> seen;
    Rng rng{static_cast<std::uint64_t>(pgl::detail::shapeRank<pgl::Polyline<Point>>)};
    while (static_cast<int>(w.size()) < n) {
        const auto base = randomPoint<Number>(rng, largeRange);
        std::vector<Point> points;
        for (int i = 0; i < m; ++i) {
            points.push_back(base + randomPoint<Number>(rng, smallRange));
        }
        Polyline poly(points);
        if (!poly.isDegenerate() && seen.insert(poly).second) {
            w.push_back(poly);
        }
    }
    return w;
}

template <class Number>
std::vector<pgl::Polyline<pgl::Point<Number>>> randomLargePolylines(int n, int m) {
    using Point = pgl::Point<Number>;
    using Polyline = pgl::Polyline<Point>;
    std::vector<Polyline> w;
    std::set<Polyline> seen;
    Rng rng{static_cast<std::uint64_t>(pgl::detail::shapeRank<pgl::Polyline<Point>>)};
    while (static_cast<int>(w.size()) < n) {
        std::vector<Point> points;
        for (int i = 0; i < m; ++i) {
            points.push_back(randomPoint<Number>(rng, largeRange));
        }
        Polyline poly(points);
        if (!poly.isDegenerate() && seen.insert(poly).second) {
            w.push_back(poly);
        }
    }
    return w;
}
