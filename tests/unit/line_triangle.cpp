#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstdint>
#include <variant>

#include "pgl.hpp"

// interiorContains can only ever be degenerate here: a 1D line cannot interior-
// contain a 2D triangle, and a bounded triangle cannot interior-contain an
// unbounded line. For real shapes both directions are false.
TEST_CASE("Line and Triangle interior-containment is always false") {
    using Point = pgl::Point<int>;
    using Triangle = pgl::Triangle<Point>;
    using Line = pgl::Line<Point>;

    const Triangle triangle({0, 0}, {6, 0}, {0, 6});

    SUBCASE("a line cutting through the interior") {
        const Line cut({-200, 2}, {-100, 2});
        CHECK_FALSE(cut.interiorContains(triangle));
        CHECK_FALSE(triangle.interiorContains(cut));
    }

    SUBCASE("a line running along an edge") {
        const Line alongEdge({-100, 0}, {-1, 0});  // y = 0 along the base
        CHECK_FALSE(alongEdge.interiorContains(triangle));
        CHECK_FALSE(triangle.interiorContains(alongEdge));
    }
}

TEST_CASE("Line and triangle predicates exercise the line's infinite extent") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Triangle = pgl::Triangle<Point>;
    using Line = pgl::Line<Point>;

    const Triangle triangle({0, 0}, {6, 0}, {0, 6});

    SUBCASE("line cuts through the triangle, defining points outside the triangle") {
        const Line cut({-100, 2}, {-50, 2});

        CHECK_FALSE(cut.contains(triangle));
        CHECK_FALSE(triangle.contains(cut));
        CHECK_FALSE(triangle.boundaryContains(cut));
        CHECK(cut.intersects(triangle));
        CHECK(triangle.intersects(cut));
        CHECK(cut.interiorsIntersect(triangle));
        CHECK(triangle.interiorsIntersect(cut));
        CHECK(cut.crosses(triangle));
        CHECK(triangle.crosses(cut));
        CHECK(cut.separates(triangle));
        CHECK(triangle.separates(cut));

        const auto clipped = cut.intersection(triangle);
        REQUIRE(clipped);
        REQUIRE(std::holds_alternative<Segment>(*clipped));
        CHECK(std::get<Segment>(*clipped) == Segment(Point(0, 2), Point(4, 2)));
    }

    SUBCASE("line through two triangle vertices, defining points are not those vertices") {
        const Line through_vertices({-3, 9}, {10, -4});

        CHECK_FALSE(triangle.contains(through_vertices));
        CHECK(through_vertices.intersects(triangle));
        CHECK(triangle.intersects(through_vertices));
        CHECK_FALSE(through_vertices.interiorsIntersect(triangle));
        CHECK_FALSE(triangle.interiorsIntersect(through_vertices));
        CHECK_FALSE(through_vertices.crosses(triangle));
        CHECK_FALSE(through_vertices.separates(triangle));

        const auto clipped = through_vertices.intersection(triangle);
        REQUIRE(clipped);
        REQUIRE(std::holds_alternative<Segment>(*clipped));
        CHECK(std::get<Segment>(*clipped) == Segment(Point(0, 6), Point(6, 0)));
    }

    SUBCASE("line lies along a triangle edge, defining points outside the edge") {
        const Line along_edge({-10, 0}, {-1, 0});

        CHECK_FALSE(triangle.contains(along_edge));
        CHECK(along_edge.intersects(triangle));
        CHECK(triangle.intersects(along_edge));
        CHECK_FALSE(along_edge.interiorsIntersect(triangle));
        CHECK_FALSE(triangle.interiorsIntersect(along_edge));
        CHECK_FALSE(along_edge.crosses(triangle));
        CHECK_FALSE(along_edge.separates(triangle));

        const auto overlap = along_edge.intersection(triangle);
        REQUIRE(overlap);
        REQUIRE(std::holds_alternative<Segment>(*overlap));
        CHECK(std::get<Segment>(*overlap) == Segment(Point(0, 0), Point(6, 0)));
    }

    SUBCASE("line touches the triangle at a single vertex (tangent)") {
        const Line tangent({-100, 6}, {100, 6});

        CHECK_FALSE(triangle.contains(tangent));
        CHECK(tangent.intersects(triangle));
        CHECK(triangle.intersects(tangent));
        CHECK_FALSE(tangent.interiorsIntersect(triangle));
        CHECK_FALSE(triangle.interiorsIntersect(tangent));
        CHECK_FALSE(tangent.crosses(triangle));

        const auto isec = tangent.intersection(triangle);
        REQUIRE(isec);
        REQUIRE(std::holds_alternative<Point>(*isec));
        CHECK(std::get<Point>(*isec) == Point(0, 6));
    }

    SUBCASE("line disjoint from the triangle, far past one side") {
        const Line farShape({-10, -3}, {10, -3});

        CHECK_FALSE(farShape.intersects(triangle));
        CHECK_FALSE(triangle.intersects(farShape));
        CHECK_FALSE(farShape.interiorsIntersect(triangle));
        CHECK_FALSE(farShape.crosses(triangle));
        CHECK_FALSE(farShape.intersection(triangle));
    }

    SUBCASE("line cuts from one edge interior to the opposite vertex region") {
        const Line cut({-100, 206}, {100, -194});

        CHECK(cut.intersects(triangle));
        CHECK(cut.interiorsIntersect(triangle));
        CHECK(cut.crosses(triangle));
        CHECK(cut.separates(triangle));

        const auto clipped = cut.intersection(triangle);
        REQUIRE(clipped);
        REQUIRE(std::holds_alternative<Segment>(*clipped));
    }
}

TEST_CASE("Vertical line cutting a triangle far from the line's defining points") {
    using Point = pgl::Point<int>;
    using Triangle = pgl::Triangle<Point>;
    using Segment = pgl::Segment<Point>;
    using Line = pgl::Line<Point>;

    const Triangle triangle({0, 0}, {6, 0}, {0, 6});
    const Line vertical({2, 100}, {2, 200});

    CHECK(vertical.isVertical());
    CHECK(vertical.intersects(triangle));
    CHECK(vertical.crosses(triangle));

    const auto clipped = vertical.intersection(triangle);
    REQUIRE(clipped);
    REQUIRE(std::holds_alternative<Segment>(*clipped));
    CHECK(std::get<Segment>(*clipped) == Segment(Point(2, 0), Point(2, 4)));
}

TEST_CASE("Rational clipping of a triangle by a line whose defining points sit far away") {
    using Rational = pgl::Rational<int64_t>;
    using IntPoint = pgl::Point<int>;
    using RationalPoint = pgl::Point<Rational>;
    using IntTriangle = pgl::Triangle<IntPoint>;
    using IntLine = pgl::Line<IntPoint>;
    using RationalSegment = pgl::Segment<RationalPoint>;

    const IntTriangle triangle({0, 0}, {6, 0}, {0, 6});
    const IntLine slanted({-100, -99}, {100, 101});

    CHECK(slanted.intersects(triangle));
    CHECK(slanted.crosses(triangle));

    const auto clipped = slanted.intersection<Rational>(triangle);
    REQUIRE(clipped);
    REQUIRE(std::holds_alternative<RationalSegment>(*clipped));
}
