#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstddef>
#include <random>
#include <vector>

#include "pgl.hpp"

using Coord = pgl::Point<int>;
using Exact = pgl::ERational;

// Whether the pair has each witness method. Wrapped in a template so the
// requires-expression is dependent; a non-dependent one is a hard error rather
// than `false` under g++.
template <class A, class B>
inline constexpr bool hasClosestSegments = requires(const A& a, const B& b) { a.closestSegments(b); };

template <class A, class B>
inline constexpr bool hasClosestPoints = requires(const A& a, const B& b) { a.closestPoints(b); };

// The whole contract of a witness: it exists exactly when the distance is
// positive, both of its parts belong to their own shape, and both reproduce the
// distance the shapes report.
template <class A, class B>
static void checkWitness(const A& first, const B& second) {
    const Exact distance = first.template squaredDistance<Exact>(second);
    const auto segments = first.template closestSegments<int>(second);
    const auto points = first.template closestPoints<Exact>(second);

    REQUIRE(segments.has_value() == (distance != Exact{}));
    REQUIRE(points.has_value() == segments.has_value());
    if (!segments) {
        return;
    }

    CHECK((*segments)[0].template squaredDistance<Exact>((*segments)[1]) == distance);
    CHECK((*points)[0].template squaredDistance<Exact>((*points)[1]) == distance);

    // Each returned element belongs to the shape it was asked about.
    CHECK(first.contains((*segments)[0].min()));
    CHECK(first.contains((*segments)[0].max()));
    CHECK(second.contains((*segments)[1].min()));
    CHECK(second.contains((*segments)[1].max()));
    CHECK(first.contains((*points)[0]));
    CHECK(second.contains((*points)[1]));

    // The points lie on the elements that were named.
    CHECK((*segments)[0].template squaredDistance<Exact>((*points)[0]) == Exact{});
    CHECK((*segments)[1].template squaredDistance<Exact>((*points)[1]) == Exact{});
}

// The same contract, for a pair that has only the point witness: an unbounded
// operand realizes the distance where there is no element to name.
template <class A, class B>
static void checkPointWitness(const A& first, const B& second) {
    const Exact distance = first.template squaredDistance<Exact>(second);
    const auto points = first.template closestPoints<Exact>(second);

    REQUIRE(points.has_value() == (distance != Exact{}));
    if (!points) {
        return;
    }
    CHECK((*points)[0].template squaredDistance<Exact>((*points)[1]) == distance);
    CHECK(first.contains((*points)[0]));
    CHECK(second.contains((*points)[1]));

    const auto mirrored = second.template closestPoints<Exact>(first);
    REQUIRE(mirrored.has_value());
    CHECK((*mirrored)[0].template squaredDistance<Exact>((*mirrored)[1]) == distance);
}

// The witness of the mirrored call is the same pair in the other order.
template <class A, class B>
static void checkSymmetry(const A& first, const B& second) {
    const auto forward = first.template closestPoints<Exact>(second);
    const auto backward = second.template closestPoints<Exact>(first);
    REQUIRE(forward.has_value() == backward.has_value());
    if (!forward) {
        return;
    }
    CHECK((*forward)[0].template squaredDistance<Exact>((*forward)[1]) ==
          (*backward)[0].template squaredDistance<Exact>((*backward)[1]));
}

TEST_CASE("closestSegments names the two elements that realize the distance") {
    const pgl::Segment<Coord> left(Coord(0, 0), Coord(0, 10));
    const pgl::Segment<Coord> right(Coord(5, 2), Coord(5, 8));

    const auto witness = left.closestSegments(right);
    REQUIRE(witness.has_value());
    CHECK((*witness)[0] == left);
    CHECK((*witness)[1] == right);

    const auto points = left.closestPoints(right);
    REQUIRE(points.has_value());
    CHECK((*points)[0].template squaredDistance<Exact>((*points)[1]) == Exact(25));
}

TEST_CASE("a point is its own degenerate element") {
    const pgl::Triangle<Coord> triangle(Coord(0, 0), Coord(4, 0), Coord(0, 4));
    const Coord outside(10, 10);

    const auto witness = triangle.closestSegments(outside);
    REQUIRE(witness.has_value());
    CHECK((*witness)[0] == pgl::Segment<Coord>(Coord(0, 4), Coord(4, 0)));
    CHECK((*witness)[1] == pgl::Segment<Coord>(outside, outside));
    CHECK((*witness)[1].min() == (*witness)[1].max());

    const auto points = triangle.template closestPoints<Exact>(outside);
    REQUIRE(points.has_value());
    CHECK((*points)[0] == pgl::Point<Exact>(2, 2));
    CHECK((*points)[1] == pgl::Point<Exact>(10, 10));
}

