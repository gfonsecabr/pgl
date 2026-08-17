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

// The endpoints are vertices and so lie on the boundary, which means
// interiorContains is never the test for clear visibility. What is asked is that
// the *relative interior* of the segment miss the boundary: that it hold no
// vertex and meet the relative interior of no edge.
template <class Polygon>
auto bruteClearVisibilityGraph(const Polygon& polygon) {
    using Vertex = typename Polygon::PointType;
    using Edge = pgl::Segment<Vertex>;
    pgl::Graph<Vertex> graph;
    const auto vertices = polygon.vertices();
    for (const auto& vertex : vertices) {
        graph.addVertex(vertex);
    }
    for (std::size_t i = 0; i < vertices.size(); ++i) {
        for (std::size_t j = i + 1; j < vertices.size(); ++j) {
            const Edge segment(vertices[i], vertices[j]);
            bool clear = polygon.contains(segment);
            for (const auto& vertex : vertices) {
                if (vertex != vertices[i] && vertex != vertices[j] && segment.contains(vertex)) {
                    clear = false;
                }
            }
            for (const auto& edge : polygon.edges()) {
                if (segment.interiorsIntersect(edge)) {
                    clear = false;
                }
            }
            if (clear) {
                graph.addEdge(vertices[i], vertices[j]);
            }
        }
    }
    return graph;
}

// A visibility edge survives the reduction when the two sides meeting at each of
// its ends lie in one closed half-plane of the line through it.
template <class Polygon>
auto bruteReducedVisibilityGraph(const Polygon& polygon) {
    using Vertex = typename Polygon::PointType;
    const auto vertices = polygon.vertices();
    const std::size_t n = vertices.size();
    const auto tangent = [&](std::size_t at, const Vertex& other) {
        int seen = 0;
        for (const Vertex& side : {vertices[(at + n - 1) % n], vertices[(at + 1) % n]}) {
            const auto order = pgl::orientationSign(vertices[at], other, side);
            const int sign = order > 0 ? 1 : (order < 0 ? -1 : 0);
            if (sign != 0) {
                if (seen != 0 && seen != sign) {
                    return false;
                }
                seen = sign;
            }
        }
        return true;
    };
    pgl::Graph<Vertex> graph;
    for (const auto& vertex : vertices) {
        graph.addVertex(vertex);
    }
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = i + 1; j < n; ++j) {
            if (polygon.contains(pgl::Segment<Vertex>(vertices[i], vertices[j])) &&
                tangent(i, vertices[j]) && tangent(j, vertices[i])) {
                graph.addEdge(vertices[i], vertices[j]);
            }
        }
    }
    return graph;
}

