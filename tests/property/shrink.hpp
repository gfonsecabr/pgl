#pragma once

/**
 * @file shrink.hpp
 * @brief Reduction of a failing case to a small, readable witness.
 *
 * A random failure on a 13x13 grid is usually a seven-vertex polygon against a
 * five-vertex one, and reading the contract violation off *that* is hopeless.
 * Shrinking replaces it with the smallest nearby case that still fails —
 * typically two or three vertices on a line, which is generally enough to see
 * what the predicate got wrong.
 *
 * Because every generated shape is a pure function of a list of lattice points
 * (see `generators.hpp`), the shrinker never has to know what shape it is
 * holding. It perturbs coordinates, rebuilds through the generator, and asks the
 * property again. A perturbation that makes the generator reject its input, or
 * makes the property pass, is simply discarded.
 *
 * ## Termination
 *
 * Each accepted reduction must strictly decrease @ref Complexity, which is
 * bounded below, so the loop cannot cycle. That is the whole argument, and it is
 * why the candidate list may be as opportunistic as it likes: `snapCoordinate`
 * moves one coordinate onto another's value, which is not obviously a
 * "reduction" at all, but it can only ever be accepted when the measure drops.
 */

#include "generators.hpp"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <set>
#include <vector>

namespace pglprop {

/** @brief The point lists of a whole case: one per operand. */
using PointLists = std::vector<std::vector<PointShape>>;

/**
 * @brief Predicate the shrinker drives: does this case still fail?
 *
 * Returns `false` both when the property holds and when the case no longer
 * builds, which are equally good reasons to discard a candidate.
 */
using StillFails = std::function<bool(const PointLists&)>;

/**
 * @brief Size measure the shrinker strictly decreases.
 *
 * Compared lexicographically in this field order, which is also the order of
 * how much each field costs a reader: an extra vertex is worse than a larger
 * coordinate, and a larger coordinate is worse than one more distinct value.
 */
struct Complexity {
    /** @brief Total number of points across all operands. */
    std::size_t points = 0;
    /** @brief Sum of the absolute values of every coordinate. */
    long long magnitude = 0;
    /** @brief How many distinct coordinate values appear anywhere in the case. */
    std::size_t distinct = 0;

