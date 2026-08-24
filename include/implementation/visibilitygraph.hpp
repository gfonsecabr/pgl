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
template <class OnVertex, class OnBlocked>
void Triangulation<TriangleType_, SegmentType_>::expandVisibility(
    const PointType& origin, VisibilityCone start, std::vector<VisibilityCone>& scratch,
    OnVertex onVertex, OnBlocked onBlocked) const {
    scratch.clear();
    scratch.push_back(start);
    while (!scratch.empty()) {
        const VisibilityCone cone = scratch.back();
        scratch.pop_back();
        if (blocksVisibility(cone.tri, cone.side)) {
            onBlocked(cone);  // the cone runs into a wall: a leaf of the traversal
            continue;
        }
        const TriIndex entered = triangles_[static_cast<std::size_t>(cone.tri)].nbr[cone.side];
        const int back = findSide(entered, cone.tri);
        const auto& v = triangles_[static_cast<std::size_t>(entered)].v;
        // `entered` winds the shared edge the other way round, so its clockwise
        // end as seen from the origin is v[(back+2)%3] and its counterclockwise
        // end v[(back+1)%3]. The apex is the third vertex.
        const VertexIndex apex = v[back];
        const int fromRight = detail::signValue(orientationSign(
            origin, vertices_[static_cast<std::size_t>(cone.right)],
            vertices_[static_cast<std::size_t>(apex)]));
        const int fromLeft = detail::signValue(orientationSign(
            origin, vertices_[static_cast<std::size_t>(cone.left)],
            vertices_[static_cast<std::size_t>(apex)]));

        VertexIndex rightChildLeft = cone.left;
        VertexIndex leftChildRight = cone.right;
        bool crossRight = true;
        bool crossLeft = true;
        if (fromRight > 0 && fromLeft < 0) {
            // Strictly inside the cone: nothing stands between the origin and the
            // apex, which therefore is clearly visible and splits the cone in
            // two. An apex merely *on* a bound is not — the vertex that set that
            // bound is in the way — and the two comparisons being strict is the
            // whole of what separates clear visibility from the grazing kind.
            onVertex(apex);
            rightChildLeft = apex;
            leftChildRight = apex;
        } else if (fromRight <= 0) {
            crossRight = false;  // the clockwise sub-cone came out empty
        } else {
            crossLeft = false;   // the counterclockwise sub-cone came out empty
        }

        // Side (back+1)%3 is the clockwise half of the entry edge, side
        // (back+2)%3 the counterclockwise half. The counterclockwise child goes
        // on the stack first so the clockwise one comes off it first: leaves then
        // arrive in counterclockwise order, which is what lets
        // regularizedVisiblePolygon lay out its ring as it goes.
        if (crossLeft) {
            scratch.push_back({entered, static_cast<std::int8_t>((back + 2) % 3),
                               leftChildRight, cone.left});
        }
        if (crossRight) {
            scratch.push_back({entered, static_cast<std::int8_t>((back + 1) % 3),
                               cone.right, rightChildLeft});
        }
    }
}

