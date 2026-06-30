#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <variant>

#include "pgl.hpp"

TEST_CASE("Line containment of OrientedSegment, and OrientedSegment containment of Line") {
    using Point = pgl::Point<int>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using Line = pgl::Line<Point>;

    const Line diagonal({0, 0}, {4, 4});
    const OrientedSegment col_os({3, 3}, {1, 1});     // collinear, going down-left
    const OrientedSegment off_os({0, 4}, {4, 0});     // crosses the diagonal

    SUBCASE("Line contains a collinear OrientedSegment") {
        CHECK_MESSAGE(diagonal.contains(col_os), diagonal, " contains ", col_os);
        CHECK_MESSAGE(diagonal.interiorContains(col_os), diagonal, " interiorContains ", col_os);
    }

    SUBCASE("Line does not contain an OrientedSegment that crosses it") {
        CHECK_FALSE_MESSAGE(diagonal.contains(off_os), diagonal, " contains ", off_os);
    }

    SUBCASE("OrientedSegment can never contain a non-degenerate Line") {
        CHECK_FALSE_MESSAGE(col_os.contains(diagonal), col_os, " contains ", diagonal);
        CHECK_FALSE_MESSAGE(col_os.interiorContains(diagonal), col_os, " interiorContains ", diagonal);
    }
}

TEST_CASE("OrientedSegment and Line intersection predicates, both directions") {
    using Point = pgl::Point<int>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using Line = pgl::Line<Point>;

    const Line vert({2, -2}, {2, 2});    // vertical line x = 2
    const OrientedSegment horiz({4, 0}, {0, 0}); // horizontal, going left

    SUBCASE("crossing: both intersect and both interiors intersect") {
        CHECK_MESSAGE(vert.intersects(horiz), vert, " intersects ", horiz);
        CHECK_MESSAGE(horiz.intersects(vert), horiz, " intersects ", vert);
        CHECK_MESSAGE(vert.interiorsIntersect(horiz), vert, " interiorsIntersect ", horiz);
        CHECK_MESSAGE(horiz.interiorsIntersect(vert), horiz, " interiorsIntersect ", vert);
    }

    SUBCASE("collinear: intersects and interiorsIntersect are both true") {
        const Line col({0, 0}, {4, 0});
        const OrientedSegment col_os({4, 0}, {0, 0});
        CHECK_MESSAGE(col.intersects(col_os), col, " intersects ", col_os);
        CHECK_MESSAGE(col_os.intersects(col), col_os, " intersects ", col);
        CHECK_MESSAGE(col.interiorsIntersect(col_os), col, " interiorsIntersect ", col_os);
        CHECK_MESSAGE(col_os.interiorsIntersect(col), col_os, " interiorsIntersect ", col);
    }

    SUBCASE("touching at source endpoint: intersects but interiors do not") {
        const OrientedSegment touch({2, 0}, {5, 3});  // source (2,0) on vert, goes off it
        CHECK_MESSAGE(vert.intersects(touch), vert, " intersects ", touch);
        CHECK_MESSAGE(touch.intersects(vert), touch, " intersects ", vert);
        CHECK_FALSE_MESSAGE(vert.interiorsIntersect(touch), vert, " interiorsIntersect ", touch);
        CHECK_FALSE_MESSAGE(touch.interiorsIntersect(vert), touch, " interiorsIntersect ", vert);
    }

    SUBCASE("disjoint: no intersection at all") {
        const Line far({10, 0}, {10, 5});
        CHECK_FALSE_MESSAGE(far.intersects(horiz), far, " intersects ", horiz);
        CHECK_FALSE_MESSAGE(horiz.intersects(far), horiz, " intersects ", far);
    }
}

