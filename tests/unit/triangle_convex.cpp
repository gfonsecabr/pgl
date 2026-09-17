#include "pgl.hpp"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <variant>
#include <vector>

TEST_CASE("Convex boundaryContains Triangle") {
    using Point = pgl::Point<int>;
    using Convex = pgl::Convex<Point>;
    using Triangle = pgl::Triangle<Point>;

    const Convex sq(std::vector<Point>{{0, 0}, {4, 0}, {4, 4}, {0, 4}});

    // Degenerate (collinear) triangle along the bottom edge.
    const Triangle deg(Point(0, 0), Point(2, 0), Point(4, 0));
    CHECK_MESSAGE(sq.boundaryContains(deg), sq, " boundaryContains degenerate triangle on edge");

    // Proper triangle strictly inside is not on the boundary.
    const Triangle inner(Point(1, 1), Point(3, 1), Point(2, 3));
    CHECK_FALSE_MESSAGE(sq.boundaryContains(inner), sq, " boundaryContains inner triangle");
}

TEST_CASE("Convex contains and interiorContains Triangle") {
    using Point = pgl::Point<int>;
    using Convex = pgl::Convex<Point>;
    using Triangle = pgl::Triangle<Point>;

    const Convex sq(std::vector<Point>{{0, 0}, {4, 0}, {4, 4}, {0, 4}});

    const Triangle inner({1, 1}, {3, 1}, {2, 3});
    const Triangle on_edge({0, 0}, {4, 0}, {2, 2});
    const Triangle poking({1, 1}, {6, 1}, {2, 3});
    const Triangle outside({5, 5}, {7, 5}, {6, 7});

    CHECK_MESSAGE(sq.contains(inner), sq, " contains inner triangle");
    CHECK_MESSAGE(sq.interiorContains(inner), sq, " interiorContains inner triangle");
    CHECK_MESSAGE(sq.contains(on_edge), sq, " contains edge-vertex triangle");
    CHECK_FALSE_MESSAGE(sq.interiorContains(on_edge), sq, " interiorContains edge-vertex triangle");
    CHECK_FALSE_MESSAGE(sq.contains(poking), sq, " contains poking triangle");
    CHECK_FALSE_MESSAGE(sq.contains(outside), sq, " contains outside triangle");
}

TEST_CASE("Convex and Triangle intersection predicates, both directions") {
    using Point = pgl::Point<int>;
    using Convex = pgl::Convex<Point>;
    using Triangle = pgl::Triangle<Point>;

    const Convex sq(std::vector<Point>{{0, 0}, {4, 0}, {4, 4}, {0, 4}});

    SUBCASE("overlapping: both intersect, interiors intersect") {
        const Triangle t({2, 2}, {6, 2}, {4, 6});
        CHECK_MESSAGE(sq.intersects(t), sq, " intersects ", t);
        CHECK_MESSAGE(t.intersects(sq), t, " intersects ", sq);
        CHECK_MESSAGE(sq.interiorsIntersect(t), sq, " interiorsIntersect ", t);
        CHECK_MESSAGE(t.interiorsIntersect(sq), t, " interiorsIntersect ", sq);
    }

    SUBCASE("disjoint: neither intersects") {
        const Triangle t({10, 10}, {12, 10}, {11, 12});
        CHECK_FALSE_MESSAGE(sq.intersects(t), sq, " intersects disjoint triangle");
        CHECK_FALSE_MESSAGE(t.intersects(sq), "disjoint triangle intersects ", sq);
    }

    SUBCASE("triangle fully inside convex: intersects but interiors only if triangle has area") {
        const Triangle inner({1, 1}, {3, 1}, {2, 3});
        CHECK_MESSAGE(sq.intersects(inner), sq, " intersects inner triangle");
        CHECK_MESSAGE(sq.interiorsIntersect(inner), sq, " interiorsIntersect inner triangle");
        CHECK_MESSAGE(inner.interiorsIntersect(sq), "inner triangle interiorsIntersect ", sq);
    }
}

TEST_CASE("Convex separates Triangle and Triangle separates Convex") {
    using Point = pgl::Point<int>;
    using Convex = pgl::Convex<Point>;
    using Triangle = pgl::Triangle<Point>;

    const Convex sq(std::vector<Point>{{0, 0}, {4, 0}, {4, 4}, {0, 4}});

    // Triangle with two vertices outside on opposite sides separates the square.
    const Triangle t({-1, 1}, {5, 2}, {-1, 3});
    CHECK_MESSAGE(sq.separates(t), sq, " separates spanning triangle");

    // Triangle entirely inside is not separated.
    const Triangle inner({1, 1}, {3, 1}, {2, 3});
    CHECK_FALSE_MESSAGE(sq.separates(inner), sq, " separates inner triangle");

    // Triangle that penetrates the convex from one side.
    const Convex tri(std::vector<Point>{{0, 0}, {4, 0}, {0, 4}});
    const Triangle cutting({2, -1}, {4, -1}, {3, 5});
    CHECK_MESSAGE(cutting.separates(tri), cutting, " separates convex");
    CHECK_MESSAGE(tri.separates(cutting), tri, " separates cutting triangle");
}

