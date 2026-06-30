#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <variant>

#include "pgl.hpp"


TEST_CASE("Halfplane containment of OrientedLine") {
    using Point = pgl::Point<int>;
    using OrientedLine = pgl::OrientedLine<Point>;
    using Halfplane = pgl::Halfplane<Point>;

    // upper = closed half-plane y >= 0
    const Halfplane upper({0, 0}, {4, 0});
    const OrientedLine inside({4, 2}, {0, 2});    // parallel, y=2, strictly inside
    const OrientedLine boundary({4, 0}, {0, 0}); // the boundary line, reversed orientation
    const OrientedLine crossing({0, -1}, {4, 3}); // crosses from below to above
    const OrientedLine outside({4, -2}, {0, -2}); // parallel, y=-2, entirely outside

    SUBCASE("contains: inside and boundary are contained; crossing and outside are not") {
        CHECK_MESSAGE(upper.contains(inside), upper, " contains ", inside);
        CHECK_MESSAGE(upper.contains(boundary), upper, " contains ", boundary);
        CHECK_FALSE_MESSAGE(upper.contains(crossing), upper, " contains ", crossing);
        CHECK_FALSE_MESSAGE(upper.contains(outside), upper, " contains ", outside);
    }

    SUBCASE("interiorContains: only the strictly interior line qualifies") {
        CHECK_MESSAGE(upper.interiorContains(inside), upper, " interiorContains ", inside);
        CHECK_FALSE_MESSAGE(upper.interiorContains(boundary), upper, " interiorContains ", boundary);
        CHECK_FALSE_MESSAGE(upper.interiorContains(crossing), upper, " interiorContains ", crossing);
        CHECK_FALSE_MESSAGE(upper.interiorContains(outside), upper, " interiorContains ", outside);
    }

    SUBCASE("boundaryContains: only the exact boundary line qualifies") {
        CHECK_MESSAGE(upper.boundaryContains(boundary), upper, " boundaryContains ", boundary);
        CHECK_FALSE_MESSAGE(upper.boundaryContains(inside), upper, " boundaryContains ", inside);
        CHECK_FALSE_MESSAGE(upper.boundaryContains(outside), upper, " boundaryContains ", outside);
    }
}

TEST_CASE("OrientedLine containment of Halfplane") {
    using Point = pgl::Point<int>;
    using OrientedLine = pgl::OrientedLine<Point>;
    using Halfplane = pgl::Halfplane<Point>;

    const OrientedLine axis({0, 0}, {4, 0});
    const Halfplane upper({0, 0}, {4, 0});
    // A degenerate halfplane (source == target) is a point.
    const Halfplane degenerate_on({2, 0}, {2, 0});   // (2,0) lies on the x-axis
    const Halfplane degenerate_off({2, 1}, {2, 1});   // (2,1) does not

    SUBCASE("OrientedLine contains only a degenerate halfplane whose source is on it") {
        CHECK_MESSAGE(axis.contains(degenerate_on), axis, " contains ", degenerate_on);
        CHECK_MESSAGE(axis.interiorContains(degenerate_on), axis, " interiorContains ", degenerate_on);
        CHECK_FALSE_MESSAGE(axis.contains(degenerate_off), axis, " contains ", degenerate_off);
        CHECK_FALSE_MESSAGE(axis.contains(upper), axis, " contains ", upper);
    }

    SUBCASE("boundaryContains is always false (oriented line has an empty boundary)") {
        CHECK_FALSE_MESSAGE(axis.boundaryContains(upper), axis, " boundaryContains ", upper);
        CHECK_FALSE_MESSAGE(axis.boundaryContains(degenerate_on), axis, " boundaryContains ", degenerate_on);
    }
}

