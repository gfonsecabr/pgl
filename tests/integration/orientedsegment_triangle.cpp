#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <variant>

#include "pgl.hpp"

TEST_CASE("Triangle containment of OrientedSegment, and OrientedSegment containment of Triangle") {
    using Point = pgl::Point<int>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using Triangle = pgl::Triangle<Point>;

    const Triangle tri(0, 0, 6, 0, 0, 6);
    const OrientedSegment inner({1, 1}, {2, 2});           // strictly inside
    const OrientedSegment on_edge({3, 0}, {1, 0});         // on the base edge
    const OrientedSegment crossing({5, 2}, {-1, 2});       // crosses the triangle
    const OrientedSegment sub({0, 0}, {3, 0});             // along edge, includes vertex

    SUBCASE("Triangle contains and interiorContains a wholly interior OrientedSegment") {
        CHECK_MESSAGE(tri.contains(inner), tri, " contains ", inner);
        CHECK_MESSAGE(tri.interiorContains(inner), tri, " interiorContains ", inner);
    }

    SUBCASE("Triangle contains a boundary OrientedSegment but does not interiorContain it") {
        CHECK_MESSAGE(tri.contains(on_edge), tri, " contains ", on_edge);
        CHECK_FALSE_MESSAGE(tri.interiorContains(on_edge), tri, " interiorContains ", on_edge);
    }

    SUBCASE("Triangle boundaryContains an edge-lying OrientedSegment") {
        CHECK_MESSAGE(tri.boundaryContains(on_edge), tri, " boundaryContains ", on_edge);
        CHECK_FALSE_MESSAGE(tri.boundaryContains(inner), tri, " boundaryContains ", inner);
    }

    SUBCASE("Triangle does not contain a crossing OrientedSegment") {
        CHECK_FALSE_MESSAGE(tri.contains(crossing), tri, " contains ", crossing);
    }

    SUBCASE("OrientedSegment never contains an area Triangle") {
        CHECK_FALSE_MESSAGE(inner.contains(tri), inner, " contains ", tri);
        CHECK_FALSE_MESSAGE(inner.interiorContains(tri), inner, " interiorContains ", tri);
    }
}

TEST_CASE("Triangle and OrientedSegment intersection predicates, both directions") {
    using Point = pgl::Point<int>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using Triangle = pgl::Triangle<Point>;

    const Triangle tri(0, 0, 6, 0, 0, 6);
    const OrientedSegment crossing({5, 2}, {-1, 2}); // crosses edge-to-edge
    const OrientedSegment on_edge({3, 0}, {1, 0});   // lies on edge

    SUBCASE("crossing: both intersect and both interiors intersect") {
        CHECK_MESSAGE(tri.intersects(crossing), tri, " intersects ", crossing);
        CHECK_MESSAGE(crossing.intersects(tri), crossing, " intersects ", tri);
        CHECK_MESSAGE(tri.interiorsIntersect(crossing), tri, " interiorsIntersect ", crossing);
        CHECK_MESSAGE(crossing.interiorsIntersect(tri), crossing, " interiorsIntersect ", tri);
    }

    SUBCASE("edge-lying OS: intersects but interiors do not") {
        CHECK_MESSAGE(tri.intersects(on_edge), tri, " intersects ", on_edge);
        CHECK_FALSE_MESSAGE(tri.interiorsIntersect(on_edge), tri, " interiorsIntersect ", on_edge);
        CHECK_FALSE_MESSAGE(on_edge.interiorsIntersect(tri), on_edge, " interiorsIntersect ", tri);
    }

    SUBCASE("disjoint: no intersection") {
        const OrientedSegment outside({7, 0}, {10, 0});
        CHECK_FALSE_MESSAGE(tri.intersects(outside), tri, " intersects ", outside);
        CHECK_FALSE_MESSAGE(outside.intersects(tri), outside, " intersects ", tri);
    }
}

