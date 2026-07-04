#include "pgl.hpp"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <variant>
#include <vector>

TEST_CASE("Convex boundaryContains Triangle") {
    using Point = pgl::Point<int>;
    using Convex = pgl::Convex<Point>;
    using Triangle = pgl::Triangle<Point>;

    const Convex sq(std::vector<Point>{{0, 0}, {4, 0}, {4, 4}, {0, 4}});

    // Degenerate (collinear) triangle along the bottom edge.
    const Triangle deg(Point(0, 0), Point(2, 0), Point(4, 0));
    CHECK_MESSAGE(sq.boundaryContains(deg), sq, " boundaryContains degenerate triangle on edge");

    // Proper triangle strictly inside is not on the boundary.
    const Triangle inner(Point(1, 1), Point(3, 1), Point(2, 3));
    CHECK_FALSE_MESSAGE(sq.boundaryContains(inner), sq, " boundaryContains inner triangle");
}

TEST_CASE("Convex contains and interiorContains Triangle") {
    using Point = pgl::Point<int>;
    using Convex = pgl::Convex<Point>;
    using Triangle = pgl::Triangle<Point>;

    const Convex sq(std::vector<Point>{{0, 0}, {4, 0}, {4, 4}, {0, 4}});

    const Triangle inner({1, 1}, {3, 1}, {2, 3});
    const Triangle on_edge({0, 0}, {4, 0}, {2, 2});
    const Triangle poking({1, 1}, {6, 1}, {2, 3});
    const Triangle outside({5, 5}, {7, 5}, {6, 7});

    CHECK_MESSAGE(sq.contains(inner), sq, " contains inner triangle");
    CHECK_MESSAGE(sq.interiorContains(inner), sq, " interiorContains inner triangle");
    CHECK_MESSAGE(sq.contains(on_edge), sq, " contains edge-vertex triangle");
    CHECK_FALSE_MESSAGE(sq.interiorContains(on_edge), sq, " interiorContains edge-vertex triangle");
    CHECK_FALSE_MESSAGE(sq.contains(poking), sq, " contains poking triangle");
    CHECK_FALSE_MESSAGE(sq.contains(outside), sq, " contains outside triangle");
}

TEST_CASE("Convex and Triangle intersection predicates, both directions") {
    using Point = pgl::Point<int>;
    using Convex = pgl::Convex<Point>;
    using Triangle = pgl::Triangle<Point>;

    const Convex sq(std::vector<Point>{{0, 0}, {4, 0}, {4, 4}, {0, 4}});

    SUBCASE("overlapping: both intersect, interiors intersect") {
        const Triangle t({2, 2}, {6, 2}, {4, 6});
        CHECK_MESSAGE(sq.intersects(t), sq, " intersects ", t);
        CHECK_MESSAGE(t.intersects(sq), t, " intersects ", sq);
        CHECK_MESSAGE(sq.interiorsIntersect(t), sq, " interiorsIntersect ", t);
        CHECK_MESSAGE(t.interiorsIntersect(sq), t, " interiorsIntersect ", sq);
    }

    SUBCASE("disjoint: neither intersects") {
        const Triangle t({10, 10}, {12, 10}, {11, 12});
        CHECK_FALSE_MESSAGE(sq.intersects(t), sq, " intersects disjoint triangle");
        CHECK_FALSE_MESSAGE(t.intersects(sq), "disjoint triangle intersects ", sq);
    }

    SUBCASE("triangle fully inside convex: intersects but interiors only if triangle has area") {
        const Triangle inner({1, 1}, {3, 1}, {2, 3});
        CHECK_MESSAGE(sq.intersects(inner), sq, " intersects inner triangle");
        CHECK_MESSAGE(sq.interiorsIntersect(inner), sq, " interiorsIntersect inner triangle");
        CHECK_MESSAGE(inner.interiorsIntersect(sq), "inner triangle interiorsIntersect ", sq);
    }
}

