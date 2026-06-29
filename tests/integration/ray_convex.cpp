#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <variant>

#include "pgl.hpp"

// Containment that can only ever be degenerate: a 1D ray cannot interior-contain
// a 2D convex, and a bounded convex can neither bound nor interior-contain an
// unbounded ray. For real shapes every such relation is false.
TEST_CASE("Ray and Convex boundary/interior containment is always false") {
    using Point = pgl::Point<int>;
    using Convex = pgl::Convex<Point>;
    using Ray = pgl::Ray<Point>;

    const Convex triangle({{0, 0}, {6, 0}, {0, 6}});

    SUBCASE("a ray cutting through is interior-contained by neither") {
        const Ray cut({-2, 2}, {-1, 2});
        CHECK_FALSE(cut.interiorContains(triangle));
        CHECK_FALSE(triangle.boundaryContains(cut));
        CHECK_FALSE(triangle.interiorContains(cut));
    }

    SUBCASE("a ray along an edge still bounds/contains nothing") {
        const Ray alongEdge({-2, 0}, {-1, 0});  // overlaps the bottom edge y = 0
        CHECK_FALSE(triangle.boundaryContains(alongEdge));
        CHECK_FALSE(triangle.interiorContains(alongEdge));
    }
}

TEST_CASE("Ray and triangle as convex predicates tests") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Convex = pgl::Convex<Point>;
    using Ray = pgl::Ray<Point>;

    const Convex triangle({{0, 0}, {6, 0}, {0, 6}});

    SUBCASE("ray from outside cuts through the interior") {
        const Ray cut({-2, 2}, {-1, 2});

        CHECK(cut.intersects(triangle));
        CHECK(triangle.intersects(cut));
        CHECK(cut.interiorsIntersect(triangle));
        CHECK(triangle.interiorsIntersect(cut));
        CHECK_FALSE(triangle.contains(cut.source()));

        // The chord runs boundary-to-boundary: it splits the triangle in two,
        // and removing it splits the ray into a head and a tail. Both separate
        // each other, hence they cross.
        CHECK(cut.separates(triangle));
        CHECK(triangle.separates(cut));
        CHECK(cut.crosses(triangle));
        CHECK(triangle.crosses(cut));

        const auto clipped = cut.intersection(triangle);
        REQUIRE(clipped);
        REQUIRE(std::holds_alternative<Segment>(*clipped));
        CHECK(std::get<Segment>(*clipped) == Segment(Point(0, 2), Point(4, 2)));
    }

    SUBCASE("ray starts strictly inside and exits once") {
        const Ray out({2, 2}, {3, 2});

        CHECK(out.intersects(triangle));
        CHECK(triangle.intersects(out));
        CHECK(out.interiorsIntersect(triangle));
        CHECK(triangle.interiorsIntersect(out));
        CHECK(triangle.contains(out.source()));

        // The inside head leaves a single outside tail: nothing is separated.
        CHECK_FALSE(out.separates(triangle));
        CHECK_FALSE(triangle.separates(out));
        CHECK_FALSE(out.crosses(triangle));
        CHECK_FALSE(triangle.crosses(out));

        const auto clipped = out.intersection(triangle);
        REQUIRE(clipped);
        REQUIRE(std::holds_alternative<Segment>(*clipped));
        CHECK(std::get<Segment>(*clipped) == Segment(Point(2, 2), Point(4, 2)));
    }

    SUBCASE("ray points away from the triangle") {
        const Ray away({-2, 2}, {-3, 2});

        CHECK_FALSE(away.intersects(triangle));
        CHECK_FALSE(triangle.intersects(away));
        CHECK_FALSE(away.interiorsIntersect(triangle));
        CHECK_FALSE(triangle.interiorsIntersect(away));
        CHECK_FALSE(away.separates(triangle));
        CHECK_FALSE(triangle.separates(away));
        CHECK_FALSE(away.crosses(triangle));
        CHECK_FALSE(triangle.crosses(away));
        CHECK_FALSE(away.intersection(triangle));
    }

    SUBCASE("ray runs along a triangle edge") {
        const Ray along_edge({-2, 0}, {-1, 0});

        CHECK(along_edge.intersects(triangle));
        CHECK(triangle.intersects(along_edge));
        CHECK_FALSE(along_edge.interiorsIntersect(triangle));
        CHECK_FALSE(triangle.interiorsIntersect(along_edge));

        // The edge overlap splits the ray, but the polygon interior is intact.
        CHECK(triangle.separates(along_edge));
        CHECK_FALSE(along_edge.separates(triangle));
        CHECK_FALSE(along_edge.crosses(triangle));
        CHECK_FALSE(triangle.crosses(along_edge));

        const auto overlap = along_edge.intersection(triangle);
        REQUIRE(overlap);
        REQUIRE(std::holds_alternative<Segment>(*overlap));
        CHECK(std::get<Segment>(*overlap) == Segment(Point(0, 0), Point(6, 0)));
    }

    SUBCASE("ray tangent at a single vertex") {
        const Ray tangent({6, -2}, {6, -1});

        CHECK(tangent.intersects(triangle));
        CHECK(triangle.intersects(tangent));
        CHECK_FALSE(tangent.interiorsIntersect(triangle));
        CHECK_FALSE(triangle.interiorsIntersect(tangent));

        // Passing through the lone vertex splits the ray but not the triangle.
        CHECK(triangle.separates(tangent));
        CHECK_FALSE(tangent.separates(triangle));
        CHECK_FALSE(tangent.crosses(triangle));
        CHECK_FALSE(triangle.crosses(tangent));

        const auto isec = tangent.intersection(triangle);
        REQUIRE(isec);
        REQUIRE(std::holds_alternative<Point>(*isec));
        CHECK(std::get<Point>(*isec) == Point(6, 0));
    }

    SUBCASE("ray disjoint from the convex") {
        const Ray farShape({-2, 9}, {-1, 9});

        CHECK_FALSE(farShape.intersects(triangle));
        CHECK_FALSE(triangle.intersects(farShape));
        CHECK_FALSE(farShape.interiorsIntersect(triangle));
        CHECK_FALSE(triangle.interiorsIntersect(farShape));
        CHECK_FALSE(farShape.intersection(triangle));
    }
}

