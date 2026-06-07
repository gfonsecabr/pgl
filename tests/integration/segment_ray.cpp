#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

TEST_CASE("Segment separates Ray only when the cut is interior to the ray") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Ray = pgl::Ray<Point>;

    const Segment horizontal({0, 0}, {4, 0});

    SUBCASE("a transversal crossing interior to the ray splits it") {
        // The ray crosses the segment's interior, and the crossing is strictly
        // inside the ray, so removing the segment splits the ray in two.
        const Ray crossing({2, -1}, {2, 3});

        CHECK(horizontal.separates(crossing));
    }

    SUBCASE("a ray leaving from the segment's interior is not separated") {
        // The ray's source sits on the segment's interior and the ray leaves
        // from there, so the segment meets the ray only at the ray's own source.
        // Removing the segment takes a single endpoint and the ray stays
        // connected -- it must NOT be reported as separated.
        const Ray from_interior({2, 0}, {2, 3});

        CHECK(horizontal.contains(from_interior.source()));
        CHECK_FALSE(horizontal.separates(from_interior));
    }

    SUBCASE("a ray leaving from the segment's endpoint is not separated") {
        const Ray from_endpoint({0, 0}, {0, 3});

        CHECK_FALSE(horizontal.separates(from_endpoint));
    }

    SUBCASE("a ray missing the segment is not separated") {
        const Ray missing({5, 1}, {6, 2});

        CHECK_FALSE(horizontal.separates(missing));
    }
}
