#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstdint>
#include <variant>

#include "pgl.hpp"

TEST_CASE("Line and triangle-as-convex predicates exercise the line's infinite extent") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Convex = pgl::Convex<Point>;
    using Line = pgl::Line<Point>;

    const Convex triangle({{0, 0}, {6, 0}, {0, 6}});

    SUBCASE("line cuts through the convex, defining points far from it") {
        const Line cut({-200, 2}, {-100, 2});

        CHECK_FALSE(triangle.contains(cut));
        CHECK(cut.intersects(triangle));
        CHECK(cut.interiorsIntersect(triangle));
        CHECK(cut.crosses(triangle));
        CHECK(cut.separates(triangle));

        const auto clipped = cut.intersection(triangle);
        REQUIRE(clipped);
        REQUIRE(std::holds_alternative<Segment>(*clipped));
        CHECK(std::get<Segment>(*clipped) == Segment(Point(0, 2), Point(4, 2)));
    }

    SUBCASE("line along one convex edge, defining points outside the edge") {
        const Line along_edge({-100, 0}, {-1, 0});

        CHECK_FALSE(triangle.contains(along_edge));
        CHECK(along_edge.intersects(triangle));
        CHECK_FALSE(along_edge.interiorsIntersect(triangle));
        CHECK_FALSE(along_edge.crosses(triangle));
        CHECK_FALSE(along_edge.separates(triangle));

        const auto overlap = along_edge.intersection(triangle);
        REQUIRE(overlap);
        REQUIRE(std::holds_alternative<Segment>(*overlap));
        CHECK(std::get<Segment>(*overlap) == Segment(Point(0, 0), Point(6, 0)));
    }

    SUBCASE("line through two convex vertices, defining points are not those vertices") {
        const Line through_vertices({-3, 9}, {10, -4});

        CHECK(through_vertices.intersects(triangle));
        CHECK_FALSE(through_vertices.interiorsIntersect(triangle));
        CHECK_FALSE(through_vertices.crosses(triangle));

        const auto clipped = through_vertices.intersection(triangle);
        REQUIRE(clipped);
        REQUIRE(std::holds_alternative<Segment>(*clipped));
        CHECK(std::get<Segment>(*clipped) == Segment(Point(0, 6), Point(6, 0)));
    }

    SUBCASE("line tangent to a single convex vertex") {
        const Line tangent({-100, 6}, {100, 6});

        CHECK(tangent.intersects(triangle));
        CHECK_FALSE(tangent.interiorsIntersect(triangle));
        CHECK_FALSE(tangent.crosses(triangle));

        const auto isec = tangent.intersection(triangle);
        REQUIRE(isec);
        REQUIRE(std::holds_alternative<Point>(*isec));
        CHECK(std::get<Point>(*isec) == Point(0, 6));
    }

    SUBCASE("line disjoint from the convex, defining points far away") {
        const Line far({-100, -50}, {100, -50});

        CHECK_FALSE(far.intersects(triangle));
        CHECK_FALSE(far.interiorsIntersect(triangle));
        CHECK_FALSE(far.crosses(triangle));
        CHECK_FALSE(far.intersection(triangle));
    }
}

TEST_CASE("Line and quadrilateral convex predicates exercise the line's infinite extent") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Convex = pgl::Convex<Point>;
    using Line = pgl::Line<Point>;

    const Convex square({{0, 0}, {4, 0}, {4, 4}, {0, 4}});

    SUBCASE("horizontal line cuts the square, defining points far left") {
        const Line cut({-500, 2}, {-400, 2});

        CHECK(cut.intersects(square));
        CHECK(cut.interiorsIntersect(square));
        CHECK(cut.crosses(square));

        const auto clipped = cut.intersection(square);
        REQUIRE(clipped);
        REQUIRE(std::holds_alternative<Segment>(*clipped));
        CHECK(std::get<Segment>(*clipped) == Segment(Point(0, 2), Point(4, 2)));
    }

    SUBCASE("line through two opposite corners with defining points outside") {
        const Line diagonal({-50, -50}, {50, 50});

        CHECK(diagonal.intersects(square));
        CHECK(diagonal.interiorsIntersect(square));
        CHECK(diagonal.crosses(square));

        const auto clipped = diagonal.intersection(square);
        REQUIRE(clipped);
        REQUIRE(std::holds_alternative<Segment>(*clipped));
        CHECK(std::get<Segment>(*clipped) == Segment(Point(0, 0), Point(4, 4)));
    }

    SUBCASE("line tangent at a corner from outside") {
        const Line tangent({-100, -104}, {100, 96});

        CHECK(tangent.intersects(square));
        CHECK_FALSE(tangent.interiorsIntersect(square));
        CHECK_FALSE(tangent.crosses(square));

        const auto isec = tangent.intersection(square);
        REQUIRE(isec);
        REQUIRE(std::holds_alternative<Point>(*isec));
        CHECK(std::get<Point>(*isec) == Point(4, 0));
    }

    SUBCASE("vertical line beyond the right side") {
        const Line beyond({1000, 0}, {1000, 1});

        CHECK_FALSE(beyond.intersects(square));
        CHECK_FALSE(beyond.crosses(square));
        CHECK_FALSE(beyond.intersection(square));
    }
}

