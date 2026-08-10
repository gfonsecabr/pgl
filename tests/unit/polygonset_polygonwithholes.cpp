#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <vector>

using Point = pgl::Point<int>;
using PolygonShape = pgl::Polygon<Point>;
using Region = pgl::PolygonWithHoles<Point>;
using RegionSet = pgl::PolygonSet<Point>;

static RegionSet apart() {
    return RegionSet(std::vector{Region(PolygonShape({0, 0, 2, 0, 2, 2, 0, 2})),
                                 Region(PolygonShape({5, 5, 7, 5, 7, 7, 5, 7}))});
}

TEST_CASE("PolygonSet and PolygonWithHoles predicates") {
    const RegionSet set = apart();
    const Region big(PolygonShape({-1, -1, 8, -1, 8, 8, -1, 8}));

    SUBCASE("containment either way round") {
        CHECK(big.contains(set));
        CHECK(!set.contains(big));
        CHECK(big.interiorContains(set));
    }

    SUBCASE("a region with a hole around a component does not contain it") {
        const Region holed(PolygonShape({-1, -1, 8, -1, 8, 8, -1, 8}),
                           std::vector{PolygonShape({4, 4, 8, 4, 8, 8, 4, 8})});
        CHECK(!holed.contains(set));
        CHECK(holed.intersects(set));
    }

    SUBCASE("distances fold to the nearest pair") {
        const Region distant(PolygonShape({20, 20, 22, 20, 22, 22, 20, 22}));
        CHECK(set.squaredDistance<double>(distant) == doctest::Approx(13.0 * 13.0 * 2.0));
        CHECK(distant.squaredDistance<double>(set) == doctest::Approx(13.0 * 13.0 * 2.0));
    }
}

TEST_CASE("PolygonSet and PolygonWithHoles cut predicates") {
    SUBCASE("a region cuts a set it runs across") {
        const Region bar(PolygonShape({-1, 1, 8, 1, 8, 2, -1, 2}));
        CHECK(bar.separates(apart()));
    }

    SUBCASE("a set cuts a region it runs across") {
        const RegionSet bar{Region(PolygonShape({-1, 1, 4, 1, 4, 2, -1, 2}))};
        const Region square(PolygonShape({0, 0, 3, 0, 3, 3, 0, 3}));
        CHECK(bar.separates(square));
        CHECK(bar.crosses(square) == square.separates(bar));
    }

    SUBCASE("a region that misses a set in one piece cuts nothing") {
        const Region distant(PolygonShape({20, 20, 22, 20, 22, 22, 20, 22}));
        CHECK(!distant.separates(RegionSet{Region(PolygonShape({0, 0, 2, 0, 2, 2, 0, 2}))}));
        // But a set already in two pieces stays in two.
        CHECK(distant.separates(apart()));
    }
}
