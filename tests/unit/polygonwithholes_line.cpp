#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <vector>

using Point = pgl::Point<int>;
using Segment = pgl::Segment<Point>;
using Line = pgl::Line<Point>;
using OrientedLine = pgl::OrientedLine<Point>;
using Ray = pgl::Ray<Point>;
using Halfplane = pgl::Halfplane<Point>;
using Polygon = pgl::Polygon<Point>;
using Region = pgl::PolygonWithHoles<Point>;

// Lines, rays and half-planes are unbounded, which settles the containment
// relations before any geometry happens, and it settles `intersects` almost as
// cheaply: an unbounded connected shape that reaches the outer polygon has to
// leave it again, so it meets the outer boundary — which belongs to the region.
//
// `interiorsIntersect` is the one that does work, and the one that does not
// follow from Polygon: a line can straddle the outer ring and still miss the
// region interior entirely, because a hole may hold the whole chord.
//
// The fixture below is the 10x10 square with a 4x4 hole in the middle:
//
//     (0,10)              (10,10)
//        +-------------------+
//        |                   |
//        |     +-------+     |      hole = [3,7] x [3,7]
//        |     |       |     |
//        |     +-------+     |
//        |                   |
//        +-------------------+
//     (0,0)               (10,0)

static Region annulus() {
    const Polygon outer({0, 0, 10, 0, 10, 10, 0, 10});
    const Polygon hole({3, 3, 7, 3, 7, 7, 3, 7});
    return Region(outer, std::vector{hole});
}

TEST_CASE("PolygonWithHoles vs Line: the region never contains a line") {
    const Region region = annulus();
    REQUIRE(region.isValid());

    SUBCASE("a line through the material is met but not contained") {
        const Line l({0, 1}, {10, 1});
        CHECK(!region.contains(l));
        CHECK(!region.interiorContains(l));
        CHECK(!region.boundaryContains(l));
        CHECK(region.intersects(l));
        CHECK(region.interiorsIntersect(l));
    }

    SUBCASE("a degenerate line is a point and follows the point rules") {
        const Line inMaterial({1, 1}, {1, 1});
        REQUIRE(inMaterial.isDegenerate());
        CHECK(region.contains(inMaterial));
        CHECK(region.interiorContains(inMaterial));
        CHECK(!region.boundaryContains(inMaterial));
        CHECK(region.intersects(inMaterial));

        const Line onHoleRing({3, 5}, {3, 5});
        CHECK(region.contains(onHoleRing));
        CHECK(!region.interiorContains(onHoleRing));
        CHECK(region.boundaryContains(onHoleRing));

        const Line inHole({5, 5}, {5, 5});
        CHECK(!region.contains(inHole));
        CHECK(!region.intersects(inHole));
    }
}

TEST_CASE("PolygonWithHoles vs Line: meeting the region") {
    const Region region = annulus();

    SUBCASE("a line across the hole still meets the material twice") {
        const Line l({0, 5}, {10, 5});
        CHECK(region.intersects(l));
        CHECK(region.interiorsIntersect(l));
    }

    SUBCASE("a line supporting an outer edge touches the boundary only") {
        const Line l({0, 0}, {0, 10});
        CHECK(region.intersects(l));
        CHECK(!region.interiorsIntersect(l));
    }

    SUBCASE("a line supporting a hole edge runs along the boundary") {
        const Line l({3, 3}, {3, 7});
        CHECK(region.intersects(l));
        // Beyond the hole the same line cuts through the material.
        CHECK(region.interiorsIntersect(l));
    }

    SUBCASE("a line missing the outer polygon misses the region") {
        const Line l({20, 0}, {20, 1});
        CHECK(!region.intersects(l));
        CHECK(!region.interiorsIntersect(l));
    }

    SUBCASE("a diagonal through two hole corners never crosses an edge") {
        // The line meets the boundary only at the ring vertices (0,0), (3,3),
        // (7,7) and (10,10), so no crossing settles it and every answer comes
        // from the midpoints of the pieces between them.
        const Line l({0, 0}, {10, 10});
        CHECK(region.intersects(l));
        CHECK(region.interiorsIntersect(l));
    }

    SUBCASE("the reverse direction routes to the region") {
        const Line l({0, 5}, {10, 5});
        CHECK(l.intersects(region));
        CHECK(l.interiorsIntersect(region));
    }
}

