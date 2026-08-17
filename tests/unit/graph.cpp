#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <algorithm>
#include <array>
#include <concepts>
#include <map>
#include <ranges>
#include <set>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "pgl.hpp"

namespace {

template <std::ranges::input_range Range>
std::set<std::ranges::range_value_t<Range>> asSet(const Range& values) {
    return std::set<std::ranges::range_value_t<Range>>(
        std::ranges::begin(values),
        std::ranges::end(values)
    );
}

template <class T>
std::set<std::set<T>> asSets(const std::vector<std::vector<T>>& components) {
    std::set<std::set<T>> result;
    for (const auto& component : components) {
        result.emplace(component.begin(), component.end());
    }
    return result;
}

template <class Vertex>
void checkCliqueCover(
    const pgl::Graph<Vertex>& graph,
    const std::vector<std::vector<Vertex>>& cover
) {
    std::unordered_set<Vertex> covered;
    for (const auto& clique : cover) {
        CHECK_FALSE(clique.empty());
        for (std::size_t i = 0; i < clique.size(); ++i) {
            CHECK(covered.insert(clique[i]).second);
            for (std::size_t j = 0; j < i; ++j) {
                CHECK(graph.containsEdge(clique[i], clique[j]));
            }
        }
    }
    CHECK(covered.size() == static_cast<std::size_t>(graph.vertexCount()));
}

// Weight of an edge of the sample weighted graph; heavy for absent edges so a
// caller can also use it on graphs where those edges exist.
int sampleWeight(int u, int v) {
    const std::map<std::pair<int, int>, int> weights{
        {{1, 2}, 1},
        {{2, 3}, 2},
        {{3, 4}, 3},
        {{1, 4}, 4},
        {{1, 3}, 10},
        {{10, 11}, 5},
    };
    const auto it = weights.find({std::min(u, v), std::max(u, v)});
    return it == weights.end() ? 100 : it->second;
}

template <class Vertex, class WeightFunction>
int totalWeight(const pgl::Graph<Vertex>& graph, WeightFunction weight) {
    int result = 0;
    for (const Vertex& vertex : graph) {
        for (const Vertex& neighbor : graph.neighbors(vertex)) {
            result += weight(vertex, neighbor);
        }
    }
    return result / 2;
}

// Total weight of a path, zero for a path of a single vertex.
template <class Vertex, class WeightFunction>
auto pathWeight(const std::vector<Vertex>& path, WeightFunction weight) {
    decltype(weight(path.front(), path.front())) result{};
    for (std::size_t i = 1; i < path.size(); ++i) {
        result = result + weight(path[i - 1], path[i]);
    }
    return result;
}

// A path runs along edges of the graph, from one endpoint to the other, and
// visits no vertex twice.
template <class Vertex>
void checkPath(
    const pgl::Graph<Vertex>& graph,
    const std::vector<Vertex>& path,
    const Vertex& source,
    const Vertex& target
) {
    REQUIRE_FALSE(path.empty());
    CHECK(path.front() == source);
    CHECK(path.back() == target);

    std::unordered_set<Vertex> visited;
    for (std::size_t i = 0; i < path.size(); ++i) {
        CHECK(visited.insert(path[i]).second);
        if (i > 0) {
            CHECK(graph.containsEdge(path[i - 1], path[i]));
        }
    }
}

// A spanning forest has the same vertices and components as its graph, one
// fewer edge than vertices in each of them, and only edges of the graph.
template <class Vertex, class WeightFunction>
void checkSpanningForest(const pgl::Graph<Vertex>& graph, WeightFunction weight) {
    const pgl::Graph<Vertex> forest = graph.spanningTree(weight);

    CHECK(forest.vertexCount() == graph.vertexCount());
    CHECK(asSets(forest.components()) == asSets(graph.components()));
    CHECK(forest.edgeCount() ==
          graph.vertexCount() - static_cast<int>(graph.components().size()));

    for (const Vertex& vertex : forest) {
        CHECK(graph.containsVertex(vertex));
        for (const Vertex& neighbor : forest.neighbors(vertex)) {
            CHECK(graph.containsEdge(vertex, neighbor));
        }
    }
}

}  // namespace

