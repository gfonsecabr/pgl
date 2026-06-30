#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <variant>

TEST_CASE("Halfplane containment of Segment") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Halfplane = pgl::Halfplane<Point>;

    // upper = the closed half-plane y >= 0 (left side of (0,0)->(4,0))
    const Halfplane upper({0, 0}, {4, 0});
    const Segment inside({1, 1}, {3, 2});    // strictly in the interior
    const Segment boundary({1, 0}, {3, 0}); // on the boundary line y = 0
    const Segment crossing({1, -1}, {3, 2}); // straddles the boundary

    SUBCASE("Halfplane contains and interiorContains a wholly interior Segment") {
        CHECK_MESSAGE(upper.contains(inside), upper, " contains ", inside);
        CHECK_MESSAGE(upper.interiorContains(inside), upper, " interiorContains ", inside);
    }

    SUBCASE("Halfplane contains a boundary Segment but does not interiorContain it") {
        CHECK_MESSAGE(upper.contains(boundary), upper, " contains ", boundary);
        CHECK_FALSE_MESSAGE(upper.interiorContains(boundary), upper, " interiorContains ", boundary);
    }

    SUBCASE("Halfplane does not contain a Segment that crosses the boundary") {
        CHECK_FALSE_MESSAGE(upper.contains(crossing), upper, " contains ", crossing);
    }

    SUBCASE("Halfplane boundaryContains a Segment on the boundary line") {
        CHECK_MESSAGE(upper.boundaryContains(boundary), upper, " boundaryContains ", boundary);
        CHECK_FALSE_MESSAGE(upper.boundaryContains(inside), upper, " boundaryContains ", inside);
        CHECK_FALSE_MESSAGE(upper.boundaryContains(crossing), upper, " boundaryContains ", crossing);
    }
}

TEST_CASE("Segment containment of Halfplane") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Halfplane = pgl::Halfplane<Point>;

    const Halfplane upper({0, 0}, {4, 0});
    const Segment inside({1, 1}, {3, 2});

    // A finite Segment can never contain an infinite Halfplane.
    CHECK_FALSE_MESSAGE(inside.contains(upper), inside, " contains ", upper);
    // Segment has no geometric boundary, so boundaryContains is always false.
    CHECK_FALSE_MESSAGE(inside.boundaryContains(upper), inside, " boundaryContains ", upper);
    // Segment::interiorContains(Halfplane) is always false.
    CHECK_FALSE_MESSAGE(inside.interiorContains(upper), inside, " interiorContains ", upper);
}

TEST_CASE("Segment and Halfplane intersection predicates, both directions") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Halfplane = pgl::Halfplane<Point>;

    const Halfplane upper({0, 0}, {4, 0});
    const Segment inside({1, 1}, {3, 2});    // entirely in the interior
    const Segment boundary({1, 0}, {3, 0}); // on the boundary
    const Segment crossing({1, -1}, {3, 2}); // crosses the boundary into the interior
    const Segment outside({1, -2}, {3, -1}); // entirely outside

    SUBCASE("inside: both intersect; halfplane interiorContains implies interiorsIntersect") {
        CHECK_MESSAGE(upper.intersects(inside), upper, " intersects ", inside);
        CHECK_MESSAGE(inside.intersects(upper), inside, " intersects ", upper);
        CHECK_MESSAGE(upper.interiorsIntersect(inside), upper, " interiorsIntersect ", inside);
        CHECK_MESSAGE(inside.interiorsIntersect(upper), inside, " interiorsIntersect ", upper);
    }

    SUBCASE("boundary: intersects but interiors do not") {
        CHECK_MESSAGE(upper.intersects(boundary), upper, " intersects ", boundary);
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

TEST_CASE("Segment and Halfplane separates and crosses are always false") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Halfplane = pgl::Halfplane<Point>;

    // A Halfplane never disconnects a Segment (convex set minus a 1D arc stays connected).
    // A Segment never disconnects a Halfplane (no finite curve can cut across an infinite convex set).
    const Halfplane upper({0, 0}, {4, 0});
    const Segment inside({1, 1}, {3, 2});
    const Segment crossing({1, -1}, {3, 2});

    CHECK_FALSE_MESSAGE(upper.separates(inside), upper, " separates ", inside);
    CHECK_FALSE_MESSAGE(upper.separates(crossing), upper, " separates ", crossing);
    CHECK_FALSE_MESSAGE(inside.separates(upper), inside, " separates ", upper);
    CHECK_FALSE_MESSAGE(crossing.separates(upper), crossing, " separates ", upper);

    CHECK_FALSE_MESSAGE(upper.crosses(inside), upper, " crosses ", inside);
    CHECK_FALSE_MESSAGE(upper.crosses(crossing), upper, " crosses ", crossing);
    CHECK_FALSE_MESSAGE(inside.crosses(upper), inside, " crosses ", upper);
    CHECK_FALSE_MESSAGE(crossing.crosses(upper), crossing, " crosses ", upper);
}

TEST_CASE("Halfplane intersection clips a Segment") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Halfplane = pgl::Halfplane<Point>;

    const Halfplane upper({0, 0}, {4, 0});  // y >= 0

    SUBCASE("segment fully inside: returned unchanged") {
        const Segment inside({1, 1}, {3, 2});
        const auto r = upper.intersection(inside);
        REQUIRE_MESSAGE(r, "halfplane ∩ inside segment should be non-empty");
        REQUIRE(std::holds_alternative<Segment>(*r));
        CHECK_MESSAGE(std::get<Segment>(*r) == inside, "fully-inside segment unchanged");
    }

    SUBCASE("segment crossing the boundary: clipped to the inside portion") {
        // (0,-2)→(4,2) crosses y=0 at (2,0); inside portion is (2,0)→(4,2)
        const Segment crossing({0, -2}, {4, 2});
        const auto r = upper.intersection(crossing);
        REQUIRE_MESSAGE(r, "halfplane ∩ crossing segment should be non-empty");
        REQUIRE(std::holds_alternative<Segment>(*r));
        CHECK_MESSAGE(std::get<Segment>(*r) == Segment(Point(2, 0), Point(4, 2)),
                      "crossing segment clipped at y=0");
    }

    SUBCASE("segment touching boundary at one endpoint: returns that Point") {
        // (2,-1)→(2,0): only the endpoint (2,0) is in the halfplane
        const Segment touching({2, -1}, {2, 0});
        const auto r = upper.intersection(touching);
        REQUIRE_MESSAGE(r, "halfplane ∩ touching segment should be non-empty");
        CHECK_MESSAGE(std::holds_alternative<Point>(*r), "boundary-endpoint touch yields a Point");
        CHECK_MESSAGE(std::get<Point>(*r) == Point(2, 0), "touch point is (2,0)");
    }

    SUBCASE("segment fully outside: empty") {
        const Segment outside({1, -3}, {3, -1});
        CHECK_FALSE_MESSAGE(upper.intersection(outside), "halfplane ∩ outside segment is empty");
    }
}
