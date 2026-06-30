#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstdint>

#include "pgl.hpp"

// Disk::intersects(Line) / interiorsIntersect(Line) are decided exactly and
// without division. The disk here is the integer circle of radius 5 centred at
// the origin, so a point (x, y) lies in the closed disk iff x*x + y*y <= 25 and
// every expected answer below is checkable by hand. A line meets the closed
// disk iff its distance to the centre is at most the radius (intersects), and
// meets the open disk iff it is a strict secant (interiorsIntersect).

TEST_CASE("Lines missing a disk") {
    using Point = pgl::Point<int>;
    using Line = pgl::Line<Point>;
    using Disk = pgl::Disk<Point>;

    Disk d(0, 0, 5);

    auto offset = GENERATE(6, 7, -6, -8);
    SUBCASE("vertical") {
        Line l(Point(offset, -3), Point(offset, 3));
        CHECK_FALSE_MESSAGE(l.intersects(d), l, " intersects ", d);
        CHECK_FALSE_MESSAGE(d.intersects(l), d, " intersects ", l);
        CHECK_FALSE_MESSAGE(d.interiorsIntersect(l), d, " interiorsIntersect ", l);
    }
    SUBCASE("horizontal") {
        Line l(Point(-3, offset), Point(3, offset));
        CHECK_FALSE_MESSAGE(d.intersects(l), d, " intersects ", l);
        CHECK_FALSE_MESSAGE(d.interiorsIntersect(l), d, " interiorsIntersect ", l);
    }
}

TEST_CASE("Lines tangent to a disk touch the boundary only") {
    using Point = pgl::Point<int>;
    using Line = pgl::Line<Point>;
    using Disk = pgl::Disk<Point>;

    Disk d(0, 0, 5);

    SUBCASE("vertical tangent at (5, 0)") {
        Line l(Point(5, -3), Point(5, 3));
        CHECK_MESSAGE(l.intersects(d), l, " intersects ", d);
        CHECK_MESSAGE(d.intersects(l), d, " intersects ", l);
        CHECK_FALSE_MESSAGE(d.interiorsIntersect(l), d, " interiorsIntersect ", l);
        CHECK_FALSE_MESSAGE(l.interiorsIntersect(d), l, " interiorsIntersect ", d);
    }

    SUBCASE("horizontal tangent at (0, -5)") {
        Line l(Point(-2, -5), Point(7, -5));
        CHECK_MESSAGE(d.intersects(l), d, " intersects ", l);
        CHECK_FALSE_MESSAGE(d.interiorsIntersect(l), d, " interiorsIntersect ", l);
    }
}

TEST_CASE("Lines cutting a disk are strict secants") {
    using Point = pgl::Point<int>;
    using Line = pgl::Line<Point>;
    using Disk = pgl::Disk<Point>;

    Disk d(0, 0, 5);

    SUBCASE("through the centre") {
        Line l(Point(0, 0), Point(2, 1));
        CHECK_MESSAGE(d.intersects(l), d, " intersects ", l);
        CHECK_MESSAGE(d.interiorsIntersect(l), d, " interiorsIntersect ", l);
        CHECK_MESSAGE(l.interiorsIntersect(d), l, " interiorsIntersect ", d);
    }

    SUBCASE("offset chord far from the defining points") {
        // The two defining points sit well outside the disk, yet the infinite
        // line still cuts it.
        Line l(Point(-9, 2), Point(11, 2));
        CHECK_MESSAGE(d.intersects(l), d, " intersects ", l);
        CHECK_MESSAGE(d.interiorsIntersect(l), d, " interiorsIntersect ", l);
    }
}

TEST_CASE("Line against a disk built from three arbitrary boundary points") {
    using Point = pgl::Point<int>;
    using Line = pgl::Line<Point>;
    using Disk = pgl::Disk<Point>;

    // Same radius-5 circle, defined by three lattice points on it.
    Disk d(Point(5, 0), Point(-3, 4), Point(0, -5));

    CHECK_MESSAGE(d.interiorsIntersect(Line(Point(-9, 2), Point(11, 2))), d, " secant");
    CHECK_MESSAGE(d.intersects(Line(Point(5, -3), Point(5, 3))), d, " tangent touches");
    CHECK_FALSE_MESSAGE(d.interiorsIntersect(Line(Point(5, -3), Point(5, 3))), d, " tangent not interior");
    CHECK_FALSE_MESSAGE(d.intersects(Line(Point(6, -3), Point(6, 3))), d, " misses");
}

TEST_CASE("Oriented lines forward to the unoriented predicate") {
    using Point = pgl::Point<int>;
    using Line = pgl::Line<Point>;
    using OrientedLine = pgl::OrientedLine<Point>;
    using Disk = pgl::Disk<Point>;

    Disk d(0, 0, 5);

    // A secant, a tangent, and a miss; each tried in both directions.
    auto a = GENERATE(Point(0, 0), Point(5, -3), Point(6, -3));
    auto b = GENERATE(Point(2, 1), Point(5, 3), Point(6, 3));
    if (a == b) {
        return;
    }
    Line l(a, b);
    OrientedLine forward(a, b);
    OrientedLine backward(b, a);

    CHECK_MESSAGE(d.intersects(forward) == d.intersects(l), d, " intersects ", forward);
    CHECK_MESSAGE(d.intersects(backward) == d.intersects(l), d, " intersects ", backward);
    CHECK_MESSAGE(d.interiorsIntersect(forward) == d.interiorsIntersect(l), d, " ii ", forward);
    CHECK_MESSAGE(d.interiorsIntersect(backward) == d.interiorsIntersect(l), d, " ii ", backward);
}

