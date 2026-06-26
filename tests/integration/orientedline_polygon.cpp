#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <variant>

// OrientedLine vs Polygon. Like the segment case, every predicate ignores the
// orientation and forwards to the unoriented Line logic, so the expectations
// mirror the Line/Polygon contract and the reversed line must agree.

TEST_CASE("Polygon predicates on an OrientedLine ignore its direction") {
    using Point = pgl::Point<int>;
    using OrientedLine = pgl::OrientedLine<Point>;
    using Polygon = pgl::Polygon<Point>;

    // U-shape: prongs x in [0,3] and [7,10], joined at the bottom y in [0,3].
    const Polygon u({0, 0, 10, 0, 10, 10, 7, 10, 7, 3, 3, 3, 3, 10, 0, 10});

    SUBCASE("a horizontal line high up meets both prongs") {
        const OrientedLine high({-5, 5}, {5, 5});
        const OrientedLine rev({5, 5}, {-5, 5});

        CHECK(u.intersects(high));
        CHECK(u.interiorsIntersect(high));
        CHECK_FALSE(u.contains(high));         // an infinite line is never contained
        CHECK_FALSE(u.boundaryContains(high));

        CHECK(u.intersects(rev));
        CHECK(u.interiorsIntersect(rev));
    }

    SUBCASE("a line missing the polygon does not meet it") {
        const OrientedLine away({-5, 20}, {5, 20});

        CHECK_FALSE(u.intersects(away));
        CHECK_FALSE(u.interiorsIntersect(away));
    }

    SUBCASE("a line along a top edge touches the boundary only") {
        // y = 10 runs along the left prong's top edge (x in [0,3]).
        const OrientedLine top({-1, 10}, {1, 10});

        CHECK(u.intersects(top));
        CHECK_FALSE(u.interiorsIntersect(top));  // grazes the boundary, not the interior
    }
}

TEST_CASE("Polygon separates / crosses an OrientedLine") {
    using Point = pgl::Point<int>;
    using OrientedLine = pgl::OrientedLine<Point>;
    using Polygon = pgl::Polygon<Point>;

    const Polygon square({0, 0, 10, 0, 10, 10, 0, 10});

    SUBCASE("a line through the interior cuts the square, both ways") {
        const OrientedLine through({5, -1}, {5, 11});
        const OrientedLine rev({5, 11}, {5, -1});

        CHECK(through.separates(square));
        CHECK(square.separates(through));
        CHECK(through.crosses(square));
        CHECK(rev.separates(square));
        CHECK(square.separates(rev));
    }

    SUBCASE("a line clear of the square does not cut it") {
        const OrientedLine away({20, -1}, {20, 11});

        CHECK_FALSE(away.separates(square));
        CHECK_FALSE(square.separates(away));
    }
}

TEST_CASE("Polygon intersection with an OrientedLine runs min-to-max") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using OrientedLine = pgl::OrientedLine<Point>;
    using Polygon = pgl::Polygon<Point>;
    using Piece = std::variant<Point, Segment>;

    // U-shape again.
    const Polygon u({0, 0, 10, 0, 10, 10, 7, 10, 7, 3, 3, 3, 3, 10, 0, 10});

    SUBCASE("a high horizontal line clips both prongs, direction-independent") {
        const auto forward = u.intersection(OrientedLine({-5, 5}, {5, 5}));
        const auto reverse = u.intersection(OrientedLine({5, 5}, {-5, 5}));

        REQUIRE(forward.size() == 2);
        CHECK(forward[0] == Piece(Segment({0, 5}, {3, 5})));
        CHECK(forward[1] == Piece(Segment({7, 5}, {10, 5})));
        CHECK(reverse == forward);
    }

    SUBCASE("a line across the solid base is a single chord") {
        const auto pieces = u.intersection(OrientedLine({0, 1}, {1, 1}));

        REQUIRE(pieces.size() == 1);
        CHECK(pieces[0] == Piece(Segment({0, 1}, {10, 1})));
    }
}
