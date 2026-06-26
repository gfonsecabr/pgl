#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <compare>
#include <cstdint>
#include <variant>

TEST_CASE("OrientedLine crossingOrder orders lines by their crossing point") {
    using Point = pgl::Point<int>;
    using Line = pgl::Line<Point>;
    using OrientedLine = pgl::OrientedLine<Point>;

    // Oriented rightward along the x-axis.
    const OrientedLine axis({0, 0}, {10, 0});
    const Line at_x2({2, -1}, {2, 1});   // crosses the axis at (2, 0)
    const Line at_x5({5, -3}, {5, 4});   // crosses the axis at (5, 0)

    SUBCASE("the earlier crossing compares less, the later one greater") {
        CHECK(axis.crossingOrder(at_x2, at_x5) == std::partial_ordering::less);
        CHECK(axis.crossingOrder(at_x5, at_x2) == std::partial_ordering::greater);
    }

    SUBCASE("reversing the orientation flips the order") {
        const OrientedLine reversed({10, 0}, {0, 0});

        CHECK(reversed.crossingOrder(at_x2, at_x5) == std::partial_ordering::greater);
        CHECK(reversed.crossingOrder(at_x5, at_x2) == std::partial_ordering::less);
    }

    SUBCASE("lines meeting the oriented line at the same point are equivalent") {
        const Line vertical_at_3({3, -1}, {3, 1});       // crosses at (3, 0)
        const Line diagonal_through_3({1, -2}, {5, 2});  // also crosses at (3, 0)

        CHECK(axis.crossingOrder(vertical_at_3, diagonal_through_3)
              == std::partial_ordering::equivalent);
    }

    SUBCASE("a line parallel to the oriented line is unordered") {
        const Line parallel({-4, 4}, {7, 4});   // horizontal, parallel to the axis

        CHECK(axis.crossingOrder(parallel, at_x5) == std::partial_ordering::unordered);
        CHECK(axis.crossingOrder(at_x5, parallel) == std::partial_ordering::unordered);
        // Both parallel is still unordered.
        const Line parallel2({0, -2}, {3, -2});
        CHECK(axis.crossingOrder(parallel, parallel2) == std::partial_ordering::unordered);
    }
}

TEST_CASE("OrientedLine crossingOrder on a diagonal oriented line") {
    using Point = pgl::Point<int>;
    using Line = pgl::Line<Point>;
    using OrientedLine = pgl::OrientedLine<Point>;

    // Oriented along y = x.
    const OrientedLine diagonal({0, 0}, {4, 4});
    const Line at_x1({1, -2}, {1, 5});   // crosses the diagonal at (1, 1)
    const Line at_x3({3, -2}, {3, 5});   // crosses the diagonal at (3, 3)

    CHECK(diagonal.crossingOrder(at_x1, at_x3) == std::partial_ordering::less);
    CHECK(diagonal.crossingOrder(at_x3, at_x1) == std::partial_ordering::greater);

    // A line parallel to the diagonal (direction (1, 1)) is unordered.
    const Line parallel_diagonal({0, 3}, {3, 6});
    CHECK(diagonal.crossingOrder(parallel_diagonal, at_x1) == std::partial_ordering::unordered);
}

// An OrientedLine is geometrically the same infinite line as its asLine(): the
// orientation only matters for crossingOrder. Every other binary predicate
// (contains, interiorContains, intersects, interiorsIntersect, separates,
// crosses, intersection) therefore behaves exactly like Line vs Line, in both
// directions. The cases below pin that down.