TEST_CASE("Graph adds vertices and undirected edges") {
    const std::vector<std::array<int, 2>> edges{{1, 2}, {2, 3}, {2, 1}, {4, 4}};
    pgl::Graph<int> graph(edges);

    CHECK(graph.vertexCount() == 3);
    CHECK(graph.edgeCount() == 2);
    CHECK(graph.containsVertex(1));
    CHECK_FALSE(graph.containsVertex(4));
    CHECK(graph.containsEdge(1, 2));
    CHECK(graph.containsEdge(2, 1));
    CHECK_FALSE(graph.containsEdge(1, 3));
    CHECK(graph.degree(2) == 2);
    CHECK(graph.degree(4) == -1);
    CHECK(graph.maxDegree() == 2);
    CHECK((asSet(graph.vertices()) == std::set<int>{1, 2, 3}));
    CHECK(std::ranges::distance(graph.vertices()) == graph.vertexCount());
    CHECK((graph.neighbors(2) == std::unordered_set<int>{1, 3}));
    CHECK((graph.closedNeighbors(2) == std::unordered_set<int>{1, 2, 3}));
}

TEST_CASE("Graph removes edges and vertices") {
    pgl::Graph<int> graph;
    graph.addEdge(1, 2);
    graph.addEdge(2, 3);
    graph.addVertex(4);

    graph.removeEdge(1, 2);
    CHECK_FALSE(graph.containsEdge(1, 2));
    CHECK(graph.containsVertex(1));
    CHECK(graph.degree(1) == 0);

    graph.removeVertex(2);
    CHECK_FALSE(graph.containsVertex(2));
    CHECK(graph.degree(3) == 0);
    CHECK(graph.edgeCount() == 0);

    graph.clear();
    CHECK(graph.vertexCount() == 0);
    CHECK(graph.maxDegree() == -1);
}

TEST_CASE("Graph traverses a connected component") {
    pgl::Graph<int> graph;
    graph.addEdge(1, 2);
    graph.addEdge(1, 3);
    graph.addEdge(2, 4);
    graph.addVertex(5);

    const std::vector<int> traversal = graph.bfs(1);
    REQUIRE(traversal.size() == 4);
    CHECK(traversal.front() == 1);
    CHECK((asSet(traversal) == std::set<int>{1, 2, 3, 4}));

    const std::vector<int> limited = graph.bfs(1, 2);
    REQUIRE(limited.size() == 2);
    CHECK(limited.front() == 1);
    CHECK(graph.bfs(9).empty());
    CHECK(graph.bfs(1, -1).empty());
}

TEST_CASE("Graph supports const and empty iteration") {
    static_assert(std::forward_iterator<pgl::Graph<int>::iterator>);
    static_assert(std::forward_iterator<pgl::Graph<int>::const_iterator>);

    pgl::Graph<int> empty;
    CHECK(empty.begin() == empty.end());

    pgl::Graph<int> graph;
    graph.addEdge(1, 2);
    graph.addVertex(3);

    const pgl::Graph<int>& constGraph = graph;
    std::set<int> vertices;
    for (int vertex : constGraph) {
        vertices.insert(vertex);
    }
    CHECK((vertices == std::set<int>{1, 2, 3}));
}

TEST_CASE("Graph views its edges in increasing endpoint order") {
    static_assert(std::forward_iterator<pgl::Graph<int>::EdgeIterator>);

    pgl::Graph<int> graph;
    graph.addEdge(3, 1);
    graph.addEdge(1, 2);
    graph.addEdge(5, 4);
    graph.addVertex(9);

    std::set<std::array<int, 2>> seen;
    for (const auto& edge : graph.edges()) {
        CHECK(edge[0] < edge[1]);
        CHECK(graph.containsEdge(edge[0], edge[1]));
        CHECK(seen.insert(edge).second);
    }
    CHECK((seen == std::set<std::array<int, 2>>{{1, 2}, {1, 3}, {4, 5}}));
    CHECK(std::ranges::distance(graph.edges()) == graph.edgeCount());

    // Multi-pass: a copy of an iterator keeps its own position.
    const auto view = graph.edges();
    auto it = view.begin();
    const auto copy = it;
    ++it;
    CHECK(*copy == *view.begin());
    CHECK(*copy != *it);
}

TEST_CASE("Graph views no edge without one") {
    const pgl::Graph<int> empty;
    CHECK(empty.edges().begin() == empty.edges().end());

    pgl::Graph<int> isolated;
    isolated.addVertex(1);
    isolated.addVertex(2);
    CHECK(isolated.edges().empty());

    isolated.addEdge(1, 2);
    isolated.removeEdge(1, 2);
    CHECK(isolated.edges().empty());
}

