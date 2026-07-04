#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstdint>
#include <optional>
#include <variant>

#include "pgl.hpp"

TEST_CASE("Segments disjoint from rectangles") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Rectangle = pgl::Rectangle<Point>;

    Rectangle r(0, 0, 4, 3);

    auto x = GENERATE(-5, 5);
    auto y = GENERATE(-2, 5);
    Segment s(Point(x, y), Point(x, y + 1));

    CHECK_FALSE_MESSAGE(s.intersects(r), s, " intersects ", r);
    CHECK_FALSE_MESSAGE(r.intersects(s), r, " intersects ", s);
    CHECK_FALSE_MESSAGE(r.interiorsIntersect(s), r, " interiorsIntersect ", s);
    CHECK_FALSE_MESSAGE(s.separates(r), s, " separates ", r);
    CHECK_FALSE_MESSAGE(r.separates(s), r, " separates ", s);
    CHECK_FALSE_MESSAGE(s.crosses(r), s, " crosses ", r);
    CHECK_FALSE_MESSAGE(r.crosses(s), r, " crosses ", s);
    CHECK_FALSE_MESSAGE(r.intersection(s), r, " intersection ", s);
    CHECK_FALSE_MESSAGE(s.intersection(r), s, " intersection ", r);
}

TEST_CASE("Segments touching a rectangle in exactly one point") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Rectangle = pgl::Rectangle<Point>;

    Rectangle r(0, 0, 4, 3);

    SUBCASE("touching a corner") {
        Segment s(-2, -1, 0, 0);
        Point p(0, 0);

        CHECK_MESSAGE(s.intersects(r), s, " intersects ", r);
        CHECK_MESSAGE(r.intersects(s), r, " intersects ", s);
        CHECK_FALSE_MESSAGE(r.interiorsIntersect(s), r, " interiorsIntersect ", s);
        CHECK_FALSE_MESSAGE(s.crosses(r), s, " crosses ", r);
        CHECK_FALSE_MESSAGE(r.crosses(s), r, " crosses ", s);

        auto isec = r.intersection(s);
        auto isec_reverse = s.intersection(r);
        REQUIRE_MESSAGE(isec, r, " intersection ", s);
        REQUIRE_MESSAGE(isec_reverse, s, " intersection ", r);
        REQUIRE(std::holds_alternative<Point>(*isec));
        REQUIRE(std::holds_alternative<Point>(*isec_reverse));
        CHECK_MESSAGE(std::get<Point>(*isec) == p, r, " intersection ", s);
        CHECK_MESSAGE(std::get<Point>(*isec_reverse) == p, s, " intersection ", r);
    }

    SUBCASE("touching a side") {
        Segment s(-2, 1, 0, 1);
        Point p(0, 1);

        CHECK_MESSAGE(s.intersects(r), s, " intersects ", r);
        CHECK_MESSAGE(r.intersects(s), r, " intersects ", s);
        CHECK_FALSE_MESSAGE(r.interiorsIntersect(s), r, " interiorsIntersect ", s);
        CHECK_FALSE_MESSAGE(s.separates(r), s, " separates ", r);
        CHECK_FALSE_MESSAGE(r.separates(s), r, " separates ", s);

        auto isec = r.intersection(s);
        auto isec_reverse = s.intersection(r);
        REQUIRE_MESSAGE(isec, r, " intersection ", s);
        REQUIRE_MESSAGE(isec_reverse, s, " intersection ", r);
        REQUIRE(std::holds_alternative<Point>(*isec));
        REQUIRE(std::holds_alternative<Point>(*isec_reverse));
        CHECK_MESSAGE(std::get<Point>(*isec) == p, r, " intersection ", s);
        CHECK_MESSAGE(std::get<Point>(*isec_reverse) == p, s, " intersection ", r);
    }
}

