#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "pgl.hpp"


TEST_CASE("Point and Disk containment predicates") {
    using Point = pgl::Point<int>;
    using Disk = pgl::Disk<Point>;

    const Disk disk(Point(0, 0), 5);
    const Point center(0, 0);    // interior
    const Point inside(3, 0);    // interior
    const Point boundary(3, 4);  // on the circle (3^2 + 4^2 = 25)
    const Point outside(6, 0);

    CHECK_MESSAGE(disk.contains(center), disk, " contains ", center);
    CHECK_MESSAGE(disk.contains(boundary), disk, " contains ", boundary);
    CHECK_FALSE_MESSAGE(disk.contains(outside), disk, " contains ", outside);

    CHECK_MESSAGE(disk.interiorContains(center), disk, " interiorContains ", center);
    CHECK_MESSAGE(disk.interiorContains(inside), disk, " interiorContains ", inside);
    CHECK_FALSE_MESSAGE(disk.interiorContains(boundary), disk, " interiorContains ", boundary);

    CHECK_MESSAGE(disk.boundaryContains(boundary), disk, " boundaryContains ", boundary);
    CHECK_FALSE_MESSAGE(disk.boundaryContains(center), disk, " boundaryContains ", center);
    CHECK_FALSE_MESSAGE(disk.boundaryContains(outside), disk, " boundaryContains ", outside);

    // A point never contains a (2D) disk.
    CHECK_FALSE_MESSAGE(center.contains(disk), center, " contains ", disk);
}

TEST_CASE("Point and Disk intersection predicates, both directions") {
    using Point = pgl::Point<int>;
    using Disk = pgl::Disk<Point>;

    const Disk disk(Point(0, 0), 5);
    const Point center(0, 0);
    const Point boundary(3, 4);
    const Point outside(6, 0);

    CHECK_MESSAGE(disk.intersects(center), disk, " intersects ", center);
    CHECK_MESSAGE(center.intersects(disk), center, " intersects ", disk);
    CHECK_MESSAGE(disk.intersects(boundary), disk, " intersects ", boundary);
    CHECK_FALSE_MESSAGE(disk.intersects(outside), disk, " intersects ", outside);
    CHECK_FALSE_MESSAGE(outside.intersects(disk), outside, " intersects ", disk);

    CHECK_MESSAGE(disk.interiorsIntersect(center), disk, " interiorsIntersect ", center);
    CHECK_MESSAGE(center.interiorsIntersect(disk), center, " interiorsIntersect ", disk);
    CHECK_FALSE_MESSAGE(disk.interiorsIntersect(boundary), disk, " interiorsIntersect ", boundary);
}

TEST_CASE("Point and Disk never separate and never cross") {
    using Point = pgl::Point<int>;
    using Disk = pgl::Disk<Point>;

    const Disk disk(Point(0, 0), 5);
    const Point center(0, 0);

    CHECK_FALSE_MESSAGE(center.separates(disk), center, " separates ", disk);
    CHECK_FALSE_MESSAGE(disk.separates(center), disk, " separates ", center);
    CHECK_FALSE_MESSAGE(center.crosses(disk), center, " crosses ", disk);
    CHECK_FALSE_MESSAGE(disk.crosses(center), disk, " crosses ", center);
}

TEST_CASE("Point and Disk intersection construction, both directions") {
    using Point = pgl::Point<int>;
    using Disk = pgl::Disk<Point>;

    const Disk disk(Point(0, 0), 5);

    SUBCASE("an interior point yields that point") {
        const Point inside(3, 0);
        const auto fromDisk = disk.intersection(inside);
        REQUIRE(fromDisk.has_value());
        CHECK(*fromDisk == inside);
        const auto fromPt = inside.intersection(disk);
        REQUIRE(fromPt.has_value());
        CHECK(*fromPt == inside);
    }

    SUBCASE("an outside point yields nothing") {
        const Point outside(6, 0);
        CHECK_FALSE(disk.intersection(outside).has_value());
        CHECK_FALSE(outside.intersection(disk).has_value());
    }
}

TEST_CASE("Point and Disk L1/LInf distance, both directions") {
    using Point = pgl::Point<int>;
    using Disk = pgl::Disk<Point>;

    const Disk disk(Point(0, 0), 5);

    SUBCASE("interior and boundary points are at distance zero") {
        CHECK(disk.distanceL1(Point(0, 0)) == doctest::Approx(0.0));
        CHECK(disk.distanceL1(Point(3, 4)) == doctest::Approx(0.0));  // exactly on the circle
        CHECK(disk.distanceLInf(Point(3, 4)) == doctest::Approx(0.0));
    }

    SUBCASE("exterior points on an axis have an exact gap") {
        CHECK(disk.distanceL1(Point(20, 0)) == doctest::Approx(15.0));
        CHECK(disk.distanceLInf(Point(20, 0)) == doctest::Approx(15.0));
        CHECK(disk.distanceL1(Point(6, 0)) == doctest::Approx(1.0));
        CHECK(disk.distanceL1(Point(0, -12)) == doctest::Approx(7.0));
    }

    SUBCASE("a diagonal exterior point matches a fine-grained brute-force scan") {
        const Point p(10, 10);
        double bestL1 = 1e18;
        double bestLInf = 1e18;
        constexpr int steps = 200000;
        for (int i = 0; i < steps; ++i) {
            const double theta = 2.0 * 3.14159265358979323846 * i / steps;
            const double x = 5.0 * std::cos(theta);
            const double y = 5.0 * std::sin(theta);
            const double l1 = std::abs(x - p.x()) + std::abs(y - p.y());
            const double linf = std::max(std::abs(x - p.x()), std::abs(y - p.y()));
            bestL1 = std::min(bestL1, l1);
            bestLInf = std::min(bestLInf, linf);
        }
        CHECK(disk.distanceL1(p) == doctest::Approx(bestL1).epsilon(1e-4));
        CHECK(disk.distanceLInf(p) == doctest::Approx(bestLInf).epsilon(1e-4));
    }

    SUBCASE("the relation is symmetric: the point forwards to the disk") {
        const Point p(20, 0);
        CHECK(p.distanceL1(disk) == disk.distanceL1(p));
        CHECK(p.distanceLInf(disk) == disk.distanceLInf(p));
    }
}