TEST_CASE("Convex crosses Triangle") {
    using Point = pgl::Point<int>;
    using Convex = pgl::Convex<Point>;
    using Triangle = pgl::Triangle<Point>;

    const Convex sq(std::vector<Point>{{0, 0}, {4, 0}, {4, 4}, {0, 4}});

    const Triangle spanning({-1, 1}, {5, 2}, {-1, 3});
    CHECK_MESSAGE(sq.crosses(spanning), sq, " crosses spanning triangle");
    CHECK_MESSAGE(spanning.crosses(sq), "spanning triangle crosses ", sq);

    const Triangle contained({1, 1}, {3, 1}, {2, 3});
    CHECK_FALSE_MESSAGE(sq.crosses(contained), sq, " crosses contained triangle");
}

TEST_CASE("Convex intersection with Triangle") {
    using Point = pgl::Point<int>;
    using Convex = pgl::Convex<Point>;
    using Triangle = pgl::Triangle<Point>;
    using Segment = pgl::Segment<Point>;

    const Convex sq(std::vector<Point>{{0, 0}, {4, 0}, {4, 4}, {0, 4}});

    SUBCASE("overlapping area: intersection is a Convex") {
        const Triangle t({2, 2}, {6, 2}, {4, 6});
        const auto result = sq.intersection<int>(t);
        REQUIRE_MESSAGE(result, "sq ∩ overlapping triangle should be non-empty");
        CHECK_MESSAGE(std::holds_alternative<Convex>(*result),
                      "area overlap clips to a Convex");
    }

    SUBCASE("triangle fully inside convex: intersection is the triangle (as Convex)") {
        // Edges along y=1 (horizontal), x=1 (vertical), and y=4-x (slope-1) all
        // intersect the sq boundary at integer points, so int arithmetic is exact.
        const Triangle inner({1, 1}, {3, 1}, {1, 3});
        const auto result = sq.intersection<int>(inner);
        REQUIRE_MESSAGE(result, "sq ∩ inner triangle should be non-empty");
        CHECK_MESSAGE(std::holds_alternative<Convex>(*result),
                      "contained triangle returned as Convex");
    }

    SUBCASE("sharing only an edge: intersection is a Segment") {
        // Triangle whose base lies along the bottom edge of the square (y=0).
        const Triangle t({0, 0}, {4, 0}, {2, -2});
        const auto result = sq.intersection<int>(t);
        REQUIRE_MESSAGE(result, "sq ∩ edge-touching triangle should be non-empty");
        CHECK_MESSAGE(std::holds_alternative<Segment>(*result),
                      "shared-edge intersection is a Segment");
    }

    SUBCASE("disjoint: no intersection") {
        const Triangle t({10, 10}, {12, 10}, {11, 12});
        CHECK_FALSE_MESSAGE(sq.intersection<int>(t), "sq ∩ disjoint triangle should be empty");
    }
}

TEST_CASE("Triangle and Convex squared Hausdorff distance") {
    using Point = pgl::Point<int>;
    using Convex = pgl::Convex<Point>;
    using Triangle = pgl::Triangle<Point>;

    const Convex sq(std::vector<Point>{{0, 0}, {4, 0}, {4, 4}, {0, 4}});
    const Triangle t({8, 0}, {12, 0}, {8, 4});

    // Farthest vertex on either side is at squared distance 64 (opposite corners).
    CHECK(sq.squaredHausdorffDistance<int>(t) == 64);
    CHECK(t.squaredHausdorffDistance<int>(sq) == 64);
}

