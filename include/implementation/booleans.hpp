#pragma once

#include "algorithm/arrangement.hpp"

/**
 * @file booleans.hpp
 * @brief Regularized boolean operations on closed polygonal regions.
 *
 * The four operations — difference `A ∖ B`, union `A ∪ B`, intersection
 * `A ∩ B` and symmetric difference `A △ B` — are one algorithm run with four
 * per-cell tests. The difference came first, and it is the one whose result
 * genuinely needs @ref PolygonWithHoles for the simplest of inputs: removing a
 * polygon from the middle of another one leaves a region with a hole, which no
 * other shape can express.
 *
 * The other three need a region too, though it takes a holed operand to see it:
 *
 * - a **union** closes a hole into being whenever the operands wrap round
 *   between them, as a `U` united with the bar that caps it;
 * - an **intersection** keeps the holes of a holed operand, so it needs a
 *   region whenever one of its operands is one. That does not contradict
 *   @ref Polygon::intersection(const OtherPolygon&) const returning plain
 *   polygons: the argument that no component of `A ∩ B` has a hole assumes both
 *   operands have a **connected complement**, which every shape in the library
 *   satisfies *except* a region with holes;
 * - a **symmetric difference** is the union of two differences, and inherits
 *   holes from both.
 *
 * What is computed is the **regularized** result, `closure` of the operation on
 * the open interiors: `closure(A° ∖ B)`, `closure(A° ∪ B°)`, `closure(A° ∩ B°)`
 * and `closure((A° ∖ B) ∪ (B° ∖ A))`. Lower-dimensional leftovers — a stretch
 * of boundary the operands share without either covering it, an isolated
 * contact point, a slit — have nowhere to go in a set of regions and are
 * dropped. That is the usual convention for boolean operations on solids.
 *
 * The engine is the cell decomposition of @ref detail::cellSeparates seen from
 * the other side: the @ref pgl::Arrangement of both boundaries cuts the plane
 * into faces on which membership in `A` and in `B` is constant, one witness
 * point per face decides whether it survives, and the boundary of the union of
 * the surviving faces is read straight off the arrangement — a halfedge is on
 * it exactly when the face to its left is kept and the face across it is not.
 * Only the per-cell test tells the four operations apart. Everything is exact —
 * the arrangement is built over rationals — and the result is converted to the
 * requested coordinate type only at the very end, so an integral answer comes
 * back integral however the intermediate crossings looked.
 */

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <map>
#include <ranges>
#include <type_traits>
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

// ringOrientation and splitWalkIntoRings, which the extraction below also
// uses, live beside the arrangement in algorithm/arrangement.hpp: they are
// what turns any cell complex's boundary walks into rings.

/**
 * @brief The union of the cells of an arrangement that @p keepWitnesses selects,
 *        as a set of regions.
 *
 * @p cuts must carry every point at which membership in the result can change —
 * the boundaries of all the operands involved, whatever they are. The
 * arrangement of those segments cuts the plane into faces on each of which
 * membership is constant, so one point strictly inside each face decides it.
 * Being strictly inside, the witness is on no boundary, which is why closed
 * containments answer the open question and the result comes out regularized.
 *
 * @p keepWitness is called once per face, on that point, and says whether the
 * face belongs to the result.
 *
 * Nothing here counts its operands: the whole engine sees only the cut segments
 * and the per-cell answer, so a boolean operation on two shapes
 * (@ref regularizedBoolean) and a union of arbitrarily many
 * (@ref regularizedUnionOf, which is what the Minkowski sum needs) are the same
 * call with a different classifier.
 *
 * The returned regions have pairwise disjoint interiors and their union is the
 * result. They are *not* nested: an island stranded inside a hole of the result
 * comes back as a region of its own, which is what a flat list of regions can
 * say (see decision (c): this library has no `PolygonSet`).
 *
 * The extraction is where the @ref Arrangement earns its place. A halfedge is on
 * the result's boundary exactly when the face to its left is kept and the face
 * across it is not, and the rotational order the DCEL already carries is what
 * walks from one boundary halfedge to the next — so a vertex where the result
 * pinches shut comes apart into two rings for free, with no fan to rebuild by
 * hand and no map keyed on rational points anywhere.
 *
 * Complexity: one arrangement of the cut segments, then one @p keepWitness call
 * per face and a linear pass over the halfedges.
 */
