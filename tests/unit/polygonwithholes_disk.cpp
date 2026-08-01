#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <vector>

using Point = pgl::Point<int>;
using Segment = pgl::Segment<Point>;
using Polygon = pgl::Polygon<Point>;
using Region = pgl::PolygonWithHoles<Point>;
using Disk = pgl::Disk<Point>;

// The disk is the first operand with area that is *not* bounded by segments, and
// the first since the segment family that is the closure of its own interior
// again. That last property is what the polygonal area operands could not be
// relied on to have — another region may carry a slit — and having it back makes
// the per-hole rewriting of A = outer ∖ ⋃ hole° apply directly:
//
//   contains(D)          outer contains D, and no hole interior meets D
//   interiorContains(D)  outer° contains D, and no hole meets D at all
//
// What the disk does not bring is edges, so interiorsIntersect has no boundary
// scan to fall back on and goes to the triangulated domain.
//
// The fixture is the 12x12 square with a 4x4 hole in the middle, which leaves
// material four units wide — room for a radius-1 disk with clearance on both
// sides.

static Region annulus() {
    const Polygon outer({0, 0, 12, 0, 12, 12, 0, 12});
    const Polygon hole({4, 4, 8, 4, 8, 8, 4, 8});
    return Region(outer, std::vector{hole});
}

// The 6x6 square cut across by a band hole that shares a stretch of edge with
// the outer ring on both sides. The region pinches shut along those slits.
static Region bandSplit() {
    const Polygon outer({0, 0, 6, 0, 6, 6, 0, 6});
    const Polygon hole({0, 2, 6, 2, 6, 4, 0, 4});
    return Region(outer, std::vector{hole});
}

TEST_CASE("PolygonWithHoles vs Disk: containment") {
    const Region region = annulus();
    REQUIRE(region.isValid());

    SUBCASE("a disk in the material, clear of both rings") {
        const Disk d(Point(2, 6), 1);
        CHECK(region.contains(d));
        CHECK(region.interiorContains(d));
        CHECK(!region.boundaryContains(d));
        CHECK(region.intersects(d));
        CHECK(region.interiorsIntersect(d));
        CHECK(region.squaredDistance(d) == doctest::Approx(0.0));
    }

    SUBCASE("a disk tangent to the outer ring from inside") {
        const Disk d(Point(1, 6), 1);
        CHECK(region.contains(d));
        CHECK(!region.interiorContains(d));  // it reaches ∂A
        CHECK(region.interiorsIntersect(d));
    }

    SUBCASE("a disk tangent to a hole from outside") {
        const Disk d(Point(3, 6), 1);
        CHECK(region.contains(d));           // the hole boundary belongs to the region
        CHECK(!region.interiorContains(d));  // but not to its interior
        CHECK(region.intersects(d));
        CHECK(region.interiorsIntersect(d));
    }

    SUBCASE("a disk crossing into the hole is not contained") {
        const Disk d(Point(4, 6), 1);
        CHECK(!region.contains(d));
        CHECK(!region.interiorContains(d));
        CHECK(region.intersects(d));
        CHECK(region.interiorsIntersect(d));
    }

    SUBCASE("a disk enclosing the hole is not contained either") {
        // Inside the outer ring with room to spare, but it swallows the whole
        // open hole, which the region does not have.
        const Disk d(Point(6, 6), 4);
        CHECK(region.outer().contains(d));
        CHECK(!region.contains(d));
        CHECK(!region.interiorContains(d));
        CHECK(region.intersects(d));
        CHECK(region.interiorsIntersect(d));
    }

    SUBCASE("a disk strictly inside the hole misses the region") {
        const Disk d(Point(6, 6), 1);
        CHECK(!region.contains(d));
        CHECK(!region.intersects(d));
        CHECK(!region.interiorsIntersect(d));
        // The nearest region point is on the hole rim, two units from the
        // centre, so the gap is 2 - 1.
        CHECK(region.squaredDistance(d) == doctest::Approx(1.0));
    }

    SUBCASE("a disk inside the hole but touching its rim") {
        const Disk d(Point(5, 6), 1);
        CHECK(!region.contains(d));
        CHECK(region.intersects(d));           // the rim belongs to the region
        CHECK(!region.interiorsIntersect(d));  // the open disk stays in the hole
        CHECK(region.squaredDistance(d) == doctest::Approx(0.0));
    }

    SUBCASE("a disk swallowing the whole region") {
        const Disk d(Point(6, 6), 20);
        CHECK(!region.contains(d));
        CHECK(region.intersects(d));
        CHECK(region.interiorsIntersect(d));
    }

    SUBCASE("a disk outside the region") {
        const Disk d(Point(18, 6), 2);
        CHECK(!region.intersects(d));
        CHECK(!region.interiorsIntersect(d));
        // Six units from the centre to the right edge, less the radius.
        CHECK(region.squaredDistance(d) == doctest::Approx(16.0));
    }
}

