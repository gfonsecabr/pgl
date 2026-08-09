#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <type_traits>
#include <vector>

using Point = pgl::Point<int>;
using PolygonShape = pgl::Polygon<Point>;
using Region = pgl::PolygonWithHoles<Point>;
using RegionSet = pgl::PolygonSet<Point>;
using RectangleShape = pgl::Rectangle<Point>;
using SegmentShape = pgl::Segment<Point>;
using PolylineShape = pgl::Polyline<Point>;
using ChainShape = pgl::MonotoneChain<Point>;

TEST_CASE("The region-valued Minkowski sums return a PolygonSet") {
    SUBCASE("a polygon receiver") {
        const PolygonShape u({0, 0, 6, 0, 6, 2, 4, 2, 4, 6, 6, 6, 6, 8, 0, 8});
        const auto sum = u.minkowskiSum<int>(RectangleShape(Point(0, 0), Point(1, 1)));
        CHECK(std::is_same_v<decltype(sum), const RegionSet>);
        CHECK(sum.componentCount() == 1);
    }

    SUBCASE("a region receiver") {
        const Region region(PolygonShape({0, 0, 10, 0, 10, 10, 0, 10}),
                            std::vector{PolygonShape({3, 3, 7, 3, 7, 7, 3, 7})});
        const auto sum = region.minkowskiSum<int>(RectangleShape(Point(0, 0), Point(1, 1)));
        CHECK(std::is_same_v<decltype(sum), const RegionSet>);
        REQUIRE(sum.componentCount() == 1);
        // The square grows by one unit and the hole is eroded by one.
        CHECK(sum.component(0).outer() == PolygonShape({0, 0, 11, 0, 11, 11, 0, 11}));
        REQUIRE(sum.component(0).holeCount() == 1);
        CHECK(sum.component(0).hole(0) == PolygonShape({4, 4, 7, 4, 7, 7, 4, 7}));
    }

    SUBCASE("a polyline receiver") {
        const PolylineShape chain({0, 0, 4, 0, 4, 4});
        const auto sum = chain.minkowskiSum<int>(RectangleShape(Point(0, 0), Point(1, 1)));
        CHECK(std::is_same_v<decltype(sum), const RegionSet>);
        CHECK(sum.componentCount() == 1);
    }

    SUBCASE("a monotone chain receiver against a thin operand") {
        const ChainShape chain({0, 0, 2, 2, 4, 0});
        const auto sum = chain.minkowskiSum<int>(SegmentShape(Point(0, 0), Point(0, 2)));
        CHECK(std::is_same_v<decltype(sum), const RegionSet>);
        CHECK(sum.componentCount() == 1);
    }

    SUBCASE("a sum with a point is still a translation, not a set") {
        const PolygonShape square({0, 0, 2, 0, 2, 2, 0, 2});
        CHECK(std::is_same_v<decltype(square.minkowskiSum(Point(1, 1))), PolygonShape>);
    }
}

TEST_CASE("A split Minkowski sum keeps every component") {
    // The behaviour change: these overloads used to return a single region and
    // silently drop all but the first component in canonical order when a
    // degenerate operand split the answer. A set of regions has room for the
    // rest.

    SUBCASE("two collinear segments sweep out nothing but a segment") {
        // Both operands are flat and parallel, so the sum has no area at all and
        // the regularization is empty — not a first component, but none.
        const PolygonShape flat({0, 0, 4, 0, 8, 0});
        const auto sum = flat.minkowskiSum<int>(PolygonShape({0, 0, 1, 0, 2, 0}));
        CHECK(sum.isEmpty());
    }

    SUBCASE("a flat receiver swept by a bent chain comes back in one piece") {
        // The sweep of a horizontal segment along a V is two bands meeting at a
        // point, which regularizes to two components — both of them returned.
        const PolygonShape flat({0, 0, 4, 0, 8, 0});
        const PolylineShape bent({0, 0, 2, 4, 4, 0});
        const auto sum = flat.minkowskiSum<int>(bent);
        CHECK(std::is_same_v<decltype(sum), const RegionSet>);
        CHECK(sum.componentCount() >= 1);
        // Whatever the split, the area is the whole swept region's.
        CHECK(sum.twiceArea() == bent.minkowskiSum<int>(flat).twiceArea());
    }

    SUBCASE("the two spellings of a pair agree component for component") {
        const PolygonShape u({0, 0, 6, 0, 6, 2, 4, 2, 4, 6, 6, 6, 6, 8, 0, 8});
        const Region region(PolygonShape({0, 0, 10, 0, 10, 10, 0, 10}),
                            std::vector{PolygonShape({3, 3, 7, 3, 7, 7, 3, 7})});
        CHECK(u.minkowskiSum<int>(region) == region.minkowskiSum<int>(u));
    }
}

TEST_CASE("A Minkowski sum feeds back into the boolean operations") {
    const PolygonShape square({0, 0, 8, 0, 8, 8, 0, 8});
    const auto grown = square.minkowskiSum<int>(RectangleShape(Point(0, 0), Point(2, 2)));
    REQUIRE(grown.componentCount() == 1);
    CHECK(grown.twiceArea() == 2 * 100);

    SUBCASE("the ring the growth adds is a difference of two sets") {
        const auto ring = grown.difference<int>(square);
        CHECK(std::is_same_v<decltype(ring), const RegionSet>);
        CHECK(ring.twiceArea() == 2 * (100 - 64));
    }

    SUBCASE("and it can be grown again") {
        const auto twice = grown.difference<int>(square).unionWith<int>(square);
        CHECK(twice == grown);
    }
}
