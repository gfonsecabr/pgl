#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

// Disk through (-2,0), (2,0), (0,2): the circle of radius 2 around the origin.
namespace {
const pgl::Disk<pgl::Point<int>> disk({-2, 0}, {2, 0}, {0, 2});
}

TEST_CASE("Disk contains a chain iff it contains every vertex") {
    using Point = pgl::Point<int>;
    using Chain = pgl::MonotoneChain<Point>;

    CHECK(disk.contains(Chain({0, 0, 1, 1})));
    CHECK(disk.interiorContains(Chain({0, 0, 1, 1})));
    CHECK_FALSE(disk.contains(Chain({0, 0, 3, 1})));
    // Both vertices on the circle: the chord between them is inside, so the
    // chain is contained but not boundary-contained.
    const Chain chord({0, 2, 2, 0});
    CHECK(disk.contains(chord));
    CHECK_FALSE(disk.boundaryContains(chord));
    CHECK_FALSE(disk.interiorContains(chord));
    CHECK_FALSE(Chain({0, 0, 1, 1}).contains(disk));
}

TEST_CASE("MonotoneChain and Disk intersection predicates, both directions") {
    using Point = pgl::Point<int>;
    using Chain = pgl::MonotoneChain<Point>;

    const Chain through({-4, 0, 4, 0});
    CHECK(through.intersects(disk));
    CHECK(disk.intersects(through));
    CHECK(through.interiorsIntersect(disk));
    CHECK_FALSE(Chain({-4, 3, 4, 3}).intersects(disk));

    // Tangent to the circle at (0,2): boundary touch only.
    const Chain tangent({-3, 2, 3, 2});
    CHECK(tangent.intersects(disk));
    CHECK_FALSE(tangent.interiorsIntersect(disk));
}

TEST_CASE("Disk separates a chain passing through it") {
    using Point = pgl::Point<int>;
    using Chain = pgl::MonotoneChain<Point>;

    CHECK(disk.separates(Chain({-4, 0, 4, 0})));
    CHECK_FALSE(disk.separates(Chain({0, 0, 4, 0})));   // eats an end piece
    CHECK_FALSE(disk.separates(Chain({-4, 3, 4, 3})));
}

TEST_CASE("Chain separates a disk passing through it") {
    using Point = pgl::Point<int>;
    using Chain = pgl::MonotoneChain<Point>;

    // A single edge cutting a full chord through the center.
    CHECK(Chain({-4, 0, 4, 0}).separates(disk));
    // Bending at an interior vertex on the way through.
    CHECK(Chain({-4, 0, 0, 1, 4, 0}).separates(disk));
    // Starting strictly inside leaves a slit.
    CHECK_FALSE(Chain({0, 0, 4, 0}).separates(disk));
    // Missing the disk entirely.
    CHECK_FALSE(Chain({-4, 3, 4, 3}).separates(disk));
    // Tangent at (0,2): a boundary touch removes one boundary point only.
    CHECK_FALSE(Chain({-3, 2, 3, 2}).separates(disk));

    // Each removal disconnects the other, so the shapes cross.
    CHECK(Chain({-4, 0, 4, 0}).crosses(disk));
}

TEST_CASE("MonotoneChain and Disk distance is always double") {
    using Point = pgl::Point<int>;
    using Chain = pgl::MonotoneChain<Point>;

    const Chain right({3, 0, 4, 1});
    CHECK(right.squaredDistance(disk) == doctest::Approx(1.0));
    CHECK(disk.squaredDistance(right) == doctest::Approx(1.0));
    CHECK(Chain({-4, 0, 4, 0}).squaredDistance(disk) == doctest::Approx(0.0));
}
