#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <vector>

using Point = pgl::Point<int>;
using PolygonShape = pgl::Polygon<Point>;
using Region = pgl::PolygonWithHoles<Point>;

// The region A = outer \ (hole_0° ∪ hole_1° ∪ ...) is closed, so every hole
// boundary belongs to A. The three point predicates are direct rewritings of
// that identity, and these tests pin each of the five positions a point can
// occupy: outer interior, outer boundary, hole boundary, hole interior, and
// outside altogether.

TEST_CASE("PolygonWithHoles point location, single hole") {
    const PolygonShape outer({0, 0, 10, 0, 10, 10, 0, 10});
    const PolygonShape hole({2, 2, 6, 2, 6, 6, 2, 6});
    const Region region(outer, std::vector{hole});
    REQUIRE(region.isValid());

    SUBCASE("strictly between the rings") {
        const Point p(8, 8);
        CHECK(region.contains(p));
        CHECK(region.interiorContains(p));
        CHECK(!region.boundaryContains(p));
        CHECK(region.intersects(p));
    }

    SUBCASE("on the outer boundary") {
        const Point p(0, 5);
        CHECK(region.contains(p));
        CHECK(!region.interiorContains(p));
        CHECK(region.boundaryContains(p));
        CHECK(region.intersects(p));
    }

    SUBCASE("on an outer corner") {
        const Point p(10, 10);
        CHECK(region.contains(p));
        CHECK(!region.interiorContains(p));
        CHECK(region.boundaryContains(p));
    }

    SUBCASE("on a hole edge is in the region, not in its interior") {
        const Point p(4, 2);
        CHECK(region.contains(p));
        CHECK(!region.interiorContains(p));
        CHECK(region.boundaryContains(p));
        CHECK(region.intersects(p));
    }

    SUBCASE("on a hole corner") {
        const Point p(2, 2);
        CHECK(region.contains(p));
        CHECK(!region.interiorContains(p));
        CHECK(region.boundaryContains(p));
    }

    SUBCASE("strictly inside the hole is carved out") {
        const Point p(4, 4);
        CHECK(!region.contains(p));
        CHECK(!region.interiorContains(p));
        CHECK(!region.boundaryContains(p));
        CHECK(!region.intersects(p));
    }

    SUBCASE("outside the outer boundary") {
        const Point p(20, 20);
        CHECK(!region.contains(p));
        CHECK(!region.interiorContains(p));
        CHECK(!region.boundaryContains(p));
        CHECK(!region.intersects(p));
    }

    SUBCASE("a point has no interior, so interiors never intersect") {
        CHECK(!region.interiorsIntersect(Point(8, 8)));
        CHECK(!region.interiorsIntersect(Point(4, 4)));
    }
}

TEST_CASE("PolygonWithHoles point location, several holes") {
    const PolygonShape outer({0, 0, 20, 0, 20, 20, 0, 20});
    const PolygonShape lower({2, 2, 6, 2, 6, 6, 2, 6});
    const PolygonShape upper({12, 12, 16, 12, 16, 16, 12, 16});
    const Region region(outer, std::vector{lower, upper});
    REQUIRE(region.isValid());

    CHECK(region.contains(Point(10, 10)));
    CHECK(region.interiorContains(Point(10, 10)));

    CHECK(!region.contains(Point(4, 4)));
    CHECK(!region.contains(Point(14, 14)));

    CHECK(region.contains(Point(4, 2)));
    CHECK(region.boundaryContains(Point(4, 2)));
    CHECK(region.contains(Point(14, 12)));
    CHECK(region.boundaryContains(Point(14, 12)));
}

TEST_CASE("PolygonWithHoles point location with touching holes") {
    // Two holes meeting at the single point (6,6). That point lies on both hole
    // boundaries, so it belongs to the region even though it is surrounded by
    // hole interiors on two sides.
    const PolygonShape outer({0, 0, 12, 0, 12, 12, 0, 12});
    const PolygonShape lower({2, 2, 6, 2, 6, 6, 2, 6});
    const PolygonShape upper({6, 6, 10, 6, 10, 10, 6, 10});
    const Region region(outer, std::vector{lower, upper});
    REQUIRE(region.isValid());

    CHECK(region.contains(Point(6, 6)));
    CHECK(!region.interiorContains(Point(6, 6)));
    CHECK(region.boundaryContains(Point(6, 6)));

    CHECK(!region.contains(Point(4, 4)));
    CHECK(!region.contains(Point(8, 8)));
    CHECK(region.interiorContains(Point(4, 8)));
    CHECK(region.interiorContains(Point(8, 4)));
}

