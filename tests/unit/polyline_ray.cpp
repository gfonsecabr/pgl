#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

using Point = pgl::Point<int>;
using Ray = pgl::Ray<Point>;
// Aliased as `PLine` (not `Polyline`) because doctest pulls in <windows.h> on
// MSVC, whose GDI `Polyline` function collides with a file-scope `using
// Polyline`.
using PLine = pgl::Polyline<Point>;

// zig-zag: (0,0) - (2,2) - (4,0)
static const PLine zig({0, 0, 2, 2, 4, 0});
// closed square loop, first vertex equal to the last
static const PLine loop({0, 0, 2, 0, 2, 2, 0, 2, 0, 0});

TEST_CASE("Polyline and Ray intersection predicates, both directions") {
    const Ray crossing(Point(1, -1), Point(1, 0));   // shoots up through x = 1
    const Ray missing(Point(0, 3), Point(1, 3));     // runs above the zig-zag
    const Ray away(Point(1, -1), Point(1, -2));      // shoots down, away

    CHECK(zig.intersects(crossing));
    CHECK(crossing.intersects(zig));
    CHECK_FALSE(zig.intersects(missing));
    CHECK_FALSE(zig.intersects(away));

    CHECK(zig.interiorsIntersect(crossing));

    SUBCASE("the ray's source is not an interior point of the ray") {
        // Source on the first edge, shooting away from the polyline.
        const Ray fromEdge(Point(1, 1), Point(1, 5));
        CHECK(zig.intersects(fromEdge));
        CHECK_FALSE(zig.interiorsIntersect(fromEdge));
    }

    SUBCASE("touching only the polyline's extreme is not interior") {
        const Ray atExtreme(Point(0, 0), Point(-1, 1));
        CHECK(zig.intersects(atExtreme));
        CHECK_FALSE(zig.interiorsIntersect(atExtreme));
    }

    SUBCASE("a collinear overlap is interior") {
        const Ray alongEdge(Point(-1, -1), Point(0, 0));
        CHECK(zig.interiorsIntersect(alongEdge));
    }

    SUBCASE("passing through the apex vertex only") {
        const Ray throughApex(Point(2, 5), Point(2, 4));
        CHECK(zig.interiorsIntersect(throughApex));
    }
}

TEST_CASE("A ray contains a polyline only when the polyline lies along it") {
    const Ray diagonal(Point(0, 0), Point(1, 1));
    CHECK(diagonal.contains(PLine({1, 1, 3, 3})));
    CHECK(diagonal.contains(PLine({0, 0, 3, 3, 1, 1})));   // back-tracking
    CHECK_FALSE(diagonal.contains(PLine({-1, -1, 2, 2}))); // sticks out behind
    CHECK_FALSE(diagonal.contains(zig));
    // The relative interior excludes the source.
    CHECK(diagonal.interiorContains(PLine({1, 1, 3, 3})));
    CHECK_FALSE(diagonal.interiorContains(PLine({0, 0, 3, 3})));
    CHECK_FALSE(diagonal.boundaryContains(PLine({1, 1, 3, 3})));
    CHECK_FALSE(zig.contains(diagonal));   // a bounded polyline holds no ray
    CHECK_FALSE(zig.interiorContains(diagonal));
}

TEST_CASE("Polyline and Ray separates and crosses") {
    SUBCASE("a crossing ray is cut, and cuts") {
        const Ray crossing(Point(1, -1), Point(1, 0));
        CHECK(zig.separates(crossing));
        CHECK(crossing.separates(zig));
        CHECK(zig.crosses(crossing));
    }

    SUBCASE("a ray whose source lies on the polyline stays connected") {
        const Ray fromEdge(Point(1, 1), Point(1, 5));
        CHECK_FALSE(zig.separates(fromEdge));
        CHECK_FALSE(zig.crosses(fromEdge));
    }

    SUBCASE("a collinear overlap through one edge severs the source piece") {
        // The ray runs from (-1,-1) through the whole first edge and past the
        // apex: the piece before (0,0) is cut off from the piece after (2,2).
        const Ray alongEdge(Point(-1, -1), Point(0, 0));
        CHECK(zig.separates(alongEdge));
    }

    SUBCASE("touching only the extreme does not cut the polyline") {
        // Sourced at the extreme: the contact is the ray's own source, so
        // neither side is disconnected.
        const Ray atExtreme(Point(0, 0), Point(-1, 1));
        CHECK_FALSE(zig.separates(atExtreme));
        CHECK_FALSE(atExtreme.separates(zig));
        CHECK_FALSE(zig.crosses(atExtreme));
        // Passing through the extreme mid-ray: the contact splits the ray but
        // still only clips the polyline's end.
        const Ray throughExtreme(Point(-2, 2), Point(-1, 1));
        CHECK(zig.separates(throughExtreme));
        CHECK_FALSE(throughExtreme.separates(zig));
        CHECK_FALSE(zig.crosses(throughExtreme));
    }

    SUBCASE("set semantics: one crossing does not disconnect a loop") {
        const Ray fromInside(Point(1, 1), Point(1, 3));   // source inside the loop
        CHECK(loop.separates(fromInside));                // loop cuts the ray once
        CHECK_FALSE(fromInside.separates(loop));          // the loop reconnects
        CHECK_FALSE(loop.crosses(fromInside));
        const Ray through(Point(1, -1), Point(1, 0));     // crosses the loop twice
        CHECK(through.separates(loop));
        CHECK(loop.crosses(through));
    }

    SUBCASE("a single-vertex polyline separates through its point") {
        const PLine dot({Point(1, 1)});
        CHECK(dot.separates(Ray(Point(0, 0), Point(1, 1))));
        CHECK_FALSE(dot.separates(Ray(Point(1, 1), Point(2, 2))));  // the source
    }
}

TEST_CASE("Polyline and Ray distance") {
    using Rational = pgl::Rational<int>;

    const Ray missing(Point(0, 3), Point(1, 3));   // nearest at the apex (2,2)
    CHECK(zig.squaredDistance<Rational>(missing) == Rational(1));
    CHECK(missing.squaredDistance<Rational>(zig) == Rational(1));
    CHECK(zig.distanceL1<Rational>(missing) == Rational(1));
    CHECK(zig.distanceLInf<Rational>(missing) == Rational(1));
    CHECK(zig.squaredDistance<Rational>(Ray(Point(1, -1), Point(1, 0))) == Rational(0));

    // A ray pointing away: the nearest point is its source.
    const Ray away(Point(6, 0), Point(7, 0));
    CHECK(zig.squaredDistance<double>(away) == doctest::Approx(4.0));
    CHECK(zig.distanceL1(away) == 2);
    CHECK(zig.distanceLInf(away) == 2);
}
