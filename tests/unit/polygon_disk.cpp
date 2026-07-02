#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

// Polygon vs Disk.
//
// Tested below: the disk-as-container containment predicates
// Disk::contains / interiorContains / boundaryContains(Polygon); the
// polygon-as-container predicates Polygon::contains / interiorContains(Disk);
// the overlap predicates Polygon::intersects / interiorsIntersect(Disk); and
// Polygon::boundaryContains(Disk).
//
// The Polygon <-> Disk cut predicates -- Polygon::separates / crosses(Disk) and
// Disk::separates(Polygon) -- are exercised in the cut-predicate cases at the
// end of this file.

TEST_CASE("Disk contains Polygon") {
    using Point = pgl::Point<int>;
    using Polygon = pgl::Polygon<Point>;
    using Disk = pgl::Disk<Point>;

    // Disk centred at the origin with radius 10 (r^2 = 100), exact for integers.
    const Disk d(Point(0, 0), 10);

    SUBCASE("polygon well inside the disk is contained, interior too") {
        // Corner (5,5): 5^2 + 5^2 = 50 < 100.
        const Polygon inner({-5, -5, 5, -5, 5, 5, -5, 5});

        CHECK(d.contains(inner));
        CHECK(d.interiorContains(inner));
        CHECK_FALSE(d.boundaryContains(inner));
    }

    SUBCASE("polygon with a vertex on the circle is contained but not interior") {
        // (6,8): 6^2 + 8^2 = 100 lies exactly on the circle; the others are inside.
        const Polygon onCircle({0, 0, 6, 8, 0, 9});

        CHECK(d.contains(onCircle));
        CHECK_FALSE(d.interiorContains(onCircle));
    }

    SUBCASE("polygon poking outside the circle is not contained") {
        // (9,9): 9^2 + 9^2 = 162 > 100.
        const Polygon poking({0, 0, 9, 9, 0, 9});

        CHECK_FALSE(d.contains(poking));
        CHECK_FALSE(d.interiorContains(poking));
    }
}

TEST_CASE("Polygon contains Disk") {
    using Point = pgl::Point<int>;
    using Polygon = pgl::Polygon<Point>;
    using Disk = pgl::Disk<Point>;

    // Axis-aligned square [-20, 20]^2. Integer center+radius disks keep r^2 exact.
    const Polygon square({-20, -20, 20, -20, 20, 20, -20, 20});

    SUBCASE("disk strictly inside is contained, interior too") {
        const Disk inside(Point(0, 0), 10);

        CHECK(square.contains(inside));
        CHECK(square.interiorContains(inside));
    }

    SUBCASE("disk internally tangent to an edge is contained but not interior") {
        // Centre (0,10), r=10 touches the top edge y=20 at (0,20) from inside.
        const Disk tangent(Point(0, 10), 10);

        CHECK(square.contains(tangent));
        CHECK_FALSE(square.interiorContains(tangent));
    }

    SUBCASE("disk poking through an edge is not contained") {
        // Centre (15,0), r=10 reaches x=25, past the right edge x=20.
        const Disk poking(Point(15, 0), 10);

        CHECK_FALSE(square.contains(poking));
        CHECK_FALSE(square.interiorContains(poking));
    }

    SUBCASE("square inside the disk does not contain it") {
        const Disk big(Point(0, 0), 25);

        CHECK_FALSE(square.contains(big));
        CHECK_FALSE(square.interiorContains(big));
    }

    SUBCASE("disjoint disk is not contained") {
        const Disk distant(Point(35, 0), 10);

        CHECK_FALSE(square.contains(distant));
        CHECK_FALSE(square.interiorContains(distant));
    }

    SUBCASE("non-convex polygon respects the reflex notch") {
        // L-shape: the notch is the quadrant x>4, y>4.
        const Polygon lshape({0, 0, 10, 0, 10, 4, 4, 4, 4, 10, 0, 10});

        // Disk sitting well inside the vertical arm.
        const Disk inArm(Point(2, 7), 1);
        CHECK(lshape.contains(inArm));
        CHECK(lshape.interiorContains(inArm));

        // Disk centred inside the arm but large enough to cover the reflex
        // vertex (4,4) and bulge into the notch -- both endpoints inside does
        // not suffice for a non-convex polygon.
        const Disk overReflex(Point(3, 3), 2);
        CHECK_FALSE(lshape.contains(overReflex));

        // Disk entirely in the notch (outside the L).
        const Disk inNotch(Point(8, 8), 1);
        CHECK_FALSE(lshape.contains(inNotch));
    }
}

