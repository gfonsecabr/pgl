#include "pgl.hpp"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <vector>

TEST_CASE("Disk contains and interiorContains Convex") {
    using Point = pgl::Point<int>;
    using Convex = pgl::Convex<Point>;
    using Disk = pgl::Disk<Point>;

    const Disk d({0, 0}, 5);

    // Triangle with all vertices well inside the disk.
    const Convex inner(std::vector<Point>{{0, 0}, {3, 0}, {0, 4}});
    CHECK_MESSAGE(d.contains(inner), "disk contains inner convex");
    CHECK_MESSAGE(d.interiorContains(inner), "disk interiorContains inner convex");

    // Convex with a vertex outside the disk.
    const Convex outside(std::vector<Point>{{0, 0}, {5, 0}, {6, 0}});
    CHECK_FALSE_MESSAGE(d.contains(outside), "disk does not contain outside convex");
}

TEST_CASE("Disk contains(Convex) is exact for three-point integer disks") {
    using Point = pgl::Point<int>;
    using Convex = pgl::Convex<Point>;
    using Disk = pgl::Disk<Point>;

    // Convex::contains(Disk) tests each edge's left halfplane, so it inherits the
    // Halfplane::contains(Disk) truncation. The triangle (-6,-6),(6,-6),(0,6)
    // does NOT contain the disk through (-4,-3),(2,-4),(0,-1).
    const Convex convex(std::vector<Point>{{-6, -6}, {6, -6}, {0, 6}});
    const Disk d(Point(-4, -3), Point(2, -4), Point(0, -1));
    CHECK_FALSE_MESSAGE(convex.contains(d), convex, " does not contain disk");
}

TEST_CASE("Convex contains and interiorContains Disk") {
    using Point = pgl::Point<int>;
    using Convex = pgl::Convex<Point>;
    using Disk = pgl::Disk<Point>;

    // A large square that contains a small disk.
    const Convex sq(std::vector<Point>{{-10, -10}, {10, -10}, {10, 10}, {-10, 10}});
    const Disk inside({0, 0}, 5);
    const Disk crossing({8, 0}, 5);  // extends past the square's right edge
    const Disk outside({20, 0}, 3);

    CHECK_MESSAGE(sq.contains(inside), sq, " contains small disk");
    CHECK_FALSE_MESSAGE(sq.contains(crossing), sq, " contains crossing disk");
    CHECK_FALSE_MESSAGE(sq.contains(outside), sq, " contains outside disk");
}

TEST_CASE("Convex separates Disk: general case counts boundary crossings") {
    using Point = pgl::Point<int>;
    using Convex = pgl::Convex<Point>;
    using Disk = pgl::Disk<Point>;

    const Disk d({0, 0}, 10);  // radius 10

    SUBCASE("thin rhombus across the disk (no full chord): separated") {
        CHECK_MESSAGE(
            Convex({{20, 0}, {0, 3}, {-20, 0}, {0, -3}}).separates(d),
            "thin rhombus separates disk");
    }
    SUBCASE("long hexagon across the disk: separated") {
        CHECK_MESSAGE(
            Convex({{20, 0}, {8, 3}, {-8, 3}, {-20, 0}, {-8, -3}, {8, -3}}).separates(d),
            "long hexagon separates disk");
    }
    SUBCASE("band across the disk (two full chords): separated") {
        CHECK_MESSAGE(
            Convex({{-50, -3}, {50, -3}, {50, 3}, {-50, 3}}).separates(d),
            "band convex separates disk");
    }
    SUBCASE("covers only a single cap: not separated") {
        CHECK_FALSE_MESSAGE(
            Convex({{-50, 1}, {50, 1}, {50, 50}, {-50, 50}}).separates(d),
            "cap convex does not separate");
    }
    SUBCASE("polygon engulfs the disk: not separated") {
        CHECK_FALSE_MESSAGE(
            Convex({{-100, -100}, {100, -100}, {100, 100}, {-100, 100}}).separates(d),
            "engulfing convex");
    }
    SUBCASE("polygon strictly inside the disk: not separated") {
        CHECK_FALSE_MESSAGE(
            Convex({{-3, -3}, {3, -3}, {3, 3}, {-3, 3}}).separates(d),
            "inner convex");
    }
    SUBCASE("corner bite from a single interior vertex: not separated") {
        CHECK_FALSE_MESSAGE(
            Convex({{0, 0}, {50, 50}, {50, -50}}).separates(d),
            "corner-bite convex");
    }
}