TEST_CASE("the first element is the receiver's and the second the argument's") {
    const pgl::Rectangle<Coord> rectangle(Coord(0, 0), Coord(2, 2));
    const pgl::Segment<Coord> segment(Coord(6, 0), Coord(6, 2));

    const auto forward = rectangle.closestSegments(segment);
    const auto backward = segment.closestSegments(rectangle);
    REQUIRE(forward.has_value());
    REQUIRE(backward.has_value());
    CHECK((*forward)[0] == (*backward)[1]);
    CHECK((*forward)[1] == (*backward)[0]);
}

TEST_CASE("shapes that meet have no witness") {
    const pgl::Triangle<Coord> triangle(Coord(0, 0), Coord(6, 0), Coord(0, 6));

    CHECK_FALSE(triangle.closestSegments(Coord(1, 1)).has_value());
    CHECK_FALSE(triangle.closestPoints(Coord(1, 1)).has_value());
    CHECK_FALSE(triangle.closestSegments(Coord(3, 3)).has_value());  // on the boundary
    CHECK_FALSE(triangle.closestSegments(pgl::Segment<Coord>(Coord(-1, 1), Coord(1, 1))).has_value());
}

TEST_CASE("a shape nested in another is at distance zero, not at its boundary gap") {
    const pgl::Rectangle<Coord> outer(Coord(-10, -10), Coord(10, 10));
    const pgl::Triangle<Coord> inner(Coord(0, 0), Coord(3, 0), Coord(0, 3));

    REQUIRE(outer.template squaredDistance<Exact>(inner) == Exact{});
    CHECK_FALSE(outer.closestSegments(inner).has_value());
    CHECK_FALSE(inner.closestSegments(outer).has_value());
}

TEST_CASE("a hole is boundary the witness can land on") {
    const std::vector<Coord> outer{Coord(0, 0), Coord(20, 0), Coord(20, 20), Coord(0, 20)};
    const std::vector<Coord> hole{Coord(5, 5), Coord(15, 5), Coord(15, 15), Coord(5, 15)};
    const pgl::PolygonWithHoles<Coord> region(pgl::Polygon<Coord>(outer),
                                              std::vector<pgl::Polygon<Coord>>{pgl::Polygon<Coord>(hole)});
    const Coord inHole(10, 10);

    const auto witness = region.closestSegments(inHole);
    REQUIRE(witness.has_value());
    CHECK((*witness)[0].template squaredDistance<Exact>(inHole) == Exact(25));
    checkWitness(region, inHole);
}

TEST_CASE("a shape with no edge contributes its vertices as degenerate elements") {
    const pgl::Polyline<Coord> single(std::vector<Coord>{Coord(7, 7)});
    const pgl::MonotoneChain<Coord> singleChain(std::vector<Coord>{Coord(7, 7)});

    checkWitness(single, Coord(0, 0));
    checkWitness(singleChain, pgl::Segment<Coord>(Coord(0, 0), Coord(0, 5)));

    const auto witness = single.closestSegments(Coord(0, 0));
    REQUIRE(witness.has_value());
    CHECK((*witness)[0].min() == (*witness)[0].max());
}

TEST_CASE("degenerate shapes keep the contract") {
    checkWitness(pgl::Rectangle<Coord>(Coord(3, 3), Coord(3, 3)), Coord(0, 0));
    checkWitness(pgl::Segment<Coord>(Coord(3, 3), Coord(3, 3)),
                 pgl::Segment<Coord>(Coord(0, 0), Coord(0, 4)));
    checkWitness(pgl::Triangle<Coord>(Coord(0, 0), Coord(2, 0), Coord(4, 0)), Coord(0, 5));
}

TEST_CASE("the receiver's vertex labels survive on the witness") {
    using Labeled = pgl::Point<int, int>;
    const pgl::Segment<Labeled> segment(Labeled(0, 0, 7), Labeled(0, 4, 8));

    const auto points = segment.closestPoints(pgl::Point<int>(5, 0));
    REQUIRE(points.has_value());
    CHECK((*points)[0].label() == 7);
}

TEST_CASE("closestPoints answers in the requested coordinate type") {
    const pgl::Segment<Coord> segment(Coord(0, 0), Coord(3, 3));
    const Coord query(4, 0);

    const auto exact = segment.template closestPoints<Exact>(query);
    REQUIRE(exact.has_value());
    CHECK((*exact)[0] == pgl::Point<Exact>(2, 2));

    const auto approximate = segment.template closestPoints<double>(query);
    REQUIRE(approximate.has_value());
    CHECK((*approximate)[0].x() == doctest::Approx(2.0));
    CHECK((*approximate)[0].y() == doctest::Approx(2.0));
}

