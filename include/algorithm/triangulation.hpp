#pragma once

#include "pgl.hpp"

/**
 * @file triangulation.hpp
 * @brief Mutable triangulation over a fixed vertex set or simple polygon.
 *
 * The vertex coordinates are fixed at construction; only the connectivity (the
 * set of triangles and their adjacencies) changes, via @ref Triangulation::flip.
 *
 * The **public interface speaks only in value types** — `pgl::Triangle`,
 * `pgl::Segment`, and `pgl::Point`. Internally the connectivity uses the
 * triangle-with-neighbors layout (each triangle stores three CCW vertex indices
 * and three neighbor-triangle indices, with neighbor `i` across the edge
 * opposite vertex `i`), closed by a single ghost vertex so every edge has two
 * incident triangles. Those internal records (`Tri`, `Edge`) and the vertex
 * indices are **private** and never exposed.
 *
 * A single `unordered_map<Segment, Edge>` bridges the two worlds: it converts an
 * outside-world edge (a `Segment`) into the internal `Edge` handle. A public
 * `Triangle` is resolved by taking one of its edges, looking up that handle, and
 * selecting the incident side whose apex matches the triangle — so no
 * triangle-keyed map is needed.
 *
 * @note Builders that *compute* a triangulation (Delaunay over a point set,
 *       ear-clipping/monotone over a polygon, `flipToDelaunay`) are layered on
 *       top of this structure and live separately; this header provides the
 *       storage, navigation, and the local `flip` mutation, plus construction
 *       from an already-known set of triangles.
 */

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <random>
#include <set>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace pgl {

namespace detail {
// The entity's LabelType, or NoLabel if it has none. Lets the triangulation
// pick up a triangle label the moment pgl::Triangle exposes one, without
// breaking while it still has none.
template <class T, class = void>
struct optional_label {
    using type = NoLabel;
};
template <class T>
struct optional_label<T, std::void_t<typename T::LabelType>> {
    using type = typename T::LabelType;
};
template <class T>
using optional_label_t = typename optional_label<T>::type;
}  // namespace detail

/**
 * @brief Triangulation of a fixed vertex set whose connectivity may change.
 *
 * @tparam TriangleType_ The triangle type stored, e.g. `pgl::Triangle<Point>`.
 *         The point type — and hence any vertex labels — is taken from it.
 * @tparam SegmentType_ The edge type stored, defaulting to the triangle's own
 *         edge-segment type. When built from a container of labeled segments it
 *         is deduced as that labeled type, so segment labels are kept.
 *
 * Any label carried by the input triangles (`TriangleType::LabelType`, once
 * `pgl::Triangle` has one) and segments (`SegmentType::LabelType`) is stored and
 * surfaced again by the accessors. A `flip` resets the labels of the edges and
 * triangles it touches to default-constructed values. Labels are stored via
 * `[[no_unique_address]]`, so they cost nothing when absent (`NoLabel`).
 */
template <TriangleConcept TriangleType_,
          SegmentConcept SegmentType_ = typename TriangleType_::template BoundaryType<false>>
struct Triangulation {
    using TriangleType = TriangleType_;
    using SegmentType = SegmentType_;
    using PointType = typename TriangleType::PointType;
    using NumberType = typename PointType::NumberType;
    using SegmentLabel = typename SegmentType::LabelType;
    using TriangleLabel = detail::optional_label_t<TriangleType>;

    /** @brief Creates an empty triangulation. */
    Triangulation() = default;

