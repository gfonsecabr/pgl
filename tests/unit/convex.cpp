#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstdint>
#include <functional>
#include <random>
#include <set>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <vector>

#include "pgl.hpp"

static_assert(std::is_same_v<pgl::Convex<>, pgl::Convex<pgl::Point<int>>>);

TEST_CASE_TEMPLATE("Convex usest the right vertex order and iterates over its vertices", Point, pgl::Point<int>, pgl::Point<double>, pgl::Point<int, std::string>, pgl::Point<pgl::Rational<int64_t>>) {
    using Convex = pgl::Convex<Point>;
    using Number = Point::NumberType;

    const auto make_point = [](Number x, Number y, const char* label = "tag") {
        if constexpr (requires { Point(x, y, label); }) {
            return Point(x, y, label);
        } else {
            return Point(x, y);
        }
    };

    {
    std::array<Point, 3> points = {
        make_point(static_cast<Number>(1), static_cast<Number>(2), "a"),
        make_point(static_cast<Number>(4), static_cast<Number>(2), "b"),
        make_point(static_cast<Number>(1), static_cast<Number>(5), "c")};
       
    const Convex triangle(points);

    CHECK(triangle[0].x() == Number(1));
    CHECK(triangle[1].x() == Number(4));
    CHECK(triangle[2].y() == Number(5));
    }

    {
    std::array<Point, 3> points = {
        make_point(static_cast<Number>(4), static_cast<Number>(2), "b"),
        make_point(static_cast<Number>(1), static_cast<Number>(2), "a"),
        make_point(static_cast<Number>(1), static_cast<Number>(5), "c")};
       
    const Convex triangle(points);

    CHECK(triangle[0].x() == Number(1));
    CHECK(triangle[1].x() == Number(4));
    CHECK(triangle[2].y() == Number(5));
    }
}

TEST_CASE("Convex binary searches") {
    using Point = pgl::Point<int>;
    using Convex = pgl::Convex<Point>;

    std::vector<Point> points;
    for (int i = 0; i <= 10; ++i) {
        points.emplace_back(2*i, 2*i * 2*i);
    }
    for (int i = 9; i >= 1; --i) {
        points.emplace_back(2*i, 1000 - 2*i * 2*i);
    }

    Convex convex(points);
    CHECK(convex.maxIndex() == 10);


    for (size_t i = 0; i < points.size(); ++i) {
        Point p0 = points[i];
        Point p1 = points[(i+1) % points.size()];
        CHECK_MESSAGE(convex.verticesContain(p0), p0);
        CHECK_MESSAGE(convex.boundaryContains(p0), p0);
        CHECK_MESSAGE(convex.contains(p0), p0);
        CHECK_FALSE_MESSAGE(convex.interiorContains(p0), p0);
        Point midPoint = (p0 + p1) / 2;
        CHECK_FALSE_MESSAGE(convex.verticesContain(midPoint), midPoint);
        CHECK_MESSAGE(convex.boundaryContains(midPoint), midPoint);
        CHECK_MESSAGE(convex.contains(midPoint), midPoint);
        CHECK_FALSE_MESSAGE(convex.interiorContains(midPoint), midPoint);

        Point p2 = points[(i+2) % points.size()];
        Point innerPoint = (p0 + p2) / 2;
        CHECK_FALSE_MESSAGE(convex.verticesContain(innerPoint), innerPoint);
        CHECK_FALSE_MESSAGE(convex.boundaryContains(innerPoint), innerPoint);
        CHECK_MESSAGE(convex.contains(innerPoint), innerPoint);
        CHECK_MESSAGE(convex.interiorContains(innerPoint), innerPoint);
    }
}


TEST_CASE("Convex converts between labeled and unlabeled vertices") {
    using PlainPoint = pgl::Point<int>;
    using LabelPoint = pgl::Point<int, std::string>;
    using PlainConvex = pgl::Convex<PlainPoint>;
    using LabelConvex = pgl::Convex<LabelPoint>;

    const LabelConvex labeled(std::vector<LabelPoint>{LabelPoint(0, 0, "a"), LabelPoint(4, 0, "b"), LabelPoint(0, 3, "c")});
    const PlainConvex plain_source(std::vector<PlainPoint>{PlainPoint(0, 0), PlainPoint(4, 0), PlainPoint(0, 3)});

    const PlainConvex plain_from_labeled = labeled;
    const LabelConvex labeled_from_plain = plain_source;

    CHECK(plain_from_labeled[0] == PlainPoint(0, 0));
    CHECK(plain_from_labeled[1] == PlainPoint(4, 0));
    CHECK(plain_from_labeled[2] == PlainPoint(0, 3));
    CHECK(labeled_from_plain[0] == LabelPoint(0, 0, ""));
    CHECK(labeled_from_plain[1] == LabelPoint(4, 0, ""));
    CHECK(labeled_from_plain[2] == LabelPoint(0, 3, ""));
}

TEST_CASE("Convex calculates area correctly") {
    using PlainPoint = pgl::Point<int>;
    using PlainConvex = pgl::Convex<PlainPoint>;

    // Right triangle with legs 3 and 4, area = 6
    const PlainConvex triangle(std::vector<PlainPoint>{PlainPoint(0, 0), PlainPoint(4, 0), PlainPoint(0, 3)});
    CHECK(triangle.area() == 6);

    // Square with side 5, area = 25
    const PlainConvex square(std::vector<PlainPoint>{PlainPoint(0, 0), PlainPoint(5, 0), PlainPoint(5, 5), PlainPoint(0, 5)});
    CHECK(square.area() == 25);
}