TEST_CASE("Line and OrientedLine predicates ignore orientation") {
    using Point = pgl::Point<int>;
    using Line = pgl::Line<Point>;
    using OrientedLine = pgl::OrientedLine<Point>;

    const Line diagonal({0, 0}, {4, 4});

    SUBCASE("oriented line collinear with the line (same direction)") {
        const OrientedLine same({10, 10}, {20, 20});

        CHECK(diagonal.contains(same));
        CHECK(same.contains(diagonal));
        CHECK(diagonal.interiorContains(same));
        CHECK(same.interiorContains(diagonal));
        CHECK(diagonal.intersects(same));
        CHECK(same.intersects(diagonal));
        CHECK(diagonal.interiorsIntersect(same));
        CHECK(same.interiorsIntersect(diagonal));
        CHECK_FALSE(diagonal.crosses(same));
        CHECK_FALSE(same.crosses(diagonal));
        CHECK_FALSE(diagonal.separates(same));
        CHECK_FALSE(same.separates(diagonal));

        const auto isec = diagonal.intersection(same);
        REQUIRE(isec);
        REQUIRE(std::holds_alternative<Line>(*isec));
        CHECK(std::get<Line>(*isec).contains(Point(0, 0)));
        CHECK(std::get<Line>(*isec).contains(Point(4, 4)));
    }

    SUBCASE("oriented line collinear but pointing the opposite way") {
        const OrientedLine reversed({20, 20}, {10, 10});

        CHECK(diagonal.contains(reversed));
        CHECK(reversed.contains(diagonal));
        CHECK(diagonal.intersects(reversed));
        CHECK_FALSE(diagonal.crosses(reversed));
        CHECK_FALSE(diagonal.separates(reversed));

        const auto isec = diagonal.intersection(reversed);
        REQUIRE(isec);
        REQUIRE(std::holds_alternative<Line>(*isec));
    }

    SUBCASE("oriented line crossing the line at a single point") {
        const OrientedLine crossing({0, 8}, {8, 0});   // crosses the diagonal at (4, 4)

        CHECK_FALSE(diagonal.contains(crossing));
        CHECK_FALSE(crossing.contains(diagonal));
        CHECK_FALSE(diagonal.interiorContains(crossing));
        CHECK(diagonal.intersects(crossing));
        CHECK(crossing.intersects(diagonal));
        CHECK(diagonal.interiorsIntersect(crossing));
        CHECK(crossing.interiorsIntersect(diagonal));
        CHECK(diagonal.crosses(crossing));
        CHECK(crossing.crosses(diagonal));
        CHECK(diagonal.separates(crossing));
        CHECK(crossing.separates(diagonal));

        const auto isec = diagonal.intersection(crossing);
        REQUIRE(isec);
        REQUIRE(std::holds_alternative<Point>(*isec));
        CHECK(std::get<Point>(*isec) == Point(4, 4));

        const auto isec_rev = crossing.intersection(diagonal);
        REQUIRE(isec_rev);
        REQUIRE(std::holds_alternative<Point>(*isec_rev));
        CHECK(std::get<Point>(*isec_rev) == Point(4, 4));
    }

    SUBCASE("oriented line parallel to the line and distinct") {
        const OrientedLine parallel({0, 3}, {4, 7});   // direction (1,1), offset up by 3

        CHECK_FALSE(diagonal.contains(parallel));
        CHECK_FALSE(parallel.contains(diagonal));
        CHECK(diagonal.parallel(parallel));
        CHECK_FALSE(diagonal.collinear(parallel));
        CHECK_FALSE(diagonal.intersects(parallel));
        CHECK_FALSE(parallel.intersects(diagonal));
        CHECK_FALSE(diagonal.interiorsIntersect(parallel));
        CHECK_FALSE(diagonal.crosses(parallel));
        CHECK_FALSE(parallel.crosses(diagonal));
        CHECK_FALSE(diagonal.separates(parallel));
        CHECK_FALSE(parallel.separates(diagonal));
        CHECK_FALSE(diagonal.intersection(parallel));
        CHECK_FALSE(parallel.intersection(diagonal));
    }
}

TEST_CASE("Vertical line crossed by an oriented line") {
    using Point = pgl::Point<int>;
    using Line = pgl::Line<Point>;
    using OrientedLine = pgl::OrientedLine<Point>;

    const Line vertical({2, 0}, {2, 1});
    const OrientedLine inbound({500, 500}, {1, 500});   // horizontal, crosses at (2, 500)

    CHECK(vertical.isVertical());
    CHECK(vertical.intersects(inbound));
    CHECK(inbound.intersects(vertical));
    CHECK(vertical.crosses(inbound));
    CHECK(inbound.crosses(vertical));
    CHECK(vertical.separates(inbound));

    const auto isec = vertical.intersection(inbound);
    REQUIRE(isec);
    REQUIRE(std::holds_alternative<Point>(*isec));
    CHECK(std::get<Point>(*isec) == Point(2, 500));
}

TEST_CASE("Horizontal line collinear with an oriented line") {
    using Point = pgl::Point<int>;
    using Line = pgl::Line<Point>;
    using OrientedLine = pgl::OrientedLine<Point>;

    const Line horizontal({0, 5}, {1, 5});
    const OrientedLine sliding({999, 5}, {2000, 5});   // same horizontal line

    CHECK(horizontal.isHorizontal());
    CHECK(horizontal.contains(sliding));
    CHECK(sliding.contains(horizontal));
    CHECK(horizontal.collinear(sliding));
    CHECK(horizontal.intersects(sliding));
    CHECK_FALSE(horizontal.crosses(sliding));

    const auto isec = horizontal.intersection(sliding);
    REQUIRE(isec);
    REQUIRE(std::holds_alternative<Line>(*isec));
}

TEST_CASE("Rational crossing point of a line and an oriented line") {
    using Rational = pgl::Rational<int64_t>;
    using IntPoint = pgl::Point<int>;
    using RationalPoint = pgl::Point<Rational>;
    using IntLine = pgl::Line<IntPoint>;
    using IntOrientedLine = pgl::OrientedLine<IntPoint>;

    const IntLine line({0, 0}, {2, 1});
    const IntOrientedLine crossing({100, 60}, {100, -50});   // vertical at x = 100

    CHECK(line.intersects(crossing));
    CHECK(line.crosses(crossing));

    const auto isec = line.intersection<Rational>(crossing);
    REQUIRE(isec);
    REQUIRE(std::holds_alternative<RationalPoint>(*isec));
    CHECK(std::get<RationalPoint>(*isec) == RationalPoint(Rational(100), Rational(50)));
}
