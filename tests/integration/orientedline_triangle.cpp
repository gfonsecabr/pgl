#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <variant>

#include "pgl.hpp"


TEST_CASE("Triangle intersection predicates with OrientedLine") {
    using Point = pgl::Point<int>;
    using Triangle = pgl::Triangle<Point>;
    using OrientedLine = pgl::OrientedLine<Point>;

    // triangle with vertices (0,0), (6,0), (0,6)
    const Triangle triangle(0, 0, 6, 0, 0, 6);

    SUBCASE("OrientedLine passing through the interior intersects and interiorsIntersect") {
        const OrientedLine inner({0, 0}, {3, 3});    // diagonal through interior
        CHECK_MESSAGE(triangle.intersects(inner), triangle, " intersects ", inner);
        CHECK_MESSAGE(inner.intersects(triangle), inner, " intersects ", triangle);
        CHECK_MESSAGE(triangle.interiorsIntersect(inner), triangle, " interiorsIntersect ", inner);
        CHECK_MESSAGE(inner.interiorsIntersect(triangle), inner, " interiorsIntersect ", triangle);
    }

    SUBCASE("OrientedLine collinear with an edge: intersects but interiors do not") {
        const OrientedLine edge({0, 0}, {0, 4});     // collinear with the left edge x=0
        CHECK_MESSAGE(triangle.intersects(edge), triangle, " intersects ", edge);
        CHECK_FALSE_MESSAGE(triangle.interiorsIntersect(edge), triangle, " interiorsIntersect ", edge);
    }

    SUBCASE("OrientedLine outside does not intersect") {
        const OrientedLine away({0, 7}, {6, 7});
        CHECK_FALSE_MESSAGE(triangle.intersects(away), triangle, " intersects ", away);
        CHECK_FALSE_MESSAGE(away.intersects(triangle), away, " intersects ", triangle);
    }
}

TEST_CASE("Triangle interiorsIntersect with OrientedLine — diagonal interior hit") {
    using Point = pgl::Point<int>;
    using Triangle = pgl::Triangle<Point>;
    using OrientedLine = pgl::OrientedLine<Point>;

    const Triangle triangle(0, 0, 6, 0, 0, 6);
    const OrientedLine inner({0, 0}, {3, 3});
    CHECK_MESSAGE(triangle.interiorsIntersect(inner), triangle, " interiorsIntersect ", inner);
    CHECK_MESSAGE(inner.interiorsIntersect(triangle), inner, " interiorsIntersect ", triangle);
}

TEST_CASE("Triangle separates and crosses with OrientedLine") {
    using Point = pgl::Point<int>;
    using Triangle = pgl::Triangle<Point>;
    using OrientedLine = pgl::OrientedLine<Point>;

    const Triangle triangle(0, 0, 6, 0, 0, 6);

    SUBCASE("an OrientedLine cutting through the interior separates and crosses the Triangle") {
        const OrientedLine separator({5, 2}, {-1, 2});   // horizontal y=2, cuts the triangle
        CHECK_MESSAGE(triangle.separates(separator), triangle, " separates ", separator);
        CHECK_MESSAGE(triangle.crosses(separator), triangle, " crosses ", separator);
        CHECK_MESSAGE(separator.separates(triangle), separator, " separates ", triangle);
        CHECK_MESSAGE(separator.crosses(triangle), separator, " crosses ", triangle);
    }

    SUBCASE("an OrientedLine only touching the boundary does not separate or cross") {
        const OrientedLine tangent({-1, 1}, {1, -1});    // only touches vertex (0,0)
        CHECK_FALSE_MESSAGE(tangent.separates(triangle), tangent, " separates ", triangle);
        CHECK_FALSE_MESSAGE(tangent.crosses(triangle), tangent, " crosses ", triangle);
    }
}

TEST_CASE("Triangle intersection construction with OrientedLine") {
    using Point = pgl::Point<int>;
    using Triangle = pgl::Triangle<Point>;
    using OrientedLine = pgl::OrientedLine<Point>;
    using Segment = pgl::Segment<Point>;

    const Triangle triangle(0, 0, 4, 0, 0, 4);

    SUBCASE("OrientedLine collinear with edge yields that edge as a Segment") {
        const OrientedLine edge_ol({0, 0}, {0, 4});
        const auto r = triangle.intersection(edge_ol);
        REQUIRE(r);
        REQUIRE(std::holds_alternative<Segment>(*r));
        CHECK_MESSAGE(std::get<Segment>(*r) == Segment({0, 0}, {0, 4}), "triangle ∩ edge OL");
    }

    SUBCASE("OrientedLine outside yields empty") {
        const OrientedLine away({0, 5}, {4, 5});
        CHECK_FALSE_MESSAGE(triangle.intersection(away), "triangle ∩ outside OL should be empty");
    }
}
