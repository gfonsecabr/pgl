#pragma once

/**
 * @file graph.hpp
 * @brief Simple undirected graph with hashable vertices.
 */

#include "implementation/lattice.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <optional>
#include <queue>
#include <ranges>
#include <stack>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace pgl {

/**
 * @brief Undirected simple graph stored as adjacency sets.
 *
 * Adding an edge also adds both of its endpoints. Self-loops are ignored and
 * repeated edges are coalesced. Vertex and traversal order are unspecified
 * because the graph uses unordered containers.
 *
 * @tparam Vertex Hashable, equality-comparable vertex type.
 */
template <class Vertex>
class Graph {
    using AdjacencyMap = std::unordered_map<Vertex, std::unordered_set<Vertex>>;

public:
    /** Type used to represent a vertex. */
    using VertexType = Vertex;

    /** Set containing the neighbors of a vertex. */
    using NeighborSet = std::unordered_set<Vertex>;

    /** Undirected edge, listing its two endpoints in increasing order. */
    using EdgeType = std::array<Vertex, 2>;

    /**
     * @brief Forward iterator over the vertices of a graph.
     *
     * Dereferencing the iterator yields a const reference because changing a
     * vertex in place would invalidate the graph's hash table.
     */
    class Iterator {
        using BaseIterator = typename AdjacencyMap::const_iterator;

    public:
        using iterator_category = std::forward_iterator_tag;
        using iterator_concept = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = Vertex;
        using pointer = const Vertex*;
        using reference = const Vertex&;

        /** @brief Creates an iterator with no associated graph. */
        Iterator() = default;

        /** @brief Returns the current vertex. */
        reference operator*() const {
            return iterator_->first;
        }

        /** @brief Returns a pointer to the current vertex. */
        pointer operator->() const {
            return &iterator_->first;
        }

        /** @brief Advances to the next vertex. */
        Iterator& operator++() {
            ++iterator_;
            return *this;
        }

        /** @brief Advances to the next vertex and returns the previous position. */
        Iterator operator++(int) {
            Iterator previous = *this;
            ++(*this);
            return previous;
        }

        friend bool operator==(const Iterator&, const Iterator&) = default;

    private:
        friend class Graph;

        explicit Iterator(BaseIterator iterator)
            : iterator_(iterator) {}

        BaseIterator iterator_;
    };

    /**
     * @brief Forward iterator over the undirected edges of a graph.
     *
     * Dereferencing yields an edge by value, as an @ref EdgeType whose first
     * vertex is the smaller of the two. The adjacency map holds both directions
     * of every edge, so the iterator walks the adjacency sets and keeps only
     * the direction that comes out increasing, visiting each edge once.
     *
     * Requires a totally ordered @ref VertexType, unlike the rest of the graph.
     */
    class EdgeIterator {
        using OuterIterator = typename AdjacencyMap::const_iterator;
        using InnerIterator = typename NeighborSet::const_iterator;

    public:
        // Multi-pass, hence a forward iterator to the C++20 concepts, but the
        // edge is built on dereference rather than stored, so the old category
        // stays "input": only the concept admits a reference that is a value.
        using iterator_category = std::input_iterator_tag;
        using iterator_concept = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = EdgeType;
        using reference = EdgeType;

        /** @brief Creates an iterator with no associated graph. */
        EdgeIterator() = default;

        /** @brief Returns the current edge, smaller endpoint first. */
        reference operator*() const {
            return EdgeType{outer_->first, *inner_};
        }

        /** @brief Advances to the next edge. */
        EdgeIterator& operator++() {
            ++inner_;
            skipToEdge();
            return *this;
        }

        /** @brief Advances to the next edge and returns the previous position. */
        EdgeIterator operator++(int) {
            EdgeIterator previous = *this;
            ++(*this);
            return previous;
        }

        friend bool operator==(const EdgeIterator&, const EdgeIterator&) = default;

    private:
        friend class Graph;

