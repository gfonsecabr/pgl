#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <vector>

using Point = pgl::Point<int>;
using Segment = pgl::Segment<Point>;
using PolygonShape = pgl::Polygon<Point>;
using Region = pgl::PolygonWithHoles<Point>;
using Chain = pgl::MonotoneChain<Point>;
using PolylineShape = pgl::Polyline<Point>;

// The polygonal chains — a monotone chain and a polyline — are the first
// operands since the segment family that are one-dimensional again, and they
// bring nothing new to the region: a chain is exactly the union of its edges, so
// each of the four set-level relations is that relation over every edge (over
// *some* edge, for intersects), and the segment overloads already carry the hole
// bookkeeping. Only interiorsIntersect needs the chain's own convention, where
// the relative interior is the chain minus its two extreme points.
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
    const PolygonShape outer({0, 0, 10, 0, 10, 10, 0, 10});
    const PolygonShape hole({3, 3, 7, 3, 7, 7, 3, 7});
    return Region(outer, std::vector{hole});
}

// The 6x6 square cut all the way across by a band hole. The band shares a
// stretch of edge with the outer ring on both sides, so the region pinches shut
// along those two slits: they belong to the region and carry no area beside
// them.
static Region bandSplit() {
    const PolygonShape outer({0, 0, 6, 0, 6, 6, 0, 6});
    const PolygonShape hole({0, 2, 6, 2, 6, 4, 0, 4});
    return Region(outer, std::vector{hole});
}

TEST_CASE("PolygonWithHoles vs MonotoneChain") {
    const Region region = annulus();
    REQUIRE(region.isValid());

    SUBCASE("a chain in the material below the hole") {
        const Chain chain(std::vector{Point(1, 1), Point(5, 1), Point(9, 1)});
        CHECK(region.contains(chain));
        CHECK(region.interiorContains(chain));
        CHECK(!region.boundaryContains(chain));
        CHECK(region.intersects(chain));
        CHECK(region.interiorsIntersect(chain));
        CHECK(region.squaredDistance<double>(chain) == doctest::Approx(0.0));
    }

    SUBCASE("a chain crossing the hole is not contained") {
        const Chain chain(std::vector{Point(1, 5), Point(5, 5), Point(9, 5)});
        CHECK(!region.contains(chain));
        CHECK(!region.interiorContains(chain));
        CHECK(region.intersects(chain));
        CHECK(region.interiorsIntersect(chain));  // the two arms are in the material
    }

    SUBCASE("a chain along the outer ring is boundary-contained") {
        const Chain chain(std::vector{Point(0, 1), Point(0, 5), Point(0, 9)});
        CHECK(region.contains(chain));
        CHECK(!region.interiorContains(chain));
        CHECK(region.boundaryContains(chain));
        CHECK(region.intersects(chain));
        CHECK(!region.interiorsIntersect(chain));
    }

    SUBCASE("a chain along a hole ring is boundary-contained too") {
        const Chain chain(std::vector{Point(3, 3), Point(5, 3), Point(7, 3)});
        CHECK(region.contains(chain));
        CHECK(!region.interiorContains(chain));
        CHECK(region.boundaryContains(chain));
        CHECK(!region.interiorsIntersect(chain));
    }

    SUBCASE("a chain touching a hole corner only") {
        const Chain chain(std::vector{Point(1, 1), Point(3, 3)});
        CHECK(region.contains(chain));
        CHECK(!region.interiorContains(chain));  // the corner is on ∂A
        CHECK(region.interiorsIntersect(chain));
    }

    SUBCASE("a chain strictly inside the hole misses the region") {
        const Chain chain(std::vector{Point(4, 5), Point(5, 5), Point(6, 5)});
        CHECK(!region.contains(chain));
        CHECK(!region.intersects(chain));
        CHECK(!region.interiorsIntersect(chain));
        CHECK(region.squaredDistance<double>(chain) == doctest::Approx(1.0));
        CHECK(region.distanceL1<double>(chain) == doctest::Approx(1.0));
        CHECK(region.distanceLInf<double>(chain) == doctest::Approx(1.0));
    }

    SUBCASE("a chain outside the region") {
        const Chain chain(std::vector{Point(12, 0), Point(14, 4)});
        CHECK(!region.intersects(chain));
        CHECK(region.squaredDistance<double>(chain) == doctest::Approx(4.0));
        CHECK(region.distanceL1<double>(chain) == doctest::Approx(2.0));
        CHECK(region.distanceLInf<double>(chain) == doctest::Approx(2.0));
    }

    SUBCASE("a one-vertex chain is a point, an empty one is the empty set") {
        const Chain inside(std::vector{Point(1, 1)});
        CHECK(region.contains(inside));
        CHECK(region.interiorContains(inside));
        CHECK(region.intersects(inside));
        CHECK(!region.interiorsIntersect(inside));  // no relative interior

        const Chain inTheHole(std::vector{Point(5, 5)});
        CHECK(!region.contains(inTheHole));
        CHECK(!region.intersects(inTheHole));

        const Chain empty{std::vector<Point>{}};
        CHECK(region.contains(empty));
        CHECK(region.interiorContains(empty));
        CHECK(region.boundaryContains(empty));
        CHECK(!region.intersects(empty));
        CHECK(!region.interiorsIntersect(empty));
    }
}