TEST_CASE("Polygon intersects and interiorsIntersect Disk") {
    using Point = pgl::Point<int>;
    using Polygon = pgl::Polygon<Point>;
    using Disk = pgl::Disk<Point>;

    const Polygon square({-20, -20, 20, -20, 20, 20, -20, 20});

    SUBCASE("disk inside the polygon overlaps both closed and open") {
        const Disk inside(Point(0, 0), 10);

        CHECK(square.intersects(inside));
        CHECK(square.interiorsIntersect(inside));
    }

    SUBCASE("polygon inside the disk overlaps both closed and open") {
        const Disk big(Point(0, 0), 25);

        CHECK(square.intersects(big));
        CHECK(square.interiorsIntersect(big));
    }

    SUBCASE("boundary-crossing disk overlaps both closed and open") {
        const Disk crossing(Point(15, 0), 10);

        CHECK(square.intersects(crossing));
        CHECK(square.interiorsIntersect(crossing));
    }

    SUBCASE("externally tangent disk touches the boundary only") {
        // Centre (30,0), r=10 touches the right edge x=20 at (20,0).
        const Disk tangent(Point(30, 0), 10);

        CHECK(square.intersects(tangent));            // closed: they touch
        CHECK_FALSE(square.interiorsIntersect(tangent));  // open: no overlap
    }

    SUBCASE("disjoint disk overlaps neither") {
        const Disk distant(Point(35, 0), 10);

        CHECK_FALSE(square.intersects(distant));
        CHECK_FALSE(square.interiorsIntersect(distant));
    }

    SUBCASE("non-convex polygon respects the reflex notch") {
        const Polygon lshape({0, 0, 10, 0, 10, 4, 4, 4, 4, 10, 0, 10});

        // Disk in the notch, clear of every edge.
        const Disk inNotch(Point(8, 8), 1);
        CHECK_FALSE(lshape.intersects(inNotch));
        CHECK_FALSE(lshape.interiorsIntersect(inNotch));

        // Disk straddling the reflex corner cuts into the arms.
        const Disk overReflex(Point(3, 3), 2);
        CHECK(lshape.intersects(overReflex));
        CHECK(lshape.interiorsIntersect(overReflex));
    }
}

TEST_CASE("Polygon boundaryContains Disk") {
    using Point = pgl::Point<int>;
    using Polygon = pgl::Polygon<Point>;
    using Disk = pgl::Disk<Point>;

    const Polygon square({-20, -20, 20, -20, 20, 20, -20, 20});
    const Disk d(Point(0, 0), 10);

    // A proper (non-degenerate) disk is a 2D area; its circle boundary never lies
    // on the polygon's 1D boundary, so this is always false.
    CHECK_FALSE(square.boundaryContains(d));
}

// ---------------------------------------------------------------------------
// Polygon <-> Disk cut predicates. Circle-boundary crossings are irrational, so
// (like every Disk cut predicate in the library) these count boundary crossings
// / arcs rather than constructing intersection points. That count is exact for a
// convex polygon; the tests pin the convex behaviour against the trusted
// Convex/Disk oracle and spell out representative hand cases.
//
// Both directions also support non-convex simple polygons. Disk::separates
// (Polygon) triangulates the polygon and glues the per-triangle answers with a
// union-find over the boundary pieces outside the disk; Polygon::separates
// (Disk) triangulates a bounding box constrained by the polygon's edges and
// glues the disk pieces of the outside triangles the same way.