        EdgeIterator(OuterIterator iterator, OuterIterator last)
            : outer_(iterator), last_(last) {
            if (outer_ != last_) {
                inner_ = outer_->second.cbegin();
                skipToEdge();
            }
        }

        // Advances the position to the next neighbor larger than its own
        // vertex, which is the one direction of an edge that this iterator
        // reports. Past the last one, the inner iterator is reset to its
        // value-initialized state so that it compares equal to the end
        // iterator's, whatever adjacency set the walk stopped in.
        void skipToEdge() {
            while (outer_ != last_) {
                if (inner_ == outer_->second.cend()) {
                    ++outer_;
                    if (outer_ == last_) {
                        break;
                    }
                    inner_ = outer_->second.cbegin();
                } else if (outer_->first < *inner_) {
                    return;
                } else {
                    ++inner_;
                }
            }
            inner_ = InnerIterator{};
        }

        OuterIterator outer_;
        OuterIterator last_;
        InnerIterator inner_;
    };

    using iterator = Iterator;
    using const_iterator = Iterator;

    /** @brief Creates an empty graph. */
    Graph() = default;

    /**
     * @brief Creates a graph from a list of undirected edges.
     *
     * @param edges Endpoint pairs. Self-loops are ignored.
     */
    explicit Graph(const std::vector<std::array<Vertex, 2>>& edges) {
        for (const auto& [u, v] : edges) {
            addEdge(u, v);
        }
    }

    /**
     * @brief Adds a vertex if it is not already present.
     *
     * @param vertex Vertex to add.
     */
    void addVertex(const Vertex& vertex) {
        adjacency_.try_emplace(vertex);
    }

    /**
     * @brief Adds an undirected edge and its endpoints.
     *
     * A self-loop is ignored, including its endpoint.
     *
     * @param u First endpoint.
     * @param v Second endpoint.
     */
    void addEdge(const Vertex& u, const Vertex& v) {
        if (u != v) {
            adjacency_[u].insert(v);
            adjacency_[v].insert(u);
        }
    }

    /**
     * @brief Tests whether a vertex is present.
     *
     * @param vertex Vertex to find.
     * @return `true` if @p vertex belongs to the graph.
     */
    [[nodiscard]] bool containsVertex(const Vertex& vertex) const {
        return adjacency_.contains(vertex);
    }

    /**
     * @brief Tests whether an undirected edge is present.
     *
     * @param u First endpoint.
     * @param v Second endpoint.
     * @return `true` if the graph contains the edge between @p u and @p v.
     */
    [[nodiscard]] bool containsEdge(const Vertex& u, const Vertex& v) const {
        const auto uIt = adjacency_.find(u);
        return uIt != adjacency_.end() && uIt->second.contains(v);
    }

    /**
     * @brief Returns the degree of a vertex.
     *
     * @param vertex Vertex whose degree to query.
     * @return Number of neighbors, or `-1` if @p vertex is absent.
     */
    [[nodiscard]] int degree(const Vertex& vertex) const {
        const auto vertexIt = adjacency_.find(vertex);
        if (vertexIt == adjacency_.end()) {
            return -1;
        }
        return static_cast<int>(vertexIt->second.size());
    }

    /**
     * @brief Returns the largest vertex degree.
     *
     * @return Maximum degree, or `-1` if the graph is empty.
     */
    [[nodiscard]] int maxDegree() const {
        int result = -1;
        for (const auto& entry : adjacency_) {
            result = std::max(result, static_cast<int>(entry.second.size()));
        }
        return result;
    }

    /** @brief Returns the number of vertices. */
    [[nodiscard]] int vertexCount() const {
        return static_cast<int>(adjacency_.size());
    }

    /** @brief Returns the number of undirected edges. */
    [[nodiscard]] int edgeCount() const {
        std::size_t directedEdgeCount = 0;
        for (const auto& entry : adjacency_) {
            directedEdgeCount += entry.second.size();
        }
        assert(directedEdgeCount % 2 == 0);
        return static_cast<int>(directedEdgeCount / 2);
    }

