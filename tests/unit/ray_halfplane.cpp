#include "pgl.hpp"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <variant>

TEST_CASE("Halfplane containment of Ray") {
    using Point = pgl::Point<int>;
    using Halfplane = pgl::Halfplane<Point>;
    using Ray = pgl::Ray<Point>;

    // upper = closed half-plane y >= 0, boundary y = 0
    const Halfplane upper({0, 0}, {4, 0});
    const Ray inside({1, 1}, {3, 2});
    const Ray leaving({1, 1}, {3, 0});
    const Ray entering({1, -2}, {3, 1});
    const Ray outside_parallel({1, -1}, {3, -1});
    const Ray boundary({1, 0}, {3, 0});

    CHECK_MESSAGE(upper.contains(inside), upper, " contains ", inside);
    CHECK_MESSAGE(upper.interiorContains(inside), upper, " interiorContains ", inside);
    CHECK_FALSE_MESSAGE(upper.contains(leaving), upper, " contains ", leaving);
    CHECK_FALSE_MESSAGE(upper.contains(entering), upper, " contains ", entering);
    CHECK_MESSAGE(upper.contains(boundary), upper, " contains ", boundary);
    CHECK_FALSE_MESSAGE(upper.interiorContains(boundary), upper, " interiorContains ", boundary);
    CHECK_FALSE_MESSAGE(upper.contains(Halfplane({2, -3}, {2, -3})), upper,
                        " contains degenerate outside ray");
}

TEST_CASE("Halfplane boundaryContains Ray") {
    using Point = pgl::Point<int>;
    using Halfplane = pgl::Halfplane<Point>;
    using Ray = pgl::Ray<Point>;

    const Halfplane diagonal({0, 0}, {4, 4});

    CHECK_MESSAGE(diagonal.boundaryContains(Ray({1, 1}, {2, 2})), diagonal,
                  " boundaryContains collinear ray");
    CHECK_FALSE_MESSAGE(diagonal.boundaryContains(Ray({1, 1}, {2, 3})), diagonal,
                        " boundaryContains off-boundary ray");
}

TEST_CASE("Halfplane and Ray intersection predicates, both directions") {
    using Point = pgl::Point<int>;
    using Halfplane = pgl::Halfplane<Point>;
    using Ray = pgl::Ray<Point>;

    const Halfplane upper({0, 0}, {4, 0});
    const Ray inside({1, 1}, {3, 2});
    const Ray leaving({1, 1}, {3, 0});
    const Ray entering({1, -2}, {3, 1});
    const Ray outside_parallel({1, -1}, {3, -1});
    const Ray boundary({1, 0}, {3, 0});

    SUBCASE("ray fully inside: both intersect; halfplane interiorContains implies interiorsIntersect") {
        CHECK_MESSAGE(upper.intersects(inside), upper, " intersects ", inside);
        CHECK_MESSAGE(upper.interiorsIntersect(inside), upper, " interiorsIntersect ", inside);
        CHECK_MESSAGE(inside.intersects(upper), inside, " intersects ", upper);
        CHECK_MESSAGE(inside.interiorsIntersect(upper), inside, " interiorsIntersect ", upper);
    }

    SUBCASE("leaving ray: intersects but ray interior exits") {
        CHECK_MESSAGE(upper.intersects(leaving), upper, " intersects ", leaving);
        CHECK_MESSAGE(upper.interiorsIntersect(leaving), upper, " interiorsIntersect ", leaving);
        CHECK_MESSAGE(leaving.interiorsIntersect(upper), leaving, " interiorsIntersect ", upper);
    }

    SUBCASE("entering ray: intersects and interiors intersect") {
        CHECK_MESSAGE(upper.intersects(entering), upper, " intersects ", entering);
        CHECK_MESSAGE(upper.interiorsIntersect(entering), upper, " interiorsIntersect ", entering);
        CHECK_MESSAGE(entering.intersects(upper), entering, " intersects ", upper);
        CHECK_MESSAGE(entering.interiorsIntersect(upper), entering, " interiorsIntersect ", upper);
    }

    SUBCASE("outside parallel: no intersection") {
        CHECK_FALSE_MESSAGE(upper.intersects(outside_parallel), upper, " intersects ", outside_parallel);
        CHECK_FALSE_MESSAGE(outside_parallel.intersects(upper), outside_parallel, " intersects ", upper);
    }

    SUBCASE("boundary ray: intersects but interiors do not meet") {
        CHECK_MESSAGE(upper.intersects(boundary), upper, " intersects ", boundary);
        // boundary ray lies on ∂H — its interior is still the half-open set, and
        // the halfplane interior is y > 0; they share only the boundary line.
    }
}

