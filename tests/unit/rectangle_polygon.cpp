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

    SUBCASE("a rectangle snug against the polygon's boundary still shares its interior") {
        // Every contact is degenerate — no vertex of either lies strictly inside the
        // other, and no pair of edges properly crosses — yet the rectangle sits in
        // the polygon and the two plainly share interior points.
        const Polygon around({0, 0, 10, 0, 10, 10, 0, 10});
        const Rectangle snug(Point(0, 0), Point(10, 5));  // shares three sides

        CHECK(around.contains(snug));
        CHECK(around.interiorsIntersect(snug));
        CHECK(snug.interiorsIntersect(around));

        // Coincident: same region, hence the same interior.
        const Rectangle same(Point(0, 0), Point(10, 10));
        CHECK(around.interiorsIntersect(same));
        CHECK(same.interiorsIntersect(around));
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

// Polygon::intersection(Rectangle) forwards to the Convex overload via
// Rectangle::asConvex. ERational keeps fractional crossings exact.
TEST_CASE("Polygon::intersection(Rectangle) forwards via the convex representation") {
    using EPoint = pgl::Point<pgl::ERational>;
    using EPolygon = pgl::Polygon<EPoint>;

    const pgl::Polygon<pgl::Point<int>> square({0, 0, 10, 0, 10, 10, 0, 10});

    SUBCASE("overlapping rectangle gives the clipped polygon") {
        const pgl::Rectangle<pgl::Point<int>> box(pgl::Point<int>(5, 5), pgl::Point<int>(15, 15));
        const auto pieces = square.intersection<pgl::ERational>(box);

        CHECK(pieces == square.intersection<pgl::ERational>(box.asConvex()));
        REQUIRE(pieces.size() == 1);
        REQUIRE(std::holds_alternative<EPolygon>(pieces[0]));
        CHECK(std::get<EPolygon>(pieces[0]).twiceArea() == pgl::ERational(50));  // the 5x5 overlap
    }

    SUBCASE("a non-convex polygon split by a thin rectangle yields two polygons") {
        // U-shape with prongs x in [0,3] and [7,10]; a high band misses the base.
        const pgl::Polygon<pgl::Point<int>> u(
            {0, 0, 10, 0, 10, 10, 7, 10, 7, 3, 3, 3, 3, 10, 0, 10});
        const pgl::Rectangle<pgl::Point<int>> band(pgl::Point<int>(-1, 5), pgl::Point<int>(11, 9));
        const auto pieces = u.intersection<pgl::ERational>(band);

        int polys = 0;
        for (const auto& v : pieces) polys += std::holds_alternative<EPolygon>(v) ? 1 : 0;
        CHECK(polys == 2);
    }

    SUBCASE("disjoint rectangle yields no intersection") {
        const pgl::Rectangle<pgl::Point<int>> away(pgl::Point<int>(20, 20), pgl::Point<int>(30, 30));
        CHECK(square.intersection<pgl::ERational>(away).empty());
    }
}