template <class ResultPoint, class ExactPoint, class KeepWitness>
std::vector<PolygonWithHoles<ResultPoint>> regularizedCells(
    const std::vector<Segment<ExactPoint>>& cuts, KeepWitness keepWitness) {
    using ExactNumber = typename ExactPoint::NumberType;
    using ExactSegment = Segment<ExactPoint>;
    using ExactPolygon = Polygon<ExactPoint>;
    using ResultPolygon = Polygon<ResultPoint>;
    constexpr std::size_t none = std::numeric_limits<std::size_t>::max();

    std::vector<PolygonWithHoles<ResultPoint>> result;
    if (cuts.empty()) {
        return result;  // no operand has an edge, so none has area
    }

    // A box strictly containing every operand, so that the region around them is
    // a face like any other rather than the unbounded one. The arrangement no
    // longer needs it — the unbounded face is never kept, every operand being
    // bounded — but it costs four edges and one classifier call, and keeping it
    // leaves the engine's behaviour exactly as it was.
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
    const std::array<ExactPoint, 4> corners{
        ExactPoint(loX - margin, loY - margin), ExactPoint(hiX + margin, loY - margin),
        ExactPoint(hiX + margin, hiY + margin), ExactPoint(loX - margin, hiY + margin)};
    std::vector<ExactSegment> segments = cuts;
    for (std::size_t i = 0; i < corners.size(); ++i) {
        segments.emplace_back(corners[i], corners[(i + 1) % corners.size()]);
    }

    const Arrangement<ExactPoint> arrangement(segments);

    // Each face is in the result or out of it as a whole: its interior meets no
    // boundary edge of any operand, so one witness point settles it. The witness
    // is strictly inside its face, hence on no boundary, which is why the closed
    // containments answer the open question here.
    std::vector<char> keep(arrangement.faceCount(), 0);
    for (const FaceId f : arrangement.faces()) {
        if (!arrangement.isUnbounded(f)) {
            keep[f.index()] = static_cast<char>(
                keepWitness(arrangement.template witness<ExactNumber>(f)));
        }
    }
    const auto isKept = [&](HalfedgeId h) { return keep[arrangement.face(h).index()] != 0; };

    // Kept faces sharing an edge are one piece of the result. Faces meeting at a
    // single vertex are not: the result pinches shut there, and the two sides
    // have to come back as two regions, since neither a polygon nor a region may
    // have a self-touching outer ring.
    std::vector<std::size_t> parent(arrangement.faceCount());
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
    std::vector<HalfedgeId> boundary;
    for (const HalfedgeId h : arrangement.halfedges()) {
        if (!isKept(h)) {
            continue;
        }
        if (isKept(arrangement.twin(h))) {
            parent[findRoot(arrangement.face(h).index())] =
                findRoot(arrangement.face(arrangement.twin(h)).index());
        } else {
            boundary.push_back(h);
        }
    }

    // Walking the boundary: from a halfedge with kept material on its left and
    // none across it, follow the face's own cycle, stepping over any edge whose
    // far side is kept too — that is the rotation around the shared vertex the
    // DCEL stores, and it stops at the far side of the same fan of kept faces.
    // An edge kept on both sides is interior to the result and is skipped, which
    // is what regularizes a slit away.
    const auto nextBoundary = [&](HalfedgeId h) {
        HalfedgeId ahead = arrangement.next(h);
        while (isKept(arrangement.twin(ahead))) {
            ahead = arrangement.next(arrangement.twin(ahead));
        }
        return ahead;
    };

    std::map<std::size_t, std::vector<std::vector<ExactPoint>>> ringsOfPiece;
    std::vector<char> walked(arrangement.halfedgeCount(), 0);
    for (const HalfedgeId start : boundary) {
        if (walked[start.index()] != 0) {
            continue;
        }
        std::vector<ExactPoint> walk;
        HalfedgeId h = start;
        do {
            walked[h.index()] = 1;
            walk.push_back(arrangement[arrangement.origin(h)]);
            h = nextBoundary(h);
        } while (h != start);
        splitWalkIntoRings(walk, ringsOfPiece[findRoot(arrangement.face(start).index())]);
    }

    // Converting an already canonical ring into the caller's coordinates keeps
    // it canonical whenever the conversion is the identity, and only then: an
    // integral result type truncates, which can reorder the vertices or flip the
    // ring, so that case is renormalized as usual.
    const auto convert = [](const std::vector<ExactPoint>& ring) {
        std::vector<ResultPoint> converted;
        converted.reserve(ring.size());
        for (const ExactPoint& vertex : ring) {
            converted.emplace_back(vertex);
        }
        constexpr bool exact = std::is_same_v<ResultPoint, ExactPoint>;
        return ResultPolygon(std::move(converted), /*trusted=*/exact);
    };

    for (auto& entry : ringsOfPiece) {
        std::vector<ExactPolygon> outers;
        std::vector<ExactPolygon> holes;
        for (std::vector<ExactPoint>& ring : entry.second) {
            dropCollinearRingVertices(ring);
            const int orientation = ringOrientation(ring);
            if (orientation == 0) {
                continue;  // a ring bounding no area is not part of any region
            }
            // The walks come out of a planar subdivision, so every ring here is
            // simple and its orientation is one predicate away. Putting it into
            // canonical form by hand then keeps @ref Polygon from measuring it
            // all over again, which over rationals costs more than everything
            // else in this function put together.
            if (orientation < 0) {
                std::reverse(ring.begin(), ring.end());
            }
            std::rotate(ring.begin(), std::min_element(ring.begin(), ring.end()), ring.end());
            (orientation > 0 ? outers : holes).emplace_back(std::move(ring), /*trusted=*/true);
        }
        if (outers.empty()) {
            continue;
        }

        // An edge-connected piece has a single outer ring unless it pinches shut
        // at a vertex, where the walk above cuts it into one ring per side. Each
        // hole then goes to the smallest outer ring holding it, decided by a
        // witness strictly inside the hole (the rings meet at most along their
        // boundaries, so the closed containment is unambiguous).
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

/**
 * @brief The regularized boolean operation @p keepCell selects, as a set of
 *        regions.
 *
 * @p keepCell is called as `keepCell(inA, inB)` with the membership of one
 * witness point per cell of the arrangement of both boundaries, and says
 * whether that cell belongs to the result: `inA && !inB` is the difference,
 * `inA || inB` the union, `inA && inB` the intersection and `inA != inB` the
 * symmetric difference.
 *
 * Both operands must be bounded and polygonal, and neither may be
 * self-overlapping; nothing else is asked of them, so either may be a polygon,
 * a convex shape, a rectangle, a triangle or a region with holes.
 */
template <class ResultPoint, class ShapeA, class ShapeB, class KeepCell>
std::vector<PolygonWithHoles<ResultPoint>> regularizedBoolean(const ShapeA& a, const ShapeB& b,
                                                              KeepCell keepCell) {
    using ExactNumber = Exact1DNumber<typename ShapeA::NumberType, typename ShapeB::NumberType>;
    using ExactPoint = Point<ExactNumber>;

    std::vector<Segment<ExactPoint>> cuts;
    appendCutSegments<ExactPoint>(a, cuts);
    appendCutSegments<ExactPoint>(b, cuts);
    return regularizedCells<ResultPoint>(cuts, [&a, &b, &keepCell](const ExactPoint& witness) {
        return keepCell(a.contains(witness), b.contains(witness));
    });
}

/**
 * @brief The regularized union of arbitrarily many shapes, as a set of regions.
 *
 * One arrangement over all their boundaries settles the whole union, where
 * folding @ref regularizedUnion over the range would build one per step and
 * re-triangulate everything accumulated so far. That is what makes it the right
 * back end for a construction whose natural form is a union of many pieces —
 * the Minkowski sum of two non-convex shapes is one.
 */
template <class ResultPoint, class ShapeRange>
std::vector<PolygonWithHoles<ResultPoint>> regularizedUnionOf(const ShapeRange& shapes) {
    using ShapeType = std::ranges::range_value_t<ShapeRange>;
    using ShapeNumber = typename ShapeType::NumberType;
    using ExactNumber = Exact1DNumber<ShapeNumber, ShapeNumber>;
    using ExactPoint = Point<ExactNumber>;

    // A repeat contributes nothing: not to the union, not to the arrangement,
    // and not to any classification. It would only be tested again for every
    // face, so drop it once here rather than pay for it everywhere.
    std::vector<ShapeType> distinct(std::ranges::begin(shapes), std::ranges::end(shapes));
    std::sort(distinct.begin(), distinct.end());
    distinct.erase(std::unique(distinct.begin(), distinct.end()), distinct.end());

    std::vector<Segment<ExactPoint>> cuts;
    for (const ShapeType& shape : distinct) {
        appendCutSegments<ExactPoint>(shape, cuts);
    }

    // A linear scan, stopping at the first operand that covers the witness.
    // Indexing the operands in a ShapeTree was measured and is not worth it
    // here: the operands of the construction this exists for — the pairwise
    // convex sums of a Minkowski sum — tile one region and so overlap heavily,
    // which is the input a bounding-volume hierarchy prunes worst. Its node
    // boxes overlap as much as the operands do, a point query descends into most
    // of them, and the whole tree bought 1.25x on the largest shape-pair cell
    // while costing 13% at ordinary sizes. What this scan really wants is not a
    // better index but no query at all: coverage propagates across the
    // arrangement's edges, which know the operands they came from
    // (@ref Arrangement::originsOf), in O(E) integer work.
    return regularizedCells<ResultPoint>(cuts, [&distinct](const ExactPoint& witness) {
        return std::ranges::any_of(distinct,
                                   [&witness](const ShapeType& s) { return s.contains(witness); });
    });
}

/** @brief The regularized difference `closure(A° ∖ B)`, as a set of regions. */
template <class ResultPoint, class ShapeA, class ShapeB>
std::vector<PolygonWithHoles<ResultPoint>> regularizedDifference(const ShapeA& a, const ShapeB& b) {
    return regularizedBoolean<ResultPoint>(a, b, [](bool inA, bool inB) { return inA && !inB; });
}

/** @brief The regularized union `closure(A° ∪ B°)`, as a set of regions. */
template <class ResultPoint, class ShapeA, class ShapeB>
std::vector<PolygonWithHoles<ResultPoint>> regularizedUnion(const ShapeA& a, const ShapeB& b) {
    return regularizedBoolean<ResultPoint>(a, b, [](bool inA, bool inB) { return inA || inB; });
}

/**
 * @brief The regularized intersection `closure(A° ∩ B°)`, as a set of regions.
 *
 * Bounding boxes settle the *box-separated* case outright. Interiors are
 * monotone, so `A° ⊆ bbox(A)°`, and operands whose boxes share no area
 * intersect in nothing that survives regularization — including the case where
 * one of them has no area at all, whose box is degenerate. Worth the test
 * because the engine's first step is quadratic in the boundary size *before*
 * anything has been triangulated, so the alternative is to pay for the whole
 * arrangement to discover that no cell is kept.
 *
 * This is narrower than "the operands are disjoint", and deliberately so: two
 * interleaved combs share no area at all while their boxes coincide, and that
 * pair still pays for the full arrangement. Cheap separation tests are all this
 * shortcut can afford. Only the intersection shortcuts even this way — a
 * difference or a union of separated operands still has to return one.
 */
template <class ResultPoint, class ShapeA, class ShapeB>
std::vector<PolygonWithHoles<ResultPoint>> regularizedIntersection(const ShapeA& a, const ShapeB& b) {
    if (!a.bbox().interiorsIntersect(b.bbox())) {
        return {};
    }
    return regularizedBoolean<ResultPoint>(a, b, [](bool inA, bool inB) { return inA && inB; });
}

/**
 * @brief The regularized symmetric difference `closure((A° ∖ B) ∪ (B° ∖ A))`,
 *        as a set of regions.
 */
template <class ResultPoint, class ShapeA, class ShapeB>
std::vector<PolygonWithHoles<ResultPoint>> regularizedSymmetricDifference(const ShapeA& a,
                                                                          const ShapeB& b) {
    return regularizedBoolean<ResultPoint>(a, b, [](bool inA, bool inB) { return inA != inB; });
}

}  // namespace detail

// Out-of-line: the boolean operations are declared in shape/polygon.hpp and
// shape/polygonwithholes.hpp, which precede this header in the layering, but
// they can only be defined once Triangulation is visible.

template <class PointType_, class TLabel>
template <class ResultNumber>
std::vector<PolygonWithHoles<Point<ResultNumber, typename PointType_::LabelType>>>
PolygonWithHoles<PointType_, TLabel>::regularized() const {
    using ResultPoint = Point<ResultNumber, typename PointType_::LabelType>;
    // Nothing with area, so closure(A°) is empty and has no pieces at all.
    if (isDegenerate()) {
        return {};
    }
    // Already the closure of its own interior: rebuilding it through the
    // arrangement could only cost time and shed collinear vertices.
    if (isRegular()) {
        return {PolygonWithHoles<ResultPoint>(*this)};
    }
    // One arrangement over this one boundary. Every cell of it is inside the
    // region or outside it, and a slit — having no cell of its own — survives
    // in neither.
    const std::array<PolygonWithHoles, 1> self{*this};
    return detail::regularizedUnionOf<ResultPoint>(self);
}

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

template <class PointType_, class TLabel>
template <class ResultNumber, PolygonConcept OtherPolygon>
std::vector<PolygonWithHoles<Point<ResultNumber, typename PointType_::LabelType>>>
Polygon<PointType_, TLabel>::unionWith(const OtherPolygon& other) const {
    return detail::regularizedUnion<Point<ResultNumber, typename PointType_::LabelType>>(*this, other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, ConvexConcept OtherConvex>
std::vector<PolygonWithHoles<Point<ResultNumber, typename PointType_::LabelType>>>
Polygon<PointType_, TLabel>::unionWith(const OtherConvex& other) const {
    return this->template unionWith<ResultNumber>(other.asPolygon());
}

template <class PointType_, class TLabel>
template <class ResultNumber, TriangleConcept OtherTriangle>
std::vector<PolygonWithHoles<Point<ResultNumber, typename PointType_::LabelType>>>
Polygon<PointType_, TLabel>::unionWith(const OtherTriangle& other) const {
    return this->template unionWith<ResultNumber>(other.asConvex());
}

template <class PointType_, class TLabel>
template <class ResultNumber, RectangleConcept OtherRectangle>
std::vector<PolygonWithHoles<Point<ResultNumber, typename PointType_::LabelType>>>
Polygon<PointType_, TLabel>::unionWith(const OtherRectangle& other) const {
    return this->template unionWith<ResultNumber>(other.asConvex());
}

template <class PointType_, class TLabel>
template <class ResultNumber, PolygonWithHolesConcept OtherRegion>
std::vector<PolygonWithHoles<Point<ResultNumber, typename PointType_::LabelType>>>
Polygon<PointType_, TLabel>::unionWith(const OtherRegion& other) const {
    return detail::regularizedUnion<Point<ResultNumber, typename PointType_::LabelType>>(*this, other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, PolygonConcept OtherPolygon>
std::vector<PolygonWithHoles<Point<ResultNumber, typename PointType_::LabelType>>>
PolygonWithHoles<PointType_, TLabel>::unionWith(const OtherPolygon& other) const {
    return detail::regularizedUnion<Point<ResultNumber, typename PointType_::LabelType>>(*this, other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, ConvexConcept OtherConvex>
std::vector<PolygonWithHoles<Point<ResultNumber, typename PointType_::LabelType>>>
PolygonWithHoles<PointType_, TLabel>::unionWith(const OtherConvex& other) const {
    return this->template unionWith<ResultNumber>(other.asPolygon());
}

template <class PointType_, class TLabel>
template <class ResultNumber, TriangleConcept OtherTriangle>
std::vector<PolygonWithHoles<Point<ResultNumber, typename PointType_::LabelType>>>
PolygonWithHoles<PointType_, TLabel>::unionWith(const OtherTriangle& other) const {
    return this->template unionWith<ResultNumber>(other.asConvex());
}

template <class PointType_, class TLabel>
template <class ResultNumber, RectangleConcept OtherRectangle>
std::vector<PolygonWithHoles<Point<ResultNumber, typename PointType_::LabelType>>>
PolygonWithHoles<PointType_, TLabel>::unionWith(const OtherRectangle& other) const {
    return this->template unionWith<ResultNumber>(other.asConvex());
}

template <class PointType_, class TLabel>
template <class ResultNumber, PolygonWithHolesConcept OtherRegion>
std::vector<PolygonWithHoles<Point<ResultNumber, typename PointType_::LabelType>>>
PolygonWithHoles<PointType_, TLabel>::unionWith(const OtherRegion& other) const {
    return detail::regularizedUnion<Point<ResultNumber, typename PointType_::LabelType>>(*this, other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, PolygonConcept OtherPolygon>
std::vector<PolygonWithHoles<Point<ResultNumber, typename PointType_::LabelType>>>
Polygon<PointType_, TLabel>::symmetricDifference(const OtherPolygon& other) const {
    return detail::regularizedSymmetricDifference<Point<ResultNumber, typename PointType_::LabelType>>(*this, other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, ConvexConcept OtherConvex>
std::vector<PolygonWithHoles<Point<ResultNumber, typename PointType_::LabelType>>>
Polygon<PointType_, TLabel>::symmetricDifference(const OtherConvex& other) const {
    return this->template symmetricDifference<ResultNumber>(other.asPolygon());
}

template <class PointType_, class TLabel>
template <class ResultNumber, TriangleConcept OtherTriangle>
std::vector<PolygonWithHoles<Point<ResultNumber, typename PointType_::LabelType>>>
Polygon<PointType_, TLabel>::symmetricDifference(const OtherTriangle& other) const {
    return this->template symmetricDifference<ResultNumber>(other.asConvex());
}

template <class PointType_, class TLabel>
template <class ResultNumber, RectangleConcept OtherRectangle>
std::vector<PolygonWithHoles<Point<ResultNumber, typename PointType_::LabelType>>>
Polygon<PointType_, TLabel>::symmetricDifference(const OtherRectangle& other) const {
    return this->template symmetricDifference<ResultNumber>(other.asConvex());
}

template <class PointType_, class TLabel>
template <class ResultNumber, PolygonWithHolesConcept OtherRegion>
std::vector<PolygonWithHoles<Point<ResultNumber, typename PointType_::LabelType>>>
Polygon<PointType_, TLabel>::symmetricDifference(const OtherRegion& other) const {
    return detail::regularizedSymmetricDifference<Point<ResultNumber, typename PointType_::LabelType>>(*this, other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, PolygonConcept OtherPolygon>
std::vector<PolygonWithHoles<Point<ResultNumber, typename PointType_::LabelType>>>
PolygonWithHoles<PointType_, TLabel>::symmetricDifference(const OtherPolygon& other) const {
    return detail::regularizedSymmetricDifference<Point<ResultNumber, typename PointType_::LabelType>>(*this, other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, ConvexConcept OtherConvex>
std::vector<PolygonWithHoles<Point<ResultNumber, typename PointType_::LabelType>>>
PolygonWithHoles<PointType_, TLabel>::symmetricDifference(const OtherConvex& other) const {
    return this->template symmetricDifference<ResultNumber>(other.asPolygon());
}

template <class PointType_, class TLabel>
template <class ResultNumber, TriangleConcept OtherTriangle>
std::vector<PolygonWithHoles<Point<ResultNumber, typename PointType_::LabelType>>>
PolygonWithHoles<PointType_, TLabel>::symmetricDifference(const OtherTriangle& other) const {
    return this->template symmetricDifference<ResultNumber>(other.asConvex());
}

template <class PointType_, class TLabel>
template <class ResultNumber, RectangleConcept OtherRectangle>
std::vector<PolygonWithHoles<Point<ResultNumber, typename PointType_::LabelType>>>
PolygonWithHoles<PointType_, TLabel>::symmetricDifference(const OtherRectangle& other) const {
    return this->template symmetricDifference<ResultNumber>(other.asConvex());
}

template <class PointType_, class TLabel>
template <class ResultNumber, PolygonWithHolesConcept OtherRegion>
std::vector<PolygonWithHoles<Point<ResultNumber, typename PointType_::LabelType>>>
PolygonWithHoles<PointType_, TLabel>::symmetricDifference(const OtherRegion& other) const {
    return detail::regularizedSymmetricDifference<Point<ResultNumber, typename PointType_::LabelType>>(*this, other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, PolygonConcept OtherPolygon>
std::vector<PolygonWithHoles<Point<ResultNumber, typename PointType_::LabelType>>>
PolygonWithHoles<PointType_, TLabel>::intersection(const OtherPolygon& other) const {
    return detail::regularizedIntersection<Point<ResultNumber, typename PointType_::LabelType>>(*this, other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, ConvexConcept OtherConvex>
std::vector<PolygonWithHoles<Point<ResultNumber, typename PointType_::LabelType>>>
PolygonWithHoles<PointType_, TLabel>::intersection(const OtherConvex& other) const {
    return this->template intersection<ResultNumber>(other.asPolygon());
}

template <class PointType_, class TLabel>
template <class ResultNumber, TriangleConcept OtherTriangle>
std::vector<PolygonWithHoles<Point<ResultNumber, typename PointType_::LabelType>>>
PolygonWithHoles<PointType_, TLabel>::intersection(const OtherTriangle& other) const {
    return this->template intersection<ResultNumber>(other.asConvex());
}

template <class PointType_, class TLabel>
template <class ResultNumber, RectangleConcept OtherRectangle>
std::vector<PolygonWithHoles<Point<ResultNumber, typename PointType_::LabelType>>>
PolygonWithHoles<PointType_, TLabel>::intersection(const OtherRectangle& other) const {
    return this->template intersection<ResultNumber>(other.asConvex());
}

template <class PointType_, class TLabel>
template <class ResultNumber, PolygonWithHolesConcept OtherRegion>
std::vector<PolygonWithHoles<Point<ResultNumber, typename PointType_::LabelType>>>
PolygonWithHoles<PointType_, TLabel>::intersection(const OtherRegion& other) const {
    return detail::regularizedIntersection<Point<ResultNumber, typename PointType_::LabelType>>(*this, other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, HalfplaneIntersectionConcept OtherIntersection>
std::vector<PolygonWithHoles<Point<ResultNumber, typename PointType_::LabelType>>>
PolygonWithHoles<PointType_, TLabel>::intersection(const OtherIntersection& other) const {
    using ExactNumber = detail::region_exact_number_t<typename OtherIntersection::NumberType>;
    // Neither a region without area nor a half-plane intersection without
    // interior can contribute to closure(A° ∩ B°).
    if (isDegenerate() || other.isDegenerate()) {
        return {};
    }
    // The clip only has to preserve A ∩ B, and A lies strictly inside the box.
    const auto clipped = detail::regionClippedToBox(other, bbox());
    if (clipped.isDegenerate()) {
        return {};
    }
    return this->template intersection<ResultNumber>(clipped.template asConvex<ExactNumber>());
}

template <class PointType_, class TLabel>
template <class ResultNumber, HalfplaneConcept OtherHalfplane>
std::vector<PolygonWithHoles<Point<ResultNumber, typename PointType_::LabelType>>>
PolygonWithHoles<PointType_, TLabel>::intersection(const OtherHalfplane& other) const {
    return this->template intersection<ResultNumber>(other.asHalfplaneIntersection());
}

}  // namespace pgl
