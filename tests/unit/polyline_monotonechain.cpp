#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

using Point = pgl::Point<int>;
using Chain = pgl::MonotoneChain<Point>;
// Aliased as `PLine` (not `Polyline`) because doctest pulls in <windows.h> on
// MSVC, whose GDI `Polyline` function collides with a file-scope `using
// Polyline`.
using PLine = pgl::Polyline<Point>;

// zig-zag polyline: (0,0) - (2,2) - (4,0)
static const PLine zig({0, 0, 2, 2, 4, 0});
// The same point set as a monotone chain.
static const Chain tent({0, 0, 2, 2, 4, 0});

TEST_CASE("Polyline and MonotoneChain intersection, both directions") {
    CHECK(zig.intersects(tent));
    CHECK(tent.intersects(zig));

    const Chain above({0, 3, 2, 5, 4, 3});
    CHECK_FALSE(zig.intersects(above));
    CHECK_FALSE(above.intersects(zig));

    const Chain crossing({1, -1, 1, 3});   // vertical chain through x = 1
    CHECK(zig.intersects(crossing));
    CHECK(zig.interiorsIntersect(crossing));

    SUBCASE("touching only at both shapes' extremes is not interior") {
        const Chain touch({-2, 0, 0, 0});   // ends exactly at zig's extreme
        CHECK(zig.intersects(touch));
        CHECK_FALSE(zig.interiorsIntersect(touch));
    }

    SUBCASE("a chain through a non-extreme polyline vertex is interior") {
        const Chain overApex({1, 2, 3, 2});
        CHECK(zig.interiorsIntersect(overApex));
    }

    SUBCASE("collinear overlap is interior") {
        const Chain along({-1, -1, 1, 1});
        CHECK(zig.interiorsIntersect(along));
    }

    SUBCASE("size-one operands") {
        CHECK(zig.intersects(Chain({Point(2, 2)})));
        CHECK(PLine({Point(2, 2)}).intersects(tent));
        CHECK_FALSE(PLine().intersects(tent));
    }
}

TEST_CASE("Polyline and MonotoneChain containment, both directions") {
    CHECK(zig.contains(tent));
    CHECK(tent.contains(zig));
    // The polyline traverses the same set with a repeated stretch.
    const PLine backtracking({0, 0, 2, 2, 1, 1, 2, 2, 4, 0});
    CHECK(tent.contains(backtracking));
    CHECK(backtracking.contains(tent));

    CHECK(zig.contains(Chain({1, 1, 2, 2})));
    CHECK_FALSE(zig.contains(Chain({0, 0, 4, 0})));   // the chord under the tent

    // Interior containment subtracts the container's extremes.
    CHECK(zig.interiorContains(Chain({1, 1, 2, 2, 3, 1})));
    CHECK_FALSE(zig.interiorContains(Chain({0, 0, 2, 2})));
    CHECK(tent.interiorContains(PLine({1, 1, 2, 2, 3, 1})));
    CHECK_FALSE(tent.interiorContains(PLine({0, 0, 2, 2})));

    // Boundary containment holds only for point-sized operands.
    CHECK_FALSE(zig.boundaryContains(tent));
    CHECK(zig.boundaryContains(Chain({Point(0, 0)})));
    CHECK_FALSE(tent.boundaryContains(zig));
    CHECK(tent.boundaryContains(PLine({Point(0, 0)})));
    CHECK(tent.boundaryContains(PLine()));
}

TEST_CASE("Polyline and MonotoneChain separates and crosses") {
    const Chain crossing({1, -1, 1, 3});
    CHECK(zig.separates(crossing));
    CHECK(crossing.separates(zig));
    CHECK(zig.crosses(crossing));

    SUBCASE("touching extreme-to-extreme does not cut either shape") {
        const Chain touch({-2, 0, 0, 0});
        CHECK_FALSE(touch.separates(zig));
        CHECK_FALSE(zig.separates(touch));
    }

    SUBCASE("set semantics: the loop resists a single cut by a chain") {
        const PLine loop({0, 0, 2, 0, 2, 2, 0, 2, 0, 0});
        const Chain oneCut({1, -1, 1, 1});   // reaches into the loop's bottom edge
        CHECK_FALSE(oneCut.separates(loop));
        const Chain twoCuts({1, -1, 1, 3});
        CHECK(twoCuts.separates(loop));
        // The loop cuts the crossing chain's middle out.
        CHECK(loop.separates(twoCuts));
    }

    SUBCASE("a chain covering one polyline extreme does not cut") {
        const Chain cover({-1, -1, 1, 1});
        CHECK_FALSE(cover.separates(zig));
    }
}

TEST_CASE("Polyline and MonotoneChain distance") {
    using Rational = pgl::Rational<int>;

    const Chain above({0, 3, 2, 3, 4, 3});   // y = 3, one unit above the apex
    CHECK(zig.squaredDistance<Rational>(above) == Rational(1));
    CHECK(above.squaredDistance<Rational>(zig) == Rational(1));
    CHECK(zig.distanceL1<Rational>(above) == Rational(1));
    CHECK(zig.distanceLInf<Rational>(above) == Rational(1));
    CHECK(zig.squaredDistance<Rational>(tent) == Rational(0));
}
