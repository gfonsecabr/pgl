#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <vector>

using Point = pgl::Point<int>;
using Segment = pgl::Segment<Point>;
using Rectangle = pgl::Rectangle<Point>;
using Triangle = pgl::Triangle<Point>;
using Convex = pgl::Convex<Point>;
using Polygon = pgl::Polygon<Point>;
using Region = pgl::PolygonWithHoles<Point>;

// The operands here are the bounded shapes with area. Two things separate them
// from the segment and line families of the earlier increments:
//
//  - they can *enclose* a hole. Every edge of such a shape can lie in the
//    material and the shape still hold points the region does not, because a
//    hole's boundary belongs to the region while its interior does not. That is
//    what the second half of `contains` is for.
//
//  - the witness argument behind `interiorsIntersect` for a simply connected
//    polygon does not carry over. A region's interior can come apart, and then
//    no single point speaks for all of it — see the band fixture below.
//
// The fixture is the 10x10 square with a 4x4 hole in the middle:
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

// The 6x6 square cut all the way across by a band hole, which leaves two slabs
// joined only along the two slits on the outer ring. Its interior has two
// components, which is what the triangulated fallback exists for.
static Region bandSplit() {
    const Polygon outer({0, 0, 6, 0, 6, 6, 0, 6});
    const Polygon hole({0, 2, 6, 2, 6, 4, 0, 4});
    return Region(outer, std::vector{hole});
}

TEST_CASE("PolygonWithHoles vs Rectangle: containment") {
    const Region region = annulus();
    REQUIRE(region.isValid());

    SUBCASE("rectangle in the material between the rings") {
        const Rectangle r(Point(1, 1), Point(2, 9));
        CHECK(region.contains(r));
        CHECK(region.interiorContains(r));
        CHECK(!region.boundaryContains(r));
        CHECK(region.intersects(r));
        CHECK(region.interiorsIntersect(r));
    }

    SUBCASE("rectangle touching the outer ring leaves the region interior") {
        const Rectangle r(Point(0, 0), Point(2, 2));
        CHECK(region.contains(r));
        CHECK(!region.interiorContains(r));
        CHECK(region.interiorsIntersect(r));
    }

    SUBCASE("rectangle touching the hole ring leaves the region interior") {
        const Rectangle r(Point(1, 3), Point(3, 7));
        CHECK(region.contains(r));
        CHECK(!region.interiorContains(r));
        CHECK(region.interiorsIntersect(r));
    }

    SUBCASE("rectangle overlapping the hole is not contained") {
        const Rectangle r(Point(1, 4), Point(5, 6));
        CHECK(!region.contains(r));
        CHECK(!region.interiorContains(r));
        CHECK(region.intersects(r));
        CHECK(region.interiorsIntersect(r));
    }

    SUBCASE("rectangle strictly inside the hole misses the region") {
        const Rectangle r(Point(4, 4), Point(6, 6));
        CHECK(!region.contains(r));
        CHECK(!region.intersects(r));
        CHECK(!region.interiorsIntersect(r));
        CHECK(region.squaredDistance<double>(r) == doctest::Approx(1.0));
    }

    SUBCASE("rectangle spanning the hole exactly meets the region on its rim") {
        const Rectangle r(Point(3, 3), Point(7, 7));
        CHECK(!region.contains(r));
        CHECK(region.intersects(r));           // the shared ring belongs to both
        CHECK(!region.interiorsIntersect(r));  // but no area is shared
        CHECK(region.boundaryContains(r) == false);
        CHECK(region.squaredDistance<double>(r) == doctest::Approx(0.0));
    }

    SUBCASE("rectangle enclosing the hole is not contained") {
        // Every edge of the rectangle runs through the material, yet the
        // rectangle holds the whole open hole, which the region does not.
        const Rectangle r(Point(2, 2), Point(8, 8));
        CHECK(!region.contains(r));
        CHECK(!region.interiorContains(r));
        CHECK(region.intersects(r));
        CHECK(region.interiorsIntersect(r));
    }

    SUBCASE("the whole outer square is not contained either") {
        const Rectangle r(Point(0, 0), Point(10, 10));
        CHECK(!region.contains(r));
        CHECK(region.intersects(r));
        CHECK(region.interiorsIntersect(r));
    }

    SUBCASE("a degenerate rectangle is a segment") {
        const Rectangle onBoundary(Point(0, 1), Point(0, 9));
        CHECK(region.contains(onBoundary));
        CHECK(!region.interiorContains(onBoundary));
        CHECK(region.boundaryContains(onBoundary));
        CHECK(!region.interiorsIntersect(onBoundary));

        const Rectangle throughTheHole(Point(1, 5), Point(9, 5));
        CHECK(!region.contains(throughTheHole));
        CHECK(!region.boundaryContains(throughTheHole));
        CHECK(region.intersects(throughTheHole));

        const Rectangle insideTheHole(Point(4, 5), Point(6, 5));
        CHECK(!region.intersects(insideTheHole));
    }

    SUBCASE("a rectangle outside the region") {
        const Rectangle r(Point(13, 0), Point(15, 2));
        CHECK(!region.intersects(r));
        CHECK(region.squaredDistance<double>(r) == doctest::Approx(9.0));
        CHECK(region.distanceL1<double>(r) == doctest::Approx(3.0));
        CHECK(region.distanceLInf<double>(r) == doctest::Approx(3.0));
    }
}

