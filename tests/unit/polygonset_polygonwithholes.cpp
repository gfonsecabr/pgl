#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <vector>

using Point = pgl::Point<int>;
using PolygonShape = pgl::Polygon<Point>;
using Region = pgl::PolygonWithHoles<Point>;
using RegionSet = pgl::PolygonSet<Point>;

static RegionSet apart() {
    return RegionSet(std::vector{Region(PolygonShape({0, 0, 2, 0, 2, 2, 0, 2})),
                                 Region(PolygonShape({5, 5, 7, 5, 7, 7, 5, 7}))});
}

TEST_CASE("PolygonSet and PolygonWithHoles predicates") {
    const RegionSet set = apart();
    const Region big(PolygonShape({-1, -1, 8, -1, 8, 8, -1, 8}));

    SUBCASE("containment either way round") {
        CHECK(big.contains(set));
        CHECK(!set.contains(big));
        CHECK(big.interiorContains(set));
    }

    SUBCASE("a region with a hole around a component does not contain it") {
        const Region holed(PolygonShape({-1, -1, 8, -1, 8, 8, -1, 8}),
                           std::vector{PolygonShape({4, 4, 8, 4, 8, 8, 4, 8})});
        CHECK(!holed.contains(set));
        CHECK(holed.intersects(set));
    }

    SUBCASE("distances fold to the nearest pair") {
        const Region distant(PolygonShape({20, 20, 22, 20, 22, 22, 20, 22}));
        CHECK(set.squaredDistance<double>(distant) == doctest::Approx(13.0 * 13.0 * 2.0));
        CHECK(distant.squaredDistance<double>(set) == doctest::Approx(13.0 * 13.0 * 2.0));
    }
}

TEST_CASE("PolygonSet and PolygonWithHoles cut predicates") {
    SUBCASE("a region cuts a set it runs across") {
        const Region bar(PolygonShape({-1, 1, 8, 1, 8, 2, -1, 2}));
        CHECK(bar.separates(apart()));
    }

    SUBCASE("a set cuts a region it runs across") {
        const RegionSet bar{Region(PolygonShape({-1, 1, 4, 1, 4, 2, -1, 2}))};
        const Region square(PolygonShape({0, 0, 3, 0, 3, 3, 0, 3}));
        CHECK(bar.separates(square));
        CHECK(bar.crosses(square) == square.separates(bar));
    }

    SUBCASE("a region that misses a set in one piece cuts nothing") {
        const Region distant(PolygonShape({20, 20, 22, 20, 22, 22, 20, 22}));
        CHECK(!distant.separates(RegionSet{Region(PolygonShape({0, 0, 2, 0, 2, 2, 0, 2}))}));
        // But a set already in two pieces stays in two.
        CHECK(distant.separates(apart()));
    }
}

// A region whose hole reaches its outer boundary is connected but its interior
// is not, so its pieces may go to different components and no single component
// holds the whole of it.
TEST_CASE("PolygonSet contains a region pinched apart at points") {
    // The square [0,2]² minus a diamond touching all four sides, against the
    // four corner triangles it falls into: the same point set, twice.
    const Region pinched(PolygonShape({0, 0, 2, 0, 2, 2, 0, 2}),
                         std::vector{PolygonShape({0, 1, 1, 2, 2, 1, 1, 0})});
    const RegionSet corners(std::vector{Region(PolygonShape({0, 0, 1, 0, 0, 1})),
                                        Region(PolygonShape({1, 0, 2, 0, 2, 1})),
                                        Region(PolygonShape({2, 1, 2, 2, 1, 2})),
                                        Region(PolygonShape({0, 1, 1, 2, 0, 2}))});
    REQUIRE(pinched.isValid());
    REQUIRE(corners.isValid());

    SUBCASE("containment holds both ways round, as equality demands") {
        CHECK(pinched.contains(corners));
        CHECK(corners.contains(pinched));
        CHECK(corners.samePointSet(pinched));
        CHECK(corners.samePointSet(pinched) ==
              (corners.contains(pinched) && pinched.contains(corners)));
    }

    SUBCASE("a corner left out leaves the region uncovered") {
        const RegionSet three(std::vector{Region(PolygonShape({0, 0, 1, 0, 0, 1})),
                                          Region(PolygonShape({1, 0, 2, 0, 2, 1})),
                                          Region(PolygonShape({2, 1, 2, 2, 1, 2}))});
        CHECK(!three.contains(pinched));
        CHECK(!three.samePointSet(pinched));
    }

    SUBCASE("the pieces reach the set through a set operand too") {
        const RegionSet asSet{pinched};
        CHECK(corners.contains(asSet));
        CHECK(asSet.contains(corners));
    }

    SUBCASE("a region with no area is its boundary and is shared just as well") {
        // Outer ring and hole coincide: the point set is the diamond's boundary,
        // which is the four hypotenuses, one per component.
        const Region diamondRing(PolygonShape({0, 1, 1, 2, 2, 1, 1, 0}),
                                 std::vector{PolygonShape({0, 1, 1, 2, 2, 1, 1, 0})});
        REQUIRE(diamondRing.isDegenerate());
        CHECK(corners.contains(diamondRing));
    }

    SUBCASE("components that stay apart still answer componentwise") {
        // Two of the four corners, and the two that never touch: a connected
        // operand no single component holds is held by no set like this one.
        const RegionSet opposite(std::vector{Region(PolygonShape({0, 0, 1, 0, 0, 1})),
                                             Region(PolygonShape({2, 1, 2, 2, 1, 2}))});
        REQUIRE(!opposite.isPinched());
        CHECK(!opposite.contains(pinched));
    }
}

TEST_CASE("PolygonSet does not contain a region it only surrounds") {
    // Four triangles hung off the sides of the square [1,3]², meeting only at
    // its corners: the set is pinched and covers the square's whole boundary,
    // but none of its interior.
    const RegionSet ring(std::vector{Region(PolygonShape({1, 1, 3, 1, 2, -1})),
                                     Region(PolygonShape({3, 1, 3, 3, 5, 2})),
                                     Region(PolygonShape({1, 3, 3, 3, 2, 5})),
                                     Region(PolygonShape({1, 1, 1, 3, -1, 2}))});
    const Region middle(PolygonShape({1, 1, 3, 1, 3, 3, 1, 3}));
    REQUIRE(ring.isValid());
    REQUIRE(ring.isPinched());

    CHECK(ring.contains(middle.outer().edges()[0]));  // the boundary is covered
    CHECK(!ring.contains(middle));
    CHECK(!ring.samePointSet(middle));

    SUBCASE("nor one whose interior a component's hole reaches into") {
        // A big square with a notch bitten out of it, plus a triangle hung off
        // its far corner so the set is pinched and takes the same path.
        const RegionSet notched(
            std::vector{Region(PolygonShape({0, 0, 8, 0, 8, 8, 0, 8}),
                               std::vector{PolygonShape({5, 3, 6, 3, 6, 4, 5, 4})}),
                        Region(PolygonShape({8, 8, 9, 8, 9, 9}))});
        const Region big(PolygonShape({0, 0, 8, 0, 8, 8, 0, 8}));
        REQUIRE(notched.isValid());
        REQUIRE(notched.isPinched());
        CHECK(!notched.contains(big));
    }
}
