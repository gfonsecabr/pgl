#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstdint>
#include <variant>

#include "pgl.hpp"

TEST_CASE("Line and segment predicates exercise the line's infinite extent") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using Line = pgl::Line<Point>;

    const Line diagonal({0, 0}, {4, 4});

    SUBCASE("collinear segment far past the defining points is contained") {
        const Segment far_collinear({100, 100}, {200, 200});
        const OrientedSegment far_oriented({-50, -50}, {-30, -30});

        CHECK(diagonal.contains(far_collinear));
        CHECK(diagonal.contains(far_oriented));
        CHECK(diagonal.interiorContains(far_collinear));
        CHECK(diagonal.collinear(far_collinear));
        CHECK(diagonal.parallel(far_collinear));
        CHECK(diagonal.intersects(far_collinear));
        CHECK(far_collinear.intersects(diagonal));
        CHECK(diagonal.interiorsIntersect(far_collinear));
        CHECK_FALSE(diagonal.crosses(far_collinear));
        CHECK_FALSE(diagonal.separates(far_collinear));

        const auto isec = diagonal.intersection(far_collinear);
        REQUIRE(isec);
        REQUIRE(std::holds_alternative<Segment>(*isec));
        CHECK(std::get<Segment>(*isec) == far_collinear);
    }

    SUBCASE("segment crosses the line beyond the defining points (positive side)") {
        const Segment crossing({100, 110}, {115, 95});

        CHECK_FALSE(diagonal.contains(crossing));
        CHECK_FALSE(diagonal.parallel(crossing));
        CHECK(diagonal.intersects(crossing));
        CHECK(crossing.intersects(diagonal));
        CHECK(diagonal.interiorsIntersect(crossing));
        CHECK(diagonal.crosses(crossing));
        CHECK(crossing.crosses(diagonal));
        CHECK(diagonal.separates(crossing));
        CHECK(crossing.separates(diagonal));

        const auto isec = diagonal.intersection(crossing);
        REQUIRE(isec);
        REQUIRE(std::holds_alternative<Point>(*isec));
    }

    SUBCASE("segment crosses the line beyond the defining points (negative side)") {
        const Segment crossing({-10, -8}, {-8, -10});

        CHECK(diagonal.intersects(crossing));
        CHECK(diagonal.crosses(crossing));
        CHECK(diagonal.separates(crossing));

        const auto isec = diagonal.intersection(crossing);
        REQUIRE(isec);
        REQUIRE(std::holds_alternative<Point>(*isec));
        CHECK(std::get<Point>(*isec) == Point(-9, -9));
    }

    SUBCASE("segment whose support would cross the line but the segment itself stops short") {
        const Segment stops_short({10, 14}, {11, 13});

        CHECK_FALSE(diagonal.contains(stops_short));
        CHECK_FALSE(diagonal.intersects(stops_short));
        CHECK_FALSE(stops_short.intersects(diagonal));
        CHECK_FALSE(diagonal.interiorsIntersect(stops_short));
        CHECK_FALSE(diagonal.crosses(stops_short));
        CHECK_FALSE(diagonal.intersection(stops_short));
    }

    SUBCASE("segment touches the line at one endpoint far past a defining point") {
        const Segment touching({10, 10}, {10, 13});

        CHECK_FALSE(diagonal.contains(touching));
        CHECK(diagonal.intersects(touching));
        CHECK(touching.intersects(diagonal));
        CHECK_FALSE(diagonal.interiorsIntersect(touching));
        CHECK_FALSE(diagonal.crosses(touching));
        CHECK_FALSE(diagonal.separates(touching));

        const auto isec = diagonal.intersection(touching);
        REQUIRE(isec);
        REQUIRE(std::holds_alternative<Point>(*isec));
        CHECK(std::get<Point>(*isec) == Point(10, 10));
    }

    SUBCASE("parallel segment offset from the line is never reached by extending the line") {
        const Segment parallel_seg({-100, -99}, {100, 101});

        CHECK_FALSE(diagonal.contains(parallel_seg));
        CHECK(diagonal.parallel(parallel_seg));
        CHECK_FALSE(diagonal.collinear(parallel_seg));
        CHECK_FALSE(diagonal.intersects(parallel_seg));
        CHECK_FALSE(parallel_seg.intersects(diagonal));
        CHECK_FALSE(diagonal.interiorsIntersect(parallel_seg));
        CHECK_FALSE(diagonal.crosses(parallel_seg));
        CHECK_FALSE(diagonal.intersection(parallel_seg));
    }

    SUBCASE("degenerate segment far along the line is still contained") {
        const Segment far_point({77, 77}, {77, 77});

        CHECK(diagonal.contains(far_point));
        CHECK(diagonal.intersects(far_point));

        const auto isec = diagonal.intersection(far_point);
        REQUIRE(isec);
        REQUIRE(std::holds_alternative<Point>(*isec));
        CHECK(std::get<Point>(*isec) == Point(77, 77));
    }

    SUBCASE("degenerate segment far off the line is disjoint") {
        const Segment far_point_off({77, 78}, {77, 78});

        CHECK_FALSE(diagonal.contains(far_point_off));
        CHECK_FALSE(diagonal.intersects(far_point_off));
        CHECK_FALSE(diagonal.intersection(far_point_off));
    }
}

TEST_CASE("Vertical line crossed by a horizontal segment far above the defining points") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Line = pgl::Line<Point>;

    const Line vertical({2, 0}, {2, 1});
    const Segment horizontal({-100, 500}, {100, 500});

    CHECK(vertical.isVertical());
    CHECK_FALSE(vertical.parallel(horizontal));
    CHECK(vertical.intersects(horizontal));
    CHECK(horizontal.intersects(vertical));
    CHECK(vertical.crosses(horizontal));

    const auto isec = vertical.intersection(horizontal);
    REQUIRE(isec);
    REQUIRE(std::holds_alternative<Point>(*isec));
    CHECK(std::get<Point>(*isec) == Point(2, 500));
}

TEST_CASE("Horizontal line crossed by a vertical segment far past the defining points") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Line = pgl::Line<Point>;

    const Line horizontal({0, 5}, {1, 5});
    const Segment vertical({999, -3}, {999, 12});

    CHECK(horizontal.isHorizontal());
    CHECK(horizontal.intersects(vertical));
    CHECK(horizontal.crosses(vertical));

    const auto isec = horizontal.intersection(vertical);
    REQUIRE(isec);
    REQUIRE(std::holds_alternative<Point>(*isec));
    CHECK(std::get<Point>(*isec) == Point(999, 5));
}

TEST_CASE("Rational crossing far past the defining points") {
    using Rational = pgl::Rational<int64_t>;
    using IntPoint = pgl::Point<int>;
    using RationalPoint = pgl::Point<Rational>;
    using IntSegment = pgl::Segment<IntPoint>;
    using IntLine = pgl::Line<IntPoint>;

    const IntLine line({0, 0}, {2, 1});
    const IntSegment segment({100, 60}, {100, 40});

    CHECK(line.intersects(segment));
    CHECK(line.crosses(segment));

    const auto isec = line.intersection<Rational>(segment);
    REQUIRE(isec);
    REQUIRE(std::holds_alternative<RationalPoint>(*isec));
    CHECK(std::get<RationalPoint>(*isec) == RationalPoint(Rational(100), Rational(50)));
}
