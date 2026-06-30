#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstdint>

#include "pgl.hpp"


TEST_CASE("Point and Halfplane containment predicates") {
    using Point = pgl::Point<int>;
    using Halfplane = pgl::Halfplane<Point>;

    const Halfplane diagonal({0, 0}, {4, 4});   // closed side y >= x
    const Point interior(0, 1);
    const Point boundary(2, 2);
    const Point outside(1, 0);

    CHECK_MESSAGE(diagonal.contains(interior), diagonal, " contains ", interior);
    CHECK_MESSAGE(diagonal.contains(boundary), diagonal, " contains ", boundary);
    CHECK_FALSE_MESSAGE(diagonal.contains(outside), diagonal, " contains ", outside);

    CHECK_MESSAGE(diagonal.boundaryContains(boundary), diagonal, " boundaryContains ", boundary);
    CHECK_FALSE_MESSAGE(diagonal.boundaryContains(interior), diagonal, " boundaryContains ", interior);

    CHECK_MESSAGE(diagonal.interiorContains(interior), diagonal, " interiorContains ", interior);
    CHECK_FALSE_MESSAGE(diagonal.interiorContains(boundary), diagonal, " interiorContains ", boundary);

    // A point never contains a (2D) halfplane.
    CHECK_FALSE_MESSAGE(interior.contains(diagonal), interior, " contains ", diagonal);
}

TEST_CASE("Point and Halfplane intersection predicates, both directions") {
    using Point = pgl::Point<int>;
    using Halfplane = pgl::Halfplane<Point>;

    const Halfplane diagonal({0, 0}, {4, 4});
    const Point interior(0, 1);
    const Point boundary(2, 2);
    const Point outside(1, 0);

    CHECK_MESSAGE(diagonal.intersects(interior), diagonal, " intersects ", interior);
    CHECK_MESSAGE(interior.intersects(diagonal), interior, " intersects ", diagonal);
    CHECK_MESSAGE(diagonal.intersects(boundary), diagonal, " intersects ", boundary);
    CHECK_FALSE_MESSAGE(diagonal.intersects(outside), diagonal, " intersects ", outside);
    CHECK_FALSE_MESSAGE(outside.intersects(diagonal), outside, " intersects ", diagonal);

    // Interiors meet only for a strictly interior point, not a boundary one.
    CHECK_MESSAGE(diagonal.interiorsIntersect(interior), diagonal, " interiorsIntersect ", interior);
    CHECK_MESSAGE(interior.interiorsIntersect(diagonal), interior, " interiorsIntersect ", diagonal);
    CHECK_FALSE_MESSAGE(diagonal.interiorsIntersect(boundary), diagonal, " interiorsIntersect ", boundary);
}

TEST_CASE("Point and Halfplane never separate and never cross") {
    using Point = pgl::Point<int>;
    using Halfplane = pgl::Halfplane<Point>;

    const Halfplane diagonal({0, 0}, {4, 4});
    const Point interior(0, 1);

    // Removing a point from a 2D region leaves it connected, and a point cannot
    // be split, so neither direction separates or crosses.
    CHECK_FALSE_MESSAGE(interior.separates(diagonal), interior, " separates ", diagonal);
    CHECK_FALSE_MESSAGE(diagonal.separates(interior), diagonal, " separates ", interior);
    CHECK_FALSE_MESSAGE(interior.crosses(diagonal), interior, " crosses ", diagonal);
    CHECK_FALSE_MESSAGE(diagonal.crosses(interior), diagonal, " crosses ", interior);
}

TEST_CASE("Point and Halfplane intersection construction, both directions") {
    using Point = pgl::Point<int>;
    using Halfplane = pgl::Halfplane<Point>;

    const Halfplane diagonal({0, 0}, {4, 4});

    SUBCASE("an interior point yields that point") {
        const Point interior(0, 1);
        const auto fromHp = diagonal.intersection(interior);
        REQUIRE(fromHp.has_value());
        CHECK(*fromHp == interior);
        const auto fromPt = interior.intersection(diagonal);
        REQUIRE(fromPt.has_value());
        CHECK(*fromPt == interior);
    }

    SUBCASE("a boundary point yields that point") {
        const Point boundary(2, 2);
        const auto fromHp = diagonal.intersection(boundary);
        REQUIRE(fromHp.has_value());
        CHECK(*fromHp == boundary);
    }

    SUBCASE("an outside point yields nothing") {
        const Point outside(1, 0);
        CHECK_FALSE(diagonal.intersection(outside).has_value());
        CHECK_FALSE(outside.intersection(diagonal).has_value());
    }
}
