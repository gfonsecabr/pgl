#include "pgl.hpp"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

// Triangle::separates(Disk) and Disk::crosses(Triangle).
//
// `A.separates(B)` means removing A disconnects B into >= 2 components.
// For a triangle removed from a disk this is determined by:
//   k = vertices strictly inside the disk, c = edges that are full chords.
//
//   k==0 -> c>=2 separates ; k==1 -> c>=1 separates ; k>=2 -> never

TEST_CASE("Triangle separates Disk: truth table by vertex count and chord count") {
    using Point = pgl::Point<int>;
    using Triangle = pgl::Triangle<Point>;
    using Disk = pgl::Disk<Point>;

    const Disk d({0, 0}, 10);  // radius 10

    SUBCASE("k=0, c=0 -- disjoint: not separated") {
        CHECK_FALSE_MESSAGE(Triangle(50, 50, 60, 50, 50, 60).separates(d), "disjoint triangle");
    }
    SUBCASE("k=0, c=0 -- triangle engulfs disk: not separated") {
        CHECK_FALSE_MESSAGE(Triangle(-100, -100, 100, -100, 0, 100).separates(d), "engulfing triangle");
    }
    SUBCASE("k=0, c=1 -- one chord cuts a cap only: not separated") {
        CHECK_FALSE_MESSAGE(Triangle(-50, 0, 50, 0, 0, -50).separates(d), "single-chord triangle");
    }
    SUBCASE("k=0, c=2 -- wedge pierces straight through: separated") {
        CHECK_MESSAGE(Triangle(0, -30, -20, 40, 20, 40).separates(d), "wedge triangle");
    }
    SUBCASE("k=1, c=0 -- corner bite from inside vertex: not separated") {
        CHECK_FALSE_MESSAGE(Triangle(0, 0, 50, 50, 50, -50).separates(d), "corner-bite triangle");
    }
    SUBCASE("k=1, c=1 -- inside vertex plus opposite chord: separated") {
        CHECK_MESSAGE(Triangle(0, -5, -50, 2, 50, 2).separates(d), "inside+chord triangle");
    }
    SUBCASE("k=2 -- two vertices inside, single bulge: not separated") {
        CHECK_FALSE_MESSAGE(Triangle(0, 0, 5, 0, 50, 50).separates(d), "two-inside triangle");
    }
    SUBCASE("k=3 -- triangle strictly inside: not separated") {
        CHECK_FALSE_MESSAGE(Triangle(-3, -3, 3, -3, 0, 3).separates(d), "contained triangle");
    }
}

TEST_CASE("Triangle separates Disk: orientation/argument order does not matter") {
    using Point = pgl::Point<int>;
    using Triangle = pgl::Triangle<Point>;
    using Disk = pgl::Disk<Point>;

    const Disk d({0, 0}, 10);
    CHECK_MESSAGE(Triangle(20, 40, 0, -30, -20, 40).separates(d), "vertex order A");
    CHECK_MESSAGE(Triangle(-20, 40, 20, 40, 0, -30).separates(d), "vertex order B");
}

TEST_CASE("Disk crosses Triangle: true iff each pierces the other") {
    using Point = pgl::Point<int>;
    using Triangle = pgl::Triangle<Point>;
    using Disk = pgl::Disk<Point>;

    const Disk d({0, 0}, 10);

    SUBCASE("long thin sliver run through the disk: crosses") {
        const Triangle t(-100, 0, 100, 2, 100, -2);
        CHECK_MESSAGE(d.crosses(t), "disk crosses sliver triangle");
        CHECK_MESSAGE(t.crosses(d), "sliver triangle crosses disk");
    }
    SUBCASE("disk strictly inside the triangle: does not cross") {
        CHECK_FALSE_MESSAGE(d.crosses(Triangle(-100, -100, 100, -100, 0, 100)), "disk inside triangle");
    }
    SUBCASE("triangle strictly inside the disk: does not cross") {
        CHECK_FALSE_MESSAGE(d.crosses(Triangle(-3, -3, 3, -3, 0, 3)), "triangle inside disk");
    }
    SUBCASE("disjoint triangle: does not cross") {
        CHECK_FALSE_MESSAGE(d.crosses(Triangle(50, 50, 60, 50, 50, 60)), "disjoint triangle");
    }
}

