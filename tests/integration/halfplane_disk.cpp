#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstdint>

#include "pgl.hpp"

// Disk::intersects(Halfplane) and Disk::interiorsIntersect(Halfplane). A
// half-plane contains everything on the LEFT of its oriented boundary
// source->target (boundary included). With the disk being the integer circle of
// radius 5 centred at the origin, the closed disk meets the closed half-plane
// iff the signed left-distance of the centre is at least -5 (intersects), and
// the open disk meets the open half-plane iff that distance is strictly greater
// than -5 (interiorsIntersect). Tangency from the non-contained side touches the
// boundary only, so it counts for intersects but not for interiorsIntersect.

TEST_CASE("Half-plane boundary cutting through a disk") {
    using Point = pgl::Point<int>;
    using Halfplane = pgl::Halfplane<Point>;
    using Disk = pgl::Disk<Point>;

    Disk d(0, 0, 5);

    SUBCASE("boundary is the x-axis, contained side y >= 0") {
        Halfplane h(Point(0, 0), Point(1, 0));
        CHECK_MESSAGE(d.intersects(h), d, " intersects ", h);
        CHECK_MESSAGE(h.intersects(d), h, " intersects ", d);
        CHECK_MESSAGE(d.interiorsIntersect(h), d, " interiorsIntersect ", h);
        CHECK_MESSAGE(h.interiorsIntersect(d), h, " interiorsIntersect ", d);
    }

    SUBCASE("the opposite orientation still cuts the disk") {
        Halfplane h(Point(1, 0), Point(0, 0));  // contained side y <= 0
        CHECK_MESSAGE(d.intersects(h), d, " intersects ", h);
        CHECK_MESSAGE(d.interiorsIntersect(h), d, " interiorsIntersect ", h);
    }
}

TEST_CASE("Half-plane tangent to a disk touches the boundary only") {
    using Point = pgl::Point<int>;
    using Halfplane = pgl::Halfplane<Point>;
    using Disk = pgl::Disk<Point>;

    Disk d(0, 0, 5);

    SUBCASE("tangent at (-5,0), disk on the non-contained side") {
        // source->target = (-5,0)->(-5,1) points up; contained side is x <= -5,
        // so the disk only touches the boundary line at (-5, 0).
        Halfplane h(Point(-5, 0), Point(-5, 1));
        CHECK_MESSAGE(d.intersects(h), d, " intersects ", h);
        CHECK_FALSE_MESSAGE(d.interiorsIntersect(h), d, " interiorsIntersect ", h);
        CHECK_FALSE_MESSAGE(h.interiorsIntersect(d), h, " interiorsIntersect ", d);
    }

    SUBCASE("tangent at (-5,0), disk on the contained side") {
        // Reversed boundary: contained side is x >= -5, so the whole disk lies
        // inside and its interior meets the open half-plane.
        Halfplane h(Point(-5, 1), Point(-5, 0));
        CHECK_MESSAGE(d.intersects(h), d, " intersects ", h);
        CHECK_MESSAGE(d.interiorsIntersect(h), d, " interiorsIntersect ", h);
    }
}

