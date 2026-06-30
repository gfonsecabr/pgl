#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <variant>

#include "pgl.hpp"

TEST_CASE("Ray containment of OrientedSegment, and OrientedSegment containment of Ray") {
    using Point = pgl::Point<int>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using Ray = pgl::Ray<Point>;

    const Ray diagonal({0, 0}, {4, 4});
    const OrientedSegment on_ray({3, 3}, {1, 1}); // collinear, within the ray's reach
    const OrientedSegment off_ray({3, 3}, {1, 2}); // goes off the ray's line

    SUBCASE("Ray contains a collinear OrientedSegment within its extent") {
        CHECK_MESSAGE(diagonal.contains(on_ray), diagonal, " contains ", on_ray);
    }

    SUBCASE("Ray does not contain an OrientedSegment going off its line") {
        CHECK_FALSE_MESSAGE(diagonal.contains(off_ray), diagonal, " contains ", off_ray);
    }

    SUBCASE("OrientedSegment cannot contain an infinite Ray") {
        CHECK_FALSE_MESSAGE(on_ray.contains(diagonal), on_ray, " contains ", diagonal);
        CHECK_FALSE_MESSAGE(on_ray.interiorContains(diagonal), on_ray, " interiorContains ", diagonal);
    }
}

TEST_CASE("OrientedSegment and Ray intersection predicates, both directions") {
    using Point = pgl::Point<int>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using Ray = pgl::Ray<Point>;

    const Ray horizontal({0, 0}, {4, 0}); // x >= 0, y = 0
    const OrientedSegment cross_os({2, -2}, {2, 2}); // crosses horizontal at (2,0)

    SUBCASE("crossing: both intersect and both interiors intersect") {
        CHECK_MESSAGE(horizontal.intersects(cross_os), horizontal, " intersects ", cross_os);
        CHECK_MESSAGE(cross_os.intersects(horizontal), cross_os, " intersects ", horizontal);
        CHECK_MESSAGE(horizontal.interiorsIntersect(cross_os), horizontal, " interiorsIntersect ", cross_os);
        CHECK_MESSAGE(cross_os.interiorsIntersect(horizontal), cross_os, " interiorsIntersect ", horizontal);
    }

    SUBCASE("OS with endpoint on the ray source: intersects but interiors do not") {
        const OrientedSegment touch_source({0, 0}, {0, 3}); // source = ray source (0,0)
        CHECK_MESSAGE(horizontal.intersects(touch_source), horizontal, " intersects ", touch_source);
        CHECK_FALSE_MESSAGE(horizontal.interiorsIntersect(touch_source), horizontal, " interiorsIntersect ", touch_source);
    }

    SUBCASE("OS entirely before the ray source: disjoint") {
        const OrientedSegment before({-3, 0}, {-1, 0}); // x in [-3,-1], left of ray
        CHECK_FALSE_MESSAGE(horizontal.intersects(before), horizontal, " intersects ", before);
        CHECK_FALSE_MESSAGE(before.intersects(horizontal), before, " intersects ", horizontal);
    }
}

TEST_CASE("OrientedSegment and Ray separates and crosses, both directions") {
    using Point = pgl::Point<int>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using Ray = pgl::Ray<Point>;

    const Ray horizontal({0, 0}, {4, 0});
    const OrientedSegment cross_os({2, -2}, {2, 2});

    SUBCASE("proper crossing through the interior of both") {
        CHECK_MESSAGE(horizontal.separates(cross_os), horizontal, " separates ", cross_os);
        CHECK_MESSAGE(cross_os.separates(horizontal), cross_os, " separates ", horizontal);
        CHECK_MESSAGE(horizontal.crosses(cross_os), horizontal, " crosses ", cross_os);
        CHECK_MESSAGE(cross_os.crosses(horizontal), cross_os, " crosses ", horizontal);
    }

    SUBCASE("OS crossing the ray source: ray separates OS, OS does not separate ray") {
        const OrientedSegment touch_src({0, -1}, {0, 1}); // cuts through (0,0), the ray source
        // The OS straddles the ray source; ray.separates(touch_src) is true because the
        // OS minus the source point is disconnected.
        CHECK_MESSAGE(horizontal.separates(touch_src), horizontal, " separates ", touch_src);
        // The ray minus (0,0) is still connected (open ray {x>0}), so OS does not separate ray.
        CHECK_FALSE_MESSAGE(touch_src.separates(horizontal), touch_src, " separates ", horizontal);
        CHECK_FALSE_MESSAGE(horizontal.crosses(touch_src), horizontal, " crosses ", touch_src);
        CHECK_FALSE_MESSAGE(touch_src.crosses(horizontal), touch_src, " crosses ", horizontal);
    }
}

TEST_CASE("Ray and OrientedSegment intersection construction, both directions") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using Ray = pgl::Ray<Point>;

    const Ray horizontal({0, 0}, {4, 0});

    SUBCASE("collinear OS overlapping the ray returns a segment") {
        const OrientedSegment col_os({3, 0}, {1, 0}); // reversed collinear OS
        const auto r = horizontal.intersection(col_os);
        REQUIRE(r);
        REQUIRE(std::holds_alternative<Segment>(*r));
        CHECK_MESSAGE(std::get<Segment>(*r) == Segment(Point(1, 0), Point(3, 0)), "horizontal ∩ col_os");

        const auto r2 = col_os.intersection(horizontal);
        REQUIRE(r2);
        REQUIRE(std::holds_alternative<Segment>(*r2));
        CHECK_MESSAGE(std::get<Segment>(*r2) == Segment(Point(1, 0), Point(3, 0)), "col_os ∩ horizontal");
    }

    SUBCASE("crossing OS yields a single point") {
        const OrientedSegment cross_os({2, -2}, {2, 2});
        const auto r = horizontal.intersection(cross_os);
        REQUIRE(r);
        REQUIRE(std::holds_alternative<Point>(*r));
        CHECK_MESSAGE(std::get<Point>(*r) == Point(2, 0), "horizontal ∩ cross_os");

        const auto r2 = cross_os.intersection(horizontal);
        REQUIRE(r2);
        REQUIRE(std::holds_alternative<Point>(*r2));
        CHECK_MESSAGE(std::get<Point>(*r2) == Point(2, 0), "cross_os ∩ horizontal");
    }

    SUBCASE("OS before the ray source: empty") {
        const OrientedSegment before({-3, 0}, {-1, 0});
        CHECK_FALSE_MESSAGE(horizontal.intersection(before), "horizontal ∩ before should be empty");
    }
}
