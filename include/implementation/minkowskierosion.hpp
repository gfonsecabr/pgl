#pragma once

#include "implementation/minkowskisum.hpp"

/**
 * @file minkowskierosion.hpp
 * @brief Minkowski erosions: the set of translations of one shape that keep it
 *        inside another.
 *
 * The erosion of `A` by `B` is the set
 * \f$A \ominus B = \{x : x \oplus B \subseteq A\} = \bigcap_{b \in B} (A - b)\f$,
 * the operation `implementation/minkowski.hpp`'s sum is the morphological dual
 * of. It is defined for exactly the pairs whose sum is
 * (@ref pgl::MinkowskiSummableConcept, plus the region-valued overload sets of
 * `implementation/minkowskisum.hpp`), and it is *not* commutative: `A ⊖ B` reads
 * its two operands quite differently, so every pair the sum answers by
 * forwarding to the higher-ranked operand is answered here on the receiver
 * itself.
 *
 * ### One identity carries the convex receivers
 *
 * Write a closed half-plane as `H = {p : cross(d, p) >= c}`. Then
 *
 *     H ⊖ B = {x : cross(d, x) >= c - inf_{b in B} cross(d, b)},
 *
 * which is `H` translated by `-b*`, where `b*` is the point of `B` attaining
 * that infimum — the very support point @ref pgl::detail::minkowskiHalfplaneSum
 * translates the *other* way to make the sum. And because erosion distributes
 * over an intersection of constraints,
 *
 *     A ⊖ B = ⋂ᵢ (Hᵢ ⊖ B)      whenever  A = ⋂ᵢ Hᵢ,
 *
 * a **convex receiver** needs nothing but its own half-planes and the operand's
 * support function: one clamp per constraint, `O(a·b)` in the two operands'
 * sizes, and no arithmetic beyond a cross product and a subtraction. Three
 * consequences are worth stating, because they are what the contract below is
 * made of:
 *
 * - **The operand need not be convex.** A support function only sees the convex
 *   hull, so `A ⊖ B` is `A ⊖ hull(B)` for convex `A`, and a `Polygon`, a
 *   `PolygonWithHoles`, a `PolygonSet`, a `Polyline` and a `MonotoneChain` are
 *   all as cheap to erode by as their vertex count. That is why the pairs the
 *   sum forwards to a region-valued overload come back here as a convex region.
 * - **It is exact on the lattice.** Every constraint of the result is a
 *   constraint of the receiver translated by one vertex of the operand, so the
 *   half-planes are integral whenever both operands are — even though the
 *   *vertices* of the result generally are not, which is exactly what a
 *   @ref pgl::HalfplaneIntersection is for. The one operand that brings a
 *   division is a `HalfplaneIntersection`, whose own support point is a crossing
 *   of two stored boundary lines; see @ref pgl::detail::minkowskiErosionPoint_t.
 * - **An unbounded operand empties a bounded receiver**, and the same clamp says
 *   so: the infimum is `-∞` in any direction the operand recedes through, that
 *   constraint admits no point at all, and the erosion is empty. No shape of a
 *   bounded receiver can survive it, which is why this file may hand a
 *   *non-convex* receiver to the convex construction when the operand is
 *   unbounded — the hull it would silently take is irrelevant to an answer that
 *   is empty either way.
 *
 * ### The non-convex receivers need the boolean engine
 *
 * That identity is all about `A` being an intersection of half-planes, so it
 * says nothing about a `Polygon` with a notch, and the erosion of one is
 * genuinely harder than its sum: it can disconnect a connected shape, which is
 * why every region-valued erosion returns a @ref pgl::PolygonSet where the
 * corresponding sum returns one @ref pgl::PolygonWithHoles.
 *
 * What always works is the complement, taken inside a window big enough to hide
 * that the plane is not one. With `W` a box containing `A` and every `a + b`,
 * and `U = closure(W° ∖ A)` the material just outside `A`,
 *
 *     A ⊖ B  =  A ∖ (U ⊕ (-B)),
 *
 * since for `x ∈ A` some `x + b` leaves `A` exactly when some `x + b` lands in
 * `U`, and `U` is where every such point is. So one difference, one Minkowski
 * sum — @ref pgl::detail::regularizedMinkowskiSum, whole — and one more
 * difference answer it. That `A ∖ …` at the front is also what makes it read
 * `A ⊖ B ⊆ A`, which is true only of an operand covering the origin, so the
 * operand is first moved onto one of its own vertices and the answer moved back.
 * See @ref pgl::detail::regularizedMinkowskiErosion.
 *
 * A **convex** receiver that merely arrived as a `Polygon` or a hole-free region
 * skips all of that for the linear clamp above, exactly as the sum's first
 * construction skips the arrangement for the linear merge.
 *
 * ### Two contracts, and why they differ
 *
 * - A convex-receiver erosion is **literal**: the point set itself, lower
 *   dimensional pieces included. `HalfplaneIntersection` holds a point, a
 *   segment, a line, a half-plane, the empty set and the whole plane, so there
 *   is nothing to round off and no reason to drop anything.
 * - A region-valued erosion is **regularized**, `closure((A ⊖ B)°)`, as its sum
 *   and every boolean operation in `booleans.hpp` is. A `PolygonSet` holds
 *   nothing thinner than a region, and an erosion produces thin material
 *   readily — a corridor exactly as wide as the operand erodes to a curve.
 *
 * ### Eroding by nothing
 *
 * `A ⊖ ∅` is the **whole plane**: every `x` satisfies `x ⊕ ∅ ⊆ A` vacuously.
 * That is the one answer a shrinking operation gives that is bigger than its
 * receiver, and what happens to it turns on *when* the operand is known to be
 * empty. An `EmptyShape` operand is empty by its type, so the dispatcher answers
 * before any result type is chosen and every receiver gets the plane as a
 * `HalfplaneIntersection`. An operand that merely *turns out* to cover no point
 * — an empty `Convex`, `Rectangle`, `Polygon`, `PolygonSet` or
 * `HalfplaneIntersection` — arrives at whatever result type its pair fixed, and
 * the three tighter ones cannot hold the plane: `Rectangle ⊖ Rectangle`,
 * `Halfplane ⊖ bounded` and every region-valued pair throw `std::logic_error`
 * rather than answer something else, where a `HalfplaneIntersection` result
 * simply returns it.
 *
 * There is deliberately **no operator** for the erosion. `operator+` spells the
 * sum because `A ⊕ {p}` is the translation `A + p` it already spelled, but `A -
 * B` is read at least as often as `A ⊕ (-B)`, which is a different set; the
 * `operator-` this library has stays what it was, a translation by a point.
 */

