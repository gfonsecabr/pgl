#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstdint>
#include <vector>

#include "pgl.hpp"


TEST_CASE("Point and Convex containment predicates") {
    using Point = pgl::Point<int>;
    using Convex = pgl::Convex<Point>;

    const Convex sq(std::vector<Point>{{0, 0}, {4, 0}, {4, 4}, {0, 4}});
    const Point interior(2, 2);
    const Point edge(2, 0);     // on an edge (boundary)
    const Point vertex(0, 0);   // a vertex (boundary)
    const Point outside(5, 2);

    CHECK_MESSAGE(sq.contains(interior), sq, " contains ", interior);
    CHECK_MESSAGE(sq.contains(edge), sq, " contains ", edge);
    CHECK_MESSAGE(sq.contains(vertex), sq, " contains ", vertex);
    CHECK_FALSE_MESSAGE(sq.contains(outside), sq, " contains ", outside);

    CHECK_MESSAGE(sq.interiorContains(interior), sq, " interiorContains ", interior);
    CHECK_FALSE_MESSAGE(sq.interiorContains(edge), sq, " interiorContains ", edge);
    CHECK_FALSE_MESSAGE(sq.interiorContains(vertex), sq, " interiorContains ", vertex);

    CHECK_MESSAGE(sq.boundaryContains(edge), sq, " boundaryContains ", edge);
    CHECK_MESSAGE(sq.boundaryContains(vertex), sq, " boundaryContains ", vertex);
    CHECK_FALSE_MESSAGE(sq.boundaryContains(interior), sq, " boundaryContains ", interior);

    // A point never contains a (2D) convex polygon.
    CHECK_FALSE_MESSAGE(interior.contains(sq), interior, " contains ", sq);
}

TEST_CASE("Point and Convex intersection predicates, both directions") {
    using Point = pgl::Point<int>;
    using Convex = pgl::Convex<Point>;

    const Convex sq(std::vector<Point>{{0, 0}, {4, 0}, {4, 4}, {0, 4}});
    const Point interior(2, 2);
    const Point edge(2, 0);
    const Point outside(5, 2);

    CHECK_MESSAGE(sq.intersects(interior), sq, " intersects ", interior);
    CHECK_MESSAGE(interior.intersects(sq), interior, " intersects ", sq);
    CHECK_MESSAGE(sq.intersects(edge), sq, " intersects ", edge);
    CHECK_FALSE_MESSAGE(sq.intersects(outside), sq, " intersects ", outside);
    CHECK_FALSE_MESSAGE(outside.intersects(sq), outside, " intersects ", sq);

    CHECK_MESSAGE(sq.interiorsIntersect(interior), sq, " interiorsIntersect ", interior);
    CHECK_MESSAGE(interior.interiorsIntersect(sq), interior, " interiorsIntersect ", sq);
    CHECK_FALSE_MESSAGE(sq.interiorsIntersect(edge), sq, " interiorsIntersect ", edge);
}

TEST_CASE("Point and Convex never separate and never cross") {
    using Point = pgl::Point<int>;
    using Convex = pgl::Convex<Point>;

    const Convex sq(std::vector<Point>{{0, 0}, {4, 0}, {4, 4}, {0, 4}});
    const Point interior(2, 2);

    CHECK_FALSE_MESSAGE(interior.separates(sq), interior, " separates ", sq);
    CHECK_FALSE_MESSAGE(sq.separates(interior), sq, " separates ", interior);
    CHECK_FALSE_MESSAGE(interior.crosses(sq), interior, " crosses ", sq);
    CHECK_FALSE_MESSAGE(sq.crosses(interior), sq, " crosses ", interior);
}

TEST_CASE("Point and Convex intersection construction, both directions") {
    using Point = pgl::Point<int>;
    using Convex = pgl::Convex<Point>;

    const Convex sq(std::vector<Point>{{0, 0}, {4, 0}, {4, 4}, {0, 4}});

    SUBCASE("an interior point yields that point") {
        const Point interior(2, 2);
        const auto fromConvex = sq.intersection(interior);
        REQUIRE(fromConvex.has_value());
        CHECK(*fromConvex == interior);
        const auto fromPt = interior.intersection(sq);
        REQUIRE(fromPt.has_value());
        CHECK(*fromPt == interior);
    }

    SUBCASE("an outside point yields nothing") {
        const Point outside(5, 2);
        CHECK_FALSE(sq.intersection(outside).has_value());
        CHECK_FALSE(outside.intersection(sq).has_value());
    }
}

TEST_CASE("Point and Convex squared Hausdorff distance") {
    using Point = pgl::Point<int>;
    using Convex = pgl::Convex<Point>;

    const Convex sq(std::vector<Point>{{0, 0}, {4, 0}, {4, 4}, {0, 4}});
    const Point p(6, 1);

    // Farthest square vertex from p is (0,4) at squared distance 45, which
    // dominates the point-side (nearest-point) term.
    CHECK(sq.squaredHausdorffDistance(p) == 45);
    CHECK(p.squaredHausdorffDistance(sq) == 45);
}
