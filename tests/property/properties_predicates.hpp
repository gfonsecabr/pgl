#pragma once

/**
 * @file properties_predicates.hpp
 * @brief The relations that hold among the seven predicates, for every pair.
 *
 * Each predicate has a set-theoretic definition (`doc/shape_methods.md`):
 *
 *   | `A.contains(B)`           | @f$A \supseteq B@f$ |
 *   | `A.boundaryContains(B)`   | @f$\partial A \supseteq B@f$ |
 *   | `A.interiorContains(B)`   | @f$A^\circ \supseteq B@f$ |
 *   | `A.intersects(B)`         | @f$A \cap B \neq \emptyset@f$ |
 *   | `A.interiorsIntersect(B)` | @f$A^\circ \cap B^\circ \neq \emptyset@f$ |
 *   | `A.separates(B)`          | @f$B \setminus A@f$ disconnected |
 *   | `A.crosses(B)`            | both @f$A \setminus B@f$ and @f$B \setminus A@f$ disconnected |
 *
 * Every property below is a consequence of those definitions alone, so it must
 * hold for all 324 ordered pairs of alternatives regardless of how any
 * individual overload is implemented. That universality is the point: the
 * implementation is one file per relation with a definition per shape pair, and
 * nothing but a cross-cutting check like this one can tell whether the several
 * hundred definitions agree with each other.
 */

#include "properties_common.hpp"