template <TriangleConcept TriangleType_, SegmentConcept SegmentType_>
std::vector<std::vector<typename Triangulation<TriangleType_, SegmentType_>::VertexIndex>>
Triangulation<TriangleType_, SegmentType_>::clearVisibleAdjacency() const {
    std::vector<std::vector<VertexIndex>> adjacency(vertices_.size());
    if (domainTriangleCount_ == 0) {
        return adjacency;
    }
    const VertexIndex vertexCount = static_cast<VertexIndex>(vertices_.size());
    std::vector<VisibilityCone> scratch;
    const auto ignore = [](const VisibilityCone&) {};

    for (VertexIndex source = GHOST + 1; source < vertexCount; ++source) {
        const TriIndex seed = incidentTriangleOf(source);
        if (seed == NO_TRI) {
            continue;
        }
        const PointType& origin = vertices_[static_cast<std::size_t>(source)];
        auto& visible = adjacency[static_cast<std::size_t>(source)];
        const auto report = [&](VertexIndex w) { visible.push_back(w); };

        // One expansion per in-domain triangle of the source's fan. Such a
        // triangle (source, p, q) is counterclockwise, so p bounds its wedge
        // clockwise and q counterclockwise, and the fan wedges together tile
        // exactly the part of the plane the source can see into.
        visitVertexFan(seed, source, [&](TriIndex t) {
            if (!inDomain(t)) {
                return;
            }
            const auto& v = triangles_[static_cast<std::size_t>(t)].v;
            const int i = v[0] == source ? 0 : (v[1] == source ? 1 : 2);
            const VertexIndex clockwise = v[(i + 1) % 3];
            // Edge (source, clockwise) is the side opposite v[(i+2)%3]. Taking
            // only the clockwise neighbour visits every fan edge exactly once,
            // since an edge with no second in-domain triangle blocks anyway.
            if (!blocksVisibility(t, (i + 2) % 3)) {
                visible.push_back(clockwise);
            }
            expandVisibility(origin,
                             {t, static_cast<std::int8_t>(i), clockwise, v[(i + 2) % 3]},
                             scratch, report, ignore);
        });
    }
    return adjacency;
}

template <TriangleConcept TriangleType_, SegmentConcept SegmentType_>
typename Triangulation<TriangleType_, SegmentType_>::TriIndex
Triangulation<TriangleType_, SegmentType_>::inDomainTriangleAt(const PointType& query,
                                                               TriIndex start) const {
    TriIndex real = start;
    if (real == NO_TRI) {
        return NO_TRI;
    }
    // A ghost triangle tiles the exterior beyond one real hull edge, so a query
    // that landed on one is outside the domain unless it sits on that very edge.
    if (isGhost(real)) {
        const auto& v = triangles_[static_cast<std::size_t>(real)].v;
        const int atGhost = v[0] == GHOST ? 0 : (v[1] == GHOST ? 1 : 2);
        const PointType& a = vertices_[static_cast<std::size_t>(v[(atGhost + 1) % 3])];
        const PointType& b = vertices_[static_cast<std::size_t>(v[(atGhost + 2) % 3])];
        if (!Segment<PointType>(a, b).contains(query)) {
            return NO_TRI;
        }
        real = triangles_[static_cast<std::size_t>(real)].nbr[atGhost];
        if (real == NO_TRI || isGhost(real)) {
            return NO_TRI;
        }
    }
    if (inDomain(real)) {
        return real;
    }
    // A hull-fill triangle outside the domain. The query can still be on the
    // domain boundary, which it shares: at one of the triangle's vertices, or in
    // the relative interior of one of its sides.
    const auto& v = triangles_[static_cast<std::size_t>(real)].v;
    for (int i = 0; i < 3; ++i) {
        if (v[i] == GHOST || !(vertices_[static_cast<std::size_t>(v[i])] == query)) {
            continue;
        }
        TriIndex answer = NO_TRI;
        visitVertexFan(real, v[i], [&](TriIndex t) {
            if (answer == NO_TRI && inDomain(t)) {
                answer = t;
            }
        });
        return answer;
    }
    for (int s = 0; s < 3; ++s) {
        const VertexIndex a = v[(s + 1) % 3];
        const VertexIndex b = v[(s + 2) % 3];
        if (a == GHOST || b == GHOST) {
            continue;
        }
        const PointType& pa = vertices_[static_cast<std::size_t>(a)];
        const PointType& pb = vertices_[static_cast<std::size_t>(b)];
        if (!Segment<PointType>(pa, pb).contains(query)) {
            continue;
        }
        const TriIndex across = triangles_[static_cast<std::size_t>(real)].nbr[s];
        if (inDomain(across)) {
            return across;
        }
    }
    return NO_TRI;
}

