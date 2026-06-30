#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <variant>

#include "pgl.hpp"


TEST_CASE("OrientedLine containment of Segment, and Segment containment of OrientedLine") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using OrientedLine = pgl::OrientedLine<Point>;

    const OrientedLine diag_ol({0, 0}, {4, 4});    // oriented line along y = x
    const Segment collinear_seg({2, 2}, {8, 8});   // lies entirely on the line y = x
    const Segment cross_seg({0, 2}, {4, 2});       // crosses the line, not collinear

    SUBCASE("OrientedLine contains a collinear Segment") {
        CHECK_MESSAGE(diag_ol.contains(collinear_seg), diag_ol, " contains ", collinear_seg);
        CHECK_MESSAGE(diag_ol.interiorContains(collinear_seg), diag_ol, " interiorContains ", collinear_seg);
        CHECK_FALSE_MESSAGE(diag_ol.contains(cross_seg), diag_ol, " contains ", cross_seg);
        // Lines have no geometric boundary; boundaryContains is always false.
        CHECK_FALSE_MESSAGE(diag_ol.boundaryContains(collinear_seg), diag_ol, " boundaryContains ", collinear_seg);
    }

    SUBCASE("Segment cannot contain a non-degenerate OrientedLine") {
        // Segment::contains(OrientedLine) only returns true when the line is
        // degenerate (source == target) and the degenerate point lies on the segment.
        CHECK_FALSE_MESSAGE(collinear_seg.contains(diag_ol), collinear_seg, " contains ", diag_ol);
        CHECK_FALSE_MESSAGE(collinear_seg.boundaryContains(diag_ol), collinear_seg, " boundaryContains ", diag_ol);
        // Segment::interiorContains(OrientedLine) is always false.
        CHECK_FALSE_MESSAGE(collinear_seg.interiorContains(diag_ol), collinear_seg, " interiorContains ", diag_ol);
    }
}

TEST_CASE("Segment and OrientedLine intersection predicates, both directions") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using OrientedLine = pgl::OrientedLine<Point>;

    const Segment horiz({0, 0}, {4, 0});
    const OrientedLine cross_ol({2, -2}, {2, 2});    // vertical line x=2, crosses horiz at (2,0)
    const OrientedLine touch_ol({4, 0}, {4, 5});     // vertical line x=4, touches horiz at endpoint (4,0)
    const OrientedLine parallel_ol({0, 1}, {4, 1});  // parallel horizontal line y=1, no intersection

    SUBCASE("interior crossing: both intersect, and interiors intersect") {
        CHECK_MESSAGE(horiz.intersects(cross_ol), horiz, " intersects ", cross_ol);
        CHECK_MESSAGE(cross_ol.intersects(horiz), cross_ol, " intersects ", horiz);
        CHECK_MESSAGE(horiz.interiorsIntersect(cross_ol), horiz, " interiorsIntersect ", cross_ol);
        CHECK_MESSAGE(cross_ol.interiorsIntersect(horiz), cross_ol, " interiorsIntersect ", horiz);
    }

    SUBCASE("endpoint touch: intersects but interiors do not") {
        CHECK_MESSAGE(horiz.intersects(touch_ol), horiz, " intersects ", touch_ol);
        CHECK_MESSAGE(touch_ol.intersects(horiz), touch_ol, " intersects ", horiz);
        CHECK_FALSE_MESSAGE(horiz.interiorsIntersect(touch_ol), horiz, " interiorsIntersect ", touch_ol);
        CHECK_FALSE_MESSAGE(touch_ol.interiorsIntersect(horiz), touch_ol, " interiorsIntersect ", horiz);
    }

    SUBCASE("parallel: no intersection") {
        CHECK_FALSE_MESSAGE(horiz.intersects(parallel_ol), horiz, " intersects ", parallel_ol);
        CHECK_FALSE_MESSAGE(parallel_ol.intersects(horiz), parallel_ol, " intersects ", horiz);
        CHECK_FALSE_MESSAGE(horiz.interiorsIntersect(parallel_ol), horiz, " interiorsIntersect ", parallel_ol);
    }
}