TEST_CASE("Segments contained in rectangles") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Rectangle = pgl::Rectangle<Point>;

    Rectangle r(0, 0, 4, 3);

    SUBCASE("strictly inside") {
        Segment s(1, 1, 3, 2);

        CHECK_MESSAGE(r.contains(s), r, " contains ", s);
        CHECK_MESSAGE(r.interiorContains(s), r, " interiorContains ", s);
        CHECK_FALSE_MESSAGE(r.boundaryContains(s), r, " boundaryContains ", s);
        CHECK_MESSAGE(s.intersects(r), s, " intersects ", r);
        CHECK_MESSAGE(r.intersects(s), r, " intersects ", s);
        CHECK_MESSAGE(r.interiorsIntersect(s), r, " interiorsIntersect ", s);
        CHECK_FALSE_MESSAGE(s.separates(r), s, " separates ", r);
        CHECK_FALSE_MESSAGE(r.separates(s), r, " separates ", s);
        CHECK_FALSE_MESSAGE(s.crosses(r), s, " crosses ", r);

        auto isec = r.intersection(s);
        REQUIRE_MESSAGE(isec, r, " intersection ", s);
        REQUIRE(std::holds_alternative<Segment>(*isec));
        CHECK_MESSAGE(std::get<Segment>(*isec) == s, r, " intersection ", s);
    }

    SUBCASE("from interior to boundary") {
        Segment s(1, 1, 4, 1);

        CHECK_MESSAGE(r.contains(s), r, " contains ", s);
        CHECK_FALSE_MESSAGE(r.interiorContains(s), r, " interiorContains ", s);
        CHECK_FALSE_MESSAGE(r.boundaryContains(s), r, " boundaryContains ", s);
        CHECK_MESSAGE(r.interiorsIntersect(s), r, " interiorsIntersect ", s);
        CHECK_FALSE_MESSAGE(s.separates(r), s, " separates ", r);
        CHECK_FALSE_MESSAGE(r.separates(s), r, " separates ", s);

        auto isec = s.intersection(r);
        REQUIRE_MESSAGE(isec, s, " intersection ", r);
        REQUIRE(std::holds_alternative<Segment>(*isec));
        CHECK_MESSAGE(std::get<Segment>(*isec) == s, s, " intersection ", r);
    }

    SUBCASE("on one boundary edge") {
        Segment s(0, 1, 0, 2);

        CHECK_MESSAGE(r.contains(s), r, " contains ", s);
        CHECK_MESSAGE(r.boundaryContains(s), r, " boundaryContains ", s);
        CHECK_FALSE_MESSAGE(r.interiorContains(s), r, " interiorContains ", s);
        CHECK_MESSAGE(r.intersects(s), r, " intersects ", s);
        CHECK_FALSE_MESSAGE(r.interiorsIntersect(s), r, " interiorsIntersect ", s);
        CHECK_FALSE_MESSAGE(s.crosses(r), s, " crosses ", r);

        auto isec = r.intersection(s);
        REQUIRE_MESSAGE(isec, r, " intersection ", s);
        REQUIRE(std::holds_alternative<Segment>(*isec));
        CHECK_MESSAGE(std::get<Segment>(*isec) == s, r, " intersection ", s);
    }
}