TEST_CASE("PolygonWithHoles vs Triangle") {
    const Region region = annulus();

    SUBCASE("triangle in the material") {
        const Triangle t(Point(1, 1), Point(2, 1), Point(1, 2));
        CHECK(region.contains(t));
        CHECK(region.interiorContains(t));
        CHECK(region.interiorsIntersect(t));
    }

    SUBCASE("triangle with one vertex in the hole") {
        const Triangle t(Point(1, 1), Point(9, 1), Point(5, 5));
        CHECK(!region.contains(t));
        CHECK(region.intersects(t));
        CHECK(region.interiorsIntersect(t));
    }

    SUBCASE("triangle inside the hole") {
        const Triangle t(Point(4, 4), Point(6, 4), Point(5, 6));
        CHECK(!region.intersects(t));
        CHECK(!region.interiorsIntersect(t));
        CHECK(region.squaredDistance<double>(t) == doctest::Approx(1.0));
    }

    SUBCASE("degenerate triangle on a hole edge") {
        const Triangle t(Point(3, 4), Point(3, 5), Point(3, 6));
        CHECK(region.contains(t));
        CHECK(region.boundaryContains(t));
        CHECK(!region.interiorContains(t));
        CHECK(!region.interiorsIntersect(t));
    }

    SUBCASE("triangle covering the whole region") {
        const Triangle t(Point(-20, -10), Point(30, -10), Point(5, 30));
        CHECK(!region.contains(t));
        CHECK(region.intersects(t));
        CHECK(region.interiorsIntersect(t));
        CHECK(t.contains(region.outer()));
    }
}

TEST_CASE("PolygonWithHoles vs Convex") {
    const Region region = annulus();

    SUBCASE("convex hull in the material") {
        const Convex c({Point(1, 1), Point(2, 1), Point(2, 2), Point(1, 2)});
        CHECK(region.contains(c));
        CHECK(region.interiorContains(c));
        CHECK(region.interiorsIntersect(c));
    }

    SUBCASE("convex hull enclosing the hole is not contained") {
        const Convex c({Point(1, 1), Point(9, 1), Point(9, 9), Point(1, 9)});
        CHECK(!region.contains(c));
        CHECK(region.interiorsIntersect(c));
    }

    SUBCASE("convex hull inside the hole") {
        const Convex c({Point(4, 4), Point(6, 4), Point(6, 6), Point(4, 6)});
        CHECK(!region.intersects(c));
        CHECK(region.distanceLInf<double>(c) == doctest::Approx(1.0));
    }
}

TEST_CASE("PolygonWithHoles vs Polygon") {
    const Region region = annulus();

    SUBCASE("an L-shaped polygon threading the material") {
        const Polygon p({1, 1, 9, 1, 9, 2, 2, 2, 2, 9, 1, 9});
        CHECK(region.contains(p));
        CHECK(region.interiorContains(p));
        CHECK(region.interiorsIntersect(p));
    }

    SUBCASE("a polygon whose edges all run through the material") {
        // The square between the two rings never touches the hole with an edge,
        // and it is still not contained: it closes over the hole, whose interior
        // is not part of the region.
        const Polygon p({1, 1, 9, 1, 9, 9, 1, 9});
        for (const auto& edge : p.edgesView()) {
            CHECK(region.contains(edge));
        }
        CHECK(!region.contains(p));
        CHECK(region.interiorsIntersect(p));
    }

    SUBCASE("a polygon crossing from the material into the hole") {
        const Polygon p({1, 4, 5, 4, 5, 6, 1, 6});
        CHECK(!region.contains(p));
        CHECK(region.intersects(p));
        CHECK(region.interiorsIntersect(p));
    }

    SUBCASE("a polygon equal to the hole") {
        const Polygon p({3, 3, 7, 3, 7, 7, 3, 7});
        CHECK(!region.contains(p));
        CHECK(region.intersects(p));
        CHECK(!region.interiorsIntersect(p));
    }

    SUBCASE("a degenerate polygon along the outer ring") {
        const Polygon p({0, 1, 0, 9});
        CHECK(region.contains(p));
        CHECK(region.boundaryContains(p));
        CHECK(!region.interiorContains(p));
    }

    SUBCASE("a distant polygon") {
        const Polygon p({20, 0, 22, 0, 22, 2, 20, 2});
        CHECK(!region.intersects(p));
        CHECK(region.squaredDistance<double>(p) == doctest::Approx(100.0));
        CHECK(region.distanceL1<double>(p) == doctest::Approx(10.0));
    }
}

