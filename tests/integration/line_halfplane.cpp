#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"


TEST_CASE("Halfplane containment of Line") {
    using Point = pgl::Point<int>;
    using Line = pgl::Line<Point>;
    using Halfplane = pgl::Halfplane<Point>;

    // upper = closed half-plane y >= 0 (left of (0,0)->(4,0))
    const Halfplane upper({0, 0}, {4, 0});
    const Line inside({0, 2}, {4, 2});    // parallel, strictly above the boundary
    const Line boundary({0, 0}, {4, 0}); // the boundary line itself
    const Line crossing({0, -1}, {4, 3}); // straddles the boundary
    const Line outside({0, -2}, {4, -2}); // parallel, entirely below

    SUBCASE("contains: inside and boundary are contained; crossing and outside are not") {
        CHECK_MESSAGE(upper.contains(inside), upper, " contains ", inside);
        CHECK_MESSAGE(upper.contains(boundary), upper, " contains ", boundary);
        CHECK_FALSE_MESSAGE(upper.contains(crossing), upper, " contains ", crossing);
        CHECK_FALSE_MESSAGE(upper.contains(outside), upper, " contains ", outside);
    }

    SUBCASE("interiorContains: only the strictly interior parallel line qualifies") {
        CHECK_MESSAGE(upper.interiorContains(inside), upper, " interiorContains ", inside);
        CHECK_FALSE_MESSAGE(upper.interiorContains(boundary), upper, " interiorContains ", boundary);
        CHECK_FALSE_MESSAGE(upper.interiorContains(crossing), upper, " interiorContains ", crossing);
        CHECK_FALSE_MESSAGE(upper.interiorContains(outside), upper, " interiorContains ", outside);
    }

    SUBCASE("boundaryContains: only the exact boundary line qualifies") {
        CHECK_MESSAGE(upper.boundaryContains(boundary), upper, " boundaryContains ", boundary);
        CHECK_FALSE_MESSAGE(upper.boundaryContains(inside), upper, " boundaryContains ", inside);
        CHECK_FALSE_MESSAGE(upper.boundaryContains(crossing), upper, " boundaryContains ", crossing);
        CHECK_FALSE_MESSAGE(upper.boundaryContains(outside), upper, " boundaryContains ", outside);
    }
}

TEST_CASE("Line containment of Halfplane") {
    using Point = pgl::Point<int>;
    using Line = pgl::Line<Point>;
    using Halfplane = pgl::Halfplane<Point>;

    const Line axis({0, 0}, {4, 0});
    const Halfplane upper({0, 0}, {4, 0});
    // A degenerate halfplane (source == target) is geometrically a point.
    const Halfplane degenerate_on({2, 0}, {2, 0});  // point (2,0) lies on the x-axis
    const Halfplane degenerate_off({2, 1}, {2, 1});  // point (2,1) does not

    SUBCASE("contains: only a degenerate halfplane whose source is on the line") {
        CHECK_MESSAGE(axis.contains(degenerate_on), axis, " contains ", degenerate_on);
        CHECK_FALSE_MESSAGE(axis.contains(degenerate_off), axis, " contains ", degenerate_off);
        CHECK_FALSE_MESSAGE(axis.contains(upper), axis, " contains ", upper);
    }

    SUBCASE("interiorContains: same condition as contains (line has no boundary)") {
        CHECK_MESSAGE(axis.interiorContains(degenerate_on), axis, " interiorContains ", degenerate_on);
        CHECK_FALSE_MESSAGE(axis.interiorContains(degenerate_off), axis, " interiorContains ", degenerate_off);
        CHECK_FALSE_MESSAGE(axis.interiorContains(upper), axis, " interiorContains ", upper);
    }

    SUBCASE("boundaryContains: always false (a line has an empty boundary)") {
        CHECK_FALSE_MESSAGE(axis.boundaryContains(upper), axis, " boundaryContains ", upper);
        CHECK_FALSE_MESSAGE(axis.boundaryContains(degenerate_on), axis, " boundaryContains ", degenerate_on);
    }
}