// Containment: a 1D line cannot hold a 2D disk, and a disk can only "contain" a
// line (or hold one on its circular boundary) in degenerate cases, so for a real
// line/disk pair every containment relation is false.
TEST_CASE("Line and Disk never contain each other") {
    using Point = pgl::Point<int>;
    using Line = pgl::Line<Point>;
    using Disk = pgl::Disk<Point>;

    Disk d(0, 0, 5);
    Line secant(Point(0, 0), Point(2, 1));  // through the centre

    CHECK_FALSE_MESSAGE(d.contains(secant), d, " contains ", secant);
    CHECK_FALSE_MESSAGE(d.boundaryContains(secant), d, " boundaryContains ", secant);
    CHECK_FALSE_MESSAGE(secant.contains(d), secant, " contains ", d);
}

// separates: a secant cuts the disk in two (line.separates(disk)) and likewise
// splits the line into two outside rays (disk.separates(line)). A tangent leaves
// the disk whole but still snips the single touch point out of the line, so the
// two directions disagree there. A miss separates neither.
TEST_CASE("Line and Disk separation") {
    using Point = pgl::Point<int>;
    using Line = pgl::Line<Point>;
    using Disk = pgl::Disk<Point>;

    Disk d(0, 0, 5);

    SUBCASE("a secant cuts both the disk and the line") {
        Line l(Point(0, 0), Point(2, 1));
        CHECK_MESSAGE(l.separates(d), l, " separates ", d);
        CHECK_MESSAGE(d.separates(l), d, " separates ", l);
    }

    SUBCASE("a tangent leaves the disk whole but snips the line") {
        Line l(Point(5, -3), Point(5, 3));  // tangent at (5,0)
        CHECK_FALSE_MESSAGE(l.separates(d), l, " separates ", d);
        CHECK_MESSAGE(d.separates(l), d, " separates ", l);  // removes the touch point
    }

    SUBCASE("a line missing the disk separates neither") {
        Line l(Point(6, -3), Point(6, 3));
        CHECK_FALSE_MESSAGE(l.separates(d), l, " separates ", d);
        CHECK_FALSE_MESSAGE(d.separates(l), d, " separates ", l);
    }
}

// crosses needs mutual separation, so only a strict secant crosses; a tangent
// (one-sided separation) and a miss do not.
TEST_CASE("Line and Disk cross only on a strict secant") {
    using Point = pgl::Point<int>;
    using Line = pgl::Line<Point>;
    using Disk = pgl::Disk<Point>;

    Disk d(0, 0, 5);

    SUBCASE("a secant crosses both ways") {
        Line l(Point(0, 0), Point(2, 1));
        CHECK_MESSAGE(l.crosses(d), l, " crosses ", d);
        CHECK_MESSAGE(d.crosses(l), d, " crosses ", l);
    }

    SUBCASE("a tangent does not cross") {
        Line l(Point(5, -3), Point(5, 3));
        CHECK_FALSE_MESSAGE(l.crosses(d), l, " crosses ", d);
        CHECK_FALSE_MESSAGE(d.crosses(l), d, " crosses ", l);
    }

    SUBCASE("a miss does not cross") {
        Line l(Point(6, -3), Point(6, 3));
        CHECK_FALSE_MESSAGE(l.crosses(d), l, " crosses ", d);
        CHECK_FALSE_MESSAGE(d.crosses(l), d, " crosses ", l);
    }
}

TEST_CASE("Generated consistency for lines around a disk") {
    using Point = pgl::Point<int>;
    using Line = pgl::Line<Point>;
    using Disk = pgl::Disk<Point>;

    Disk d(0, 0, 5);

    auto x1 = GENERATE(-7, -3, 0, 4, 7);
    auto y1 = GENERATE(-6, -1, 2, 8);
    auto x2 = GENERATE(-6, 0, 3, 6);
    auto y2 = GENERATE(-7, 1, 5);
    if (Point(x1, y1) == Point(x2, y2)) {
        return;  // a line needs two distinct points
    }
    Line l(Point(x1, y1), Point(x2, y2));

    const bool meets = d.intersects(l);
    const bool meets_interior = d.interiorsIntersect(l);

    // Both relations are symmetric.
    CHECK_MESSAGE(meets == l.intersects(d), l, " intersects symmetry ", d);
    CHECK_MESSAGE(meets_interior == l.interiorsIntersect(d), l, " interiorsIntersect symmetry ", d);

    // Cutting the open disk is strictly stronger than touching the closed disk.
    if (meets_interior) {
        CHECK_MESSAGE(meets, d, " must intersect when interiors meet ", l);
    }

    // A point of the line strictly inside the disk forces an interior crossing.
    if (d.interiorContains(l[0]) || d.interiorContains(l[1])) {
        CHECK_MESSAGE(meets_interior, d, " interior must meet ", l);
    }
}
