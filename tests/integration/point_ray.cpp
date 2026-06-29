#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstdint>

#include "pgl.hpp"


TEST_CASE("Point and Ray containment predicates") {
    using Point = pgl::Point<int>;
    using Ray = pgl::Ray<Point>;

    const Ray diagonal({0, 0}, {4, 4});   // from (0,0) towards (4,4)
    const Point source(0, 0);   // boundary
    const Point inside(2, 2);   // strict interior
    const Point behind(-1, -1); // on the support line, behind the source
    const Point off(2, 3);

    CHECK_MESSAGE(diagonal.contains(inside), diagonal, " contains ", inside);
    CHECK_MESSAGE(diagonal.contains(source), diagonal, " contains ", source);
    CHECK_FALSE_MESSAGE(diagonal.contains(behind), diagonal, " contains ", behind);
    CHECK_FALSE_MESSAGE(diagonal.contains(off), diagonal, " contains ", off);

    CHECK_MESSAGE(diagonal.boundaryContains(source), diagonal, " boundaryContains ", source);
    CHECK_FALSE_MESSAGE(diagonal.boundaryContains(inside), diagonal, " boundaryContains ", inside);

    CHECK_MESSAGE(diagonal.interiorContains(inside), diagonal, " interiorContains ", inside);
    CHECK_FALSE_MESSAGE(diagonal.interiorContains(source), diagonal, " interiorContains ", source);

    // A point contains a ray only when the ray degenerates to that point.
    const Ray degenerate({2, 3}, {2, 3});
    CHECK_MESSAGE(off.contains(degenerate), off, " contains ", degenerate);
    CHECK_FALSE_MESSAGE(off.contains(diagonal), off, " contains ", diagonal);
}

TEST_CASE("Point and Ray intersection predicates, both directions") {
    using Point = pgl::Point<int>;
    using Ray = pgl::Ray<Point>;

    const Ray diagonal({0, 0}, {4, 4});
    const Point source(0, 0);
    const Point inside(2, 2);
    const Point off(2, 3);

    CHECK_MESSAGE(diagonal.intersects(inside), diagonal, " intersects ", inside);
    CHECK_MESSAGE(inside.intersects(diagonal), inside, " intersects ", diagonal);
    CHECK_MESSAGE(diagonal.intersects(source), diagonal, " intersects ", source);
    CHECK_FALSE_MESSAGE(diagonal.intersects(off), diagonal, " intersects ", off);
    CHECK_FALSE_MESSAGE(off.intersects(diagonal), off, " intersects ", diagonal);

    // Interiors meet only at a strictly interior point (not the source).
    CHECK_MESSAGE(diagonal.interiorsIntersect(inside), diagonal, " interiorsIntersect ", inside);
    CHECK_MESSAGE(inside.interiorsIntersect(diagonal), inside, " interiorsIntersect ", diagonal);
    CHECK_FALSE_MESSAGE(diagonal.interiorsIntersect(source), diagonal, " interiorsIntersect ", source);
    CHECK_FALSE_MESSAGE(diagonal.interiorsIntersect(off), diagonal, " interiorsIntersect ", off);
}

TEST_CASE("Point separates a Ray, never the reverse, and they never cross") {
    using Point = pgl::Point<int>;
    using Ray = pgl::Ray<Point>;

    const Ray diagonal({0, 0}, {4, 4});
    const Point inside(2, 2);   // splits the ray into a segment plus a ray
    const Point source(0, 0);   // removing the source leaves one piece
    const Point off(2, 3);

    CHECK_MESSAGE(inside.separates(diagonal), inside, " separates ", diagonal);
    CHECK_FALSE_MESSAGE(source.separates(diagonal), source, " separates ", diagonal);
    CHECK_FALSE_MESSAGE(off.separates(diagonal), off, " separates ", diagonal);

    CHECK_FALSE_MESSAGE(diagonal.separates(inside), diagonal, " separates ", inside);

    CHECK_FALSE_MESSAGE(inside.crosses(diagonal), inside, " crosses ", diagonal);
    CHECK_FALSE_MESSAGE(diagonal.crosses(inside), diagonal, " crosses ", inside);
}

TEST_CASE("Point and Ray intersection construction, both directions") {
    using Point = pgl::Point<int>;
    using Ray = pgl::Ray<Point>;

    const Ray diagonal({0, 0}, {4, 4});

    SUBCASE("a point on the ray yields that point") {
        const Point inside(2, 2);
        CHECK(diagonal.intersection(inside) == inside);
        const auto fromPt = inside.intersection(diagonal);
        REQUIRE(fromPt.has_value());
        CHECK(*fromPt == inside);
    }

    SUBCASE("a point off the ray yields nothing") {
        const Point off(2, 3);
        CHECK_FALSE(diagonal.intersection(off).has_value());
        CHECK_FALSE(off.intersection(diagonal).has_value());
    }
}
