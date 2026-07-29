#pragma once

#include "algorithm/triangulation.hpp"

/**
 * @file difference.hpp
 * @brief Regularized set difference of two closed polygonal regions.
 *
 * `A ∖ B` is the first boolean operation in the library, and the first
 * construction whose result genuinely needs @ref PolygonWithHoles: removing a
 * polygon from the middle of another one leaves a region with a hole, which no
 * other shape can express. `A ∩ B` never needs it — the intersection of two
 * closed sets with connected complements has a connected complement of its own,
 * so it can carry no hole (see @ref Polygon::intersection(const OtherPolygon&)).
 *
 * What is computed is the **regularized** difference `closure(A° ∖ B)`: the
 * part of `A` with area that survives the removal, with lower-dimensional
 * leftovers (a stretch of `∂A` that `B` touches without covering, an isolated
 * contact point) dropped. That is what makes the result a set of regions rather
 * than a set of regions plus loose edges, and it is the usual convention for
 * boolean operations on solids.
 *
 * The engine is the cell decomposition of @ref detail::cellSeparates seen from
 * the other side: the arrangement of both boundaries cuts the plane into
 * triangles on which membership in `A` and in `B` is constant, one witness
 * point per triangle decides whether it survives, and the boundary of the union
 * of the surviving triangles is read off the mesh. Everything is exact — the
 * arrangement is built over rationals — and the result is converted to the
 * requested coordinate type only at the very end, so an integral answer comes
 * back integral however the intermediate crossings looked.
 */

#include <algorithm>
#include <cstddef>
#include <limits>
#include <map>
#include <set>
#include <utility>
#include <vector>

