#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

using Point = pgl::Point<int>;
using Disk = pgl::Disk<Point>;
// Aliased as `PLine` (not `Polyline`) because doctest pulls in <windows.h> on
// MSVC, whose GDI `Polyline` function collides with a file-scope `using
// Polyline`.
using PLine = pgl::Polyline<Point>;

// Disk centered at (0,0) with radius 5 (through three boundary points).
static const Disk disk(Point(-5, 0), Point(0, -5), Point(5, 0));

TEST_CASE("Polyline and Disk containment and intersection") {
    const PLine inside({-2, 0, 0, 2, 2, 0});
    const PLine crossing({-7, 0, 7, 0});
    const PLine outside({6, 6, 8, 8});

    CHECK(inside.intersects(disk));
    CHECK(disk.intersects(inside));
    CHECK(crossing.intersects(disk));
    CHECK_FALSE(outside.intersects(disk));

    CHECK(disk.contains(inside));
    CHECK(disk.interiorContains(inside));
    CHECK_FALSE(disk.contains(crossing));
    CHECK_FALSE(disk.boundaryContains(inside));
    // A polyline covering a single circle point is boundary-contained.
    CHECK(disk.boundaryContains(PLine({Point(5, 0)})));
    CHECK(disk.boundaryContains(PLine()));

    CHECK_FALSE(inside.contains(disk));
    CHECK_FALSE(inside.interiorContains(disk));
    CHECK_FALSE(inside.boundaryContains(disk));

    CHECK(inside.interiorsIntersect(disk));
    // Touching the circle from outside at one point: closed contact only.
    const PLine tangent({5, -3, 5, 3});
    CHECK(tangent.intersects(disk));
    CHECK_FALSE(tangent.interiorsIntersect(disk));
}

TEST_CASE("Disk separates a polyline") {
    // The disk removes the middle of a segment passing through it.
    CHECK(disk.separates(PLine({-7, 0, 7, 0})));
    // A swallowed polyline is not separated.
    CHECK_FALSE(disk.separates(PLine({-2, 0, 0, 2, 2, 0})));
    // Clipping one end only does not separate.
    CHECK_FALSE(disk.separates(PLine({0, 0, 7, 0})));

    SUBCASE("set semantics: a surrounding loop survives one bite") {
        // A big diamond loop around the disk, dipping inside near one vertex.
        const PLine loop({-8, 0, 0, -8, 8, 0, 0, 8, -8, 0});
        CHECK_FALSE(disk.separates(loop));
        // The disk of radius 5 does reach the diamond's four edges (distance
        // 4·sqrt(2) < 5), biting all of them: four bites cut the loop into
        // four arcs.
        const Disk big(Point(-6, 0), Point(0, -6), Point(6, 0));
        CHECK(big.separates(loop));
    }
}

TEST_CASE("Polyline separates a disk") {
    SUBCASE("a full chord cuts") {
        CHECK(PLine({-7, 0, 7, 0}).separates(disk));
        CHECK(PLine({-7, 0, 7, 0}).crosses(disk));
    }

    SUBCASE("a bent crosscut through the interior cuts") {
        CHECK(PLine({-7, 0, 0, 2, 7, 0}).separates(disk));
    }

    SUBCASE("slits do not cut") {
        CHECK_FALSE(PLine({-7, 0, 0, 0}).separates(disk));    // ends at the center
        CHECK_FALSE(PLine({-2, 0, 0, 2, 2, 0}).separates(disk)); // strictly inside
    }

    SUBCASE("a loop strictly inside seals a pocket") {
        const PLine loop({-2, 0, 0, -2, 2, 0, 0, 2, -2, 0});
        CHECK(loop.separates(disk));
        CHECK_FALSE(loop.crosses(disk));   // the disk swallows the loop
    }

    SUBCASE("a tangent touch does not cut") {
        CHECK_FALSE(PLine({5, -3, 5, 3}).separates(disk));
    }

    SUBCASE("a spike retracing itself into the disk does not cut") {
        // In and back out along the same segment: the doubled edge collapses
        // to one slit.
        const PLine spike({-7, 0, 0, 0, -7, 0});
        CHECK_FALSE(spike.separates(disk));
    }
}

TEST_CASE("Polyline and Disk distance") {
    const PLine outside({6, 8, 8, 8});   // nearest at (6,8), 10 - 5 from the center
    CHECK(outside.squaredDistance(disk) == doctest::Approx(25.0));
    CHECK(disk.squaredDistance(outside) == doctest::Approx(25.0));
    CHECK(PLine({-7, 0, 7, 0}).squaredDistance(disk) == doctest::Approx(0.0));
}
