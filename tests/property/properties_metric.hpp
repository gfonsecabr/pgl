#pragma once

/**
 * @file properties_metric.hpp
 * @brief Agreement between the distance functions and the predicates.
 *
 * Distances and predicates are computed by completely separate code — one
 * family in `implementation/distance.hpp` and its L1/LInf siblings, the other
 * across the seven predicate files — yet they answer overlapping questions. Two
 * shapes are at distance zero exactly when they intersect, so each family is a
 * full oracle for part of the other. That makes these the cheapest
 * cross-validation available anywhere in the library.
 *
 * ## Why floating point here
 *
 * The distance between two shapes with integer coordinates is generally
 * irrational — and for a `Disk` operand the library says so explicitly, forcing
 * the result into `detail::floating_result_t` no matter what is asked for. So
 * these properties run in `double`, and the comparisons are written accordingly:
 * a *tolerance* for the ordering and symmetry relations, and exact `== 0` only
 * for the vanishing tests, where the value is a true zero rather than a rounded
 * one. On the small grids the harness draws from, the smallest nonzero squared
 * distance is on the order of 1e-3 — nowhere near where double rounding could
 * turn it into a zero, so the zero tests are safe.
 */

#include "properties_common.hpp"

#include <algorithm>
#include <cmath>

namespace pglprop {

namespace props {

/**
 * @brief Whether `squaredHausdorffDistance` is defined for this alternative.
 *
 * Documented as the pairs among `Point`, `Segment`, `OrientedSegment`,
 * `Rectangle`, `Triangle` and `Convex` — all bounded and convex, so a directed
 * distance is always attained at a vertex. Everything else throws, so the
 * Hausdorff properties select on this rather than catching.
 */
inline bool supportsHausdorff(const AnyShape& shape) {
    return shape.isPoint() || shape.isSegment() || shape.isOrientedSegment() ||
           shape.isRectangle() || shape.isTriangle() || shape.isConvex();
}

// -------------------------------------------------------------------- domain

/**
 * @brief The three metrics are defined for the same pairs.
 *
 * `squaredDistance`, `distanceL1` and `distanceLInf` answer the same question in
 * three metrics, so a pair one of them handles and another rejects is a gap in
 * the family rather than a deliberate restriction. Reported once here so that a
 * missing overload does not surface as a failure of every property that happens
 * to call the metric it is missing from.
 */
inline Result distanceFamiliesShareADomain(const AnyShape& a, const AnyShape& b) {
    if (a.empty() || b.empty()) {
        return skipped();
    }
    const bool euclidean = isSupported([&] { return a.squaredDistance(b); });
    const bool l1 = isSupported([&] { return a.distanceL1(b); });
    const bool lInf = isSupported([&] { return a.distanceLInf(b); });

    PGLPROP_CHECK(euclidean == l1 && l1 == lInf,
                  pair(a, b) + " ; defined for this pair: squaredDistance=" +
                      detail::show(euclidean) + " distanceL1=" + detail::show(l1) +
                      " distanceLInf=" + detail::show(lInf));
    return held();
}

// ------------------------------------------------------------------ symmetry

inline Result squaredDistanceIsSymmetric(const AnyShape& a, const AnyShape& b) {
    if (a.empty() || b.empty()) {
        return skipped();
    }
    const auto forward = attempt([&] { return a.squaredDistance(b); });
    const auto backward = attempt([&] { return b.squaredDistance(a); });
    if (!forward || !backward) {
        return skipped();
    }
    PGLPROP_CHECK(nearlyEqual(*forward, *backward),
                  pair(a, b) + " ; A.squaredDistance(B)=" + detail::show(*forward) +
                      " but B.squaredDistance(A)=" + detail::show(*backward));
    return held();
}

inline Result l1DistanceIsSymmetric(const AnyShape& a, const AnyShape& b) {
    if (a.empty() || b.empty()) {
        return skipped();
    }
    const auto forward = attempt([&] { return a.distanceL1(b); });
    const auto backward = attempt([&] { return b.distanceL1(a); });
    if (!forward || !backward) {
        return skipped();
    }
    PGLPROP_CHECK(nearlyEqual(*forward, *backward),
                  pair(a, b) + " ; A.distanceL1(B)=" + detail::show(*forward) +
                      " but B.distanceL1(A)=" + detail::show(*backward));
    return held();
}

inline Result lInfDistanceIsSymmetric(const AnyShape& a, const AnyShape& b) {
    if (a.empty() || b.empty()) {
        return skipped();
    }
    const auto forward = attempt([&] { return a.distanceLInf(b); });
    const auto backward = attempt([&] { return b.distanceLInf(a); });
    if (!forward || !backward) {
        return skipped();
    }
    PGLPROP_CHECK(nearlyEqual(*forward, *backward),
                  pair(a, b) + " ; A.distanceLInf(B)=" + detail::show(*forward) +
                      " but B.distanceLInf(A)=" + detail::show(*backward));
    return held();
}

// ------------------------------------------------- distance against predicate

/**
 * @brief Zero distance and intersection are the same question.
 *
 * The strongest single property in the harness: it makes the whole distance
 * family an oracle for `intersects` over every pair, and `intersects` an oracle
 * for the vanishing of all three distances.
 */
inline Result zeroDistanceMeansIntersecting(const AnyShape& a, const AnyShape& b) {
    if (a.empty() || b.empty()) {
        return skipped();
    }
    const auto squared = attempt([&] { return a.squaredDistance(b); });
    const auto l1 = attempt([&] { return a.distanceL1(b); });
    const auto lInf = attempt([&] { return a.distanceLInf(b); });
    const bool meets = a.intersects(b);

    if (squared) {
        PGLPROP_CHECK(meets == (*squared == 0.0),
                      pair(a, b) + " ; A.intersects(B)=" + detail::show(meets) +
                          " but squaredDistance=" + detail::show(*squared));
    }
    if (l1) {
        PGLPROP_CHECK(meets == (*l1 == 0.0),
                      pair(a, b) + " ; A.intersects(B)=" + detail::show(meets) +
                          " but distanceL1=" + detail::show(*l1));
    }
    if (lInf) {
        PGLPROP_CHECK(meets == (*lInf == 0.0),
                      pair(a, b) + " ; A.intersects(B)=" + detail::show(meets) +
                          " but distanceLInf=" + detail::show(*lInf));
    }
    return (squared || l1 || lInf) ? held() : skipped();
}

/** @brief A contained non-empty shape is at distance zero. */
inline Result containmentMeansZeroDistance(const AnyShape& a, const AnyShape& b) {
    if (b.empty() || !a.contains(b)) {
        return skipped();
    }
    const auto squared = attempt([&] { return a.squaredDistance(b); });
    if (!squared) {
        return skipped();
    }
    PGLPROP_CHECK(*squared == 0.0,
                  pair(a, b) + " ; A.contains(B) but A.squaredDistance(B)=" +
                      detail::show(*squared));
    return held();
}

/**
 * @brief The three metrics stand in the usual order.
 *
 * For any single pair of points @f$L_\infty \le L_2 \le L_1 \le 2 L_\infty@f$,
 * and each shape distance is a minimum of its metric over the same set of point
 * pairs — so evaluating the chain at whichever pair minimizes the *larger* side
 * carries every inequality over to the minima.
 */
inline Result metricsAreOrdered(const AnyShape& a, const AnyShape& b) {
    if (a.empty() || b.empty()) {
        return skipped();
    }
    const auto squared = attempt([&] { return a.squaredDistance(b); });
    const auto l1 = attempt([&] { return a.distanceL1(b); });
    const auto lInf = attempt([&] { return a.distanceLInf(b); });
    if (!squared || !l1 || !lInf) {
        return skipped();
    }
    const double euclidean = std::sqrt(*squared);

    const std::string values = " ; LInf=" + detail::show(*lInf) + " L2=" +
                               detail::show(euclidean) + " L1=" + detail::show(*l1);
    PGLPROP_CHECK(nearlyAtMost(*lInf, euclidean), pair(a, b) + values);
    PGLPROP_CHECK(nearlyAtMost(euclidean, *l1), pair(a, b) + values);
    PGLPROP_CHECK(nearlyAtMost(*l1, 2.0 * *lInf), pair(a, b) + values);
    return held();
}

// ----------------------------------------------------------------- Hausdorff

/** @brief The Hausdorff distance is symmetric by definition. */
inline Result hausdorffIsSymmetric(const AnyShape& a, const AnyShape& b) {
    if (!supportsHausdorff(a) || !supportsHausdorff(b) || a.empty() || b.empty()) {
        return skipped();
    }
    const Exact forward = a.template squaredHausdorffDistance<Exact>(b);
    const Exact backward = b.template squaredHausdorffDistance<Exact>(a);
    PGLPROP_CHECK(forward == backward,
                  pair(a, b) + " ; A.squaredHausdorffDistance(B)=" + detail::show(forward) +
                      " but B.squaredHausdorffDistance(A)=" + detail::show(backward));
    return held();
}

/**
 * @brief The Hausdorff distance dominates the nearest-point distance.
 *
 * One is a maximum of point-to-set distances and the other their minimum, over
 * the same two sets.
 */
inline Result hausdorffDominatesDistance(const AnyShape& a, const AnyShape& b) {
    if (!supportsHausdorff(a) || !supportsHausdorff(b) || a.empty() || b.empty()) {
        return skipped();
    }
    const Exact hausdorff = a.template squaredHausdorffDistance<Exact>(b);
    const Exact nearest = a.template squaredDistance<Exact>(b);
    PGLPROP_CHECK(nearest <= hausdorff,
                  pair(a, b) + " ; squaredDistance=" + detail::show(nearest) +
                      " exceeds squaredHausdorffDistance=" + detail::show(hausdorff));
    return held();
}

/**
 * @brief Hausdorff distance zero means the two point sets coincide.
 *
 * Both sets are closed, so a vanishing Hausdorff distance is set equality — and
 * set equality is mutual containment, which is how the predicates spell it. A
 * genuinely independent cross-check: no code is shared between the two sides.
 */
inline Result vanishingHausdorffMeansEqualSets(const AnyShape& a, const AnyShape& b) {
    if (!supportsHausdorff(a) || !supportsHausdorff(b) || a.empty() || b.empty()) {
        return skipped();
    }
    const bool coincide = a.template squaredHausdorffDistance<Exact>(b) == Exact(0);
    const bool mutualContainment = a.contains(b) && b.contains(a);
    PGLPROP_CHECK(coincide == mutualContainment,
                  pair(a, b) + " ; squaredHausdorffDistance==0 is " + detail::show(coincide) +
                      " but mutual containment is " + detail::show(mutualContainment));
    return held();
}

}  // namespace props

/** @brief Adds the distance-versus-predicate properties to a registry. */
inline void registerMetricProperties(Registry& registry) {
    registry.binary.push_back({"metric", "distance-families-share-a-domain", kNoTag,
                               props::distanceFamiliesShareADomain});
    registry.binary.push_back({"metric", "squared-distance-is-symmetric", kNoTag,
                               props::squaredDistanceIsSymmetric});
    registry.binary.push_back({"metric", "l1-distance-is-symmetric", kNoTag,
                               props::l1DistanceIsSymmetric});
    registry.binary.push_back({"metric", "linf-distance-is-symmetric", kNoTag,
                               props::lInfDistanceIsSymmetric});
    registry.binary.push_back({"metric", "zero-distance-means-intersecting", kNoTag,
                               props::zeroDistanceMeansIntersecting});
    registry.binary.push_back({"metric", "containment-means-zero-distance", kNoTag,
                               props::containmentMeansZeroDistance});
    registry.binary.push_back({"metric", "metrics-are-ordered", kNoTag, props::metricsAreOrdered});
    registry.binary.push_back({"metric", "hausdorff-is-symmetric", kNoTag,
                               props::hausdorffIsSymmetric});
    registry.binary.push_back({"metric", "hausdorff-dominates-distance", kNoTag,
                               props::hausdorffDominatesDistance});
    registry.binary.push_back({"metric", "vanishing-hausdorff-means-equal-sets", kNoTag,
                               props::vanishingHausdorffMeansEqualSets});
}

}  // namespace pglprop
