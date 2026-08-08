#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <cmath>
#include <cstdint>
#include <random>
#include <vector>

namespace {

using Point = pgl::Point<int>;
// Named PolygonShape, not Polygon: under MSVC, <windows.h> (pulled in
// transitively by doctest.h) injects a Win32 GDI function called `Polygon`
// into the global namespace, making a file-scope alias of the same name
// ambiguous from TEST_CASE bodies.
using PolygonShape = pgl::Polygon<Point>;

void checkEdge(const pgl::Graph<Point>& graph, const Point& a, const Point& b, bool expected) {
    CHECK(graph.containsEdge(a, b) == expected);
    CHECK(graph.containsEdge(b, a) == expected);
}

template <class Polygon>
auto bruteVisibilityGraph(const Polygon& polygon) {
    using Vertex = typename Polygon::PointType;
    pgl::Graph<Vertex> graph;
    const auto vertices = polygon.vertices();
    for (const auto& vertex : vertices) {
        graph.addVertex(vertex);
    }
    for (std::size_t i = 0; i < vertices.size(); ++i) {
        for (std::size_t j = i + 1; j < vertices.size(); ++j) {
            if (polygon.contains(pgl::Segment<Vertex>(vertices[i], vertices[j]))) {
                graph.addEdge(vertices[i], vertices[j]);
            }
        }
    }
    return graph;
}

}  // namespace

TEST_CASE("Polygon visibility graph is complete for a convex polygon") {
    const PolygonShape square({0, 0, 4, 0, 4, 4, 0, 4});
    const auto graph = square.visibilityGraph();

    CHECK(graph.vertexCount() == 4);
    CHECK(graph.edgeCount() == 6);
    for (const auto& vertex : square) {
        CHECK(graph.containsVertex(vertex));
        CHECK(graph.degree(vertex) == 3);
    }
}

TEST_CASE("Polygon visibility graph excludes diagonals outside a concavity") {
    // An L-shape whose missing upper-right square blocks four vertex pairs.
    const PolygonShape polygon({0, 0, 4, 0, 4, 1, 1, 1, 1, 4, 0, 4});
    const auto graph = polygon.visibilityGraph();

    CHECK(graph.vertexCount() == 6);
    CHECK(graph.edgeCount() == 11);

    // Every boundary edge remains visible.
    for (std::size_t i = 0; i < polygon.size(); ++i) {
        checkEdge(graph, polygon[i], polygon[(i + 1) % polygon.size()], true);
    }

    checkEdge(graph, Point(0, 0), Point(1, 1), true);
    checkEdge(graph, Point(4, 0), Point(1, 4), false);
    checkEdge(graph, Point(4, 1), Point(1, 4), false);
}

TEST_CASE("Polygon visibility graph includes collinear boundary visibility") {
    const PolygonShape polygon({0, 0, 2, 0, 4, 0, 4, 4, 0, 4});
    const auto graph = polygon.visibilityGraph();

    CHECK(graph.vertexCount() == 5);
    CHECK(graph.edgeCount() == 10);
    checkEdge(graph, Point(0, 0), Point(4, 0), true);
}

TEST_CASE("Polygon visibility graph applies lazy translation") {
    PolygonShape polygon({0, 0, 4, 0, 4, 1, 1, 1, 1, 4, 0, 4});
    polygon += Point(10, -3);
    const auto graph = polygon.visibilityGraph();

    CHECK(graph.vertexCount() == 6);
    CHECK(graph.edgeCount() == 11);
    CHECK(graph.containsVertex(Point(10, -3)));
    CHECK_FALSE(graph.containsVertex(Point(0, 0)));
    checkEdge(graph, Point(10, -3), Point(11, -2), true);
    checkEdge(graph, Point(14, -3), Point(11, 1), false);
}