TEST_CASE("an unbounded operand has a point witness but no element to name") {
    CHECK(hasClosestSegments<pgl::Segment<Coord>, pgl::Polygon<Coord>>);
    CHECK(hasClosestPoints<Coord, pgl::PolygonSet<Coord>>);

    // No element to name, but the point is still there.
    CHECK_FALSE(hasClosestSegments<pgl::Segment<Coord>, pgl::Line<Coord>>);
    CHECK_FALSE(hasClosestSegments<pgl::Segment<Coord>, pgl::Ray<Coord>>);
    CHECK_FALSE(hasClosestSegments<pgl::Segment<Coord>, pgl::Halfplane<Coord>>);
    CHECK_FALSE(hasClosestSegments<pgl::Segment<Coord>, pgl::HalfplaneIntersection<Coord>>);
    CHECK_FALSE(hasClosestSegments<pgl::Line<Coord>, pgl::Segment<Coord>>);
    CHECK(hasClosestPoints<pgl::Segment<Coord>, pgl::Line<Coord>>);
    CHECK(hasClosestPoints<pgl::Segment<Coord>, pgl::OrientedLine<Coord>>);
    CHECK(hasClosestPoints<pgl::Segment<Coord>, pgl::Ray<Coord>>);
    CHECK(hasClosestPoints<pgl::Segment<Coord>, pgl::Halfplane<Coord>>);
    CHECK(hasClosestPoints<pgl::Segment<Coord>, pgl::HalfplaneIntersection<Coord>>);
    CHECK(hasClosestPoints<pgl::Line<Coord>, pgl::Segment<Coord>>);
    CHECK(hasClosestPoints<pgl::HalfplaneIntersection<Coord>, pgl::Polygon<Coord>>);

    // Two unbounded operands realize the distance along their whole length.
    CHECK_FALSE(hasClosestPoints<pgl::Line<Coord>, pgl::Line<Coord>>);
    CHECK_FALSE(hasClosestPoints<pgl::Ray<Coord>, pgl::Halfplane<Coord>>);

    // Curved, and the runtime wrapper, are out of both.
    CHECK_FALSE(hasClosestSegments<pgl::Segment<Coord>, pgl::Disk<Coord>>);
    CHECK_FALSE(hasClosestPoints<pgl::Segment<Coord>, pgl::Disk<Coord>>);
    CHECK_FALSE(hasClosestPoints<pgl::Disk<Coord>, pgl::Segment<Coord>>);
    CHECK_FALSE(hasClosestPoints<pgl::Shape<Coord>, pgl::Segment<Coord>>);
}

TEST_CASE("a ray's witness can fall strictly inside an element") {
    // The closest point of the ray is its source, and the closest point of the
    // segment to that source is the segment's midpoint -- an end of neither.
    const pgl::Segment<Coord> segment(Coord(-3, -1), Coord(-3, 1));
    const pgl::Ray<Coord> ray(Coord(0, 0), Coord(5, 0));

    const auto points = segment.template closestPoints<Exact>(ray);
    REQUIRE(points.has_value());
    CHECK((*points)[0] == pgl::Point<Exact>(-3, 0));
    CHECK((*points)[1] == pgl::Point<Exact>(0, 0));
    checkPointWitness(segment, ray);
}

TEST_CASE("a line's witness is the perpendicular foot") {
    const pgl::Line<Coord> line(Coord(0, 0), Coord(1, 1));
    const pgl::Triangle<Coord> triangle(Coord(5, 0), Coord(9, 0), Coord(9, 4));

    const auto points = triangle.template closestPoints<Exact>(line);
    REQUIRE(points.has_value());
    CHECK((*points)[0] == pgl::Point<Exact>(5, 0));
    CHECK((*points)[1] == pgl::Point<Exact>(Exact(5, 2), Exact(5, 2)));
    checkPointWitness(triangle, line);
}

TEST_CASE("a half-plane is reached at its boundary line") {
    const pgl::Halfplane<Coord> halfplane(Coord(0, 0), Coord(0, 5));
    const pgl::Rectangle<Coord> rectangle(Coord(4, 1), Coord(7, 3));

    const auto points = rectangle.template closestPoints<Exact>(halfplane);
    if (points) {
        CHECK(halfplane.boundaryContains((*points)[1]));
        checkPointWitness(rectangle, halfplane);
    }
}

