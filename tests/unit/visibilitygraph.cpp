#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

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
