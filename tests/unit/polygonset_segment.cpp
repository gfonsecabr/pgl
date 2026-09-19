#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <variant>
#include <vector>

using Point = pgl::Point<int>;
using PolygonShape = pgl::Polygon<Point>;
using Region = pgl::PolygonWithHoles<Point>;
using RegionSet = pgl::PolygonSet<Point>;
using SegmentShape = pgl::Segment<Point>;

// Two unit squares meeting corner to corner at the origin. This is the witness
// that a one-dimensional operand need not lie in any single component: the
// segment from (-1,-1) to (1,1) is in the set and in neither square.
static RegionSet cornerToCorner() {
    return RegionSet(std::vector{Region(PolygonShape({-1, -1, 0, -1, 0, 0, -1, 0})),
                                 Region(PolygonShape({0, 0, 1, 0, 1, 1, 0, 1}))});
}

// The same two squares turned so that their shared corner sits in the middle of
// a straight run of both boundaries.
static RegionSet cornerBoundary() {
    return RegionSet(std::vector{Region(PolygonShape({-1, 0, 0, 0, 0, 1, -1, 1})),
                                 Region(PolygonShape({0, -1, 1, -1, 1, 0, 0, 0}))});
}

static RegionSet apart() {
    return RegionSet(std::vector{Region(PolygonShape({0, 0, 2, 0, 2, 2, 0, 2})),
                                 Region(PolygonShape({5, 5, 7, 5, 7, 7, 5, 7}))});
}

TEST_CASE("PolygonSet and Segment containment across a pinch") {
    const RegionSet set = cornerToCorner();
    REQUIRE(set.isPinched());

    SUBCASE("a segment may be shared between two components") {
        const SegmentShape diagonal(Point(-1, -1), Point(1, 1));
        CHECK(set.contains(diagonal));
        CHECK(!set.component(0).contains(diagonal));
        CHECK(!set.component(1).contains(diagonal));
    }

    SUBCASE("but only over the points the two components actually cover") {
        CHECK(!set.contains(SegmentShape(Point(-1, -1), Point(2, 2))));
        CHECK(!set.contains(SegmentShape(Point(-1, 1), Point(1, 1))));
        // The x-axis, on the other hand, is the first square's top edge and the
        // second's bottom edge laid end to end, so it is covered whole.
        CHECK(set.contains(SegmentShape(Point(-1, 0), Point(1, 0))));
    }

    SUBCASE("the interior does not pinch through: it is a union of open sets") {
        // The diagonal passes through the origin, which is on both components'
        // boundaries and in neither's interior.
        CHECK(!set.interiorContains(SegmentShape(Point(-1, -1), Point(1, 1))));
        // A segment strictly inside one component is inside the set's interior.
        const SegmentShape inside(Point(-1, -1), Point(0, 0));
        CHECK(set.contains(inside));
        CHECK(!set.interiorContains(inside));  // the endpoints are on the boundary
        CHECK(set.interiorContainsInterior(inside));
        CHECK(!set.interiorContainsInterior(SegmentShape(Point(-1, -1), Point(1, 1))));
    }

    SUBCASE("a boundary run may also cross a pinch") {
        const RegionSet boundarySet = cornerBoundary();
        const SegmentShape axis(Point(-1, 0), Point(1, 0));
        CHECK(boundarySet.boundaryContains(axis));
        CHECK(boundarySet.contains(axis));
        CHECK(!boundarySet.component(0).boundaryContains(axis));
        CHECK(!boundarySet.component(1).boundaryContains(axis));
    }
}

TEST_CASE("PolygonSet and Segment containment without a pinch") {
    const RegionSet set = apart();
    REQUIRE(!set.isPinched());

    SUBCASE("components that never touch fold componentwise") {
        CHECK(set.contains(SegmentShape(Point(0, 0), Point(2, 2))));
        CHECK(set.contains(SegmentShape(Point(5, 5), Point(7, 7))));
        // Between the two components there is nothing at all.
        CHECK(!set.contains(SegmentShape(Point(1, 1), Point(6, 6))));
    }

    SUBCASE("interior containment") {
        CHECK(set.interiorContains(SegmentShape(Point(1, 1), Point(1, 1))));
        CHECK(!set.interiorContains(SegmentShape(Point(0, 0), Point(2, 2))));
        CHECK(set.interiorContainsInterior(SegmentShape(Point(0, 0), Point(2, 2))));
        CHECK(!set.interiorContainsInterior(SegmentShape(Point(1, 1), Point(6, 6))));
    }

    SUBCASE("boundary containment") {
        CHECK(set.boundaryContains(SegmentShape(Point(0, 0), Point(2, 0))));
        CHECK(!set.boundaryContains(SegmentShape(Point(0, 1), Point(2, 1))));
    }
}

