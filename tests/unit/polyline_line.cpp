#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

using Point = pgl::Point<int>;
using Line = pgl::Line<Point>;
using OrientedLine = pgl::OrientedLine<Point>;
// Aliased as `PLine` (not `Polyline`) because doctest pulls in <windows.h> on
// MSVC, whose GDI `Polyline` function collides with a file-scope `using
// Polyline`.
using PLine = pgl::Polyline<Point>;

// zig-zag: (0,0) - (2,2) - (4,0)
static const PLine zig({0, 0, 2, 2, 4, 0});
// closed square loop, first vertex equal to the last
static const PLine loop({0, 0, 2, 0, 2, 2, 0, 2, 0, 0});

TEST_CASE("Polyline and Line intersection predicates, both directions") {
    const Line crossing({1, 0}, {1, 1});      // x = 1, crosses the first edge
    const Line above({0, 3}, {1, 3});         // y = 3, misses
    const Line touching({0, 0}, {1, -1});     // touches only the extreme (0,0)

    CHECK(zig.intersects(crossing));
    CHECK(crossing.intersects(zig));
    CHECK_FALSE(zig.intersects(above));
    CHECK_FALSE(above.intersects(zig));
    CHECK(zig.intersects(touching));

    // The touching line meets the polyline only at its extreme vertex.
    CHECK(zig.interiorsIntersect(crossing));
    CHECK_FALSE(zig.interiorsIntersect(touching));

    // The apex is a non-extreme vertex, hence an interior point.
    const Line atApex({0, 2}, {1, 2});
    CHECK(zig.interiorsIntersect(atApex));

    const OrientedLine oriented({1, 0}, {1, 1});
    CHECK(zig.intersects(oriented));
    CHECK(zig.interiorsIntersect(oriented));

    SUBCASE("single-vertex and empty polylines") {
        CHECK(PLine({Point(1, 1)}).intersects(Line({0, 0}, {2, 2})));
        CHECK_FALSE(PLine({Point(1, 2)}).intersects(Line({0, 0}, {2, 2})));
        CHECK_FALSE(PLine().intersects(Line({0, 0}, {2, 2})));
    }
}

TEST_CASE("A line contains a polyline only when the polyline lies along it") {
    const Line diagonal({0, 0}, {1, 1});
    const PLine straight({0, 0, 1, 1, 3, 3});
    const PLine backtracking({0, 0, 3, 3, 1, 1});   // revisits parts of the line
    CHECK(diagonal.contains(straight));
    CHECK(diagonal.contains(backtracking));
    CHECK(diagonal.interiorContains(straight));
    CHECK_FALSE(diagonal.boundaryContains(straight));
    CHECK_FALSE(diagonal.contains(zig));
    CHECK_FALSE(zig.contains(diagonal));   // a bounded polyline holds no line
    CHECK_FALSE(zig.interiorContains(diagonal));
    CHECK_FALSE(zig.boundaryContains(diagonal));
}

TEST_CASE("Polyline and Line separates and crosses") {
    const Line crossing({1, 0}, {1, 1});
    CHECK(zig.separates(crossing));
    CHECK(crossing.separates(zig));
    CHECK(zig.crosses(crossing));
    CHECK(crossing.crosses(zig));

    // y = 0 meets the zig-zag only at its two extreme vertices: removing them
    // leaves the middle in one piece.
    const Line baseline({0, 0}, {1, 0});
    CHECK_FALSE(baseline.separates(zig));
    CHECK(zig.separates(baseline));   // any contact disconnects a line

    // y = 2 touches only the apex, an interior point of the polyline.
    const Line atApex({0, 2}, {1, 2});
    CHECK(atApex.separates(zig));
    CHECK(zig.crosses(atApex));

    SUBCASE("set semantics: a loop resists cuts a chain would not") {
        // x = 1 crosses the loop twice: two arcs remain.
        CHECK(Line({1, 0}, {1, 1}).separates(loop));
        // y = 0 swallows the loop's bottom edge; the other three sides stay
        // connected end to end.
        CHECK_FALSE(Line({0, 0}, {1, 0}).separates(loop));
        // The diagonal through the corner (0,0) touches the loop at a single
        // point, where the loop reconnects through its closing vertex.
        CHECK_FALSE(Line({0, 0}, {1, -1}).separates(loop));
        // The loop still disconnects each of these lines.
        CHECK(loop.separates(Line({1, 0}, {1, 1})));
        CHECK(loop.separates(Line({0, 0}, {1, -1})));
    }
}