TEST_CASE("Halfplane never separates or crosses a Ray, and vice versa") {
    using Point = pgl::Point<int>;
    using Halfplane = pgl::Halfplane<Point>;
    using Ray = pgl::Ray<Point>;

    const Halfplane upper({0, 0}, {4, 0});
    // A ray that fully passes through the interior
    const Ray cutting({1, -2}, {3, 1});
    const Ray inside({1, 1}, {3, 2});

    CHECK_FALSE_MESSAGE(upper.separates(cutting), upper, " separates ", cutting);
    CHECK_FALSE_MESSAGE(upper.separates(inside), upper, " separates ", inside);
    CHECK_FALSE_MESSAGE(upper.crosses(cutting), upper, " crosses ", cutting);
    CHECK_FALSE_MESSAGE(upper.crosses(inside), upper, " crosses ", inside);

    // A ray never disconnects a half-plane.
    CHECK_FALSE_MESSAGE(cutting.separates(upper), cutting, " separates ", upper);
    CHECK_FALSE_MESSAGE(inside.separates(upper), inside, " separates ", upper);
    CHECK_FALSE_MESSAGE(cutting.crosses(upper), cutting, " crosses ", upper);
    CHECK_FALSE_MESSAGE(inside.crosses(upper), inside, " crosses ", upper);
}

TEST_CASE("Halfplane intersection clips Ray into point, segment, or ray") {
    using Point = pgl::Point<int>;
    using Rational = pgl::Rational<int>;
    using RationalPoint = pgl::Point<Rational>;
    using Halfplane = pgl::Halfplane<Point>;
    using Ray = pgl::Ray<Point>;
    using Segment = pgl::Segment<Point>;
    using RationalRay = pgl::Ray<RationalPoint>;

    const Halfplane upper({0, 0}, {4, 0});

    SUBCASE("ray fully inside: returned unchanged") {
        const auto inside = upper.intersection(Ray({1, 1}, {3, 2}));
        REQUIRE(inside);
        REQUIRE(std::holds_alternative<Ray>(*inside));
        CHECK_MESSAGE(std::get<Ray>(*inside) == Ray({1, 1}, {3, 2}), "inside ray clipped");
    }

    SUBCASE("leaving ray: clipped to segment from source to boundary crossing") {
        const auto leaving = upper.intersection(Ray({1, 1}, {3, -1}));
        REQUIRE(leaving);
        REQUIRE(std::holds_alternative<Segment>(*leaving));
        CHECK_MESSAGE(std::get<Segment>(*leaving) == Segment({1, 1}, {2, 0}), "leaving ray clipped");
    }

    SUBCASE("entering ray: clipped to rational ray starting at boundary") {
        const auto entering = upper.intersection<Rational>(Ray({0, -1}, {3, 1}));
        REQUIRE(entering);
        REQUIRE(std::holds_alternative<RationalRay>(*entering));
        CHECK(std::get<RationalRay>(*entering) ==
              RationalRay(RationalPoint(Rational(3, 2), Rational(0)),
                          RationalPoint(Rational(3), Rational(1))));
    }

    SUBCASE("boundary ray: returned unchanged (lies on ∂H)") {
        const auto boundary = upper.intersection(Ray({1, 0}, {3, 0}));
        REQUIRE(boundary);
        REQUIRE(std::holds_alternative<Ray>(*boundary));
        CHECK(std::get<Ray>(*boundary) == Ray({1, 0}, {3, 0}));
    }

    SUBCASE("source touches boundary, ray points outside: touching Point") {
        const auto touching = upper.intersection(Ray({0, 0}, {1, -1}));
        REQUIRE(touching);
        REQUIRE(std::holds_alternative<Point>(*touching));
        CHECK(std::get<Point>(*touching) == Point(0, 0));
    }

    SUBCASE("ray entirely outside: empty") {
        CHECK_FALSE(upper.intersection(Ray({1, -2}, {3, -3})));
    }
}
