#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstdint>

#include "pgl.hpp"


TEST_CASE("Point and OrientedSegment containment predicates") {
    using Point = pgl::Point<int>;
    using OrientedSegment = pgl::OrientedSegment<Point>;

    const OrientedSegment s({0, 0}, {4, 0});
    const Point mid(2, 0);   // strict interior
    const Point end(0, 0);   // source endpoint, boundary
    const Point off(2, 2);   // off the segment

    CHECK_MESSAGE(s.contains(mid), s, " contains ", mid);
    CHECK_MESSAGE(s.contains(end), s, " contains ", end);
    CHECK_FALSE_MESSAGE(s.contains(off), s, " contains ", off);

    CHECK_MESSAGE(s.interiorContains(mid), s, " interiorContains ", mid);
    CHECK_FALSE_MESSAGE(s.interiorContains(end), s, " interiorContains ", end);

    CHECK_FALSE_MESSAGE(s.boundaryContains(mid), s, " boundaryContains ", mid);
    CHECK_MESSAGE(s.boundaryContains(end), s, " boundaryContains ", end);

    // A point contains an oriented segment only when it degenerates to that point.
    const OrientedSegment degenerate({2, 2}, {2, 2});
    CHECK_MESSAGE(off.contains(degenerate), off, " contains ", degenerate);
    CHECK_MESSAGE(off.interiorContains(degenerate), off, " interiorContains ", degenerate);
    CHECK_FALSE_MESSAGE(off.contains(s), off, " contains ", s);
}

TEST_CASE("Point and OrientedSegment intersection predicates, both directions") {
    using Point = pgl::Point<int>;
    using OrientedSegment = pgl::OrientedSegment<Point>;

    const OrientedSegment s({0, 0}, {4, 0});
    const Point mid(2, 0);
    const Point end(0, 0);
    const Point off(2, 2);

    CHECK_MESSAGE(s.intersects(mid), s, " intersects ", mid);
    CHECK_MESSAGE(mid.intersects(s), mid, " intersects ", s);
    CHECK_MESSAGE(s.intersects(end), s, " intersects ", end);
    CHECK_MESSAGE(end.intersects(s), end, " intersects ", s);
    CHECK_FALSE_MESSAGE(s.intersects(off), s, " intersects ", off);
    CHECK_FALSE_MESSAGE(off.intersects(s), off, " intersects ", s);

    // Interiors meet only at a strictly interior point.
    CHECK_MESSAGE(s.interiorsIntersect(mid), s, " interiorsIntersect ", mid);
    CHECK_MESSAGE(mid.interiorsIntersect(s), mid, " interiorsIntersect ", s);
    CHECK_FALSE_MESSAGE(s.interiorsIntersect(end), s, " interiorsIntersect ", end);
    CHECK_FALSE_MESSAGE(s.interiorsIntersect(off), s, " interiorsIntersect ", off);
}

TEST_CASE("Point separates an OrientedSegment, never the reverse, and they never cross") {
    using Point = pgl::Point<int>;
    using OrientedSegment = pgl::OrientedSegment<Point>;

    const OrientedSegment s({0, 0}, {4, 0});
    const Point mid(2, 0);
    const Point end(0, 0);
    const Point off(2, 2);

    CHECK_MESSAGE(mid.separates(s), mid, " separates ", s);
    CHECK_FALSE_MESSAGE(end.separates(s), end, " separates ", s);
    CHECK_FALSE_MESSAGE(off.separates(s), off, " separates ", s);

    // A (1D) segment can never disconnect a (0D) point.
    CHECK_FALSE_MESSAGE(s.separates(mid), s, " separates ", mid);

    CHECK_FALSE_MESSAGE(mid.crosses(s), mid, " crosses ", s);
    CHECK_FALSE_MESSAGE(s.crosses(mid), s, " crosses ", mid);
}

TEST_CASE("Point and OrientedSegment intersection construction, both directions") {
    using Point = pgl::Point<int>;
    using OrientedSegment = pgl::OrientedSegment<Point>;

    const OrientedSegment s({0, 0}, {4, 0});

    SUBCASE("a point on the segment yields that point") {
        const Point mid(2, 0);
        const auto fromSeg = s.intersection(mid);
        const auto fromPt = mid.intersection(s);
        REQUIRE(fromSeg.has_value());
        CHECK(*fromSeg == mid);
        REQUIRE(fromPt.has_value());
        CHECK(*fromPt == mid);
    }

    SUBCASE("a point off the segment yields nothing") {
        const Point off(2, 2);
        CHECK_FALSE(s.intersection(off).has_value());
        CHECK_FALSE(off.intersection(s).has_value());
    }
}

TEST_CASE("Point and OrientedSegment squared Hausdorff distance") {
    using Point = pgl::Point<int>;
    using OrientedSegment = pgl::OrientedSegment<Point>;

    const OrientedSegment s({0, 0}, {4, 0});
    const Point p(1, 3);

    // Matches the unoriented Segment case: the farthest endpoint dominates.
    CHECK(s.squaredHausdorffDistance(p) == 18);
    CHECK(p.squaredHausdorffDistance(s) == 18);
}