TEST_CASE("Disk contains and interiorContains Triangle") {
    using Point = pgl::Point<int>;
    using Triangle = pgl::Triangle<Point>;
    using Disk = pgl::Disk<Point>;

    const Disk d({0, 0}, 10);

    // Triangle with all vertices strictly inside the disk.
    const Triangle inner({-3, -3}, {3, -3}, {0, 3});
    CHECK_MESSAGE(d.contains(inner), "disk contains inner triangle");
    CHECK_MESSAGE(d.interiorContains(inner), "disk interiorContains inner triangle");

    // Triangle straddling the boundary.
    const Triangle crossing({-3, -3}, {15, 0}, {-3, 3});
    CHECK_FALSE_MESSAGE(d.contains(crossing), "disk does not contain crossing triangle");

    // Triangle entirely outside.
    const Triangle outside({15, 15}, {20, 15}, {15, 20});
    CHECK_FALSE_MESSAGE(d.contains(outside), "disk does not contain outside triangle");
}

TEST_CASE("Disk and Triangle intersects and interiorsIntersect, both directions") {
    using Point = pgl::Point<int>;
    using Triangle = pgl::Triangle<Point>;
    using Disk = pgl::Disk<Point>;

    // Both center+radius and 3-point (no shared x or y coordinate) forms.
    // (6,8), (-8,6), (10,0) all lie on the circle centered at (0,0) with radius 10.
    for (const Disk d : {Disk(Point(0, 0), 10),
                         Disk(Point(6, 8), Point(-8, 6), Point(10, 0))}) {
        SUBCASE("triangle inside disk: both intersect, interiors intersect") {
            const Triangle inner({-3, -3}, {3, -3}, {0, 3});
            CHECK_MESSAGE(d.intersects(inner), "disk intersects inner triangle");
            CHECK_MESSAGE(inner.intersects(d), "inner triangle intersects disk");
            CHECK_MESSAGE(d.interiorsIntersect(inner), "disk interiorsIntersect inner triangle");
            CHECK_MESSAGE(inner.interiorsIntersect(d), "inner triangle interiorsIntersect disk");
        }

        SUBCASE("triangle crossing disk boundary: both intersect, interiors intersect") {
            const Triangle crossing({-3, -3}, {15, 0}, {-3, 3});
            CHECK_MESSAGE(d.intersects(crossing), "disk intersects crossing triangle");
            CHECK_MESSAGE(crossing.intersects(d), "crossing triangle intersects disk");
            CHECK_MESSAGE(d.interiorsIntersect(crossing), "disk interiorsIntersect crossing triangle");
            CHECK_MESSAGE(crossing.interiorsIntersect(d), "crossing triangle interiorsIntersect disk");
        }

        SUBCASE("disk inside triangle: both intersect, interiors intersect") {
            const Triangle engulfing({-100, -100}, {100, -100}, {0, 100});
            CHECK_MESSAGE(d.intersects(engulfing), "disk intersects engulfing triangle");
            CHECK_MESSAGE(engulfing.intersects(d), "engulfing triangle intersects disk");
            CHECK_MESSAGE(d.interiorsIntersect(engulfing), "disk interiorsIntersect engulfing triangle");
            CHECK_MESSAGE(engulfing.interiorsIntersect(d), "engulfing triangle interiorsIntersect disk");
        }

        SUBCASE("disjoint: neither intersects") {
            const Triangle outside({15, 15}, {20, 15}, {15, 20});
            CHECK_FALSE_MESSAGE(d.intersects(outside), "disk does not intersect outside triangle");
            CHECK_FALSE_MESSAGE(outside.intersects(d), "outside triangle does not intersect disk");
            CHECK_FALSE_MESSAGE(d.interiorsIntersect(outside), "disk does not interiorsIntersect outside triangle");
            CHECK_FALSE_MESSAGE(outside.interiorsIntersect(d), "outside triangle does not interiorsIntersect disk");
        }
    }
}