TEST_CASE("PolygonWithHoles vs Line: a hole holding the whole chord") {
    // The hole touches the outer ring at (0,5) and at (10,5), so the region is
    // two pieces joined nowhere, and the line y = 5 lies in the closed hole for
    // its whole passage across the square: it meets the region at the two touch
    // points and never reaches the interior. The outer polygon on its own says
    // the opposite, which is exactly why this cannot forward to Polygon.
    const Polygon outer({0, 0, 10, 0, 10, 10, 0, 10});
    const Polygon hole({0, 5, 5, 4, 10, 5, 5, 6});
    const Region region(outer, std::vector{hole});
    REQUIRE(region.isValid());

    SUBCASE("the swallowed chord") {
        const Line l({0, 5}, {10, 5});
        CHECK(region.intersects(l));
        CHECK(!region.interiorsIntersect(l));
        CHECK(outer.interiorsIntersect(l));  // the difference the holes make
        CHECK(region.contains(Point(0, 5)));
        CHECK(region.boundaryContains(Point(0, 5)));
    }

    SUBCASE("a line just above the chord cuts the upper piece") {
        const Line l({0, 6}, {10, 6});
        CHECK(region.intersects(l));
        CHECK(region.interiorsIntersect(l));
    }

    SUBCASE("a line just below the chord cuts the lower piece") {
        const Line l({0, 4}, {10, 4});
        CHECK(region.interiorsIntersect(l));
    }

    SUBCASE("a vertical line crosses both pieces") {
        const Line l({2, 0}, {2, 10});
        CHECK(region.interiorsIntersect(l));
    }
}

TEST_CASE("PolygonWithHoles vs OrientedLine") {
    const Region region = annulus();
    const OrientedLine l({10, 5}, {0, 5});

    CHECK(!region.contains(l));
    CHECK(!region.interiorContains(l));
    CHECK(!region.boundaryContains(l));
    CHECK(region.intersects(l));
    CHECK(region.interiorsIntersect(l));

    const OrientedLine away({20, 0}, {20, 1});
    CHECK(!region.intersects(away));
    CHECK(!region.interiorsIntersect(away));

    const OrientedLine degenerate({5, 5}, {5, 5});
    REQUIRE(degenerate.isDegenerate());
    CHECK(!region.contains(degenerate));  // the point is inside the hole
    CHECK(region.contains(OrientedLine({1, 1}, {1, 1})));
}

TEST_CASE("PolygonWithHoles vs Ray") {
    const Region region = annulus();

    SUBCASE("a ray out of the hole reaches the material") {
        const Ray r({5, 5}, {5, 6});
        CHECK(!region.contains(r));
        CHECK(!region.boundaryContains(r));
        CHECK(region.intersects(r));
        CHECK(region.interiorsIntersect(r));
    }

    SUBCASE("a ray leaving the region at its source only touches it") {
        const Ray r({0, 5}, {-1, 5});
        CHECK(region.intersects(r));
        CHECK(!region.interiorsIntersect(r));
    }

    SUBCASE("the same source pointing inward reaches the interior") {
        const Ray r({0, 5}, {1, 5});
        CHECK(region.intersects(r));
        CHECK(region.interiorsIntersect(r));
    }

    SUBCASE("a ray pointing away from the region misses it") {
        const Ray r({20, 20}, {30, 30});
        CHECK(!region.intersects(r));
        CHECK(!region.interiorsIntersect(r));
    }

    SUBCASE("a degenerate ray is its source") {
        const Ray inHole({5, 5}, {5, 5});
        REQUIRE(inHole.isDegenerate());
        CHECK(!region.contains(inHole));
        CHECK(!region.intersects(inHole));

        const Ray inMaterial({9, 9}, {9, 9});
        CHECK(region.contains(inMaterial));
        CHECK(region.interiorContains(inMaterial));
        CHECK(region.intersects(inMaterial));
    }

    SUBCASE("a ray from the material leaving through ring vertices only") {
        // Source in the material, then the hole corners (3,3) and (7,7) and the
        // outer corner (10,10): no edge is ever crossed transversally.
        const Ray r({1, 1}, {2, 2});
        CHECK(region.intersects(r));
        CHECK(region.interiorsIntersect(r));
        CHECK(!region.contains(r));
    }

    SUBCASE("a ray along the boundary of a swallowed chord") {
        const Polygon outer({0, 0, 10, 0, 10, 10, 0, 10});
        const Polygon hole({0, 5, 5, 4, 10, 5, 5, 6});
        const Region pinched(outer, std::vector{hole});
        const Ray r({0, 5}, {10, 5});
        CHECK(pinched.intersects(r));
        CHECK(!pinched.interiorsIntersect(r));
    }
}

