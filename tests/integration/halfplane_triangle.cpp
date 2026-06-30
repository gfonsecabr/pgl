#include "pgl.hpp"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <variant>

TEST_CASE("Halfplane contains and interiorContains Triangle") {
    using Point = pgl::Point<int>;
    using Halfplane = pgl::Halfplane<Point>;
    using Triangle = pgl::Triangle<Point>;

    const Halfplane upper({0, 0}, {4, 0});  // y >= 0

    SUBCASE("triangle fully inside: halfplane contains and interiorContains it") {
        const Triangle inside({1, 1}, {3, 1}, {2, 3});
        CHECK_MESSAGE(upper.contains(inside), upper, " contains inside triangle");
        CHECK_MESSAGE(upper.interiorContains(inside), upper, " interiorContains inside triangle");
    }

    SUBCASE("triangle touching boundary only: halfplane contains but not interiorContains") {
        const Triangle touching({0, 0}, {4, 0}, {2, 0});
        CHECK_MESSAGE(upper.contains(touching), upper, " contains boundary triangle");
        CHECK_FALSE_MESSAGE(upper.interiorContains(touching), upper, " interiorContains boundary triangle");
    }

    SUBCASE("triangle straddling boundary: halfplane does not contain it") {
        const Triangle crossing({1, -1}, {3, -1}, {2, 2});
        CHECK_FALSE_MESSAGE(upper.contains(crossing), upper, " contains crossing triangle");
    }

    // A finite triangle cannot contain or interiorContain an infinite halfplane.
    SUBCASE("triangle cannot contain a halfplane") {
        const Triangle t({-100, -100}, {100, -100}, {0, 100});
        CHECK_FALSE_MESSAGE(t.contains(upper), t, " contains halfplane");
        CHECK_FALSE_MESSAGE(t.interiorContains(upper), t, " interiorContains halfplane");
    }
}

TEST_CASE("Halfplane boundaryContains Triangle") {
    using Point = pgl::Point<int>;
    using Halfplane = pgl::Halfplane<Point>;
    using Triangle = pgl::Triangle<Point>;

    const Halfplane diagonal({0, 0}, {4, 4});

    // Degenerate (collinear) triangle on the boundary line.
    CHECK_MESSAGE(diagonal.boundaryContains(Triangle({0, 0}, {1, 1}, {2, 2})), diagonal,
                  " boundaryContains collinear triangle on boundary");
    // Non-degenerate triangle cannot lie on a 1D boundary.
    CHECK_FALSE_MESSAGE(diagonal.boundaryContains(Triangle({0, 0}, {1, 1}, {2, 3})), diagonal,
                        " boundaryContains off-boundary triangle");
}

TEST_CASE("Halfplane and Triangle intersection predicates, both directions") {
    using Point = pgl::Point<int>;
    using Halfplane = pgl::Halfplane<Point>;
    using Triangle = pgl::Triangle<Point>;

    const Halfplane upper({0, 0}, {4, 0});

    SUBCASE("triangle fully inside: both intersect, interiors intersect") {
        const Triangle inside({1, 1}, {3, 1}, {2, 3});
        CHECK_MESSAGE(upper.intersects(inside), upper, " intersects ", inside);
        CHECK_MESSAGE(inside.intersects(upper), inside, " intersects ", upper);
        CHECK_MESSAGE(upper.interiorsIntersect(inside), upper, " interiorsIntersect ", inside);
        CHECK_MESSAGE(inside.interiorsIntersect(upper), inside, " interiorsIntersect ", upper);
    }

    SUBCASE("triangle boundary only on halfplane boundary: intersects but not interiors") {
        // Triangle sitting below the line y=0 but touching it with its base.
        const Triangle boundary_only({0, 0}, {2, 0}, {1, -1});
        CHECK_MESSAGE(upper.intersects(boundary_only), upper, " intersects boundary_only triangle");
        CHECK_FALSE_MESSAGE(upper.interiorsIntersect(boundary_only), upper, " interiorsIntersect boundary_only triangle");
        CHECK_FALSE_MESSAGE(boundary_only.interiorsIntersect(upper), "boundary_only interiorsIntersect ", upper);
    }

    SUBCASE("triangle entering the halfplane interior: interiorsIntersect") {
        const Triangle entering({0, 0}, {2, 0}, {1, 1});
        CHECK_MESSAGE(upper.interiorsIntersect(entering), upper, " interiorsIntersect entering triangle");
        CHECK_MESSAGE(entering.interiorsIntersect(upper), "entering interiorsIntersect ", upper);
    }

    SUBCASE("triangle fully outside: no intersection") {
        const Triangle outside({1, -3}, {3, -3}, {2, -1});
        CHECK_FALSE_MESSAGE(upper.intersects(outside), upper, " intersects outside triangle");
        CHECK_FALSE_MESSAGE(outside.intersects(upper), "outside intersects ", upper);
    }
}

