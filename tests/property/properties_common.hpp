#pragma once

/**
 * @file properties_common.hpp
 * @brief Helpers shared by the property groups.
 *
 * Small enough to be uninteresting, but they have to be shared: the failure
 * details all render operands the same way, and the floating-point comparisons
 * all need the same tolerance, or the reports and the verdicts stop being
 * comparable across groups.
 */

#include "framework.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <string>

namespace pglprop {

/** @brief The property implementations, one group per header. */
namespace props {

/** @brief Renders both operands as pgl prints them, for a failure detail. */
inline std::string pair(const AnyShape& a, const AnyShape& b) {
    return "A = " + detail::show(a) + " ; B = " + detail::show(b);
}

/**
 * @brief Whether a shape's point set is connected.
 *
 * `separates` asks whether @f$B \setminus A@f$ comes apart, and a `PolygonSet`
 * of several components is already apart before anything is removed — so the
 * properties that reason from "removing a disjoint shape cannot disconnect
 * anything" have to exclude it. Every other alternative is connected by
 * construction.
 */
inline bool isConnected(const AnyShape& shape) {
    if (const auto* set = shape.getIfPolygonSet()) {
        return set->componentCount() <= 1;
    }
    return true;
}

/**
 * @brief Compares two doubles with a relative tolerance.
 *
 * Used where two code paths compute the same real number by different formulas,
 * which can differ in the last bits. Never used for a vanishing test: those are
 * exact, because a true zero is computed as a true zero.
 */
inline bool nearlyEqual(double left, double right) {
    const double scale = std::max({1.0, std::fabs(left), std::fabs(right)});
    return std::fabs(left - right) <= 1e-9 * scale;
}

/** @brief Tests `left <= right` with the same relative tolerance. */
inline bool nearlyAtMost(double left, double right) {
    const double scale = std::max({1.0, std::fabs(left), std::fabs(right)});
    return left - right <= 1e-9 * scale;
}

/**
 * @brief Evaluates an operation that may not be defined for the pair at hand.
 *
 * Several operations are partial over the 324 shape pairs and say so by throwing
 * `std::logic_error` — `intersection` has no single `Shape` to return for a
 * disconnected result, `squaredHausdorffDistance` is only defined among the
 * bounded convex shapes, and the regularized boolean operations each cover their
 * own set of pairs. A pair outside an operation's domain is a fact about the API,
 * not a violated relation, so the *value* properties skip it.
 *
 * That would lose a real signal if it were the whole story, so it is not: the
 * domains themselves are checked by their own properties — see
 * `booleanOperationsShareADomain` and `distanceFamiliesShareADomain` — which
 * compare sibling operations against each other and report a pair that one
 * supports and another does not. Splitting it this way keeps one API gap from
 * showing up as six unrelated failures.
 *
 * @return The value, or `std::nullopt` if the pair is outside the domain.
 */
template <class Invoke>
auto attempt(Invoke&& invoke) -> std::optional<decltype(invoke())> {
    try {
        return invoke();
    } catch (const std::logic_error&) {
        return std::nullopt;
    }
}

/** @brief Whether an operation is defined for the pair at hand. */
template <class Invoke>
bool isSupported(Invoke&& invoke) {
    try {
        (void)invoke();
        return true;
    } catch (const std::logic_error&) {
        return false;
    }
}

}  // namespace props

}  // namespace pglprop