TEST_CASE("PolygonWithHoles vs Halfplane") {
    const Region region = annulus();

    SUBCASE("a half-plane covering the region") {
        const Halfplane h({12, 0}, {12, 1});  // x <= 12
        CHECK(!region.contains(h));
        CHECK(!region.interiorContains(h));
        CHECK(!region.boundaryContains(h));
        CHECK(region.intersects(h));
        CHECK(region.interiorsIntersect(h));
    }

    SUBCASE("a half-plane touching the region along one outer edge") {
        const Halfplane h({0, 0}, {0, 1});  // x <= 0
        CHECK(region.intersects(h));
        CHECK(!region.interiorsIntersect(h));
    }

    SUBCASE("a half-plane cutting the region in two") {
        const Halfplane h({5, 1}, {5, 0});  // x >= 5
        CHECK(region.intersects(h));
        CHECK(region.interiorsIntersect(h));
    }

    SUBCASE("a half-plane clear of the region") {
        const Halfplane h({-1, 0}, {-1, 1});  // x <= -1
        CHECK(!region.intersects(h));
        CHECK(!region.interiorsIntersect(h));
    }

    SUBCASE("without holes the region answers exactly like its outer polygon") {
        const Polygon outer({0, 0, 10, 0, 10, 10, 0, 10});
        const Region solid(outer);
        for (const Halfplane& h : {Halfplane({0, 0}, {0, 1}), Halfplane({5, 1}, {5, 0}),
                                   Halfplane({-1, 0}, {-1, 1}), Halfplane({10, 0}, {10, 1})}) {
            CHECK(solid.intersects(h) == outer.intersects(h));
            CHECK(solid.interiorsIntersect(h) == outer.interiorsIntersect(h));
        }
    }

    SUBCASE("a degenerate half-plane is its source") {
        const Halfplane h({5, 5}, {5, 5});
        REQUIRE(h.isDegenerate());
        CHECK(!region.contains(h));
        CHECK(!region.intersects(h));
        CHECK(region.contains(Halfplane({1, 1}, {1, 1})));
    }
}

TEST_CASE("PolygonWithHoles vs Halfplane: a slit tip carries no interior") {
    // The hole shares two whole edges with the outer square, so the region is an
    // L shape with two slits, and the corner (0,0) is the tip where they meet:
    // every neighbourhood of it holds region points, but no region interior.
    const Polygon outer({0, 0, 8, 0, 8, 8, 0, 8});
    const Polygon hole({0, 0, 4, 0, 4, 4, 0, 4});
    const Region region(outer, std::vector{hole});
    REQUIRE(region.isValid());
    REQUIRE(region.contains(Point(0, 0)));
    REQUIRE(region.boundaryContains(Point(0, 0)));

    SUBCASE("a half-plane holding only the slit tip") {
        const Halfplane h({1, 0}, {0, 1});  // x + y <= 1
        CHECK(region.intersects(h));
        CHECK(!region.interiorsIntersect(h));
        // Without the hole the same half-plane would reach the interior.
        CHECK(outer.interiorsIntersect(h));
    }

    SUBCASE("a half-plane reaching past the slit") {
        const Halfplane h({5, 0}, {0, 5});  // x + y <= 5
        CHECK(region.interiorsIntersect(h));
    }

    SUBCASE("a half-plane on the other side of the tip") {
        const Halfplane h({0, 1}, {1, 0});  // x + y >= 1
        CHECK(region.interiorsIntersect(h));
    }

    SUBCASE("the free corner of the hole is an ordinary vertex") {
        // (4,4) has material around it, so a half-plane holding only it counts.
        const Halfplane h({5, 4}, {4, 5});  // x + y <= 9
        CHECK(region.interiorsIntersect(h));
    }
}

