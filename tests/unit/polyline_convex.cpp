#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <vector>

using Point = pgl::Point<int>;
using Convex = pgl::Convex<Point>;
// Aliased as `PLine` (not `Polyline`) because doctest pulls in <windows.h> on
// MSVC, whose GDI `Polyline` function collides with a file-scope `using
// Polyline`.
using PLine = pgl::Polyline<Point>;

// Convex square [0,4] x [0,4].
static const Convex square(std::vector<Point>{
    Point(0, 0), Point(4, 0), Point(4, 4), Point(0, 4)});
// zig-zag: (0,0) - (2,2) - (4,0)
static const PLine zig({0, 0, 2, 2, 4, 0});

TEST_CASE("Polyline and Convex containment and intersection") {
    CHECK(zig.intersects(square));
    CHECK(square.intersects(zig));
    CHECK(square.contains(zig));
    CHECK_FALSE(square.interiorContains(zig));   // extremes on the boundary
    CHECK(square.interiorContains(PLine({1, 1, 2, 2, 3, 1})));
    CHECK_FALSE(zig.contains(square));
    CHECK_FALSE(zig.interiorContains(square));
    CHECK_FALSE(zig.boundaryContains(square));
    CHECK(zig.interiorsIntersect(square));

    SUBCASE("a polyline turning a corner of the boundary is boundary-contained") {
        const PLine rim({2, 0, 4, 0, 4, 3});
        CHECK(square.boundaryContains(rim));
        CHECK_FALSE(square.interiorContains(rim));
        CHECK_FALSE(rim.interiorsIntersect(square));
    }

    SUBCASE("containment of a two-vertex convex reduces to its segment") {
        const Convex flat(std::vector<Point>{Point(0, 0), Point(2, 2)});
        CHECK(zig.contains(flat));
        CHECK_FALSE(zig.contains(Convex(std::vector<Point>{Point(0, 0), Point(4, 0)})));
    }
}

TEST_CASE("Convex separates a polyline") {
    // A small square over the apex removes the middle of the zig-zag.
    const Convex apexBox(std::vector<Point>{
        Point(1, 1), Point(3, 1), Point(3, 3), Point(1, 3)});
    CHECK(apexBox.separates(zig));
    CHECK_FALSE(square.separates(zig));   // swallowed

    SUBCASE("set semantics: the loop reconnects around one bite") {
        const PLine loop({0, 0, 4, 0, 4, 4, 0, 4, 0, 0});
        const Convex cornerBox(std::vector<Point>{
            Point(-1, -1), Point(1, -1), Point(1, 1), Point(-1, 1)});
        CHECK_FALSE(cornerBox.separates(loop));
    }
}

TEST_CASE("Polyline separates a convex polygon") {
    SUBCASE("a chord cuts") {
        CHECK(PLine({-1, 2, 5, 2}).separates(square));
        CHECK(PLine({-1, 2, 5, 2}).crosses(square));
    }

    SUBCASE("the zig-zag crosscut seals a pocket against the bottom edge") {
        CHECK(zig.separates(square));
    }

    SUBCASE("slits do not cut") {
        CHECK_FALSE(PLine({-1, 2, 2, 2}).separates(square));
        CHECK_FALSE(PLine({1, 1, 3, 3}).separates(square));
    }

    SUBCASE("a loop strictly inside seals a pocket") {
        const PLine loop({1, 1, 3, 1, 3, 3, 1, 3, 1, 1});
        CHECK(loop.separates(square));
        CHECK_FALSE(loop.crosses(square));
    }

    SUBCASE("a run along the boundary does not cut") {
        CHECK_FALSE(PLine({0, 0, 4, 0}).separates(square));
    }
}

TEST_CASE("Polyline and Convex distance") {
    using Rational = pgl::Rational<int>;

    const Convex away(std::vector<Point>{
        Point(6, 0), Point(8, 0), Point(8, 2), Point(6, 2)});
    CHECK(zig.squaredDistance<Rational>(away) == Rational(4));
    CHECK(away.squaredDistance<Rational>(zig) == Rational(4));
    CHECK(zig.distanceL1<Rational>(away) == Rational(2));
    CHECK(zig.distanceLInf<Rational>(away) == Rational(2));
    CHECK(zig.squaredDistance<Rational>(square) == Rational(0));
}

TEST_CASE("Polyline and Convex intersection pieces") {
    using Segment = pgl::Segment<Point>;

    SUBCASE("a containing convex returns the edges themselves") {
        const auto pieces = zig.intersection(square);
        REQUIRE(pieces.size() == 2);
        REQUIRE(std::holds_alternative<Segment>(pieces[0]));
        CHECK(std::get<Segment>(pieces[0]) == Segment(Point(0, 0), Point(2, 2)));
        REQUIRE(std::holds_alternative<Segment>(pieces[1]));
        CHECK(std::get<Segment>(pieces[1]) == Segment(Point(2, 2), Point(4, 0)));
    }

    SUBCASE("an edge is clipped at the convex boundary") {
        const auto pieces = PLine({-1, 2, 5, 2}).intersection(square);
        REQUIRE(pieces.size() == 1);
        REQUIRE(std::holds_alternative<Segment>(pieces[0]));
        CHECK(std::get<Segment>(pieces[0]) == Segment(Point(0, 2), Point(4, 2)));
    }
}
