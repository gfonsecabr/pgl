#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

// Reference chain: up to a peak at (2,4), down to (4,0), up the vertical edge
// to (4,4), and down to (6,0).
namespace {
const pgl::MonotoneChain<pgl::Point<int>> zigzag({0, 0, 2, 4, 4, 0, 4, 4, 6, 0});
}

TEST_CASE("MonotoneChain and Ray intersection predicates, both directions") {
    using Point = pgl::Point<int>;
    using Ray = pgl::Ray<Point>;

    const Ray downThrough({1, 5}, {1, 4});   // hits the first edge at (1,2)
    const Ray missing({0, 5}, {-1, 6});      // points away from the chain
    CHECK(zigzag.intersects(downThrough));
    CHECK(downThrough.intersects(zigzag));
    CHECK_FALSE(zigzag.intersects(missing));
    CHECK(zigzag.interiorsIntersect(downThrough));
}

TEST_CASE("A ray contains a chain lying along it") {
    using Point = pgl::Point<int>;
    using Ray = pgl::Ray<Point>;

    const Ray diagonal({0, 0}, {1, 1});
    CHECK(diagonal.contains(pgl::MonotoneChain<Point>({1, 1, 2, 2})));
    CHECK(diagonal.contains(pgl::MonotoneChain<Point>({0, 0, 2, 2})));
    CHECK_FALSE(diagonal.contains(zigzag));
    // The ray's relative interior excludes its source.
    CHECK(diagonal.interiorContains(pgl::MonotoneChain<Point>({1, 1, 2, 2})));
    CHECK_FALSE(diagonal.interiorContains(pgl::MonotoneChain<Point>({0, 0, 2, 2})));
}

TEST_CASE("MonotoneChain and Ray separates and crosses") {
    using Point = pgl::Point<int>;
    using Ray = pgl::Ray<Point>;

    SUBCASE("a proper crossing separates in both directions") {
        const Ray downThrough({1, 5}, {1, 4});
        CHECK(zigzag.separates(downThrough));
        CHECK(downThrough.separates(zigzag));
        CHECK(zigzag.crosses(downThrough));
        CHECK(downThrough.crosses(zigzag));
    }

    SUBCASE("a ray leaving from a point on the chain is not disconnected") {
        // The single intersection component contains the ray's source, so
        // removing the chain leaves the ray connected; the source is still an
        // interior point of the chain, so the reverse direction separates.
        const Ray fromChain({1, 2}, {1, 5});
        CHECK_FALSE(zigzag.separates(fromChain));
        CHECK(fromChain.separates(zigzag));
        CHECK_FALSE(zigzag.crosses(fromChain));
    }

    SUBCASE("a ray through an extreme vertex does not disconnect the chain") {
        const Ray throughFront({-1, 1}, {0, 0});
        CHECK_FALSE(throughFront.separates(zigzag));
    }
}

TEST_CASE("MonotoneChain and Ray distance and intersection") {
    using Point = pgl::Point<int>;
    using Ray = pgl::Ray<Point>;
    using Rational = pgl::Rational<int>;

    const Ray above({0, 5}, {1, 5});   // horizontal at y = 5, over the peaks
    CHECK(zigzag.squaredDistance<Rational>(above) == Rational(1));
    CHECK(above.squaredDistance<Rational>(zigzag) == Rational(1));

    const auto pieces = zigzag.intersection(Ray({3, -5}, {3, 5}));
    REQUIRE(pieces.size() == 1);
    REQUIRE(std::holds_alternative<Point>(pieces[0]));
    CHECK(std::get<Point>(pieces[0]) == Point(3, 2));
}