TEST_CASE("PolygonWithHoles point location with a hole on the outer boundary") {
    // The hole shares the edge x = 0 with the outer square, so the region
    // pinches: points on that shared stretch are on both rings.
    const PolygonShape outer({0, 0, 10, 0, 10, 10, 0, 10});
    const PolygonShape hole({0, 2, 3, 2, 3, 6, 0, 6});
    const Region region(outer, std::vector{hole});
    REQUIRE(region.isValid());

    SUBCASE("on the shared stretch of boundary") {
        const Point p(0, 4);
        CHECK(region.contains(p));
        CHECK(!region.interiorContains(p));
        CHECK(region.boundaryContains(p));
    }

    SUBCASE("inside the hole is still carved out") {
        CHECK(!region.contains(Point(1, 4)));
    }

    SUBCASE("the rest of the region is unaffected") {
        CHECK(region.interiorContains(Point(6, 4)));
        CHECK(region.contains(Point(0, 8)));
    }
}

TEST_CASE("PolygonWithHoles point location degenerate regions") {
    SUBCASE("the empty region contains nothing") {
        const Region region;
        CHECK(!region.contains(Point(0, 0)));
        CHECK(!region.interiorContains(Point(0, 0)));
        CHECK(!region.boundaryContains(Point(0, 0)));
        CHECK(!region.intersects(Point(0, 0)));
    }

    SUBCASE("a hole-free region behaves exactly like its outer polygon") {
        const PolygonShape outer({0, 0, 10, 0, 10, 10, 0, 10});
        const Region region(outer);
        for (const Point p : {Point(5, 5), Point(0, 0), Point(0, 5), Point(20, 1), Point(10, 10)}) {
            CHECK(region.contains(p) == outer.contains(p));
            CHECK(region.interiorContains(p) == outer.interiorContains(p));
            CHECK(region.boundaryContains(p) == outer.boundaryContains(p));
        }
    }
}

TEST_CASE("PolygonWithHoles point location on a non-convex outer ring") {
    // An L-shaped outer boundary with a hole in the thick part, so the winding
    // test on the outer ring and the hole test both matter.
    const PolygonShape outer({0, 0, 12, 0, 12, 4, 4, 4, 4, 12, 0, 12});
    const PolygonShape hole({6, 1, 10, 1, 10, 3, 6, 3});
    const Region region(outer, std::vector{hole});
    REQUIRE(region.isValid());

    CHECK(region.interiorContains(Point(2, 8)));    // in the tall arm
    CHECK(region.interiorContains(Point(11, 1)));   // past the hole, still in the arm
    CHECK(!region.contains(Point(8, 2)));           // inside the hole
    CHECK(region.contains(Point(6, 2)));            // on the hole boundary
    CHECK(!region.contains(Point(8, 8)));           // in the L's notch
    CHECK(region.contains(Point(4, 4)));            // the reflex corner
}

TEST_CASE("PolygonWithHoles point location with exact rational coordinates") {
    using RNumber = pgl::Rational<long long>;
    using RPoint = pgl::Point<RNumber>;
    using RRegion = pgl::PolygonWithHoles<RPoint>;

    const pgl::Polygon<RPoint> outer({0, 0, 4, 0, 4, 4, 0, 4});
    const pgl::Polygon<RPoint> hole({1, 1, 3, 1, 3, 3, 1, 3});
    const RRegion region(outer, std::vector{hole});
    REQUIRE(region.isValid());

    // A point on the hole boundary that only a fractional coordinate can name.
    const RPoint onHole(RNumber(3, 2), RNumber(1));
    CHECK(region.contains(onHole));
    CHECK(region.boundaryContains(onHole));
    CHECK(!region.interiorContains(onHole));

    const RPoint insideHole(RNumber(3, 2), RNumber(3, 2));
    CHECK(!region.contains(insideHole));

    const RPoint between(RNumber(1, 2), RNumber(1, 2));
    CHECK(region.interiorContains(between));
}

// PolygonWithHoles::intersection with a point is `contains` in constructive
// form: the region is closed, so a point on any ring comes back.
TEST_CASE("PolygonWithHoles intersection with a Point") {
    const PolygonShape outer({0, 0, 10, 0, 10, 10, 0, 10});
    const PolygonShape hole({2, 2, 6, 2, 6, 6, 2, 6});
    const Region region(outer, std::vector{hole});
    REQUIRE(region.isValid());

    CHECK(region.intersection(Point(8, 8)) == Point(8, 8));    // outer interior
    CHECK(region.intersection(Point(0, 5)) == Point(0, 5));    // outer boundary
    CHECK(region.intersection(Point(2, 4)) == Point(2, 4));    // hole boundary
    CHECK(!region.intersection(Point(4, 4)).has_value());      // hole interior
    CHECK(!region.intersection(Point(12, 4)).has_value());     // outside

    // The point ranks below the region, so the pair is answered the same way
    // round, and the result carries the region's coordinate type.
    for (const auto& p : {Point(8, 8), Point(2, 4), Point(4, 4), Point(12, 4)}) {
        CHECK(p.intersection(region) == region.intersection(p));
    }

    using Rat = pgl::Rational<int64_t>;
    CHECK(region.intersection<Rat>(Point(8, 8)) == pgl::Point<Rat>(Rat(8), Rat(8)));
}