    /**
     * @brief Builds a triangulation from a set of triangles.
     *
     * The vertex set is the union of the triangles' vertices (fixed thereafter).
     * Adjacencies, the ghost triangles over the boundary, and the
     * segment-to-edge map are all computed automatically.
     *
     * @tparam TriangleRange Range whose elements are @ref TriangleType.
     * @param tris Triangles forming the triangulation.
     * @pre No triangle is degenerate; the triangles tile a region without
     *      overlaps (i.e. they form a valid triangulation).
     */
    template <class TriangleRange>
        requires TriangleConcept<typename TriangleRange::value_type>
    explicit Triangulation(const TriangleRange& tris) {
        std::unordered_map<PointType, VertexId> vid;
        std::vector<std::array<VertexId, 3>> triples;
        std::vector<TriangleLabel> triLabels;
        const auto idOfPoint = makeVertexInterner(vid);
        for (const auto& tr : tris) {
            triples.push_back({idOfPoint(tr[0]), idOfPoint(tr[1]), idOfPoint(tr[2])});
            triLabels.push_back(detail::copyLabel<TriangleLabel>(tr));
        }
        buildFromTriples(triples, triLabels);
    }

    /**
     * @brief Builds a triangulation from its set of edges.
     *
     * The triangular faces are recovered from the edge set by sorting each
     * vertex's incident edges counterclockwise and tracing the bounded faces
     * (an exact rotation-system / DCEL face walk). The vertex set is the union
     * of the segments' endpoints (fixed thereafter).
     *
     * @tparam SegmentRange Range whose elements are `pgl::Segment`.
     * @param segs Edges of the triangulation.
     * @pre The segments are the edges of a valid triangulation (every bounded
     *      face is a triangle); no segment is degenerate.
     */
    template <class SegmentRange>
        requires SegmentConcept<typename SegmentRange::value_type>
    explicit Triangulation(const SegmentRange& segs) {
        // Intern endpoints and collect the edges as vertex-id pairs.
        std::unordered_map<PointType, VertexId> vid;
        const auto idOfPoint = makeVertexInterner(vid);
        std::vector<std::pair<VertexId, VertexId>> elist;
        for (const auto& s : segs) {
            VertexId a = idOfPoint(s[0]);
            VertexId b = idOfPoint(s[1]);
            if (a != b) {
                elist.emplace_back(a, b);
            }
        }
        const VertexId n = static_cast<VertexId>(vertices_.size());

        // Undirected adjacency, deduplicated.
        std::vector<std::vector<VertexId>> adj(static_cast<std::size_t>(n));
        std::set<std::pair<VertexId, VertexId>> seen;
        for (auto [a, b] : elist) {
            auto k = a < b ? std::pair{a, b} : std::pair{b, a};
            if (seen.insert(k).second) {
                adj[a].push_back(b);
                adj[b].push_back(a);
            }
        }

        // Sort each vertex's neighbors counterclockwise; record their position.
        std::vector<std::unordered_map<VertexId, int>> posIn(static_cast<std::size_t>(n));
        for (VertexId v = 0; v < n; ++v) {
            std::vector<PointType> nbr;
            nbr.reserve(adj[v].size());
            for (VertexId w : adj[v]) {
                nbr.push_back(vertices_[w]);
            }
            sortAround(nbr, vertices_[v]);
            adj[v].clear();
            for (const auto& p : nbr) {
                adj[v].push_back(vid.at(p));
            }
            for (int i = 0; i < static_cast<int>(adj[v].size()); ++i) {
                posIn[v][adj[v][i]] = i;
            }
        }

        // next(u->v): the half-edge leaving v that keeps the face on its left,
        // i.e. v -> (neighbor of v immediately clockwise from u). Tracing it
        // walks each face; bounded faces come out CCW, the outer face CW.
        const auto nextHE = [&](VertexId u, VertexId v) -> std::pair<VertexId, VertexId> {
            const auto& a = adj[v];
            const int deg = static_cast<int>(a.size());
            const int pu = posIn[v].at(u);
            return {v, a[(pu + deg - 1) % deg]};
        };

        std::vector<std::array<VertexId, 3>> triples;
        std::set<std::pair<VertexId, VertexId>> visited;
        for (VertexId v = 0; v < n; ++v) {
            for (VertexId w : adj[v]) {
                std::pair<VertexId, VertexId> h{v, w};
                if (visited.count(h)) {
                    continue;
                }
                std::vector<VertexId> cycle;
                while (visited.insert(h).second) {
                    cycle.push_back(h.first);
                    h = nextHE(h.first, h.second);
                }
                if (cycle.size() == 3 &&
                    orientationSign(vertices_[cycle[0]], vertices_[cycle[1]], vertices_[cycle[2]]) > 0) {
                    triples.push_back({cycle[0], cycle[1], cycle[2]});
                }
            }
        }
        buildFromTriples(triples, std::vector<TriangleLabel>(triples.size()));

        // Carry each input segment's label onto its edge record.
        if constexpr (detail::has_label_v<SegmentLabel>) {
            for (const auto& s : segs) {
                auto it = segToEdge_.find(s);
                if (it != segToEdge_.end()) {
                    it->second.segLabel = detail::copyLabel<SegmentLabel>(s);
                }
            }
        }
    }

