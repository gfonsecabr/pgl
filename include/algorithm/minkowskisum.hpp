#pragma once

#include "algorithm/booleans.hpp"

/**
 * @file minkowskisum.hpp
 * @brief Minkowski sums whose result needs a region with holes.
 *
 * `implementation/minkowski.hpp` sums the pairs whose result is a single shape:
 * a translation, a rectangle, or — for two bounded convex operands — a
 * `Convex`, merged from the two edge-direction sequences in linear time. What it
 * cannot do is the case the sum was invented for, a **non-convex** operand:
 * `doc/raw/shape_methods.md` said outright that such a sum "can be a region with
 * holes … none of those is representable today". It is now, so this header adds
 * it.
 *
 * The construction is the one identity the sum satisfies over unions,
 *
 *     A ⊕ B  =  ⋃ᵢⱼ (Aᵢ ⊕ Bⱼ)      whenever  A = ⋃ᵢ Aᵢ  and  B = ⋃ⱼ Bⱼ,
 *
 * used with a **convex** decomposition of each operand: the triangles of its
 * triangulated domain, plus the pieces of it that have no area beside them (a
 * degenerate operand is its own boundary, and a region carries slits). Each
 * `Aᵢ ⊕ Bⱼ` is then the linear convex merge, and the union of the `|A|·|B|`
 * results is one call to @ref pgl::detail::regularizedUnionOf — the cell engine
 * of `booleans.hpp`, which increment 11 observed was already n-ary in everything
 * but its signature.
 *
 * Two consequences worth stating, because they are what tells this entry point
 * apart from the shape-valued one:
 *
 * - **The result is a set of regions, not one region.** `A ⊕ B` is connected
 *   whenever both operands are, so it is a single region for every input anyone
 *   is likely to write; a flat list is what lets the boundary pinch shut without
 *   the answer having to lie about it, since neither a polygon nor a region may
 *   have a self-touching outer ring.
 * - **The result is regularized**, `closure((A ⊕ B)°)`. That costs nothing when
 *   both operands have area — a simple polygon is the closure of its own
 *   interior, and so is a sum with one — and drops the lower-dimensional parts
 *   otherwise, exactly as the boolean operations do.
 *
 * A @ref pgl::Polyline receiver is the same construction with the same two
 * consequences, and it is here for the same reason: a chain has no area, but
 * dragging another shape along one sweeps out material that closes over a hole as
 * readily as a `C` does — a closed chain is the plainest example there is. It
 * takes every bounded operand with area to sweep (`Triangle`, `Rectangle`,
 * `Convex`, `Polygon`, `PolygonWithHoles`) plus `Segment` and `OrientedSegment`,
 * which have none and sweep one out all the same: an edge of the chain and the
 * segment span a parallelogram unless the two are parallel. Being its own
 * boundary, the chain's own decomposition is its edges.
 *
 * A `Segment` is the thinnest operand all three receivers take, and the cheapest:
 * it is one convex piece, so it costs one convex merge per piece of the receiver
 * and the arrangement it feeds is a single translated copy of that decomposition.
 *
 * Exactness follows the same rule as `booleans.hpp`: every vertex of every
 * convex piece sum is a sum of two input vertices, so the pieces are exact in
 * the operands' promoted coordinate type; only the union of them can put a
 * vertex at a crossing, and that arrangement is built over rationals and
 * converted to the requested type once, at the end.
 */

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

