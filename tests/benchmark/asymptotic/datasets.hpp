#pragma once
//
// Dataset generators for the asymptotic benchmarks.
//
// Point, segment, rectangle and triangle datasets come straight from the
// shape-pair benchmark's generators (../randomshapes.hpp), so "small" and
// "large" mean exactly what they mean on the pairs page: a small shape spans
// smallRange in a field of largeRange and a random pair usually misses; a large
// one spans mediumRange in a field of mediumRange and a random pair usually
// meets.
//
// Polygons do not, and that is deliberate. randomshapes.hpp's polygon
// generators call legacyUntangledPolygon() to pin the shapes the pairs page's
// recorded history was measured on (see ../legacy_untangle.hpp); the asymptotic
// benchmarks have no such history to protect and would only be paying the old
// implementation's cubic cost for nothing. randomPolygon() below is
// randomLargePolygons()'s construction with the library's current untangle().
//
// Everything is generated with `int` coordinates. Callers convert (see
// bench::convert) rather than re-generating per number type: the generators
// draw integer coordinates whatever the target type is, so generating in
// ERational would buy nothing but slower arithmetic on the same numbers, and
// converting is what guarantees the int and ERational runs measure the
// identical input — without which comparing their result signatures would prove
// nothing.
//
#include "../randomshapes.hpp"

#include <cstdint>
#include <vector>

namespace bench {

using IntPoint     = pgl::Point<int>;
using IntSegment   = pgl::Segment<IntPoint>;
using IntTriangle  = pgl::Triangle<IntPoint>;
using IntRectangle = pgl::Rectangle<IntPoint>;
using IntPolygon   = pgl::Polygon<IntPoint>;

// n distinct random points over the large field.
inline std::vector<IntPoint> points(int n) {
    return randomPoints<int>(n);
}

// n query points over the same field, drawn from a different seed than
// points(). Sharing the seed would make the queries a prefix of the dataset, so
// every one of them would land exactly on a vertex — the one case a point
// location is least representative of.
inline std::vector<IntPoint> queryPoints(int n) {
    Rng rng{0x9E3779B97F4A7C15ULL};
    std::vector<IntPoint> v;
    v.reserve(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        v.push_back(randomPoint<int>(rng, largeRange));
    }
    return v;
}

// n small / large random segments, as on the pairs page.
inline std::vector<IntSegment> smallSegments(int n) {
    return randomSmallBishape<IntSegment>(n);
}
inline std::vector<IntSegment> largeSegments(int n) {
    return randomLargeBishape<IntSegment>(n);
}

// n large random triangles, as on the pairs page: each spans the field it is
// scattered over, so a set of them overlaps heavily.
inline std::vector<IntTriangle> largeTriangles(int n) {
    return randomLargeTrishape<IntTriangle>(n);
}

// Query shapes. Small, so a query selects a modest neighbourhood rather than
// most of the tree.
inline std::vector<IntRectangle> queryRectangles(int n) {
    return randomSmallBishape<IntRectangle>(n);
}
inline std::vector<IntTriangle> queryTriangles(int n) {
    return randomSmallTrishape<IntTriangle>(n);
}

namespace detail {

// One simple polygon of at most m vertices: m points drawn around a common
// anchor and untangled, which is randomLargePolygons' construction with the
// library's current untangle(). Retries on the anchor's own stream, so a draw
// that collapses to a degenerate ring simply advances it.
inline IntPolygon polygonOfSpan(int m, int field, int span, std::uint64_t seed) {
    Rng rng{seed};
    while (true) {
        const auto base = randomPoint<int>(rng, field);
        std::vector<IntPoint> vertices;
        vertices.reserve(static_cast<std::size_t>(m));
        for (int i = 0; i < m; ++i) {
            vertices.push_back(base + randomPoint<int>(rng, span));
        }
        IntPolygon polygon(vertices);
        polygon.untangle();
        if (!polygon.isDegenerate()) {
            return polygon;
        }
    }
}

}  // namespace detail

// A polygon at the pairs page's "large" scale. `seed` shifts the draw, so a
// caller needing two independent polygons of the same size gets two different
// ones.
inline IntPolygon randomPolygon(int m, std::uint64_t seed = 0) {
    return detail::polygonOfSpan(m, mediumRange, mediumRange, seed);
}

// A polygon at the "small" scale, for the categories that pair a polygon of the
// swept size against a fixed small operand.
inline IntPolygon randomSmallPolygon(int m, std::uint64_t seed = 0) {
    return detail::polygonOfSpan(m, largeRange, smallRange, seed);
}

// `count` points strictly inside `polygon`, rejection-sampled from its bounding
// box. Drawn per polygon, since a point inside one means nothing for another,
// and drawn on the integer polygon so that converting gives the ERational run —
// and the CGAL baseline — the identical queries.
inline std::vector<IntPoint> interiorPoints(const IntPolygon& polygon, int count) {
    const auto box = polygon.bbox();
    Rng rng{12345};
    const int width  = box.max().x() - box.min().x();
    const int height = box.max().y() - box.min().y();
    std::vector<IntPoint> queries;
    queries.reserve(static_cast<std::size_t>(count));
    while (static_cast<int>(queries.size()) < count) {
        const IntPoint candidate(box.min().x() + rng.range(width),
                                 box.min().y() + rng.range(height));
        if (polygon.interiorContains(candidate)) {
            queries.push_back(candidate);
        }
    }
    return queries;
}

// The polygon's boundary as independent segments — a dataset of n segments that,
// unlike smallSegments/largeSegments, has no crossings at all.
inline std::vector<IntSegment> polygonEdges(int m) {
    const IntPolygon polygon = randomPolygon(m);
    std::vector<IntSegment> edges;
    edges.reserve(polygon.vertices().size());
    for (const auto& e : polygon.edges()) {
        edges.emplace_back(e[0], e[1]);
    }
    return edges;
}

}  // namespace bench
