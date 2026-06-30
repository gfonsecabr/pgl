#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

TEST_CASE("Segment and triangle predicates tests") {
    using Point = pgl::Point<int>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using Segment = pgl::Segment<Point>;
    using Triangle = pgl::Triangle<Point>;

    const Triangle triangle({0, 0}, {6, 0}, {0, 6});

    SUBCASE("edge-to-edge cut") {
        const Segment cut({-1, 2}, {5, 2});
        const OrientedSegment oriented_cut({5, 2}, {-1, 2});
        const Point interior_hit(2, 2);

        CHECK(cut.intersects(triangle));
        CHECK(triangle.intersects(cut));
        CHECK(oriented_cut.intersects(triangle));

        CHECK_FALSE(cut.contains(triangle));
        CHECK_FALSE(triangle.contains(cut));
        CHECK_FALSE(triangle.boundaryContains(cut));

        CHECK(cut.contains(interior_hit));
        CHECK(cut.interiorContains(interior_hit));
        CHECK(triangle.contains(interior_hit));
        CHECK(triangle.interiorContains(interior_hit));

        CHECK(cut.separates(triangle));
        CHECK(triangle.separates(cut));
        CHECK(oriented_cut.separates(triangle));
        CHECK(cut.crosses(triangle));
        CHECK(triangle.crosses(cut));
        CHECK(oriented_cut.crosses(triangle));

        const auto clipped = cut.intersection(triangle);
        REQUIRE(clipped);
        REQUIRE(std::holds_alternative<Segment>(*clipped));
        CHECK(std::get<Segment>(*clipped) == Segment(Point(0, 2), Point(4, 2)));
    }

    SUBCASE("vertex-to-opposite-edge cut") {
        const Segment cut({0, 0}, {3, 3});

        CHECK(cut.intersects(triangle));
        CHECK(triangle.intersects(cut));
        CHECK(cut.separates(triangle));
        CHECK_FALSE(triangle.separates(cut));
        CHECK_FALSE(cut.crosses(triangle));
        CHECK_FALSE(triangle.crosses(cut));
    }

    SUBCASE("single-vertex touch") {
        const Segment touch({-1, -1}, {0, 0});
        const Point shared_vertex(0, 0);

        CHECK(touch.intersects(triangle));
        CHECK(triangle.intersects(touch));

        CHECK(touch.contains(shared_vertex));
        CHECK(touch.boundaryContains(shared_vertex));
        CHECK(triangle.boundaryContains(shared_vertex));

        CHECK_FALSE(touch.separates(triangle));
        CHECK_FALSE(triangle.separates(touch));
        CHECK_FALSE(touch.crosses(triangle));
        CHECK_FALSE(triangle.crosses(touch));
    }

    SUBCASE("segment starts strictly inside triangle") {
        const Segment starts_inside({1, 1}, {5, 1});

        CHECK(starts_inside.intersects(triangle));
        CHECK(triangle.intersects(starts_inside));

        CHECK_FALSE(starts_inside.separates(triangle));
        CHECK_FALSE(triangle.separates(starts_inside));
        CHECK_FALSE(starts_inside.crosses(triangle));
        CHECK_FALSE(triangle.crosses(starts_inside));
    }

    SUBCASE("segment lies on a triangle edge") {
        const Segment along_edge({0, 0}, {6, 0});
        const Point edge_midpoint(3, 0);

        CHECK(along_edge.intersects(triangle));
        CHECK(triangle.intersects(along_edge));

        CHECK(along_edge.contains(edge_midpoint));
        CHECK(along_edge.interiorContains(edge_midpoint));
        CHECK(triangle.boundaryContains(edge_midpoint));
        CHECK_FALSE(triangle.interiorContains(edge_midpoint));

        CHECK_FALSE(along_edge.separates(triangle));
        CHECK_FALSE(triangle.separates(along_edge));
        CHECK_FALSE(along_edge.crosses(triangle));
        CHECK_FALSE(triangle.crosses(along_edge));

        const auto overlap = along_edge.intersection(triangle);
        REQUIRE(overlap);
        REQUIRE(std::holds_alternative<Segment>(*overlap));
        CHECK(std::get<Segment>(*overlap) == along_edge);
    }

    SUBCASE("strictly interior segment is contained but does not cut") {
        const Segment inner({1, 1}, {2, 2});

        CHECK(inner.intersects(triangle));
        CHECK(triangle.intersects(inner));
        CHECK(triangle.contains(inner));
        CHECK_FALSE(inner.contains(triangle));
        CHECK_FALSE(triangle.boundaryContains(inner));
        CHECK_FALSE(inner.separates(triangle));
        CHECK_FALSE(triangle.separates(inner));
        CHECK_FALSE(inner.crosses(triangle));
        CHECK_FALSE(triangle.crosses(inner));
    }

    SUBCASE("interiorsIntersect needs a shared interior point") {
        // A transversal chord and a strictly interior segment both reach the open
        // interior, both directions; a segment along an edge, one outside, and one
        // merely touching a vertex stay on the boundary, so interiors never meet.
        const Segment cut({-1, 2}, {5, 2});
        CHECK(triangle.interiorsIntersect(cut));
        CHECK(cut.interiorsIntersect(triangle));

        const Segment inner({1, 1}, {2, 2});
        CHECK(triangle.interiorsIntersect(inner));
        CHECK(inner.interiorsIntersect(triangle));

        const Segment along_edge({0, 0}, {6, 0});
        CHECK_FALSE(triangle.interiorsIntersect(along_edge));
        CHECK_FALSE(along_edge.interiorsIntersect(triangle));

        const Segment outside({-5, 2}, {-1, 2});
        CHECK_FALSE(triangle.interiorsIntersect(outside));
        CHECK_FALSE(outside.interiorsIntersect(triangle));

        const Segment vertex_touch({-1, -1}, {0, 0});
        CHECK_FALSE(triangle.interiorsIntersect(vertex_touch));
        CHECK_FALSE(vertex_touch.interiorsIntersect(triangle));
    }

    SUBCASE("boundary subsegment is contained by the boundary only") {
        const Segment boundary_subsegment({1, 0}, {3, 0});
        const Point boundary_midpoint(2, 0);

        CHECK(boundary_subsegment.intersects(triangle));
        CHECK(triangle.intersects(boundary_subsegment));
        CHECK(triangle.contains(boundary_subsegment));
        CHECK(triangle.boundaryContains(boundary_subsegment));
        CHECK_FALSE(triangle.interiorContains(boundary_midpoint));
        CHECK(boundary_subsegment.contains(boundary_midpoint));
        CHECK_FALSE(boundary_subsegment.separates(triangle));
        CHECK_FALSE(triangle.separates(boundary_subsegment));
    }
}