TEST_CASE("a half-plane intersection is searched over its boundary pieces") {
    const std::vector<pgl::Halfplane<Coord>> sides{
        pgl::Halfplane<Coord>(Coord(0, 0), Coord(4, 0)),
        pgl::Halfplane<Coord>(Coord(4, 0), Coord(4, 4)),
        pgl::Halfplane<Coord>(Coord(4, 4), Coord(0, 0)),
    };
    const pgl::HalfplaneIntersection<Coord> region(sides);
    REQUIRE_FALSE(region.empty());

    checkPointWitness(region, Coord(20, -20));
    checkPointWitness(region, pgl::Segment<Coord>(Coord(20, 0), Coord(20, 8)));
    checkPointWitness(pgl::Triangle<Coord>(Coord(30, 0), Coord(34, 0), Coord(34, 4)), region);
}

TEST_CASE("random bounded shapes agree with squaredDistance against unbounded ones") {
    std::mt19937 generator(20260907);
    auto coordinate = [&generator] { return std::uniform_int_distribution<int>(-12, 12)(generator); };
    auto point = [&coordinate] { return Coord(coordinate(), coordinate()); };

    for (int trial = 0; trial < 150; ++trial) {
        const Coord first = point();
        const Coord second = point();
        if (first == second) {
            continue;
        }
        const pgl::Line<Coord> line(first, second);
        const pgl::OrientedLine<Coord> orientedLine(first, second);
        const pgl::Ray<Coord> ray(first, second);
        const pgl::Halfplane<Coord> halfplane(first, second);

        std::vector<pgl::Halfplane<Coord>> sides;
        for (int i = 0; i < 3; ++i) {
            const Coord a = point();
            const Coord b = point();
            if (a != b) {
                sides.emplace_back(a, b);
            }
        }
        const pgl::HalfplaneIntersection<Coord> region(sides);

        const Coord query = point();
        const pgl::Segment<Coord> segment(point(), point());
        const pgl::Triangle<Coord> triangle(point(), point(), point());
        const pgl::Rectangle<Coord> rectangle(point(), point());

        checkPointWitness(query, line);
        checkPointWitness(segment, line);
        checkPointWitness(triangle, orientedLine);
        checkPointWitness(rectangle, ray);
        checkPointWitness(segment, ray);
        checkPointWitness(triangle, halfplane);
        if (!region.empty()) {
            checkPointWitness(segment, region);
            checkPointWitness(triangle, region);
            checkPointWitness(query, region);
        }
    }
}

TEST_CASE("random shapes agree with squaredDistance") {
    std::mt19937 generator(20260906);
    auto coordinate = [&generator] { return std::uniform_int_distribution<int>(-12, 12)(generator); };
    auto point = [&coordinate] { return Coord(coordinate(), coordinate()); };
    auto segment = [&point] { return pgl::Segment<Coord>(point(), point()); };
    auto triangle = [&point] { return pgl::Triangle<Coord>(point(), point(), point()); };
    auto rectangle = [&point] { return pgl::Rectangle<Coord>(point(), point()); };
    auto convex = [&point] {
        std::vector<Coord> vertices;
        for (int i = 0; i < 6; ++i) {
            vertices.push_back(point());
        }
        return pgl::Convex<Coord>(pgl::convexHull(vertices), true);
    };
    auto polyline = [&point] {
        std::vector<Coord> vertices;
        for (int i = 0; i < 4; ++i) {
            vertices.push_back(point());
        }
        return pgl::Polyline<Coord>(vertices);
    };
    auto chain = [&point] {
        std::vector<Coord> vertices;
        for (int i = 0; i < 4; ++i) {
            vertices.push_back(point());
        }
        std::sort(vertices.begin(), vertices.end());
        vertices.erase(std::unique(vertices.begin(), vertices.end()), vertices.end());
        return pgl::MonotoneChain<Coord>(vertices);
    };

    for (int trial = 0; trial < 120; ++trial) {
        const auto p = point();
        const auto s = segment();
        const auto t = triangle();
        const auto r = rectangle();
        const auto c = convex();
        const auto l = polyline();
        const auto m = chain();

        checkWitness(p, s);
        checkWitness(s, segment());
        checkWitness(t, p);
        checkWitness(t, s);
        checkWitness(t, triangle());
        checkWitness(r, rectangle());
        checkWitness(r, t);
        checkWitness(c, convex());
        checkWitness(c, p);
        checkWitness(l, s);
        checkWitness(l, polyline());
        checkWitness(m, t);

        checkSymmetry(t, s);
        checkSymmetry(c, r);
        checkSymmetry(l, m);
    }
}