TEST_CASE("Graph views edges between geometric vertices") {
    using PointType = pgl::Point<int>;

    pgl::Graph<PointType> graph;
    graph.addEdge(PointType(1, 1), PointType(0, 0));
    graph.addEdge(PointType(0, 0), PointType(0, 2));

    std::set<std::array<PointType, 2>> seen;
    for (const auto& edge : graph.edges()) {
        CHECK(edge[0] < edge[1]);
        CHECK(seen.insert(edge).second);
    }
    CHECK((seen == std::set<std::array<PointType, 2>>{
                       {PointType(0, 0), PointType(0, 2)},
                       {PointType(0, 0), PointType(1, 1)},
                   }));
}

TEST_CASE("Graph returns connected components largest first") {
    pgl::Graph<int> graph;
    graph.addEdge(10, 11);
    graph.addEdge(11, 12);
    graph.addEdge(20, 21);
    graph.addVertex(30);

    const auto components = graph.components();
    REQUIRE(components.size() == 3);
    CHECK(components[0].size() == 3);
    CHECK(components[1].size() == 2);
    CHECK(components[2].size() == 1);
    CHECK((asSets(components) == std::set<std::set<int>>{
        {10, 11, 12},
        {20, 21},
        {30},
    }));
}

TEST_CASE("Graph returns vertex-biconnected blocks largest first") {
    pgl::Graph<std::string> graph;

    // Two triangular blocks sharing articulation vertex c, followed by a
    // bridge. The isolated vertex is not an edge-defined block.
    graph.addEdge("a", "b");
    graph.addEdge("b", "c");
    graph.addEdge("c", "a");
    graph.addEdge("c", "d");
    graph.addEdge("d", "e");
    graph.addEdge("e", "c");
    graph.addEdge("e", "f");
    graph.addVertex("isolated");

    const auto components = graph.biconnectedComponents();
    REQUIRE(components.size() == 3);
    CHECK(components[0].size() == 3);
    CHECK(components[1].size() == 3);
    CHECK(components[2].size() == 2);
    CHECK((asSets(components) == std::set<std::set<std::string>>{
        {"a", "b", "c"},
        {"c", "d", "e"},
        {"e", "f"},
    }));
}

TEST_CASE("Graph handles empty and acyclic biconnected decompositions") {
    const pgl::Graph<int> empty;
    CHECK(empty.components().empty());
    CHECK(empty.biconnectedComponents().empty());

    pgl::Graph<int> tree;
    tree.addEdge(1, 2);
    tree.addEdge(2, 3);

    const auto components = tree.biconnectedComponents();
    REQUIRE(components.size() == 2);
    CHECK((asSets(components) == std::set<std::set<int>>{{1, 2}, {2, 3}}));
}

TEST_CASE("Graph computes a DSATUR clique cover") {
    SUBCASE("empty graph") {
        const pgl::Graph<int> graph;
        const auto cover = graph.cliqueCover();
        CHECK(cover.empty());
        checkCliqueCover(graph, cover);
    }

    SUBCASE("complete graph") {
        pgl::Graph<int> graph;
        for (int u = 0; u < 4; ++u) {
            graph.addVertex(u);
            for (int v = 0; v < u; ++v) {
                graph.addEdge(u, v);
            }
        }

        const auto cover = graph.cliqueCover();
        REQUIRE(cover.size() == 1);
        CHECK(cover.front().size() == 4);
        checkCliqueCover(graph, cover);
    }

    SUBCASE("edgeless graph") {
        pgl::Graph<int> graph;
        for (int vertex = 0; vertex < 4; ++vertex) {
            graph.addVertex(vertex);
        }

        const auto cover = graph.cliqueCover();
        REQUIRE(cover.size() == 4);
        for (const auto& clique : cover) {
            CHECK(clique.size() == 1);
        }
        checkCliqueCover(graph, cover);
    }

    SUBCASE("two disjoint triangles") {
        pgl::Graph<int> graph;
        for (int offset : {0, 3}) {
            graph.addEdge(offset, offset + 1);
            graph.addEdge(offset + 1, offset + 2);
            graph.addEdge(offset + 2, offset);
        }

        const auto cover = graph.cliqueCover();
        REQUIRE(cover.size() == 2);
        CHECK(cover[0].size() == 3);
        CHECK(cover[1].size() == 3);
        checkCliqueCover(graph, cover);
    }

    SUBCASE("odd cycle") {
        pgl::Graph<int> graph;
        for (int vertex = 0; vertex < 5; ++vertex) {
            graph.addEdge(vertex, (vertex + 1) % 5);
        }

        const auto cover = graph.cliqueCover();
        REQUIRE(cover.size() == 3);
        CHECK(cover[0].size() == 2);
        CHECK(cover[1].size() == 2);
        CHECK(cover[2].size() == 1);
        checkCliqueCover(graph, cover);
    }
}

