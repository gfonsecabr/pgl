#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <vector>

using Point = pgl::Point<int>;
using Halfplane = pgl::Halfplane<Point>;
using Region = pgl::HalfplaneIntersection<Point>;

namespace {
// A file-scope `using Polygon = pgl::Polygon<Point>;` would collide with the
// Win32 GDI `Polygon` function that <windows.h> injects into the global
// namespace under MSVC (pulled in transitively by doctest.h). Keeping the
// alias in this anonymous namespace shadows it instead of redefining it.
using Polygon = pgl::Polygon<Point>;

Region box6() {
    return Region({Halfplane(0, 0, 1, 0), Halfplane(6, 0, 6, 1),
                   Halfplane(6, 6, 5, 6), Halfplane(0, 6, 0, 5)});
}
Region vslab() {
    return Region({Halfplane(0, 1, 0, 0), Halfplane(3, 0, 3, 1)});
}
Polygon square(int lo, int hi) {
    return Polygon(std::vector<Point>{{lo, lo}, {hi, lo}, {hi, hi}, {lo, hi}});
}
// A reflex, C-shaped polygon opening to the right.
Polygon cShape() {
    return Polygon(std::vector<Point>{{0, 0}, {5, 0}, {5, 2}, {2, 2},
                                      {2, 4}, {5, 4}, {5, 6}, {0, 6}});
}
}  // namespace

TEST_CASE("Region contains a polygon") {
    const Region k = box6();
    CHECK(k.contains(square(1, 3)));
    CHECK(k.interiorContains(square(1, 3)));
    CHECK(k.contains(cShape()));
    CHECK(!k.contains(square(4, 8)));
}

TEST_CASE("A polygon contains a region") {
    CHECK(square(-1, 7).contains(box6()));
    CHECK(!square(-1, 7).contains(vslab()));
    // The reflex notch of the C excludes a box that reaches into it.
    CHECK(!cShape().contains(box6()));
}

TEST_CASE("Region intersects a polygon") {
    const Region k = box6();
    CHECK(k.intersects(square(3, 9)));
    CHECK(k.interiorsIntersect(square(3, 9)));
    CHECK(!k.intersects(square(8, 10)));
}

TEST_CASE("Separation and crossing with a polygon") {
    const Region k = box6();
    const Polygon band(std::vector<Point>{{-1, 2}, {7, 2}, {7, 4}, {-1, 4}});
    CHECK(band.separates(k));
    CHECK(k.separates(band));
    CHECK(k.crosses(band));
    CHECK(!square(2, 4).separates(k));
}

TEST_CASE("An unbounded slab is cut by a spanning polygon") {
    const Region s = vslab();
    const Polygon spanning(std::vector<Point>{{-1, 8}, {4, 8}, {4, 10}, {-1, 10}});
    CHECK(spanning.separates(s));
    const Polygon narrow(std::vector<Point>{{0, 8}, {1, 8}, {1, 10}, {0, 10}});
    CHECK(!narrow.separates(s));
}

TEST_CASE("Distance to a polygon") {
    const Region k = box6();
    // A square three units to the right of the box, overlapping it in y.
    const Polygon right(std::vector<Point>{{9, 2}, {11, 2}, {11, 4}, {9, 4}});
    CHECK(k.squaredDistance<double>(right) == doctest::Approx(9.0));
    CHECK(k.distanceL1<double>(right) == doctest::Approx(3.0));
    CHECK(k.squaredDistance<double>(square(2, 4)) == doctest::Approx(0.0));
}
