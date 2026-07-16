#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <stdexcept>

using Point = pgl::Point<int>;
using Halfplane = pgl::Halfplane<Point>;
using Region = pgl::HalfplaneIntersection<Point>;
using ERational = pgl::Rational<long long>;

namespace {
Region box6() {
    return Region({Halfplane(0, 0, 1, 0), Halfplane(6, 0, 6, 1),
                   Halfplane(6, 6, 5, 6), Halfplane(0, 6, 0, 5)});
}
Region triangleRegion() {
    // The triangle (0,0),(4,0),(0,4).
    return Region({Halfplane(0, 0, 4, 0), Halfplane(4, 0, 0, 4), Halfplane(0, 4, 0, 0)});
}
Region quadrant() {
    return Region({Halfplane(0, 0, 1, 0), Halfplane(0, 1, 0, 0)});  // x>=0, y>=0
}
}  // namespace

TEST_CASE("Area of a bounded region") {
    CHECK(box6().twiceArea<long long>() == 72);
    CHECK(box6().area<double>() == doctest::Approx(36.0));
    CHECK(triangleRegion().twiceArea<long long>() == 16);
    CHECK(triangleRegion().area<ERational>() == ERational(8));
}

TEST_CASE("Empty and unbounded measures") {
    Region empty({Halfplane(0, 0, 1, 0)});
    empty.insert(Halfplane(1, -1, 0, -1));   // y <= -1 makes it empty
    CHECK(empty.isEmpty());
    CHECK(empty.twiceArea<long long>() == 0);
    CHECK(empty.area<double>() == doctest::Approx(0.0));

    CHECK_THROWS_AS((void)quadrant().twiceArea<long long>(), std::logic_error);
    CHECK_THROWS_AS((void)quadrant().area<double>(), std::logic_error);
    CHECK_THROWS_AS((void)quadrant().centroid<double>(), std::logic_error);
}

TEST_CASE("Centroid of a bounded region") {
    const auto c = box6().centroid<double>();
    CHECK(c.x() == doctest::Approx(3.0));
    CHECK(c.y() == doctest::Approx(3.0));
    const auto tc = triangleRegion().centroid<ERational>();
    CHECK(tc.x() == ERational(4, 3));
    CHECK(tc.y() == ERational(4, 3));
}

TEST_CASE("pointInside lands inside the region") {
    const auto pb = box6().pointInside<ERational>();
    CHECK(box6().contains(Point(static_cast<int>(pb.x()), static_cast<int>(pb.y()))));
    // Unbounded regions still return a representative point of the region.
    const auto pq = quadrant().pointInside<ERational>();
    CHECK(quadrant().contains(pq));
    const auto pt = triangleRegion().pointInside<ERational>();
    CHECK(triangleRegion().contains(pt));
}

TEST_CASE("pointInsideInteriorContainedIn witnesses interior containment") {
    // The box's interior witness lies inside a strictly larger box.
    CHECK(box6().pointInsideInteriorContainedIn(pgl::Rectangle<Point>(Point(-1, -1), Point(7, 7))));
    CHECK(!box6().pointInsideInteriorContainedIn(pgl::Rectangle<Point>(Point(10, 10), Point(20, 20))));
}
