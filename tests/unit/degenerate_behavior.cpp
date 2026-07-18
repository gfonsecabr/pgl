#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstdint>
#include <string>
#include <vector>
#include "pgl.hpp"

// Behavioral contract for degenerate shapes.
//
// Undefined shapes (see isUndefined) remain undefined behavior, but a shape
// that has collapsed to a lower dimension is fully defined: it covers exactly
// the point set of its carrier -- the point of isPoint, the segment of
// isSegment -- and it is *all boundary, with empty interior*. That last part
// is what distinguishes it from the carrier itself: Point treats itself as its
// own interior and has empty boundary, and a proper Segment's interior is the
// open segment, but an area shape squeezed down to a point or a segment is the
// limit of shapes whose interior has vanished, so nothing is left inside.
//
// Straight polylines and monotone chains are deliberately NOT covered by that
// rule: a chain with two or more distinct collinear vertices is already
// one-dimensional rather than collapsed, so it keeps the ordinary relative
// interior it shares with Segment. Only a chain whose vertices all coincide
// has collapsed.

using P = pgl::Point<int>;

namespace {

// Whether the shape under test has collapsed to a lower dimension (all
// boundary, empty interior) or merely equals its carrier in the usual sense.
enum class Mode { Collapsed, Equivalent };

// Checks one degenerate shape `d` against its carrier `c` for a single probe
// shape `x`. Predicates missing for a given pair are skipped.
template <class D, class C, class X>
void checkPair(Mode mode, const D& d, const C& c, const X& x, const std::string& label) {
    INFO("probe: ", label);
    const bool collapsed = (mode == Mode::Collapsed);

    // The point set is exactly the carrier's.
    if constexpr (requires { d.contains(x); c.contains(x); }) {
        CHECK(d.contains(x) == c.contains(x));
    }
    if constexpr (requires { d.intersects(x); c.intersects(x); }) {
        CHECK(d.intersects(x) == c.intersects(x));
    }
    if constexpr (requires { d.squaredDistance(x); c.squaredDistance(x); }) {
        CHECK(d.squaredDistance(x) == c.squaredDistance(x));
    }

    // Boundary and interior: a collapsed shape is all boundary and no interior.
    if constexpr (requires { d.boundaryContains(x); c.contains(x); c.boundaryContains(x); }) {
        CHECK(d.boundaryContains(x) == (collapsed ? c.contains(x) : c.boundaryContains(x)));
    }
    if constexpr (requires { d.interiorContains(x); c.interiorContains(x); }) {
        CHECK(d.interiorContains(x) == (collapsed ? false : c.interiorContains(x)));
    }
    if constexpr (requires { d.interiorsIntersect(x); c.interiorsIntersect(x); }) {
        CHECK(d.interiorsIntersect(x) == (collapsed ? false : c.interiorsIntersect(x)));
    }

    // Reverse direction: equal point sets must give equal answers, and nothing
    // can meet the interior of a collapsed shape.
    if constexpr (requires { x.contains(d); x.contains(c); }) {
        CHECK(x.contains(d) == x.contains(c));
    }
    if constexpr (requires { x.intersects(d); x.intersects(c); }) {
        CHECK(x.intersects(d) == x.intersects(c));
    }
    if constexpr (requires { x.boundaryContains(d); x.boundaryContains(c); }) {
        CHECK(x.boundaryContains(d) == x.boundaryContains(c));
    }
    if constexpr (requires { x.interiorContains(d); x.interiorContains(c); }) {
        CHECK(x.interiorContains(d) == x.interiorContains(c));
    }
    if constexpr (requires { x.interiorsIntersect(d); x.interiorsIntersect(c); }) {
        CHECK(x.interiorsIntersect(d) == (collapsed ? false : x.interiorsIntersect(c)));
    }
}

// Runs the whole probe battery against one degenerate shape and its carrier.
// The probes straddle the carriers used below -- (1,1)-(3,3) and its vertical
// twin (1,1)-(1,3) -- touching them at an end, in the middle, and not at all.
template <class D, class C>
void checkShape(const D& d, const C& c, Mode mode = Mode::Collapsed) {
    checkPair(mode, d, c, P(2, 2), "Point(2,2)");
    checkPair(mode, d, c, P(1, 1), "Point(1,1)");
    checkPair(mode, d, c, P(1, 3), "Point(1,3)");
    checkPair(mode, d, c, P(9, 9), "Point(9,9) outside");
    checkPair(mode, d, c, pgl::Segment<P>(P(1, 1), P(3, 3)), "Segment(1,1)-(3,3)");
    checkPair(mode, d, c, pgl::Segment<P>(P(1, 1), P(2, 2)), "Segment(1,1)-(2,2)");
    checkPair(mode, d, c, pgl::Segment<P>(P(1, 1), P(1, 3)), "Segment(1,1)-(1,3)");
    checkPair(mode, d, c, pgl::Segment<P>(P(0, 4), P(4, 0)), "Segment crossing");
    checkPair(mode, d, c, pgl::OrientedSegment<P>(P(1, 1), P(3, 3)), "OrientedSegment");
    checkPair(mode, d, c, pgl::OrientedSegment<P>(P(1, 1), P(2, 2)), "OrientedSegment half");
    checkPair(mode, d, c, pgl::Line<P>(P(0, 0), P(1, 1)), "Line y=x");
    checkPair(mode, d, c, pgl::Ray<P>(P(0, 0), P(1, 1)), "Ray y=x");
    checkPair(mode, d, c, pgl::Halfplane<P>(P(0, 0), P(1, 0)), "Halfplane y>=0");
    checkPair(mode, d, c, pgl::Triangle<P>(P(0, 0), P(8, 0), P(0, 8)), "Triangle");
    checkPair(mode, d, c, pgl::Rectangle<P>(P(0, 0), P(8, 8)), "Rectangle");
    checkPair(mode, d, c, pgl::Convex<P>(std::vector<P>{P(0, 0), P(8, 0), P(0, 8)}), "Convex");
    checkPair(mode, d, c, pgl::Polygon<P>(std::vector<P>{P(0, 0), P(8, 0), P(0, 8)}), "Polygon");
    checkPair(mode, d, c, pgl::Polyline<P>(std::vector<P>{P(1, 1), P(2, 2), P(3, 3)}), "Polyline straight");
    checkPair(mode, d, c, pgl::Polyline<P>(std::vector<P>{P(0, 4), P(2, 2), P(4, 4)}), "Polyline bent");
    checkPair(mode, d, c, pgl::MonotoneChain<P>(std::vector<P>{P(1, 1), P(2, 2), P(3, 3)}), "MonotoneChain");
}

}  // namespace

