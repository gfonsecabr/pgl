#include "pgl.hpp"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <variant>

TEST_CASE("Rectangle containment of Ray: always false since rays are unbounded") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;
    using Ray = pgl::Ray<Point>;

    const Rectangle rect({0, 0}, {4, 3});

    // A ray extends infinitely, so no finite rectangle can contain it.
    const Ray inner({1, 1}, {3, 2});
    CHECK_FALSE_MESSAGE(rect.contains(inner), rect, " contains ", inner);
    CHECK_FALSE_MESSAGE(rect.interiorContains(inner), rect, " interiorContains ", inner);

    const Ray crossing({-2, 1}, {2, 1});
    CHECK_FALSE_MESSAGE(rect.contains(crossing), rect, " contains ", crossing);

    const Ray along_edge({0, 0}, {4, 0});
    CHECK_FALSE_MESSAGE(rect.contains(along_edge), rect, " contains ", along_edge);
    CHECK_FALSE_MESSAGE(rect.interiorContains(along_edge), rect, " interiorContains ", along_edge);
}

TEST_CASE("Rectangle and Ray intersection predicates, both directions") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;
    using Ray = pgl::Ray<Point>;

    const Rectangle rect({0, 0}, {4, 3});
    const Ray crossing_ray({-2, 1}, {2, 1});
    const Ray corner_entry_ray({-1, 0}, {0, 1});
    const Ray source_inside_ray({2, 1}, {8, 1});
    const Ray boundary_entering_ray({0, 1}, {2, 1});
    const Ray boundary_leaving_ray({0, 1}, {-2, 1});
    const Ray edge_following_ray({0, 0}, {4, 0});
    const Ray corner_tangent_ray({-1, 1}, {1, -1});
    const Ray outside_ray({-2, 5}, {2, 5});

    SUBCASE("intersects") {
        CHECK_MESSAGE(rect.intersects(crossing_ray), rect, " intersects crossing ray");
        CHECK_MESSAGE(crossing_ray.intersects(rect), "crossing ray intersects ", rect);
        CHECK_MESSAGE(rect.intersects(corner_entry_ray), rect, " intersects corner_entry");
        CHECK_MESSAGE(rect.intersects(source_inside_ray), rect, " intersects source_inside");
        CHECK_MESSAGE(rect.intersects(boundary_entering_ray), rect, " intersects boundary_entering");
        CHECK_MESSAGE(rect.intersects(boundary_leaving_ray), rect, " intersects boundary_leaving");
        CHECK_MESSAGE(rect.intersects(edge_following_ray), rect, " intersects edge_following");
        CHECK_MESSAGE(rect.intersects(corner_tangent_ray), rect, " intersects corner_tangent");
        CHECK_FALSE_MESSAGE(rect.intersects(outside_ray), rect, " intersects outside");
        CHECK_FALSE_MESSAGE(outside_ray.intersects(rect), "outside ray intersects ", rect);
    }

    SUBCASE("interiorsIntersect") {
        CHECK_MESSAGE(rect.interiorsIntersect(crossing_ray), rect, " interiorsIntersect crossing");
        CHECK_MESSAGE(crossing_ray.interiorsIntersect(rect), "crossing interiorsIntersect ", rect);
        CHECK_MESSAGE(rect.interiorsIntersect(corner_entry_ray), rect, " interiorsIntersect corner_entry");
        CHECK_MESSAGE(rect.interiorsIntersect(source_inside_ray), rect, " interiorsIntersect source_inside");
        CHECK_MESSAGE(rect.interiorsIntersect(boundary_entering_ray), rect, " interiorsIntersect boundary_entering");
        CHECK_FALSE_MESSAGE(rect.interiorsIntersect(boundary_leaving_ray), rect, " interiorsIntersect boundary_leaving");
        CHECK_FALSE_MESSAGE(rect.interiorsIntersect(edge_following_ray), rect, " interiorsIntersect edge_following");
        CHECK_FALSE_MESSAGE(rect.interiorsIntersect(corner_tangent_ray), rect, " interiorsIntersect corner_tangent");
        CHECK_FALSE_MESSAGE(rect.interiorsIntersect(outside_ray), rect, " interiorsIntersect outside");
    }
}

