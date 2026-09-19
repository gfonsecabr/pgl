#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <variant>
#include <vector>

using Point = pgl::Point<int>;
using Segment = pgl::Segment<Point>;
using Line = pgl::Line<Point>;
using OrientedLine = pgl::OrientedLine<Point>;
using Ray = pgl::Ray<Point>;
using PolygonShape = pgl::Polygon<Point>;
using Region = pgl::PolygonWithHoles<Point>;
using RegionSet = pgl::PolygonSet<Point>;

// A 10x10 square with a 6x6 hole, and a 2x2 island stored beside it in the
// middle of that hole:
//
//     (0,10)                     (10,10)
//        +--------------------------+
//        |  +--------------------+  |
//        |  |       +----+       |  |   hole   = [2,8] x [2,8]
//        |  |       |    |       |  |   island = [4,6] x [4,6]
//        |  |       +----+       |  |
//        |  +--------------------+  |
//        +--------------------------+
//     (0,0)                      (10,0)
static RegionSet islandInHole() {
    return RegionSet(std::vector{
        Region(PolygonShape({0, 0, 10, 0, 10, 10, 0, 10}),
               std::vector{PolygonShape({2, 2, 8, 2, 8, 8, 2, 8})}),
        Region(PolygonShape({4, 4, 6, 4, 6, 6, 4, 6}))});
}

// Two unit squares meeting corner to corner at the origin.
static RegionSet cornerToCorner() {
    return RegionSet(std::vector{Region(PolygonShape({-1, -1, 0, -1, 0, 0, -1, 0})),
                                 Region(PolygonShape({0, 0, 1, 0, 1, 1, 0, 1}))});
}

TEST_CASE("PolygonSet intersection with a Line") {
    using Piece = std::variant<Point, Segment>;

    SUBCASE("a line across the hole keeps the frame and the island") {
        const auto pieces = islandInHole().intersection<int>(Line({0, 5}, {1, 5}));

        REQUIRE(pieces.size() == 3);
        CHECK(pieces[0] == Piece(Segment({0, 5}, {2, 5})));
        CHECK(pieces[1] == Piece(Segment({4, 5}, {6, 5})));
        CHECK(pieces[2] == Piece(Segment({8, 5}, {10, 5})));
    }

    SUBCASE("a line along the island's edge keeps that edge") {
        const auto pieces = islandInHole().intersection<int>(Line({0, 4}, {1, 4}));

        REQUIRE(pieces.size() == 3);
        CHECK(pieces[0] == Piece(Segment({0, 4}, {2, 4})));
        CHECK(pieces[1] == Piece(Segment({4, 4}, {6, 4})));
        CHECK(pieces[2] == Piece(Segment({8, 4}, {10, 4})));
    }

    SUBCASE("a line through a pinch is one piece") {
        const auto pieces = cornerToCorner().intersection<int>(Line({0, 0}, {1, 1}));

        REQUIRE(pieces.size() == 1);
        CHECK(pieces[0] == Piece(Segment({-1, -1}, {1, 1})));
    }

    SUBCASE("a line touching both components only at the pinch is a point") {
        const auto pieces = cornerToCorner().intersection<int>(Line({0, 0}, {1, -1}));

        REQUIRE(pieces.size() == 1);
        CHECK(pieces[0] == Piece(Point(0, 0)));
    }

    SUBCASE("a line missing the set, and the empty set") {
        CHECK(islandInHole().intersection<int>(Line({0, 12}, {1, 12})).empty());
        CHECK(RegionSet().intersection<int>(Line({0, 0}, {1, 0})).empty());
    }

    SUBCASE("the line answers the pair the same way round") {
        const RegionSet set = islandInHole();
        const Line l({0, 5}, {1, 5});
        CHECK(l.intersection<int>(set) == set.intersection<int>(l));
    }
}

TEST_CASE("PolygonSet intersection with an OrientedLine: direction never matters") {
    const RegionSet set = islandInHole();
    const Line l({0, 5}, {1, 5});

    for (const auto& o : {OrientedLine({0, 5}, {1, 5}), OrientedLine({1, 5}, {0, 5})}) {
        CHECK(set.intersection<int>(o) == set.intersection<int>(l));
        CHECK(o.intersection<int>(set) == set.intersection<int>(l));
    }
}

TEST_CASE("PolygonSet intersection with a Ray") {
    using Piece = std::variant<Point, Segment>;
    const RegionSet set = islandInHole();

    SUBCASE("a ray from inside the island is cut at the source") {
        const auto pieces = set.intersection<int>(Ray({5, 5}, {6, 5}));

        REQUIRE(pieces.size() == 2);
        CHECK(pieces[0] == Piece(Segment({5, 5}, {6, 5})));
        CHECK(pieces[1] == Piece(Segment({8, 5}, {10, 5})));
    }

    SUBCASE("the pieces come in order from the source outward") {
        const auto pieces = set.intersection<int>(Ray({7, 5}, {6, 5}));

        REQUIRE(pieces.size() == 2);
        CHECK(pieces[0] == Piece(Segment({4, 5}, {6, 5})));
        CHECK(pieces[1] == Piece(Segment({0, 5}, {2, 5})));
    }

    SUBCASE("a ray pointing away from the set yields nothing") {
        CHECK(set.intersection<int>(Ray({-5, 5}, {-6, 5})).empty());
    }

    SUBCASE("the ray answers the pair the same way round") {
        const Ray r({-5, 5}, {-4, 5});
        CHECK(r.intersection<int>(set) == set.intersection<int>(r));
        CHECK(r.intersection<int>(set).size() == 3);
    }
}

TEST_CASE("PolygonSet intersection with a Line: fractional crossings") {
    using Rat = pgl::Rational<int64_t>;
    using RatPoint = pgl::Point<Rat>;
    using RatSegment = pgl::Segment<RatPoint>;
    using RatPiece = std::variant<RatPoint, RatSegment>;

    // y = 1 + x/4 enters the hole at (4,2), passes under the island, and
    // leaves the hole at (8,3).
    const auto pieces = islandInHole().intersection<Rat>(Line({0, 1}, {4, 2}));

    REQUIRE(pieces.size() == 2);
    CHECK(pieces[0] == RatPiece(RatSegment({Rat(0), Rat(1)}, {Rat(4), Rat(2)})));
    CHECK(pieces[1] == RatPiece(RatSegment({Rat(8), Rat(3)}, {Rat(10), Rat(7, 2)})));
}