template <TriangleConcept TriangleType_, SegmentConcept SegmentType_>
typename Triangulation<TriangleType_, SegmentType_>::VisibilitySeeds
Triangulation<TriangleType_, SegmentType_>::visibilitySeeds(const PointType& query) const {
    VisibilitySeeds seeds;
    if (domainTriangleCount_ == 0) {
        return seeds;
    }
    TriIndex found = locateIndex(query);
    if (!inDomain(found)) {
        found = inDomainTriangleAt(query, found);
    }
    if (found == NO_TRI) {
        return seeds;
    }
    seeds.located = true;

    // `found` holds the query in its closure, so the query is one of its
    // vertices, inside one of its sides, or strictly within it.
    const auto& fv = triangles_[static_cast<std::size_t>(found)].v;
    int atVertex = -1;
    int onSide = -1;
    for (int i = 0; i < 3; ++i) {
        if (vertices_[static_cast<std::size_t>(fv[i])] == query) {
            atVertex = i;
        }
    }
    if (atVertex < 0) {
        for (int s = 0; s < 3; ++s) {
            if (orientationSign(vertices_[static_cast<std::size_t>(fv[(s + 1) % 3])],
                                vertices_[static_cast<std::size_t>(fv[(s + 2) % 3])],
                                query) == 0) {
                onSide = s;
            }
        }
    }

    // Crossing side `side` of `t` spans the wedge from v[(side+1)%3] clockwise to
    // v[(side+2)%3] counterclockwise, whichever of the three cases seeds it.
    const auto addCone = [&](TriIndex t, int side) {
        const auto& v = triangles_[static_cast<std::size_t>(t)].v;
        seeds.cones.push_back({t, static_cast<std::int8_t>(side), v[(side + 1) % 3],
                               v[(side + 2) % 3]});
    };

    if (atVertex >= 0) {
        const VertexIndex self = fv[atVertex];
        visitVertexFan(found, self, [&](TriIndex t) {
            if (!inDomain(t)) {
                return;
            }
            const auto& v = triangles_[static_cast<std::size_t>(t)].v;
            const int i = v[0] == self ? 0 : (v[1] == self ? 1 : 2);
            // Both bounds, not just the clockwise one: where the fan runs out of
            // domain — at every boundary vertex — the last edge is no other
            // in-domain triangle's clockwise bound and would go unlisted. The
            // duplicates this leaves are dropped below, and an edge's two records
            // agree on whether sight along it grazes, that being a property of
            // the edge.
            seeds.direct.emplace_back(v[(i + 1) % 3], blocksVisibility(t, (i + 2) % 3));
            seeds.direct.emplace_back(v[(i + 2) % 3], blocksVisibility(t, (i + 1) % 3));
            addCone(t, i);
        });
    } else if (onSide >= 0) {
        // The query splits a mesh edge. Sight along that edge only grazes when the
        // edge bounds the domain or walls it off; the wedges to either side of it
        // are seeded independently, so a wall is never crossed.
        const bool grazes = blocksVisibility(found, onSide);
        seeds.direct.emplace_back(fv[(onSide + 1) % 3], grazes);
        seeds.direct.emplace_back(fv[(onSide + 2) % 3], grazes);
        const TriIndex across = triangles_[static_cast<std::size_t>(found)].nbr[onSide];
        for (const TriIndex t : {found, across}) {
            if (!inDomain(t)) {
                continue;
            }
            const int s = t == found ? onSide : static_cast<int>(findSide(t, found));
            seeds.direct.emplace_back(triangles_[static_cast<std::size_t>(t)].v[s], false);
            addCone(t, (s + 1) % 3);
            addCone(t, (s + 2) % 3);
        }
    } else {
        for (int i = 0; i < 3; ++i) {
            seeds.direct.emplace_back(fv[i], false);
        }
        for (int s = 0; s < 3; ++s) {
            addCone(found, s);
        }
    }

    // Counterclockwise by each cone's clockwise bound. Two cones never share that
    // bound's direction — that would put one bound inside the edge to the other —
    // so the order is total.
    const auto upperHalf = [&](VertexIndex w) {
        const PointType& p = vertices_[static_cast<std::size_t>(w)];
        return p.y() > query.y() || (p.y() == query.y() && p.x() > query.x()) ? 0 : 1;
    };
    std::sort(seeds.cones.begin(), seeds.cones.end(),
              [&](const VisibilityCone& a, const VisibilityCone& b) {
        const int half = upperHalf(a.right);
        const int other = upperHalf(b.right);
        if (half != other) {
            return half < other;
        }
        return orientationSign(query, vertices_[static_cast<std::size_t>(a.right)],
                               vertices_[static_cast<std::size_t>(b.right)]) > 0;
    });

    // A cone continues the previous one when it opens where that one closed;
    // anywhere else the visible directions break, and the region reaches the
    // query along a separate lobe. Rotating a break to the front leaves the arcs
    // as ascending ranges.
    const std::size_t count = seeds.cones.size();
    std::size_t firstBreak = count;
    for (std::size_t k = 0; k < count && firstBreak == count; ++k) {
        if (seeds.cones[(k + count - 1) % count].left != seeds.cones[k].right) {
            firstBreak = k;
        }
    }
    seeds.fullTurn = firstBreak == count;
    if (seeds.fullTurn) {
        seeds.arcs.push_back(0);
    } else {
        std::rotate(seeds.cones.begin(),
                    seeds.cones.begin() + static_cast<std::ptrdiff_t>(firstBreak),
                    seeds.cones.end());
        for (std::size_t k = 0; k < count; ++k) {
            if (k == 0 || seeds.cones[k - 1].left != seeds.cones[k].right) {
                seeds.arcs.push_back(k);
            }
        }
    }

    std::sort(seeds.direct.begin(), seeds.direct.end());
    seeds.direct.erase(std::unique(seeds.direct.begin(), seeds.direct.end(),
                                   [](const auto& a, const auto& b) {
                                       return a.first == b.first;
                                   }),
                       seeds.direct.end());
    return seeds;
}