TEST_CASE("Convex::intersection(Line) handles all geometric cases") {
    using Point   = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Line    = pgl::Line<Point>;
    using Convex  = pgl::Convex<Point>;

    // Triangle (0,0)-(4,0)-(0,4)
    const Convex tri(std::vector<Point>{{0,0},{4,0},{0,4}});

    // Interior crossing: horizontal line y=1 → segment (0,1)-(3,1)
    {
        const Line hline({0,1},{1,1});
        const auto r = tri.intersection(hline);
        REQUIRE(r);
        REQUIRE(std::holds_alternative<Segment>(*r));
        CHECK(std::get<Segment>(*r).min() == Point(0,1));
        CHECK(std::get<Segment>(*r).max() == Point(3,1));
    }

    // Interior crossing: vertical line x=2 → segment (2,0)-(2,2)
    {
        const Line vline({2,0},{2,1});
        const auto r = tri.intersection(vline);
        REQUIRE(r);
        REQUIRE(std::holds_alternative<Segment>(*r));
        CHECK(std::get<Segment>(*r).min() == Point(2,0));
        CHECK(std::get<Segment>(*r).max() == Point(2,2));
    }

    // Line along bottom edge (y=0) → segment (0,0)-(4,0)
    {
        const Line edge({0,0},{4,0});
        const auto r = tri.intersection(edge);
        REQUIRE(r);
        REQUIRE(std::holds_alternative<Segment>(*r));
        CHECK(std::get<Segment>(*r).min() == Point(0,0));
        CHECK(std::get<Segment>(*r).max() == Point(4,0));
    }

    // Line tangent at vertex (0,0) — all other vertices strictly to one side
    {
        // Line x+y=0 through (-1,1) and (1,-1): only (0,0) is on it
        const Line tangent({-1,1},{1,-1});
        const auto r = tri.intersection(tangent);
        REQUIRE(r);
        REQUIRE(std::holds_alternative<Point>(*r));
        CHECK(std::get<Point>(*r) == Point(0,0));
    }

    // Line tangent at (0,4) from outside: y=4 touches only vertex (0,4)
    {
        const Line top({0,4},{1,4});
        const auto r = tri.intersection(top);
        REQUIRE(r);
        REQUIRE(std::holds_alternative<Point>(*r));
        CHECK(std::get<Point>(*r) == Point(0,4));
    }

    // Complete miss: line y=-1 below the triangle
    {
        const Line below({0,-1},{1,-1});
        CHECK_FALSE(tri.intersection(below));
    }

    // Complete miss: line x=-1 left of the triangle
    {
        const Line left_of({-1,0},{-1,1});
        CHECK_FALSE(tri.intersection(left_of));
    }

    // Square (0,0)-(4,0)-(4,4)-(0,4)
    const Convex sq(std::vector<Point>{{0,0},{4,0},{4,4},{0,4}});

    // Diagonal y=x: enters at (0,0) and exits at (4,4)
    {
        const Line diag({0,0},{1,1});
        const auto r = sq.intersection(diag);
        REQUIRE(r);
        REQUIRE(std::holds_alternative<Segment>(*r));
        CHECK(std::get<Segment>(*r).min() == Point(0,0));
        CHECK(std::get<Segment>(*r).max() == Point(4,4));
    }

    // Horizontal y=2 through interior: (0,2)-(4,2)
    {
        const Line mid({0,2},{1,2});
        const auto r = sq.intersection(mid);
        REQUIRE(r);
        REQUIRE(std::holds_alternative<Segment>(*r));
        CHECK(std::get<Segment>(*r).min() == Point(0,2));
        CHECK(std::get<Segment>(*r).max() == Point(4,2));
    }

    // Translated convex: triangle shifted by (10,10)
    {
        Convex shifted(std::vector<Point>{{0,0},{4,0},{0,4}});
        shifted += Point(10, 10);
        // Horizontal line y=11 → segment (10,11)-(13,11)
        const Line hline({0,11},{1,11});
        const auto r = shifted.intersection(hline);
        REQUIRE(r);
        REQUIRE(std::holds_alternative<Segment>(*r));
        CHECK(std::get<Segment>(*r).min() == Point(10,11));
        CHECK(std::get<Segment>(*r).max() == Point(13,11));
    }

    // Degenerate: empty convex → always empty
    {
        const Convex empty;
        CHECK_FALSE(empty.intersection(Line({0,0},{1,0})));
    }

    // Degenerate: single-vertex convex
    {
        const Convex pt(std::vector<Point>{{3,5}});
        const auto on  = pt.intersection(Line({0,5},{1,5}));
        REQUIRE(on);
        REQUIRE(std::holds_alternative<Point>(*on));
        CHECK(std::get<Point>(*on) == Point(3,5));
        CHECK_FALSE(pt.intersection(Line({0,6},{1,6})));
    }
}

TEST_CASE("Convex::boundaryContains handles every shape category") {
    using Point          = pgl::Point<int>;
    using Segment        = pgl::Segment<Point>;
    using OrientedSeg    = pgl::OrientedSegment<Point>;
    using Line           = pgl::Line<Point>;
    using OrientedLine_  = pgl::OrientedLine<Point>;
    using Ray            = pgl::Ray<Point>;
    using Halfplane      = pgl::Halfplane<Point>;
    using Rectangle      = pgl::Rectangle<Point>;
    using Triangle       = pgl::Triangle<Point>;
    using Convex         = pgl::Convex<Point>;

    // Unit square (0,0)-(4,0)-(4,4)-(0,4)
    const Convex sq(std::vector<Point>{{0,0},{4,0},{4,4},{0,4}});

    SUBCASE("segment on a single edge is on boundary; chord and interior segments are not") {
        CHECK(sq.boundaryContains(Segment({1,0},{3,0})));
        CHECK(sq.boundaryContains(Segment({0,0},{4,0})));
        CHECK_FALSE(sq.boundaryContains(Segment({1,1},{3,3}))); // interior chord
        CHECK_FALSE(sq.boundaryContains(Segment({0,0},{4,4}))); // diagonal chord
        CHECK_FALSE(sq.boundaryContains(Segment({-1,0},{5,0}))); // extends past edge
    }

    SUBCASE("oriented segment delegates to unoriented") {
        CHECK(sq.boundaryContains(OrientedSeg({3,0},{1,0})));
        CHECK_FALSE(sq.boundaryContains(OrientedSeg({1,1},{3,3})));
    }

    SUBCASE("non-degenerate line / oriented line / ray / halfplane are never on the boundary") {
        CHECK_FALSE(sq.boundaryContains(Line({0,0},{4,0})));
        CHECK_FALSE(sq.boundaryContains(OrientedLine_({0,0},{4,0})));
        CHECK_FALSE(sq.boundaryContains(Ray({0,0},{4,0})));
        CHECK_FALSE(sq.boundaryContains(Halfplane({0,0},{4,0})));
    }

    SUBCASE("degenerate line/ray/halfplane reduce to point containment") {
        CHECK(sq.boundaryContains(Line({2,0},{2,0})));
        CHECK_FALSE(sq.boundaryContains(Line({2,2},{2,2})));
        CHECK(sq.boundaryContains(Ray({2,0},{2,0})));
        CHECK(sq.boundaryContains(Halfplane({2,0},{2,0})));
    }

    SUBCASE("rectangle and triangle on the boundary") {
        // Degenerate rectangle as a boundary segment
        const Rectangle line_rect(Point(0,0), Point(4,0));
        CHECK(sq.boundaryContains(line_rect));
        // Non-degenerate rectangle inside the square is not on the boundary
        const Rectangle inner(Point(1,1), Point(2,2));
        CHECK_FALSE(sq.boundaryContains(inner));
        // Triangle with all edges on the boundary cannot exist for a square,
        // but a triangle whose every edge happens to lie on a single square edge can:
        const Triangle deg(Point(0,0), Point(2,0), Point(4,0));
        CHECK(sq.boundaryContains(deg));
        // Triangle strictly inside is not on the boundary
        CHECK_FALSE(sq.boundaryContains(Triangle(Point(1,1), Point(3,1), Point(2,3))));
    }

    SUBCASE("convex polygon whose every edge lies on a boundary edge") {
        const Convex empty;
        CHECK(sq.boundaryContains(empty));
        const Convex point_poly(std::vector<Point>{{2,0}});
        CHECK(sq.boundaryContains(point_poly));
        // An interior triangle is not on the boundary
        const Convex inner_tri(std::vector<Point>{{1,1},{3,1},{2,3}});
        CHECK_FALSE(sq.boundaryContains(inner_tri));
    }
}

