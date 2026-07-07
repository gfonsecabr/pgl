#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

// Reference chain used throughout: up to a peak at (2,4), down to (4,0), up the
// vertical edge to (4,4), and down to (6,0).
namespace {
const pgl::MonotoneChain<pgl::Point<int>> zigzag({0, 0, 2, 4, 4, 0, 4, 4, 6, 0});
}

TEST_CASE("MonotoneChain contains a segment iff it is a straight sub-path") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;

    SUBCASE("sub-segments of a single edge") {
        CHECK(zigzag.contains(Segment({0, 0}, {2, 4})));   // a whole edge
        CHECK(zigzag.contains(Segment({0, 0}, {1, 2})));   // from a vertex inward
        CHECK(zigzag.contains(Segment({5, 2}, {6, 0})));   // on the last edge
    }

    SUBCASE("sub-segments of the vertical edge") {
        CHECK(zigzag.contains(Segment({4, 0}, {4, 4})));
        CHECK(zigzag.contains(Segment({4, 1}, {4, 3})));
        CHECK_FALSE(zigzag.contains(Segment({4, 0}, {4, 5})));   // sticks out the top
        CHECK_FALSE(zigzag.contains(Segment({4, -1}, {4, 2})));  // sticks out the bottom
    }

    SUBCASE("both endpoints on the chain but spanning a bend") {
        CHECK_FALSE(zigzag.contains(Segment({1, 2}, {3, 2})));   // chord under the peak
        CHECK_FALSE(zigzag.contains(Segment({0, 0}, {4, 0})));   // chord between vertices
        CHECK_FALSE(zigzag.contains(Segment({2, 4}, {4, 4})));   // chord between the peaks
    }

    SUBCASE("an endpoint off the chain") {
        CHECK_FALSE(zigzag.contains(Segment({0, 0}, {1, 3})));
        CHECK_FALSE(zigzag.contains(Segment({1, 1}, {2, 4})));
        CHECK_FALSE(zigzag.contains(Segment({6, 0}, {7, 0})));   // leaves the x-extent
    }

    SUBCASE("collinear run spanning multiple edges") {
        // Chain with collinear vertices (1,1) and (3,3) inside the run from
        // (0,0) to (4,4), then a bend down to (5,0).
        const pgl::MonotoneChain<Point> run({0, 0, 1, 1, 3, 3, 4, 4, 5, 0});
        CHECK(run.contains(Segment({0, 0}, {4, 4})));    // spans the whole run
        CHECK(run.contains(Segment({1, 1}, {3, 3})));    // vertex to vertex
        CHECK(run.contains(Segment({2, 2}, {4, 4})));    // starts inside an edge
        CHECK_FALSE(run.contains(Segment({3, 3}, {5, 0})));  // spans the bend at (4,4)
        CHECK_FALSE(run.contains(Segment({0, 0}, {5, 0})));
    }

    SUBCASE("oriented segments delegate to the segment logic") {
        const pgl::OrientedSegment<Point> forward({0, 0}, {2, 4});
        const pgl::OrientedSegment<Point> backward({2, 4}, {0, 0});
        CHECK(zigzag.contains(forward));
        CHECK(zigzag.contains(backward));
        CHECK_FALSE(zigzag.contains(pgl::OrientedSegment<Point>({1, 2}, {3, 2})));
    }
}

TEST_CASE("MonotoneChain boundary and interior against segments") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;

    SUBCASE("the boundary is a finite point set, so it contains no segment") {
        CHECK_FALSE(zigzag.boundaryContains(Segment({0, 0}, {2, 4})));
        CHECK_FALSE(zigzag.boundaryContains(Segment({4, 1}, {4, 3})));
    }

    SUBCASE("interior containment excludes the extreme vertices") {
        CHECK(zigzag.interiorContains(Segment({1, 2}, {2, 4})));    // strictly inside
        CHECK(zigzag.interiorContains(Segment({4, 1}, {4, 3})));    // vertical, inside
        CHECK(zigzag.interiorContains(Segment({4, 0}, {4, 4})));    // touches non-extreme vertices only
        CHECK_FALSE(zigzag.interiorContains(Segment({0, 0}, {1, 2})));   // starts at an extreme
        CHECK_FALSE(zigzag.interiorContains(Segment({5, 2}, {6, 0})));   // ends at an extreme
        CHECK_FALSE(zigzag.interiorContains(Segment({1, 2}, {3, 2})));   // not even contained
    }
}