    // ---- sizes -----------------------------------------------------------

    /** @brief Number of real vertices (excludes the ghost vertex). */
    [[nodiscard]] std::size_t numVertices() const {
        return ghost_ == NO_TRI ? vertices_.size() : vertices_.size() - 1;
    }

    /** @brief Number of triangles (excludes ghost triangles). */
    [[nodiscard]] std::size_t numTriangles() const {
        return static_cast<std::size_t>(firstGhost_);
    }

    /** @brief Number of undirected edges. */
    [[nodiscard]] std::size_t numEdges() const { return segToEdge_.size(); }

    /** @brief True if the triangulation stores no triangles. */
    [[nodiscard]] bool empty() const { return firstGhost_ == 0; }

    // ---- membership ------------------------------------------------------

    /** @brief True if @p t is one of the triangles of this triangulation. */
    [[nodiscard]] bool contains(const TriangleType& t) const { return idOf(t) != NO_TRI; }

    /** @brief True if @p s is one of the edges of this triangulation. */
    [[nodiscard]] bool contains(const SegmentType& s) const { return segToEdge_.contains(s); }

    // ---- navigation ------------------------------------------------------

    /**
     * @brief The triangle on the other side of @p shared from @p t.
     *
     * @param t A triangle of the triangulation.
     * @param shared An edge of @p t.
     * @return The adjacent triangle across @p shared, or `std::nullopt` if that
     *         edge is on the boundary or the arguments are not part of the mesh.
     */
    [[nodiscard]] std::optional<TriangleType> otherTriangle(const TriangleType& t,
                                                       const SegmentType& shared) const {
        auto se = segToEdge_.find(shared);
        if (se == segToEdge_.end()) {
            return std::nullopt;
        }
        const TriId given = idOf(t);
        if (given == NO_TRI) {
            return std::nullopt;
        }
        const Edge e = se->second;
        const TriId i1 = e.tri;
        const TriId i2 = mirror(e).tri;
        TriId other = (given == i1) ? i2 : (given == i2 ? i1 : NO_TRI);
        if (other == NO_TRI || isGhost(other)) {
            return std::nullopt;  // shared not on t, or boundary
        }
        return triangleValue(other);
    }

    /** @brief The (up to three) triangles sharing an edge with @p t. */
    [[nodiscard]] std::vector<TriangleType> adjacentTriangles(const TriangleType& t) const {
        std::vector<TriangleType> out;
        const TriId id = idOf(t);
        if (id == NO_TRI) {
            return out;
        }
        for (int s = 0; s < 3; ++s) {
            const TriId nb = triangles_[id].nbr[s];
            if (nb != NO_TRI && !isGhost(nb)) {
                out.push_back(triangleValue(nb));
            }
        }
        return out;
    }

    /** @brief The (up to two) triangles incident to edge @p s. */
    [[nodiscard]] std::vector<TriangleType> incidentTriangles(const SegmentType& s) const {
        std::vector<TriangleType> out;
        auto se = segToEdge_.find(s);
        if (se == segToEdge_.end()) {
            return out;
        }
        const Edge e = se->second;
        out.push_back(triangleValue(e.tri));
        const TriId other = mirror(e).tri;
        if (other != NO_TRI && !isGhost(other)) {
            out.push_back(triangleValue(other));
        }
        return out;
    }

