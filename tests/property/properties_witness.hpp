#pragma once

/**
 * @file properties_witness.hpp
 * @brief Constructions that name *where* an answer comes from, checked against
 *        the answer itself.
 *
 * Two families here, and they share a shape of argument. Each computes a scalar
 * by one route and a *witness* for that scalar by another, so the witness can be
 * fed back through the predicates and the metric and has to reproduce what the
 * scalar already said.
 *
 *  - **`closestPoints` / `closestSegments`** name the pair of points, and the
 *    pair of elements carrying them, that realize `squaredDistance`. Three
 *    independent things must then agree: the witness exists exactly when the
 *    shapes are disjoint, each point lies on the shape it came from, and the
 *    distance between the two points is the distance between the two shapes.
 *    The last is the one that matters — it closes the loop between a search over
 *    element pairs and the distance members, which are separate code.
 *  - **`yAtX` / `xAtY`** name the point of a curve at a given abscissa. The
 *    contract is an equivalence, and both halves are worth asking: what comes
 *    back must lie on the curve, and nothing may come back only when the curve
 *    has no point there at all. A one-sided check would pass for an
 *    implementation that answered `nullopt` everywhere.
 *
 * Neither family has any property coverage elsewhere, and neither is reachable
 * from the predicate or boolean groups: a witness that is merely *plausible*
 * satisfies every relation those check.
 */

#include "properties_common.hpp"

#include <array>
#include <optional>
#include <stdexcept>