TEST_CASE("PolygonWithHoles vs Polyline") {
    const Region region = annulus();

    SUBCASE("a polyline may double back, which a monotone chain cannot") {
        // Out into the material and back to where it started.
        const PolylineShape line(std::vector{Point(1, 1), Point(1, 9), Point(2, 1)});
        CHECK(region.contains(line));
        CHECK(region.interiorContains(line));
        CHECK(region.interiorsIntersect(line));
    }

    SUBCASE("a polyline dipping into the hole is not contained") {
        const PolylineShape line(std::vector{Point(1, 1), Point(5, 5), Point(1, 9)});
        CHECK(!region.contains(line));
        CHECK(region.intersects(line));
        CHECK(region.interiorsIntersect(line));
    }

    SUBCASE("a polyline around the hole, touching two of its corners") {
        const PolylineShape line(std::vector{Point(3, 3), Point(1, 5), Point(3, 7)});
        CHECK(region.contains(line));
        CHECK(!region.interiorContains(line));
        CHECK(region.interiorsIntersect(line));
    }

    SUBCASE("a polyline on the boundary, from an outer edge onto a hole edge") {
        const PolylineShape onOuter(std::vector{Point(0, 0), Point(10, 0), Point(10, 10)});
        CHECK(region.contains(onOuter));
        CHECK(region.boundaryContains(onOuter));
        CHECK(!region.interiorsIntersect(onOuter));
    }

    SUBCASE("a polyline inside the hole misses the region") {
        const PolylineShape line(std::vector{Point(4, 4), Point(6, 4), Point(6, 6)});
        CHECK(!region.intersects(line));
        CHECK(region.squaredDistance<double>(line) == doctest::Approx(1.0));
    }
}

TEST_CASE("PolygonWithHoles vs a chain: the pinched boundary of a slit") {
    // The band's two slits belong to the region and have no region interior
    // beside them, so a chain running along one is contained without ever
    // meeting the open region.
    const Region region = bandSplit();
    REQUIRE(region.isValid());

    SUBCASE("a chain along the left slit") {
        const Chain slit(std::vector{Point(0, 2), Point(0, 3), Point(0, 4)});
        CHECK(region.contains(slit));
        CHECK(!region.interiorContains(slit));
        CHECK(region.boundaryContains(slit));
        CHECK(region.intersects(slit));
        CHECK(!region.interiorsIntersect(slit));
    }

    SUBCASE("a chain from one slab across the band to the other") {
        const Chain across(std::vector{Point(3, 1), Point(3, 3), Point(3, 5)});
        CHECK(!region.contains(across));
        CHECK(region.intersects(across));
        CHECK(region.interiorsIntersect(across));  // both slabs are open region
    }

    SUBCASE("a polyline touching the region only at the slit") {
        // From outside the square to a point of the left slit and back out.
        const PolylineShape touch(std::vector{Point(-2, 2), Point(0, 3), Point(-2, 4)});
        CHECK(!region.contains(touch));
        CHECK(region.intersects(touch));
        CHECK(!region.interiorsIntersect(touch));
    }
}

