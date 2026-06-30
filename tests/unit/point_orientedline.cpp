#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstdint>

#include "pgl.hpp"


TEST_CASE("Point and OrientedLine containment predicates") {
    using Point = pgl::Point<int>;
    using OrientedLine = pgl::OrientedLine<Point>;

    const OrientedLine diagonal({0, 0}, {4, 4});   // y = x
    const Point on(2, 2);
    const Point off(2, 3);

    CHECK_MESSAGE(diagonal.contains(on), diagonal, " contains ", on);
    CHECK_FALSE_MESSAGE(diagonal.contains(off), diagonal, " contains ", off);

    // A line has no boundary, so interiorContains coincides with contains.
    CHECK_MESSAGE(diagonal.interiorContains(on), diagonal, " interiorContains ", on);
    CHECK_FALSE_MESSAGE(diagonal.boundaryContains(on), diagonal, " boundaryContains ", on);

    // A point contains an oriented line only when it degenerates to that point.
    const OrientedLine degenerate({2, 3}, {2, 3});
    CHECK_MESSAGE(off.contains(degenerate), off, " contains ", degenerate);
    CHECK_MESSAGE(off.interiorContains(degenerate), off, " interiorContains ", degenerate);
    CHECK_FALSE_MESSAGE(off.contains(diagonal), off, " contains ", diagonal);
}

TEST_CASE("Point and OrientedLine intersection predicates, both directions") {
    using Point = pgl::Point<int>;
    using OrientedLine = pgl::OrientedLine<Point>;

    const OrientedLine diagonal({0, 0}, {4, 4});
    const Point on(2, 2);
    const Point off(2, 3);

    CHECK_MESSAGE(diagonal.intersects(on), diagonal, " intersects ", on);
    CHECK_MESSAGE(on.intersects(diagonal), on, " intersects ", diagonal);
    CHECK_FALSE_MESSAGE(diagonal.intersects(off), diagonal, " intersects ", off);
    CHECK_FALSE_MESSAGE(off.intersects(diagonal), off, " intersects ", diagonal);

    CHECK_MESSAGE(diagonal.interiorsIntersect(on), diagonal, " interiorsIntersect ", on);
    CHECK_MESSAGE(on.interiorsIntersect(diagonal), on, " interiorsIntersect ", diagonal);
    CHECK_FALSE_MESSAGE(diagonal.interiorsIntersect(off), diagonal, " interiorsIntersect ", off);
}

TEST_CASE("Point separates an OrientedLine, never the reverse, and they never cross") {
    using Point = pgl::Point<int>;
    using OrientedLine = pgl::OrientedLine<Point>;

    const OrientedLine diagonal({0, 0}, {4, 4});
    const Point on(2, 2);
    const Point off(1, 0);

    CHECK_MESSAGE(on.separates(diagonal), on, " separates ", diagonal);
    CHECK_FALSE_MESSAGE(off.separates(diagonal), off, " separates ", diagonal);
    CHECK_FALSE_MESSAGE(diagonal.separates(on), diagonal, " separates ", on);

    CHECK_FALSE_MESSAGE(on.crosses(diagonal), on, " crosses ", diagonal);
    CHECK_FALSE_MESSAGE(diagonal.crosses(on), diagonal, " crosses ", on);
}

TEST_CASE("Point and OrientedLine intersection construction, both directions") {
    using Point = pgl::Point<int>;
    using Rational = pgl::Rational<int64_t>;
    using OrientedLine = pgl::OrientedLine<Point>;
    using RationalPoint = pgl::Point<Rational>;

    const OrientedLine diagonal({0, 0}, {4, 4});

    SUBCASE("a point on the line yields that point") {
        const Point on(3, 3);
        CHECK(diagonal.intersection(on) == on);
        const auto fromPt = on.intersection(diagonal);
        REQUIRE(fromPt.has_value());
        CHECK(*fromPt == on);

        const auto rational = diagonal.intersection<Rational>(on);
        REQUIRE(rational);
        CHECK(*rational == RationalPoint(Rational(3), Rational(3)));
    }

    SUBCASE("a point off the line yields nothing") {
        const Point off(3, 2);
        CHECK_FALSE(diagonal.intersection(off).has_value());
        CHECK_FALSE(off.intersection(diagonal).has_value());
        CHECK_FALSE(diagonal.intersection<Rational>(off).has_value());
    }
}
