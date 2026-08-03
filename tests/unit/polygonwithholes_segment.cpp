#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <vector>

using Point = pgl::Point<int>;
using Segment = pgl::Segment<Point>;
using OrientedSegment = pgl::OrientedSegment<Point>;
using PolygonShape = pgl::Polygon<Point>;
using Region = pgl::PolygonWithHoles<Point>;

// A segment is connected, which is what lets every predicate here reduce to
// per-ring Polygon calls with no clipping: a segment that reaches a hole
// interior reaches it along its own relative interior, and a segment that
// misses every ring boundary lies wholly inside or wholly outside the region.
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
    const PolygonShape outer({0, 0, 10, 0, 10, 10, 0, 10});
    const PolygonShape hole({3, 3, 7, 3, 7, 7, 3, 7});
    return Region(outer, std::vector{hole});
}

TEST_CASE("PolygonWithHoles vs Segment: containment") {
    const Region region = annulus();
    REQUIRE(region.isValid());

    SUBCASE("segment in the material between the rings") {
        const Segment s({1, 1}, {1, 9});
        CHECK(region.contains(s));
        CHECK(region.interiorContains(s));
        CHECK(!region.boundaryContains(s));
        CHECK(region.intersects(s));
        CHECK(region.interiorsIntersect(s));
    }

    SUBCASE("segment crossing the hole is not contained") {
        const Segment s({1, 5}, {9, 5});
        CHECK(!region.contains(s));
        CHECK(!region.interiorContains(s));
        CHECK(!region.boundaryContains(s));
        CHECK(region.intersects(s));
        CHECK(region.interiorsIntersect(s));
    }

    SUBCASE("segment strictly inside the hole misses the region") {
        const Segment s({4, 5}, {6, 5});
        CHECK(!region.contains(s));
        CHECK(!region.interiorContains(s));
        CHECK(!region.intersects(s));
        CHECK(!region.interiorsIntersect(s));
    }

    SUBCASE("segment along a hole edge is on the region boundary") {
        const Segment s({3, 3}, {7, 3});
        CHECK(region.contains(s));
        CHECK(!region.interiorContains(s));
        CHECK(region.boundaryContains(s));
        CHECK(region.intersects(s));
        CHECK(!region.interiorsIntersect(s));
    }

    SUBCASE("sub-segment of a hole edge is on the region boundary") {
        const Segment s({4, 7}, {6, 7});
        CHECK(region.contains(s));
        CHECK(region.boundaryContains(s));
        CHECK(!region.interiorsIntersect(s));
    }

    SUBCASE("segment along an outer edge is on the region boundary") {
        const Segment s({0, 2}, {0, 8});
        CHECK(region.contains(s));
        CHECK(!region.interiorContains(s));
        CHECK(region.boundaryContains(s));
        CHECK(!region.interiorsIntersect(s));
    }

    SUBCASE("segment touching a hole corner stays in the region") {
        const Segment s({1, 1}, {3, 3});
        CHECK(region.contains(s));
        CHECK(!region.interiorContains(s));  // the endpoint is on a hole ring
        CHECK(!region.boundaryContains(s));
        CHECK(region.interiorsIntersect(s));
    }

    SUBCASE("segment leaving the outer boundary is not contained") {
        const Segment s({8, 1}, {14, 1});
        CHECK(!region.contains(s));
        CHECK(!region.interiorContains(s));
        CHECK(region.intersects(s));
        CHECK(region.interiorsIntersect(s));
    }

    SUBCASE("segment entirely outside the outer boundary") {
        const Segment s({12, 0}, {12, 10});
        CHECK(!region.contains(s));
        CHECK(!region.intersects(s));
        CHECK(!region.interiorsIntersect(s));
    }

    SUBCASE("degenerate segment reduces to its point") {
        const Segment inHole({5, 5}, {5, 5});
        CHECK(!region.contains(inHole));
        CHECK(!region.intersects(inHole));

        const Segment onHoleRing({3, 5}, {3, 5});
        CHECK(region.contains(onHoleRing));
        CHECK(!region.interiorContains(onHoleRing));
        CHECK(region.boundaryContains(onHoleRing));
        CHECK(!region.interiorsIntersect(onHoleRing));

        const Segment between({1, 1}, {1, 1});
        CHECK(region.contains(between));
        CHECK(region.interiorContains(between));
        CHECK(!region.boundaryContains(between));
    }
}

