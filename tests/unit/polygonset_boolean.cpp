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
        const auto result = outer.regularizedUnion<int>(square(20, 20, 2));
        CHECK(result.componentCount() == 2);
        CHECK(result.twiceArea() == 200 + 8);
    }

    SUBCASE("a symmetric difference") {
        const auto result = outer.symmetricDifference<int>(square(5, 5, 10));
        CHECK(result.twiceArea() == 2 * (100 + 100 - 2 * 25));
    }

    SUBCASE("a region's own operations answer the same way") {
        const Region region(outer, std::vector{inner});
        const auto result = region.regularizedUnion<int>(inner);
        CHECK(std::is_same_v<decltype(result), const RegionSet>);
        REQUIRE(result.componentCount() == 1);
        CHECK(!result.component(0).hasHoles());
        CHECK(result.twiceArea() == 200);
    }

    SUBCASE("the region-valued intersection") {
        const Region region(outer, std::vector{inner});
        const auto result = region.regularizedIntersection<int>(square(-1, -1, 20));
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

TEST_CASE("regularizedUnionOf unites a range in one arrangement") {
    const std::vector<PolygonShape> shapes{
        square(0, 0, 4), square(3, 0, 4), square(6, 0, 4)};

    const auto result = pgl::regularizedUnionOf<Point>(shapes);

    CHECK(std::is_same_v<decltype(result), const RegionSet>);
    CHECK(result == PolygonShape({0, 0, 10, 0, 10, 4, 0, 4}).asPolygonSet());

    const std::vector<PolygonShape> empty;
    CHECK(pgl::regularizedUnionOf<Point>(empty).empty());
}

TEST_CASE("regularizedUnionOf unites shapes that carry their edges as an array") {
    using TriangleShape = pgl::Triangle<Point>;
    using RectangleShape = pgl::Rectangle<Point>;

    SUBCASE("a range of triangles") {
        const std::vector<TriangleShape> triangles{
            TriangleShape(Point(0, 0), Point(4, 0), Point(0, 4)),
            TriangleShape(Point(4, 4), Point(4, 0), Point(0, 4))};

        const auto result = pgl::regularizedUnionOf<Point>(triangles);

        CHECK(result == square(0, 0, 4).asPolygonSet());
    }

    SUBCASE("a range of rectangles, with a degenerate one and a repeat") {
        const std::vector<RectangleShape> rectangles{
            RectangleShape(Point(0, 0), Point(4, 4)), RectangleShape(Point(2, 2), Point(6, 6)),
            RectangleShape(Point(0, 0), Point(4, 4)), RectangleShape(Point(0, 0), Point(0, 4)),
            RectangleShape(Point(10, 10), Point(12, 12))};

        const auto result = pgl::regularizedUnionOf<Point>(rectangles);

        REQUIRE(result.componentCount() == 2);
        CHECK(result.twiceArea() == 2 * (16 + 16 - 4 + 4));
    }

    SUBCASE("asserting simple boundaries picks the same answer") {
        const std::vector<RectangleShape> rectangles{RectangleShape(Point(0, 0), Point(4, 4)),
                                                     RectangleShape(Point(2, 2), Point(6, 6))};

        CHECK(pgl::regularizedUnionOf<Point>(rectangles, true) ==
              pgl::regularizedUnionOf<Point>(rectangles));
    }
}

TEST_CASE("the many-triangle union extracts only exposed boundary intervals") {
    using TriangleShape = pgl::Triangle<Point>;

    SUBCASE("a grid of shared edges closes into one square") {
        std::vector<TriangleShape> triangles;
        for (int y = 0; y < 4; ++y) {
            for (int x = 0; x < 4; ++x) {
                triangles.emplace_back(Point(x, y), Point(x + 1, y), Point(x, y + 1));
                triangles.emplace_back(Point(x + 1, y + 1), Point(x, y + 1),
                                       Point(x + 1, y));
            }
        }

        CHECK(pgl::regularizedUnionOf<pgl::EPoint>(triangles) ==
              pgl::EPolygon(square(0, 0, 4)).asPolygonSet());
    }

    SUBCASE("overlapping lattice triangles agree with the full overlay") {
        std::vector<TriangleShape> triangles;
        for (int i = 0; i < 20; ++i) {
            const int x = (7 * i) % 11;
            const int y = (5 * i) % 9;
            triangles.emplace_back(Point(x, y), Point(x + 8, y + 1 + i % 2),
                                   Point(x + 2 + i % 3, y + 9));
        }

        const auto exposed = pgl::regularizedUnionOf<pgl::EPoint>(triangles);
        const auto fullOverlay =
            pgl::detail::regularizedUnionByCoverage<pgl::EPoint>(triangles);
        CHECK(exposed == fullOverlay);
    }
}

TEST_CASE("the simple-boundary sweep preserves a large polygon pair union") {
    const auto jagged = [](int phase) {
        std::vector<Point> vertices;
        for (int x = 0; x < 130; ++x) {
            vertices.emplace_back(x, 10 + (x + phase) % 2);
        }
        for (int x = 129; x >= 0; --x) {
            vertices.emplace_back(x, -10 - (x + phase) % 2);
        }
        return PolygonShape(std::move(vertices));
    };
    const PolygonShape a = jagged(0);
    const PolygonShape b = jagged(1);

    const auto swept = a.regularizedUnion<pgl::ERational>(b);
    const auto fullOverlay = pgl::detail::regularizedUnion<pgl::EPoint>(a, b);
    CHECK(swept == fullOverlay);
}

TEST_CASE("regularizedUnionOf takes a range of sets by their components") {
    const RegionSet left = square(0, 0, 4).regularizedUnion<int>(square(10, 0, 4));
    const RegionSet right = square(2, 2, 4).regularizedUnion<int>(square(20, 0, 4));
    REQUIRE(left.componentCount() == 2);

    const std::vector<RegionSet> sets{left, right, left};
    const auto result = pgl::regularizedUnionOf<Point>(sets);

    CHECK(result.componentCount() == 3);
    CHECK(result.twiceArea() == 2 * (16 + 16 - 4 + 16 + 16));
    CHECK(pgl::regularizedUnionOf<Point>(sets, true) == result);
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
        const auto united = holed.regularizedUnion<int>(other);
        CHECK(std::is_same_v<decltype(united), const RegionSet>);
        CHECK(united.componentCount() == 1);
        // The two overlap on [8,10]x[8,10], which neither of them holes out.
        CHECK(united.twiceArea() == (200 - 32) + (200 - 32) - 8);
    }

    SUBCASE("a set intersected with itself is itself") {
        CHECK(holed.regularizedIntersection<int>(holed) == holed);
    }

    SUBCASE("a set differenced from itself is empty") {
        CHECK(holed.difference<int>(holed).empty());
        CHECK(holed.symmetricDifference<int>(holed).empty());
    }

    SUBCASE("a union with itself is idempotent") {
        CHECK(holed.regularizedUnion<int>(holed) == holed);
    }

    SUBCASE("a multi-component receiver goes in whole") {
        const RegionSet apart(std::vector{Region(square(0, 0, 2)), Region(square(5, 5, 2))});
        const auto shifted = apart.regularizedUnion<int>(square(1, 0, 2));
        // The first component grows and the second is untouched.
        REQUIRE(shifted.componentCount() == 2);
        CHECK(shifted.twiceArea() == 2 * (4 + 2 + 4));
        const auto cut = apart.difference<int>(PolygonShape({-1, 0, 10, 0, 10, 1, -1, 1}));
        CHECK(cut.componentCount() == 2);
        CHECK(cut.twiceArea() == 2 * (2 + 4));
    }

    SUBCASE("every area operand is accepted") {
        CHECK(!holed.regularizedIntersection<int>(pgl::Rectangle<Point>(Point(0, 0), Point(2, 2))).empty());
        CHECK(!holed.regularizedIntersection<int>(pgl::Triangle<Point>(Point(0, 0), Point(2, 0), Point(0, 2)))
                   .empty());
        CHECK(!holed.regularizedIntersection<int>(pgl::Convex<Point>({0, 0, 2, 0, 2, 2, 0, 2})).empty());
        CHECK(!holed.regularizedIntersection<int>(Region(square(0, 0, 2))).empty());
    }

    SUBCASE("an unbounded operand is clipped to the set first") {
        const auto half = holed.regularizedIntersection<int>(pgl::Halfplane<Point>(Point(0, 0), Point(1, 0)));
        CHECK(half.componentCount() == 1);
        CHECK(half.twiceArea() == 200 - 32);
        const auto below = holed.regularizedIntersection<int>(pgl::Halfplane<Point>(Point(1, 0), Point(0, 0)));
        CHECK(below.empty());
    }

    SUBCASE("a set is accepted as the argument of a union too") {
        // The pair is defined on the set, and the lower-ranked receiver hands it
        // over, so the answer does not depend on which side the set is.
        const PolygonShape cap = square(0, 0, 10);
        const auto fromPolygon = cap.regularizedUnion<int>(holed);
        CHECK(std::is_same_v<decltype(fromPolygon), const RegionSet>);
        CHECK(fromPolygon == holed.regularizedUnion<int>(cap));
        CHECK(fromPolygon.twiceArea() == 200);  // the hole is filled back in

        const Region region(square(0, 0, 4), std::vector{square(1, 1, 2)});
        const auto fromRegion = region.regularizedUnion<int>(holed);
        CHECK(std::is_same_v<decltype(fromRegion), const RegionSet>);
        CHECK(fromRegion == holed.regularizedUnion<int>(region));
        // The region's own hole [1,3]x[1,3] is covered by the set, and all the
        // region adds is the corner [3,4]x[3,4] of the set's hole.
        CHECK(fromRegion.twiceArea() == 200 - 32 + 2 * 1);
    }

    SUBCASE("a disjoint operand leaves an intersection empty and a difference whole") {
        const PolygonShape elsewhere = square(50, 50, 2);
        CHECK(holed.regularizedIntersection<int>(elsewhere).empty());
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
