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

TEST_CASE("Polyline intersects Segment") {
    CHECK(zig.intersects(Segment(Point(1, -1), Point(1, 3))));   // crosses the first edge
    CHECK(zig.intersects(Segment(Point(2, 2), Point(2, 5))));    // touches the apex vertex
    CHECK(zig.intersects(Segment(Point(0, 0), Point(0, -3))));   // touches the extreme
    CHECK(!zig.intersects(Segment(Point(0, 3), Point(4, 3))));   // passes above
    CHECK(!zig.intersects(Segment(Point(5, 0), Point(6, 0))));

    SUBCASE("single-vertex polyline") {
        const PLine dot({Point(1, 1)});
        CHECK(dot.intersects(Segment(Point(0, 0), Point(2, 2))));
        CHECK(!dot.intersects(Segment(Point(0, 1), Point(0, 5))));
    }

    SUBCASE("empty polyline") {
        CHECK(!PLine().intersects(Segment(Point(0, 0), Point(1, 1))));
    }
}

TEST_CASE("Polyline contains Segment") {
    CHECK(zig.contains(Segment(Point(1, 1), Point(2, 2))));      // inside the first edge
    CHECK(zig.contains(Segment(Point(3, 1), Point(4, 0))));      // inside the second edge
    CHECK(!zig.contains(Segment(Point(0, 0), Point(4, 0))));     // chord under the zig-zag
    CHECK(!zig.contains(Segment(Point(1, 1), Point(3, 1))));     // spans the bend

    SUBCASE("a segment covered by two non-consecutive collinear edges") {
        // Edges (0,0)-(2,0) and (1,0)-(4,0) overlap on y = 0; together they
        // cover (0,0)-(4,0) even though they are not consecutive.
        const PLine overlapping({0, 0, 2, 0, 1, 3, 1, 0, 4, 0});
        CHECK(overlapping.contains(Segment(Point(0, 0), Point(4, 0))));
        CHECK(overlapping.contains(Segment(Point(1, 0), Point(2, 0))));
        // A gap breaks the coverage: edges (0,0)-(1,0) and (2,0)-(4,0).
        const PLine gapped({0, 0, 1, 0, 1, 3, 2, 0, 4, 0});
        CHECK(!gapped.contains(Segment(Point(0, 0), Point(4, 0))));
        CHECK(gapped.contains(Segment(Point(2, 0), Point(3, 0))));
    }
}

TEST_CASE("Segment contains Polyline") {
    const Segment diagonal(Point(0, 0), Point(4, 4));
    CHECK(diagonal.contains(PLine({1, 1, 2, 2})));
    CHECK(diagonal.contains(PLine({1, 1, 3, 3, 2, 2})));  // back-tracking but on the segment
    CHECK(!diagonal.contains(zig));
    CHECK(diagonal.contains(PLine()));

    CHECK(diagonal.interiorContains(PLine({1, 1, 2, 2})));
    CHECK(!diagonal.interiorContains(PLine({0, 0, 2, 2})));  // touches the endpoint
    CHECK(!diagonal.boundaryContains(zig));
}

TEST_CASE("Polyline interiorContains Segment") {
    CHECK(zig.interiorContains(Segment(Point(1, 1), Point(2, 2))));
    CHECK(!zig.interiorContains(Segment(Point(0, 0), Point(1, 1))));  // touches the extreme
    CHECK(!zig.interiorContains(Segment(Point(1, 1), Point(3, 1))));  // not even contained

    SUBCASE("a revisited extreme excludes a covering segment") {
        // The polyline revisits its extreme point (0,0) in the middle of the
        // last edge, so a segment through (0,0) is never interior.
        const PLine revisits({0, 0, 2, 0, 2, 2, -1, -1, 1, 1});
        CHECK(revisits.contains(Segment(Point(0, 0), Point(1, 1))));
        CHECK(!revisits.interiorContains(Segment(Point(-1, -1), Point(1, 1))));
    }
}