TEST_CASE("Convex::intersects covers points, half-planes, and other convex polygons") {
    using Point     = pgl::Point<int>;
    using Halfplane = pgl::Halfplane<Point>;
    using Convex    = pgl::Convex<Point>;

    const Convex sq(std::vector<Point>{{0,0},{4,0},{4,4},{0,4}});

    SUBCASE("point intersection is just containment") {
        CHECK(sq.intersects(Point(2,2)));   // interior
        CHECK(sq.intersects(Point(0,0)));   // vertex
        CHECK(sq.intersects(Point(2,0)));   // edge midpoint
        CHECK_FALSE(sq.intersects(Point(5,2))); // outside
    }

    SUBCASE("half-plane intersection") {
        // Halfplane to the left of x=2 (using source→target convention, ccw side is "in")
        const Halfplane hp({2,0},{2,4});
        CHECK(sq.intersects(hp));
        // Halfplane whose "in" side is x >= 10 (boundary direction reversed)
        // does not contain any vertex of the square.
        const Halfplane far_hp({10,4},{10,0});
        CHECK_FALSE(sq.intersects(far_hp));
    }

    SUBCASE("convex-convex intersection") {
        // Overlapping squares
        const Convex other(std::vector<Point>{{2,2},{6,2},{6,6},{2,6}});
        CHECK(sq.intersects(other));
        // Disjoint
        const Convex disjoint(std::vector<Point>{{10,10},{12,10},{11,12}});
        CHECK_FALSE(sq.intersects(disjoint));
        // Nested
        const Convex inner(std::vector<Point>{{1,1},{3,1},{3,3},{1,3}});
        CHECK(sq.intersects(inner));
        CHECK(inner.intersects(sq));
        // Edge-touching
        const Convex adj(std::vector<Point>{{4,0},{8,0},{8,4},{4,4}});
        CHECK(sq.intersects(adj));
        // Empty polygon never intersects
        const Convex empty;
        CHECK_FALSE(sq.intersects(empty));
        CHECK_FALSE(empty.intersects(sq));
    }
}

TEST_CASE("Convex::interiorsIntersect distinguishes interior overlap from boundary touch") {
    using Point     = pgl::Point<int>;
    using Segment   = pgl::Segment<Point>;
    using Line      = pgl::Line<Point>;
    using Ray       = pgl::Ray<Point>;
    using Halfplane = pgl::Halfplane<Point>;
    using Rectangle = pgl::Rectangle<Point>;
    using Triangle  = pgl::Triangle<Point>;
    using Convex    = pgl::Convex<Point>;

    const Convex sq(std::vector<Point>{{0,0},{4,0},{4,4},{0,4}});

    SUBCASE("point") {
        CHECK(sq.interiorsIntersect(Point(2,2)));   // interior point
        CHECK_FALSE(sq.interiorsIntersect(Point(2,0))); // boundary
        CHECK_FALSE(sq.interiorsIntersect(Point(5,5)));
    }

    SUBCASE("line") {
        CHECK(sq.interiorsIntersect(Line({0,2},{1,2}))); // cuts through interior
        CHECK_FALSE(sq.interiorsIntersect(Line({0,0},{4,0}))); // along an edge: no interior cut
        CHECK_FALSE(sq.interiorsIntersect(Line({-1,1},{1,-1}))); // tangent at vertex
        CHECK_FALSE(sq.interiorsIntersect(Line({0,5},{4,5}))); // disjoint
    }

    SUBCASE("segment") {
        CHECK(sq.interiorsIntersect(Segment({-1,2},{5,2})));   // crosses
        CHECK(sq.interiorsIntersect(Segment({1,1},{3,3})));    // both endpoints inside
        CHECK_FALSE(sq.interiorsIntersect(Segment({0,0},{4,0}))); // on edge
        CHECK_FALSE(sq.interiorsIntersect(Segment({-1,0},{-1,4}))); // disjoint
        CHECK_FALSE(sq.interiorsIntersect(Segment({-1,0},{5,0}))); // collinear with edge, no interior
    }

    SUBCASE("ray") {
        // Use a target on the far side of the polygon so the underlying
        // Ray::interiorsIntersect(Segment) sees opposite-side source/target.
        CHECK(sq.interiorsIntersect(Ray({-1,2},{5,2})));   // enters from outside
        CHECK(sq.interiorsIntersect(Ray({2,2},{3,2})));    // source inside
        CHECK_FALSE(sq.interiorsIntersect(Ray({-1,5},{-2,5}))); // pointing away
        CHECK_FALSE(sq.interiorsIntersect(Ray({0,0},{4,0}))); // along edge
    }

    SUBCASE("halfplane") {
        const Halfplane hp({2,0},{2,4});
        CHECK(sq.interiorsIntersect(hp));
        // Halfplane whose boundary is the bottom edge of the square; whichever
        // orientation puts the square interior on the "inside" must overlap.
        const bool either_side =
            sq.interiorsIntersect(Halfplane({0,0},{4,0})) ||
            sq.interiorsIntersect(Halfplane({4,0},{0,0}));
        CHECK(either_side);
    }

    SUBCASE("rectangle, triangle, convex") {
        const Rectangle r(Point(2,2), Point(6,6));
        CHECK(sq.interiorsIntersect(r));
        const Rectangle disjoint_r(Point(10,10), Point(12,12));
        CHECK_FALSE(sq.interiorsIntersect(disjoint_r));

        const Triangle t(Point(2,2), Point(6,2), Point(4,6));
        CHECK(sq.interiorsIntersect(t));
        const Triangle outside_t(Point(10,10), Point(12,10), Point(11,12));
        CHECK_FALSE(sq.interiorsIntersect(outside_t));

        const Convex c(std::vector<Point>{{2,2},{6,2},{6,6},{2,6}});
        CHECK(sq.interiorsIntersect(c));
        const Convex disjoint_c(std::vector<Point>{{10,10},{12,10},{11,12}});
        CHECK_FALSE(sq.interiorsIntersect(disjoint_c));
        // Identical polygons: interiors fully overlap
        CHECK(sq.interiorsIntersect(sq));
        // Edge-adjacent: share an edge, no interior overlap
        const Convex adj(std::vector<Point>{{4,0},{8,0},{8,4},{4,4}});
        CHECK_FALSE(sq.interiorsIntersect(adj));
    }
}