TEST_CASE("PolygonSet and Segment intersection predicates") {
    const RegionSet set = apart();

    SUBCASE("a segment meets the set when it meets a component") {
        CHECK(set.intersects(SegmentShape(Point(1, 1), Point(6, 6))));
        CHECK(!set.intersects(SegmentShape(Point(3, 3), Point(4, 4))));
    }

    SUBCASE("interiors") {
        CHECK(set.interiorsIntersect(SegmentShape(Point(1, 1), Point(6, 6))));
        CHECK(!set.interiorsIntersect(SegmentShape(Point(0, 0), Point(0, 2))));
    }

    SUBCASE("the pair answers the same either way round") {
        CHECK(SegmentShape(Point(1, 1), Point(6, 6)).intersects(set));
        CHECK(SegmentShape(Point(1, 1), Point(6, 6)).interiorsIntersect(set));
    }

    SUBCASE("an oriented segment answers as its underlying segment") {
        const pgl::OrientedSegment<Point> oriented(Point(6, 6), Point(1, 1));
        CHECK(set.intersects(oriented));
        CHECK(!set.contains(oriented));
    }
}

TEST_CASE("PolygonSet and Segment cut predicates") {
    SUBCASE("a set cuts a segment that runs through both components") {
        const RegionSet set = apart();
        // The segment enters the first square, comes out into the gap, and
        // enters the second: what is left of it is one middle piece plus
        // nothing, so it is not cut.
        CHECK(!set.separates(SegmentShape(Point(1, 1), Point(6, 6))));
        // Running right across one component leaves two free stubs.
        CHECK(set.separates(SegmentShape(Point(-1, 1), Point(3, 1))));
    }

    SUBCASE("a segment cuts a set that was already in two pieces") {
        CHECK(SegmentShape(Point(100, 100), Point(101, 101)).separates(apart()));
    }

    SUBCASE("crosses is mutual separation") {
        const RegionSet set = apart();
        const SegmentShape across(Point(-1, 1), Point(3, 1));
        CHECK(set.separates(across));
        CHECK(across.separates(set) == set.crosses(across));
    }
}

TEST_CASE("PolygonSet and Segment distances") {
    const RegionSet set = apart();
    CHECK(set.squaredDistance<double>(SegmentShape(Point(3, 3), Point(4, 4))) ==
          doctest::Approx(2.0));
    CHECK(set.squaredDistance<double>(SegmentShape(Point(1, 1), Point(6, 6))) ==
          doctest::Approx(0.0));
    CHECK(set.distanceLInf<double>(SegmentShape(Point(3, 3), Point(3, 3))) ==
          doctest::Approx(1.0));
}

TEST_CASE("PolygonSet and chain containment") {
    const RegionSet set = cornerToCorner();

    SUBCASE("a chain is contained when every edge is") {
        const pgl::Polyline<Point> across({-1, -1, 0, 0, 1, 1});
        CHECK(set.contains(across));
        const pgl::Polyline<Point> escaping({-1, -1, 0, 0, 2, 2});
        CHECK(!set.contains(escaping));
    }

    SUBCASE("an empty chain is contained and a single vertex is a point") {
        CHECK(set.contains(pgl::Polyline<Point>()));
        CHECK(set.contains(pgl::Polyline<Point>({0, 0})));
        CHECK(!set.contains(pgl::Polyline<Point>({5, 5})));
    }

    SUBCASE("a monotone chain answers the same way") {
        const pgl::MonotoneChain<Point> chain({-1, -1, 0, 0, 1, 1});
        CHECK(set.contains(chain));
        CHECK(set.intersects(chain));
    }
}

// A 10x10 square with a 6x6 hole, and a 2x2 island stored beside it in the
// middle of that hole: a point of the island is enclosed by three rings.
static RegionSet islandInHole() {
    return RegionSet(std::vector{
        Region(PolygonShape({0, 0, 10, 0, 10, 10, 0, 10}),
               std::vector{PolygonShape({2, 2, 8, 2, 8, 8, 2, 8})}),
        Region(PolygonShape({4, 4, 6, 4, 6, 6, 4, 6}))});
}

