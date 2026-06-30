#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <variant>

#include "pgl.hpp"


TEST_CASE("Rectangle intersection predicates with OrientedLine") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;
    using OrientedLine = pgl::OrientedLine<Point>;

    const Rectangle rect({0, 0}, {4, 3});
    const OrientedLine crossing({2, -1}, {2, 4});      // enters bottom, exits top
    const OrientedLine tangent({4, 0}, {0, 0});         // along the bottom edge
    const OrientedLine corner({1, -1}, {-1, 1});        // grazes the corner (0,0)
    const OrientedLine outside({0, 5}, {4, 5});         // entirely above

    SUBCASE("intersects: crossing, tangent, corner all intersect; outside does not") {
        CHECK_MESSAGE(rect.intersects(crossing), rect, " intersects ", crossing);
        CHECK_MESSAGE(crossing.intersects(rect), crossing, " intersects ", rect);
        CHECK_MESSAGE(rect.intersects(tangent), rect, " intersects ", tangent);
        CHECK_MESSAGE(rect.intersects(corner), rect, " intersects ", corner);
        CHECK_FALSE_MESSAGE(rect.intersects(outside), rect, " intersects ", outside);
        CHECK_FALSE_MESSAGE(outside.intersects(rect), outside, " intersects ", rect);
    }

    SUBCASE("interiorsIntersect: only a true interior crossing qualifies") {
        CHECK_MESSAGE(rect.interiorsIntersect(crossing), rect, " interiorsIntersect ", crossing);
        CHECK_MESSAGE(crossing.interiorsIntersect(rect), crossing, " interiorsIntersect ", rect);
        CHECK_FALSE_MESSAGE(rect.interiorsIntersect(tangent), rect, " interiorsIntersect ", tangent);
        CHECK_FALSE_MESSAGE(rect.interiorsIntersect(corner), rect, " interiorsIntersect ", corner);
        CHECK_FALSE_MESSAGE(rect.interiorsIntersect(outside), rect, " interiorsIntersect ", outside);
    }
}

TEST_CASE("Rectangle separates and crosses with OrientedLine") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;
    using OrientedLine = pgl::OrientedLine<Point>;

    const Rectangle rect({0, 0}, {4, 3});
    const OrientedLine crossing({2, -1}, {2, 4});

    SUBCASE("an OrientedLine cutting through the interior separates and crosses the Rectangle") {
        CHECK_MESSAGE(rect.separates(crossing), rect, " separates ", crossing);
        CHECK_MESSAGE(rect.crosses(crossing), rect, " crosses ", crossing);
    }

    SUBCASE("OrientedLine.separates(Rectangle) and crosses(Rectangle) both directions") {
        // The oriented line x=5, pointing right, is to the right of the rectangle.
        // It separates the rectangle (cuts through it fully from -∞ to +∞).
        const OrientedLine separator({5, 1}, {-1, 1});  // y=1 passing through rect
        CHECK_MESSAGE(separator.separates(rect), separator, " separates ", rect);
        CHECK_MESSAGE(separator.crosses(rect), separator, " crosses ", rect);
    }

    SUBCASE("tangent line does not cross") {
        const OrientedLine tangent({4, 0}, {0, 0});
        CHECK_FALSE_MESSAGE(rect.crosses(tangent), rect, " crosses ", tangent);
    }
}

TEST_CASE("Rectangle intersection construction with OrientedLine") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;
    using Segment = pgl::Segment<Point>;
    using OrientedLine = pgl::OrientedLine<Point>;

    const Rectangle rect({0, 0}, {4, 3});

    SUBCASE("a crossing OrientedLine clips to the interior chord segment") {
        const OrientedLine crossing({2, -1}, {2, 4});
        const auto r = rect.intersection(crossing);
        REQUIRE(r);
        REQUIRE(std::holds_alternative<Segment>(*r));
        CHECK_MESSAGE(std::get<Segment>(*r) == Segment({2, 0}, {2, 3}),
                      "rect ∩ crossing OL");
    }

    SUBCASE("OrientedLine outside yields empty") {
        const OrientedLine outside({0, 5}, {4, 5});
        CHECK_FALSE_MESSAGE(rect.intersection(outside), "rect ∩ outside OL should be empty");
    }
}
