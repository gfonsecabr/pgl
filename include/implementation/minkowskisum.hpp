#pragma once

#include "implementation/booleans.hpp"

/**
 * @file minkowskisum.hpp
 * @brief Minkowski sums whose result is not a single convex shape: one region
 *        when the substantive case is connected and regular, several only when
 *        thin or slit geometry can genuinely separate it, and one polygon for
 *        the receiver whose monotonicity rules holes out.
 *
 * `implementation/minkowski.hpp` sums the pairs whose result is a single shape:
 * a translation, a rectangle, or — for two bounded convex operands — a
 * `Convex`, merged from the two edge-direction sequences in linear time. What it
 * cannot do is the case the sum was invented for, a **non-convex** operand:
 * `doc/raw/shape_methods.md` said outright that such a sum "can be a region with
 * holes … none of those is representable today". It is now, so this header adds
 * it.
 *
 * The construction that always works is the one identity the sum satisfies over
 * unions,
 *
 *     A ⊕ B  =  ⋃ᵢⱼ (Aᵢ ⊕ Bⱼ)      whenever  A = ⋃ᵢ Aᵢ  and  B = ⋃ⱼ Bⱼ,
 *
 * used with a **convex** decomposition of each operand: the triangles of its
 * triangulated domain, plus the pieces of it that have no area beside them (a
 * degenerate operand is its own boundary, and a region carries slits). Each
 * `Aᵢ ⊕ Bⱼ` is then the linear convex merge, and the union of the `|A|·|B|`
 * results is one call to @ref pgl::detail::regularizedUnionOf — the cell engine
 * of `booleans.hpp`, which increment 11 observed was already n-ary in everything
 * but its signature. That is @ref pgl::detail::decomposedMinkowskiSum, and it
 * costs `Θ(a²b²)`.
 *
 * It is also the *last* thing @ref pgl::detail::regularizedMinkowskiSum tries,
 * because it charges for both operands' concavity whether or not either has any.
 * Three cheaper constructions come first. The first two turn on a **convex**
 * operand rather than on a type:
 *
 * - **Both operands convex** — a `Polygon` or a hole-free region can be, and then
 *   the answer is just the linear merge of `minkowski.hpp`, in `O(a + b)`.
 * - **One operand convex** — the other's *boundary* is decomposed into
 *   x-monotone runs instead of its area being triangulated, on the identity
 *   `A ⊕ B = (A + q₀) ∪ (∂A ⊕ B)`; each run's sum is the chain sweep below, which
 *   needs no arrangement at all. See @ref pgl::detail::minkowskiBoundaryPieces.
 * - **Neither convex, both with area** — only *one* of them is decomposed, and
 *   the whole of the other is summed against each of its pieces by the two
 *   constructions above. That leaves the arrangement `a` regions to unite where
 *   the all-pairs decomposition left it `a·b` convex pieces, and the pieces it
 *   does leave scatter instead of piling up. See
 *   @ref pgl::detail::minkowskiOneSidedPieces, and
 *   @ref pgl::detail::minkowskiOneSidedDecomposesLeft for which operand pays.
 *
 * None changes the worst case — a boundary that turns at every vertex has one
 * monotone run per edge, and the one-sided decomposition still ends in an
 * arrangement of `Θ(a·b)` edges — and none is a special case in the contract: all
 * four return the same answer, and the paragraphs below describe all of them.
 *
 * Two consequences worth stating, because they are what tells this entry point
 * apart from the convex-shape-valued one:
 *
 * - **The result type follows the nondegenerate case.** A sum with a
 *   nondegenerate `Polygon`, or between area-carrying regions, has connected
 *   regularized interior and returns one `PolygonWithHoles`. A vector is kept
 *   only where two thin operands, or a region slit swept by a thin operand, can
 *   leave several components even for a valid input. A rare degenerate split in
 *   a single-region overload returns its first component in canonical order;
 *   degeneracy does not widen the public type.
 * - **The result is regularized**, `closure((A ⊕ B)°)`. That costs nothing when
 *   both operands have area — a simple polygon is the closure of its own
 *   interior, and so is a sum with one — and drops the lower-dimensional parts
 *   otherwise, exactly as the boolean operations do.
 *
 * A @ref pgl::Polyline receiver is the same construction, and it is here for
 * the same reason: a chain has no area, but
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
 *
 * The boundary decomposition is the one construction here that does not fit that
 * rule, since a run's sum can put a vertex at a crossing of two of *its own*
 * pieces. Its pieces are therefore built over the exact type directly, and it is
 * taken only when that type is a rational: on floating-point coordinates it would
 * be rounding twice where the convex decomposition rounds once, which was measured
 * to matter.
 *
 * ### The chain that needs none of it
 *
 * A @ref pgl::MonotoneChain receiver is the exception this file also holds, and
 * it is worth reading as the counterpoint to everything above. A chain sorted
 * along x cannot bend back on itself, and its sum with a **convex** operand
 * therefore meets every vertical line in a single interval — see
 * @ref pgl::detail::chainSumWalk for why. Such a set is the region between two
 * x-monotone chains: **one polygon**, never holed, never in pieces, with nothing
 * to regularize.
 *
 * So that pair skips the whole engine above. No arrangement is built and nothing
 * is triangulated: one convex merge per chain edge, then a sweep merging the
 * pieces' boundaries into the sum's two, which the chain hands over already
 * sorted along x. The sweep carries an envelope as a sequence of the pieces' own
 * integer segments and leaves the points where one takes over from the next
 * implicit, so it never divides — the answer is exact in integers whenever it
 * lands on the lattice, and a crossing is converted from an exact fraction only
 * where the boundary actually has one.
 *
 * What stays here for a chain is the pairs the theorem does not cover: a
 * `Polygon`, a @ref pgl::PolygonWithHoles — whose own concavity strands cavities
 * however monotone the chain is — and a `Segment` or @ref pgl::OrientedSegment,
 * which have no area, so that consecutive pieces of the sum can meet at a point
 * rather than overlap and the answer can pinch shut where no polygon may.
 */

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <type_traits>
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
 * A polygon or a region decomposes into the convex pieces of its triangulated
 * domain — @ref Triangulation::convexPartition, the triangles with every
 * diagonal deleted that can be — which tile `closure(A°)`, everything of the
 * shape that has area. Merging the triangles first is worth doing here rather
 * than left to taste: the piece count is what both sums downstream are charged
 * for, quadratically in the all-pairs decomposition and through the arrangement's
 * crossings in the one-sided one, and halving it measured 1.8x–3.9x on region
 * pairs. What the partition leaves out is what the shape holds without any
 * neighbourhood of it being in the shape, and there are exactly two ways to have
 * some:
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
        // Already canonical, and already `Convex`: no Graham scan to redo.
        pieces = shape.convexPartition();
        if constexpr (is_polygon_with_holes_v<Shape>) {
            for (const auto& slit : regionSlits(shape)) {
                addEdge(slit);
            }
        }
    } else if constexpr (is_polyline_v<Shape> || is_monotone_chain_v<Shape>) {
        // A chain of one vertex covers that vertex and has no edge to say so
        // with; every other chain is exactly the union of its edges, zero-length
        // ones included (the Graham scan prunes those to a point). An x-monotone
        // chain decomposes the same way — it is a polyline that happens to be
        // sorted, and nothing here needs the sorting.
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
 * @brief @p shape with the holes that cannot survive `shape ⊕ other` filled in.
 *
 * A hole shows through the sum only where some translate of `−other` fits
 * inside it. A point `q` is missing from `shape ⊕ other` exactly when `q − other`
 * misses `shape` altogether; `q − other` is connected and the hole is enclosed by
 * material, so that whole translate has to lie within one hole. Reflecting
 * `other` mirrors its bounding box and so preserves its width and height, which
 * makes the test a comparison of extents: a hole narrower or shorter than
 * `other`'s box holds no translate of it and is filled in by the sum. A hole
 * matching those extents exactly might still hold one, so the comparison keeps
 * it — the filter only ever drops a hole it can rule out.
 *
 * Worth doing because holes are expensive downstream rather than here: the
 * decomposition yields `a − 2 + 2h` pieces for `h` holes, and the sum pairs
 * those counts. On two 44-vertex operands carrying four holes each, every hole
 * drops, the pairs fall from 2500 to 484 and the sum runs 15× faster for a
 * byte-identical result. The test itself is four bounding-box comparisons per
 * hole and stays in the operands' own arithmetic.
 *
 * Kept holes stay in the order they had, which is the canonical one already, so
 * the region is rebuilt trusted.
 */
template <class Shape, class OtherShape>
decltype(auto) holeFilteredFor(const Shape& shape, const OtherShape& other) {
    if constexpr (is_polygon_with_holes_v<Shape>) {
        if (shape.holes().empty()) {
            return Shape(shape);
        }
        using Extent =
            std::common_type_t<typename Shape::NumberType, typename OtherShape::NumberType>;
        const auto box = other.bbox();
        const Extent width = Extent(box.max().x()) - Extent(box.min().x());
        const Extent height = Extent(box.max().y()) - Extent(box.min().y());

        std::vector<typename Shape::PolygonType> kept;
        for (const auto& hole : shape.holes()) {
            const auto holeBox = hole.bbox();
            if (Extent(holeBox.max().x()) - Extent(holeBox.min().x()) >= width &&
                Extent(holeBox.max().y()) - Extent(holeBox.min().y()) >= height) {
                kept.push_back(hole);
            }
        }
        return Shape(shape.outer(), std::move(kept), true);
    } else {
        return (shape);  // nothing that could have a hole in it
    }
}

/**
 * @brief The Minkowski sum of two convex operands, decomposed into pairs of
 *        convex pieces and united — the fallback every pair can take.
 *
 * Decomposes both operands into convex pieces, sums every pair of them with the
 * linear convex merge, and takes one regularized union of the results. Pieces
 * whose sum has no area are dropped before the union, which is sound and not
 * merely an optimization: a closed set with empty interior cannot add an
 * interior point to a closed union, so the regularized answer does not see it.
 *
 * Complexity: `|A|·|B|` convex merges, then the cell engine over their combined
 * boundary — O(m²) segment intersections for m edges in total, over the
 * arrangement of them. That is quadratic in a quantity that is itself quadratic
 * in the operands, which is why @ref regularizedMinkowskiSum sends every pair it
 * can somewhere else first.
 */
template <class ResultPoint, class ShapeA, class ShapeB>
std::vector<PolygonWithHoles<ResultPoint>> decomposedMinkowskiSum(const ShapeA& a,
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

// -----------------------------------------------------------------------------
// What an operand is, as far as the sum is concerned. Three questions decide
// which construction @ref regularizedMinkowskiSum runs, and all three are answered
// at run time: a `Polygon` that happens to be convex takes the same path a
// `Convex` does, and it is only convex on this particular call.

/**
 * @brief Tests whether a bounded operand's point set is convex.
 *
 * A `Convex`, a `Triangle`, a `Rectangle` and a `Segment` are convex by their
 * type. A `Polygon` is convex when its own `isConvex` says so — which, on a
 * shape meeting the simplicity precondition, is exactly convexity — and a region
 * when it has no hole left and its outer ring is convex. A chain is not: a
 * `Polyline` or a `MonotoneChain` is convex only when it is a segment, and its
 * sums are handled well enough elsewhere not to need the extra case.
 */
template <class Shape>
bool minkowskiIsConvex(const Shape& shape) {
    if constexpr (is_polygon_v<Shape>) {
        return shape.isConvex();
    } else if constexpr (is_polygon_with_holes_v<Shape>) {
        return shape.holes().empty() && shape.outer().isConvex();
    } else if constexpr (is_polyline_v<Shape> || is_monotone_chain_v<Shape>) {
        return false;
    } else {
        return true;
    }
}

/**
 * @brief Tests whether an operand has area.
 *
 * Distinct from `!isDegenerate()`, which a `Segment` answers about its *length*.
 * What the sum needs to know is whether sweeping the other operand along this one
 * leaves material behind, and only a two-dimensional operand does.
 */
template <class Shape>
bool minkowskiHasArea(const Shape& shape) {
    if constexpr (is_segment_v<Shape> || is_oriented_segment_v<Shape> ||
                  is_polyline_v<Shape> || is_monotone_chain_v<Shape>) {
        return false;
    } else {
        return !shape.isDegenerate();
    }
}

/**
 * @brief A convex operand as a @ref Convex, hulled once.
 *
 * @ref minkowskiConvexSum re-derives its operands' vertex lists on every call,
 * and for anything but a `Convex` that means a Graham scan. Harmless when the
 * call happens once, but the chain sweep below makes one call per chain edge, so
 * a `Polygon` operand would be re-hulled `n` times. Converting up front makes
 * that scan happen once.
 */
template <class Shape>
auto minkowskiAsConvex(const Shape& shape) {
    using ShapePoint = typename Shape::PointType;
    if constexpr (is_convex_v<Shape>) {
        return shape;
    } else if constexpr (is_polygon_v<Shape>) {
        return Convex<ShapePoint>(shape.vertices());
    } else if constexpr (is_polygon_with_holes_v<Shape>) {
        return Convex<ShapePoint>(shape.outer().vertices());
    } else {
        std::vector<ShapePoint> vertices;
        for (const auto& vertex : shape.vertices()) {
            vertices.emplace_back(vertex);
        }
        return Convex<ShapePoint>(std::move(vertices));
    }
}

// -----------------------------------------------------------------------------
// The x-monotone chain's own sum, which needs none of the above.

/**
 * @brief Coordinate type for the two predicates that read a crossing point.
 *
 * An orientation test is quadratic in the coordinates and computes in
 * @ref promoted_number_t. Substituting a crossing for one of its points doubles
 * that degree — the crossing's homogeneous coordinates are themselves
 * determinants — so those predicates take one further step up the same ladder:
 * `int` inputs land in `pgl::int128`, `int64_t` ones in @ref pgl::BigInt, and a
 * type that manages its own width (a `Rational`, a floating-point coordinate) is
 * left where it is.
 */
template <class Number>
using chainWide_t = promoted_number_t<promoted_number_t<Number>>;

/** @brief Sign of a value, as `-1`, `0` or `1`. */
template <class Number>
constexpr int chainSignOf(const Number& value) {
    const Number zero{};
    return value > zero ? 1 : (value < zero ? -1 : 0);
}

/**
 * @brief Tells which side of the line `a → b` the point @p p lies on.
 *
 * With `a.x() < b.x()` — the only way this file calls it — a positive answer
 * means @p p is *above* the segment.
 */
template <class P, class Q>
constexpr int chainSideSign(const P& a, const P& b, const Q& p) {
    return chainSignOf(orientationDeterminant(a, b, p));
}

/**
 * @brief A point of an envelope, named rather than computed.
 *
 * Either a vertex of a piece — a sum of two input vertices, so exact in the
 * operands' own coordinates — or the crossing of two such segments, which is the
 * one kind of point of the sum that need not be on the lattice. Keeping the
 * crossing as *the two segments that make it* is what lets the whole sweep run
 * without a division: every question it asks of the point (which side of a
 * segment it is on, whether its x is left of another) is answered by an integer
 * determinant instead.
 *
 * A vertical cut — where a piece ends and the boundary steps across to the piece
 * below or above it — is the crossing with a vertical segment, so it needs no
 * case of its own.
 */
template <class P>
struct ChainEnvelopeVertex {
    P point;                  ///< The point itself, when it is a piece vertex.
    P a, b;                   ///< First segment of the crossing, otherwise.
    P c, d;                   ///< Second segment of the crossing.
    bool isCrossing = false;  ///< Which of the two the vertex is.
};

/** @brief The envelope vertex at a piece vertex. */
template <class P>
constexpr ChainEnvelopeVertex<P> chainVertexAt(const P& point) {
    return ChainEnvelopeVertex<P>{point, P(), P(), P(), P(), false};
}

/** @brief The envelope vertex where the segments `a → b` and `c → d` cross. */
template <class P>
constexpr ChainEnvelopeVertex<P> chainVertexCrossing(const P& a, const P& b, const P& c,
                                                     const P& d) {
    return ChainEnvelopeVertex<P>{P(), a, b, c, d, true};
}

/** @brief The envelope vertex where the segment `a → b` meets the vertical line at @p x. */
template <class P, class Number>
constexpr ChainEnvelopeVertex<P> chainVertexOnVertical(const P& a, const P& b, const Number& x) {
    using Coordinate = typename P::NumberType;
    const Coordinate cut = static_cast<Coordinate>(x);
    return chainVertexCrossing(a, b, P(cut, Coordinate{}), P(cut, Coordinate(1)));
}

/**
 * @brief Tells which side of the line `u1 → u2` an envelope vertex lies on,
 *        without dividing.
 *
 * A crossing is `a + r·(num/den)` with `r = b − a`, `s = d − c`, `den = r × s`
 * and `num = (c − a) × s`, so its signed area against `u` is
 *
 *     (u₂ − u₁) × (X − u₁) = base + slope·(num/den),   base = u × (a − u₁),
 *                                                      slope = u × r.
 *
 * Multiplying through by `den` and correcting for its sign settles the side with
 * integer arithmetic alone.
 */
template <class P>
constexpr int chainVertexSide(const ChainEnvelopeVertex<P>& vertex, const P& u1, const P& u2) {
    if (!vertex.isCrossing) {
        return chainSideSign(u1, u2, vertex.point);
    }
    using Wide = chainWide_t<typename P::NumberType>;
    const auto wide = [](const auto& value) { return static_cast<Wide>(value); };

    const Wide rx = wide(vertex.b.x()) - wide(vertex.a.x());
    const Wide ry = wide(vertex.b.y()) - wide(vertex.a.y());
    const Wide sx = wide(vertex.d.x()) - wide(vertex.c.x());
    const Wide sy = wide(vertex.d.y()) - wide(vertex.c.y());
    const Wide den = rx * sy - ry * sx;
    assert(den != Wide{} && "an envelope only ever names a crossing of two crossing lines");
    const Wide num =
        (wide(vertex.c.x()) - wide(vertex.a.x())) * sy - (wide(vertex.c.y()) - wide(vertex.a.y())) * sx;

    const Wide ux = wide(u2.x()) - wide(u1.x());
    const Wide uy = wide(u2.y()) - wide(u1.y());
    const Wide base =
        ux * (wide(vertex.a.y()) - wide(u1.y())) - uy * (wide(vertex.a.x()) - wide(u1.x()));
    const Wide slope = ux * ry - uy * rx;

    return chainSignOf(base * den + slope * num) * chainSignOf(den);
}

/**
 * @brief Compares an envelope vertex's x-coordinate against @p x, without
 *        dividing.
 *
 * Same substitution as @ref chainVertexSide, one coordinate instead of a
 * determinant: `X.x − x = (a.x − x) + rx·(num/den)`.
 */
template <class P, class Number>
constexpr int chainVertexXSign(const ChainEnvelopeVertex<P>& vertex, const Number& x) {
    if (!vertex.isCrossing) {
        return chainSignOf(vertex.point.x() - x);
    }
    using Wide = chainWide_t<typename P::NumberType>;
    const auto wide = [](const auto& value) { return static_cast<Wide>(value); };

    const Wide rx = wide(vertex.b.x()) - wide(vertex.a.x());
    const Wide ry = wide(vertex.b.y()) - wide(vertex.a.y());
    const Wide sx = wide(vertex.d.x()) - wide(vertex.c.x());
    const Wide sy = wide(vertex.d.y()) - wide(vertex.c.y());
    const Wide den = rx * sy - ry * sx;
    assert(den != Wide{} && "an envelope only ever names a crossing of two crossing lines");
    const Wide num =
        (wide(vertex.c.x()) - wide(vertex.a.x())) * sy - (wide(vertex.c.y()) - wide(vertex.a.y())) * sx;

    return chainSignOf((wide(vertex.a.x()) - wide(x)) * den + rx * num) * chainSignOf(den);
}

/**
 * @brief Writes an envelope vertex out in the requested point type.
 *
 * The one place this construction divides, and only for a crossing: a piece
 * vertex is a sum of two input vertices and converts as it stands. The
 * crossing's fraction is formed exactly in @ref chainWide_t and converted once,
 * so an exact `ResultNumber` keeps it exactly and an integral one truncates it —
 * the contract the boolean operations and the region-valued sums carry.
 */
template <class ResultPoint, class P>
ResultPoint chainVertexPoint(const ChainEnvelopeVertex<P>& vertex) {
    using ResultNumber = typename ResultPoint::NumberType;
    if (!vertex.isCrossing) {
        return ResultPoint(static_cast<ResultNumber>(vertex.point.x()),
                           static_cast<ResultNumber>(vertex.point.y()));
    }
    using Wide = chainWide_t<typename P::NumberType>;
    const auto wide = [](const auto& value) { return static_cast<Wide>(value); };

    const Wide rx = wide(vertex.b.x()) - wide(vertex.a.x());
    const Wide ry = wide(vertex.b.y()) - wide(vertex.a.y());
    const Wide sx = wide(vertex.d.x()) - wide(vertex.c.x());
    const Wide sy = wide(vertex.d.y()) - wide(vertex.c.y());
    const Wide den = rx * sy - ry * sx;
    assert(den != Wide{} && "an envelope only ever names a crossing of two crossing lines");
    const Wide num =
        (wide(vertex.c.x()) - wide(vertex.a.x())) * sy - (wide(vertex.c.y()) - wide(vertex.a.y())) * sx;

    const Wide xn = wide(vertex.a.x()) * den + rx * num;
    const Wide yn = wide(vertex.a.y()) * den + ry * num;

    if constexpr (extended_integral<Wide>) {
        // An exact fraction over the widened integers, handed to the same
        // conversion the boolean engine makes at the end of an arrangement.
        return ResultPoint(static_cast<ResultNumber>(Rational<Wide>(xn, den)),
                           static_cast<ResultNumber>(Rational<Wide>(yn, den)));
    } else {
        // The coordinate type already divides exactly (a Rational) or inexactly
        // by nature (a floating-point one); either way it needs no wrapper.
        return ResultPoint(static_cast<ResultNumber>(xn / den),
                           static_cast<ResultNumber>(yn / den));
    }
}

/**
 * @brief One arc of an envelope: the segment bounding the sum over an x-range,
 *        and how the boundary arrives at it.
 *
 * The arc's right end is left implicit — it is wherever the next arc starts, or
 * the segment's own right endpoint for the last one. It is entered at @ref start,
 * which lies on its own segment; when the boundary reaches it over a **vertical
 * step**, @ref step is the far end of that step and lies on the *previous* arc's
 * segment instead. A step is what a chain's vertical edge produces: the piece it
 * makes is as tall as that edge at the x where it ends, so the piece beside it
 * cannot meet it there and the sum's boundary genuinely runs vertically for a
 * stretch.
 */
template <class P>
struct ChainEnvelopeArc {
    P a;                            ///< Left endpoint of the supporting segment.
    P b;                            ///< Right endpoint, with `a.x() < b.x()`.
    ChainEnvelopeVertex<P> start;   ///< Where the arc takes over, on its own segment.
    ChainEnvelopeVertex<P> step;    ///< Far end of the vertical step into it, if any.
    typename P::NumberType stepAt{};  ///< The x the step stands at, when there is one.
    bool hasStep = false;           ///< Whether @ref step is part of the boundary.
};

/** @brief An arc entered without a step, at a point of its own segment. */
template <class P>
ChainEnvelopeArc<P> chainArcEnteredAt(const P& a, const P& b,
                                      const ChainEnvelopeVertex<P>& start) {
    ChainEnvelopeArc<P> arc;
    arc.a = a;
    arc.b = b;
    arc.start = start;
    return arc;
}

/** @brief An arc the boundary reaches over a vertical step at @p x. */
template <class P>
ChainEnvelopeArc<P> chainArcSteppedInto(const P& a, const P& b,
                                        const ChainEnvelopeVertex<P>& start,
                                        const ChainEnvelopeVertex<P>& step,
                                        const typename P::NumberType& x) {
    ChainEnvelopeArc<P> arc = chainArcEnteredAt(a, b, start);
    arc.step = step;
    arc.stepAt = x;
    arc.hasStep = true;
    return arc;
}

/**
 * @brief Merges one piece's boundary arc into the envelope of the pieces before
 *        it.
 *
 * @param envelope Envelope so far, extended in place; empty on the first call.
 * @param arc      The piece's arc, at least two points, strictly increasing in x.
 * @param keepHigher `true` builds the upper envelope, `false` the lower one.
 *
 * The walk starts at the arc's left end — everything of the envelope to the left
 * of it is final, since the pieces arrive in x-order and no later one reaches
 * back there — and runs to the arc's right end, which is at or past the
 * envelope's own. In between it advances one span at a time, a span ending at
 * whichever comes first of the current envelope arc's takeover point and the
 * current arc segment's right endpoint, and it watches the sign of the
 * difference of the two segments. A sign that flips inside a span is a crossing,
 * recorded by *naming* the two segments rather than by computing it.
 *
 * A **step** is the other way the sign can change, and the reason the two are
 * kept apart: where the envelope jumps vertically, the two boundaries swap
 * places without ever meeting, so the boundary leaves one of them at the near
 * side of the jump and arrives on the other at the far side. Both of those
 * points are read off the segments the jump stands between, so a step costs the
 * sweep no more arithmetic than a crossing does.
 */
template <class P>
void chainMergeArc(std::vector<ChainEnvelopeArc<P>>& envelope, const std::vector<P>& arc,
                   bool keepHigher) {
    assert(arc.size() >= 2 && "a piece with no span cannot bound an envelope");
    const int keep = keepHigher ? 1 : -1;

    const auto plain = [&arc](std::size_t index) {
        return chainArcEnteredAt(arc[index], arc[index + 1], chainVertexAt(arc[index]));
    };

    if (envelope.empty()) {
        for (std::size_t index = 0; index + 1 < arc.size(); ++index) {
            envelope.push_back(plain(index));
        }
        return;
    }

    // Where the boundary arrives at an arc: on the previous arc's segment when a
    // step brings it there, which is the value the span before it ends on.
    const auto entry = [&envelope](std::size_t index) -> const ChainEnvelopeVertex<P>& {
        return envelope[index].hasStep ? envelope[index].step : envelope[index].start;
    };

    const auto& x0 = arc.front().x();
    std::size_t i = envelope.size() - 1;
    while (i > 0 && chainVertexXSign(entry(i), x0) > 0) {
        --i;
    }

    // Arcs up to and including `i` survive; whatever the sweep decides is
    // appended after them, so it may keep reading the old envelope as it goes.
    std::size_t kept = i + 1;
    std::vector<ChainEnvelopeArc<P>> merged;
    std::size_t j = 0;

    // Who bounds the sum where the incoming arc starts. That start is a vertex of
    // the arc, so it lies on it and the envelope's own segment decides.
    const int startSide = -chainSideSign(envelope[i].a, envelope[i].b, arc.front());
    int winner = keep * startSide >= 0 ? 1 : -1;
    if (winner < 0) {
        // The incoming piece starts beyond the envelope, so the boundary steps
        // across to it. It leaves the envelope wherever the boundary had reached:
        // partway along the arc in force, or — when that arc begins right here,
        // which leaves it bounding nothing at all — at the point that arc was
        // itself entered at.
        ChainEnvelopeVertex<P> from;
        if (chainVertexXSign(entry(i), x0) == 0) {
            from = entry(i);
            kept = i;
        } else {
            from = chainVertexOnVertical(envelope[i].a, envelope[i].b, x0);
        }
        merged.push_back(chainArcSteppedInto(arc[0], arc[1], chainVertexAt(arc[0]), from, x0));
    }

    while (true) {
        const bool envelopeEndsLast = i + 1 == envelope.size();

        // Which of the two spans ends first; a negative order is the envelope's.
        const int order = envelopeEndsLast
                              ? chainSignOf(envelope[i].b.x() - arc[j + 1].x())
                              : chainVertexXSign(entry(i + 1), arc[j + 1].x());

        // Which of the two is ahead where the span ends, read at whichever of the
        // two segments that point belongs to.
        int side;
        if (order <= 0) {
            side = envelopeEndsLast ? chainSideSign(arc[j], arc[j + 1], envelope[i].b)
                                    : chainVertexSide(entry(i + 1), arc[j], arc[j + 1]);
        } else {
            side = -chainSideSign(envelope[i].a, envelope[i].b, arc[j + 1]);
        }
        const int ahead = keep * side;
        if (ahead != 0 && ahead != winner) {
            // The two segments swapped places inside the span, so they cross
            // there. Which two segments cross is all the boundary needs to know.
            const ChainEnvelopeVertex<P> crossing =
                chainVertexCrossing(envelope[i].a, envelope[i].b, arc[j], arc[j + 1]);
            merged.push_back(ahead > 0
                                 ? chainArcEnteredAt(envelope[i].a, envelope[i].b, crossing)
                                 : chainArcEnteredAt(arc[j], arc[j + 1], crossing));
            winner = ahead;
        }

        if (order >= 0) {
            // The incoming arc's segment ends here. It runs at least as far as
            // the envelope, so running out of segments means the merge is done.
            ++j;
            if (j + 1 >= arc.size()) {
                break;
            }
            if (winner < 0) {
                merged.push_back(plain(j));
            }
        }
        if (order <= 0) {
            if (envelopeEndsLast) {
                // Past the old envelope's right end the incoming arc is alone. It
                // takes over there, over a step unless the two happen to meet.
                if (winner > 0) {
                    const auto& stepAt = envelope[i].b.x();
                    merged.push_back(
                        side == 0
                            ? chainArcEnteredAt(arc[j], arc[j + 1], chainVertexAt(envelope[i].b))
                            : chainArcSteppedInto(
                                  arc[j], arc[j + 1],
                                  chainVertexOnVertical(arc[j], arc[j + 1], stepAt),
                                  chainVertexAt(envelope[i].b), stepAt));
                }
                for (std::size_t rest = j + 1; rest + 1 < arc.size(); ++rest) {
                    merged.push_back(plain(rest));
                }
                break;
            }
            ++i;
            if (!envelope[i].hasStep) {
                if (winner > 0) {
                    merged.push_back(envelope[i]);
                }
            } else {
                // The envelope jumps here, so whichever of the two bounds the sum
                // can change without the segments ever meeting. Both ends of what
                // the boundary actually walks are read afresh: the near one from
                // whoever bounded it before the jump, the far one from whoever
                // bounds it after.
                const int afterSide = chainVertexSide(envelope[i].start, arc[j], arc[j + 1]);
                const int after = afterSide == 0 ? winner : keep * afterSide;
                const auto& stepAt = envelope[i].stepAt;
                if (after > 0) {
                    merged.push_back(
                        winner > 0 ? envelope[i]
                                   : chainArcSteppedInto(
                                         envelope[i].a, envelope[i].b, envelope[i].start,
                                         chainVertexOnVertical(arc[j], arc[j + 1], stepAt),
                                         stepAt));
                } else if (winner > 0) {
                    merged.push_back(chainArcSteppedInto(
                        arc[j], arc[j + 1], chainVertexOnVertical(arc[j], arc[j + 1], stepAt),
                        envelope[i].step, stepAt));
                }
                winner = after;
            }
        }
    }

    envelope.resize(kept);
    envelope.insert(envelope.end(), std::make_move_iterator(merged.begin()),
                    std::make_move_iterator(merged.end()));
}

/**
 * @brief Returns one boundary arc of a convex piece as a strictly x-increasing
 *        vertex list.
 *
 * @ref Convex::lowerHull and @ref Convex::upperHull split the boundary at the
 * lexicographic extremes, which puts a vertical edge at the rightmost x in the
 * lower chain and one at the leftmost x in the upper chain. An envelope is a
 * function of x, so those two edges are trimmed away here: the lower arc stops at
 * the bottom of the right edge and the upper arc starts at the top of the left
 * one. Neither is lost — they are where the two envelopes are stitched back
 * together, or where a step crosses between them.
 */
template <class P, class L>
std::vector<P> chainPieceArc(const Convex<P, L>& piece, bool upper) {
    const std::size_t n = piece.size();
    const std::size_t top = piece.maxIndex();
    assert(n >= 2 && "a piece with no span has no arc; an operand of no width is handled apart");

    std::vector<P> arc;
    if (upper) {
        // Clockwise from the lexicographic minimum: the boundary above, already
        // in increasing lexicographic order.
        arc.reserve(n - top + 1);
        arc.push_back(piece[0]);
        for (std::size_t k = n; k > top; --k) {
            arc.push_back(piece[k - 1]);
        }
        std::size_t drop = 0;
        while (drop + 1 < arc.size() && arc[drop].x() == arc[drop + 1].x()) {
            ++drop;
        }
        arc.erase(arc.begin(), arc.begin() + static_cast<std::ptrdiff_t>(drop));
    } else {
        arc.reserve(top + 1);
        for (std::size_t k = 0; k <= top; ++k) {
            arc.push_back(piece[k]);
        }
        while (arc.size() >= 2 && arc[arc.size() - 2].x() == arc.back().x()) {
            arc.pop_back();
        }
    }
    return arc;
}

/** @brief Appends an envelope's vertices to a boundary walk, in x-order. */
template <class ResultPoint, class P>
void chainAppendEnvelope(std::vector<ResultPoint>& walk,
                         const std::vector<ChainEnvelopeArc<P>>& envelope) {
    using ResultNumber = typename ResultPoint::NumberType;
    for (const ChainEnvelopeArc<P>& arc : envelope) {
        if (arc.hasStep) {
            walk.push_back(chainVertexPoint<ResultPoint>(arc.step));
        }
        walk.push_back(chainVertexPoint<ResultPoint>(arc.start));
    }
    walk.emplace_back(static_cast<ResultNumber>(envelope.back().b.x()),
                      static_cast<ResultNumber>(envelope.back().b.y()));
}

/** @brief Removes the repeated vertices of a closed boundary walk. */
template <class ResultPoint>
void chainDropRepeated(std::vector<ResultPoint>& walk) {
    walk.erase(std::unique(walk.begin(), walk.end()), walk.end());
    while (walk.size() >= 2 && walk.front() == walk.back()) {
        walk.pop_back();
    }
}

/**
 * @brief Drops the vertices in the middle of a straight stretch of a boundary
 *        walk.
 *
 * A vertex where the walk *reverses* is kept even though its neighbours are
 * collinear with it: that is the tip of a spur, the shape of the sum where it has
 * no area beside it, and dropping it would unravel the spur rather than tidy it.
 */
template <class ResultPoint>
void chainDropCollinear(std::vector<ResultPoint>& walk) {
    using Number = typename ResultPoint::NumberType;
    if (walk.size() < 3) {
        return;
    }
    std::vector<ResultPoint> kept;
    kept.reserve(walk.size());
    for (std::size_t index = 0; index < walk.size(); ++index) {
        const ResultPoint& previous = walk[(index + walk.size() - 1) % walk.size()];
        const ResultPoint& vertex = walk[index];
        const ResultPoint& next = walk[(index + 1) % walk.size()];
        const bool straight = collinear(previous, vertex, next) &&
                              (vertex.x() - previous.x()) * (next.x() - vertex.x()) +
                                      (vertex.y() - previous.y()) * (next.y() - vertex.y()) >
                                  Number{};
        if (!straight) {
            kept.push_back(vertex);
        }
    }
    walk.swap(kept);
}

/**
 * @brief The boundary of the Minkowski sum of an x-monotone chain with a bounded
 *        convex shape, as one closed walk.
 *
 * Builds one convex piece per chain edge and merges the pieces' lower and upper
 * arcs into the sum's two boundaries. The pieces are handed over in the chain's
 * own order, which is the x-order the sweep needs.
 *
 * The walk is the sum's point set exactly, so it may trace a **spur** where the
 * sum has no area beside it and may **repeat a vertex** where the sum pinches
 * shut — both of which need an operand with no area of its own. What the two
 * callers below differ in is only what they do about that.
 *
 * The one operand with no span to sweep over is handled apart: an operand of zero
 * width drags the chain straight up, so the boundary is the chain twice over,
 * once through each end of the operand, and no envelope is needed at all.
 */
template <class ResultPoint, class ChainType, class ConvexOperand>
std::vector<ResultPoint> chainSumWalk(const ChainType& chain, const ConvexOperand& other) {
    using SumPoint = minkowskiPoint_t<ChainType, ConvexOperand>;
    using SumNumber = typename SumPoint::NumberType;
    using ResultNumber = typename ResultPoint::NumberType;

    const auto convert = [](const auto& vertex) {
        return ResultPoint(static_cast<ResultNumber>(vertex.x()),
                           static_cast<ResultNumber>(vertex.y()));
    };
    const auto shifted = [](const auto& p, const auto& q) {
        return SumPoint(static_cast<SumNumber>(p.x()) + static_cast<SumNumber>(q.x()),
                        static_cast<SumNumber>(p.y()) + static_cast<SumNumber>(q.y()));
    };

    const std::vector<SumPoint> operandVertices = minkowskiVertices<SumPoint>(other);
    if (chain.empty() || operandVertices.empty()) {
        return {};  // an empty operand absorbs
    }

    const auto byX = [](const SumPoint& p, const SumPoint& q) { return p.x() < q.x(); };
    const auto [leftmost, rightmost] =
        std::minmax_element(operandVertices.begin(), operandVertices.end(), byX);
    if (leftmost->x() == rightmost->x()) {
        // An operand of zero width — a vertical segment, or a shape that has
        // collapsed onto one — leaves the sum's fibre over x the chain's own
        // fibre widened by the operand's, so the boundary traces the chain out
        // through the operand's top and back through its bottom.
        const auto byY = [](const SumPoint& p, const SumPoint& q) { return p.y() < q.y(); };
        const auto [lowest, highest] =
            std::minmax_element(operandVertices.begin(), operandVertices.end(), byY);
        std::vector<ResultPoint> swept;
        swept.reserve(2 * chain.size());
        for (std::size_t index = 0; index < chain.size(); ++index) {
            swept.push_back(convert(shifted(chain[index], *lowest)));
        }
        for (std::size_t index = chain.size(); index > 0; --index) {
            swept.push_back(convert(shifted(chain[index - 1], *highest)));
        }
        chainDropRepeated(swept);
        chainDropCollinear(swept);
        return swept;
    }

    std::vector<ChainEnvelopeArc<SumPoint>> lower;
    std::vector<ChainEnvelopeArc<SumPoint>> upper;
    const auto mergePiece = [&lower, &upper](const auto& piece) {
        chainMergeArc(lower, chainPieceArc(piece, false), false);
        chainMergeArc(upper, chainPieceArc(piece, true), true);
    };

    if (chain.size() == 1) {
        // No edge to sweep along: the sum is the operand, translated.
        std::vector<SumPoint> translated;
        translated.reserve(operandVertices.size());
        for (const SumPoint& vertex : operandVertices) {
            translated.push_back(shifted(vertex, chain[0]));
        }
        mergePiece(Convex<SumPoint>(translated));
    } else {
        for (const auto& edge : chain.edgesView()) {
            mergePiece(minkowskiConvexSum(edge, other));
        }
    }

    std::vector<ResultPoint> walk;
    chainAppendEnvelope(walk, lower);
    const std::size_t fromRight = walk.size();
    chainAppendEnvelope(walk, upper);
    // The upper envelope was read left to right; the boundary walks it back.
    std::reverse(walk.begin() + static_cast<std::ptrdiff_t>(fromRight), walk.end());
    chainDropRepeated(walk);
    chainDropCollinear(walk);
    return walk;
}

/**
 * @brief The Minkowski sum of an x-monotone chain with a bounded convex shape,
 *        as one polygon.
 *
 * The sum of a monotone chain with a convex shape meets every vertical line in a
 * single interval, so it is the region between two x-monotone chains: one
 * polygon, whatever the chain does in between. It is simple as long as the
 * operand has area, and the walk it is built from is the sum's point set exactly,
 * so a degenerate operand comes back as a degenerate polygon rather than as
 * nothing. The two operands that have no area by nature — a `Segment`, an
 * @ref OrientedSegment — are not on this contract: their sums can pinch shut,
 * which a polygon may not, so they keep the region-valued one.
 */
template <class ResultPoint, class ChainType, class ConvexOperand>
Polygon<ResultPoint> chainMinkowskiSum(const ChainType& chain, const ConvexOperand& other) {
    std::vector<ResultPoint> walk = chainSumWalk<ResultPoint>(chain, other);
    // The walk already runs counterclockwise — the lower boundary left to right,
    // then the upper one back — and a walk with no area has no orientation to get
    // wrong, so all the canonical form still wants is its lexicographically
    // smallest vertex first. Rotating it here rather than leaving it to the
    // constructor skips an exact signed area over the whole ring, which for a
    // rational result type is the one costly thing left in this construction: its
    // running denominator is the common multiple of every crossing's.
    std::rotate(walk.begin(), std::min_element(walk.begin(), walk.end()), walk.end());
    return Polygon<ResultPoint>(std::move(walk), true);
}

// -----------------------------------------------------------------------------
// The boundary decomposition: what a convex operand buys the other one.

/**
 * @brief Splits a walk into its maximal lexicographically monotone runs.
 *
 * A @ref MonotoneChain is a *strictly* lexicographically increasing sequence, so
 * a run ends wherever the walk turns back on itself in that order — at an
 * x-extreme, and also where a vertical stretch reverses, since equal x is ordered
 * by y. A decreasing run is reversed on the way out, which costs nothing and
 * halves the number of cuts. Two equal consecutive points end a run and start the
 * next at the second of them, so a repeated vertex neither joins two runs nor
 * strands anything between them.
 *
 * The runs cover every edge of the walk exactly once, which is what makes their
 * union the walk itself — the only property the sum below needs of them.
 */
template <class P>
std::vector<std::vector<P>> minkowskiMonotoneRuns(const std::vector<P>& walk) {
    std::vector<std::vector<P>> runs;
    const auto direction = [](const P& from, const P& to) {
        return from == to ? 0 : (from < to ? 1 : -1);
    };

    std::size_t start = 0;
    while (start + 1 < walk.size()) {
        const int forward = direction(walk[start], walk[start + 1]);
        if (forward == 0) {
            ++start;  // a repeated vertex spans no edge
            continue;
        }
        std::size_t end = start + 1;
        while (end + 1 < walk.size() && direction(walk[end], walk[end + 1]) == forward) {
            ++end;
        }
        std::vector<P> run(walk.begin() + static_cast<std::ptrdiff_t>(start),
                           walk.begin() + static_cast<std::ptrdiff_t>(end) + 1);
        if (forward < 0) {
            std::reverse(run.begin(), run.end());
        }
        runs.push_back(std::move(run));
        start = end;
    }
    return runs;
}

/**
 * @brief The monotone runs of a bounded shape's whole boundary.
 *
 * A region contributes every ring, each closed by repeating its first vertex so
 * that the closing edge is covered like any other; a chain contributes its own
 * vertex sequence, which is open and needs no closing. Their union is the
 * boundary exactly, which is the property @ref minkowskiBoundaryPieces sums over.
 *
 * Computed apart from the pieces so that the dispatcher can *count* the runs
 * before committing to them: a boundary that turns back on itself at every vertex
 * has one run per edge and decomposing it buys nothing.
 */
template <class Shape>
std::vector<std::vector<typename Shape::PointType>> minkowskiBoundaryRuns(const Shape& shape) {
    using ShapePoint = typename Shape::PointType;

    std::vector<std::vector<ShapePoint>> walks;
    const auto addRing = [&walks](const auto& ring) {
        std::vector<ShapePoint> walk = ring.vertices();
        if (!walk.empty()) {
            walk.push_back(walk.front());
        }
        walks.push_back(std::move(walk));
    };
    if constexpr (is_polygon_with_holes_v<Shape>) {
        addRing(shape.outer());
        for (const auto& hole : shape.holes()) {
            addRing(hole);
        }
    } else if constexpr (is_polygon_v<Shape>) {
        addRing(shape);
    } else {
        // A chain is open: no closing edge, and its own vertex order is the walk.
        std::vector<ShapePoint> walk;
        walk.reserve(shape.size());
        for (std::size_t index = 0; index < shape.size(); ++index) {
            walk.push_back(shape[index]);
        }
        walks.push_back(std::move(walk));
    }

    std::vector<std::vector<ShapePoint>> runs;
    for (const std::vector<ShapePoint>& walk : walks) {
        std::vector<std::vector<ShapePoint>> walkRuns = minkowskiMonotoneRuns(walk);
        if (walkRuns.empty() && !walk.empty()) {
            // No run means no two consecutive vertices differ, so the whole walk
            // is one point. It still sums to something — the operand translated
            // there — and a chain of that single vertex is what says so.
            walkRuns.push_back({walk.front()});
        }
        runs.insert(runs.end(), std::make_move_iterator(walkRuns.begin()),
                    std::make_move_iterator(walkRuns.end()));
    }
    return runs;
}

/**
 * @brief The pieces of `shape ⊕ other` for a **convex** @p other with area:
 *        one polygon per monotone run of @p shape's boundary, plus @p shape
 *        itself translated.
 *
 * The identity this rests on is, for any `q₀ ∈ B` and connected `B`,
 *
 *     A ⊕ B  =  (A + q₀)  ∪  (∂A ⊕ B).
 *
 * `⊇` is immediate. For `⊆`, take `x = p + q` and slide `q` to `q₀` along the
 * segment in `B`, which is in `B` because `B` is convex: either `x − q(t)` stays
 * in `A` the whole way, and then `x ∈ A + q₀`, or it leaves, and — `A` being
 * closed — it crosses `∂A` at some `t`, giving `x ∈ ∂A ⊕ B`. A chain has no
 * interior and is its own boundary, so the first term drops for one and the
 * identity is just `A = ∂A`.
 *
 * That trades the triangulation's `n − 2` pieces for `k + 1`, where `k` counts
 * the boundary's monotone runs: the arrangement is fed `O(n + k·m)` edges instead
 * of `Θ(n·m)`, and it is fed *polygons* whose own overlaps the chain sweep has
 * already resolved. `k` is 1 for a boundary that turns back on itself once, `n`
 * for a zigzag, and the worst case is therefore unchanged — what changes is
 * everything between.
 *
 * Each run's sum is @ref chainMinkowskiSum, which builds no arrangement at all.
 * The pieces come out in the exact coordinate the union engine arranges in, since
 * unlike a convex piece sum a run's sum can put a vertex at a crossing of two of
 * its own pieces, which need not be on the operands' lattice.
 *
 * @pre @p other is convex and has area, so that each run's sum is a simple
 *      polygon rather than a walk that can pinch shut or trace a spur.
 */
template <class ExactPoint, class Shape, class ConvexOperand>
std::vector<PolygonWithHoles<ExactPoint>> minkowskiBoundaryPieces(
    const Shape& shape, const ConvexOperand& other,
    std::vector<std::vector<typename Shape::PointType>> runs) {
    using ShapePoint = typename Shape::PointType;
    using SumPoint = minkowskiPoint_t<Shape, ConvexOperand>;
    using SumNumber = typename SumPoint::NumberType;
    using ExactNumber = typename ExactPoint::NumberType;
    using ExactPolygon = Polygon<ExactPoint>;

    std::vector<PolygonWithHoles<ExactPoint>> pieces;
    pieces.reserve(runs.size() + 1);

    for (std::vector<ShapePoint>& run : runs) {
        const MonotoneChain<ShapePoint> chain(std::move(run), true);
        ExactPolygon sum = chainMinkowskiSum<ExactPoint>(chain, other);
        if (sum.size() >= 3) {
            pieces.emplace_back(std::move(sum), std::vector<ExactPolygon>{}, true);
        }
    }

    if constexpr (is_polygon_v<Shape> || is_polygon_with_holes_v<Shape>) {
        // The interior term. A boundary with no area beside it needs none: the
        // runs already cover such a shape entirely.
        if (!shape.isDegenerate()) {
            const std::vector<SumPoint> operandVertices = minkowskiVertices<SumPoint>(other);
            if (operandVertices.empty()) {
                return {};  // an empty operand absorbs
            }
            const SumPoint& q0 = operandVertices.front();
            // Translating a ring preserves both the lexicographic order of its
            // vertices and its orientation, so the canonical form survives and
            // the rings can be rebuilt trusted.
            const auto translated = [&q0](const auto& ring) {
                std::vector<ExactPoint> moved;
                moved.reserve(ring.size());
                for (const auto& vertex : ring.vertices()) {
                    moved.emplace_back(static_cast<ExactNumber>(static_cast<SumNumber>(vertex.x()) +
                                                                q0.x()),
                                       static_cast<ExactNumber>(static_cast<SumNumber>(vertex.y()) +
                                                                q0.y()));
                }
                return ExactPolygon(std::move(moved), true);
            };
            if constexpr (is_polygon_with_holes_v<Shape>) {
                std::vector<ExactPolygon> holes;
                holes.reserve(shape.holes().size());
                for (const auto& hole : shape.holes()) {
                    holes.push_back(translated(hole));
                }
                pieces.emplace_back(translated(shape.outer()), std::move(holes), true);
            } else {
                pieces.emplace_back(translated(shape), std::vector<ExactPolygon>{}, true);
            }
        }
    }
    return pieces;
}

/**
 * @brief The shape kinds @ref minkowskiBoundaryPieces knows how to decompose:
 *        those whose boundary is a walk over their own vertices.
 *
 * A compile-time gate rather than a run-time one, since the decomposition asks
 * for rings a `Triangle` or a `Rectangle` cannot hand it — and neither wants it,
 * being convex and settled by the linear merge long before.
 */
template <class Shape>
inline constexpr bool minkowskiHasWalkableBoundary =
    is_polygon_v<Shape> || is_polygon_with_holes_v<Shape> || is_polyline_v<Shape> ||
    is_monotone_chain_v<Shape>;

/**
 * @brief Tests whether @ref minkowskiBoundaryPieces may decompose this operand.
 *
 * Every shape it accepts qualifies except a region carrying a **slit**, a stretch
 * of boundary two of its rings cover between them. Such a region translates into
 * a piece whose own boundary overlaps itself, which is the one thing the coverage
 * classifier cannot read — see @ref regularizedUnionByCoverage. Slits are rare
 * and the test is only reached when a region still has a hole after the filter.
 */
template <class Shape>
bool minkowskiHasSimpleBoundary(const Shape& shape) {
    if constexpr (is_polygon_with_holes_v<Shape>) {
        return shape.holes().empty() || regionSlits(shape).empty();
    } else {
        return is_polygon_v<Shape> || is_polyline_v<Shape> || is_monotone_chain_v<Shape>;
    }
}

/**
 * @brief How many **reflex** vertices a shape's domain boundary turns at, which
 *        is what its convex partition costs.
 *
 * A vertex is reflex when the domain's interior angle there exceeds `π`, and a
 * convex partition has to cut at every one of them: `r` reflex vertices need at
 * least `⌈r/2⌉` diagonals, since a diagonal can only resolve its two endpoints,
 * and @ref Triangulation::convexPartition typically spends one apiece. So `r + 1`
 * is the piece count to plan for, against the `n − 2` a triangulation gives.
 *
 * A ring is stored counterclockwise, so a vertex of the **outer** ring is reflex
 * exactly when it turns clockwise. A **hole** is stored counterclockwise too but
 * bounds the domain the other way round, so its interior angle is the reflex of
 * the ring's own and the test flips with it: a hole is a pit whose every convex
 * corner is a reflex corner of the region around it.
 *
 * Costs one orientation test per vertex, exactly, and nothing is triangulated.
 */
template <class Ring>
std::size_t minkowskiReflexCount(const Ring& ring, bool isHole) {
    const std::size_t n = ring.size();
    if (n < 3) {
        return 0;
    }
    std::size_t reflex = 0;
    for (std::size_t i = 0; i < n; ++i) {
        const auto turn = orientationSign(ring[(i + n - 1) % n], ring[i], ring[(i + 1) % n]);
        if (isHole ? turn > 0 : turn < 0) {
            ++reflex;
        }
    }
    return reflex;
}

/**
 * @brief The convex pieces @ref minkowskiConvexPieces will cut @p shape into, and
 *        how many vertices they carry between them — both estimated without
 *        triangulating anything.
 *
 * A chain is its own answer: one two-vertex piece per edge. A polygon or a region
 * goes to @ref Triangulation::convexPartition, whose piece count is `r + 1` for
 * `r` reflex vertices (@ref minkowskiReflexCount) plus one more per hole, since a
 * hole has to be cut open before the partition can reach round it. Those pieces
 * share their diagonals in pairs and use the boundary once, so between them they
 * carry `E + 2·d` vertices for the `d = pieces − 1 + holes` diagonals a partition
 * of a region with holes needs.
 */
template <class Shape>
std::pair<std::size_t, std::size_t> minkowskiPieceEstimate(const Shape& shape) {
    if constexpr (is_polygon_v<Shape> || is_polygon_with_holes_v<Shape>) {
        std::size_t edges = 0;
        std::size_t reflex = 0;
        std::size_t holes = 0;
        if constexpr (is_polygon_with_holes_v<Shape>) {
            edges = shape.outer().size();
            reflex = minkowskiReflexCount(shape.outer(), false);
            holes = shape.holes().size();
            for (const auto& hole : shape.holes()) {
                edges += hole.size();
                reflex += minkowskiReflexCount(hole, true);
            }
        } else {
            edges = shape.size();
            reflex = minkowskiReflexCount(shape, false);
        }
        const std::size_t pieces = reflex + 1 + holes;
        return {pieces, edges + 2 * (pieces - 1 + holes)};
    } else {
        const std::size_t edges = shape.size() > 1 ? shape.size() - 1 : 0;
        const std::size_t pieces = edges > 0 ? edges : 1;
        return {pieces, 2 * pieces};
    }
}

/**
 * @brief Tests whether decomposing @p shape's boundary into @p runs is cheaper
 *        than decomposing it into convex pieces.
 *
 * Both decompositions end in the same arrangement, whose cost tracks the number
 * of edges it is handed, so that is what the two are compared on. For an
 * `m`-vertex operand and a boundary of `E` edges falling into `k` runs:
 *
 * - the boundary decomposition gives `2·E + k·m`, since a run of `e` edges sums
 *   to a polygon bounded above and below by those `e` edges' sweeps and closed off
 *   by the operand's own `m` at either end;
 * - the convex decomposition sums each piece with the linear merge, so a piece of
 *   `kᵢ` vertices gives one of `kᵢ + m`, and the `p` pieces give
 *   `Σkᵢ + p·m` between them — both of which @ref minkowskiPieceEstimate reads off
 *   the boundary without triangulating it.
 *
 * That second line used to say `p·(m + 3)` for `p = n − 2` triangles, which was
 * the decomposition of the day. It is a convex *partition* now, so `p` tracks the
 * **reflex** vertices rather than all of them — half as many on a random simple
 * polygon, and one on a shape that is nearly convex — and the pieces carry the
 * boundary once between them instead of three vertices each. Both corrections
 * push the same way: the convex decomposition is cheaper than this test used to
 * think, so the boundary decomposition has to be better than it used to have to
 * be. A nearly convex receiver is where that bites, and it is exactly where the
 * old estimate was worst: an `L` against an `m`-gon is two convex pieces summing
 * to `8 + 2m` edges, where the triangle count called it four pieces and `4m + 12`.
 *
 * A run of a single edge produces *the very same piece* either way, so a chain
 * that turns at every vertex — `k = E` — gains nothing at all and the two
 * estimates meet, as they should.
 *
 * The `6/5` is what makes this a real comparison rather than a tie-break there.
 * The boundary decomposition's pieces can carry a vertex at a crossing of two of
 * their own sub-sums, so they are built over `Rational<BigInt>` where a convex
 * piece sum stays in the operands' integers; that was measured at **1.17x per
 * edge** on exactly the `k = E` case, where the two produce identical pieces and
 * one of them is needlessly exact. So the decomposition has to save about a fifth
 * of the edges before it is worth taking, which on a chain works out at `k` under
 * roughly three quarters of `E`.
 *
 * That ratio has been swept, over twelve cases that reach this test — five
 * receiver shapes against a triangle, a rectangle and a convex operand, plus the
 * region pairs whose construction-3 inner sums ask the same question — at
 * `5/6 → 1/3, 1/2, 2/3, 1, 5/4, 3/2, 2, 3`. The answer is a **plateau and a
 * cliff**: everything from `1/3` to `1` lands within run-to-run noise of
 * everything else, and `5/4` costs **26%** on the spot, every case above it with
 * it. So the value hardly matters as long as it stays under 1, and what it must
 * not do is reach it. `1` itself measured about 1.7% better than `5/6` and is not
 * worth taking: it is the cliff edge, it leaves no margin at all, and it prices
 * the rational arithmetic above at exactly nothing.
 *
 * The estimate stays conservative for a receiver with area, whose convex
 * decomposition has to triangulate before it can produce a single piece — a cost
 * counted nowhere here, and the reason the plateau reaches as high as it does.
 */
template <class Shape, class ConvexOperand, class Runs>
bool minkowskiBoundaryPays(const Shape& shape, const ConvexOperand& other, const Runs& runs) {
    const std::size_t operandEdges = other.size();
    std::size_t edges = 0;
    if constexpr (is_polygon_with_holes_v<Shape>) {
        edges = shape.outer().size();
        for (const auto& hole : shape.holes()) {
            edges += hole.size();
        }
    } else if constexpr (is_polygon_v<Shape>) {
        edges = shape.size();  // a ring has one edge per vertex
    } else {
        edges = shape.size() > 1 ? shape.size() - 1 : 0;
    }
    const auto [pieces, pieceVertices] = minkowskiPieceEstimate(shape);
    const std::size_t convexEdges = pieceVertices + pieces * operandEdges;
    const std::size_t boundaryEdges = 2 * edges + runs.size() * operandEdges;
    return 6 * boundaryEdges < 5 * convexEdges;
}

// -----------------------------------------------------------------------------
// The one-sided decomposition, which sums its pieces with the engine below and
// so has to be declared before it.

template <class ResultPoint, class ShapeA, class ShapeB>
std::vector<PolygonWithHoles<ResultPoint>> regularizedMinkowskiSum(const ShapeA& a,
                                                                   const ShapeB& b);

/**
 * @brief The pieces of `A ⊕ B` for two non-convex operands: the whole of @p b
 *        summed against each convex piece of @p a.
 *
 * The same union identity @ref decomposedMinkowskiSum rests on, decomposing one
 * operand instead of both:
 *
 *     A ⊕ B  =  ⋃ᵢ (Aᵢ ⊕ B)      whenever  A = ⋃ᵢ Aᵢ.
 *
 * Each `Aᵢ ⊕ B` has a convex operand and so is a sum the engine already does
 * well — construction 1, 2 or 4 of @ref regularizedMinkowskiSum, never this one,
 * which is what makes the recursion finite. What comes back is `|A|` regions
 * where the all-pairs decomposition produced `|A|·|B|` convex pieces, and the
 * cost of the sum is the arrangement of them:
 *
 * - **Edges.** A piece here carries `O(b)` of them, so the arrangement is fed
 *   `Θ(a·b)` either way — but the all-pairs decomposition feeds it `a·b` separate
 *   hexagons, where every one of the `b` pieces of `B` contributes its own copy of
 *   the operand's `3` edges. Forcing the pieces through the all-pairs sum too, so
 *   that only the shape of the final union differs, still measured 2x–5x, which is
 *   how much of the gain that accounts for.
 * - **Crossings**, which is where the rest of it is. Two pieces here meet only
 *   when their own operands do — `(Aᵢ ⊕ B) ∩ (Aⱼ ⊕ B)` is empty unless `Aᵢ` and
 *   `Aⱼ` are within `diam(B)` of each other — so the pieces overlap in `O(a²)`
 *   pairs against the all-pairs decomposition's `O(a²b²)`, and far fewer than that
 *   whenever `B` is the smaller operand. Two 48-gon regions of six holes each sum
 *   8x faster this way, and the pair only gets further apart as they do.
 *
 * Which operand to decompose is @ref minkowskiOneSidedDecomposesLeft, and it is
 * not a free choice: getting it backwards is worse than not decomposing at all.
 *
 * The pieces come out over the exact type for the same reason
 * @ref minkowskiBoundaryPieces' do — each is itself the output of an arrangement,
 * so its vertices need not be on the operands' lattice — and this construction is
 * gated on that type being a rational for the same reason too.
 */
template <class ExactPoint, class ShapeA, class ShapeB>
std::vector<PolygonWithHoles<ExactPoint>> minkowskiOneSidedPieces(const ShapeA& a,
                                                                  const ShapeB& b) {
    std::vector<PolygonWithHoles<ExactPoint>> pieces;
    for (const auto& piece : minkowskiConvexPieces(a)) {
        std::vector<PolygonWithHoles<ExactPoint>> sum =
            regularizedMinkowskiSum<ExactPoint>(b, piece);
        pieces.insert(pieces.end(), std::make_move_iterator(sum.begin()),
                      std::make_move_iterator(sum.end()));
    }
    return pieces;
}

/**
 * @brief How many convex pieces @ref minkowskiConvexPieces will cut an operand
 *        into, near enough to choose a side by.
 *
 * One per vertex, and deliberately *not* the reflex-vertex estimate
 * @ref minkowskiPieceEstimate gives — which is the accurate count, and is what
 * @ref minkowskiBoundaryPays plans against. What is wanted here is only the
 * *ratio* of the two operands' counts, and substituting the accurate one was
 * measured neutral on a random pair and slightly negative on the asymmetric case
 * it was meant for: a nearly convex operand against a jagged one, at three
 * relative sizes, where it moved the choice and did not improve it. So the crude
 * count stays until there is a case that wants the other.
 */
template <class Shape>
std::size_t minkowskiPieceCount(const Shape& shape) {
    if constexpr (is_polygon_with_holes_v<Shape>) {
        return shape.vertexCount();
    } else {
        return shape.size();
    }
}

/**
 * @brief Tells which operand @ref minkowskiOneSidedPieces should decompose.
 *
 * Both operands have area by the time this is asked — see the call site — so the
 * choice is a real one, and getting it backwards is worse than never decomposing
 * at all: on a pair whose extents differ by 32 the two directions were measured
 * 30x apart, one of them 7x faster than the all-pairs decomposition and the other
 * 4x slower. What decides is how much the pieces
 * cross each other, since that is what the arrangement is charged for. The pieces
 * of `⋃ᵢ (Aᵢ ⊕ B)` are copies of `B` fattened by a triangle and scattered over
 * `A`, so two of them meet only where their own triangles lie within `diam(B)` of
 * each other, and a pair that does meet crosses in `O(b)` points — that being how
 * many edges a piece carries. For `p` pieces over an operand of area `S`:
 *
 *     crossings(A decomposed)  ≈  pₐ² · min(1, S_b/S_a) · p_b
 *     crossings(B decomposed)  ≈  p_b² · min(1, S_a/S_b) · pₐ
 *
 * The `min` is the same factor either way — whichever operand is smaller is the
 * one that bounds the reach — and so is the `pₐ·p_b`, leaving the ratio of the two
 * as `(pₐ/S_a) / (p_b/S_b)`. Decompose the operand with **fewer pieces per unit
 * area**, then: the one whose triangles are coarsest relative to its own size,
 * which is the one whose pieces scatter furthest apart.
 *
 * Area is read off the bounding box rather than measured. A sliver's own area
 * says its pieces are tiny where what matters is that they are strung out along
 * its length, and the box is the same one @ref holeFilteredFor already asks for.
 *
 * The ratio is read in `long double` rather than exactly. Nothing but the running
 * time turns on it — both sides compute the same sum — and an operand whose
 * extent overflows the conversion lands on a side deterministically all the same.
 */
template <class ShapeA, class ShapeB>
bool minkowskiOneSidedDecomposesLeft(const ShapeA& a, const ShapeB& b) {
    const auto boxAreaOf = [](const auto& shape) {
        const auto box = shape.bbox();
        const long double width =
            static_cast<long double>(box.max().x()) - static_cast<long double>(box.min().x());
        const long double height =
            static_cast<long double>(box.max().y()) - static_cast<long double>(box.min().y());
        return width * height;
    };
    return static_cast<long double>(minkowskiPieceCount(a)) * boxAreaOf(b) <=
           static_cast<long double>(minkowskiPieceCount(b)) * boxAreaOf(a);
}

/**
 * @brief The regularized Minkowski sum `closure((A ⊕ B)°)`, represented
 *        internally as all of its regions.
 *
 * Four constructions, cheapest first, and which one runs is decided on the
 * operands' *values* rather than their types:
 *
 * 1. **Both convex** — one linear merge of the two edge-direction sequences, and
 *    the answer is that convex polygon. No arrangement, no rational arithmetic,
 *    `O(a + b)`. This is the same construction `Convex ⊕ Convex` has always taken
 *    and it is worst-case optimal; what it adds is that a `Polygon` or a region
 *    that *happens* to be convex now takes it too, where it used to pay the full
 *    `Θ(a²b²)` for an `O(a + b)` answer.
 * 2. **One convex operand with area, exact coordinates, and a boundary worth
 *    decomposing** — the identity of @ref minkowskiBoundaryPieces: `k + 1` pieces
 *    where the triangulation gave `a − 2`, none of them needing a triangulation
 *    to find. The tight worst-case output bound for this pair is `Θ(a·b)` rather
 *    than `Θ(a²b²)`, so it is the pair with the most left on the table; the pieces
 *    are fewer and larger, which is what the arrangement is cheapest on. Three
 *    things can send it on, and each is a separate judgement: a **slit** in a
 *    region operand, which the coverage classifier cannot read; **floating-point**
 *    coordinates, where the decomposition's divisions cost more accuracy than it
 *    is worth; and a boundary that **turns too often** for the decomposition to
 *    save anything (@ref minkowskiBoundaryPays).
 * 3. **Neither convex, both with area, exact coordinates** —
 *    @ref minkowskiOneSidedPieces: decompose *one* of them, and sum the whole of
 *    the other against each of its pieces with construction 1 or 2. The union it
 *    ends in is over `a` regions where the all-pairs decomposition left `a·b`
 *    convex pieces, and over regions that mostly do not reach each other — worth
 *    1.75x–9.3x over construction 4 across every region pair measured. Exact
 *    coordinates for the same reason construction 2 needs them: its pieces come
 *    out of arrangements of their own. Area on *both* sides because the scatter
 *    it trades on needs small pieces, which a chain's edges are not; see the call
 *    site.
 * 4. **Neither** — @ref decomposedMinkowskiSum, the all-pairs convex decomposition,
 *    unchanged and still what every pair falls back to. It is reached by a
 *    floating-point pair, by any pair with a chain or a segment in it, and — one
 *    level down — by construction 3's own pieces where one of them is a slit.
 *
 * Holes the sum would fill in anyway are dropped first, before any of the four
 * — see @ref holeFilteredFor — which is also what lets a holed region reach
 * construction 1 or 2 at all.
 *
 * None of the four changes the worst case, which stays `Θ(a²b²)`: two boundaries
 * that turn at every vertex cross that many times however the pieces are cut.
 * What they change is everything either side of that.
 */
template <class ResultPoint, class ShapeA, class ShapeB>
std::vector<PolygonWithHoles<ResultPoint>> regularizedMinkowskiSum(const ShapeA& a,
                                                                   const ShapeB& b) {
    using SumPoint = minkowskiPoint_t<ShapeA, ShapeB>;
    using SumNumber = typename SumPoint::NumberType;
    using ExactPoint = Point<Exact1DNumber<SumNumber, SumNumber>>;

    // Filtering either operand leaves the other's outer boundary untouched, so
    // the two tests see the same boxes whichever order they run in.
    const auto& left = holeFilteredFor(a, b);
    const auto& right = holeFilteredFor(b, a);

    const bool leftConvex = minkowskiIsConvex(left);
    const bool rightConvex = minkowskiIsConvex(right);

    if (leftConvex && rightConvex) {
        const auto sum = minkowskiConvexSum(minkowskiAsConvex(left), minkowskiAsConvex(right));
        if (sum.isDegenerate()) {
            return {};  // nothing with area, so the regularized sum is empty
        }
        return {PolygonWithHoles<ResultPoint>(Polygon<ResultPoint>(sum.asPolygon()))};
    }

    // The sum is commutative, so it is the convex operand that decides and not
    // which side it arrived on. Only one of the two branches can fire: a pair that
    // got past the test above has at most one convex operand, so the other is the
    // one to decompose.
    //
    // Both branches are gated on the coordinates being exact, and that is not a
    // performance choice. A convex piece sum never divides — every one of its
    // vertices is a sum of two input vertices — so the all-pairs decomposition is
    // exact in *any* coordinate type, floating-point included, and only the final
    // arrangement rounds. A run's sum does divide, to place the crossings of its
    // own sub-sums, and those rounded vertices then feed a second arrangement. On
    // integral operands stored as `double`, measured against the exact answer, that
    // costs a worst-case relative area error of 0.145 where the all-pairs
    // decomposition holds 2e-14, and it turns single regions into two or invents
    // holes. So the decomposition is taken only where its pieces can carry their
    // own crossings, which is exactly where `Exact1DNumber` is a rational.
    constexpr bool exactPieces = !std::is_floating_point_v<SumNumber>;
    if constexpr (exactPieces && minkowskiHasWalkableBoundary<std::remove_cvref_t<decltype(left)>>) {
        if (rightConvex && minkowskiHasArea(right) && minkowskiHasSimpleBoundary(left)) {
            const auto convexRight = minkowskiAsConvex(right);
            auto runs = minkowskiBoundaryRuns(left);
            if (minkowskiBoundaryPays(left, convexRight, runs)) {
                return regularizedUnionOf<ResultPoint>(
                    minkowskiBoundaryPieces<ExactPoint>(left, convexRight, std::move(runs)), true);
            }
        }
    }
    if constexpr (exactPieces && minkowskiHasWalkableBoundary<std::remove_cvref_t<decltype(right)>>) {
        if (leftConvex && minkowskiHasArea(left) && minkowskiHasSimpleBoundary(right)) {
            const auto convexLeft = minkowskiAsConvex(left);
            auto runs = minkowskiBoundaryRuns(right);
            if (minkowskiBoundaryPays(right, convexLeft, runs)) {
                return regularizedUnionOf<ResultPoint>(
                    minkowskiBoundaryPieces<ExactPoint>(right, convexLeft, std::move(runs)), true);
            }
        }
    }

    // Neither operand is convex, so one of them is decomposed and the other is
    // not. The recursion this opens is one level deep: every sum below has a
    // `Convex` operand and so is answered by construction 1 or 2, or — for a
    // piece with no area, which only a slit produces — by construction 4.
    //
    // Both operands must have **area**, and that is what the construction turns
    // on rather than a size or a vertex count. Its whole advantage is that the
    // pieces scatter instead of piling up, and that needs a decomposition into
    // pieces *small* relative to the operand — which triangles of a triangulation
    // are and the edges of a chain are not. A 32-vertex chain over the large
    // coordinate range has edges as long as the chain itself, so every piece of
    // its sum spans the whole answer and they all cross each other; measured
    // against a region of the same extent that is 3x *slower* than construction 4,
    // whichever of the two is decomposed. The same chain against a *small* polygon
    // is 22x faster, so there is something here for a criterion that can tell a
    // chain's pieces apart by length — but extent alone does not do it, since two
    // large regions gain 1.75x where a large chain and a large region lose.
    if constexpr (exactPieces) {
        if (!leftConvex && !rightConvex && minkowskiHasArea(left) &&
            minkowskiHasArea(right)) {
            auto pieces = minkowskiOneSidedDecomposesLeft(left, right)
                              ? minkowskiOneSidedPieces<ExactPoint>(left, right)
                              : minkowskiOneSidedPieces<ExactPoint>(right, left);
            return regularizedUnionOf<ResultPoint>(pieces, true);
        }
    }

    return decomposedMinkowskiSum<ResultPoint>(left, right);
}

/**
 * @brief Returns the sole regularized component of a Minkowski sum.
 *
 * The overloads using this helper have a connected regularized sum whenever
 * their area-carrying operands are nondegenerate. Degenerate spellings can
 * still make the regularization split (for example, a flat `Polygon` summed
 * with a bent chain); those deliberately keep only the first component in the
 * canonical ordering. Degenerate behavior does not widen the public return
 * type of the geometrically substantive case.
 */
template <class ResultPoint, class ShapeA, class ShapeB>
PolygonWithHoles<ResultPoint> singleRegularizedMinkowskiSum(const ShapeA& a,
                                                             const ShapeB& b) {
    auto pieces = regularizedMinkowskiSum<ResultPoint>(a, b);
    if (pieces.empty()) {
        return {};
    }
    return std::move(pieces.front());
}

}  // namespace detail

// -----------------------------------------------------------------------------
// Out-of-line: the region-valued Minkowski sums are declared in
// shape/polyline.hpp, shape/polygon.hpp and shape/polygonwithholes.hpp, which
// precede this header in the layering, but they can only be defined once
// Triangulation is visible.

#define PGL_DEFINE_REGION_MINKOWSKI_SUM(RECEIVER, CONCEPT, OPERAND)                        \
    template <class PointType_, class TLabel>                                              \
    template <class ResultNumber, CONCEPT OPERAND>                                         \
    std::vector<PolygonWithHoles<Point<ResultNumber, typename PointType_::LabelType>>>     \
    RECEIVER<PointType_, TLabel>::minkowskiSum(const OPERAND& other) const {               \
        return detail::regularizedMinkowskiSum<                                            \
            Point<ResultNumber, typename PointType_::LabelType>>(*this, other);            \
    }

#define PGL_DEFINE_SINGLE_REGION_MINKOWSKI_SUM(RECEIVER, CONCEPT, OPERAND)                 \
    template <class PointType_, class TLabel>                                              \
    template <class ResultNumber, CONCEPT OPERAND>                                         \
    PolygonWithHoles<Point<ResultNumber, typename PointType_::LabelType>>                  \
    RECEIVER<PointType_, TLabel>::minkowskiSum(const OPERAND& other) const {               \
        return detail::singleRegularizedMinkowskiSum<                                      \
            Point<ResultNumber, typename PointType_::LabelType>>(*this, other);            \
    }

PGL_DEFINE_SINGLE_REGION_MINKOWSKI_SUM(Polygon, PolygonConcept, OtherPolygon)
PGL_DEFINE_SINGLE_REGION_MINKOWSKI_SUM(Polygon, ConvexConcept, OtherConvex)
PGL_DEFINE_SINGLE_REGION_MINKOWSKI_SUM(Polygon, TriangleConcept, OtherTriangle)
PGL_DEFINE_SINGLE_REGION_MINKOWSKI_SUM(Polygon, RectangleConcept, OtherRectangle)
PGL_DEFINE_SINGLE_REGION_MINKOWSKI_SUM(Polygon, PolygonWithHolesConcept, OtherRegion)

PGL_DEFINE_SINGLE_REGION_MINKOWSKI_SUM(PolygonWithHoles, PolygonConcept, OtherPolygon)
PGL_DEFINE_SINGLE_REGION_MINKOWSKI_SUM(PolygonWithHoles, ConvexConcept, OtherConvex)
PGL_DEFINE_SINGLE_REGION_MINKOWSKI_SUM(PolygonWithHoles, TriangleConcept, OtherTriangle)
PGL_DEFINE_SINGLE_REGION_MINKOWSKI_SUM(PolygonWithHoles, RectangleConcept, OtherRectangle)
PGL_DEFINE_SINGLE_REGION_MINKOWSKI_SUM(PolygonWithHoles, PolygonWithHolesConcept, OtherRegion)

// A Polyline has no area, so most of its operands are the ones that have some.
// A Segment is the exception, and belongs here for the reason a chain does: two
// shapes with no area between them still sweep one out, since an edge of the
// chain and the segment span a parallelogram unless they are parallel.
PGL_DEFINE_SINGLE_REGION_MINKOWSKI_SUM(Polyline, ConvexConcept, OtherConvex)
PGL_DEFINE_SINGLE_REGION_MINKOWSKI_SUM(Polyline, TriangleConcept, OtherTriangle)
PGL_DEFINE_SINGLE_REGION_MINKOWSKI_SUM(Polyline, RectangleConcept, OtherRectangle)
PGL_DEFINE_SINGLE_REGION_MINKOWSKI_SUM(Polyline, PolygonConcept, OtherPolygon)
PGL_DEFINE_REGION_MINKOWSKI_SUM(Polyline, PolygonWithHolesConcept, OtherRegion)

// The thinnest operand any of the three receivers takes. A segment is a single
// convex piece, so it costs one convex merge per piece of the receiver, and it
// is the receiver's own shape that decides whether the sweep strands a cavity.
PGL_DEFINE_SINGLE_REGION_MINKOWSKI_SUM(Polygon, SegmentConcept, OtherSegment)
PGL_DEFINE_SINGLE_REGION_MINKOWSKI_SUM(Polygon, OrientedSegmentConcept, OtherOriented)
PGL_DEFINE_REGION_MINKOWSKI_SUM(PolygonWithHoles, SegmentConcept, OtherSegment)
PGL_DEFINE_REGION_MINKOWSKI_SUM(PolygonWithHoles, OrientedSegmentConcept, OtherOriented)
PGL_DEFINE_REGION_MINKOWSKI_SUM(Polyline, SegmentConcept, OtherSegment)
PGL_DEFINE_REGION_MINKOWSKI_SUM(Polyline, OrientedSegmentConcept, OtherOriented)

// The chain's two non-convex pairs, mirrored so that neither spelling is the
// privileged one, exactly as `Polygon` and `PolygonWithHoles` mirror theirs. The
// two calls build the same piece sums and take the same union, so they agree by
// construction rather than by a forwarding hop.
PGL_DEFINE_SINGLE_REGION_MINKOWSKI_SUM(Polygon, PolylineConcept, OtherPolyline)
PGL_DEFINE_REGION_MINKOWSKI_SUM(PolygonWithHoles, PolylineConcept, OtherPolyline)

// A monotone chain against a non-convex receiver, which owns the pair by rank.
// The chain's monotonicity buys nothing here: it is the receiver's concavity that
// calls for a region, and no sorting of the chain's edges takes that back.
PGL_DEFINE_SINGLE_REGION_MINKOWSKI_SUM(Polygon, MonotoneChainConcept, OtherChain)
PGL_DEFINE_REGION_MINKOWSKI_SUM(PolygonWithHoles, MonotoneChainConcept, OtherChain)

#undef PGL_DEFINE_REGION_MINKOWSKI_SUM
#undef PGL_DEFINE_SINGLE_REGION_MINKOWSKI_SUM

// -----------------------------------------------------------------------------
// The chain-valued receiver's own overload set: declared in
// shape/monotonechain.hpp, and a polygon rather than a region-with-holes result
// for every bounded convex operand a monotone chain accepts.

#define PGL_DEFINE_CHAIN_MINKOWSKI_SUM(CONCEPT, OPERAND)                                       \
    template <class PointType_, class TLabel, class Storage>                                   \
    template <class ResultNumber, CONCEPT OPERAND>                                             \
    Polygon<Point<ResultNumber, typename PointType_::LabelType>>                                \
    MonotoneChain<PointType_, TLabel, Storage>::minkowskiSum(const OPERAND& other) const {     \
        return detail::chainMinkowskiSum<Point<ResultNumber, typename PointType_::LabelType>>(  \
            *this, other);                                                                     \
    }

PGL_DEFINE_CHAIN_MINKOWSKI_SUM(ConvexConcept, OtherConvex)
PGL_DEFINE_CHAIN_MINKOWSKI_SUM(TriangleConcept, OtherTriangle)
PGL_DEFINE_CHAIN_MINKOWSKI_SUM(RectangleConcept, OtherRectangle)

#undef PGL_DEFINE_CHAIN_MINKOWSKI_SUM

// The two operands with no area of their own. The sweep above has nothing to say
// about them: a summand with no area leaves consecutive pieces of the sum merely
// touching rather than overlapping, so the sum can pinch shut and needs regions —
// the same answer, from the same engine, as a Polyline's.

#define PGL_DEFINE_CHAIN_REGULARIZED_SUM(CONCEPT, OPERAND)                                     \
    template <class PointType_, class TLabel, class Storage>                                   \
    template <class ResultNumber, CONCEPT OPERAND>                                             \
    std::vector<PolygonWithHoles<Point<ResultNumber, typename PointType_::LabelType>>>          \
    MonotoneChain<PointType_, TLabel, Storage>::minkowskiSum(const OPERAND& other) const {     \
        return detail::regularizedMinkowskiSum<                                                \
            Point<ResultNumber, typename PointType_::LabelType>>(*this, other);                \
    }

PGL_DEFINE_CHAIN_REGULARIZED_SUM(SegmentConcept, OtherSegment)
PGL_DEFINE_CHAIN_REGULARIZED_SUM(OrientedSegmentConcept, OtherOriented)

#undef PGL_DEFINE_CHAIN_REGULARIZED_SUM

}  // namespace pgl
