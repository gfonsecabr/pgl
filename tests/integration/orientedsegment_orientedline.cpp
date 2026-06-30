#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <variant>

#include "pgl.hpp"

TEST_CASE("OrientedLine containment of OrientedSegment, and OrientedSegment containment of OrientedLine") {
    using Point = pgl::Point<int>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using OrientedLine = pgl::OrientedLine<Point>;

    // diagonal: y = x, going up-right
    const OrientedLine diagonal({0, 0}, {4, 4});
    const OrientedSegment col_os({3, 3}, {1, 1});     // collinear, going down-left
    const OrientedSegment off_os({0, 4}, {4, 0});     // crosses the diagonal

    SUBCASE("OrientedLine contains a collinear OrientedSegment regardless of direction") {
        const OrientedSegment same_dir({1, 1}, {3, 3}); // same direction as diagonal
        CHECK_MESSAGE(diagonal.contains(col_os), diagonal, " contains ", col_os);
        CHECK_MESSAGE(diagonal.contains(same_dir), diagonal, " contains ", same_dir);
        CHECK_MESSAGE(diagonal.interiorContains(col_os), diagonal, " interiorContains ", col_os);
    }

    SUBCASE("OrientedLine does not contain an OrientedSegment that crosses it") {
        CHECK_FALSE_MESSAGE(diagonal.contains(off_os), diagonal, " contains ", off_os);
    }

    SUBCASE("OrientedSegment can never contain a non-degenerate OrientedLine") {
        CHECK_FALSE_MESSAGE(col_os.contains(diagonal), col_os, " contains ", diagonal);
        CHECK_FALSE_MESSAGE(col_os.interiorContains(diagonal), col_os, " interiorContains ", diagonal);
    }
}

TEST_CASE("OrientedSegment and OrientedLine intersection predicates, both directions") {
    using Point = pgl::Point<int>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using OrientedLine = pgl::OrientedLine<Point>;

    const OrientedLine vert({2, -2}, {2, 2});         // x = 2, pointing up
    const OrientedSegment horiz({4, 0}, {0, 0});      // y = 0, going left

    SUBCASE("crossing: both intersect and both interiors intersect") {
        CHECK_MESSAGE(vert.intersects(horiz), vert, " intersects ", horiz);
        CHECK_MESSAGE(horiz.intersects(vert), horiz, " intersects ", vert);
        CHECK_MESSAGE(vert.interiorsIntersect(horiz), vert, " interiorsIntersect ", horiz);
        CHECK_MESSAGE(horiz.interiorsIntersect(vert), horiz, " interiorsIntersect ", vert);
    }

    SUBCASE("collinear: intersects and interiorsIntersect are both true") {
        const OrientedLine col({0, 0}, {4, 0});
        const OrientedSegment col_os({4, 0}, {0, 0});
        CHECK_MESSAGE(col.intersects(col_os), col, " intersects ", col_os);
        CHECK_MESSAGE(col_os.intersects(col), col_os, " intersects ", col);
        CHECK_MESSAGE(col.interiorsIntersect(col_os), col, " interiorsIntersect ", col_os);
        CHECK_MESSAGE(col_os.interiorsIntersect(col), col_os, " interiorsIntersect ", col);
    }

    SUBCASE("touching at endpoint: intersects but interiors do not") {
        const OrientedSegment touch({2, 0}, {5, 3}); // source (2,0) on vert, goes off it
        CHECK_MESSAGE(vert.intersects(touch), vert, " intersects ", touch);
        CHECK_MESSAGE(touch.intersects(vert), touch, " intersects ", vert);
        CHECK_FALSE_MESSAGE(vert.interiorsIntersect(touch), vert, " interiorsIntersect ", touch);
        CHECK_FALSE_MESSAGE(touch.interiorsIntersect(vert), touch, " interiorsIntersect ", vert);
    }

    SUBCASE("disjoint: no intersection") {
        const OrientedLine far({10, 0}, {10, 5});
        CHECK_FALSE_MESSAGE(far.intersects(horiz), far, " intersects ", horiz);
        CHECK_FALSE_MESSAGE(horiz.intersects(far), horiz, " intersects ", far);
    }
}

TEST_CASE("OrientedSegment and OrientedLine separates and crosses, both directions") {
    using Point = pgl::Point<int>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using OrientedLine = pgl::OrientedLine<Point>;

    const OrientedLine vert({2, -2}, {2, 2});
    const OrientedSegment horiz({4, 0}, {0, 0});

    SUBCASE("proper crossing: both separate each other and both cross") {
        CHECK_MESSAGE(horiz.separates(vert), horiz, " separates ", vert);
        CHECK_MESSAGE(vert.separates(horiz), vert, " separates ", horiz);
        CHECK_MESSAGE(horiz.crosses(vert), horiz, " crosses ", vert);
        CHECK_MESSAGE(vert.crosses(horiz), vert, " crosses ", horiz);
    }

    SUBCASE("endpoint touch: segment separates line, line does not separate segment") {
        const OrientedSegment touch({2, 0}, {5, 3});
        CHECK_MESSAGE(touch.separates(vert), touch, " separates ", vert);
        CHECK_FALSE_MESSAGE(vert.separates(touch), vert, " separates ", touch);
        CHECK_FALSE_MESSAGE(touch.crosses(vert), touch, " crosses ", vert);
        CHECK_FALSE_MESSAGE(vert.crosses(touch), vert, " crosses ", touch);
    }

    SUBCASE("collinear: segment separates oriented line, oriented line does not separate segment") {
        const OrientedLine col({0, 0}, {4, 0});
        CHECK_FALSE_MESSAGE(col.separates(horiz), col, " separates ", horiz);
        CHECK_MESSAGE(horiz.separates(col), horiz, " separates ", col);
        CHECK_FALSE_MESSAGE(horiz.crosses(col), horiz, " crosses ", col);
        CHECK_FALSE_MESSAGE(col.crosses(horiz), col, " crosses ", horiz);
    }
}

TEST_CASE("OrientedSegment and OrientedLine intersection construction, both directions") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using OrientedLine = pgl::OrientedLine<Point>;

    const OrientedLine vert({2, -2}, {2, 2});
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

    SUBCASE("collinear: line intersection with collinear OS returns that segment") {
        const OrientedLine col({0, 0}, {4, 0});
        const auto r = col.intersection(horiz);
        REQUIRE(r);
        REQUIRE(std::holds_alternative<Segment>(*r));
        CHECK_MESSAGE(std::get<Segment>(*r) == Segment({0, 0}, {4, 0}), "col ∩ horiz");
    }

    SUBCASE("disjoint: empty result") {
        const OrientedLine far({10, 0}, {10, 5});
        CHECK_FALSE_MESSAGE(far.intersection(horiz), "far ∩ horiz should be empty");
    }
}
