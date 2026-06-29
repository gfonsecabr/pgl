#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <variant>

// intersects / interiorsIntersect, both directions (the Segment side forwards to
// the Ray implementation by symmetry). The ray is x >= 0 along y = 0.
TEST_CASE("Segment and Ray intersection predicates") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Ray = pgl::Ray<Point>;

    const Ray r({0, 0}, {1, 0});

    SUBCASE("a transversal crossing ahead of the source meets, interiors too") {
        const Segment cross({2, -1}, {2, 1});  // crosses the ray at (2,0)
        CHECK(r.intersects(cross));
        CHECK(cross.intersects(r));
        CHECK(r.interiorsIntersect(cross));
        CHECK(cross.interiorsIntersect(r));
    }

    SUBCASE("a collinear segment ahead of the source overlaps it") {
        const Segment along({1, 0}, {3, 0});
        CHECK(r.intersects(along));
        CHECK(along.intersects(r));
        CHECK(r.interiorsIntersect(along));
        CHECK(along.interiorsIntersect(r));
    }

    SUBCASE("a segment touching only the ray's source grazes the boundary") {
        const Segment touch({0, 0}, {0, 3});  // endpoint at the source
        CHECK(r.intersects(touch));
        CHECK_FALSE(r.interiorsIntersect(touch));  // meets only at the source
        CHECK_FALSE(touch.interiorsIntersect(r));
    }

    SUBCASE("a segment behind the source misses the ray") {
        const Segment behind({-3, -1}, {-3, 1});
        CHECK_FALSE(r.intersects(behind));
        CHECK_FALSE(behind.intersects(r));
        CHECK_FALSE(r.interiorsIntersect(behind));
        CHECK_FALSE(behind.interiorsIntersect(r));
    }
}

// interiorContains: only the Ray can interior-contain a Segment (a finite
// segment never contains an infinite ray). The ray's interior excludes its
// source, so an endpoint at the source breaks containment.
TEST_CASE("Ray interior-contains a Segment strictly ahead of its source") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Ray = pgl::Ray<Point>;

    const Ray r({0, 0}, {1, 0});

    SUBCASE("a collinear segment strictly ahead is interior-contained") {
        const Segment along({1, 0}, {3, 0});
        CHECK(r.contains(along));
        CHECK(r.interiorContains(along));
    }

    SUBCASE("a segment reaching the source is contained but not interior") {
        const Segment fromSource({0, 0}, {3, 0});
        CHECK(r.contains(fromSource));
        CHECK_FALSE(r.interiorContains(fromSource));  // the source is on ∂(ray)
    }

    SUBCASE("a transversal segment is not contained") {
        const Segment cross({2, -1}, {2, 1});
        CHECK_FALSE(r.contains(cross));
        CHECK_FALSE(r.interiorContains(cross));
    }
}

// crosses requires mutual separation: a transversal cut interior to both
// crosses, a collinear overlap does not.
TEST_CASE("Segment and Ray cross only on a transversal interior cut") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Ray = pgl::Ray<Point>;

    const Ray r({0, 0}, {1, 0});

    SUBCASE("a transversal crossing ahead of the source crosses both ways") {
        const Segment cross({2, -1}, {2, 1});
        CHECK(r.crosses(cross));
        CHECK(cross.crosses(r));
    }

    SUBCASE("a collinear overlap does not cross") {
        const Segment along({1, 0}, {3, 0});
        CHECK_FALSE(r.crosses(along));
        CHECK_FALSE(along.crosses(r));
    }
}

// intersection returns a Point for a transversal cut, the overlap Segment for a
// collinear one, and nothing when disjoint. Both directions agree.
TEST_CASE("Segment and Ray intersection construction") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Ray = pgl::Ray<Point>;
    using Piece = std::variant<Point, Segment>;

    const Ray r({0, 0}, {1, 0});

    SUBCASE("a transversal cut yields the crossing point") {
        const Segment cross({2, -1}, {2, 1});
        const auto a = r.intersection(cross);
        REQUIRE(a.has_value());
        CHECK(*a == Piece(Point(2, 0)));
        CHECK(cross.intersection(r) == a);
    }

    SUBCASE("a collinear overlap yields the shared segment") {
        const Segment along({1, 0}, {3, 0});
        const auto a = r.intersection(along);
        REQUIRE(a.has_value());
        CHECK(*a == Piece(Segment({1, 0}, {3, 0})));
        CHECK(along.intersection(r) == a);
    }

    SUBCASE("a disjoint segment yields nothing") {
        const Segment behind({-3, -1}, {-3, 1});
        CHECK_FALSE(r.intersection(behind).has_value());
        CHECK_FALSE(behind.intersection(r).has_value());
    }
}

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
