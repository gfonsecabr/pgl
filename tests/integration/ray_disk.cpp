#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstdint>

#include "pgl.hpp"

// Disk::intersects(Ray) / interiorsIntersect(Ray) are decided exactly and
// without division on the integer circle of radius 5 centred at the origin, so
// a point (x, y) lies in the closed disk iff x*x + y*y <= 25. A ray meets the
// disk iff its supporting line does and the contact lies ahead of the source.

TEST_CASE("Rays pointing away from a disk") {
    using Point = pgl::Point<int>;
    using Ray = pgl::Ray<Point>;
    using Disk = pgl::Disk<Point>;

    Disk d(0, 0, 5);

    SUBCASE("source outside, heading further out") {
        Ray r(Point(10, 0), Point(11, 0));
        CHECK_FALSE_MESSAGE(r.intersects(d), r, " intersects ", d);
        CHECK_FALSE_MESSAGE(d.intersects(r), d, " intersects ", r);
        CHECK_FALSE_MESSAGE(d.interiorsIntersect(r), d, " interiorsIntersect ", r);
    }

    SUBCASE("supporting line is a secant but the disk is behind the source") {
        // The whole x-axis cuts the disk, yet this ray leaves to the right.
        Ray r(Point(6, 0), Point(7, 0));
        CHECK_FALSE_MESSAGE(d.intersects(r), d, " intersects ", r);
        CHECK_FALSE_MESSAGE(d.interiorsIntersect(r), d, " interiorsIntersect ", r);
    }

    SUBCASE("forward but parallel near-miss") {
        Ray r(Point(6, -10), Point(6, 10));
        CHECK_FALSE_MESSAGE(d.intersects(r), d, " intersects ", r);
        CHECK_FALSE_MESSAGE(d.interiorsIntersect(r), d, " interiorsIntersect ", r);
    }
}

TEST_CASE("Rays driving into a disk") {
    using Point = pgl::Point<int>;
    using Ray = pgl::Ray<Point>;
    using Disk = pgl::Disk<Point>;

    Disk d(0, 0, 5);

    SUBCASE("source outside, aimed through the disk") {
        Ray r(Point(10, 0), Point(-1, 0));
        CHECK_MESSAGE(r.intersects(d), r, " intersects ", d);
        CHECK_MESSAGE(d.intersects(r), d, " intersects ", r);
        CHECK_MESSAGE(d.interiorsIntersect(r), d, " interiorsIntersect ", r);
        CHECK_MESSAGE(r.interiorsIntersect(d), r, " interiorsIntersect ", d);
    }

    SUBCASE("source strictly inside") {
        Ray r(Point(0, 0), Point(20, 1));
        CHECK_MESSAGE(d.intersects(r), d, " intersects ", r);
        CHECK_MESSAGE(d.interiorsIntersect(r), d, " interiorsIntersect ", r);
    }

    SUBCASE("source outside aimed at the near edge") {
        Ray r(Point(6, 0), Point(5, 0));
        CHECK_MESSAGE(d.intersects(r), d, " intersects ", r);
        CHECK_MESSAGE(d.interiorsIntersect(r), d, " interiorsIntersect ", r);
    }
}

TEST_CASE("Rays whose source sits on the boundary circle") {
    using Point = pgl::Point<int>;
    using Ray = pgl::Ray<Point>;
    using Disk = pgl::Disk<Point>;

    Disk d(0, 0, 5);

    SUBCASE("heading inward") {
        Ray r(Point(5, 0), Point(0, 0));
        CHECK_MESSAGE(d.intersects(r), d, " intersects ", r);
        CHECK_MESSAGE(d.interiorsIntersect(r), d, " interiorsIntersect ", r);
    }

    SUBCASE("heading outward only touches the boundary") {
        Ray r(Point(5, 0), Point(10, 0));
        CHECK_MESSAGE(d.intersects(r), d, " intersects ", r);
        CHECK_FALSE_MESSAGE(d.interiorsIntersect(r), d, " interiorsIntersect ", r);
    }

    SUBCASE("running along the tangent only touches the boundary") {
        Ray r(Point(5, 0), Point(5, 5));
        CHECK_MESSAGE(d.intersects(r), d, " intersects ", r);
        CHECK_FALSE_MESSAGE(d.interiorsIntersect(r), d, " interiorsIntersect ", r);
    }
}

TEST_CASE("Ray against a disk built from three arbitrary boundary points") {
    using Point = pgl::Point<int>;
    using Ray = pgl::Ray<Point>;
    using Disk = pgl::Disk<Point>;

    Disk d(Point(5, 0), Point(-3, 4), Point(0, -5));

    CHECK_MESSAGE(d.interiorsIntersect(Ray(Point(10, 0), Point(-1, 0))), d, " driving through");
    CHECK_FALSE_MESSAGE(d.interiorsIntersect(Ray(Point(10, 0), Point(11, 0))), d, " heading away");
    CHECK_MESSAGE(d.intersects(Ray(Point(5, 0), Point(10, 0))), d, " touches boundary leaving");
    CHECK_FALSE_MESSAGE(d.interiorsIntersect(Ray(Point(5, 0), Point(10, 0))), d, " boundary only");
}

TEST_CASE("Generated consistency for rays around a disk") {
    using Point = pgl::Point<int>;
    using Ray = pgl::Ray<Point>;
    using Disk = pgl::Disk<Point>;

    Disk d(0, 0, 5);

    auto sx = GENERATE(-7, -2, 0, 6);
    auto sy = GENERATE(-6, 0, 3, 8);
    auto tx = GENERATE(-5, 1, 7);
    auto ty = GENERATE(-4, 0, 5);
    if (Point(sx, sy) == Point(tx, ty)) {
        return;  // a ray needs a direction
    }
    Ray r(Point(sx, sy), Point(tx, ty));

    const bool meets = d.intersects(r);
    const bool meets_interior = d.interiorsIntersect(r);

    // Both relations are symmetric.
    CHECK_MESSAGE(meets == r.intersects(d), r, " intersects symmetry ", d);
    CHECK_MESSAGE(meets_interior == r.interiorsIntersect(d), r, " interiorsIntersect symmetry ", d);

    // Meeting the open disk implies touching the closed disk.
    if (meets_interior) {
        CHECK_MESSAGE(meets, d, " must intersect when interiors meet ", r);
    }

    // A source in the closed disk forces an intersection; a source strictly
    // inside forces an interior intersection.
    if (d.contains(r.source())) {
        CHECK_MESSAGE(meets, d, " must intersect a contained source of ", r);
    }
    if (d.interiorContains(r.source())) {
        CHECK_MESSAGE(meets_interior, d, " interior must meet an interior source of ", r);
    }
}
