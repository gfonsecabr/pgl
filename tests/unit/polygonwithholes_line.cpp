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
using PolygonShape = pgl::Polygon<Point>;
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
    const PolygonShape outer({0, 0, 10, 0, 10, 10, 0, 10});
    const PolygonShape hole({3, 3, 7, 3, 7, 7, 3, 7});
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
    const PolygonShape outer({0, 0, 10, 0, 10, 10, 0, 10});
    const PolygonShape hole({0, 5, 5, 4, 10, 5, 5, 6});
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
        const PolygonShape outer({0, 0, 10, 0, 10, 10, 0, 10});
        const PolygonShape hole({0, 5, 5, 4, 10, 5, 5, 6});
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
        const PolygonShape outer({0, 0, 10, 0, 10, 10, 0, 10});
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

// A half-plane has area, so it uses `regularizedIntersection` -- the
// one that keeps holes -- rather than the component vector
// `Polygon::intersection(Halfplane)` returns, and it answers the same in either
// order. It is the one-constraint half-plane intersection and is handled as one:
// bounded against the region first, so nothing here is unbounded by the time an
// arrangement is built.

TEST_CASE("PolygonWithHoles intersection with a Halfplane") {
    using ERational = pgl::ERational;
    using ERegion = pgl::PolygonWithHoles<pgl::Point<ERational>>;
    const Region region = annulus();

    SUBCASE("a half-plane covering the region gives it back, hole and all") {
        const Halfplane h({0, -5}, {1, -5});  // y >= -5
        const auto pieces = region.regularizedIntersection<ERational>(h);

        REQUIRE(pieces.componentCount() == 1);
        CHECK(pieces.component(0) == ERegion(region));
        CHECK(h.regularizedIntersection<ERational>(region) == pieces);
    }

    SUBCASE("a cut through the hole opens it into a notch") {
        // y >= 5 keeps the top half; the hole loses its bottom and stops being
        // one, so the piece has no hole left.
        const Halfplane h({0, 5}, {1, 5});
        const auto pieces = region.regularizedIntersection<ERational>(h);

        REQUIRE(pieces.componentCount() == 1);
        CHECK(pieces.component(0).holes().empty());
        CHECK(pieces.component(0).area<ERational>() == ERational(50 - 8));
        CHECK(h.regularizedIntersection<ERational>(region) == pieces);
    }

    SUBCASE("a cut clear of the hole keeps it") {
        // The line through (0,1) and (3,0) takes off the corner triangle only.
        const Halfplane h({0, 1}, {3, 0});
        const auto pieces = region.regularizedIntersection<ERational>(h);

        REQUIRE(pieces.componentCount() == 1);
        REQUIRE(pieces.component(0).holes().size() == 1);
        CHECK(pieces.component(0).area<ERational>() == ERational(100 - 16) - ERational(3, 2));
    }

    SUBCASE("a cut missing the region gives nothing") {
        CHECK(region.regularizedIntersection<ERational>(Halfplane({20, 1}, {20, 0})).empty());
    }

    SUBCASE("a cut meeting the region only along an edge has no area") {
        // y >= 10 keeps the top outer edge, which a regularized result drops.
        CHECK(region.regularizedIntersection<ERational>(Halfplane({0, 10}, {1, 10})).empty());
    }

    SUBCASE("a cut can leave several pieces") {
        // A U with arms at x in [0,3] and [7,10]; y >= 5 keeps the two tips.
        const PolygonShape u({0, 0, 10, 0, 10, 10, 7, 10, 7, 3, 3, 3, 3, 10, 0, 10});
        const Region shape(u);
        const auto pieces = shape.regularizedIntersection<ERational>(Halfplane({0, 5}, {1, 5}));

        REQUIRE(pieces.componentCount() == 2);
        CHECK(pieces.component(0).area<ERational>() == ERational(15));
        CHECK(pieces.component(1).area<ERational>() == ERational(15));
    }

    SUBCASE("without holes the region answers like its outer polygon would") {
        const PolygonShape outer({0, 0, 10, 0, 10, 10, 0, 10});
        const Region solid(outer);
        const auto pieces = solid.regularizedIntersection<ERational>(Halfplane({5, 1}, {5, 0}));

        REQUIRE(pieces.componentCount() == 1);
        CHECK(pieces.component(0) == ERegion(pgl::Polygon<pgl::Point<ERational>>(
                  {pgl::Point<ERational>(5, 0), pgl::Point<ERational>(10, 0),
                   pgl::Point<ERational>(10, 10), pgl::Point<ERational>(5, 10)})));
    }
}

