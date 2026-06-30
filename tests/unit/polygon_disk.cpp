#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

// Polygon vs Disk.
//
// Most of this pair is NOT implemented yet in the library (the methods throw
// std::runtime_error "not implemented yet"): Disk::separates(Polygon), and every
// Polygon-side area predicate against a Disk -- Polygon::intersects / crosses /
// interiorsIntersect / interiorContains / separates(Disk). They are therefore not
// exercised here.
//
// Genuinely implemented and tested below: the disk-as-container containment
// predicates Disk::contains / interiorContains / boundaryContains(Polygon), and
// Polygon::boundaryContains(Disk).

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
