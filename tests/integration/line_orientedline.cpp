#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <compare>

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