TEST_CASE("Polyline interiorsIntersect Segment") {
    SUBCASE("crossing an edge interior") {
        CHECK(zig.interiorsIntersect(Segment(Point(1, -1), Point(1, 3))));
    }

    SUBCASE("passing through a non-extreme vertex only") {
        CHECK(zig.interiorsIntersect(Segment(Point(1, 2), Point(3, 2))));
    }

    SUBCASE("touching only at the polyline's extreme is not interior") {
        CHECK(!zig.interiorsIntersect(Segment(Point(0, 0), Point(0, 2))));
        CHECK(!zig.interiorsIntersect(Segment(Point(-2, 0), Point(0, 0))));
    }

    SUBCASE("touching only at the segment's endpoint is not interior") {
        CHECK(!zig.interiorsIntersect(Segment(Point(1, 1), Point(1, 5))));
    }

    SUBCASE("collinear overlap is interior") {
        CHECK(zig.interiorsIntersect(Segment(Point(-1, -1), Point(1, 1))));
    }

    SUBCASE("disjoint shapes") {
        CHECK(!zig.interiorsIntersect(Segment(Point(0, 3), Point(4, 3))));
    }
}

TEST_CASE("Polyline separates Segment and vice versa") {
    SUBCASE("a chain cutting through the segment separates it") {
        CHECK(zig.separates(Segment(Point(1, -1), Point(1, 3))));
        CHECK(Segment(Point(1, -1), Point(1, 3)).separates(zig));
        CHECK(zig.crosses(Segment(Point(1, -1), Point(1, 3))));
    }

    SUBCASE("touching at an endpoint does not separate") {
        const Segment touch(Point(2, 2), Point(2, 5));
        CHECK(!zig.separates(touch));
        CHECK(!zig.crosses(touch));
    }

    SUBCASE("a chord of the polyline separates the polyline but not itself") {
        // The vertical segment through the apex cuts the zig-zag at (2,2);
        // the zig-zag does not cut the segment, whose endpoint sits on it.
        const Segment throughApex(Point(2, 2), Point(2, -1));
        CHECK(throughApex.separates(zig));
        CHECK(!zig.separates(throughApex));
        CHECK(!zig.crosses(throughApex));
    }

    SUBCASE("a collinear overlap swallowing one extreme does not separate") {
        const Segment cover(Point(-1, -1), Point(1, 1));
        CHECK(!cover.separates(zig));
    }

    SUBCASE("set semantics: a closed polyline resists a single crossing") {
        const PLine loop({0, 0, 2, 0, 2, 2, 0, 2, 0, 0});
        const Segment oneCut(Point(1, -1), Point(1, 1));
        CHECK(!oneCut.separates(loop));
        const Segment twoCuts(Point(1, -1), Point(1, 3));
        CHECK(twoCuts.separates(loop));
    }

    SUBCASE("a single-vertex polyline separates through its point") {
        const PLine dot({Point(1, 1)});
        CHECK(dot.separates(Segment(Point(0, 0), Point(2, 2))));
        CHECK(!dot.separates(Segment(Point(1, 1), Point(2, 2))));
    }
}

TEST_CASE("Polyline distances to Segment") {
    CHECK(zig.squaredDistance(Segment(Point(1, -1), Point(1, 3))) == 0);
    CHECK(zig.squaredDistance<double>(Segment(Point(0, 3), Point(4, 3))) ==
          doctest::Approx(1.0));  // closest at the apex (2,2), one unit below the segment
    CHECK(Segment(Point(0, 3), Point(4, 3)).squaredDistance<double>(zig) == doctest::Approx(1.0));
    CHECK(zig.squaredDistance<double>(Segment(Point(6, 0), Point(7, 0))) == doctest::Approx(4.0));

    CHECK(zig.distanceL1(Segment(Point(6, 0), Point(7, 0))) == 2);
    CHECK(Segment(Point(6, 0), Point(7, 0)).distanceL1(zig) == 2);
    CHECK(zig.distanceLInf(Segment(Point(6, 0), Point(7, 0))) == 2);
    CHECK(zig.distanceL1(Segment(Point(0, 3), Point(4, 3))) == 1);
}