TEST_CASE("Half-plane whose boundary misses the disk entirely") {
    using Point = pgl::Point<int>;
    using Halfplane = pgl::Halfplane<Point>;
    using Disk = pgl::Disk<Point>;

    Disk d(0, 0, 5);

    SUBCASE("disk fully inside the half-plane") {
        // boundary x = 6 with (6,0)->(6,1) up; contained side x <= 6 holds the
        // whole disk.
        Halfplane h(Point(6, 0), Point(6, 1));
        CHECK_MESSAGE(d.intersects(h), d, " intersects ", h);
        CHECK_MESSAGE(d.interiorsIntersect(h), d, " interiorsIntersect ", h);
    }

    SUBCASE("disk fully outside the half-plane") {
        // Reversed: contained side x >= 6 excludes the whole disk.
        Halfplane h(Point(6, 1), Point(6, 0));
        CHECK_FALSE_MESSAGE(d.intersects(h), d, " intersects ", h);
        CHECK_FALSE_MESSAGE(h.intersects(d), h, " intersects ", d);
        CHECK_FALSE_MESSAGE(d.interiorsIntersect(h), d, " interiorsIntersect ", h);
        CHECK_FALSE_MESSAGE(h.interiorsIntersect(d), h, " interiorsIntersect ", d);
    }

    SUBCASE("far boundary, disk deep inside") {
        Halfplane h(Point(0, -10), Point(1, -10));  // contained side y >= -10
        CHECK_MESSAGE(d.intersects(h), d, " intersects ", h);
        CHECK_MESSAGE(d.interiorsIntersect(h), d, " interiorsIntersect ", h);
    }

    SUBCASE("far boundary, disk well outside") {
        Halfplane h(Point(0, 10), Point(1, 10));  // contained side y >= 10
        CHECK_FALSE_MESSAGE(d.intersects(h), d, " intersects ", h);
        CHECK_FALSE_MESSAGE(d.interiorsIntersect(h), d, " interiorsIntersect ", h);
    }
}

TEST_CASE("Half-plane against a disk built from three arbitrary boundary points") {
    using Point = pgl::Point<int>;
    using Halfplane = pgl::Halfplane<Point>;
    using Disk = pgl::Disk<Point>;

    Disk d(Point(5, 0), Point(-3, 4), Point(0, -5));

    CHECK_MESSAGE(d.interiorsIntersect(Halfplane(Point(0, 0), Point(1, 0))), d, " cut by x-axis");
    CHECK_MESSAGE(d.intersects(Halfplane(Point(-5, 0), Point(-5, 1))), d, " tangent touches");
    CHECK_FALSE_MESSAGE(d.interiorsIntersect(Halfplane(Point(-5, 0), Point(-5, 1))), d, " tangent not interior");
    CHECK_FALSE_MESSAGE(d.intersects(Halfplane(Point(6, 1), Point(6, 0))), d, " disk outside");
}

TEST_CASE("Generated consistency for half-planes around a disk") {
    using Point = pgl::Point<int>;
    using Halfplane = pgl::Halfplane<Point>;
    using Disk = pgl::Disk<Point>;

    Disk d(0, 0, 5);

    auto sx = GENERATE(-8, -2, 0, 6);
    auto sy = GENERATE(-6, 0, 3, 8);
    auto tx = GENERATE(-5, 1, 7);
    auto ty = GENERATE(-4, 0, 5);
    if (Point(sx, sy) == Point(tx, ty)) {
        return;  // a half-plane needs a non-degenerate boundary
    }
    Halfplane h(Point(sx, sy), Point(tx, ty));

    const bool meets = d.intersects(h);
    const bool meets_interior = d.interiorsIntersect(h);

    // Both relations are symmetric.
    CHECK_MESSAGE(meets == h.intersects(d), h, " intersects symmetry ", d);
    CHECK_MESSAGE(meets_interior == h.interiorsIntersect(d), h, " interiorsIntersect symmetry ", d);

    // Meeting the open half-plane is strictly stronger than touching the closed one.
    if (meets_interior) {
        CHECK_MESSAGE(meets, d, " must intersect when interiors meet ", h);
    }

    // A reversed half-plane shares only the boundary line: at most one of the
    // open half-plane and its complement can be missed by the open disk, so if
    // the open disk misses both the disk must sit on the boundary line, which a
    // radius-5 disk centred at the origin never does degenerately here. Hence at
    // least one orientation must meet the open disk.
    Halfplane reversed(Point(tx, ty), Point(sx, sy));
    const bool meets_some_side = meets_interior || d.interiorsIntersect(reversed);
    CHECK_MESSAGE(meets_some_side,
                  d, " open disk must meet one side of the boundary of ", h);
}