TEST_CASE("Graph computes a minimum spanning tree with Prim") {
    pgl::Graph<int> graph;
    graph.addEdge(1, 2);
    graph.addEdge(2, 3);
    graph.addEdge(3, 4);
    graph.addEdge(1, 4);
    graph.addEdge(1, 3);

    const pgl::Graph<int> tree = graph.spanningTree(sampleWeight);
    checkSpanningForest(graph, sampleWeight);

    // The cycle is broken at its two heaviest edges.
    CHECK(tree.containsEdge(1, 2));
    CHECK(tree.containsEdge(2, 3));
    CHECK(tree.containsEdge(3, 4));
    CHECK_FALSE(tree.containsEdge(1, 4));
    CHECK_FALSE(tree.containsEdge(1, 3));
    CHECK(totalWeight(tree, sampleWeight) == 6);
}

TEST_CASE("Graph spans each component of a disconnected graph") {
    pgl::Graph<int> graph;
    graph.addEdge(1, 2);
    graph.addEdge(2, 3);
    graph.addEdge(1, 3);
    graph.addEdge(10, 11);
    graph.addVertex(20);

    const pgl::Graph<int> forest = graph.spanningTree(sampleWeight);
    checkSpanningForest(graph, sampleWeight);

    CHECK(forest.vertexCount() == 6);
    CHECK(forest.edgeCount() == 3);
    CHECK(forest.degree(20) == 0);
    CHECK(forest.containsEdge(10, 11));
    CHECK(totalWeight(forest, sampleWeight) == 8);
}

TEST_CASE("Graph spans degenerate graphs") {
    SUBCASE("empty graph") {
        const pgl::Graph<int> graph;
        CHECK(graph.spanningTree(sampleWeight).vertexCount() == 0);
    }

    SUBCASE("edgeless graph") {
        pgl::Graph<int> graph;
        graph.addVertex(1);
        graph.addVertex(2);

        const pgl::Graph<int> forest = graph.spanningTree(sampleWeight);
        CHECK(forest.vertexCount() == 2);
        CHECK(forest.edgeCount() == 0);
        checkSpanningForest(graph, sampleWeight);
    }

    SUBCASE("single edge") {
        pgl::Graph<std::string> graph;
        graph.addEdge("a", "b");

        const auto weight = [](const std::string& u, const std::string& v) {
            return static_cast<int>(u.size() + v.size());
        };
        const pgl::Graph<std::string> tree = graph.spanningTree(weight);
        CHECK(tree.containsEdge("a", "b"));
        checkSpanningForest(graph, weight);
    }
}

TEST_CASE("Graph spans geometric vertices with an exact weight") {
    using Point = pgl::Point<int>;

    // A unit square plus a far vertex: the diagonals are the heaviest edges of
    // the square, and the distant vertex attaches by its cheapest incident edge.
    const Point a(0, 0);
    const Point b(4, 0);
    const Point c(4, 4);
    const Point d(0, 4);
    const Point distant(100, 0);

    pgl::Graph<Point> graph;
    for (const Point& u : {a, b, c, d, distant}) {
        for (const Point& v : {a, b, c, d, distant}) {
            graph.addEdge(u, v);
        }
    }

    // The weight function picks the number type; squared distances stay exact
    // in the coordinate type.
    const auto weight = [](const Point& u, const Point& v) {
        return u.squaredDistance(v);
    };
    static_assert(std::same_as<decltype(weight(a, b)), int>);

    const pgl::Graph<Point> tree = graph.spanningTree(weight);
    checkSpanningForest(graph, weight);

    CHECK(tree.containsEdge(distant, b));
    CHECK_FALSE(tree.containsEdge(a, c));
    CHECK_FALSE(tree.containsEdge(b, d));
    CHECK(totalWeight(tree, weight) == 3 * 16 + 96 * 96);
}