namespace pgl {

namespace detail {

/**
 * @brief Removes the vertices of a ring that lie in the middle of a straight
 *        stretch.
 *
 * The rings come out of an arrangement, so every crossing of the two operands
 * is a vertex of both cells beside it even where the boundary runs straight
 * through. Dropping those keeps the result equal to what one would write by
 * hand, and it never changes the point set.
 */
template <class ExactPoint>
void dropCollinearRingVertices(std::vector<ExactPoint>& ring) {
    bool changed = true;
    while (changed && ring.size() > 3) {
        changed = false;
        for (std::size_t i = 0; i < ring.size() && ring.size() > 3;) {
            const ExactPoint& previous = ring[(i + ring.size() - 1) % ring.size()];
            const ExactPoint& next = ring[(i + 1) % ring.size()];
            if (collinear(previous, ring[i], next)) {
                ring.erase(ring.begin() + static_cast<std::ptrdiff_t>(i));
                changed = true;
            } else {
                ++i;
            }
        }
    }
}

/** @brief Twice the signed area of a ring; positive when it runs counterclockwise. */
template <class ExactPoint>
typename ExactPoint::NumberType twiceSignedRingArea(const std::vector<ExactPoint>& ring) {
    typename ExactPoint::NumberType total(0);
    for (std::size_t i = 0; i < ring.size(); ++i) {
        const ExactPoint& p = ring[i];
        const ExactPoint& q = ring[(i + 1) % ring.size()];
        total += p.x() * q.y() - q.x() * p.y();
    }
    return total;
}

/**
 * @brief Cuts a closed boundary walk into simple rings at its repeated vertices.
 *
 * A boundary walk visits a vertex twice exactly where the region pinches shut
 * there — a hole closing over the outer ring at a point, or two holes meeting.
 * The walk is then a correct description of the boundary but not a polygon, so
 * the loop between the two visits is peeled off as a ring of its own. That is
 * precisely the decomposition @ref PolygonWithHoles wants: the peeled loop is
 * the hole and what remains is the ring it touches.
 */
template <class ExactPoint>
void splitWalkIntoRings(const std::vector<ExactPoint>& walk,
                        std::vector<std::vector<ExactPoint>>& out) {
    std::vector<ExactPoint> pending;
    std::map<ExactPoint, std::size_t> position;
    for (const ExactPoint& vertex : walk) {
        const auto seen = position.find(vertex);
        if (seen != position.end()) {
            const std::size_t from = seen->second;
            out.emplace_back(pending.begin() + static_cast<std::ptrdiff_t>(from), pending.end());
            for (std::size_t i = from; i < pending.size(); ++i) {
                position.erase(pending[i]);
            }
            pending.resize(from);
        }
        position.emplace(vertex, pending.size());
        pending.push_back(vertex);
    }
    if (!pending.empty()) {
        out.push_back(std::move(pending));
    }
}

/**
 * @brief The regularized difference `closure(A° ∖ B)`, as a set of regions.
 *
 * Both operands must be bounded and polygonal, and neither may be
 * self-overlapping; nothing else is asked of them, so either may be a polygon,
 * a convex shape, a rectangle, a triangle or a region with holes.
 *
 * The returned regions have pairwise disjoint interiors and their union is the
 * difference. They are *not* nested: an island of `A` stranded inside a hole of
 * the result comes back as a region of its own, which is what a flat list of
 * regions can say (see decision (c): this library has no `PolygonSet`).
 *
 * Complexity: O(m²) segment intersections for m boundary edges, then a
 * constrained triangulation over the arrangement and one O(n) point location
 * per cell.
 */
template <class ResultPoint, class ShapeA, class ShapeB>
std::vector<PolygonWithHoles<ResultPoint>> regularizedDifference(const ShapeA& a, const ShapeB& b) {
    using ExactNumber = Exact1DNumber<typename ShapeA::NumberType, typename ShapeB::NumberType>;
    using ExactPoint = Point<ExactNumber>;
    using ExactSegment = Segment<ExactPoint>;
    using ExactPolygon = Polygon<ExactPoint>;
    using ResultPolygon = Polygon<ResultPoint>;
    using Dart = std::pair<ExactPoint, ExactPoint>;
    constexpr std::size_t none = std::numeric_limits<std::size_t>::max();

    std::vector<PolygonWithHoles<ResultPoint>> result;

    std::vector<ExactSegment> cuts;
    appendCutSegments<ExactPoint>(a, cuts);
    appendCutSegments<ExactPoint>(b, cuts);
    if (cuts.empty()) {
        return result;  // neither operand has an edge, so neither has area
    }

    // A box strictly containing both operands, so no cell of the arrangement
    // straddles its boundary and the surviving cells are decided entirely by
    // the two operands.
    ExactNumber loX = cuts.front().min().x();
    ExactNumber loY = cuts.front().min().y();
    ExactNumber hiX = loX;
    ExactNumber hiY = loY;
    for (const ExactSegment& cut : cuts) {
        for (const ExactPoint& point : {cut.min(), cut.max()}) {
            loX = std::min(loX, point.x());
            loY = std::min(loY, point.y());
            hiX = std::max(hiX, point.x());
            hiY = std::max(hiY, point.y());
        }
    }
    const ExactNumber margin(1);
    const ExactPolygon box(std::vector<ExactPoint>{
        ExactPoint(loX - margin, loY - margin), ExactPoint(hiX + margin, loY - margin),
        ExactPoint(hiX + margin, hiY + margin), ExactPoint(loX - margin, hiY + margin)});
    const auto mesh = box.triangulation(arrangedCutSegments(cuts, std::vector<ExactPoint>{}));
    const auto triangles = mesh.triangles();

    // Each cell is in the difference or out of it as a whole: its interior meets
    // no boundary edge of either operand, so one witness point settles it. The
    // witness is strictly inside its triangle, hence on neither boundary, which
    // is why the closed containments answer the open question here.
    std::vector<char> keep(triangles.size(), 0);
    std::map<Dart, std::size_t> leftOf;
    for (std::size_t i = 0; i < triangles.size(); ++i) {
        const auto& triangle = triangles[i];
        for (int k = 0; k < 3; ++k) {
            leftOf.emplace(Dart(ExactPoint(triangle.get(k)), ExactPoint(triangle.get(k + 1))), i);
        }
        const ExactPoint witness = triangle.template pointInside<ExactNumber>();
        keep[i] = static_cast<char>(a.contains(witness) && !b.contains(witness));
    }

    // The triangle across a directed edge is the one carrying the reverse dart.
    const auto across = [&](const ExactPoint& from, const ExactPoint& to) {
        const auto it = leftOf.find(Dart(to, from));
        return it == leftOf.end() ? none : it->second;
    };

    // Kept cells sharing an edge are one piece of the difference. Cells meeting
    // at a single vertex are not: the difference pinches shut there, and the two
    // sides have to come back as two regions, since neither a polygon nor a
    // region may have a self-touching outer ring.
    std::vector<std::size_t> parent(triangles.size());
    for (std::size_t i = 0; i < parent.size(); ++i) {
        parent[i] = i;
    }
    const auto findRoot = [&parent](std::size_t x) {
        while (parent[x] != x) {
            parent[x] = parent[parent[x]];
            x = parent[x];
        }
        return x;
    };
    std::vector<Dart> boundary;
    for (std::size_t i = 0; i < triangles.size(); ++i) {
        if (!keep[i]) {
            continue;
        }
        for (int k = 0; k < 3; ++k) {
            const ExactPoint from(triangles[i].get(k));
            const ExactPoint to(triangles[i].get(k + 1));
            const std::size_t other = across(from, to);
            if (other == none || !keep[other]) {
                boundary.emplace_back(from, to);
            } else {
                parent[findRoot(i)] = findRoot(other);
            }
        }
    }

    // Walking the boundary: arriving at `v` along a dart with kept material on
    // its left, leave along the far side of the *same* fan of kept cells around
    // `v`. Rotating from one edge of a counterclockwise triangle to the next
    // turns clockwise around the shared vertex, so following the triangles until
    // one is dropped stops exactly at the fan's other edge. This is what keeps a
    // pinch vertex, where the fan comes apart, from joining what meets there.
    const auto nextDart = [&](const Dart& dart) {
        std::size_t current = leftOf.at(dart);
        for (;;) {
            const auto& triangle = triangles[current];
            int k = 0;
            while (!(ExactPoint(triangle.get(k)) == dart.second)) {
                ++k;
            }
            const ExactPoint ahead(triangle.get(k + 1));
            const std::size_t other = across(dart.second, ahead);
            if (other == none || !keep[other]) {
                return Dart(dart.second, ahead);
            }
            current = other;
        }
    };

    std::map<std::size_t, std::vector<std::vector<ExactPoint>>> ringsOfPiece;
    std::set<Dart> walked;
    for (const Dart& start : boundary) {
        if (walked.count(start) != 0) {
            continue;
        }
        std::vector<ExactPoint> walk;
        Dart dart = start;
        do {
            walked.insert(dart);
            walk.push_back(dart.first);
            dart = nextDart(dart);
        } while (dart != start);
        splitWalkIntoRings(walk, ringsOfPiece[findRoot(leftOf.at(start))]);
    }

    const auto convert = [](const std::vector<ExactPoint>& ring) {
        std::vector<ResultPoint> converted;
        converted.reserve(ring.size());
        for (const ExactPoint& vertex : ring) {
            converted.emplace_back(vertex);
        }
        return ResultPolygon(std::move(converted));
    };

    for (auto& entry : ringsOfPiece) {
        std::vector<ExactPolygon> outers;
        std::vector<ExactPolygon> holes;
        for (std::vector<ExactPoint>& ring : entry.second) {
            dropCollinearRingVertices(ring);
            const ExactNumber twiceArea = twiceSignedRingArea(ring);
            if (twiceArea > ExactNumber(0)) {
                outers.emplace_back(std::move(ring));
            } else if (twiceArea < ExactNumber(0)) {
                holes.emplace_back(std::move(ring));
            }
        }
        if (outers.empty()) {
            continue;
        }

        // An edge-connected piece has a single outer ring, so its holes have
        // nowhere else to go. The loop below is the general form, kept as a
        // defence rather than for a case anyone can exhibit: each hole would go
        // to the smallest outer ring holding it, decided by a witness strictly
        // inside the hole (the rings meet at most along their boundaries, so the
        // closed containment is unambiguous).
        std::vector<std::vector<ResultPolygon>> holesOfOuter(outers.size());
        for (const ExactPolygon& hole : holes) {
            std::size_t owner = 0;
            if (outers.size() > 1) {
                const ExactPoint witness = hole.template pointInside<ExactNumber>();
                std::size_t best = none;
                for (std::size_t i = 0; i < outers.size(); ++i) {
                    if (outers[i].contains(witness) &&
                        (best == none || outers[i].twiceArea() < outers[best].twiceArea())) {
                        best = i;
                    }
                }
                owner = best == none ? 0 : best;
            }
            holesOfOuter[owner].push_back(convert(hole.vertices()));
        }
        for (std::size_t i = 0; i < outers.size(); ++i) {
            result.emplace_back(convert(outers[i].vertices()), std::move(holesOfOuter[i]));
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

}  // namespace detail

// Out-of-line: difference is declared in shape/polygon.hpp and
// shape/polygonwithholes.hpp, which precede this header in the layering, but it
// can only be defined once Triangulation is visible.

template <class PointType_, class TLabel>
template <class ResultNumber, PolygonConcept OtherPolygon>
std::vector<PolygonWithHoles<Point<ResultNumber, typename PointType_::LabelType>>>
Polygon<PointType_, TLabel>::difference(const OtherPolygon& other) const {
    return detail::regularizedDifference<Point<ResultNumber, typename PointType_::LabelType>>(*this,
                                                                                              other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, ConvexConcept OtherConvex>
std::vector<PolygonWithHoles<Point<ResultNumber, typename PointType_::LabelType>>>
Polygon<PointType_, TLabel>::difference(const OtherConvex& other) const {
    return this->template difference<ResultNumber>(other.asPolygon());
}

template <class PointType_, class TLabel>
template <class ResultNumber, TriangleConcept OtherTriangle>
std::vector<PolygonWithHoles<Point<ResultNumber, typename PointType_::LabelType>>>
Polygon<PointType_, TLabel>::difference(const OtherTriangle& other) const {
    return this->template difference<ResultNumber>(other.asConvex());
}

template <class PointType_, class TLabel>
template <class ResultNumber, RectangleConcept OtherRectangle>
std::vector<PolygonWithHoles<Point<ResultNumber, typename PointType_::LabelType>>>
Polygon<PointType_, TLabel>::difference(const OtherRectangle& other) const {
    return this->template difference<ResultNumber>(other.asConvex());
}

template <class PointType_, class TLabel>
template <class ResultNumber, PolygonWithHolesConcept OtherRegion>
std::vector<PolygonWithHoles<Point<ResultNumber, typename PointType_::LabelType>>>
Polygon<PointType_, TLabel>::difference(const OtherRegion& other) const {
    return detail::regularizedDifference<Point<ResultNumber, typename PointType_::LabelType>>(*this,
                                                                                              other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, PolygonConcept OtherPolygon>
std::vector<PolygonWithHoles<Point<ResultNumber, typename PointType_::LabelType>>>
PolygonWithHoles<PointType_, TLabel>::difference(const OtherPolygon& other) const {
    return detail::regularizedDifference<Point<ResultNumber, typename PointType_::LabelType>>(*this,
                                                                                              other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, ConvexConcept OtherConvex>
std::vector<PolygonWithHoles<Point<ResultNumber, typename PointType_::LabelType>>>
PolygonWithHoles<PointType_, TLabel>::difference(const OtherConvex& other) const {
    return this->template difference<ResultNumber>(other.asPolygon());
}

template <class PointType_, class TLabel>
template <class ResultNumber, TriangleConcept OtherTriangle>
std::vector<PolygonWithHoles<Point<ResultNumber, typename PointType_::LabelType>>>
PolygonWithHoles<PointType_, TLabel>::difference(const OtherTriangle& other) const {
    return this->template difference<ResultNumber>(other.asConvex());
}

template <class PointType_, class TLabel>
template <class ResultNumber, RectangleConcept OtherRectangle>
std::vector<PolygonWithHoles<Point<ResultNumber, typename PointType_::LabelType>>>
PolygonWithHoles<PointType_, TLabel>::difference(const OtherRectangle& other) const {
    return this->template difference<ResultNumber>(other.asConvex());
}

template <class PointType_, class TLabel>
template <class ResultNumber, PolygonWithHolesConcept OtherRegion>
std::vector<PolygonWithHoles<Point<ResultNumber, typename PointType_::LabelType>>>
PolygonWithHoles<PointType_, TLabel>::difference(const OtherRegion& other) const {
    return detail::regularizedDifference<Point<ResultNumber, typename PointType_::LabelType>>(*this,
                                                                                              other);
}

}  // namespace pgl
