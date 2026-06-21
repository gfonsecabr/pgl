// Include the library before doctest: on Windows doctest pulls in <windows.h>,
// whose legacy `near`/`far`/`Rectangle` macros would otherwise clobber the
// library headers. Order is irrelevant on the Linux CI toolchain.
#include "pgl.hpp"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

// Triangle::separates(Disk) and Rectangle::separates(Disk).
//
// `A.separates(B)` means "removing A disconnects B", i.e. D \ A has >= 2
// connected pieces.  For a polygon removed from a disk this happens iff the
// polygon boundary crosses the circle >= 4 times (>= 2 in/out arcs).
//
// Both overloads are specialized (NOT delegated to Convex): for these fixed-size
// shapes the answer is a closed-form table in
//   k = vertices strictly inside the disk,
//   c = edges that carry a full chord of the disk.
//
//   Triangle:  k==0 -> c>=2 ;  k==1 -> c>=1 ;  k>=2 -> never
//   Rectangle: k==0 -> c>=2 ;  k in {1,2} -> c>=1 ;  k>=3 -> never
//
// All tests below use a disk of radius 10 centred at the origin and stay in
// general position (no vertex on the circle, no tangent edge); boundary and
// degenerate configurations are explicitly undefined (see the last cases).

using P = pgl::Point<int>;
using Triangle = pgl::Triangle<P>;
using Rect = pgl::Rectangle<P>;
using Convex = pgl::Convex<P>;
using Seg = pgl::Segment<P>;
using Disk = pgl::Disk<P>;

static const Disk unitDisk({0, 0}, 10);  // centre (0,0), radius 10

TEST_CASE("Triangle separates disk: truth table by (k vertices inside, c chords)") {
    const Disk d = unitDisk;

    SUBCASE("k=0, c=0 -- disjoint: not separated") {
        CHECK_FALSE(Triangle(50, 50, 60, 50, 50, 60).separates(d));
    }
    SUBCASE("k=0, c=0 -- triangle engulfs the disk: not separated") {
        CHECK_FALSE(Triangle(-100, -100, 100, -100, 0, 100).separates(d));
    }
    SUBCASE("k=0, c=1 -- one chord cuts off a cap only: not separated") {
        // Top edge y=0 is a full chord; the body sits below it -> one cap.
        CHECK_FALSE(Triangle(-50, 0, 50, 0, 0, -50).separates(d));
    }
    SUBCASE("k=0, c=2 -- wedge pierces straight through: separated") {
        // Apex below the disk, the two sides chord it -> disk split left/right.
        CHECK(Triangle(0, -30, -20, 40, 20, 40).separates(d));
    }
    SUBCASE("k=1, c=0 -- corner bite from an inside vertex: not separated") {
        CHECK_FALSE(Triangle(0, 0, 50, 50, 50, -50).separates(d));
    }
    SUBCASE("k=1, c=1 -- inside vertex plus opposite chord: separated") {
        // A=(0,-5) inside; opposite edge y=2 is a chord; sides cross once each.
        CHECK(Triangle(0, -5, -50, 2, 50, 2).separates(d));
    }
    SUBCASE("k=2 -- two vertices inside, single bulge: never separated") {
        CHECK_FALSE(Triangle(0, 0, 5, 0, 50, 50).separates(d));
    }
    SUBCASE("k=3 -- triangle strictly inside the disk: never separated") {
        CHECK_FALSE(Triangle(-3, -3, 3, -3, 0, 3).separates(d));
    }
}

TEST_CASE("Rectangle separates disk: only a band across the disk separates it") {
    const Disk d = unitDisk;

    // For an axis-aligned rectangle, a chord edge forces all four corners
    // outside (the perpendicular pair shares the far x or y), so k>=1 with a
    // chord is geometrically impossible: the sole separating case is the band.
    SUBCASE("k=0, c=2 -- band across the disk: separated") {
        CHECK(Rect(-50, -3, 50, 3).separates(d));  // edges y=+/-3 are chords
    }
    SUBCASE("k=0, c=1 -- covers a single cap: not separated") {
        CHECK_FALSE(Rect(-50, 1, 50, 50).separates(d));  // only y=1 is a chord
    }
    SUBCASE("k=0, c=0 -- rectangle engulfs the disk: not separated") {
        CHECK_FALSE(Rect(-100, -100, 100, 100).separates(d));
    }
    SUBCASE("k=0, c=0 -- disjoint: not separated") {
        CHECK_FALSE(Rect(50, 50, 60, 60).separates(d));
    }
    SUBCASE("k=2 -- pokes in from below (two corners inside), no chord: not separated") {
        CHECK_FALSE(Rect(-2, -50, 2, 5).separates(d));
    }
    SUBCASE("k=4 -- rectangle strictly inside the disk: not separated") {
        CHECK_FALSE(Rect(-3, -3, 3, 3).separates(d));
    }
}