TEST_CASE("Shapes collapsed to a point behave as that point with empty interior") {
    const P p(2, 2);

    SUBCASE("Segment") { checkShape(pgl::Segment<P>(p, p), p); }
    SUBCASE("OrientedSegment") { checkShape(pgl::OrientedSegment<P>(p, p), p); }
    SUBCASE("Triangle") { checkShape(pgl::Triangle<P>(p, p, p), p); }
    SUBCASE("Rectangle") { checkShape(pgl::Rectangle<P>(p, p), p); }
    SUBCASE("Convex") { checkShape(pgl::Convex<P>(std::vector<P>{p}), p); }
    SUBCASE("Polygon") { checkShape(pgl::Polygon<P>(std::vector<P>{p, p, p}), p); }
    SUBCASE("Polyline") { checkShape(pgl::Polyline<P>(std::vector<P>{p, p}), p); }
    SUBCASE("MonotoneChain") { checkShape(pgl::MonotoneChain<P>(std::vector<P>{p}), p); }
}

TEST_CASE("A radius-zero disk behaves as its centre with empty interior") {
    const P p(2, 2);
    const pgl::Disk<P> dot(p, p, p);
    REQUIRE(dot.isPoint());
    REQUIRE_FALSE(dot.isUndefined());
    CHECK(*dot.getIfPoint() == p);

    CHECK(dot.contains(p));
    CHECK_FALSE(dot.contains(P(3, 3)));
    CHECK(dot.intersects(p));
    CHECK(dot.boundaryContains(p));
    CHECK_FALSE(dot.interiorContains(p));

    // Area shapes must see the centre, not a disk with a radius. The witness
    // used for a proper disk is a point strictly inside it, which a radius-zero
    // disk does not have.
    const pgl::Triangle<P> triangle(P(0, 0), P(8, 0), P(0, 8));
    const pgl::Rectangle<P> rectangle(P(0, 0), P(8, 8));
    const pgl::Convex<P> hull(std::vector<P>{P(0, 0), P(8, 0), P(0, 8)});
    const pgl::Halfplane<P> halfplane(P(0, 0), P(1, 0));

    CHECK(triangle.interiorContains(dot));
    CHECK(rectangle.interiorContains(dot));
    CHECK(hull.interiorContains(dot));
    CHECK(halfplane.interiorContains(dot));

    CHECK_FALSE(triangle.interiorsIntersect(dot));
    CHECK_FALSE(rectangle.interiorsIntersect(dot));
    CHECK_FALSE(hull.interiorsIntersect(dot));
    CHECK_FALSE(halfplane.interiorsIntersect(dot));
    CHECK_FALSE(dot.interiorsIntersect(triangle));
    CHECK_FALSE(dot.interiorsIntersect(rectangle));
    CHECK_FALSE(dot.interiorsIntersect(halfplane));

    // Two collapsed disks, and a collapsed disk against a proper one.
    const pgl::Disk<P> big(P(0, 4), P(4, 0), P(4, 8));
    CHECK_FALSE(dot.interiorsIntersect(big));
    CHECK_FALSE(big.interiorsIntersect(dot));
    CHECK_FALSE(dot.interiorsIntersect(pgl::Disk<P>(p, p, p)));

    // The in-circle formulation behind ray intersection needs three
    // non-collinear boundary points, which a radius-zero disk lacks.
    CHECK(dot.intersects(pgl::Ray<P>(P(0, 0), P(1, 1))));
    CHECK(pgl::Ray<P>(P(0, 0), P(1, 1)).intersects(dot));
    CHECK_FALSE(dot.intersects(pgl::Ray<P>(P(0, 1), P(1, 2))));
}