TEST_CASE("PolygonWithHoles vs Segment: a chord touching the hole tangentially") {
    const Region region = annulus();

    SUBCASE("chord grazing a hole edge from outside the hole") {
        // y = 3 runs along the hole's bottom edge and continues past it in both
        // directions, so it is in the region but not in its interior.
        const Segment s({0, 3}, {10, 3});
        CHECK(region.contains(s));
        CHECK(!region.interiorContains(s));
        CHECK(!region.boundaryContains(s));  // the parts outside the hole edge are interior
        CHECK(region.interiorsIntersect(s));
    }

    SUBCASE("chord from an outer edge to a hole corner") {
        const Segment s({0, 0}, {3, 3});
        CHECK(region.contains(s));
        CHECK(!region.interiorContains(s));
        CHECK(!region.boundaryContains(s));
        CHECK(region.interiorsIntersect(s));
    }
}

TEST_CASE("PolygonWithHoles vs Segment: two holes") {
    // A 12x6 box with two square holes, leaving a vertical bridge of material
    // between them at x in [5,7].
    const PolygonShape outer({0, 0, 12, 0, 12, 6, 0, 6});
    const PolygonShape left({1, 1, 5, 1, 5, 5, 1, 5});
    const PolygonShape right({7, 1, 11, 1, 11, 5, 7, 5});
    const Region region(outer, std::vector{left, right});
    REQUIRE(region.isValid());

    SUBCASE("segment across the bridge is contained") {
        const Segment s({5, 3}, {7, 3});
        CHECK(region.contains(s));
        CHECK(!region.interiorContains(s));  // both endpoints sit on hole rings
        CHECK(!region.boundaryContains(s));
        CHECK(region.interiorsIntersect(s));
    }

    SUBCASE("segment through both holes meets the region only on the bridge") {
        const Segment s({3, 3}, {9, 3});
        CHECK(!region.contains(s));
        CHECK(region.intersects(s));
        CHECK(region.interiorsIntersect(s));
    }

    SUBCASE("segment spanning both holes but detouring outside stays out") {
        const Segment s({2, 3}, {4, 3});
        CHECK(!region.intersects(s));
        CHECK(!region.interiorsIntersect(s));
    }
}

TEST_CASE("PolygonWithHoles vs Segment: holes touching the outer ring") {
    // The hole shares the corner (0,0) with the outer square, so the region is
    // weakly simple: the shared point belongs to the region.
    const PolygonShape outer({0, 0, 8, 0, 8, 8, 0, 8});
    const PolygonShape hole({0, 0, 4, 0, 4, 4, 0, 4});
    const Region region(outer, std::vector{hole});

    SUBCASE("segment along the shared edge is on the boundary") {
        const Segment s({0, 0}, {4, 0});
        CHECK(region.contains(s));
        CHECK(region.boundaryContains(s));
        CHECK(!region.interiorsIntersect(s));
    }

    SUBCASE("segment through the hole interior is not contained") {
        const Segment s({1, 1}, {3, 3});
        CHECK(!region.contains(s));
        CHECK(!region.intersects(s));
    }

    SUBCASE("segment from the hole corner into the material") {
        const Segment s({4, 4}, {8, 8});
        CHECK(region.contains(s));
        CHECK(!region.interiorContains(s));
        CHECK(region.interiorsIntersect(s));
    }
}

// Where two rings meet the region pinches shut, and a segment crossing the
// boundary exactly there passes between two non-interior sides. These are the
// cases the plain "a transversal crossing means an interior hit" shortcut gets
// wrong, so they are pinned separately.

TEST_CASE("PolygonWithHoles vs Segment: crossing a ring touch point") {
    // The hole is a triangle whose apex (4,0) sits on the outer edge y = 0.
    const PolygonShape outer({0, 0, 8, 0, 8, 8, 0, 8});
    const PolygonShape hole({4, 0, 6, 2, 2, 2});
    const Region region(outer, std::vector{hole});
    REQUIRE(region.isValid());

    SUBCASE("straight through the touch point") {
        // Below y = 0 is outside the region, above it is inside the hole, and
        // the touch point itself is boundary — the interiors never meet.
        const Segment s({4, -1}, {4, 1});
        CHECK(region.intersects(s));
        CHECK(!region.contains(s));
        CHECK(!region.interiorsIntersect(s));
    }

    SUBCASE("through the outer edge beside the touch point") {
        const Segment s({7, -1}, {7, 1});
        CHECK(region.intersects(s));
        CHECK(region.interiorsIntersect(s));
    }
}