TEST_CASE("Convex::separates cuts shapes into multiple components") {
    using Point     = pgl::Point<int>;
    using Segment   = pgl::Segment<Point>;
    using Line      = pgl::Line<Point>;
    using Ray       = pgl::Ray<Point>;
    using Halfplane = pgl::Halfplane<Point>;
    using Rectangle = pgl::Rectangle<Point>;
    using Triangle  = pgl::Triangle<Point>;
    using Convex    = pgl::Convex<Point>;

    const Convex sq(std::vector<Point>{{0,0},{4,0},{4,4},{0,4}});

    SUBCASE("separates segment iff both endpoints are outside and segment passes through") {
        CHECK(sq.separates(Segment({-1,2},{5,2}))); // proper crossing
        CHECK_FALSE(sq.separates(Segment({1,1},{3,3}))); // both endpoints inside
        CHECK_FALSE(sq.separates(Segment({2,2},{5,2}))); // one endpoint inside
        CHECK_FALSE(sq.separates(Segment({-1,5},{5,5}))); // disjoint
    }

    SUBCASE("line / ray separation") {
        CHECK(sq.separates(Line({0,2},{1,2})));         // line through interior
        // Line tangent at the (0,0) vertex still splits the line into two
        // rays once that point is removed.
        CHECK(sq.separates(Line({-1,1},{1,-1})));
        CHECK_FALSE(sq.separates(Line({0,5},{1,5})));   // disjoint line
        CHECK(sq.separates(Ray({-1,2},{5,2})));         // ray crossing polygon
        CHECK_FALSE(sq.separates(Ray({2,2},{3,2})));    // source inside
    }

    SUBCASE("never separates a point or half-plane") {
        CHECK_FALSE(sq.separates(Point(2,2)));
        CHECK_FALSE(sq.separates(Halfplane({0,0},{4,0})));
    }

    SUBCASE("separates rectangle/triangle/convex with a chord through them") {
        // Horizontal rectangle that pokes out both the left and right side of sq
        const Rectangle r(Point(-1,1), Point(5,3));
        CHECK(sq.separates(r));
        // Triangle with two vertices outside the square on opposite sides
        const Triangle t(Point(-1,1), Point(5,2), Point(-1,3));
        CHECK(sq.separates(t));
        // Convex strip crossing horizontally
        const Convex c(std::vector<Point>{{-1,1},{5,1},{5,3},{-1,3}});
        CHECK(sq.separates(c));
        // A polygon entirely inside is not separated
        const Convex inner(std::vector<Point>{{1,1},{3,1},{3,3},{1,3}});
        CHECK_FALSE(sq.separates(inner));
        // A polygon entirely outside is not separated
        const Convex outside(std::vector<Point>{{10,10},{12,10},{11,12}});
        CHECK_FALSE(sq.separates(outside));
    }

    SUBCASE("separates via two vertex-straddling arcs (no fully cut edge)") {
        // A thin band along the diagonal contains the (0,0) and (4,4) corners
        // of sq and excludes (4,0),(0,4); removing it splits sq into two
        // triangles. No edge of sq is cut clean through (each has a corner
        // inside the band), so the split is detected only by the two straddled
        // vertices, not by any separated edge.
        const Convex band(std::vector<Point>{{-1,-3},{7,5},{5,7},{-3,-1}});
        CHECK(band.separates(sq));

        // A wedge that bites only the (0,0) corner touches one arc and leaves
        // sq connected.
        const Convex corner(std::vector<Point>{{-2,-2},{3,-2},{-2,3}});
        CHECK_FALSE(corner.separates(sq));
    }
}

TEST_CASE("Convex::crosses needs both sides to be cut") {
    using Point   = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Line    = pgl::Line<Point>;
    using Ray     = pgl::Ray<Point>;
    using Convex  = pgl::Convex<Point>;

    const Convex sq(std::vector<Point>{{0,0},{4,0},{4,4},{0,4}});

    SUBCASE("crosses segment") {
        CHECK(sq.crosses(Segment({-1,2},{5,2})));            // proper crossing
        CHECK_FALSE(sq.crosses(Segment({1,1},{3,3})));        // endpoints inside
        CHECK_FALSE(sq.crosses(Segment({-1,0},{5,0})));       // collinear with edge
        CHECK_FALSE(sq.crosses(Segment({-1,4},{1,4})));       // touches one edge vertex
    }

    SUBCASE("crosses line / ray") {
        CHECK(sq.crosses(Line({0,2},{1,2})));
        CHECK_FALSE(sq.crosses(Line({-1,1},{1,-1})));        // tangent at vertex
        CHECK(sq.crosses(Ray({-1,2},{5,2})));
        CHECK_FALSE(sq.crosses(Ray({2,2},{3,2})));            // source inside
    }

    SUBCASE("two convex polygons in a + configuration cross") {
        // Wide horizontal and tall vertical rectangles meeting at the origin
        const Convex horizontal(std::vector<Point>{{-3,-1},{3,-1},{3,1},{-3,1}});
        const Convex vertical(std::vector<Point>{{-1,-3},{1,-3},{1,3},{-1,3}});
        CHECK(horizontal.crosses(vertical));
        CHECK(vertical.crosses(horizontal));
        // Identical polygons don't cross (no piece outside the other)
        CHECK_FALSE(horizontal.crosses(horizontal));
    }
}