TEST_CASE("Convex separates disk: general case counts boundary/circle crossings") {
    const Disk d = unitDisk;

    // Regression: a thin rhombus laid across the disk has its two *opposite*
    // vertices (0,+/-3) inside and its tips outside, so it crosses the circle
    // four times via single-crossing edges with no full chord. The disk is split
    // into a top and a bottom cap. The old chord-only logic answered false.
    SUBCASE("thin rhombus across the disk (no full chord): separated") {
        CHECK(Convex({{20, 0}, {0, 3}, {-20, 0}, {0, -3}}).separates(d));
    }
    // Same failure mode with four interior vertices (hexagon along x).
    SUBCASE("long hexagon across the disk: separated") {
        CHECK(Convex({{20, 0}, {8, 3}, {-8, 3}, {-20, 0}, {-8, -3}, {8, -3}})
                  .separates(d));
    }
    SUBCASE("band across the disk (two full chords): separated") {
        CHECK(Convex({{-50, -3}, {50, -3}, {50, 3}, {-50, 3}}).separates(d));
    }
    SUBCASE("covers a single cap (one full chord): not separated") {
        CHECK_FALSE(Convex({{-50, 1}, {50, 1}, {50, 50}, {-50, 50}}).separates(d));
    }
    SUBCASE("polygon engulfs the disk: not separated") {
        CHECK_FALSE(
            Convex({{-100, -100}, {100, -100}, {100, 100}, {-100, 100}}).separates(d));
    }
    SUBCASE("polygon strictly inside the disk: not separated") {
        CHECK_FALSE(Convex({{-3, -3}, {3, -3}, {3, 3}, {-3, 3}}).separates(d));
    }
    SUBCASE("corner bite from a single interior vertex: not separated") {
        CHECK_FALSE(Convex({{0, 0}, {50, 50}, {50, -50}}).separates(d));
    }
}

// Disk::crosses(X) == disk.separates(X) && X.separates(disk): each shape cuts
// the other. These were just implemented as the conjunction (Tier A).
TEST_CASE("Disk crosses Segment: true iff the segment is a chord through the disk") {
    const Disk d = unitDisk;
    SUBCASE("chord with both endpoints outside: crosses") {
        CHECK(d.crosses(Seg({-50, 0}, {50, 0})));
    }
    SUBCASE("one endpoint inside the disk: does not cross") {
        CHECK_FALSE(d.crosses(Seg({0, 0}, {50, 0})));
    }
    SUBCASE("segment entirely inside the disk: does not cross") {
        CHECK_FALSE(d.crosses(Seg({-3, 0}, {3, 0})));
    }
    SUBCASE("disjoint segment: does not cross") {
        CHECK_FALSE(d.crosses(Seg({100, 100}, {200, 200})));
    }
}

TEST_CASE("Disk crosses Triangle: true iff each pierces the other") {
    const Disk d = unitDisk;
    SUBCASE("long thin sliver run through the disk: crosses") {
        CHECK(d.crosses(Triangle(-100, 0, 100, 2, 100, -2)));
    }
    SUBCASE("disk strictly inside the triangle: does not cross") {
        CHECK_FALSE(d.crosses(Triangle(-100, -100, 100, -100, 0, 100)));
    }
    SUBCASE("triangle strictly inside the disk: does not cross") {
        CHECK_FALSE(d.crosses(Triangle(-3, -3, 3, -3, 0, 3)));
    }
    SUBCASE("disjoint triangle: does not cross") {
        CHECK_FALSE(d.crosses(Triangle(50, 50, 60, 50, 50, 60)));
    }
}

TEST_CASE("Disk crosses Rectangle: true iff each pierces the other") {
    const Disk d = unitDisk;
    SUBCASE("long thin band run through the disk: crosses") {
        CHECK(d.crosses(Rect(-100, -1, 100, 1)));
    }
    SUBCASE("rectangle engulfs the disk: does not cross") {
        CHECK_FALSE(d.crosses(Rect(-100, -100, 100, 100)));
    }
    SUBCASE("rectangle strictly inside the disk: does not cross") {
        CHECK_FALSE(d.crosses(Rect(-3, -3, 3, 3)));
    }
    SUBCASE("disjoint rectangle: does not cross") {
        CHECK_FALSE(d.crosses(Rect(50, 50, 60, 60)));
    }
}

TEST_CASE("Disk crosses Disk: never (a convex bite leaves a connected crescent)") {
    const Disk d = unitDisk;
    CHECK_FALSE(d.crosses(Disk({5, 0}, 10)));    // overlapping
    CHECK_FALSE(d.crosses(Disk({0, 0}, 5)));     // concentric, contained
    CHECK_FALSE(d.crosses(Disk({100, 0}, 3)));   // disjoint
    CHECK_FALSE(d.crosses(unitDisk));            // identical
}

TEST_CASE("separates(disk) sanity: orientation/argument order does not matter") {
    const Disk d = unitDisk;
    // Same wedge as the k=0,c=2 case, vertices given in a different order.
    CHECK(Triangle(20, 40, 0, -30, -20, 40).separates(d));
    CHECK(Triangle(-20, 40, 20, 40, 0, -30).separates(d));
}

TEST_CASE("separates(disk) degenerate inputs are undefined -- only required not to crash") {
    const Disk d = unitDisk;
    const Triangle collinear(0, 0, 1, 1, 2, 2);   // degenerate triangle
    const Rect flat(5, 5, 5, 5);             // degenerate (zero-area) rectangle
    const Disk pointDisk({0, 0}, 0);              // degenerate (zero-radius) disk

    CHECK_NOTHROW((void)collinear.separates(d));
    CHECK_NOTHROW((void)Triangle(0, -30, -20, 40, 20, 40).separates(pointDisk));
    CHECK_NOTHROW((void)flat.separates(d));
    CHECK_NOTHROW((void)Rect(-50, -3, 50, 3).separates(pointDisk));
}
