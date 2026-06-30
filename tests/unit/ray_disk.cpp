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

// A disk's circular boundary can only hold a degenerate (point-like) ray, so for
// a real ray the boundary-containment is false.
TEST_CASE("Disk boundary never contains a Ray") {
    using Point = pgl::Point<int>;
    using Ray = pgl::Ray<Point>;
    using Disk = pgl::Disk<Point>;

    Disk d(0, 0, 5);
    CHECK_FALSE_MESSAGE(d.boundaryContains(Ray(Point(10, 0), Point(-1, 0))),
                        d, " boundaryContains ", Ray(Point(10, 0), Point(-1, 0)));
}

// separates / crosses: a ray that drives all the way through the disk cuts it in
// two and is itself split into two outside pieces, so it separates and crosses
// both ways. A ray launched from inside, a boundary-tangent ray, and a miss
// separate neither and never cross.
TEST_CASE("Ray and Disk separation and crossing") {
    using Point = pgl::Point<int>;
    using Ray = pgl::Ray<Point>;
    using Disk = pgl::Disk<Point>;

    Disk d(0, 0, 5);

    SUBCASE("a ray driving through the whole disk cuts and crosses both ways") {
        Ray r(Point(10, 0), Point(-1, 0));  // enters at (5,0), exits at (-5,0)
        CHECK_MESSAGE(r.separates(d), r, " separates ", d);
        CHECK_MESSAGE(d.separates(r), d, " separates ", r);
        CHECK_MESSAGE(r.crosses(d), r, " crosses ", d);
        CHECK_MESSAGE(d.crosses(r), d, " crosses ", r);
    }

    SUBCASE("a ray launched from inside leaves a slit, not a cut") {
        Ray r(Point(0, 0), Point(20, 1));
        CHECK_FALSE_MESSAGE(r.separates(d), r, " separates ", d);
        CHECK_FALSE_MESSAGE(d.separates(r), d, " separates ", r);
        CHECK_FALSE_MESSAGE(r.crosses(d), r, " crosses ", d);
        CHECK_FALSE_MESSAGE(d.crosses(r), d, " crosses ", r);
    }

    SUBCASE("a ray grazing the boundary separates neither") {
        Ray r(Point(5, 0), Point(5, 5));  // tangent, from (5,0) upward
        CHECK_FALSE_MESSAGE(r.separates(d), r, " separates ", d);
        CHECK_FALSE_MESSAGE(d.separates(r), d, " separates ", r);
        CHECK_FALSE_MESSAGE(r.crosses(d), r, " crosses ", d);
        CHECK_FALSE_MESSAGE(d.crosses(r), d, " crosses ", r);
    }

    SUBCASE("a ray missing the disk separates neither") {
        Ray r(Point(10, 0), Point(11, 0));
        CHECK_FALSE_MESSAGE(r.separates(d), r, " separates ", d);
        CHECK_FALSE_MESSAGE(d.separates(r), d, " separates ", r);
        CHECK_FALSE_MESSAGE(r.crosses(d), r, " crosses ", d);
        CHECK_FALSE_MESSAGE(d.crosses(r), d, " crosses ", r);
    }
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