TEST_CASE("Line and Halfplane intersection predicates, both directions") {
    using Point = pgl::Point<int>;
    using Line = pgl::Line<Point>;
    using Halfplane = pgl::Halfplane<Point>;

    const Halfplane upper({0, 0}, {4, 0});
    const Line inside({0, 2}, {4, 2});    // entirely in interior
    const Line boundary({0, 0}, {4, 0}); // the boundary line
    const Line crossing({0, -1}, {4, 3}); // crosses from outside into interior
    const Line outside({0, -2}, {4, -2}); // entirely outside

    SUBCASE("inside: both intersect and both interiors intersect") {
        CHECK_MESSAGE(upper.intersects(inside), upper, " intersects ", inside);
        CHECK_MESSAGE(inside.intersects(upper), inside, " intersects ", upper);
        CHECK_MESSAGE(upper.interiorsIntersect(inside), upper, " interiorsIntersect ", inside);
        CHECK_MESSAGE(inside.interiorsIntersect(upper), inside, " interiorsIntersect ", upper);
    }

    SUBCASE("boundary: line intersects the halfplane but interiors do not") {
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

    SUBCASE("outside: no intersection in either direction") {
        CHECK_FALSE_MESSAGE(upper.intersects(outside), upper, " intersects ", outside);
        CHECK_FALSE_MESSAGE(outside.intersects(upper), outside, " intersects ", upper);
        CHECK_FALSE_MESSAGE(upper.interiorsIntersect(outside), upper, " interiorsIntersect ", outside);
        CHECK_FALSE_MESSAGE(outside.interiorsIntersect(upper), outside, " interiorsIntersect ", upper);
    }
}

TEST_CASE("Line and Halfplane separates and crosses are always false") {
    using Point = pgl::Point<int>;
    using Line = pgl::Line<Point>;
    using Halfplane = pgl::Halfplane<Point>;

    // A Halfplane never disconnects a Line: removing a closed half-space from a
    // line leaves a connected ray (the open complement) or nothing.
    // A Line never disconnects a Halfplane: the halfplane interior is simply connected.
    const Halfplane upper({0, 0}, {4, 0});
    const Line inside({0, 2}, {4, 2});
    const Line crossing({0, -1}, {4, 3});

    CHECK_FALSE_MESSAGE(upper.separates(inside), upper, " separates ", inside);
    CHECK_FALSE_MESSAGE(upper.separates(crossing), upper, " separates ", crossing);
    CHECK_FALSE_MESSAGE(inside.separates(upper), inside, " separates ", upper);
    CHECK_FALSE_MESSAGE(crossing.separates(upper), crossing, " separates ", upper);

    CHECK_FALSE_MESSAGE(upper.crosses(inside), upper, " crosses ", inside);
    CHECK_FALSE_MESSAGE(upper.crosses(crossing), upper, " crosses ", crossing);
    CHECK_FALSE_MESSAGE(inside.crosses(upper), inside, " crosses ", upper);
    CHECK_FALSE_MESSAGE(crossing.crosses(upper), crossing, " crosses ", upper);
}

TEST_CASE("Halfplane clips a Line via intersection") {
    using Point = pgl::Point<int>;
    using Rational = pgl::Rational<int>;
    using RationalPoint = pgl::Point<Rational>;
    using Line = pgl::Line<Point>;
    using Halfplane = pgl::Halfplane<Point>;
    using RationalRay = pgl::Ray<RationalPoint>;

    const Halfplane upper({0, 0}, {4, 0});

    SUBCASE("crossing line clips to a ray starting at the boundary crossing") {
        const auto r = upper.intersection<Rational>(Line({0, -1}, {3, 1}));
        REQUIRE(r);
        REQUIRE(std::holds_alternative<RationalRay>(*r));
        const auto& ray = std::get<RationalRay>(*r);
        CHECK_MESSAGE(ray.source() == RationalPoint(Rational(3, 2), Rational(0)),
                      "crossing point with y=0");
        CHECK_MESSAGE(ray.contains(RationalPoint(3, 1)), "original point stays on the clipped ray");
        CHECK_MESSAGE(upper.contains(ray.target()), "target of clipped ray is inside the halfplane");
    }

    SUBCASE("line inside the halfplane clips to itself") {
        const auto r = upper.intersection(Line({0, 2}, {4, 2}));
        REQUIRE(r);
        REQUIRE(std::holds_alternative<Line>(*r));
        CHECK_MESSAGE(std::get<Line>(*r) == Line({0, 2}, {4, 2}), "inside line unchanged");
    }

    SUBCASE("boundary line clips to itself") {
        const auto r = upper.intersection(Line({0, 0}, {4, 0}));
        REQUIRE(r);
        REQUIRE(std::holds_alternative<Line>(*r));
        CHECK_MESSAGE(std::get<Line>(*r) == Line({0, 0}, {4, 0}), "boundary line unchanged");
    }

    SUBCASE("line entirely outside yields empty result") {
        CHECK_FALSE_MESSAGE(upper.intersection(Line({0, -2}, {4, -2})),
                            "outside line clips to nothing");
    }

    SUBCASE("degenerate halfplane (a point) intersected with a line yields that point if it lies on it") {
        const Halfplane degenerate({0, 0}, {0, 0});
        const auto r = degenerate.intersection(Line({-1, -1}, {1, 1}));
        REQUIRE(r);
        REQUIRE(std::holds_alternative<Point>(*r));
        CHECK_MESSAGE(std::get<Point>(*r) == Point(0, 0), "degenerate halfplane gives its point");
    }
}