TEST_CASE("Ray and convex predicates are invariant under convex translation") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Convex = pgl::Convex<Point>;
    using Ray = pgl::Ray<Point>;

    // Convex::intersects(Ray) used to read the raw vertices while ignoring the
    // polygon's translation_ offset, so the answer flipped once the polygon was
    // translated. The ray sources here stay outside the polygon so the query
    // exercises the chord search rather than the contains() short-circuit.
    for (const Point shift : {Point(0, 0), Point(36, 24), Point(-50, -17)}) {
        Convex pentagon({{0, 0}, {8, 8}, {16, 8}, {24, 0}, {12, -8}});
        pentagon += shift;

        const auto ray = [&shift](Point a, Point b) { return Ray(a + shift, b + shift); };
        const auto pt = [&shift](Point p) { return p + shift; };

        // Ray along y = 2 enters from the left and crosses the interior.
        const Ray cut = ray({-10, 2}, {-9, 2});
        CHECK(cut.intersects(pentagon));
        CHECK(pentagon.intersects(cut));
        CHECK(cut.interiorsIntersect(pentagon));
        CHECK(pentagon.interiorsIntersect(cut));
        CHECK(cut.separates(pentagon));
        CHECK(pentagon.separates(cut));
        CHECK(cut.crosses(pentagon));
        CHECK(pentagon.crosses(cut));
        {
            const auto clipped = cut.intersection(pentagon);
            REQUIRE(clipped);
            REQUIRE(std::holds_alternative<Segment>(*clipped));
            CHECK(std::get<Segment>(*clipped) == Segment(pt({2, 2}), pt({22, 2})));
        }

        // Ray along y = 8 overlaps the top edge (8,8)-(16,8) only.
        const Ray along_edge = ray({-10, 8}, {-9, 8});
        CHECK(along_edge.intersects(pentagon));
        CHECK(pentagon.intersects(along_edge));
        CHECK_FALSE(along_edge.interiorsIntersect(pentagon));
        CHECK_FALSE(pentagon.interiorsIntersect(along_edge));
        {
            const auto overlap = along_edge.intersection(pentagon);
            REQUIRE(overlap);
            REQUIRE(std::holds_alternative<Segment>(*overlap));
            CHECK(std::get<Segment>(*overlap) == Segment(pt({8, 8}), pt({16, 8})));
        }

        // Ray along y = -8 only grazes the lone bottom vertex (12,-8).
        const Ray tangent = ray({-10, -8}, {-9, -8});
        CHECK(tangent.intersects(pentagon));
        CHECK(pentagon.intersects(tangent));
        CHECK_FALSE(tangent.interiorsIntersect(pentagon));
        CHECK_FALSE(pentagon.interiorsIntersect(tangent));
        {
            const auto isec = tangent.intersection(pentagon);
            REQUIRE(isec);
            REQUIRE(std::holds_alternative<Point>(*isec));
            CHECK(std::get<Point>(*isec) == pt({12, -8}));
        }

        // Ray along y = 2 but pointing away (-x): never reaches the polygon.
        const Ray away = ray({-10, 2}, {-11, 2});
        CHECK_FALSE(away.intersects(pentagon));
        CHECK_FALSE(pentagon.intersects(away));
        CHECK_FALSE(away.interiorsIntersect(pentagon));
        CHECK_FALSE(pentagon.interiorsIntersect(away));
        CHECK_FALSE(away.intersection(pentagon));
    }
}
