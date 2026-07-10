#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

using Point = pgl::Point<int>;
using Segment = pgl::Segment<Point>;
// Aliased as `PLine` (not `Polyline`) because doctest pulls in <windows.h> on
// MSVC, whose GDI `Polyline` function collides with a file-scope `using
// Polyline`.
using PLine = pgl::Polyline<Point>;

// zig-zag: (0,0) - (2,2) - (4,0)
static const PLine zig({0, 0, 2, 2, 4, 0});
// closed square written as a polyline: first vertex equals the last
static const PLine loop({0, 0, 2, 0, 2, 2, 0, 2, 0, 0});

TEST_CASE("Polyline intersects Polyline") {
    CHECK(zig.intersects(PLine({1, -1, 1, 3, 3, 3})));  // crosses the first edge
    CHECK(zig.intersects(PLine({2, 2, 2, 5})));         // touches the apex
    CHECK(!zig.intersects(PLine({0, 3, 4, 3})));
    CHECK(!zig.intersects(PLine({10, 10, 12, 12})));    // bbox cull path

    SUBCASE("self-intersecting operands") {
        const PLine bow({0, 0, 2, 2, 2, 0, 0, 2});
        CHECK(bow.intersects(zig));
        CHECK(bow.intersects(PLine({1, 1, 1, 4})));
    }

    SUBCASE("degenerate sizes") {
        CHECK(!PLine().intersects(zig));
        CHECK(!zig.intersects(PLine()));
        CHECK(zig.intersects(PLine({Point(1, 1)})));
        CHECK(PLine({Point(1, 1)}).intersects(zig));
        CHECK(!PLine({Point(1, 0)}).intersects(zig));
    }
}

TEST_CASE("Polyline contains Polyline") {
    CHECK(zig.contains(PLine({1, 1, 2, 2, 3, 1})));   // sub-path across the bend
    CHECK(zig.contains(PLine({1, 1, 2, 2, 1, 1})));   // back-tracking sub-path
    CHECK(zig.contains(zig));
    CHECK(!zig.contains(PLine({0, 0, 4, 0})));        // chord
    CHECK(zig.contains(PLine()));
    CHECK(zig.contains(PLine({Point(3, 1)})));

    SUBCASE("reversal equality interplay") {
        const PLine reversed({4, 0, 2, 2, 0, 0});
        CHECK(zig == reversed);
        CHECK(zig.contains(reversed));
    }
}

TEST_CASE("Polyline interiorContains Polyline") {
    CHECK(zig.interiorContains(PLine({1, 1, 2, 2, 3, 1})));
    CHECK(!zig.interiorContains(PLine({0, 0, 2, 2})));  // reaches the extreme
    CHECK(zig.interiorContains(PLine()));

    SUBCASE("boundaryContains only fits point-like polylines") {
        CHECK(zig.boundaryContains(PLine()));
        CHECK(zig.boundaryContains(PLine({Point(0, 0)})));
        CHECK(zig.boundaryContains(PLine({Point(4, 0), Point(4, 0)})));
        CHECK(!zig.boundaryContains(PLine({Point(2, 2)})));
        CHECK(!zig.boundaryContains(PLine({1, 1, 2, 2})));
    }
}

TEST_CASE("Polyline interiorsIntersect Polyline") {
    SUBCASE("crossing interiors") {
        CHECK(zig.interiorsIntersect(PLine({1, -1, 1, 3, 0, 3})));
    }

    SUBCASE("meeting only at both extremes is not interior") {
        const PLine sameEnds({0, 0, 2, -2, 4, 0});  // shares only (0,0) and (4,0) with zig
        CHECK(zig.intersects(sameEnds));
        CHECK(!zig.interiorsIntersect(sameEnds));
    }

    SUBCASE("an extreme of one inside the other counts for the other side only") {
        // (2,2) is zig's interior vertex and probe's extreme: the common point
        // is excluded as probe's extreme, so the interiors do not meet.
        const PLine probe({2, 2, 2, 5, 4, 5});
        CHECK(!zig.interiorsIntersect(probe));
    }

    SUBCASE("collinear overlap is interior") {
        CHECK(zig.interiorsIntersect(PLine({-1, -1, 1, 1, -1, 3})));
    }

    SUBCASE("interior vertex on interior vertex") {
        const PLine cross({2, 0, 2, 2, 2, 4});  // its interior vertex is zig's apex
        CHECK(zig.interiorsIntersect(cross));
    }

    SUBCASE("degenerate sizes have empty interiors") {
        CHECK(!zig.interiorsIntersect(PLine({Point(2, 2)})));
        CHECK(!PLine({Point(2, 2)}).interiorsIntersect(zig));
    }
}

TEST_CASE("Polyline separates and crosses Polyline") {
    SUBCASE("mutual transversal crossing") {
        const PLine a({0, 0, 4, 4});
        const PLine b({0, 4, 4, 0});
        CHECK(a.separates(b));
        CHECK(b.separates(a));
        CHECK(a.crosses(b));
    }

    SUBCASE("touching at a vertex does not separate") {
        const PLine a({0, 0, 2, 2});
        const PLine b({2, 2, 4, 0});
        CHECK(!a.separates(b));
        CHECK(!a.crosses(b));
    }

    SUBCASE("cutting an open chain in the middle separates it") {
        const PLine cut({1, -1, 1, 3});
        CHECK(cut.separates(zig));
        CHECK(zig.separates(cut));
        CHECK(zig.crosses(cut));
    }

    SUBCASE("set semantics: one crossing does not disconnect a closed polyline") {
        const PLine oneCut({1, -1, 1, 1});
        CHECK(!oneCut.separates(loop));
        const PLine twoCuts({1, -1, 1, 3});
        CHECK(twoCuts.separates(loop));
        // The closed loop cuts the crossing chain in one interior point only.
        CHECK(!twoCuts.crosses(oneCut));
    }

    SUBCASE("a self-intersecting remover") {
        const PLine bow({0, 0, 2, 2, 2, 0, 0, 2});  // crosses itself at (1,1)
        const PLine straight({-1, 1, 3, 1});        // passes through the bow twice
        CHECK(bow.separates(straight));
        CHECK(straight.separates(bow));
        CHECK(bow.crosses(straight));
    }

    SUBCASE("a single-vertex polyline acts as a point") {
        const PLine dot({Point(1, 1)});
        CHECK(dot.separates(zig));
        CHECK(!dot.separates(loop));
    }
}

TEST_CASE("Polyline distances to Polyline") {
    CHECK(zig.squaredDistance(PLine({1, -1, 1, 3})) == 0);
    CHECK(zig.squaredDistance<double>(PLine({0, 3, 4, 3})) == doctest::Approx(1.0));
    CHECK(zig.squaredDistance<double>(PLine({6, 0, 7, 0, 7, 3})) == doctest::Approx(4.0));
    CHECK(zig.distanceL1(PLine({6, 0, 7, 0})) == 2);
    CHECK(zig.distanceLInf(PLine({6, 0, 7, 0})) == 2);

    SUBCASE("exact rational distances") {
        const pgl::EPolyline a(zig);
        const pgl::EPolyline b(PLine({1, 3, 3, 3}));
        // The apex (2,2) is one unit below the segment y = 3.
        CHECK(a.squaredDistance<pgl::ERational>(b) == pgl::ERational(1));
    }
}