TEST_CASE("Rational clipping of a convex polygon by a line with far defining points") {
    using Rational = pgl::Rational<int64_t>;
    using IntPoint = pgl::Point<int>;
    using RationalPoint = pgl::Point<Rational>;
    using IntConvex = pgl::Convex<IntPoint>;
    using IntLine = pgl::Line<IntPoint>;
    using RationalSegment = pgl::Segment<RationalPoint>;

    const IntConvex square({{0, 0}, {4, 0}, {4, 4}, {0, 4}});
    const IntLine slanted({-100, -99}, {100, 101});

    CHECK(slanted.intersects(square));
    CHECK(slanted.crosses(square));

    const auto clipped = slanted.intersection<Rational>(square);
    REQUIRE(clipped);
    REQUIRE(std::holds_alternative<RationalSegment>(*clipped));
}

TEST_CASE("Line and convex predicates are invariant under convex translation") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Convex = pgl::Convex<Point>;
    using Line = pgl::Line<Point>;

    // Convex::intersects(Line) used to read the raw vertices while ignoring the
    // polygon's translation_ offset, so the same query flipped its answer once
    // the polygon carried a non-zero translation. Re-run an identical battery in
    // several frames; a rigid translation must not change any result.
    for (const Point shift : {Point(0, 0), Point(36, 24), Point(-50, -17)}) {
        Convex pentagon({{0, 0}, {8, 8}, {16, 8}, {24, 0}, {12, -8}});
        pentagon += shift;

        const auto line = [&shift](Point a, Point b) { return Line(a + shift, b + shift); };
        const auto pt = [&shift](Point p) { return p + shift; };

        // Infinite line y = 2 cuts straight through the interior.
        const Line cut = line({-10, 2}, {-5, 2});
        CHECK(cut.intersects(pentagon));
        CHECK(pentagon.intersects(cut));
        CHECK(cut.interiorsIntersect(pentagon));
        CHECK(cut.crosses(pentagon));
        {
            const auto clipped = cut.intersection(pentagon);
            REQUIRE(clipped);
            REQUIRE(std::holds_alternative<Segment>(*clipped));
            CHECK(std::get<Segment>(*clipped) == Segment(pt({2, 2}), pt({22, 2})));
        }

        // Infinite line y = 8 lies along the top edge (8,8)-(16,8).
        const Line along_edge = line({-10, 8}, {-5, 8});
        CHECK(along_edge.intersects(pentagon));
        CHECK(pentagon.intersects(along_edge));
        CHECK_FALSE(along_edge.interiorsIntersect(pentagon));
        CHECK_FALSE(along_edge.crosses(pentagon));
        {
            const auto overlap = along_edge.intersection(pentagon);
            REQUIRE(overlap);
            REQUIRE(std::holds_alternative<Segment>(*overlap));
            CHECK(std::get<Segment>(*overlap) == Segment(pt({8, 8}), pt({16, 8})));
        }

        // Infinite line y = -8 is tangent at the single bottom vertex (12,-8).
        const Line tangent = line({-10, -8}, {-5, -8});
        CHECK(tangent.intersects(pentagon));
        CHECK(pentagon.intersects(tangent));
        CHECK_FALSE(tangent.interiorsIntersect(pentagon));
        CHECK_FALSE(tangent.crosses(pentagon));
        {
            const auto isec = tangent.intersection(pentagon);
            REQUIRE(isec);
            REQUIRE(std::holds_alternative<Point>(*isec));
            CHECK(std::get<Point>(*isec) == pt({12, -8}));
        }

        // Infinite line y = 100 stays clear of the polygon.
        const Line away = line({-10, 100}, {-5, 100});
        CHECK_FALSE(away.intersects(pentagon));
        CHECK_FALSE(pentagon.intersects(away));
        CHECK_FALSE(away.interiorsIntersect(pentagon));
        CHECK_FALSE(away.crosses(pentagon));
        CHECK_FALSE(away.intersection(pentagon));
    }
}
