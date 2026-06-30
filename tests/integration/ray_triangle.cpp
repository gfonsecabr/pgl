#include "pgl.hpp"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstdint>
#include <variant>

TEST_CASE("Triangle and Ray intersection predicates, both directions") {
    using Point = pgl::Point<int>;
    using Triangle = pgl::Triangle<Point>;
    using Ray = pgl::Ray<Point>;

    const Triangle tri({0, 0}, {4, 0}, {0, 4});

    SUBCASE("ray crossing through the interior") {
        const Ray cut({-1, 1}, {5, 1});
        CHECK_MESSAGE(tri.intersects(cut), tri, " intersects ", cut);
        CHECK_MESSAGE(cut.intersects(tri), cut, " intersects ", tri);
        CHECK_MESSAGE(tri.interiorsIntersect(cut), tri, " interiorsIntersect ", cut);
        CHECK_MESSAGE(cut.interiorsIntersect(tri), cut, " interiorsIntersect ", tri);
    }

    SUBCASE("ray touching only a vertex") {
        const Ray touch({-1, 1}, {0, 0});
        CHECK_MESSAGE(tri.intersects(touch), tri, " intersects tangent ray");
        CHECK_FALSE_MESSAGE(tri.interiorsIntersect(touch), tri, " interiorsIntersect tangent ray");
        CHECK_FALSE_MESSAGE(touch.interiorsIntersect(tri), "tangent ray interiorsIntersect ", tri);
    }

    SUBCASE("ray pointing away from triangle") {
        const Ray away({5, 5}, {6, 6});
        CHECK_FALSE_MESSAGE(tri.intersects(away), tri, " intersects away ray");
        CHECK_FALSE_MESSAGE(away.intersects(tri), "away ray intersects ", tri);
        CHECK_FALSE_MESSAGE(tri.interiorsIntersect(away), tri, " interiorsIntersect away");
        CHECK_FALSE_MESSAGE(away.interiorsIntersect(tri), "away interiorsIntersect ", tri);
    }
}

TEST_CASE("Triangle interiorsIntersect Ray: strict interior hit only") {
    using Point = pgl::Point<int>;
    using Triangle = pgl::Triangle<Point>;
    using Ray = pgl::Ray<Point>;

    const Triangle tri({0, 0}, {6, 0}, {0, 6});

    CHECK_MESSAGE(tri.interiorsIntersect(Ray({-1, 2}, {5, 2})), tri, " interiorsIntersect crossing ray");
    CHECK_MESSAGE(tri.interiorsIntersect(Ray({1, 1}, {5, 1})), tri, " interiorsIntersect source-inside ray");
    CHECK_FALSE_MESSAGE(tri.interiorsIntersect(Ray({0, 0}, {6, 0})), tri, " interiorsIntersect along-edge ray");
    // The ray passes through vertex (0,0) and continues into interior.
    CHECK_MESSAGE(tri.interiorsIntersect(Ray({-1, -1}, {0, 0})), tri, " interiorsIntersect ray through vertex into interior");
}

TEST_CASE("Triangle separates Ray and Ray separates Triangle") {
    using Point = pgl::Point<int>;
    using Triangle = pgl::Triangle<Point>;
    using Ray = pgl::Ray<Point>;

    const Triangle tri({0, 0}, {6, 0}, {0, 6});

    // Ray entering from outside and passing fully through both sides.
    const Ray edge_to_edge({-1, 2}, {5, 2});
    CHECK_MESSAGE(edge_to_edge.separates(tri), "edge-to-edge ray separates ", tri);
    CHECK_MESSAGE(tri.separates(edge_to_edge), tri, " separates edge-to-edge ray");

    // Ray from a vertex diagonally through the interior: separates the triangle
    // (cuts it into two pieces), but the triangle does NOT separate the ray (only
    // one connected piece of the ray lies outside the triangle — from exit to ∞).
    const Ray vertex_ray({0, 0}, {3, 3});
    CHECK_MESSAGE(vertex_ray.separates(tri), "vertex-to-edge ray separates ", tri);
    CHECK_FALSE_MESSAGE(tri.separates(vertex_ray), tri, " separates vertex-to-edge ray");

    // Ray source strictly inside: doesn't separate the triangle.
    const Ray inside({1, 1}, {5, 1});
    CHECK_FALSE_MESSAGE(inside.separates(tri), "inside-source ray does not separate ", tri);
    CHECK_FALSE_MESSAGE(tri.separates(inside), tri, " does not separate inside-source ray");

    // Ray along an edge: doesn't separate the triangle.
    const Ray along_edge({0, 0}, {6, 0});
    CHECK_FALSE_MESSAGE(along_edge.separates(tri), "along-edge ray does not separate ", tri);
}

TEST_CASE("Ray crosses Triangle and Triangle crosses Ray") {
    using Point = pgl::Point<int>;
    using Triangle = pgl::Triangle<Point>;
    using Ray = pgl::Ray<Point>;

    // Use a triangle straddling y=0 so the horizontal ray cuts straight through.
    const Ray horizontal({0, 0}, {4, 0});
    const Triangle straddles({1, -1}, {3, -1}, {2, 2});

    CHECK_MESSAGE(horizontal.crosses(straddles), horizontal, " crosses straddling triangle");
    CHECK_MESSAGE(straddles.crosses(horizontal), "straddling triangle crosses ", horizontal);

    // A ray with its source strictly inside the triangle does not cross it.
    const Triangle tri({0, 0}, {6, 0}, {0, 6});
    const Ray source_inside({1, 1}, {5, 1});
    CHECK_FALSE_MESSAGE(source_inside.crosses(tri), "source_inside ray does not cross ", tri);
    CHECK_FALSE_MESSAGE(tri.crosses(source_inside), tri, " does not cross source_inside ray");
}

TEST_CASE("Triangle intersection with Ray returns clipped segment or touching point") {
    using Point = pgl::Point<int>;
    using Rational = pgl::Rational<int64_t>;
    using RationalPoint = pgl::Point<Rational>;
    using Triangle = pgl::Triangle<Point>;
    using Ray = pgl::Ray<Point>;
    using RationalSegment = pgl::Segment<RationalPoint>;

    const Triangle tri({0, 0}, {4, 0}, {0, 4});

    SUBCASE("crossing ray: clipped to segment inside the triangle") {
        const auto r = tri.intersection<Rational>(Ray({-1, 1}, {5, 1}));
        REQUIRE(r);
        REQUIRE(std::holds_alternative<RationalSegment>(*r));
        CHECK(std::get<RationalSegment>(*r) ==
              RationalSegment(RationalPoint(0, 1), RationalPoint(3, 1)));
    }

    SUBCASE("source inside, exits once: clipped from source to exit") {
        const auto r = tri.intersection<Rational>(Ray({1, 1}, {5, 1}));
        REQUIRE(r);
        REQUIRE(std::holds_alternative<RationalSegment>(*r));
        CHECK(std::get<RationalSegment>(*r) ==
              RationalSegment(RationalPoint(1, 1), RationalPoint(3, 1)));
    }

    SUBCASE("ray touching only a vertex: Point result") {
        const auto r = tri.intersection(Ray({-1, 1}, {0, 0}));
        REQUIRE(r);
        REQUIRE(std::holds_alternative<Point>(*r));
        CHECK(std::get<Point>(*r) == Point(0, 0));
    }

    SUBCASE("ray pointing away: empty") {
        CHECK_FALSE(tri.intersection(Ray({5, 5}, {6, 6})));
    }
}
