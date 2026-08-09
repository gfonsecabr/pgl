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

static RegionSet cornerToCorner() {
    return RegionSet(std::vector{Region(PolygonShape({-1, -1, 0, -1, 0, 0, -1, 0})),
                                 Region(PolygonShape({0, 0, 1, 0, 1, 1, 0, 1}))});
}

TEST_CASE("PolygonSet and Polygon containment") {
    const RegionSet set = apart();

    SUBCASE("an operand with area lies in one component or in none") {
        CHECK(set.contains(PolygonShape({0, 0, 1, 0, 1, 1, 0, 1})));
        CHECK(set.contains(PolygonShape({5, 5, 6, 5, 6, 6, 5, 6})));
        CHECK(!set.contains(PolygonShape({1, 1, 6, 1, 6, 6, 1, 6})));
    }

    SUBCASE("an operand with area is never shared across a pinch") {
        // Unlike a segment, a shape with area cannot be covered by two
        // components at once: they would have to share a stretch of edge, which
        // the invariant rules out.
        const RegionSet pinched = cornerToCorner();
        CHECK(!pinched.contains(PolygonShape({-1, -1, 1, -1, 1, 1, -1, 1})));
        CHECK(pinched.contains(PolygonShape({-1, -1, 0, -1, 0, 0, -1, 0})));
    }

    SUBCASE("a collapsed operand goes through its carrier") {
        // A polygon with no area is exactly a segment, and that segment may run
        // across a pinch even though no component holds it.
        const RegionSet pinched = cornerToCorner();
        CHECK(pinched.contains(PolygonShape({-1, -1, 0, 0, 1, 1})));
    }

    SUBCASE("interior containment") {
        CHECK(set.interiorContains(PolygonShape({0, 0, 1, 0, 1, 1, 0, 1})) == false);
        CHECK(set.interiorContains(pgl::Rectangle<Point>(Point(0, 0), Point(2, 2))) == false);
        CHECK(RegionSet{Region(PolygonShape({0, 0, 10, 0, 10, 10, 0, 10}))}.interiorContains(
            PolygonShape({1, 1, 2, 1, 2, 2, 1, 2})));
    }

    SUBCASE("boundary containment holds only for a collapsed operand") {
        CHECK(!set.boundaryContains(PolygonShape({0, 0, 1, 0, 1, 1, 0, 1})));
        CHECK(set.boundaryContains(PolygonShape({0, 0, 1, 0, 2, 0})));
    }
}

TEST_CASE("PolygonSet and area-shape intersection predicates") {
    const RegionSet set = apart();

    SUBCASE("a shape meets the set when it meets a component") {
        CHECK(set.intersects(PolygonShape({1, 1, 6, 1, 6, 6, 1, 6})));
        CHECK(!set.intersects(PolygonShape({3, 3, 4, 3, 4, 4, 3, 4})));
    }

    SUBCASE("interiors") {
        CHECK(set.interiorsIntersect(pgl::Rectangle<Point>(Point(1, 1), Point(6, 6))));
        CHECK(!set.interiorsIntersect(pgl::Rectangle<Point>(Point(2, 2), Point(5, 5))));
    }

    SUBCASE("every area shape reaches the same answer") {
        CHECK(set.intersects(pgl::Triangle<Point>(Point(0, 0), Point(1, 0), Point(0, 1))));
        CHECK(set.intersects(pgl::Convex<Point>({0, 0, 1, 0, 1, 1, 0, 1})));
        CHECK(set.intersects(pgl::Rectangle<Point>(Point(0, 0), Point(1, 1))));
        CHECK(set.intersects(pgl::Disk<Point>(Point(0, 0), Point(2, 0), Point(1, 1))));
        CHECK(set.intersects(pgl::Halfplane<Point>(Point(0, 0), Point(1, 0))));
    }

    SUBCASE("the pair answers the same either way round") {
        const PolygonShape polygon({1, 1, 6, 1, 6, 6, 1, 6});
        CHECK(polygon.intersects(set));
        CHECK(polygon.interiorsIntersect(set));
        CHECK(!polygon.contains(set));
        CHECK(PolygonShape({-1, -1, 8, -1, 8, 8, -1, 8}).contains(set));
    }
}

TEST_CASE("PolygonSet and Polygon cut predicates") {
    SUBCASE("a set cuts a polygon it runs across") {
        const RegionSet bar{Region(PolygonShape({-1, 1, 4, 1, 4, 2, -1, 2}))};
        const PolygonShape square({0, 0, 3, 0, 3, 3, 0, 3});
        CHECK(bar.separates(square));
        CHECK(square.separates(bar) == bar.crosses(square));
    }

    SUBCASE("a polygon cuts a set that was already in two pieces") {
        CHECK(PolygonShape({100, 100, 101, 100, 101, 101, 100, 101}).separates(apart()));
    }

    SUBCASE("an unbounded remover reaches the set through its clip") {
        const RegionSet bar{Region(PolygonShape({0, 0, 4, 0, 4, 4, 0, 4}))};
        CHECK(!pgl::Line<Point>(Point(-1, 10), Point(1, 10)).separates(bar));
        CHECK(pgl::Line<Point>(Point(-1, 2), Point(1, 2)).separates(bar));
    }
}

TEST_CASE("PolygonSet against itself") {
    const RegionSet set = apart();

    SUBCASE("a set contains itself and meets itself") {
        CHECK(set.contains(set));
        CHECK(set.intersects(set));
        CHECK(set.interiorsIntersect(set));
        CHECK(!set.boundaryContains(set));
    }

    SUBCASE("a subset is contained") {
        const RegionSet part{set.component(0)};
        CHECK(set.contains(part));
        CHECK(!part.contains(set));
        CHECK(part.intersects(set));
    }

    SUBCASE("disjoint sets miss each other") {
        const RegionSet elsewhere{Region(PolygonShape({20, 20, 22, 20, 22, 22, 20, 22}))};
        CHECK(!set.intersects(elsewhere));
        CHECK(!set.contains(elsewhere));
        CHECK(set.squaredDistance<double>(elsewhere) == doctest::Approx(13.0 * 13.0 * 2.0));
    }

    SUBCASE("a set cuts another that it runs across") {
        const RegionSet bar{Region(PolygonShape({-1, 1, 8, 1, 8, 2, -1, 2}))};
        CHECK(bar.separates(set));
        CHECK(!set.separates(RegionSet{Region(PolygonShape({100, 100, 102, 100, 102, 102, 100, 102}))}));
    }

    SUBCASE("the empty set is contained in everything and meets nothing") {
        CHECK(set.contains(RegionSet()));
        CHECK(!set.intersects(RegionSet()));
        CHECK(!set.separates(RegionSet()));
        // Removing nothing still leaves a set that was already in two pieces in
        // two pieces, which is what `B ∖ A is disconnected` asks.
        CHECK(RegionSet().separates(set));
        CHECK(!RegionSet().separates(RegionSet{set.component(0)}));
    }
}