TEST_CASE("PolygonWithHoles vs PolygonWithHoles") {
    const Region region = annulus();

    SUBCASE("a region nested in the material") {
        const Region inner(Polygon({0, 0, 2, 0, 2, 10, 0, 10}));
        CHECK(region.contains(inner));
        CHECK(!region.interiorContains(inner));
        CHECK(region.intersects(inner));
        CHECK(region.interiorsIntersect(inner));
        CHECK(!inner.contains(region));
    }

    SUBCASE("a region whose own hole is the region's hole") {
        // Same outer square, same hole: identical regions.
        const Region same = annulus();
        CHECK(region.contains(same));
        CHECK(same.contains(region));
        CHECK(region.interiorsIntersect(same));
        CHECK(region.squaredDistance<double>(same) == doctest::Approx(0.0));
    }

    SUBCASE("a region holding a bigger hole is contained") {
        const Region wider(Polygon({0, 0, 10, 0, 10, 10, 0, 10}),
                           std::vector{Polygon({2, 2, 8, 2, 8, 8, 2, 8})});
        REQUIRE(wider.isValid());
        CHECK(region.contains(wider));
        CHECK(!wider.contains(region));
        CHECK(region.interiorsIntersect(wider));
    }

    SUBCASE("two regions meeting only along the shared hole rim") {
        const Region insideTheHole(Polygon({3, 3, 7, 3, 7, 7, 3, 7}));
        CHECK(!region.contains(insideTheHole));
        CHECK(region.intersects(insideTheHole));
        CHECK(!region.interiorsIntersect(insideTheHole));
        CHECK(region.squaredDistance<double>(insideTheHole) == doctest::Approx(0.0));
    }

    SUBCASE("disjoint regions") {
        const Region away = annulus() + Point(20, 0);
        CHECK(!region.intersects(away));
        CHECK(!region.interiorsIntersect(away));
        CHECK(region.squaredDistance<double>(away) == doctest::Approx(100.0));
        CHECK(region.distanceLInf<double>(away) == doctest::Approx(10.0));
    }

    SUBCASE("a slit of one region lying in the other region's hole") {
        // `slit` is the 10x10 square with a hole that shares its bottom edge, so
        // the stretch x in [4,6], y = 5 is a slit: it belongs to `slit` but has
        // no interior beside it. That stretch is inside the annulus' hole, so
        // `slit` is *not* contained even though no interior of either meets it.
        const Region slit(Polygon({0, 0, 10, 0, 10, 10, 0, 10}),
                          std::vector{Polygon({4, 5, 6, 5, 6, 9, 4, 9})});
        REQUIRE(slit.isValid());
        CHECK(!region.contains(slit));
        CHECK(region.intersects(slit));
        CHECK(region.interiorsIntersect(slit));
    }
}