    /**
     * @brief Removes an undirected edge if it is present.
     *
     * The endpoints remain in the graph.
     *
     * @param u First endpoint.
     * @param v Second endpoint.
     */
    void removeEdge(const Vertex& u, const Vertex& v) {
        const auto uIt = adjacency_.find(u);
        if (uIt == adjacency_.end() || !uIt->second.contains(v)) {
            return;
        }
        uIt->second.erase(v);
        adjacency_.at(v).erase(u);
    }

    /**
     * @brief Removes a vertex and every incident edge.
     *
     * @param vertex Vertex to remove.
     */
    void removeVertex(const Vertex& vertex) {
        const auto vertexIt = adjacency_.find(vertex);
        if (vertexIt == adjacency_.end()) {
            return;
        }

        // Copy because erasing incident edges invalidates adjacency iterators.
        const NeighborSet neighbors = vertexIt->second;
        for (const Vertex& neighbor : neighbors) {
            adjacency_.at(neighbor).erase(vertex);
        }
        adjacency_.erase(vertexIt);
    }

    /** @brief Removes every vertex and edge. */
    void clear() {
        adjacency_.clear();
    }

    /**
     * @brief Returns a lazy view over the vertices.
     *
     * The vertices are the keys of the adjacency map, so the view is the graph
     * iteration range itself: it yields every vertex as a const reference, in
     * unspecified order, copying and allocating nothing. It refers to this
     * graph and is invalidated by anything that modifies it.
     *
     * @return A forward view of the @ref vertexCount() vertices.
     */
    [[nodiscard]] auto vertices() const {
        return std::ranges::subrange(begin(), end());
    }

    /**
     * @brief Returns a lazy view over the undirected edges.
     *
     * Each edge appears exactly once, as an @ref EdgeType holding its two
     * endpoints in increasing order; edges come in unspecified order. Nothing
     * is copied or allocated: the view walks the adjacency sets in place, so it
     * refers to this graph and is invalidated by anything that modifies it.
     *
     * Iterating the whole view costs $O(n + m)$ for a graph with $n$ vertices
     * and $m$ edges, which is also the cost of reaching the first edge of a
     * graph made of isolated vertices.
     *
     * @return A forward view of the @ref edgeCount() edges.
     */
    [[nodiscard]] auto edges() const
        requires std::totally_ordered<Vertex>
    {
        return std::ranges::subrange(
            EdgeIterator(adjacency_.cbegin(), adjacency_.cend()),
            EdgeIterator(adjacency_.cend(), adjacency_.cend())
        );
    }

    /**
     * @brief Returns the neighbors of a vertex.
     *
     * @param vertex Vertex whose neighbors to access.
     * @return Const reference to the vertex's adjacency set.
     * @throws std::out_of_range if @p vertex is absent.
     */
    [[nodiscard]] const NeighborSet& neighbors(const Vertex& vertex) const {
        return adjacency_.at(vertex);
    }

    /**
     * @brief Returns a vertex together with all of its neighbors.
     *
     * @param vertex Vertex whose closed neighborhood to return.
     * @return Closed neighborhood in unspecified order.
     * @throws std::out_of_range if @p vertex is absent.
     */
    [[nodiscard]] NeighborSet closedNeighbors(const Vertex& vertex) const {
        NeighborSet result = adjacency_.at(vertex);
        result.insert(vertex);
        return result;
    }

    /**
     * @brief Traverses a connected component in breadth-first order.
     *
     * @param vertex Starting vertex.
     * @param maxVertices Maximum number of vertices to return; `0` means no
     * limit.
     * @return Visited vertices in breadth-first order, or an empty vector if
     * @p vertex is absent.
     */
    [[nodiscard]] std::vector<Vertex> bfs(const Vertex& vertex, int maxVertices = 0) const {
        if (!containsVertex(vertex) || maxVertices < 0) {
            return {};
        }

        const std::size_t limit = maxVertices == 0
            ? adjacency_.size()
            : static_cast<std::size_t>(maxVertices);

        NeighborSet visited;
        std::vector<Vertex> result;
        std::queue<Vertex> queue;

        visited.insert(vertex);
        queue.push(vertex);
        while (!queue.empty() && result.size() < limit) {
            const Vertex current = queue.front();
            queue.pop();
            result.push_back(current);

            for (const Vertex& neighbor : neighbors(current)) {
                if (visited.insert(neighbor).second) {
                    queue.push(neighbor);
                }
            }
        }

        return result;
    }