TEST_CASE("MonotoneChain intersects segments through the x-window") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;

    SUBCASE("proper crossings") {
        CHECK(zigzag.intersects(Segment({1, 0}, {1, 4})));    // vertical through the first edge
        CHECK(zigzag.intersects(Segment({0, 2}, {6, 2})));    // horizontal through everything
        CHECK(zigzag.intersects(Segment({3, 0}, {5, 4})));    // crosses near the valley
    }

    SUBCASE("touching without crossing") {
        CHECK(zigzag.intersects(Segment({2, 4}, {3, 7})));    // leaves the peak upward
        CHECK(zigzag.intersects(Segment({-1, -1}, {0, 0})));  // reaches the first vertex
        CHECK(zigzag.intersects(Segment({6, 0}, {7, 2})));    // starts at the last vertex
        CHECK(zigzag.intersects(Segment({1, 2}, {1, 5})));    // endpoint on an edge interior
    }

    SUBCASE("collinear overlap with an edge") {
        CHECK(zigzag.intersects(Segment({1, 2}, {3, 6})));    // overlaps half the first edge
        CHECK(zigzag.intersects(Segment({4, -2}, {4, 1})));   // overlaps the vertical edge
    }

    SUBCASE("misses") {
        CHECK_FALSE(zigzag.intersects(Segment({0, 3}, {1, 5})));    // above the first edge
        CHECK_FALSE(zigzag.intersects(Segment({5, 4}, {6, 4})));    // above the last edge
        CHECK_FALSE(zigzag.intersects(Segment({1, -1}, {5, -1})));  // below everything
        CHECK_FALSE(zigzag.intersects(Segment({4, 5}, {4, 7})));    // above the vertical edge
        CHECK_FALSE(zigzag.intersects(Segment({7, 0}, {8, 1})));    // right of the x-extent
        CHECK_FALSE(zigzag.intersects(Segment({-3, 0}, {-1, 5})));  // left of the x-extent
    }

    SUBCASE("window boundary cases at the extremes") {
        CHECK(zigzag.intersects(Segment({0, -2}, {0, 3})));   // vertical at the first vertex's x
        CHECK(zigzag.intersects(Segment({6, -1}, {6, 1})));   // vertical at the last vertex's x
        CHECK_FALSE(zigzag.intersects(Segment({0, 1}, {0, 3})));   // same x, above the first vertex
        CHECK_FALSE(zigzag.intersects(Segment({6, 1}, {6, 3})));   // same x, above the last vertex
    }

    SUBCASE("symmetric direction through the segment's rank forwarder") {
        const Segment crossing({1, 0}, {1, 4});
        const Segment miss({0, 3}, {1, 5});
        CHECK(crossing.intersects(zigzag));
        CHECK_FALSE(miss.intersects(zigzag));
        const pgl::OrientedSegment<Point> oriented({1, 4}, {1, 0});
        CHECK(oriented.intersects(zigzag));
        CHECK(zigzag.intersects(oriented));
    }
}

TEST_CASE("MonotoneChain interiorsIntersect treats non-extreme vertices as interior") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;

    SUBCASE("proper crossing through an edge interior") {
        CHECK(zigzag.interiorsIntersect(Segment({1, 0}, {1, 4})));
        CHECK(zigzag.interiorsIntersect(Segment({0, 2}, {6, 2})));
    }

    SUBCASE("open segment through a non-extreme vertex engages the vertex rule") {
        // Passes through (2,4) without touching any open edge of the chain.
        CHECK(zigzag.interiorsIntersect(Segment({1, 3}, {3, 5})));
        // Same through the valley vertex (4,0), the bottom of the vertical edge.
        CHECK(zigzag.interiorsIntersect(Segment({3, -1}, {5, 1})));
    }

    SUBCASE("touching an extreme vertex does not engage") {
        // (0,0) is chain boundary; the segment crosses no open edge.
        CHECK(zigzag.intersects(Segment({-1, -1}, {1, 1})));
        CHECK_FALSE(zigzag.interiorsIntersect(Segment({-1, -1}, {1, 1})));
    }

    SUBCASE("segment endpoint resting on the chain does not engage") {
        // The open segment excludes its endpoint (1,2), which is the only
        // common point with the chain.
        CHECK(zigzag.intersects(Segment({1, 2}, {1, 5})));
        CHECK_FALSE(zigzag.interiorsIntersect(Segment({1, 2}, {1, 5})));
        // Symmetric case: the chain's open edge meets the segment only at the
        // segment's own endpoint.
        CHECK_FALSE(zigzag.interiorsIntersect(Segment({3, 2}, {3, 5})));
    }

    SUBCASE("collinear overlap of positive length engages") {
        CHECK(zigzag.interiorsIntersect(Segment({1, 2}, {3, 6})));
        CHECK(zigzag.interiorsIntersect(Segment({4, 1}, {4, 3})));
    }

    SUBCASE("misses stay misses") {
        CHECK_FALSE(zigzag.interiorsIntersect(Segment({0, 3}, {1, 5})));
        CHECK_FALSE(zigzag.interiorsIntersect(Segment({7, 0}, {8, 1})));
    }
}