TEST_CASE("Segments crossing rectangles") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Rectangle = pgl::Rectangle<Point>;

    Rectangle r(0, 0, 4, 3);

    SUBCASE("cutting horizontally") {
        Segment s(-2, 1, 6, 1);
        Segment clipped(0, 1, 4, 1);

        CHECK_FALSE_MESSAGE(r.contains(s), r, " contains ", s);
        CHECK_MESSAGE(s.intersects(r), s, " intersects ", r);
        CHECK_MESSAGE(r.intersects(s), r, " intersects ", s);
        CHECK_MESSAGE(r.interiorsIntersect(s), r, " interiorsIntersect ", s);
        CHECK_MESSAGE(s.separates(r), s, " separates ", r);
        CHECK_MESSAGE(r.separates(s), r, " separates ", s);
        CHECK_MESSAGE(s.crosses(r), s, " crosses ", r);
        CHECK_MESSAGE(r.crosses(s), r, " crosses ", s);

        auto isec = r.intersection(s);
        auto isec_reverse = s.intersection(r);
        REQUIRE_MESSAGE(isec, r, " intersection ", s);
        REQUIRE_MESSAGE(isec_reverse, s, " intersection ", r);
        REQUIRE(std::holds_alternative<Segment>(*isec));
        REQUIRE(std::holds_alternative<Segment>(*isec_reverse));
        CHECK_MESSAGE(std::get<Segment>(*isec) == clipped, r, " intersection ", s);
        CHECK_MESSAGE(std::get<Segment>(*isec_reverse) == clipped, s, " intersection ", r);
    }

    SUBCASE("cutting vertically") {
        Segment s(2, -1, 2, 5);
        Segment clipped(2, 0, 2, 3);

        CHECK_MESSAGE(r.interiorsIntersect(s), r, " interiorsIntersect ", s);
        CHECK_MESSAGE(s.separates(r), s, " separates ", r);
        CHECK_MESSAGE(r.separates(s), r, " separates ", s);
        CHECK_MESSAGE(s.crosses(r), s, " crosses ", r);
        CHECK_MESSAGE(r.crosses(s), r, " crosses ", s);

        auto isec = r.intersection(s);
        REQUIRE_MESSAGE(isec, r, " intersection ", s);
        REQUIRE(std::holds_alternative<Segment>(*isec));
        CHECK_MESSAGE(std::get<Segment>(*isec) == clipped, r, " intersection ", s);
    }

    SUBCASE("cutting through opposite corners") {
        Segment s(-4, -3, 8, 6);
        Segment clipped(0, 0, 4, 3);

        CHECK_MESSAGE(r.interiorsIntersect(s), r, " interiorsIntersect ", s);
        CHECK_MESSAGE(s.separates(r), s, " separates ", r);
        CHECK_MESSAGE(r.separates(s), r, " separates ", s);
        CHECK_MESSAGE(s.crosses(r), s, " crosses ", r);
        CHECK_MESSAGE(r.crosses(s), r, " crosses ", s);

        auto isec = s.intersection(r);
        REQUIRE_MESSAGE(isec, s, " intersection ", r);
        REQUIRE(std::holds_alternative<Segment>(*isec));
        CHECK_MESSAGE(std::get<Segment>(*isec) == clipped, s, " intersection ", r);
    }

    SUBCASE("starting inside and leaving through one side") {
        Segment s(2, 1, 6, 1);
        Segment clipped(2, 1, 4, 1);

        CHECK_MESSAGE(r.interiorsIntersect(s), r, " interiorsIntersect ", s);
        CHECK_FALSE_MESSAGE(s.separates(r), s, " separates ", r);
        CHECK_FALSE_MESSAGE(r.separates(s), r, " separates ", s);
        CHECK_FALSE_MESSAGE(s.crosses(r), s, " crosses ", r);

        auto isec = r.intersection(s);
        REQUIRE_MESSAGE(isec, r, " intersection ", s);
        REQUIRE(std::holds_alternative<Segment>(*isec));
        CHECK_MESSAGE(std::get<Segment>(*isec) == clipped, r, " intersection ", s);
    }
}

TEST_CASE("Generated horizontal and vertical cuts against a rectangle") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Rectangle = pgl::Rectangle<Point>;

    Rectangle r(0, 0, 4, 3);

    SUBCASE("horizontal cuts") {
        auto y = GENERATE(-1, 0, 1, 2, 3, 4);
        Segment s(-2, y, 6, y);
        bool intersects = 0 <= y && y <= 3;
        bool cuts_interior = 0 < y && y < 3;

        CHECK_MESSAGE(s.intersects(r) == intersects, s, " intersects ", r);
        CHECK_MESSAGE(r.intersects(s) == intersects, r, " intersects ", s);
        CHECK_MESSAGE(r.interiorsIntersect(s) == cuts_interior, r, " interiorsIntersect ", s);
        CHECK_MESSAGE(s.separates(r) == cuts_interior, s, " separates ", r);
        CHECK_MESSAGE(s.crosses(r) == cuts_interior, s, " crosses ", r);
        CHECK_MESSAGE(r.crosses(s) == cuts_interior, r, " crosses ", s);

        auto isec = r.intersection(s);
        auto isec_reverse = s.intersection(r);
        if (intersects) {
            Segment clipped(0, y, 4, y);
            REQUIRE_MESSAGE(isec, r, " intersection ", s);
            REQUIRE_MESSAGE(isec_reverse, s, " intersection ", r);
            REQUIRE(std::holds_alternative<Segment>(*isec));
            REQUIRE(std::holds_alternative<Segment>(*isec_reverse));
            CHECK_MESSAGE(std::get<Segment>(*isec) == clipped, r, " intersection ", s);
            CHECK_MESSAGE(std::get<Segment>(*isec_reverse) == clipped, s, " intersection ", r);
        } else {
            CHECK_FALSE_MESSAGE(isec, r, " intersection ", s);
            CHECK_FALSE_MESSAGE(isec_reverse, s, " intersection ", r);
        }
    }

    SUBCASE("vertical cuts") {
        auto x = GENERATE(-1, 0, 1, 2, 4, 5);
        Segment s(x, -2, x, 5);
        bool intersects = 0 <= x && x <= 4;
        bool cuts_interior = 0 < x && x < 4;

        CHECK_MESSAGE(s.intersects(r) == intersects, s, " intersects ", r);
        CHECK_MESSAGE(r.intersects(s) == intersects, r, " intersects ", s);
        CHECK_MESSAGE(r.interiorsIntersect(s) == cuts_interior, r, " interiorsIntersect ", s);
        CHECK_MESSAGE(s.separates(r) == cuts_interior, s, " separates ", r);
        CHECK_MESSAGE(s.crosses(r) == cuts_interior, s, " crosses ", r);
        CHECK_MESSAGE(r.crosses(s) == cuts_interior, r, " crosses ", s);

        auto isec = r.intersection(s);
        auto isec_reverse = s.intersection(r);
        if (intersects) {
            Segment clipped(x, 0, x, 3);
            REQUIRE_MESSAGE(isec, r, " intersection ", s);
            REQUIRE_MESSAGE(isec_reverse, s, " intersection ", r);
            REQUIRE(std::holds_alternative<Segment>(*isec));
            REQUIRE(std::holds_alternative<Segment>(*isec_reverse));
            CHECK_MESSAGE(std::get<Segment>(*isec) == clipped, r, " intersection ", s);
            CHECK_MESSAGE(std::get<Segment>(*isec_reverse) == clipped, s, " intersection ", r);
        } else {
            CHECK_FALSE_MESSAGE(isec, r, " intersection ", s);
            CHECK_FALSE_MESSAGE(isec_reverse, s, " intersection ", r);
        }
    }
}

