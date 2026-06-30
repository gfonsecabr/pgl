#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <variant>

#include "pgl.hpp"

TEST_CASE("Rectangle containment of OrientedSegment, and OrientedSegment containment of Rectangle") {
    using Point = pgl::Point<int>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using Segment = pgl::Segment<Point>;
    using Rectangle = pgl::Rectangle<Point>;

    const Rectangle rect({0, 0}, {4, 3});
    const OrientedSegment inner({3, 2}, {1, 1});         // strictly inside
    const OrientedSegment boundary({3, 0}, {1, 0});      // on the boundary edge
    const OrientedSegment crossing({5, 2}, {-1, 2});     // crosses the rectangle

    SUBCASE("Rectangle contains and interiorContains a wholly interior OrientedSegment") {
        CHECK_MESSAGE(rect.contains(inner), rect, " contains ", inner);
        CHECK_MESSAGE(rect.interiorContains(inner), rect, " interiorContains ", inner);
    }

    SUBCASE("Rectangle contains a boundary OrientedSegment but does not interiorContain it") {
        CHECK_MESSAGE(rect.contains(boundary), rect, " contains ", boundary);
        CHECK_FALSE_MESSAGE(rect.interiorContains(boundary), rect, " interiorContains ", boundary);
    }

    SUBCASE("Rectangle does not contain a crossing OrientedSegment") {
        CHECK_FALSE_MESSAGE(rect.contains(crossing), rect, " contains ", crossing);
    }

    SUBCASE("OrientedSegment never contains an area Rectangle") {
        CHECK_FALSE_MESSAGE(inner.contains(rect), inner, " contains ", rect);
        CHECK_FALSE_MESSAGE(inner.interiorContains(rect), inner, " interiorContains ", rect);
    }

    SUBCASE("Rectangle boundaryContains an OrientedSegment on an edge") {
        CHECK_MESSAGE(rect.boundaryContains(boundary), rect, " boundaryContains ", boundary);
        CHECK_FALSE_MESSAGE(rect.boundaryContains(inner), rect, " boundaryContains ", inner);
        CHECK_FALSE_MESSAGE(rect.boundaryContains(crossing), rect, " boundaryContains ", crossing);
    }
}

TEST_CASE("Rectangle and OrientedSegment intersection predicates, both directions") {
    using Point = pgl::Point<int>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using Rectangle = pgl::Rectangle<Point>;

    const Rectangle rect({0, 0}, {4, 3});
    const OrientedSegment crossing({5, 2}, {-1, 2});         // crosses left-right
    const OrientedSegment boundary_only({0, 1}, {-1, 0});    // only endpoint on boundary

    SUBCASE("crossing: both intersect and both interiors intersect") {
        CHECK_MESSAGE(rect.intersects(crossing), rect, " intersects ", crossing);
        CHECK_MESSAGE(crossing.intersects(rect), crossing, " intersects ", rect);
        CHECK_MESSAGE(rect.interiorsIntersect(crossing), rect, " interiorsIntersect ", crossing);
        CHECK_MESSAGE(crossing.interiorsIntersect(rect), crossing, " interiorsIntersect ", rect);
    }

    SUBCASE("endpoint on boundary: intersects but interiors do not") {
        CHECK_MESSAGE(rect.intersects(boundary_only), rect, " intersects ", boundary_only);
        CHECK_FALSE_MESSAGE(rect.interiorsIntersect(boundary_only), rect, " interiorsIntersect ", boundary_only);
        CHECK_FALSE_MESSAGE(boundary_only.interiorsIntersect(rect), boundary_only, " interiorsIntersect ", rect);
    }

    SUBCASE("fully outside: no intersection") {
        const OrientedSegment outside({5, 0}, {8, 0});
        CHECK_FALSE_MESSAGE(rect.intersects(outside), rect, " intersects ", outside);
        CHECK_FALSE_MESSAGE(outside.intersects(rect), outside, " intersects ", rect);
    }
}

TEST_CASE("Rectangle and OrientedSegment separates and crosses, both directions") {
    using Point = pgl::Point<int>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using Rectangle = pgl::Rectangle<Point>;

    const Rectangle rect({0, 0}, {4, 3});
    const OrientedSegment crossing({5, 2}, {-1, 2});

    SUBCASE("crossing OS separates the rectangle and vice versa") {
        CHECK_MESSAGE(rect.separates(crossing), rect, " separates ", crossing);
        CHECK_MESSAGE(crossing.separates(rect), crossing, " separates ", rect);
        CHECK_MESSAGE(rect.crosses(crossing), rect, " crosses ", crossing);
        CHECK_MESSAGE(crossing.crosses(rect), crossing, " crosses ", rect);
    }

    SUBCASE("OS with endpoint only on boundary: no separation") {
        const OrientedSegment touch({4, 1}, {6, 1});
        CHECK_FALSE_MESSAGE(rect.separates(touch), rect, " separates ", touch);
        CHECK_FALSE_MESSAGE(touch.separates(rect), touch, " separates ", rect);
        CHECK_FALSE_MESSAGE(rect.crosses(touch), rect, " crosses ", touch);
        CHECK_FALSE_MESSAGE(touch.crosses(rect), touch, " crosses ", rect);
    }
}

TEST_CASE("Rectangle and OrientedSegment intersection construction") {
    using Point = pgl::Point<int>;
    using Rational = pgl::Rational<int64_t>;
    using RationalPoint = pgl::Point<Rational>;
    using RationalSegment = pgl::Segment<RationalPoint>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using Segment = pgl::Segment<Point>;
    using Rectangle = pgl::Rectangle<Point>;

    const Rectangle rect({0, 0}, {4, 3});

    SUBCASE("crossing OS: clipped to interior chord") {
        const OrientedSegment crossing({5, 2}, {-1, 2});
        const auto r = rect.intersection<Rational>(crossing);
        REQUIRE(r);
        REQUIRE(std::holds_alternative<RationalSegment>(*r));
        CHECK_MESSAGE(std::get<RationalSegment>(*r) ==
                          RationalSegment(RationalPoint(Rational(0), Rational(2)),
                                         RationalPoint(Rational(4), Rational(2))),
                      "rect ∩ crossing");
    }

    SUBCASE("fully inside OS: returns a segment equal to the OS") {
        const OrientedSegment inside({1, 1}, {3, 2});
        const auto r = rect.intersection(inside);
        REQUIRE(r);
        REQUIRE(std::holds_alternative<Segment>(*r));
        CHECK_MESSAGE(std::get<Segment>(*r) == Segment({1, 1}, {3, 2}), "rect ∩ inside");
    }

    SUBCASE("fully outside OS: empty") {
        const OrientedSegment outside({5, 0}, {8, 0});
        CHECK_FALSE_MESSAGE(rect.intersection(outside), "rect ∩ outside should be empty");
    }
}
