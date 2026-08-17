#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstdint>
#include <random>
#include <variant>
#include <vector>

#include "pgl.hpp"

namespace {

using Number = pgl::ERational;
using Point = pgl::EPoint;
using Segment = pgl::ESegment;
using Line = pgl::ELine;
using Ray = pgl::ERay;
using Shape = pgl::EShape;
using Arrangement = pgl::Arrangement<Point>;

Point P(int x, int y) {
    return Point(Number(x), Number(y));
}

Point Q(int xNumerator, int xDenominator, int yNumerator, int yDenominator) {
    return Point(Number(xNumerator) / Number(xDenominator),
                 Number(yNumerator) / Number(yDenominator));
}

Segment S(int ax, int ay, int bx, int by) {
    return Segment(P(ax, ay), P(bx, by));
}

template <class Shapes>
void checkIndexedParity(const Shapes& shapes, int radius = 8) {
    Arrangement arrangement(shapes);
    std::vector<Point> queries;
    std::vector<Arrangement::FaceId> expected;
    for (int y = -radius; y <= radius; ++y) {
        for (int x = -radius; x <= radius; ++x) {
            queries.push_back(P(x, y));
            expected.push_back(arrangement.locateFace(queries.back()));
        }
    }

    std::mt19937 generator(1729);
    arrangement.buildPointLocation(generator);
    CHECK(arrangement.hasPointLocation());
    for (std::size_t i = 0; i < queries.size(); ++i) {
        CAPTURE(queries[i]);
        CHECK(arrangement.locateFace(queries[i]) == expected[i]);
    }
}

TEST_CASE("trapezoidal point location preserves the scan on bounded arrangements") {
    checkIndexedParity(std::vector<Segment>{S(-5, -4, 5, -4), S(5, -4, 5, 4),
                                            S(5, 4, -5, 4), S(-5, 4, -5, -4),
                                            S(-3, -2, 3, 2), S(-3, 2, 3, -2),
                                            S(-1, -4, -1, 4), S(-5, 0, 5, 0)});
}

TEST_CASE("trapezoidal point location handles unbounded edges") {
    checkIndexedParity(std::vector<Shape>{Line(P(-2, 0), P(2, 0)),
                                          Line(P(0, -2), P(0, 2)),
                                          Ray(P(1, 1), P(3, 2)),
                                          Ray(P(-1, -1), P(-3, -2))});
    checkIndexedParity(std::vector<Line>{Line(P(0, -3), P(1, -3)),
                                         Line(P(0, 0), P(1, 0)),
                                         Line(P(0, 3), P(1, 3))});
}

// The map is built in a symbolically sheared frame, where a vertical edge
// occupies a single abscissa and an unbounded one escapes to an end
// infinitesimally beside it rather than to infinity.
TEST_CASE("trapezoidal point location handles vertical unbounded edges") {
    checkIndexedParity(std::vector<Shape>{Ray(P(0, 1), P(0, 4)),
                                          Ray(P(0, -1), P(0, -4)),
                                          Segment(P(-3, 0), P(3, 0))});
    checkIndexedParity(std::vector<Shape>{Line(P(-2, 0), P(-2, 5)),
                                          Line(P(2, 5), P(2, 0)),
                                          Ray(P(0, 0), P(0, 6)),
                                          Ray(P(0, 0), P(6, 0)),
                                          Line(P(-6, 3), P(6, 3))});
}

// Vertices stacked on one abscissa leave slabs of infinitesimal width between
// their walls, which is where the sheared frame has to place a curve at a point
// the plane itself does not hold.
TEST_CASE("trapezoidal point location handles vertices stacked on one abscissa") {
    std::vector<Segment> segments;
    for (int x = -4; x <= 4; x += 2) {
        segments.emplace_back(P(x, -4), P(x, 4));
    }
    for (int y = -4; y <= 4; y += 2) {
        segments.emplace_back(P(-4, y), P(4, y));
    }
    segments.emplace_back(P(-4, -4), P(4, 4));
    checkIndexedParity(segments, 6);
}

TEST_CASE("trapezoidal point location falls back for non-integral lines") {
    // Neither supporting line meets the integer lattice, so neither can use
    // the index's cached int64 representative.
    checkIndexedParity(std::vector<Shape>{
        Line(Q(1, 2, -2, 1), Q(1, 2, 2, 1)),
        Ray(Q(-2, 1, -13, 6), Q(2, 1, 11, 6))});
}

TEST_CASE("trapezoidal point location handles empty and lower-dimensional maps") {
    checkIndexedParity(std::vector<Segment>{});
    checkIndexedParity(std::vector<Point>{P(-2, 1), P(0, 0), P(3, -1)});
    checkIndexedParity(std::vector<Segment>{S(-4, 0, 4, 0), S(0, 0, 0, 5),
                                            S(0, 0, 0, -5)});
}

TEST_CASE("locateCell identifies vertices edges and faces") {
    Arrangement arrangement(std::vector<Segment>{S(0, 0, 4, 0), S(4, 0, 4, 4),
                                                  S(4, 4, 0, 4), S(0, 4, 0, 0)});
    std::mt19937 generator(7);
    arrangement.buildPointLocation(generator);

    const auto vertex = arrangement.locateCell(P(0, 0));
    REQUIRE(std::holds_alternative<Arrangement::VertexId>(vertex));
    CHECK(arrangement[std::get<Arrangement::VertexId>(vertex)] == P(0, 0));

    const auto edge = arrangement.locateCell(P(2, 0));
    REQUIRE(std::holds_alternative<Arrangement::HalfedgeId>(edge));
    CHECK(std::get<Arrangement::HalfedgeId>(edge).index() % 2 == 0);

    CHECK(std::holds_alternative<Arrangement::FaceId>(arrangement.locateCell(P(2, 2))));
}

TEST_CASE("locateCell does not extend bounded and half-bounded edges") {
    Arrangement segment(std::vector<Segment>{S(0, 0, 2, 0)});
    std::mt19937 segmentOrder(13);
    segment.buildPointLocation(segmentOrder);
    CHECK(std::holds_alternative<Arrangement::HalfedgeId>(segment.locateCell(P(1, 0))));
    CHECK(std::holds_alternative<Arrangement::FaceId>(segment.locateCell(P(3, 0))));

    Arrangement ray(std::vector<Ray>{Ray(P(0, 0), P(2, 0))});
    std::mt19937 rayOrder(17);
    ray.buildPointLocation(rayOrder);
    CHECK(std::holds_alternative<Arrangement::HalfedgeId>(ray.locateCell(P(3, 0))));
    CHECK(std::holds_alternative<Arrangement::FaceId>(ray.locateCell(P(-1, 0))));
}

TEST_CASE("locateCell has the same contract before and after indexing isolated vertices") {
    Arrangement arrangement(std::vector<Point>{P(-2, 1), P(0, 0), P(3, -1)});
    CHECK(std::holds_alternative<Arrangement::VertexId>(arrangement.locateCell(P(0, 0))));
    CHECK(std::holds_alternative<Arrangement::FaceId>(arrangement.locateCell(P(1, 0))));

    std::mt19937 generator(19);
    arrangement.buildPointLocation(generator);
    CHECK(std::holds_alternative<Arrangement::VertexId>(arrangement.locateCell(P(0, 0))));
    CHECK(std::holds_alternative<Arrangement::FaceId>(arrangement.locateCell(P(1, 0))));
}

TEST_CASE("the index keeps integral arrangement construction exact") {
    using IntPoint = pgl::Point<int>;
    using IntSegment = pgl::Segment<IntPoint>;
    using IntArrangement = pgl::Arrangement<IntPoint>;
    const std::vector<IntSegment> segments{
        IntSegment(IntPoint(0, 0), IntPoint(6, 0)),
        IntSegment(IntPoint(6, 0), IntPoint(6, 6)),
        IntSegment(IntPoint(6, 6), IntPoint(0, 6)),
        IntSegment(IntPoint(0, 6), IntPoint(0, 0))};
    IntArrangement arrangement(segments);
    const auto inside = arrangement.locateFace(IntPoint(3, 3));
    const auto outside = arrangement.locateFace(IntPoint(8, 3));
    std::mt19937 generator(23);
    arrangement.buildPointLocation(generator);
    CHECK(arrangement.locateFace(IntPoint(3, 3)) == inside);
    CHECK(arrangement.locateFace(IntPoint(8, 3)) == outside);
}

TEST_CASE("an indexed arrangement copy shares a valid immutable index") {
    Arrangement original(std::vector<Segment>{S(0, 0, 4, 0), S(4, 0, 2, 4),
                                               S(2, 4, 0, 0)});
    std::mt19937 generator(11);
    original.buildPointLocation(generator);
    Arrangement copy = original;
    CHECK(copy.hasPointLocation());
    CHECK(copy.locateFace(P(2, 1)) == original.locateFace(P(2, 1)));
    copy.clearPointLocation();
    CHECK_FALSE(copy.hasPointLocation());
    CHECK(original.hasPointLocation());
    CHECK(copy.locateFace(P(2, 1)) == original.locateFace(P(2, 1)));
}

TEST_CASE("the default point-location builder uses the public face-location API") {
    Arrangement arrangement(std::vector<Segment>{S(0, 0, 4, 0), S(4, 0, 2, 4),
                                                  S(2, 4, 0, 0)});
    const auto expected = arrangement.locateFace(P(2, 1));
    arrangement.buildPointLocation();
    CHECK(arrangement.locateFace(P(2, 1)) == expected);
    const Point unreducedInteger(
        Number(pgl::BigInt(4), pgl::BigInt(2)),
        Number(pgl::BigInt(3), pgl::BigInt(3)));
    CHECK(arrangement.locateFace(unreducedInteger) == expected);
}

TEST_CASE("trapezoidal point location matches scans on randomized overlays") {
    std::mt19937 data(20260813);
    std::uniform_int_distribution<int> coordinate(-6, 6);
    for (std::uint32_t trial = 0; trial < 24; ++trial) {
        std::vector<Segment> segments;
        while (segments.size() < 10) {
            const Point a = P(coordinate(data), coordinate(data));
            const Point b = P(coordinate(data), coordinate(data));
            if (a != b) {
                segments.emplace_back(a, b);
            }
        }

        Arrangement arrangement(segments);
        std::vector<Point> queries;
        std::vector<Arrangement::FaceId> expected;
        for (int y = -11; y <= 11; y += 2) {
            for (int x = -11; x <= 11; x += 2) {
                queries.emplace_back(Number(x) / Number(2), Number(y) / Number(2));
                expected.push_back(arrangement.locateFace(queries.back()));
            }
        }

        std::mt19937 order(1000 + trial);
        arrangement.buildPointLocation(order);
        for (std::size_t i = 0; i < queries.size(); ++i) {
            CAPTURE(trial);
            CAPTURE(queries[i]);
            CHECK(arrangement.locateFace(queries[i]) == expected[i]);
        }
    }
}

// A vertical query keeps its own slab in the sheared frame, which is the case
// the search DAG prunes hardest and the one that has to stay exact.
TEST_CASE("trapezoidal traversal matches the scan on vertical queries") {
    const std::vector<Shape> shapes{S(-6, -3, 6, -3), S(-6, 3, 6, 3),
                                     S(-2, -6, -2, 6), S(2, -6, 2, 6),
                                     S(-6, -6, 6, 6),  Ray(P(0, 4), P(0, 9)),
                                     Ray(P(4, 0), P(4, -9)),
                                     Line(P(-4, -1), P(-4, 1))};
    const Arrangement linear(shapes);
    Arrangement indexed = linear;
    std::mt19937 order(31);
    indexed.buildPointLocation(order);

    for (int x = -7; x <= 7; ++x) {
        for (int y = -7; y <= 7; ++y) {
            CAPTURE(x);
            CAPTURE(y);
            const Ray up(P(x, y), P(x, y + 1));
            const Ray down(P(x, y), P(x, y - 1));
            CHECK(indexed.firstIntersecting(up) == linear.firstIntersecting(up));
            CHECK(indexed.firstIntersecting(down) == linear.firstIntersecting(down));
            CHECK(indexed.reportIntersecting(up) == linear.reportIntersecting(up));
            CHECK(indexed.locateCell(P(x, y)) == linear.locateCell(P(x, y)));
            CHECK(indexed.locateFace(P(x, y)) == linear.locateFace(P(x, y)));
        }
    }
}

}  // namespace
