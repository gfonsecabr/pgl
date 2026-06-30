#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <variant>

#include "pgl.hpp"


TEST_CASE("Segment containment of OrientedSegment, and OrientedSegment containment of Segment") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using OrientedSegment = pgl::OrientedSegment<Point>;

    // inner_os lies strictly inside diag, but with reversed orientation.
    const Segment diag({0, 0}, {6, 6});
    const OrientedSegment inner_os({4, 4}, {2, 2});
    const OrientedSegment endpoint_os({0, 0}, {3, 3});  // shares one endpoint with diag
    const OrientedSegment outer_os({1, 1}, {8, 8});     // extends beyond diag

    SUBCASE("Segment contains OrientedSegment when both endpoints lie on it") {
        CHECK_MESSAGE(diag.contains(inner_os), diag, " contains ", inner_os);
        CHECK_MESSAGE(diag.contains(endpoint_os), diag, " contains ", endpoint_os);
        CHECK_FALSE_MESSAGE(diag.contains(outer_os), diag, " contains ", outer_os);
        // Segment has no geometric boundary, so boundaryContains is always false.
        CHECK_FALSE_MESSAGE(diag.boundaryContains(inner_os), diag, " boundaryContains ", inner_os);
    }

    SUBCASE("Segment interiorContains OrientedSegment when both endpoints are strictly interior") {
        CHECK_MESSAGE(diag.interiorContains(inner_os), diag, " interiorContains ", inner_os);
        CHECK_FALSE_MESSAGE(diag.interiorContains(endpoint_os), diag, " interiorContains ", endpoint_os);
    }

    // Reverse: OrientedSegment containing a plain Segment.
    const OrientedSegment host({6, 6}, {0, 0});
    const Segment inner_seg({2, 2}, {4, 4});     // strictly inside host
    const Segment touching_seg({0, 0}, {2, 2});  // shares the target endpoint with host

    SUBCASE("OrientedSegment contains Segment when it lies within") {
        CHECK_MESSAGE(host.contains(inner_seg), host, " contains ", inner_seg);
        CHECK_FALSE_MESSAGE(host.boundaryContains(inner_seg), host, " boundaryContains ", inner_seg);
        CHECK_MESSAGE(host.interiorContains(inner_seg), host, " interiorContains ", inner_seg);
        CHECK_FALSE_MESSAGE(host.interiorContains(touching_seg), host, " interiorContains ", touching_seg);
    }
}

TEST_CASE("Segment and OrientedSegment intersection predicates, both directions") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using OrientedSegment = pgl::OrientedSegment<Point>;

    const Segment horiz({0, 0}, {4, 0});
    const OrientedSegment vert_os({2, -2}, {2, 2});     // crosses horiz at interior (2,0)
    const OrientedSegment touch_os({4, 0}, {6, 2});     // touches horiz only at endpoint (4,0)
    const OrientedSegment disjoint_os({5, -1}, {5, 1}); // no intersection with horiz

    SUBCASE("interior crossing: both intersect, and interiors intersect") {
        CHECK_MESSAGE(horiz.intersects(vert_os), horiz, " intersects ", vert_os);
        CHECK_MESSAGE(vert_os.intersects(horiz), vert_os, " intersects ", horiz);
        CHECK_MESSAGE(horiz.interiorsIntersect(vert_os), horiz, " interiorsIntersect ", vert_os);
        CHECK_MESSAGE(vert_os.interiorsIntersect(horiz), vert_os, " interiorsIntersect ", horiz);
    }

    SUBCASE("endpoint touch: intersects but interiors do not") {
        CHECK_MESSAGE(horiz.intersects(touch_os), horiz, " intersects ", touch_os);
        CHECK_MESSAGE(touch_os.intersects(horiz), touch_os, " intersects ", horiz);
        CHECK_FALSE_MESSAGE(horiz.interiorsIntersect(touch_os), horiz, " interiorsIntersect ", touch_os);
        CHECK_FALSE_MESSAGE(touch_os.interiorsIntersect(horiz), touch_os, " interiorsIntersect ", horiz);
    }

    SUBCASE("disjoint: neither intersects") {
        CHECK_FALSE_MESSAGE(horiz.intersects(disjoint_os), horiz, " intersects ", disjoint_os);
        CHECK_FALSE_MESSAGE(disjoint_os.intersects(horiz), disjoint_os, " intersects ", horiz);
    }
}

TEST_CASE("Segment and OrientedSegment separates and crosses, both directions") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using OrientedSegment = pgl::OrientedSegment<Point>;

    const Segment horiz({0, 0}, {4, 0});
    const OrientedSegment vert_os({2, -2}, {2, 2});  // crosses horiz at interior (2,0)
    const OrientedSegment touch_os({4, 0}, {6, 2});  // touches horiz only at endpoint

    SUBCASE("interior crossing: both separate and cross each other") {
        CHECK_MESSAGE(horiz.separates(vert_os), horiz, " separates ", vert_os);
        CHECK_MESSAGE(vert_os.separates(horiz), vert_os, " separates ", horiz);
        CHECK_MESSAGE(horiz.crosses(vert_os), horiz, " crosses ", vert_os);
        CHECK_MESSAGE(vert_os.crosses(horiz), vert_os, " crosses ", horiz);
    }

    SUBCASE("endpoint touch only: neither separates nor crosses") {
        CHECK_FALSE_MESSAGE(horiz.separates(touch_os), horiz, " separates ", touch_os);
        CHECK_FALSE_MESSAGE(touch_os.separates(horiz), touch_os, " separates ", horiz);
        CHECK_FALSE_MESSAGE(horiz.crosses(touch_os), horiz, " crosses ", touch_os);
        CHECK_FALSE_MESSAGE(touch_os.crosses(horiz), touch_os, " crosses ", horiz);
    }
}

TEST_CASE("Segment and OrientedSegment intersection construction, both directions") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using OrientedSegment = pgl::OrientedSegment<Point>;

    const Segment horiz({0, 0}, {4, 0});
    const OrientedSegment vert_os({2, -2}, {2, 2});   // transverse crossing
    const OrientedSegment sub_os({3, 0}, {1, 0});     // collinear sub-segment (reversed)

    SUBCASE("transverse crossing yields a Point") {
        const auto from_seg = horiz.intersection(vert_os);
        REQUIRE(from_seg.has_value());
        REQUIRE(std::holds_alternative<Point>(*from_seg));
        CHECK(std::get<Point>(*from_seg) == Point(2, 0));

        const auto from_os = vert_os.intersection(horiz);
        REQUIRE(from_os.has_value());
        REQUIRE(std::holds_alternative<Point>(*from_os));
        CHECK(std::get<Point>(*from_os) == Point(2, 0));
    }

    SUBCASE("collinear overlap yields the overlapping Segment") {
        const auto from_seg = horiz.intersection(sub_os);
        REQUIRE(from_seg.has_value());
        REQUIRE(std::holds_alternative<Segment>(*from_seg));
        CHECK(std::get<Segment>(*from_seg) == Segment({1, 0}, {3, 0}));

        const auto from_os = sub_os.intersection(horiz);
        REQUIRE(from_os.has_value());
        REQUIRE(std::holds_alternative<Segment>(*from_os));
        CHECK(std::get<Segment>(*from_os) == Segment({1, 0}, {3, 0}));
    }

    SUBCASE("disjoint yields empty") {
        const OrientedSegment disjoint_os({5, -1}, {5, 1});
        CHECK_FALSE(horiz.intersection(disjoint_os).has_value());
        CHECK_FALSE(disjoint_os.intersection(horiz).has_value());
    }
}
