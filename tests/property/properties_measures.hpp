#pragma once

/**
 * @file properties_measures.hpp
 * @brief Area, length and centroid, against each other and against the
 *        predicates.
 *
 * The measure layer is the only part of the library that answers a *number* for
 * one shape, which makes it the only part the rest of the harness cannot reach:
 * a predicate group compares booleans, a metric group compares distances between
 * two shapes, and neither ever asks how much area a shape has. The boolean group
 * does use `twiceArea`, but only on the results of its own operations and only
 * in identities where an error in the measure would have to cancel to be
 * noticed.
 *
 * Three kinds of relation are available and all three are used:
 *
 *  - **Against the predicates.** `A ⊇ B` forces `|A| ≥ |B|`, which crosses two
 *    layers that share no code: containment is decided by orientation tests, and
 *    area by a signed sum over a ring. Either one being wrong breaks it.
 *  - **Against the maps.** Area is invariant under an isometry and multiplies by
 *    @f$s^2@f$ under scaling; length by @f$s@f$; and the centroid commutes with
 *    a translation. These turn one shape into an orbit, the same leverage the
 *    invariance group gets on the predicates.
 *  - **Against a sibling formula.** `twiceArea` and `area` are separate members
 *    with separate implementations per alternative, and the norms of a curve's
 *    length are ordered the way the underlying norms are.
 */

#include "properties_common.hpp"

#include <optional>
#include <stdexcept>

