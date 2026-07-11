#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

using Point = pgl::Point<int>;
// These aliases avoid Win32 GDI functions pulled in by doctest on MSVC.
using Box = pgl::Rectangle<Point>;
using PLine = pgl::Polyline<Point>;

// zig-zag: (0,0) - (2,2) - (4,0)
static const PLine zig({0, 0, 2, 2, 4, 0});
static const Box box(Point(0, 0), Point(4, 4));

TEST_CASE("Polyline and Rectangle containment and intersection") {
    CHECK(zig.intersects(box));
    CHECK(box.intersects(zig));
    CHECK(box.contains(zig));
    CHECK_FALSE(box.interiorContains(zig));   // the extremes sit on the boundary
    CHECK(Box(Point(-1, -1), Point(5, 5)).interiorContains(zig));
    CHECK_FALSE(zig.contains(box));           // a 1D set holds no 2D rectangle
    CHECK_FALSE(zig.interiorContains(box));
    CHECK_FALSE(zig.boundaryContains(box));

    CHECK(zig.interiorsIntersect(box));
    // The zig-zag only grazes the boundary of a box hanging below it.
    const Box below(Point(0, -4), Point(4, 0));
    CHECK(zig.intersects(below));
    CHECK_FALSE(zig.interiorsIntersect(below));

    const Box away(Point(6, 0), Point(8, 2));
    CHECK_FALSE(zig.intersects(away));

    SUBCASE("a polyline along the boundary is boundary-contained") {
        const PLine rim({0, 0, 4, 0, 4, 4});
        CHECK(box.boundaryContains(rim));
        CHECK_FALSE(box.boundaryContains(zig));
        CHECK(box.contains(rim));
        CHECK_FALSE(box.interiorContains(rim));
    }
}

TEST_CASE("Rectangle separates a polyline") {
    // A box over the apex removes the middle of the zig-zag.
    const Box apexBox(Point(1, 1), Point(3, 3));
    CHECK(apexBox.separates(zig));
    CHECK_FALSE(box.separates(zig));   // the big box swallows everything

    SUBCASE("set semantics: the loop reconnects around a clipped corner") {
        const PLine loop({0, 0, 4, 0, 4, 4, 0, 4, 0, 0});
        const Box cornerBox(Point(-1, -1), Point(1, 1));
        CHECK_FALSE(cornerBox.separates(loop));       // one bite, arc survives
        const Box sideBox(Point(1, -1), Point(3, 1));
        CHECK(sideBox.separates(loop) == false);      // bottom bite only
        const Box throughBox(Point(1, -1), Point(3, 5));
        CHECK(throughBox.separates(loop));            // bites top and bottom
    }
}

TEST_CASE("Polyline separates a rectangle") {
    SUBCASE("a chord through the box") {
        CHECK(PLine({-1, 1, 5, 1}).separates(box));
        CHECK(PLine({-1, 1, 5, 1}).crosses(box));
    }

    SUBCASE("the zig-zag crosscut") {
        // Enters at (0,0), runs through the interior, exits at (4,0): the
        // pocket under the tent is sealed against the bottom edge.
        CHECK(zig.separates(box));
    }

    SUBCASE("a slit does not cut") {
        CHECK_FALSE(PLine({-1, 2, 2, 2}).separates(box));
        CHECK_FALSE(PLine({1, 1, 3, 3}).separates(box));   // strictly inside
    }

    SUBCASE("a loop strictly inside the box seals a pocket") {
        const PLine loop({1, 1, 3, 1, 3, 3, 1, 3, 1, 1});
        CHECK(loop.separates(box));
        CHECK_FALSE(loop.crosses(box));   // the box does not disconnect the loop
    }

    SUBCASE("a self-crossing bow-tie inside the box seals pockets") {
        const PLine bowtie({0, 0, 4, 4, 4, 0, 0, 4, 0, 0});
        CHECK(bowtie.separates(box));
    }

    SUBCASE("an open V strictly inside does not cut") {
        CHECK_FALSE(PLine({1, 3, 2, 1, 3, 3}).separates(box));
    }

    SUBCASE("a run along the boundary does not cut") {
        CHECK_FALSE(PLine({0, 0, 4, 0}).separates(box));
    }
}

TEST_CASE("Polyline and Rectangle distance") {
    using Rational = pgl::Rational<int>;

    const Box away(Point(6, 0), Point(8, 2));
    CHECK(zig.squaredDistance<Rational>(away) == Rational(4));
    CHECK(away.squaredDistance<Rational>(zig) == Rational(4));
    CHECK(zig.distanceL1<Rational>(away) == Rational(2));
    CHECK(zig.distanceLInf<Rational>(away) == Rational(2));
    CHECK(zig.squaredDistance<Rational>(box) == Rational(0));

    // Nearest pair: the point (3,1) on the second edge and the corner (6,4).
    const Box diagonal(Point(6, 4), Point(8, 6));
    CHECK(zig.squaredDistance<double>(diagonal) == doctest::Approx(18.0));
    CHECK(zig.distanceL1<Rational>(diagonal) == Rational(6));
    CHECK(zig.distanceLInf<Rational>(diagonal) == Rational(3));
}

TEST_CASE("Polyline and Rectangle intersection pieces") {
    using Segment = pgl::Segment<Point>;

    SUBCASE("a containing box returns the edges themselves") {
        const auto pieces = zig.intersection(box);
        REQUIRE(pieces.size() == 2);
        REQUIRE(std::holds_alternative<Segment>(pieces[0]));
        CHECK(std::get<Segment>(pieces[0]) == Segment(Point(0, 0), Point(2, 2)));
        REQUIRE(std::holds_alternative<Segment>(pieces[1]));
        CHECK(std::get<Segment>(pieces[1]) == Segment(Point(2, 2), Point(4, 0)));
    }

    SUBCASE("a clipping box truncates the edge") {
        const auto pieces = zig.intersection(Box(Point(0, 0), Point(2, 1)));
        REQUIRE(pieces.size() == 1);
        REQUIRE(std::holds_alternative<Segment>(pieces[0]));
        CHECK(std::get<Segment>(pieces[0]) == Segment(Point(0, 0), Point(1, 1)));
    }

    SUBCASE("a disjoint box") {
        CHECK(zig.intersection(Box(Point(5, 5), Point(6, 6))).empty());
    }
}