    /** @brief Calls `fn(Triangle)` on every triangle. */
    template <class Fn>
    void visitTriangles(Fn fn) const {
        for (TriId t = 0; t < firstGhost_; ++t) {
            fn(triangleValue(t));
        }
    }

    /** @brief Calls `fn(Segment)` on every edge, with its stored label. */
    template <class Fn>
    void visitEdges(Fn fn) const {
        for (const auto& [seg, e] : segToEdge_) {
            (void)seg;
            fn(edgeSegment(e));
        }
    }

    // ---- point location --------------------------------------------------

    /**
     * @brief Finds the triangle containing the query point by walking the mesh.
     *
     * Uses a stochastic (randomized) visibility walk, which terminates with
     * probability one on any valid triangulation, not just Delaunay ones; a
     * generous step cap remains only as a defensive bound.
     *
     * @param p Query point.
     * @return The containing triangle, or `std::nullopt` if @p p lies outside
     *         the triangulated region (or the triangulation is empty).
     */
    [[nodiscard]] std::optional<TriangleType> locate(const PointType& p) const {
        const TriId id = locateId(p);
        if (id == NO_TRI || isGhost(id)) {
            return std::nullopt;
        }
        return triangleValue(id);
    }

    // ---- constrained edges ----------------------------------------------

    /** @brief True if edge @p s is flagged as constrained. */
    [[nodiscard]] bool isConstrained(const SegmentType& s) const {
        auto se = segToEdge_.find(s);
        return se != segToEdge_.end() && bit(triangles_[se->second.tri].constrainedMask, se->second.side);
    }

    /** @brief Flags (or clears) edge @p s as constrained on both incident sides. */
    void setConstrained(const SegmentType& s, bool value = true) {
        auto se = segToEdge_.find(s);
        if (se == segToEdge_.end()) {
            return;
        }
        const Edge e = se->second;
        setBit(triangles_[e.tri].constrainedMask, e.side, value);
        const Edge m = mirror(e);
        if (m.tri != NO_TRI) {
            setBit(triangles_[m.tri].constrainedMask, m.side, value);
        }
    }

    // ---- mutation --------------------------------------------------------

    /** @brief True if edge @p s can be flipped (unconstrained, interior, convex quad). */
    [[nodiscard]] bool flippable(const SegmentType& s) const {
        auto se = segToEdge_.find(s);
        return se != segToEdge_.end() && flippableEdge(se->second);
    }

    /**
     * @brief Flips edge @p s, replacing it by the opposite diagonal.
     *
     * The two triangles sharing @p s are retriangulated across the other
     * diagonal; the segment-to-edge map is updated accordingly.
     *
     * @param s Edge to flip.
     * @return The new diagonal segment, or `std::nullopt` if @p s is not
     *         @ref flippable.
     */
    std::optional<SegmentType> flip(const SegmentType& s) {
        auto se = segToEdge_.find(s);
        if (se == segToEdge_.end()) {
            return std::nullopt;
        }
        const Edge e = se->second;
        if (!flippableEdge(e)) {
            return std::nullopt;
        }
        const TriId t = e.tri;
        const TriId t2 = mirror(e).tri;
        flipEdge(e);
        // The shared edge is gone; re-register the six sides of the two
        // rewritten triangles (the four surrounding edges get fresh handles and
        // the new diagonal is added).
        segToEdge_.erase(s);
        registerSides(t);
        registerSides(t2);
        return edgeSegment(Edge{t, 1});  // new diagonal (side opposite a in t)
    }

    // ---- validation ------------------------------------------------------