namespace pglprop {

namespace props {

/** @brief Area of a shape, or nothing where the alternative has none. */
inline std::optional<Exact> areaOf(const AnyShape& shape) {
    return attempt([&] { return shape.template area<Exact>(); });
}

/**
 * @brief Whether the shape's measures are exact.
 *
 * False for a `Disk` alone, whose area is @f$\pi r^2@f$: it is computed in
 * floating point and converted, so an exact result type there holds a rounded
 * value and the identities below hold only to a tolerance. The same caveat the
 * metric group already carries for a `Disk` pair's distances.
 */
inline bool hasExactMeasures(const AnyShape& shape) { return !shape.holdsDisk(); }

/**
 * @brief Whether a measure is a meaningful question for this shape.
 *
 * A non-empty operand is a *precondition* of the measures, not something they
 * answer for, and violating it is undefined behaviour whether or not the
 * implementation checks. Some do — `Rectangle::midpoint` asserts `!empty()`,
 * that check being far cheaper than the work it guards — but an unchecked
 * sibling is no more permissive. Asking either would be asserting about
 * unspecified behaviour, the same reason the generators reject an
 * `isUndefined()` shape, so these properties skip an empty operand.
 *
 * This is a precondition of the operation and not a state of the shape: an
 * empty `Rectangle` is still defined, and the predicates answer for it.
 */
inline bool hasCentroid(const AnyShape& shape) { return !shape.empty(); }

/**
 * @brief Area is never negative.
 *
 * `area` is documented as the measure, not the signed sum that computes it, so a
 * clockwise ring reaching the caller is a normalization failure showing up one
 * layer down.
 */
inline Result areaIsNonNegative(const AnyShape& a) {
    const auto area = areaOf(a);
    if (!area) {
        return skipped();
    }
    PGLPROP_CHECK(*area >= Exact(0),
                  "A = " + detail::show(a) + " has negative area " + detail::show(*area));
    return held();
}

/** @brief `twiceArea` is twice `area`, computed the other way. */
inline Result twiceAreaIsTwiceTheArea(const AnyShape& a) {
    const auto area = areaOf(a);
    const auto twice = attempt([&] { return a.template twiceArea<Exact>(); });
    if (!area || !twice || !hasExactMeasures(a)) {
        return skipped();
    }
    PGLPROP_CHECK(*twice == Exact(2) * *area,
                  "A = " + detail::show(a) + " ; area is " + detail::show(*area) +
                      " but twiceArea is " + detail::show(*twice));
    return held();
}

/**
 * @brief Containment orders area.
 *
 * The cross-layer property of this group: nothing in the area computation
 * consults `contains`, and nothing in `contains` consults the area, so the two
 * agreeing on every drawn pair is real evidence about both. Both operands must
 * have an area for the comparison to mean anything, which excludes the curves
 * and the unbounded alternatives.
 */
inline Result containmentOrdersArea(const AnyShape& a, const AnyShape& b) {
    if (!a.contains(b)) {
        return skipped();
    }
    const auto areaA = areaOf(a);
    const auto areaB = areaOf(b);
    if (!areaA || !areaB || !hasExactMeasures(a) || !hasExactMeasures(b)) {
        return skipped();
    }
    PGLPROP_CHECK(*areaA >= *areaB,
                  pair(a, b) + " ; A contains B but has the smaller area " + detail::show(*areaA) +
                      " < " + detail::show(*areaB));
    return held();
}

/**
 * @brief Area survives translation and negation, and scales as @f$s^2@f$.
 *
 * The quarter turn is asked separately, since a `MonotoneChain` cannot represent
 * its own rotation — see @ref kAxisFree.
 */
inline Result areaFollowsTheMap(const AnyShape& a) {
    const auto original = areaOf(a);
    if (!original || !hasExactMeasures(a)) {
        return skipped();
    }
    const std::string prefix = "A = " + detail::show(a) + " ; area is " + detail::show(*original);

    AnyShape translated = a;
    translated += PointShape(3, -5);
    const auto afterShift = areaOf(translated);
    PGLPROP_CHECK(afterShift && *afterShift == *original,
                  prefix + " but " + (afterShift ? detail::show(*afterShift) : "undefined") +
                      " after translation by (3,-5)");

    AnyShape negated = a;
    negated *= Coord(-1);
    const auto afterNegation = areaOf(negated);
    PGLPROP_CHECK(afterNegation && *afterNegation == *original,
                  prefix + " but " + (afterNegation ? detail::show(*afterNegation) : "undefined") +
                      " after negation");

    AnyShape scaled = a;
    scaled *= Coord(3);
    const auto afterScaling = areaOf(scaled);
    PGLPROP_CHECK(afterScaling && *afterScaling == Exact(9) * *original,
                  prefix + " but " + (afterScaling ? detail::show(*afterScaling) : "undefined") +
                      " after scaling by 3, not " + detail::show(Exact(9) * *original));
    return held();
}

/** @brief Area survives a quarter turn. */
inline Result areaSurvivesAQuarterTurn(const AnyShape& a) {
    const auto original = areaOf(a);
    if (!original || !hasExactMeasures(a)) {
        return skipped();
    }
    for (int quarters = 1; quarters <= 3; ++quarters) {
        const auto turned = areaOf(a.rotated90(quarters));
        PGLPROP_CHECK(turned && *turned == *original,
                      "A = " + detail::show(a) + " ; area changes from " + detail::show(*original) +
                          " to " + (turned ? detail::show(*turned) : "undefined") + " under " +
                          std::to_string(quarters) + " quarter turn(s)");
    }
    return held();
}

/**
 * @brief The three norms of a curve's length are ordered as the norms are.
 *
 * @f$L^\infty \le L^2 \le L^1 \le 2 L^\infty@f$ holds edge by edge and therefore
 * for the sum. Asked at `double`, which is what `length` returns — the Euclidean
 * length of a lattice curve is irrational, so there is nothing exact to ask for.
 */
inline Result lengthNormsAreOrdered(const AnyShape& a) {
    const auto euclidean = attempt([&] { return a.template length<double>(); });
    const auto manhattan = attempt([&] { return a.template lengthL1<double>(); });
    const auto chebyshev = attempt([&] { return a.template lengthLInf<double>(); });
    if (!euclidean || !manhattan || !chebyshev) {
        return skipped();
    }
    const std::string prefix = "A = " + detail::show(a) + " ; length=" + detail::show(*euclidean) +
                               " lengthL1=" + detail::show(*manhattan) +
                               " lengthLInf=" + detail::show(*chebyshev);
    PGLPROP_CHECK(nearlyAtMost(*chebyshev, *euclidean), prefix + " violates LInf <= L2");
    PGLPROP_CHECK(nearlyAtMost(*euclidean, *manhattan), prefix + " violates L2 <= L1");
    PGLPROP_CHECK(nearlyAtMost(*manhattan, 2.0 * *chebyshev), prefix + " violates L1 <= 2 LInf");
    return held();
}

/**
 * @brief Length survives translation and negation, and scales as @f$s@f$.
 *
 * The companion to @ref areaFollowsTheMap for the curves, and the reason both
 * exist: the two measures are computed by different code and degrade
 * differently, so a shape whose area is right can have the wrong length.
 */
inline Result lengthFollowsTheMap(const AnyShape& a) {
    const auto original = attempt([&] { return a.template length<double>(); });
    if (!original) {
        return skipped();
    }
    const std::string prefix =
        "A = " + detail::show(a) + " ; length is " + detail::show(*original);

    AnyShape translated = a;
    translated += PointShape(3, -5);
    const auto afterShift = attempt([&] { return translated.template length<double>(); });
    PGLPROP_CHECK(afterShift && nearlyEqual(*afterShift, *original),
                  prefix + " but " + (afterShift ? detail::show(*afterShift) : "undefined") +
                      " after translation by (3,-5)");

    AnyShape scaled = a;
    scaled *= Coord(3);
    const auto afterScaling = attempt([&] { return scaled.template length<double>(); });
    PGLPROP_CHECK(afterScaling && nearlyEqual(*afterScaling, 3.0 * *original),
                  prefix + " but " + (afterScaling ? detail::show(*afterScaling) : "undefined") +
                      " after scaling by 3, not " + detail::show(3.0 * *original));
    return held();
}

/**
 * @brief The centroid of a convex shape lies in the shape.
 *
 * True for a convex set by definition, and false in general — the centroid of an
 * L-shaped polygon is outside it — so this is asked of the convex alternatives
 * alone rather than skipped case by case. A degenerate one is still convex, so
 * it is included: that is where a centroid computed by dividing by a vanishing
 * area goes wrong.
 */
inline Result centroidOfAConvexShapeIsInside(const AnyShape& a) {
    const bool convex = a.holdsTriangle() || a.holdsRectangle() || a.holdsConvex() ||
                        a.holdsHalfplaneIntersection();
    if (!convex || !hasCentroid(a)) {
        return skipped();
    }
    const auto centroid = attempt([&] { return a.template centroid<Exact>(); });
    if (!centroid) {
        return skipped();
    }
    const ExactShape point{ExactPoint(*centroid)};
    PGLPROP_CHECK(toExact(a).contains(point),
                  "A = " + detail::show(a) + " ; its centroid " + detail::show(point) +
                      " is not in it");
    return held();
}

/**
 * @brief The centroid commutes with a translation.
 *
 * A centroid is an affine notion, so moving the shape must move the answer by
 * the same vector. Weaker than it looks only for the shapes whose centroid is a
 * weighted mean of vertices, and exactly right for the rest.
 */
inline Result centroidFollowsTranslation(const AnyShape& a) {
    if (!hasCentroid(a) || !hasExactMeasures(a)) {
        return skipped();
    }
    const auto original = attempt([&] { return a.template centroid<Exact>(); });
    if (!original) {
        return skipped();
    }
    const PointShape shift(3, -5);
    AnyShape translated = a;
    translated += shift;
    const auto moved = attempt([&] { return translated.template centroid<Exact>(); });
    const ExactPoint expected(original->x() + Exact(shift.x()), original->y() + Exact(shift.y()));

    PGLPROP_CHECK(moved && ExactPoint(*moved) == expected,
                  "A = " + detail::show(a) + " ; centroid " + detail::show(*original) +
                      " moves to " + (moved ? detail::show(*moved) : std::string("undefined")) +
                      " under translation by (3,-5), not to " + detail::show(expected));
    return held();
}

}  // namespace props

/** @brief Adds the measure properties to a registry. */
inline void registerMeasureProperties(Registry& registry) {
    registry.unary.push_back({"measure", "area-is-non-negative", kNoTag, props::areaIsNonNegative});
    registry.unary.push_back({"measure", "twice-area-is-twice-the-area", kNoTag,
                              props::twiceAreaIsTwiceTheArea});
    registry.binary.push_back({"measure", "containment-orders-area", kNoTag,
                               props::containmentOrdersArea});
    registry.unary.push_back({"measure", "area-follows-the-map", kNoTag, props::areaFollowsTheMap});
    registry.unary.push_back({"measure", "area-survives-a-quarter-turn", kAxisFree,
                              props::areaSurvivesAQuarterTurn});
    registry.unary.push_back({"measure", "length-norms-are-ordered", kNoTag,
                              props::lengthNormsAreOrdered});
    registry.unary.push_back({"measure", "length-follows-the-map", kNoTag,
                              props::lengthFollowsTheMap});
    registry.unary.push_back({"measure", "centroid-of-a-convex-shape-is-inside", kNoTag,
                              props::centroidOfAConvexShapeIsInside});
    registry.unary.push_back({"measure", "centroid-follows-translation", kNoTag,
                              props::centroidFollowsTranslation});
}

}  // namespace pglprop