TEST_CASE("Convex separates Triangle and Triangle separates Convex") {
    using Point = pgl::Point<int>;
    using Convex = pgl::Convex<Point>;
    using Triangle = pgl::Triangle<Point>;

    const Convex sq(std::vector<Point>{{0, 0}, {4, 0}, {4, 4}, {0, 4}});

    // Triangle with two vertices outside on opposite sides separates the square.
    const Triangle t({-1, 1}, {5, 2}, {-1, 3});
    CHECK_MESSAGE(sq.separates(t), sq, " separates spanning triangle");

    // Triangle entirely inside is not separated.
    const Triangle inner({1, 1}, {3, 1}, {2, 3});
    CHECK_FALSE_MESSAGE(sq.separates(inner), sq, " separates inner triangle");

    // Triangle that penetrates the convex from one side.
    const Convex tri(std::vector<Point>{{0, 0}, {4, 0}, {0, 4}});
    const Triangle cutting({2, -1}, {4, -1}, {3, 5});
    CHECK_MESSAGE(cutting.separates(tri), cutting, " separates convex");
    CHECK_MESSAGE(tri.separates(cutting), tri, " separates cutting triangle");
}

TEST_CASE("Convex crosses Triangle") {
    using Point = pgl::Point<int>;
    using Convex = pgl::Convex<Point>;
    using Triangle = pgl::Triangle<Point>;

    const Convex sq(std::vector<Point>{{0, 0}, {4, 0}, {4, 4}, {0, 4}});

    const Triangle spanning({-1, 1}, {5, 2}, {-1, 3});
    CHECK_MESSAGE(sq.crosses(spanning), sq, " crosses spanning triangle");
    CHECK_MESSAGE(spanning.crosses(sq), "spanning triangle crosses ", sq);

    const Triangle contained({1, 1}, {3, 1}, {2, 3});
    CHECK_FALSE_MESSAGE(sq.crosses(contained), sq, " crosses contained triangle");
}

TEST_CASE("Convex intersection with Triangle") {
    using Point = pgl::Point<int>;
    using Convex = pgl::Convex<Point>;
    using Triangle = pgl::Triangle<Point>;
    using Segment = pgl::Segment<Point>;

    const Convex sq(std::vector<Point>{{0, 0}, {4, 0}, {4, 4}, {0, 4}});

    SUBCASE("overlapping area: intersection is a Convex") {
        const Triangle t({2, 2}, {6, 2}, {4, 6});
        const auto result = sq.intersection(t);
        REQUIRE_MESSAGE(result, "sq ∩ overlapping triangle should be non-empty");
        CHECK_MESSAGE(std::holds_alternative<Convex>(*result),
                      "area overlap clips to a Convex");
    }

    SUBCASE("triangle fully inside convex: intersection is the triangle (as Convex)") {
        // Edges along y=1 (horizontal), x=1 (vertical), and y=4-x (slope-1) all
        // intersect the sq boundary at integer points, so int arithmetic is exact.
        const Triangle inner({1, 1}, {3, 1}, {1, 3});
        const auto result = sq.intersection(inner);
        REQUIRE_MESSAGE(result, "sq ∩ inner triangle should be non-empty");
        CHECK_MESSAGE(std::holds_alternative<Convex>(*result),
                      "contained triangle returned as Convex");
    }

    SUBCASE("sharing only an edge: intersection is a Segment") {
        // Triangle whose base lies along the bottom edge of the square (y=0).
        const Triangle t({0, 0}, {4, 0}, {2, -2});
        const auto result = sq.intersection(t);
        REQUIRE_MESSAGE(result, "sq ∩ edge-touching triangle should be non-empty");
        CHECK_MESSAGE(std::holds_alternative<Segment>(*result),
                      "shared-edge intersection is a Segment");
    }

    SUBCASE("disjoint: no intersection") {
        const Triangle t({10, 10}, {12, 10}, {11, 12});
        CHECK_FALSE_MESSAGE(sq.intersection(t), "sq ∩ disjoint triangle should be empty");
    }
}

TEST_CASE("Triangle and Convex squared Hausdorff distance") {
    using Point = pgl::Point<int>;
    using Convex = pgl::Convex<Point>;
    using Triangle = pgl::Triangle<Point>;

    const Convex sq(std::vector<Point>{{0, 0}, {4, 0}, {4, 4}, {0, 4}});
    const Triangle t({8, 0}, {12, 0}, {8, 4});

    // Farthest vertex on either side is at squared distance 64 (opposite corners).
    CHECK(sq.squaredHausdorffDistance(t) == 64);
    CHECK(t.squaredHausdorffDistance(sq) == 64);
}
