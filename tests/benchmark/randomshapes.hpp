#pragma once
#include "pgl.hpp"

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
