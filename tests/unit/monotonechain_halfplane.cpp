#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

// Reference chain: up to a peak at (2,4), down to (4,0), up the vertical edge
// to (4,4), and down to (6,0).
namespace {
const pgl::MonotoneChain<pgl::Point<int>> zigzag({0, 0, 2, 4, 4, 0, 4, 4, 6, 0});
}

TEST_CASE("MonotoneChain and Halfplane containment and intersection") {
    using Point = pgl::Point<int>;
    using Halfplane = pgl::Halfplane<Point>;

    const Halfplane upper({0, 2}, {1, 2});    // closed side y >= 2
    const Halfplane high({0, 5}, {1, 5});     // closed side y >= 5, misses
    const Halfplane low({0, -1}, {1, -1});    // closed side y >= -1, swallows

    CHECK(zigzag.intersects(upper));
    CHECK(upper.intersects(zigzag));
    CHECK_FALSE(zigzag.intersects(high));
    CHECK(low.contains(zigzag));
    CHECK(low.interiorContains(zigzag));
    CHECK_FALSE(upper.contains(zigzag));
    CHECK(zigzag.interiorsIntersect(upper));
    CHECK_FALSE(zigzag.interiorsIntersect(high));
    CHECK_FALSE(zigzag.contains(upper));   // a bounded chain holds no halfplane

    // A chain along the boundary line is boundary-contained.
    const pgl::MonotoneChain<Point> onBoundary({0, 2, 3, 2, 5, 2});
    CHECK(upper.boundaryContains(onBoundary));
    CHECK_FALSE(upper.boundaryContains(zigzag));
    // On the boundary means not in the open halfplane.
    CHECK_FALSE(upper.interiorContains(onBoundary));
    CHECK(upper.contains(onBoundary));
}

TEST_CASE("Halfplane separates a chain passing through it") {
    using Point = pgl::Point<int>;
    using Halfplane = pgl::Halfplane<Point>;

    // y >= 2 clips the chain into components strictly between the extremes.
    const Halfplane upper({0, 2}, {1, 2});
    CHECK(upper.separates(zigzag));

    // y >= -1 swallows the whole chain: nothing is left to disconnect.
    const Halfplane low({0, -1}, {1, -1});
    CHECK_FALSE(low.separates(zigzag));

    // A component that reaches an extreme vertex does not disconnect: x <= 1
    // (left of the vertical line) removes an end piece only.
    const Halfplane left({1, 0}, {1, 1});   // contains x <= 1
    CHECK(left.contains(Point(0, 0)));
    CHECK_FALSE(left.separates(zigzag));
}

TEST_CASE("Chain separates a halfplane by sealing off a pocket") {
    using Point = pgl::Point<int>;
    using Chain = pgl::MonotoneChain<Point>;
    using Halfplane = pgl::Halfplane<Point>;

    const Halfplane upper({0, 2}, {1, 2});   // closed side y >= 2

    // The zigzag rises above y = 2 between stops below it: each piece above
    // the line, together with the line, encloses a pocket.
    CHECK(zigzag.separates(upper));
    CHECK(zigzag.crosses(upper));

    // A tent with both ends on the boundary line pockets the region under it.
    CHECK(Chain({0, 2, 2, 4, 4, 2}).separates(upper));
    // Crossing the line once leaves a slit anchored at the boundary.
    CHECK_FALSE(Chain({0, 0, 4, 4}).separates(upper));
    // Strictly inside the halfplane: both loose ends stay interior.
    CHECK_FALSE(Chain({0, 3, 4, 5}).separates(upper));
    // Entirely below the halfplane: never meets the interior.
    CHECK_FALSE(Chain({0, 0, 4, 1}).separates(upper));
    // Dipping to touch the line between interior stops removes one boundary
    // point plus a slit: the region flows around the loose ends.
    CHECK_FALSE(Chain({0, 3, 2, 2, 4, 3}).separates(upper));
}

TEST_CASE("MonotoneChain and Halfplane distance and intersection pieces") {
    using Point = pgl::Point<int>;
    using Halfplane = pgl::Halfplane<Point>;
    using Rational = pgl::Rational<int>;

    const Halfplane high({0, 5}, {1, 5});
    CHECK(zigzag.squaredDistance<Rational>(high) == Rational(1));
    CHECK(high.squaredDistance<Rational>(zigzag) == Rational(1));
    CHECK(zigzag.distanceLInf<Rational>(high) == Rational(1));

    // Clipping the chain to y >= 2 keeps the pieces around the two peaks; the
    // pieces meeting at a peak are not collinear, so they stay separate.
    const auto pieces = zigzag.intersection(Halfplane({0, 2}, {1, 2}));
    REQUIRE(pieces.size() == 4);
    const std::vector<pgl::Segment<Point>> expected{
        pgl::Segment<Point>({1, 2}, {2, 4}),
        pgl::Segment<Point>({2, 4}, {3, 2}),
        pgl::Segment<Point>({4, 2}, {4, 4}),
        pgl::Segment<Point>({4, 4}, {5, 2}),
    };
    for (std::size_t i = 0; i < expected.size(); ++i) {
        REQUIRE(std::holds_alternative<pgl::Segment<Point>>(pieces[i]));
        CHECK(std::get<pgl::Segment<Point>>(pieces[i]) == expected[i]);
    }
}