TEST_CASE("Area shapes collapsed to a segment are all boundary with empty interior") {
    const pgl::Segment<P> diagonal(P(1, 1), P(3, 3));
    const pgl::Segment<P> vertical(P(1, 1), P(1, 3));

    SUBCASE("Triangle with collinear vertices") {
        checkShape(pgl::Triangle<P>(P(1, 1), P(2, 2), P(3, 3)), diagonal);
    }
    SUBCASE("Rectangle of zero width") {
        // A rectangle can only collapse to an axis-parallel segment.
        checkShape(pgl::Rectangle<P>(P(1, 1), P(1, 3)), vertical);
    }
    SUBCASE("Convex hull of two points") {
        checkShape(pgl::Convex<P>(std::vector<P>{P(1, 1), P(3, 3)}), diagonal);
        checkShape(pgl::Convex<P>(std::vector<P>{P(1, 1), P(1, 3)}), vertical);
    }
    SUBCASE("Polygon with collinear vertices") {
        checkShape(pgl::Polygon<P>(std::vector<P>{P(1, 1), P(3, 3), P(2, 2)}), diagonal);
    }
}

TEST_CASE("Straight chains keep segment semantics rather than collapsing") {
    // A chain with distinct collinear vertices is one-dimensional already, so
    // it matches Segment exactly -- relative interior and two-endpoint
    // boundary included -- instead of becoming all boundary.
    const pgl::Segment<P> diagonal(P(1, 1), P(3, 3));
    const std::vector<P> vertices{P(1, 1), P(2, 2), P(3, 3)};

    SUBCASE("Polyline") {
        checkShape(pgl::Polyline<P>(vertices), diagonal, Mode::Equivalent);
    }
    SUBCASE("MonotoneChain") {
        checkShape(pgl::MonotoneChain<P>(vertices), diagonal, Mode::Equivalent);
    }

    SUBCASE("the relative interior is genuinely non-empty") {
        const pgl::Polyline<P> line(vertices);
        CHECK(line.interiorContains(P(2, 2)));
        CHECK_FALSE(line.interiorContains(P(1, 1)));
        CHECK(line.boundaryContains(P(1, 1)));
        CHECK_FALSE(line.boundaryContains(P(2, 2)));
    }
}

TEST_CASE_TEMPLATE("A collapsed shape has boundary but no interior at its own points", Point,
                   pgl::Point<int>, pgl::Point<double>, pgl::Point<pgl::Rational<int64_t>>) {
    const Point p(2, 2);

    // The point itself is the interior of a Point but the boundary of anything
    // that collapsed onto it -- the contrast the whole contract turns on.
    CHECK(Point(p).interiorContains(p));

    const pgl::Triangle<Point> triangle(p, p, p);
    CHECK(triangle.contains(p));
    CHECK(triangle.boundaryContains(p));
    CHECK_FALSE(triangle.interiorContains(p));

    const pgl::Rectangle<Point> rectangle(p, p);
    CHECK(rectangle.contains(p));
    CHECK(rectangle.boundaryContains(p));
    CHECK_FALSE(rectangle.interiorContains(p));

    const pgl::Segment<Point> segment(p, p);
    CHECK(segment.contains(p));
    CHECK(segment.boundaryContains(p));
    CHECK_FALSE(segment.interiorContains(p));

    // A proper segment's midpoint is interior, but the midpoint of a triangle
    // squeezed onto that segment is boundary.
    const Point a(1, 1);
    const Point b(3, 3);
    const pgl::Segment<Point> proper(a, b);
    CHECK(proper.interiorContains(p));
    CHECK_FALSE(proper.boundaryContains(p));

    const pgl::Triangle<Point> flat(a, p, b);
    REQUIRE(flat.isSegment());
    CHECK(flat.contains(p));
    CHECK(flat.boundaryContains(p));
    CHECK_FALSE(flat.interiorContains(p));
}