TEST_CASE("Convex::contains(Convex) across nested, disjoint, and degenerate cases") {
    using Point  = pgl::Point<int>;
    using Convex = pgl::Convex<Point>;

    const Convex sq(std::vector<Point>{{0,0},{4,0},{4,4},{0,4}});

    // The implementation dispatches on relative size: when `other` has fewer
    // vertices it tests other's vertices against this (point-in-polygon), and
    // otherwise it tests other against this polygon's edge half-planes. The
    // cases below deliberately span both branches.
    SUBCASE("point-in-polygon branch (other has fewer vertices)") {
        const Convex tri(std::vector<Point>{{1,1},{3,1},{2,3}});      // strictly inside
        CHECK(sq.contains(tri));
        // Triangle on the boundary (all vertices are corners/edges of sq).
        const Convex onEdge(std::vector<Point>{{0,0},{4,0},{4,4}});
        CHECK(sq.contains(onEdge));
        // One vertex pokes out.
        const Convex poke(std::vector<Point>{{1,1},{3,1},{5,3}});
        CHECK_FALSE(sq.contains(poke));
    }

    SUBCASE("half-plane branch (other has >= vertices)") {
        // Nested square of equal vertex count.
        const Convex inner(std::vector<Point>{{1,1},{3,1},{3,3},{1,3}});
        CHECK(sq.contains(inner));
        // Identical polygon is contained (closed containment).
        CHECK(sq.contains(sq));
        // Strictly larger square is not contained.
        const Convex big(std::vector<Point>{{-1,-1},{5,-1},{5,5},{-1,5}});
        CHECK_FALSE(sq.contains(big));
        // Partial overlap.
        const Convex shifted(std::vector<Point>{{2,2},{6,2},{6,6},{2,6}});
        CHECK_FALSE(sq.contains(shifted));
        // Disjoint.
        const Convex away(std::vector<Point>{{10,10},{12,10},{11,12}});
        CHECK_FALSE(sq.contains(away));
    }

    SUBCASE("triangle container holding a larger-vertex-count polygon") {
        const Convex tri(std::vector<Point>{{0,0},{6,0},{0,6}});
        const Convex small(std::vector<Point>{{1,1},{2,1},{2,2},{1,2}});  // 4 verts inside tri
        CHECK(tri.contains(small));
        const Convex outBox(std::vector<Point>{{1,1},{5,1},{5,5},{1,5}});  // pokes past hypotenuse
        CHECK_FALSE(tri.contains(outBox));
    }

    SUBCASE("empty polygon") {
        const Convex empty;
        CHECK(sq.contains(empty));            // a polygon contains the empty set
        CHECK_FALSE(empty.contains(sq));      // the empty set contains nothing
        CHECK_FALSE(empty.contains(empty));   // ... not even itself
    }

    SUBCASE("single-point polygon") {
        const Convex pt(std::vector<Point>{{2,2}});
        CHECK(pt.contains(Convex(std::vector<Point>{{2,2}})));        // same point
        CHECK_FALSE(pt.contains(Convex(std::vector<Point>{{3,3}})));  // different point
        CHECK_FALSE(pt.contains(sq));                                  // can't contain area
        CHECK(sq.contains(pt));                                        // interior point
    }

    SUBCASE("segment polygon (two vertices)") {
        const Convex seg(std::vector<Point>{{0,0},{4,0}});
        CHECK(seg.contains(Convex(std::vector<Point>{{2,0}})));            // point on segment
        CHECK_FALSE(seg.contains(Convex(std::vector<Point>{{2,1}})));      // point off segment
        CHECK(seg.contains(Convex(std::vector<Point>{{1,0},{3,0}})));      // subsegment
        CHECK_FALSE(seg.contains(Convex(std::vector<Point>{{-1,0},{5,0}}))); // longer segment
        CHECK_FALSE(seg.contains(sq));                                     // can't contain area
        CHECK(sq.contains(seg));                                           // segment along bottom edge
    }
}

TEST_CASE("Convex::interiorContains(Convex) requires strict interior containment") {
    using Point  = pgl::Point<int>;
    using Convex = pgl::Convex<Point>;

    // 6x6 square with a clear open interior (strict interior is 0 < x,y < 6).
    const Convex sq(std::vector<Point>{{0,0},{6,0},{6,6},{0,6}});

    // Dispatch is by relative size: when `other` has <= vertices it tests each
    // of other's vertices against this' open interior, otherwise it tests other
    // against the open half-planes of this' edges. Both branches below.
    SUBCASE("strictly inside (vertex branch: other has <= vertices)") {
        const Convex tri(std::vector<Point>{{2,2},{4,2},{3,4}});
        CHECK(sq.interiorContains(tri));
        const Convex inner(std::vector<Point>{{1,1},{5,1},{5,5},{1,5}});
        CHECK(sq.interiorContains(inner));
    }

    SUBCASE("touching the boundary is not strict containment") {
        // Shares the corner (0,0) and edges with sq.
        const Convex corner(std::vector<Point>{{0,0},{5,0},{5,5},{0,5}});
        CHECK_FALSE(sq.interiorContains(corner));
        // One vertex sits on the top edge.
        const Convex onEdge(std::vector<Point>{{2,2},{4,2},{3,6}});
        CHECK_FALSE(sq.interiorContains(onEdge));
        // Identical polygon: all vertices on the boundary.
        CHECK_FALSE(sq.interiorContains(sq));
    }

    SUBCASE("partially or fully outside") {
        const Convex poke(std::vector<Point>{{1,1},{8,1},{8,5},{1,5}});  // crosses right edge
        CHECK_FALSE(sq.interiorContains(poke));
        const Convex away(std::vector<Point>{{10,10},{12,10},{11,12}});
        CHECK_FALSE(sq.interiorContains(away));
    }

    SUBCASE("triangle container holding a strictly-interior square (half-plane branch)") {
        const Convex tri(std::vector<Point>{{0,0},{12,0},{0,12}});
        const Convex small(std::vector<Point>{{2,2},{4,2},{4,4},{2,4}});  // inside, off hypotenuse
        CHECK(tri.interiorContains(small));
        // Square whose far corner lands on the hypotenuse x+y==12.
        const Convex onHyp(std::vector<Point>{{2,2},{8,2},{8,4},{2,4}});
        CHECK_FALSE(tri.interiorContains(onHyp));
    }

    SUBCASE("degenerate container has no interior") {
        const Convex seg(std::vector<Point>{{0,0},{4,0}});
        const Convex pt(std::vector<Point>{{2,2}});
        CHECK_FALSE(seg.interiorContains(Convex(std::vector<Point>{{2,0}})));
        CHECK_FALSE(pt.interiorContains(pt));
        CHECK_FALSE(seg.interiorContains(seg));
    }

    SUBCASE("empty other is vacuously contained, but only by a real polygon") {
        const Convex empty;
        CHECK(sq.interiorContains(empty));
        const Convex seg(std::vector<Point>{{0,0},{4,0}});
        CHECK_FALSE(seg.interiorContains(empty));  // no interior to contain anything
    }

    SUBCASE("single-point and segment others") {
        CHECK(sq.interiorContains(Convex(std::vector<Point>{{3,3}})));          // strict interior point
        CHECK_FALSE(sq.interiorContains(Convex(std::vector<Point>{{0,3}})));    // boundary point
        CHECK(sq.interiorContains(Convex(std::vector<Point>{{1,1},{5,5}})));    // interior segment
        CHECK_FALSE(sq.interiorContains(Convex(std::vector<Point>{{0,3},{5,3}}))); // endpoint on edge
    }
}