TEST_CASE("Polyline and Line distance") {
    using Rational = pgl::Rational<int>;

    const Line above({0, 3}, {1, 3});   // nearest polyline point is the apex
    CHECK(zig.squaredDistance<Rational>(above) == Rational(1));
    CHECK(above.squaredDistance<Rational>(zig) == Rational(1));
    CHECK(zig.distanceL1<Rational>(above) == Rational(1));
    CHECK(zig.distanceLInf<Rational>(above) == Rational(1));
    CHECK(zig.squaredDistance<Rational>(Line({1, 0}, {1, 1})) == Rational(0));

    const OrientedLine orientedAbove({0, 3}, {1, 3});
    CHECK(zig.squaredDistance<Rational>(orientedAbove) == Rational(1));
    CHECK(zig.distanceL1<Rational>(orientedAbove) == Rational(1));
    CHECK(zig.distanceLInf<Rational>(orientedAbove) == Rational(1));
}

TEST_CASE("Polyline and OrientedSegment delegate to the unoriented segment") {
    using OrientedSegment = pgl::OrientedSegment<Point>;

    const OrientedSegment crossing(Point(1, 3), Point(1, -1));  // reversed direction
    CHECK(zig.intersects(crossing));
    CHECK(zig.interiorsIntersect(crossing));
    CHECK(zig.separates(crossing));
    CHECK(zig.crosses(crossing));
    CHECK_FALSE(zig.contains(crossing));
    CHECK_FALSE(zig.boundaryContains(crossing));

    const OrientedSegment onEdge(Point(2, 2), Point(1, 1));
    CHECK(zig.contains(onEdge));
    CHECK(zig.interiorContains(onEdge));
    const OrientedSegment carrier(Point(3, 3), Point(0, 0));
    CHECK(carrier.contains(PLine({1, 1, 2, 2})));
    CHECK(carrier.interiorContains(PLine({1, 1, 2, 2})));
    CHECK_FALSE(carrier.interiorContains(PLine({0, 0, 2, 2})));  // touches an endpoint
    CHECK_FALSE(carrier.boundaryContains(PLine({1, 1, 2, 2})));
    CHECK_FALSE(carrier.separates(PLine({1, 1, 2, 2})));         // swallowed entirely

    const OrientedSegment away(Point(6, 0), Point(7, 0));
    CHECK(zig.squaredDistance<double>(away) == doctest::Approx(4.0));
    CHECK(zig.distanceL1(away) == 2);
    CHECK(zig.distanceLInf(away) == 2);
}

TEST_CASE("Polyline and Line intersection pieces") {
    using Segment = pgl::Segment<Point>;

    SUBCASE("a line crossing both edges") {
        const auto pieces = zig.intersection(Line({0, 1}, {1, 1}));  // y = 1
        REQUIRE(pieces.size() == 2);
        REQUIRE(std::holds_alternative<Point>(pieces[0]));
        CHECK(std::get<Point>(pieces[0]) == Point(1, 1));
        REQUIRE(std::holds_alternative<Point>(pieces[1]));
        CHECK(std::get<Point>(pieces[1]) == Point(3, 1));
    }

    SUBCASE("a line along an edge overlaps in a segment") {
        const auto pieces = zig.intersection(Line({0, 0}, {1, 1}));  // y = x
        REQUIRE(pieces.size() == 1);
        REQUIRE(std::holds_alternative<Segment>(pieces[0]));
        CHECK(std::get<Segment>(pieces[0]) == Segment(Point(0, 0), Point(2, 2)));
    }

    SUBCASE("a vertical line through the closed loop") {
        const auto pieces = loop.intersection(Line({1, 0}, {1, 1}));  // x = 1
        REQUIRE(pieces.size() == 2);
        REQUIRE(std::holds_alternative<Point>(pieces[0]));
        CHECK(std::get<Point>(pieces[0]) == Point(1, 0));
        REQUIRE(std::holds_alternative<Point>(pieces[1]));
        CHECK(std::get<Point>(pieces[1]) == Point(1, 2));
    }

    SUBCASE("oriented lines delegate to the unoriented overload") {
        const auto pieces = zig.intersection(OrientedLine({4, 1}, {0, 1}));
        REQUIRE(pieces.size() == 2);
        REQUIRE(std::holds_alternative<Point>(pieces[0]));
        CHECK(std::get<Point>(pieces[0]) == Point(1, 1));
    }
}
