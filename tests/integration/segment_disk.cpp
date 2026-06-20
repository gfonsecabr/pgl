#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstdint>

#include "pgl.hpp"

// Disk::intersects(Segment) is decided exactly and without division. The disk
// here is the integer circle of radius 5 centred at the origin, so a point
// (x, y) lies in the closed disk iff x*x + y*y <= 25, which makes every
// expected answer below checkable by hand.

TEST_CASE("Segments disjoint from a disk") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Disk = pgl::Disk<Point>;

    Disk d(0, 0, 5);

    SUBCASE("far away, foot beyond an endpoint") {
        // Nearest point of the segment to the centre is the endpoint (6, 0).
        Segment s(6, 0, 10, 0);
        CHECK_FALSE_MESSAGE(s.intersects(d), s, " intersects ", d);
        CHECK_FALSE_MESSAGE(d.intersects(s), d, " intersects ", s);
    }

    SUBCASE("parallel near-miss, foot inside the segment span") {
        // Supporting line x = 6 stays one unit outside the circle.
        Segment s(6, -3, 6, 3);
        CHECK_FALSE_MESSAGE(s.intersects(d), s, " intersects ", d);
        CHECK_FALSE_MESSAGE(d.intersects(s), d, " intersects ", s);
    }

    SUBCASE("would cross the circle, but the foot is off the segment") {
        // Line x = 3 cuts the circle, yet the segment lives above it (y in
        // [5, 10]); its nearest point (3, 5) is outside the disk.
        Segment s(3, 5, 3, 10);
        CHECK_FALSE_MESSAGE(s.intersects(d), s, " intersects ", d);
        CHECK_FALSE_MESSAGE(d.intersects(s), d, " intersects ", s);
    }
}

TEST_CASE("Segments meeting a disk through an endpoint") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Disk = pgl::Disk<Point>;

    Disk d(0, 0, 5);

    SUBCASE("endpoint strictly inside") {
        Segment s(0, 0, 20, 0);
        CHECK_MESSAGE(s.intersects(d), s, " intersects ", d);
        CHECK_MESSAGE(d.intersects(s), d, " intersects ", s);
    }

    SUBCASE("endpoint on the boundary circle") {
        Segment s(5, 0, 5, 5);
        CHECK_MESSAGE(s.intersects(d), s, " intersects ", d);
        CHECK_MESSAGE(d.intersects(s), d, " intersects ", s);
    }

    SUBCASE("endpoint on the boundary at a non-axis lattice point") {
        // (3, 4) is on the circle: 9 + 16 = 25.
        Segment s(3, 4, 9, 9);
        CHECK_MESSAGE(s.intersects(d), s, " intersects ", d);
        CHECK_MESSAGE(d.intersects(s), d, " intersects ", s);
    }
}

TEST_CASE("Segments crossing a disk with both endpoints outside") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Disk = pgl::Disk<Point>;

    Disk d(0, 0, 5);

    SUBCASE("diameter along the x axis") {
        Segment s(-6, 0, 6, 0);
        CHECK_MESSAGE(s.intersects(d), s, " intersects ", d);
        CHECK_MESSAGE(d.intersects(s), d, " intersects ", s);
    }

    SUBCASE("offset chord, foot strictly inside") {
        Segment s(-6, 2, 6, 2);
        CHECK_MESSAGE(s.intersects(d), s, " intersects ", d);
        CHECK_MESSAGE(d.intersects(s), d, " intersects ", s);
    }

    SUBCASE("tangent line touching the circle at one point") {
        // x = 5 grazes the circle at (5, 0); both endpoints are outside.
        Segment s(5, -3, 5, 3);
        CHECK_MESSAGE(s.intersects(d), s, " intersects ", d);
        CHECK_MESSAGE(d.intersects(s), d, " intersects ", s);
    }
}