template <class Vertex>
void checkSameGraph(const pgl::Graph<Vertex>& actual, const pgl::Graph<Vertex>& expected,
                    const std::vector<Vertex>& vertices) {
    CHECK(actual.vertexCount() == expected.vertexCount());
    CHECK(actual.edgeCount() == expected.edgeCount());
    for (std::size_t i = 0; i < vertices.size(); ++i) {
        for (std::size_t j = i + 1; j < vertices.size(); ++j) {
            CHECK(actual.containsEdge(vertices[i], vertices[j]) ==
                  expected.containsEdge(vertices[i], vertices[j]));
        }
    }
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

TEST_CASE("Polygon visibility agrees with direct containment") {
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

TEST_CASE_TEMPLATE("Polygon visibility supports number types",
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

TEST_CASE("Polygon clear visibility keeps only the legal diagonals") {
    // An L-shape. Clear sight forbids grazing, so none of the six sides is an
    // edge and only the diagonals a triangulation could use remain.
    const PolygonShape polygon({0, 0, 4, 0, 4, 1, 1, 1, 1, 4, 0, 4});
    const auto graph = polygon.clearVisibilityGraph();

    CHECK(graph.vertexCount() == 6);
    for (std::size_t i = 0; i < polygon.size(); ++i) {
        checkEdge(graph, polygon[i], polygon[(i + 1) % polygon.size()], false);
    }
    checkEdge(graph, Point(0, 0), Point(1, 1), true);
    checkEdge(graph, Point(4, 0), Point(1, 1), true);
    checkEdge(graph, Point(0, 0), Point(4, 1), true);
    // Blocked by the concavity, exactly as in the full graph.
    checkEdge(graph, Point(4, 0), Point(1, 4), false);
    checkEdge(graph, Point(4, 1), Point(1, 4), false);

    // A convex polygon keeps its diagonals and loses its sides.
    const PolygonShape square({0, 0, 4, 0, 4, 4, 0, 4});
    const auto squareGraph = square.clearVisibilityGraph();
    CHECK(squareGraph.edgeCount() == 2);
    checkEdge(squareGraph, Point(0, 0), Point(4, 4), true);
    checkEdge(squareGraph, Point(4, 0), Point(0, 4), true);
    checkEdge(squareGraph, Point(0, 0), Point(4, 0), false);
}

TEST_CASE("Polygon clear visibility is stopped by a vertex in the way") {
    // The bottom side carries a third vertex, so (0,0) and (4,0) see each other
    // — the segment never leaves the square — but not clearly.
    const PolygonShape polygon({0, 0, 2, 0, 4, 0, 4, 4, 0, 4});
    const auto full = polygon.visibilityGraph();
    const auto clear = polygon.clearVisibilityGraph();

    checkEdge(full, Point(0, 0), Point(4, 0), true);
    checkEdge(clear, Point(0, 0), Point(4, 0), false);
    checkEdge(clear, Point(2, 0), Point(0, 4), true);
    checkEdge(clear, Point(2, 0), Point(4, 4), true);
    // Only the four diagonals that skip no vertex: the five sides are out, and
    // so is the one pair the extra vertex stands between.
    CHECK(clear.edgeCount() == 4);
}

TEST_CASE("Polygon reduced visibility keeps the sides and the bitangents") {
    // A convex polygon bends no shortest path, so only its sides survive.
    const PolygonShape square({0, 0, 4, 0, 4, 4, 0, 4});
    const auto squareGraph = square.reducedVisibilityGraph();
    CHECK(squareGraph.edgeCount() == 4);
    for (std::size_t i = 0; i < square.size(); ++i) {
        checkEdge(squareGraph, square[i], square[(i + 1) % square.size()], true);
    }
    checkEdge(squareGraph, Point(0, 0), Point(4, 4), false);

    // The L-shape has a single reflex corner, at (1,1), and a bitangent needs
    // two, so it too comes down to its six sides even though it is not convex.
    const PolygonShape ell({0, 0, 4, 0, 4, 1, 1, 1, 1, 4, 0, 4});
    const auto ellGraph = ell.reducedVisibilityGraph();
    CHECK(ell.visibilityGraph().edgeCount() == 11);
    CHECK(ellGraph.edgeCount() == 6);
    for (std::size_t i = 0; i < ell.size(); ++i) {
        checkEdge(ellGraph, ell[i], ell[(i + 1) % ell.size()], true);
    }
    // (0,0)-(4,1) and (0,0)-(1,1) are genuine visibility edges, but (0,0) is a
    // convex corner whose two sides straddle either line, so no taut path bends
    // there and neither survives.
    checkEdge(ell.visibilityGraph(), Point(0, 0), Point(4, 1), true);
    checkEdge(ellGraph, Point(0, 0), Point(4, 1), false);
    checkEdge(ellGraph, Point(0, 0), Point(1, 1), false);

    // A zigzag with a spike between two reflex corners: the segment joining them
    // wraps the spike and is the one bitangent, so the reduction keeps the eight
    // sides and exactly that.
    const PolygonShape zigzag({0, 0, 10, 0, 10, 10, 7, 10, 7, 4, 5, 8, 3, 3, 0, 10});
    const auto zigzagGraph = zigzag.reducedVisibilityGraph();
    CHECK(zigzagGraph.edgeCount() == 9);
    checkEdge(zigzagGraph, Point(7, 4), Point(3, 3), true);
    for (std::size_t i = 0; i < zigzag.size(); ++i) {
        checkEdge(zigzagGraph, zigzag[i], zigzag[(i + 1) % zigzag.size()], true);
    }
}

TEST_CASE("Polygon clear and reduced visibility agree with direct containment") {
    std::mt19937 random(20260817);

    // A coarse grid puts collinear vertices everywhere, which is what separates
    // the three conventions; a fine one keeps the polygons in general position.
    for (const int extent : {5, 9, 4000}) {
        std::uniform_int_distribution<int> coordinate(0, extent);
        for (int trial = 0; trial < 60; ++trial) {
            std::vector<Point> vertices;
            const int n = 5 + trial % 16;
            vertices.reserve(static_cast<std::size_t>(n));
            while (static_cast<int>(vertices.size()) < n) {
                const Point candidate(coordinate(random), coordinate(random));
                if (std::find(vertices.begin(), vertices.end(), candidate) == vertices.end()) {
                    vertices.push_back(candidate);
                }
            }
            PolygonShape polygon(vertices);
            polygon.untangle();
            if (!polygon.isSimple() || polygon.isDegenerate() || polygon.size() < 3) {
                continue;
            }
            const auto corners = polygon.vertices();
            checkSameGraph(polygon.visibilityGraph(), bruteVisibilityGraph(polygon), corners);
            checkSameGraph(polygon.clearVisibilityGraph(), bruteClearVisibilityGraph(polygon),
                           corners);
            checkSameGraph(polygon.reducedVisibilityGraph(), bruteReducedVisibilityGraph(polygon),
                           corners);
        }
    }
}

TEST_CASE("Polygon clear and reduced visibility handle degenerate polygons") {
    SUBCASE("empty") {
        const PolygonShape polygon;
        CHECK(polygon.clearVisibilityGraph().vertexCount() == 0);
        CHECK(polygon.reducedVisibilityGraph().vertexCount() == 0);
    }

    SUBCASE("one point") {
        const PolygonShape polygon({2, 3});
        CHECK(polygon.clearVisibilityGraph().vertexCount() == 1);
        CHECK(polygon.clearVisibilityGraph().edgeCount() == 0);
        CHECK(polygon.reducedVisibilityGraph().vertexCount() == 1);
    }

    SUBCASE("collapsed to a segment") {
        const PolygonShape polygon({0, 0, 2, 0, 4, 0});
        // No interior at all, so nothing is clearly visible; every pair is
        // collinear with every side, so nothing is reduced away.
        CHECK(polygon.clearVisibilityGraph().edgeCount() == 0);
        CHECK(polygon.reducedVisibilityGraph().edgeCount() ==
              polygon.visibilityGraph().edgeCount());
    }
}

TEST_CASE("PolygonWithHoles visibility is blocked by the holes") {
    using Region = pgl::PolygonWithHoles<Point>;
    const PolygonShape outer({0, 0, 10, 0, 10, 10, 0, 10});
    const std::vector<PolygonShape> holes{PolygonShape({3, 3, 3, 7, 7, 7, 7, 3})};
    const Region region(outer, holes);
    REQUIRE(region.isValid());

    const auto graph = region.visibilityGraph();
    CHECK(graph.vertexCount() == 8);
    // The hole sits square in the middle, so no pair of opposite outer corners
    // sees through it, while each outer corner sees the hole corner it faces.
    checkEdge(graph, Point(0, 0), Point(10, 10), false);
    checkEdge(graph, Point(10, 0), Point(0, 10), false);
    checkEdge(graph, Point(0, 0), Point(3, 3), true);
    checkEdge(graph, Point(0, 0), Point(7, 7), false);
    // Both rings are boundary, so their own edges are visible but never clear.
    checkEdge(graph, Point(3, 3), Point(3, 7), true);
    checkEdge(region.clearVisibilityGraph(), Point(3, 3), Point(3, 7), false);

    const auto vertices = region.vertices();
    for (std::size_t i = 0; i < vertices.size(); ++i) {
        for (std::size_t j = i + 1; j < vertices.size(); ++j) {
            CHECK(graph.containsEdge(vertices[i], vertices[j]) ==
                  region.contains(pgl::Segment<Point>(vertices[i], vertices[j])));
        }
    }

    // Without holes the outer ring answers on its own.
    const Region plain(outer, std::vector<PolygonShape>{});
    CHECK(plain.visibilityGraph().edgeCount() == outer.visibilityGraph().edgeCount());
    CHECK(plain.reducedVisibilityGraph().edgeCount() == 4);
}

TEST_CASE("Triangulation visibility treats constrained edges as walls") {
    using Mesh = pgl::Triangulation<pgl::Triangle<Point>>;
    const PolygonShape room({0, 0, 10, 0, 10, 10, 0, 10});

    SUBCASE("a free point set sees itself completely") {
        // No constraints and a convex hull for a domain: nothing can block.
        const std::vector<Point> points{Point(0, 0), Point(6, 0), Point(6, 6),
                                        Point(0, 6), Point(3, 2)};
        const Mesh mesh(points);
        const auto graph = mesh.visibilityGraph();
        CHECK(graph.vertexCount() == 5);
        CHECK(graph.edgeCount() == 10);
        // The interior point bends no taut path, so the reduction drops it.
        CHECK(mesh.reducedVisibilityGraph().degree(Point(3, 2)) == 0);
    }

    SUBCASE("a wall across the room blocks sight through it") {
        const std::vector<pgl::Segment<Point>> walls{
            pgl::Segment<Point>(Point(5, 2), Point(5, 8))};
        const Mesh mesh(room, walls);
        const auto graph = mesh.visibilityGraph();

        CHECK(graph.vertexCount() == 6);
        checkEdge(graph, Point(0, 0), Point(10, 0), true);   // along the floor
        checkEdge(graph, Point(0, 5), Point(10, 5), false);  // straight through
        // Around either end of the wall, grazing its tip.
        checkEdge(graph, Point(0, 0), Point(10, 10), false);
        checkEdge(graph, Point(0, 0), Point(5, 2), true);
        checkEdge(graph, Point(10, 10), Point(5, 8), true);
        // The wall itself is visible along its own length, but not clearly.
        checkEdge(graph, Point(5, 2), Point(5, 8), true);
        checkEdge(mesh.clearVisibilityGraph(), Point(5, 2), Point(5, 8), false);

        const auto reduced = mesh.reducedVisibilityGraph();
        // A wall tip wraps a path in every direction, so its visibility edges
        // are tangent at that end and survive whenever the far end is tangent
        // too. The room's own convex corners are never tangent.
        checkEdge(reduced, Point(5, 2), Point(5, 8), true);
        checkEdge(reduced, Point(0, 0), Point(5, 2), false);
    }
}