TEST_CASE("Generated segment rectangle consistency checks") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Rectangle = pgl::Rectangle<Point>;
    using Rational = pgl::Rational<int64_t>;
    using RationalPoint = pgl::Point<Rational>;
    using RationalSegment = pgl::Segment<RationalPoint>;

    Rectangle r(0, 0, 4, 3);

    auto x1 = GENERATE(-1, 0, 1, 2, 4, 5);
    auto y1 = GENERATE(-1, 0, 1, 3, 4);
    auto x2 = GENERATE(-1, 0, 2, 3, 4, 5);
    auto y2 = GENERATE(-1, 0, 2, 3, 4);
    Segment s(x1, y1, x2, y2);

    CHECK_MESSAGE(r.contains(s) == (r.contains(s.min()) && r.contains(s.max())), r, " contains ", s);
    CHECK_MESSAGE(s.intersects(r) == r.intersects(s), s, " intersects ", r);
    CHECK_MESSAGE(s.crosses(r) == r.crosses(s), s, " crosses ", r);
    CHECK_MESSAGE(s.crosses(r) == (s.separates(r) && r.separates(s)), s, " crosses ", r);

    auto isec = r.intersection<Rational>(s);
    auto isec_reverse = s.intersection<Rational>(r);
    CHECK_MESSAGE(static_cast<bool>(isec) == r.intersects(s), r, " intersection ", s);
    CHECK_MESSAGE(static_cast<bool>(isec_reverse) == s.intersects(r), s, " intersection ", r);
    CHECK_MESSAGE(static_cast<bool>(isec) == static_cast<bool>(isec_reverse), r, " intersection symmetry ", s);

    if (isec && std::holds_alternative<RationalPoint>(*isec)) {
        RationalPoint p = std::get<RationalPoint>(*isec);
        CHECK_MESSAGE(r.contains(p), r, " contains intersection point ", p);
        CHECK_MESSAGE(s.contains(p), s, " contains intersection point ", p);
    }
    if (isec && std::holds_alternative<RationalSegment>(*isec)) {
        RationalSegment clipped = std::get<RationalSegment>(*isec);
        CHECK_MESSAGE(r.contains(clipped), r, " contains intersection segment ", clipped);
        CHECK_MESSAGE(s.contains(clipped), s, " contains intersection segment ", clipped);
    }
    if (isec && isec_reverse) {
        CHECK_MESSAGE(isec->index() == isec_reverse->index(), r, " intersection type symmetry ", s);
    }
}