TEST_CASE("Polygon separates Disk: a band cut across the disk") {
    using Point = pgl::Point<int>;
    using Polygon = pgl::Polygon<Point>;
    using Disk = pgl::Disk<Point>;

    const Disk d(Point(0, 0), 5);                       // r^2 = 25
    const Polygon band({-2, -8, 2, -8, 2, 8, -2, 8});   // vertical bar through the disk
    CHECK(band.separates(d));                            // disk split left / right
    CHECK(d.separates(band));                            // disk cuts the bar's middle out
    CHECK(band.crosses(d));
}

TEST_CASE("Polygon separates Disk: disk inside the polygon does not") {
    using Point = pgl::Point<int>;
    using Polygon = pgl::Polygon<Point>;
    using Disk = pgl::Disk<Point>;

    const Disk d(Point(0, 0), 3);
    const Polygon square({-10, -10, 10, -10, 10, 10, -10, 10});
    CHECK_FALSE(square.separates(d));   // removing the frame leaves an annulus, still connected
    CHECK_FALSE(square.crosses(d));
}

TEST_CASE("Polygon separates Disk: polygon inside the disk does not") {
    using Point = pgl::Point<int>;
    using Polygon = pgl::Polygon<Point>;
    using Disk = pgl::Disk<Point>;

    const Disk d(Point(0, 0), 10);
    const Polygon tri({-3, -3, 3, -3, 0, 3});
    CHECK_FALSE(tri.separates(d));      // tiny polygon cannot span the disk
    CHECK_FALSE(d.separates(tri));      // disk covers the whole polygon
    CHECK_FALSE(tri.crosses(d));
}

TEST_CASE("Polygon separates Disk: disjoint shapes do not") {
    using Point = pgl::Point<int>;
    using Polygon = pgl::Polygon<Point>;
    using Disk = pgl::Disk<Point>;

    const Disk d(Point(0, 0), 3);
    const Polygon away({20, 20, 24, 20, 24, 24, 20, 24});
    CHECK_FALSE(away.separates(d));
    CHECK_FALSE(d.separates(away));
    CHECK_FALSE(away.crosses(d));
}

TEST_CASE("Polygon separates Disk: non-convex bites that do not disconnect the disk") {
    using Point = pgl::Point<int>;
    using Polygon = pgl::Polygon<Point>;
    using Disk = pgl::Disk<Point>;

    const Polygon staple(
        {-6, 0, -6, 16, 6, 16, 6, 0, 2, 0, 2, 12, -2, 12, -2, 0});
    REQUIRE(staple.isSimple());
    REQUIRE_FALSE(staple.isConvex());

    // Disk hugging the notch mouth: the legs bite into it left and right (four
    // boundary crossings, which would fool the convex crossing count), but the
    // surviving pieces stay joined through the notch and below y = 0.
    const Disk atMouth(Point(0, 2), 3);
    CHECK_FALSE(staple.separates(atMouth));
    CHECK_FALSE(staple.crosses(atMouth));

    // Disk swallowing the bottom of both legs: again four crossings, but
    // everything the polygon leaves of the disk is joined below the legs.
    const Disk overBottom(Point(0, 0), 10);
    CHECK_FALSE(staple.separates(overBottom));
}

