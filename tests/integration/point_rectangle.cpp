#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstdint>

#include "pgl.hpp"


TEST_CASE("Point and Rectangle containment predicates") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;

    const Rectangle outer({0, 0}, {4, 3});
    const Point interior(2, 2);
    const Point edge(4, 2);     // on the right edge (boundary)
    const Point corner(4, 3);   // corner (boundary)
    const Point outside(5, 2);

    CHECK_MESSAGE(outer.contains(interior), outer, " contains ", interior);
    CHECK_MESSAGE(outer.contains(corner), outer, " contains ", corner);
    CHECK_FALSE_MESSAGE(outer.contains(outside), outer, " contains ", outside);

    CHECK_MESSAGE(outer.interiorContains(interior), outer, " interiorContains ", interior);
    CHECK_FALSE_MESSAGE(outer.interiorContains(corner), outer, " interiorContains ", corner);

    CHECK_MESSAGE(outer.boundaryContains(edge), outer, " boundaryContains ", edge);
    CHECK_FALSE_MESSAGE(outer.boundaryContains(interior), outer, " boundaryContains ", interior);

    // A point contains a rectangle only when the rectangle degenerates to it.
    const Rectangle degenerate({5, 2}, {5, 2});
    CHECK_MESSAGE(outside.contains(degenerate), outside, " contains ", degenerate);
    CHECK_MESSAGE(outside.interiorContains(degenerate), outside, " interiorContains ", degenerate);
    CHECK_FALSE_MESSAGE(interior.contains(outer), interior, " contains ", outer);
}

TEST_CASE("Point and Rectangle intersection predicates, both directions") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;

    const Rectangle outer({0, 0}, {4, 3});
    const Point interior(2, 2);
    const Point corner(4, 3);
    const Point outside(5, 2);

    CHECK_MESSAGE(outer.intersects(interior), outer, " intersects ", interior);
    CHECK_MESSAGE(interior.intersects(outer), interior, " intersects ", outer);
    CHECK_MESSAGE(outer.intersects(corner), outer, " intersects ", corner);
    CHECK_FALSE_MESSAGE(outer.intersects(outside), outer, " intersects ", outside);
    CHECK_FALSE_MESSAGE(outside.intersects(outer), outside, " intersects ", outer);

    CHECK_MESSAGE(outer.interiorsIntersect(interior), outer, " interiorsIntersect ", interior);
    CHECK_MESSAGE(interior.interiorsIntersect(outer), interior, " interiorsIntersect ", outer);
    CHECK_FALSE_MESSAGE(outer.interiorsIntersect(corner), outer, " interiorsIntersect ", corner);
}

TEST_CASE("Point and Rectangle never separate and never cross") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;

    const Rectangle outer({0, 0}, {4, 3});
    const Point interior(2, 2);

    CHECK_FALSE_MESSAGE(interior.separates(outer), interior, " separates ", outer);
    CHECK_FALSE_MESSAGE(outer.separates(interior), outer, " separates ", interior);
    CHECK_FALSE_MESSAGE(interior.crosses(outer), interior, " crosses ", outer);
    CHECK_FALSE_MESSAGE(outer.crosses(interior), outer, " crosses ", interior);
}

TEST_CASE("Point and Rectangle intersection construction, both directions") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;

    const Rectangle outer({0, 0}, {4, 3});

    SUBCASE("an interior point yields that point") {
        const Point interior(2, 2);
        const auto fromRect = outer.intersection(interior);
        REQUIRE(fromRect.has_value());
        CHECK(*fromRect == interior);
        const auto fromPt = interior.intersection(outer);
        REQUIRE(fromPt.has_value());
        CHECK(*fromPt == interior);
    }

    SUBCASE("a boundary point yields that point") {
        const Point corner(4, 3);
        const auto fromRect = outer.intersection(corner);
        REQUIRE(fromRect.has_value());
        CHECK(*fromRect == corner);
    }

    SUBCASE("an outside point yields nothing") {
        const Point outside(5, 2);
        CHECK_FALSE(outer.intersection(outside).has_value());
        CHECK_FALSE(outside.intersection(outer).has_value());
    }
}