namespace pgl {

namespace detail {

/**
 * @brief A convex decomposition of a bounded shape: convex pieces whose union
 *        is exactly the shape.
 *
 * A convex operand is its own single piece, and so is a `Segment` or an
 * `OrientedSegment` — a two-vertex piece the Graham scan orders like any other.
 * A polygon or a region decomposes
 * into the triangles of its triangulated domain, which tile `closure(A°)` —
 * everything of the shape that has area. What that leaves out is what the shape
 * holds without any neighbourhood of it being in the shape, and there are
 * exactly two ways to have some:
 *
 * - the shape has **no area at all**, in which case it *is* its own boundary
 *   and there is no triangle anywhere; its edges are the decomposition;
 * - the shape is a **region with a slit**, a stretch of boundary two rings
 *   cover between them, which decision (b) admits and which
 *   @ref regionSlits finds.
 *
 * Missing either of those would silently shrink the sum, since a slit sweeps
 * out area as readily as a triangle does.
 *
 * A @ref Polyline is the first case by construction — it never has area — so its
 * edges are its decomposition, one convex piece each. Its vertex *set* is not:
 * the hull of the vertices is the answer only for a convex operand, and a chain
 * that bends is not one. A polyline of a single vertex has no edge and is that
 * vertex, which is the one shape whose decomposition is a lone point.
 */
template <class Shape>
std::vector<Convex<typename Shape::PointType>> minkowskiConvexPieces(const Shape& shape) {
    using ShapePoint = typename Shape::PointType;
    using PieceConvex = Convex<ShapePoint>;

    std::vector<PieceConvex> pieces;
    // Untrusted throughout: the constructor's Graham scan is what orders the
    // vertices and prunes a piece that has collapsed, and the convex merge below
    // relies on both.
    const auto add = [&pieces](std::vector<ShapePoint> vertices) {
        pieces.push_back(PieceConvex(std::move(vertices)));
    };

    if constexpr (is_polygon_v<Shape> || is_polygon_with_holes_v<Shape>) {
        const auto addEdge = [&add](const auto& edge) { add({edge.min(), edge.max()}); };
        if (shape.isDegenerate()) {
            if constexpr (is_polygon_with_holes_v<Shape>) {
                for (const auto& edge : shape.edges()) {
                    addEdge(edge);
                }
            } else {
                for (const auto& edge : shape.edgesView()) {
                    addEdge(edge);
                }
            }
            return pieces;
        }
        for (const auto& triangle : shape.triangulation().triangles()) {
            add({triangle.a(), triangle.b(), triangle.c()});
        }
        if constexpr (is_polygon_with_holes_v<Shape>) {
            for (const auto& slit : regionSlits(shape)) {
                addEdge(slit);
            }
        }
    } else if constexpr (is_polyline_v<Shape>) {
        // A chain of one vertex covers that vertex and has no edge to say so
        // with; every other chain is exactly the union of its edges, zero-length
        // ones included (the Graham scan prunes those to a point).
        if (shape.size() == 1) {
            add({shape[0]});
        }
        for (const auto& edge : shape.edgesView()) {
            add({edge.min(), edge.max()});
        }
    } else {
        std::vector<ShapePoint> vertices;
        for (const auto& vertex : shape.vertices()) {
            vertices.emplace_back(vertex);
        }
        add(std::move(vertices));
    }
    return pieces;
}

/**
 * @brief The regularized Minkowski sum `closure((A ⊕ B)°)`, as a set of regions.
 *
 * Decomposes both operands into convex pieces, sums every pair of them with the
 * linear convex merge, and takes one regularized union of the results. Pieces
 * whose sum has no area are dropped before the union, which is sound and not
 * merely an optimization: a closed set with empty interior cannot add an
 * interior point to a closed union, so the regularized answer does not see it.
 *
 * Complexity: `|A|·|B|` convex merges, then the cell engine over their combined
 * boundary — O(m²) segment intersections for m edges in total, and a constrained
 * triangulation of the arrangement. That is quadratic in a quantity that is
 * itself quadratic in the operands, so this is a construction for the shapes
 * someone writes down rather than for large meshes.
 */
template <class ResultPoint, class ShapeA, class ShapeB>
std::vector<PolygonWithHoles<ResultPoint>> regularizedMinkowskiSum(const ShapeA& a,
                                                                   const ShapeB& b) {
    const auto left = minkowskiConvexPieces(a);
    const auto right = minkowskiConvexPieces(b);
    using SumConvex = decltype(minkowskiConvexSum(left.front(), right.front()));

    std::vector<SumConvex> sums;
    sums.reserve(left.size() * right.size());
    for (const auto& piece : left) {
        for (const auto& other : right) {
            SumConvex sum = minkowskiConvexSum(piece, other);
            if (!sum.isDegenerate()) {
                sums.push_back(std::move(sum));
            }
        }
    }
    // Repeats are common once either decomposition has congruent pieces in the
    // same place — two slits sharing a direction, or a rectilinear operand whose
    // triangles come in matching pairs — and each duplicate would otherwise pay
    // for its whole boundary again in the arrangement.
    std::sort(sums.begin(), sums.end());
    sums.erase(std::unique(sums.begin(), sums.end()), sums.end());

    return regularizedUnionOf<ResultPoint>(sums);
}

}  // namespace detail

// -----------------------------------------------------------------------------
// Out-of-line: the region-valued Minkowski sums are declared in
// shape/polyline.hpp, shape/polygon.hpp and shape/polygonwithholes.hpp, which
// precede this header in the layering, but they can only be defined once
// Triangulation is visible.
//
// Every template parameter named here has to be spelled the way the declaring
// header spells it, OPERAND included: some compilers match the constraints on an
// out-of-line definition against the declaration's by parameter name rather than
// by position, and reject the definition otherwise.

#define PGL_DEFINE_REGION_MINKOWSKI_SUM(RECEIVER, CONCEPT, OPERAND)                        \
    template <class PointType_, class TLabel>                                              \
    template <class ResultNumber, CONCEPT OPERAND>                                         \
    std::vector<PolygonWithHoles<Point<ResultNumber, typename PointType_::LabelType>>>     \
    RECEIVER<PointType_, TLabel>::minkowskiSum(const OPERAND& other) const {               \
        return detail::regularizedMinkowskiSum<                                            \
            Point<ResultNumber, typename PointType_::LabelType>>(*this, other);            \
    }

PGL_DEFINE_REGION_MINKOWSKI_SUM(Polygon, PolygonConcept, OtherPolygon)
PGL_DEFINE_REGION_MINKOWSKI_SUM(Polygon, ConvexConcept, OtherConvex)
PGL_DEFINE_REGION_MINKOWSKI_SUM(Polygon, TriangleConcept, OtherTriangle)
PGL_DEFINE_REGION_MINKOWSKI_SUM(Polygon, RectangleConcept, OtherRectangle)
PGL_DEFINE_REGION_MINKOWSKI_SUM(Polygon, PolygonWithHolesConcept, OtherRegion)

PGL_DEFINE_REGION_MINKOWSKI_SUM(PolygonWithHoles, PolygonConcept, OtherPolygon)
PGL_DEFINE_REGION_MINKOWSKI_SUM(PolygonWithHoles, ConvexConcept, OtherConvex)
PGL_DEFINE_REGION_MINKOWSKI_SUM(PolygonWithHoles, TriangleConcept, OtherTriangle)
PGL_DEFINE_REGION_MINKOWSKI_SUM(PolygonWithHoles, RectangleConcept, OtherRectangle)
PGL_DEFINE_REGION_MINKOWSKI_SUM(PolygonWithHoles, PolygonWithHolesConcept, OtherRegion)

// A Polyline has no area, so most of its operands are the ones that have some.
// A Segment is the exception, and belongs here for the reason a chain does: two
// shapes with no area between them still sweep one out, since an edge of the
// chain and the segment span a parallelogram unless they are parallel.
PGL_DEFINE_REGION_MINKOWSKI_SUM(Polyline, ConvexConcept, OtherConvex)
PGL_DEFINE_REGION_MINKOWSKI_SUM(Polyline, TriangleConcept, OtherTriangle)
PGL_DEFINE_REGION_MINKOWSKI_SUM(Polyline, RectangleConcept, OtherRectangle)
PGL_DEFINE_REGION_MINKOWSKI_SUM(Polyline, PolygonConcept, OtherPolygon)
PGL_DEFINE_REGION_MINKOWSKI_SUM(Polyline, PolygonWithHolesConcept, OtherRegion)

// The thinnest operand any of the three receivers takes. A segment is a single
// convex piece, so it costs one convex merge per piece of the receiver, and it
// is the receiver's own shape that decides whether the sweep strands a cavity.
PGL_DEFINE_REGION_MINKOWSKI_SUM(Polygon, SegmentConcept, OtherSegment)
PGL_DEFINE_REGION_MINKOWSKI_SUM(Polygon, OrientedSegmentConcept, OtherSegment)
PGL_DEFINE_REGION_MINKOWSKI_SUM(PolygonWithHoles, SegmentConcept, OtherSegment)
PGL_DEFINE_REGION_MINKOWSKI_SUM(PolygonWithHoles, OrientedSegmentConcept, OtherSegment)
PGL_DEFINE_REGION_MINKOWSKI_SUM(Polyline, SegmentConcept, OtherSegment)
PGL_DEFINE_REGION_MINKOWSKI_SUM(Polyline, OrientedSegmentConcept, OtherSegment)

// The chain's two non-convex pairs, mirrored so that neither spelling is the
// privileged one, exactly as `Polygon` and `PolygonWithHoles` mirror theirs. The
// two calls build the same piece sums and take the same union, so they agree by
// construction rather than by a forwarding hop.
PGL_DEFINE_REGION_MINKOWSKI_SUM(Polygon, PolylineConcept, OtherPolyline)
PGL_DEFINE_REGION_MINKOWSKI_SUM(PolygonWithHoles, PolylineConcept, OtherPolyline)

#undef PGL_DEFINE_REGION_MINKOWSKI_SUM

}  // namespace pgl