TEST_CASE("Degenerate segments and rectangles") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Rectangle = pgl::Rectangle<Point>;

    SUBCASE("point segment inside a rectangle") {
        Rectangle r(0, 0, 4, 3);
        Segment s(2, 1, 2, 1);
        Point p(2, 1);

        CHECK_MESSAGE(r.contains(s), r, " contains ", s);
        CHECK_MESSAGE(r.interiorContains(s), r, " interiorContains ", s);
        CHECK_MESSAGE(r.intersects(s), r, " intersects ", s);
        CHECK_FALSE_MESSAGE(r.interiorsIntersect(s), r, " interiorsIntersect ", s);

        auto isec = r.intersection(s);
        REQUIRE_MESSAGE(isec, r, " intersection ", s);
        REQUIRE(std::holds_alternative<Point>(*isec));
        CHECK_MESSAGE(std::get<Point>(*isec) == p, r, " intersection ", s);
    }

    SUBCASE("segment contained in a flat rectangle") {
        Rectangle r(0, 0, 4, 0);
        Segment s(1, 0, 3, 0);

        CHECK_MESSAGE(r.contains(s), r, " contains ", s);
        CHECK_MESSAGE(r.boundaryContains(s), r, " boundaryContains ", s);
        CHECK_FALSE_MESSAGE(r.interiorsIntersect(s), r, " interiorsIntersect ", s);

        auto isec = r.intersection(s);
        REQUIRE_MESSAGE(isec, r, " intersection ", s);
        REQUIRE(std::holds_alternative<Segment>(*isec));
        CHECK_MESSAGE(std::get<Segment>(*isec) == s, r, " intersection ", s);
    }

    SUBCASE("segment crosses a flat rectangle at one point") {
        Rectangle r(0, 0, 4, 0);
        Segment s(2, -2, 2, 2);
        Point p(2, 0);

        CHECK_MESSAGE(r.intersects(s), r, " intersects ", s);
        CHECK_FALSE_MESSAGE(r.interiorsIntersect(s), r, " interiorsIntersect ", s);
        CHECK_FALSE_MESSAGE(s.crosses(r), s, " crosses ", r);

        auto isec = s.intersection(r);
        REQUIRE_MESSAGE(isec, s, " intersection ", r);
        REQUIRE(std::holds_alternative<Point>(*isec));
        CHECK_MESSAGE(std::get<Point>(*isec) == p, s, " intersection ", r);
    }
}

TEST_CASE("Rational clipping of a segment by a rectangle") {
    using Rational = pgl::Rational<int64_t>;
    using Point = pgl::Point<Rational>;
    using Segment = pgl::Segment<Point>;
    using Rectangle = pgl::Rectangle<Point>;

    Rectangle r(Point(Rational(0), Rational(0)), Point(Rational(4), Rational(3)));
    Segment s(Point(Rational(-1), Rational(1)), Point(Rational(5), Rational(4)));
    Segment clipped(Point(Rational(0), Rational(3, 2)), Point(Rational(3), Rational(3)));

    CHECK_MESSAGE(r.intersects(s), r, " intersects ", s);
    CHECK_MESSAGE(r.interiorsIntersect(s), r, " interiorsIntersect ", s);
    CHECK_MESSAGE(s.crosses(r), s, " crosses ", r);
    CHECK_MESSAGE(r.crosses(s), r, " crosses ", s);

    auto isec = r.intersection<Rational>(s);
    auto isec_reverse = s.intersection<Rational>(r);
    REQUIRE_MESSAGE(isec, r, " intersection ", s);
    REQUIRE_MESSAGE(isec_reverse, s, " intersection ", r);
    REQUIRE(std::holds_alternative<Segment>(*isec));
    REQUIRE(std::holds_alternative<Segment>(*isec_reverse));
    CHECK_MESSAGE(std::get<Segment>(*isec) == clipped, r, " intersection ", s);
    CHECK_MESSAGE(std::get<Segment>(*isec_reverse) == clipped, s, " intersection ", r);
}

TEST_CASE("Segment and Rectangle squared Hausdorff distance") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Rectangle = pgl::Rectangle<Point>;

    const Rectangle r({0, 0}, {4, 4});
    const Segment s({6, 1}, {6, 3});

    // Farthest rectangle vertices from s are (0,0) and (0,4), each at squared
    // distance 37, which dominates the segment-side term (squared distance 4).
    CHECK(r.squaredHausdorffDistance(s) == 37);
    CHECK(s.squaredHausdorffDistance(r) == 37);
}