TEST_CASE("Polyline and Segment intersection pieces") {
    SUBCASE("a segment crossing both edges") {
        const auto pieces = zig.intersection(Segment(Point(0, 1), Point(4, 1)));
        REQUIRE(pieces.size() == 2);
        REQUIRE(std::holds_alternative<Point>(pieces[0]));
        CHECK(std::get<Point>(pieces[0]) == Point(1, 1));
        REQUIRE(std::holds_alternative<Point>(pieces[1]));
        CHECK(std::get<Point>(pieces[1]) == Point(3, 1));
    }

    SUBCASE("collinear overlap yields a single segment") {
        const auto pieces = zig.intersection(Segment(Point(1, 1), Point(2, 2)));
        REQUIRE(pieces.size() == 1);
        REQUIRE(std::holds_alternative<Segment>(pieces[0]));
        CHECK(std::get<Segment>(pieces[0]) == Segment(Point(1, 1), Point(2, 2)));
    }

    SUBCASE("a touch at the apex is reported once") {
        const auto pieces = zig.intersection(Segment(Point(2, 2), Point(2, 5)));
        REQUIRE(pieces.size() == 1);
        REQUIRE(std::holds_alternative<Point>(pieces[0]));
        CHECK(std::get<Point>(pieces[0]) == Point(2, 2));
    }

    SUBCASE("overlaps with non-consecutive collinear edges merge") {
        // Edges (0,0)-(2,0) and (1,0)-(4,0) jointly cover the segment; the
        // vertical edge's touch at (1,0) is absorbed by the merged overlap.
        const PLine overlapping({0, 0, 2, 0, 1, 3, 1, 0, 4, 0});
        const auto pieces = overlapping.intersection(Segment(Point(0, 0), Point(4, 0)));
        REQUIRE(pieces.size() == 1);
        REQUIRE(std::holds_alternative<Segment>(pieces[0]));
        CHECK(std::get<Segment>(pieces[0]) == Segment(Point(0, 0), Point(4, 0)));
    }

    SUBCASE("a coverage gap yields two segments") {
        // Edges (0,0)-(1,0) and (2,0)-(4,0) leave the gap (1,0)-(2,0) open.
        const PLine gapped({0, 0, 1, 0, 1, 3, 2, 0, 4, 0});
        const auto pieces = gapped.intersection(Segment(Point(0, 0), Point(4, 0)));
        REQUIRE(pieces.size() == 2);
        REQUIRE(std::holds_alternative<Segment>(pieces[0]));
        CHECK(std::get<Segment>(pieces[0]) == Segment(Point(0, 0), Point(1, 0)));
        REQUIRE(std::holds_alternative<Segment>(pieces[1]));
        CHECK(std::get<Segment>(pieces[1]) == Segment(Point(2, 0), Point(4, 0)));
    }

    SUBCASE("the segment forwards to the polyline's overload") {
        const auto pieces = Segment(Point(0, 1), Point(4, 1)).intersection(zig);
        REQUIRE(pieces.size() == 2);
        REQUIRE(std::holds_alternative<Point>(pieces[0]));
        CHECK(std::get<Point>(pieces[0]) == Point(1, 1));
    }

    SUBCASE("oriented segments delegate to the unoriented overload") {
        const auto pieces =
            zig.intersection(pgl::OrientedSegment<Point>(Point(4, 1), Point(0, 1)));
        REQUIRE(pieces.size() == 2);
        REQUIRE(std::holds_alternative<Point>(pieces[0]));
        CHECK(std::get<Point>(pieces[0]) == Point(1, 1));
    }
}