TEST_CASE("Triangle unites with Convex into a set of regions") {
    using Point = pgl::Point<int>;
    using Convex = pgl::Convex<Point>;
    using Triangle = pgl::Triangle<Point>;

    const Convex square(std::vector<Point>{{0, 0}, {4, 0}, {4, 4}, {0, 4}});
    // Sits on the square's top edge, so the two fuse into a house shape.
    const Triangle roof(Point(0, 4), Point(4, 4), Point(2, 6));

    SUBCASE("the union is the same set whichever operand receives it") {
        const auto fromTriangle = roof.regularizedUnion<int>(square);
        const auto fromConvex = square.regularizedUnion<int>(roof);
        static_assert(std::is_same_v<decltype(fromTriangle), const pgl::PolygonSet<Point>>);
        static_assert(std::is_same_v<decltype(fromConvex), const pgl::PolygonSet<Point>>);
        CHECK(fromTriangle == fromConvex);
        REQUIRE(fromTriangle.componentCount() == 1);
        CHECK(fromTriangle.twiceArea() == 2 * 16 + 8);
        CHECK(fromTriangle.component(0).outer().size() == 5);
    }

    SUBCASE("a triangle inside the square leaves just the square") {
        const Triangle inner(Point(1, 1), Point(3, 1), Point(2, 3));
        const auto result = inner.regularizedUnion<int>(square);
        REQUIRE(result.componentCount() == 1);
        CHECK(result.component(0) == pgl::PolygonWithHoles<Point>(square.asPolygon()));
    }

    SUBCASE("disjoint operands stay two components either way round") {
        const Triangle away(Point(10, 10), Point(12, 10), Point(10, 12));
        CHECK(square.regularizedUnion<int>(away).componentCount() == 2);
        CHECK(away.regularizedUnion<int>(square) == square.regularizedUnion<int>(away));
    }
}

namespace {

// The vertex-by-vertex answer the O(log n) Triangle ⊇ Convex tests replaced.
template <class TriangleT, class ConvexT>
bool containsEveryVertex(const TriangleT& triangle, const ConvexT& polygon, bool interior) {
    for (std::size_t i = 0; i < polygon.size(); ++i) {
        const bool inside = interior ? triangle.interiorContains(polygon[i])
                                     : triangle.contains(polygon[i]);
        if (!inside) {
            return false;
        }
    }
    return true;
}

template <class Number>
void checkTriangleContainsConvexAgainstVertices() {
    using Point = pgl::Point<Number>;
    using Convex = pgl::Convex<Point>;
    using Triangle = pgl::Triangle<Point>;
    std::uint64_t state = 0x9E3779B97F4A7C15ull;
    const auto next = [&state](int range) {
        state = state * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<int>((state >> 33) % static_cast<std::uint64_t>(range));
    };
    const std::vector<Triangle> triangles{
        Triangle(Point(0, 0), Point(12, 0), Point(0, 12)),
        Triangle(Point(-2, -3), Point(14, 1), Point(3, 15)),
        Triangle(Point(0, 0), Point(6, 6), Point(12, 12)),  // collinear
        Triangle(Point(0, 0), Point(12, 0), Point(12, 0)),  // two equal vertices
        Triangle(Point(4, 4), Point(4, 4), Point(4, 4)),    // a point
    };
    for (int trial = 0; trial < 3000; ++trial) {
        std::vector<Point> points;
        const int count = 1 + next(12);
        const int span = 1 + next(13);
        const int ox = next(8) - 2;
        const int oy = next(8) - 2;
        for (int k = 0; k < count; ++k) {
            points.emplace_back(ox + next(span), oy + next(span));
        }
        if (trial % 5 == 0) {
            // Points on the triangles' edges, so boundary contact is common.
            points.emplace_back(next(13), 0);
            points.emplace_back(0, next(13));
        }
        const Convex polygon(points);
        for (const auto& triangle : triangles) {
            CHECK_MESSAGE(triangle.contains(polygon) == containsEveryVertex(triangle, polygon, false),
                          triangle, " contains ", polygon);
            CHECK_MESSAGE(triangle.interiorContains(polygon) == containsEveryVertex(triangle, polygon, true),
                          triangle, " interiorContains ", polygon);
        }
    }
    // Polygons sharing the triangle's edges and a vertex, inside, and poking out.
    const Triangle triangle(Point(0, 0), Point(12, 0), Point(0, 12));
    const Convex touching(std::vector<Point>{{0, 0}, {6, 0}, {6, 6}, {0, 6}});
    CHECK(triangle.contains(touching));
    CHECK_FALSE(triangle.interiorContains(touching));
    const Convex inner(std::vector<Point>{{1, 1}, {5, 1}, {5, 5}, {1, 5}});
    CHECK(triangle.contains(inner));
    CHECK(triangle.interiorContains(inner));
    const Convex poking(std::vector<Point>{{1, 1}, {7, 1}, {7, 6}, {1, 5}});
    CHECK_FALSE(triangle.contains(poking));
    CHECK_FALSE(triangle.interiorContains(poking));
}

}  // namespace

TEST_CASE("Triangle contains Convex agrees with testing every vertex") {
    checkTriangleContainsConvexAgainstVertices<int>();
    checkTriangleContainsConvexAgainstVertices<pgl::ERational>();
}