    /**
     * @brief Checks the structural invariants (orientation + neighbor symmetry).
     * Intended for debug assertions.
     */
    [[nodiscard]] bool checkInvariants() const {
        for (TriId t = 0; t < static_cast<TriId>(triangles_.size()); ++t) {
            const auto& T = triangles_[t];
            if (!isGhost(t) &&
                !(orientationSign(vertices_[T.v[0]], vertices_[T.v[1]], vertices_[T.v[2]]) > 0)) {
                return false;
            }
            for (int i = 0; i < 3; ++i) {
                const TriId t2 = T.nbr[i];
                if (t2 == NO_TRI) {
                    continue;
                }
                int j = -1;
                for (int s = 0; s < 3; ++s) {
                    if (triangles_[t2].nbr[s] == t) {
                        j = s;
                    }
                }
                if (j < 0) {
                    return false;  // link not mutual
                }
                const VertexId a = T.v[(i + 1) % 3];
                const VertexId b = T.v[(i + 2) % 3];
                const VertexId c = triangles_[t2].v[(j + 1) % 3];
                const VertexId d = triangles_[t2].v[(j + 2) % 3];
                if (!((a == c && b == d) || (a == d && b == c))) {
                    return false;  // links disagree on the shared edge
                }
            }
        }
        return true;
    }

  private:
    // ---- internal vocabulary (never exposed) -----------------------------

    using VertexId = std::int32_t;  // index into vertices_
    using TriId = std::int32_t;     // index into triangles_
    static constexpr TriId NO_TRI = -1;

    // A directed reference to one side of one triangle: `side` selects the edge
    // opposite vertex `side`, i.e. { v[(side+1)%3], v[(side+2)%3] }. As the value
    // stored in segToEdge_ it also carries that edge's label.
    struct Edge {
        TriId tri = NO_TRI;
        std::int8_t side = 0;
        [[no_unique_address]] SegmentLabel segLabel{};
    };

    // One triangle record: three CCW vertices, three neighbors (neighbor i is
    // across the edge opposite v[i]), a constrained-edge mask, and the label.
    struct Tri {
        std::array<VertexId, 3> v{NO_TRI, NO_TRI, NO_TRI};
        std::array<TriId, 3> nbr{NO_TRI, NO_TRI, NO_TRI};
        std::uint8_t constrainedMask = 0;
        [[no_unique_address]] TriangleLabel triLabel{};
    };
    static_assert(sizeof(Tri) == 28 || detail::has_label_v<TriangleLabel>,
                  "Tri should stay 28 bytes when unlabeled");

    std::vector<PointType> vertices_;  // real vertices, then one ghost vertex
    std::vector<Tri> triangles_;       // real triangles [0,firstGhost_), then ghost triangles
    std::unordered_map<SegmentType, Edge> segToEdge_;  // outside edge -> internal handle
    VertexId ghost_ = NO_TRI;          // index of the ghost vertex
    TriId firstGhost_ = 0;             // first ghost triangle index
    mutable TriId hint_ = NO_TRI;      // last located triangle (walk seed)
    mutable std::mt19937 rng_;         // drives the stochastic walk in locateId

    // ---- small helpers ---------------------------------------------------

    static constexpr bool bit(std::uint8_t mask, int i) { return (mask >> i) & 1; }
    static constexpr void setBit(std::uint8_t& mask, int i, bool value) {
        mask = static_cast<std::uint8_t>(value ? (mask | (1u << i)) : (mask & ~(1u << i)));
    }
    static constexpr std::uint8_t mask(bool b0, bool b1, bool b2) {
        return static_cast<std::uint8_t>((b0 ? 1 : 0) | (b1 ? 2 : 0) | (b2 ? 4 : 0));
    }

    [[nodiscard]] bool isGhost(TriId t) const { return t >= firstGhost_; }

    // Side of triangle x whose neighbor is `target` (x shares <=1 edge with it).
    [[nodiscard]] std::int8_t findSide(TriId x, TriId target) const {
        const auto& n = triangles_[x].nbr;
        return static_cast<std::int8_t>(n[0] == target ? 0 : (n[1] == target ? 1 : 2));
    }