namespace pglprop {

namespace props {

// ------------------------------------------------------ closest-point witness

/**
 * @brief The closest-point witness exists exactly for a disjoint pair.
 *
 * Documented as "nothing exactly when `squaredDistance` is zero". Asked against
 * `intersects` rather than against the distance so that the two routes stay
 * independent — `zeroDistanceMeansIntersecting` already ties the distance to the
 * predicate, and repeating that here would make this property derive from it
 * rather than corroborate it.
 */
inline Result closestWitnessExistsWhenDisjoint(const AnyShape& a, const AnyShape& b) {
    if (a.empty() || b.empty()) {
        return skipped();  // A non-empty operand is a precondition of the distances.
    }
    const auto witness = attempt([&] { return a.template closestPoints<Exact>(b); });
    if (!witness) {
        return skipped();  // No `closestPoints` for this pair.
    }
    const bool meets = a.intersects(b);
    PGLPROP_CHECK(witness->has_value() == !meets,
                  pair(a, b) + " ; A.intersects(B)=" + detail::show(meets) +
                      " but closestPoints " +
                      (witness->has_value() ? "named a pair" : "answered nothing"));
    return held();
}

/** @brief Each closest point lies on the shape it was taken from. */
inline Result closestPointsLieOnTheirShapes(const AnyShape& a, const AnyShape& b) {
    if (a.empty() || b.empty()) {
        return skipped();  // A non-empty operand is a precondition of the distances.
    }
    const auto witness = attempt([&] { return a.template closestPoints<Exact>(b); });
    if (!witness || !witness->has_value()) {
        return skipped();
    }
    const ExactShape onA{ExactPoint((**witness)[0])};
    const ExactShape onB{ExactPoint((**witness)[1])};
    const ExactShape exactA = toExact(a);
    const ExactShape exactB = toExact(b);

    PGLPROP_CHECK(exactA.contains(onA),
                  pair(a, b) + " ; the closest point " + detail::show(onA) + " is not on A");
    PGLPROP_CHECK(exactB.contains(onB),
                  pair(a, b) + " ; the closest point " + detail::show(onB) + " is not on B");
    return held();
}

/**
 * @brief The two closest points are exactly `squaredDistance` apart.
 *
 * The closing identity of the family, and the only one that would catch a
 * witness that is on both shapes and simply not the nearest such pair.
 */
inline Result closestPointsRealizeTheDistance(const AnyShape& a, const AnyShape& b) {
    if (a.empty() || b.empty()) {
        return skipped();  // A non-empty operand is a precondition of the distances.
    }
    const auto witness = attempt([&] { return a.template closestPoints<Exact>(b); });
    if (!witness || !witness->has_value()) {
        return skipped();
    }
    const auto expected = attempt([&] { return a.template squaredDistance<Exact>(b); });
    if (!expected) {
        return skipped();
    }
    const ExactPoint first((**witness)[0]);
    const ExactPoint second((**witness)[1]);
    const Exact realized = first.template squaredDistance<Exact>(second);

    PGLPROP_CHECK(realized == *expected,
                  pair(a, b) + " ; squaredDistance is " + detail::show(*expected) +
                      " but the closest points " + detail::show(first) + " and " +
                      detail::show(second) + " are " + detail::show(realized) + " apart");
    return held();
}

/**
 * @brief The closest *elements* carry the closest points, and lie on the shapes.
 *
 * `closestSegments` is the coarser of the two witnesses — an edge of each shape
 * rather than a point on each edge — so the point witness must sit on it. That
 * makes the two answers checkable against each other, which neither is against
 * the distance alone.
 */
inline Result closestSegmentsCarryTheClosestPoints(const AnyShape& a, const AnyShape& b) {
    if (a.empty() || b.empty()) {
        return skipped();  // A non-empty operand is a precondition of the distances.
    }
    const auto elements = attempt([&] { return a.template closestSegments<Exact>(b); });
    if (!elements || !elements->has_value()) {
        return skipped();
    }
    const auto witness = attempt([&] { return a.template closestPoints<Exact>(b); });
    if (!witness || !witness->has_value()) {
        return skipped();
    }

    const ExactShape firstElement{(**elements)[0]};
    const ExactShape secondElement{(**elements)[1]};
    const ExactShape onA{ExactPoint((**witness)[0])};
    const ExactShape onB{ExactPoint((**witness)[1])};

    PGLPROP_CHECK(toExact(a).contains(firstElement),
                  pair(a, b) + " ; the closest element " + detail::show(firstElement) +
                      " is not contained in A");
    PGLPROP_CHECK(toExact(b).contains(secondElement),
                  pair(a, b) + " ; the closest element " + detail::show(secondElement) +
                      " is not contained in B");
    PGLPROP_CHECK(firstElement.contains(onA),
                  pair(a, b) + " ; the closest point " + detail::show(onA) +
                      " is not on the closest element " + detail::show(firstElement));
    PGLPROP_CHECK(secondElement.contains(onB),
                  pair(a, b) + " ; the closest point " + detail::show(onB) +
                      " is not on the closest element " + detail::show(secondElement));
    return held();
}

// ------------------------------------------------------------- yAtX and xAtY

/**
 * @brief Calls @p ask on whichever alternative offers `yAtX` / `xAtY`.
 *
 * The two are members of the six curve alternatives rather than of `Shape`, so
 * reaching them means naming the alternatives. Everything else skips.
 *
 * @return Whether an alternative was reached.
 */
template <class Ask>
bool withCurve(const AnyShape& shape, Ask&& ask) {
    if (const auto* segment = shape.getIfHoldsSegment()) {
        ask(*segment);
    } else if (const auto* oriented = shape.getIfHoldsOrientedSegment()) {
        ask(*oriented);
    } else if (const auto* line = shape.getIfHoldsLine()) {
        ask(*line);
    } else if (const auto* orientedLine = shape.getIfHoldsOrientedLine()) {
        ask(*orientedLine);
    } else if (const auto* ray = shape.getIfHoldsRay()) {
        ask(*ray);
    } else if (const auto* chain = shape.getIfHoldsMonotoneChain()) {
        ask(*chain);
    } else {
        return false;
    }
    return true;
}

/** @brief Coordinates the curve properties probe, a little past the grid. */
inline const std::vector<Coord>& probeCoordinates() {
    static const std::vector<Coord> coordinates = [] {
        std::vector<Coord> values;
        for (Coord value = -8; value <= 8; ++value) {
            values.push_back(value);
        }
        return values;
    }();
    return coordinates;
}

/**
 * @brief A vertical or horizontal line through the given coordinate.
 *
 * The oracle for "does the curve have a point at this abscissa": the curve has
 * one exactly when it meets that line, which is a predicate rather than another
 * call into the same arithmetic.
 */
inline ExactShape axisLineAt(Coord value, bool vertical) {
    const ExactPoint first = vertical ? ExactPoint(value, 0) : ExactPoint(0, value);
    const ExactPoint second = vertical ? ExactPoint(value, 1) : ExactPoint(1, value);
    return ExactShape{pgl::Line<ExactPoint>(first, second)};
}

/**
 * @brief `yAtX` answers a point of the curve, and answers exactly when there is
 *        one.
 *
 * Both halves of the equivalence. The forward half catches an answer off the
 * curve; the backward half catches a `nullopt` where the curve does have a
 * point, which no amount of checking the returned values would ever notice.
 */
inline Result yAtXLandsOnTheCurve(const AnyShape& a) {
    const ExactShape exact = toExact(a);
    Result outcome = skipped();

    const bool reached = withCurve(a, [&](const auto& curve) {
        for (const Coord x : probeCoordinates()) {
            const std::optional<Exact> y = curve.template yAtX<Exact>(x);
            const bool meets = exact.intersects(axisLineAt(x, /*vertical=*/true));
            if (y.has_value()) {
                const ExactShape point{ExactPoint(Exact(x), *y)};
                if (!exact.contains(point)) {
                    outcome = failure(detail::joinCheck(
                        "A = " + detail::show(a) + " ; yAtX(" + detail::show(x) + ") = " +
                            detail::show(*y) + ", but " + detail::show(point) +
                            " is not on the curve",
                        "exact.contains(point)"));
                    return;
                }
            } else if (meets) {
                outcome = failure(detail::joinCheck(
                    "A = " + detail::show(a) + " ; yAtX(" + detail::show(x) +
                        ") answered nothing, but the curve meets the line x=" + detail::show(x),
                    "yAtX(x).has_value()"));
                return;
            }
            outcome = held();
        }
    });
    return reached ? outcome : skipped();
}

/** @brief `xAtY` answers a point of the curve, and answers exactly when there is one. */
inline Result xAtYLandsOnTheCurve(const AnyShape& a) {
    const ExactShape exact = toExact(a);
    Result outcome = skipped();

    const bool reached = withCurve(a, [&](const auto& curve) {
        for (const Coord y : probeCoordinates()) {
            if constexpr (!requires { curve.template xAtY<Exact>(y); }) {
                return;  // A MonotoneChain is a function of x alone.
            } else {
                const std::optional<Exact> x = curve.template xAtY<Exact>(y);
                const bool meets = exact.intersects(axisLineAt(y, /*vertical=*/false));
                if (x.has_value()) {
                    const ExactShape point{ExactPoint(*x, Exact(y))};
                    if (!exact.contains(point)) {
                        outcome = failure(detail::joinCheck(
                            "A = " + detail::show(a) + " ; xAtY(" + detail::show(y) + ") = " +
                                detail::show(*x) + ", but " + detail::show(point) +
                                " is not on the curve",
                            "exact.contains(point)"));
                        return;
                    }
                } else if (meets) {
                    outcome = failure(detail::joinCheck(
                        "A = " + detail::show(a) + " ; xAtY(" + detail::show(y) +
                            ") answered nothing, but the curve meets the line y=" +
                            detail::show(y),
                        "xAtY(y).has_value()"));
                    return;
                }
                outcome = held();
            }
        }
    });
    return reached ? outcome : skipped();
}

}  // namespace props

/** @brief Adds the witness properties to a registry. */
inline void registerWitnessProperties(Registry& registry) {
    registry.binary.push_back({"closest", "closest-witness-exists-when-disjoint", kNoTag,
                               props::closestWitnessExistsWhenDisjoint});
    registry.binary.push_back({"closest", "closest-points-lie-on-their-shapes", kNoTag,
                               props::closestPointsLieOnTheirShapes});
    registry.binary.push_back({"closest", "closest-points-realize-the-distance", kNoTag,
                               props::closestPointsRealizeTheDistance});
    registry.binary.push_back({"closest", "closest-segments-carry-the-closest-points", kNoTag,
                               props::closestSegmentsCarryTheClosestPoints});
    registry.unary.push_back({"curve", "y-at-x-lands-on-the-curve", kNoTag,
                              props::yAtXLandsOnTheCurve});
    registry.unary.push_back({"curve", "x-at-y-lands-on-the-curve", kNoTag,
                              props::xAtYLandsOnTheCurve});
}

}  // namespace pglprop
