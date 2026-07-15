#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <optional>
#include <variant>

using Point = pgl::Point<int>;
using Halfplane = pgl::Halfplane<Point>;
using Line = pgl::Line<Point>;
using OrientedLine = pgl::OrientedLine<Point>;
using Ray = pgl::Ray<Point>;
using Region = pgl::HalfplaneIntersection<Point>;
using Q = pgl::Rational<int>;
using QPoint = pgl::Point<Q>;

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
Region strip() {
    return Region({yGE0, yLE1});
}
}  // namespace

TEST_CASE("Line containment") {
    // Only regions made of constraints parallel to the line can contain it.
    CHECK(Region().contains(Line(Point(0, 0), Point(1, 1))));
    CHECK(Region(yGE0).contains(Line(Point(0, 0), Point(1, 0))));
    CHECK(Region(yGE0).contains(Line(Point(0, 1), Point(1, 1))));
    CHECK(!Region(yGE0).contains(Line(Point(0, 0), Point(0, 1))));
    CHECK(strip().contains(Line(Point(0, 0), Point(1, 0))));
    CHECK(!strip().contains(Line(Point(0, 0), Point(1, 1))));
    CHECK(!unitSquare().contains(Line(Point(0, 0), Point(1, 0))));
    CHECK(!quadrant().contains(Line(Point(0, 0), Point(1, 0))));

    // A degenerate line region contains exactly its carrier line.
    const Region axis({yGE0, Halfplane(1, 0, 0, 0)});
    CHECK(axis.contains(Line(Point(0, 0), Point(1, 0))));
    CHECK(!axis.contains(Line(Point(0, 1), Point(1, 1))));
}

TEST_CASE("Line boundary and interior containment") {
    CHECK(strip().boundaryContains(Line(Point(0, 0), Point(1, 0))));
    CHECK(strip().boundaryContains(Line(Point(0, 1), Point(1, 1))));
    CHECK(!strip().boundaryContains(Line(Point(0, 0), Point(2, 1))));
    CHECK(Region(yGE0).boundaryContains(Line(Point(0, 0), Point(1, 0))));
    CHECK(!Region(yGE0).boundaryContains(Line(Point(0, 1), Point(1, 1))));
    CHECK(!Region().boundaryContains(Line(Point(0, 0), Point(1, 0))));

    // A wide strip strictly contains interior parallels.
    const Region wide({yGE0, Halfplane(1, 2, 0, 2)});  // 0 <= y <= 2
    CHECK(wide.interiorContains(Line(Point(0, 1), Point(1, 1))));
    CHECK(!wide.interiorContains(Line(Point(0, 0), Point(1, 0))));
    CHECK(Region().interiorContains(Line(Point(0, 0), Point(1, 0))));

    // The degenerate line region equals its own boundary.
    const Region axis({yGE0, Halfplane(1, 0, 0, 0)});
    CHECK(axis.boundaryContains(Line(Point(0, 0), Point(1, 0))));
    CHECK(!axis.interiorContains(Line(Point(0, 0), Point(1, 0))));
}

TEST_CASE("Line intersection tests") {
    const Region k = unitSquare();
    CHECK(k.intersects(Line(Point(-1, -1), Point(2, 2))));
    CHECK(k.intersects(Line(Point(-1, 0), Point(2, 0))));
    CHECK(k.intersects(Line(Point(-1, 1), Point(1, -1))));  // through (0,0) only
    CHECK(!k.intersects(Line(Point(2, 0), Point(2, 1))));
    CHECK(!k.intersects(Line(Point(-2, 1), Point(1, -2))));

    CHECK(quadrant().intersects(Line(Point(-5, 6), Point(5, 6))));
    CHECK(!strip().intersects(Line(Point(0, 2), Point(1, 2))));
    CHECK(strip().intersects(Line(Point(0, 1), Point(1, 1))));
}

TEST_CASE("Line separates and crosses") {
    const Region k = unitSquare();
    // The square eats a bounded interval out of a crossing line.
    CHECK(k.separates(Line(Point(-1, -1), Point(2, 2))));
    CHECK(k.separates(Line(Point(-1, 0), Point(2, 0))));    // eats the bottom edge
    CHECK(k.separates(Line(Point(-1, 1), Point(1, -1))));   // eats one corner point
    CHECK(!k.separates(Line(Point(2, 0), Point(2, 1))));    // disjoint

    // The strip swallows an unbounded piece of a parallel line, and any
    // non-parallel line keeps both ends: only bounded chords separate.
    CHECK(!strip().separates(Line(Point(0, 0), Point(1, 0))));
    CHECK(strip().separates(Line(Point(0, 0), Point(0, 1))));

    // Mutual separation needs the region strictly on both sides of the line.
    CHECK(k.crosses(Line(Point(-1, -1), Point(2, 2))));
    CHECK(!k.crosses(Line(Point(-1, 0), Point(2, 0))));     // the region stays above
    CHECK(!k.crosses(Line(Point(-1, 1), Point(1, -1))));    // vertex touch only
    CHECK(strip().crosses(Line(Point(0, 0), Point(0, 1))));
}