TEST_CASE("PolygonWithHoles vs a chain: exact rational coordinates") {
    using ERational = pgl::ERational;
    using EPoint = pgl::Point<ERational>;
    using EPolygon = pgl::Polygon<EPoint>;
    using ERegion = pgl::PolygonWithHoles<EPoint>;
    using EChain = pgl::MonotoneChain<EPoint>;

    const EPolygon outer(std::vector{EPoint(0, 0), EPoint(4, 0), EPoint(4, 4), EPoint(0, 4)});
    const EPolygon hole(std::vector{EPoint(1, 1), EPoint(3, 1), EPoint(3, 3), EPoint(1, 3)});
    const ERegion region(outer, std::vector{hole});
    REQUIRE(region.isValid());

    const EChain half(std::vector{EPoint(ERational(1, 2), ERational(1, 2)),
                                  EPoint(ERational(1, 2), ERational(7, 2))});
    CHECK(region.contains(half));
    CHECK(region.interiorContains(half));
    CHECK(region.interiorsIntersect(half));

    const EChain throughTheHole(std::vector{EPoint(ERational(1, 2), 2), EPoint(ERational(7, 2), 2)});
    CHECK(!region.contains(throughTheHole));
    CHECK(region.intersects(throughTheHole));
}

TEST_CASE("PolygonWithHoles vs a chain: the symmetric predicates and distances") {
    const Region region = annulus();
    const Chain chain(std::vector{Point(1, 5), Point(5, 5), Point(9, 5)});
    const PolylineShape line(std::vector{Point(4, 4), Point(6, 4), Point(6, 6)});

    CHECK(chain.intersects(region) == region.intersects(chain));
    CHECK(chain.interiorsIntersect(region) == region.interiorsIntersect(chain));
    CHECK(line.intersects(region) == region.intersects(line));
    CHECK(line.interiorsIntersect(region) == region.interiorsIntersect(line));

    CHECK(chain.squaredDistance<double>(region) ==
          doctest::Approx(region.squaredDistance<double>(chain)));
    CHECK(line.squaredDistance<double>(region) ==
          doctest::Approx(region.squaredDistance<double>(line)));
    CHECK(line.distanceL1<double>(region) == doctest::Approx(region.distanceL1<double>(line)));
    CHECK(line.distanceLInf<double>(region) == doctest::Approx(region.distanceLInf<double>(line)));

    // A region without holes is its outer polygon, and answers exactly as it does.
    const PolygonShape poly({0, 0, 10, 0, 10, 10, 0, 10});
    const Region solid(poly);
    CHECK(solid.contains(chain) == poly.contains(chain));
    CHECK(solid.interiorContains(chain) == poly.interiorContains(chain));
    CHECK(solid.boundaryContains(chain) == poly.boundaryContains(chain));
    CHECK(solid.intersects(chain) == poly.intersects(chain));
    CHECK(solid.interiorsIntersect(chain) == poly.interiorsIntersect(chain));
    CHECK(solid.contains(line) == poly.contains(line));
    CHECK(solid.interiorsIntersect(line) == poly.interiorsIntersect(line));

    // The union of the edges is what the relations are about, so a chain and the
    // segments it is made of agree edge by edge.
    const Segment left(Point(1, 5), Point(5, 5));
    const Segment right(Point(5, 5), Point(9, 5));
    CHECK(region.contains(chain) == (region.contains(left) && region.contains(right)));
    CHECK(region.intersects(chain) == (region.intersects(left) || region.intersects(right)));
}

// PolygonWithHoles::intersection clips a chain edge by edge and coalesces the
// pieces, exactly as the Polygon overload does — the region outranks both chain
// kinds, so it owns the pair and the shared clip lives on Polyline. The pieces
// come back sorted lexicographically, not in order along the chain, and maximal:
// collinear touching runs are merged even across edges.

TEST_CASE("PolygonWithHoles intersection with a Polyline") {
    using Piece = std::variant<Point, Segment>;
    const Region region = annulus();

    SUBCASE("a polyline crossing the hole keeps the two runs beside it") {
        const PolylineShape line(std::vector{Point(-2, 5), Point(5, 5), Point(12, 5)});
        const auto pieces = region.intersection<int>(line);

        REQUIRE(pieces.size() == 2);
        CHECK(pieces[0] == Piece(Segment({0, 5}, {3, 5})));
        CHECK(pieces[1] == Piece(Segment({7, 5}, {10, 5})));
    }

    SUBCASE("a polyline inside the material comes back whole") {
        const PolylineShape line(std::vector{Point(1, 1), Point(1, 9), Point(2, 9)});
        const auto pieces = region.intersection<int>(line);

        REQUIRE(pieces.size() == 2);
        CHECK(pieces[0] == Piece(Segment({1, 1}, {1, 9})));
        CHECK(pieces[1] == Piece(Segment({1, 9}, {2, 9})));
    }

    SUBCASE("a polyline buried in the hole meets nothing") {
        const PolylineShape line(std::vector{Point(4, 4), Point(6, 4), Point(6, 6)});
        CHECK(region.intersection<int>(line).empty());
    }

    SUBCASE("a polyline inside the hole touching one of its corners") {
        // Both edges run through the hole interior and meet at the corner
        // (3,3), which is the only point of the polyline in the region.
        const PolylineShape line(std::vector{Point(5, 4), Point(3, 3), Point(4, 5)});
        const auto pieces = region.intersection<int>(line);

        REQUIRE(pieces.size() == 1);
        CHECK(pieces[0] == Piece(Point(3, 3)));
    }

    SUBCASE("a touch point covered by a run beside it is dropped") {
        // The first edge meets the region only at the hole corner (3,3), which
        // the second edge's run out into the material already covers.
        const PolylineShape line(std::vector{Point(5, 4), Point(3, 3), Point(4, 2)});
        const auto pieces = region.intersection<int>(line);

        REQUIRE(pieces.size() == 1);
        CHECK(pieces[0] == Piece(Segment({3, 3}, {4, 2})));
    }

    SUBCASE("the polyline answers the pair the same way round") {
        const PolylineShape line(std::vector{Point(-2, 5), Point(5, 5), Point(12, 5)});
        CHECK(line.intersection<int>(region) == region.intersection<int>(line));
    }
}

