#pragma once

/**
 * @file properties_invariance.hpp
 * @brief What must not change: value semantics, normalization, and geometry
 *        under exact affine maps.
 *
 * Three kinds of invariance, all of them things the library promises and none of
 * them checkable by looking at one shape pair at a time:
 *
 *  - **Value semantics.** Shapes go into `std::set` and `std::unordered_set`, so
 *    `==`, `<=>` and `std::hash` have to agree with each other. A hash that
 *    disagrees with equality is silent until a container starts losing elements.
 *  - **Normalization.** `Segment` orders its endpoints, `Triangle` puts its
 *    lex-min vertex first and turns counterclockwise, `Disk` compares equal for
 *    any three points on the same circle, `Convex` and `MonotoneChain` treat
 *    their input as a set. Every one of those is a promise that *how* a value
 *    was built cannot be recovered from it — so building it two ways must give
 *    two equal values, and the property is exactly that.
 *  - **Geometry under exact affine maps.** Translating, rotating by a multiple
 *    of a quarter turn, scaling by a positive integer and shearing by an integer
 *    are all invertible maps that keep the integer lattice. Incidence,
 *    containment, interiors and connectivity are affine notions, so all seven
 *    predicates must answer identically before and after. This turns any *one*
 *    correct predicate answer into a test of a whole orbit of inputs, which is
 *    what makes it so effective at finding axis-aligned special-case code that
 *    quietly does not generalize.
 */

#include "properties_common.hpp"

#include <functional>

namespace pglprop {

namespace props {

/** @brief All seven predicate answers for an ordered pair. */
struct PredicateVector {
    /** @brief `A.contains(B)`. */
    bool contains = false;
    /** @brief `A.boundaryContains(B)`. */
    bool boundaryContains = false;
    /** @brief `A.interiorContains(B)`. */
    bool interiorContains = false;
    /** @brief `A.intersects(B)`. */
    bool intersects = false;
    /** @brief `A.interiorsIntersect(B)`. */
    bool interiorsIntersect = false;
    /** @brief `A.separates(B)`. */
    bool separates = false;
    /** @brief `A.crosses(B)`. */
    bool crosses = false;

