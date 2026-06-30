#include "pgl.hpp"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

TEST_CASE("Halfplane containment of Rectangle") {
    using Point = pgl::Point<int>;
    using Halfplane = pgl::Halfplane<Point>;
    using Rectangle = pgl::Rectangle<Point>;

    // upper = closed half-plane y >= 0, boundary y = 0
    const Halfplane upper({0, 0}, {4, 0});
    const Rectangle inside({1, 1}, {3, 3});
    const Rectangle touching({1, 0}, {3, 2});
    const Rectangle crossing({1, -1}, {3, 2});
    const Rectangle outside({1, -3}, {3, -1});

    CHECK_MESSAGE(upper.contains(inside), upper, " contains ", inside);
    CHECK_MESSAGE(upper.interiorContains(inside), upper, " interiorContains ", inside);
    CHECK_MESSAGE(upper.contains(touching), upper, " contains ", touching);
    CHECK_FALSE_MESSAGE(upper.interiorContains(touching), upper, " interiorContains ", touching);
    CHECK_FALSE_MESSAGE(upper.contains(crossing), upper, " contains ", crossing);
    CHECK_FALSE_MESSAGE(upper.contains(outside), upper, " contains ", outside);
}

TEST_CASE("Halfplane boundaryContains Rectangle") {
    using Point = pgl::Point<int>;
    using Halfplane = pgl::Halfplane<Point>;
    using Rectangle = pgl::Rectangle<Point>;

    const Halfplane diagonal({0, 0}, {4, 4});

    // A degenerate rectangle that collapses to a line segment on the boundary.
    CHECK_MESSAGE(diagonal.boundaryContains(Rectangle({2, 2}, {2, 2})), diagonal,
                  " boundaryContains degenerate point rect on boundary");
    // A proper rectangle cannot lie entirely on a 1D boundary.
    CHECK_FALSE_MESSAGE(diagonal.boundaryContains(Rectangle({1, 1}, {3, 2})), diagonal,
                        " boundaryContains off-boundary rectangle");
}

TEST_CASE("Halfplane and Rectangle intersection predicates, both directions") {
    using Point = pgl::Point<int>;
    using Halfplane = pgl::Halfplane<Point>;
    using Rectangle = pgl::Rectangle<Point>;

    const Halfplane upper({0, 0}, {4, 0});
    const Rectangle inside({1, 1}, {3, 3});
    const Rectangle touching({1, 0}, {3, 2});
    const Rectangle crossing({1, -1}, {3, 2});
    const Rectangle outside({1, -3}, {3, -1});
    // Halfplane whose boundary is above the rectangle.
    const Halfplane outside_upper({0, 5}, {4, 5});
    // Halfplane tangent along the top edge.
    const Halfplane tangent_upper({0, 3}, {4, 3});

    SUBCASE("rectangle inside: both intersect, interiors intersect") {
        CHECK_MESSAGE(upper.intersects(inside), upper, " intersects ", inside);
        CHECK_MESSAGE(inside.intersects(upper), inside, " intersects ", upper);
        CHECK_MESSAGE(upper.interiorsIntersect(inside), upper, " interiorsIntersect ", inside);
        CHECK_MESSAGE(inside.interiorsIntersect(upper), inside, " interiorsIntersect ", upper);
    }

    SUBCASE("crossing rectangle: both intersect, interiors intersect") {
        CHECK_MESSAGE(upper.intersects(crossing), upper, " intersects ", crossing);
        CHECK_MESSAGE(crossing.intersects(upper), crossing, " intersects ", upper);
        CHECK_MESSAGE(upper.interiorsIntersect(crossing), upper, " interiorsIntersect ", crossing);
        CHECK_MESSAGE(crossing.interiorsIntersect(upper), crossing, " interiorsIntersect ", upper);
    }

    SUBCASE("outside rectangle: no intersection") {
        CHECK_FALSE_MESSAGE(upper.intersects(outside), upper, " intersects ", outside);
        CHECK_FALSE_MESSAGE(outside.intersects(upper), outside, " intersects ", upper);
    }

    SUBCASE("halfplane above rectangle: no intersection") {
        const Rectangle low({0, 0}, {4, 3});
        CHECK_FALSE_MESSAGE(outside_upper.intersects(low), outside_upper, " intersects ", low);
    }

    SUBCASE("tangent along top: intersects but interiors do not") {
        const Rectangle low({0, 0}, {4, 3});
        CHECK_MESSAGE(tangent_upper.intersects(low), tangent_upper, " intersects ", low);
        CHECK_FALSE_MESSAGE(tangent_upper.interiorsIntersect(low), tangent_upper, " interiorsIntersect ", low);
    }
}

TEST_CASE("Halfplane never separates or crosses Rectangle, and Rectangle never separates Halfplane") {
    using Point = pgl::Point<int>;
    using Halfplane = pgl::Halfplane<Point>;
    using Rectangle = pgl::Rectangle<Point>;

    const Halfplane upper({0, 0}, {4, 0});
    const Rectangle crossing({1, -1}, {3, 2});
    const Rectangle inside({1, 1}, {3, 2});

    // A half-plane is convex: removing it from a rectangle leaves a convex region.
    CHECK_FALSE_MESSAGE(upper.separates(crossing), upper, " separates ", crossing);
    CHECK_FALSE_MESSAGE(upper.separates(inside), upper, " separates ", inside);
    CHECK_FALSE_MESSAGE(upper.crosses(crossing), upper, " crosses ", crossing);
    CHECK_FALSE_MESSAGE(upper.crosses(inside), upper, " crosses ", inside);

    // A finite rectangle cannot cut across an infinite half-plane.
    CHECK_FALSE_MESSAGE(crossing.separates(upper), crossing, " separates ", upper);
    CHECK_FALSE_MESSAGE(crossing.crosses(upper), crossing, " crosses ", upper);
}
