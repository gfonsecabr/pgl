#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <vector>

using Point = pgl::Point<int>;
using Halfplane = pgl::Halfplane<Point>;
using Region = pgl::HalfplaneIntersection<Point>;
using Polyline = pgl::Polyline<Point>;

namespace {
Region box6() {
    return Region({Halfplane(0, 0, 1, 0), Halfplane(6, 0, 6, 1),
                   Halfplane(6, 6, 5, 6), Halfplane(0, 6, 0, 5)});
}
}  // namespace

TEST_CASE("Region contains a polyline") {
    const Region k = box6();
    const Polyline inside(std::vector<Point>{{1, 1}, {3, 5}, {5, 1}});
    CHECK(k.contains(inside));
    const Polyline poking(std::vector<Point>{{1, 1}, {3, 8}, {5, 1}});
    CHECK(!k.contains(poking));
}

TEST_CASE("Region intersects a polyline") {
    const Region k = box6();
    const Polyline crossing(std::vector<Point>{{3, 3}, {5, 7}, {9, 3}});
    CHECK(k.intersects(crossing));
    CHECK(k.interiorsIntersect(crossing));
    const Polyline away(std::vector<Point>{{7, 7}, {8, 9}, {9, 7}});
    CHECK(!k.intersects(away));
}

TEST_CASE("Region separates a polyline") {
    const Region k = box6();
    // A polyline entering and leaving the box through one side, with a bend
    // outside, is cut into two surviving pieces.
    const Polyline through(std::vector<Point>{{-2, 3}, {3, 3}, {8, 3}});
    CHECK(k.separates(through));
    const Polyline inside(std::vector<Point>{{1, 3}, {3, 3}, {5, 3}});
    CHECK(!k.separates(inside));
    CHECK(!through.separates(k));
}

TEST_CASE("Distance to a polyline") {
    const Region k = box6();
    const Polyline away(std::vector<Point>{{9, 0}, {10, 6}, {11, 0}});
    CHECK(k.squaredDistance<double>(away) == doctest::Approx(9.0));
    const Polyline touching(std::vector<Point>{{3, 3}, {5, 7}, {9, 3}});
    CHECK(k.squaredDistance<double>(touching) == doctest::Approx(0.0));
}
