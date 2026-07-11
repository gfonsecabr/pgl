#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

using Point = pgl::Point<int>;
// Aliased as `PLine` (not `Polyline`) because doctest pulls in <windows.h> on
// MSVC, whose GDI `Polyline` function collides with a file-scope `using
// Polyline`.
using PLine = pgl::Polyline<Point>;

// zig-zag: (0,0) - (2,2) - (4,0)
static const PLine zig({0, 0, 2, 2, 4, 0});
// bow-tie crossing itself at (1,1): (0,0) - (2,2) - (2,0) - (0,2)
static const PLine bow({0, 0, 2, 2, 2, 0, 0, 2});
// closed square written as a polyline: first vertex equals the last
static const PLine loop({0, 0, 2, 0, 2, 2, 0, 2, 0, 0});

TEST_CASE("Polyline contains Point") {
    CHECK(zig.contains(Point(0, 0)));
    CHECK(zig.contains(Point(2, 2)));
    CHECK(zig.contains(Point(1, 1)));  // inside the first edge
    CHECK(zig.contains(Point(3, 1)));  // inside the second edge
    CHECK(!zig.contains(Point(1, 0)));
    CHECK(!zig.contains(Point(5, 5)));

    SUBCASE("a self-intersection point is contained") {
        CHECK(bow.contains(Point(1, 1)));
    }

    SUBCASE("single-vertex polyline") {
        const PLine dot({Point(3, 4)});
        CHECK(dot.contains(Point(3, 4)));
        CHECK(!dot.contains(Point(3, 5)));
    }

    SUBCASE("empty polyline contains nothing") {
        CHECK(!PLine().contains(Point(0, 0)));
    }
}

TEST_CASE("Polyline boundaryContains Point") {
    CHECK(zig.boundaryContains(Point(0, 0)));
    CHECK(zig.boundaryContains(Point(4, 0)));
    CHECK(!zig.boundaryContains(Point(2, 2)));
    CHECK(!zig.boundaryContains(Point(1, 1)));
    CHECK(!PLine().boundaryContains(Point(0, 0)));

    SUBCASE("the point's boundary is empty") {
        CHECK(!Point(0, 0).boundaryContains(zig));
    }
}

TEST_CASE("Polyline interiorContains Point") {
    CHECK(zig.interiorContains(Point(2, 2)));  // non-extreme vertex
    CHECK(zig.interiorContains(Point(1, 1)));  // edge interior
    CHECK(!zig.interiorContains(Point(0, 0)));
    CHECK(!zig.interiorContains(Point(4, 0)));
    CHECK(!zig.interiorContains(Point(1, 0)));

    SUBCASE("a closed polyline still excludes its doubled extreme point") {
        CHECK(loop.contains(Point(0, 0)));
        CHECK(!loop.interiorContains(Point(0, 0)));
        CHECK(loop.interiorContains(Point(1, 0)));
    }

    SUBCASE("point interior-contains a degenerate polyline") {
        CHECK(Point(1, 1).interiorContains(PLine({1, 1, 1, 1})));
        CHECK(!Point(1, 1).interiorContains(zig));
    }
}

TEST_CASE("Polyline intersects Point in both directions") {
    CHECK(zig.intersects(Point(3, 1)));
    CHECK(!zig.intersects(Point(3, 2)));
    CHECK(Point(3, 1).intersects(zig));  // through the rank forwarder
    CHECK(!Point(3, 2).intersects(zig));

    CHECK(zig.interiorsIntersect(Point(2, 2)));
    CHECK(!zig.interiorsIntersect(Point(0, 0)));
    CHECK(Point(2, 2).interiorsIntersect(zig));
}

TEST_CASE("Point contains Polyline") {
    CHECK(Point(1, 1).contains(PLine({1, 1, 1, 1})));
    CHECK(!Point(1, 1).contains(zig));
    CHECK(Point(1, 1).contains(PLine()));
}

TEST_CASE("Point separates Polyline") {
    SUBCASE("an interior point cuts an open chain") {
        CHECK(Point(1, 0).separates(PLine({0, 0, 2, 0, 2, 2})));
        CHECK(Point(2, 0).separates(PLine({0, 0, 2, 0, 2, 2})));  // interior vertex
    }

    SUBCASE("an extreme point does not cut") {
        CHECK(!Point(0, 0).separates(PLine({0, 0, 2, 0, 2, 2})));
        CHECK(!Point(2, 2).separates(PLine({0, 0, 2, 0, 2, 2})));
    }

    SUBCASE("a point off the polyline does not cut") {
        CHECK(!Point(5, 5).separates(zig));
    }

    SUBCASE("set semantics: a closed polyline reconnects around the removed point") {
        CHECK(!Point(1, 0).separates(loop));
        CHECK(!Point(2, 2).separates(loop));
    }

    SUBCASE("a loop with a tail: cutting the tail separates, cutting the loop does not") {
        const PLine loopTail({-2, 0, 0, 0, 2, 0, 2, 2, 0, 2, 0, 0});
        CHECK(Point(-1, 0).separates(loopTail));
        CHECK(!Point(1, 2).separates(loopTail));
    }

    SUBCASE("polyline never separates a point") {
        CHECK(!zig.separates(Point(1, 1)));
        CHECK(!zig.crosses(Point(1, 1)));
    }
}

TEST_CASE("Polyline squaredDistance to Point") {
    CHECK(zig.squaredDistance(Point(1, 1)) == 0);
    CHECK(zig.squaredDistance<double>(Point(2, 3)) == doctest::Approx(1.0));
    CHECK(zig.squaredDistance<double>(Point(5, 0)) == doctest::Approx(1.0));
    CHECK(Point(2, 3).squaredDistance<double>(zig) == doctest::Approx(1.0));

    // The nearest edge is not necessarily the first one.
    CHECK(zig.squaredDistance<double>(Point(4, -2)) == doctest::Approx(4.0));
}

TEST_CASE("Polyline distanceL1 and distanceLInf to Point") {
    CHECK(zig.distanceL1(Point(3, 1)) == 0);
    CHECK(zig.distanceL1(Point(4, 2)) == 2);
    CHECK(Point(4, 2).distanceL1(zig) == 2);
    CHECK(zig.distanceLInf(Point(4, 2)) == 1);
    CHECK(Point(4, 2).distanceLInf(zig) == 1);
    CHECK(zig.distanceL1<double>(Point(0, -1)) == doctest::Approx(1.0));
}

TEST_CASE("Polyline and Point intersection") {
    const auto hit = zig.intersection(Point(1, 1));
    REQUIRE(hit.has_value());
    CHECK(*hit == Point(1, 1));
    CHECK_FALSE(zig.intersection(Point(1, 2)).has_value());

    // The point forwards to the polyline's overload.
    const auto forwarded = Point(2, 2).intersection(zig);
    REQUIRE(forwarded.has_value());
    CHECK(*forwarded == Point(2, 2));
}
