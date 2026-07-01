#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

// Polygon <-> Disk cut predicates. Circle-boundary crossings are irrational, so
// (like every Disk cut predicate in the library) these count boundary crossings
// / arcs rather than constructing intersection points. That count is exact for a
// convex polygon; the tests pin the convex behaviour against the trusted
// Convex/Disk oracle and spell out representative hand cases.

using Point = pgl::Point<int>;
using Polygon = pgl::Polygon<Point>;
using Convex = pgl::Convex<Point>;
using Disk = pgl::Disk<Point>;

TEST_CASE("Polygon separates Disk: a band cut across the disk") {
    const Disk d(Point(0, 0), 5);                       // r^2 = 25
    const Polygon band({-2, -8, 2, -8, 2, 8, -2, 8});   // vertical bar through the disk
    CHECK(band.separates(d));                            // disk split left / right
    CHECK(d.separates(band));                            // disk cuts the bar's middle out
    CHECK(band.crosses(d));
}

TEST_CASE("Polygon separates Disk: disk inside the polygon does not") {
    const Disk d(Point(0, 0), 3);
    const Polygon square({-10, -10, 10, -10, 10, 10, -10, 10});
    CHECK_FALSE(square.separates(d));   // removing the frame leaves an annulus, still connected
    CHECK_FALSE(square.crosses(d));
}

TEST_CASE("Polygon separates Disk: polygon inside the disk does not") {
    const Disk d(Point(0, 0), 10);
    const Polygon tri({-3, -3, 3, -3, 0, 3});
    CHECK_FALSE(tri.separates(d));      // tiny polygon cannot span the disk
    CHECK_FALSE(d.separates(tri));      // disk covers the whole polygon
    CHECK_FALSE(tri.crosses(d));
}

TEST_CASE("Polygon separates Disk: disjoint shapes do not") {
    const Disk d(Point(0, 0), 3);
    const Polygon away({20, 20, 24, 20, 24, 24, 20, 24});
    CHECK_FALSE(away.separates(d));
    CHECK_FALSE(d.separates(away));
    CHECK_FALSE(away.crosses(d));
}

TEST_CASE("Polygon/Disk cut predicates reject non-convex polygons") {
    const Disk d(Point(0, 0), 5);
    // A reflex "staple" whose two legs poke into the disk while joined outside it:
    // the boundary-crossing count cannot resolve this exactly, so it must throw
    // rather than risk a wrong answer.
    const Polygon staple({-3, 0, -3, 8, 3, 8, 3, 0, 1, 0, 1, 6, -1, 6, -1, 0});
    REQUIRE(staple.isSimple());
    REQUIRE_FALSE(staple.isConvex());

    CHECK_THROWS((void)staple.separates(d));
    CHECK_THROWS((void)d.separates(staple));
    CHECK_THROWS((void)staple.crosses(d));

    // A convex polygon of the same broad shape is fine.
    const Polygon convexBar({-2, -8, 2, -8, 2, 8, -2, 8});
    REQUIRE(convexBar.isConvex());
    CHECK_NOTHROW((void)convexBar.separates(d));
    CHECK_NOTHROW((void)d.separates(convexBar));
    CHECK_NOTHROW((void)convexBar.crosses(d));
}

// For a convex polygon the count is exact, so the Polygon path must match the
// Convex path in both directions, over a sweep of disks and convex polygons.
TEST_CASE("Polygon/Disk cut predicates match the Convex oracle for convex polygons") {
    const Convex shapes[] = {
        Convex({-2, -9, 2, -9, 2, 9, -2, 9}),      // tall thin bar
        Convex({-9, -2, 9, -2, 9, 2, -9, 2}),      // wide thin bar
        Convex({-4, -4, 4, -4, 4, 4, -4, 4}),      // square
        Convex({0, 0, 8, 0, 4, 8}),                // triangle
        Convex({-6, 0, 0, -6, 6, 0, 0, 6}),        // diamond
        Convex({3, 3, 12, 3, 12, 12, 3, 12}),      // offset square
    };
    long checks = 0;
    for (int cx = -6; cx <= 6; cx += 3)
        for (int cy = -6; cy <= 6; cy += 3)
            for (int r = 2; r <= 7; ++r) {
                const Disk d(Point(cx, cy), r);
                for (const Convex& c : shapes) {
                    const Polygon p(c.vertices());
                    REQUIRE(p.isSimple());
                    CHECK(p.separates(d) == c.separates(d));
                    CHECK(d.separates(p) == d.separates(c));
                    ++checks;
                }
            }
    CHECK(checks > 500);
}