TEST_CASE("PolygonSet intersection with a Segment") {
    using Piece = std::variant<Point, SegmentShape>;

    SUBCASE("the island inside a hole is part of the set") {
        const RegionSet set = islandInHole();
        const auto pieces = set.intersection<int>(SegmentShape(Point(-1, 5), Point(5, 5)));

        REQUIRE(pieces.size() == 2);
        CHECK(pieces[0] == Piece(SegmentShape({0, 5}, {2, 5})));
        CHECK(pieces[1] == Piece(SegmentShape({4, 5}, {5, 5})));
    }

    SUBCASE("a segment through a pinch comes back as one piece") {
        const RegionSet set = cornerToCorner();
        const auto pieces = set.intersection<int>(SegmentShape(Point(-2, -2), Point(2, 2)));

        REQUIRE(pieces.size() == 1);
        CHECK(pieces[0] == Piece(SegmentShape({-1, -1}, {1, 1})));
    }

    SUBCASE("a segment meeting both components only at the pinch is a point") {
        const RegionSet set = cornerToCorner();
        const auto pieces = set.intersection<int>(SegmentShape(Point(-1, 1), Point(1, -1)));

        REQUIRE(pieces.size() == 1);
        CHECK(pieces[0] == Piece(Point(0, 0)));
    }

    SUBCASE("a segment in the hole around the island meets nothing") {
        CHECK(islandInHole().intersection<int>(SegmentShape(Point(3, 3), Point(3, 7))).empty());
        CHECK(RegionSet().intersection<int>(SegmentShape(Point(0, 0), Point(1, 1))).empty());
    }

    SUBCASE("a degenerate segment is its point") {
        const auto pieces = islandInHole().intersection<int>(SegmentShape(Point(5, 5), Point(5, 5)));

        REQUIRE(pieces.size() == 1);
        CHECK(pieces[0] == Piece(Point(5, 5)));
    }

    SUBCASE("the segment answers the pair the same way round") {
        const RegionSet set = islandInHole();
        const SegmentShape s(Point(-1, 5), Point(5, 5));
        CHECK(s.intersection<int>(set) == set.intersection<int>(s));
    }

    SUBCASE("an oriented segment answers as its segment, either way round") {
        const RegionSet set = islandInHole();
        const SegmentShape s(Point(-1, 5), Point(5, 5));
        for (const auto& o : {pgl::OrientedSegment<Point>(Point(-1, 5), Point(5, 5)),
                              pgl::OrientedSegment<Point>(Point(5, 5), Point(-1, 5))}) {
            CHECK(set.intersection<int>(o) == set.intersection<int>(s));
            CHECK(o.intersection<int>(set) == set.intersection<int>(s));
        }
    }
}

TEST_CASE("PolygonSet intersection with a chain") {
    using Piece = std::variant<Point, SegmentShape>;
    const RegionSet set = islandInHole();

    SUBCASE("a polyline keeps its runs through the frame and the island") {
        const pgl::Polyline<Point> line({-1, 5, 5, 5, 5, 11});
        const auto pieces = set.intersection<int>(line);

        REQUIRE(pieces.size() == 4);
        CHECK(pieces[0] == Piece(SegmentShape({0, 5}, {2, 5})));
        CHECK(pieces[1] == Piece(SegmentShape({4, 5}, {5, 5})));
        CHECK(pieces[2] == Piece(SegmentShape({5, 5}, {5, 6})));
        CHECK(pieces[3] == Piece(SegmentShape({5, 8}, {5, 10})));
        CHECK(line.intersection<int>(set) == pieces);
    }

    SUBCASE("a polyline crossing a pinch coalesces its collinear runs") {
        const pgl::Polyline<Point> line({-2, -2, 0, 0, 2, 2});
        const auto pieces = cornerToCorner().intersection<int>(line);

        REQUIRE(pieces.size() == 1);
        CHECK(pieces[0] == Piece(SegmentShape({-1, -1}, {1, 1})));
    }

    SUBCASE("a monotone chain agrees with the polyline it views itself as") {
        const pgl::MonotoneChain<Point> chain({-1, 5, 5, 5, 11, 11});
        CHECK(set.intersection<int>(chain) == set.intersection<int>(chain.asPolyline()));
        CHECK(chain.intersection<int>(set) == set.intersection<int>(chain));
    }

    SUBCASE("an empty or single-vertex chain") {
        CHECK(set.intersection<int>(pgl::Polyline<Point>()).empty());
        const auto pieces = set.intersection<int>(pgl::Polyline<Point>({5, 5}));
        REQUIRE(pieces.size() == 1);
        CHECK(pieces[0] == Piece(Point(5, 5)));
    }
}

TEST_CASE("PolygonSet intersection through the runtime Shape") {
    using ShapeType = pgl::Shape<Point>;
    const ShapeType set(islandInHole());

    CHECK(pgl::pieces(set.intersection<int>(ShapeType(SegmentShape(Point(-1, 5), Point(5, 5))))).size() == 2);
    CHECK(pgl::pieces(ShapeType(SegmentShape(Point(-1, 5), Point(5, 5))).intersection<int>(set)).size() == 2);
    CHECK(pgl::pieces(set.intersection<int>(ShapeType(pgl::Polyline<Point>({-1, 5, 5, 5, 5, 11})))).size() == 4);
    CHECK(pgl::pieces(ShapeType(Point(5, 5)).intersection<int>(set)).size() == 1);
    CHECK(pgl::pieces(ShapeType(pgl::Line<Point>(Point(0, 5), Point(1, 5))).intersection<int>(set)).size() == 3);
}
