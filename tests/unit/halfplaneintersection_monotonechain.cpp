#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <variant>
#include <vector>

using Point = pgl::Point<int>;
using Halfplane = pgl::Halfplane<Point>;
using Region = pgl::HalfplaneIntersection<Point>;
using MonotoneChain = pgl::MonotoneChain<Point>;

namespace {
Region box6() {
    return Region({Halfplane(0, 0, 1, 0), Halfplane(6, 0, 6, 1),
                   Halfplane(6, 6, 5, 6), Halfplane(0, 6, 0, 5)});
}
}  // namespace

TEST_CASE("Region contains a monotone chain") {
    const Region k = box6();
    const MonotoneChain inside(std::vector<Point>{{1, 1}, {3, 2}, {5, 1}});
    CHECK(k.contains(inside));
    const MonotoneChain crossing(std::vector<Point>{{1, 1}, {4, 4}, {8, 1}});
    CHECK(!k.contains(crossing));
}

TEST_CASE("Region intersects a monotone chain") {
    const Region k = box6();
    const MonotoneChain crossing(std::vector<Point>{{3, 3}, {5, 4}, {9, 3}});
    CHECK(k.intersects(crossing));
    CHECK(k.interiorsIntersect(crossing));
    const MonotoneChain away(std::vector<Point>{{7, 7}, {8, 8}, {9, 7}});
    CHECK(!k.intersects(away));
}

TEST_CASE("Region separates a monotone chain") {
    const Region k = box6();
    // A chain that dips through the box, with both ends outside, is cut in two.
    const MonotoneChain through(std::vector<Point>{{-2, 3}, {3, 3}, {8, 3}});
    CHECK(k.separates(through));
    // A chain fully inside the region is not cut.
    const MonotoneChain inside(std::vector<Point>{{1, 3}, {3, 3}, {5, 3}});
    CHECK(!k.separates(inside));
    // The other direction: that same chain is a crosscut of the box, so it
    // does disconnect the region — as it does the equivalent Polygon.
    CHECK(through.separates(k));
    CHECK(k.crosses(through));
    // A chain that stops inside leaves a slit, which keeps the region in one
    // piece.
    const MonotoneChain slit(std::vector<Point>{{-2, 3}, {3, 3}});
    CHECK(!slit.separates(k));
    CHECK(!inside.separates(k));
}

TEST_CASE("Distance to a monotone chain") {
    const Region k = box6();
    const MonotoneChain away(std::vector<Point>{{9, 0}, {10, 3}, {11, 0}});
    CHECK(k.squaredDistance<double>(away) == doctest::Approx(9.0));
    const MonotoneChain touching(std::vector<Point>{{3, 3}, {5, 4}, {9, 3}});
    CHECK(k.squaredDistance<double>(touching) == doctest::Approx(0.0));
}

TEST_CASE("Region intersection with a monotone chain") {
    using Segment = pgl::Segment<Point>;
    using Piece = std::variant<Point, Segment>;
    const Region k = box6();

    SUBCASE("each edge is clipped to the box") {
        const MonotoneChain chain(std::vector<Point>{{-2, 3}, {3, 3}, {9, 9}});
        const auto pieces = k.intersection<int>(chain);

        REQUIRE(pieces.size() == 2);
        CHECK(pieces[0] == Piece(Segment({0, 3}, {3, 3})));
        CHECK(pieces[1] == Piece(Segment({3, 3}, {6, 6})));
    }

    SUBCASE("a chain agrees with the polyline it views itself as") {
        const MonotoneChain chain(std::vector<Point>{{-2, 3}, {3, 3}, {9, 9}});
        CHECK(k.intersection<int>(chain) == k.intersection<int>(chain.asPolyline()));
        CHECK(chain.intersection<int>(k) == k.intersection<int>(chain));
    }

    SUBCASE("a chain outside the box meets nothing") {
        CHECK(k.intersection<int>(MonotoneChain(std::vector<Point>{{7, 7}, {8, 9}, {9, 7}})).empty());
    }
}
