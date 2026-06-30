#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <variant>
#include <vector>

#include "pgl.hpp"


TEST_CASE("Convex boundaryContains and intersection predicates with OrientedLine") {
    using Point = pgl::Point<int>;
    using OrientedLine = pgl::OrientedLine<Point>;
    using Convex = pgl::Convex<Point>;

    // unit square (0,0)-(4,0)-(4,4)-(0,4)
    const Convex sq(std::vector<Point>{{0, 0}, {4, 0}, {4, 4}, {0, 4}});

    SUBCASE("non-degenerate OrientedLine is never on the boundary of a Convex") {
        const OrientedLine edge_ol({0, 0}, {4, 0});   // along the bottom edge direction
        CHECK_FALSE_MESSAGE(sq.boundaryContains(edge_ol), sq, " boundaryContains ", edge_ol);
    }

    SUBCASE("OrientedLine crossing the interior: intersects and interiorsIntersect") {
        const OrientedLine crossing({2, -1}, {2, 5});
        CHECK_MESSAGE(sq.intersects(crossing), sq, " intersects ", crossing);
        CHECK_MESSAGE(crossing.intersects(sq), crossing, " intersects ", sq);
        CHECK_MESSAGE(sq.interiorsIntersect(crossing), sq, " interiorsIntersect ", crossing);
        CHECK_MESSAGE(crossing.interiorsIntersect(sq), crossing, " interiorsIntersect ", sq);
    }

    SUBCASE("OrientedLine tangent to an edge: intersects but interiors do not") {
        const OrientedLine tangent({4, 0}, {0, 0});    // along the bottom edge y=0
        CHECK_MESSAGE(sq.intersects(tangent), sq, " intersects ", tangent);
        CHECK_FALSE_MESSAGE(sq.interiorsIntersect(tangent), sq, " interiorsIntersect ", tangent);
    }

    SUBCASE("OrientedLine outside: no intersection") {
        const OrientedLine outside({0, 5}, {4, 5});
        CHECK_FALSE_MESSAGE(sq.intersects(outside), sq, " intersects ", outside);
        CHECK_FALSE_MESSAGE(outside.intersects(sq), outside, " intersects ", sq);
    }
}

TEST_CASE("Convex separates and crosses with OrientedLine") {
    using Point = pgl::Point<int>;
    using OrientedLine = pgl::OrientedLine<Point>;
    using Convex = pgl::Convex<Point>;

    const Convex sq(std::vector<Point>{{0, 0}, {4, 0}, {4, 4}, {0, 4}});

    SUBCASE("OrientedLine cutting through the interior separates and crosses the Convex") {
        const OrientedLine crossing({2, -1}, {2, 5});
        CHECK_MESSAGE(sq.separates(crossing), sq, " separates ", crossing);
        CHECK_MESSAGE(sq.crosses(crossing), sq, " crosses ", crossing);
        CHECK_MESSAGE(crossing.separates(sq), crossing, " separates ", sq);
        CHECK_MESSAGE(crossing.crosses(sq), crossing, " crosses ", sq);
    }

    SUBCASE("OrientedLine tangent to an edge does not cross") {
        const OrientedLine tangent({4, 0}, {0, 0});
        CHECK_FALSE_MESSAGE(sq.crosses(tangent), sq, " crosses ", tangent);
    }
}

TEST_CASE("Convex intersection with OrientedLine yields a chord segment") {
    using Point = pgl::Point<int>;
    using OrientedLine = pgl::OrientedLine<Point>;
    using Convex = pgl::Convex<Point>;
    using Segment = pgl::Segment<Point>;

    const Convex sq(std::vector<Point>{{0, 0}, {4, 0}, {4, 4}, {0, 4}});

    SUBCASE("crossing OrientedLine clips to the chord segment") {
        const OrientedLine crossing({2, -1}, {2, 5});
        const auto r = sq.intersection(crossing);
        REQUIRE(r);
        REQUIRE(std::holds_alternative<Segment>(*r));
        CHECK_MESSAGE(std::get<Segment>(*r) == Segment({2, 0}, {2, 4}), "sq ∩ crossing");
    }

    SUBCASE("OrientedLine outside yields empty") {
        const OrientedLine outside({0, 5}, {4, 5});
        CHECK_FALSE_MESSAGE(sq.intersection(outside), "sq ∩ outside should be empty");
    }
}