namespace {

template <class Point>
pgl::Convex<Point> randomTriangleClipConvex(std::mt19937& generator) {
    using Number = typename Point::NumberType;
    std::vector<Point> points;
    const bool scattered = generator() % 3 == 0;
    const int count = scattered ? 1 + static_cast<int>(generator() % 5) : 3 + static_cast<int>(generator() % 60);
    const int radius = 4 + static_cast<int>(generator() % 40);
    for (int i = 0; i < count; ++i) {
        if (scattered) {
            points.emplace_back(Number(static_cast<int>(generator() % 9) - 4),
                                Number(static_cast<int>(generator() % 9) - 4));
        } else {
            const double angle = 2 * 3.141592653589793 * i / count;
            points.emplace_back(Number(static_cast<int>(std::lround(radius * std::cos(angle)))),
                                Number(static_cast<int>(std::lround(radius * std::sin(angle)))));
        }
    }
    return pgl::Convex<Point>(points);
}

}  // namespace

TEST_CASE_TEMPLATE("A triangle clip equals the clip by the triangle as a Convex",
                   Point, pgl::Point<int>, pgl::Point<double>, pgl::Point<pgl::ERational>) {
    using Number = typename Point::NumberType;
    using ResultPoint = pgl::Point<pgl::ERational>;
    std::mt19937 generator(15);
    for (int trial = 0; trial < 400; ++trial) {
        const pgl::Convex<Point> convex = randomTriangleClipConvex<Point>(generator);
        const auto coordinate = [&] { return Number(static_cast<int>(generator() % 81) - 40); };
        const Point a(coordinate(), coordinate());
        const Point b(coordinate(), coordinate());
        // Now and then collinear with a and b, or equal to one of them.
        const Point c = generator() % 5 == 0 ? Point(a.x() + (b.x() - a.x()) * Number(2), a.y() + (b.y() - a.y()) * Number(2))
                                             : Point(coordinate(), coordinate());
        const pgl::Triangle<Point> triangle(a, b, c);
        const auto clipped = convex.template intersection<pgl::ERational>(triangle);
        // Clipped in exact coordinates, where the reference needs no rounding.
        const pgl::Triangle<ResultPoint> exactTriangle{ResultPoint(a), ResultPoint(b), ResultPoint(c)};
        const auto expected = pgl::Convex<ResultPoint>(convex).template intersection<pgl::ERational>(exactTriangle.asConvex());
        CHECK_MESSAGE(clipped == expected, convex, " ", triangle);
        if (clipped && std::holds_alternative<pgl::Convex<ResultPoint>>(*clipped)) {
            CHECK(std::get<pgl::Convex<ResultPoint>>(*clipped).size() >= 3);
        }
    }
}

TEST_CASE("Triangle meets Convex in a convex region") {
    using Point = pgl::Point<int>;
    using Triangle = pgl::Triangle<Point>;
    using ConvexShape = pgl::Convex<Point>;
    using PolygonShape = pgl::Polygon<Point>;
    using Region = pgl::PolygonWithHoles<Point>;

    // x >= 0, y >= 0, x + y <= 6.
    const Triangle corner(Point(0, 0), Point(6, 0), Point(0, 6));
    const ConvexShape square(std::vector<Point>{{2, 2}, {6, 2}, {6, 6}, {2, 6}});

    SUBCASE("the overlap is the corner the hypotenuse leaves") {
        const auto met = corner.regularizedIntersection<int>(square);
        static_assert(std::is_same_v<decltype(met), const pgl::PolygonSet<Point>>);
        REQUIRE(met.componentCount() == 1);
        CHECK(met.twiceArea() == 4);
        CHECK(met.component(0) == Region(PolygonShape({2, 2, 4, 2, 2, 4})));
        CHECK(met == square.regularizedIntersection<int>(corner));
    }

    SUBCASE("touching, disjoint and area-free operands give the empty set") {
        // Meets the hypotenuse at the single point (3,3).
        const ConvexShape tip(std::vector<Point>{{3, 3}, {7, 3}, {7, 7}, {3, 7}});
        CHECK(corner.regularizedIntersection<int>(tip).empty());
        const ConvexShape away(std::vector<Point>{{9, 9}, {11, 9}, {11, 11}, {9, 11}});
        CHECK(corner.regularizedIntersection<int>(away).empty());
        const ConvexShape collapsed(std::vector<Point>{{1, 1}, {2, 2}});
        CHECK(corner.regularizedIntersection<int>(collapsed).empty());
        CHECK(collapsed.regularizedIntersection<int>(corner).empty());
    }
}