// ---------------------------------------------------------------------------
// antipodalPairs() validation against an independent O(n^2) brute force.
//
// A pair (i,j) is antipodal iff the polygon admits two parallel supporting
// lines, one through each vertex. Equivalently the outward-normal cone of i
// and the negated outward-normal cone of j share a direction. With exact
// integer vectors this is decided by signed cross products only.
namespace {

using BPoint = pgl::Point<long long>;

struct Vec { long long x, y; };
static long long cross(Vec a, Vec b) { return a.x * b.y - a.y * b.x; }

// Is direction d inside the closed CCW arc [lo, hi] of angular width < pi?
static bool inArc(Vec lo, Vec hi, Vec d) {
    return cross(lo, d) >= 0 && cross(d, hi) >= 0;
}

template <class Convex>
static std::set<std::pair<std::size_t, std::size_t>> bruteAntipodal(const Convex& c) {
    const std::size_t n = c.size();
    std::set<std::pair<std::size_t, std::size_t>> out;
    if (n < 2) return out;
    if (n == 2) { out.insert({0, 1}); return out; }

    auto vert = [&](std::size_t i) {
        const auto p = c[i];
        return Vec{static_cast<long long>(p.x()), static_cast<long long>(p.y())};
    };
    // Outward normal of edge k = (e.y, -e.x) for a CCW polygon.
    auto normal = [&](std::size_t k) {
        Vec a = vert(k), b = vert((k + 1) % n);
        return Vec{b.y - a.y, -(b.x - a.x)};
    };
    // Vertex i's normal cone is [lo_i, hi_i] = [normal(i-1), normal(i)].
    auto lo = [&](std::size_t i) { return normal((i + n - 1) % n); };
    auto hi = [&](std::size_t i) { return normal(i); };

    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = i + 1; j < n; ++j) {
            Vec lo_i = lo(i), hi_i = hi(i);
            Vec nlo_j = {-lo(j).x, -lo(j).y};  // negated cone of j
            Vec nhi_j = {-hi(j).x, -hi(j).y};
            bool overlap = inArc(lo_i, hi_i, nlo_j) || inArc(lo_i, hi_i, nhi_j) ||
                           inArc(nlo_j, nhi_j, lo_i) || inArc(nlo_j, nhi_j, hi_i);
            if (overlap) out.insert({i, j});
        }
    }
    return out;
}

template <class Convex>
static std::set<std::pair<std::size_t, std::size_t>> methodAntipodal(const Convex& c) {
    std::set<std::pair<std::size_t, std::size_t>> out;
    const auto pairs = c.antipodalPairs();
    for (const auto& [a, b] : pairs) {
        REQUIRE(a < b);                      // normalized
        REQUIRE(out.insert({a, b}).second);  // no duplicates
    }
    return out;
}

}  // namespace

TEST_CASE("Convex::antipodalPairs matches brute force on fixed polygons") {
    using Convex = pgl::Convex<BPoint>;
    auto check = [](std::vector<BPoint> pts) {
        Convex c(pts);
        CHECK(methodAntipodal(c) == bruteAntipodal(c));
    };

    check({{0, 0}, {4, 0}, {0, 3}});                          // triangle
    check({{0, 0}, {1, 0}, {1, 1}, {0, 1}});                  // unit square (parallel edges)
    check({{0, 0}, {6, 0}, {6, 2}, {0, 2}});                  // rectangle
    check({{0, 0}, {4, 0}, {5, 2}, {2, 4}, {-1, 2}});         // convex pentagon
    check({{0, 0}, {2, 0}, {3, 1}, {3, 3}, {1, 4}, {-1, 2}}); // hexagon
    check({{0, 0}, {2, 0}, {3, 2}, {2, 4}, {0, 4}, {-1, 2}}); // hexagon w/ parallel opposite edges
    check({{0, 0}, {10, 1}, {9, 2}});                         // thin triangle
}

TEST_CASE("Convex::antipodalPairs handles degenerate sizes") {
    using Convex = pgl::Convex<BPoint>;
    CHECK(Convex(std::vector<BPoint>{}).antipodalPairs().empty());
    CHECK(Convex(std::vector<BPoint>{{2, 3}}).antipodalPairs().empty());
    const auto seg = Convex(std::vector<BPoint>{{0, 0}, {5, 2}}).antipodalPairs();
    REQUIRE(seg.size() == 1);
    CHECK(seg[0] == std::pair<std::size_t, std::size_t>{0, 1});
}