TEST_CASE("Polygon separates Disk: non-convex cuts that disconnect the disk") {
    using Point = pgl::Point<int>;
    using Polygon = pgl::Polygon<Point>;
    using Disk = pgl::Disk<Point>;

    const Polygon staple(
        {-6, 0, -6, 16, 6, 16, 6, 0, 2, 0, 2, 12, -2, 12, -2, 0});

    // Disk across the left leg: one lens survives beyond the outer wall, one
    // inside the notch, and the leg keeps them apart. Both cut directions hold,
    // so this pair crosses.
    const Disk acrossLeg(Point(-4, 8), 3);
    CHECK(staple.separates(acrossLeg));
    CHECK(staple.crosses(acrossLeg));

    // Disk over the top-left area: its lenses past the outer wall and above
    // the top edge merge around the corner (-6,16), which lies strictly inside
    // the disk (distance^2 = 20 < 25) -- but the piece it pushes into the
    // notch stays walled off. Two components.
    const Disk atCorner(Point(-4, 12), 5);
    CHECK(staple.separates(atCorner));
}

TEST_CASE("Polygon separates Disk: non-convex containment, tangency, disjointness") {
    using Point = pgl::Point<int>;
    using Polygon = pgl::Polygon<Point>;
    using Disk = pgl::Disk<Point>;

    const Polygon staple(
        {-6, 0, -6, 16, 6, 16, 6, 0, 2, 0, 2, 12, -2, 12, -2, 0});

    // Disk inside a leg: nothing of it survives the removal.
    CHECK_FALSE(staple.separates(Disk(Point(-4, 8), 1)));

    // Disk containing the whole polygon: the leftover ring stays connected.
    CHECK_FALSE(staple.separates(Disk(Point(0, 8), 15)));

    // Disk in the notch, tangent to both inner walls from outside the polygon:
    // only the two tangency points are removed.
    CHECK_FALSE(staple.separates(Disk(Point(0, 6), 2)));

    // Externally tangent disk: loses the single point (-6, 8).
    CHECK_FALSE(staple.separates(Disk(Point(-8, 8), 2)));

    // Disjoint disk.
    CHECK_FALSE(staple.separates(Disk(Point(30, 30), 3)));
}

// ---------------------------------------------------------------------------
// Disk::separates(Polygon) on non-convex simple polygons. The staple used
// below is an upside-down U: outer rectangle [-6,6]x[0,16] minus the notch
// (-2,2)x[0,12), leaving two vertical legs of width 4 (x in [-6,-2] and
// [2,6]) joined by a top band y in [12,16].

namespace {
const pgl::Polygon<pgl::Point<int>> bigStaple(
    {-6, 0, -6, 16, 6, 16, 6, 0, 2, 0, 2, 12, -2, 12, -2, 0});
}

TEST_CASE("Disk separates non-convex Polygon: legs rejoining around the disk do not separate") {
    using Point = pgl::Point<int>;
    using Disk = pgl::Disk<Point>;

    REQUIRE(bigStaple.isSimple());
    REQUIRE_FALSE(bigStaple.isConvex());

    // r^2 = 100 swallows the bottom of both legs completely (every leg
    // cross-section with y <= 8 lies inside, since 36 + 64 = 100), so nothing
    // is cut off and the legs stay joined through the top band. The polygon
    // boundary leaves the disk in two arcs, so a convex-style arc count would
    // wrongly report a separation here.
    const Disk d(Point(0, 0), 10);
    CHECK_FALSE(d.separates(bigStaple));
}

TEST_CASE("Disk separates non-convex Polygon: severing one leg") {
    using Point = pgl::Point<int>;
    using Disk = pgl::Disk<Point>;

    // Centred inside the left leg, wide enough to poke through both of its
    // walls (wall distance 2 < 3): the leg's bottom is cut off from the rest.
    const Disk acrossLeftLeg(Point(-4, 8), 3);
    CHECK(acrossLeftLeg.separates(bigStaple));

    // Same centre but clear of both walls: the disk floats inside the leg.
    const Disk insideLeftLeg(Point(-4, 8), 1);
    CHECK_FALSE(insideLeftLeg.separates(bigStaple));
}