TEST_CASE("Graph computes a shortest path with Dijkstra") {
    pgl::Graph<int> graph;
    graph.addEdge(1, 2);
    graph.addEdge(2, 3);
    graph.addEdge(3, 4);
    graph.addEdge(1, 4);
    graph.addEdge(1, 3);

    // Two hops of weight 1 and 2 beat the direct edge of weight 10.
    const std::vector<int> detour = graph.shortestPath(1, 3, sampleWeight);
    checkPath(graph, detour, 1, 3);
    CHECK(detour == std::vector<int>{1, 2, 3});
    CHECK(pathWeight(detour, sampleWeight) == 3);

    // The reverse path of an undirected graph is the same path backwards.
    const std::vector<int> reversed = graph.shortestPath(3, 1, sampleWeight);
    checkPath(graph, reversed, 3, 1);
    CHECK(reversed == std::vector<int>{3, 2, 1});

    // Here the direct edge wins: 4 against 1 + 2 + 3 the long way around.
    const std::vector<int> direct = graph.shortestPath(1, 4, sampleWeight);
    checkPath(graph, direct, 1, 4);
    CHECK(direct == std::vector<int>{1, 4});

    // Both ways around the cycle weigh 5, so only the weight is specified.
    const std::vector<int> tied = graph.shortestPath(2, 4, sampleWeight);
    checkPath(graph, tied, 2, 4);
    CHECK(pathWeight(tied, sampleWeight) == 5);
}

TEST_CASE("Graph finds no path outside a connected component") {
    pgl::Graph<int> graph;
    graph.addEdge(1, 2);
    graph.addEdge(2, 3);
    graph.addEdge(10, 11);
    graph.addVertex(20);

    CHECK(graph.shortestPath(1, 10, sampleWeight).empty());
    CHECK(graph.shortestPath(1, 20, sampleWeight).empty());
    CHECK(graph.shortestPath(10, 11, sampleWeight) == std::vector<int>{10, 11});
}

TEST_CASE("Graph finds shortest paths in degenerate cases") {
    SUBCASE("empty graph") {
        const pgl::Graph<int> graph;
        CHECK(graph.shortestPath(1, 2, sampleWeight).empty());
    }

    SUBCASE("absent endpoint") {
        pgl::Graph<int> graph;
        graph.addEdge(1, 2);

        CHECK(graph.shortestPath(1, 7, sampleWeight).empty());
        CHECK(graph.shortestPath(7, 1, sampleWeight).empty());
        CHECK(graph.shortestPath(7, 7, sampleWeight).empty());
    }

    SUBCASE("path from a vertex to itself") {
        pgl::Graph<int> graph;
        graph.addEdge(1, 2);
        graph.addVertex(20);

        CHECK(graph.shortestPath(1, 1, sampleWeight) == std::vector<int>{1});
        CHECK(graph.shortestPath(20, 20, sampleWeight) == std::vector<int>{20});
    }

    SUBCASE("single edge") {
        pgl::Graph<std::string> graph;
        graph.addEdge("a", "b");

        const auto weight = [](const std::string& u, const std::string& v) {
            return static_cast<int>(u.size() + v.size());
        };
        const std::vector<std::string> path = graph.shortestPath("a", "b", weight);
        checkPath(graph, path, std::string("a"), std::string("b"));
        CHECK(path.size() == 2);
    }
}

TEST_CASE("Graph finds a geodesic shortest path between geometric vertices") {
    using Point = pgl::Point<int>;

    // Two ways around an obstacle: a short one through b, a long one through d.
    const Point a(0, 0);
    const Point b(4, 0);
    const Point c(4, 4);
    const Point d(0, 20);

    pgl::Graph<Point> graph;
    graph.addEdge(a, b);
    graph.addEdge(b, c);
    graph.addEdge(a, d);
    graph.addEdge(d, c);

    // Euclidean lengths add up, unlike the squared distances used elsewhere,
    // so the weight leaves the exact coordinate type.
    const auto weight = [](const Point& u, const Point& v) {
        return u.distance(v);
    };

    const std::vector<Point> path = graph.shortestPath(a, c, weight);
    checkPath(graph, path, a, c);
    CHECK(path == std::vector<Point>{a, b, c});
    CHECK(pathWeight(path, weight) == doctest::Approx(8.0));
}
