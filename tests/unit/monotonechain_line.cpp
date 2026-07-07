#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

// Reference chain: up to a peak at (2,4), down to (4,0), up the vertical edge
// to (4,4), and down to (6,0).
namespace {
const pgl::MonotoneChain<pgl::Point<int>> zigzag({0, 0, 2, 4, 4, 0, 4, 4, 6, 0});
}

TEST_CASE("MonotoneChain and Line intersection predicates, both directions") {
    using Point = pgl::Point<int>;
    using Line = pgl::Line<Point>;

    const Line crossing({0, 2}, {1, 2});      // y = 2, crosses everything
    const Line above({0, 5}, {1, 5});         // y = 5, misses
    const Line touching({0, 0}, {1, -1});     // y = -x, touches only (0,0)

    CHECK(zigzag.intersects(crossing));
    CHECK(crossing.intersects(zigzag));
    CHECK_FALSE(zigzag.intersects(above));
    CHECK_FALSE(above.intersects(zigzag));
    CHECK(zigzag.intersects(touching));

    // The touching line meets the chain only at its boundary vertex (0,0).
    CHECK(zigzag.interiorsIntersect(crossing));
    CHECK_FALSE(zigzag.interiorsIntersect(touching));

    const pgl::OrientedLine<Point> oriented({0, 2}, {1, 2});
    CHECK(zigzag.intersects(oriented));
    CHECK(zigzag.interiorsIntersect(oriented));
}

TEST_CASE("A line contains a chain only when the chain lies along it") {
    using Point = pgl::Point<int>;
    using Line = pgl::Line<Point>;

    const Line diagonal({0, 0}, {1, 1});
    const pgl::MonotoneChain<Point> straight({0, 0, 1, 1, 3, 3});
    CHECK(diagonal.contains(straight));
    CHECK(diagonal.interiorContains(straight));
    CHECK_FALSE(diagonal.boundaryContains(straight));
    CHECK_FALSE(diagonal.contains(zigzag));
    CHECK_FALSE(zigzag.contains(diagonal));   // a bounded chain holds no line
}

TEST_CASE("MonotoneChain and Line separates and crosses") {
    using Point = pgl::Point<int>;
    using Line = pgl::Line<Point>;

    const Line crossing({0, 2}, {1, 2});
    // Any nonempty intersection with a line disconnects it, and the crossing
    // points are interior points of the chain, so this pair mutually crosses.
    CHECK(zigzag.separates(crossing));
    CHECK(crossing.separates(zigzag));
    CHECK(zigzag.crosses(crossing));
    CHECK(crossing.crosses(zigzag));

    // y = 0 meets the chain at (0,0), (4,0), and (6,0): the component {(4,0)}
    // avoids both extreme vertices, so the line disconnects the chain.
    const Line baseline({0, 0}, {1, 0});
    CHECK(baseline.separates(zigzag));

    // The line through (0,0) alone only touches the chain's boundary vertex.
    const Line touching({0, 0}, {1, -1});
    CHECK(zigzag.separates(touching));
    CHECK_FALSE(touching.separates(zigzag));
    CHECK_FALSE(zigzag.crosses(touching));
    CHECK_FALSE(touching.crosses(zigzag));
}

TEST_CASE("MonotoneChain and Line distance") {
    using Point = pgl::Point<int>;
    using Line = pgl::Line<Point>;
    using Rational = pgl::Rational<int>;

    const Line above({0, 5}, {1, 5});   // nearest chain points are the peaks at y = 4
    CHECK(zigzag.squaredDistance<Rational>(above) == Rational(1));
    CHECK(above.squaredDistance<Rational>(zigzag) == Rational(1));
    CHECK(zigzag.distanceL1<Rational>(above) == Rational(1));
    CHECK(zigzag.distanceLInf<Rational>(above) == Rational(1));
    CHECK(zigzag.squaredDistance<Rational>(Line({0, 2}, {1, 2})) == Rational(0));
}

TEST_CASE("MonotoneChain and Line intersection pieces") {
    using Point = pgl::Point<int>;
    using Line = pgl::Line<Point>;

    SUBCASE("multiple crossings arrive sorted") {
        const auto pieces = zigzag.intersection(Line({0, 2}, {1, 2}));
        REQUIRE(pieces.size() == 4);
        const std::vector<Point> expected{Point(1, 2), Point(3, 2), Point(4, 2), Point(5, 2)};
        for (std::size_t i = 0; i < expected.size(); ++i) {
            REQUIRE(std::holds_alternative<Point>(pieces[i]));
            CHECK(std::get<Point>(pieces[i]) == expected[i]);
        }
    }

    SUBCASE("a collinear chain coalesces into a single segment") {
        const pgl::MonotoneChain<Point> straight({0, 0, 1, 1, 3, 3});
        const auto pieces = straight.intersection(Line({0, 0}, {1, 1}));
        REQUIRE(pieces.size() == 1);
        REQUIRE(std::holds_alternative<pgl::Segment<Point>>(pieces[0]));
        CHECK(std::get<pgl::Segment<Point>>(pieces[0]) == pgl::Segment<Point>({0, 0}, {3, 3}));
    }

    SUBCASE("disjoint pair yields no pieces") {
        CHECK(zigzag.intersection(Line({0, 5}, {1, 5})).empty());
    }
}