    /**
     * @brief Returns the graph's connected components.
     *
     * Isolated vertices form one-vertex components. Components are sorted by
     * decreasing size; their order among equal-size components and the order
     * of vertices within each component are unspecified.
     *
     * @return Connected components, largest first.
     */
    [[nodiscard]] std::vector<std::vector<Vertex>> components() const {
        NeighborSet visited;
        std::vector<std::vector<Vertex>> result;

        for (const auto& entry : adjacency_) {
            if (visited.contains(entry.first)) {
                continue;
            }

            result.push_back(bfs(entry.first));
            visited.insert(result.back().begin(), result.back().end());
        }

        std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
            return a.size() > b.size();
        });
        return result;
    }

    /**
     * @brief Returns the vertex-biconnected blocks of the graph.
     *
     * A bridge is returned as a two-vertex block. Isolated vertices do not
     * belong to an edge-defined block and are omitted. Articulation vertices
     * consequently appear in more than one result. Blocks are sorted by
     * decreasing size; their order among equal-size blocks and the order of
     * vertices within each block are unspecified.
     *
     * The implementation is an iterative form of Tarjan's depth-first search,
     * avoiding recursion depth proportional to the graph size.
     *
     * @return Vertex sets of the biconnected blocks, largest first.
     */
    [[nodiscard]] std::vector<std::vector<Vertex>> biconnectedComponents() const {
        struct SearchData {
            std::size_t index;
            std::size_t low;
        };

        struct Frame {
            Vertex vertex;
            std::optional<Vertex> parent;
            typename NeighborSet::const_iterator nextNeighbor;
            typename NeighborSet::const_iterator endNeighbor;
        };

        std::size_t timer = 0;
        std::unordered_map<Vertex, SearchData> searchData;
        std::vector<std::pair<Vertex, Vertex>> edgeStack;
        std::vector<std::vector<Vertex>> result;

        for (const auto& entry : adjacency_) {
            const Vertex& start = entry.first;
            if (searchData.contains(start)) {
                continue;
            }

            searchData.emplace(start, SearchData{timer, timer});
            ++timer;

            std::stack<Frame> stack;
            stack.push(Frame{start, std::nullopt, entry.second.cbegin(), entry.second.cend()});

            while (!stack.empty()) {
                Frame& frame = stack.top();
                const Vertex vertex = frame.vertex;

                if (frame.nextNeighbor != frame.endNeighbor) {
                    const Vertex neighbor = *frame.nextNeighbor;
                    ++frame.nextNeighbor;

                    if (frame.parent.has_value() && neighbor == *frame.parent) {
                        continue;
                    }

                    const auto neighborData = searchData.find(neighbor);
                    if (neighborData == searchData.end()) {
                        edgeStack.emplace_back(vertex, neighbor);
                        searchData.emplace(neighbor, SearchData{timer, timer});
                        ++timer;

                        const NeighborSet& nextNeighbors = adjacency_.at(neighbor);
                        stack.push(Frame{
                            neighbor,
                            vertex,
                            nextNeighbors.cbegin(),
                            nextNeighbors.cend(),
                        });
                    } else if (neighborData->second.index < searchData.at(vertex).index) {
                        SearchData& vertexData = searchData.at(vertex);
                        vertexData.low = std::min(vertexData.low, neighborData->second.index);
                        edgeStack.emplace_back(vertex, neighbor);
                    }
                    continue;
                }

                const std::optional<Vertex> parent = frame.parent;
                stack.pop();
                if (!parent.has_value()) {
                    continue;
                }

                SearchData& parentData = searchData.at(*parent);
                const SearchData& vertexData = searchData.at(vertex);
                parentData.low = std::min(parentData.low, vertexData.low);

                if (vertexData.low >= parentData.index) {
                    NeighborSet addedVertices;
                    std::vector<Vertex> component;
                    bool foundTreeEdge = false;

                    do {
                        assert(!edgeStack.empty());
                        const auto edge = std::move(edgeStack.back());
                        edgeStack.pop_back();

                        if (addedVertices.insert(edge.first).second) {
                            component.push_back(edge.first);
                        }
                        if (addedVertices.insert(edge.second).second) {
                            component.push_back(edge.second);
                        }
                        foundTreeEdge = edge.first == *parent && edge.second == vertex;
                    } while (!foundTreeEdge);

                    result.push_back(std::move(component));
                }
            }
        }

        std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
            return a.size() > b.size();
        });
        return result;
    }

    /**
     * @brief Computes a vertex clique cover using the DSATUR heuristic.
     *
     * DSATUR colors the complement graph: vertices receiving the same color
     * are pairwise adjacent in this graph and therefore form a clique. Every
     * graph vertex appears in exactly one returned clique. The heuristic does
     * not guarantee a minimum-cardinality cover.
     *
     * At each step, the algorithm selects an uncolored vertex adjacent in the
     * complement to the largest number of distinct colors. Ties are broken by
     * complement degree, then by the graph's unspecified iteration order.
     * Returned cliques are sorted by decreasing size.
     *
     * @return A partition of the vertices into cliques, largest first.
     */
    [[nodiscard]] std::vector<std::vector<Vertex>> cliqueCover() const {
        // DSATUR works with vertex indices, so the vertices are materialized
        // once rather than taken from the lazy view.
        const std::vector<Vertex> graphVertices(begin(), end());
        const std::size_t vertexCount = graphVertices.size();
        const std::size_t uncolored = vertexCount;

        std::vector<std::size_t> colors(vertexCount, uncolored);
        std::vector<std::size_t> complementDegrees(vertexCount);
        std::vector<std::unordered_set<std::size_t>> neighborColors(vertexCount);
        std::vector<std::vector<Vertex>> result;

        for (std::size_t i = 0; i < vertexCount; ++i) {
            complementDegrees[i] = vertexCount - 1 - adjacency_.at(graphVertices[i]).size();
        }

        for (std::size_t coloredCount = 0; coloredCount < vertexCount; ++coloredCount) {
            std::size_t selected = uncolored;
            for (std::size_t i = 0; i < vertexCount; ++i) {
                if (colors[i] != uncolored) {
                    continue;
                }

                if (selected == uncolored ||
                    neighborColors[i].size() > neighborColors[selected].size() ||
                    (neighborColors[i].size() == neighborColors[selected].size() &&
                     complementDegrees[i] > complementDegrees[selected])) {
                    selected = i;
                }
            }
            assert(selected != uncolored);

            std::size_t color = 0;
            while (neighborColors[selected].contains(color)) {
                ++color;
            }
            colors[selected] = color;

            if (color == result.size()) {
                result.emplace_back();
            }
            result[color].push_back(graphVertices[selected]);

            const NeighborSet& selectedNeighbors = adjacency_.at(graphVertices[selected]);
            for (std::size_t i = 0; i < vertexCount; ++i) {
                if (colors[i] == uncolored && i != selected &&
                    !selectedNeighbors.contains(graphVertices[i])) {
                    neighborColors[i].insert(color);
                }
            }
        }

        std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
            return a.size() > b.size();
        });
        return result;
    }

    /**
     * @brief Computes a maximal independent set greedily from low-degree vertices.
     *
     * Vertices are considered in increasing order of their degree in this
     * graph. A vertex is added when it is not adjacent to any vertex already
     * in the result. Equal-degree vertices are considered in the graph's
     * unspecified iteration order. The returned set is maximal: every vertex
     * outside it is adjacent to at least one vertex in it. It is not generally
     * a maximum-cardinality independent set.
     *
     * @return Pairwise non-adjacent vertices forming a maximal independent set.
     */
    [[nodiscard]] std::vector<Vertex> independentSet() const {
        std::vector<Vertex> orderedVertices(begin(), end());
        std::sort(orderedVertices.begin(), orderedVertices.end(), [this](
            const Vertex& a,
            const Vertex& b
        ) {
            return adjacency_.at(a).size() < adjacency_.at(b).size();
        });

        NeighborSet selected;
        std::vector<Vertex> result;
        for (const Vertex& vertex : orderedVertices) {
            const NeighborSet& vertexNeighbors = adjacency_.at(vertex);
            if (std::ranges::none_of(vertexNeighbors, [&selected](const Vertex& neighbor) {
                    return selected.contains(neighbor);
                })) {
                selected.insert(vertex);
                result.push_back(vertex);
            }
        }
        return result;
    }

    /**
     * @brief Computes a minimum spanning forest using Prim's algorithm.
     *
     * The tree of a connected graph is a minimum spanning tree. A disconnected
     * graph yields one minimum spanning tree per connected component, so the
     * result always contains every vertex of this graph, isolated ones
     * included. Ties between equally heavy edges are broken by the graph's
     * unspecified iteration order.
     *
     * The frontier is a lazy binary heap (`std::priority_queue`): an edge is
     * pushed when one endpoint enters the tree and discarded when popped if
     * its other endpoint has since been reached. Complexity is
     * $O(m \log m)$ weight comparisons and $O(m)$ calls to @p weight for a
     * graph with $m$ edges.
     *
     * @tparam WeightFunction Callable taking two vertices and returning a
     * copyable, less-than-comparable edge weight; the weight type is chosen by
     * the callable.
     * @param weight Edge weight function. It must be symmetric, otherwise
     * which of the two values is used for an edge is unspecified.
     * @return A minimum spanning forest of this graph.
     */
    template <class WeightFunction>
    [[nodiscard]] Graph spanningTree(WeightFunction weight) const {
        using Weight = std::invoke_result_t<WeightFunction&, const Vertex&, const Vertex&>;

        struct Candidate {
            Weight weight;
            Vertex from;
            Vertex to;
        };

        // A priority_queue pops its largest element, so order edges by
        // decreasing weight to obtain the lightest frontier edge.
        const auto heavier = [](const Candidate& a, const Candidate& b) {
            return b.weight < a.weight;
        };

        Graph result;
        NeighborSet visited;
        std::priority_queue<Candidate, std::vector<Candidate>, decltype(heavier)> frontier(heavier);

        const auto pushIncidentEdges = [&](const Vertex& vertex) {
            for (const Vertex& neighbor : adjacency_.at(vertex)) {
                if (!visited.contains(neighbor)) {
                    frontier.push(Candidate{weight(vertex, neighbor), vertex, neighbor});
                }
            }
        };

        for (const auto& entry : adjacency_) {
            if (visited.contains(entry.first)) {
                continue;
            }

            // Grow one tree per connected component. The frontier is empty
            // again once a component is exhausted, so it can be reused.
            visited.insert(entry.first);
            result.addVertex(entry.first);
            pushIncidentEdges(entry.first);

            while (!frontier.empty()) {
                const Candidate best = frontier.top();
                frontier.pop();
                if (!visited.insert(best.to).second) {
                    continue;
                }
                result.addEdge(best.from, best.to);
                pushIncidentEdges(best.to);
            }
        }

        return result;
    }

    /**
     * @brief Computes a shortest path between two vertices using Dijkstra's
     * algorithm.
     *
     * The returned path starts at @p source, ends at @p target and lists every
     * vertex along the way; the path from a vertex to itself is that vertex
     * alone. An empty result means that no path exists, either because the two
     * vertices lie in different connected components or because one of them is
     * absent. Ties between equally long paths are broken by the graph's
     * unspecified iteration order.
     *
     * The frontier is a lazy binary heap (`std::priority_queue`): a vertex is
     * pushed once per incident edge relaxed and discarded when popped if it has
     * already been settled. The search stops as soon as @p target is settled.
     * Complexity is $O(m \log m)$ weight comparisons and $O(m)$ calls to
     * @p weight for a graph with $m$ edges.
     *
     * @tparam WeightFunction Callable taking two vertices and returning a
     * copyable edge weight ordered by `<` and added by `+`; the weight type is
     * chosen by the callable.
     * @param source First vertex of the path.
     * @param target Last vertex of the path.
     * @param weight Edge weight function. It must be symmetric and must not
     * return a negative weight; neither is checked, and the returned path is
     * unspecified when either fails.
     * @return The vertices of a shortest path from @p source to @p target, or
     * an empty vector if there is none.
     */
    template <class WeightFunction>
    [[nodiscard]] std::vector<Vertex> shortestPath(
        const Vertex& source,
        const Vertex& target,
        WeightFunction weight
    ) const {
        using Weight = std::invoke_result_t<WeightFunction&, const Vertex&, const Vertex&>;

        struct Candidate {
            Weight weight;
            Vertex from;
            Vertex to;
        };

        if (!containsVertex(source) || !containsVertex(target)) {
            return {};
        }
        if (source == target) {
            return {source};
        }

        // A priority_queue pops its largest element, so order candidates by
        // decreasing distance to obtain the closest unsettled vertex.
        const auto farther = [](const Candidate& a, const Candidate& b) {
            return b.weight < a.weight;
        };

        NeighborSet settled;
        std::unordered_map<Vertex, Vertex> parent;
        std::priority_queue<Candidate, std::vector<Candidate>, decltype(farther)> frontier(farther);

        const auto pushIncidentEdges = [&](const Vertex& vertex, const Candidate& candidate) {
            for (const Vertex& neighbor : adjacency_.at(vertex)) {
                if (!settled.contains(neighbor)) {
                    frontier.push(Candidate{
                        candidate.weight + weight(vertex, neighbor),
                        vertex,
                        neighbor,
                    });
                }
            }
        };

        // Seeding the frontier with the edges leaving the source, rather than
        // with the source itself, keeps every distance a sum of edge weights:
        // the weight type needs no zero of its own.
        settled.insert(source);
        for (const Vertex& neighbor : adjacency_.at(source)) {
            frontier.push(Candidate{weight(source, neighbor), source, neighbor});
        }

        while (!frontier.empty()) {
            const Candidate best = frontier.top();
            frontier.pop();
            if (!settled.insert(best.to).second) {
                continue;
            }
            parent.emplace(best.to, best.from);

            if (best.to == target) {
                std::vector<Vertex> result{target};
                while (result.back() != source) {
                    result.push_back(parent.at(result.back()));
                }
                std::reverse(result.begin(), result.end());
                return result;
            }

            pushIncidentEdges(best.to, best);
        }

        return {};
    }

    /**
     * @brief Computes a shortest path between two vertices using the A*
     * algorithm.
     *
     * This overload has the same result and endpoint behavior as the Dijkstra
     * overload, but uses @p lowerBound to prioritize vertices that appear
     * closer to @p target. The lower bound need not be consistent: a vertex is
     * reopened whenever a shorter path to it is found.
     *
     * The frontier is a lazy binary heap (`std::priority_queue`) ordered by the
     * sum of the path length so far and the estimated remaining distance. With
     * a consistent lower bound, complexity is $O(m \log m)$ weight comparisons
     * and $O(m)$ calls to @p weight and @p lowerBound for a graph with $m$
     * edges. An inconsistent lower bound can cause vertices to be reopened.
     *
     * @tparam WeightFunction Callable taking two vertices and returning a
     * copyable edge weight ordered by `<` and added by `+`; the weight type is
     * chosen by the callable.
     * @tparam LowerBoundFunction Callable taking two vertices and returning a
     * lower bound in the same weight type.
     * @param source First vertex of the path.
     * @param target Last vertex of the path.
     * @param weight Edge weight function. It must be symmetric and must not
     * return a negative weight; neither is checked, and the returned path is
     * unspecified when either fails.
     * @param lowerBound Estimate of the distance between two vertices. For
     * every vertex reached by the search, `lowerBound(vertex, target)` must be
     * nonnegative, must not exceed the shortest distance to @p target, and
     * must be zero when `vertex == target`. These requirements are not checked.
     * @return The vertices of a shortest path from @p source to @p target, or
     * an empty vector if there is none.
     */
    template <class WeightFunction, class LowerBoundFunction>
    [[nodiscard]] std::vector<Vertex> shortestPath(
        const Vertex& source,
        const Vertex& target,
        WeightFunction weight,
        LowerBoundFunction lowerBound
    ) const {
        using Weight =
            std::invoke_result_t<WeightFunction&, const Vertex&, const Vertex&>;

        struct Candidate {
            Weight distance;
            Weight estimate;
            Vertex vertex;
        };

        if (!containsVertex(source) || !containsVertex(target)) {
            return {};
        }
        if (source == target) {
            return {source};
        }

        // A priority_queue pops its largest element, so order candidates by
        // decreasing estimated total distance to obtain the most promising
        // unsettled vertex.
        const auto farther = [](const Candidate& a, const Candidate& b) {
            return b.estimate < a.estimate;
        };

        std::unordered_map<Vertex, Weight> distance;
        std::unordered_map<Vertex, Vertex> parent;
        std::priority_queue<Candidate, std::vector<Candidate>, decltype(farther)>
            frontier(farther);

        const auto relax = [&](const Vertex& from,
                               const Vertex& to,
                               const Weight& fromDistance) {
            if (to == source) {
                return;
            }

            const Weight newDistance = fromDistance + weight(from, to);
            const auto known = distance.find(to);
            if (known != distance.end() && !(newDistance < known->second)) {
                return;
            }

            distance.insert_or_assign(to, newDistance);
            parent.insert_or_assign(to, from);
            frontier.push(Candidate{
                newDistance,
                newDistance + lowerBound(to, target),
                to,
            });
        };

        // As in the Dijkstra overload, seed with the source's edges so the
        // weight type does not need a default-constructed zero.
        for (const Vertex& neighbor : adjacency_.at(source)) {
            const Weight neighborDistance = weight(source, neighbor);
            distance.emplace(neighbor, neighborDistance);
            parent.emplace(neighbor, source);
            frontier.push(Candidate{
                neighborDistance,
                neighborDistance + lowerBound(neighbor, target),
                neighbor,
            });
        }

        while (!frontier.empty()) {
            const Candidate best = frontier.top();
            frontier.pop();

            const auto known = distance.find(best.vertex);
            if (known == distance.end() || known->second < best.distance) {
                continue;
            }

            if (best.vertex == target) {
                std::vector<Vertex> result{target};
                while (result.back() != source) {
                    result.push_back(parent.at(result.back()));
                }
                std::reverse(result.begin(), result.end());
                return result;
            }

            for (const Vertex& neighbor : adjacency_.at(best.vertex)) {
                relax(best.vertex, neighbor, best.distance);
            }
        }

        return {};
    }

    /** @brief Returns an iterator to the first vertex. */
    [[nodiscard]] iterator begin() {
        return iterator(adjacency_.cbegin());
    }

    /** @brief Returns the end iterator. */
    [[nodiscard]] iterator end() {
        return iterator(adjacency_.cend());
    }

    /** @brief Returns a const iterator to the first vertex. */
    [[nodiscard]] const_iterator begin() const {
        return const_iterator(adjacency_.cbegin());
    }

    /** @brief Returns the const end iterator. */
    [[nodiscard]] const_iterator end() const {
        return const_iterator(adjacency_.cend());
    }

    /** @brief Returns a const iterator to the first vertex. */
    [[nodiscard]] const_iterator cbegin() const {
        return begin();
    }

    /** @brief Returns the const end iterator. */
    [[nodiscard]] const_iterator cend() const {
        return end();
    }

private:
    AdjacencyMap adjacency_;
};

}  // namespace pgl