TEST_CASE("Convex::antipodalPairs matches brute force on random hulls") {
    using Convex = pgl::Convex<BPoint>;
    std::mt19937 rng(12345);
    std::uniform_int_distribution<int> coord(-40, 40);
    std::uniform_int_distribution<int> count(3, 30);
    for (int trial = 0; trial < 2000; ++trial) {
        std::vector<BPoint> pts;
        const int m = count(rng);
        for (int k = 0; k < m; ++k) pts.push_back(BPoint(coord(rng), coord(rng)));
        Convex c(pts);
        if (c.size() < 3) continue;  // degenerate hulls covered separately
        const auto got = methodAntipodal(c);
        const auto want = bruteAntipodal(c);
        CHECK_MESSAGE(got == want, "n=", c.size(), " trial=", trial);
    }
}

TEST_CASE("Convex::diameter matches the brute-force farthest vertex pair") {
    using Convex = pgl::Convex<BPoint>;

    auto bruteMaxSquared = [](const Convex& c) -> long long {
        const std::size_t n = c.size();
        long long best = 0;
        for (std::size_t i = 0; i < n; ++i) {
            for (std::size_t j = i + 1; j < n; ++j) {
                const auto a = c[i];
                const auto b = c[j];
                const long long dx = static_cast<long long>(a.x()) - static_cast<long long>(b.x());
                const long long dy = static_cast<long long>(a.y()) - static_cast<long long>(b.y());
                best = std::max(best, dx * dx + dy * dy);
            }
        }
        return best;
    };

    std::mt19937 rng(98765);
    std::uniform_int_distribution<int> coord(-50, 50);
    std::uniform_int_distribution<int> count(3, 30);
    for (int trial = 0; trial < 2000; ++trial) {
        std::vector<BPoint> pts;
        const int m = count(rng);
        for (int k = 0; k < m; ++k) pts.push_back(BPoint(coord(rng), coord(rng)));
        Convex c(pts);
        if (c.size() < 2) continue;
        const auto d = c.diameter();
        const long long got = d.min().squaredDistance(d.max());
        CHECK_MESSAGE(got == bruteMaxSquared(c), "n=", c.size(), " trial=", trial);
    }

    SUBCASE("degenerate sizes") {
        CHECK(Convex(std::vector<BPoint>{}).diameter() == pgl::Segment<BPoint>());
        const auto pt = Convex(std::vector<BPoint>{{3, 7}}).diameter();
        CHECK(pt.min() == BPoint(3, 7));
        CHECK(pt.max() == BPoint(3, 7));
        const auto seg = Convex(std::vector<BPoint>{{0, 0}, {6, 8}}).diameter();
        CHECK(seg.min().squaredDistance(seg.max()) == 100);
    }
}

TEST_CASE("Convex hashing is consistent and cached across mutations") {
    using Point = pgl::Point<int>;
    using Convex = pgl::Convex<Point>;
    const std::hash<Convex> hasher{};

    const Convex square(std::vector<Point>{{0, 0}, {4, 0}, {4, 3}, {0, 3}});

    SUBCASE("equal polygons hash equally regardless of input vertex order") {
        // Same square, vertices given starting from a different corner.
        const Convex rotated(std::vector<Point>{{4, 3}, {0, 3}, {0, 0}, {4, 0}});
        REQUIRE(square == rotated);
        CHECK(hasher(square) == hasher(rotated));
    }

    SUBCASE("repeated hashing returns the same cached value") {
        const auto first = hasher(square);
        CHECK(hasher(square) == first);
        CHECK(hasher(square) == first);
    }

    SUBCASE("translation invalidates the cache and recomputes correctly") {
        Convex c = square;
        const auto original = hasher(c);  // primes the cache

        c += Point(10, 7);
        const Convex shifted(std::vector<Point>{{10, 7}, {14, 7}, {14, 10}, {10, 10}});
        REQUIRE(c == shifted);
        CHECK(hasher(c) == hasher(shifted));

        c -= Point(10, 7);  // back to the original position
        REQUIRE(c == square);
        CHECK(hasher(c) == original);
    }

    SUBCASE("scaling invalidates the cache") {
        Convex c = square;
        const auto original = hasher(c);  // primes the cache
        c *= 2;
        const Convex scaled(std::vector<Point>{{0, 0}, {8, 0}, {8, 6}, {0, 6}});
        REQUIRE(c == scaled);
        CHECK(hasher(c) == hasher(scaled));
        c /= 2;
        REQUIRE(c == square);
        CHECK(hasher(c) == original);
    }

    SUBCASE("usable as a key in an unordered_set") {
        std::unordered_set<Convex> seen;
        seen.insert(square);
        seen.insert(Convex(std::vector<Point>{{4, 3}, {0, 3}, {0, 0}, {4, 0}}));  // equal
        CHECK(seen.size() == 1);
        seen.insert(Convex(std::vector<Point>{{0, 0}, {1, 0}, {0, 1}}));  // different
        CHECK(seen.size() == 2);
    }
}

TEST_CASE("Convex squaredDistance to a point") {
    using Point = pgl::Point<int>;
    using Convex = pgl::Convex<Point>;
    using pgl::ERational;

    SUBCASE("interior and boundary points are at distance zero") {
        const Convex square(std::vector<Point>{{0, 0}, {4, 0}, {4, 4}, {0, 4}});
        CHECK(square.squaredDistance<ERational>(Point(2, 2)) == 0);  // interior
        CHECK(square.squaredDistance<ERational>(Point(4, 2)) == 0);  // on an edge
        CHECK(square.squaredDistance<ERational>(Point(4, 4)) == 0);  // on a vertex
    }

    SUBCASE("exterior points: nearest is an edge or a vertex") {
        const Convex square(std::vector<Point>{{0, 0}, {4, 0}, {4, 4}, {0, 4}});
        CHECK(square.squaredDistance<ERational>(Point(7, 2)) == 9);    // perpendicular to an edge
        CHECK(square.squaredDistance<ERational>(Point(7, 8)) == 25);   // nearest is corner (4,4)
        CHECK(square.squaredDistance<ERational>(Point(2, -3)) == 9);   // below an edge
        CHECK(square.squaredDistance<int>(Point(7, 2)) == 9);          // default integer result
    }

    SUBCASE("nearest point may lie far from the nearest vertex") {
        // Long thin triangle: the closest vertex is one tip while the closest
        // point sits on the opposite long edge.
        const Convex triangle(std::vector<Point>{{-23, 23}, {16, 0}, {26, 4}});
        ERational brute = triangle.edges().front().squaredDistance<ERational>(Point(8, 12));
        for (const auto& e : triangle.edges()) {
            brute = std::min(brute, e.squaredDistance<ERational>(Point(8, 12)));
        }
        CHECK(triangle.squaredDistance<ERational>(Point(8, 12)) == brute);
    }

    SUBCASE("matches an O(n) brute-force scan over random convex polygons") {
        std::mt19937 rng(2026);
        std::uniform_int_distribution<int> coord(-25, 25);
        for (int trial = 0; trial < 4000; ++trial) {
            std::vector<Point> pts;
            const int count = 3 + static_cast<int>(rng() % 9);
            for (int i = 0; i < count; ++i) {
                pts.emplace_back(coord(rng), coord(rng));
            }
            const Convex convex(pts);
            if (convex.isDegenerate()) {
                continue;
            }
            const Point q(coord(rng), coord(rng));

            ERational expected = convex.contains(q) ? ERational{0}
                : convex.edges().front().squaredDistance<ERational>(q);
            for (const auto& e : convex.edges()) {
                expected = std::min(expected, e.squaredDistance<ERational>(q));
            }
            if (convex.contains(q)) {
                expected = ERational{0};
            }

            CHECK(convex.squaredDistance<ERational>(q) == expected);
        }
    }
}

