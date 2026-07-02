#pragma once

#include "algorithm/polyominoes.hpp"

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
 */

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <random>
#include <set>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
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

// A directed or undirected segment. The triangulation's traversal queries
// accept either, tracing from endpoint [0] to endpoint [1] (for an oriented
// segment that is source -> target; for a plain segment, its sorted endpoints).
template <class T>
concept SegmentOrOriented = SegmentConcept<T> || OrientedSegmentConcept<T>;

// A doubly-infinite straight query: a line or an oriented line. It is traced in
// order by the same directed walk as a segment — the only differences are that
// it enters the hull at a ghost found by a directional descent (rather than at a
// finite source) and never stops at a finite target, so it runs until it leaves
// the hull. An oriented line keeps its own direction; a plain line's order
// follows its (arbitrary) defining-point direction.
template <class T>
concept LineOrOriented = LineConcept<T> || OrientedLineConcept<T>;

// Queries traced in order by the directed walk of visitTrianglesIntersecting:
// segments, oriented segments, lines, oriented lines, and rays. A ray keeps a
// finite source (entered like a segment, or at a directional ghost when the
// source is outside the hull) but is unbounded forwards, so like a line it runs
// until it leaves the hull instead of stopping at a target.
template <class T>
concept DirectedTraversal = SegmentOrOriented<T> || LineOrOriented<T> || RayConcept<T>;

// A connected convex query shape accepted by the region-traversal overloads of
// visitTrianglesIntersecting / trianglesIntersecting. Segments, oriented
// segments, (oriented) lines, and rays are deliberately excluded — they have
// their own directed walk. The set is convex so its intersection with the
// (convex) triangulated hull stays connected, which is what lets a single seed
// flood-fill find every triangle it meets. (Non-convex polygons are future work.)
template <class T>
concept TriangulationRegionQuery =
    PointConcept<T> || TriangleConcept<T> || RectangleConcept<T> || ConvexConcept<T> ||
    DiskConcept<T> || HalfplaneConcept<T>;

