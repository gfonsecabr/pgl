#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <variant>

#include "pgl.hpp"


TEST_CASE("OrientedLine containment of Ray, both directions") {
    using Point = pgl::Point<int>;
    using Ray = pgl::Ray<Point>;
    using OrientedLine = pgl::OrientedLine<Point>;

    const OrientedLine diagonal({0, 0}, {4, 4});

    SUBCASE("collinear ray is contained by the oriented line; orientation of ray is ignored") {
        const Ray forward({1, 1}, {3, 3});   // same direction as diagonal
        const Ray backward({3, 3}, {1, 1});  // opposite direction, still on the line
        const Ray distant({50, 50}, {60, 60});   // far past defining points
        CHECK_MESSAGE(diagonal.contains(forward), diagonal, " contains ", forward);
        CHECK_MESSAGE(diagonal.interiorContains(forward), diagonal, " interiorContains ", forward);
        CHECK_MESSAGE(diagonal.contains(backward), diagonal, " contains ", backward);
        CHECK_MESSAGE(diagonal.contains(distant), diagonal, " contains ", distant);
    }

    SUBCASE("off-line ray is not contained") {
        const Ray crossing({0, 4}, {4, 0});
        CHECK_FALSE_MESSAGE(diagonal.contains(crossing), diagonal, " contains ", crossing);
    }

    SUBCASE("a finite Ray can never contain an infinite OrientedLine") {
        const Ray forward({1, 1}, {3, 3});
        CHECK_FALSE_MESSAGE(forward.contains(diagonal), forward, " contains ", diagonal);
        CHECK_FALSE_MESSAGE(forward.interiorContains(diagonal), forward, " interiorContains ", diagonal);
    }
}

TEST_CASE("OrientedLine and Ray intersection predicates, both directions") {
    using Point = pgl::Point<int>;
    using Ray = pgl::Ray<Point>;
    using OrientedLine = pgl::OrientedLine<Point>;

    // vertical oriented line x = 2, pointing upward
    const OrientedLine vertical({2, -2}, {2, 2});

    SUBCASE("transversal crossing: both intersect and both interiors intersect") {
        const Ray crossing({0, 0}, {4, 0});  // horizontal ray, crosses at (2,0)
        CHECK_MESSAGE(vertical.intersects(crossing), vertical, " intersects ", crossing);
        CHECK_MESSAGE(crossing.intersects(vertical), crossing, " intersects ", vertical);
        CHECK_MESSAGE(vertical.interiorsIntersect(crossing), vertical, " interiorsIntersect ", crossing);
        CHECK_MESSAGE(crossing.interiorsIntersect(vertical), crossing, " interiorsIntersect ", vertical);
    }

    SUBCASE("ray source on the line: intersects but interiors do not") {
        const Ray sourced({2, 0}, {4, 2});   // source (2,0) on vertical, goes off it
        CHECK_MESSAGE(vertical.intersects(sourced), vertical, " intersects ", sourced);
        CHECK_MESSAGE(sourced.intersects(vertical), sourced, " intersects ", vertical);
        CHECK_FALSE_MESSAGE(vertical.interiorsIntersect(sourced), vertical, " interiorsIntersect ", sourced);
        CHECK_FALSE_MESSAGE(sourced.interiorsIntersect(vertical), sourced, " interiorsIntersect ", vertical);
    }

    SUBCASE("ray pointing away from the line: no intersection") {
        const Ray away({-1, 0}, {-4, 0});    // pointing left, away from x=2
        CHECK_FALSE_MESSAGE(vertical.intersects(away), vertical, " intersects ", away);
        CHECK_FALSE_MESSAGE(away.intersects(vertical), away, " intersects ", vertical);
    }

    SUBCASE("collinear ray: intersects and interiors intersect (line contains the ray)") {
        const Ray collinear({2, 3}, {2, 5});
        CHECK_MESSAGE(vertical.intersects(collinear), vertical, " intersects ", collinear);
        CHECK_MESSAGE(collinear.intersects(vertical), collinear, " intersects ", vertical);
        CHECK_MESSAGE(vertical.interiorsIntersect(collinear), vertical, " interiorsIntersect ", collinear);
    }

    SUBCASE("parallel ray: no intersection") {
        const Ray parallel({0, 0}, {0, 4});
        CHECK_FALSE_MESSAGE(vertical.intersects(parallel), vertical, " intersects ", parallel);
        CHECK_FALSE_MESSAGE(parallel.intersects(vertical), parallel, " intersects ", vertical);
    }
}