    [[nodiscard]] Edge mirror(Edge e) const {
        const TriId t2 = triangles_[e.tri].nbr[e.side];
        if (t2 == NO_TRI) {
            return Edge{NO_TRI, 0};
        }
        return Edge{t2, findSide(t2, e.tri)};
    }

    [[nodiscard]] TriangleType triangleValue(TriId t) const {
        const auto& T = triangles_[t];
        TriangleType tri(vertices_[T.v[0]], vertices_[T.v[1]], vertices_[T.v[2]]);
        // Activates automatically once pgl::Triangle gains a mutable label():
        // until then TriangleLabel is NoLabel and this branch is discarded.
        if constexpr (detail::has_label_v<TriangleLabel>) {
            tri.label() = T.triLabel;
        }
        return tri;
    }

    // Materializes the edge as a Segment, attaching the label stored on @p e.
    // (Transient handles from mirror()/registerSides carry a default label, used
    // only for key construction where labels are ignored.)
    [[nodiscard]] SegmentType edgeSegment(Edge e) const {
        const auto& T = triangles_[e.tri];
        SegmentType s(vertices_[T.v[(e.side + 1) % 3]], vertices_[T.v[(e.side + 2) % 3]]);
        if constexpr (detail::has_label_v<SegmentLabel>) {
            s.label() = e.segLabel;
        }
        return s;
    }

    // The vertex of t not lying on edge s (its apex relative to s).
    [[nodiscard]] static PointType apexOf(const TriangleType& t, const SegmentType& s) {
        for (const auto& p : t.vertices()) {
            if (p != s[0] && p != s[1]) {
                return p;
            }
        }
        return t[0];  // unreachable when s is an edge of t
    }

    // Resolves a public Triangle to its internal id, or NO_TRI if absent: look
    // up one of its edges, then pick the incident side whose apex matches t.
    [[nodiscard]] TriId idOf(const TriangleType& t) const {
        const auto edges = t.edges();
        auto se = segToEdge_.find(edges[0]);
        if (se == segToEdge_.end()) {
            return NO_TRI;
        }
        const PointType apex = apexOf(t, edges[0]);
        const Edge e = se->second;
        if (vertices_[triangles_[e.tri].v[e.side]] == apex) {
            return e.tri;
        }
        const Edge m = mirror(e);
        if (m.tri != NO_TRI && !isGhost(m.tri) && vertices_[triangles_[m.tri].v[m.side]] == apex) {
            return m.tri;
        }
        return NO_TRI;
    }

    void registerSides(TriId t) {
        for (std::int8_t s = 0; s < 3; ++s) {
            segToEdge_[edgeSegment(Edge{t, s})] = Edge{t, s};
        }
    }

    void buildMap() {
        for (TriId t = 0; t < firstGhost_; ++t) {
            registerSides(t);
        }
    }

    // Returns a function that maps a point to its vertex id, appending it to
    // vertices_ (and to `vid`) the first time it is seen.
    auto makeVertexInterner(std::unordered_map<PointType, VertexId>& vid) {
        return [this, &vid](const PointType& p) -> VertexId {
            auto it = vid.find(p);
            if (it != vid.end()) {
                return it->second;
            }
            VertexId id = static_cast<VertexId>(vertices_.size());
            vertices_.push_back(p);
            vid.emplace(p, id);
            return id;
        };
    }

    // Appends the ghost vertex, materializes the triangle records (normalized to
    // CCW) from vertex-index triples and their parallel labels, then links
    // adjacency and the edge map.
    void buildFromTriples(std::vector<std::array<VertexId, 3>>& triples,
                          const std::vector<TriangleLabel>& triLabels) {
        ghost_ = static_cast<VertexId>(vertices_.size());
        vertices_.push_back(PointType{});  // ghost vertex; coordinates unused

        for (std::size_t k = 0; k < triples.size(); ++k) {
            VertexId x = triples[k][0], y = triples[k][1], z = triples[k][2];
            if (orientationSign(vertices_[x], vertices_[y], vertices_[z]) < 0) {
                std::swap(y, z);
            }
            assert(orientationSign(vertices_[x], vertices_[y], vertices_[z]) > 0 &&
                   "Triangulation: degenerate triangle");
            triangles_.push_back(Tri{{x, y, z}, {NO_TRI, NO_TRI, NO_TRI}, 0, triLabels[k]});
        }
        firstGhost_ = static_cast<TriId>(triangles_.size());
        buildAdjacency();
        buildMap();
        assert(checkInvariants());
    }

