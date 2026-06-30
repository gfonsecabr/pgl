#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"


TEST_CASE("Disk contains OrientedLine: only degenerate oriented lines (points) qualify") {
    using Point = pgl::Point<int>;
    using Disk = pgl::Disk<Point>;
    using OrientedLine = pgl::OrientedLine<Point>;

    // disk centered at (0,0) with radius 5 (a² + b² = 25 for any boundary point)
    const Disk disk(Point(0, 0), 5);

    SUBCASE("degenerate OL (source == target) inside the disk is contained") {
        // (3,4): 3²+4²=25, so it is on the boundary → contained
        CHECK_MESSAGE(disk.contains(OrientedLine(Point(3, 4), Point(3, 4))),
                      "disk contains degenerate OL at (3,4)");
    }

    SUBCASE("degenerate OL outside the disk is not contained") {
        CHECK_FALSE_MESSAGE(disk.contains(OrientedLine(Point(6, 0), Point(6, 0))),
                            "disk does not contain degenerate OL at (6,0)");
    }

    SUBCASE("non-degenerate OrientedLine is never fully inside the disk") {
        CHECK_FALSE_MESSAGE(disk.contains(OrientedLine(Point(-5, 0), Point(5, 0))),
                            "disk cannot contain an infinite oriented line");
    }
}

TEST_CASE("Disk and OrientedLine intersection predicates, both directions") {
    using Point = pgl::Point<int>;
    using Disk = pgl::Disk<Point>;
    using OrientedLine = pgl::OrientedLine<Point>;

    const Disk disk(Point(0, 0), 5);

    SUBCASE("OrientedLine through the interior intersects and interiorsIntersect") {
        const OrientedLine through({-6, 0}, {6, 0});   // y=0 through center
        CHECK_MESSAGE(disk.intersects(through), disk, " intersects ", through);
        CHECK_MESSAGE(through.intersects(disk), through, " intersects ", disk);
        CHECK_MESSAGE(disk.interiorsIntersect(through), disk, " interiorsIntersect ", through);
        CHECK_MESSAGE(through.interiorsIntersect(disk), through, " interiorsIntersect ", disk);
    }

    SUBCASE("OrientedLine tangent to the disk: intersects but interiors do not") {
        // y=5 is tangent to the top of the disk
        const OrientedLine tangent({-6, 5}, {6, 5});
        CHECK_MESSAGE(disk.intersects(tangent), disk, " intersects ", tangent);
        CHECK_FALSE_MESSAGE(disk.interiorsIntersect(tangent), disk, " interiorsIntersect ", tangent);
    }

    SUBCASE("OrientedLine outside: no intersection") {
        const OrientedLine outside({-6, 6}, {6, 6});   // y=6, above the disk
        CHECK_FALSE_MESSAGE(disk.intersects(outside), disk, " intersects ", outside);
        CHECK_FALSE_MESSAGE(outside.intersects(disk), outside, " intersects ", disk);
    }
}

TEST_CASE("Disk and OrientedLine separates and crosses") {
    using Point = pgl::Point<int>;
    using Disk = pgl::Disk<Point>;
    using OrientedLine = pgl::OrientedLine<Point>;

    const Disk disk(Point(0, 0), 5);

    SUBCASE("OrientedLine through the interior separates and crosses the Disk") {
        const OrientedLine through({-6, 0}, {6, 0});
        CHECK_MESSAGE(disk.separates(through), disk, " separates ", through);
        CHECK_MESSAGE(disk.crosses(through), disk, " crosses ", through);
        CHECK_MESSAGE(through.separates(disk), through, " separates ", disk);
        CHECK_MESSAGE(through.crosses(disk), through, " crosses ", disk);
    }

    SUBCASE("OrientedLine tangent does not cross") {
        const OrientedLine tangent({-6, 5}, {6, 5});
        CHECK_FALSE_MESSAGE(disk.crosses(tangent), disk, " crosses ", tangent);
    }
}