TEST_CASE("Convex hulls with fewer than three vertices contain their own points") {
    // Regression: the point-location path splits the hull into a lower and an
    // upper chain, which does not exist below three vertices.
    const pgl::Convex<P> vertex(std::vector<P>{P(2, 2)});
    CHECK(vertex.contains(P(2, 2)));
    CHECK(vertex.intersects(P(2, 2)));
    CHECK(vertex.boundaryContains(P(2, 2)));
    CHECK_FALSE(vertex.contains(P(2, 3)));

    const pgl::Convex<P> edge(std::vector<P>{P(1, 1), P(3, 3)});
    CHECK(edge.contains(P(1, 1)));
    CHECK(edge.contains(P(2, 2)));
    CHECK(edge.contains(P(3, 3)));
    CHECK_FALSE(edge.contains(P(2, 3)));
    CHECK(edge.contains(pgl::Segment<P>(P(1, 1), P(2, 2))));
    CHECK(edge.boundaryContains(pgl::Segment<P>(P(1, 1), P(3, 3))));
    CHECK_FALSE(edge.interiorContains(P(2, 2)));
}

TEST_CASE("Polygons collapsed to a point terminate against other polygons") {
    // Regression: the boundary decomposition splits a polygon at its
    // lexicographic direction reversals, and an all-equal boundary has none,
    // which used to spin forever.
    const pgl::Polygon<P> point(std::vector<P>{P(2, 2), P(2, 2), P(2, 2)});
    const pgl::Polygon<P> triangle(std::vector<P>{P(0, 0), P(8, 0), P(0, 8)});
    const pgl::Polygon<P> apart(std::vector<P>{P(20, 20), P(28, 20), P(20, 28)});

    CHECK(triangle.contains(point));
    CHECK(triangle.intersects(point));
    CHECK(point.intersects(triangle));
    CHECK_FALSE(point.contains(triangle));
    CHECK_FALSE(point.interiorsIntersect(triangle));
    CHECK_FALSE(apart.intersects(point));
    CHECK_FALSE(apart.contains(point));
    CHECK(point.squaredDistance(triangle) == 0);

    const pgl::Polygon<P> samePoint(std::vector<P>{P(2, 2), P(2, 2), P(2, 2)});
    CHECK(point.contains(samePoint));
    CHECK(point.intersects(samePoint));
}

TEST_CASE("A monotone chain of one vertex measures distance from that vertex") {
    // Regression: the distance scan required at least one edge.
    const pgl::MonotoneChain<P> chain(std::vector<P>{P(2, 2)});
    CHECK(chain.squaredDistance(P(2, 2)) == 0);
    CHECK(chain.squaredDistance(P(5, 6)) == 25);
    CHECK(chain.squaredDistance(pgl::Segment<P>(P(5, 2), P(5, 9))) == 9);
}

TEST_CASE("Degenerate carriers account for a lazily applied translation") {
    // Regression: getIfPoint/getIfSegment read the stored vertices directly,
    // dropping the translation that every other accessor applies.
    const P shift(10, 20);

    const auto chain = pgl::MonotoneChain<P>(std::vector<P>{P(2, 2)}) + shift;
    CHECK(*chain.getIfPoint() == P(12, 22));
    CHECK(*chain.getIfPoint() == chain[0]);

    const auto line = pgl::Polyline<P>(std::vector<P>{P(2, 2), P(2, 2)}) + shift;
    CHECK(*line.getIfPoint() == P(12, 22));

    const auto polygon = pgl::Polygon<P>(std::vector<P>{P(2, 2), P(2, 2), P(2, 2)}) + shift;
    CHECK(*polygon.getIfPoint() == P(12, 22));

    const auto hull = pgl::Convex<P>(std::vector<P>{P(2, 2)}) + shift;
    CHECK(*hull.getIfPoint() == P(12, 22));

    const pgl::Segment<P> expected(P(11, 21), P(13, 23));
    const std::vector<P> collinear{P(1, 1), P(2, 2), P(3, 3)};
    CHECK(*(pgl::MonotoneChain<P>(collinear) + shift).getIfSegment() == expected);
    CHECK(*(pgl::Polyline<P>(collinear) + shift).getIfSegment() == expected);
    CHECK(*(pgl::Polygon<P>(std::vector<P>{P(1, 1), P(3, 3), P(2, 2)}) + shift).getIfSegment() == expected);
    CHECK(*(pgl::Convex<P>(std::vector<P>{P(1, 1), P(3, 3)}) + shift).getIfSegment() == expected);
}

