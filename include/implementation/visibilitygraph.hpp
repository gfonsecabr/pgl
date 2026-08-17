#pragma once

// Included from pgl.hpp after algorithm/triangulation.hpp, whose Triangulation,
// Polygon and PolygonWithHoles members this defines out of line.
#include "algorithm/graph.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <vector>

/**
 * @file visibilitygraph.hpp
 * @brief Visibility graphs by triangular expansion.
 *
 * One traversal of the triangulated domain per vertex, carrying a cone of still
 * unobstructed directions that every crossed diagonal clips. The cone is held as
 * a pair of *vertices* rather than as materialized rays, so the whole algorithm
 * is orientation predicates on stored points: nothing is constructed, and an
 * exact coordinate type stays exact and never grows.
 *
 * The three graphs share that expansion. Clear visibility is what it computes
 * directly. Full visibility is clear visibility plus the mesh's own blocking
 * edges — together the pairs that see each other with no vertex in between —
 * closed along collinear chains, since a segment through a vertex stays in the
 * domain exactly when both halves do. Reduced visibility filters the full graph
 * by tangency at both endpoints.
 */

namespace pgl {

namespace detail {

// Integer form of an orientation sign. `unordered` — reachable only for
// floating-point coordinates carrying a NaN — reads as collinear, as elsewhere
// in the library.
constexpr int signValue(std::partial_ordering order) {
    return order > 0 ? 1 : (order < 0 ? -1 : 0);
}

}  // namespace detail

// -----------------------------------------------------------------------------
// Triangulation
// -----------------------------------------------------------------------------

template <TriangleConcept TriangleType_, SegmentConcept SegmentType_>
std::vector<std::vector<typename Triangulation<TriangleType_, SegmentType_>::VertexId>>
Triangulation<TriangleType_, SegmentType_>::clearVisibleAdjacency() const {
    std::vector<std::vector<VertexId>> adjacency(vertices_.size());
    if (domainTriangleCount_ == 0) {
        return adjacency;
    }
    const VertexId vertexCount = static_cast<VertexId>(vertices_.size());

    // One step of the expansion: cross side `side` of `tri` carrying the open
    // cone whose clockwise bound is the ray source->right and whose
    // counterclockwise bound is source->left. Both bounds are vertices, so every
    // test below is one orientation predicate on stored points.
    struct Frame {
        TriId tri;
        std::int8_t side;
        VertexId right;
        VertexId left;
    };
    std::vector<Frame> stack;

    for (VertexId source = GHOST + 1; source < vertexCount; ++source) {
        const TriId seed = incidentTriangleOf(source);
        if (seed == NO_TRI) {
            continue;
        }
        const PointType& origin = vertices_[static_cast<std::size_t>(source)];
        auto& visible = adjacency[static_cast<std::size_t>(source)];

        // Seed one expansion per in-domain triangle of the source's fan. Such a
        // triangle (source, p, q) is counterclockwise, so p bounds its wedge
        // clockwise and q counterclockwise, and the fan wedges together tile
        // exactly the part of the plane the source can see into.
        visitVertexFan(seed, source, [&](TriId t) {
            if (!inDomain(t)) {
                return;
            }
            const auto& v = triangles_[static_cast<std::size_t>(t)].v;
            const int i = v[0] == source ? 0 : (v[1] == source ? 1 : 2);
            const VertexId clockwise = v[(i + 1) % 3];
            // Edge (source, clockwise) is the side opposite v[(i+2)%3]. Taking
            // only the clockwise neighbour visits every fan edge exactly once,
            // since an edge with no second in-domain triangle blocks anyway.
            if (!blocksVisibility(t, (i + 2) % 3)) {
                visible.push_back(clockwise);
            }
            if (!blocksVisibility(t, i)) {
                stack.push_back({t, static_cast<std::int8_t>(i), clockwise, v[(i + 2) % 3]});
            }
        });

        while (!stack.empty()) {
            const Frame frame = stack.back();
            stack.pop_back();
            const TriId entered = triangles_[static_cast<std::size_t>(frame.tri)].nbr[frame.side];
            const int back = findSide(entered, frame.tri);
            const auto& v = triangles_[static_cast<std::size_t>(entered)].v;
            // `entered` winds the shared edge the other way round, so its
            // clockwise end as seen from the source is v[(back+2)%3] and its
            // counterclockwise end v[(back+1)%3]. The apex is the third vertex.
            const VertexId apex = v[back];
            const int fromRight = detail::signValue(orientationSign(
                origin, vertices_[static_cast<std::size_t>(frame.right)],
                vertices_[static_cast<std::size_t>(apex)]));
            const int fromLeft = detail::signValue(orientationSign(
                origin, vertices_[static_cast<std::size_t>(frame.left)],
                vertices_[static_cast<std::size_t>(apex)]));

            VertexId rightChildLeft = frame.left;
            VertexId leftChildRight = frame.right;
            bool crossRight = true;
            bool crossLeft = true;
            if (fromRight > 0 && fromLeft < 0) {
                // Strictly inside the cone: nothing stands between the source and
                // the apex, which therefore is clearly visible and splits the
                // cone in two. An apex merely *on* a bound is not — the vertex
                // that set that bound is in the way — and the two comparisons
                // being strict is the whole of what separates clear visibility
                // from the grazing kind.
                visible.push_back(apex);
                rightChildLeft = apex;
                leftChildRight = apex;
            } else if (fromRight <= 0) {
                crossRight = false;  // the clockwise sub-cone came out empty
            } else {
                crossLeft = false;   // the counterclockwise sub-cone came out empty
            }

            // Side (back+1)%3 is the clockwise half of the entry edge, side
            // (back+2)%3 the counterclockwise half.
            if (crossRight) {
                const int side = (back + 1) % 3;
                if (!blocksVisibility(entered, side)) {
                    stack.push_back({entered, static_cast<std::int8_t>(side),
                                     frame.right, rightChildLeft});
                }
            }
            if (crossLeft) {
                const int side = (back + 2) % 3;
                if (!blocksVisibility(entered, side)) {
                    stack.push_back({entered, static_cast<std::int8_t>(side),
                                     leftChildRight, frame.left});
                }
            }
        }
    }
    return adjacency;
}

template <TriangleConcept TriangleType_, SegmentConcept SegmentType_>
bool Triangulation<TriangleType_, SegmentType_>::passesThrough(VertexId m) const {
    const TriId seed = incidentTriangleOf(m);
    if (seed == NO_TRI) {
        return false;
    }
    // Extremes of the domain wedges meeting at m: the ends of a wedge are the
    // fan edges whose other triangle is out of domain.
    VertexId clockwise = GHOST;
    VertexId counter = GHOST;
    int extremes = 0;
    bool any = false;
    visitVertexFan(seed, m, [&](TriId t) {
        if (!inDomain(t)) {
            return;
        }
        any = true;
        const auto& tri = triangles_[static_cast<std::size_t>(t)];
        const int i = tri.v[0] == m ? 0 : (tri.v[1] == m ? 1 : 2);
        if (!inDomain(tri.nbr[(i + 2) % 3])) {
            clockwise = tri.v[(i + 1) % 3];
            ++extremes;
        }
        if (!inDomain(tri.nbr[(i + 1) % 3])) {
            counter = tri.v[(i + 2) % 3];
            ++extremes;
        }
    });
    if (!any) {
        return false;
    }
    if (extremes == 0) {
        return true;  // the domain surrounds m
    }
    if (extremes != 2 || clockwise == GHOST || counter == GHOST) {
        return true;  // several wedges meet at m; assume a line fits through one
    }
    // A single wedge, running counterclockwise from `clockwise` to `counter`,
    // holds a pair of opposite directions exactly when it spans half a turn or
    // more — that is, when its ends do not make a left turn at m.
    return detail::signValue(orientationSign(vertices_[static_cast<std::size_t>(m)],
                                             vertices_[static_cast<std::size_t>(clockwise)],
                                             vertices_[static_cast<std::size_t>(counter)])) <= 0;
}

template <TriangleConcept TriangleType_, SegmentConcept SegmentType_>
typename Triangulation<TriangleType_, SegmentType_>::VertexId
Triangulation<TriangleType_, SegmentType_>::nextVertexAlongRay(VertexId previous,
                                                               VertexId current) const {
    const TriId seed = incidentTriangleOf(current);
    if (seed == NO_TRI) {
        return GHOST;
    }
    const PointType& tail = vertices_[static_cast<std::size_t>(previous)];
    const PointType& head = vertices_[static_cast<std::size_t>(current)];
    // The ray direction is head - tail, and the cross product of that direction
    // with (w - head) is exactly orientationSign(tail, head, w). So the whole
    // walk speaks the shared predicate on stored points, and the direction is
    // never subtracted out. Its half-plane is read off the coordinates directly,
    // for the same reason.
    const int forwardHalf =
        (head.y() > tail.y() || (head.y() == tail.y() && head.x() > tail.x())) ? 0 : 1;
    const auto sameRay = [&](VertexId w) {
        const PointType& p = vertices_[static_cast<std::size_t>(w)];
        const int half = (p.y() > head.y() || (p.y() == head.y() && p.x() > head.x())) ? 0 : 1;
        return half == forwardHalf;
    };
    const auto side = [&](VertexId w) {
        return detail::signValue(
            orientationSign(tail, head, vertices_[static_cast<std::size_t>(w)]));
    };

    // Where the ray leaves `current`: along a fan edge, or into the interior of
    // one fan triangle. Anywhere else it leaves the domain at once.
    VertexId onEdge = GHOST;
    TriId entry = NO_TRI;
    int entrySide = 0;
    visitVertexFan(seed, current, [&](TriId t) {
        if (onEdge != GHOST || entry != NO_TRI || !inDomain(t)) {
            return;
        }
        const auto& v = triangles_[static_cast<std::size_t>(t)].v;
        const int i = v[0] == current ? 0 : (v[1] == current ? 1 : 2);
        const VertexId clockwise = v[(i + 1) % 3];
        const VertexId counter = v[(i + 2) % 3];
        const int fromClockwise = side(clockwise);
        const int fromCounter = side(counter);
        if (fromClockwise == 0 && sameRay(clockwise)) {
            onEdge = clockwise;
        } else if (fromCounter == 0 && sameRay(counter)) {
            onEdge = counter;
        } else if (fromClockwise < 0 && fromCounter > 0) {
            entry = t;
            entrySide = i;  // the side opposite `current`
        }
    });
    if (onEdge != GHOST) {
        // A mesh edge of an in-domain triangle: always both in the domain and
        // free of any vertex in between.
        return onEdge;
    }
    if (entry == NO_TRI || blocksVisibility(entry, entrySide)) {
        return GHOST;
    }

    // Straight walk from triangle to triangle. The ray entered through the
    // relative interior of the shared edge, so the two ends of that edge lie
    // strictly on opposite sides of it and the apex settles which side the ray
    // leaves by.
    TriId tri = triangles_[static_cast<std::size_t>(entry)].nbr[entrySide];
    int back = findSide(tri, entry);
    for (;;) {
        const auto& v = triangles_[static_cast<std::size_t>(tri)].v;
        const VertexId apex = v[back];
        const int fromApex = side(apex);
        if (fromApex == 0) {
            return apex;  // the ray runs straight into it
        }
        // The apex and the entry end on its own side of the ray stay together;
        // the ray leaves through the edge joining that group to the other end.
        const int leaving = fromApex == side(v[(back + 1) % 3]) ? (back + 1) % 3 : (back + 2) % 3;
        if (blocksVisibility(tri, leaving)) {
            return GHOST;
        }
        const TriId next = triangles_[static_cast<std::size_t>(tri)].nbr[leaving];
        back = findSide(next, tri);
        tri = next;
    }
}

template <TriangleConcept TriangleType_, SegmentConcept SegmentType_>
std::vector<std::vector<typename Triangulation<TriangleType_, SegmentType_>::VertexId>>
Triangulation<TriangleType_, SegmentType_>::visibleAdjacency() const {
    auto adjacency = clearVisibleAdjacency();
    if (domainTriangleCount_ == 0) {
        return adjacency;
    }
    const VertexId vertexCount = static_cast<VertexId>(vertices_.size());

    // Every in-domain mesh edge joins two mutually visible vertices. The
    // unblocked ones are already clearly visible; only the walls and the
    // boundary are missing. A constrained edge with domain on both sides is seen
    // from two triangles, and is taken from the lower-numbered one.
    for (TriId t = 0; t < firstGhost_; ++t) {
        if (!inDomain(t)) {
            continue;
        }
        for (int side = 0; side < 3; ++side) {
            if (!blocksVisibility(t, side)) {
                continue;
            }
            const TriId across = triangles_[static_cast<std::size_t>(t)].nbr[side];
            if (inDomain(across) && across < t) {
                continue;
            }
            const VertexId a = triangles_[static_cast<std::size_t>(t)].v[(side + 1) % 3];
            const VertexId b = triangles_[static_cast<std::size_t>(t)].v[(side + 2) % 3];
            adjacency[static_cast<std::size_t>(a)].push_back(b);
            adjacency[static_cast<std::size_t>(b)].push_back(a);
        }
    }

    // Collinear closure. What `adjacency` holds so far is exactly the pairs
    // seeing each other with no vertex in between, so a segment that does pass
    // through vertices is in the domain precisely when each of its pieces is:
    // walking a chain of such pieces in a fixed direction enumerates the rest.
    // Only a vertex a line can cross starts a chain, which spares a convex
    // domain — the one where this relation is densest — the walk entirely.
    std::vector<std::uint8_t> crossable(vertices_.size(), 0);
    bool anyCrossable = false;
    for (VertexId m = GHOST + 1; m < vertexCount; ++m) {
        crossable[static_cast<std::size_t>(m)] = passesThrough(m) ? 1 : 0;
        anyCrossable = anyCrossable || crossable[static_cast<std::size_t>(m)] != 0;
    }
    if (!anyCrossable) {
        return adjacency;
    }

    // Collected apart, since a chain reads the adjacency it is about to extend.
    std::vector<std::pair<VertexId, VertexId>> chained;
    for (VertexId source = GHOST + 1; source < vertexCount; ++source) {
        const std::size_t direct = adjacency[static_cast<std::size_t>(source)].size();
        for (std::size_t k = 0; k < direct; ++k) {
            VertexId previous = source;
            VertexId current = adjacency[static_cast<std::size_t>(source)][k];
            while (crossable[static_cast<std::size_t>(current)] != 0) {
                const VertexId next = nextVertexAlongRay(previous, current);
                if (next == GHOST) {
                    break;
                }
                chained.emplace_back(source, next);
                previous = current;
                current = next;
            }
        }
    }
    for (const auto& [a, b] : chained) {
        adjacency[static_cast<std::size_t>(a)].push_back(b);
        adjacency[static_cast<std::size_t>(b)].push_back(a);
    }
    return adjacency;
}

template <TriangleConcept TriangleType_, SegmentConcept SegmentType_>
std::vector<std::vector<typename Triangulation<TriangleType_, SegmentType_>::VertexId>>
Triangulation<TriangleType_, SegmentType_>::wallNeighbors() const {
    std::vector<std::vector<VertexId>> walls(vertices_.size());
    for (TriId t = 0; t < firstGhost_; ++t) {
        if (!inDomain(t)) {
            continue;
        }
        for (int side = 0; side < 3; ++side) {
            if (!blocksVisibility(t, side)) {
                continue;
            }
            const TriId across = triangles_[static_cast<std::size_t>(t)].nbr[side];
            if (inDomain(across) && across < t) {
                continue;
            }
            const VertexId a = triangles_[static_cast<std::size_t>(t)].v[(side + 1) % 3];
            const VertexId b = triangles_[static_cast<std::size_t>(t)].v[(side + 2) % 3];
            walls[static_cast<std::size_t>(a)].push_back(b);
            walls[static_cast<std::size_t>(b)].push_back(a);
        }
    }
    return walls;
}

template <TriangleConcept TriangleType_, SegmentConcept SegmentType_>
Graph<typename Triangulation<TriangleType_, SegmentType_>::PointType>
Triangulation<TriangleType_, SegmentType_>::clearVisibilityGraph() const {
    Graph<PointType> result;
    const VertexId vertexCount = static_cast<VertexId>(vertices_.size());
    for (VertexId v = GHOST + 1; v < vertexCount; ++v) {
        result.addVertex(vertices_[static_cast<std::size_t>(v)]);
    }
    const auto adjacency = clearVisibleAdjacency();
    for (VertexId u = GHOST + 1; u < vertexCount; ++u) {
        for (const VertexId w : adjacency[static_cast<std::size_t>(u)]) {
            if (u < w) {
                result.addEdge(vertices_[static_cast<std::size_t>(u)],
                               vertices_[static_cast<std::size_t>(w)]);
            }
        }
    }
    return result;
}

template <TriangleConcept TriangleType_, SegmentConcept SegmentType_>
Graph<typename Triangulation<TriangleType_, SegmentType_>::PointType>
Triangulation<TriangleType_, SegmentType_>::visibilityGraph() const {
    Graph<PointType> result;
    const VertexId vertexCount = static_cast<VertexId>(vertices_.size());
    for (VertexId v = GHOST + 1; v < vertexCount; ++v) {
        result.addVertex(vertices_[static_cast<std::size_t>(v)]);
    }
    const auto adjacency = visibleAdjacency();
    for (VertexId u = GHOST + 1; u < vertexCount; ++u) {
        for (const VertexId w : adjacency[static_cast<std::size_t>(u)]) {
            if (u < w) {
                result.addEdge(vertices_[static_cast<std::size_t>(u)],
                               vertices_[static_cast<std::size_t>(w)]);
            }
        }
    }
    return result;
}

template <TriangleConcept TriangleType_, SegmentConcept SegmentType_>
Graph<typename Triangulation<TriangleType_, SegmentType_>::PointType>
Triangulation<TriangleType_, SegmentType_>::reducedVisibilityGraph() const {
    Graph<PointType> result;
    const VertexId vertexCount = static_cast<VertexId>(vertices_.size());
    for (VertexId v = GHOST + 1; v < vertexCount; ++v) {
        result.addVertex(vertices_[static_cast<std::size_t>(v)]);
    }
    const auto adjacency = visibleAdjacency();
    const auto walls = wallNeighbors();

    // A taut path can bend at u only by wrapping around the walls meeting there,
    // which needs all of them on one side of the line it bends about. A wall
    // running along that line counts for either side, which is what keeps the
    // walls themselves — and anything collinear with one — in the graph.
    const auto tangent = [&](VertexId u, VertexId w) {
        const auto& incident = walls[static_cast<std::size_t>(u)];
        if (incident.empty()) {
            return false;
        }
        int seen = 0;
        for (const VertexId x : incident) {
            const int order = detail::signValue(
                orientationSign(vertices_[static_cast<std::size_t>(u)],
                                vertices_[static_cast<std::size_t>(w)],
                                vertices_[static_cast<std::size_t>(x)]));
            if (order != 0) {
                if (seen != 0 && seen != order) {
                    return false;
                }
                seen = order;
            }
        }
        return true;
    };

    for (VertexId u = GHOST + 1; u < vertexCount; ++u) {
        for (const VertexId w : adjacency[static_cast<std::size_t>(u)]) {
            if (u < w && tangent(u, w) && tangent(w, u)) {
                result.addEdge(vertices_[static_cast<std::size_t>(u)],
                               vertices_[static_cast<std::size_t>(w)]);
            }
        }
    }
    return result;
}

// -----------------------------------------------------------------------------
// Polygon
// -----------------------------------------------------------------------------

template <class PointType, class LabelType>
Graph<PointType> Polygon<PointType, LabelType>::visibilityGraph() const {
    const auto corners = vertices();
    const std::size_t n = corners.size();
    // A convex polygon — including one collapsed to a point or a segment —
    // contains every segment between its vertices, so the answer is the complete
    // graph and there is nothing to triangulate. Besides being much the cheaper
    // route for the case where this relation is densest, this is what handles
    // the documented degeneracies, which have no triangulation to speak of.
    if (n < 3 || isDegenerate() || isConvex()) {
        Graph<PointType> result;
        for (const auto& corner : corners) {
            result.addVertex(corner);
        }
        for (std::size_t i = 0; i < n; ++i) {
            for (std::size_t j = i + 1; j < n; ++j) {
                result.addEdge(corners[i], corners[j]);
            }
        }
        return result;
    }
    return triangulation().visibilityGraph();
}

template <class PointType, class LabelType>
Graph<PointType> Polygon<PointType, LabelType>::clearVisibilityGraph() const {
    const auto corners = vertices();
    if (corners.size() < 3 || isDegenerate()) {
        Graph<PointType> result;  // no interior, hence no clear sight anywhere
        for (const auto& corner : corners) {
            result.addVertex(corner);
        }
        return result;
    }
    return triangulation().clearVisibilityGraph();
}

template <class PointType, class LabelType>
Graph<PointType> Polygon<PointType, LabelType>::reducedVisibilityGraph() const {
    const auto corners = vertices();
    const std::size_t n = corners.size();
    if (n < 3 || isDegenerate()) {
        // Every vertex is collinear with every side, so tangency holds
        // everywhere and nothing is reduced away.
        return visibilityGraph();
    }
    return triangulation().reducedVisibilityGraph();
}

// -----------------------------------------------------------------------------
// PolygonWithHoles
// -----------------------------------------------------------------------------

template <class PointType_, class LabelType>
Graph<PointType_> PolygonWithHoles<PointType_, LabelType>::visibilityGraph() const {
    // Without holes the outer ring answers on its own, keeping its convex and
    // degenerate shortcuts.
    return holes().empty() ? outer().visibilityGraph() : triangulation().visibilityGraph();
}

template <class PointType_, class LabelType>
Graph<PointType_> PolygonWithHoles<PointType_, LabelType>::clearVisibilityGraph() const {
    return holes().empty() ? outer().clearVisibilityGraph()
                           : triangulation().clearVisibilityGraph();
}

template <class PointType_, class LabelType>
Graph<PointType_> PolygonWithHoles<PointType_, LabelType>::reducedVisibilityGraph() const {
    return holes().empty() ? outer().reducedVisibilityGraph()
                           : triangulation().reducedVisibilityGraph();
}

}  // namespace pgl
