#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <variant>

// An infinite line is never bounded, so a polygon can neither contain it nor
// hold it on its boundary; symmetrically a 1D line cannot contain a 2D polygon.
TEST_CASE("Line and Polygon never contain each other") {
    using Point = pgl::Point<int>;
    using Line = pgl::Line<Point>;
    using Polygon = pgl::Polygon<Point>;

    const Polygon square({0, 0, 10, 0, 10, 10, 0, 10});
    const Line through({-1, 5}, {11, 5});  // straight through the interior

    CHECK_FALSE(square.contains(through));
    CHECK_FALSE(square.boundaryContains(through));
    CHECK_FALSE(square.interiorContains(through));

    CHECK_FALSE(through.contains(square));
    CHECK_FALSE(through.interiorContains(square));
}

// intersects / interiorsIntersect, both directions (the Line side forwards to
// the Polygon implementation by symmetry).
TEST_CASE("Polygon and Line intersection predicates") {
    using Point = pgl::Point<int>;
    using Line = pgl::Line<Point>;
    using Polygon = pgl::Polygon<Point>;

    const Polygon square({0, 0, 10, 0, 10, 10, 0, 10});

    SUBCASE("a line through the interior meets it everywhere") {
        const Line through({-1, 5}, {11, 5});

        CHECK(square.intersects(through));
        CHECK(square.interiorsIntersect(through));
        CHECK(through.intersects(square));
        CHECK(through.interiorsIntersect(square));
    }

    SUBCASE("a line along an edge grazes the boundary only") {
        const Line edge({-1, 0}, {1, 0});  // y = 0 runs along the bottom edge

        CHECK(square.intersects(edge));
        CHECK_FALSE(square.interiorsIntersect(edge));
        CHECK(edge.intersects(square));
        CHECK_FALSE(edge.interiorsIntersect(square));
    }

    SUBCASE("a line clear of the polygon misses it") {
        const Line away({-1, 20}, {1, 20});

        CHECK_FALSE(square.intersects(away));
        CHECK_FALSE(square.interiorsIntersect(away));
        CHECK_FALSE(away.intersects(square));
        CHECK_FALSE(away.interiorsIntersect(square));
    }
}

// separates / crosses, both directions.
TEST_CASE("Polygon and Line separate / cross each other") {
    using Point = pgl::Point<int>;
    using Line = pgl::Line<Point>;
    using Polygon = pgl::Polygon<Point>;

    const Polygon square({0, 0, 10, 0, 10, 10, 0, 10});

    SUBCASE("a line through the interior cuts the square, both ways") {
        const Line through({5, -1}, {5, 11});

        CHECK(through.separates(square));
        CHECK(square.separates(through));
        CHECK(through.crosses(square));
        CHECK(square.crosses(through));
    }

    SUBCASE("a line clear of the square does not cut it") {
        const Line away({20, -1}, {20, 11});

        CHECK_FALSE(away.separates(square));
        CHECK_FALSE(square.separates(away));
        CHECK_FALSE(away.crosses(square));
        CHECK_FALSE(square.crosses(away));
    }
}

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