TEST_CASE("Convex interiorsIntersect Disk uses the exact circumcentre") {
    using Point = pgl::Point<int>;
    using Convex = pgl::Convex<Point>;
    using Disk = pgl::Disk<Point>;

    // Disk d2 has a rational circumcentre; Convex::interiorsIntersect(Disk)
    // must compute it exactly to give the right answer.
    const Disk d2(Point(6, 4), Point(11, 2), Point(10, 7));
    const Convex convex(std::vector<Point>{
        Point(1, 4), Point(2, 3), Point(5, 1), Point(7, 11), Point(6, 11)});
    CHECK_FALSE_MESSAGE(convex.interiorsIntersect(d2), convex, " interiorsIntersect(disk with rational center)");
}

TEST_CASE("Convex and Disk intersects and interiorsIntersect, both directions") {
    using Point = pgl::Point<int>;
    using Convex = pgl::Convex<Point>;
    using Disk = pgl::Disk<Point>;

    const Convex sq(std::vector<Point>{{0, 0}, {4, 0}, {4, 4}, {0, 4}});

    SUBCASE("overlapping: both intersect and interiors intersect") {
        const Disk over({2, 2}, 3);
        CHECK_MESSAGE(sq.intersects(over), sq, " intersects overlapping disk");
        CHECK_MESSAGE(over.intersects(sq), "overlapping disk intersects ", sq);
        CHECK_MESSAGE(sq.interiorsIntersect(over), sq, " interiorsIntersect overlapping disk");
    }

    SUBCASE("disk far away: no intersection") {
        const Disk away({20, 20}, 3);
        CHECK_FALSE_MESSAGE(sq.intersects(away), sq, " intersects far disk");
        CHECK_FALSE_MESSAGE(away.intersects(sq), "far disk intersects ", sq);
    }
}

TEST_CASE("Convex and Disk crosses, both directions") {
    using Point = pgl::Point<int>;
    using Convex = pgl::Convex<Point>;
    using Disk = pgl::Disk<Point>;

    // Both center+radius and 3-point (no shared x or y coordinate) forms.
    // (6,8), (-8,6), (10,0) all lie on the circle centered at (0,0) with radius 10.
    for (const Disk d : {Disk(Point(0, 0), 10),
                         Disk(Point(6, 8), Point(-8, 6), Point(10, 0))}) {
        SUBCASE("thin band through the disk: convex and disk cross each other") {
            const Convex band(std::vector<Point>{{-20, -2}, {20, -2}, {20, 2}, {-20, 2}});
            CHECK_MESSAGE(band.crosses(d), band, " crosses disk");
            CHECK_MESSAGE(d.crosses(band), "disk crosses ", band);
        }

        SUBCASE("convex inside disk: neither crosses the other") {
            const Convex inner(std::vector<Point>{{-3, -3}, {3, -3}, {3, 3}, {-3, 3}});
            CHECK_FALSE_MESSAGE(inner.crosses(d), inner, " crosses disk (inner)");
            CHECK_FALSE_MESSAGE(d.crosses(inner), "disk crosses inner convex");
        }

        SUBCASE("disk inside convex: neither crosses the other") {
            const Convex outer(std::vector<Point>{{-50, -50}, {50, -50}, {50, 50}, {-50, 50}});
            CHECK_FALSE_MESSAGE(outer.crosses(d), outer, " crosses disk (outer)");
            CHECK_FALSE_MESSAGE(d.crosses(outer), "disk crosses outer convex");
        }

        SUBCASE("disjoint: neither crosses") {
            const Convex away(std::vector<Point>{{20, 20}, {30, 20}, {30, 30}, {20, 30}});
            CHECK_FALSE_MESSAGE(away.crosses(d), away, " crosses disk (disjoint)");
            CHECK_FALSE_MESSAGE(d.crosses(away), "disk crosses disjoint convex");
        }
    }
}
