#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

namespace {
const pgl::Convex<pgl::Point<int>> square(
    {pgl::Point<int>(0, 0), pgl::Point<int>(6, 0), pgl::Point<int>(6, 6), pgl::Point<int>(0, 6)});
}

TEST_CASE("Convex contains a chain iff it contains every vertex") {
    using Point = pgl::Point<int>;
    using Chain = pgl::MonotoneChain<Point>;

    const Chain zigzag({0, 0, 2, 4, 4, 0, 4, 4, 6, 0});
    CHECK(square.contains(zigzag));
    CHECK_FALSE(square.interiorContains(zigzag));   // touches the boundary at (0,0)
    CHECK(square.interiorContains(Chain({1, 1, 3, 5, 5, 1})));
    CHECK_FALSE(square.contains(Chain({1, 1, 7, 1})));

    // A chain running along the bottom hull edge through collinear vertices.
    const Chain bottom({1, 0, 2, 0, 5, 0});
    CHECK(square.boundaryContains(bottom));
    CHECK_FALSE(square.boundaryContains(Chain({1, 0, 2, 1})));
    CHECK_FALSE(zigzag.contains(square));
}

TEST_CASE("MonotoneChain and Convex intersection predicates, both directions") {
    using Point = pgl::Point<int>;
    using Chain = pgl::MonotoneChain<Point>;

    const Chain through({-2, 3, 8, 3});
    CHECK(through.intersects(square));
    CHECK(square.intersects(through));
    CHECK(through.interiorsIntersect(square));
    CHECK_FALSE(Chain({-2, 8, 8, 8}).intersects(square));

    // Sliding along the top boundary: closed sets touch, open ones do not.
    const Chain alongTop({-2, 6, 8, 6});
    CHECK(alongTop.intersects(square));
    CHECK_FALSE(alongTop.interiorsIntersect(square));
}

TEST_CASE("Convex separates a chain crossing through it") {
    using Point = pgl::Point<int>;
    using Chain = pgl::MonotoneChain<Point>;

    CHECK(square.separates(Chain({-2, 3, 8, 3})));
    CHECK_FALSE(square.separates(Chain({1, 3, 8, 3})));   // eats an end piece
    CHECK_FALSE(square.separates(Chain({-2, 8, 8, 8})));
}

TEST_CASE("Chain separates a convex polygon crossing through it") {
    using Point = pgl::Point<int>;
    using Chain = pgl::MonotoneChain<Point>;

    // A single edge cutting a chord straight through.
    CHECK(Chain({-2, 3, 8, 3}).separates(square));
    // Bending at an interior vertex on the way through.
    CHECK(Chain({-2, 3, 3, 5, 8, 3}).separates(square));
    // Ending strictly inside leaves a slit.
    CHECK_FALSE(Chain({-2, 3, 3, 3}).separates(square));
    // Passing above: no contact.
    CHECK_FALSE(Chain({-2, 8, 8, 8}).separates(square));
    // Running along the left hull edge removes boundary points only.
    CHECK_FALSE(Chain({0, 1, 0, 5}).separates(square));

    // Each removal disconnects the other, so the shapes cross.
    CHECK(Chain({-2, 3, 8, 3}).crosses(square));
}

TEST_CASE("MonotoneChain and Convex distance and intersection pieces") {
    using Point = pgl::Point<int>;
    using Chain = pgl::MonotoneChain<Point>;
    using Rational = pgl::Rational<int>;

    const Chain right({8, 0, 9, 6});
    CHECK(square.squaredDistance<Rational>(right) == Rational(4));
    CHECK(right.squaredDistance<Rational>(square) == Rational(4));
    CHECK(right.distanceL1<Rational>(square) == Rational(2));

    const auto pieces = Chain({-2, 3, 8, 3}).intersection(square);
    REQUIRE(pieces.size() == 1);
    REQUIRE(std::holds_alternative<pgl::Segment<Point>>(pieces[0]));
    CHECK(std::get<pgl::Segment<Point>>(pieces[0]) == pgl::Segment<Point>({0, 3}, {6, 3}));
}
