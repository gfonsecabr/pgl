#include "pgl.hpp"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <variant>

TEST_CASE("Rectangle interiorsIntersect Triangle") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;
    using Triangle = pgl::Triangle<Point>;

    const Rectangle rect({0, 0}, {4, 4});
    const Triangle touching_corner({4, 4}, {5, 4}, {4, 5});
    const Triangle crossing({2, -1}, {5, 2}, {2, 5});

    // Touching only at the corner: intersects but interiors do not.
    CHECK_MESSAGE(rect.intersects(touching_corner), rect, " intersects corner triangle");
    CHECK_FALSE_MESSAGE(rect.interiorsIntersect(touching_corner), rect, " interiorsIntersect corner triangle");

    // Proper crossing: both shapes' interiors overlap.
    CHECK_MESSAGE(rect.interiorsIntersect(crossing), rect, " interiorsIntersect crossing triangle");
    CHECK_MESSAGE(crossing.interiorsIntersect(rect), "crossing triangle interiorsIntersect ", rect);
}

TEST_CASE("Rectangle separates and crosses Triangle") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;
    using Triangle = pgl::Triangle<Point>;

    const Rectangle rect({0, 0}, {4, 3});
    const Triangle cutting({1, -1}, {3, -1}, {2, 4});

    CHECK_MESSAGE(rect.separates(cutting), rect, " separates ", cutting);
    CHECK_MESSAGE(rect.crosses(cutting), rect, " crosses ", cutting);

    // A small corner triangle that doesn't pierce through: no separation.
    const Triangle corner({0, 0}, {2, 0}, {0, 2});
    CHECK_FALSE_MESSAGE(rect.crosses(corner), rect, " crosses corner triangle");
}

TEST_CASE("Rectangle separates Triangle: bar-shaped rectangle as a true barrier") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;
    using Triangle = pgl::Triangle<Point>;

    const Triangle tri({0, 0}, {6, 0}, {0, 6});
    const Rectangle vertical_bar({2, 0}, {3, 4});
    const Rectangle corner_box({0, 0}, {1, 1});

    CHECK_MESSAGE(vertical_bar.separates(tri), vertical_bar, " separates ", tri);
    CHECK_FALSE_MESSAGE(corner_box.separates(tri), corner_box, " does not separate ", tri);
}

TEST_CASE("Triangle intersects Rectangle") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;
    using Triangle = pgl::Triangle<Point>;

    const Triangle tri({0, 0}, {4, 0}, {0, 4});
    const Rectangle inner({1, 1}, {2, 2});
    const Rectangle touching_corner({4, 0}, {6, 2});
    const Rectangle crossing_box({2, -1}, {4, 2});
    const Rectangle disjoint({6, 6}, {8, 8});

    CHECK_MESSAGE(tri.intersects(inner), tri, " intersects inner rect");
    CHECK_MESSAGE(inner.intersects(tri), "inner rect intersects ", tri);
    // Corner touch counts as intersection.
    CHECK_MESSAGE(tri.intersects(touching_corner), tri, " intersects corner rect");
    CHECK_MESSAGE(tri.intersects(crossing_box), tri, " intersects crossing rect");
    CHECK_FALSE_MESSAGE(tri.intersects(disjoint), tri, " intersects disjoint rect");
    CHECK_FALSE_MESSAGE(disjoint.intersects(tri), "disjoint intersects ", tri);
}

TEST_CASE("Triangle interiorsIntersect Rectangle: strict interior contact only") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;
    using Triangle = pgl::Triangle<Point>;

    const Triangle tri({0, 0}, {6, 0}, {0, 6});
    const Rectangle touching_corner({6, 0}, {8, 2});
    const Rectangle crossing_box({2, -1}, {4, 2});

    CHECK_FALSE_MESSAGE(tri.interiorsIntersect(touching_corner), tri, " interiorsIntersect corner rect");
    CHECK_MESSAGE(tri.interiorsIntersect(crossing_box), tri, " interiorsIntersect crossing rect");
    CHECK_MESSAGE(crossing_box.interiorsIntersect(tri), "crossing rect interiorsIntersect ", tri);
}

TEST_CASE("Triangle separates Rectangle and vice versa") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;
    using Triangle = pgl::Triangle<Point>;

    const Rectangle rect({0, 0}, {6, 6});
    const Triangle vertical_wedge({2, -1}, {4, -1}, {3, 7});
    const Triangle corner_wedge({0, 0}, {2, 0}, {0, 2});

    CHECK_MESSAGE(vertical_wedge.separates(rect), vertical_wedge, " separates ", rect);
    CHECK_FALSE_MESSAGE(corner_wedge.separates(rect), corner_wedge, " does not separate ", rect);

    // Triangle that is the lower-left half of the square: does NOT separate the
    // rectangle (the complement is a single connected region).
    const Triangle big({0, 0}, {6, 0}, {0, 6});
    CHECK_FALSE_MESSAGE(big.separates(rect), big, " does not separate ", rect);
}

TEST_CASE("Triangle crosses Rectangle") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;
    using Triangle = pgl::Triangle<Point>;

    const Triangle tri({0, 0}, {6, 0}, {0, 6});
    const Rectangle rect({0, 0}, {6, 6});

    CHECK_FALSE_MESSAGE(tri.crosses(rect), tri, " crosses ", rect);

    // Wide triangle whose base extends past both sides of the rectangle: at
    // y∈[2,2.5] it spans the full [0,6] width, so the rectangle is split into
    // disconnected top and bottom halves.  The two wings outside the rectangle
    // (near x<0 and x>6) are likewise disconnected by the rectangle.
    const Triangle wide({-1, 2}, {7, 2}, {3, 4});
    CHECK_MESSAGE(wide.crosses(rect), wide, " crosses ", rect);
    CHECK_MESSAGE(rect.crosses(wide), rect, " crosses wide triangle");
}

TEST_CASE("Triangle intersection with Rectangle returns Convex or empty") {
    using Point = pgl::Point<int>;
    using Convex = pgl::Convex<Point>;
    using Rectangle = pgl::Rectangle<Point>;
    using Triangle = pgl::Triangle<Point>;

    const Triangle tri({0, 0}, {6, 0}, {0, 6});

    SUBCASE("overlapping rectangle clips triangle to smaller triangle") {
        // [2,8] x [2,8] ∩ triangle = the triangle (2,2)(4,2)(2,4)
        const auto r = tri.intersection(Rectangle({2, 2}, {8, 8}));
        REQUIRE(r);
        REQUIRE(std::holds_alternative<Convex>(*r));
        CHECK(std::get<Convex>(*r).twiceArea() == 4);
    }

    SUBCASE("disjoint rectangle yields empty") {
        CHECK_FALSE(tri.intersection(Rectangle({10, 10}, {12, 12})));
    }
}