TEST_CASE("PolygonWithHoles intersection with a MonotoneChain") {
    using Piece = std::variant<Point, Segment>;
    const Region region = annulus();

    SUBCASE("a chain crossing the hole keeps the two runs beside it") {
        const Chain chain(std::vector{Point(-2, 5), Point(5, 5), Point(12, 5)});
        const auto pieces = region.intersection<int>(chain);

        REQUIRE(pieces.size() == 2);
        CHECK(pieces[0] == Piece(Segment({0, 5}, {3, 5})));
        CHECK(pieces[1] == Piece(Segment({7, 5}, {10, 5})));
    }

    SUBCASE("a chain agrees with the polyline it views itself as") {
        const Chain chain(std::vector{Point(1, 5), Point(5, 5), Point(9, 5)});
        CHECK(region.intersection<int>(chain) == region.intersection<int>(chain.asPolyline()));
        CHECK(chain.intersection<int>(region) == region.intersection<int>(chain));
    }
}

TEST_CASE("PolygonWithHoles intersection with a chain: the pinched boundary of a slit") {
    using Piece = std::variant<Point, Segment>;
    const Region region = bandSplit();
    REQUIRE(region.isValid());

    SUBCASE("a chain along the left slit survives whole") {
        const Chain slit(std::vector{Point(0, 2), Point(0, 3), Point(0, 4)});
        const auto pieces = region.intersection<int>(slit);

        REQUIRE(pieces.size() == 1);
        CHECK(pieces[0] == Piece(Segment({0, 2}, {0, 4})));
    }

    SUBCASE("a chain across the band keeps one run per slab") {
        const Chain across(std::vector{Point(3, 1), Point(3, 3), Point(3, 5)});
        const auto pieces = region.intersection<int>(across);

        REQUIRE(pieces.size() == 2);
        CHECK(pieces[0] == Piece(Segment({3, 1}, {3, 2})));
        CHECK(pieces[1] == Piece(Segment({3, 4}, {3, 5})));
    }

    SUBCASE("a polyline reaching the slit from outside keeps that point alone") {
        const PolylineShape touch(std::vector{Point(-2, 2), Point(0, 3), Point(-2, 4)});
        const auto pieces = region.intersection<int>(touch);

        REQUIRE(pieces.size() == 1);
        CHECK(pieces[0] == Piece(Point(0, 3)));
    }
}

TEST_CASE("PolygonWithHoles intersection with a chain: fractional crossings") {
    using Rat = pgl::Rational<int64_t>;
    using RatPoint = pgl::Point<Rat>;
    using RatSegment = pgl::Segment<RatPoint>;
    using RatPiece = std::variant<RatPoint, RatSegment>;

    const Region region = annulus();

    // The polyline runs y = 1 + x/2, entering the hole at (4,3) and leaving it
    // at (7,9/2).
    const PolylineShape line(std::vector{Point(0, 1), Point(10, 6)});
    const auto pieces = region.intersection<Rat>(line);

    REQUIRE(pieces.size() == 2);
    CHECK(pieces[0] == RatPiece(RatSegment({Rat(0), Rat(1)}, {Rat(4), Rat(3)})));
    CHECK(pieces[1] == RatPiece(RatSegment({Rat(7), Rat(9, 2)}, {Rat(10), Rat(6)})));
}
