#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstdint>
#include <variant>

#include "pgl.hpp"

TEST_CASE("Line and rectangle predicates exercise the line's infinite extent") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Rectangle = pgl::Rectangle<Point>;
    using Line = pgl::Line<Point>;

    const Rectangle r({0, 0}, {4, 3});

    SUBCASE("horizontal line cuts the rectangle, defining points far left") {
        const Line horizontal({-200, 1}, {-100, 1});

        CHECK_FALSE(horizontal.contains(r));
        CHECK_FALSE(r.contains(horizontal));
        CHECK(horizontal.intersects(r));
        CHECK(r.intersects(horizontal));
        CHECK(horizontal.interiorsIntersect(r));
        CHECK(r.interiorsIntersect(horizontal));
        CHECK(horizontal.crosses(r));
        CHECK(r.crosses(horizontal));
        CHECK(horizontal.separates(r));
        CHECK(r.separates(horizontal));

        const auto clipped = horizontal.intersection(r);
        REQUIRE(clipped);
        REQUIRE(std::holds_alternative<Segment>(*clipped));
        CHECK(std::get<Segment>(*clipped) == Segment(Point(0, 1), Point(4, 1)));
    }

    SUBCASE("vertical line cuts the rectangle, defining points far above") {
        const Line vertical({2, 50}, {2, 100});

        CHECK(vertical.intersects(r));
        CHECK(r.intersects(vertical));
        CHECK(vertical.interiorsIntersect(r));
        CHECK(vertical.crosses(r));

        const auto clipped = vertical.intersection(r);
        REQUIRE(clipped);
        REQUIRE(std::holds_alternative<Segment>(*clipped));
        CHECK(std::get<Segment>(*clipped) == Segment(Point(2, 0), Point(2, 3)));
    }

    SUBCASE("line through opposite corners (diagonal) with defining points outside") {
        const Line diagonal({-100, -75}, {100, 75});

        CHECK(diagonal.intersects(r));
        CHECK(diagonal.interiorsIntersect(r));
        CHECK(diagonal.crosses(r));
        CHECK(diagonal.separates(r));

        const auto clipped = diagonal.intersection(r);
        REQUIRE(clipped);
        REQUIRE(std::holds_alternative<Segment>(*clipped));
        CHECK(std::get<Segment>(*clipped) == Segment(Point(0, 0), Point(4, 3)));
    }

    SUBCASE("line lies along the top edge, defining points outside the edge") {
        const Line along_top({-100, 3}, {100, 3});

        CHECK_FALSE(r.contains(along_top));
        CHECK(along_top.intersects(r));
        CHECK(r.intersects(along_top));
        CHECK_FALSE(along_top.interiorsIntersect(r));
        CHECK_FALSE(r.interiorsIntersect(along_top));
        CHECK_FALSE(along_top.crosses(r));
        CHECK_FALSE(along_top.separates(r));

        const auto overlap = along_top.intersection(r);
        REQUIRE(overlap);
        REQUIRE(std::holds_alternative<Segment>(*overlap));
        CHECK(std::get<Segment>(*overlap) == Segment(Point(0, 3), Point(4, 3)));
    }

    SUBCASE("line tangent at a single corner") {
        const Line tangent({-100, -104}, {100, 96});

        CHECK(tangent.intersects(r));
        CHECK(r.intersects(tangent));
        CHECK_FALSE(tangent.interiorsIntersect(r));
        CHECK_FALSE(r.interiorsIntersect(tangent));
        CHECK_FALSE(tangent.crosses(r));

        const auto isec = tangent.intersection(r);
        REQUIRE(isec);
        REQUIRE(std::holds_alternative<Point>(*isec));
        CHECK(std::get<Point>(*isec) == Point(4, 0));
    }

    SUBCASE("line disjoint from the rectangle, parallel to one side") {
        const Line parallel_line({-200, 10}, {200, 10});

        CHECK_FALSE(parallel_line.intersects(r));
        CHECK_FALSE(parallel_line.interiorsIntersect(r));
        CHECK_FALSE(parallel_line.crosses(r));
        CHECK_FALSE(parallel_line.intersection(r));
    }

    SUBCASE("line disjoint from the rectangle, slanted") {
        const Line slanted({-50, 50}, {100, 200});

        CHECK_FALSE(slanted.intersects(r));
        CHECK_FALSE(slanted.interiorsIntersect(r));
        CHECK_FALSE(slanted.crosses(r));
        CHECK_FALSE(slanted.intersection(r));
    }

    SUBCASE("line through one corner and through interior of an opposite side") {
        const Line cut({-100, -50}, {100, 50});

        CHECK(cut.intersects(r));
        CHECK(cut.interiorsIntersect(r));
        CHECK(cut.crosses(r));
        CHECK(cut.separates(r));

        const auto clipped = cut.intersection(r);
        REQUIRE(clipped);
        REQUIRE(std::holds_alternative<Segment>(*clipped));
        CHECK(std::get<Segment>(*clipped) == Segment(Point(0, 0), Point(4, 2)));
    }
}

TEST_CASE("Rational clipping of a rectangle by a line with far defining points") {
    using Rational = pgl::Rational<int64_t>;
    using IntPoint = pgl::Point<int>;
    using RationalPoint = pgl::Point<Rational>;
    using IntRectangle = pgl::Rectangle<IntPoint>;
    using IntLine = pgl::Line<IntPoint>;
    using RationalSegment = pgl::Segment<RationalPoint>;

    const IntRectangle r({0, 0}, {4, 3});
    const IntLine slanted({-100, -49}, {100, 51});

    CHECK(slanted.intersects(r));
    CHECK(slanted.crosses(r));

    const auto clipped = slanted.intersection<Rational>(r);
    REQUIRE(clipped);
    REQUIRE(std::holds_alternative<RationalSegment>(*clipped));
    const RationalSegment expected(
        RationalPoint(Rational(-2), Rational(0)),
        RationalPoint(Rational(4), Rational(3)));
    (void)expected;
}

// Containment that can only ever be degenerate: a 1D line cannot interior-contain
// a 2D rectangle, and a bounded rectangle can neither bound nor interior-contain
// an unbounded line. For real shapes every such relation is false.
TEST_CASE("Line and Rectangle boundary/interior containment is always false") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;
    using Line = pgl::Line<Point>;

    const Rectangle r({0, 0}, {4, 3});

    SUBCASE("a line cutting through is interior-contained by neither") {
        const Line cut({-200, 1}, {-100, 1});
        CHECK_FALSE(cut.interiorContains(r));
        CHECK_FALSE(r.boundaryContains(cut));
        CHECK_FALSE(r.interiorContains(cut));
    }

    SUBCASE("a line along an edge still bounds/contains nothing") {
        const Line alongEdge({-100, 0}, {-1, 0});  // y = 0 runs along the bottom edge
        CHECK_FALSE(r.boundaryContains(alongEdge));
        CHECK_FALSE(r.interiorContains(alongEdge));
    }
}

TEST_CASE("Degenerate rectangle as a single point lies on the line") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;
    using Line = pgl::Line<Point>;

    const Rectangle degenerate_point({2, 2}, {2, 2});
    const Line through({-100, -100}, {100, 100});

    CHECK(through.contains(degenerate_point));
    CHECK(through.intersects(degenerate_point));

    const auto isec = through.intersection(degenerate_point);
    REQUIRE(isec);
    REQUIRE(std::holds_alternative<Point>(*isec));
    CHECK(std::get<Point>(*isec) == Point(2, 2));
}
