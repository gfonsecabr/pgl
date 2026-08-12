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
#include <cassert>
#include <cstdint>
#include <cstddef>
#include <limits>
#include <map>
#include <ranges>
#include <type_traits>
#include <variant>
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
 * @brief Extracts the union of the selected cells of an arrangement as regions.
 *
 * @p keep holds one membership decision per face. The unbounded face must be
 * excluded. The caller may obtain those decisions from witnesses or by
 * propagating membership across the arrangement's edge history.
 *
 * The result is a @ref PolygonSet: its components have pairwise disjoint
 * interiors, share no stretch of edge, and their union is the answer. They are
 * *not* nested — an island stranded inside a hole of the result comes back as a
 * component of its own, which is what a flat set of regions says.
 *
 * The extraction is where the @ref Arrangement earns its place. A halfedge is on
 * the result's boundary exactly when the face to its left is kept and the face
 * across it is not, and the rotational order the DCEL already carries is what
 * walks from one boundary halfedge to the next — so a vertex where the result
 * pinches shut comes apart into two rings for free, with no fan to rebuild by
 * hand and no map keyed on rational points anywhere.
 *
 * Complexity: linear in the arrangement's faces and halfedges.
 */
template <class ResultPoint, class ExactPoint>
PolygonSet<ResultPoint> regularizedCellsFromKeep(
    const Arrangement<ExactPoint>& arrangement, const std::vector<char>& keep) {
    using HalfedgeId = typename Arrangement<ExactPoint>::HalfedgeId;
    using ExactNumber = typename ExactPoint::NumberType;
    using ExactPolygon = Polygon<ExactPoint>;
    using ResultPolygon = Polygon<ResultPoint>;
    constexpr std::size_t none = std::numeric_limits<std::size_t>::max();

    std::vector<PolygonWithHoles<ResultPoint>> result;
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
    for (std::uint32_t i = 0; i < arrangement.halfedgeCount(); ++i) {
        const HalfedgeId h(i);
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
            walk.push_back(arrangement[arrangement.source(h)]);
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
    // The pieces have pairwise disjoint interiors and share no stretch of edge —
    // an edge kept on both sides is interior to the result and never reaches the
    // boundary walk — so they are a canonical PolygonSet once sorted, and the
    // set adopts them without measuring a single area again.
    std::sort(result.begin(), result.end());
    return PolygonSet<ResultPoint>(std::move(result), /*trusted=*/true);
}

/**
 * @brief The arrangement of the cuts together with a box strictly containing
 *        them.
 *
 * The box makes the region around the operands a face like any other rather
 * than the unbounded one. The arrangement no longer needs it — the unbounded
 * face is never kept, every operand being bounded — but it costs four edges and
 * one classifier call, and keeping it leaves the engine's behaviour exactly as
 * it was. Nothing on the box can belong to any answer: it misses every operand.
 *
 * @pre @p cuts is not empty.
 */
template <class ExactPoint>
Arrangement<ExactPoint> framedArrangement(const std::vector<Segment<ExactPoint>>& cuts) {
    using ExactNumber = typename ExactPoint::NumberType;
    using ExactSegment = Segment<ExactPoint>;

    assert(!cuts.empty());
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
    return Arrangement<ExactPoint>(segments);
}

/**
 * @brief Builds an arrangement of cuts, classifies its bounded faces by witness,
 *        then extracts the selected cells.
 */
template <class ResultPoint, class ExactPoint, class KeepWitness>
PolygonSet<ResultPoint> regularizedCells(
    const std::vector<Segment<ExactPoint>>& cuts, KeepWitness keepWitness) {
    using ExactNumber = typename ExactPoint::NumberType;

    if (cuts.empty()) {
        return {};  // no operand has an edge, so none has area
    }

    const Arrangement<ExactPoint> arrangement = framedArrangement(cuts);
    using FaceId = typename Arrangement<ExactPoint>::FaceId;
    std::vector<char> keep(arrangement.faceCount(), 0);
    for (std::uint32_t i = 0; i < arrangement.faceCount(); ++i) {
        const FaceId f(i);
        if (!arrangement.isUnbounded(f)) {
            keep[f.index()] = static_cast<char>(
                keepWitness(arrangement.template witness<ExactNumber>(f)));
        }
    }
    return regularizedCellsFromKeep<ResultPoint>(arrangement, keep);
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
 *
 * An operand **without area is read as the empty set**, and both halves of that
 * matter. It is what regularization means — the result is `closure(A° op B°)`,
 * and a shape with empty interior contributes nothing to it. And it is what
 * keeps the cell classification sound: a zero-area operand has no edge, so
 * @ref appendCutSegments contributes no cut for it and it does not subdivide the
 * cells, yet `contains` still answers true on the points it covers. One witness
 * landing on it would then classify a whole cell by a single point of a set that
 * covers no area — which is exactly how `A ∖ point` and `A △ point` came back
 * empty instead of `A` when the arrangement happened to pick that point as the
 * witness for A's interior.
 */
template <class ResultPoint, class ShapeA, class ShapeB, class KeepCell>
PolygonSet<ResultPoint> regularizedBoolean(const ShapeA& a, const ShapeB& b, KeepCell keepCell) {
    using ExactNumber = Exact1DNumber<typename ShapeA::NumberType, typename ShapeB::NumberType>;
    using ExactPoint = Point<ExactNumber>;

    const bool aHasArea = !a.isDegenerate();
    const bool bHasArea = !b.isDegenerate();

    std::vector<Segment<ExactPoint>> cuts;
    appendCutSegments<ExactPoint>(a, cuts);
    appendCutSegments<ExactPoint>(b, cuts);
    return regularizedCells<ResultPoint>(
        cuts, [&a, &b, aHasArea, bHasArea, &keepCell](const ExactPoint& witness) {
            return keepCell(aHasArea && a.contains(witness), bHasArea && b.contains(witness));
        });
}

/**
 * @brief The regularized union of shapes each of whose boundaries is a disjoint
 *        set of simple closed rings, classified by propagated parity.
 *
 * Crossing a ring toggles membership in the shape that carries it, so a face's
 * membership is its neighbour's with the crossed edge's origins flipped, and one
 * depth-first walk of the face adjacency graph settles every face in `O(E)` bit
 * flips — where a witness scan costs one exact containment query per face and
 * per shape. Parity reads insideness off a ring set only because the rings are
 * disjoint: a region's outer ring counts once and a hole twice, which is exactly
 * `inside outer and outside every hole`.
 *
 * @pre No two boundary edges of the same shape overlap. A stretch two rings
 *      cover between them — a slit — reaches @ref Arrangement::originsOf as a
 *      single origin rather than two, so crossing it would toggle once and
 *      report the far side as outside. @ref regularizedUnionOf's caller is what
 *      rules that out; see its `simpleBoundaries` parameter.
 */
template <class ResultPoint, class ShapeType>
PolygonSet<ResultPoint> regularizedUnionByCoverage(
    const std::vector<ShapeType>& distinct) {
    using ShapeNumber = typename ShapeType::NumberType;
    using ExactNumber = Exact1DNumber<ShapeNumber, ShapeNumber>;
    using ExactPoint = Point<ExactNumber>;

    // Start outside every piece in the unbounded face, then propagate those
    // parity bits across the arrangement's face adjacency graph.
    const Arrangement<ExactPoint> arrangement(distinct);
    using HalfedgeId = typename Arrangement<ExactPoint>::HalfedgeId;
    using FaceId = typename Arrangement<ExactPoint>::FaceId;
    const std::size_t faceCount = arrangement.faceCount();
    const std::size_t pieceCount = distinct.size();
    constexpr std::size_t wordBits = 64;
    const std::size_t words = (pieceCount + wordBits - 1) / wordBits;

    // The halfedges bounding each face, as one pair of arrays rather than a
    // vector per face: the face count runs to hundreds of thousands here, and
    // that many separate allocations costs more than the traversal.
    std::vector<std::uint32_t> faceEdgeBegin(faceCount + 1, 0);
    for (std::uint32_t i = 0; i < arrangement.halfedgeCount(); ++i) {
        const HalfedgeId h(i);
        ++faceEdgeBegin[arrangement.face(h).index() + 1];
    }
    for (std::size_t i = 0; i < faceCount; ++i) {
        faceEdgeBegin[i + 1] += faceEdgeBegin[i];
    }
    std::vector<std::uint32_t> faceEdge(arrangement.halfedgeCount(), 0);
    {
        std::vector<std::uint32_t> cursor(faceEdgeBegin.begin(), faceEdgeBegin.end() - 1);
        for (std::uint32_t i = 0; i < arrangement.halfedgeCount(); ++i) {
            const HalfedgeId h(i);
            faceEdge[cursor[arrangement.face(h).index()]++] = h.index();
        }
    }

    // One shared membership word set, not one per face. Walking a spanning
    // tree of the face adjacency graph depth first, a face's membership is
    // its parent's with the crossed edge's origins toggled, so the descent
    // toggles them and the return toggles them back. The flip is its own
    // inverse in the coverage count too — a bit that was clear counts one
    // more piece, a bit that was set one fewer — so both restore exactly.
    // Storing membership per face instead would be faces x pieces bits,
    // nearly half a gigabyte on the largest shape-pair cell.
    std::vector<std::uint64_t> membership(words, 0);
    std::size_t covered = 0;
    const auto crossEdge = [&](std::uint32_t halfedge) {
        for (const std::uint32_t origin : arrangement.originsOf(HalfedgeId(halfedge))) {
            const std::size_t word = origin / wordBits;
            const std::uint64_t mask = std::uint64_t{1} << (origin % wordBits);
            if ((membership[word] & mask) == 0) {
                ++covered;
            } else {
                --covered;
            }
            membership[word] ^= mask;
        }
    };

    struct Frame {
        std::uint32_t face;
        std::uint32_t cursor;   // next index into faceEdge
        std::uint32_t entered;  // halfedge crossed to get here, or noEdge at the root
    };
    constexpr std::uint32_t noEdge = ~std::uint32_t{0};

    std::vector<std::size_t> coverage(faceCount, 0);
    std::vector<char> seen(faceCount, 0);
    std::vector<Frame> stack;
    // Face 0 is the unbounded one, which lies outside every piece.
    stack.push_back(Frame{0, faceEdgeBegin[0], noEdge});
    seen[0] = 1;

    while (!stack.empty()) {
        Frame& top = stack.back();
        if (top.cursor == faceEdgeBegin[top.face + 1]) {
            if (top.entered != noEdge) {
                crossEdge(top.entered);  // undo, restoring the parent's state
            }
            stack.pop_back();
            continue;
        }
        const std::uint32_t h = faceEdge[top.cursor++];
        const std::uint32_t next = arrangement.face(arrangement.twin(HalfedgeId(h))).index();
        if (seen[next] != 0) {
            continue;
        }
        seen[next] = 1;
        crossEdge(h);
        coverage[next] = covered;
        // `top` may dangle after this, so nothing above may be used again.
        stack.push_back(Frame{next, faceEdgeBegin[next], h});
    }

    assert(covered == 0);  // every descent undone
    assert(std::ranges::all_of(seen, [](char value) { return value != 0; }));
    std::vector<char> keep(faceCount, 0);
    for (std::uint32_t i = 0; i < arrangement.faceCount(); ++i) {
        const FaceId f(i);
        keep[f.index()] = static_cast<char>(coverage[f.index()] != 0);
    }
    return regularizedCellsFromKeep<ResultPoint>(arrangement, keep);
}

/**
 * @brief The regularized union of arbitrarily many shapes, as a set of regions.
 *
 * One arrangement over all their boundaries settles the whole union, where
 * folding @ref regularizedUnion over the range would build one per step and
 * re-triangulate everything accumulated so far. That is what makes it the right
 * back end for a construction whose natural form is a union of many pieces —
 * the Minkowski sum of two non-convex shapes is one.
 *
 * @param shapes The pieces to unite.
 * @param simpleBoundaries Set when no two boundary edges of the *same* piece
 *        overlap, which lets @ref regularizedUnionByCoverage classify the faces
 *        instead of the witness scan below. A @ref Convex is such a piece by
 *        construction and never has to say so; a @ref Polygon is one whenever it
 *        meets its own simplicity precondition, and a @ref PolygonWithHoles is
 *        one exactly when it carries no slit — which is why this is the caller's
 *        to assert and not a property read off the type.
 */
template <class ResultPoint, class ShapeRange>
PolygonSet<ResultPoint> regularizedUnionOf(const ShapeRange& shapes,
                                           bool simpleBoundaries = false) {
    using ShapeType = std::ranges::range_value_t<ShapeRange>;

    // What survives to be unioned. A shape with no interior contributes nothing
    // to `closure(union of interiors)`, so dropping it cannot change the result
    // on either path below — and the parity argument on the covered one *needs*
    // it gone: such a boundary is traversed twice, but the two traversals
    // coincide and the arrangement merges them into a single edge carrying that
    // origin once, so crossing it would toggle an odd number of times and report
    // the far side as inside. A repeat contributes nothing either, and would be
    // tested again for every face it covers. Both go before the sort, which then
    // has less to order.
    std::vector<ShapeType> distinct(std::ranges::begin(shapes), std::ranges::end(shapes));
    std::erase_if(distinct, [](const ShapeType& shape) { return shape.isDegenerate(); });
    std::sort(distinct.begin(), distinct.end());
    distinct.erase(std::unique(distinct.begin(), distinct.end()), distinct.end());

    if (distinct.empty()) {
        return {};
    }

    if constexpr (is_convex_v<ShapeType>) {
        return regularizedUnionByCoverage<ResultPoint>(distinct);
    } else {
        using ShapeNumber = typename ShapeType::NumberType;
        using ExactPoint = Point<Exact1DNumber<ShapeNumber, ShapeNumber>>;

        if (simpleBoundaries) {
            return regularizedUnionByCoverage<ResultPoint>(distinct);
        }
        std::vector<Segment<ExactPoint>> cuts;
        for (const ShapeType& shape : distinct) {
            appendCutSegments<ExactPoint>(shape, cuts);
        }
        return regularizedCells<ResultPoint>(cuts, [&distinct](const ExactPoint& witness) {
            return std::ranges::any_of(
                distinct, [&witness](const ShapeType& shape) { return shape.contains(witness); });
        });
    }
}

/**
 * @brief The operand as something @ref appendCutSegments can walk.
 *
 * A rectangle, a triangle and a convex polygon each carry their boundary in
 * their own way, and the cell engine only knows how to ask a `Polygon` or a
 * region for its edges — which is why @ref pgl::PolygonWithHoles's operations
 * convert them one overload at a time. A set states its four operations once
 * over every operand, so it converts here instead.
 */
template <class OtherShape>
constexpr decltype(auto) booleanOperand(const OtherShape& other) {
    if constexpr (is_rectangle_v<OtherShape> || is_triangle_v<OtherShape> ||
                  is_convex_v<OtherShape>) {
        return other.asPolygon();
    } else {
        return (other);
    }
}

/** @brief The regularized difference `closure(A° ∖ B)`, as a set of regions. */
template <class ResultPoint, class ShapeA, class ShapeB>
PolygonSet<ResultPoint> regularizedDifference(const ShapeA& a, const ShapeB& b) {
    return regularizedBoolean<ResultPoint>(a, b, [](bool inA, bool inB) { return inA && !inB; });
}

/** @brief The regularized union `closure(A° ∪ B°)`, as a set of regions. */
template <class ResultPoint, class ShapeA, class ShapeB>
PolygonSet<ResultPoint> regularizedUnion(const ShapeA& a, const ShapeB& b) {
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
PolygonSet<ResultPoint> regularizedIntersection(const ShapeA& a, const ShapeB& b) {
    if (!a.bbox().interiorsIntersect(b.bbox())) {
        return {};
    }
    return regularizedBoolean<ResultPoint>(a, b, [](bool inA, bool inB) { return inA && inB; });
}

/**
 * @brief The literal (unregularized) intersection `A ∩ B` of two bounded
 *        polygonal operands, as its connected pieces.
 *
 * Where @ref regularizedIntersection answers `closure(A° ∩ B°)` and drops
 * everything of lower dimension, this answers the point set itself: the regions
 * the regularized form returns, *plus* the stretches of shared boundary neither
 * operand covers on both sides and the isolated points where the two boundaries
 * only touch. It is the counterpart, one dimension up in its operands, of
 * @ref pgl::Polygon::intersection(const OtherPolygon&) const — and the reason
 * its area pieces are regions rather than polygons is the one that header note
 * gives: a component of `A ∩ B` gains a hole exactly when an operand has one.
 *
 * The engine is the same cell decomposition the regularized operations use, read
 * for all three dimensions at once. In the @ref pgl::Arrangement of both
 * boundaries, membership in each operand is constant on every face, so
 *
 * - a **face** belongs to the answer when its witness lies in both operands, and
 *   the faces that do are exactly the regularized result;
 * - an **edge** contributes material no region piece already holds exactly when
 *   its midpoint lies in both operands and *neither* face beside it is kept —
 *   with one side kept the edge is on a region piece's boundary, with both it is
 *   interior to one;
 * - a **vertex** is an isolated contact point when it lies in both operands and
 *   neither a kept face nor such an edge touches it.
 *
 * The surviving edges are then assembled into polylines: the open strands are
 * peeled from their loose ends, and whatever closes up on itself comes back as a
 * polyline repeating its first vertex last. A strand hanging off a region piece
 * is a piece of its own, as it is for two polygons.
 *
 * Everything is computed over exact rationals and converted once, so an integral
 * answer comes back integral however the intermediate crossings looked.
 */
template <class ResultPoint, class ShapeA, class ShapeB>
std::vector<std::variant<ResultPoint, Polyline<ResultPoint>, PolygonWithHoles<ResultPoint>>>
literalIntersection(const ShapeA& a, const ShapeB& b) {
    using ExactNumber = Exact1DNumber<typename ShapeA::NumberType, typename ShapeB::NumberType>;
    using ExactPoint = Point<ExactNumber>;
    using ResultPolyline = Polyline<ResultPoint>;
    using ResultRegion = PolygonWithHoles<ResultPoint>;
    using Piece = std::variant<ResultPoint, ResultPolyline, ResultRegion>;

    std::vector<Piece> pieces;
    // Boxes that miss each other settle the whole answer, this time including
    // the boundaries: the boxes are closed, so a shared point of the operands
    // would be a shared point of them.
    if (a.empty() || b.empty() || !a.bbox().intersects(b.bbox())) {
        return pieces;
    }

    std::vector<Segment<ExactPoint>> cuts;
    appendCutSegments<ExactPoint>(a, cuts);
    const std::size_t cutsOfA = cuts.size();
    appendCutSegments<ExactPoint>(b, cuts);
    // An operand whose every edge has zero length has collapsed onto a single
    // point — its own bounding box — which the arrangement below would never see
    // as a vertex, there being no edge to carry it.
    if (cutsOfA == 0) {
        const ExactPoint vertex(a.bbox().min());
        if (b.contains(vertex)) {
            pieces.emplace_back(ResultPoint(vertex));
        }
        return pieces;
    }
    if (cuts.size() == cutsOfA) {
        const ExactPoint vertex(b.bbox().min());
        if (a.contains(vertex)) {
            pieces.emplace_back(ResultPoint(vertex));
        }
        return pieces;
    }

    const Arrangement<ExactPoint> arrangement = framedArrangement(cuts);
    using FaceId = typename Arrangement<ExactPoint>::FaceId;
    using HalfedgeId = typename Arrangement<ExactPoint>::HalfedgeId;
    using VertexId = typename Arrangement<ExactPoint>::VertexId;

    std::vector<char> keep(arrangement.faceCount(), 0);
    for (std::uint32_t i = 0; i < arrangement.faceCount(); ++i) {
        const FaceId f(i);
        if (!arrangement.isUnbounded(f)) {
            const ExactPoint witness = arrangement.template witness<ExactNumber>(f);
            keep[f.index()] = static_cast<char>(a.contains(witness) && b.contains(witness));
        }
    }

    // The strands, as an undirected graph on the arrangement's own vertices.
    std::vector<std::uint32_t> strandDegree(arrangement.vertexCount(), 0);
    std::vector<std::vector<std::uint32_t>> adjacency(arrangement.vertexCount());
    for (std::uint32_t i = 0; i < arrangement.edgeCount(); ++i) {
        const HalfedgeId h(2 * i);
        if (keep[arrangement.face(h).index()] != 0 ||
            keep[arrangement.face(arrangement.twin(h)).index()] != 0) {
            continue;
        }
        const ExactPoint witness = arrangement.template witness<ExactNumber>(h);
        if (!a.contains(witness) || !b.contains(witness)) {
            continue;
        }
        const std::uint32_t source = arrangement.source(h).index();
        const std::uint32_t target = arrangement.target(h).index();
        adjacency[source].push_back(target);
        adjacency[target].push_back(source);
        ++strandDegree[source];
        ++strandDegree[target];
    }

    // The isolated contact points. A vertex beside a kept face is in that
    // region piece already, and the rotational fan the DCEL carries answers that
    // without building a vector per vertex.
    for (std::uint32_t i = 0; i < arrangement.vertexCount(); ++i) {
        if (strandDegree[i] != 0) {
            continue;
        }
        const VertexId v(i);
        const HalfedgeId start = arrangement.outgoing(v);
        assert(start.valid());  // every vertex here is an endpoint of some cut
        bool besideKept = false;
        HalfedgeId h = start;
        do {
            besideKept = keep[arrangement.face(h).index()] != 0;
            h = arrangement.next(arrangement.twin(h));
        } while (h != start && !besideKept);
        if (besideKept) {
            continue;
        }
        const ExactPoint& position = arrangement[v];
        if (a.contains(position) && b.contains(position)) {
            pieces.emplace_back(ResultPoint(position));
        }
    }

    // Peel the open strands from their loose ends, then take whatever is left —
    // every vertex of which now carries at least two strand edges — as closed
    // walks. Peeling cannot strand a new loose end: a walk runs on through every
    // vertex it leaves with one edge, and stops only at a junction or at nothing.
    const auto takeEdge = [&adjacency](std::uint32_t from, std::uint32_t to) {
        auto& out = adjacency[from];
        out.erase(std::find(out.begin(), out.end(), to));
        auto& back = adjacency[to];
        back.erase(std::find(back.begin(), back.end(), from));
    };
    const auto walkFrom = [&](std::uint32_t start, bool stopAtJunctions) {
        std::vector<std::uint32_t> walk{start};
        std::uint32_t current = start;
        while (!adjacency[current].empty() &&
               (!stopAtJunctions || adjacency[current].size() == 1)) {
            const std::uint32_t next = adjacency[current].front();
            takeEdge(current, next);
            walk.push_back(next);
            current = next;
        }
        return walk;
    };
    std::vector<std::vector<std::uint32_t>> strands;
    for (std::uint32_t i = 0; i < adjacency.size(); ++i) {
        if (adjacency[i].size() == 1) {
            strands.push_back(walkFrom(i, /*stopAtJunctions=*/true));
        }
    }
    for (std::uint32_t i = 0; i < adjacency.size(); ++i) {
        while (!adjacency[i].empty()) {
            strands.push_back(walkFrom(i, /*stopAtJunctions=*/false));
        }
    }

    // A vertex the arrangement put in the middle of a straight stretch says
    // nothing about the strand, exactly as in a ring of the region pieces. Only
    // one of strand degree two may go: a junction is shared with another strand.
    const auto removable = [&](std::uint32_t before, std::uint32_t at, std::uint32_t after) {
        return strandDegree[at] == 2 &&
               collinear(arrangement[VertexId(before)], arrangement[VertexId(at)],
                         arrangement[VertexId(after)]);
    };
    for (std::vector<std::uint32_t>& walk : strands) {
        if (walk.size() > 2 && walk.front() == walk.back()) {
            // A closed strand has no distinguished first vertex, so put one that
            // survives the simplification there before simplifying.
            walk.pop_back();
            std::size_t corner = 0;
            while (corner < walk.size() &&
                   removable(walk[(corner + walk.size() - 1) % walk.size()], walk[corner],
                             walk[(corner + 1) % walk.size()])) {
                ++corner;
            }
            std::rotate(walk.begin(),
                        walk.begin() + static_cast<std::ptrdiff_t>(corner % walk.size()),
                        walk.end());
            walk.push_back(walk.front());
        }
        std::vector<ResultPoint> vertices;
        vertices.reserve(walk.size());
        vertices.emplace_back(arrangement[VertexId(walk.front())]);
        for (std::size_t i = 1; i + 1 < walk.size(); ++i) {
            if (!removable(walk[i - 1], walk[i], walk[i + 1])) {
                vertices.emplace_back(arrangement[VertexId(walk[i])]);
            }
        }
        vertices.emplace_back(arrangement[VertexId(walk.back())]);
        pieces.emplace_back(ResultPolyline(std::move(vertices)));
    }

    for (const ResultRegion& region : regularizedCellsFromKeep<ResultPoint>(arrangement, keep)) {
        pieces.emplace_back(region);
    }
    return pieces;
}

/**
 * @brief The regularized symmetric difference `closure((A° ∖ B) ∪ (B° ∖ A))`,
 *        as a set of regions.
 */
template <class ResultPoint, class ShapeA, class ShapeB>
PolygonSet<ResultPoint> regularizedSymmetricDifference(const ShapeA& a, const ShapeB& b) {
    return regularizedBoolean<ResultPoint>(a, b, [](bool inA, bool inB) { return inA != inB; });
}

}  // namespace detail

// Out-of-line: the boolean operations are declared in shape/polygon.hpp and
// shape/polygonwithholes.hpp, which precede this header in the layering, but
// they can only be defined once Triangulation is visible.

template <class PointType_, class TLabel>
template <class ResultNumber>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
PolygonWithHoles<PointType_, TLabel>::regularized() const {
    using ResultPoint = Point<ResultNumber, typename PointType_::LabelType>;
    // Nothing with area, so closure(A°) is empty and has no pieces at all.
    if (isDegenerate()) {
        return {};
    }
    // Already the closure of its own interior: rebuilding it through the
    // arrangement could only cost time and shed collinear vertices.
    if (isRegular()) {
        return PolygonSet<ResultPoint>(PolygonWithHoles<ResultPoint>(*this));
    }
    // One arrangement over this one boundary. Every cell of it is inside the
    // region or outside it, and a slit — having no cell of its own — survives
    // in neither.
    const std::array<PolygonWithHoles, 1> self{*this};
    return detail::regularizedUnionOf<ResultPoint>(self);
}

template <class PointType_, class TLabel>
template <class ResultNumber>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
PolygonSet<PointType_, TLabel>::regularized() const {
    using ResultPoint = Point<ResultNumber, typename PointType_::LabelType>;
    // The components meet at finitely many points at most, so no slit runs
    // between two of them and each regularizes on its own. The pieces of
    // different components keep the disjoint interiors their components had.
    std::vector<PolygonWithHoles<ResultPoint>> pieces;
    pieces.reserve(components_.size());
    for (const auto& component : components_) {
        for (const auto& piece : component.template regularized<ResultNumber>()) {
            pieces.push_back(piece);
        }
    }
    return PolygonSet<ResultPoint>(std::move(pieces));
}

template <class PointType_, class TLabel>
template <class ResultNumber, PolygonConcept OtherPolygon>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
Polygon<PointType_, TLabel>::difference(const OtherPolygon& other) const {
    return detail::regularizedDifference<Point<ResultNumber, typename PointType_::LabelType>>(*this,
                                                                                              other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, ConvexConcept OtherConvex>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
Polygon<PointType_, TLabel>::difference(const OtherConvex& other) const {
    return this->template difference<ResultNumber>(other.asPolygon());
}

template <class PointType_, class TLabel>
template <class ResultNumber, TriangleConcept OtherTriangle>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
Polygon<PointType_, TLabel>::difference(const OtherTriangle& other) const {
    return this->template difference<ResultNumber>(other.asConvex());
}

template <class PointType_, class TLabel>
template <class ResultNumber, RectangleConcept OtherRectangle>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
Polygon<PointType_, TLabel>::difference(const OtherRectangle& other) const {
    return this->template difference<ResultNumber>(other.asConvex());
}

template <class PointType_, class TLabel>
template <class ResultNumber, PolygonWithHolesConcept OtherRegion>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
Polygon<PointType_, TLabel>::difference(const OtherRegion& other) const {
    return detail::regularizedDifference<Point<ResultNumber, typename PointType_::LabelType>>(*this,
                                                                                              other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, PolygonSetConcept OtherSet>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
Polygon<PointType_, TLabel>::difference(const OtherSet& other) const {
    return detail::regularizedDifference<Point<ResultNumber, typename PointType_::LabelType>>(*this,
                                                                                              other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, PolygonConcept OtherPolygon>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
PolygonWithHoles<PointType_, TLabel>::difference(const OtherPolygon& other) const {
    return detail::regularizedDifference<Point<ResultNumber, typename PointType_::LabelType>>(*this,
                                                                                              other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, ConvexConcept OtherConvex>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
PolygonWithHoles<PointType_, TLabel>::difference(const OtherConvex& other) const {
    return this->template difference<ResultNumber>(other.asPolygon());
}

template <class PointType_, class TLabel>
template <class ResultNumber, TriangleConcept OtherTriangle>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
PolygonWithHoles<PointType_, TLabel>::difference(const OtherTriangle& other) const {
    return this->template difference<ResultNumber>(other.asConvex());
}

template <class PointType_, class TLabel>
template <class ResultNumber, RectangleConcept OtherRectangle>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
PolygonWithHoles<PointType_, TLabel>::difference(const OtherRectangle& other) const {
    return this->template difference<ResultNumber>(other.asConvex());
}

template <class PointType_, class TLabel>
template <class ResultNumber, PolygonWithHolesConcept OtherRegion>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
PolygonWithHoles<PointType_, TLabel>::difference(const OtherRegion& other) const {
    return detail::regularizedDifference<Point<ResultNumber, typename PointType_::LabelType>>(*this,
                                                                                              other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, PolygonSetConcept OtherSet>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
PolygonWithHoles<PointType_, TLabel>::difference(const OtherSet& other) const {
    return detail::regularizedDifference<Point<ResultNumber, typename PointType_::LabelType>>(*this,
                                                                                              other);
}

// The three bounded convex regions take a difference against every region
// through their polygon spelling. Unlike the symmetric three, a difference has
// no higher-ranked operand to forward to — `A ∖ B` is not `B ∖ A` — so each of
// them states it against all six, and `asPolygon` is what hands the pair to the
// engine. The conversion costs nothing, the vertices already being in canonical
// polygon order, and it is the same one `detail::booleanOperand` makes for a
// set's operands.

template <class PointType_, class TLabel>
template <class ResultNumber, PolygonalRegionConcept OtherRegion>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
Rectangle<PointType_, TLabel>::difference(const OtherRegion& other) const {
    return asPolygon().template difference<ResultNumber>(other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, PolygonalRegionConcept OtherRegion>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
Triangle<PointType_, TLabel>::difference(const OtherRegion& other) const {
    return asPolygon().template difference<ResultNumber>(other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, PolygonalRegionConcept OtherRegion>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
Convex<PointType_, TLabel>::difference(const OtherRegion& other) const {
    return asPolygon().template difference<ResultNumber>(other);
}

// ---------------------------------------------------------------------------
// The unbounded subtrahends.
//
// A union or a symmetric difference with an unbounded operand is unbounded, and
// no set of regions can hold it. A *difference* against one is not: `A ∖ B` is
// contained in `A`, so it is bounded whenever the receiver is, whatever `B`
// covers — which is why these overloads exist where the symmetric ones cannot.
// The latitude is one-sided, and visibly so: it is the receiver that has to be
// bounded, so `halfplane.difference(polygon)` is nowhere to be found.
//
// Being bounded is also what makes them computable. Only the part of `B` near
// `A` can matter, so `B` is clipped to a box strictly containing `A` — which
// leaves `A ∖ B` untouched, `A` being inside the box — and what comes back is a
// convex polygon the engine already takes. The two shortcuts below are the cases
// where the clip leaves nothing with area: a subtrahend with empty interior
// removes nothing, since `A° ∖ B` is dense in `A°` when `B` is nowhere dense, so
// the answer is `closure(A°)`, which is exactly @ref PolygonWithHoles::regularized.
//
// Everything else forwards: the convex three through their polygon spelling as
// they do for a bounded operand, and a polygon through the region spelling of
// itself, so the clip is written once.

template <class PointType_, class TLabel>
template <class ResultNumber, HalfplaneIntersectionConcept OtherIntersection>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
PolygonWithHoles<PointType_, TLabel>::difference(const OtherIntersection& other) const {
    using ExactNumber = detail::region_exact_number_t<typename OtherIntersection::NumberType>;
    // Nothing with area to keep, and no bounding rectangle to clip against.
    if (isDegenerate()) {
        return {};
    }
    if (other.isDegenerate()) {
        return this->template regularized<ResultNumber>();
    }
    const auto clipped = detail::regionClippedToBox(other, bbox());
    if (clipped.isDegenerate()) {
        return this->template regularized<ResultNumber>();
    }
    return this->template difference<ResultNumber>(clipped.template asConvex<ExactNumber>());
}

template <class PointType_, class TLabel>
template <class ResultNumber, HalfplaneConcept OtherHalfplane>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
PolygonWithHoles<PointType_, TLabel>::difference(const OtherHalfplane& other) const {
    return this->template difference<ResultNumber>(other.asHalfplaneIntersection());
}

template <class PointType_, class TLabel>
template <class ResultNumber, HalfplaneIntersectionConcept OtherIntersection>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
PolygonSet<PointType_, TLabel>::difference(const OtherIntersection& other) const {
    using ExactNumber = detail::region_exact_number_t<typename OtherIntersection::NumberType>;
    if (isDegenerate()) {
        return {};
    }
    if (other.isDegenerate()) {
        return this->template regularized<ResultNumber>();
    }
    const auto clipped = detail::regionClippedToBox(other, bbox());
    if (clipped.isDegenerate()) {
        return this->template regularized<ResultNumber>();
    }
    return this->template difference<ResultNumber>(clipped.template asConvex<ExactNumber>());
}

template <class PointType_, class TLabel>
template <class ResultNumber, HalfplaneConcept OtherHalfplane>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
PolygonSet<PointType_, TLabel>::difference(const OtherHalfplane& other) const {
    return this->template difference<ResultNumber>(other.asHalfplaneIntersection());
}

template <class PointType_, class TLabel>
template <class ResultNumber, HalfplaneIntersectionConcept OtherIntersection>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
Polygon<PointType_, TLabel>::difference(const OtherIntersection& other) const {
    return asPolygonWithHoles().template difference<ResultNumber>(other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, HalfplaneConcept OtherHalfplane>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
Polygon<PointType_, TLabel>::difference(const OtherHalfplane& other) const {
    return asPolygonWithHoles().template difference<ResultNumber>(other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, HalfplaneIntersectionConcept OtherIntersection>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
Rectangle<PointType_, TLabel>::difference(const OtherIntersection& other) const {
    return asPolygon().template difference<ResultNumber>(other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, HalfplaneConcept OtherHalfplane>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
Rectangle<PointType_, TLabel>::difference(const OtherHalfplane& other) const {
    return asPolygon().template difference<ResultNumber>(other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, HalfplaneIntersectionConcept OtherIntersection>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
Triangle<PointType_, TLabel>::difference(const OtherIntersection& other) const {
    return asPolygon().template difference<ResultNumber>(other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, HalfplaneConcept OtherHalfplane>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
Triangle<PointType_, TLabel>::difference(const OtherHalfplane& other) const {
    return asPolygon().template difference<ResultNumber>(other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, HalfplaneIntersectionConcept OtherIntersection>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
Convex<PointType_, TLabel>::difference(const OtherIntersection& other) const {
    return asPolygon().template difference<ResultNumber>(other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, HalfplaneConcept OtherHalfplane>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
Convex<PointType_, TLabel>::difference(const OtherHalfplane& other) const {
    return asPolygon().template difference<ResultNumber>(other);
}

// The three bounded convex regions unite among themselves through the polygon
// engine, each pair on the higher-ranked of its two operands. The union of two
// convex shapes is not convex in general, so there is nothing a convex operand
// could contribute that its outline does not; going through `asPolygon` is the
// same conversion `detail::booleanOperand` makes for a set's operands, and it
// costs nothing, the vertices already being in canonical polygon order.

template <class PointType_, class TLabel>
template <class ResultNumber, RectangleConcept OtherRectangle>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
Rectangle<PointType_, TLabel>::regularizedUnion(const OtherRectangle& other) const {
    return asPolygon().template regularizedUnion<ResultNumber>(other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, TriangleConcept OtherTriangle>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
Triangle<PointType_, TLabel>::regularizedUnion(const OtherTriangle& other) const {
    return asPolygon().template regularizedUnion<ResultNumber>(other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, RectangleConcept OtherRectangle>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
Triangle<PointType_, TLabel>::regularizedUnion(const OtherRectangle& other) const {
    return asPolygon().template regularizedUnion<ResultNumber>(other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, ConvexConcept OtherConvex>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
Convex<PointType_, TLabel>::regularizedUnion(const OtherConvex& other) const {
    return asPolygon().template regularizedUnion<ResultNumber>(other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, TriangleConcept OtherTriangle>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
Convex<PointType_, TLabel>::regularizedUnion(const OtherTriangle& other) const {
    return asPolygon().template regularizedUnion<ResultNumber>(other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, RectangleConcept OtherRectangle>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
Convex<PointType_, TLabel>::regularizedUnion(const OtherRectangle& other) const {
    return asPolygon().template regularizedUnion<ResultNumber>(other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, PolygonConcept OtherPolygon>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
Polygon<PointType_, TLabel>::regularizedUnion(const OtherPolygon& other) const {
    return detail::regularizedUnion<Point<ResultNumber, typename PointType_::LabelType>>(*this, other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, ConvexConcept OtherConvex>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
Polygon<PointType_, TLabel>::regularizedUnion(const OtherConvex& other) const {
    return this->template regularizedUnion<ResultNumber>(other.asPolygon());
}

template <class PointType_, class TLabel>
template <class ResultNumber, TriangleConcept OtherTriangle>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
Polygon<PointType_, TLabel>::regularizedUnion(const OtherTriangle& other) const {
    return this->template regularizedUnion<ResultNumber>(other.asConvex());
}

template <class PointType_, class TLabel>
template <class ResultNumber, RectangleConcept OtherRectangle>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
Polygon<PointType_, TLabel>::regularizedUnion(const OtherRectangle& other) const {
    return this->template regularizedUnion<ResultNumber>(other.asConvex());
}

template <class PointType_, class TLabel>
template <class ResultNumber, PolygonWithHolesConcept OtherRegion>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
Polygon<PointType_, TLabel>::regularizedUnion(const OtherRegion& other) const {
    return detail::regularizedUnion<Point<ResultNumber, typename PointType_::LabelType>>(*this, other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, PolygonConcept OtherPolygon>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
PolygonWithHoles<PointType_, TLabel>::regularizedUnion(const OtherPolygon& other) const {
    return detail::regularizedUnion<Point<ResultNumber, typename PointType_::LabelType>>(*this, other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, ConvexConcept OtherConvex>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
PolygonWithHoles<PointType_, TLabel>::regularizedUnion(const OtherConvex& other) const {
    return this->template regularizedUnion<ResultNumber>(other.asPolygon());
}

template <class PointType_, class TLabel>
template <class ResultNumber, TriangleConcept OtherTriangle>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
PolygonWithHoles<PointType_, TLabel>::regularizedUnion(const OtherTriangle& other) const {
    return this->template regularizedUnion<ResultNumber>(other.asConvex());
}

template <class PointType_, class TLabel>
template <class ResultNumber, RectangleConcept OtherRectangle>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
PolygonWithHoles<PointType_, TLabel>::regularizedUnion(const OtherRectangle& other) const {
    return this->template regularizedUnion<ResultNumber>(other.asConvex());
}

template <class PointType_, class TLabel>
template <class ResultNumber, PolygonWithHolesConcept OtherRegion>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
PolygonWithHoles<PointType_, TLabel>::regularizedUnion(const OtherRegion& other) const {
    return detail::regularizedUnion<Point<ResultNumber, typename PointType_::LabelType>>(*this, other);
}

// A symmetric difference is symmetric, so the three convex regions own exactly
// the same pairs among themselves that they own for the union — each on the
// higher-ranked of its two operands — and reach everything above them through
// the same rank forwarder. The polygon spelling is the engine's operand here too.

template <class PointType_, class TLabel>
template <class ResultNumber, RectangleConcept OtherRectangle>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
Rectangle<PointType_, TLabel>::symmetricDifference(const OtherRectangle& other) const {
    return asPolygon().template symmetricDifference<ResultNumber>(other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, TriangleConcept OtherTriangle>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
Triangle<PointType_, TLabel>::symmetricDifference(const OtherTriangle& other) const {
    return asPolygon().template symmetricDifference<ResultNumber>(other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, RectangleConcept OtherRectangle>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
Triangle<PointType_, TLabel>::symmetricDifference(const OtherRectangle& other) const {
    return asPolygon().template symmetricDifference<ResultNumber>(other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, ConvexConcept OtherConvex>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
Convex<PointType_, TLabel>::symmetricDifference(const OtherConvex& other) const {
    return asPolygon().template symmetricDifference<ResultNumber>(other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, TriangleConcept OtherTriangle>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
Convex<PointType_, TLabel>::symmetricDifference(const OtherTriangle& other) const {
    return asPolygon().template symmetricDifference<ResultNumber>(other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, RectangleConcept OtherRectangle>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
Convex<PointType_, TLabel>::symmetricDifference(const OtherRectangle& other) const {
    return asPolygon().template symmetricDifference<ResultNumber>(other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, PolygonConcept OtherPolygon>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
Polygon<PointType_, TLabel>::symmetricDifference(const OtherPolygon& other) const {
    return detail::regularizedSymmetricDifference<Point<ResultNumber, typename PointType_::LabelType>>(*this, other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, ConvexConcept OtherConvex>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
Polygon<PointType_, TLabel>::symmetricDifference(const OtherConvex& other) const {
    return this->template symmetricDifference<ResultNumber>(other.asPolygon());
}

template <class PointType_, class TLabel>
template <class ResultNumber, TriangleConcept OtherTriangle>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
Polygon<PointType_, TLabel>::symmetricDifference(const OtherTriangle& other) const {
    return this->template symmetricDifference<ResultNumber>(other.asConvex());
}

template <class PointType_, class TLabel>
template <class ResultNumber, RectangleConcept OtherRectangle>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
Polygon<PointType_, TLabel>::symmetricDifference(const OtherRectangle& other) const {
    return this->template symmetricDifference<ResultNumber>(other.asConvex());
}

template <class PointType_, class TLabel>
template <class ResultNumber, PolygonWithHolesConcept OtherRegion>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
Polygon<PointType_, TLabel>::symmetricDifference(const OtherRegion& other) const {
    return detail::regularizedSymmetricDifference<Point<ResultNumber, typename PointType_::LabelType>>(*this, other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, PolygonConcept OtherPolygon>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
PolygonWithHoles<PointType_, TLabel>::symmetricDifference(const OtherPolygon& other) const {
    return detail::regularizedSymmetricDifference<Point<ResultNumber, typename PointType_::LabelType>>(*this, other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, ConvexConcept OtherConvex>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
PolygonWithHoles<PointType_, TLabel>::symmetricDifference(const OtherConvex& other) const {
    return this->template symmetricDifference<ResultNumber>(other.asPolygon());
}

template <class PointType_, class TLabel>
template <class ResultNumber, TriangleConcept OtherTriangle>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
PolygonWithHoles<PointType_, TLabel>::symmetricDifference(const OtherTriangle& other) const {
    return this->template symmetricDifference<ResultNumber>(other.asConvex());
}

template <class PointType_, class TLabel>
template <class ResultNumber, RectangleConcept OtherRectangle>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
PolygonWithHoles<PointType_, TLabel>::symmetricDifference(const OtherRectangle& other) const {
    return this->template symmetricDifference<ResultNumber>(other.asConvex());
}

template <class PointType_, class TLabel>
template <class ResultNumber, PolygonWithHolesConcept OtherRegion>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
PolygonWithHoles<PointType_, TLabel>::symmetricDifference(const OtherRegion& other) const {
    return detail::regularizedSymmetricDifference<Point<ResultNumber, typename PointType_::LabelType>>(*this, other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, PolygonConcept OtherPolygon>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
PolygonWithHoles<PointType_, TLabel>::regularizedIntersection(const OtherPolygon& other) const {
    return detail::regularizedIntersection<Point<ResultNumber, typename PointType_::LabelType>>(*this, other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, ConvexConcept OtherConvex>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
PolygonWithHoles<PointType_, TLabel>::regularizedIntersection(const OtherConvex& other) const {
    return this->template regularizedIntersection<ResultNumber>(other.asPolygon());
}

template <class PointType_, class TLabel>
template <class ResultNumber, TriangleConcept OtherTriangle>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
PolygonWithHoles<PointType_, TLabel>::regularizedIntersection(const OtherTriangle& other) const {
    return this->template regularizedIntersection<ResultNumber>(other.asConvex());
}

template <class PointType_, class TLabel>
template <class ResultNumber, RectangleConcept OtherRectangle>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
PolygonWithHoles<PointType_, TLabel>::regularizedIntersection(const OtherRectangle& other) const {
    return this->template regularizedIntersection<ResultNumber>(other.asConvex());
}

template <class PointType_, class TLabel>
template <class ResultNumber, PolygonWithHolesConcept OtherRegion>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
PolygonWithHoles<PointType_, TLabel>::regularizedIntersection(const OtherRegion& other) const {
    return detail::regularizedIntersection<Point<ResultNumber, typename PointType_::LabelType>>(*this, other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, HalfplaneIntersectionConcept OtherIntersection>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
PolygonWithHoles<PointType_, TLabel>::regularizedIntersection(const OtherIntersection& other) const {
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
    return this->template regularizedIntersection<ResultNumber>(clipped.template asConvex<ExactNumber>());
}

template <class PointType_, class TLabel>
template <class ResultNumber, HalfplaneConcept OtherHalfplane>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
PolygonWithHoles<PointType_, TLabel>::regularizedIntersection(const OtherHalfplane& other) const {
    return this->template regularizedIntersection<ResultNumber>(other.asHalfplaneIntersection());
}


// The literal intersection: the one operation here that keeps what
// regularization drops. It takes the same operand grid as
// regularizedIntersection and makes the same reductions onto the engine's own
// shapes — a bounded convex operand goes in as its polygon, an unbounded one is
// clipped against this region first — and differs only in the engine it calls.

template <class PointType_, class TLabel>
template <class ResultNumber, PolygonConcept OtherPolygon>
std::vector<std::variant<Point<ResultNumber, typename PointType_::LabelType>,
                         Polyline<Point<ResultNumber, typename PointType_::LabelType>>,
                         PolygonWithHoles<Point<ResultNumber, typename PointType_::LabelType>>>>
PolygonWithHoles<PointType_, TLabel>::intersection(const OtherPolygon& other) const {
    return detail::literalIntersection<Point<ResultNumber, typename PointType_::LabelType>>(
        *this, other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, ConvexConcept OtherConvex>
std::vector<std::variant<Point<ResultNumber, typename PointType_::LabelType>,
                         Polyline<Point<ResultNumber, typename PointType_::LabelType>>,
                         PolygonWithHoles<Point<ResultNumber, typename PointType_::LabelType>>>>
PolygonWithHoles<PointType_, TLabel>::intersection(const OtherConvex& other) const {
    return this->template intersection<ResultNumber>(other.asPolygon());
}

template <class PointType_, class TLabel>
template <class ResultNumber, TriangleConcept OtherTriangle>
std::vector<std::variant<Point<ResultNumber, typename PointType_::LabelType>,
                         Polyline<Point<ResultNumber, typename PointType_::LabelType>>,
                         PolygonWithHoles<Point<ResultNumber, typename PointType_::LabelType>>>>
PolygonWithHoles<PointType_, TLabel>::intersection(const OtherTriangle& other) const {
    return this->template intersection<ResultNumber>(other.asConvex());
}

template <class PointType_, class TLabel>
template <class ResultNumber, RectangleConcept OtherRectangle>
std::vector<std::variant<Point<ResultNumber, typename PointType_::LabelType>,
                         Polyline<Point<ResultNumber, typename PointType_::LabelType>>,
                         PolygonWithHoles<Point<ResultNumber, typename PointType_::LabelType>>>>
PolygonWithHoles<PointType_, TLabel>::intersection(const OtherRectangle& other) const {
    return this->template intersection<ResultNumber>(other.asConvex());
}

template <class PointType_, class TLabel>
template <class ResultNumber, PolygonWithHolesConcept OtherRegion>
std::vector<std::variant<Point<ResultNumber, typename PointType_::LabelType>,
                         Polyline<Point<ResultNumber, typename PointType_::LabelType>>,
                         PolygonWithHoles<Point<ResultNumber, typename PointType_::LabelType>>>>
PolygonWithHoles<PointType_, TLabel>::intersection(const OtherRegion& other) const {
    return detail::literalIntersection<Point<ResultNumber, typename PointType_::LabelType>>(
        *this, other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, HalfplaneIntersectionConcept OtherIntersection>
std::vector<std::variant<Point<ResultNumber, typename PointType_::LabelType>,
                         Polyline<Point<ResultNumber, typename PointType_::LabelType>>,
                         PolygonWithHoles<Point<ResultNumber, typename PointType_::LabelType>>>>
PolygonWithHoles<PointType_, TLabel>::intersection(const OtherIntersection& other) const {
    using ExactNumber = detail::region_exact_number_t<typename OtherIntersection::NumberType>;
    if (empty() || other.empty()) {
        return {};
    }
    // The clip only has to preserve A ∩ B, and A lies strictly inside the box.
    // A clip that comes back without interior is kept and handed to the engine
    // as the segment or point it collapsed to: unlike the regularized answer,
    // a contact of that dimension is a piece of this one.
    const auto clipped = detail::regionClippedToBox(other, bbox());
    return detail::literalIntersection<Point<ResultNumber, typename PointType_::LabelType>>(
        *this, clipped.template asConvex<ExactNumber>());
}

template <class PointType_, class TLabel>
template <class ResultNumber, HalfplaneConcept OtherHalfplane>
std::vector<std::variant<Point<ResultNumber, typename PointType_::LabelType>,
                         Polyline<Point<ResultNumber, typename PointType_::LabelType>>,
                         PolygonWithHoles<Point<ResultNumber, typename PointType_::LabelType>>>>
PolygonWithHoles<PointType_, TLabel>::intersection(const OtherHalfplane& other) const {
    return this->template intersection<ResultNumber>(other.asHalfplaneIntersection());
}


// ---------------------------------------------------------------------------
// The set of regions, where the four operations close.
//
// One definition per operation over every operand the engine takes, another set
// included. There is nothing per-operand about them: the engine asks a shape for
// its cut segments and for one containment test per cell, which a set answers
// like anything else. Note in particular that a set operand goes in whole rather
// than one component at a time — folding would build one arrangement per step.

template <class PointType_, class TLabel>
template <class ResultNumber, detail::SetBooleanOperandConcept OtherShape>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
PolygonSet<PointType_, TLabel>::difference(const OtherShape& other) const {
    return detail::regularizedDifference<Point<ResultNumber, typename PointType_::LabelType>>(
        *this, detail::booleanOperand(other));
}

template <class PointType_, class TLabel>
template <class ResultNumber, detail::SetBooleanOperandConcept OtherShape>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
PolygonSet<PointType_, TLabel>::regularizedUnion(const OtherShape& other) const {
    return detail::regularizedUnion<Point<ResultNumber, typename PointType_::LabelType>>(
        *this, detail::booleanOperand(other));
}

template <class PointType_, class TLabel>
template <class ResultNumber, detail::SetBooleanOperandConcept OtherShape>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
PolygonSet<PointType_, TLabel>::regularizedIntersection(const OtherShape& other) const {
    return detail::regularizedIntersection<Point<ResultNumber, typename PointType_::LabelType>>(
        *this, detail::booleanOperand(other));
}

template <class PointType_, class TLabel>
template <class ResultNumber, detail::SetBooleanOperandConcept OtherShape>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
PolygonSet<PointType_, TLabel>::symmetricDifference(const OtherShape& other) const {
    return detail::regularizedSymmetricDifference<
        Point<ResultNumber, typename PointType_::LabelType>>(*this, detail::booleanOperand(other));
}

template <class PointType_, class TLabel>
template <class ResultNumber, HalfplaneIntersectionConcept OtherIntersection>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
PolygonSet<PointType_, TLabel>::regularizedIntersection(const OtherIntersection& other) const {
    using ExactNumber = detail::region_exact_number_t<typename OtherIntersection::NumberType>;
    // Neither a set without area nor a half-plane intersection without interior
    // can contribute to closure(A° ∩ B°).
    if (isDegenerate() || other.isDegenerate()) {
        return {};
    }
    // The clip only has to preserve A ∩ B, and A lies strictly inside the box.
    const auto clipped = detail::regionClippedToBox(other, bbox());
    if (clipped.isDegenerate()) {
        return {};
    }
    return this->template regularizedIntersection<ResultNumber>(clipped.template asConvex<ExactNumber>());
}

template <class PointType_, class TLabel>
template <class ResultNumber, HalfplaneConcept OtherHalfplane>
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
PolygonSet<PointType_, TLabel>::regularizedIntersection(const OtherHalfplane& other) const {
    return this->template regularizedIntersection<ResultNumber>(other.asHalfplaneIntersection());
}

template <class PointType_, class TLabel>
template <class ResultNumber, detail::SetBooleanOperandConcept OtherShape>
std::vector<std::variant<Point<ResultNumber, typename PointType_::LabelType>,
                         Polyline<Point<ResultNumber, typename PointType_::LabelType>>,
                         PolygonWithHoles<Point<ResultNumber, typename PointType_::LabelType>>>>
PolygonSet<PointType_, TLabel>::intersection(const OtherShape& other) const {
    return detail::literalIntersection<Point<ResultNumber, typename PointType_::LabelType>>(
        *this, detail::booleanOperand(other));
}

template <class PointType_, class TLabel>
template <class ResultNumber, HalfplaneIntersectionConcept OtherIntersection>
std::vector<std::variant<Point<ResultNumber, typename PointType_::LabelType>,
                         Polyline<Point<ResultNumber, typename PointType_::LabelType>>,
                         PolygonWithHoles<Point<ResultNumber, typename PointType_::LabelType>>>>
PolygonSet<PointType_, TLabel>::intersection(const OtherIntersection& other) const {
    using ExactNumber = detail::region_exact_number_t<typename OtherIntersection::NumberType>;
    if (empty() || other.empty()) {
        return {};
    }
    // The clip only has to preserve A ∩ B, and A lies strictly inside the box.
    // A clip without interior still carries pieces of this answer; see
    // PolygonWithHoles::intersection(const OtherIntersection&) const.
    const auto clipped = detail::regionClippedToBox(other, bbox());
    return detail::literalIntersection<Point<ResultNumber, typename PointType_::LabelType>>(
        *this, clipped.template asConvex<ExactNumber>());
}

template <class PointType_, class TLabel>
template <class ResultNumber, HalfplaneConcept OtherHalfplane>
std::vector<std::variant<Point<ResultNumber, typename PointType_::LabelType>,
                         Polyline<Point<ResultNumber, typename PointType_::LabelType>>,
                         PolygonWithHoles<Point<ResultNumber, typename PointType_::LabelType>>>>
PolygonSet<PointType_, TLabel>::intersection(const OtherHalfplane& other) const {
    return this->template intersection<ResultNumber>(other.asHalfplaneIntersection());
}

}  // namespace pgl
