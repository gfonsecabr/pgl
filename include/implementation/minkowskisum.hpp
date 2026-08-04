#pragma once

#include "implementation/booleans.hpp"

/**
 * @file minkowskisum.hpp
 * @brief Minkowski sums whose result is not a single convex shape: a set of
 *        regions for a receiver that can strand a cavity, one polygon for the
 *        one receiver that cannot.
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
 * @brief The regularized Minkowski sum `closure((A ⊕ B)°)`, as a set of regions.
 *
 * Decomposes both operands into convex pieces, sums every pair of them with the
 * linear convex merge, and takes one regularized union of the results. Pieces
 * whose sum has no area are dropped before the union, which is sound and not
 * merely an optimization: a closed set with empty interior cannot add an
 * interior point to a closed union, so the regularized answer does not see it.
 * Holes that the sum would fill in anyway are dropped first — see
 * @ref holeFilteredFor — which is where an operand's holes stop costing anything.
 *
 * Complexity: `|A|·|B|` convex merges, then the cell engine over their combined
 * boundary — O(m²) segment intersections for m edges in total, over the
 * arrangement of them. That is quadratic in a quantity that is itself quadratic
 * in the operands, so this is a construction for the shapes someone writes down
 * rather than for large meshes.
 */
template <class ResultPoint, class ShapeA, class ShapeB>
std::vector<PolygonWithHoles<ResultPoint>> regularizedMinkowskiSum(const ShapeA& a,
                                                                   const ShapeB& b) {
    // Filtering either operand leaves the other's outer boundary untouched, so
    // the two tests see the same boxes whichever order they run in.
    const auto left = minkowskiConvexPieces(holeFilteredFor(a, b));
    const auto right = minkowskiConvexPieces(holeFilteredFor(b, a));
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
PGL_DEFINE_REGION_MINKOWSKI_SUM(Polygon, OrientedSegmentConcept, OtherOriented)
PGL_DEFINE_REGION_MINKOWSKI_SUM(PolygonWithHoles, SegmentConcept, OtherSegment)
PGL_DEFINE_REGION_MINKOWSKI_SUM(PolygonWithHoles, OrientedSegmentConcept, OtherOriented)
PGL_DEFINE_REGION_MINKOWSKI_SUM(Polyline, SegmentConcept, OtherSegment)
PGL_DEFINE_REGION_MINKOWSKI_SUM(Polyline, OrientedSegmentConcept, OtherOriented)

// The chain's two non-convex pairs, mirrored so that neither spelling is the
// privileged one, exactly as `Polygon` and `PolygonWithHoles` mirror theirs. The
// two calls build the same piece sums and take the same union, so they agree by
// construction rather than by a forwarding hop.
PGL_DEFINE_REGION_MINKOWSKI_SUM(Polygon, PolylineConcept, OtherPolyline)
PGL_DEFINE_REGION_MINKOWSKI_SUM(PolygonWithHoles, PolylineConcept, OtherPolyline)

// A monotone chain against a non-convex receiver, which owns the pair by rank.
// The chain's monotonicity buys nothing here: it is the receiver's concavity that
// calls for a region, and no sorting of the chain's edges takes that back.
PGL_DEFINE_REGION_MINKOWSKI_SUM(Polygon, MonotoneChainConcept, OtherChain)
PGL_DEFINE_REGION_MINKOWSKI_SUM(PolygonWithHoles, MonotoneChainConcept, OtherChain)

#undef PGL_DEFINE_REGION_MINKOWSKI_SUM

// -----------------------------------------------------------------------------
// The chain-valued receiver's own overload set: declared in
// shape/monotonechain.hpp, and a polygon rather than a set of regions for every
// operand a monotone chain accepts.

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