// Any shape the triangulation's intersection queries accept: a directed-traversal
// shape (traced in order by the directed walk) or a region-query shape (grown by
// the flood fill, unspecified order). The derived wrappers — trianglesIntersecting,
// the interior-intersecting variants, the edge variants — are all built on
// visitTrianglesIntersecting plus a per-shape predicate, so they take this whole
// set; only the two visitTrianglesIntersecting overloads carry distinct walks.
template <class T>
concept TriangulationQuery = DirectedTraversal<T> || TriangulationRegionQuery<T>;
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
 * Any label carried by the input triangles (`TriangleType::LabelType`) and
 * segments (`SegmentType::LabelType`) is stored and surfaced again by the
 * accessors, and can be read or edited in place through @ref label. A `flip`
 * resets the labels of the edges and triangles it touches to default-constructed
 * values. Labels are stored via `[[no_unique_address]]`, so they cost nothing
 * when absent (`NoLabel`).
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
        const auto idOfPoint = makeVertexInterner(vid);
        // First pass: intern every vertex so vertices_ is complete, then keep it
        // in Hilbert order for cache locality (see the point-set constructor) and
        // rebuild the point->id map against the reordered vertices.
        for (const auto& tr : tris) {
            idOfPoint(tr[0]);
            idOfPoint(tr[1]);
            idOfPoint(tr[2]);
        }
        hilbertSort(vertices_);
        vid.clear();
        for (VertexId i = 0; i < static_cast<VertexId>(vertices_.size()); ++i) {
            vid.emplace(vertices_[i], i);
        }
        // Second pass: resolve each triangle's vertices against the reordered ids.
        std::vector<std::array<VertexId, 3>> triples;
        std::vector<TriangleLabel> triLabels;
        for (const auto& tr : tris) {
            triples.push_back({vid.at(tr[0]), vid.at(tr[1]), vid.at(tr[2])});
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
        // Intern endpoints so vertices_ is complete, then keep it in Hilbert
        // order for cache locality (see the point-set constructor) and rebuild
        // the point->id map against the reordered vertices.
        std::unordered_map<PointType, VertexId> vid;
        const auto idOfPoint = makeVertexInterner(vid);
        for (const auto& s : segs) {
            idOfPoint(s[0]);
            idOfPoint(s[1]);
        }
        hilbertSort(vertices_);
        vid.clear();
        for (VertexId i = 0; i < static_cast<VertexId>(vertices_.size()); ++i) {
            vid.emplace(vertices_[i], i);
        }
        // Collect the edges as vertex-id pairs against the reordered ids.
        std::vector<std::pair<VertexId, VertexId>> elist;
        for (const auto& s : segs) {
            VertexId a = vid.at(s[0]);
            VertexId b = vid.at(s[1]);
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

    /**
     * @brief Builds the Delaunay triangulation of a set of points.
     *
     * The vertex set is the points (deduplicated; fixed thereafter) and the
     * connectivity is their Delaunay triangulation, computed exactly. Points
     * that are collinear with all others, or duplicated, simply carry no
     * incident triangle.
     *
     * @tparam PointRange Range whose elements are `pgl::Point`.
     * @param pts Points to triangulate.
     */
    template <class PointRange>
        requires PointConcept<typename PointRange::value_type>
    explicit Triangulation(const PointRange& pts) {
        std::unordered_map<PointType, VertexId> vid;
        const auto idOfPoint = makeVertexInterner(vid);
        for (const auto& p : pts) {
            idOfPoint(PointType(p));
        }
        // Store the vertices in Hilbert-curve order: spatially close points then
        // sit close together both in vertices_ and — because triangles are
        // created in insertion order — in triangles_. That keeps the incremental
        // build's point-location walks short (each is seeded from the previous
        // insertion) and improves cache locality for later query walks too.
        // Vertex order is purely internal, so this is transparent downstream.
        hilbertSort(vertices_);
        auto triples = delaunayTriples(vertices_);
        buildFromTriples(triples, std::vector<TriangleLabel>(triples.size()));
    }

    /**
     * @brief Builds the constrained Delaunay triangulation of a simple polygon,
     *        optionally with extra interior points and constraint segments.
     *
     * The convex hull of all input vertices is Delaunay-triangulated, the
     * polygon's boundary edges and every constraint segment are inserted as
     * constraints, and the triangles lying outside the polygon (between it and
     * its convex hull) are marked out-of-domain. The public view — @ref triangles,
     * @ref numTriangles, @ref locate, ... — then sees exactly the polygon's
     * interior; @ref locate therefore works for non-convex polygons.
     *
     * The vertex set is the union of the polygon's vertices, the points in
     * @p points, and the endpoints of the segments in @p segments; each segment
     * additionally becomes a constrained edge. Either @p points or @p segments
     * (or both) may be omitted.
     *
     * @param poly Simple polygon (convex or not) to triangulate.
     * @pre @p poly is simple (non-self-intersecting) and non-degenerate, and any
     *      extra points and segments lie inside it (the latter is not checked).
     */
    explicit Triangulation(const Polygon<PointType>& poly) {
        constructConstrained(poly, std::array<PointType, 0>{}, std::array<SegmentType, 0>{});
    }

    /**
     * @overload
     * @brief Adds the interior @p points as extra triangulation vertices.
     * @param points Extra vertices to insert; assumed to lie inside @p poly.
     */
    template <class PointRange>
        requires PointConcept<typename PointRange::value_type>
    Triangulation(const Polygon<PointType>& poly, const PointRange& points) {
        constructConstrained(poly, points, std::array<SegmentType, 0>{});
    }

    /**
     * @overload
     * @brief Adds the interior @p segments as constrained edges and vertices.
     * @param segments Constraint edges (and their endpoint vertices); assumed to
     *        lie inside @p poly.
     */
    template <class SegmentRange>
        requires SegmentConcept<typename SegmentRange::value_type>
    Triangulation(const Polygon<PointType>& poly, const SegmentRange& segments) {
        constructConstrained(poly, std::array<PointType, 0>{}, segments);
    }

    /**
     * @overload
     * @brief Adds both interior @p points and constraint @p segments.
     * @param points Extra vertices to insert; assumed to lie inside @p poly.
     * @param segments Constraint edges (and their endpoint vertices); assumed to
     *        lie inside @p poly.
     */
    template <class PointRange, class SegmentRange>
        requires PointConcept<typename PointRange::value_type> &&
                 SegmentConcept<typename SegmentRange::value_type>
    Triangulation(const Polygon<PointType>& poly, const PointRange& points,
                  const SegmentRange& segments) {
        constructConstrained(poly, points, segments);
    }

    // ---- sizes -----------------------------------------------------------

    /** @brief Number of real vertices (excludes the ghost vertex). */
    [[nodiscard]] std::size_t numVertices() const {
        return ghost_ == NO_TRI ? vertices_.size() : vertices_.size() - 1;
    }

    /** @brief Number of triangles (excludes ghost and out-of-domain fill triangles). */
    [[nodiscard]] std::size_t numTriangles() const { return domainTriangleCount_; }

    /** @brief Number of undirected edges incident to the visible triangulation. */
    [[nodiscard]] std::size_t numEdges() const {
        std::size_t count = 0;
        for (const auto& [seg, e] : segToEdge_) {
            (void)seg;
            if (edgeInDomain(e)) {
                ++count;
            }
        }
        return count;
    }

    /** @brief True if the triangulation stores no in-domain triangles. */
    [[nodiscard]] bool empty() const { return domainTriangleCount_ == 0; }

    // ---- membership ------------------------------------------------------

    /** @brief True if @p t is one of the triangles of this triangulation. */
    [[nodiscard]] bool contains(const TriangleType& t) const { return inDomain(idOf(t)); }

    /** @brief True if @p s is an edge incident to the visible triangulation. */
    [[nodiscard]] bool contains(const SegmentType& s) const {
        auto se = segToEdge_.find(s);
        return se != segToEdge_.end() && edgeInDomain(se->second);
    }

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
        if (!inDomain(other)) {
            return std::nullopt;  // shared not on t, or boundary
        }
        return triangleValue(other);
    }

    /** @brief The (up to three) triangles sharing an edge with @p t. */
    [[nodiscard]] std::vector<TriangleType> edgeAdjacentTriangles(const TriangleType& t) const {
        std::vector<TriangleType> out;
        const TriId id = idOf(t);
        if (id == NO_TRI) {
            return out;
        }
        for (int s = 0; s < 3; ++s) {
            const TriId nb = triangles_[id].nbr[s];
            if (inDomain(nb)) {
                out.push_back(triangleValue(nb));
            }
        }
        return out;
    }

    /**
     * @brief The triangles sharing at least one vertex with @p t (excluding @p t).
     *
     * The union of the vertex fans of @p t's three vertices: every triangle
     * touching @p t at a vertex or across an edge, each listed once. A superset
     * of @ref edgeAdjacentTriangles.
     *
     * @param t A triangle of the triangulation.
     * @return The triangles sharing a vertex with @p t; empty if @p t is not
     *         part of the mesh.
     */
    [[nodiscard]] std::vector<TriangleType> vertexAdjacentTriangles(const TriangleType& t) const {
        std::vector<TriangleType> out;
        const TriId self = idOf(t);
        if (self == NO_TRI) {
            return out;
        }
        // "Already listed" is tracked by the per-triangle walkMark bit (as in
        // visitTrianglesIntersecting): marking `self` first skips t itself, and
        // the guard clears every set bit on the way out so the const query
        // leaves the triangulation pristine.
        std::vector<TriId> marked{self};
        triangles_[self].walkMark = 1;
        struct MarkClearer {
            const std::vector<Tri>& tris;
            const std::vector<TriId>& marked;
            ~MarkClearer() { for (TriId m : marked) tris[m].walkMark = 0; }
        } markClearer{triangles_, marked};
        for (const VertexId w : triangles_[self].v) {
            visitVertexFan(self, w, [&](TriId cur) {
                if (inDomain(cur) && !triangles_[cur].walkMark) {
                    triangles_[cur].walkMark = 1;
                    marked.push_back(cur);
                    out.push_back(triangleValue(cur));
                }
            });
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
        if (inDomain(e.tri)) {
            out.push_back(triangleValue(e.tri));
        }
        const TriId other = mirror(e).tri;
        if (inDomain(other)) {
            out.push_back(triangleValue(other));
        }
        return out;
    }

    /**
     * @brief The triangles incident to vertex @p p — its full fan.
     *
     * @param p A vertex of the triangulation.
     * @return Every in-domain triangle having @p p as one of its vertices, in
     *         rotational order around @p p. Empty if @p p is not a vertex of the
     *         triangulation (or lies outside the triangulated region).
     */
    [[nodiscard]] std::vector<TriangleType> incidentTriangles(const PointType& p) const {
        std::vector<TriangleType> out;
        const TriId start = locateId(p);
        if (start == NO_TRI || isGhost(start)) {
            return out;  // empty triangulation, or p outside the hull
        }
        // The vertex of `start` that coincides with p; p is not a vertex if none.
        const auto& sv = triangles_[start].v;
        VertexId w = NO_TRI;
        for (int i = 0; i < 3; ++i) {
            if (vertices_[sv[i]] == p) {
                w = sv[i];
                break;
            }
        }
        if (w == NO_TRI) {
            return out;
        }
        visitVertexFan(start, w, [&](TriId cur) {
            if (inDomain(cur)) {
                out.push_back(triangleValue(cur));
            }
        });
        return out;
    }

    /**
     * @brief Calls `fn(Triangle)` on every triangle.
     *
     * If @p fn returns a value convertible to `bool`, the visit stops early as
     * soon as it returns `true`; a `void`-returning @p fn visits every triangle.
     *
     * @return `true` if the visit was stopped early, `false` otherwise.
     */
    template <class Fn>
    bool visitTriangles(Fn fn) const {
        for (TriId t = 0; t < firstGhost_; ++t) {
            if (!inDomain(t)) {
                continue;
            }
            if (detail::invokeVisitor(fn, triangleValue(t))) {
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Calls `fn(Segment)` on every edge, with its stored label.
     *
     * If @p fn returns a value convertible to `bool`, the visit stops early as
     * soon as it returns `true`; a `void`-returning @p fn visits every edge.
     *
     * @return `true` if the visit was stopped early, `false` otherwise.
     */
    template <class Fn>
    bool visitEdges(Fn fn) const {
        for (const auto& [seg, e] : segToEdge_) {
            (void)seg;
            if (!edgeInDomain(e)) {
                continue;
            }
            if (detail::invokeVisitor(fn, edgeSegment(e))) {
                return true;
            }
        }
        return false;
    }

    /** @brief Returns all triangles, sorted. */
    [[nodiscard]] std::vector<TriangleType> triangles() const {
        std::vector<TriangleType> out;
        out.reserve(numTriangles());
        visitTriangles([&](const TriangleType& t) { out.push_back(t); });
        std::sort(out.begin(), out.end());
        return out;
    }

    /** @brief Returns all edges, sorted, each with its stored label. */
    [[nodiscard]] std::vector<SegmentType> edges() const {
        std::vector<SegmentType> out;
        out.reserve(numEdges());
        visitEdges([&](const SegmentType& s) { out.push_back(s); });
        std::sort(out.begin(), out.end());
        return out;
    }

    // ---- traversal along a segment or (oriented) line --------------------

    /**
     * @brief Visits every triangle met by the directed query @p s, in order.
     *
     * @p s may be a segment, oriented segment, line, oriented line, or ray. For a
     * segment, it locates `s.source()`, visits the triangles touching it (if
     * any), then traces `s` to `s.target()`. A line or oriented line has no
     * finite endpoints: it enters the hull at the ghost where the directed line
     * `s[0]->s[1]` crosses in (found by a directional descent of the ghost ring)
     * and traces forward until it leaves the hull again. A ray keeps `s.source()`
     * (located, or entered at that directional ghost when the source is outside
     * the hull) but, like a line, runs forward until it leaves the hull rather
     * than stopping at a target. Either way each triangle crossed is visited —
     * including the fan where `s` passes through a vertex —
     * and the set passed to @p f is exactly `{ t : s.intersects(t) }` over the
     * in-domain triangles, in the order they are met along `s[0]->s[1]` (a plain
     * line's order follows its arbitrary defining-point direction). Ghost
     * triangles and (for a polygon) out-of-domain hull-fill triangles are
     * traversed for navigation but never reported, so a query leaving and
     * re-entering the polygon through a reflex pocket simply shows a gap.
     *
     * @p f follows the visitor convention: returning `true` stops the walk
     * early, a `void` return visits all. Returns whether it stopped early.
     * @p s may use a different point type than the triangulation.
     */
    template <detail::DirectedTraversal OS, class Fn>
    bool visitTrianglesIntersecting(const OS& s, Fn f) const {
        if (firstGhost_ == 0) return false;
        const auto a = s[0];
        const auto b = s[1];
        // How the directed query extends past its defining points `a`, `b` (which
        // always give its supporting line and forward direction a->b):
        //  - unboundedBack: no finite source (a line / oriented line). Entered at
        //    the ghost where a->b crosses into the hull, found by a directional
        //    descent, rather than by locating a source.
        //  - unboundedFront: no finite target (a line / oriented line / ray). The
        //    walk never stops at `b`; it runs until it leaves the hull, and picks
        //    each forward exit edge by direction rather than by segment bounds.
        constexpr bool unboundedBack = detail::LineOrOriented<OS>;
        constexpr bool unboundedFront = detail::LineOrOriented<OS> || RayConcept<OS>;

        // "Already emitted" is tracked by a per-triangle mark bit rather than a
        // hash set: O(1) test, no per-lookup allocation. `marked` records which
        // bits were set; the guard clears every one of them on the way out —
        // normal return, early exit, or an exception from f — so the const walk
        // leaves the triangulation pristine. NOTE: because the mark lives on the
        // triangulation, the walk is not reentrant: f must not start another
        // walk on the same Triangulation.
        std::vector<TriId> marked;
        struct MarkClearer {
            const std::vector<Tri>& tris;
            const std::vector<TriId>& marked;
            ~MarkClearer() { for (TriId t : marked) tris[t].walkMark = 0; }
        } markClearer{triangles_, marked};
        bool stop = false;
        // Emit a triangle to f, once. Ghost and out-of-domain (polygon hull-fill)
        // triangles are walked through for navigation but never reported, so the
        // reported set is exactly the in-domain triangles s meets.
        const auto emit = [&](TriId t) {
            if (!stop && inDomain(t) && !triangles_[t].walkMark) {
                triangles_[t].walkMark = 1;
                marked.push_back(t);
                if (detail::invokeVisitor(f, triangleValue(t))) stop = true;
            }
        };
        // The next triangle when rotating around vertex w, having arrived from
        // `from` (NO_TRI to start). Orientation-independent (ghost triangles are
        // not CCW-normalized): leave through the w-incident edge we didn't enter.
        const auto rotateAround = [&](TriId cur, VertexId w, TriId from) -> TriId {
            const int lw = localIndex(cur, w);
            int s1 = -1, s2 = -1;
            for (int s = 0; s < 3; ++s) {
                if (s != lw) {
                    if (s1 < 0) s1 = s; else s2 = s;
                }
            }
            return triangles_[cur].nbr[triangles_[cur].nbr[s1] == from ? s2 : s1];
        };
        // Every real triangle of vertex w's fan (rotate around w through ghosts).
        const auto emitFan = [&](VertexId w, TriId startTri) {
            TriId cur = startTri, from = NO_TRI;
            std::size_t g = 0, lim = triangles_.size() + 1;
            do {
                emit(cur);
                const TriId next = rotateAround(cur, w, from);
                from = cur;
                cur = next;
            } while (cur != startTri && cur != NO_TRI && !stop && ++g < lim);
        };
        // Does the ray from p (on closure of t) toward b enter t's interior?
        // 1 = enters, 0 = no, -1 = runs collinearly along an edge of t.
        const auto rayEnters = [&](TriId t, const auto& p) -> int {
            const auto& v = triangles_[t].v;
            for (int k = 0; k < 3; ++k) {
                const auto& u = vertices_[v[(k + 1) % 3]];
                const auto& w = vertices_[v[(k + 2) % 3]];
                if (orientationSign(u, w, p) == 0) {
                    const auto ob = orientationSign(u, w, b);
                    if (ob < 0) return 0;
                    if (ob == 0) return -1;
                }
            }
            return 1;
        };
        // The fan triangle around w whose interior the ray w->b enters (or NO_TRI).
        const auto forwardAround = [&](VertexId w, TriId startTri) -> TriId {
            TriId cur = startTri, from = NO_TRI;
            std::size_t g = 0, lim = triangles_.size() + 1;
            do {
                if (!isGhost(cur) && rayEnters(cur, vertices_[w]) == 1) return cur;
                const TriId next = rotateAround(cur, w, from);
                from = cur;
                cur = next;
            } while (cur != startTri && cur != NO_TRI && ++g < lim);
            return NO_TRI;
        };
        // Signed progress of vertex w along the directed line a->b (monotone along
        // it); 0 at a, `pmax` at b. Used to keep motion within the segment.
        const auto progress = [&](VertexId w) {
            return (vertices_[w].x() - a.x()) * (b.x() - a.x()) +
                   (vertices_[w].y() - a.y()) * (b.y() - a.y());
        };
        // progress(b): the value at the target, i.e. |a->b|^2; the upper bound.
        const auto pmax = (b.x() - a.x()) * (b.x() - a.x()) + (b.y() - a.y()) * (b.y() - a.y());
        // From a vertex w on the segment, emit its fan and follow the segment: if
        // it enters a triangle interior return that triangle (resume the trace);
        // if it continues collinearly along an edge, hop to the next on-segment
        // vertex and repeat; return NO_TRI when the segment leaves/ends here.
        const auto advanceFromVertex = [&](VertexId w, TriId anchor) -> TriId {
            std::size_t hops = 0, lim = vertices_.size() + 1;
            while (!stop && ++hops < lim) {
                emitFan(w, anchor);
                if (stop) return NO_TRI;
                if constexpr (!unboundedFront) {
                    if (progress(w) >= pmax) return NO_TRI;  // reached the target end
                }
                const TriId interior = forwardAround(w, anchor);
                if (interior != NO_TRI) return interior;
                // Look for a forward neighbor x with edge {w,x} collinear with a->b,
                // not past the target.
                VertexId nextV = NO_TRI;
                TriId nextAnchor = NO_TRI;
                TriId cur = anchor, from = NO_TRI;
                std::size_t g = 0, glim = triangles_.size() + 1;
                do {
                    if (!isGhost(cur)) {
                        for (VertexId y : triangles_[cur].v) {
                            if (y != w && orientationSign(a, b, vertices_[y]) == 0 &&
                                progress(y) > progress(w) &&
                                (unboundedFront || progress(y) <= pmax)) {
                                nextV = y;
                                nextAnchor = cur;
                                break;
                            }
                        }
                    }
                    if (nextV != NO_TRI) break;
                    const TriId next = rotateAround(cur, w, from);
                    from = cur;
                    cur = next;
                } while (cur != anchor && cur != NO_TRI && ++g < glim);
                if (nextV == NO_TRI) return NO_TRI;  // segment leaves or ends at w
                w = nextV;
                anchor = nextAnchor;
            }
            return NO_TRI;
        };

        // When s starts outside the convex hull, find the ghost triangle whose
        // hull edge s crosses to enter it — by *following the ghost ring*, not
        // scanning the whole boundary. The triangulated region is the convex
        // hull, so s meets it in a single interval: one entry suffices.
        //
        // Ghost triangles share a single placeholder apex (they are topological,
        // not Euclidean), so navigation uses only the real hull endpoints: at
        // each ghost step toward whichever endpoint of its hull edge is closer
        // to the supporting line a->b (smaller |orientation determinant|). Once
        // an edge straddles that line, test whether s actually reaches it. This
        // walks O(arc) ghosts toward the crossing rather than all of them.
        [[maybe_unused]] const auto enterFromGhost = [&](TriId g0) -> TriId {
            TriId g = g0;
            std::size_t guard = 0, lim = triangles_.size() + 1;
            while (g != NO_TRI && isGhost(g) && ++guard < lim) {
                const VertexId va = triangles_[g].v[0];
                const VertexId vb = triangles_[g].v[1];
                const auto da = orientationDeterminant(a, b, vertices_[va]);
                const auto db = orientationDeterminant(a, b, vertices_[vb]);
                const bool straddle = da == 0 || db == 0 || (da > 0) != (db > 0);
                if (straddle) {
                    // The hull edge crosses the line a->b; s enters here iff it
                    // actually reaches the edge (else it never meets the hull).
                    return s.intersects(edgeSegment(Edge{g, 2})) ? g : NO_TRI;
                }
                // Same side of the line: step toward the closer endpoint. The
                // ghost sharing va is nbr[1]; the one sharing vb is nbr[0].
                const auto absA = da < 0 ? -da : da;
                const auto absB = db < 0 ? -db : db;
                g = triangles_[g].nbr[absA < absB ? 1 : 0];
            }
            return NO_TRI;
        };

        // Whether the directed line a->b crosses ghost g's hull edge from outside
        // into the hull (rather than exiting it). The interior lies on the side of
        // the hull edge holding the inside triangle's apex; the line enters iff its
        // forward direction a->b points toward that same side. (Line mode only.)
        [[maybe_unused]] const auto lineEnters = [&](TriId g) -> bool {
            const VertexId va = triangles_[g].v[0];
            const VertexId vb = triangles_[g].v[1];
            const auto& iv = triangles_[triangles_[g].nbr[2]].v;  // inside (real) triangle
            VertexId apex = iv[0];
            for (int i = 1; i < 3; ++i) {
                if (iv[i] != va && iv[i] != vb) apex = iv[i];
            }
            const auto insideSide =
                orientationDeterminant(vertices_[va], vertices_[vb], vertices_[apex]);
            // (vb - va) x (b - a): the side of the hull edge that a->b points toward.
            const auto dirCross = orientationDeterminant(vertices_[va], vertices_[vb], b) -
                                  orientationDeterminant(vertices_[va], vertices_[vb], a);
            return dirCross != 0 && (dirCross > 0) == (insideSide > 0);
        };

        // For a line / oriented line: the ghost whose hull edge the directed line
        // a->b crosses to ENTER the hull, or NO_TRI if the line misses it. The
        // O(arc) descent toward the supporting line (the same stepping as
        // enterFromGhost) lands on one of the two hull crossings; if that one is
        // where the line exits, the entry is the only other crossing, reached by
        // walking the ghost ring in one direction to the next straddle. Unlike
        // enterFromGhost it never uses s.intersects: an infinite line meets both
        // straddling edges, so only the direction a->b distinguishes entry from exit.
        [[maybe_unused]] const auto lineEntry = [&]() -> TriId {
            TriId g = firstGhost_, crossing = NO_TRI;
            std::size_t guard = 0, lim = triangles_.size() + 1;
            while (g != NO_TRI && isGhost(g) && ++guard < lim) {
                const VertexId va = triangles_[g].v[0];
                const VertexId vb = triangles_[g].v[1];
                const auto da = orientationDeterminant(a, b, vertices_[va]);
                const auto db = orientationDeterminant(a, b, vertices_[vb]);
                if (da == 0 || db == 0 || (da > 0) != (db > 0)) { crossing = g; break; }
                const auto absA = da < 0 ? -da : da;
                const auto absB = db < 0 ? -db : db;
                g = triangles_[g].nbr[absA < absB ? 1 : 0];
            }
            if (crossing == NO_TRI) return NO_TRI;      // the line misses the hull
            if (lineEnters(crossing)) return crossing;   // the descent found the entry
            // The descent found the exit; the entry is the only other crossing —
            // walk the ghost ring in one direction until the next straddle.
            TriId h = triangles_[crossing].nbr[0], from = crossing;
            guard = 0;
            while (h != NO_TRI && isGhost(h) && ++guard < lim) {
                const VertexId va = triangles_[h].v[0];
                const VertexId vb = triangles_[h].v[1];
                const auto da = orientationDeterminant(a, b, vertices_[va]);
                const auto db = orientationDeterminant(a, b, vertices_[vb]);
                if (da == 0 || db == 0 || (da > 0) != (db > 0)) return h;  // entry crossing
                const TriId next = triangles_[h].nbr[0] == from ? triangles_[h].nbr[1]
                                                                : triangles_[h].nbr[0];
                from = h;
                h = next;
            }
            return NO_TRI;  // unreachable: a line crossing the hull crosses it twice
        };

        // Pick the triangle to begin tracing the component reached at `start`,
        // which contains point `p`; visit p's contact triangles along the way.
        [[maybe_unused]] const auto enterAt = [&](const auto& p, TriId start) -> TriId {
            const auto& v = triangles_[start].v;
            int zeros = 0, z0 = -1, z1 = -1;
            for (int k = 0; k < 3; ++k) {
                if (orientationSign(vertices_[v[(k + 1) % 3]], vertices_[v[(k + 2) % 3]], p) == 0) {
                    ++zeros;
                    if (z0 < 0) z0 = k; else z1 = k;
                }
            }
            if (zeros >= 2) {  // p is a vertex: visit its fan, then continue
                return advanceFromVertex(v[3 - z0 - z1], start);
            }
            if (zeros == 1) {  // p on an edge: visit both sides, continue into the forward one
                const TriId other = triangles_[start].nbr[z0];
                emit(start);
                emit(other);
                if (rayEnters(start, p) == 1) return start;
                if (other != NO_TRI && !isGhost(other) && rayEnters(other, p) == 1) return other;
                const VertexId e1 = v[(z0 + 1) % 3];
                const VertexId e2 = v[(z0 + 2) % 3];
                // Continue only if the segment runs ALONG the shared edge (hop to
                // its forward endpoint). Otherwise the segment crosses the edge
                // into the other side already emitted — if that side is outside
                // the hull, nothing more lies on this side.
                if (orientationSign(vertices_[e1], vertices_[e2], b) != 0) {
                    return NO_TRI;
                }
                return advanceFromVertex(progress(e1) > progress(e2) ? e1 : e2, start);
            }
            emit(start);  // p strictly inside
            return start;
        };

        // Emit exactly the triangle(s) whose closure contains the target b — the
        // one triangle it is interior to, both triangles sharing the edge it lies
        // on, or the whole fan if b is a mesh vertex — then the walk ends. (Unlike
        // enterAt, this never continues, so it cannot over-emit a vertex fan when
        // b merely lies on an edge.)
        [[maybe_unused]] const auto emitTargetContacts = [&](TriId t) {
            const auto& v = triangles_[t].v;
            int zeros = 0, z0 = -1, z1 = -1;
            for (int k = 0; k < 3; ++k) {
                if (orientationSign(vertices_[v[(k + 1) % 3]], vertices_[v[(k + 2) % 3]], b) == 0) {
                    ++zeros;
                    if (z0 < 0) z0 = k; else z1 = k;
                }
            }
            if (zeros >= 2) {  // b is a mesh vertex: every incident triangle touches it
                emitFan(v[3 - z0 - z1], t);
                return;
            }
            emit(t);
            if (zeros == 1) {  // b on an edge: the triangle on the far side also touches it
                emit(triangles_[t].nbr[z0]);
            }
        };

        TriId t;
        TriId prev = NO_TRI;
        if constexpr (unboundedBack) {
            // A line has no finite source: enter the hull at the ghost where the
            // directed line a->b crosses in, then trace forward like a segment.
            const TriId g = lineEntry();
            if (g == NO_TRI) {
                return false;  // the line misses the triangulated region
            }
            prev = g;  // entry boundary edge; don't step back out through it
            t = triangles_[g].nbr[2];
            if (!(t != NO_TRI && !isGhost(t))) {
                return false;
            }
        } else {
            t = locateId(a);
            if (t == NO_TRI) {
                return false;  // empty triangulation
            }
            if (isGhost(t)) {
                // Source outside the hull: walk the ghost ring to the entry edge.
                // A ray, like a line, crosses the hull boundary twice, so it needs
                // the directional entry (and must actually reach it); a segment's
                // source seeds enterFromGhost near its own crossing.
                TriId g;
                if constexpr (unboundedFront) {  // a ray
                    g = lineEntry();
                    if (g != NO_TRI && !s.intersects(edgeSegment(Edge{g, 2}))) {
                        g = NO_TRI;  // the entry is behind the source; the ray misses
                    }
                } else {  // a segment
                    g = enterFromGhost(t);
                }
                if (g == NO_TRI) {
                    return false;  // s never meets the triangulated region
                }
                prev = g;  // entry boundary edge; don't step back out through it
                t = triangles_[g].nbr[2];
                if (!(t != NO_TRI && !isGhost(t))) {
                    return false;
                }
            } else {
                t = enterAt(a, t);  // source inside the hull: start there
            }
        }

        const std::size_t cap = triangles_.size() * 4 + 16;
        std::size_t guard = 0;
        while (!stop && t != NO_TRI) {
            if (++guard > cap) break;  // safety net
            emit(t);
            if (stop) break;
            if constexpr (!unboundedFront) {
                if (pointInClosure(b, t)) {  // target reached: visit its contact triangles
                    emitTargetContacts(t);
                    break;
                }
            }
            const auto& v = triangles_[t].v;
            // Forward exit edge: the supporting line ab crosses it, it isn't where
            // we came from, and we leave forward through it — for a segment that is
            // "the segment ab straddles the edge"; for an unbounded-forward query
            // (ray / line) it is "a->b points out of the triangle across the edge"
            // (no finite endpoint to bound the crossing).
            int exitK = -1;
            for (int k = 0; k < 3; ++k) {
                if (triangles_[t].nbr[k] == prev) continue;
                const auto& u = vertices_[v[(k + 1) % 3]];
                const auto& w = vertices_[v[(k + 2) % 3]];
                const auto du = orientationSign(a, b, u);
                const auto dw = orientationSign(a, b, w);
                const bool straddleLine = (du > 0 && dw < 0) || (du < 0 && dw > 0);
                if (!straddleLine) continue;
                bool forward;
                if constexpr (unboundedFront) {
                    // a->b leaves t outward across (u,w): its direction points to the
                    // side of (u,w) away from the apex v[k] (the triangle interior).
                    const auto apexSide = orientationSign(u, w, vertices_[v[k]]);
                    const auto dirCross = orientationDeterminant(u, w, b) -
                                          orientationDeterminant(u, w, a);
                    forward = dirCross != 0 && (dirCross > 0) != (apexSide > 0);
                } else {
                    const auto ea = orientationSign(u, w, a);
                    const auto eb = orientationSign(u, w, b);
                    forward = (ea > 0 && eb < 0) || (ea < 0 && eb > 0);
                }
                if (forward) { exitK = k; break; }
            }
            if (exitK != -1) {
                prev = t;
                t = triangles_[t].nbr[exitK];
                if (isGhost(t)) {
                    break;  // left the convex hull; by convexity s cannot re-enter
                }
                continue;
            }
            // Degenerate: s leaves t through a vertex or runs along an edge.
            int onCount = 0, on0 = -1, on1 = -1;
            for (int m = 0; m < 3; ++m) {
                if (orientationSign(a, b, vertices_[v[m]]) == 0) {
                    ++onCount;
                    if (on0 < 0) on0 = m; else on1 = m;
                }
            }
            if (onCount >= 1) {
                // s leaves t through the on-line vertex farthest along a->b; from
                // there, advance (fan + collinear hops) until it re-enters an interior.
                const int m = (onCount >= 2 && progress(v[on1]) > progress(v[on0])) ? on1 : on0;
                t = advanceFromVertex(v[m], t);
                prev = NO_TRI;
                continue;
            }
            break;  // no progress (should not happen for a valid triangulation)
        }
        return stop;
    }

    /**
     * @brief Returns the triangles met by @p s (a segment or (oriented) line, in
     *        order, or a region query shape, in an unspecified order).
     *
     * A materialized form of @ref visitTrianglesIntersecting: the in-domain
     * triangles `t` with `s.intersects(t)`, in the order they are encountered
     * along `s`. The order is preserved (not sorted), since it is the point of
     * the traversal.
     *
     * @param s A directed query — segment, oriented segment, line, or oriented
     *          line — (traced in order) or a region query shape (reported in an
     *          unspecified order); may use a different
     *          point type.
     */
    template <detail::TriangulationQuery OS>
    [[nodiscard]] std::vector<TriangleType> trianglesIntersecting(const OS& s) const {
        std::vector<TriangleType> out;
        visitTrianglesIntersecting(s, [&](const TriangleType& t) { out.push_back(t); });
        return out;
    }

    /**
     * @brief Visits the triangles whose interior @p s actually enters.
     *
     * Like @ref visitTrianglesIntersecting but reports only the triangles `t`
     * with `t.interiorsIntersect(s)` — dropping those merely touched along an
     * edge or at a vertex. The traversal still passes through every met triangle
     * for navigation; the filtered ones simply are not reported. @p s may be a
     * segment (reported in order) or a region query shape (unspecified order).
     *
     * @p f follows the visitor convention (return `true` to stop early, `void`
     * to visit all); returns whether the walk stopped early. @p s may use a
     * different point type than the triangulation.
     */
    template <detail::TriangulationQuery OS, class Fn>
    bool visitTrianglesInteriorIntersecting(const OS& s, Fn f) const {
        return visitTrianglesIntersecting(s, [&](const TriangleType& t) -> bool {
            if (t.interiorsIntersect(s)) {
                return detail::invokeVisitor(f, t);
            }
            return false;  // only boundary contact: skip, but keep walking
        });
    }

    /**
     * @brief Returns the triangles whose interior @p s enters.
     *
     * A materialized form of @ref visitTrianglesInteriorIntersecting: the
     * in-domain triangles `t` with `t.interiorsIntersect(s)` — in the order they
     * are encountered along a segment `s`, or an unspecified order for a region
     * query shape.
     *
     * @param s A directed query — segment, oriented segment, line, or oriented
     *          line — (traced in order) or a region query shape (reported in an
     *          unspecified order); may use a different
     *          point type.
     */
    template <detail::TriangulationQuery OS>
    [[nodiscard]] std::vector<TriangleType> trianglesInteriorIntersecting(const OS& s) const {
        std::vector<TriangleType> out;
        visitTrianglesInteriorIntersecting(s, [&](const TriangleType& t) { out.push_back(t); });
        return out;
    }

    /**
     * @brief Visits every triangulation edge met by @p s.
     *
     * Reports each in-domain edge `e` with `s.intersects(e)` exactly once, with
     * its stored label (for a segment `s`, roughly in the order it meets them,
     * ties broken arbitrarily; for a region query shape, in an unspecified
     * order). Built on @ref visitTrianglesIntersecting: as each met triangle is
     * visited, its edges that @p s meets are reported (deduplicated across the
     * two triangles sharing an edge). Edges of ghost or out-of-domain triangles
     * are never reported, matching @ref edges.
     *
     * @p f follows the visitor convention (return `true` to stop early, `void`
     * to visit all); returns whether the walk stopped early. @p s may use a
     * different point type than the triangulation.
     */
    template <detail::TriangulationQuery OS, class Fn>
    bool visitEdgesIntersecting(const OS& s, Fn f) const {
        std::unordered_set<SegmentType> seen;
        bool stop = false;
        visitTrianglesIntersecting(s, [&](const TriangleType& t) -> bool {
            for (const auto& edge : t.edges()) {
                if (!s.intersects(edge)) {
                    continue;
                }
                // Canonical edge (the constructor re-sorts the endpoints), used
                // for reporting and for deduplicating across the two incident
                // triangles. Only when edges carry a label do we look it up in
                // segToEdge_ to recover the stored label; otherwise the segment
                // built from the endpoints already is the value to report.
                SegmentType seg(edge[0], edge[1]);
                if constexpr (detail::has_label_v<SegmentLabel>) {
                    auto se = segToEdge_.find(seg);
                    if (se == segToEdge_.end()) {
                        continue;  // not a stored edge (should not happen)
                    }
                    seg = edgeSegment(se->second);
                }
                if (seen.insert(seg).second && detail::invokeVisitor(f, seg)) {
                    stop = true;
                    return true;
                }
            }
            return false;
        });
        return stop;
    }

    /**
     * @brief Visits the triangulation edges whose interior @p s crosses.
     *
     * Like @ref visitEdgesIntersecting but reports only the edges `e` with
     * `s.interiorsIntersect(e)` — dropping those merely touched at a shared
     * endpoint (e.g. where a segment @p s passes through a mesh vertex, or an
     * endpoint of @p s lands on an edge). @p s may be a segment or a region
     * query shape.
     *
     * @p f follows the visitor convention (return `true` to stop early, `void`
     * to visit all); returns whether the walk stopped early. @p s may use a
     * different point type than the triangulation.
     */
    template <detail::TriangulationQuery OS, class Fn>
    bool visitEdgesInteriorIntersecting(const OS& s, Fn f) const {
        return visitEdgesIntersecting(s, [&](const SegmentType& e) -> bool {
            if (s.interiorsIntersect(e)) {
                return detail::invokeVisitor(f, e);
            }
            return false;  // endpoint-only contact: skip, but keep walking
        });
    }

    /**
     * @brief Returns the triangulation edges met by @p s.
     *
     * A materialized form of @ref visitEdgesIntersecting: the in-domain edges
     * `e` with `s.intersects(e)`, each with its stored label — in the order they
     * are encountered along a segment `s`, or an unspecified order for a region
     * query shape.
     *
     * @param s A directed query — segment, oriented segment, line, or oriented
     *          line — (traced in order) or a region query shape (reported in an
     *          unspecified order); may use a different
     *          point type.
     */
    template <detail::TriangulationQuery OS>
    [[nodiscard]] std::vector<SegmentType> edgesIntersecting(const OS& s) const {
        std::vector<SegmentType> out;
        visitEdgesIntersecting(s, [&](const SegmentType& e) { out.push_back(e); });
        return out;
    }

    /**
     * @brief Returns the triangulation edges whose interior @p s crosses.
     *
     * A materialized form of @ref visitEdgesInteriorIntersecting: the in-domain
     * edges `e` with `s.interiorsIntersect(e)`, each with its stored label — in
     * the order they are encountered along a segment `s`, or an unspecified order
     * for a region query shape.
     *
     * @param s A directed query — segment, oriented segment, line, or oriented
     *          line — (traced in order) or a region query shape (reported in an
     *          unspecified order); may use a different
     *          point type.
     */
    template <detail::TriangulationQuery OS>
    [[nodiscard]] std::vector<SegmentType> edgesInteriorIntersecting(const OS& s) const {
        std::vector<SegmentType> out;
        visitEdgesInteriorIntersecting(s, [&](const SegmentType& e) { out.push_back(e); });
        return out;
    }

    // ---- traversal over a region (a connected convex query shape) --------

    /**
     * @brief Visits every in-domain triangle that intersects @p shape.
     *
     * Generalizes @ref visitTrianglesIntersecting(const OS&, Fn) const from a
     * segment to a connected convex query shape — a point, triangle, rectangle,
     * convex polygon, or an unbounded line, oriented line, ray, or half-plane.
     * The set of triangles `t` passed to @p f is exactly
     * `{ t : t.intersects(shape) }` over the in-domain triangles, in an
     * unspecified order.
     *
     * The work is local. A seed triangle meeting @p shape is found by
     * navigation — tracing @p shape's boundary edges with the segment walk (for
     * a bounded shape), or scanning the convex-hull boundary for an edge it
     * crosses (for an unbounded one) — and the reported set is then grown by a
     * flood fill through edge/vertex adjacency that never strays past the
     * triangles meeting @p shape and their immediate neighbours. The cost is
     * therefore proportional to the number of triangles met, not to the size of
     * the triangulation; the lone exception is the unbounded-shape seed search,
     * which scans the hull and is O(hull).
     *
     * @p f follows the visitor convention: returning `true` stops the walk
     * early, a `void` return visits every such triangle. Returns whether it
     * stopped early. @p shape may use a different point type.
     */
    template <detail::TriangulationRegionQuery Q, class Fn>
    bool visitTrianglesIntersecting(const Q& shape, Fn f) const {
        if (firstGhost_ == 0) return false;  // no real triangles
        const std::vector<TriId> seeds = seedTrianglesIntersecting(shape);
        if (seeds.empty()) return false;
        return floodTrianglesIntersecting(shape, seeds, f);
    }

    // The materialized form — trianglesIntersecting(shape) — and the
    // interior-intersecting and edge variants are the shared wrappers above,
    // constrained on detail::TriangulationQuery, so they accept a region shape
    // here just as they accept a segment.

    // ---- point location --------------------------------------------------

    /**
     * @brief Finds the triangle containing the query point by walking the mesh.
     *
     * Uses a stochastic (randomized) visibility walk, which terminates with
     * probability one on any valid triangulation, not just Delaunay ones; a
     * generous step cap remains only as a defensive bound.
     *
     * @param p Query point; may use a different point type than the triangulation.
     * @return The containing triangle, or `std::nullopt` if @p p lies outside
     *         the triangulated region (or the triangulation is empty).
     */
    template <PointConcept QueryPoint>
    [[nodiscard]] std::optional<TriangleType> locate(const QueryPoint& p) const {
        const TriId id = locateId(p);
        if (!inDomain(id)) {
            return std::nullopt;  // outside the region, or in a hull-fill triangle
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

    // ---- labels ----------------------------------------------------------

    /**
     * @brief Returns a reference to the label stored for triangle @p t.
     *
     * The reference is into the triangulation's own storage, so assigning
     * through it (`tri.label(t) = ...`) changes the label every later accessor
     * reports for that triangle. Only available when the triangle type carries
     * a label.
     *
     * @param t A triangle of this triangulation.
     * @return Reference to its stored label.
     * @pre `contains(t)` — @p t is one of the triangulation's triangles.
     */
    template <class L = TriangleLabel>
        requires(detail::has_label_v<L>)
    [[nodiscard]] L& label(const TriangleType& t) {
        const TriId id = idOf(t);
        assert(inDomain(id) && "label(): triangle is not part of the triangulation");
        return triangles_[id].triLabel;
    }

    /** @overload @brief Read-only access to triangle @p t's stored label. */
    template <class L = TriangleLabel>
        requires(detail::has_label_v<L>)
    [[nodiscard]] const L& label(const TriangleType& t) const {
        const TriId id = idOf(t);
        assert(inDomain(id) && "label(): triangle is not part of the triangulation");
        return triangles_[id].triLabel;
    }

    /**
     * @brief Returns a reference to the label stored for edge @p s.
     *
     * The reference is into the triangulation's own storage, so assigning
     * through it (`tri.label(s) = ...`) changes the label every later accessor
     * reports for that edge. Only available when the segment type carries a
     * label.
     *
     * @param s An edge of this triangulation.
     * @return Reference to its stored label.
     * @pre `contains(s)` — @p s is one of the triangulation's edges.
     */
    template <class L = SegmentLabel>
        requires(detail::has_label_v<L>)
    [[nodiscard]] L& label(const SegmentType& s) {
        auto se = segToEdge_.find(s);
        assert(se != segToEdge_.end() && "label(): segment is not an edge of the triangulation");
        return se->second.segLabel;
    }

    /** @overload @brief Read-only access to edge @p s's stored label. */
    template <class L = SegmentLabel>
        requires(detail::has_label_v<L>)
    [[nodiscard]] const L& label(const SegmentType& s) const {
        auto se = segToEdge_.find(s);
        assert(se != segToEdge_.end() && "label(): segment is not an edge of the triangulation");
        return se->second.segLabel;
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

    /**
     * @brief True if every edge in @p edges can be flipped simultaneously.
     *
     * A parallel flip is valid only when each edge is individually @ref flippable
     * (unconstrained, interior, convex quad) and the quadrilaterals are pairwise
     * disjoint — i.e. no triangle is shared by two of the edges' quads. A shared
     * triangle would be rewritten by the first flip, invalidating the other, so
     * such a set is rejected (a repeated edge fails for the same reason). An empty
     * range is trivially flippable.
     *
     * @param edges A range of edge segments (`edges.value_type` is a segment).
     */
    template <class EdgeRange>
        requires SegmentConcept<typename EdgeRange::value_type>
    [[nodiscard]] bool flippable(const EdgeRange& edges) const {
        std::unordered_set<TriId> claimed;  // triangles already covered by some quad
        for (const auto& s : edges) {
            const auto se = segToEdge_.find(SegmentType(s[0], s[1]));
            if (se == segToEdge_.end() || !flippableEdge(se->second)) {
                return false;
            }
            const Edge e = se->second;
            if (!claimed.insert(e.tri).second || !claimed.insert(mirror(e).tri).second) {
                return false;  // a quad shares a triangle with an earlier one
            }
        }
        return true;
    }

    /**
     * @brief Flips every edge in @p edges at once, if the whole set allows it.
     *
     * All-or-nothing: if @ref flippable(edges) holds, each edge is replaced by the
     * opposite diagonal and the new diagonals are returned in the order of
     * @p edges; otherwise the triangulation is left unchanged and `std::nullopt`
     * is returned. Because the quadrilaterals are disjoint, the individual flips
     * are independent — one never disturbs another's quad — so order does not
     * matter.
     *
     * @param edges A range of edge segments (`edges.value_type` is a segment).
     * @return The new diagonal segments, or `std::nullopt` if @p edges is not
     *         @ref flippable as a set.
     */
    template <class EdgeRange>
        requires SegmentConcept<typename EdgeRange::value_type>
    std::optional<std::vector<SegmentType>> flip(const EdgeRange& edges) {
        if (!flippable(edges)) {
            return std::nullopt;
        }
        std::vector<SegmentType> diagonals;
        for (const auto& s : edges) {
            diagonals.push_back(*flip(SegmentType(s[0], s[1])));  // disjoint: each succeeds
        }
        return diagonals;
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

    /**
     * @brief Draws every triangle to a canvas.
     * @param canvas Destination canvas.
     * @param triangulation Triangulation whose triangles are drawn.
     * @return The canvas.
     */
    friend Canvas& operator<<(Canvas& canvas, const Triangulation& triangulation) {
        for (const auto &t : triangulation.triangles()) {
            canvas << t;
        }
        return canvas;
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
    // across the edge opposite v[i]), a constrained-edge mask, an out-of-domain
    // flag (set for hull-fill triangles outside a polygon), a transient
    // walk-visited mark (used by visitTrianglesIntersecting in lieu of a hash
    // set; always cleared again before that const method returns), and the
    // label. `walkMark` is mutable because the walk is a const query.
    struct Tri {
        std::array<VertexId, 3> v{NO_TRI, NO_TRI, NO_TRI};
        std::array<TriId, 3> nbr{NO_TRI, NO_TRI, NO_TRI};
        std::uint8_t constrainedMask = 0;
        std::uint8_t outOfDomain = 0;
        mutable std::uint8_t walkMark = 0;
        [[no_unique_address]] TriangleLabel triLabel{};
    };
    static_assert(sizeof(Tri) == 28 || detail::has_label_v<TriangleLabel>,
                  "Tri should stay 28 bytes when unlabeled");

    std::vector<PointType> vertices_;  // real vertices, then one ghost vertex
    std::vector<Tri> triangles_;       // real triangles [0,firstGhost_), then ghost triangles
    std::unordered_map<SegmentType, Edge> segToEdge_;  // outside edge -> internal handle
    VertexId ghost_ = NO_TRI;          // index of the ghost vertex
    TriId firstGhost_ = 0;             // first ghost triangle index
    std::size_t domainTriangleCount_ = 0;  // in-domain real triangles (<= firstGhost_)
    mutable TriId hint_ = NO_TRI;      // last located triangle (walk seed)
    mutable std::mt19937 rng_;         // drives the stochastic walk in locateId

    // ---- small helpers ---------------------------------------------------

    // Reads bit `i` of a 3-bit edge mask (e.g. constrainedMask).
    static constexpr bool bit(std::uint8_t mask, int i) { return (mask >> i) & 1; }
    // Sets or clears bit `i` of a 3-bit edge mask.
    static constexpr void setBit(std::uint8_t& mask, int i, bool value) {
        mask = static_cast<std::uint8_t>(value ? (mask | (1u << i)) : (mask & ~(1u << i)));
    }
    // Packs three per-edge flags into a 3-bit mask (bit i is edge i).
    static constexpr std::uint8_t mask(bool b0, bool b1, bool b2) {
        return static_cast<std::uint8_t>((b0 ? 1 : 0) | (b1 ? 2 : 0) | (b2 ? 4 : 0));
    }

    // True if t is one of the boundary-closing ghost triangles (stored last).
    [[nodiscard]] bool isGhost(TriId t) const { return t >= firstGhost_; }

    // A real triangle that is part of the visible triangulation: not a ghost and
    // not a hull-fill triangle outside a polygon's boundary. The public view
    // (sizes, navigation, locate) speaks only of in-domain triangles, while the
    // internal walks still traverse every real triangle, including fill ones.
    [[nodiscard]] bool inDomain(TriId t) const {
        return t != NO_TRI && t < firstGhost_ && !triangles_[t].outOfDomain;
    }

    // Side of triangle x whose neighbor is `target` (x shares <=1 edge with it).
    [[nodiscard]] std::int8_t findSide(TriId x, TriId target) const {
        const auto& n = triangles_[x].nbr;
        return static_cast<std::int8_t>(n[0] == target ? 0 : (n[1] == target ? 1 : 2));
    }

    // An edge belongs to the visible triangulation if at least one of its two
    // incident triangles is in-domain.
    [[nodiscard]] bool edgeInDomain(Edge e) const {
        return inDomain(e.tri) || inDomain(mirror(e).tri);
    }

    // The same edge seen from the triangle on its other side (an invalid Edge if
    // there is none). mirror(e).tri is the neighbor across e.
    [[nodiscard]] Edge mirror(Edge e) const {
        const TriId t2 = triangles_[e.tri].nbr[e.side];
        if (t2 == NO_TRI) {
            return Edge{NO_TRI, 0};
        }
        return Edge{t2, findSide(t2, e.tri)};
    }

    // Materializes triangle t as a public Triangle value (with its label, if any).
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

    // True if p lies in the closed triangle t (interior or boundary). Assumes t
    // is CCW; @p p may use a different point type.
    template <class QueryPoint>
    [[nodiscard]] bool pointInClosure(const QueryPoint& p, TriId t) const {
        const auto& v = triangles_[t].v;
        return orientationSign(vertices_[v[0]], vertices_[v[1]], p) >= 0 &&
               orientationSign(vertices_[v[1]], vertices_[v[2]], p) >= 0 &&
               orientationSign(vertices_[v[2]], vertices_[v[0]], p) >= 0;
    }

    // Position (0,1,2) of vertex w within triangle t.
    [[nodiscard]] int localIndex(TriId t, VertexId w) const {
        const auto& v = triangles_[t].v;
        return v[0] == w ? 0 : (v[1] == w ? 1 : 2);
    }

    // The neighbour of `cur` reached by rotating around vertex `w`, having
    // arrived from `from` (NO_TRI to start): leave through the w-incident edge we
    // did not enter. Orientation-independent (ghosts are not CCW-normalized), so
    // it steps through the ghost triangles of w's fan as well. Mirrors the lambda
    // in the segment walk; shared by the region flood fill.
    [[nodiscard]] TriId rotateAroundVertex(TriId cur, VertexId w, TriId from) const {
        const int lw = localIndex(cur, w);
        int s1 = -1, s2 = -1;
        for (int s = 0; s < 3; ++s) {
            if (s != lw) {
                if (s1 < 0) s1 = s; else s2 = s;
            }
        }
        return triangles_[cur].nbr[triangles_[cur].nbr[s1] == from ? s2 : s1];
    }

    // Calls fn(TriId) on every triangle of vertex w's fan, in rotational order
    // starting from `start` (which must have w as a vertex). The rotation steps
    // through ghost and fill triangles too — that is what closes the ring — so
    // fn also sees those and must filter with inDomain if it wants only visible
    // triangles.
    template <class Fn>
    void visitVertexFan(TriId start, VertexId w, Fn fn) const {
        TriId cur = start, from = NO_TRI;
        std::size_t g = 0, lim = triangles_.size() + 1;
        do {
            fn(cur);
            const TriId next = rotateAroundVertex(cur, w, from);
            from = cur;
            cur = next;
        } while (cur != start && cur != NO_TRI && ++g < lim);
    }

    // The first in-domain real triangle, or NO_TRI if the domain is empty.
    [[nodiscard]] TriId firstInDomainTriangle() const {
        for (TriId t = 0; t < firstGhost_; ++t) {
            if (!triangles_[t].outOfDomain) return t;
        }
        return NO_TRI;
    }

    // Finds one or more triangles meeting `shape` to seed the region flood fill,
    // by navigation rather than a full scan. Returns an empty vector when `shape`
    // does not meet the triangulated region at all. (Helper for the
    // detail::TriangulationRegionQuery overload of visitTrianglesIntersecting.)
    template <class Q>
    [[nodiscard]] std::vector<TriId> seedTrianglesIntersecting(const Q& shape) const {
        std::vector<TriId> seeds;
        if constexpr (PointConcept<Q>) {
            // A point: the triangle locate lands in already meets it (its closure
            // contains the point). Outside the hull, locate returns a ghost.
            const TriId t = locateId(shape);
            if (t != NO_TRI && !isGhost(t)) {
                seeds.push_back(t);
            }
        } else if constexpr (HalfplaneConcept<Q>) {
            // Unbounded shape: it meets the triangulated region iff it meets some
            // convex-hull edge. Scan the ghost ring for the first such edge; the
            // real triangle just inside it is a seed. (O(hull).) Segments, lines,
            // oriented lines, and rays are not region queries — they have their
            // own ordered walk.
            for (TriId g = firstGhost_; g < static_cast<TriId>(triangles_.size()); ++g) {
                if (edgeSegment(Edge{g, 2}).intersects(shape)) {
                    seeds.push_back(triangles_[g].nbr[2]);  // the real triangle inside
                    break;
                }
            }
        } else if constexpr (DiskConcept<Q>) {
            // A disk is bounded but has no straight edges to trace. One of its
            // defining boundary points, shape[0], lies in the (closed) disk, so
            // the triangle locate lands in is a seed when that point is inside
            // the hull. Otherwise the disk reaches in from outside the hull, so —
            // as for an unbounded shape — a hull edge it crosses gives the seed.
            const TriId t = locateId(shape[0]);
            if (t != NO_TRI && !isGhost(t)) {
                seeds.push_back(t);
            } else {
                for (TriId g = firstGhost_; g < static_cast<TriId>(triangles_.size()); ++g) {
                    if (edgeSegment(Edge{g, 2}).intersects(shape)) {
                        seeds.push_back(triangles_[g].nbr[2]);
                        break;
                    }
                }
            }
        } else {
            // Bounded shape with straight edges (triangle, rectangle, convex
            // polygon): any triangle its boundary meets is a seed. Trace each edge
            // with the segment walk and take the first triangle it reports.
            for (const auto& e : shape.edges()) {
                TriId found = NO_TRI;
                visitTrianglesIntersecting(e, [&](const TriangleType& t) {
                    found = idOf(t);
                    return true;  // the first triangle suffices
                });
                if (found != NO_TRI) {
                    seeds.push_back(found);
                    break;
                }
            }
            // No edge met the domain: either the whole (connected) domain lies
            // inside `shape` or the two are disjoint — one vertex decides.
            if (seeds.empty() && shape.contains(vertices_.front())) {
                const TriId t = firstInDomainTriangle();
                if (t != NO_TRI) {
                    seeds.push_back(t);
                }
            }
        }
        return seeds;
    }

    // Grows the set of triangles meeting `shape` outward from `seeds` by a flood
    // fill over edge/vertex adjacency, emitting the in-domain ones to `f`. Every
    // real (non-ghost) triangle meeting `shape` and reachable from a seed through
    // other meeting triangles is visited; ghosts are walls. A per-triangle mark
    // bit stands in for a visited set and is cleared on every exit path (normal,
    // early-stop, or an exception from f), so the const query leaves the mesh
    // pristine. Like the segment walk, it is not reentrant: f must not start
    // another walk on the same Triangulation. Returns whether f stopped early.
    template <class Q, class Fn>
    bool floodTrianglesIntersecting(const Q& shape, const std::vector<TriId>& seeds, Fn f) const {
        std::vector<TriId> marked;
        struct MarkClearer {
            const std::vector<Tri>& tris;
            const std::vector<TriId>& marked;
            ~MarkClearer() { for (TriId t : marked) tris[t].walkMark = 0; }
        } markClearer{triangles_, marked};

        std::vector<TriId> stack;
        // Marks t seen; if it is a real triangle meeting `shape`, queues it for
        // expansion. Rejected triangles are marked too, so each is tested once.
        const auto consider = [&](TriId t) {
            if (t == NO_TRI || isGhost(t) || triangles_[t].walkMark) {
                return;
            }
            triangles_[t].walkMark = 1;
            marked.push_back(t);
            if (triangleValue(t).intersects(shape)) {
                stack.push_back(t);
            }
        };
        for (const TriId s : seeds) {
            consider(s);
        }

        bool stop = false;
        while (!stop && !stack.empty()) {
            const TriId t = stack.back();
            stack.pop_back();
            if (inDomain(t) && detail::invokeVisitor(f, triangleValue(t))) {
                stop = true;
                break;
            }
            // Expand through every triangle sharing a vertex with t (which covers
            // its edge neighbours too): the meeting set is connected under this
            // adjacency, so this reaches all of it without scanning the mesh.
            for (const VertexId w : triangles_[t].v) {
                TriId cur = t, from = NO_TRI;
                std::size_t g = 0, lim = triangles_.size() + 1;
                do {
                    consider(cur);
                    const TriId next = rotateAroundVertex(cur, w, from);
                    from = cur;
                    cur = next;
                } while (cur != t && cur != NO_TRI && ++g < lim);
            }
        }
        return stop;
    }

    // Inserts t's three edges into the segment-to-edge map, keyed by Segment.
    void registerSides(TriId t) {
        for (std::int8_t s = 0; s < 3; ++s) {
            segToEdge_[edgeSegment(Edge{t, s})] = Edge{t, s};
        }
    }

    // Builds the segment-to-edge map over all real triangles.
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

    // Exact Delaunay triangle triples (CCW, vertex indices into `pts`) via
    // incremental Bowyer–Watson insertion. A single symbolic "vertex at infinity"
    // (INF) closes the convex hull instead of a containing super-triangle: every
    // hull edge a->b carries a ghost triangle {b,a,INF} that tiles the exterior,
    // so the working triangulation is a closed surface in which every triangle has
    // three neighbours. No far-away coordinates are ever constructed, so every
    // in-circle / orientation test runs on the real point coordinates at the
    // library's normal exactness — no BigInt, and no magnitude assumption (which
    // is why a super-triangle is unsound for rational points: its required scale
    // is unbounded).
    //
    // Each point is located by a visibility walk over the neighbour links (the
    // same walk locateId() runs on the finished structure) and inserted by
    // carving out the triangles whose open circumdisk contains it — found by a
    // local flood-fill from the located triangle, not a global scan — then
    // re-fanning the star-shaped cavity to the new vertex. With the random
    // insertion order the walk is short (seeded from the previously inserted
    // triangle), so the build is ~O(n^1.5) here rather than the O(n^2) of testing
    // every triangle against every point.
    static std::vector<std::array<VertexId, 3>>
    delaunayTriples(const std::vector<PointType>& pts) {
        const VertexId n = static_cast<VertexId>(pts.size());
        std::vector<std::array<VertexId, 3>> out;
        if (n < 3) {
            return out;
        }
        const VertexId INF = n;  // the symbolic vertex at infinity

        // Local closed triangulation: CCW vertices (ghosts contain INF) and three
        // neighbours each (nbr[i] is across the edge opposite v[i]). Killed
        // triangles keep `dead` set and recycle their slots through freeList; no
        // live triangle ever references a dead slot, so `dead` doubles as the
        // per-insertion "already in the cavity" mark during the flood-fill.
        struct LTri {
            std::array<VertexId, 3> v{};
            std::array<int, 3> nbr{-1, -1, -1};
            bool dead = false;
        };
        std::vector<LTri> tri;
        std::vector<int> freeList;

        auto newTri = [&](VertexId a, VertexId b, VertexId d) -> int {
            int id;
            if (!freeList.empty()) {
                id = freeList.back();
                freeList.pop_back();
                tri[id] = LTri{{a, b, d}, {-1, -1, -1}, false};
            } else {
                id = static_cast<int>(tri.size());
                tri.push_back(LTri{{a, b, d}, {-1, -1, -1}, false});
            }
            return id;
        };

        auto isGhost = [&](int t) {
            const auto& q = tri[t].v;
            return q[0] == INF || q[1] == INF || q[2] == INF;
        };

        // Inside-circumdisk test, finite or ghost. The circle through two finite
        // points and INF degenerates to the line through them, so for a ghost
        // {.,.,INF} "inside the open disk" reduces to "left of the finite directed
        // edge" (the edge opposite INF, in CCW order so its left is the hull
        // exterior).
        //
        // The collinear boundary case (orientation 0) is decided so the re-fan
        // never builds a zero-area triangle: p exactly on the open hull-edge
        // segment counts as inside, pulling the ghost into the cavity so the edge
        // splits in two instead of spanning a degenerate {edge, p}. p on the
        // edge's line but beyond an endpoint stays outside — the hull just extends
        // straight along the line, no degenerate triangle either way.
        auto inDisk = [&](int t, VertexId p) -> bool {
            const auto& q = tri[t].v;
            const int inf = q[0] == INF ? 0 : (q[1] == INF ? 1 : (q[2] == INF ? 2 : -1));
            if (inf < 0) {
                return inCircleSign(pts[q[0]], pts[q[1]], pts[q[2]], pts[p]) ==
                       std::partial_ordering::greater;
            }
            const VertexId u = q[(inf + 1) % 3];
            const VertexId w = q[(inf + 2) % 3];
            const auto side = orientationSign(pts[u], pts[w], pts[p]);
            if (side > 0) {
                return true;
            }
            if (side == 0) {  // collinear: inside iff strictly between u and w
                return (pts[p] - pts[u]) * (pts[w] - pts[u]) > 0 &&
                       (pts[p] - pts[w]) * (pts[u] - pts[w]) > 0;
            }
            return false;
        };

        // Seed with the first non-collinear triple, oriented CCW, plus the three
        // ghost triangles covering its hull edges. Points 2..c-1 (if any) are
        // collinear with 0 and 1 and get inserted in the main loop like any other.
        VertexId c = 2;
        while (c < n && orientationSign(pts[0], pts[1], pts[c]) == 0) {
            ++c;
        }
        if (c == n) {
            return out;  // all points collinear: the Delaunay triangulation is empty
        }
        const std::array<VertexId, 3> seed =
            orientationSign(pts[0], pts[1], pts[c]) > 0 ? std::array<VertexId, 3>{0, 1, c}
                                                        : std::array<VertexId, 3>{1, 0, c};
        const int seedTris[4] = {
            newTri(seed[0], seed[1], seed[2]),
            newTri(seed[1], seed[0], INF),  // ghost outside edge seed0->seed1
            newTri(seed[2], seed[1], INF),  // ghost outside edge seed1->seed2
            newTri(seed[0], seed[2], INF),  // ghost outside edge seed2->seed0
        };
        // Link the seed: match each directed edge (a,b) to the neighbour carrying
        // its reverse (b,a). The four triangles tile the sphere, so every side
        // finds a partner.
        for (int t : seedTris) {
            for (int s = 0; s < 3; ++s) {
                const VertexId a = tri[t].v[(s + 1) % 3];
                const VertexId b = tri[t].v[(s + 2) % 3];
                for (int u : seedTris) {
                    for (int q = 0; q < 3; ++q) {
                        if (tri[u].v[(q + 1) % 3] == b && tri[u].v[(q + 2) % 3] == a) {
                            tri[t].nbr[s] = u;
                        }
                    }
                }
            }
        }

        // Visibility walk: from triangle t, step across whichever edge p lies
        // strictly to the right of (outside), until p is inside the current
        // triangle (no such edge) or a ghost is reached (p outside the hull). A
        // randomised start edge guarantees termination; the step cap is a safety
        // net only.
        std::uint64_t rngState = 0x9e3779b97f4a7c15ULL;
        auto walk = [&](VertexId p, int t) -> int {
            int from = -1;
            const int64_t cap = int64_t(3) * static_cast<int64_t>(tri.size()) + 16;
            for (int64_t step = 0; step < cap; ++step) {
                if (isGhost(t)) {
                    return t;
                }
                rngState = rngState * 6364136223846793005ULL + 1442695040888963407ULL;
                const int begin = static_cast<int>((rngState >> 33) % 3);
                int next = -1;
                for (int k = 0; k < 3; ++k) {
                    const int s = (begin + k) % 3;
                    if (tri[t].nbr[s] == from) {
                        continue;
                    }
                    const VertexId ea = tri[t].v[(s + 1) % 3];
                    const VertexId eb = tri[t].v[(s + 2) % 3];
                    if (orientationSign(pts[ea], pts[eb], pts[p]) < 0) {
                        next = tri[t].nbr[s];
                        break;
                    }
                }
                if (next < 0) {
                    return t;
                }
                from = t;
                t = next;
            }
            return t;
        };

        std::vector<int> cavity;
        // A boundary edge of the cavity: its directed edge (a,b), the surviving
        // triangle behind it, and that triangle's side facing the cavity.
        struct Bnd {
            VertexId a, b;
            int surv, survSide;
        };
        std::vector<Bnd> boundary;
        // Spoke edges {vertex,p} awaiting their partner among the new triangles.
        struct Spoke {
            VertexId vertex;
            int tri, side;
        };
        std::vector<Spoke> spokes;

        int hint = seedTris[0];
        for (VertexId i = 0; i < n; ++i) {
            if (i == seed[0] || i == seed[1] || i == seed[2]) {
                continue;
            }
            const int start = walk(i, hint);
            if (!inDisk(start, i)) {
                continue;  // i lies in no open circumdisk (collinear/duplicate): skip
            }

            // Flood-fill the cavity: every triangle whose open disk contains i,
            // reachable from `start` through neighbours. Mark them dead as we go
            // (so they double as the visited set); record each edge where the
            // flood meets a surviving triangle.
            cavity.clear();
            boundary.clear();
            tri[start].dead = true;
            cavity.push_back(start);
            for (std::size_t qi = 0; qi < cavity.size(); ++qi) {
                const int t = cavity[qi];
                for (int s = 0; s < 3; ++s) {
                    const int nb = tri[t].nbr[s];
                    if (tri[nb].dead) {
                        continue;  // already carved into the cavity this insertion
                    }
                    if (inDisk(nb, i)) {
                        tri[nb].dead = true;
                        cavity.push_back(nb);
                    } else {
                        const int survSide =
                            tri[nb].nbr[0] == t ? 0 : (tri[nb].nbr[1] == t ? 1 : 2);
                        boundary.push_back(
                            {tri[t].v[(s + 1) % 3], tri[t].v[(s + 2) % 3], nb, survSide});
                    }
                }
            }
            for (int t : cavity) {
                freeList.push_back(t);
            }

            // Re-fan: one new triangle {a,b,i} per boundary edge, linked across its
            // base {a,b} to the surviving triangle and across its two spokes
            // {b,i}/{a,i} to the adjacent new triangles, paired by shared vertex.
            spokes.clear();
            int newReal = -1;
            for (const Bnd& e : boundary) {
                const int nt = newTri(e.a, e.b, i);
                tri[nt].nbr[2] = e.surv;  // side 2 (opposite i) is the base edge {a,b}
                tri[e.surv].nbr[e.survSide] = nt;
                if (newReal < 0 && !isGhost(nt)) {
                    newReal = nt;
                }
                // side 0 (opposite a) is edge {b,i}; side 1 (opposite b) is {a,i}.
                for (const auto& [vertex, side] :
                     {std::pair<VertexId, int>{e.b, 0}, std::pair<VertexId, int>{e.a, 1}}) {
                    bool paired = false;
                    for (std::size_t k = 0; k < spokes.size(); ++k) {
                        if (spokes[k].vertex == vertex) {
                            tri[nt].nbr[side] = spokes[k].tri;
                            tri[spokes[k].tri].nbr[spokes[k].side] = nt;
                            spokes[k] = spokes.back();
                            spokes.pop_back();
                            paired = true;
                            break;
                        }
                    }
                    if (!paired) {
                        spokes.push_back({vertex, nt, side});
                    }
                }
            }
            if (newReal >= 0) {
                hint = newReal;
            }
        }

        for (int t = 0; t < static_cast<int>(tri.size()); ++t) {
            if (tri[t].dead) {
                continue;
            }
            const auto& q = tri[t].v;
            if (q[0] != INF && q[1] != INF && q[2] != INF) {
                out.push_back({q[0], q[1], q[2]});
            }
        }
        return out;
    }

    // ---- constrained Delaunay (polygon constructor) ----------------------

    // True if the open segments AB and CD cross properly (no shared endpoint,
    // no collinear contact). Exact via orientationSign.
    [[nodiscard]] bool properCross(const PointType& a, const PointType& b,
                                   const PointType& c, const PointType& d) const {
        const auto s1 = orientationSign(a, b, c);
        const auto s2 = orientationSign(a, b, d);
        const auto s3 = orientationSign(c, d, a);
        const auto s4 = orientationSign(c, d, b);
        if (s1 == 0 || s2 == 0 || s3 == 0 || s4 == 0) {
            return false;
        }
        return ((s1 > 0) != (s2 > 0)) && ((s3 > 0) != (s4 > 0));
    }

    // The current internal handle of edge {p,q}, or an invalid edge if absent.
    [[nodiscard]] Edge edgeHandle(VertexId p, VertexId q) const {
        auto se = segToEdge_.find(SegmentType(vertices_[p], vertices_[q]));
        return se == segToEdge_.end() ? Edge{NO_TRI, 0} : se->second;
    }

    [[nodiscard]] bool edgeExists(VertexId p, VertexId q) const {
        return segToEdge_.contains(SegmentType(vertices_[p], vertices_[q]));
    }

    // The interior edges that the open segment va->vb crosses, as vertex pairs,
    // in order from va to vb. Empty when {va,vb} is already an edge. Assumes no
    // vertex lies in the interior of the segment (true for simple-polygon edges).
    [[nodiscard]] std::vector<std::pair<VertexId, VertexId>>
    collectCrossings(VertexId va, VertexId vb) const {
        const PointType& A = vertices_[va];
        const PointType& B = vertices_[vb];
        std::vector<std::pair<VertexId, VertexId>> out;

        // Find the triangle incident to va that the segment first enters.
        TriId t = NO_TRI;
        std::pair<VertexId, VertexId> entry{NO_TRI, NO_TRI};
        for (TriId k = 0; k < firstGhost_ && t == NO_TRI; ++k) {
            const auto& v = triangles_[k].v;
            for (int i = 0; i < 3; ++i) {
                if (v[i] != va) {
                    continue;
                }
                const VertexId p = v[(i + 1) % 3];
                const VertexId q = v[(i + 2) % 3];
                if (properCross(A, B, vertices_[p], vertices_[q])) {
                    t = triangles_[k].nbr[i];
                    entry = {p, q};
                    out.push_back({p, q});
                }
                break;
            }
        }
        if (t == NO_TRI) {
            return out;  // {va,vb} is already an edge (or va not incident anywhere)
        }

        // Walk across the crossed edges until a triangle holds vb.
        std::size_t guard = 0, cap = triangles_.size() + 1;
        while (++guard < cap) {
            const auto& v = triangles_[t].v;
            if (v[0] == vb || v[1] == vb || v[2] == vb) {
                break;
            }
            int exitK = -1;
            for (int k = 0; k < 3; ++k) {
                const VertexId p = v[(k + 1) % 3];
                const VertexId q = v[(k + 2) % 3];
                const bool sameEntry = (p == entry.first && q == entry.second) ||
                                       (p == entry.second && q == entry.first);
                if (sameEntry) {
                    continue;
                }
                if (properCross(A, B, vertices_[p], vertices_[q])) {
                    exitK = k;
                    entry = {p, q};
                    out.push_back({p, q});
                    break;
                }
            }
            if (exitK < 0) {
                break;  // should not happen for a valid triangulation
            }
            t = triangles_[t].nbr[exitK];
        }
        return out;
    }

    // Inserts the segment {va,vb} as an edge by flipping the edges it crosses
    // (Sloan's flip algorithm), then flags it constrained on both sides.
    //
    // A FIFO queue of crossing edges is processed front-to-back: a convex
    // crossing edge is flipped, and if its new diagonal still crosses the
    // constraint it goes to the back; a non-convex one is deferred to the back
    // until its quad becomes convex. This ordering guarantees progress (a naive
    // "flip the first convex one" can oscillate, repeatedly flipping a diagonal
    // back and forth).
    void insertConstraint(VertexId va, VertexId vb) {
        if (va == vb || edgeExists(va, vb)) {
            setConstrained(SegmentType(vertices_[va], vertices_[vb]), true);
            return;
        }
        const PointType& A = vertices_[va];
        const PointType& B = vertices_[vb];
        std::deque<std::pair<VertexId, VertexId>> queue;
        for (const auto& pq : collectCrossings(va, vb)) {
            queue.push_back(pq);
        }
        std::size_t guard = 0, cap = (queue.size() + 1) * (triangles_.size() + 1) * 4 + 64;
        while (!queue.empty() && ++guard < cap) {
            auto [p, q] = queue.front();
            queue.pop_front();
            const Edge e = edgeHandle(p, q);
            if (e.tri == NO_TRI || !properCross(A, B, vertices_[p], vertices_[q])) {
                continue;  // edge gone, or no longer crosses the constraint
            }
            if (!flippableEdge(e)) {
                queue.push_back({p, q});  // quad not yet convex; revisit later
                continue;
            }
            const VertexId r = triangles_[e.tri].v[e.side];        // apex on one side
            const Edge m = mirror(e);
            const VertexId l = triangles_[m.tri].v[m.side];        // apex on the other
            flip(SegmentType(vertices_[p], vertices_[q]));
            if (properCross(A, B, vertices_[r], vertices_[l])) {
                queue.push_back({r, l});  // new diagonal still crosses; re-queue
            }
        }
        setConstrained(SegmentType(vertices_[va], vertices_[vb]), true);
    }

    // Restores the constrained Delaunay property: flip every non-constrained
    // interior edge whose opposite apex lies inside the incident circumcircle,
    // until none remain. Constrained edges are never flipped.
    void restoreConstrainedDelaunay() {
        bool changed = true;
        std::size_t guard = 0, cap = triangles_.size() * triangles_.size() + 64;
        while (changed && ++guard < cap) {
            changed = false;
            const auto snapshot = segToEdge_;
            for (const auto& [seg, handle] : snapshot) {
                (void)handle;
                auto se = segToEdge_.find(seg);
                if (se == segToEdge_.end() || !flippableEdge(se->second)) {
                    continue;
                }
                const Edge cur = se->second;
                const auto& T = triangles_[cur.tri];
                const VertexId d = triangles_[mirror(cur).tri].v[mirror(cur).side];
                if (inCircleSign(vertices_[T.v[0]], vertices_[T.v[1]], vertices_[T.v[2]],
                                 vertices_[d]) == std::partial_ordering::greater) {
                    flip(seg);
                    changed = true;
                }
            }
        }
    }

    // Flood-fills from the ghost (outside) across non-constrained edges, marking
    // every real triangle reachable without crossing the polygon boundary as
    // out-of-domain (the hull-fill triangles between polygon and convex hull).
    void markOutOfDomain() {
        std::vector<char> seen(triangles_.size(), 0);
        std::vector<TriId> stack;
        for (TriId g = firstGhost_; g < static_cast<TriId>(triangles_.size()); ++g) {
            seen[g] = 1;
            stack.push_back(g);
        }
        std::size_t marked = 0;
        while (!stack.empty()) {
            const TriId t = stack.back();
            stack.pop_back();
            for (int s = 0; s < 3; ++s) {
                if (bit(triangles_[t].constrainedMask, s)) {
                    continue;  // the polygon boundary fences off the interior
                }
                const TriId nb = triangles_[t].nbr[s];
                if (nb == NO_TRI || seen[nb]) {
                    continue;
                }
                seen[nb] = 1;
                if (!isGhost(nb)) {
                    triangles_[nb].outOfDomain = 1;
                    ++marked;
                }
                stack.push_back(nb);
            }
        }
        domainTriangleCount_ = static_cast<std::size_t>(firstGhost_) - marked;
    }

    // Shared implementation of the polygon constructors. Triangulates the union
    // of the polygon vertices, the extra interior points, and the constraint
    // segment endpoints; constrains the polygon boundary and every interior
    // segment; then marks the exterior (between the polygon and its convex hull)
    // out of domain. Interior constraints never reach that exterior flood — the
    // boundary fences it off — so they stay in-domain. @p extraPoints and
    // @p constraintSegments are assumed to lie inside @p poly (not checked).
    template <class PointRange, class SegmentRange>
    void constructConstrained(const Polygon<PointType>& poly, const PointRange& extraPoints,
                              const SegmentRange& constraintSegments) {
        std::unordered_map<PointType, VertexId> vid;
        const auto idOfPoint = makeVertexInterner(vid);
        for (std::size_t i = 0; i < poly.size(); ++i) {
            idOfPoint(poly[i]);
        }
        for (const auto& p : extraPoints) {
            idOfPoint(PointType(p));
        }
        for (const auto& s : constraintSegments) {
            idOfPoint(PointType(s[0]));
            idOfPoint(PointType(s[1]));
        }
        // Keep vertices_ in Hilbert order (see the point-set constructor); the
        // interner's ids are now stale, so rebuild the map and resolve the
        // boundary loop and constraint endpoints against the reordered vertices.
        hilbertSort(vertices_);
        vid.clear();
        for (VertexId i = 0; i < static_cast<VertexId>(vertices_.size()); ++i) {
            vid.emplace(vertices_[i], i);
        }
        std::vector<VertexId> loop;
        loop.reserve(poly.size());
        for (std::size_t i = 0; i < poly.size(); ++i) {
            loop.push_back(vid.at(poly[i]));
        }
        auto triples = delaunayTriples(vertices_);
        buildFromTriples(triples, std::vector<TriangleLabel>(triples.size()));

        // Constrain the polygon boundary and every interior segment, restore the
        // constrained Delaunay property, then carve away the exterior.
        for (std::size_t i = 0; i < loop.size(); ++i) {
            insertConstraint(loop[i], loop[(i + 1) % loop.size()]);
        }
        for (const auto& s : constraintSegments) {
            const VertexId a = vid.at(PointType(s[0]));
            const VertexId b = vid.at(PointType(s[1]));
            if (a != b) {
                insertConstraint(a, b);
            }
        }
        restoreConstrainedDelaunay();
        markOutOfDomain();

        // Carry each constraint segment's label onto its edge record. Constrained
        // edges are never flipped, so they are still in segToEdge_ (keyed by
        // coordinates; the label is ignored for the lookup). Mirrors the
        // segment-range constructor.
        if constexpr (detail::has_label_v<SegmentLabel>) {
            for (const auto& s : constraintSegments) {
                auto it = segToEdge_.find(SegmentType(PointType(s[0]), PointType(s[1])));
                if (it != segToEdge_.end()) {
                    it->second.segLabel = detail::copyLabel<SegmentLabel>(s);
                }
            }
        }
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
            triangles_.push_back(Tri{{x, y, z}, {NO_TRI, NO_TRI, NO_TRI}, 0, 0, 0, triLabels[k]});
        }
        firstGhost_ = static_cast<TriId>(triangles_.size());
        domainTriangleCount_ = static_cast<std::size_t>(firstGhost_);
        buildAdjacency();
        buildMap();
        assert(checkInvariants());
    }

    // ---- internal predicates / mutation ----------------------------------

    // True if edge e can be flipped: unconstrained, interior (both sides real),
    // and the two incident triangles form a strictly convex quadrilateral.
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

    // Replaces edge e by the opposite diagonal of its quadrilateral, rewriting
    // the two incident triangle records and relinking the four outer neighbors.
    // Surrounding constrained flags are carried over; the new diagonal is
    // unconstrained. Does not touch segToEdge_ (callers re-register). Returns
    // false if e is not flippable.
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

    // Locates the triangle containing p by a stochastic visibility walk (random
    // start edge guarantees termination). Returns a ghost triangle if p is
    // outside the triangulated region, NO_TRI only if empty. Seeds from, and
    // updates, hint_; @p p may use a different point type.
    template <class QueryPoint>
    [[nodiscard]] TriId locateId(const QueryPoint& p) const {
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

    // Links neighbor pointers between real triangles sharing an edge, then closes
    // every still-unmatched (boundary) edge with a ghost triangle to the ghost
    // vertex and links those ghosts into a ring, so every edge has two sides.
    void buildAdjacency() {
        // Undirected edge key (endpoints sorted) for matching the two sides.
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

template <class PointRange>
    requires PointConcept<typename PointRange::value_type>
Triangulation(const PointRange&)
    -> Triangulation<Triangle<Point<typename PointRange::value_type::NumberType>>>;

template <class PointType>
Triangulation(const Polygon<PointType>&) -> Triangulation<Triangle<PointType>>;

// Polygon with extra interior points and/or constraint segments: the point and
// triangle types come from the polygon. When constraint segments are present the
// stored edge type takes their label (over the polygon's own point type), so the
// segments' labels survive; otherwise the edge type defaults like the
// polygon-only guide. The `requires` clauses disambiguate the two two-argument
// forms (a points range vs a segment range), mirroring the disjoint guides above.
template <class PolyPoint, class PointRange>
    requires PointConcept<typename PointRange::value_type>
Triangulation(const Polygon<PolyPoint>&, const PointRange&)
    -> Triangulation<Triangle<PolyPoint>>;

template <class PolyPoint, class SegmentRange>
    requires SegmentConcept<typename SegmentRange::value_type>
Triangulation(const Polygon<PolyPoint>&, const SegmentRange&)
    -> Triangulation<Triangle<PolyPoint>,
                     Segment<PolyPoint, typename SegmentRange::value_type::LabelType>>;

template <class PolyPoint, class PointRange, class SegmentRange>
    requires PointConcept<typename PointRange::value_type> &&
             SegmentConcept<typename SegmentRange::value_type>
Triangulation(const Polygon<PolyPoint>&, const PointRange&, const SegmentRange&)
    -> Triangulation<Triangle<PolyPoint>,
                     Segment<PolyPoint, typename SegmentRange::value_type::LabelType>>;

// Out-of-line: Polygon::triangulation is declared in shape/polygon.hpp (which
// precedes this header in the layering) but can only be defined once
// Triangulation and its deduction guides are visible.
template <class PointType_, class TLabel>
auto Polygon<PointType_, TLabel>::triangulation() const {
    return Triangulation(*this);
}

template <class PointType_, class TLabel>
template <class SegmentRange>
auto Polygon<PointType_, TLabel>::triangulation(const SegmentRange& segments) const {
    return Triangulation(*this, segments);
}

}  // namespace pgl