template <TriangleConcept TriangleType_, SegmentConcept SegmentType_>
std::vector<typename Triangulation<TriangleType_, SegmentType_>::VertexIndex>
Triangulation<TriangleType_, SegmentType_>::visibleIds(const PointType& query,
                                                       const VisibilitySeeds& seeds,
                                                       bool grazing) const {
    std::vector<VertexIndex> found;
    if (!seeds.located) {
        return found;
    }
    for (const auto& [vertex, grazes] : seeds.direct) {
        if (grazing || !grazes) {
            found.push_back(vertex);
        }
    }
    std::vector<VisibilityCone> scratch;
    const auto report = [&](VertexIndex w) { found.push_back(w); };
    const auto ignore = [](const VisibilityCone&) {};
    for (const VisibilityCone& cone : seeds.cones) {
        expandVisibility(query, cone, scratch, report, ignore);
    }
    std::sort(found.begin(), found.end());
    found.erase(std::unique(found.begin(), found.end()), found.end());
    return found;
}

template <TriangleConcept TriangleType_, SegmentConcept SegmentType_>
std::vector<typename Triangulation<TriangleType_, SegmentType_>::PointType>
Triangulation<TriangleType_, SegmentType_>::clearlyVisibleVertices(
    const PointType& query) const {
    const VisibilitySeeds seeds = visibilitySeeds(query);
    std::vector<PointType> result;
    for (const VertexIndex v : visibleIds(query, seeds, false)) {
        result.push_back(vertices_[static_cast<std::size_t>(v)]);
    }
    sortAround(result, query);
    return result;
}

