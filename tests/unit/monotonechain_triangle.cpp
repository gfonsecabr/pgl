#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

namespace {
const pgl::Triangle<pgl::Point<int>> triangle({0, 0}, {6, 0}, {3, 6});
}

TEST_CASE("Triangle contains a chain iff it contains every vertex") {
    using Point = pgl::Point<int>;
    using Chain = pgl::MonotoneChain<Point>;

    CHECK(triangle.contains(Chain({1, 1, 3, 1, 4, 2})));
    CHECK(triangle.interiorContains(Chain({1, 1, 3, 1, 4, 2})));
    CHECK_FALSE(triangle.contains(Chain({1, 1, 7, 1})));
    // On the bottom edge, through a collinear middle vertex.
    const Chain bottom({1, 0, 2, 0, 4, 0});
    CHECK(triangle.contains(bottom));
    CHECK(triangle.boundaryContains(bottom));
    CHECK_FALSE(triangle.interiorContains(bottom));
    CHECK_FALSE(Chain({1, 1, 3, 1}).contains(triangle));   // 1D holds no triangle
}

TEST_CASE("MonotoneChain and Triangle intersection predicates, both directions") {
    using Point = pgl::Point<int>;
    using Chain = pgl::MonotoneChain<Point>;

    const Chain crossing({-2, 1, 8, 1});   // horizontal chain through the triangle
    CHECK(crossing.intersects(triangle));
    CHECK(triangle.intersects(crossing));
    CHECK(crossing.interiorsIntersect(triangle));

    const Chain above({-2, 7, 8, 7});
    CHECK_FALSE(above.intersects(triangle));

    // Touching the apex only: on the boundary, not the open interior.
    const Chain apexTouch({0, 6, 3, 6, 6, 7});
    CHECK(apexTouch.intersects(triangle));
    CHECK_FALSE(apexTouch.interiorsIntersect(triangle));
}

TEST_CASE("Triangle separates a chain crossing through it") {
    using Point = pgl::Point<int>;
    using Chain = pgl::MonotoneChain<Point>;

    CHECK(triangle.separates(Chain({-2, 1, 8, 1})));
    // The removed piece contains the chain's first vertex: no disconnection.
    CHECK_FALSE(triangle.separates(Chain({1, 1, 8, 1})));
    CHECK_FALSE(triangle.separates(Chain({-2, 7, 8, 7})));
}

TEST_CASE("Chain separates a triangle crossing through it") {
    using Point = pgl::Point<int>;
    using Chain = pgl::MonotoneChain<Point>;

    // A single edge cutting a chord through both slanted sides.
    CHECK(Chain({-2, 1, 8, 1}).separates(triangle));
    // Bending at an interior vertex on the way through.
    CHECK(Chain({-1, 3, 3, 3, 7, 3}).separates(triangle));
    // Ending strictly inside leaves a slit.
    CHECK_FALSE(Chain({-2, 1, 3, 1}).separates(triangle));
    // Passing above the apex: no contact.
    CHECK_FALSE(Chain({-2, 7, 8, 7}).separates(triangle));
    // Running along the bottom edge removes boundary points only.
    CHECK_FALSE(Chain({1, 0, 2, 0, 4, 0}).separates(triangle));

    // Each removal disconnects the other, so the shapes cross.
    CHECK(Chain({-2, 1, 8, 1}).crosses(triangle));
    CHECK(triangle.crosses(Chain({-2, 1, 8, 1})));
}

TEST_CASE("MonotoneChain and Triangle distance and intersection pieces") {
    using Point = pgl::Point<int>;
    using Chain = pgl::MonotoneChain<Point>;
    using Rational = pgl::Rational<int>;
    using RationalPoint = pgl::Point<Rational>;

    const Chain below({0, -2, 6, -2});
    CHECK(triangle.squaredDistance<Rational>(below) == Rational(4));
    CHECK(below.squaredDistance<Rational>(triangle) == Rational(4));

    // Clip the horizontal chain y = 1 to the triangle: one segment between the
    // two slanted edges, with rational endpoints x = 1/2 and x = 11/2.
    const auto pieces = Chain({-1, 1, 7, 1}).intersection<Rational>(triangle);
    REQUIRE(pieces.size() == 1);
    REQUIRE(std::holds_alternative<pgl::Segment<RationalPoint>>(pieces[0]));
    CHECK(std::get<pgl::Segment<RationalPoint>>(pieces[0]) ==
          pgl::Segment<RationalPoint>(RationalPoint(Rational(1, 2), Rational(1)),
                                      RationalPoint(Rational(11, 2), Rational(1))));
}