TEST_CASE("Disk built from three arbitrary boundary points") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Disk = pgl::Disk<Point>;

    // Same radius-5 circle, but defined by three lattice points on it rather
    // than by centre/radius, exercising a non-trivial orientation determinant.
    Disk d(Point(5, 0), Point(-3, 4), Point(0, -5));

    CHECK_MESSAGE(d.intersects(Segment(-6, 2, 6, 2)), d, " intersects offset chord");
    CHECK_MESSAGE(d.intersects(Segment(5, -3, 5, 3)), d, " intersects tangent");
    CHECK_FALSE_MESSAGE(d.intersects(Segment(6, -3, 6, 3)), d, " misses near-tangent");
    CHECK_FALSE_MESSAGE(d.intersects(Segment(6, 0, 10, 0)), d, " misses far segment");
}

TEST_CASE("Degenerate point segments against a disk") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Disk = pgl::Disk<Point>;

    Disk d(0, 0, 5);

    SUBCASE("point segment inside the disk") {
        Segment s(1, 1, 1, 1);
        CHECK_MESSAGE(s.intersects(d), s, " intersects ", d);
        CHECK_MESSAGE(d.intersects(s), d, " intersects ", s);
    }

    SUBCASE("point segment on the boundary") {
        Segment s(3, 4, 3, 4);
        CHECK_MESSAGE(s.intersects(d), s, " intersects ", d);
        CHECK_MESSAGE(d.intersects(s), d, " intersects ", s);
    }

    SUBCASE("point segment outside the disk") {
        Segment s(6, 0, 6, 0);
        CHECK_FALSE_MESSAGE(s.intersects(d), s, " intersects ", d);
        CHECK_FALSE_MESSAGE(d.intersects(s), d, " intersects ", s);
    }
}

TEST_CASE("Rational coordinates with a fractional tangent") {
    using Rational = pgl::Rational<int64_t>;
    using Point = pgl::Point<Rational>;
    using Segment = pgl::Segment<Point>;
    using Disk = pgl::Disk<Point>;

    // Unit circle centred at the origin.
    Disk d(Point(Rational(0), Rational(0)), Rational(1));

    SUBCASE("horizontal line tangent at y = 1") {
        Segment s(Point(Rational(-2), Rational(1)), Point(Rational(2), Rational(1)));
        CHECK_MESSAGE(s.intersects(d), s, " intersects ", d);
        CHECK_MESSAGE(d.intersects(s), d, " intersects ", s);
    }

    SUBCASE("horizontal line just above the circle") {
        Segment s(Point(Rational(-2), Rational(11, 10)), Point(Rational(2), Rational(11, 10)));
        CHECK_FALSE_MESSAGE(s.intersects(d), s, " intersects ", d);
        CHECK_FALSE_MESSAGE(d.intersects(s), d, " intersects ", s);
    }

    SUBCASE("chord at y = 1/2") {
        Segment s(Point(Rational(-2), Rational(1, 2)), Point(Rational(2), Rational(1, 2)));
        CHECK_MESSAGE(s.intersects(d), s, " intersects ", d);
        CHECK_MESSAGE(d.intersects(s), d, " intersects ", s);
    }
}

