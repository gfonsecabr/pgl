#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

using Point = pgl::Point<int>;
using Halfplane = pgl::Halfplane<Point>;
using Region = pgl::HalfplaneIntersection<Point>;
using ERational = pgl::Rational<long long>;
// Named RectangleShape, not Rectangle: under MSVC, <windows.h> (pulled in
// transitively by doctest.h) injects a Win32 GDI function called `Rectangle`
// into the global namespace, and an alias of the same name used from
// TEST_CASE bodies (global scope) resolves ambiguously against it.
using RectangleShape = pgl::Rectangle<Point>;

namespace {
// Axis-aligned box [0,6] x [0,6].
Region box6() {
    return Region({Halfplane(0, 0, 1, 0), Halfplane(6, 0, 6, 1),
                   Halfplane(6, 6, 5, 6), Halfplane(0, 6, 0, 5)});
}
// Vertical slab 0 <= x <= 3, unbounded in y.
Region vslab() {
    return Region({Halfplane(0, 1, 0, 0), Halfplane(3, 0, 3, 1)});
}
}  // namespace

TEST_CASE("Region contains a rectangle") {
    const Region k = box6();
    CHECK(k.contains(RectangleShape(Point(1, 1), Point(3, 3))));
    CHECK(k.interiorContains(RectangleShape(Point(1, 1), Point(3, 3))));
    CHECK(k.contains(RectangleShape(Point(0, 0), Point(6, 6))));      // touches boundary
    CHECK(!k.interiorContains(RectangleShape(Point(0, 0), Point(6, 6))));
    CHECK(!k.contains(RectangleShape(Point(4, 4), Point(8, 8))));     // pokes out
}

TEST_CASE("A rectangle contains a region") {
    const RectangleShape r(Point(-1, -1), Point(7, 7));
    CHECK(r.contains(box6()));
    CHECK(r.interiorContains(box6()));
    CHECK(!r.contains(vslab()));   // the slab is unbounded, no bounded box holds it
    const RectangleShape exact(Point(0, 0), Point(6, 6));
    CHECK(exact.contains(box6()));
    CHECK(!exact.interiorContains(box6()));  // shared boundary
}

TEST_CASE("Region intersects and interior-intersects a rectangle") {
    const Region k = box6();
    CHECK(k.intersects(RectangleShape(Point(3, 3), Point(9, 9))));
    CHECK(k.interiorsIntersect(RectangleShape(Point(3, 3), Point(9, 9))));
    CHECK(k.intersects(RectangleShape(Point(6, 6), Point(9, 9))));       // corner touch
    CHECK(!k.interiorsIntersect(RectangleShape(Point(6, 6), Point(9, 9))));
    CHECK(!k.intersects(RectangleShape(Point(7, 7), Point(9, 9))));
}

TEST_CASE("Intersection with a rectangle stays a region") {
    const Region k = box6();
    const Region clip = k.intersection(RectangleShape(Point(1, 1), Point(3, 3)));
    CHECK(clip.isBounded());
    CHECK(clip.twiceArea<long long>() == 8);   // 2 x 2 box
    const Region miss = k.intersection(RectangleShape(Point(10, 10), Point(12, 12)));
    CHECK(miss.isEmpty());
}

TEST_CASE("Separation and crossing with a rectangle") {
    const Region k = box6();
    // A band crossing the full width cuts the box into two, and the box cuts
    // the band into two side strips.
    const RectangleShape band(Point(-1, 2), Point(7, 4));
    CHECK(band.separates(k));
    CHECK(k.separates(band));
    CHECK(k.crosses(band));
    // An interior rectangle severs nothing.
    const RectangleShape inside(Point(2, 2), Point(4, 4));
    CHECK(!inside.separates(k));
    CHECK(!k.separates(inside));
}

TEST_CASE("An unbounded slab is cut by a spanning rectangle") {
    const Region s = vslab();
    // A rectangle spanning the full width of the slab disconnects it.
    const RectangleShape spanning(Point(-1, 8), Point(4, 10));
    CHECK(spanning.separates(s));
    // A rectangle not reaching across the slab does not.
    const RectangleShape narrow(Point(0, 8), Point(1, 10));
    CHECK(!narrow.separates(s));
}

TEST_CASE("Distance to a rectangle") {
    const Region k = box6();
    // Rectangle three units to the right of the box.
    const RectangleShape r(Point(9, 0), Point(10, 6));
    CHECK(k.squaredDistance<double>(r) == doctest::Approx(9.0));
    CHECK(k.distanceL1<double>(r) == doctest::Approx(3.0));
    CHECK(k.distanceLInf<double>(r) == doctest::Approx(3.0));
    CHECK(k.squaredDistance<double>(RectangleShape(Point(1, 1), Point(2, 2))) == doctest::Approx(0.0));
    // Exact rational distance.
    CHECK(k.squaredDistance<ERational>(r) == ERational(9));
}