template <TriangleConcept TriangleType_, SegmentConcept SegmentType_>
std::vector<typename Triangulation<TriangleType_, SegmentType_>::PointType>
Triangulation<TriangleType_, SegmentType_>::visibleVertices(const PointType& query) const {
    const VisibilitySeeds seeds = visibilitySeeds(query);
    std::vector<VertexIndex> reached = visibleIds(query, seeds, true);
    if (reached.empty()) {
        return {};
    }
    // Grazing sight through a vertex, exactly as in visibleAdjacency: a segment
    // that passes straight through vertices is in the domain when each of its
    // pieces is, so walking each ray on from the first vertex it meets picks up
    // the rest. Only a vertex a line can cross starts a chain.
    const std::size_t direct = reached.size();
    for (std::size_t k = 0; k < direct; ++k) {
        VertexIndex current = reached[k];
        if (!passesThrough(current)) {
            continue;
        }
        VertexIndex next = nextVertexAlongRay(query, current);
        while (next != GHOST) {
            reached.push_back(next);
            if (!passesThrough(next)) {
                break;
            }
            const VertexIndex previous = current;
            current = next;
            next = nextVertexAlongRay(vertices_[static_cast<std::size_t>(previous)], current);
        }
    }
    std::sort(reached.begin(), reached.end());
    reached.erase(std::unique(reached.begin(), reached.end()), reached.end());
    std::vector<PointType> result;
    result.reserve(reached.size());
    for (const VertexIndex v : reached) {
        result.push_back(vertices_[static_cast<std::size_t>(v)]);
    }
    sortAround(result, query);
    return result;
}

