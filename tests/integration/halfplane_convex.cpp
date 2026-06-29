#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

// Containment: a half-plane (convex, unbounded) can swallow a convex polygon,
// but a bounded convex can never contain or bound an unbounded half-plane.
// Half-plane convention: interior is to the LEFT of source -> target, so
// Halfplane({0,c},{1,c}) -> interior y >= c.
TEST_CASE("Half-plane and Convex containment") {
    using Point = pgl::Point<int>;
    using Convex = pgl::Convex<Point>;
    using Halfplane = pgl::Halfplane<Point>;

    const Convex triangle({{0, 0}, {6, 0}, {0, 6}});

    SUBCASE("a half-plane enclosing the triangle contains it both ways") {
        const Halfplane covers({0, -10}, {1, -10});  // interior y >= -10
        CHECK(covers.contains(triangle));
        CHECK(covers.interiorContains(triangle));
    }

    SUBCASE("a half-plane bounded by an edge contains but does not interior-contain") {
        const Halfplane onEdge({0, 0}, {1, 0});  // interior y >= 0, bottom edge on ∂H
        CHECK(onEdge.contains(triangle));
        CHECK_FALSE(onEdge.interiorContains(triangle));
    }

    SUBCASE("a half-plane cutting through does not contain the triangle") {
        const Halfplane cut({0, 2}, {1, 2});  // interior y >= 2
        CHECK_FALSE(cut.contains(triangle));
        CHECK_FALSE(cut.interiorContains(triangle));
    }

    SUBCASE("a convex never contains or bounds an (unbounded) half-plane") {
        const Halfplane cut({0, 2}, {1, 2});
        CHECK_FALSE(triangle.contains(cut));
        CHECK_FALSE(triangle.interiorContains(cut));
        CHECK_FALSE(triangle.boundaryContains(cut));
    }
}

// separates / crosses: removing a closed half-plane from a convex leaves a
// convex (hence connected) remainder, so a half-plane never separates a convex
// and the two never cross — in either direction.
TEST_CASE("Half-plane and Convex never separate or cross") {
    using Point = pgl::Point<int>;
    using Convex = pgl::Convex<Point>;
    using Halfplane = pgl::Halfplane<Point>;

    const Convex triangle({{0, 0}, {6, 0}, {0, 6}});
    const Halfplane cut({0, 2}, {1, 2});  // boundary y = 2 slices the triangle

    CHECK_FALSE(cut.separates(triangle));
    CHECK_FALSE(triangle.separates(cut));
    CHECK_FALSE(cut.crosses(triangle));
    CHECK_FALSE(triangle.crosses(cut));
}

TEST_CASE("Half-plane and convex predicates are invariant under convex translation") {
    using Point = pgl::Point<int>;
    using Convex = pgl::Convex<Point>;
    using Halfplane = pgl::Halfplane<Point>;

    // Convex::intersects(Halfplane) walks the raw vertices but already adds the
    // translation_ offset before every orientation/containment test, so a rigid
    // translation must not change any answer. Re-run an identical battery in
    // several frames to lock that in.
    for (const Point shift : {Point(0, 0), Point(36, 24), Point(-50, -17)}) {
        Convex triangle({{0, 0}, {6, 0}, {0, 6}});
        triangle += shift;

        // Interior of the half-plane is to the left of source -> target.
        const auto hp = [&shift](Point a, Point b) { return Halfplane(a + shift, b + shift); };

        // Boundary y = 2, interior y >= 2: slices through the triangle.
        const Halfplane cut = hp({0, 2}, {1, 2});
        CHECK(triangle.intersects(cut));
        CHECK(cut.intersects(triangle));
        CHECK(triangle.interiorsIntersect(cut));
        CHECK(cut.interiorsIntersect(triangle));

        // Boundary y = -10, interior y >= -10: covers the whole triangle.
        const Halfplane covers = hp({0, -10}, {1, -10});
        CHECK(triangle.intersects(covers));
        CHECK(covers.intersects(triangle));
        CHECK(triangle.interiorsIntersect(covers));
        CHECK(covers.interiorsIntersect(triangle));

        // Boundary y = -10, interior y <= -10: excludes the whole triangle.
        const Halfplane excludes = hp({0, -10}, {-1, -10});
        CHECK_FALSE(triangle.intersects(excludes));
        CHECK_FALSE(excludes.intersects(triangle));
        CHECK_FALSE(triangle.interiorsIntersect(excludes));
        CHECK_FALSE(excludes.interiorsIntersect(triangle));

        // Boundary y = 6, interior y >= 6: closed half-plane touches apex (0,6)
        // only, so it intersects but the open interiors do not.
        const Halfplane tangent = hp({0, 6}, {1, 6});
        CHECK(triangle.intersects(tangent));
        CHECK(tangent.intersects(triangle));
        CHECK_FALSE(triangle.interiorsIntersect(tangent));
        CHECK_FALSE(tangent.interiorsIntersect(triangle));

        // Boundary y = 7, interior y >= 7: strictly above the triangle.
        const Halfplane above = hp({0, 7}, {1, 7});
        CHECK_FALSE(triangle.intersects(above));
        CHECK_FALSE(above.intersects(triangle));
        CHECK_FALSE(triangle.interiorsIntersect(above));
        CHECK_FALSE(above.interiorsIntersect(triangle));

        // Degenerate half-plane (source == target) reduces to a point test.
        const Halfplane degenerate_inside(hp({2, 2}, {2, 2}));
        CHECK(triangle.intersects(degenerate_inside));
        CHECK(degenerate_inside.intersects(triangle));

        const Halfplane degenerate_outside(hp({-2, -2}, {-2, -2}));
        CHECK_FALSE(triangle.intersects(degenerate_outside));
        CHECK_FALSE(degenerate_outside.intersects(triangle));
    }
}
