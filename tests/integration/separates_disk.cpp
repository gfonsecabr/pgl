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
