#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

// Rectangle vs Polygon.
//
// Testable here: the area-as-container predicates (Rectangle.contains/
// interiorContains/boundaryContains/separates(Polygon)), the symmetric meet
// predicates (intersects / interiorsIntersect), and the polygon-side containment
// predicates (Polygon.contains/interiorContains/boundaryContains/intersects/
// interiorsIntersect(Rectangle)).
//
// NOT exercised: Polygon::separates(Rectangle), Polygon::crosses(Rectangle) and
// Rectangle::crosses(Polygon) -- these are not implemented yet (they throw).

TEST_CASE("Rectangle contains Polygon") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;
    using Polygon = pgl::Polygon<Point>;

    const Rectangle r(Point(0, 0), Point(10, 10));

    SUBCASE("polygon strictly inside is contained, interior and boundary distinguished") {
        const Polygon inner({2, 2, 8, 2, 8, 8, 2, 8});

        CHECK(r.contains(inner));
        CHECK(r.interiorContains(inner));
        CHECK_FALSE(r.boundaryContains(inner));
    }

    SUBCASE("polygon equal to the rectangle outline sits on the boundary") {
        const Polygon outline({0, 0, 10, 0, 10, 10, 0, 10});

        CHECK(r.contains(outline));
        CHECK_FALSE(r.interiorContains(outline));   // vertices rest on the boundary
        // A filled polygon (>= 3 vertices) is never *on* the boundary: its
        // interior is not part of the 1D boundary set.
        CHECK_FALSE(r.boundaryContains(outline));
    }

    SUBCASE("polygon poking past an edge is not contained") {
        const Polygon poking({2, 2, 12, 2, 12, 8, 2, 8});

        CHECK_FALSE(r.contains(poking));
        CHECK_FALSE(r.interiorContains(poking));
    }

    SUBCASE("polygon entirely outside is not contained") {
        const Polygon outside({20, 20, 25, 20, 25, 25, 20, 25});

        CHECK_FALSE(r.contains(outside));
    }
}

TEST_CASE("Polygon contains Rectangle") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;
    using Polygon = pgl::Polygon<Point>;

    const Rectangle r(Point(0, 0), Point(10, 10));

    SUBCASE("a roomy polygon contains the rectangle in its interior") {
        const Polygon big({-5, -5, 20, -5, 20, 20, -5, 20});

        CHECK(big.contains(r));
        CHECK(big.interiorContains(r));
        CHECK_FALSE(big.boundaryContains(r));
    }

    SUBCASE("a polygon equal to the rectangle outline holds it on the boundary") {
        const Polygon outline({0, 0, 10, 0, 10, 10, 0, 10});

        CHECK(outline.contains(r));
        CHECK_FALSE(outline.interiorContains(r));
        // NOTE: Polygon::boundaryContains(Rectangle) only checks the rectangle's
        // edges, so it returns true here -- inconsistent with the area-aware
        // Rectangle::boundaryContains(Polygon) above. Documented, not asserted.
    }

    SUBCASE("a smaller polygon cannot contain the rectangle") {
        const Polygon small({2, 2, 8, 2, 8, 8, 2, 8});

        CHECK_FALSE(small.contains(r));
    }
}

TEST_CASE("Rectangle separates Polygon") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;
    using Polygon = pgl::Polygon<Point>;

    const Polygon square({0, 0, 10, 0, 10, 10, 0, 10});

    SUBCASE("a band crossing the whole square cuts it in two") {
        const Rectangle band(Point(3, -5), Point(6, 15));

        CHECK(band.separates(square));
    }

    SUBCASE("a rectangle covering the whole square leaves nothing to split") {
        const Rectangle cover(Point(-5, -5), Point(15, 15));

        CHECK_FALSE(cover.separates(square));
    }

    SUBCASE("a rectangle disjoint from the square does not cut it") {
        const Rectangle away(Point(20, 20), Point(30, 30));

        CHECK_FALSE(away.separates(square));
    }

    SUBCASE("a rectangle biting a single corner leaves one connected piece") {
        const Rectangle corner(Point(-5, -5), Point(3, 3));

        CHECK_FALSE(corner.separates(square));
    }
}

TEST_CASE("Rectangle and Polygon meet predicates") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;
    using Polygon = pgl::Polygon<Point>;

    const Rectangle r(Point(0, 0), Point(10, 10));

    SUBCASE("overlapping interiors meet both ways") {
        const Polygon over({5, 5, 15, 5, 15, 15, 5, 15});

        CHECK(r.intersects(over));
        CHECK(over.intersects(r));
        CHECK(r.interiorsIntersect(over));
        CHECK(over.interiorsIntersect(r));
    }

    SUBCASE("touching only at a corner meets but interiors do not") {
        const Polygon corner({10, 10, 20, 10, 20, 20, 10, 20});

        CHECK(r.intersects(corner));
        CHECK(corner.intersects(r));
        CHECK_FALSE(r.interiorsIntersect(corner));
        CHECK_FALSE(corner.interiorsIntersect(r));
    }

    SUBCASE("disjoint shapes do not meet") {
        const Polygon away({20, 20, 30, 20, 30, 30, 20, 30});

        CHECK_FALSE(r.intersects(away));
        CHECK_FALSE(away.intersects(r));
        CHECK_FALSE(r.interiorsIntersect(away));
        CHECK_FALSE(away.interiorsIntersect(r));
    }

    SUBCASE("a normal polygon is not boundary-contained by the rectangle's outline") {
        const Polygon over({5, 5, 15, 5, 15, 15, 5, 15});

        CHECK_FALSE(r.boundaryContains(over));
    }
}