TEST_CASE("PolygonWithHoles vs Line: distances") {
    const Region region = annulus();

    SUBCASE("a line crossing the region is at distance zero") {
        const Line l({0, 5}, {10, 5});
        CHECK(region.squaredDistance<double>(l) == doctest::Approx(0.0));
        CHECK(region.distanceL1<double>(l) == doctest::Approx(0.0));
        CHECK(region.distanceLInf<double>(l) == doctest::Approx(0.0));
    }

    SUBCASE("a line clear of the region") {
        const Line l({20, 0}, {20, 1});
        CHECK(region.squaredDistance<double>(l) == doctest::Approx(100.0));
        CHECK(region.distanceL1<double>(l) == doctest::Approx(10.0));
        CHECK(region.distanceLInf<double>(l) == doctest::Approx(10.0));
    }

    SUBCASE("a diagonal line clear of the corner") {
        // x + y = 25 is at distance 5/sqrt(2) from the corner (10,10).
        const Line l({25, 0}, {0, 25});
        CHECK(region.squaredDistance<double>(l) == doctest::Approx(12.5));
    }

    SUBCASE("an oriented line clear of the region") {
        const OrientedLine l({20, 1}, {20, 0});
        CHECK(region.squaredDistance<double>(l) == doctest::Approx(100.0));
        CHECK(region.distanceLInf<double>(l) == doctest::Approx(10.0));
    }

    SUBCASE("a ray clear of the region") {
        const Ray r({20, 20}, {30, 30});
        CHECK(region.squaredDistance<double>(r) == doctest::Approx(200.0));
        CHECK(region.distanceL1<double>(r) == doctest::Approx(20.0));
        CHECK(region.distanceLInf<double>(r) == doctest::Approx(10.0));
    }

    SUBCASE("a ray pointing at the region has distance zero") {
        const Ray r({20, 5}, {19, 5});
        CHECK(region.squaredDistance<double>(r) == doctest::Approx(0.0));
    }

    SUBCASE("a half-plane clear of the region") {
        const Halfplane h({15, 1}, {15, 0});  // x >= 15
        CHECK(region.squaredDistance<double>(h) == doctest::Approx(25.0));
        CHECK(region.distanceL1<double>(h) == doctest::Approx(5.0));
        CHECK(region.distanceLInf<double>(h) == doctest::Approx(5.0));
    }

    SUBCASE("a half-plane overlapping the region has distance zero") {
        const Halfplane h({5, 1}, {5, 0});  // x >= 5
        CHECK(region.squaredDistance<double>(h) == doctest::Approx(0.0));
    }

    SUBCASE("the hole never shortens a distance") {
        // The nearest boundary is the outer ring, not the hole ring.
        const Segment nearest(Point(10, 5), Point(20, 5));
        CHECK(region.squaredDistance<double>(nearest.max()) == doctest::Approx(100.0));
    }
}

TEST_CASE("PolygonWithHoles vs Line: exact rational coordinates") {
    using ERational = pgl::Rational<pgl::BigInt>;
    using EPoint = pgl::Point<ERational>;
    using EPolygon = pgl::Polygon<EPoint>;
    using ERegion = pgl::PolygonWithHoles<EPoint>;
    using ELine = pgl::Line<EPoint>;
    using ERay = pgl::Ray<EPoint>;
    using EHalfplane = pgl::Halfplane<EPoint>;

    const EPolygon outer({EPoint(0, 0), EPoint(4, 0), EPoint(4, 4), EPoint(0, 4)});
    const EPolygon hole({EPoint(1, 1), EPoint(3, 1), EPoint(3, 3), EPoint(1, 3)});
    const ERegion region(outer, std::vector{hole});
    REQUIRE(region.isValid());

    const ELine half(EPoint(ERational(1, 2), ERational(0)), EPoint(ERational(1, 2), ERational(1)));
    CHECK(region.intersects(half));
    CHECK(region.interiorsIntersect(half));
    CHECK(!region.contains(half));

    const ELine through(EPoint(0, 2), EPoint(4, 2));
    CHECK(region.interiorsIntersect(through));

    const ERay ray(EPoint(2, 2), EPoint(2, 3));
    CHECK(region.intersects(ray));
    CHECK(region.interiorsIntersect(ray));

    const EHalfplane halfplane(EPoint(6, 1), EPoint(6, 0));  // x >= 6
    CHECK(!region.intersects(halfplane));
    CHECK(region.squaredDistance<ERational>(halfplane) == ERational(4));
}
