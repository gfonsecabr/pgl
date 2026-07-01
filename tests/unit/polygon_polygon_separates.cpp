#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

// Polygon::separates(Polygon): A.separates(B) asks whether removing the region A
// disconnects B, i.e. B \ A has two or more connected components. The contract
// mirrors Convex::separates(Polygon) but A may be non-convex, so A can poke into
// B through several pockets while B stays connected -- only a genuine crosscut
// (one A-component meeting ∂B in two arcs) counts.

using Polygon = pgl::Polygon<pgl::Point<int>>;

TEST_CASE("Polygon separates Polygon: a band cutting across") {
    // A vertical bar A laid across a square B, touching top and bottom edges:
    // B \ A is a left half and a right half.
    const Polygon square({0, 0, 6, 0, 6, 6, 0, 6});
    const Polygon band({2, -1, 4, -1, 4, 7, 2, 7});
    CHECK(band.separates(square));       // splits the square into left and right
    CHECK(square.separates(band));       // and symmetrically cuts the bar's middle out
}

TEST_CASE("Polygon separates Polygon: a single bite does not") {
    const Polygon square({0, 0, 6, 0, 6, 6, 0, 6});
    const Polygon bite({2, -1, 4, -1, 4, 3, 2, 3});  // pokes in from the bottom only
    CHECK_FALSE(bite.separates(square));
}

TEST_CASE("Polygon separates Polygon: disjoint bites do not") {
    // A U opening upward, joined by a base bar *below* B. Inside B the two prongs
    // are separate components of A ∩ B, each a single bottom-edge bite, so B stays
    // connected (you can walk around them).
    const Polygon square({0, 0, 10, 0, 10, 6, 0, 6});
    const Polygon u({1, -2, 9, -2, 9, 4, 7, 4, 7, -1, 3, -1, 3, 4, 1, 4});
    REQUIRE(u.isSimple());
    CHECK_FALSE(u.separates(square));
}

TEST_CASE("Polygon separates Polygon: interior island does not (creates a hole)") {
    const Polygon square({0, 0, 10, 0, 10, 10, 0, 10});
    const Polygon island({4, 4, 6, 4, 6, 6, 4, 6});
    CHECK_FALSE(island.separates(square));
}

TEST_CASE("Polygon separates Polygon: cover leaves nothing") {
    const Polygon inner({2, 2, 4, 2, 4, 4, 2, 4});
    const Polygon outer({0, 0, 6, 0, 6, 6, 0, 6});
    CHECK_FALSE(outer.separates(inner));  // B \ A empty, not disconnected
}

TEST_CASE("Polygon separates Polygon: non-convex A crosscuts through the interior") {
    // A is a plus/cross (non-convex); its vertical bar spans B top-to-bottom.
    const Polygon square({0, 0, 12, 0, 12, 6, 0, 6});
    const Polygon plus({5, -1, 7, -1, 7, 2, 9, 2, 9, 4, 7, 4,
                        7, 7, 5, 7, 5, 4, 3, 4, 3, 2, 5, 2});
    REQUIRE(plus.isSimple());
    CHECK(plus.separates(square));
}

TEST_CASE("Polygon separates Polygon: reflex A pinching the boundary at two points") {
    // A meets ∂B only at two isolated boundary points but its body bridges
    // between them through the interior -> crosscut.
    const Polygon square({0, 0, 6, 0, 6, 6, 0, 6});
    const Polygon pinch({0, 3, 3, 1, 6, 3, 3, 5});  // diamond touching left/right mid-edges
    CHECK(pinch.separates(square));
}

TEST_CASE("Polygon crosses Polygon: two bars mutually cut (delegates to separates)") {
    // A plus sign: each bar cuts the other into two, so the two shapes cross.
    const Polygon vbar({4, -2, 6, -2, 6, 8, 4, 8});   // tall vertical
    const Polygon hbar({-2, 2, 12, 2, 12, 4, -2, 4});  // wide horizontal
    REQUIRE(vbar.separates(hbar));
    REQUIRE(hbar.separates(vbar));
    CHECK(vbar.crosses(hbar));
    CHECK(hbar.crosses(vbar));

    // A single bite separates neither, so the shapes do not cross.
    const Polygon square({0, 0, 6, 0, 6, 6, 0, 6});
    const Polygon bite({2, -1, 4, -1, 4, 3, 2, 3});
    CHECK_FALSE(bite.crosses(square));
}

TEST_CASE("Polygon separates Polygon: agrees with Convex when A is convex (triangle band)") {
    const Polygon square({0, 0, 6, 0, 6, 6, 0, 6});
    // The polygon path must match the reference convex path for a convex A.
    const Polygon tri({3, -1, 8, 3, 3, 7});
    const pgl::Triangle<pgl::Point<int>> t({3, -1, 8, 3, 3, 7});
    CHECK(tri.separates(square) == t.separates(square));
}

// The Polygon <-> {Convex, Triangle, Rectangle} overloads all delegate to
// Polygon::separates(Polygon) through asPolygon(), so each must agree with the
// equivalent all-polygon query in both directions.
TEST_CASE("Polygon separates Convex/Triangle/Rectangle and back, via asPolygon") {
    using Point = pgl::Point<int>;
    const Polygon barPoly({4, -2, 6, -2, 6, 8, 4, 8});   // vertical bar as a polygon
    const Polygon squarePoly({0, 0, 10, 0, 10, 6, 0, 6});

    SUBCASE("Rectangle") {
        const pgl::Rectangle<Point> rect(Point(0, 0), Point(10, 6));
        // Bar cuts the box into left/right; the box slices the bar's middle out.
        CHECK(barPoly.separates(rect) == barPoly.separates(squarePoly));
        CHECK(rect.separates(barPoly) == squarePoly.separates(barPoly));
        CHECK(barPoly.separates(rect));
        CHECK(rect.separates(barPoly));
    }

    SUBCASE("Triangle") {
        const pgl::Triangle<Point> tri({0, 0, 10, 0, 5, 8});
        const Polygon triPoly(tri.asPolygon().vertices());
        CHECK(barPoly.separates(tri) == barPoly.separates(triPoly));
        CHECK(tri.separates(barPoly) == triPoly.separates(barPoly));
    }

    SUBCASE("Convex") {
        const pgl::Convex<Point> conv({0, 0, 10, 0, 10, 6, 0, 6});
        const Polygon convPoly(conv.asPolygon().vertices());
        CHECK(barPoly.separates(conv) == barPoly.separates(convPoly));
        CHECK(conv.separates(barPoly) == convPoly.separates(barPoly));
    }
}