    // ---- internal predicates / mutation ----------------------------------

    [[nodiscard]] bool flippableEdge(Edge e) const {
        const TriId t = e.tri;
        if (t == NO_TRI || bit(triangles_[t].constrainedMask, e.side)) {
            return false;
        }
        const TriId t2 = triangles_[t].nbr[e.side];
        if (t2 == NO_TRI || isGhost(t) || isGhost(t2)) {
            return false;
        }
        const Edge m = mirror(e);
        const VertexId c = triangles_[t].v[e.side];
        const VertexId a = triangles_[t].v[(e.side + 1) % 3];
        const VertexId b = triangles_[t].v[(e.side + 2) % 3];
        const VertexId d = triangles_[t2].v[m.side];
        const auto oa = orientationSign(vertices_[c], vertices_[d], vertices_[a]);
        const auto ob = orientationSign(vertices_[c], vertices_[d], vertices_[b]);
        return (oa > 0 && ob < 0) || (oa < 0 && ob > 0);  // strictly convex quad
    }

    bool flipEdge(Edge e) {
        if (!flippableEdge(e)) {
            return false;
        }
        const TriId t = e.tri;
        const int i = e.side;
        const Edge m = mirror(e);
        const TriId t2 = m.tri;
        const int j = m.side;

        const VertexId c = triangles_[t].v[i];
        const VertexId a = triangles_[t].v[(i + 1) % 3];
        const VertexId b = triangles_[t].v[(i + 2) % 3];
        const VertexId d = triangles_[t2].v[j];

        const TriId nCA = triangles_[t].nbr[(i + 2) % 3];
        const TriId nBC = triangles_[t].nbr[(i + 1) % 3];
        const TriId nDB = triangles_[t2].nbr[(j + 2) % 3];
        const TriId nAD = triangles_[t2].nbr[(j + 1) % 3];
        const bool cCA = bit(triangles_[t].constrainedMask, (i + 2) % 3);
        const bool cBC = bit(triangles_[t].constrainedMask, (i + 1) % 3);
        const bool cDB = bit(triangles_[t2].constrainedMask, (j + 2) % 3);
        const bool cAD = bit(triangles_[t2].constrainedMask, (j + 1) % 3);

        const int sAD = (nAD != NO_TRI) ? findSide(nAD, t2) : -1;
        const int sBC = (nBC != NO_TRI) ? findSide(nBC, t) : -1;

        triangles_[t].v = {c, a, d};
        triangles_[t].nbr = {nAD, t2, nCA};
        triangles_[t].constrainedMask = mask(cAD, false, cCA);
        triangles_[t].triLabel = TriangleLabel{};  // flipped-in triangle has no source label

        triangles_[t2].v = {c, d, b};
        triangles_[t2].nbr = {nDB, nBC, t};
        triangles_[t2].constrainedMask = mask(cDB, cBC, false);
        triangles_[t2].triLabel = TriangleLabel{};

        if (sAD >= 0) {
            triangles_[nAD].nbr[sAD] = t;
        }
        if (sBC >= 0) {
            triangles_[nBC].nbr[sBC] = t2;
        }
        assert(checkInvariants());
        return true;
    }