TEST_CASE("Convex squaredDistance to a segment") {
    using Point = pgl::Point<int>;
    using Convex = pgl::Convex<Point>;
    using Segment = pgl::Segment<Point>;
    using pgl::ERational;

    SUBCASE("intersecting segments are at distance zero") {
        const Convex square(std::vector<Point>{{0, 0}, {4, 0}, {4, 4}, {0, 4}});
        CHECK(square.squaredDistance<ERational>(Segment(Point(2, 2), Point(8, 2))) == 0);  // crosses
        CHECK(square.squaredDistance<ERational>(Segment(Point(1, 1), Point(3, 3))) == 0);  // inside
        CHECK(square.squaredDistance<ERational>(Segment(Point(4, 0), Point(4, 4))) == 0);  // along an edge
    }

    SUBCASE("disjoint: nearest is endpoint, perpendicular, or vertex") {
        const Convex square(std::vector<Point>{{0, 0}, {4, 0}, {4, 4}, {0, 4}});
        // Segment to the right, parallel: perpendicular to the right edge.
        CHECK(square.squaredDistance<ERational>(Segment(Point(7, 1), Point(7, 3))) == 9);
        // Segment endpoint nearest a corner.
        CHECK(square.squaredDistance<ERational>(Segment(Point(7, 8), Point(9, 10))) == 25);
        // Polygon vertex nearest the segment interior.
        CHECK(square.squaredDistance<ERational>(Segment(Point(-3, 7), Point(7, 7))) == 9);
    }

    SUBCASE("matches an O(n) brute-force scan over random convex polygons") {
        std::mt19937 rng(7);
        std::uniform_int_distribution<int> coord(-30, 30);
        for (int trial = 0; trial < 4000; ++trial) {
            std::vector<Point> pts;
            const int count = 3 + static_cast<int>(rng() % 9);
            for (int i = 0; i < count; ++i) {
                pts.emplace_back(coord(rng), coord(rng));
            }
            const Convex convex(pts);
            if (convex.isDegenerate()) {
                continue;
            }
            const Segment segment(Point(coord(rng), coord(rng)), Point(coord(rng), coord(rng)));

            ERational expected = ERational{0};
            if (!convex.intersects(segment)) {
                expected = convex.edges().front().squaredDistance<ERational>(segment);
                for (const auto& e : convex.edges()) {
                    expected = std::min(expected, e.squaredDistance<ERational>(segment));
                }
            }

            CHECK(convex.squaredDistance<ERational>(segment) == expected);
        }
    }

    SUBCASE("large polygons exercise the O(log n) path against a brute-force scan") {
        // Generate convex polygons with well over 32 vertices so the logarithmic
        // branch runs, and compare against the O(n) edge scan.
        std::mt19937 rng(11);
        std::uniform_int_distribution<int> jitter(0, 3);
        std::uniform_int_distribution<int> coord(-200, 200);
        for (int trial = 0; trial < 200; ++trial) {
            std::vector<Point> pts;
            const int count = 60 + static_cast<int>(rng() % 40);
            for (int i = 0; i < count; ++i) {
                // Points on a large circle stay in convex position.
                const double t = 2.0 * 3.14159265358979 * i / count;
                const int x = static_cast<int>(150.0 * std::cos(t)) + jitter(rng);
                const int y = static_cast<int>(150.0 * std::sin(t)) + jitter(rng);
                pts.emplace_back(x, y);
            }
            const Convex convex(pts);
            if (convex.isDegenerate() || convex.size() <= 32) {
                continue;
            }
            const Segment segment(Point(coord(rng), coord(rng)), Point(coord(rng), coord(rng)));

            ERational expected = ERational{0};
            if (!convex.intersects(segment)) {
                expected = convex.edges().front().squaredDistance<ERational>(segment);
                for (const auto& e : convex.edges()) {
                    expected = std::min(expected, e.squaredDistance<ERational>(segment));
                }
            }

            CHECK(convex.squaredDistance<ERational>(segment) == expected);
        }
    }
}

TEST_CASE("Convex squaredDistance to an oriented segment matches the unoriented one") {
    using Point = pgl::Point<int>;
    using Convex = pgl::Convex<Point>;
    using Segment = pgl::Segment<Point>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using pgl::ERational;

    std::mt19937 rng(13);
    std::uniform_int_distribution<int> coord(-30, 30);
    for (int trial = 0; trial < 4000; ++trial) {
        std::vector<Point> pts;
        const int count = 3 + static_cast<int>(rng() % 9);
        for (int i = 0; i < count; ++i) {
            pts.emplace_back(coord(rng), coord(rng));
        }
        const Convex convex(pts);
        if (convex.isDegenerate()) {
            continue;
        }
        const Point u(coord(rng), coord(rng));
        const Point v(coord(rng), coord(rng));

        // Distance ignores orientation, so both endpoint orderings must agree
        // with the unoriented segment.
        const ERational expected = convex.squaredDistance<ERational>(Segment(u, v));
        CHECK(convex.squaredDistance<ERational>(OrientedSegment(u, v)) == expected);
        CHECK(convex.squaredDistance<ERational>(OrientedSegment(v, u)) == expected);
    }
}