TEST_CASE("Halfplane never separates or crosses Triangle") {
    using Point = pgl::Point<int>;
    using Halfplane = pgl::Halfplane<Point>;
    using Triangle = pgl::Triangle<Point>;

    const Halfplane upper({0, 0}, {4, 0});
    const Triangle inside({0, 0}, {2, 0}, {1, 1});
    const Triangle crossing({1, -1}, {3, -1}, {2, 2});

    // A halfplane is convex: removing it from a triangle leaves a convex piece.
    CHECK_FALSE_MESSAGE(upper.separates(inside), upper, " separates ", inside);
    CHECK_FALSE_MESSAGE(upper.separates(crossing), upper, " separates ", crossing);
    CHECK_FALSE_MESSAGE(upper.crosses(inside), upper, " crosses ", inside);
    CHECK_FALSE_MESSAGE(upper.crosses(crossing), upper, " crosses ", crossing);

    // Triangle never separates or crosses a halfplane.
    CHECK_FALSE_MESSAGE(inside.separates(upper), inside, " separates ", upper);
    CHECK_FALSE_MESSAGE(crossing.separates(upper), crossing, " separates ", upper);
    CHECK_FALSE_MESSAGE(inside.crosses(upper), inside, " crosses ", upper);
    CHECK_FALSE_MESSAGE(crossing.crosses(upper), crossing, " crosses ", upper);
}

TEST_CASE("Halfplane intersection clips Triangle to Convex, Segment, or Point") {
    using Point = pgl::Point<int>;
    using Convex = pgl::Convex<Point>;
    using Segment = pgl::Segment<Point>;
    using Halfplane = pgl::Halfplane<Point>;
    using Triangle = pgl::Triangle<Point>;

    const Triangle tri({0, 0}, {6, 0}, {0, 6});

    SUBCASE("halfplane cut keeps the interior-side area as Convex") {
        // y >= 2: clips off bottom strip, leaving a quadrilateral.
        const auto r = tri.intersection(Halfplane({0, 2}, {1, 2}));
        REQUIRE(r);
        REQUIRE(std::holds_alternative<Convex>(*r));
        CHECK(std::get<Convex>(*r).twiceArea() == 16);  // trapezoid area 8
    }

    SUBCASE("halfplane touching only along a triangle edge yields that Segment") {
        // y <= 0: the triangle sits above, meeting the boundary only along base.
        const auto r = tri.intersection(Halfplane({1, 0}, {0, 0}));
        REQUIRE(r);
        REQUIRE(std::holds_alternative<Segment>(*r));
        CHECK(std::get<Segment>(*r) == Segment(Point(0, 0), Point(6, 0)));
    }

    SUBCASE("halfplane missing the triangle yields empty") {
        CHECK_FALSE(tri.intersection(Halfplane({0, 7}, {1, 7})));
    }
}

TEST_CASE("Triangle intersection with Halfplane returns Convex, Segment, or Point") {
    using Point = pgl::Point<int>;
    using Convex = pgl::Convex<Point>;
    using Segment = pgl::Segment<Point>;
    using Halfplane = pgl::Halfplane<Point>;
    using Triangle = pgl::Triangle<Point>;

    const Triangle tri({0, 0}, {6, 0}, {0, 6});

    SUBCASE("halfplane slicing through interior yields Convex overlap") {
        const auto r = tri.intersection(Halfplane({0, 2}, {1, 2}));
        REQUIRE(r);
        REQUIRE(std::holds_alternative<Convex>(*r));
        CHECK(std::get<Convex>(*r).twiceArea() == 16);
    }

    SUBCASE("halfplane tangent along base yields that Segment") {
        const auto r = tri.intersection(Halfplane({1, 0}, {0, 0}));
        REQUIRE(r);
        REQUIRE(std::holds_alternative<Segment>(*r));
    }

    SUBCASE("halfplane that misses the triangle yields empty") {
        CHECK_FALSE(tri.intersection(Halfplane({0, 7}, {1, 7})));
    }
}
