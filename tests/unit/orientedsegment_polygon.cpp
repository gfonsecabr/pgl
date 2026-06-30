#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <variant>

// OrientedSegment vs Polygon. Every predicate ignores the segment orientation and
// forwards to the unoriented Segment logic (OrientedSegment::asSegment()), so the
// expectations mirror the Segment/Polygon contract; the reversed segment must give
// identical answers.

TEST_CASE("Polygon predicates on an OrientedSegment ignore its direction") {
    using Point = pgl::Point<int>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using Polygon = pgl::Polygon<Point>;

    // Square with a rectangular notch in the top middle (gap x in (4,6), y > 4).
    const Polygon notch({0, 0, 10, 0, 10, 10, 6, 10, 6, 4, 4, 4, 4, 10, 0, 10});

    SUBCASE("segment strictly inside the solid base") {
        const OrientedSegment s({1, 1}, {9, 1});
        const OrientedSegment rev({9, 1}, {1, 1});

        CHECK(notch.contains(s));
        CHECK(notch.interiorContains(s));
        CHECK(notch.intersects(s));
        CHECK(notch.interiorsIntersect(s));
        CHECK_FALSE(notch.boundaryContains(s));

        // Direction does not matter.
        CHECK(notch.contains(rev));
        CHECK(notch.interiorContains(rev));
    }

    SUBCASE("segment spanning both arms across the notch gap") {
        const OrientedSegment s({1, 8}, {9, 8});

        CHECK_FALSE(notch.contains(s));          // the gap part is outside
        CHECK_FALSE(notch.interiorContains(s));
        CHECK(notch.intersects(s));
        CHECK(notch.interiorsIntersect(s));      // the arm parts meet the interior
    }

    SUBCASE("segment lying along the notch wall is on the boundary") {
        const OrientedSegment s({6, 5}, {6, 9});

        CHECK(notch.contains(s));
        CHECK(notch.boundaryContains(s));
        CHECK_FALSE(notch.interiorContains(s));
        CHECK(notch.intersects(s));
        CHECK_FALSE(notch.interiorsIntersect(s));
    }

    SUBCASE("segment entirely outside") {
        const OrientedSegment s({-3, 8}, {-1, 8});

        CHECK_FALSE(notch.contains(s));
        CHECK_FALSE(notch.intersects(s));
        CHECK_FALSE(notch.interiorsIntersect(s));
    }
}

TEST_CASE("Polygon separates / crosses an OrientedSegment") {
    using Point = pgl::Point<int>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using Polygon = pgl::Polygon<Point>;

    const Polygon square({0, 0, 10, 0, 10, 10, 0, 10});

    SUBCASE("chord clear across the square cuts it in two, both ways") {
        const OrientedSegment cut({-1, 5}, {11, 5});
        const OrientedSegment rev({11, 5}, {-1, 5});

        CHECK(cut.separates(square));
        CHECK(square.separates(cut));
        CHECK(rev.separates(square));
        CHECK(square.separates(rev));
    }

    SUBCASE("segment with one endpoint inside leaves a single piece") {
        const OrientedSegment slit({5, 5}, {11, 5});

        CHECK(square.interiorContains(slit[0]));
        CHECK_FALSE(slit.separates(square));
        CHECK_FALSE(square.separates(slit));
    }

    SUBCASE("a chord that pokes out both ends crosses the polygon") {
        // A.crosses(B) == A.separates(B) && B.separates(A).
        const OrientedSegment through({5, -5}, {5, 15});

        CHECK(through.separates(square));
        CHECK(square.separates(through));
        CHECK(through.crosses(square));
        CHECK(square.crosses(through));
    }
}

TEST_CASE("Polygon intersection with an OrientedSegment runs min-to-max") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using Polygon = pgl::Polygon<Point>;
    using Piece = std::variant<Point, Segment>;

    const Polygon notch({0, 0, 10, 0, 10, 10, 6, 10, 6, 4, 4, 4, 4, 10, 0, 10});

    SUBCASE("a forward and a reversed segment give the same ordered pieces") {
        const auto forward = notch.intersection(OrientedSegment({1, 8}, {9, 8}));
        const auto reverse = notch.intersection(OrientedSegment({9, 8}, {1, 8}));

        REQUIRE(forward.size() == 2);
        CHECK(forward[0] == Piece(Segment({1, 8}, {4, 8})));
        CHECK(forward[1] == Piece(Segment({6, 8}, {9, 8})));
        CHECK(reverse == forward);
    }
}
