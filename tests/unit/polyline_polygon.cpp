#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <vector>

using Point = pgl::Point<int>;
// Aliased as `PGon`/`PLine` (not `Polygon`/`Polyline`) because doctest pulls
// in <windows.h> on MSVC, whose GDI `Polygon`/`Polyline` functions collide
// with file-scope `using Polygon`/`using Polyline`.
using PGon = pgl::Polygon<Point>;
using PLine = pgl::Polyline<Point>;

// Square [0,6] x [0,6].
static const PGon square(std::vector<Point>{
    Point(0, 0), Point(6, 0), Point(6, 6), Point(0, 6)});
// U-shaped polygon: the notch [2,4] x (2,6] is carved out of the square.
static const PGon ushape(std::vector<Point>{
    Point(0, 0), Point(6, 0), Point(6, 6), Point(4, 6),
    Point(4, 2), Point(2, 2), Point(2, 6), Point(0, 6)});
// zig-zag: (1,1) - (3,3) - (5,1), inside the square
static const PLine zig({1, 1, 3, 3, 5, 1});

TEST_CASE("Polygon and Polyline containment and intersection") {
    CHECK(square.intersects(zig));
    CHECK(zig.intersects(square));
    CHECK(square.contains(zig));
    CHECK(square.interiorContains(zig));
    CHECK_FALSE(square.boundaryContains(zig));
    CHECK_FALSE(zig.contains(square));
    CHECK_FALSE(zig.interiorContains(square));
    CHECK_FALSE(zig.boundaryContains(square));
    CHECK(square.interiorsIntersect(zig));
    CHECK(zig.interiorsIntersect(square));

    SUBCASE("the notch is outside the U-shape") {
        CHECK_FALSE(ushape.contains(zig));   // the apex (3,3) pokes into the notch
        CHECK(ushape.intersects(zig));
        const PLine inNotch({3, 4, 3, 5});
        CHECK_FALSE(ushape.intersects(inNotch));
    }

    SUBCASE("a polyline along the boundary is boundary-contained") {
        const PLine rim({0, 0, 6, 0, 6, 6});
        CHECK(square.boundaryContains(rim));
        CHECK(square.contains(rim));
        CHECK_FALSE(square.interiorContains(rim));
        CHECK_FALSE(rim.interiorsIntersect(square));
        // Around the notch: still on the U-shape's boundary.
        const PLine notchRim({2, 6, 2, 2, 4, 2, 4, 6});
        CHECK(ushape.boundaryContains(notchRim));
    }

    SUBCASE("a polygon on a polyline must be degenerate") {
        // A "flat" polygon cannot be built without degeneracy, so only the
        // trivial containment direction is checked here.
        CHECK_FALSE(zig.contains(square));
    }
}

TEST_CASE("Polygon separates a polyline") {
    // The square removes the middle of a segment passing over it.
    CHECK(square.separates(PLine({-1, 3, 7, 3})));
    CHECK_FALSE(square.separates(zig));                 // swallowed
    CHECK_FALSE(square.separates(PLine({3, 3, 9, 3}))); // clips one end

    SUBCASE("a reflex polygon bites two intervals out of one segment") {
        // y = 4 passes through both arms of the U but not through the notch.
        const PLine through({-1, 4, 7, 4});
        CHECK(ushape.separates(through));
        // The freed middle piece lies in the notch: three components total.
        // A segment at y = 1 crosses the solid base: only two end pieces.
        CHECK(ushape.separates(PLine({-1, 1, 7, 1})));
    }

    SUBCASE("set semantics: a loop reconnects around one bite") {
        // A loop around the square's corner (0,0), partly swallowed by it.
        const PLine loop({-2, 0, 0, -2, 2, 0, 0, 2, -2, 0});
        CHECK_FALSE(square.separates(loop));
    }
}

TEST_CASE("Polyline separates a polygon") {
    SUBCASE("a chord cuts") {
        CHECK(PLine({-1, 3, 7, 3}).separates(square));
        CHECK(PLine({-1, 3, 7, 3}).crosses(square));
    }

    SUBCASE("slits do not cut") {
        CHECK_FALSE(zig.separates(square));               // strictly inside
        CHECK_FALSE(PLine({-1, 3, 3, 3}).separates(square));
    }

    SUBCASE("a loop strictly inside seals a pocket") {
        const PLine loop({1, 1, 5, 1, 5, 5, 1, 5, 1, 1});
        CHECK(loop.separates(square));
        CHECK_FALSE(loop.crosses(square));
    }

    SUBCASE("cutting one arm of the U disconnects it") {
        const PLine armCut({-1, 4, 3, 4});   // severs the left arm, ends in the notch
        CHECK(armCut.separates(ushape));
    }

    SUBCASE("a bridge across the notch mouth does not cut") {
        // From arm to arm along y = 6, above the notch: it only touches the
        // boundary at its endpoints and never enters the interior.
        const PLine bridge({2, 6, 4, 6});
        CHECK_FALSE(bridge.separates(ushape));
    }

    SUBCASE("a crosscut hugging the notch does cut") {
        // Enters the left arm at (0,4), crosses to the notch wall, around the
        // notch inside the base, and out the right arm: the polygon splits
        // into the part above and the part below the cut.
        const PLine around({-1, 4, 1, 4, 1, 1, 5, 1, 5, 4, 7, 4});
        CHECK(around.separates(ushape));
    }
}

TEST_CASE("Polygon and Polyline distance") {
    using Rational = pgl::Rational<int>;

    const PLine away({8, 0, 9, 2});
    CHECK(square.squaredDistance<Rational>(away) == Rational(4));
    CHECK(away.squaredDistance<Rational>(square) == Rational(4));
    CHECK(square.distanceL1<Rational>(away) == Rational(2));
    CHECK(square.distanceLInf<Rational>(away) == Rational(2));
    CHECK(square.squaredDistance<Rational>(zig) == Rational(0));

    // The notch is outside the U-shape, one unit from either arm.
    const PLine inNotch({3, 4, 3, 5});
    CHECK(ushape.squaredDistance<Rational>(inNotch) == Rational(1));
    CHECK(inNotch.squaredDistance<Rational>(ushape) == Rational(1));
}

TEST_CASE("Polyline and Polygon intersection clips each edge and coalesces") {
    using Segment = pgl::Segment<Point>;

    SUBCASE("a contained polyline returns its own edges") {
        const auto pieces = zig.intersection(square);
        REQUIRE(pieces.size() == 2);
        REQUIRE(std::holds_alternative<Segment>(pieces[0]));
        CHECK(std::get<Segment>(pieces[0]) == Segment(Point(1, 1), Point(3, 3)));
        REQUIRE(std::holds_alternative<Segment>(pieces[1]));
        CHECK(std::get<Segment>(pieces[1]) == Segment(Point(3, 3), Point(5, 1)));
    }

    SUBCASE("a single edge crossing the notch splits into two segments") {
        // Horizontal line at y = 4 meets the U-shape in its two arms only.
        const auto pieces = PLine({-1, 4, 7, 4}).intersection(ushape);
        REQUIRE(pieces.size() == 2);
        REQUIRE(std::holds_alternative<Segment>(pieces[0]));
        CHECK(std::get<Segment>(pieces[0]) == Segment(Point(0, 4), Point(2, 4)));
        REQUIRE(std::holds_alternative<Segment>(pieces[1]));
        CHECK(std::get<Segment>(pieces[1]) == Segment(Point(4, 4), Point(6, 4)));
    }

    SUBCASE("a polyline missing the polygon has no intersection") {
        CHECK(PLine({7, 7, 9, 9}).intersection(square).empty());
    }
}
