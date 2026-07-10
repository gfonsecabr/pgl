#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

using Point = pgl::Point<int>;
using Triangle = pgl::Triangle<Point>;
// Aliased as `PLine` (not `Polyline`) because doctest pulls in <windows.h> on
// MSVC, whose GDI `Polyline` function collides with a file-scope `using
// Polyline`.
using PLine = pgl::Polyline<Point>;

// Right triangle with legs on the axes.
static const Triangle tri(Point(0, 0), Point(8, 0), Point(0, 8));
// zig-zag inside the triangle's corner: (1,1) - (2,3) - (3,1)
static const PLine zig({1, 1, 2, 3, 3, 1});

TEST_CASE("Polyline and Triangle containment and intersection") {
    CHECK(zig.intersects(tri));
    CHECK(tri.intersects(zig));
    CHECK(tri.contains(zig));
    CHECK(tri.interiorContains(zig));
    CHECK_FALSE(tri.boundaryContains(zig));
    CHECK_FALSE(zig.contains(tri));
    CHECK_FALSE(zig.interiorContains(tri));
    CHECK_FALSE(zig.boundaryContains(tri));
    CHECK(zig.interiorsIntersect(tri));

    SUBCASE("a polyline along the triangle boundary") {
        const PLine rim({4, 0, 0, 0, 0, 4});   // runs along both legs
        CHECK(tri.boundaryContains(rim));
        CHECK(tri.contains(rim));
        CHECK_FALSE(tri.interiorContains(rim));
        CHECK(rim.intersects(tri));
        CHECK_FALSE(rim.interiorsIntersect(tri));   // rim on ∂, no interior meet
    }

    SUBCASE("disjoint pair") {
        const PLine away({9, 9, 10, 10});
        CHECK_FALSE(away.intersects(tri));
        CHECK_FALSE(tri.intersects(away));
    }
}

TEST_CASE("Triangle separates a polyline") {
    // The wide V dips into the triangle between its ends.
    const PLine wide({-2, 2, 2, -2, 6, 2});
    CHECK(tri.separates(wide));

    // A polyline swallowed whole is not separated.
    CHECK_FALSE(tri.separates(zig));

    SUBCASE("set semantics: a loop survives one bite") {
        // A diamond loop around the corner (0,0): the triangle removes the
        // part inside it, but the outside arc stays connected.
        const PLine diamond({-4, 0, 0, -4, 4, 0, 0, 4, -4, 0});
        CHECK_FALSE(tri.separates(diamond));
    }
}

TEST_CASE("Polyline separates a triangle") {
    SUBCASE("a chord cuts") {
        CHECK(PLine({-1, 1, 9, 1}).separates(tri));
        CHECK(PLine({-1, 1, 9, 1}).crosses(tri));
    }

    SUBCASE("a crosscut through the interior between two boundary points") {
        CHECK(PLine({0, 0, 2, 2, 4, 0}).separates(tri));
    }

    SUBCASE("slits do not cut") {
        CHECK_FALSE(zig.separates(tri));               // strictly inside
        CHECK_FALSE(PLine({-1, 1, 3, 1}).separates(tri)); // enters, ends inside
    }

    SUBCASE("a loop strictly inside seals a pocket") {
        const PLine loop({1, 1, 3, 1, 1, 3, 1, 1});
        CHECK(loop.separates(tri));
        CHECK_FALSE(tri.separates(loop));
        CHECK_FALSE(loop.crosses(tri));
    }

    SUBCASE("a loop with a tail entering from outside") {
        // Enters at (-1,4) -> (1,1), loops (1,1)-(3,1)-(1,3)-(1,1): the scan
        // order never leaves the triangle after entering, but the loop still
        // seals a pocket.
        const PLine lasso({-1, 4, 1, 1, 3, 1, 1, 3, 1, 1});
        CHECK(lasso.separates(tri));
    }

    SUBCASE("a run along the boundary does not cut") {
        CHECK_FALSE(PLine({0, 0, 4, 0}).separates(tri));
    }
}

TEST_CASE("Polyline and Triangle distance") {
    using Rational = pgl::Rational<int>;

    const Triangle away(Point(10, 0), Point(12, 0), Point(10, 2));
    // Nearest pair: zig vertex (3,1) and the triangle's vertical leg at (10,1).
    CHECK(zig.squaredDistance<Rational>(away) == Rational(49));
    CHECK(away.squaredDistance<Rational>(zig) == Rational(49));
    CHECK(zig.distanceL1<Rational>(away) == Rational(7));
    CHECK(zig.distanceLInf<Rational>(away) == Rational(7));
    CHECK(zig.squaredDistance<Rational>(tri) == Rational(0));
}