TEST_CASE("OrientedLine and Ray separates and crosses, both directions") {
    using Point = pgl::Point<int>;
    using Ray = pgl::Ray<Point>;
    using OrientedLine = pgl::OrientedLine<Point>;

    const OrientedLine vertical({2, -2}, {2, 2});

    SUBCASE("transversal crossing: both separate and both cross") {
        const Ray crossing({0, 0}, {4, 0});
        CHECK_MESSAGE(vertical.separates(crossing), vertical, " separates ", crossing);
        CHECK_MESSAGE(crossing.separates(vertical), crossing, " separates ", vertical);
        CHECK_MESSAGE(vertical.crosses(crossing), vertical, " crosses ", crossing);
        CHECK_MESSAGE(crossing.crosses(vertical), crossing, " crosses ", vertical);
    }

    SUBCASE("ray source on the line: ray separates the line, but line does not separate the ray") {
        const Ray sourced({2, 0}, {4, 2});
        CHECK_MESSAGE(sourced.separates(vertical), sourced, " separates ", vertical);
        CHECK_FALSE_MESSAGE(vertical.separates(sourced), vertical, " separates ", sourced);
        CHECK_FALSE_MESSAGE(vertical.crosses(sourced), vertical, " crosses ", sourced);
        CHECK_FALSE_MESSAGE(sourced.crosses(vertical), sourced, " crosses ", vertical);
    }

    SUBCASE("collinear ray: no separation") {
        const Ray collinear({2, 3}, {2, 5});
        CHECK_FALSE_MESSAGE(vertical.separates(collinear), vertical, " separates ", collinear);
        CHECK_FALSE_MESSAGE(collinear.separates(vertical), collinear, " separates ", vertical);
        CHECK_FALSE_MESSAGE(vertical.crosses(collinear), vertical, " crosses ", collinear);
    }
}

TEST_CASE("OrientedLine and Ray intersection construction, both directions") {
    using Point = pgl::Point<int>;
    using Ray = pgl::Ray<Point>;
    using OrientedLine = pgl::OrientedLine<Point>;

    const OrientedLine horizontal({0, 0}, {4, 0});

    SUBCASE("transversal crossing yields a single point") {
        const Ray crossing({2, -2}, {2, 2});

        const auto r1 = horizontal.intersection(crossing);
        REQUIRE(r1);
        REQUIRE(std::holds_alternative<Point>(*r1));
        CHECK_MESSAGE(std::get<Point>(*r1) == Point(2, 0), "horizontal ∩ crossing");

        const auto r2 = crossing.intersection(horizontal);
        REQUIRE(r2);
        REQUIRE(std::holds_alternative<Point>(*r2));
        CHECK_MESSAGE(std::get<Point>(*r2) == Point(2, 0), "crossing ∩ horizontal");
    }

    SUBCASE("collinear ray in the same-ish direction: intersection is that ray") {
        const Ray along({2, 0}, {6, 0});

        const auto r = horizontal.intersection(along);
        REQUIRE(r);
        REQUIRE(std::holds_alternative<Ray>(*r));
        CHECK_MESSAGE(std::get<Ray>(*r) == along, "horizontal ∩ along");
    }

    SUBCASE("collinear opposite-direction ray: intersection is the ray itself (line contains it)") {
        const Ray opposite({3, 0}, {1, 0});   // goes left from x=3, entirely on the line

        const auto r = horizontal.intersection(opposite);
        REQUIRE(r);
        REQUIRE(std::holds_alternative<Ray>(*r));
        CHECK_MESSAGE(std::get<Ray>(*r) == opposite, "horizontal ∩ opposite");
    }

    SUBCASE("disjoint ray: empty result") {
        const Ray away({-1, 1}, {-4, 1});     // parallel, above
        CHECK_FALSE_MESSAGE(horizontal.intersection(away), "horizontal ∩ away should be empty");
        CHECK_FALSE_MESSAGE(away.intersection(horizontal), "away ∩ horizontal should be empty");
    }
}