    /** @brief Lexicographic order over the three fields. */
    friend bool operator<(const Complexity& left, const Complexity& right) {
        if (left.points != right.points) {
            return left.points < right.points;
        }
        if (left.magnitude != right.magnitude) {
            return left.magnitude < right.magnitude;
        }
        return left.distinct < right.distinct;
    }
};

/** @brief Measures a case. */
inline Complexity complexityOf(const PointLists& lists) {
    Complexity measure;
    std::set<Coord> values;
    for (const std::vector<PointShape>& list : lists) {
        measure.points += list.size();
        for (const PointShape& point : list) {
            const Coord x = point.x();
            const Coord y = point.y();
            measure.magnitude += static_cast<long long>(x < 0 ? -x : x);
            measure.magnitude += static_cast<long long>(y < 0 ? -y : y);
            values.insert(x);
            values.insert(y);
        }
    }
    measure.distinct = values.size();
    return measure;
}

namespace detail {

/** @brief Returns `|value|`, avoiding a signed-overflow corner at the minimum. */
inline long long absolute(Coord value) {
    return value < 0 ? -static_cast<long long>(value) : static_cast<long long>(value);
}

/** @brief Rebuilds a point with one coordinate replaced. */
inline PointShape withCoordinate(const PointShape& point, int axis, Coord value) {
    return axis == 0 ? PointShape(value, point.y()) : PointShape(point.x(), value);
}

/** @brief Reads one coordinate of a point by axis index. */
inline Coord coordinate(const PointShape& point, int axis) {
    return axis == 0 ? point.x() : point.y();
}

/**
 * @brief Appends every one-step reduction of one operand's point list.
 *
 * The candidates, in the order tried:
 *  - drop a point, whenever the generator can still be fed;
 *  - drive a coordinate to zero, halve it, or step it one toward zero;
 *  - snap a coordinate onto another point's value on the same axis, but only
 *    toward the smaller magnitude — so the case gets more degenerate (shared
 *    x, shared y, coincident vertices) without the numbers getting larger.
 *
 * Cheapest and most drastic first: the greedy loop takes the first candidate
 * that still fails, so ordering is what decides how fast it converges.
 */
inline void appendCandidates(const std::vector<PointShape>& list, std::size_t minPoints,
                             std::vector<std::vector<PointShape>>& out) {
    const std::size_t count = list.size();

    if (count > minPoints) {
        for (std::size_t i = 0; i < count; ++i) {
            std::vector<PointShape> candidate = list;
            candidate.erase(candidate.begin() + static_cast<std::ptrdiff_t>(i));
            out.push_back(std::move(candidate));
        }
    }

    for (std::size_t i = 0; i < count; ++i) {
        for (int axis = 0; axis < 2; ++axis) {
            const Coord value = coordinate(list[i], axis);
            if (value == 0) {
                continue;
            }
            const Coord halved = static_cast<Coord>(value / 2);
            const Coord stepped = static_cast<Coord>(value > 0 ? value - 1 : value + 1);
            for (const Coord replacement : {Coord(0), halved, stepped}) {
                if (replacement == value) {
                    continue;
                }
                std::vector<PointShape> candidate = list;
                candidate[i] = withCoordinate(list[i], axis, replacement);
                out.push_back(std::move(candidate));
            }
        }
    }

    for (std::size_t i = 0; i < count; ++i) {
        for (std::size_t j = 0; j < count; ++j) {
            if (i == j) {
                continue;
            }
            for (int axis = 0; axis < 2; ++axis) {
                const Coord from = coordinate(list[i], axis);
                const Coord to = coordinate(list[j], axis);
                if (from == to || absolute(to) >= absolute(from)) {
                    continue;
                }
                std::vector<PointShape> candidate = list;
                candidate[i] = withCoordinate(list[i], axis, to);
                out.push_back(std::move(candidate));
            }
        }
    }
}

}  // namespace detail

/**
 * @brief Reduces a failing case as far as the candidate moves reach.
 *
 * Greedy descent: repeatedly scan the candidates of every operand, accept the
 * first that still fails and measures strictly smaller, and start over. Stops at
 * a local minimum — no single move improves it — or when the budget runs out.
 *
 * @param start The case as randomly generated, known to fail.
 * @param minSizes Fewest points each operand's generator will accept, so that
 *        dropping a point does not produce candidates that can never build.
 * @param stillFails Rebuilds a candidate and re-runs the property on it.
 * @param budget Maximum number of `stillFails` calls; the shrinker is charged
 *        per candidate tested, not per accepted reduction.
 * @return The smallest failing case found. Never worse than @p start.
 */
inline PointLists shrink(const PointLists& start, const std::vector<std::size_t>& minSizes,
                         const StillFails& stillFails, int budget) {
    PointLists best = start;
    Complexity bestMeasure = complexityOf(best);

    bool improved = true;
    while (improved && budget > 0) {
        improved = false;
        for (std::size_t operand = 0; operand < best.size() && budget > 0; ++operand) {
            std::vector<std::vector<PointShape>> candidates;
            detail::appendCandidates(best[operand], minSizes[operand], candidates);

            for (std::vector<PointShape>& candidateList : candidates) {
                if (budget <= 0) {
                    break;
                }
                PointLists candidate = best;
                candidate[operand] = std::move(candidateList);

                const Complexity measure = complexityOf(candidate);
                if (!(measure < bestMeasure)) {
                    continue;  // Free to reject: no call, no charge.
                }

                --budget;
                if (!stillFails(candidate)) {
                    continue;
                }

                best = std::move(candidate);
                bestMeasure = measure;
                improved = true;
                break;  // Restart the scan from the new, smaller case.
            }
        }
    }

    return best;
}

}  // namespace pglprop