TEST_CASE("Disk separates non-convex Polygon: severing both legs (three components)") {
    using Point = pgl::Point<int>;
    using Disk = pgl::Disk<Point>;

    // r^2 = 64 spans the full width of each leg near y = 6 but leaves both
    // bottom corners (+-6, 0) outside (distance^2 = 72): each leg keeps an
    // isolated bottom piece, and the top band survives above the disk
    // ((0, 15) has distance^2 = 81). Three components in total.
    const Disk d(Point(0, 6), 8);
    CHECK(d.separates(bigStaple));
}

TEST_CASE("Disk separates non-convex Polygon: eating a dead end does not separate") {
    using Point = pgl::Point<int>;
    using Disk = pgl::Disk<Point>;

    // r^2 = 16 swallows the whole bottom of the left leg (worst corners
    // (-6, 0) and (-2, 0) have distance^2 = 4, and every point below the cut
    // is covered): removing a dead end leaves the rest connected.
    const Disk d(Point(-4, 0), 4);
    CHECK_FALSE(d.separates(bigStaple));
}

TEST_CASE("Disk separates non-convex Polygon: tangencies") {
    using Point = pgl::Point<int>;
    using Disk = pgl::Disk<Point>;

    // Sitting in the notch, tangent to both inner walls from outside the
    // polygon at (+-2, 6): the closed disk only removes two boundary points,
    // which never disconnects an area.
    const Disk inNotch(Point(0, 6), 2);
    CHECK_FALSE(inNotch.separates(bigStaple));

    // Inscribed in the left leg, tangent to its walls from inside at (-6, 6)
    // and (-2, 6): the closed disk covers the full cross-section y = 6
    // (endpoints on the circle included), so the leg's bottom is cut off.
    const Disk inscribed(Point(-4, 6), 2);
    CHECK(inscribed.separates(bigStaple));
}

TEST_CASE("Disk separates non-convex Polygon: containment and disjointness") {
    using Point = pgl::Point<int>;
    using Disk = pgl::Disk<Point>;

    // Disk swallowing the whole polygon: the extreme corners (+-6, 0) and
    // (+-6, 16) lie exactly on the circle (36 + 64 = 100).
    const Disk everything(Point(0, 8), 10);
    CHECK_FALSE(everything.separates(bigStaple));

    // Disjoint disk.
    const Disk faraway(Point(30, 30), 3);
    CHECK_FALSE(faraway.separates(bigStaple));
}

// For a convex polygon the count is exact, so the Polygon path must match the
// Convex path in both directions, over a sweep of disks and convex polygons.
TEST_CASE("Polygon/Disk cut predicates match the Convex oracle for convex polygons") {
    using Point = pgl::Point<int>;
    using Polygon = pgl::Polygon<Point>;
    using Convex = pgl::Convex<Point>;
    using Disk = pgl::Disk<Point>;

    const Convex shapes[] = {
        Convex({-2, -9, 2, -9, 2, 9, -2, 9}),      // tall thin bar
        Convex({-9, -2, 9, -2, 9, 2, -9, 2}),      // wide thin bar
        Convex({-4, -4, 4, -4, 4, 4, -4, 4}),      // square
        Convex({0, 0, 8, 0, 4, 8}),                // triangle
        Convex({-6, 0, 0, -6, 6, 0, 0, 6}),        // diamond
        Convex({3, 3, 12, 3, 12, 12, 3, 12}),      // offset square
    };
    long checks = 0;
    for (int cx = -6; cx <= 6; cx += 3)
        for (int cy = -6; cy <= 6; cy += 3)
            for (int r = 2; r <= 7; ++r) {
                const Disk d(Point(cx, cy), r);
                for (const Convex& c : shapes) {
                    const Polygon p(c.vertices());
                    REQUIRE(p.isSimple());
                    CHECK(p.separates(d) == c.separates(d));
                    CHECK(d.separates(p) == d.separates(c));
                    ++checks;
                }
            }
    CHECK(checks > 500);
}