TEST_CASE("PolygonWithHoles vs Disk: a disk of radius zero is its point") {
    // The only degenerate disk the library defines: three equal defining points,
    // covering exactly the point a(). Three distinct collinear points determine
    // no circle and are undefined (doc/raw/shapes.md), so they are not tested.
    const Region region = annulus();

    SUBCASE("in the material") {
        const Disk zero(Point(2, 6), 0);
        REQUIRE(zero.isPoint());
        REQUIRE(!zero.isUndefined());
        const Point p(2, 6);
        CHECK(region.contains(zero) == region.contains(p));
        CHECK(region.interiorContains(zero) == region.interiorContains(p));
        CHECK(region.boundaryContains(zero) == region.boundaryContains(p));
        CHECK(region.intersects(zero) == region.intersects(p));
        CHECK(region.contains(zero));
        CHECK(region.interiorContains(zero));
        CHECK(!region.interiorsIntersect(zero));  // no area to share
        CHECK(region.squaredDistance(zero) == doctest::Approx(0.0));
    }

    SUBCASE("on the outer ring") {
        const Disk zero(Point(0, 6), 0);
        CHECK(region.contains(zero));
        CHECK(!region.interiorContains(zero));
        CHECK(region.boundaryContains(zero));
        CHECK(!region.interiorsIntersect(zero));
    }

    SUBCASE("inside the hole") {
        const Disk zero(Point(6, 6), 0);
        CHECK(!region.contains(zero));
        CHECK(!region.intersects(zero));
        CHECK(region.squaredDistance(zero) ==
              doctest::Approx(region.squaredDistance<double>(Point(6, 6))));
        CHECK(region.squaredDistance(zero) == doctest::Approx(4.0));
    }

    SUBCASE("far outside the region") {
        // Worth pinning: Disk::intersects(Segment) answers true for *every*
        // segment once the defining points are collinear, so a radius-zero disk
        // reaches the region through Polygon::intersects(Disk) unless the region
        // settles it from the point itself.
        const Disk zero(Point(50, 50), 0);
        CHECK(!region.contains(zero));
        CHECK(!region.intersects(zero));
        CHECK(!region.interiorsIntersect(zero));
        CHECK(region.squaredDistance(zero) ==
              doctest::Approx(region.squaredDistance<double>(Point(50, 50))));
    }
}

TEST_CASE("PolygonWithHoles vs Disk: a pinched region") {
    // The band's slits belong to the region and carry no area beside them, so a
    // disk that reaches nothing but a slit meets the region without meeting its
    // interior. No edge of the disk can say so — it has none — and the answer
    // comes from the triangulated domain.
    const Region region = bandSplit();
    REQUIRE(region.isValid());

    const Disk onTheSlit(Point(0, 3), 1);
    CHECK(!region.contains(onTheSlit));
    CHECK(region.intersects(onTheSlit));
    CHECK(!region.interiorsIntersect(onTheSlit));

    // Reaching past the band into a slab is a different matter.
    const Disk intoASlab(Point(0, 3), 2);
    CHECK(region.intersects(intoASlab));
    CHECK(region.interiorsIntersect(intoASlab));
}

TEST_CASE("PolygonWithHoles vs Disk: exact rational coordinates") {
    using ERational = pgl::ERational;
    using EPoint = pgl::Point<ERational>;
    using EPolygon = pgl::Polygon<EPoint>;
    using ERegion = pgl::PolygonWithHoles<EPoint>;
    using EDisk = pgl::Disk<EPoint>;

    const EPolygon outer(std::vector{EPoint(0, 0), EPoint(8, 0), EPoint(8, 8), EPoint(0, 8)});
    const EPolygon hole(std::vector{EPoint(3, 3), EPoint(5, 3), EPoint(5, 5), EPoint(3, 5)});
    const ERegion region(outer, std::vector{hole});
    REQUIRE(region.isValid());

    const EDisk half(EPoint(ERational(3, 2), 4), ERational(1, 2));
    CHECK(region.contains(half));
    CHECK(region.interiorContains(half));

    const EDisk overTheHole(EPoint(4, 4), ERational(3, 2));
    CHECK(!region.contains(overTheHole));
    CHECK(region.intersects(overTheHole));
    CHECK(region.interiorsIntersect(overTheHole));
}

TEST_CASE("PolygonWithHoles vs Disk: the symmetric predicates and distances") {
    const Region region = annulus();
    const Disk overlapping(Point(4, 6), 1);
    const Disk inTheHole(Point(6, 6), 1);

    CHECK(overlapping.intersects(region) == region.intersects(overlapping));
    CHECK(overlapping.interiorsIntersect(region) == region.interiorsIntersect(overlapping));
    CHECK(inTheHole.intersects(region) == region.intersects(inTheHole));
    CHECK(inTheHole.interiorsIntersect(region) == region.interiorsIntersect(inTheHole));
    CHECK(inTheHole.squaredDistance(region) == doctest::Approx(region.squaredDistance(inTheHole)));

    // A region without holes is its outer polygon, and answers exactly as it does.
    const Polygon poly({0, 0, 12, 0, 12, 12, 0, 12});
    const Region solid(poly);
    for (const Disk& d : {Disk(Point(2, 6), 1), Disk(Point(6, 6), 1), Disk(Point(0, 0), 3),
                          Disk(Point(18, 6), 2)}) {
        CHECK(solid.contains(d) == poly.contains(d));
        CHECK(solid.interiorContains(d) == poly.interiorContains(d));
        CHECK(solid.boundaryContains(d) == poly.boundaryContains(d));
        CHECK(solid.intersects(d) == poly.intersects(d));
        CHECK(solid.interiorsIntersect(d) == poly.interiorsIntersect(d));
        CHECK(solid.squaredDistance(d) == doctest::Approx(poly.squaredDistance(d)));
    }
}
