#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

// The disk used throughout is the integer circle of radius 5 centred at the
// origin, so (x, y) is in the closed disk iff x*x + y*y <= 25.  All expected
// answers below are checkable by hand.  OrientedSegment delegates to the
// underlying unoriented Segment for all predicates, so the results match the
// segment_disk.cpp tests in both directions.

TEST_CASE("Disk containment of OrientedSegment, and OrientedSegment containment of Disk") {
    using Point = pgl::Point<int>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using Disk = pgl::Disk<Point>;

    Disk d(0, 0, 5);

    SUBCASE("OrientedSegment strictly inside the disk: contains and interiorContains") {
        const OrientedSegment os(Point(1, 1), Point(-1, -1));
        CHECK_MESSAGE(d.contains(os), d, " contains ", os);
        CHECK_MESSAGE(d.interiorContains(os), d, " interiorContains ", os);
    }

    SUBCASE("OrientedSegment with one endpoint on the boundary circle") {
        const OrientedSegment os(Point(3, 4), Point(0, 0)); // (3,4) on circle
        CHECK_MESSAGE(d.contains(os), d, " contains ", os);
        CHECK_FALSE_MESSAGE(d.interiorContains(os), d, " interiorContains ", os);
    }

    SUBCASE("OrientedSegment crossing the boundary: disk does not contain it") {
        const OrientedSegment os(Point(0, 0), Point(10, 0));
        CHECK_FALSE_MESSAGE(d.contains(os), d, " contains ", os);
    }

    SUBCASE("Disk boundary never contains a non-degenerate OrientedSegment") {
        const OrientedSegment os(Point(-6, 2), Point(6, 2));
        CHECK_FALSE_MESSAGE(d.boundaryContains(os), d, " boundaryContains ", os);
    }

    SUBCASE("An OrientedSegment (finite) can never contain an infinite-extent Disk") {
        const OrientedSegment os(Point(-6, 0), Point(6, 0));
        CHECK_FALSE_MESSAGE(os.contains(d), os, " contains ", d);
    }
}

TEST_CASE("Disk and OrientedSegment intersects, both directions") {
    using Point = pgl::Point<int>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using Disk = pgl::Disk<Point>;

    Disk d(0, 0, 5);

    SUBCASE("chord through interior: both directions intersect") {
        const OrientedSegment os(Point(-6, 2), Point(6, 2));
        CHECK_MESSAGE(d.intersects(os), d, " intersects ", os);
        CHECK_MESSAGE(os.intersects(d), os, " intersects ", d);
    }

    SUBCASE("endpoint on boundary: intersects") {
        const OrientedSegment os(Point(3, 4), Point(9, 9)); // (3,4) on circle
        CHECK_MESSAGE(d.intersects(os), d, " intersects ", os);
        CHECK_MESSAGE(os.intersects(d), os, " intersects ", d);
    }

    SUBCASE("tangent from outside reaching the boundary: intersects") {
        const OrientedSegment os(Point(5, -3), Point(5, 3)); // tangent at (5,0)
        CHECK_MESSAGE(d.intersects(os), d, " intersects ", os);
        CHECK_MESSAGE(os.intersects(d), os, " intersects ", d);
    }

    SUBCASE("miss: neither direction intersects") {
        const OrientedSegment os(Point(6, -3), Point(6, 3));
        CHECK_FALSE_MESSAGE(d.intersects(os), d, " intersects ", os);
        CHECK_FALSE_MESSAGE(os.intersects(d), os, " intersects ", d);
    }

    SUBCASE("orientation is irrelevant: reversed OS agrees with forward OS") {
        const OrientedSegment fwd(Point(-6, 2), Point(6, 2));
        const OrientedSegment rev(Point(6, 2), Point(-6, 2));
        CHECK_MESSAGE(d.intersects(fwd) == d.intersects(rev), d, " intersects same both ways");
    }
}

TEST_CASE("Disk and OrientedSegment interiorsIntersect, both directions") {
    using Point = pgl::Point<int>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using Disk = pgl::Disk<Point>;

    Disk d(0, 0, 5);

    SUBCASE("chord through interior: interiors intersect") {
        const OrientedSegment os(Point(-6, 2), Point(6, 2));
        CHECK_MESSAGE(d.interiorsIntersect(os), d, " interiorsIntersect ", os);
        CHECK_MESSAGE(os.interiorsIntersect(d), os, " interiorsIntersect ", d);
    }

    SUBCASE("tangent from outside: touches boundary only; interiors do not meet") {
        const OrientedSegment os(Point(5, -3), Point(5, 3)); // tangent at (5,0)
        CHECK_FALSE_MESSAGE(d.interiorsIntersect(os), d, " interiorsIntersect ", os);
        CHECK_FALSE_MESSAGE(os.interiorsIntersect(d), os, " interiorsIntersect ", d);
    }

    SUBCASE("endpoint strictly inside: interiors intersect") {
        const OrientedSegment os(Point(0, 0), Point(20, 0));
        CHECK_MESSAGE(d.interiorsIntersect(os), d, " interiorsIntersect ", os);
        CHECK_MESSAGE(os.interiorsIntersect(d), os, " interiorsIntersect ", d);
    }

    SUBCASE("miss: no interior contact") {
        const OrientedSegment os(Point(6, -3), Point(6, 3));
        CHECK_FALSE_MESSAGE(d.interiorsIntersect(os), d, " interiorsIntersect ", os);
        CHECK_FALSE_MESSAGE(os.interiorsIntersect(d), os, " interiorsIntersect ", d);
    }
}

TEST_CASE("Disk and OrientedSegment separates and crosses") {
    using Point = pgl::Point<int>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using Disk = pgl::Disk<Point>;

    Disk d(0, 0, 5);

    SUBCASE("full chord: both separate each other and both cross") {
        const OrientedSegment os(Point(-6, 2), Point(6, 2)); // both endpoints outside
        CHECK_MESSAGE(os.separates(d), os, " separates ", d);
        CHECK_MESSAGE(d.separates(os), d, " separates ", os);
        CHECK_MESSAGE(os.crosses(d), os, " crosses ", d);
        CHECK_MESSAGE(d.crosses(os), d, " crosses ", os);
    }

    SUBCASE("endpoint inside: neither separates") {
        const OrientedSegment os(Point(0, 0), Point(20, 0));
        CHECK_FALSE_MESSAGE(os.separates(d), os, " separates ", d);
        CHECK_FALSE_MESSAGE(d.separates(os), d, " separates ", os);
        CHECK_FALSE_MESSAGE(os.crosses(d), os, " crosses ", d);
        CHECK_FALSE_MESSAGE(d.crosses(os), d, " crosses ", os);
    }

    SUBCASE("miss: neither separates") {
        const OrientedSegment os(Point(6, 0), Point(10, 0));
        CHECK_FALSE_MESSAGE(os.separates(d), os, " separates ", d);
        CHECK_FALSE_MESSAGE(d.separates(os), d, " separates ", os);
    }
}