TEST_CASE("OrientedSegment and Line separates and crosses, both directions") {
    using Point = pgl::Point<int>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using Line = pgl::Line<Point>;

    const Line vert({2, -2}, {2, 2});
    const OrientedSegment horiz({4, 0}, {0, 0});

    SUBCASE("proper crossing: both separate each other and both cross") {
        CHECK_MESSAGE(horiz.separates(vert), horiz, " separates ", vert);
        CHECK_MESSAGE(vert.separates(horiz), vert, " separates ", horiz);
        CHECK_MESSAGE(horiz.crosses(vert), horiz, " crosses ", vert);
        CHECK_MESSAGE(vert.crosses(horiz), vert, " crosses ", horiz);
    }

    SUBCASE("endpoint touch only: segment separates line, line does not separate segment") {
        const OrientedSegment touch({2, 0}, {5, 3}); // source (2,0) on vert, goes off it
        // touch touches vert at its source endpoint; vert minus {(2,0)} = two rays = disconnected
        CHECK_MESSAGE(touch.separates(vert), touch, " separates ", vert);
        // vert cannot separate touch because touch's min endpoint lies on vert (not strictly opposite)
        CHECK_FALSE_MESSAGE(vert.separates(touch), vert, " separates ", touch);
        CHECK_FALSE_MESSAGE(touch.crosses(vert), touch, " crosses ", vert);
        CHECK_FALSE_MESSAGE(vert.crosses(touch), vert, " crosses ", touch);
    }

    SUBCASE("collinear: segment separates line, line does not separate segment") {
        const Line col({0, 0}, {4, 0});
        // horiz lies on col, so col doesn't disconnect it
        CHECK_FALSE_MESSAGE(col.separates(horiz), col, " separates ", horiz);
        // horiz (as segment) removes a piece from col, splitting it into two rays
        CHECK_MESSAGE(horiz.separates(col), horiz, " separates ", col);
        CHECK_FALSE_MESSAGE(horiz.crosses(col), horiz, " crosses ", col);
        CHECK_FALSE_MESSAGE(col.crosses(horiz), col, " crosses ", horiz);
    }

    SUBCASE("parallel segment misses the line entirely") {
        const Line col({0, 0}, {4, 0});
        const OrientedSegment parallel_off({0, 2}, {4, 2}); // y = 2, above col
        CHECK_FALSE_MESSAGE(col.separates(parallel_off), col, " separates ", parallel_off);
        CHECK_FALSE_MESSAGE(parallel_off.separates(col), parallel_off, " separates ", col);
        CHECK_FALSE_MESSAGE(col.crosses(parallel_off), col, " crosses ", parallel_off);
        CHECK_FALSE_MESSAGE(parallel_off.crosses(col), parallel_off, " crosses ", col);
    }
}

TEST_CASE("OrientedSegment and Line intersection construction, both directions") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using Line = pgl::Line<Point>;

    const Line vert({2, -2}, {2, 2});
    const OrientedSegment horiz({4, 0}, {0, 0});

    SUBCASE("proper crossing yields a single point") {
        const auto r1 = vert.intersection(horiz);
        REQUIRE(r1);
        REQUIRE(std::holds_alternative<Point>(*r1));
        CHECK_MESSAGE(std::get<Point>(*r1) == Point(2, 0), "vert ∩ horiz");

        const auto r2 = horiz.intersection(vert);
        REQUIRE(r2);
        REQUIRE(std::holds_alternative<Point>(*r2));
        CHECK_MESSAGE(std::get<Point>(*r2) == Point(2, 0), "horiz ∩ vert");
    }

    SUBCASE("collinear oriented segment clipped by the line returns that segment") {
        const Line col({0, 0}, {4, 0});
        const auto r1 = col.intersection(horiz);
        REQUIRE(r1);
        REQUIRE(std::holds_alternative<Segment>(*r1));
        CHECK_MESSAGE(std::get<Segment>(*r1) == Segment({0, 0}, {4, 0}), "col ∩ horiz");
    }

    SUBCASE("disjoint: empty result") {
        const Line far({10, 0}, {10, 5});
        CHECK_FALSE_MESSAGE(vert.intersection(horiz) == std::nullopt, "just checking non-empty");
        CHECK_FALSE_MESSAGE(far.intersection(horiz), "far ∩ horiz should be empty");
    }

    SUBCASE("touching at source endpoint returns a point") {
        const OrientedSegment touch({2, 0}, {5, 3}); // source (2,0) on vert
        const auto r = vert.intersection(touch);
        REQUIRE(r);
        REQUIRE(std::holds_alternative<Point>(*r));
        CHECK_MESSAGE(std::get<Point>(*r) == Point(2, 0), "vert ∩ touch");
    }
}
