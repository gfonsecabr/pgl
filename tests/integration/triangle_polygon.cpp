#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

// Triangle vs Polygon.
//
// Testable here: the area-as-container predicates (Triangle.contains/
// interiorContains/boundaryContains/separates(Polygon)), the symmetric meet
// predicates, and the polygon-side containment predicates
// (Polygon.contains/interiorContains/intersects/interiorsIntersect(Triangle)).
//
// NOT exercised: Polygon::separates(Triangle), Polygon::crosses(Triangle) and
// Triangle::crosses(Polygon) -- not implemented yet (they throw).

TEST_CASE("Triangle contains Polygon") {
    using Point = pgl::Point<int>;
    using Triangle = pgl::Triangle<Point>;
    using Polygon = pgl::Polygon<Point>;

    // Right triangle with legs on the axes, hypotenuse x + y = 10.
    const Triangle t(Point(0, 0), Point(10, 0), Point(0, 10));

    SUBCASE("polygon strictly inside is contained") {
        const Polygon inner({1, 1, 4, 1, 1, 4});

        CHECK(t.contains(inner));
        CHECK(t.interiorContains(inner));
        CHECK_FALSE(t.boundaryContains(inner));
    }

    SUBCASE("polygon equal to the triangle outline rests on the boundary") {
        const Polygon outline({0, 0, 10, 0, 0, 10});

        CHECK(t.contains(outline));
        CHECK_FALSE(t.interiorContains(outline));
        // A filled polygon is never wholly on the 1D boundary.
        CHECK_FALSE(t.boundaryContains(outline));
    }

    SUBCASE("polygon poking past the hypotenuse is not contained") {
        const Polygon poking({1, 1, 9, 1, 9, 9});  // (9,9): x + y = 18 > 10

        CHECK_FALSE(t.contains(poking));
        CHECK_FALSE(t.interiorContains(poking));
    }

    SUBCASE("polygon entirely outside is not contained") {
        const Polygon outside({20, 20, 25, 20, 20, 25});

        CHECK_FALSE(t.contains(outside));
    }
}

TEST_CASE("Polygon contains Triangle") {
    using Point = pgl::Point<int>;
    using Triangle = pgl::Triangle<Point>;
    using Polygon = pgl::Polygon<Point>;

    const Triangle t(Point(0, 0), Point(10, 0), Point(0, 10));

    SUBCASE("a roomy polygon contains the triangle in its interior") {
        const Polygon big({-1, -1, 11, -1, 11, 11, -1, 11});

        CHECK(big.contains(t));
        CHECK(big.interiorContains(t));
        CHECK_FALSE(big.boundaryContains(t));
    }

    SUBCASE("a polygon equal to the triangle outline holds it, not in the interior") {
        const Polygon outline({0, 0, 10, 0, 0, 10});

        CHECK(outline.contains(t));
        CHECK_FALSE(outline.interiorContains(t));
    }

    SUBCASE("a smaller polygon cannot contain the triangle") {
        const Polygon small({1, 1, 4, 1, 1, 4});

        CHECK_FALSE(small.contains(t));
    }
}

TEST_CASE("Triangle separates Polygon") {
    using Point = pgl::Point<int>;
    using Triangle = pgl::Triangle<Point>;
    using Polygon = pgl::Polygon<Point>;

    const Polygon square({0, 0, 10, 0, 10, 10, 0, 10});

    SUBCASE("a tall wedge spanning the full height cuts the square in two") {
        // Apex at the bottom, base above the square: within the square it is a
        // vertical band reaching both the top and bottom edges.
        const Triangle wedge(Point(5, -5), Point(3, 15), Point(7, 15));

        CHECK(wedge.separates(square));
    }

    SUBCASE("a triangle covering the whole square leaves nothing to split") {
        const Triangle cover(Point(-10, -10), Point(40, -10), Point(-10, 40));

        CHECK_FALSE(cover.separates(square));
    }

    SUBCASE("a triangle disjoint from the square does not cut it") {
        const Triangle away(Point(20, 20), Point(25, 20), Point(20, 25));

        CHECK_FALSE(away.separates(square));
    }

    SUBCASE("a triangle biting a single corner leaves one connected piece") {
        const Triangle corner(Point(-5, -5), Point(3, -5), Point(-5, 3));

        CHECK_FALSE(corner.separates(square));
    }
}

TEST_CASE("Triangle and Polygon meet predicates") {
    using Point = pgl::Point<int>;
    using Triangle = pgl::Triangle<Point>;
    using Polygon = pgl::Polygon<Point>;

    const Triangle t(Point(0, 0), Point(10, 0), Point(0, 10));

    SUBCASE("overlapping interiors meet both ways") {
        const Polygon over({2, 2, 8, 2, 2, 8});

        CHECK(t.intersects(over));
        CHECK(over.intersects(t));
        CHECK(t.interiorsIntersect(over));
        CHECK(over.interiorsIntersect(t));
    }

    SUBCASE("touching only at a vertex meets but interiors do not") {
        const Polygon corner({10, 0, 20, 0, 20, 10, 10, 10});

        CHECK(t.intersects(corner));
        CHECK(corner.intersects(t));
        CHECK_FALSE(t.interiorsIntersect(corner));
        CHECK_FALSE(corner.interiorsIntersect(t));
    }

    SUBCASE("disjoint shapes do not meet") {
        const Polygon away({20, 20, 30, 20, 30, 30, 20, 30});

        CHECK_FALSE(t.intersects(away));
        CHECK_FALSE(away.intersects(t));
        CHECK_FALSE(t.interiorsIntersect(away));
        CHECK_FALSE(away.interiorsIntersect(t));
    }
}