TEST_CASE("Collapsed area shapes convert to the matching degenerate region") {
    // Regression: the triangle conversion asserted on any collinear triangle,
    // even though the region type represents points and segments natively and
    // the rectangle and convex conversions already handled them.
    const P p(2, 2);

    const auto vertex = pgl::Triangle<P>(p, p, p).asHalfplaneIntersection();
    CHECK(vertex.isDegenerate());
    CHECK(vertex.isPoint());
    CHECK(vertex.contains(p));
    CHECK_FALSE(vertex.contains(P(3, 3)));

    const auto flat = pgl::Triangle<P>(P(1, 1), P(2, 2), P(3, 3)).asHalfplaneIntersection();
    CHECK(flat.isDegenerate());
    CHECK(flat.isSegment());
    CHECK(flat.contains(P(1, 1)));
    CHECK(flat.contains(p));
    CHECK(flat.contains(P(3, 3)));
    CHECK_FALSE(flat.contains(P(4, 4)));
    CHECK_FALSE(flat.contains(P(9, 9)));
    CHECK(*flat.getIfSegment() == pgl::Segment<P>(P(1, 1), P(3, 3)));

    // The rectangle conversions already agreed with this and must keep doing so.
    const auto rectVertex = pgl::Rectangle<P>(p, p).asHalfplaneIntersection();
    CHECK(rectVertex.isPoint());
    CHECK(rectVertex.contains(p));

    const auto rectFlat = pgl::Rectangle<P>(P(1, 1), P(1, 3)).asHalfplaneIntersection();
    CHECK(rectFlat.isSegment());
    CHECK(rectFlat.contains(P(1, 2)));
    CHECK_FALSE(rectFlat.contains(P(2, 2)));
}

TEST_CASE("A collapsed shape can sit on the boundary of a lower-ranked shape") {
    // The boundary of a segment is its two endpoints, so it contains a shape
    // that collapsed onto one of them but nothing of positive extent.
    const pgl::Segment<P> host(P(1, 1), P(2, 2));
    const P endpoint(2, 2);

    CHECK(host.boundaryContains(pgl::Segment<P>(endpoint, endpoint)));
    CHECK(host.boundaryContains(pgl::Triangle<P>(endpoint, endpoint, endpoint)));
    CHECK(host.boundaryContains(pgl::Rectangle<P>(endpoint, endpoint)));
    CHECK(host.boundaryContains(pgl::Convex<P>(std::vector<P>{endpoint})));
    CHECK(host.boundaryContains(pgl::Polygon<P>(std::vector<P>{endpoint, endpoint, endpoint})));
    CHECK(host.boundaryContains(pgl::Polyline<P>(std::vector<P>{endpoint, endpoint})));

    // The other endpoint qualifies too; anything of positive extent does not.
    CHECK(host.boundaryContains(pgl::Triangle<P>(P(1, 1), P(1, 1), P(1, 1))));
    CHECK_FALSE(host.boundaryContains(pgl::Triangle<P>(P(9, 9), P(9, 9), P(9, 9))));
    CHECK_FALSE(host.boundaryContains(pgl::Rectangle<P>(P(0, 0), P(4, 4))));
    CHECK_FALSE(host.boundaryContains(pgl::Triangle<P>(P(1, 1), P(2, 2), P(3, 3))));

    // The relative interior of a longer segment likewise swallows a collapsed
    // shape sitting strictly inside it.
    const pgl::Segment<P> longer(P(0, 0), P(4, 4));
    CHECK(longer.interiorContains(pgl::Rectangle<P>(endpoint, endpoint)));
    CHECK(longer.interiorContains(pgl::Convex<P>(std::vector<P>{endpoint})));
    CHECK(longer.interiorContains(pgl::Polygon<P>(std::vector<P>{endpoint, endpoint, endpoint})));
    CHECK_FALSE(longer.interiorContains(pgl::Rectangle<P>(P(0, 0), P(0, 0))));
}
