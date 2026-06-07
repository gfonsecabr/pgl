#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstdint>
#include <variant>

#include "pgl.hpp"

TEST_CASE("Line and ray predicates exercise the line's infinite extent") {
    using Point = pgl::Point<int>;
    using Ray = pgl::Ray<Point>;
    using Line = pgl::Line<Point>;

    const Line diagonal({0, 0}, {4, 4});

    SUBCASE("ray collinear with the line, source past the defining points") {
        const Ray far_ray({50, 50}, {60, 60});

        CHECK(diagonal.contains(far_ray));
        CHECK(diagonal.interiorContains(far_ray));
        CHECK(diagonal.collinear(far_ray));
        CHECK(far_ray.collinear(diagonal));
        CHECK(diagonal.parallel(far_ray));
        CHECK(far_ray.parallel(diagonal));
        CHECK(diagonal.intersects(far_ray));
        CHECK(far_ray.intersects(diagonal));
        CHECK(diagonal.interiorsIntersect(far_ray));
        CHECK_FALSE(diagonal.crosses(far_ray));
        CHECK_FALSE(diagonal.separates(far_ray));

        const auto isec = diagonal.intersection(far_ray);
        REQUIRE(isec);
        REQUIRE(std::holds_alternative<Ray>(*isec));
        CHECK(std::get<Ray>(*isec) == far_ray);
    }

    SUBCASE("ray collinear with the line but pointing back through it") {
        const Ray reverse_ray({100, 100}, {50, 50});

        CHECK(diagonal.contains(reverse_ray));
        CHECK(diagonal.collinear(reverse_ray));
        CHECK(diagonal.intersects(reverse_ray));

        const auto isec = diagonal.intersection(reverse_ray);
        REQUIRE(isec);
        REQUIRE(std::holds_alternative<Ray>(*isec));
        CHECK(std::get<Ray>(*isec) == reverse_ray);
    }

    SUBCASE("ray crosses the line beyond the defining points") {
        const Ray crossing({100, 90}, {100, 200});

        CHECK_FALSE(diagonal.contains(crossing));
        CHECK_FALSE(diagonal.parallel(crossing));
        CHECK_FALSE(crossing.parallel(diagonal));
        CHECK(diagonal.intersects(crossing));
        CHECK(crossing.intersects(diagonal));
        CHECK(diagonal.interiorsIntersect(crossing));
        CHECK(diagonal.crosses(crossing));
        CHECK(crossing.crosses(diagonal));
        CHECK(diagonal.separates(crossing));

        const auto isec = diagonal.intersection(crossing);
        REQUIRE(isec);
        REQUIRE(std::holds_alternative<Point>(*isec));
        CHECK(std::get<Point>(*isec) == Point(100, 100));
    }

    SUBCASE("ray source sits on the line far past the defining points and ray points away") {
        const Ray leaving({80, 80}, {80, 200});

        CHECK_FALSE(diagonal.contains(leaving));
        CHECK(diagonal.intersects(leaving));
        CHECK(leaving.intersects(diagonal));
        CHECK_FALSE(diagonal.interiorsIntersect(leaving));
        CHECK_FALSE(diagonal.crosses(leaving));
        CHECK_FALSE(diagonal.separates(leaving));

        const auto isec = diagonal.intersection(leaving);
        REQUIRE(isec);
        REQUIRE(std::holds_alternative<Point>(*isec));
        CHECK(std::get<Point>(*isec) == Point(80, 80));
    }

    SUBCASE("ray points away from the line and never reaches it") {
        const Ray away({100, 105}, {100, 200});

        CHECK_FALSE(diagonal.contains(away));
        CHECK_FALSE(diagonal.intersects(away));
        CHECK_FALSE(away.intersects(diagonal));
        CHECK_FALSE(diagonal.interiorsIntersect(away));
        CHECK_FALSE(diagonal.crosses(away));
        CHECK_FALSE(diagonal.separates(away));
        CHECK_FALSE(diagonal.intersection(away));
    }

    SUBCASE("ray parallel to the line never reaches it even at infinity") {
        const Ray parallel_ray({-100, -99}, {100, 101});

        CHECK_FALSE(diagonal.contains(parallel_ray));
        CHECK(diagonal.parallel(parallel_ray));
        CHECK_FALSE(diagonal.collinear(parallel_ray));
        CHECK_FALSE(diagonal.intersects(parallel_ray));
        CHECK_FALSE(diagonal.interiorsIntersect(parallel_ray));
        CHECK_FALSE(diagonal.crosses(parallel_ray));
        CHECK_FALSE(diagonal.intersection(parallel_ray));
    }
}

TEST_CASE("Vertical line crossed by a ray pointing back from far away") {
    using Point = pgl::Point<int>;
    using Ray = pgl::Ray<Point>;
    using Line = pgl::Line<Point>;

    const Line vertical({2, 0}, {2, 1});
    const Ray inbound({500, 500}, {1, 500});

    CHECK(vertical.isVertical());
    CHECK(vertical.intersects(inbound));
    CHECK(vertical.crosses(inbound));

    const auto isec = vertical.intersection(inbound);
    REQUIRE(isec);
    REQUIRE(std::holds_alternative<Point>(*isec));
    CHECK(std::get<Point>(*isec) == Point(2, 500));
}

TEST_CASE("Horizontal line, ray source past defining points, sliding along the line") {
    using Point = pgl::Point<int>;
    using Ray = pgl::Ray<Point>;
    using Line = pgl::Line<Point>;

    const Line horizontal({0, 5}, {1, 5});
    const Ray sliding({999, 5}, {2000, 5});

    CHECK(horizontal.isHorizontal());
    CHECK(horizontal.contains(sliding));
    CHECK(horizontal.collinear(sliding));
    CHECK(horizontal.intersects(sliding));
    CHECK_FALSE(horizontal.crosses(sliding));

    const auto isec = horizontal.intersection(sliding);
    REQUIRE(isec);
    REQUIRE(std::holds_alternative<Ray>(*isec));
    CHECK(std::get<Ray>(*isec) == sliding);
}

TEST_CASE("Rational crossing of a ray far past the line's defining points") {
    using Rational = pgl::Rational<int64_t>;
    using IntPoint = pgl::Point<int>;
    using RationalPoint = pgl::Point<Rational>;
    using IntRay = pgl::Ray<IntPoint>;
    using IntLine = pgl::Line<IntPoint>;

    const IntLine line({0, 0}, {2, 1});
    const IntRay ray({100, 60}, {100, -50});

    CHECK(line.intersects(ray));
    CHECK(line.crosses(ray));

    const auto isec = line.intersection<Rational>(ray);
    REQUIRE(isec);
    REQUIRE(std::holds_alternative<RationalPoint>(*isec));
    CHECK(std::get<RationalPoint>(*isec) == RationalPoint(Rational(100), Rational(50)));
}
