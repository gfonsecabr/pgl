#include "pgl.hpp"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

// Rectangle::separates(Disk) and Disk::crosses(Rectangle).
//
// A rectangle separates a disk iff it forms a "band" across the disk — both its
// horizontal (or vertical) chord edges cross the circle with no corner inside.
// For axis-aligned rectangles, the only possible separating case is k=0, c=2
// (both chord edges cut the disk).

TEST_CASE("Rectangle separates Disk: band through the disk") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;
    using Disk = pgl::Disk<Point>;

    const Disk d({0, 0}, 10);  // centre (0,0), radius 10

    SUBCASE("band across the disk: separated") {
        CHECK_MESSAGE(Rectangle(-50, -3, 50, 3).separates(d), "band separates disk");
    }
    SUBCASE("only covers a single cap: not separated") {
        CHECK_FALSE_MESSAGE(Rectangle(-50, 1, 50, 50).separates(d), "cap rect does not separate");
    }
    SUBCASE("rectangle engulfs the disk: not separated") {
        CHECK_FALSE_MESSAGE(Rectangle(-100, -100, 100, 100).separates(d), "engulfing rect");
    }
    SUBCASE("disjoint rectangle: not separated") {
        CHECK_FALSE_MESSAGE(Rectangle(50, 50, 60, 60).separates(d), "disjoint rect");
    }
    SUBCASE("two corners inside, no chord: not separated") {
        CHECK_FALSE_MESSAGE(Rectangle(-2, -50, 2, 5).separates(d), "corner-bite rect");
    }
    SUBCASE("rectangle strictly inside the disk: not separated") {
        CHECK_FALSE_MESSAGE(Rectangle(-3, -3, 3, 3).separates(d), "inner rect");
    }
}

TEST_CASE("Disk crosses Rectangle: true iff each pierces the other") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;
    using Disk = pgl::Disk<Point>;

    const Disk d({0, 0}, 10);

    SUBCASE("long thin band run through the disk: crosses") {
        CHECK_MESSAGE(d.crosses(Rectangle(-100, -1, 100, 1)), "disk crosses thin band");
        CHECK_MESSAGE(Rectangle(-100, -1, 100, 1).crosses(d), "thin band crosses disk");
    }
    SUBCASE("rectangle engulfs the disk: does not cross") {
        CHECK_FALSE_MESSAGE(d.crosses(Rectangle(-100, -100, 100, 100)), "engulfing does not cross");
    }
    SUBCASE("rectangle strictly inside the disk: does not cross") {
        CHECK_FALSE_MESSAGE(d.crosses(Rectangle(-3, -3, 3, 3)), "inner rect does not cross");
    }
    SUBCASE("disjoint rectangle: does not cross") {
        CHECK_FALSE_MESSAGE(d.crosses(Rectangle(50, 50, 60, 60)), "disjoint does not cross");
    }
}

TEST_CASE("Disk contains and interiorContains Rectangle") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;
    using Disk = pgl::Disk<Point>;

    const Disk d({0, 0}, 10);

    // A rectangle whose every corner is inside the disk.
    const Rectangle inner({-3, -3}, {3, 3});
    CHECK_MESSAGE(d.contains(inner), "disk contains inner rect");
    CHECK_MESSAGE(d.interiorContains(inner), "disk interiorContains inner rect");

    // A rectangle straddling the boundary.
    const Rectangle crossing({-5, -3}, {15, 3});
    CHECK_FALSE_MESSAGE(d.contains(crossing), "disk does not contain crossing rect");

    // A rectangle entirely outside.
    const Rectangle outside({15, 15}, {20, 20});
    CHECK_FALSE_MESSAGE(d.contains(outside), "disk does not contain outside rect");
}

TEST_CASE("Disk boundaryContains Rectangle: only degenerate cases (point)") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;
    using Disk = pgl::Disk<Point>;

    const Disk d({0, 0}, 5);

    // A non-degenerate rectangle cannot lie entirely on the circle.
    const Rectangle inner({-3, -3}, {3, 3});
    CHECK_FALSE_MESSAGE(d.boundaryContains(inner), "disk does not boundaryContain inner rect");
}

TEST_CASE("Disk and Rectangle intersects and interiorsIntersect, both directions") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;
    using Disk = pgl::Disk<Point>;

    // Both center+radius and 3-point (no shared x or y coordinate) forms.
    // (6,8), (-8,6), (10,0) all lie on the circle centered at (0,0) with radius 10.
    for (const Disk d : {Disk(Point(0, 0), 10),
                         Disk(Point(6, 8), Point(-8, 6), Point(10, 0))}) {
        SUBCASE("rectangle inside disk: both intersect and interiors intersect") {
            const Rectangle r({-3, -3}, {3, 3});
            CHECK_MESSAGE(d.intersects(r), "disk intersects inner rect");
            CHECK_MESSAGE(r.intersects(d), "inner rect intersects disk");
            CHECK_MESSAGE(d.interiorsIntersect(r), "disk interiorsIntersect inner rect");
            CHECK_MESSAGE(r.interiorsIntersect(d), "inner rect interiorsIntersect disk");
        }

        SUBCASE("rectangle crossing disk boundary: intersects and interiors intersect") {
            const Rectangle r({-5, -3}, {15, 3});
            CHECK_MESSAGE(d.intersects(r), "disk intersects straddling rect");
            CHECK_MESSAGE(r.intersects(d), "straddling rect intersects disk");
            CHECK_MESSAGE(d.interiorsIntersect(r), "disk interiorsIntersect straddling rect");
            CHECK_MESSAGE(r.interiorsIntersect(d), "straddling rect interiorsIntersect disk");
        }

        SUBCASE("disjoint: neither intersects") {
            const Rectangle r({15, 15}, {20, 20});
            CHECK_FALSE_MESSAGE(d.intersects(r), "disk does not intersect disjoint rect");
            CHECK_FALSE_MESSAGE(r.intersects(d), "disjoint rect does not intersect disk");
            CHECK_FALSE_MESSAGE(d.interiorsIntersect(r), "disk does not interiorsIntersect disjoint rect");
            CHECK_FALSE_MESSAGE(r.interiorsIntersect(d), "disjoint rect does not interiorsIntersect disk");
        }
    }
}
