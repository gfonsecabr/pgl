#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <optional>
#include <variant>

using Point = pgl::Point<int>;
using Halfplane = pgl::Halfplane<Point>;
using Segment = pgl::Segment<Point>;
using OrientedSegment = pgl::OrientedSegment<Point>;
using Region = pgl::HalfplaneIntersection<Point>;
using Q = pgl::Rational<int>;
using QPoint = pgl::Point<Q>;
using QSegment = pgl::Segment<QPoint>;

namespace {
const Halfplane yGE0(0, 0, 1, 0);   // y >= 0
const Halfplane xLE1(1, 0, 1, 1);   // x <= 1
const Halfplane yLE1(1, 1, 0, 1);   // y <= 1
const Halfplane xGE0(0, 1, 0, 0);   // x >= 0

Region unitSquare() {
    return Region({yGE0, xLE1, yLE1, xGE0});
}
Region quadrant() {
    return Region({yGE0, xGE0});
}
}  // namespace

TEST_CASE("Segment containment") {
    const Region k = unitSquare();
    CHECK(k.contains(Segment(Point(0, 0), Point(1, 1))));
    CHECK(k.contains(Segment(Point(0, 0), Point(1, 0))));
    CHECK(!k.contains(Segment(Point(0, 0), Point(2, 2))));
    CHECK(!k.contains(Segment(Point(-1, 0), Point(1, 0))));

    const Region wedge = quadrant();
    CHECK(wedge.contains(Segment(Point(0, 0), Point(1000, 1))));
    CHECK(!wedge.contains(Segment(Point(-1, 1), Point(1, 1))));
}

TEST_CASE("Segment boundary containment") {
    const Region k = unitSquare();
    CHECK(k.boundaryContains(Segment(Point(0, 0), Point(1, 0))));
    CHECK(k.boundaryContains(Segment(Point(1, 0), Point(1, 1))));
    CHECK(!k.boundaryContains(Segment(Point(0, 0), Point(1, 1))));  // a chord
    CHECK(!k.boundaryContains(Segment(Point(0, 0), Point(2, 0))));  // leaves the square

    // On a degenerate region the boundary is the region itself.
    const Region line({yGE0, Halfplane(1, 0, 0, 0)});
    CHECK(line.boundaryContains(Segment(Point(-3, 0), Point(5, 0))));
    CHECK(!line.boundaryContains(Segment(Point(0, 0), Point(1, 1))));
}

TEST_CASE("Segment interior containment") {
    const Region big({yGE0, xGE0, Halfplane(4, 0, 4, 1), Halfplane(1, 4, 0, 4)});
    CHECK(big.interiorContains(Segment(Point(1, 1), Point(3, 3))));
    CHECK(!big.interiorContains(Segment(Point(0, 1), Point(3, 3))));  // touches the boundary
    CHECK(!big.interiorContains(Segment(Point(1, 1), Point(5, 5))));
}

TEST_CASE("Segment intersection tests") {
    const Region k = unitSquare();
    CHECK(k.intersects(Segment(Point(0, 0), Point(1, 1))));       // inside
    CHECK(k.intersects(Segment(Point(-1, 0), Point(2, 0))));      // along an edge line
    CHECK(k.intersects(Segment(Point(-1, -1), Point(2, 2))));     // crossing
    CHECK(k.intersects(Segment(Point(1, 1), Point(2, 2))));       // touching a vertex
    CHECK(k.intersects(Segment(Point(-1, 1), Point(1, -1))));     // touching the corner (0,0)
    CHECK(!k.intersects(Segment(Point(2, 0), Point(2, 2))));      // to the right
    CHECK(!k.intersects(Segment(Point(-1, 2), Point(2, 5))));     // above, slanted
    CHECK(!k.intersects(Segment(Point(-2, 1), Point(1, -2))));    // just misses (0,0)

    const Region wedge = quadrant();
    CHECK(wedge.intersects(Segment(Point(-1, 5), Point(5, -1))));
    CHECK(!wedge.intersects(Segment(Point(-2, 5), Point(1, -4))));
}

TEST_CASE("Segment interiorsIntersect") {
    const Region k = unitSquare();
    CHECK(k.interiorsIntersect(Segment(Point(-1, -1), Point(2, 2))));
    // Sliding along an edge stays out of the interior.
    CHECK(!k.interiorsIntersect(Segment(Point(-1, 0), Point(2, 0))));
    // Touching a vertex only is not an interior meeting.
    CHECK(!k.interiorsIntersect(Segment(Point(-1, 1), Point(1, -1))));
    // A degenerate region has no interior.
    const Region line({yGE0, Halfplane(1, 0, 0, 0)});
    CHECK(!line.interiorsIntersect(Segment(Point(0, -1), Point(0, 1))));
}

