#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <variant>

#include "pgl.hpp"

TEST_CASE("Convex containment of OrientedSegment, and OrientedSegment containment of Convex") {
    using Point = pgl::Point<int>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using Segment = pgl::Segment<Point>;
    using Convex = pgl::Convex<Point>;

    // Unit square [0,4]x[0,4]
    const Convex sq(std::vector<Point>{{0, 0}, {4, 0}, {4, 4}, {0, 4}});
    const OrientedSegment inner({3, 1}, {1, 3});           // strictly inside
    const OrientedSegment on_edge({3, 0}, {1, 0});         // on the bottom edge
    const OrientedSegment crossing({5, 2}, {-1, 2});       // crosses left-right

    SUBCASE("Convex contains and interiorContains a wholly interior OrientedSegment") {
        CHECK_MESSAGE(sq.contains(inner), sq, " contains ", inner);
        CHECK_MESSAGE(sq.interiorContains(inner), sq, " interiorContains ", inner);
    }

    SUBCASE("Convex contains a boundary OrientedSegment but does not interiorContain it") {
        CHECK_MESSAGE(sq.contains(on_edge), sq, " contains ", on_edge);
        CHECK_FALSE_MESSAGE(sq.interiorContains(on_edge), sq, " interiorContains ", on_edge);
    }

    SUBCASE("Convex boundaryContains an OS that lies exactly on an edge") {
        CHECK_MESSAGE(sq.boundaryContains(on_edge), sq, " boundaryContains ", on_edge);
        CHECK_FALSE_MESSAGE(sq.boundaryContains(inner), sq, " boundaryContains ", inner);
        CHECK_FALSE_MESSAGE(sq.boundaryContains(crossing), sq, " boundaryContains ", crossing);
    }

    SUBCASE("Convex does not contain a crossing OrientedSegment") {
        CHECK_FALSE_MESSAGE(sq.contains(crossing), sq, " contains ", crossing);
    }

    SUBCASE("OrientedSegment never contains an area Convex polygon") {
        CHECK_FALSE_MESSAGE(inner.contains(sq), inner, " contains ", sq);
        CHECK_FALSE_MESSAGE(inner.interiorContains(sq), inner, " interiorContains ", sq);
    }
}

TEST_CASE("Convex and OrientedSegment intersection predicates, both directions") {
    using Point = pgl::Point<int>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using Convex = pgl::Convex<Point>;

    const Convex sq(std::vector<Point>{{0, 0}, {4, 0}, {4, 4}, {0, 4}});
    const OrientedSegment crossing({5, 2}, {-1, 2});
    const OrientedSegment on_edge({3, 0}, {1, 0});

    SUBCASE("crossing: both intersect and both interiors intersect") {
        CHECK_MESSAGE(sq.intersects(crossing), sq, " intersects ", crossing);
        CHECK_MESSAGE(crossing.intersects(sq), crossing, " intersects ", sq);
        CHECK_MESSAGE(sq.interiorsIntersect(crossing), sq, " interiorsIntersect ", crossing);
        CHECK_MESSAGE(crossing.interiorsIntersect(sq), crossing, " interiorsIntersect ", sq);
    }

    SUBCASE("edge-only: intersects but interiors do not") {
        CHECK_MESSAGE(sq.intersects(on_edge), sq, " intersects ", on_edge);
        CHECK_FALSE_MESSAGE(sq.interiorsIntersect(on_edge), sq, " interiorsIntersect ", on_edge);
        CHECK_FALSE_MESSAGE(on_edge.interiorsIntersect(sq), on_edge, " interiorsIntersect ", sq);
    }

    SUBCASE("disjoint: no intersection") {
        const OrientedSegment outside({5, 5}, {6, 6});
        CHECK_FALSE_MESSAGE(sq.intersects(outside), sq, " intersects ", outside);
        CHECK_FALSE_MESSAGE(outside.intersects(sq), outside, " intersects ", sq);
    }
}

TEST_CASE("Convex and OrientedSegment separates and crosses, both directions") {
    using Point = pgl::Point<int>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using Convex = pgl::Convex<Point>;

    const Convex sq(std::vector<Point>{{0, 0}, {4, 0}, {4, 4}, {0, 4}});

    SUBCASE("crossing OS: both separate each other and both cross") {
        const OrientedSegment crossing({5, 2}, {-1, 2});
        CHECK_MESSAGE(sq.separates(crossing), sq, " separates ", crossing);
        CHECK_MESSAGE(crossing.separates(sq), crossing, " separates ", sq);
        CHECK_MESSAGE(sq.crosses(crossing), sq, " crosses ", crossing);
        CHECK_MESSAGE(crossing.crosses(sq), crossing, " crosses ", sq);
    }

    SUBCASE("edge-only OS: no separation") {
        const OrientedSegment touch({4, 1}, {6, 1}); // endpoint on right edge
        CHECK_FALSE_MESSAGE(sq.separates(touch), sq, " separates ", touch);
        CHECK_FALSE_MESSAGE(touch.separates(sq), touch, " separates ", sq);
        CHECK_FALSE_MESSAGE(sq.crosses(touch), sq, " crosses ", touch);
        CHECK_FALSE_MESSAGE(touch.crosses(sq), touch, " crosses ", sq);
    }
}

TEST_CASE("Convex and OrientedSegment intersection construction") {
    using Point = pgl::Point<int>;
    using Rational = pgl::Rational<int64_t>;
    using RationalPoint = pgl::Point<Rational>;
    using RationalSegment = pgl::Segment<RationalPoint>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using Segment = pgl::Segment<Point>;
    using Convex = pgl::Convex<Point>;

    const Convex sq(std::vector<Point>{{0, 0}, {4, 0}, {4, 4}, {0, 4}});

    SUBCASE("crossing OS: clipped to the interior chord") {
        const OrientedSegment crossing({5, 2}, {-1, 2});
        const auto r = sq.intersection<Rational>(crossing);
        REQUIRE(r);
        REQUIRE(std::holds_alternative<RationalSegment>(*r));
        CHECK_MESSAGE(std::get<RationalSegment>(*r) ==
                          RationalSegment(RationalPoint(Rational(0), Rational(2)),
                                         RationalPoint(Rational(4), Rational(2))),
                      "sq ∩ crossing");
    }

    SUBCASE("fully inside OS: returns the segment itself") {
        const OrientedSegment inside({3, 1}, {1, 3});
        const auto r = sq.intersection(inside);
        REQUIRE(r);
        REQUIRE(std::holds_alternative<Segment>(*r));
        CHECK_MESSAGE(std::get<Segment>(*r) == Segment({1, 3}, {3, 1}), "sq ∩ inside");
    }

    SUBCASE("disjoint OS: empty") {
        const OrientedSegment outside({5, 5}, {6, 6});
        CHECK_FALSE_MESSAGE(sq.intersection(outside), "sq ∩ outside should be empty");
    }
}

TEST_CASE("OrientedSegment and Convex squared Hausdorff distance") {
    using Point = pgl::Point<int>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using Convex = pgl::Convex<Point>;

    const Convex square(std::vector<Point>{{0, 0}, {4, 0}, {4, 4}, {0, 4}});
    const OrientedSegment s({6, 1}, {6, 3});

    // Matches the unoriented Segment case: the farthest square vertices
    // dominate, at squared distance 37.
    CHECK(square.squaredHausdorffDistance(s) == 37);
    CHECK(s.squaredHausdorffDistance(square) == 37);
}