TEST_CASE("OrientedLine and Halfplane intersection predicates, both directions") {
    using Point = pgl::Point<int>;
    using OrientedLine = pgl::OrientedLine<Point>;
    using Halfplane = pgl::Halfplane<Point>;

    const Halfplane upper({0, 0}, {4, 0});
    const OrientedLine inside({4, 2}, {0, 2});
    const OrientedLine boundary({4, 0}, {0, 0});
    const OrientedLine crossing({0, -1}, {4, 3});
    const OrientedLine outside({4, -2}, {0, -2});

    SUBCASE("inside: both intersect and both interiors intersect") {
        CHECK_MESSAGE(upper.intersects(inside), upper, " intersects ", inside);
        CHECK_MESSAGE(inside.intersects(upper), inside, " intersects ", upper);
        CHECK_MESSAGE(upper.interiorsIntersect(inside), upper, " interiorsIntersect ", inside);
        CHECK_MESSAGE(inside.interiorsIntersect(upper), inside, " interiorsIntersect ", upper);
    }

    SUBCASE("boundary: intersects but interiors do not") {
        CHECK_MESSAGE(upper.intersects(boundary), upper, " intersects ", boundary);
        CHECK_MESSAGE(boundary.intersects(upper), boundary, " intersects ", upper);
        CHECK_FALSE_MESSAGE(upper.interiorsIntersect(boundary), upper, " interiorsIntersect ", boundary);
        CHECK_FALSE_MESSAGE(boundary.interiorsIntersect(upper), boundary, " interiorsIntersect ", upper);
    }

    SUBCASE("crossing: intersects and interiors intersect") {
        CHECK_MESSAGE(upper.intersects(crossing), upper, " intersects ", crossing);
        CHECK_MESSAGE(crossing.intersects(upper), crossing, " intersects ", upper);
        CHECK_MESSAGE(upper.interiorsIntersect(crossing), upper, " interiorsIntersect ", crossing);
        CHECK_MESSAGE(crossing.interiorsIntersect(upper), crossing, " interiorsIntersect ", upper);
    }

    SUBCASE("outside: no intersection") {
        CHECK_FALSE_MESSAGE(upper.intersects(outside), upper, " intersects ", outside);
        CHECK_FALSE_MESSAGE(outside.intersects(upper), outside, " intersects ", upper);
    }
}

TEST_CASE("OrientedLine and Halfplane separates and crosses are always false") {
    using Point = pgl::Point<int>;
    using OrientedLine = pgl::OrientedLine<Point>;
    using Halfplane = pgl::Halfplane<Point>;

    const Halfplane upper({0, 0}, {4, 0});
    const OrientedLine inside({4, 2}, {0, 2});
    const OrientedLine crossing({0, -1}, {4, 3});

    CHECK_FALSE_MESSAGE(upper.separates(inside), upper, " separates ", inside);
    CHECK_FALSE_MESSAGE(upper.separates(crossing), upper, " separates ", crossing);
    CHECK_FALSE_MESSAGE(inside.separates(upper), inside, " separates ", upper);
    CHECK_FALSE_MESSAGE(crossing.separates(upper), crossing, " separates ", upper);

    CHECK_FALSE_MESSAGE(upper.crosses(inside), upper, " crosses ", inside);
    CHECK_FALSE_MESSAGE(upper.crosses(crossing), upper, " crosses ", crossing);
    CHECK_FALSE_MESSAGE(inside.crosses(upper), inside, " crosses ", upper);
    CHECK_FALSE_MESSAGE(crossing.crosses(upper), crossing, " crosses ", upper);
}

TEST_CASE("Halfplane clips an OrientedLine via intersection") {
    using Point = pgl::Point<int>;
    using Rational = pgl::Rational<int>;
    using RationalPoint = pgl::Point<Rational>;
    using OrientedLine = pgl::OrientedLine<Point>;
    using Halfplane = pgl::Halfplane<Point>;
    using RationalRay = pgl::Ray<RationalPoint>;
    using Line = pgl::Line<Point>;

    const Halfplane upper({0, 0}, {4, 0});

    SUBCASE("crossing oriented line clips to a ray starting at the boundary crossing") {
        // goes from (-∞,direction) crossing y=0 at x=3/2 into the upper half
        const auto r = upper.intersection<Rational>(OrientedLine({3, 1}, {0, -1}));
        REQUIRE(r);
        REQUIRE(std::holds_alternative<RationalRay>(*r));
        CHECK_MESSAGE(std::get<RationalRay>(*r).source() == RationalPoint(Rational(3, 2), Rational(0)),
                      "crossing at y=0");
    }

    SUBCASE("oriented line inside the halfplane clips to a plain Line") {
        const auto r = upper.intersection(OrientedLine({0, 2}, {4, 2}));
        REQUIRE(r);
        REQUIRE(std::holds_alternative<Line>(*r));
    }

    SUBCASE("oriented line entirely outside yields empty") {
        CHECK_FALSE_MESSAGE(upper.intersection(OrientedLine({4, -2}, {0, -2})),
                            "outside OL clips to nothing");
    }
}