TEST_CASE("PolygonWithHoles vs Halfplane: a slit tip carries no interior") {
    // The hole shares two whole edges with the outer square, so the region is an
    // L shape with two slits, and the corner (0,0) is the tip where they meet:
    // every neighbourhood of it holds region points, but no region interior.
    const PolygonShape outer({0, 0, 8, 0, 8, 8, 0, 8});
    const PolygonShape hole({0, 0, 4, 0, 4, 4, 0, 4});
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

// PolygonWithHoles::intersection clips an unbounded one-dimensional shape
// against the closed region. The region is bounded, so every piece is bounded
// too; a line differs from a segment only in having no parameter window to clip
// to, and a ray in having a half-open one.

TEST_CASE("PolygonWithHoles intersection with a Line") {
    using Piece = std::variant<Point, Segment>;
    const Region region = annulus();

    SUBCASE("a line across the hole yields the two chords beside it") {
        const auto pieces = region.intersection<int>(Line({0, 5}, {1, 5}));

        REQUIRE(pieces.size() == 2);
        CHECK(pieces[0] == Piece(Segment({0, 5}, {3, 5})));
        CHECK(pieces[1] == Piece(Segment({7, 5}, {10, 5})));
    }

    SUBCASE("a line along a hole edge keeps the whole chord") {
        // y = 3 runs along the hole's bottom edge, which is region boundary.
        const auto pieces = region.intersection<int>(Line({0, 3}, {1, 3}));

        REQUIRE(pieces.size() == 1);
        CHECK(pieces[0] == Piece(Segment({0, 3}, {10, 3})));
    }

    SUBCASE("a line along an outer edge keeps that edge") {
        const auto pieces = region.intersection<int>(Line({0, 0}, {1, 0}));

        REQUIRE(pieces.size() == 1);
        CHECK(pieces[0] == Piece(Segment({0, 0}, {10, 0})));
    }

    SUBCASE("a line missing the region yields nothing") {
        CHECK(region.intersection<int>(Line({0, 12}, {1, 12})).empty());
    }

    SUBCASE("a line tangent at one outer corner keeps that point") {
        const auto pieces = region.intersection<int>(Line({8, 12}, {12, 8}));

        REQUIRE(pieces.size() == 1);
        CHECK(pieces[0] == Piece(Point(10, 10)));
    }

    SUBCASE("the line answers the pair the same way round") {
        const Line l({0, 5}, {1, 5});
        CHECK(l.intersection<int>(region) == region.intersection<int>(l));
    }
}

TEST_CASE("PolygonWithHoles intersection with a Line: a hole apex on the outer ring") {
    using Piece = std::variant<Point, Segment>;
    // The hole is a triangle whose apex (4,0) sits on the outer edge y = 0, so
    // the region pinches shut there.
    const PolygonShape outer({0, 0, 8, 0, 8, 8, 0, 8});
    const PolygonShape hole({4, 0, 6, 2, 2, 2});
    const Region region(outer, std::vector{hole});
    REQUIRE(region.isValid());

    SUBCASE("a line straight through the apex keeps it as its own piece") {
        const auto pieces = region.intersection<int>(Line({4, 0}, {4, 1}));

        REQUIRE(pieces.size() == 2);
        CHECK(pieces[0] == Piece(Point(4, 0)));
        CHECK(pieces[1] == Piece(Segment({4, 2}, {4, 8})));
    }

    SUBCASE("a line beside the apex keeps one chord") {
        const auto pieces = region.intersection<int>(Line({7, 0}, {7, 1}));

        REQUIRE(pieces.size() == 1);
        CHECK(pieces[0] == Piece(Segment({7, 0}, {7, 8})));
    }
}

TEST_CASE("PolygonWithHoles intersection with an OrientedLine: direction never matters") {
    const Region region = annulus();
    const Line l({0, 5}, {1, 5});

    for (const auto& o : {OrientedLine({0, 5}, {1, 5}), OrientedLine({1, 5}, {0, 5})}) {
        CHECK(region.intersection<int>(o) == region.intersection<int>(l));
        CHECK(o.intersection<int>(region) == region.intersection<int>(l));
    }
}

TEST_CASE("PolygonWithHoles intersection with a Ray") {
    using Piece = std::variant<Point, Segment>;
    const Region region = annulus();

    SUBCASE("a ray from inside the hole keeps only what lies ahead") {
        const auto pieces = region.intersection<int>(Ray({5, 5}, {6, 5}));

        REQUIRE(pieces.size() == 1);
        CHECK(pieces[0] == Piece(Segment({7, 5}, {10, 5})));
    }

    SUBCASE("a ray from outside crossing the whole region keeps both chords") {
        const auto pieces = region.intersection<int>(Ray({-5, 5}, {-4, 5}));

        REQUIRE(pieces.size() == 2);
        CHECK(pieces[0] == Piece(Segment({0, 5}, {3, 5})));
        CHECK(pieces[1] == Piece(Segment({7, 5}, {10, 5})));
    }

    SUBCASE("a ray from the material outward is cut at the source") {
        const auto pieces = region.intersection<int>(Ray({8, 5}, {9, 5}));

        REQUIRE(pieces.size() == 1);
        CHECK(pieces[0] == Piece(Segment({8, 5}, {10, 5})));
    }

    SUBCASE("a ray pointing away from the region yields nothing") {
        CHECK(region.intersection<int>(Ray({-5, 5}, {-6, 5})).empty());
    }

    SUBCASE("the ray answers the pair the same way round") {
        const Ray r({-5, 5}, {-4, 5});
        CHECK(r.intersection<int>(region) == region.intersection<int>(r));
    }
}

TEST_CASE("PolygonWithHoles intersection with a Line: fractional crossings") {
    using Rat = pgl::Rational<int64_t>;
    using RatPoint = pgl::Point<Rat>;
    using RatSegment = pgl::Segment<RatPoint>;
    using RatPiece = std::variant<RatPoint, RatSegment>;

    const Region region = annulus();

    // y = 1 + x/2 enters the hole at (4,3) and leaves it at (7,9/2).
    const auto pieces = region.intersection<Rat>(Line({0, 1}, {2, 2}));

    REQUIRE(pieces.size() == 2);
    CHECK(pieces[0] == RatPiece(RatSegment({Rat(0), Rat(1)}, {Rat(4), Rat(3)})));
    CHECK(pieces[1] == RatPiece(RatSegment({Rat(7), Rat(9, 2)}, {Rat(10), Rat(6)})));
}
