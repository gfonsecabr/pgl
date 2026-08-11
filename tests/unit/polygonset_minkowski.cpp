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

// Which type each region-valued Minkowski sum lands in, and why that is the
// tightest one. The rule is about **bodies**: a shape that is the closure of a
// connected, non-empty interior. Whenever one operand is one,
//
//     A ⊕ B  ⊇  ⋃_{a ∈ A} (a + B°),
//
// and the translates `a + B°` vary continuously along any path in `A`, so that
// union is connected and open and `A ⊕ B` is its closure. One component, holes
// and all: a `PolygonWithHoles`. A `PolygonSet` is the answer only where no
// operand is a body and the regularization can genuinely scatter the sum.

TEST_CASE("A sum with a body in it is a single region") {
    SUBCASE("a polygon receiver") {
        const PolygonShape u({0, 0, 6, 0, 6, 2, 4, 2, 4, 6, 6, 6, 6, 8, 0, 8});
        const auto sum = u.minkowskiSum<int>(RectangleShape(Point(0, 0), Point(1, 1)));
        CHECK(std::is_same_v<decltype(sum), const Region>);
        CHECK(sum.isValid());
    }

    SUBCASE("a region receiver") {
        const Region region(PolygonShape({0, 0, 10, 0, 10, 10, 0, 10}),
                            std::vector{PolygonShape({3, 3, 7, 3, 7, 7, 3, 7})});
        const auto sum = region.minkowskiSum<int>(RectangleShape(Point(0, 0), Point(1, 1)));
        CHECK(std::is_same_v<decltype(sum), const Region>);
        // The square grows by one unit and the hole is eroded by one.
        CHECK(sum.outer() == PolygonShape({0, 0, 11, 0, 11, 11, 0, 11}));
        REQUIRE(sum.holeCount() == 1);
        CHECK(sum.hole(0) == PolygonShape({4, 4, 7, 4, 7, 7, 4, 7}));
    }

    SUBCASE("a polyline receiver, where the operand is the body") {
        // The receiver has no area of its own, and needs none: one body on either
        // side is what the connectedness argument asks for.
        const PolylineShape chain({0, 0, 4, 0, 4, 4});
        const auto sum = chain.minkowskiSum<int>(RectangleShape(Point(0, 0), Point(1, 1)));
        CHECK(std::is_same_v<decltype(sum), const Region>);
        CHECK(sum.isValid());
    }

    SUBCASE("a sum with a point is still a translation, not a region") {
        const PolygonShape square({0, 0, 2, 0, 2, 2, 0, 2});
        CHECK(std::is_same_v<decltype(square.minkowskiSum(Point(1, 1))), PolygonShape>);
    }
}

TEST_CASE("A sum with no body in it stays a set of regions") {
    // Two thin operands are the pairs a region cannot hold, and nothing about
    // them is degenerate: an edge parallel to the segment sweeps out a segment,
    // which the regularization drops, and what is left can be in pieces.

    SUBCASE("a polyline receiver against a segment") {
        const PolylineShape chain({0, 0, 4, 0, 4, 4});
        const auto sum = chain.minkowskiSum<int>(SegmentShape(Point(0, 0), Point(0, 2)));
        CHECK(std::is_same_v<decltype(sum), const RegionSet>);
        CHECK(sum.componentCount() == 1);
    }

    SUBCASE("a monotone chain receiver against a segment") {
        const ChainShape chain({0, 0, 2, 2, 4, 0});
        const auto sum = chain.minkowskiSum<int>(SegmentShape(Point(0, 0), Point(0, 2)));
        CHECK(std::is_same_v<decltype(sum), const RegionSet>);
        CHECK(sum.componentCount() == 1);
    }

    SUBCASE("and it does scatter, on operands that are perfectly valid") {
        // The closed rectilinear chain against an axis-parallel segment: the two
        // walls parallel to the segment sweep nothing, and the two bands left do
        // not touch. Neither operand is degenerate.
        const PolylineShape square({0, 0, 8, 0, 8, 8, 0, 8, 0, 0});
        const auto sum = square.minkowskiSum<int>(SegmentShape(Point(0, 0), Point(0, 3)));
        REQUIRE(sum.componentCount() == 2);
    }
}

