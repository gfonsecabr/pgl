#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <vector>

using Point = pgl::Point<int>;
using PolygonShape = pgl::Polygon<Point>;
using Region = pgl::PolygonWithHoles<Point>;
using RegionSet = pgl::PolygonSet<Point>;

// Two 2x2 squares that never touch, and a 10x10 square with a 2x2 hole.
static RegionSet apart() {
    return RegionSet(std::vector{Region(PolygonShape({0, 0, 2, 0, 2, 2, 0, 2})),
                                 Region(PolygonShape({5, 5, 7, 5, 7, 7, 5, 7}))});
}

static RegionSet holed() {
    return RegionSet{Region(PolygonShape({0, 0, 10, 0, 10, 10, 0, 10}),
                            std::vector{PolygonShape({2, 2, 4, 2, 4, 4, 2, 4})})};
}

TEST_CASE("PolygonSet and Point containment") {
    const RegionSet set = apart();

    SUBCASE("a point is in the set when some component holds it") {
        CHECK(set.contains(Point(1, 1)));
        CHECK(set.contains(Point(6, 6)));
        CHECK(!set.contains(Point(3, 3)));
    }

    SUBCASE("the boundary of a component is the boundary of the set") {
        CHECK(set.contains(Point(0, 1)));
        CHECK(set.boundaryContains(Point(0, 1)));
        CHECK(!set.interiorContains(Point(0, 1)));
        CHECK(set.interiorContains(Point(1, 1)));
        CHECK(!set.boundaryContains(Point(1, 1)));
    }

    SUBCASE("a hole is outside the set") {
        const RegionSet withHole = holed();
        CHECK(!withHole.interiorContains(Point(3, 3)));
        CHECK(withHole.contains(Point(2, 2)));       // on the hole ring
        CHECK(withHole.boundaryContains(Point(3, 2)));
        CHECK(!withHole.contains(Point(3, 3)));
    }

    SUBCASE("the empty set holds nothing") {
        CHECK(!RegionSet().contains(Point(0, 0)));
        CHECK(!RegionSet().intersects(Point(0, 0)));
    }
}

TEST_CASE("PolygonSet and Point intersection predicates") {
    const RegionSet set = apart();

    SUBCASE("intersects agrees with contains") {
        CHECK(set.intersects(Point(1, 1)));
        CHECK(set.intersects(Point(0, 0)));
        CHECK(!set.intersects(Point(3, 3)));
    }

    SUBCASE("a point has no interior") {
        CHECK(!set.interiorsIntersect(Point(1, 1)));
    }

    SUBCASE("the pair answers the same either way round") {
        CHECK(Point(1, 1).intersects(set));
        CHECK(!Point(3, 3).intersects(set));
        CHECK(!Point(1, 1).interiorsIntersect(set));
    }
}

TEST_CASE("PolygonSet and Point cut predicates") {
    SUBCASE("a set never cuts a point") {
        CHECK(!apart().separates(Point(1, 1)));
        CHECK(!apart().crosses(Point(1, 1)));
    }

    SUBCASE("a point cuts a set that was already in two pieces") {
        // `A.separates(B)` asks whether `B ∖ A` is disconnected, and a set of
        // separated components is the first shape in the library for which that
        // holds without the remover doing anything.
        CHECK(Point(100, 100).separates(apart()));
        CHECK(!Point(100, 100).separates(holed()));
    }

    SUBCASE("a point cuts a set held together only by a pinch") {
        // Two squares meeting at a corner are one connected piece, and the
        // corner is all that joins them — so removing it does cut the set. This
        // is what no single point can do to a PolygonWithHoles, whose rings
        // always lead around a pinch.
        const RegionSet pinched(std::vector{Region(PolygonShape({-1, -1, 0, -1, 0, 0, -1, 0})),
                                            Region(PolygonShape({0, 0, 1, 0, 1, 1, 0, 1}))});
        REQUIRE(pinched.isConnected());
        CHECK(Point(0, 0).separates(pinched));
        CHECK(!Point(0, 1).separates(pinched));
    }
}

TEST_CASE("PolygonSet and Point distances") {
    const RegionSet set = apart();

    SUBCASE("the distance to a union is the smallest over the components") {
        CHECK(set.squaredDistance<double>(Point(3, 3)) == doctest::Approx(2.0));
        CHECK(set.distanceL1<double>(Point(3, 3)) == doctest::Approx(2.0));
        CHECK(set.distanceLInf<double>(Point(3, 3)) == doctest::Approx(1.0));
    }

    SUBCASE("a point in the set is at distance zero") {
        CHECK(set.squaredDistance<double>(Point(1, 1)) == doctest::Approx(0.0));
    }

    SUBCASE("the nearer component wins") {
        CHECK(set.squaredDistance<double>(Point(4, 4)) == doctest::Approx(2.0));
        CHECK(set.squaredDistance<double>(Point(-2, 1)) == doctest::Approx(4.0));
    }

    SUBCASE("the pair answers the same either way round") {
        CHECK(Point(3, 3).squaredDistance<double>(set) == doctest::Approx(2.0));
        CHECK(Point(3, 3).distanceL1<double>(set) == doctest::Approx(2.0));
        CHECK(Point(3, 3).distanceLInf<double>(set) == doctest::Approx(1.0));
    }
}