TEST_CASE("Polygon visibility graph preserves vertices without edges") {
    SUBCASE("empty polygon") {
        const PolygonShape polygon;
        const auto graph = polygon.visibilityGraph();
        CHECK(graph.vertexCount() == 0);
        CHECK(graph.edgeCount() == 0);
    }

    SUBCASE("one-point polygon") {
        const PolygonShape polygon({2, 3});
        const auto graph = polygon.visibilityGraph();
        CHECK(graph.vertexCount() == 1);
        CHECK(graph.edgeCount() == 0);
        CHECK(graph.containsVertex(Point(2, 3)));
    }

    SUBCASE("segment polygon") {
        const PolygonShape polygon({0, 0, 2, 0});
        const auto graph = polygon.visibilityGraph();
        CHECK(graph.vertexCount() == 2);
        CHECK(graph.edgeCount() == 1);
        CHECK(graph.containsEdge(Point(0, 0), Point(2, 0)));
    }
}

TEST_CASE("Polygon angular-sweep visibility agrees with direct containment") {
    std::mt19937 random(20260808);
    std::uniform_int_distribution<int> radius(1000, 10000);

    for (int n = 7; n <= 25; n += 2) {
        for (int trial = 0; trial < 20; ++trial) {
            std::vector<Point> vertices;
            vertices.reserve(static_cast<std::size_t>(n));
            for (int i = 0; i < n; ++i) {
                const double angle = 2.0 * std::acos(-1.0) * i / n;
                const int r = radius(random);
                vertices.emplace_back(std::lround(r * std::cos(angle)),
                                      std::lround(r * std::sin(angle)));
            }
            const PolygonShape polygon(vertices);
            REQUIRE(polygon.isSimple());

            const auto expected = bruteVisibilityGraph(polygon);
            const auto actual = polygon.visibilityGraph();
            CHECK(actual.vertexCount() == expected.vertexCount());
            CHECK(actual.edgeCount() == expected.edgeCount());
            for (std::size_t i = 0; i < vertices.size(); ++i) {
                for (std::size_t j = i + 1; j < vertices.size(); ++j) {
                    CHECK(actual.containsEdge(vertices[i], vertices[j]) ==
                          expected.containsEdge(vertices[i], vertices[j]));
                }
            }
        }
    }

    // Exercise arbitrary simple polygons too, including the occasional
    // collinear vertex left by uncrossing random point orders.
    std::uniform_int_distribution<int> coordinate(-10000, 10000);
    for (int trial = 0; trial < 100; ++trial) {
        std::vector<Point> vertices;
        const int n = 7 + trial % 14;
        vertices.reserve(static_cast<std::size_t>(n));
        for (int i = 0; i < n; ++i) {
            vertices.emplace_back(coordinate(random), coordinate(random));
        }
        PolygonShape polygon(vertices);
        polygon.untangle();
        REQUIRE(polygon.isSimple());

        const auto expected = bruteVisibilityGraph(polygon);
        const auto actual = polygon.visibilityGraph();
        CHECK(actual.vertexCount() == expected.vertexCount());
        CHECK(actual.edgeCount() == expected.edgeCount());
        for (const auto& a : polygon) {
            for (const auto& b : polygon) {
                CHECK(actual.containsEdge(a, b) == expected.containsEdge(a, b));
            }
        }
    }
}

TEST_CASE_TEMPLATE("Polygon angular-sweep visibility supports number types",
                   Vertex, pgl::Point<double>, pgl::Point<pgl::Rational<int64_t>>) {
    const pgl::Polygon<Vertex> polygon(std::vector<Vertex>{
        Vertex(0, 0), Vertex(4, 0), Vertex(4, 1),
        Vertex(1, 1), Vertex(1, 4), Vertex(0, 4)});
    const auto expected = bruteVisibilityGraph(polygon);
    const auto actual = polygon.visibilityGraph();
    CHECK(actual.vertexCount() == expected.vertexCount());
    CHECK(actual.edgeCount() == expected.edgeCount());
    for (const auto& a : polygon) {
        for (const auto& b : polygon) {
            CHECK(actual.containsEdge(a, b) == expected.containsEdge(a, b));
        }
    }
}