TEST_CASE("PolygonWithHoles vs Segment: crossing a doubly covered edge") {
    // The hole shares the corner and two whole edges with the outer square, so
    // the region is an L shape and the shared stretches are slits.
    const PolygonShape outer({0, 0, 8, 0, 8, 8, 0, 8});
    const PolygonShape hole({0, 0, 4, 0, 4, 4, 0, 4});
    const Region region(outer, std::vector{hole});
    REQUIRE(region.isValid());

    SUBCASE("crossing the slit: outside below, hole above") {
        const Segment s({2, -1}, {2, 1});
        CHECK(region.intersects(s));
        CHECK(!region.interiorsIntersect(s));
    }

    SUBCASE("crossing the same outer edge past the slit") {
        const Segment s({6, -1}, {6, 1});
        CHECK(region.interiorsIntersect(s));
    }

    SUBCASE("crossing the hole's free edge") {
        const Segment s({3, 2}, {5, 2});
        CHECK(region.interiorsIntersect(s));
    }

    SUBCASE("running along the slit is boundary") {
        const Segment s({0, 0}, {8, 0});
        CHECK(region.contains(s));
        CHECK(region.boundaryContains(s));
        CHECK(!region.interiorsIntersect(s));
    }
}

TEST_CASE("PolygonWithHoles vs Segment: two holes touching at a point") {
    const PolygonShape outer({0, 0, 10, 0, 10, 10, 0, 10});
    const PolygonShape lower({1, 1, 5, 1, 5, 5, 1, 5});
    const PolygonShape upper({5, 5, 9, 5, 9, 9, 5, 9});
    const Region region(outer, std::vector{lower, upper});
    REQUIRE(region.isValid());

    CHECK(region.contains(Point(5, 5)));
    CHECK(region.boundaryContains(Point(5, 5)));

    SUBCASE("a diagonal through both holes only touches the shared corner") {
        const Segment s({3, 3}, {7, 7});
        CHECK(region.intersects(s));
        CHECK(!region.contains(s));
        CHECK(!region.interiorsIntersect(s));
    }

    SUBCASE("a diagonal squeezing past the corner outside both holes") {
        const Segment s({7, 3}, {3, 7});
        CHECK(region.contains(s));
        CHECK(region.interiorsIntersect(s));
    }
}

TEST_CASE("PolygonWithHoles vs Segment: hole-free region matches its outer polygon") {
    const PolygonShape outer({0, 0, 6, 0, 6, 6, 0, 6});
    const Region region(outer);
    const Segment inside({1, 1}, {5, 5});
    const Segment onEdge({0, 1}, {0, 5});
    const Segment outside({7, 0}, {7, 6});
    const Segment crossing({-2, 3}, {8, 3});

    for (const auto& s : {inside, onEdge, outside, crossing}) {
        CHECK(region.contains(s) == outer.contains(s));
        CHECK(region.interiorContains(s) == outer.interiorContains(s));
        CHECK(region.boundaryContains(s) == outer.boundaryContains(s));
        CHECK(region.intersects(s) == outer.intersects(s));
        CHECK(region.interiorsIntersect(s) == outer.interiorsIntersect(s));
    }
}

TEST_CASE("PolygonWithHoles vs OrientedSegment: direction never matters") {
    const Region region = annulus();
    const Segment s({1, 5}, {9, 5});
    const OrientedSegment forward({1, 5}, {9, 5});
    const OrientedSegment backward({9, 5}, {1, 5});

    for (const auto& o : {forward, backward}) {
        CHECK(region.contains(o) == region.contains(s));
        CHECK(region.interiorContains(o) == region.interiorContains(s));
        CHECK(region.boundaryContains(o) == region.boundaryContains(s));
        CHECK(region.intersects(o) == region.intersects(s));
        CHECK(region.interiorsIntersect(o) == region.interiorsIntersect(s));
        CHECK(region.squaredDistance(o) == region.squaredDistance(s));
    }
}

