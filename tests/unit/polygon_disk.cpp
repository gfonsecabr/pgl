#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

// Polygon vs Disk.
//
// Tested below: the disk-as-container containment predicates
// Disk::contains / interiorContains / boundaryContains(Polygon); the
// polygon-as-container predicates Polygon::contains / interiorContains(Disk);
// the overlap predicates Polygon::intersects / interiorsIntersect(Disk); and
// Polygon::boundaryContains(Disk).
//
// Still NOT implemented in the library (the methods throw
// std::runtime_error "not implemented yet") and therefore not exercised here:
// Disk::separates(Polygon) and Polygon::separates / crosses(Disk).

TEST_CASE("Disk contains Polygon") {
    using Point = pgl::Point<int>;
    using Polygon = pgl::Polygon<Point>;
    using Disk = pgl::Disk<Point>;

    // Disk centred at the origin with radius 10 (r^2 = 100), exact for integers.
    const Disk d(Point(0, 0), 10);

    SUBCASE("polygon well inside the disk is contained, interior too") {
        // Corner (5,5): 5^2 + 5^2 = 50 < 100.
        const Polygon inner({-5, -5, 5, -5, 5, 5, -5, 5});

        CHECK(d.contains(inner));
        CHECK(d.interiorContains(inner));
        CHECK_FALSE(d.boundaryContains(inner));
    }

    SUBCASE("polygon with a vertex on the circle is contained but not interior") {
        // (6,8): 6^2 + 8^2 = 100 lies exactly on the circle; the others are inside.
        const Polygon onCircle({0, 0, 6, 8, 0, 9});

        CHECK(d.contains(onCircle));
        CHECK_FALSE(d.interiorContains(onCircle));
    }

    SUBCASE("polygon poking outside the circle is not contained") {
        // (9,9): 9^2 + 9^2 = 162 > 100.
        const Polygon poking({0, 0, 9, 9, 0, 9});

        CHECK_FALSE(d.contains(poking));
        CHECK_FALSE(d.interiorContains(poking));
    }
}

TEST_CASE("Polygon contains Disk") {
    using Point = pgl::Point<int>;
    using Polygon = pgl::Polygon<Point>;
    using Disk = pgl::Disk<Point>;

    // Axis-aligned square [-20, 20]^2. Integer center+radius disks keep r^2 exact.
    const Polygon square({-20, -20, 20, -20, 20, 20, -20, 20});

    SUBCASE("disk strictly inside is contained, interior too") {
        const Disk inside(Point(0, 0), 10);

        CHECK(square.contains(inside));
        CHECK(square.interiorContains(inside));
    }

    SUBCASE("disk internally tangent to an edge is contained but not interior") {
        // Centre (0,10), r=10 touches the top edge y=20 at (0,20) from inside.
        const Disk tangent(Point(0, 10), 10);

        CHECK(square.contains(tangent));
        CHECK_FALSE(square.interiorContains(tangent));
    }

    SUBCASE("disk poking through an edge is not contained") {
        // Centre (15,0), r=10 reaches x=25, past the right edge x=20.
        const Disk poking(Point(15, 0), 10);

        CHECK_FALSE(square.contains(poking));
        CHECK_FALSE(square.interiorContains(poking));
    }

    SUBCASE("square inside the disk does not contain it") {
        const Disk big(Point(0, 0), 25);

        CHECK_FALSE(square.contains(big));
        CHECK_FALSE(square.interiorContains(big));
    }

    SUBCASE("disjoint disk is not contained") {
        const Disk distant(Point(35, 0), 10);

        CHECK_FALSE(square.contains(distant));
        CHECK_FALSE(square.interiorContains(distant));
    }

    SUBCASE("non-convex polygon respects the reflex notch") {
        // L-shape: the notch is the quadrant x>4, y>4.
        const Polygon lshape({0, 0, 10, 0, 10, 4, 4, 4, 4, 10, 0, 10});

        // Disk sitting well inside the vertical arm.
        const Disk inArm(Point(2, 7), 1);
        CHECK(lshape.contains(inArm));
        CHECK(lshape.interiorContains(inArm));

        // Disk centred inside the arm but large enough to cover the reflex
        // vertex (4,4) and bulge into the notch -- both endpoints inside does
        // not suffice for a non-convex polygon.
        const Disk overReflex(Point(3, 3), 2);
        CHECK_FALSE(lshape.contains(overReflex));

        // Disk entirely in the notch (outside the L).
        const Disk inNotch(Point(8, 8), 1);
        CHECK_FALSE(lshape.contains(inNotch));
    }
}

TEST_CASE("Polygon intersects and interiorsIntersect Disk") {
    using Point = pgl::Point<int>;
    using Polygon = pgl::Polygon<Point>;
    using Disk = pgl::Disk<Point>;

    const Polygon square({-20, -20, 20, -20, 20, 20, -20, 20});

    SUBCASE("disk inside the polygon overlaps both closed and open") {
        const Disk inside(Point(0, 0), 10);

        CHECK(square.intersects(inside));
        CHECK(square.interiorsIntersect(inside));
    }

    SUBCASE("polygon inside the disk overlaps both closed and open") {
        const Disk big(Point(0, 0), 25);

        CHECK(square.intersects(big));
        CHECK(square.interiorsIntersect(big));
    }

    SUBCASE("boundary-crossing disk overlaps both closed and open") {
        const Disk crossing(Point(15, 0), 10);

        CHECK(square.intersects(crossing));
        CHECK(square.interiorsIntersect(crossing));
    }

    SUBCASE("externally tangent disk touches the boundary only") {
        // Centre (30,0), r=10 touches the right edge x=20 at (20,0).
        const Disk tangent(Point(30, 0), 10);

        CHECK(square.intersects(tangent));            // closed: they touch
        CHECK_FALSE(square.interiorsIntersect(tangent));  // open: no overlap
    }

    SUBCASE("disjoint disk overlaps neither") {
        const Disk distant(Point(35, 0), 10);

        CHECK_FALSE(square.intersects(distant));
        CHECK_FALSE(square.interiorsIntersect(distant));
    }

    SUBCASE("non-convex polygon respects the reflex notch") {
        const Polygon lshape({0, 0, 10, 0, 10, 4, 4, 4, 4, 10, 0, 10});

        // Disk in the notch, clear of every edge.
        const Disk inNotch(Point(8, 8), 1);
        CHECK_FALSE(lshape.intersects(inNotch));
        CHECK_FALSE(lshape.interiorsIntersect(inNotch));

        // Disk straddling the reflex corner cuts into the arms.
        const Disk overReflex(Point(3, 3), 2);
        CHECK(lshape.intersects(overReflex));
        CHECK(lshape.interiorsIntersect(overReflex));
    }
}

TEST_CASE("Polygon boundaryContains Disk") {
    using Point = pgl::Point<int>;
    using Polygon = pgl::Polygon<Point>;
    using Disk = pgl::Disk<Point>;

    const Polygon square({-20, -20, 20, -20, 20, 20, -20, 20});
    const Disk d(Point(0, 0), 10);

    // A proper (non-degenerate) disk is a 2D area; its circle boundary never lies
    // on the polygon's 1D boundary, so this is always false.
    CHECK_FALSE(square.boundaryContains(d));
}