TEST_CASE("Rectangle separates and crosses Ray") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;
    using Ray = pgl::Ray<Point>;

    const Rectangle rect({0, 0}, {4, 3});

    const Ray crossing({-2, 1}, {2, 1});
    const Ray source_inside({2, 1}, {8, 1});
    const Ray boundary_entering({0, 1}, {2, 1});

    CHECK_MESSAGE(rect.separates(crossing), rect, " separates crossing ray");
    CHECK_MESSAGE(rect.crosses(crossing), rect, " crosses crossing ray");
    CHECK_FALSE_MESSAGE(rect.separates(source_inside), rect, " separates source_inside ray");
    CHECK_FALSE_MESSAGE(rect.separates(boundary_entering), rect, " separates boundary_entering ray");
    CHECK_FALSE_MESSAGE(rect.crosses(boundary_entering), rect, " crosses boundary_entering ray");
}

TEST_CASE("Ray separates, interiorsIntersect, and crosses Rectangle") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;
    using Ray = pgl::Ray<Point>;

    const Ray horizontal({0, 0}, {4, 0});
    const Rectangle rect({1, -1}, {3, 1});

    CHECK_MESSAGE(horizontal.interiorsIntersect(rect), horizontal, " interiorsIntersect ", rect);
    CHECK_MESSAGE(horizontal.separates(rect), horizontal, " separates ", rect);
    CHECK_MESSAGE(horizontal.crosses(rect), horizontal, " crosses ", rect);

    // Ray fully to the left of the rectangle does not separate it.
    CHECK_FALSE_MESSAGE(Ray({2, 1}, {5, 1}).separates(rect), "source-inside ray", " separates ", rect);
}

TEST_CASE("Ray separates Rectangle: full passage only") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;
    using Ray = pgl::Ray<Point>;

    const Rectangle rect({0, 0}, {4, 3});

    CHECK_MESSAGE(Ray({-1, 1}, {5, 1}).separates(rect), "side-to-side ray separates ", rect);
    CHECK_FALSE_MESSAGE(Ray({2, 1}, {5, 1}).separates(rect), "source-inside ray does not separate");
}

TEST_CASE("Rectangle intersection clips Ray into segment or point") {
    using Point = pgl::Point<int>;
    using Rectangle = pgl::Rectangle<Point>;
    using Ray = pgl::Ray<Point>;
    using Segment = pgl::Segment<Point>;

    const Rectangle rect({0, 0}, {4, 3});

    SUBCASE("crossing ray is clipped to interior segment") {
        const auto r = rect.intersection(Ray({-2, 1}, {2, 1}));
        REQUIRE(r);
        REQUIRE(std::holds_alternative<Segment>(*r));
        CHECK(std::get<Segment>(*r) == Segment({0, 1}, {4, 1}));
    }

    SUBCASE("source-inside ray is clipped to exit segment") {
        const auto r = rect.intersection(Ray({2, 1}, {8, 1}));
        REQUIRE(r);
        REQUIRE(std::holds_alternative<Segment>(*r));
        CHECK(std::get<Segment>(*r) == Segment({2, 1}, {4, 1}));
    }

    SUBCASE("ray along boundary edge is clipped to that edge segment") {
        const auto r = rect.intersection(Ray({-2, 0}, {2, 0}));
        REQUIRE(r);
        REQUIRE(std::holds_alternative<Segment>(*r));
        CHECK(std::get<Segment>(*r) == Segment({0, 0}, {4, 0}));
    }

    SUBCASE("ray entirely outside: empty") {
        CHECK_FALSE(rect.intersection(Ray({-2, 5}, {2, 5})));
    }
}