TEST_CASE("PolygonWithHoles distances") {
    const Region region = annulus();

    SUBCASE("a point in the region is at distance zero") {
        CHECK(region.squaredDistance(Point(1, 1)) == 0);
        CHECK(region.distanceL1(Point(1, 1)) == 0);
        CHECK(region.distanceLInf(Point(1, 1)) == 0);
        CHECK(region.squaredDistance(Point(3, 5)) == 0);  // on a hole ring
    }

    SUBCASE("a point inside a hole measures to the nearest hole edge") {
        // (5,5) is the hole centre; the nearest boundary is any hole edge at
        // distance 2.
        CHECK(region.squaredDistance(Point(5, 5)) == 4);
        CHECK(region.distanceL1(Point(5, 5)) == 2);
        CHECK(region.distanceLInf(Point(5, 5)) == 2);

        // Off-centre inside the hole: nearest hole edge is x = 3.
        CHECK(region.squaredDistance(Point(4, 5)) == 1);
        CHECK(region.distanceL1(Point(4, 5)) == 1);
    }

    SUBCASE("a point outside measures to the outer ring") {
        CHECK(region.squaredDistance(Point(13, 5)) == 9);
        CHECK(region.distanceL1(Point(13, 5)) == 3);
        CHECK(region.distanceLInf(Point(13, 5)) == 3);
        CHECK(region.squaredDistance(Point(13, 14)) == 25);
        CHECK(region.distanceL1(Point(13, 14)) == 7);
        CHECK(region.distanceLInf(Point(13, 14)) == 4);
    }

    SUBCASE("a segment inside a hole measures to the hole boundary") {
        const Segment s({4, 4}, {6, 6});
        CHECK(!region.intersects(s));
        CHECK(region.squaredDistance(s) == 1);
        CHECK(region.distanceL1(s) == 1);
        CHECK(region.distanceLInf(s) == 1);
    }

    SUBCASE("a segment meeting the region is at distance zero") {
        CHECK(region.squaredDistance(Segment({1, 5}, {9, 5})) == 0);
        CHECK(region.distanceL1(Segment({1, 5}, {9, 5})) == 0);
    }

    SUBCASE("a segment outside measures to the outer ring") {
        const Segment s({12, 0}, {12, 10});
        CHECK(region.squaredDistance(s) == 4);
        CHECK(region.distanceL1(s) == 2);
        CHECK(region.distanceLInf(s) == 2);
    }

    SUBCASE("a hole-free region agrees with its outer polygon") {
        const PolygonShape outer({0, 0, 6, 0, 6, 6, 0, 6});
        const Region plain(outer);
        for (const Point p : {Point(3, 3), Point(9, 3), Point(-2, -2)}) {
            CHECK(plain.squaredDistance(p) == outer.squaredDistance(p));
            CHECK(plain.distanceL1(p) == outer.distanceL1(p));
            CHECK(plain.distanceLInf(p) == outer.distanceLInf(p));
        }
    }
}

TEST_CASE("PolygonWithHoles vs Segment: exact rational coordinates") {
    using ERegion = pgl::PolygonWithHoles<pgl::EPoint>;
    using EPolygon = pgl::Polygon<pgl::EPoint>;
    using ESegment = pgl::Segment<pgl::EPoint>;
    using ERational = pgl::ERational;

    const EPolygon outer({pgl::EPoint(0, 0), pgl::EPoint(4, 0), pgl::EPoint(4, 4), pgl::EPoint(0, 4)});
    const EPolygon hole({pgl::EPoint(1, 1), pgl::EPoint(3, 1), pgl::EPoint(3, 3), pgl::EPoint(1, 3)});
    const ERegion region(outer, std::vector{hole});
    REQUIRE(region.isValid());

    // A half-integer chord straight through the hole.
    const ESegment through({ERational(1, 2), ERational(2)}, {ERational(7, 2), ERational(2)});
    CHECK(!region.contains(through));
    CHECK(region.intersects(through));
    CHECK(region.interiorsIntersect(through));

    // A half-integer chord staying in the material.
    const ESegment beside({ERational(1, 2), ERational(1, 2)}, {ERational(1, 2), ERational(7, 2)});
    CHECK(region.contains(beside));
    CHECK(region.interiorContains(beside));

    // Strictly inside the hole, at distance 1/2 from the nearest hole edge.
    const ESegment buried({ERational(3, 2), ERational(2)}, {ERational(5, 2), ERational(2)});
    CHECK(!region.intersects(buried));
    CHECK(region.squaredDistance(buried) == ERational(1, 4));
    CHECK(region.distanceL1(buried) == ERational(1, 2));
}