TEST_CASE("The two spellings of a pair agree") {
    const PolygonShape u({0, 0, 6, 0, 6, 2, 4, 2, 4, 6, 6, 6, 6, 8, 0, 8});
    const Region region(PolygonShape({0, 0, 10, 0, 10, 10, 0, 10}),
                        std::vector{PolygonShape({3, 3, 7, 3, 7, 7, 3, 7})});
    CHECK(u.minkowskiSum<int>(region) == region.minkowskiSum<int>(u));

    const PolylineShape chain({0, 0, 4, 0, 4, 4});
    const RectangleShape box(Point(0, 0), Point(1, 1));
    CHECK(chain.minkowskiSum<int>(box) == box.minkowskiSum<int>(chain));
}

TEST_CASE("A set receiver sums component by component") {
    // The sum distributes over a union and a set is one, so the answer is the
    // union of the component sums — which is also the whole construction.
    const PolygonShape left({0, 0, 4, 0, 4, 4, 0, 4});
    const PolygonShape right({10, 0, 14, 0, 14, 4, 10, 4});
    const auto set = left.regularizedUnion<int>(right);
    REQUIRE(set.componentCount() == 2);

    const RectangleShape unit(Point(0, 0), Point(1, 1));
    const auto grown = set.minkowskiSum<int>(unit);
    CHECK(std::is_same_v<decltype(grown), const RegionSet>);
    CHECK(grown == left.minkowskiSum<int>(unit).regularizedUnion<int>(right.minkowskiSum<int>(unit)));
    CHECK(grown.twiceArea() == 2 * 2 * 25);

    SUBCASE("components stay apart or merge, on the operand's size alone") {
        // Six units of gap: an operand narrower than that leaves two components,
        // one wider closes it. This is the receiver that needs a set however
        // nondegenerate its operands are.
        CHECK(set.minkowskiSum<int>(unit).componentCount() == 2);
        CHECK(set.minkowskiSum<int>(RectangleShape(Point(0, 0), Point(6, 1))).componentCount() == 1);
    }

    SUBCASE("every operand kind, and both spellings of each pair") {
        const PolygonShape triangle({0, 0, 2, 0, 0, 2});
        const Region region(PolygonShape({0, 0, 8, 0, 8, 8, 0, 8}),
                            std::vector{PolygonShape({2, 2, 6, 2, 6, 6, 2, 6})});
        const SegmentShape segment(Point(0, 0), Point(0, 3));
        const PolylineShape chain({0, 0, 2, 2});
        const ChainShape sorted({0, 0, 2, 2});

        CHECK(triangle.minkowskiSum<int>(set) == set.minkowskiSum<int>(triangle));
        CHECK(region.minkowskiSum<int>(set) == set.minkowskiSum<int>(region));
        CHECK(chain.minkowskiSum<int>(set) == set.minkowskiSum<int>(chain));
        CHECK(segment.minkowskiSum<int>(set) == set.minkowskiSum<int>(segment));
        CHECK(sorted.minkowskiSum<int>(set) == set.minkowskiSum<int>(sorted));
        CHECK(unit.minkowskiSum<int>(set) == grown);
    }

    SUBCASE("a set against a set") {
        // Two components each, three blobs: the middle pair of sums lands on the
        // same place and merges.
        const auto self = set.minkowskiSum<int>(set);
        CHECK(self.componentCount() == 3);
    }

    SUBCASE("a point is still a translation, and a half-plane still absorbs") {
        CHECK(std::is_same_v<decltype(set.minkowskiSum(Point(1, 1))), RegionSet>);
        const pgl::Halfplane<Point> up(Point(0, 0), Point(1, 0));
        CHECK(up.minkowskiSum(set) == up);
    }
}

TEST_CASE("A Minkowski sum feeds back into the boolean operations") {
    const PolygonShape square({0, 0, 8, 0, 8, 8, 0, 8});
    const auto grown = square.minkowskiSum<int>(RectangleShape(Point(0, 0), Point(2, 2)));
    CHECK(grown.twiceArea() == 2 * 100);

    SUBCASE("the ring the growth adds is a difference of two sets") {
        const auto ring = grown.difference<int>(square);
        CHECK(std::is_same_v<decltype(ring), const RegionSet>);
        CHECK(ring.twiceArea() == 2 * (100 - 64));
    }

    SUBCASE("and it can be grown again") {
        const auto twice = grown.difference<int>(square).regularizedUnion<int>(square);
        REQUIRE(twice.componentCount() == 1);
        CHECK(twice.component(0) == grown);
    }
}