namespace pglprop {

namespace props {

// ---------------------------------------------------------------- symmetry

inline Result intersectsIsSymmetric(const AnyShape& a, const AnyShape& b) {
    const bool forward = a.intersects(b);
    const bool backward = b.intersects(a);
    PGLPROP_CHECK(forward == backward,
                  pair(a, b) + " ; A.intersects(B)=" + detail::show(forward) +
                      " but B.intersects(A)=" + detail::show(backward));
    return held();
}

inline Result interiorsIntersectIsSymmetric(const AnyShape& a, const AnyShape& b) {
    const bool forward = a.interiorsIntersect(b);
    const bool backward = b.interiorsIntersect(a);
    PGLPROP_CHECK(forward == backward,
                  pair(a, b) + " ; A.interiorsIntersect(B)=" + detail::show(forward) +
                      " but B.interiorsIntersect(A)=" + detail::show(backward));
    return held();
}

inline Result crossesIsSymmetric(const AnyShape& a, const AnyShape& b) {
    const bool forward = a.crosses(b);
    const bool backward = b.crosses(a);
    PGLPROP_CHECK(forward == backward,
                  pair(a, b) + " ; A.crosses(B)=" + detail::show(forward) +
                      " but B.crosses(A)=" + detail::show(backward));
    return held();
}

// ------------------------------------------------------------- definitional

/**
 * @brief `A.crosses(B)` is exactly mutual separation.
 *
 * Straight from the definition: `crosses` asks that both differences come apart,
 * and each of those questions *is* a `separates` call. The library's own README
 * states the identity, which makes this the one property here that checks a
 * documented equation rather than a derived implication.
 */
inline Result crossesIsMutualSeparation(const AnyShape& a, const AnyShape& b) {
    const bool crosses = a.crosses(b);
    const bool cutsB = a.separates(b);
    const bool cutsA = b.separates(a);
    PGLPROP_CHECK(crosses == (cutsB && cutsA),
                  pair(a, b) + " ; A.crosses(B)=" + detail::show(crosses) +
                      " but A.separates(B)=" + detail::show(cutsB) +
                      " and B.separates(A)=" + detail::show(cutsA));
    return held();
}

// -------------------------------------------------------------- implications

/** @brief @f$A^\circ \supseteq B \Rightarrow A \supseteq B@f$. */
inline Result interiorContainsImpliesContains(const AnyShape& a, const AnyShape& b) {
    if (!a.interiorContains(b)) {
        return skipped();
    }
    PGLPROP_CHECK(a.contains(b),
                  pair(a, b) + " ; A.interiorContains(B) but not A.contains(B)");
    return held();
}

/** @brief @f$\partial A \supseteq B \Rightarrow A \supseteq B@f$, every shape being closed. */
inline Result boundaryContainsImpliesContains(const AnyShape& a, const AnyShape& b) {
    if (!a.boundaryContains(b)) {
        return skipped();
    }
    PGLPROP_CHECK(a.contains(b),
                  pair(a, b) + " ; A.boundaryContains(B) but not A.contains(B)");
    return held();
}

/** @brief @f$A \supseteq B \neq \emptyset \Rightarrow A \cap B \neq \emptyset@f$. */
inline Result containsImpliesIntersects(const AnyShape& a, const AnyShape& b) {
    if (b.empty() || !a.contains(b)) {
        return skipped();
    }
    PGLPROP_CHECK(a.intersects(b),
                  pair(a, b) + " ; A.contains(B) and B is non-empty, but not A.intersects(B)");
    return held();
}

/** @brief @f$A^\circ \cap B^\circ \subseteq A \cap B@f$. */
inline Result interiorsIntersectImpliesIntersects(const AnyShape& a, const AnyShape& b) {
    if (!a.interiorsIntersect(b)) {
        return skipped();
    }
    PGLPROP_CHECK(a.intersects(b),
                  pair(a, b) + " ; A.interiorsIntersect(B) but not A.intersects(B)");
    return held();
}

/**
 * @brief The boundary and the interior cannot both contain a non-empty shape.
 *
 * @f$\partial A \cap A^\circ = \emptyset@f$ by definition of the relative
 * interior, so a non-empty `B` fits in at most one of them.
 */
inline Result boundaryAndInteriorAreExclusive(const AnyShape& a, const AnyShape& b) {
    if (b.empty()) {
        return skipped();
    }
    const bool onBoundary = a.boundaryContains(b);
    const bool inInterior = a.interiorContains(b);
    PGLPROP_CHECK(!(onBoundary && inInterior),
                  pair(a, b) + " ; non-empty B is reported both on the boundary of A and in its interior");
    return held();
}

/**
 * @brief Containment leaves nothing to separate.
 *
 * @f$A \supseteq B@f$ makes @f$B \setminus A@f$ empty, and the empty set is not
 * disconnected.
 */
inline Result containsImpliesNoSeparation(const AnyShape& a, const AnyShape& b) {
    if (!a.contains(b)) {
        return skipped();
    }
    PGLPROP_CHECK(!a.separates(b),
                  pair(a, b) + " ; A.contains(B) yet A.separates(B) -- B\\A is empty, so it cannot come apart");
    return held();
}

/**
 * @brief Only a shape that meets `B` can cut it.
 *
 * If @f$A \cap B = \emptyset@f$ then @f$B \setminus A = B@f$, whose connectivity
 * is `B`'s own. So a disjoint `A` separates `B` only if `B` was already
 * disconnected — which excludes a multi-component `PolygonSet` from the claim,
 * and nothing else.
 */
inline Result separationImpliesIntersection(const AnyShape& a, const AnyShape& b) {
    if (!isConnected(b) || !a.separates(b)) {
        return skipped();
    }
    PGLPROP_CHECK(a.intersects(b),
                  pair(a, b) + " ; A.separates(connected B) without intersecting it");
    return held();
}

/**
 * @brief A region's interior meets the interior of anything inside it.
 *
 * @f$B \subseteq A^\circ@f$ gives @f$B^\circ \subseteq A^\circ@f$, so the two
 * interiors meet as soon as @f$B^\circ \neq \emptyset@f$ — which is what the
 * non-degenerate region guard buys.
 */
inline Result interiorContainmentMeetsInteriors(const AnyShape& a, const AnyShape& b) {
    if (b.isDegenerate() || b.empty() || !a.interiorContains(b)) {
        return skipped();
    }
    PGLPROP_CHECK(a.interiorsIntersect(b),
                  pair(a, b) + " ; B has interior and sits inside A's interior, yet the interiors are reported disjoint");
    return held();
}

// ------------------------------------------------------------- the empty set

/**
 * @brief The empty set is contained in everything and meets nothing.
 *
 * The clause every predicate needs and each one has to get right separately.
 * Checked against a real `EmptyShape` operand rather than against an
 * incidentally-empty shape, so a failure points at the alternative and not at
 * some emptiness test.
 */
inline Result emptyOperandIsDegenerateCase(const AnyShape& a) {
    const AnyShape nothing = pgl::EmptyShape<PointShape>();
    const std::string prefix = "A = " + detail::show(a) + " against the empty shape: ";

    PGLPROP_CHECK(a.contains(nothing), prefix + "A.contains(empty) is false");
    PGLPROP_CHECK(a.boundaryContains(nothing), prefix + "A.boundaryContains(empty) is false");
    PGLPROP_CHECK(a.interiorContains(nothing), prefix + "A.interiorContains(empty) is false");
    PGLPROP_CHECK(!a.intersects(nothing), prefix + "A.intersects(empty) is true");
    PGLPROP_CHECK(!a.interiorsIntersect(nothing), prefix + "A.interiorsIntersect(empty) is true");
    PGLPROP_CHECK(!a.separates(nothing), prefix + "A.separates(empty) is true");
    PGLPROP_CHECK(!nothing.separates(a), prefix + "empty.separates(A) is true");
    PGLPROP_CHECK(!a.crosses(nothing), prefix + "A.crosses(empty) is true");
    PGLPROP_CHECK(nothing.contains(a) == a.empty(),
                  prefix + "empty.contains(A)=" + detail::show(nothing.contains(a)) +
                      " but A.empty()=" + detail::show(a.empty()));
    return held();
}

// ------------------------------------------------------------------ self-pairs

/**
 * @brief What every shape must say about itself.
 *
 * The self-pair is the case a shape-pair test matrix most often omits, and the
 * three identities here are all forced: @f$A \supseteq A@f$; @f$A \cap A = A@f$,
 * so it is non-empty exactly when `A` is; and @f$A \subseteq \partial A@f$
 * exactly when @f$A^\circ = \emptyset@f$, which is exactly when @f$A^\circ@f$
 * fails to meet itself.
 */
inline Result selfPairIsConsistent(const AnyShape& a) {
    const std::string prefix = "A = " + detail::show(a) + " against itself: ";

    PGLPROP_CHECK(a.contains(a), prefix + "A.contains(A) is false");
    PGLPROP_CHECK(a.intersects(a) == !a.empty(),
                  prefix + "A.intersects(A)=" + detail::show(a.intersects(a)) +
                      " but A.empty()=" + detail::show(a.empty()));
    PGLPROP_CHECK(a.boundaryContains(a) == !a.interiorsIntersect(a),
                  prefix + "A.boundaryContains(A)=" + detail::show(a.boundaryContains(a)) +
                      " but A.interiorsIntersect(A)=" + detail::show(a.interiorsIntersect(a)) +
                      " -- a shape lies in its own boundary exactly when it has no interior");
    return held();
}

}  // namespace props

/** @brief Adds the predicate-algebra properties to a registry. */
inline void registerPredicateProperties(Registry& registry) {
    registry.binary.push_back({"predicates", "intersects-is-symmetric", kNoTag,
                               props::intersectsIsSymmetric});
    registry.binary.push_back({"predicates", "interiors-intersect-is-symmetric", kNoTag,
                               props::interiorsIntersectIsSymmetric});
    registry.binary.push_back({"predicates", "crosses-is-symmetric", kNoTag,
                               props::crossesIsSymmetric});
    registry.binary.push_back({"predicates", "crosses-is-mutual-separation", kNoTag,
                               props::crossesIsMutualSeparation});
    registry.binary.push_back({"predicates", "interior-contains-implies-contains", kNoTag,
                               props::interiorContainsImpliesContains});
    registry.binary.push_back({"predicates", "boundary-contains-implies-contains", kNoTag,
                               props::boundaryContainsImpliesContains});
    registry.binary.push_back({"predicates", "contains-implies-intersects", kNoTag,
                               props::containsImpliesIntersects});
    registry.binary.push_back({"predicates", "interiors-intersect-implies-intersects", kNoTag,
                               props::interiorsIntersectImpliesIntersects});
    registry.binary.push_back({"predicates", "boundary-and-interior-are-exclusive", kNoTag,
                               props::boundaryAndInteriorAreExclusive});
    registry.binary.push_back({"predicates", "contains-implies-no-separation", kNoTag,
                               props::containsImpliesNoSeparation});
    registry.binary.push_back({"predicates", "separation-implies-intersection", kNoTag,
                               props::separationImpliesIntersection});
    registry.binary.push_back({"predicates", "interior-containment-meets-interiors", kRegion,
                               props::interiorContainmentMeetsInteriors});
    registry.unary.push_back({"predicates", "empty-operand-is-degenerate-case", kNoTag,
                              props::emptyOperandIsDegenerateCase});
    registry.unary.push_back({"predicates", "self-pair-is-consistent", kNoTag,
                              props::selfPairIsConsistent});
}

}  // namespace pglprop