// PolygonWithHoles::intersection clips the segment against the closed region and
// returns the disjoint pieces (points and sub-segments) in order along it. This
// is the plain, unregularized intersection, and it is the only one a segment can
// have: the boolean `closure(A° ∩ B°)` of a region and a segment is always empty.

TEST_CASE("PolygonWithHoles intersection with a Segment") {
    using Piece = std::variant<Point, Segment>;
    const Region region = annulus();

    SUBCASE("a chord across the hole yields the two pieces beside it") {
        const auto pieces = region.intersection(Segment({-5, 5}, {15, 5}));

        REQUIRE(pieces.size() == 2);
        CHECK(pieces[0] == Piece(Segment({0, 5}, {3, 5})));
        CHECK(pieces[1] == Piece(Segment({7, 5}, {10, 5})));
    }

    SUBCASE("a segment buried in the hole meets nothing") {
        CHECK(region.intersection(Segment({4, 5}, {6, 5})).empty());
    }

    SUBCASE("a segment ending in the hole is cut at the ring") {
        const auto pieces = region.intersection(Segment({1, 5}, {5, 5}));

        REQUIRE(pieces.size() == 1);
        CHECK(pieces[0] == Piece(Segment({1, 5}, {3, 5})));
    }

    SUBCASE("a segment along a hole ring survives whole") {
        // A hole ring is boundary, and the boundary belongs to the region.
        const auto pieces = region.intersection(Segment({3, 3}, {3, 7}));

        REQUIRE(pieces.size() == 1);
        CHECK(pieces[0] == Piece(Segment({3, 3}, {3, 7})));
    }

    SUBCASE("a diagonal through opposite hole corners touches each of them") {
        const auto pieces = region.intersection(Segment({0, 0}, {10, 10}));

        REQUIRE(pieces.size() == 2);
        CHECK(pieces[0] == Piece(Segment({0, 0}, {3, 3})));
        CHECK(pieces[1] == Piece(Segment({7, 7}, {10, 10})));
    }

    SUBCASE("a segment touching the region at one corner keeps that point") {
        const auto pieces = region.intersection(Segment({8, 12}, {12, 8}));

        REQUIRE(pieces.size() == 1);
        CHECK(pieces[0] == Piece(Point(10, 10)));
    }

    SUBCASE("a segment missing the region yields nothing") {
        CHECK(region.intersection(Segment({11, 0}, {11, 10})).empty());
    }

    SUBCASE("the segment answers the pair the same way round") {
        const Segment s({-5, 5}, {15, 5});
        CHECK(s.intersection(region) == region.intersection(s));
    }
}

TEST_CASE("PolygonWithHoles intersection with a Segment: two holes") {
    using Piece = std::variant<Point, Segment>;
    const PolygonShape outer({0, 0, 12, 0, 12, 6, 0, 6});
    const PolygonShape left({1, 1, 5, 1, 5, 5, 1, 5});
    const PolygonShape right({7, 1, 11, 1, 11, 5, 7, 5});
    const Region region(outer, std::vector{left, right});
    REQUIRE(region.isValid());

    SUBCASE("a chord through both holes keeps only the bridge") {
        const auto pieces = region.intersection(Segment({3, 3}, {9, 3}));

        REQUIRE(pieces.size() == 1);
        CHECK(pieces[0] == Piece(Segment({5, 3}, {7, 3})));
    }

    SUBCASE("a chord spanning the whole box keeps three pieces") {
        const auto pieces = region.intersection(Segment({-1, 3}, {13, 3}));

        REQUIRE(pieces.size() == 3);
        CHECK(pieces[0] == Piece(Segment({0, 3}, {1, 3})));
        CHECK(pieces[1] == Piece(Segment({5, 3}, {7, 3})));
        CHECK(pieces[2] == Piece(Segment({11, 3}, {12, 3})));
    }
}

// Where two rings meet the region pinches shut, and a segment crossing the
// boundary exactly there passes between two non-region sides — the piece it
// keeps is the single pinch point.