TEST_CASE("Line intersection constructions produce all four result shapes") {
    SUBCASE("segment through a bounded region") {
        const auto isec = unitSquare().intersection<Q>(Line(Point(-1, -1), Point(2, 2)));
        REQUIRE(isec.has_value());
        REQUIRE(isec->index() == 1);
        const auto seg = std::get<1>(*isec);
        CHECK(seg == pgl::Segment<QPoint>(QPoint(Q(0), Q(0)), QPoint(Q(1), Q(1))));
    }
    SUBCASE("point at a vertex touch") {
        const auto isec = unitSquare().intersection<Q>(Line(Point(-1, 1), Point(1, -1)));
        REQUIRE(isec.has_value());
        REQUIRE(isec->index() == 0);
        CHECK(std::get<0>(*isec) == QPoint(Q(0), Q(0)));
    }
    SUBCASE("ray out of an unbounded region") {
        const auto isec = quadrant().intersection<Q>(Line(Point(-2, 1), Point(2, 1)));
        REQUIRE(isec.has_value());
        REQUIRE(isec->index() == 2);
        CHECK(std::get<2>(*isec).source() == QPoint(Q(0), Q(1)));
    }
    SUBCASE("line kept whole by a strip") {
        const auto isec = strip().intersection<Q>(Line(Point(0, 1), Point(1, 1)));
        REQUIRE(isec.has_value());
        CHECK(isec->index() == 3);
    }
    SUBCASE("nothing for a disjoint line") {
        CHECK(!unitSquare().intersection<Q>(Line(Point(2, 0), Point(2, 1))).has_value());
    }
}

TEST_CASE("Oriented lines behave like their unoriented counterparts") {
    const Region k = unitSquare();
    const OrientedLine l(Point(2, 2), Point(-1, -1));
    CHECK(k.intersects(l));
    CHECK(k.separates(l));
    CHECK(k.crosses(l));
    CHECK(!k.contains(l));
    CHECK(!k.boundaryContains(l));
    CHECK(!k.interiorContains(l));
    const auto isec = k.intersection<Q>(l);
    REQUIRE(isec.has_value());
    CHECK(isec->index() == 1);
}

TEST_CASE("Ray containment") {
    const Region wedge = quadrant();
    CHECK(wedge.contains(Ray(Point(1, 1), Point(2, 2))));
    CHECK(wedge.contains(Ray(Point(0, 0), Point(1, 0))));
    CHECK(!wedge.contains(Ray(Point(1, 1), Point(0, 0))));   // leaves through the corner
    CHECK(!wedge.contains(Ray(Point(-1, 1), Point(1, 1))));  // source outside
    CHECK(!unitSquare().contains(Ray(Point(0, 0), Point(1, 1))));  // bounded region

    // Boundary containment: the ray must ride an edge.
    CHECK(wedge.boundaryContains(Ray(Point(1, 0), Point(2, 0))));
    CHECK(!wedge.boundaryContains(Ray(Point(1, 0), Point(0, 0))));  // exits at the corner
    CHECK(!wedge.boundaryContains(Ray(Point(1, 1), Point(2, 2))));

    // Interior containment on a half-plane region.
    const Region half(yGE0);
    CHECK(half.interiorContains(Ray(Point(0, 1), Point(1, 1))));
    CHECK(!half.interiorContains(Ray(Point(0, 1), Point(1, 0))));  // sinks to the boundary
    CHECK(!half.interiorContains(Ray(Point(0, 0), Point(1, 1))));  // source on the boundary
}

TEST_CASE("Ray intersection tests") {
    const Region k = unitSquare();
    CHECK(k.intersects(Ray(Point(2, 2), Point(-1, -1))));
    CHECK(!k.intersects(Ray(Point(2, 2), Point(3, 3))));   // points away
    CHECK(k.intersects(Ray(Point(1, 1), Point(3, 3))));    // source touches
    CHECK(quadrant().intersects(Ray(Point(-1, 5), Point(1, 4))));

    CHECK(k.interiorsIntersect(Ray(Point(2, 2), Point(-1, -1))));
    CHECK(!k.interiorsIntersect(Ray(Point(1, 1), Point(3, 3))));
    CHECK(!k.interiorsIntersect(Ray(Point(-1, 0), Point(2, 0))));  // rides the edge
}

