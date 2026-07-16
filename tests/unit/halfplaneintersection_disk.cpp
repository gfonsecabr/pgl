#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

using Point = pgl::Point<int>;
using Halfplane = pgl::Halfplane<Point>;
using Region = pgl::HalfplaneIntersection<Point>;
using Disk = pgl::Disk<Point>;

namespace {
Region box6() {
    return Region({Halfplane(0, 0, 1, 0), Halfplane(6, 0, 6, 1),
                   Halfplane(6, 6, 5, 6), Halfplane(0, 6, 0, 5)});
}
Region vslab() {
    return Region({Halfplane(0, 1, 0, 0), Halfplane(3, 0, 3, 1)});
}
}  // namespace

TEST_CASE("Region contains a disk") {
    const Region k = box6();
    CHECK(k.contains(Disk(Point(3, 3), 2)));
    CHECK(k.interiorContains(Disk(Point(3, 3), 2)));
    CHECK(k.contains(Disk(Point(3, 3), 3)));       // touches two sides
    CHECK(!k.interiorContains(Disk(Point(3, 3), 3)));
    CHECK(!k.contains(Disk(Point(5, 5), 3)));
}

TEST_CASE("A disk contains a region") {
    const Disk big(Point(3, 3), 10);
    CHECK(big.contains(box6()));
    CHECK(big.interiorContains(box6()));
    CHECK(!big.contains(vslab()));                 // unbounded
    CHECK(!Disk(Point(3, 3), 2).contains(box6())); // too small
}

TEST_CASE("Region intersects a disk") {
    const Region k = box6();
    CHECK(k.intersects(Disk(Point(7, 3), 2)));
    CHECK(k.interiorsIntersect(Disk(Point(7, 3), 2)));
    CHECK(!k.intersects(Disk(Point(10, 3), 2)));
    CHECK(!k.interiorsIntersect(Disk(Point(10, 3), 2)));
}

TEST_CASE("Separation with a disk") {
    // The unbounded vertical slab 0 <= x <= 3 is severed by a disk that spans
    // its full width.
    const Region s = vslab();
    CHECK(Disk(Point(2, 10), 3).separates(s));   // covers x in [-1, 5]
    CHECK(!Disk(Point(1, 10), 1).separates(s));  // covers x in [0, 2], too narrow
    // A disk never separates a bounded box that contains it fully.
    CHECK(!Disk(Point(3, 3), 1).separates(box6()));
}

TEST_CASE("Distance to a disk is double") {
    const Region k = box6();
    // Disk of radius 1 centered at (9,3): gap = (9-6) - 1 = 2, squared 4.
    CHECK(k.squaredDistance<double>(Disk(Point(9, 3), 1)) == doctest::Approx(4.0));
    CHECK(k.squaredDistance<double>(Disk(Point(3, 3), 1)) == doctest::Approx(0.0));
}