TEST_CASE("PolygonWithHoles intersection with a Segment: slits and touch points") {
    using Piece = std::variant<Point, Segment>;
    // The hole shares the corner and two whole edges with the outer square, so
    // the region is an L shape and the shared stretches are slits.
    const PolygonShape outer({0, 0, 8, 0, 8, 8, 0, 8});
    const PolygonShape hole({0, 0, 4, 0, 4, 4, 0, 4});
    const Region region(outer, std::vector{hole});
    REQUIRE(region.isValid());

    SUBCASE("running along the slit comes back whole") {
        const auto pieces = region.intersection(Segment({0, 0}, {8, 0}));

        REQUIRE(pieces.size() == 1);
        CHECK(pieces[0] == Piece(Segment({0, 0}, {8, 0})));
    }

    SUBCASE("crossing the slit keeps the pinch point alone") {
        // Below the slit is outside the region, above it is the hole interior.
        const auto pieces = region.intersection(Segment({2, -1}, {2, 1}));

        REQUIRE(pieces.size() == 1);
        CHECK(pieces[0] == Piece(Point(2, 0)));
    }

    SUBCASE("crossing the same outer edge past the slit keeps a chord") {
        const auto pieces = region.intersection(Segment({6, -1}, {6, 1}));

        REQUIRE(pieces.size() == 1);
        CHECK(pieces[0] == Piece(Segment({6, 0}, {6, 1})));
    }

    SUBCASE("a chord straddling a hole edge is cut there") {
        const auto pieces = region.intersection(Segment({3, 2}, {5, 2}));

        REQUIRE(pieces.size() == 1);
        CHECK(pieces[0] == Piece(Segment({4, 2}, {5, 2})));
    }
}

TEST_CASE("PolygonWithHoles intersection with a Segment: two holes touching at a point") {
    using Piece = std::variant<Point, Segment>;
    const PolygonShape outer({0, 0, 10, 0, 10, 10, 0, 10});
    const PolygonShape lower({1, 1, 5, 1, 5, 5, 1, 5});
    const PolygonShape upper({5, 5, 9, 5, 9, 9, 5, 9});
    const Region region(outer, std::vector{lower, upper});
    REQUIRE(region.isValid());

    SUBCASE("a diagonal through both holes keeps the shared corner alone") {
        const auto pieces = region.intersection(Segment({3, 3}, {7, 7}));

        REQUIRE(pieces.size() == 1);
        CHECK(pieces[0] == Piece(Point(5, 5)));
    }

    SUBCASE("a diagonal squeezing past the corner survives whole") {
        const auto pieces = region.intersection(Segment({7, 3}, {3, 7}));

        REQUIRE(pieces.size() == 1);
        CHECK(pieces[0] == Piece(Segment({3, 7}, {7, 3})));
    }
}

TEST_CASE("PolygonWithHoles intersection with a Segment: hole-free region matches its outer polygon") {
    const PolygonShape outer({0, 0, 10, 0, 6, 4, 10, 8, 0, 8});  // non-convex
    const Region region(outer);

    for (const auto& s : {Segment({-2, 2}, {12, 2}), Segment({-2, 6}, {12, 6}),
                          Segment({1, 1}, {3, 3}), Segment({0, 0}, {0, 8}),
                          Segment({20, 0}, {20, 8})}) {
        CHECK(region.intersection(s) == outer.intersection(s));
    }
}

TEST_CASE("PolygonWithHoles intersection with an OrientedSegment: direction never matters") {
    const Region region = annulus();
    const Segment s({-5, 5}, {15, 5});

    for (const auto& o : {OrientedSegment({-5, 5}, {15, 5}), OrientedSegment({15, 5}, {-5, 5})}) {
        CHECK(region.intersection(o) == region.intersection(s));
        CHECK(o.intersection(region) == region.intersection(s));
    }
}

TEST_CASE("PolygonWithHoles intersection with a Segment: fractional crossings") {
    using Rat = pgl::Rational<int64_t>;
    using RatPoint = pgl::Point<Rat>;
    using RatSegment = pgl::Segment<RatPoint>;
    using RatPiece = std::variant<RatPoint, RatSegment>;

    const Region region = annulus();

    // y = 1 + x/2 enters the hole where y = 3 (x = 4) and leaves it where
    // x = 7 (y = 9/2), so both hole crossings are off-lattice on one side.
    const auto pieces = region.intersection<Rat>(Segment({0, 1}, {10, 6}));

    REQUIRE(pieces.size() == 2);
    CHECK(pieces[0] == RatPiece(RatSegment({Rat(0), Rat(1)}, {Rat(4), Rat(3)})));
    CHECK(pieces[1] == RatPiece(RatSegment({Rat(7), Rat(9, 2)}, {Rat(10), Rat(6)})));
}