TEST_CASE("Ray separates and crosses") {
    const Region k = unitSquare();
    // The ray keeps a piece behind the source and a piece past the region.
    CHECK(k.separates(Ray(Point(-1, -1), Point(2, 2))));
    CHECK(!k.separates(Ray(Point(0, 0), Point(2, 2))));    // source inside
    // A ray pointing into the wedge's recession cone keeps only the piece
    // behind the source: no disconnection.
    const Region wedge = quadrant();
    CHECK(!wedge.separates(Ray(Point(-1, 5), Point(1, 6))));
    // Aimed across the wedge, the ray loses a bounded middle piece.
    CHECK(wedge.separates(Ray(Point(-1, 5), Point(1, 4))));

    CHECK(k.crosses(Ray(Point(-1, -1), Point(2, 2))));
    CHECK(!k.crosses(Ray(Point(-1, 0), Point(2, 0))));
    // The chord starts behind the ray's source: no mutual separation.
    CHECK(!k.crosses(Ray(Point(1, 1), Point(3, 3))));
}

TEST_CASE("Ray intersection constructions") {
    SUBCASE("segment through a bounded region") {
        const auto isec = unitSquare().intersection<Q>(Ray(Point(-1, -1), Point(2, 2)));
        REQUIRE(isec.has_value());
        REQUIRE(isec->index() == 1);
        CHECK(std::get<1>(*isec) == pgl::Segment<QPoint>(QPoint(Q(0), Q(0)), QPoint(Q(1), Q(1))));
    }
    SUBCASE("ray clipped to a ray by a wedge") {
        const auto isec = quadrant().intersection<Q>(Ray(Point(-2, 1), Point(-1, 1)));
        REQUIRE(isec.has_value());
        REQUIRE(isec->index() == 2);
        CHECK(std::get<2>(*isec).source() == QPoint(Q(0), Q(1)));
    }
    SUBCASE("source touch gives a point") {
        const auto isec = unitSquare().intersection<Q>(Ray(Point(1, 1), Point(3, 3)));
        REQUIRE(isec.has_value());
        REQUIRE(isec->index() == 0);
        CHECK(std::get<0>(*isec) == QPoint(Q(1), Q(1)));
    }
    SUBCASE("nothing for a ray pointing away") {
        CHECK(!unitSquare().intersection<Q>(Ray(Point(2, 2), Point(3, 3))).has_value());
    }
}

TEST_CASE("Halfplane pair predicates") {
    const Region k = unitSquare();
    CHECK(k.intersects(yGE0));
    CHECK(k.intersects(Halfplane(0, 1, 1, 1)));            // y >= 1 touches the top
    CHECK(!k.intersects(Halfplane(0, 2, 1, 2)));           // y >= 2 misses
    CHECK(k.interiorsIntersect(yGE0));
    CHECK(!k.interiorsIntersect(Halfplane(0, 1, 1, 1)));   // touch only

    // Containment of a half-plane needs an unbounded region.
    CHECK(Region().contains(yGE0));
    CHECK(Region(yGE0).contains(Halfplane(0, 1, 1, 1)));   // y >= 1 inside y >= 0
    CHECK(!Region(yGE0).contains(Halfplane(1, -1, 0, -1))); // y <= -1 outside
    CHECK(!quadrant().contains(yGE0));
    CHECK(!k.contains(yGE0));
    CHECK(Region(yGE0).interiorContains(Halfplane(0, 1, 1, 1)));
    CHECK(!Region(yGE0).interiorContains(yGE0));
    CHECK(!k.boundaryContains(yGE0));

    // Separation: strips and wedges can disconnect a half-plane.
    const Region band = strip();
    CHECK(band.separates(Halfplane(0, -1, 1, -1)));   // y >= -1 loses its middle
    CHECK(!band.separates(Halfplane(0, 1, 1, 1)));    // contact reaches both line ends
    const Region vertical({xGE0, Halfplane(1, 0, 1, 1)});  // 0 <= x <= 1
    CHECK(vertical.separates(yGE0));
    CHECK(!unitSquare().separates(yGE0));             // bounded regions never separate
    CHECK(!Region(yGE0).separates(Halfplane(0, -1, 1, -1)));

    // A half-plane never separates the convex region, so crosses is false.
    CHECK(!band.crosses(Halfplane(0, -1, 1, -1)));
    CHECK(!k.crosses(yGE0));
}

TEST_CASE("Halfplane intersection construction chains exactly") {
    Region k;  // the whole plane
    const auto once = k.intersection(yGE0);
    CHECK(once.size() == 1);
    const auto twice = once.intersection(xGE0);
    CHECK(twice == Region({yGE0, xGE0}));
    const auto vanished = twice.intersection(Halfplane(1, -1, 0, -1));  // y <= -1
    CHECK(vanished.isEmpty());
}