#include <algorithm>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>


namespace pgl {

namespace detail {

/**
 * @brief Vertex type of a Minkowski erosion.
 *
 * The erosion's constraints are the receiver's own, each translated by one
 * vertex of the operand, so unlike the sum's @ref minkowskiRegionPoint_t this
 * stays on the operands' lattice for every receiver — a `HalfplaneIntersection`
 * receiver included, since its stored half-planes are what the construction
 * reads and its fractional vertices are never touched.
 *
 * A `HalfplaneIntersection` *operand* is the one that divides: the support
 * point of one is a crossing of two of its boundary lines, so the constraint it
 * translates lands off the lattice, and the result carries
 * @ref division_result_t coordinates the way that shape's own accessors do.
 */
template <class A, class B>
using minkowskiErosionPoint_t = Point<
    std::conditional_t<is_halfplane_intersection_v<std::remove_cvref_t<B>>,
                       division_result_t<typename minkowskiPoint_t<A, B>::NumberType>,
                       typename minkowskiPoint_t<A, B>::NumberType>,
    typename minkowskiPoint_t<A, B>::LabelType>;

/** @brief The region type a convex receiver's erosion comes back as. */
template <class A, class B>
using minkowskiErosionRegion_t = HalfplaneIntersection<minkowskiErosionPoint_t<A, B>>;

/**
 * @brief One closed half-plane `{p : cross(direction, p - anchor) >= 0}` of a
 *        convex receiver.
 *
 * The same spelling @ref Halfplane uses — a boundary direction and a point on
 * the boundary — kept as a pair so that the clamp can translate the anchor and
 * leave the direction alone.
 */
template <class ResultPoint>
struct MinkowskiErosionConstraint {
    /** @brief Direction the boundary runs in, with the region on its left. */
    ResultPoint direction;
    /** @brief A point on that boundary. */
    ResultPoint anchor;
};

/**
 * @brief Writes a convex shape as the closed half-planes whose intersection it
 *        is, exactly.
 *
 * Exactly is the whole point: the erosion clamps one constraint at a time, so a
 * missing constraint is a bigger answer than the truth. That is why a shape
 * that has collapsed gets its **caps** here where @ref minkowskiPolyhedronOf,
 * which needs the same shapes' *edge directions*, has no use for them: the two
 * side constraints of a segment describe its supporting line, and only the two
 * caps cut the line back to the segment. A point takes four constraints for the
 * same reason.
 *
 * @return The constraints, or `std::nullopt` when the shape is the empty set,
 * which no family of half-planes describes. An empty list is not that case — it
 * is the whole plane, which a @ref HalfplaneIntersection with no constraint at
 * all genuinely is.
 *
 * @pre The shape is not undefined: a degenerate `Line`, `Ray` or `Halfplane`
 *      bounds nothing, and nothing is promised for one, as everywhere in the
 *      library.
 */
template <class ResultPoint, class ShapeT>
constexpr std::optional<std::vector<MinkowskiErosionConstraint<ResultPoint>>>
minkowskiErosionConstraints(const ShapeT& shape) {
    using ResultNumber = typename ResultPoint::NumberType;

    std::vector<MinkowskiErosionConstraint<ResultPoint>> constraints;
    const auto cast = [](const auto& point) {
        return ResultPoint(detail::asNumber<ResultNumber>(point.x()),
                           detail::asNumber<ResultNumber>(point.y()));
    };
    const auto add = [&constraints](const ResultPoint& direction, const ResultPoint& anchor) {
        constraints.push_back(MinkowskiErosionConstraint<ResultPoint>{direction, anchor});
    };
    const auto reversed = [](const ResultPoint& vector) {
        return ResultPoint(-vector.x(), -vector.y());
    };
    // The four constraints whose intersection is a single point: `cross(d, ·)`
    // reads a coordinate for each axis direction, and the two signs of it pin
    // that coordinate from both sides.
    const auto pin = [&add](const ResultPoint& point) {
        const ResultNumber zero{};
        const ResultNumber one = static_cast<ResultNumber>(1);
        add(ResultPoint(one, zero), point);
        add(ResultPoint(-one, zero), point);
        add(ResultPoint(zero, one), point);
        add(ResultPoint(zero, -one), point);
    };

    if constexpr (is_point_v<ShapeT>) {
        pin(cast(shape));
    } else if constexpr (is_line_v<ShapeT> || is_oriented_line_v<ShapeT> || is_ray_v<ShapeT> ||
                         is_halfplane_v<ShapeT>) {
        const ResultPoint source = cast(shape[0]);
        const ResultPoint target = cast(shape[1]);
        const ResultPoint forward(target.x() - source.x(), target.y() - source.y());
        add(forward, source);
        if constexpr (!is_halfplane_v<ShapeT>) {
            // A line and a ray are both pinned to their supporting line; only a
            // half-plane keeps a side.
            add(reversed(forward), source);
            if constexpr (is_ray_v<ShapeT>) {
                // `cross((dy, -dx), p - s)` is `dot(d, p - s)`, which is the cap
                // at the source: the ray runs forward from it and no further
                // back.
                add(ResultPoint(forward.y(), -forward.x()), source);
            }
        }
    } else if constexpr (is_halfplane_intersection_v<ShapeT>) {
        if (shape.empty()) {
            return std::nullopt;
        }
        // Stored as constraints already, and this is the receiver whose vertices
        // the construction never has to look at.
        for (const auto& halfplane : shape) {
            const ResultPoint source = cast(halfplane.source());
            const ResultPoint target = cast(halfplane.target());
            add(ResultPoint(target.x() - source.x(), target.y() - source.y()), source);
        }
    } else {
        // Bounded: the hull vertices counterclockwise, with the collinear ones
        // dropped, so that consecutive ones span an edge of the boundary.
        const std::vector<ResultPoint> vertices = minkowskiVertices<ResultPoint>(shape);
        if (vertices.empty()) {
            return std::nullopt;
        }
        if (vertices.size() == 1) {
            pin(vertices.front());
        } else if (vertices.size() == 2) {
            const ResultPoint& from = vertices.front();
            const ResultPoint& to = vertices.back();
            const ResultPoint along(to.x() - from.x(), to.y() - from.y());
            add(along, from);
            add(reversed(along), from);
            add(ResultPoint(along.y(), -along.x()), from);
            add(ResultPoint(-along.y(), along.x()), to);
        } else {
            for (std::size_t i = 0; i < vertices.size(); ++i) {
                const ResultPoint& from = vertices[i];
                const ResultPoint& to = vertices[(i + 1) % vertices.size()];
                add(ResultPoint(to.x() - from.x(), to.y() - from.y()), from);
            }
        }
    }
    return constraints;
}

/**
 * @brief Returns the erosion of a convex shape by any other, as a
 *        @ref HalfplaneIntersection.
 *
 * One clamp per constraint of @p a, each translated by the support point of
 * @p b in that constraint's direction:
 *
 * - No support point means the operand recedes through the constraint, so
 *   nothing satisfies it and the erosion is **empty**.
 * - An operand covering no point has no constraint to fail, and the erosion is
 *   the **whole plane** — every translate of the empty set fits in anything.
 *
 * The insertion does the rest, as it does for @ref minkowskiPolyhedralSum:
 * @ref HalfplaneIntersection::insert drops a redundant constraint, keeps the
 * tighter of two facing the same way, and notices both emptiness and a result
 * that has dropped below two dimensions, so an erosion that is a point, a
 * segment or a line comes back recognizable through `getIfPoint`,
 * `getIfSegment` or `getIfLine`.
 *
 * The operand is read only through @ref minkowskiPolyhedronOf, which is why a
 * non-convex one is free: what it reduces to for a bounded operand is that
 * operand's hull, and `A ⊖ B` is `A ⊖ hull(B)` whenever `A` is convex.
 *
 * Complexity: `O(a·b)` cross products for a receiver of `a` constraints and an
 * operand of `b` vertices, plus what the insertions cost.
 */
template <class A, class B>
constexpr auto minkowskiConvexErosion(const A& a, const B& b) {
    using ResultPoint = minkowskiErosionPoint_t<A, B>;
    using Region = HalfplaneIntersection<ResultPoint>;

    const MinkowskiPolyhedron<ResultPoint> eroder = minkowskiPolyhedronOf<ResultPoint>(b);
    if (eroder.empty) {
        return Region();  // the whole plane, which is what no constraint means
    }
    const auto constraints = minkowskiErosionConstraints<ResultPoint>(a);
    if (!constraints) {
        return Region(Convex<ResultPoint>());  // nothing fits in the empty set
    }

    Region region;
    for (const auto& constraint : *constraints) {
        const std::optional<ResultPoint> support =
            minkowskiInfimumPoint(eroder, constraint.direction);
        if (!support) {
            return Region(Convex<ResultPoint>());
        }
        const ResultPoint base(constraint.anchor.x() - support->x(),
                               constraint.anchor.y() - support->y());
        region.insert(Halfplane<ResultPoint>(
            base, ResultPoint(base.x() + constraint.direction.x(),
                              base.y() + constraint.direction.y())));
    }
    return region;
}

/**
 * @brief Returns the erosion of a half-plane by a bounded polygonal shape: the
 *        same half-plane, translated.
 *
 * The mirror of @ref minkowskiHalfplaneSum, and the same support point: with the
 * boundary running from `s` to `t`, `d = t - s`, and `b*` the vertex of the
 * operand minimizing `cross(d, ·)`, the sum is the half-plane translated by `b*`
 * and the erosion is the half-plane translated by `-b*`. A half-plane absorbs
 * whatever is bounded in both directions, so the operand's concavity, holes and
 * disconnection are as irrelevant here as they are there.
 *
 * @throws std::logic_error when the operand covers no point, whose erosion is
 *         the whole plane and not a half-plane.
 */
template <class HalfplaneT, class ShapeT>
constexpr auto minkowskiHalfplaneErosion(const HalfplaneT& halfplane, const ShapeT& shape) {
    using ResultPoint = minkowskiErosionPoint_t<HalfplaneT, ShapeT>;
    using ResultNumber = typename ResultPoint::NumberType;
    using ResultHalfplane = Halfplane<ResultPoint, typename HalfplaneT::LabelType>;

    const auto& source = halfplane.source();
    const auto& target = halfplane.target();
    const ResultNumber dx =
        detail::asNumber<ResultNumber>(target.x()) - detail::asNumber<ResultNumber>(source.x());
    const ResultNumber dy =
        detail::asNumber<ResultNumber>(target.y()) - detail::asNumber<ResultNumber>(source.y());

    bool found = false;
    ResultPoint support(ResultNumber{}, ResultNumber{});
    ResultNumber best{};
    for (const auto& vertex : shape.vertices()) {
        const ResultNumber x = detail::asNumber<ResultNumber>(vertex.x());
        const ResultNumber y = detail::asNumber<ResultNumber>(vertex.y());
        const ResultNumber side = dx * y - dy * x;  // cross(d, vertex)
        if (!found || side < best) {
            found = true;
            best = side;
            support = ResultPoint(x, y);
        }
    }
    if (!found) {
        throw std::logic_error(
            "Halfplane::minkowskiErosion by a shape that covers no point is the whole plane, "
            "which no half-plane represents");
    }
    const auto moved = [&support](const auto& point) {
        return ResultPoint(detail::asNumber<ResultNumber>(point.x()) - support.x(),
                           detail::asNumber<ResultNumber>(point.y()) - support.y());
    };
    return ResultHalfplane(moved(source), moved(target));
}

/**
 * @brief Returns the erosion of two shapes whose pair the Minkowski sum answers
 *        with a single shape.
 *
 * Dispatches to the tightest result type, in the same order and on the same
 * tests as @ref minkowskiSumOf: an empty operand, a translation, a rectangle, a
 * half-plane, or the general convex clamp. What is *not* here is the erosion of
 * a non-convex receiver by a bounded operand, which needs regions and lives in
 * @ref regularizedMinkowskiErosion — exactly as the sum's non-convex pairs live
 * in `implementation/minkowskisum.hpp`.
 */
template <class A, class B>
    requires MinkowskiSummableConcept<A, B>
constexpr auto minkowskiErosionOf(const A& a, const B& b) {
    using ResultPoint = minkowskiPoint_t<A, B>;

    if constexpr (is_shape_v<A> || is_shape_v<B>) {
        using ResultShape = Shape<ResultPoint>;
        // As for the sum: only the pair of stored alternatives decides whether
        // the erosion exists and fits the wrapper, and neither is known until
        // run time.
        const auto erode = [](const auto& left, const auto& right) -> ResultShape {
            if constexpr (requires { ResultShape(minkowskiErosionOf(left, right)); }) {
                return ResultShape(minkowskiErosionOf(left, right));
            } else {
                throw std::logic_error(
                    "Shape::minkowskiErosion is not defined for this pair of alternatives, or "
                    "its result does not fit the wrapper's point type");
            }
        };
        if constexpr (is_shape_v<A> && is_shape_v<B>) {
            return std::visit(erode, a.variant(), b.variant());
        } else if constexpr (is_shape_v<A>) {
            return std::visit([&b, &erode](const auto& left) { return erode(left, b); },
                              a.variant());
        } else {
            return std::visit([&a, &erode](const auto& right) { return erode(a, right); },
                              b.variant());
        }
    } else if constexpr (is_empty_shape_v<B>) {
        // Every translate of the empty set fits in every shape, so this is the
        // whole plane whatever the receiver -- the one answer of a shrinking
        // operation that is larger than what it shrinks.
        return minkowskiErosionRegion_t<A, B>();
    } else if constexpr (is_point_v<B>) {
        // Eroding by one point is the translation by its negation, and every
        // shape kind is closed under it, so this is the one erosion that keeps
        // the receiver's own type -- the empty shape included, which is why this
        // comes before the empty receiver below.
        return minkowskiTranslated(a, -b);
    } else if constexpr (is_empty_shape_v<A>) {
        using Region = minkowskiErosionRegion_t<A, B>;
        // Nothing fits in the empty set except the empty set.
        return coversNoPoint(b) ? Region() : Region(Convex<minkowskiErosionPoint_t<A, B>>());
    } else if constexpr (is_disk_v<B>) {
        // A disk operand reaches here only against a `Point` receiver -- an
        // `EmptyShape` one is answered above, and @ref Disk and @ref Halfplane
        // carry their own overloads -- so the only disk that fits is one
        // covering a single point, and its centre is then its own defining
        // point, exactly.
        using Region = minkowskiErosionRegion_t<A, B>;
        if (!b.isPoint()) {
            return Region(Convex<minkowskiErosionPoint_t<A, B>>());
        }
        return Region(minkowskiConvexErosion(a, b.a()));
    } else if constexpr (is_rectangle_v<A> && is_rectangle_v<B>) {
        // Two axis-aligned rectangles are the one non-trivial pair closed under
        // the erosion, as they are under the sum: an interval fits in an
        // interval by both ends at once, so the minima and the maxima subtract
        // and the result is empty exactly when a side of the operand is longer.
        using ResultRectangle = Rectangle<ResultPoint>;
        if (b.empty()) {
            throw std::logic_error(
                "Rectangle::minkowskiErosion by an empty rectangle is the whole plane, which no "
                "rectangle represents");
        }
        if (a.empty()) {
            return ResultRectangle();
        }
        const auto corner = [](const auto& left, const auto& right) {
            using ResultNumber = typename ResultPoint::NumberType;
            return ResultPoint(
                detail::asNumber<ResultNumber>(left.x()) - detail::asNumber<ResultNumber>(right.x()),
                detail::asNumber<ResultNumber>(left.y()) -
                    detail::asNumber<ResultNumber>(right.y()));
        };
        const ResultPoint low = corner(a.min(), b.min());
        const ResultPoint high = corner(a.max(), b.max());
        if (high.x() < low.x() || high.y() < low.y()) {
            return ResultRectangle();
        }
        return ResultRectangle(low, high, true);
    } else if constexpr (is_halfplane_v<A> && !UnboundedConvexConcept<B>) {
        // A half-plane absorbs anything bounded and stays one.
        return minkowskiHalfplaneErosion(a, b);
    } else {
        // Everything left is a convex receiver clamped constraint by
        // constraint -- and the pairs whose receiver is *not* convex, which the
        // concept admits only against a half-plane operand: an unbounded
        // operand fits in no bounded receiver, so the answer is the empty
        // region, and the hull the clamp reads instead of the receiver cannot
        // change it. See the file comment.
        return minkowskiConvexErosion(a, b);
    }
}

/**
 * @brief Returns the regularized erosion `closure((A ⊖ B)°)` of a bounded
 *        polygonal shape by another, as the set of regions it is.
 *
 * A @ref PolygonSet and not one @ref PolygonWithHoles, because an erosion
 * disconnects: a dumbbell eroded by anything wider than its handle is two
 * regions, for operands that are in no way degenerate. That is the one
 * structural difference from @ref regularizedMinkowskiSum, whose result is a
 * single region whenever an operand is a body.
 *
 * Two constructions, and the first is the sum's first construction turned
 * around:
 *
 * - **A convex receiver** — a `Polygon` that happens to be convex, or a region
 *   whose holes are gone and whose outer ring is — is an intersection of its own
 *   half-planes, so @ref minkowskiConvexErosion answers it in `O(a·b)` with no
 *   arrangement at all, and the bounded convex region that comes back converts
 *   to the one polygon it is. The operand's shape is irrelevant to this path;
 *   only the receiver's convexity is.
 * - **Anything else** goes through the complement identity. With `W` a box
 *   holding `A` and every `a + b` for `a ∈ A`, `b ∈ B`, and `U = closure(W° ∖
 *   A)`:
 *
 *       A ⊖ B  =  A ∖ (U ⊕ (-B)),
 *
 *   because for `x ∈ A` the translate `x ⊕ B` leaves `A` exactly when one of its
 *   points lands in `U` — every point of `A ⊕ B` is inside `W`, so no escape
 *   route avoids it — and `x ∈ U ⊕ (-B)` says exactly that. Taking the answer
 *   out of `A` is what asks for `0 ∈ B`, which the anchoring below arranges. The
 *   operand is
 *   reflected through the origin by @ref pgl::PolygonWithHoles::rotated90 and
 *   friends, which is `-B` for `k = 2`, and the sum is
 *   @ref regularizedMinkowskiSum whole: whichever of its four constructions
 *   suits the pair `(U, -B)`.
 *
 * Both differences are @ref regularizedDifference, so both halves of the answer
 * are regularized, and the lower-dimensional erosion of a corridor exactly as
 * wide as its operand comes back empty rather than as a curve no `PolygonSet`
 * holds. A receiver with no area erodes to nothing at all and is answered
 * before any of this.
 *
 * Exactness follows the rule of `booleans.hpp`: the window, the complement and
 * the sum are all built over the exact type, and only the final difference
 * converts to @p ResultPoint.
 *
 * Complexity: the sum's, on an operand pair no larger than `(a + 4, b)`, plus
 * two arrangements — `Θ(a²b²)` in the worst case, and `O(a·b)` on the convex
 * path.
 *
 * @throws std::logic_error when the operand covers no point, whose erosion is
 *         the whole plane and not a set of bounded regions.
 */
template <class ResultPoint, class ShapeA, class ShapeB>
PolygonSet<ResultPoint> regularizedMinkowskiErosion(const ShapeA& a, const ShapeB& b) {
    using ResultNumber = typename ResultPoint::NumberType;
    using SumPoint = minkowskiPoint_t<ShapeA, ShapeB>;
    using SumNumber = typename SumPoint::NumberType;
    using ExactPoint = Point<Exact1DNumber<SumNumber, SumNumber>>;
    using ExactNumber = typename ExactPoint::NumberType;

    if (coversNoPoint(b)) {
        throw std::logic_error(
            "minkowskiErosion by a shape that covers no point is the whole plane, which no "
            "PolygonSet represents");
    }
    if (!minkowskiHasArea(a)) {
        // Regularization keeps only what has area, and an erosion only shrinks:
        // a chain, a polyline and a collapsed region all erode to nothing.
        return {};
    }

    if constexpr (is_polygon_v<ShapeA> || is_polygon_with_holes_v<ShapeA>) {
        if (minkowskiIsConvex(a)) {
            const auto region = minkowskiConvexErosion(minkowskiAsConvex(a), b);
            if (region.isDegenerate()) {
                return {};  // nothing with area survives the regularization
            }
            return PolygonSet<ResultPoint>(PolygonWithHoles<ResultPoint>(
                Polygon<ResultPoint>(region.template asConvex<ResultNumber>().asPolygon())));
        }
    }

    // `A ⊖ B ⊆ A` holds only when the operand covers the origin, and the last
    // step below is a difference from the receiver, which would truncate the
    // answer for an operand that does not: the erosion by an operand placed
    // elsewhere is the same set translated. So the operand is moved onto one of
    // its own vertices first — a vertex is a point of it, where a corner of its
    // bounding box need not be — and the answer is moved back at the end.
    const auto operandVertices = b.vertices();
    const auto anchor = *operandVertices.begin();
    const auto centered = minkowskiTranslated(b, -anchor);

    // The window. It has to hold the receiver, so that its complement inside it
    // is the receiver's own boundary, and every translate `a + b`, so that a
    // point escaping the receiver escapes into that complement rather than out
    // of the window. One unit of slack keeps the two boundaries apart.
    const auto exact = [](const auto& value) { return detail::asNumber<ExactNumber>(value); };
    const auto boxA = a.bbox();
    const auto boxB = centered.bbox();
    const ExactNumber one = static_cast<ExactNumber>(1);
    const ExactPoint low(
        std::min(exact(boxA.min().x()), exact(boxA.min().x()) + exact(boxB.min().x())) - one,
        std::min(exact(boxA.min().y()), exact(boxA.min().y()) + exact(boxB.min().y())) - one);
    const ExactPoint high(
        std::max(exact(boxA.max().x()), exact(boxA.max().x()) + exact(boxB.max().x())) + one,
        std::max(exact(boxA.max().y()), exact(boxA.max().y()) + exact(boxB.max().y())) + one);
    const Rectangle<ExactPoint> window(low, high, true);

    const PolygonSet<ExactPoint> outside =
        regularizedDifference<ExactPoint>(window.asPolygon(), booleanOperand(a));
    const PolygonSet<ExactPoint> forbidden =
        setMinkowskiSum<ExactPoint>(outside, centered.rotated90(2));
    PolygonSet<ResultPoint> erosion =
        regularizedDifference<ResultPoint>(booleanOperand(a), forbidden);
    erosion -= ResultPoint(asNumber<ResultNumber>(anchor.x()), asNumber<ResultNumber>(anchor.y()));
    return erosion;
}

}  // namespace detail

// -----------------------------------------------------------------------------
// Member entry points: the pairs whose sum is one shape
//
// Each shape forwards to the same dispatcher, over the same pairs its
// minkowskiSum accepts, since MinkowskiSummableConcept gates both.

#define PGL_DEFINE_MINKOWSKI_EROSION(SHAPE)                                                    \
    template <class PointType, class LabelType>                                                \
    template <class OtherShape>                                                                \
        requires MinkowskiSummableConcept<SHAPE<PointType, LabelType>, OtherShape>             \
    constexpr auto SHAPE<PointType, LabelType>::minkowskiErosion(const OtherShape& other)       \
        const {                                                                                \
        return detail::minkowskiErosionOf(*this, other);                                       \
    }

PGL_DEFINE_MINKOWSKI_EROSION(Segment)
PGL_DEFINE_MINKOWSKI_EROSION(OrientedSegment)
PGL_DEFINE_MINKOWSKI_EROSION(Line)
PGL_DEFINE_MINKOWSKI_EROSION(OrientedLine)
PGL_DEFINE_MINKOWSKI_EROSION(Ray)
PGL_DEFINE_MINKOWSKI_EROSION(Halfplane)
PGL_DEFINE_MINKOWSKI_EROSION(Rectangle)
PGL_DEFINE_MINKOWSKI_EROSION(Triangle)
PGL_DEFINE_MINKOWSKI_EROSION(Disk)
PGL_DEFINE_MINKOWSKI_EROSION(Convex)
PGL_DEFINE_MINKOWSKI_EROSION(Polygon)
PGL_DEFINE_MINKOWSKI_EROSION(Polyline)
PGL_DEFINE_MINKOWSKI_EROSION(PolygonWithHoles)
PGL_DEFINE_MINKOWSKI_EROSION(PolygonSet)
PGL_DEFINE_MINKOWSKI_EROSION(HalfplaneIntersection)

#undef PGL_DEFINE_MINKOWSKI_EROSION

template <class Number, class Label>
template <class OtherShape>
    requires MinkowskiSummableConcept<Point<Number, Label>, OtherShape>
constexpr auto Point<Number, Label>::minkowskiErosion(const OtherShape& other) const {
    return detail::minkowskiErosionOf(*this, other);
}

template <class PointType>
template <class OtherShape>
    requires MinkowskiSummableConcept<EmptyShape<PointType>, OtherShape>
constexpr auto EmptyShape<PointType>::minkowskiErosion(const OtherShape& other) const {
    return detail::minkowskiErosionOf(*this, other);
}

template <class PointType, class LabelType, class Storage>
template <class OtherShape>
    requires MinkowskiSummableConcept<MonotoneChain<PointType, LabelType, Storage>, OtherShape>
constexpr auto MonotoneChain<PointType, LabelType, Storage>::minkowskiErosion(
    const OtherShape& other) const {
    return detail::minkowskiErosionOf(*this, other);
}

template <class PointType>
template <class OtherShape>
    requires MinkowskiSummableConcept<Shape<PointType>, OtherShape>
constexpr auto Shape<PointType>::minkowskiErosion(const OtherShape& other) const {
    return detail::minkowskiErosionOf(*this, other);
}

// -----------------------------------------------------------------------------
// A convex receiver eroded by a bounded operand the sum answers elsewhere
//
// These are the pairs minkowskiSum hands to the higher-ranked operand, because
// the sum is commutative and the operand's concavity decides the result type.
// The erosion is not commutative, and the *receiver's* convexity decides it
// here: only the operand's support function is read, so a non-convex one costs
// nothing and the answer is the same convex region every other convex receiver
// gets.

#define PGL_DEFINE_CONVEX_MINKOWSKI_EROSION(SHAPE)                                             \
    template <class PointType_, class TLabel>                                                  \
    template <class OtherShape>                                                                \
        requires (!MinkowskiSummableConcept<SHAPE<PointType_, TLabel>, OtherShape> &&          \
                  BoundedPolygonalConcept<OtherShape>)                                         \
    constexpr auto SHAPE<PointType_, TLabel>::minkowskiErosion(const OtherShape& other)        \
        const {                                                                                \
        return detail::minkowskiConvexErosion(*this, other);                                   \
    }

PGL_DEFINE_CONVEX_MINKOWSKI_EROSION(Segment)
PGL_DEFINE_CONVEX_MINKOWSKI_EROSION(OrientedSegment)
PGL_DEFINE_CONVEX_MINKOWSKI_EROSION(Rectangle)
PGL_DEFINE_CONVEX_MINKOWSKI_EROSION(Triangle)
PGL_DEFINE_CONVEX_MINKOWSKI_EROSION(Convex)

#undef PGL_DEFINE_CONVEX_MINKOWSKI_EROSION

// -----------------------------------------------------------------------------
// The region-valued receivers
//
// One definition each, over the whole operand family: unlike the sum, whose
// result type turns on whether an operand is a body, every one of these erodes
// to a set of regions, and the construction reads the pair the same way whatever
// the operand is.

#define PGL_DEFINE_REGION_MINKOWSKI_EROSION(RECEIVER)                                          \
    template <class PointType_, class TLabel>                                                  \
    template <class ResultNumber, class OtherShape>                                            \
        requires (!MinkowskiSummableConcept<RECEIVER<PointType_, TLabel>, OtherShape> &&        \
                  BoundedPolygonalConcept<OtherShape>)                                         \
    PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>                            \
    RECEIVER<PointType_, TLabel>::minkowskiErosion(const OtherShape& other) const {            \
        return detail::regularizedMinkowskiErosion<                                            \
            Point<ResultNumber, typename PointType_::LabelType>>(*this, other);                \
    }

PGL_DEFINE_REGION_MINKOWSKI_EROSION(Polygon)
PGL_DEFINE_REGION_MINKOWSKI_EROSION(PolygonWithHoles)
PGL_DEFINE_REGION_MINKOWSKI_EROSION(Polyline)
PGL_DEFINE_REGION_MINKOWSKI_EROSION(PolygonSet)

#undef PGL_DEFINE_REGION_MINKOWSKI_EROSION

// The chain carries the same definition, with its third template parameter. Its
// sum keeps a polygon-valued overload set of its own for a convex operand,
// which the erosion has no use for: a chain has no area, so it erodes to the
// empty set whatever it is eroded by, and the regularized engine says so
// without building anything.

template <class PointType_, class TLabel, class Storage>
template <class ResultNumber, class OtherShape>
    requires (!MinkowskiSummableConcept<MonotoneChain<PointType_, TLabel, Storage>, OtherShape> &&
              BoundedPolygonalConcept<OtherShape>)
PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
MonotoneChain<PointType_, TLabel, Storage>::minkowskiErosion(const OtherShape& other) const {
    return detail::regularizedMinkowskiErosion<Point<ResultNumber, typename PointType_::LabelType>>(
        *this, other);
}

// -----------------------------------------------------------------------------
// The curved pairs, mirroring the two the sum can answer
//
// Both carry a ResultNumber of their own, for the reason
// @ref Disk::minkowskiSum(const OtherDisk&) const does: a disk's radius is a
// square root of what it stores, so these leave the lattice where every other
// erosion stays on it.

template <class PointType_, class TLabel>
template <class ResultNumber, DiskConcept OtherDisk>
std::optional<Disk<Point<ResultNumber, typename Disk<PointType_, TLabel>::PointLabelType>>>
Disk<PointType_, TLabel>::minkowskiErosion(const OtherDisk& other) const {
    using ResultPoint = Point<ResultNumber, PointLabelType>;

    const ResultNumber radii = radius<ResultNumber>() - other.template radius<ResultNumber>();
    if (radii < ResultNumber{}) {
        return std::nullopt;  // the operand is wider than the receiver
    }
    const auto leftCenter = center<ResultNumber>();
    const auto rightCenter = other.template center<ResultNumber>();
    return Disk<ResultPoint>(ResultPoint(leftCenter.x() - rightCenter.x(),
                                         leftCenter.y() - rightCenter.y()),
                             radii);
}

template <class PointType_, class TLabel>
template <class ResultNumber, DiskConcept OtherDisk>
Halfplane<Point<ResultNumber, typename PointType_::LabelType>>
Halfplane<PointType_, TLabel>::minkowskiErosion(const OtherDisk& other) const {
    using ResultPoint = Point<ResultNumber, typename PointType_::LabelType>;

    const ResultNumber dx = detail::asNumber<ResultNumber>(target().x()) -
                            detail::asNumber<ResultNumber>(source().x());
    const ResultNumber dy = detail::asNumber<ResultNumber>(target().y()) -
                            detail::asNumber<ResultNumber>(source().y());
    // Reported the way Disk::radius reports it: an exact result type has no
    // square root to offer, and says so rather than rounding silently.
    if constexpr (!requires(ResultNumber v) { std::sqrt(v); }) {
        throw std::runtime_error("std::sqrt is not available for the requested ResultNumber type");
    } else {
        const ResultNumber length = std::sqrt(dx * dx + dy * dy);

        // The sum slides the boundary out by the disk's support point in the
        // outward normal `(dy, -dx)/|d|`; the erosion slides it in by the same
        // point, which is the whole difference between the two.
        const auto center = other.template center<ResultNumber>();
        const ResultNumber radius = other.template radius<ResultNumber>();
        const ResultNumber offsetX = center.x() + radius * dy / length;
        const ResultNumber offsetY = center.y() - radius * dx / length;

        const auto moved = [&offsetX, &offsetY](const auto& point) {
            return ResultPoint(detail::asNumber<ResultNumber>(point.x()) - offsetX,
                               detail::asNumber<ResultNumber>(point.y()) - offsetY);
        };
        return Halfplane<ResultPoint>(moved(source()), moved(target()));
    }
}

template <class PointType_, class TLabel>
template <class ResultNumber, HalfplaneConcept OtherHalfplane>
EmptyShape<Point<ResultNumber, typename Disk<PointType_, TLabel>::PointLabelType>>
Disk<PointType_, TLabel>::minkowskiErosion(const OtherHalfplane& other) const {
    // A half-plane is unbounded and a disk is not, so no translate of one fits:
    // the one pair whose erosion is the empty set by its types alone.
    (void)other;
    return EmptyShape<Point<ResultNumber, PointLabelType>>{};
}

}  // namespace pgl
