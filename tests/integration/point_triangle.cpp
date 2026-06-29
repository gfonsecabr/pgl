#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstdint>

#include "pgl.hpp"


TEST_CASE("Point and Triangle containment predicates") {
    using Point = pgl::Point<int>;
    using Triangle = pgl::Triangle<Point>;

    const Triangle triangle(0, 0, 6, 0, 0, 6);
    const Point interior(1, 1);
    const Point edge(3, 0);     // on the base edge (boundary)
    const Point outside(4, 4);

    CHECK_MESSAGE(triangle.contains(interior), triangle, " contains ", interior);
    CHECK_MESSAGE(triangle.contains(edge), triangle, " contains ", edge);
    CHECK_FALSE_MESSAGE(triangle.contains(outside), triangle, " contains ", outside);

    CHECK_MESSAGE(triangle.interiorContains(interior), triangle, " interiorContains ", interior);
    CHECK_FALSE_MESSAGE(triangle.interiorContains(edge), triangle, " interiorContains ", edge);

    CHECK_MESSAGE(triangle.boundaryContains(edge), triangle, " boundaryContains ", edge);
    CHECK_FALSE_MESSAGE(triangle.boundaryContains(interior), triangle, " boundaryContains ", interior);

    // A point never contains a (2D) triangle.
    CHECK_FALSE_MESSAGE(interior.contains(triangle), interior, " contains ", triangle);
}

TEST_CASE("Point and Triangle intersection predicates, both directions") {
    using Point = pgl::Point<int>;
    using Triangle = pgl::Triangle<Point>;

    const Triangle triangle(0, 0, 6, 0, 0, 6);
    const Point interior(1, 1);
    const Point edge(3, 0);
    const Point outside(4, 4);

    CHECK_MESSAGE(triangle.intersects(interior), triangle, " intersects ", interior);
    CHECK_MESSAGE(interior.intersects(triangle), interior, " intersects ", triangle);
    CHECK_MESSAGE(triangle.intersects(edge), triangle, " intersects ", edge);
    CHECK_FALSE_MESSAGE(triangle.intersects(outside), triangle, " intersects ", outside);
    CHECK_FALSE_MESSAGE(outside.intersects(triangle), outside, " intersects ", triangle);

    CHECK_MESSAGE(triangle.interiorsIntersect(interior), triangle, " interiorsIntersect ", interior);
    CHECK_MESSAGE(interior.interiorsIntersect(triangle), interior, " interiorsIntersect ", triangle);
    CHECK_FALSE_MESSAGE(triangle.interiorsIntersect(edge), triangle, " interiorsIntersect ", edge);
    CHECK_FALSE_MESSAGE(triangle.interiorsIntersect(outside), triangle, " interiorsIntersect ", outside);
}

TEST_CASE("Point and Triangle never separate and never cross") {
    using Point = pgl::Point<int>;
    using Triangle = pgl::Triangle<Point>;

    const Triangle triangle(0, 0, 6, 0, 0, 6);
    const Point interior(1, 1);

    CHECK_FALSE_MESSAGE(interior.separates(triangle), interior, " separates ", triangle);
    CHECK_FALSE_MESSAGE(triangle.separates(interior), triangle, " separates ", interior);
    CHECK_FALSE_MESSAGE(interior.crosses(triangle), interior, " crosses ", triangle);
    CHECK_FALSE_MESSAGE(triangle.crosses(interior), triangle, " crosses ", interior);
}

TEST_CASE("Point and Triangle intersection construction, both directions") {
    using Point = pgl::Point<int>;
    using Triangle = pgl::Triangle<Point>;

    const Triangle triangle(0, 0, 6, 0, 0, 6);

    SUBCASE("an interior point yields that point") {
        const Point interior(1, 1);
        const auto fromTri = triangle.intersection(interior);
        REQUIRE(fromTri.has_value());
        CHECK(*fromTri == interior);
        const auto fromPt = interior.intersection(triangle);
        REQUIRE(fromPt.has_value());
        CHECK(*fromPt == interior);
    }

    SUBCASE("a boundary point yields that point") {
        const Point edge(3, 0);
        const auto fromTri = triangle.intersection(edge);
        REQUIRE(fromTri.has_value());
        CHECK(*fromTri == edge);
    }

    SUBCASE("an outside point yields nothing") {
        const Point outside(4, 4);
        CHECK_FALSE(triangle.intersection(outside).has_value());
        CHECK_FALSE(outside.intersection(triangle).has_value());
    }
}
