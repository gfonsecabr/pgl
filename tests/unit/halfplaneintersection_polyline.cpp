#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <variant>
#include <vector>

using Point = pgl::Point<int>;
using Halfplane = pgl::Halfplane<Point>;
using Region = pgl::HalfplaneIntersection<Point>;
// Named PolylineShape, not Polyline: under MSVC, <windows.h> (pulled in
// transitively by doctest.h) injects a Win32 GDI function called `Polyline`
// into the global namespace, and an alias of the same name used from
// TEST_CASE bodies (global scope) resolves ambiguously against it.
using PolylineShape = pgl::Polyline<Point>;

namespace {
Region box6() {
    return Region({Halfplane(0, 0, 1, 0), Halfplane(6, 0, 6, 1),
                   Halfplane(6, 6, 5, 6), Halfplane(0, 6, 0, 5)});
}
}  // namespace

TEST_CASE("Region contains a polyline") {
    const Region k = box6();
    const PolylineShape inside(std::vector<Point>{{1, 1}, {3, 5}, {5, 1}});
    CHECK(k.contains(inside));
    const PolylineShape poking(std::vector<Point>{{1, 1}, {3, 8}, {5, 1}});
    CHECK(!k.contains(poking));
}

TEST_CASE("Region intersects a polyline") {
    const Region k = box6();
    const PolylineShape crossing(std::vector<Point>{{3, 3}, {5, 7}, {9, 3}});
    CHECK(k.intersects(crossing));
    CHECK(k.interiorsIntersect(crossing));
    const PolylineShape away(std::vector<Point>{{7, 7}, {8, 9}, {9, 7}});
    CHECK(!k.intersects(away));
}

TEST_CASE("Region separates a polyline") {
    const Region k = box6();
    // A polyline entering and leaving the box through one side, with a bend
    // outside, is cut into two surviving pieces.
    const PolylineShape through(std::vector<Point>{{-2, 3}, {3, 3}, {8, 3}});
    CHECK(k.separates(through));
    const PolylineShape inside(std::vector<Point>{{1, 3}, {3, 3}, {5, 3}});
    CHECK(!k.separates(inside));
    // The other direction: that same polyline is a crosscut of the box, so it
    // does disconnect the region — as it does the equivalent Polygon.
    CHECK(through.separates(k));
    CHECK(k.crosses(through));
    // A polyline that stops inside leaves a slit, which keeps the region in
    // one piece.
    const PolylineShape slit(std::vector<Point>{{-2, 3}, {3, 3}});
    CHECK(!slit.separates(k));
    CHECK(!inside.separates(k));
}

TEST_CASE("Distance to a polyline") {
    const Region k = box6();
    const PolylineShape away(std::vector<Point>{{9, 0}, {10, 6}, {11, 0}});
    CHECK(k.squaredDistance<double>(away) == doctest::Approx(9.0));
    const PolylineShape touching(std::vector<Point>{{3, 3}, {5, 7}, {9, 3}});
    CHECK(k.squaredDistance<double>(touching) == doctest::Approx(0.0));
}

TEST_CASE("An unbounded region answers the cut predicates instead of throwing") {
    // One half-plane: a region with no bounding box at all. Every predicate is
    // documented to answer, so none of them may ask for one.
    const Region halfplane({Halfplane(0, 0, 0, 1)});
    REQUIRE_FALSE(halfplane.isBounded());
    REQUIRE_FALSE(halfplane.empty());

    const PolylineShape point(std::vector<Point>{{0, 0}, {0, 0}});
    CHECK_FALSE(point.separates(halfplane));
    CHECK_FALSE(halfplane.separates(point));
    CHECK_FALSE(point.crosses(halfplane));
    CHECK_FALSE(halfplane.crosses(point));

    // A bounded polyline cannot cut an unbounded region in two: the complement
    // stays connected around its ends, however the polyline sits.
    const Region wedge({Halfplane(0, 1, 1, 0)});
    const PolylineShape open(std::vector<Point>{{0, 0}, {0, 1}, {-1, 0}});
    CHECK_FALSE(open.separates(wedge));
    CHECK_FALSE(open.crosses(wedge));
    // The region does cut that polyline, which is what crosses needs both ways.
    CHECK(wedge.separates(open));
    CHECK_FALSE(wedge.crosses(open));

    // A polyline whose box misses the region entirely still answers.
    const PolylineShape away(std::vector<Point>{{5, 5}, {7, 9}});
    CHECK_FALSE(away.separates(halfplane));
    CHECK_FALSE(halfplane.separates(away));
}