TEST_CASE("Open disk against the relative interior of a segment") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Disk = pgl::Disk<Point>;

    Disk d(0, 0, 5);

    SUBCASE("chord through the interior") {
        Segment s(-6, 2, 6, 2);
        CHECK_MESSAGE(d.interiorsIntersect(s), d, " interiorsIntersect ", s);
        CHECK_MESSAGE(s.interiorsIntersect(d), s, " interiorsIntersect ", d);
    }

    SUBCASE("diameter") {
        Segment s(-6, 0, 6, 0);
        CHECK_MESSAGE(d.interiorsIntersect(s), d, " interiorsIntersect ", s);
        CHECK_MESSAGE(s.interiorsIntersect(d), s, " interiorsIntersect ", d);
    }

    SUBCASE("endpoint strictly inside") {
        Segment s(0, 0, 20, 0);
        CHECK_MESSAGE(d.interiorsIntersect(s), d, " interiorsIntersect ", s);
        CHECK_MESSAGE(s.interiorsIntersect(d), s, " interiorsIntersect ", d);
    }

    SUBCASE("from a boundary point inward") {
        // (3, 4) is on the circle; the segment heads into the interior.
        Segment s(3, 4, 0, 0);
        CHECK_MESSAGE(d.interiorsIntersect(s), d, " interiorsIntersect ", s);
        CHECK_MESSAGE(s.interiorsIntersect(d), s, " interiorsIntersect ", d);
    }

    SUBCASE("tangent line only touches the boundary") {
        Segment s(5, -3, 5, 3);
        CHECK_MESSAGE(d.intersects(s), d, " intersects ", s);  // closed-disk contact
        CHECK_FALSE_MESSAGE(d.interiorsIntersect(s), d, " interiorsIntersect ", s);
        CHECK_FALSE_MESSAGE(s.interiorsIntersect(d), s, " interiorsIntersect ", d);
    }

    SUBCASE("from a boundary point outward") {
        // (3, 4) is on the circle; the segment leaves the disk, so the open
        // disk and the open segment never meet.
        Segment s(3, 4, 9, 9);
        CHECK_MESSAGE(d.intersects(s), d, " intersects ", s);
        CHECK_FALSE_MESSAGE(d.interiorsIntersect(s), d, " interiorsIntersect ", s);
        CHECK_FALSE_MESSAGE(s.interiorsIntersect(d), s, " interiorsIntersect ", d);
    }

    SUBCASE("disjoint segments never meet interiors") {
        CHECK_FALSE_MESSAGE(d.interiorsIntersect(Segment(6, 0, 10, 0)), d, " ii far");
        CHECK_FALSE_MESSAGE(d.interiorsIntersect(Segment(6, -3, 6, 3)), d, " ii near-miss");
        CHECK_FALSE_MESSAGE(d.interiorsIntersect(Segment(3, 5, 3, 10)), d, " ii foot-off");
    }

    SUBCASE("degenerate point segment has no relative interior") {
        // Even strictly inside the disk, a point segment contributes no interior.
        CHECK_FALSE_MESSAGE(d.interiorsIntersect(Segment(1, 1, 1, 1)), d, " ii point inside");
        CHECK_FALSE_MESSAGE(d.interiorsIntersect(Segment(6, 0, 6, 0)), d, " ii point outside");
    }
}

TEST_CASE("Open disk built from three arbitrary boundary points") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Disk = pgl::Disk<Point>;

    Disk d(Point(5, 0), Point(-3, 4), Point(0, -5));

    CHECK_MESSAGE(d.interiorsIntersect(Segment(-6, 2, 6, 2)), d, " ii offset chord");
    CHECK_FALSE_MESSAGE(d.interiorsIntersect(Segment(5, -3, 5, 3)), d, " ii tangent");
    CHECK_FALSE_MESSAGE(d.interiorsIntersect(Segment(6, -3, 6, 3)), d, " ii near-miss");
}

TEST_CASE("Generated consistency on a lattice around a disk") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Disk = pgl::Disk<Point>;

    Disk d(0, 0, 5);

    auto x1 = GENERATE(-7, -3, 0, 4, 7);
    auto y1 = GENERATE(-7, -1, 0, 5, 8);
    auto x2 = GENERATE(-6, 0, 2, 6);
    auto y2 = GENERATE(-6, 0, 3, 7);
    Segment s(Point(x1, y1), Point(x2, y2));

    const bool meets = d.intersects(s);
    const bool meets_interior = d.interiorsIntersect(s);

    // Both relations are symmetric.
    CHECK_MESSAGE(meets == s.intersects(d), s, " intersects symmetry ", d);
    CHECK_MESSAGE(meets_interior == s.interiorsIntersect(d), s, " interiorsIntersect symmetry ", d);

    // Meeting the open disk's interior is strictly stronger than touching the
    // closed disk.
    if (meets_interior) {
        CHECK_MESSAGE(meets, d, " must intersect when interiors of ", s, " meet");
    }

    // Containing either endpoint forces an intersection; a strictly interior
    // endpoint also forces an interior intersection.
    if (d.contains(s.min()) || d.contains(s.max())) {
        CHECK_MESSAGE(meets, d, " must intersect when it holds an endpoint of ", s);
    }
    if (!s.isDegenerate() && (d.interiorContains(s.min()) || d.interiorContains(s.max()))) {
        CHECK_MESSAGE(meets_interior, d, " interior must meet ", s, " holding an interior endpoint");
    }
}