TEST_CASE("Segment and OrientedLine separates and crosses, both directions") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using OrientedLine = pgl::OrientedLine<Point>;

    const Segment horiz({0, 0}, {4, 0});
    const OrientedLine cross_ol({2, -2}, {2, 2});    // crosses horiz at interior (2,0)
    const OrientedLine parallel_ol({0, 1}, {4, 1});  // parallel, no intersection

    SUBCASE("interior crossing: both separate and cross each other") {
        CHECK_MESSAGE(horiz.separates(cross_ol), horiz, " separates ", cross_ol);
        CHECK_MESSAGE(cross_ol.separates(horiz), cross_ol, " separates ", horiz);
        CHECK_MESSAGE(horiz.crosses(cross_ol), horiz, " crosses ", cross_ol);
        CHECK_MESSAGE(cross_ol.crosses(horiz), cross_ol, " crosses ", horiz);
    }

    SUBCASE("parallel: neither separates nor crosses") {
        CHECK_FALSE_MESSAGE(horiz.separates(parallel_ol), horiz, " separates ", parallel_ol);
        CHECK_FALSE_MESSAGE(parallel_ol.separates(horiz), parallel_ol, " separates ", horiz);
        CHECK_FALSE_MESSAGE(horiz.crosses(parallel_ol), horiz, " crosses ", parallel_ol);
        CHECK_FALSE_MESSAGE(parallel_ol.crosses(horiz), parallel_ol, " crosses ", horiz);
    }

    SUBCASE("endpoint touch: crosses is false in both directions") {
        const OrientedLine touch_ol({4, 0}, {4, 5});
        CHECK_FALSE_MESSAGE(horiz.crosses(touch_ol), horiz, " crosses ", touch_ol);
        CHECK_FALSE_MESSAGE(touch_ol.crosses(horiz), touch_ol, " crosses ", horiz);
    }
}

TEST_CASE("Segment and OrientedLine intersection construction, both directions") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using OrientedLine = pgl::OrientedLine<Point>;

    const Segment horiz({0, 0}, {4, 0});
    const OrientedLine cross_ol({2, -2}, {2, 2});    // crosses at (2,0)
    const OrientedLine parallel_ol({0, 1}, {4, 1});  // no intersection

    SUBCASE("transverse crossing yields a Point") {
        const auto from_seg = horiz.intersection(cross_ol);
        REQUIRE(from_seg.has_value());
        REQUIRE(std::holds_alternative<Point>(*from_seg));
        CHECK(std::get<Point>(*from_seg) == Point(2, 0));

        const auto from_ol = cross_ol.intersection(horiz);
        REQUIRE(from_ol.has_value());
        REQUIRE(std::holds_alternative<Point>(*from_ol));
        CHECK(std::get<Point>(*from_ol) == Point(2, 0));
    }

    SUBCASE("collinear overlap yields the Segment itself") {
        const OrientedLine col_ol({1, 0}, {3, 0});  // same line as horiz (y=0)

        const auto from_seg = horiz.intersection(col_ol);
        REQUIRE(from_seg.has_value());
        REQUIRE(std::holds_alternative<Segment>(*from_seg));
        CHECK(std::get<Segment>(*from_seg) == horiz);

        const auto from_ol = col_ol.intersection(horiz);
        REQUIRE(from_ol.has_value());
        REQUIRE(std::holds_alternative<Segment>(*from_ol));
        CHECK(std::get<Segment>(*from_ol) == horiz);
    }

    SUBCASE("parallel yields empty") {
        CHECK_FALSE(horiz.intersection(parallel_ol).has_value());
        CHECK_FALSE(parallel_ol.intersection(horiz).has_value());
    }
}
