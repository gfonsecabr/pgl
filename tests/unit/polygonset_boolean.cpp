#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <type_traits>
#include <vector>

using Point = pgl::Point<int>;
using PolygonShape = pgl::Polygon<Point>;
using Region = pgl::PolygonWithHoles<Point>;
using RegionSet = pgl::PolygonSet<Point>;

static PolygonShape square(int x, int y, int side) {
    return PolygonShape({x, y, x + side, y, x + side, y + side, x, y + side});
}

TEST_CASE("The boolean operations return a PolygonSet") {
    const PolygonShape outer = square(0, 0, 10);
    const PolygonShape inner = square(3, 3, 4);

    SUBCASE("a difference that opens a hole") {
        const auto result = outer.difference<int>(inner);
        CHECK(std::is_same_v<decltype(result), const RegionSet>);
        REQUIRE(result.componentCount() == 1);
        CHECK(result.component(0).holeCount() == 1);
        CHECK(result.twiceArea() == 200 - 32);
    }

    SUBCASE("a difference that splits the receiver") {
        const auto result = outer.difference<int>(PolygonShape({-1, 4, 11, 4, 11, 6, -1, 6}));
        REQUIRE(result.componentCount() == 2);
        CHECK(result.twiceArea() == 200 - 40);
    }

    SUBCASE("a union of separated shapes keeps both") {
        const auto result = outer.unionWith<int>(square(20, 20, 2));
        CHECK(result.componentCount() == 2);
        CHECK(result.twiceArea() == 200 + 8);
    }

    SUBCASE("a symmetric difference") {
        const auto result = outer.symmetricDifference<int>(square(5, 5, 10));
        CHECK(result.twiceArea() == 2 * (100 + 100 - 2 * 25));
    }

    SUBCASE("a region's own operations answer the same way") {
        const Region region(outer, std::vector{inner});
        const auto result = region.unionWith<int>(inner);
        CHECK(std::is_same_v<decltype(result), const RegionSet>);
        REQUIRE(result.componentCount() == 1);
        CHECK(!result.component(0).hasHoles());
        CHECK(result.twiceArea() == 200);
    }

    SUBCASE("the region-valued intersection") {
        const Region region(outer, std::vector{inner});
        const auto result = region.intersection<int>(square(-1, -1, 20));
        CHECK(std::is_same_v<decltype(result), const RegionSet>);
        REQUIRE(result.componentCount() == 1);
        CHECK(result.component(0) == region);
    }

    SUBCASE("regularized returns a set too") {
        const Region region(outer, std::vector{PolygonShape({0, 4, 10, 4, 10, 6, 0, 6})});
        const auto result = region.regularized<int>();
        CHECK(std::is_same_v<decltype(result), const RegionSet>);
        CHECK(result.componentCount() == 2);
    }
}

TEST_CASE("The boolean operations are closed over PolygonSet") {
    const RegionSet holed = square(0, 0, 10).difference<int>(square(3, 3, 4));

    SUBCASE("a result feeds straight back in") {
        const auto again = holed.difference<int>(square(0, 0, 2));
        CHECK(std::is_same_v<decltype(again), const RegionSet>);
        CHECK(again.twiceArea() == 200 - 32 - 8);
    }

    SUBCASE("a set against a set") {
        const RegionSet other = square(8, 8, 10).difference<int>(square(11, 11, 4));
        const auto united = holed.unionWith<int>(other);
        CHECK(std::is_same_v<decltype(united), const RegionSet>);
        CHECK(united.componentCount() == 1);
        // The two overlap on [8,10]x[8,10], which neither of them holes out.
        CHECK(united.twiceArea() == (200 - 32) + (200 - 32) - 8);
    }

    SUBCASE("a set intersected with itself is itself") {
        CHECK(holed.intersection<int>(holed) == holed);
    }

    SUBCASE("a set differenced from itself is empty") {
        CHECK(holed.difference<int>(holed).isEmpty());
        CHECK(holed.symmetricDifference<int>(holed).isEmpty());
    }

    SUBCASE("a union with itself is idempotent") {
        CHECK(holed.unionWith<int>(holed) == holed);
    }

    SUBCASE("a multi-component receiver goes in whole") {
        const RegionSet apart(std::vector{Region(square(0, 0, 2)), Region(square(5, 5, 2))});
        const auto shifted = apart.unionWith<int>(square(1, 0, 2));
        // The first component grows and the second is untouched.
        REQUIRE(shifted.componentCount() == 2);
        CHECK(shifted.twiceArea() == 2 * (4 + 2 + 4));
        const auto cut = apart.difference<int>(PolygonShape({-1, 0, 10, 0, 10, 1, -1, 1}));
        CHECK(cut.componentCount() == 2);
        CHECK(cut.twiceArea() == 2 * (2 + 4));
    }

    SUBCASE("every area operand is accepted") {
        CHECK(!holed.intersection<int>(pgl::Rectangle<Point>(Point(0, 0), Point(2, 2))).isEmpty());
        CHECK(!holed.intersection<int>(pgl::Triangle<Point>(Point(0, 0), Point(2, 0), Point(0, 2)))
                   .isEmpty());
        CHECK(!holed.intersection<int>(pgl::Convex<Point>({0, 0, 2, 0, 2, 2, 0, 2})).isEmpty());
        CHECK(!holed.intersection<int>(Region(square(0, 0, 2))).isEmpty());
    }

    SUBCASE("an unbounded operand is clipped to the set first") {
        const auto half = holed.intersection<int>(pgl::Halfplane<Point>(Point(0, 0), Point(1, 0)));
        CHECK(half.componentCount() == 1);
        CHECK(half.twiceArea() == 200 - 32);
        const auto below = holed.intersection<int>(pgl::Halfplane<Point>(Point(1, 0), Point(0, 0)));
        CHECK(below.isEmpty());
    }

    SUBCASE("a disjoint operand leaves an intersection empty and a difference whole") {
        const PolygonShape elsewhere = square(50, 50, 2);
        CHECK(holed.intersection<int>(elsewhere).isEmpty());
        CHECK(holed.difference<int>(elsewhere) == holed);
    }
}

TEST_CASE("PolygonSet boolean results are valid sets") {
    const RegionSet split =
        square(0, 0, 10).difference<int>(PolygonShape({-1, 4, 11, 4, 11, 6, -1, 6}));

    SUBCASE("the engine's own output satisfies the set contract") {
        CHECK(split.isValid());
        CHECK(split.isRegular());
        CHECK(!split.isConnected());
    }

    SUBCASE("and regularizing it changes nothing") {
        CHECK(split.regularized<int>() == split);
    }

    SUBCASE("exact rational coordinates round-trip") {
        const pgl::EPolygon a({0, 0, 4, 0, 4, 4, 0, 4});
        const pgl::EPolygon b({2, 2, 6, 2, 6, 6, 2, 6});
        const auto result = a.difference<pgl::ERational>(b);
        CHECK(std::is_same_v<decltype(result), const pgl::EPolygonSet>);
        CHECK(result.twiceArea() == pgl::ERational(32 - 8));
    }
}