TEST_CASE("Triangle and OrientedSegment separates and crosses, both directions") {
    using Point = pgl::Point<int>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using Triangle = pgl::Triangle<Point>;

    const Triangle tri(0, 0, 6, 0, 0, 6);

    SUBCASE("crossing OS: both separate each other and both cross") {
        const OrientedSegment edge_to_edge({5, 2}, {-1, 2}); // enters and exits the triangle
        CHECK_MESSAGE(tri.separates(edge_to_edge), tri, " separates ", edge_to_edge);
        CHECK_MESSAGE(edge_to_edge.separates(tri), edge_to_edge, " separates ", tri);
        CHECK_MESSAGE(tri.crosses(edge_to_edge), tri, " crosses ", edge_to_edge);
        CHECK_MESSAGE(edge_to_edge.crosses(tri), edge_to_edge, " crosses ", tri);
    }

    SUBCASE("OS starting inside the triangle does not separate the triangle") {
        const OrientedSegment starts_inside({1, 1}, {8, 1}); // source inside, exits
        CHECK_FALSE_MESSAGE(tri.separates(starts_inside), tri, " separates ", starts_inside);
        CHECK_FALSE_MESSAGE(starts_inside.separates(tri), starts_inside, " separates ", tri);
        CHECK_FALSE_MESSAGE(tri.crosses(starts_inside), tri, " crosses ", starts_inside);
        CHECK_FALSE_MESSAGE(starts_inside.crosses(tri), starts_inside, " crosses ", tri);
    }

    SUBCASE("OS only touching a vertex: no separation") {
        const OrientedSegment touch_vertex({-1, -1}, {0, 0}); // endpoint at vertex (0,0)
        CHECK_FALSE_MESSAGE(tri.separates(touch_vertex), tri, " separates ", touch_vertex);
        CHECK_FALSE_MESSAGE(touch_vertex.separates(tri), touch_vertex, " separates ", tri);
    }
}

TEST_CASE("Triangle and OrientedSegment intersection construction") {
    using Point = pgl::Point<int>;
    using Rational = pgl::Rational<int64_t>;
    using RationalPoint = pgl::Point<Rational>;
    using RationalSegment = pgl::Segment<RationalPoint>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using Segment = pgl::Segment<Point>;
    using Triangle = pgl::Triangle<Point>;

    const Triangle tri(0, 0, 4, 0, 0, 4);

    SUBCASE("crossing OS: result is the clipped interior segment") {
        const OrientedSegment crossing({-1, 1}, {5, 1});
        const auto r = tri.intersection<Rational>(crossing);
        REQUIRE(r);
        REQUIRE(std::holds_alternative<RationalSegment>(*r));
        CHECK_MESSAGE(std::get<RationalSegment>(*r) ==
                          RationalSegment(RationalPoint(Rational(0), Rational(1)),
                                         RationalPoint(Rational(3), Rational(1))),
                      "tri ∩ crossing");
    }

    SUBCASE("OS along a triangle edge: returns that segment") {
        const OrientedSegment on_edge({0, 0}, {3, 0});
        const auto r = tri.intersection(on_edge);
        REQUIRE(r);
        REQUIRE(std::holds_alternative<Segment>(*r));
        CHECK_MESSAGE(std::get<Segment>(*r) == Segment({0, 0}, {3, 0}), "tri ∩ on_edge");
    }

    SUBCASE("OS touching a vertex only: returns a point") {
        const OrientedSegment touch({-1, -1}, {0, 0});
        const auto r = tri.intersection(touch);
        REQUIRE(r);
        REQUIRE(std::holds_alternative<Point>(*r));
        CHECK_MESSAGE(std::get<Point>(*r) == Point(0, 0), "tri ∩ touch vertex");
    }

    SUBCASE("disjoint OS: empty") {
        const OrientedSegment outside({5, 5}, {6, 6});
        CHECK_FALSE_MESSAGE(tri.intersection(outside), "tri ∩ outside should be empty");
    }
}
