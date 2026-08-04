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
 * the other side: the arrangement of both boundaries cuts the plane into
 * triangles on which membership in `A` and in `B` is constant, one witness
 * point per triangle decides whether it survives, and the boundary of the union
 * of the surviving triangles is read off the mesh. Only the per-cell test tells
 * the four operations apart. Everything is exact — the arrangement is built
 * over rationals — and the result is converted to the requested coordinate type
 * only at the very end, so an integral answer comes back integral however the
 * intermediate crossings looked.
 */

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <map>
#include <ranges>
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

// splitWalkIntoRings, which the extraction below also uses, lives beside the
// arrangement in algorithm/arrangement.hpp: turning a cell complex's boundary
// walks into rings is what both of them are for.

/**
 * @brief The union of the cells of an arrangement that @p keepWitness selects,
 *        as a set of regions.
 *
 * @p cuts must carry every point at which membership in the result can change —
 * the boundaries of all the operands involved, whatever they are. The
 * arrangement of those segments cuts the plane into triangles on each of which
 * membership is constant, so @p keepWitness is called once per cell, on a point
 * strictly inside it, and says whether the cell belongs to the result. Being
 * strictly inside, the witness is on no boundary, which is why closed
 * containments answer the open question and the result comes out regularized.
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
 * Complexity: O(m²) segment intersections for m cut segments, then a
 * constrained triangulation over the arrangement and one @p keepWitness call
 * per cell.
 */
template <class ResultPoint, class ExactPoint, class KeepWitness>
std::vector<PolygonWithHoles<ResultPoint>> regularizedCells(
    const std::vector<Segment<ExactPoint>>& cuts, KeepWitness keepWitness) {
    using ExactNumber = typename ExactPoint::NumberType;
    using ExactSegment = Segment<ExactPoint>;
    using ExactPolygon = Polygon<ExactPoint>;
    using ResultPolygon = Polygon<ResultPoint>;
    using Dart = std::pair<ExactPoint, ExactPoint>;
    constexpr std::size_t none = std::numeric_limits<std::size_t>::max();

    std::vector<PolygonWithHoles<ResultPoint>> result;
    if (cuts.empty()) {
        return result;  // no operand has an edge, so none has area
    }

    // A box strictly containing every operand, so no cell of the arrangement
    // straddles its boundary and the surviving cells are decided entirely by
    // the operands.
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

    // Each cell is in the result or out of it as a whole: its interior meets no
    // boundary edge of either operand, so one witness point settles it. The
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
        keep[i] = static_cast<char>(keepWitness(witness));
    }

    // The triangle across a directed edge is the one carrying the reverse dart.
    const auto across = [&](const ExactPoint& from, const ExactPoint& to) {
        const auto it = leftOf.find(Dart(to, from));
        return it == leftOf.end() ? none : it->second;
    };

    // Kept cells sharing an edge are one piece of the result. Cells meeting at a
    // single vertex are not: the result pinches shut there, and the two sides
    // have to come back as two regions, since neither a polygon nor a region may
    // have a self-touching outer ring.
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

    std::vector<Segment<ExactPoint>> cuts;
    for (const ShapeType& shape : shapes) {
        appendCutSegments<ExactPoint>(shape, cuts);
    }
    return regularizedCells<ResultPoint>(cuts, [&shapes](const ExactPoint& witness) {
        return std::ranges::any_of(shapes,
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