TEST_CASE("Region intersection with a polyline") {
    using Segment = pgl::Segment<Point>;
    using Piece = std::variant<Point, Segment>;

    SUBCASE("each edge is clipped to the box") {
        const Region k = box6();
        const PolylineShape line(std::vector<Point>{{-2, 3}, {3, 3}, {3, 9}});
        const auto pieces = k.intersection<int>(line);

        REQUIRE(pieces.size() == 2);
        CHECK(pieces[0] == Piece(Segment({0, 3}, {3, 3})));
        CHECK(pieces[1] == Piece(Segment({3, 3}, {3, 6})));
        CHECK(line.intersection<int>(k) == pieces);
    }

    SUBCASE("a touch point covered by a run beside it is dropped") {
        // The first edge meets the box only at (0,3), where the second edge's
        // run begins.
        const Region k = box6();
        const PolylineShape line(std::vector<Point>{{-2, 1}, {0, 3}, {3, 6}});
        const auto pieces = k.intersection<int>(line);

        REQUIRE(pieces.size() == 1);
        CHECK(pieces[0] == Piece(Segment({0, 3}, {3, 6})));
    }

    SUBCASE("an unbounded region") {
        const Region halfplane({Halfplane(0, 0, 0, 1)});  // x <= 0
        const PolylineShape line(std::vector<Point>{{-2, -1}, {2, 1}, {2, 5}});
        const auto pieces = halfplane.intersection<int>(line);

        REQUIRE(pieces.size() == 1);
        CHECK(pieces[0] == Piece(Segment({-2, -1}, {0, 0})));
    }

    SUBCASE("a region collapsed to a line keeps the touch points") {
        const Region axis({Halfplane(0, 0, 1, 0), Halfplane(1, 0, 0, 0)});  // y = 0
        const PolylineShape zigzag(std::vector<Point>{{-1, -1}, {1, 1}, {3, -1}});
        const auto pieces = axis.intersection<int>(zigzag);

        REQUIRE(pieces.size() == 2);
        CHECK(pieces[0] == Piece(Point(0, 0)));
        CHECK(pieces[1] == Piece(Point(2, 0)));
    }

    SUBCASE("an empty region meets nothing") {
        const Region none({Halfplane(0, 0, 1, 0), Halfplane(1, -1, 0, -1)});  // y >= 0, y <= -1
        REQUIRE(none.empty());
        CHECK(none.intersection<int>(PolylineShape(std::vector<Point>{{0, 0}, {1, 1}})).empty());
    }

    SUBCASE("fractional crossings") {
        using Rat = pgl::Rational<int64_t>;
        using RatPoint = pgl::Point<Rat>;
        using RatSegment = pgl::Segment<RatPoint>;
        using RatPiece = std::variant<RatPoint, RatSegment>;

        // The edge has slope 7/3: it enters at (0,10/3) and leaves at (8/7,6).
        const auto pieces =
            box6().intersection<Rat>(PolylineShape(std::vector<Point>{{-1, 1}, {2, 8}}));

        REQUIRE(pieces.size() == 1);
        CHECK(pieces[0] == RatPiece(RatSegment({Rat(0), Rat(10, 3)}, {Rat(8, 7), Rat(6)})));
    }

    SUBCASE("through the runtime Shape") {
        using ShapeType = pgl::Shape<Point>;
        const PolylineShape line(std::vector<Point>{{-2, 3}, {3, 3}, {3, 9}});
        CHECK(pgl::pieces(ShapeType(box6()).intersection<int>(ShapeType(line))).size() == 2);
        CHECK(pgl::pieces(ShapeType(line).intersection<int>(ShapeType(box6()))).size() == 2);
    }
}