template <TriangleConcept TriangleType_, SegmentConcept SegmentType_>
bool Triangulation<TriangleType_, SegmentType_>::passesThrough(VertexIndex m) const {
    const TriIndex seed = incidentTriangleOf(m);
    if (seed == NO_TRI) {
        return false;
    }
    // Extremes of the domain wedges meeting at m: the ends of a wedge are the
    // fan edges whose other triangle is out of domain.
    VertexIndex clockwise = GHOST;
    VertexIndex counter = GHOST;
    int extremes = 0;
    bool any = false;
    visitVertexFan(seed, m, [&](TriIndex t) {
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
typename Triangulation<TriangleType_, SegmentType_>::VertexIndex
Triangulation<TriangleType_, SegmentType_>::nextVertexAlongRay(const PointType& tail,
                                                               VertexIndex current) const {
    const TriIndex seed = incidentTriangleOf(current);
    if (seed == NO_TRI) {
        return GHOST;
    }
    const PointType& head = vertices_[static_cast<std::size_t>(current)];
    // The ray direction is head - tail, and the cross product of that direction
    // with (w - head) is exactly orientationSign(tail, head, w). So the whole
    // walk speaks the shared predicate on stored points, and the direction is
    // never subtracted out. Its half-plane is read off the coordinates directly,
    // for the same reason.
    const int forwardHalf =
        (head.y() > tail.y() || (head.y() == tail.y() && head.x() > tail.x())) ? 0 : 1;
    const auto sameRay = [&](VertexIndex w) {
        const PointType& p = vertices_[static_cast<std::size_t>(w)];
        const int half = (p.y() > head.y() || (p.y() == head.y() && p.x() > head.x())) ? 0 : 1;
        return half == forwardHalf;
    };
    const auto side = [&](VertexIndex w) {
        return detail::signValue(
            orientationSign(tail, head, vertices_[static_cast<std::size_t>(w)]));
    };

    // Where the ray leaves `current`: along a fan edge, or into the interior of
    // one fan triangle. Anywhere else it leaves the domain at once.
    VertexIndex onEdge = GHOST;
    TriIndex entry = NO_TRI;
    int entrySide = 0;
    visitVertexFan(seed, current, [&](TriIndex t) {
        if (onEdge != GHOST || entry != NO_TRI || !inDomain(t)) {
            return;
        }
        const auto& v = triangles_[static_cast<std::size_t>(t)].v;
        const int i = v[0] == current ? 0 : (v[1] == current ? 1 : 2);
        const VertexIndex clockwise = v[(i + 1) % 3];
        const VertexIndex counter = v[(i + 2) % 3];
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
    TriIndex tri = triangles_[static_cast<std::size_t>(entry)].nbr[entrySide];
    int back = findSide(tri, entry);
    for (;;) {
        const auto& v = triangles_[static_cast<std::size_t>(tri)].v;
        const VertexIndex apex = v[back];
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
        const TriIndex next = triangles_[static_cast<std::size_t>(tri)].nbr[leaving];
        back = findSide(next, tri);
        tri = next;
    }
}

template <TriangleConcept TriangleType_, SegmentConcept SegmentType_>
std::vector<std::vector<typename Triangulation<TriangleType_, SegmentType_>::VertexIndex>>
Triangulation<TriangleType_, SegmentType_>::visibleAdjacency() const {
    auto adjacency = clearVisibleAdjacency();
    if (domainTriangleCount_ == 0) {
        return adjacency;
    }
    const VertexIndex vertexCount = static_cast<VertexIndex>(vertices_.size());

    // Every in-domain mesh edge joins two mutually visible vertices. The
    // unblocked ones are already clearly visible; only the walls and the
    // boundary are missing. A constrained edge with domain on both sides is seen
    // from two triangles, and is taken from the lower-numbered one.
    for (TriIndex t = 0; t < firstGhost_; ++t) {
        if (!inDomain(t)) {
            continue;
        }
        for (int side = 0; side < 3; ++side) {
            if (!blocksVisibility(t, side)) {
                continue;
            }
            const TriIndex across = triangles_[static_cast<std::size_t>(t)].nbr[side];
            if (inDomain(across) && across < t) {
                continue;
            }
            const VertexIndex a = triangles_[static_cast<std::size_t>(t)].v[(side + 1) % 3];
            const VertexIndex b = triangles_[static_cast<std::size_t>(t)].v[(side + 2) % 3];
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
    for (VertexIndex m = GHOST + 1; m < vertexCount; ++m) {
        crossable[static_cast<std::size_t>(m)] = passesThrough(m) ? 1 : 0;
        anyCrossable = anyCrossable || crossable[static_cast<std::size_t>(m)] != 0;
    }
    if (!anyCrossable) {
        return adjacency;
    }

    // Collected apart, since a chain reads the adjacency it is about to extend.
    std::vector<std::pair<VertexIndex, VertexIndex>> chained;
    for (VertexIndex source = GHOST + 1; source < vertexCount; ++source) {
        const std::size_t direct = adjacency[static_cast<std::size_t>(source)].size();
        for (std::size_t k = 0; k < direct; ++k) {
            VertexIndex previous = source;
            VertexIndex current = adjacency[static_cast<std::size_t>(source)][k];
            while (crossable[static_cast<std::size_t>(current)] != 0) {
                const VertexIndex next =
                    nextVertexAlongRay(vertices_[static_cast<std::size_t>(previous)], current);
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
std::vector<std::vector<typename Triangulation<TriangleType_, SegmentType_>::VertexIndex>>
Triangulation<TriangleType_, SegmentType_>::wallNeighbors() const {
    std::vector<std::vector<VertexIndex>> walls(vertices_.size());
    for (TriIndex t = 0; t < firstGhost_; ++t) {
        if (!inDomain(t)) {
            continue;
        }
        for (int side = 0; side < 3; ++side) {
            if (!blocksVisibility(t, side)) {
                continue;
            }
            const TriIndex across = triangles_[static_cast<std::size_t>(t)].nbr[side];
            if (inDomain(across) && across < t) {
                continue;
            }
            const VertexIndex a = triangles_[static_cast<std::size_t>(t)].v[(side + 1) % 3];
            const VertexIndex b = triangles_[static_cast<std::size_t>(t)].v[(side + 2) % 3];
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
    const VertexIndex vertexCount = static_cast<VertexIndex>(vertices_.size());
    for (VertexIndex v = GHOST + 1; v < vertexCount; ++v) {
        result.addVertex(vertices_[static_cast<std::size_t>(v)]);
    }
    const auto adjacency = clearVisibleAdjacency();
    for (VertexIndex u = GHOST + 1; u < vertexCount; ++u) {
        for (const VertexIndex w : adjacency[static_cast<std::size_t>(u)]) {
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
    const VertexIndex vertexCount = static_cast<VertexIndex>(vertices_.size());
    for (VertexIndex v = GHOST + 1; v < vertexCount; ++v) {
        result.addVertex(vertices_[static_cast<std::size_t>(v)]);
    }
    const auto adjacency = visibleAdjacency();
    for (VertexIndex u = GHOST + 1; u < vertexCount; ++u) {
        for (const VertexIndex w : adjacency[static_cast<std::size_t>(u)]) {
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
    const VertexIndex vertexCount = static_cast<VertexIndex>(vertices_.size());
    for (VertexIndex v = GHOST + 1; v < vertexCount; ++v) {
        result.addVertex(vertices_[static_cast<std::size_t>(v)]);
    }
    const auto adjacency = visibleAdjacency();
    const auto walls = wallNeighbors();

    // A taut path can bend at u only by wrapping around the walls meeting there,
    // which needs all of them on one side of the line it bends about. A wall
    // running along that line counts for either side, which is what keeps the
    // walls themselves — and anything collinear with one — in the graph.
    const auto tangent = [&](VertexIndex u, VertexIndex w) {
        const auto& incident = walls[static_cast<std::size_t>(u)];
        if (incident.empty()) {
            return false;
        }
        int seen = 0;
        for (const VertexIndex x : incident) {
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

    for (VertexIndex u = GHOST + 1; u < vertexCount; ++u) {
        for (const VertexIndex w : adjacency[static_cast<std::size_t>(u)]) {
            if (u < w && tangent(u, w) && tangent(w, u)) {
                result.addEdge(vertices_[static_cast<std::size_t>(u)],
                               vertices_[static_cast<std::size_t>(w)]);
            }
        }
    }
    return result;
}

template <TriangleConcept TriangleType_, SegmentConcept SegmentType_>
template <class ResultNumber>
Polygon<Point<ResultNumber>>
Triangulation<TriangleType_, SegmentType_>::regularizedVisiblePolygon(
    const PointType& query) const {
    using ResultPoint = Point<ResultNumber>;
    const VisibilitySeeds seeds = visibilitySeeds(query);
    if (!seeds.located) {
        return Polygon<ResultPoint>();
    }
    const ResultNumber originX = detail::asNumber<ResultNumber>(query.x());
    const ResultNumber originY = detail::asNumber<ResultNumber>(query.y());

    // Where the ray from the query through vertex `through` meets the line of the
    // edge (a,b). Every cone that reaches a blocking edge lies inside the wedge
    // that edge spans from the query, so the ray always meets it and the
    // denominator never vanishes; when `through` is an end of the edge the
    // quotient reproduces that end exactly. This is the only division in the
    // whole construction, which is why the result type is the caller's to pick.
    const auto rayHit = [&](VertexIndex through, VertexIndex a, VertexIndex b) {
        const PointType& target = vertices_[static_cast<std::size_t>(through)];
        const PointType& head = vertices_[static_cast<std::size_t>(a)];
        const PointType& tail = vertices_[static_cast<std::size_t>(b)];
        const ResultNumber dx = detail::asNumber<ResultNumber>(target.x()) - originX;
        const ResultNumber dy = detail::asNumber<ResultNumber>(target.y()) - originY;
        const ResultNumber edgeX =
            detail::asNumber<ResultNumber>(tail.x()) - detail::asNumber<ResultNumber>(head.x());
        const ResultNumber edgeY =
            detail::asNumber<ResultNumber>(tail.y()) - detail::asNumber<ResultNumber>(head.y());
        const ResultNumber toEdgeX = detail::asNumber<ResultNumber>(head.x()) - originX;
        const ResultNumber toEdgeY = detail::asNumber<ResultNumber>(head.y()) - originY;
        const ResultNumber along = toEdgeX * edgeY - toEdgeY * edgeX;
        const ResultNumber sweep = dx * edgeY - dy * edgeX;
        const ResultNumber scale = along / sweep;
        return ResultPoint(originX + scale * dx, originY + scale * dy);
    };

    std::vector<ResultPoint> ring;
    const auto append = [&](const ResultPoint& p) {
        if (ring.empty() || !(ring.back() == p)) {
            ring.push_back(p);
        }
    };

    std::vector<VisibilityCone> scratch;
    const auto ignore = [](VertexIndex) {};
    const std::size_t count = seeds.cones.size();
    for (std::size_t arc = 0; arc < seeds.arcs.size(); ++arc) {
        const std::size_t begin = seeds.arcs[arc];
        const std::size_t end = arc + 1 < seeds.arcs.size() ? seeds.arcs[arc + 1] : count;
        // A lobe that stops short of a full turn is bounded by the two boundary
        // edges the query itself lies on, which meet at the query: it belongs to
        // the ring, and hinges the lobe onto whatever came before.
        if (!seeds.fullTurn) {
            append(ResultPoint(originX, originY));
        }
        for (std::size_t k = begin; k < end; ++k) {
            expandVisibility(query, seeds.cones[k], scratch, ignore,
                             [&](const VisibilityCone& cone) {
                // The wall this cone ran into. Its visible stretch is the part
                // between the two bounding rays, and the leaves arrive
                // counterclockwise, so appending as they come lays out the ring.
                const auto& v = triangles_[static_cast<std::size_t>(cone.tri)].v;
                const VertexIndex a = v[(cone.side + 1) % 3];
                const VertexIndex b = v[(cone.side + 2) % 3];
                append(rayHit(cone.right, a, b));
                append(rayHit(cone.left, a, b));
            });
        }
    }
    while (ring.size() > 1 && ring.front() == ring.back()) {
        ring.pop_back();
    }
    if (ring.size() < 3) {
        return Polygon<ResultPoint>();  // no area to bound
    }
    // Already counterclockwise by construction, so rotating the lexicographically
    // smallest vertex to the front is the whole of the canonical form and the
    // constructor need not settle the orientation — which for a rational
    // coordinate type costs more than everything above.
    std::rotate(ring.begin(), std::min_element(ring.begin(), ring.end()), ring.end());
    return Polygon<ResultPoint>(ring, true);
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

template <class PointType, class LabelType>
std::vector<PointType> Polygon<PointType, LabelType>::visibleVertices(
    const PointType& query) const {
    if (size() < 3 || isDegenerate()) {
        return {};  // no area to see across
    }
    return triangulation().visibleVertices(query);
}

template <class PointType, class LabelType>
std::vector<PointType> Polygon<PointType, LabelType>::clearlyVisibleVertices(
    const PointType& query) const {
    if (size() < 3 || isDegenerate()) {
        return {};  // no interior to see through
    }
    return triangulation().clearlyVisibleVertices(query);
}

template <class PointType, class LabelType>
template <class ResultNumber>
Polygon<Point<ResultNumber>> Polygon<PointType, LabelType>::regularizedVisiblePolygon(
    const PointType& query) const {
    if (size() < 3 || isDegenerate()) {
        return Polygon<Point<ResultNumber>>();
    }
    return triangulation().template regularizedVisiblePolygon<ResultNumber>(query);
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

template <class PointType_, class LabelType>
std::vector<PointType_> PolygonWithHoles<PointType_, LabelType>::visibleVertices(
    const PointType& query) const {
    return holes().empty() ? outer().visibleVertices(query)
                           : triangulation().visibleVertices(query);
}

template <class PointType_, class LabelType>
std::vector<PointType_> PolygonWithHoles<PointType_, LabelType>::clearlyVisibleVertices(
    const PointType& query) const {
    return holes().empty() ? outer().clearlyVisibleVertices(query)
                           : triangulation().clearlyVisibleVertices(query);
}

template <class PointType_, class LabelType>
template <class ResultNumber>
Polygon<Point<ResultNumber>>
PolygonWithHoles<PointType_, LabelType>::regularizedVisiblePolygon(
    const PointType& query) const {
    return holes().empty()
               ? outer().template regularizedVisiblePolygon<ResultNumber>(query)
               : triangulation().template regularizedVisiblePolygon<ResultNumber>(query);
}

}  // namespace pgl