    /** @brief Compares all seven answers. */
    friend bool operator==(const PredicateVector&, const PredicateVector&) = default;
};

/** @brief Evaluates the seven predicates on an ordered pair. */
template <class ShapeA, class ShapeB>
PredicateVector predicatesOf(const ShapeA& a, const ShapeB& b) {
    PredicateVector answers;
    answers.contains = a.contains(b);
    answers.boundaryContains = a.boundaryContains(b);
    answers.interiorContains = a.interiorContains(b);
    answers.intersects = a.intersects(b);
    answers.interiorsIntersect = a.interiorsIntersect(b);
    answers.separates = a.separates(b);
    answers.crosses = a.crosses(b);
    return answers;
}

/** @brief Renders a predicate vector as a compact list of the true relations. */
inline std::string showPredicates(const PredicateVector& answers) {
    std::string text = "{";
    const auto add = [&text](const char* name, bool value) {
        if (value) {
            if (text.size() > 1) {
                text += " ";
            }
            text += name;
        }
    };
    add("contains", answers.contains);
    add("boundaryContains", answers.boundaryContains);
    add("interiorContains", answers.interiorContains);
    add("intersects", answers.intersects);
    add("interiorsIntersect", answers.interiorsIntersect);
    add("separates", answers.separates);
    add("crosses", answers.crosses);
    return text + "}";
}

// ------------------------------------------------------------ value semantics

/**
 * @brief A copy is equal, orders equal, and hashes equal.
 *
 * The floor of value semantics. Cheap, and it catches a shape that hashes a
 * cached field rather than its geometry.
 */
inline Result copyIsIndistinguishable(const AnyShape& a) {
    const AnyShape copy = a;  // NOLINT: the copy is the point of the property.
    const std::string prefix = "A = " + detail::show(a) + " ; ";
    PGLPROP_CHECK(copy == a, prefix + "a copy compares unequal to the original");
    PGLPROP_CHECK((a <=> copy) == 0, prefix + "a copy does not order equal to the original");
    PGLPROP_CHECK(std::hash<AnyShape>{}(copy) == std::hash<AnyShape>{}(a),
                  prefix + "a copy hashes differently from the original");
    return held();
}

/**
 * @brief Equal shapes hash equally.
 *
 * The hash-table contract. Two shapes drawn independently but built to the same
 * value — a `Triangle` from permuted vertices, a `Convex` from shuffled points —
 * are how this gets exercised, so it is worth the pair draw rather than being
 * folded into the copy property.
 */
inline Result equalityImpliesEqualHash(const AnyShape& a, const AnyShape& b) {
    if (!(a == b)) {
        return skipped();
    }
    PGLPROP_CHECK(std::hash<AnyShape>{}(a) == std::hash<AnyShape>{}(b),
                  pair(a, b) + " ; the two compare equal but hash differently");
    return held();
}

/** @brief `<=>`, `==` and `<` tell one consistent story. */
inline Result orderingIsConsistent(const AnyShape& a, const AnyShape& b) {
    const bool less = a < b;
    const bool greater = a > b;
    const bool equal = a == b;

    const int trueCount = (less ? 1 : 0) + (greater ? 1 : 0) + (equal ? 1 : 0);
    PGLPROP_CHECK(trueCount == 1,
                  pair(a, b) + " ; exactly one of <, ==, > must hold, but got less=" +
                      detail::show(less) + " equal=" + detail::show(equal) + " greater=" +
                      detail::show(greater));
    PGLPROP_CHECK(less == (b > a),
                  pair(a, b) + " ; A<B and B>A disagree");
    PGLPROP_CHECK(equal == (b == a), pair(a, b) + " ; == is not symmetric");
    return held();
}

// ------------------------------------------------------------- normalization

/**
 * @brief Rebuilding a shape from its own defining points in a different order
 *        gives an equal shape.
 *
 * Only the classes that document a normalization are checked, and each is
 * checked against what it documents: an unordered pair for `Segment` and
 * `Rectangle`, all six vertex permutations for `Triangle` and for a `Disk` given
 * by three boundary points, an arbitrary reordering of the input for the two
 * classes that treat their points as a set, and any cyclic rotation of the ring
 * for `Polygon`. `OrientedSegment`, `OrientedLine`, `Halfplane` and `Polyline`
 * are absent on purpose — for them the order *is* part of the value.
 */
inline Result normalizationIsRouteIndependent(const AnyShape& a) {
    const std::string prefix = "A = " + detail::show(a) + " ; ";

    if (const auto* segment = a.getIfSegment()) {
        const detail::SegmentShape swapped(segment->max(), segment->min());
        PGLPROP_CHECK(swapped == *segment,
                      prefix + "rebuilding the segment with its endpoints swapped gives " +
                          detail::show(swapped));
        return held();
    }

    if (const auto* line = a.getIfLine()) {
        const detail::LineShape swapped(line->max(), line->min());
        PGLPROP_CHECK(swapped == *line,
                      prefix + "rebuilding the line with its two points swapped gives " +
                          detail::show(swapped));
        return held();
    }

    if (const auto* rectangle = a.getIfRectangle()) {
        if (rectangle->empty()) {
            return skipped();  // No corners to reorder.
        }
        const detail::RectangleShape swapped(rectangle->max(), rectangle->min());
        PGLPROP_CHECK(swapped == *rectangle,
                      prefix + "rebuilding the rectangle with its corners swapped gives " +
                          detail::show(swapped));
        return held();
    }

    if (const auto* triangle = a.getIfTriangle()) {
        const PointShape vertices[3] = {triangle->a(), triangle->b(), triangle->c()};
        static const int permutations[6][3] = {{0, 1, 2}, {0, 2, 1}, {1, 0, 2},
                                               {1, 2, 0}, {2, 0, 1}, {2, 1, 0}};
        for (const auto& order : permutations) {
            const detail::TriangleShape permuted(vertices[order[0]], vertices[order[1]],
                                                        vertices[order[2]]);
            PGLPROP_CHECK(permuted == *triangle,
                          prefix + "the vertex permutation giving " + detail::show(permuted) +
                              " compares unequal");
        }
        return held();
    }

    if (const auto* disk = a.getIfDisk()) {
        const PointShape boundary[3] = {disk->a(), disk->b(), disk->c()};
        static const int permutations[6][3] = {{0, 1, 2}, {0, 2, 1}, {1, 0, 2},
                                               {1, 2, 0}, {2, 0, 1}, {2, 1, 0}};
        for (const auto& order : permutations) {
            const detail::DiskShape permuted(boundary[order[0]], boundary[order[1]],
                                                    boundary[order[2]]);
            if (permuted.isUndefined()) {
                return skipped();  // Undefined carries no contract, equality included.
            }
            PGLPROP_CHECK(permuted == *disk,
                          prefix + "the boundary-point permutation giving " +
                              detail::show(permuted) + " compares unequal");
        }
        return held();
    }

    if (const auto* convex = a.getIfConvex()) {
        if (convex->empty()) {
            return skipped();
        }
        std::vector<PointShape> reversed(convex->begin(), convex->end());
        std::reverse(reversed.begin(), reversed.end());
        const detail::ConvexShape rebuilt(reversed);
        PGLPROP_CHECK(rebuilt == *convex,
                      prefix + "re-hulling the vertices in reverse order gives " +
                          detail::show(rebuilt));
        return held();
    }

    if (const auto* chain = a.getIfMonotoneChain()) {
        if (chain->empty()) {
            return skipped();
        }
        std::vector<PointShape> reversed(chain->begin(), chain->end());
        std::reverse(reversed.begin(), reversed.end());
        const detail::MonotoneChainShape rebuilt(reversed);
        PGLPROP_CHECK(rebuilt == *chain,
                      prefix + "rebuilding the chain from its reversed vertices gives " +
                          detail::show(rebuilt));
        return held();
    }

    if (const auto* polygon = a.getIfPolygon()) {
        const std::size_t count = polygon->size();
        if (count < 3) {
            return skipped();
        }
        std::vector<PointShape> ring(polygon->begin(), polygon->end());
        for (std::size_t shift = 1; shift < count; ++shift) {
            std::vector<PointShape> rotated;
            rotated.reserve(count);
            for (std::size_t i = 0; i < count; ++i) {
                rotated.push_back(ring[(i + shift) % count]);
            }
            const detail::PolygonShape rebuilt(rotated);
            PGLPROP_CHECK(rebuilt == *polygon,
                          prefix + "rotating the ring by " + std::to_string(shift) +
                              " gives the unequal polygon " + detail::show(rebuilt));
        }
        return held();
    }

    return skipped();
}

// ------------------------------------------------------- affine invariance

/** @brief Compares the predicate vector before and after a map. */
inline Result comparePredicates(const AnyShape& a, const AnyShape& b, const AnyShape& mappedA,
                                const AnyShape& mappedB, const char* what) {
    const PredicateVector before = predicatesOf(a, b);
    const PredicateVector after = predicatesOf(mappedA, mappedB);
    PGLPROP_CHECK(before == after,
                  pair(a, b) + " ; under " + what + " the predicates change from " +
                      showPredicates(before) + " to " + showPredicates(after) + " (mapped: A = " +
                      detail::show(mappedA) + " ; B = " + detail::show(mappedB) + ")");
    return held();
}

/** @brief Translation changes no predicate. */
inline Result translationPreservesPredicates(const AnyShape& a, const AnyShape& b) {
    const PointShape shift(3, -5);
    AnyShape mappedA = a;
    AnyShape mappedB = b;
    mappedA += shift;
    mappedB += shift;
    return comparePredicates(a, b, mappedA, mappedB, "translation by (3,-5)");
}

/** @brief A quarter, half or three-quarter turn changes no predicate. */
inline Result rotationPreservesPredicates(const AnyShape& a, const AnyShape& b) {
    for (int quarters = 1; quarters <= 3; ++quarters) {
        const Result result = comparePredicates(a, b, a.rotated90(quarters), b.rotated90(quarters),
                                                "rotation by a multiple of 90 degrees");
        if (result.outcome != Outcome::kHeld) {
            return result;
        }
    }
    return held();
}

/**
 * @brief Scaling up by a positive integer changes no predicate.
 *
 * A similarity, so it preserves everything; and it keeps the lattice, so the
 * scaled case is exactly representable and the comparison stays exact.
 */
inline Result scalingPreservesPredicates(const AnyShape& a, const AnyShape& b) {
    for (const Coord factor : {Coord(2), Coord(3)}) {
        AnyShape mappedA = a;
        AnyShape mappedB = b;
        mappedA *= factor;
        mappedB *= factor;
        const Result result =
            comparePredicates(a, b, mappedA, mappedB, "scaling up by an integer factor");
        if (result.outcome != Outcome::kHeld) {
            return result;
        }
    }
    return held();
}

/**
 * @brief Point reflection through the origin changes no predicate.
 *
 * Multiplying by `-1` is the half turn again, but reached through the scalar
 * operator rather than `rotated90`, which is a different code path in every
 * shape — and the one that has to re-establish each class's normalization
 * afterwards.
 */
inline Result negationPreservesPredicates(const AnyShape& a, const AnyShape& b) {
    AnyShape mappedA = a;
    AnyShape mappedB = b;
    mappedA *= Coord(-1);
    mappedB *= Coord(-1);
    return comparePredicates(a, b, mappedA, mappedB, "negation");
}

/**
 * @brief An integer shear changes no predicate.
 *
 * The one map here that is not a similarity: it preserves incidence and
 * containment but destroys angles, axis-alignment and every distance. Anything
 * that answers a predicate by way of an axis-aligned shortcut fails here and
 * nowhere else. Skipped for `Rectangle` and `Disk` operands, which cannot
 * represent the image and throw — the `kAffine` tag selects for that.
 */
inline Result shearPreservesPredicates(const AnyShape& a, const AnyShape& b) {
    const auto shear = pgl::Transformation<Coord>::shearX(Coord(1));
    const AnyShape mappedA = shear * a;
    const AnyShape mappedB = shear * b;
    return comparePredicates(a, b, mappedA, mappedB, "an integer shear");
}

/**
 * @brief Distances behave as an isometry and a similarity require.
 *
 * Translation and rotation leave the squared distance alone; scaling by `s`
 * multiplies it by `s^2`. Exact in `double` for these operands, the values being
 * small rationals with power-of-two-free denominators that survive the scaling
 * unchanged in relative terms — so this compares with a tolerance rather than
 * for equality.
 */
inline Result distancesFollowTheMap(const AnyShape& a, const AnyShape& b) {
    if (a.empty() || b.empty()) {
        return skipped();
    }
    const double original = a.squaredDistance(b);

    const PointShape shift(3, -5);
    AnyShape translatedA = a;
    AnyShape translatedB = b;
    translatedA += shift;
    translatedB += shift;
    const double translated = translatedA.squaredDistance(translatedB);
    PGLPROP_CHECK(nearlyEqual(original, translated),
                  pair(a, b) + " ; squaredDistance changes from " + detail::show(original) +
                      " to " + detail::show(translated) + " under translation");

    const double rotated = a.rotated90(1).squaredDistance(b.rotated90(1));
    PGLPROP_CHECK(nearlyEqual(original, rotated),
                  pair(a, b) + " ; squaredDistance changes from " + detail::show(original) +
                      " to " + detail::show(rotated) + " under a quarter turn");

    AnyShape scaledA = a;
    AnyShape scaledB = b;
    scaledA *= Coord(3);
    scaledB *= Coord(3);
    const double scaled = scaledA.squaredDistance(scaledB);
    PGLPROP_CHECK(nearlyEqual(9.0 * original, scaled),
                  pair(a, b) + " ; squaredDistance is " + detail::show(original) +
                      " but " + detail::show(scaled) + " after scaling by 3, not " +
                      detail::show(9.0 * original));
    return held();
}

}  // namespace props

/** @brief Adds the invariance and value-semantics properties to a registry. */
inline void registerInvarianceProperties(Registry& registry) {
    registry.unary.push_back({"value", "copy-is-indistinguishable", kNoTag,
                              props::copyIsIndistinguishable});
    registry.unary.push_back({"value", "normalization-is-route-independent", kNoTag,
                              props::normalizationIsRouteIndependent});
    registry.binary.push_back({"value", "equality-implies-equal-hash", kNoTag,
                               props::equalityImpliesEqualHash});
    registry.binary.push_back({"value", "ordering-is-consistent", kNoTag,
                               props::orderingIsConsistent});
    registry.binary.push_back({"invariance", "translation-preserves-predicates", kNoTag,
                               props::translationPreservesPredicates});
    registry.binary.push_back({"invariance", "rotation-preserves-predicates", kAxisFree,
                               props::rotationPreservesPredicates});
    registry.binary.push_back({"invariance", "scaling-preserves-predicates", kNoTag,
                               props::scalingPreservesPredicates});
    registry.binary.push_back({"invariance", "negation-preserves-predicates", kNoTag,
                               props::negationPreservesPredicates});
    registry.binary.push_back({"invariance", "shear-preserves-predicates", kAffine | kAxisFree,
                               props::shearPreservesPredicates});
    registry.binary.push_back({"invariance", "distances-follow-the-map", kNoTag,
                               props::distancesFollowTheMap});
}

}  // namespace pglprop