TEST_CASE("Segment separates and crosses") {
    const Region k = unitSquare();
    // The square swallows the middle of a segment passing through it.
    CHECK(k.separates(Segment(Point(-1, -1), Point(2, 2))));
    CHECK(!k.separates(Segment(Point(0, 0), Point(2, 2))));   // one endpoint inside
    CHECK(!k.separates(Segment(Point(2, 0), Point(3, 0))));   // disjoint

    // The segment covers the square's chord, so they mutually separate.
    CHECK(k.crosses(Segment(Point(-1, -1), Point(2, 2))));
    // The chord is covered but the region only touches the segment's interior
    // along the boundary line: no mutual separation.
    CHECK(!k.crosses(Segment(Point(-1, 0), Point(2, 0))));

    // The segment ends inside the region: the region is not disconnected.
    const Region wedge = quadrant();
    CHECK(wedge.separates(Segment(Point(-1, 5), Point(5, -1))));
    CHECK(!wedge.crosses(Segment(Point(-1, 5), Point(1, 1))));
    // The wedge's chord on x + y = 4 is bounded and covered: mutual separation.
    CHECK(wedge.crosses(Segment(Point(-1, 5), Point(5, -1))));
    // A shorter segment leaves part of the chord uncovered.
    CHECK(!wedge.crosses(Segment(Point(-1, 5), Point(3, 1))));
}

TEST_CASE("Segment intersection constructions") {
    const Region k = unitSquare();

    SUBCASE("a crossing segment clips to a segment") {
        const auto isec = k.intersection<Q>(Segment(Point(-1, -1), Point(2, 2)));
        REQUIRE(isec.has_value());
        REQUIRE(isec->index() == 1);
        const auto seg = std::get<1>(*isec);
        CHECK(seg == QSegment(QPoint(Q(0), Q(0)), QPoint(Q(1), Q(1))));
    }

    SUBCASE("an inside segment is returned whole") {
        const auto isec = k.intersection<Q>(Segment(Point(0, 0), Point(1, 0)));
        REQUIRE(isec.has_value());
        REQUIRE(isec->index() == 1);
        CHECK(std::get<1>(*isec) == QSegment(QPoint(Q(0), Q(0)), QPoint(Q(1), Q(0))));
    }

    SUBCASE("a vertex touch gives a point") {
        const auto isec = k.intersection<Q>(Segment(Point(-1, 1), Point(1, -1)));
        REQUIRE(isec.has_value());
        REQUIRE(isec->index() == 0);
        CHECK(std::get<0>(*isec) == QPoint(Q(0), Q(0)));
    }

    SUBCASE("a fractional crossing is exact with rational results") {
        // x + 2y <= 1 triangle: the segment x = 0 .. clipped at y = 1/2.
        const Region t({yGE0, xGE0, Halfplane(1, 0, -1, 1)});
        const auto isec = t.intersection<Q>(Segment(Point(0, -1), Point(0, 2)));
        REQUIRE(isec.has_value());
        REQUIRE(isec->index() == 1);
        CHECK(std::get<1>(*isec) == QSegment(QPoint(Q(0), Q(0)), QPoint(Q(0), Q(1, 2))));
    }

    SUBCASE("a disjoint segment gives nothing") {
        CHECK(!k.intersection<Q>(Segment(Point(2, 0), Point(3, 3))).has_value());
    }

    SUBCASE("the whole plane returns the segment") {
        const Region plane;
        const auto isec = plane.intersection<Q>(Segment(Point(1, 2), Point(3, 4)));
        REQUIRE(isec.has_value());
        CHECK(isec->index() == 1);
    }
}

TEST_CASE("Oriented segments behave like their unoriented counterparts") {
    const Region k = unitSquare();
    const OrientedSegment forward(Point(2, 2), Point(-1, -1));
    CHECK(k.intersects(forward));
    CHECK(k.contains(OrientedSegment(Point(1, 1), Point(0, 0))));
    CHECK(k.separates(forward));
    CHECK(k.crosses(forward));
    CHECK(!k.interiorContains(forward));
    const auto isec = k.intersection<Q>(forward);
    REQUIRE(isec.has_value());
    CHECK(isec->index() == 1);
}
