#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

// Reference chain: up to a peak at (2,4), down to (4,0), up the vertical edge
// to (4,4), and down to (6,0).
namespace {
const pgl::MonotoneChain<pgl::Point<int>> zigzag({0, 0, 2, 4, 4, 0, 4, 4, 6, 0});
}

TEST_CASE("Rectangle contains a chain iff it contains every vertex") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;

    const Rectangle loose({-1, -1}, {7, 5});
    CHECK(loose.contains(zigzag));
    CHECK(loose.interiorContains(zigzag));

    const Rectangle snug({0, 0}, {6, 4});
    CHECK(snug.contains(zigzag));
    CHECK_FALSE(snug.interiorContains(zigzag));   // (0,0) lies on the boundary
    CHECK_FALSE(Rectangle({0, 0}, {5, 4}).contains(zigzag));

    // A chain running along the bottom edge, with a collinear middle vertex.
    const pgl::MonotoneChain<Point> bottom({0, 0, 3, 0, 6, 0});
    CHECK(snug.boundaryContains(bottom));
    CHECK_FALSE(snug.boundaryContains(zigzag));
    CHECK_FALSE(zigzag.contains(snug));   // a 1D chain holds no rectangle
}

TEST_CASE("MonotoneChain and Rectangle intersection, windowed by x") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;

    CHECK(zigzag.intersects(Rectangle({1, 1}, {5, 5})));
    CHECK(Rectangle({1, 1}, {5, 5}).intersects(zigzag));
    CHECK_FALSE(zigzag.intersects(Rectangle({7, 0}, {8, 1})));   // right of the x-extent
    CHECK(zigzag.intersects(Rectangle({6, 0}, {7, 1})));         // touches the last vertex
    CHECK_FALSE(zigzag.intersects(Rectangle({2, 5}, {4, 6})));   // above the peaks
    CHECK(zigzag.interiorsIntersect(Rectangle({1, 1}, {5, 5})));
    // Touching the boundary square corner-to-vertex only: (2,4) is a chain
    // interior point but on the rectangle's boundary, not its open interior.
    CHECK(zigzag.intersects(Rectangle({2, 4}, {3, 6})));
    CHECK_FALSE(zigzag.interiorsIntersect(Rectangle({2, 4}, {3, 6})));
}

TEST_CASE("Rectangle and chain as mutual removers") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;

    // The window x in [1,3] cuts a middle piece out of the chain.
    CHECK(Rectangle({1, -1}, {3, 5}).separates(zigzag));
    // A piece containing the extreme vertex (0,0) does not disconnect.
    CHECK_FALSE(Rectangle({-1, -1}, {1, 5}).separates(zigzag));
    // Swallowing the chain leaves nothing to disconnect.
    CHECK_FALSE(Rectangle({-1, -1}, {7, 5}).separates(zigzag));

    // Chain side: the chain spans the window top to bottom, cutting the
    // rectangle into a left and a right part.
    CHECK(zigzag.separates(Rectangle({1, -1}, {3, 5})));
    // Entering at x = 1 and ending strictly inside leaves a slit, not a cut.
    CHECK_FALSE(zigzag.separates(Rectangle({1, -1}, {7, 5})));
    // Touching the top edge at three vertices removes boundary points only.
    CHECK_FALSE(zigzag.separates(Rectangle({0, -2}, {6, 0})));
    // Disjoint bounding boxes.
    CHECK_FALSE(zigzag.separates(Rectangle({7, 0}, {8, 1})));

    // Each removal disconnects the other, so the shapes cross.
    CHECK(zigzag.crosses(Rectangle({1, -1}, {3, 5})));
    CHECK(Rectangle({1, -1}, {3, 5}).crosses(zigzag));
}

TEST_CASE("MonotoneChain and Rectangle distance and intersection pieces") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;
    using Rational = pgl::Rational<int>;

    const Rectangle right({8, 0}, {9, 1});
    CHECK(zigzag.squaredDistance<Rational>(right) == Rational(4));
    CHECK(right.squaredDistance<Rational>(zigzag) == Rational(4));
    CHECK(zigzag.distanceL1<Rational>(right) == Rational(2));
    CHECK(zigzag.distanceLInf<Rational>(right) == Rational(2));

    // Clipping the chain to the band y <= 2 keeps four non-collinear pieces.
    const auto pieces = zigzag.intersection(Rectangle({0, 0}, {6, 2}));
    REQUIRE(pieces.size() == 4);
    const std::vector<pgl::Segment<Point>> expected{
        pgl::Segment<Point>({0, 0}, {1, 2}),
        pgl::Segment<Point>({3, 2}, {4, 0}),
        pgl::Segment<Point>({4, 0}, {4, 2}),
        pgl::Segment<Point>({5, 2}, {6, 0}),
    };
    for (std::size_t i = 0; i < expected.size(); ++i) {
        REQUIRE(std::holds_alternative<pgl::Segment<Point>>(pieces[i]));
        CHECK(std::get<pgl::Segment<Point>>(pieces[i]) == expected[i]);
    }
}
