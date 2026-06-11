#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <variant>

// Polygon::intersection(Line) clips an infinite line against the closed polygon,
// returning the disjoint chords (segments) and isolated boundary touches (points)
// in order along the line.
TEST_CASE("Polygon intersects Line into points and chords") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Line = pgl::Line<Point>;
    using Polygon = pgl::Polygon<Point>;
    using Piece = std::variant<Point, Segment>;

    // U-shape: prongs x in [0,3] and [7,10], joined at the bottom y in [0,3].
    const Polygon u({0, 0, 10, 0, 10, 10, 7, 10, 7, 3, 3, 3, 3, 10, 0, 10});

    SUBCASE("a horizontal line high up cuts both prongs into two chords") {
        const auto pieces = u.intersection(Line({-5, 5}, {5, 5}));

        REQUIRE(pieces.size() == 2);
        CHECK(pieces[0] == Piece(Segment({0, 5}, {3, 5})));
        CHECK(pieces[1] == Piece(Segment({7, 5}, {10, 5})));
    }

    SUBCASE("a line across the solid base is a single chord") {
        const auto pieces = u.intersection(Line({0, 1}, {1, 1}));

        REQUIRE(pieces.size() == 1);
        CHECK(pieces[0] == Piece(Segment({0, 1}, {10, 1})));
    }

    SUBCASE("a line lying along the two top edges yields two chords") {
        // y = 10 runs along the prongs' top edges (x in [0,3] and [7,10]); the
        // notch between them opens the top, so the overlap is two segments.
        const auto pieces = u.intersection(Line({-1, 10}, {1, 10}));

        REQUIRE(pieces.size() == 2);
        CHECK(pieces[0] == Piece(Segment({0, 10}, {3, 10})));
        CHECK(pieces[1] == Piece(Segment({7, 10}, {10, 10})));
    }

    SUBCASE("a line missing the polygon yields nothing") {
        CHECK(u.intersection(Line({-5, 20}, {5, 20})).empty());
    }

    SUBCASE("a vertical line through a prong is one chord") {
        const auto pieces = u.intersection(Line({1, 0}, {1, 1}));

        REQUIRE(pieces.size() == 1);
        CHECK(pieces[0] == Piece(Segment({1, 0}, {1, 10})));
    }

    SUBCASE("an oriented line gives the same result, ignoring direction") {
        using OrientedLine = pgl::OrientedLine<Point>;
        const auto forward = u.intersection(OrientedLine({-5, 5}, {5, 5}));
        const auto reverse = u.intersection(OrientedLine({5, 5}, {-5, 5}));

        CHECK(forward == u.intersection(Line({-5, 5}, {5, 5})));
        CHECK(reverse == forward);
    }
}