TEST_CASE("MonotoneChain segment predicates with other numeric types") {
    SUBCASE("rational coordinates") {
        using Rational = pgl::Rational<int>;
        using Point = pgl::Point<Rational>;
        const pgl::MonotoneChain<Point> chain(
            {Point(Rational(0), Rational(0)), Point(Rational(2), Rational(1)),
             Point(Rational(4), Rational(0))});
        const pgl::Segment<Point> crossing(Point(Rational(1), Rational(0)),
                                           Point(Rational(1), Rational(1)));
        CHECK(chain.intersects(crossing));
        CHECK(chain.contains(pgl::Segment<Point>(Point(Rational(1), Rational(1, 2)),
                                                 Point(Rational(2), Rational(1)))));
    }

    SUBCASE("rational segment against an integer chain") {
        using Rational = pgl::Rational<int>;
        using RationalPoint = pgl::Point<Rational>;
        const pgl::Segment<RationalPoint> onEdge(RationalPoint(Rational(1, 2), Rational(1)),
                                                 RationalPoint(Rational(3, 2), Rational(3)));
        CHECK(zigzag.contains(onEdge));
        CHECK(zigzag.interiorsIntersect(onEdge));
    }

    SUBCASE("double coordinates") {
        using Point = pgl::Point<double>;
        const pgl::MonotoneChain<Point> chain({0.0, 0.0, 2.0, 1.0, 4.0, 0.0});
        CHECK(chain.intersects(pgl::Segment<Point>({1.0, -1.0}, {1.0, 1.0})));
        CHECK_FALSE(chain.intersects(pgl::Segment<Point>({1.0, 0.75}, {1.0, 2.0})));
    }
}

TEST_CASE("MonotoneChain and Segment separates and crosses") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;

    SUBCASE("a proper crossing separates in both directions") {
        const Segment vertical({1, 0}, {1, 4});
        CHECK(zigzag.separates(vertical));
        CHECK(vertical.separates(zigzag));
        CHECK(zigzag.crosses(vertical));
        CHECK(vertical.crosses(zigzag));
    }

    SUBCASE("crossing exactly at a shared non-extreme vertex") {
        // The chain's peak (2,4) lies in the open segment: no single edge
        // properly crosses it, but the one-point component disconnects the
        // segment, and removing that interior point disconnects the chain.
        const Segment throughPeak({1, 3}, {3, 5});
        CHECK(zigzag.separates(throughPeak));
        CHECK(throughPeak.separates(zigzag));
        CHECK(zigzag.crosses(throughPeak));
    }

    SUBCASE("touching does not separate") {
        // Ends on the chain: the component contains a segment endpoint.
        CHECK_FALSE(zigzag.separates(Segment({1, 2}, {1, 5})));
        // Passes through the extreme vertex (0,0): the chain keeps one piece.
        CHECK_FALSE(Segment({-1, -1}, {1, 1}).separates(zigzag));
        CHECK_FALSE(zigzag.crosses(Segment({1, 2}, {1, 5})));
        CHECK_FALSE(Segment({-1, -1}, {1, 1}).crosses(zigzag));
    }

    SUBCASE("collinear overlaps") {
        // The chain runs along the middle of the segment: the overlap
        // component avoids both segment endpoints, so the segment is cut.
        CHECK(zigzag.separates(Segment({-1, -2}, {3, 6})));
        // The overlap reaches the segment's endpoint: still connected.
        CHECK_FALSE(zigzag.separates(Segment({0, 0}, {1, 2})));
        // The overlap swallows an end of the chain: the chain stays connected.
        CHECK_FALSE(Segment({-1, -2}, {1, 2}).separates(zigzag));
    }

    SUBCASE("point separates the chain iff it is a relative-interior point") {
        CHECK(pgl::Point<int>(1, 2).separates(zigzag));
        CHECK(pgl::Point<int>(2, 4).separates(zigzag));
        CHECK_FALSE(pgl::Point<int>(0, 0).separates(zigzag));
        CHECK_FALSE(pgl::Point<int>(1, 3).separates(zigzag));
        CHECK_FALSE(zigzag.separates(pgl::Point<int>(1, 2)));
        CHECK_FALSE(zigzag.crosses(pgl::Point<int>(1, 2)));
    }
}

TEST_CASE("MonotoneChain and Segment intersection pieces") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;

    SUBCASE("a segment crossing several edges") {
        const auto pieces = zigzag.intersection(Segment({0, 2}, {6, 2}));
        REQUIRE(pieces.size() == 4);
        const std::vector<Point> expected{Point(1, 2), Point(3, 2), Point(4, 2), Point(5, 2)};
        for (std::size_t i = 0; i < expected.size(); ++i) {
            REQUIRE(std::holds_alternative<Point>(pieces[i]));
            CHECK(std::get<Point>(pieces[i]) == expected[i]);
        }
    }

    SUBCASE("collinear overlap yields a single segment") {
        const auto pieces = zigzag.intersection(Segment({1, 2}, {3, 6}));
        REQUIRE(pieces.size() == 1);
        REQUIRE(std::holds_alternative<Segment>(pieces[0]));
        CHECK(std::get<Segment>(pieces[0]) == Segment({1, 2}, {2, 4}));
    }

    SUBCASE("intersection with a point") {
        const auto hit = zigzag.intersection(Point(1, 2));
        REQUIRE(hit.has_value());
        CHECK(*hit == Point(1, 2));
        CHECK_FALSE(zigzag.intersection(Point(1, 3)).has_value());
    }
}