    [[nodiscard]] TriId locateId(const PointType& p) const {
        if (triangles_.empty()) {
            return NO_TRI;
        }
        TriId t = (hint_ != NO_TRI && !isGhost(hint_)) ? hint_ : 0;
        TriId from = NO_TRI;
        const std::size_t cap = triangles_.size() * 3 + 16;
        for (std::size_t step = 0; step < cap; ++step) {
            if (isGhost(t)) {
                hint_ = NO_TRI;
                return t;  // outside the triangulated region
            }
            const auto& T = triangles_[t];
            const int begin = static_cast<int>(rng_() % 3);  // random start: provably terminates
            TriId next = NO_TRI;
            for (int k = 0; k < 3; ++k) {
                const int s = (begin + k) % 3;
                if (T.nbr[s] == from) {
                    continue;
                }
                const VertexId ea = T.v[(s + 1) % 3];
                const VertexId eb = T.v[(s + 2) % 3];
                if (orientationSign(vertices_[ea], vertices_[eb], p) < 0) {
                    next = T.nbr[s];
                    break;
                }
            }
            if (next == NO_TRI) {
                hint_ = t;
                return t;
            }
            from = t;
            t = next;
        }
        return t;
    }

    void buildAdjacency() {
        const auto key = [](VertexId u, VertexId w) {
            return u < w ? std::pair<VertexId, VertexId>{u, w} : std::pair<VertexId, VertexId>{w, u};
        };

        std::map<std::pair<VertexId, VertexId>, std::pair<TriId, int>> edges;
        for (TriId t = 0; t < firstGhost_; ++t) {
            for (int i = 0; i < 3; ++i) {
                const VertexId a = triangles_[t].v[(i + 1) % 3];
                const VertexId b = triangles_[t].v[(i + 2) % 3];
                auto k = key(a, b);
                auto it = edges.find(k);
                if (it == edges.end()) {
                    edges.emplace(k, std::pair<TriId, int>{t, i});
                } else {
                    auto [t2, jj] = it->second;
                    triangles_[t].nbr[i] = t2;
                    triangles_[t2].nbr[jj] = t;
                    edges.erase(it);
                }
            }
        }

        std::map<VertexId, std::pair<TriId, int>> ghostEdges;
        for (const auto& [k, val] : edges) {
            (void)k;
            const auto [t, i] = val;
            const VertexId a = triangles_[t].v[(i + 1) % 3];
            const VertexId b = triangles_[t].v[(i + 2) % 3];
            const TriId g = static_cast<TriId>(triangles_.size());
            triangles_.push_back(Tri{{a, b, ghost_}, {NO_TRI, NO_TRI, NO_TRI}, 0});
            triangles_[g].nbr[2] = t;  // side 2 (opposite ghost) is the shared edge {a,b}
            triangles_[t].nbr[i] = g;
            for (auto [realVertex, side] :
                 {std::pair<VertexId, int>{b, 0}, std::pair<VertexId, int>{a, 1}}) {
                auto it = ghostEdges.find(realVertex);
                if (it == ghostEdges.end()) {
                    ghostEdges.emplace(realVertex, std::pair<TriId, int>{g, side});
                } else {
                    auto [g2, s2] = it->second;
                    triangles_[g].nbr[side] = g2;
                    triangles_[g2].nbr[s2] = g;
                    ghostEdges.erase(it);
                }
            }
        }
        assert(ghostEdges.empty() && "Triangulation: open boundary (input is not a triangulation)");
    }
};

// Deduction guides: the stored triangle type is not deducible from the
// container-templated constructors on their own. From a triangle container it is
// the element type (keeping its labels); from a segment container it is
// `Triangle<PointType>`. The `requires` clauses keep the two guides disjoint.
template <class TriangleRange>
    requires TriangleConcept<typename TriangleRange::value_type>
Triangulation(const TriangleRange&)
    -> Triangulation<typename TriangleRange::value_type>;

template <class SegmentRange>
    requires SegmentConcept<typename SegmentRange::value_type>
Triangulation(const SegmentRange&)
    -> Triangulation<Triangle<typename SegmentRange::value_type::PointType>,
                     typename SegmentRange::value_type>;

}  // namespace pgl