TEST_CASE("PolygonWithHoles with a split interior") {
    const Region region = bandSplit();
    REQUIRE(region.isValid());

    SUBCASE("a rectangle whose boundary lies entirely on the region boundary") {
        // Every edge of this rectangle lies on ∂A — three on the outer ring, one
        // on the hole's bottom edge — every ring vertex of the region lies on
        // the rectangle's boundary, and the rectangle's own centre falls on the
        // hole's rim. So no witness point on either boundary settles it, and
        // neither does the region's own `pointInside`, which speaks for one slab
        // only. The lower slab is nonetheless the rectangle, and the interiors
        // do meet: this is the case the triangulated domain exists for.
        const Rectangle r(Point(0, 0), Point(6, 2));
        CHECK(region.interiorsIntersect(r));
        CHECK(region.contains(r));
        CHECK(!region.interiorContains(r));
    }

    SUBCASE("a rectangle reaching into the hole is not contained") {
        const Rectangle r(Point(0, 0), Point(6, 3));
        CHECK(!region.contains(r));
        CHECK(region.intersects(r));
        CHECK(region.interiorsIntersect(r));
    }

    SUBCASE("a rectangle covering the hole alone") {
        const Rectangle r(Point(0, 2), Point(6, 4));
        CHECK(!region.interiorsIntersect(r));
        CHECK(region.intersects(r));  // the slits and the hole rim are shared
        CHECK(!region.contains(r));
    }

    SUBCASE("the upper slab alone is contained") {
        CHECK(region.contains(Rectangle(Point(1, 4), Point(5, 6))));
        CHECK(region.interiorsIntersect(Rectangle(Point(1, 4), Point(5, 6))));
    }

    SUBCASE("a rectangle touching only the upper slab") {
        const Rectangle r(Point(2, 5), Point(4, 8));
        CHECK(region.interiorsIntersect(r));
        CHECK(!region.contains(r));
    }
}

TEST_CASE("PolygonWithHoles vs Polygon: exact rational coordinates") {
    using ERational = pgl::Rational<pgl::BigInt>;
    using EPoint = pgl::Point<ERational>;
    using EPolygon = pgl::Polygon<EPoint>;
    using ERegion = pgl::PolygonWithHoles<EPoint>;
    using ERectangle = pgl::Rectangle<EPoint>;
    using ETriangle = pgl::Triangle<EPoint>;

    const EPolygon outer({EPoint(0, 0), EPoint(4, 0), EPoint(4, 4), EPoint(0, 4)});
    const EPolygon hole({EPoint(1, 1), EPoint(3, 1), EPoint(3, 3), EPoint(1, 3)});
    const ERegion region(outer, std::vector{hole});
    REQUIRE(region.isValid());

    const ERectangle sliver(EPoint(ERational(1, 2), ERational(1, 2)),
                            EPoint(ERational(3, 4), ERational(7, 2)));
    CHECK(region.contains(sliver));
    CHECK(region.interiorContains(sliver));
    CHECK(region.interiorsIntersect(sliver));

    const ERectangle overTheHole(EPoint(ERational(1, 2), ERational(1, 2)),
                                 EPoint(ERational(7, 2), ERational(7, 2)));
    CHECK(!region.contains(overTheHole));
    CHECK(region.interiorsIntersect(overTheHole));

    const ETriangle inTheHole(EPoint(ERational(3, 2), ERational(3, 2)),
                              EPoint(ERational(5, 2), ERational(3, 2)),
                              EPoint(2, 2));
    CHECK(!region.intersects(inTheHole));
    CHECK(region.squaredDistance<ERational>(inTheHole) == ERational(1, 4));

    const ERegion nested(EPolygon({EPoint(0, 0), EPoint(1, 0), EPoint(1, 4), EPoint(0, 4)}));
    CHECK(region.contains(nested));
    CHECK(region.interiorsIntersect(nested));
}

TEST_CASE("PolygonWithHoles: the symmetric predicates answer the same either way") {
    const Region region = annulus();
    const Rectangle r(Point(2, 2), Point(8, 8));
    const Triangle t(Point(1, 1), Point(9, 1), Point(5, 5));
    const Convex c({Point(4, 4), Point(6, 4), Point(6, 6), Point(4, 6)});
    const Polygon p({1, 4, 5, 4, 5, 6, 1, 6});

    CHECK(r.intersects(region) == region.intersects(r));
    CHECK(r.interiorsIntersect(region) == region.interiorsIntersect(r));
    CHECK(t.intersects(region) == region.intersects(t));
    CHECK(t.interiorsIntersect(region) == region.interiorsIntersect(t));
    CHECK(c.intersects(region) == region.intersects(c));
    CHECK(c.interiorsIntersect(region) == region.interiorsIntersect(c));
    CHECK(p.intersects(region) == region.intersects(p));
    CHECK(p.interiorsIntersect(region) == region.interiorsIntersect(p));

    CHECK(r.squaredDistance<double>(region) == doctest::Approx(region.squaredDistance<double>(r)));
    CHECK(p.distanceL1<double>(region) == doctest::Approx(region.distanceL1<double>(p)));
    CHECK(c.distanceLInf<double>(region) == doctest::Approx(region.distanceLInf<double>(c)));

    // A segment against an area operand still reaches the region the same way.
    const Segment s(Point(1, 5), Point(9, 5));
    CHECK(s.intersects(region) == region.intersects(s));
}
