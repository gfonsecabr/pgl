#pragma once

/**
 * @file properties_constructions.hpp
 * @brief Constructions checked against the predicates, and against each other.
 *
 * A predicate answers a question about two shapes; a construction *builds* the
 * answer. Wherever both exist they must agree — `intersection` is non-empty
 * exactly when `intersects` says so, and whatever it returns must lie inside
 * both operands. That makes each an oracle for the other, computed by entirely
 * separate code.
 *
 * The boolean operations get the strongest treatment available anywhere in this
 * harness, because they admit *exact* algebraic identities rather than
 * implications. Areas are rationals over the input lattice and
 * @f$|A \cup B| + |A \cap B| = |A| + |B|@f$ is an equation, not an inequality —
 * so a single arrangement bug shows up as a nonzero residue that no amount of
 * degeneracy can excuse.
 *
 * ## Regularization, and why `A` is compared as `A ∪ A`
 *
 * The boolean operations here are the *regularized* ones: they return
 * @f$\mathrm{closure}(A^\circ \circ B^\circ)@f$, discarding contact of lower
 * dimension. So the identity above cannot be stated against the operands'
 * own areas — for a region whose slits are its only connective tissue,
 * @f$A \cup A@f$ is `closure(A°)` and not `A`, and even the component *count*
 * can change. The properties therefore compare against @f$A \cup A@f$, the
 * regularization of `A`, which is the value the algebra is actually closed over.
 * Idempotence holds up to regularization and no further.
 */

#include "properties_common.hpp"

#include <stdexcept>

namespace pglprop {

namespace props {

/** @brief Result type of every boolean operation here. */
using ExactSet = pgl::PolygonSet<ExactPoint>;

/**
 * @brief Whether the shape has a bounding box at all.
 *
 * `EmptyShape`, `Line`, `OrientedLine`, `Ray` and `Halfplane` never do, and a
 * `HalfplaneIntersection` does only when it happens to come out bounded and
 * non-empty — which is a property of the draw, not of the alternative, so it has
 * to be asked rather than tabulated.
 */
inline bool hasBoundingBox(const AnyShape& shape) {
    try {
        (void)shape.bbox();
        return true;
    } catch (const std::logic_error&) {
        return false;
    }
}

/** @brief Returns @f$A \cup A@f$: the regularization the boolean algebra is closed over. */
inline ExactSet regularized(const AnyShape& shape) {
    return shape.template regularizedUnion<Exact>(shape);
}

// --------------------------------------------------------------- bounding box

/** @brief A shape lies inside its own bounding box. */
inline Result boundingBoxContainsShape(const AnyShape& a) {
    if (!hasBoundingBox(a)) {
        return skipped();
    }
    const AnyShape box = a.bbox();
    PGLPROP_CHECK(box.contains(a),
                  "A = " + detail::show(a) + " is not contained in its own bbox " +
                      detail::show(box));
    return held();
}

/** @brief Intersecting shapes have intersecting bounding boxes. */
inline Result boundingBoxesBoundIntersection(const AnyShape& a, const AnyShape& b) {
    if (!hasBoundingBox(a) || !hasBoundingBox(b) || !a.intersects(b)) {
        return skipped();
    }
    const AnyShape boxA = a.bbox();
    const AnyShape boxB = b.bbox();
    PGLPROP_CHECK(boxA.intersects(boxB),
                  pair(a, b) + " ; the shapes intersect but their boxes " + detail::show(boxA) +
                      " and " + detail::show(boxB) + " do not");
    return held();
}

/**
 * @brief Containment carries over to the bounding boxes.
 *
 * Only where the boxes are *tight*, which rules out a `Disk` operand: its extent
 * is the centre plus an irrational radius, so an integer box has to round
 * outward to remain a bounding box at all. A disk inside a rectangle can then
 * have the larger box of the two, and correctly so — that is the price of a
 * conservative bound, not a containment bug. Every other alternative has lattice
 * vertices and a box that touches them.
 */
inline Result boundingBoxesBoundContainment(const AnyShape& a, const AnyShape& b) {
    if (a.holdsDisk() || b.holdsDisk()) {
        return skipped();
    }
    if (!hasBoundingBox(a) || !hasBoundingBox(b) || !a.contains(b)) {
        return skipped();
    }
    const AnyShape boxA = a.bbox();
    const AnyShape boxB = b.bbox();
    PGLPROP_CHECK(boxA.contains(boxB),
                  pair(a, b) + " ; A contains B but the box " + detail::show(boxA) +
                      " does not contain the box " + detail::show(boxB));
    return held();
}

// --------------------------------------------------------------- intersection

/**
 * @brief `intersection` is non-empty exactly when `intersects` is true.
 *
 * `Shape::intersection` answers with the connected *pieces* of the meet, so a
 * disjoint pair is the empty list and any other pair has at least one piece.
 * Skipped for the pairs that have no `intersection` at all — documented to
 * throw, and a documented throw is not a violation.
 */
inline Result intersectionAgreesWithPredicate(const AnyShape& a, const AnyShape& b) {
    const auto meet = attempt([&] { return a.template intersection<Exact>(b); });
    if (!meet) {
        return skipped();
    }
    const bool nonEmpty =
        std::any_of(meet->begin(), meet->end(), [](const ExactShape& p) { return !p.empty(); });
    const bool meets = a.intersects(b);
    PGLPROP_CHECK(meets == nonEmpty,
                  pair(a, b) + " ; A.intersects(B)=" + detail::show(meets) +
                      " but the intersection is " + showPieces(*meet));
    return held();
}

/** @brief @f$A \cap B \subseteq A@f$ and @f$A \cap B \subseteq B@f$, piece by piece. */
inline Result intersectionSitsInsideBothOperands(const AnyShape& a, const AnyShape& b) {
    const auto meet = attempt([&] { return a.template intersection<Exact>(b); });
    if (!meet || meet->empty()) {
        return skipped();
    }

    const ExactShape exactA = toExact(a);
    const ExactShape exactB = toExact(b);
    bool judged = false;
    for (const ExactShape& piece : *meet) {
        if (piece.empty()) {
            continue;
        }
        try {
            PGLPROP_CHECK(exactA.contains(piece),
                          pair(a, b) + " ; the intersection piece " + detail::show(piece) +
                              " is not contained in A");
            PGLPROP_CHECK(exactB.contains(piece),
                          pair(a, b) + " ; the intersection piece " + detail::show(piece) +
                              " is not contained in B");
        } catch (const std::logic_error&) {
            continue;  // No `contains` for this piece against this operand.
        }
        judged = true;
    }
    return judged ? held() : skipped();
}

// -------------------------------------------------------- boolean area algebra

/**
 * @brief The four boolean operations are defined for the same pairs.
 *
 * They are siblings over one arrangement engine, so a region pair that three of
 * them handle and the fourth rejects is a gap in the family. Checked on its own
 * so that one missing overload is one finding, rather than a failure in every
 * area identity that happens to mention the operation.
 */
inline Result booleanOperationsShareADomain(const AnyShape& a, const AnyShape& b) {
    const bool united = isSupported([&] { return a.template regularizedUnion<Exact>(b); });
    const bool met = isSupported([&] { return a.template regularizedIntersection<Exact>(b); });
    const bool carved = isSupported([&] { return a.template difference<Exact>(b); });
    const bool symmetric = isSupported([&] { return a.template symmetricDifference<Exact>(b); });

    PGLPROP_CHECK(united == met && met == carved && carved == symmetric,
                  pair(a, b) + " ; defined for this region pair: regularizedUnion=" +
                      detail::show(united) + " regularizedIntersection=" + detail::show(met) +
                      " difference=" + detail::show(carved) + " symmetricDifference=" +
                      detail::show(symmetric));
    return held();
}

/** @brief @f$|A \cup B| + |A \cap B| = |A| + |B|@f$, exactly. */
inline Result unionAndIntersectionAreasAddUp(const AnyShape& a, const AnyShape& b) {
    const auto united = attempt([&] { return a.template regularizedUnion<Exact>(b).twiceArea(); });
    const auto met =
        attempt([&] { return a.template regularizedIntersection<Exact>(b).twiceArea(); });
    const auto areaA = attempt([&] { return regularized(a).twiceArea(); });
    const auto areaB = attempt([&] { return regularized(b).twiceArea(); });
    if (!united || !met || !areaA || !areaB) {
        return skipped();
    }

    PGLPROP_CHECK(*united + *met == *areaA + *areaB,
                  pair(a, b) + " ; twiceArea: union=" + detail::show(*united) + " intersection=" +
                      detail::show(*met) + " but A=" + detail::show(*areaA) + " B=" +
                      detail::show(*areaB) + " (" + detail::show(*united + *met) + " != " +
                      detail::show(*areaA + *areaB) + ")");
    return held();
}

/** @brief @f$|A \setminus B| + |A \cap B| = |A|@f$, exactly. */
inline Result differencePartitionsArea(const AnyShape& a, const AnyShape& b) {
    const auto carved = attempt([&] { return a.template difference<Exact>(b).twiceArea(); });
    const auto met =
        attempt([&] { return a.template regularizedIntersection<Exact>(b).twiceArea(); });
    const auto areaA = attempt([&] { return regularized(a).twiceArea(); });
    if (!carved || !met || !areaA) {
        return skipped();
    }

    PGLPROP_CHECK(*carved + *met == *areaA,
                  pair(a, b) + " ; twiceArea: A-B=" + detail::show(*carved) + " intersection=" +
                      detail::show(*met) + " sum " + detail::show(*carved + *met) + " != A=" +
                      detail::show(*areaA));
    return held();
}

/** @brief @f$|A \triangle B| = |A \cup B| - |A \cap B|@f$, exactly. */
inline Result symmetricDifferenceArea(const AnyShape& a, const AnyShape& b) {
    const auto symmetric =
        attempt([&] { return a.template symmetricDifference<Exact>(b).twiceArea(); });
    const auto united = attempt([&] { return a.template regularizedUnion<Exact>(b).twiceArea(); });
    const auto met =
        attempt([&] { return a.template regularizedIntersection<Exact>(b).twiceArea(); });
    if (!symmetric || !united || !met) {
        return skipped();
    }

    PGLPROP_CHECK(*symmetric == *united - *met,
                  pair(a, b) + " ; twiceArea: symmetric difference=" + detail::show(*symmetric) +
                      " but union-intersection=" + detail::show(*united - *met));
    return held();
}

/** @brief @f$A \triangle B = (A \setminus B) \cup (B \setminus A)@f$, as sets. */
inline Result symmetricDifferenceIsUnionOfDifferences(const AnyShape& a, const AnyShape& b) {
    const auto symmetric = attempt([&] { return a.template symmetricDifference<Exact>(b); });
    const auto left = attempt([&] { return a.template difference<Exact>(b); });
    const auto right = attempt([&] { return b.template difference<Exact>(a); });
    if (!symmetric || !left || !right) {
        return skipped();
    }
    const auto rebuilt = attempt([&] { return left->template regularizedUnion<Exact>(*right); });
    if (!rebuilt) {
        return skipped();
    }

    PGLPROP_CHECK(*symmetric == *rebuilt,
                  pair(a, b) + " ; symmetricDifference gives " + detail::show(*symmetric) +
                      " but (A-B) union (B-A) gives " + detail::show(*rebuilt));
    return held();
}

/** @brief Union, intersection and symmetric difference do not depend on the order. */
inline Result booleansAreCommutative(const AnyShape& a, const AnyShape& b) {
    bool checked = false;

    const auto unionForward = attempt([&] { return a.template regularizedUnion<Exact>(b); });
    const auto unionBackward = attempt([&] { return b.template regularizedUnion<Exact>(a); });
    if (unionForward && unionBackward) {
        checked = true;
        PGLPROP_CHECK(*unionForward == *unionBackward,
                      pair(a, b) + " ; A|B=" + detail::show(*unionForward) + " but B|A=" +
                          detail::show(*unionBackward));
    }

    const auto meetForward = attempt([&] { return a.template regularizedIntersection<Exact>(b); });
    const auto meetBackward = attempt([&] { return b.template regularizedIntersection<Exact>(a); });
    if (meetForward && meetBackward) {
        checked = true;
        PGLPROP_CHECK(*meetForward == *meetBackward,
                      pair(a, b) + " ; A&B=" + detail::show(*meetForward) + " but B&A=" +
                          detail::show(*meetBackward));
    }

    const auto symmetricForward = attempt([&] { return a.template symmetricDifference<Exact>(b); });
    const auto symmetricBackward = attempt([&] { return b.template symmetricDifference<Exact>(a); });
    if (symmetricForward && symmetricBackward) {
        checked = true;
        PGLPROP_CHECK(*symmetricForward == *symmetricBackward,
                      pair(a, b) + " ; A^B=" + detail::show(*symmetricForward) + " but B^A=" +
                          detail::show(*symmetricBackward));
    }
    return checked ? held() : skipped();
}

/** @brief The union contains each operand, and each operand contains the intersection. */
inline Result booleanLatticeOrdersOperands(const AnyShape& a, const AnyShape& b) {
    const auto regularA = attempt([&] { return regularized(a); });
    const auto regularB = attempt([&] { return regularized(b); });
    if (!regularA || !regularB) {
        return skipped();
    }
    const ExactShape shapeA(*regularA);
    const ExactShape shapeB(*regularB);
    bool checked = false;

    if (const auto united = attempt([&] { return a.template regularizedUnion<Exact>(b); })) {
        checked = true;
        const ExactShape shapeUnited(*united);
        PGLPROP_CHECK(shapeUnited.contains(shapeA),
                      pair(a, b) + " ; the union " + detail::show(shapeUnited) +
                          " does not contain A as " + detail::show(shapeA));
        PGLPROP_CHECK(shapeUnited.contains(shapeB),
                      pair(a, b) + " ; the union " + detail::show(shapeUnited) +
                          " does not contain B as " + detail::show(shapeB));
    }

    if (const auto met = attempt([&] { return a.template regularizedIntersection<Exact>(b); })) {
        checked = true;
        const ExactShape shapeMet(*met);
        PGLPROP_CHECK(shapeA.contains(shapeMet),
                      pair(a, b) + " ; A as " + detail::show(shapeA) +
                          " does not contain the intersection " + detail::show(shapeMet));
        PGLPROP_CHECK(shapeB.contains(shapeMet),
                      pair(a, b) + " ; B as " + detail::show(shapeB) +
                          " does not contain the intersection " + detail::show(shapeMet));
    }
    return checked ? held() : skipped();
}

/** @brief What is removed is really gone: @f$(A \setminus B)^\circ \cap B^\circ = \emptyset@f$. */
inline Result differenceAvoidsWhatWasRemoved(const AnyShape& a, const AnyShape& b) {
    const auto carved = attempt([&] { return a.template difference<Exact>(b); });
    const auto regularB = attempt([&] { return regularized(b); });
    if (!carved || !regularB) {
        return skipped();
    }
    const ExactShape shapeCarved(*carved);
    const ExactShape shapeB(*regularB);
    PGLPROP_CHECK(!shapeCarved.interiorsIntersect(shapeB),
                  pair(a, b) + " ; the difference " + detail::show(shapeCarved) +
                      " still shares interior with B as " + detail::show(shapeB));
    return held();
}

/**
 * @brief A shape and its own regularization answer the predicates consistently.
 *
 * @f$A \cup A@f$ is the closure of `A`'s interior, cut into components by the
 * cell engine — a decomposition of `A` that owes nothing to how `A` was built.
 * `A` contains it whatever `A` is, and the two are the same point set exactly
 * when `A` has no lower-dimensional part to lose.
 *
 * This is the one place the harness puts a connected operand against a set that
 * covers it a piece at a time: the regularization of a region pinched apart at
 * a point hands those pieces out to several components, and no component holds
 * the region. Drawing the two independently would never pair them.
 */
inline Result regularizationAgreesWithPredicates(const AnyShape& a) {
    const auto regular = attempt([&] { return regularized(a); });
    if (!regular || regular->empty()) {
        return skipped();  // nothing with area survives; the empty set is its own case
    }
    const ExactShape exactA = toExact(a);
    const ExactShape decomposed(*regular);
    const bool holdsDecomposed = exactA.contains(decomposed);
    const bool holdsA = decomposed.contains(exactA);
    PGLPROP_CHECK(holdsDecomposed,
                  "A = " + detail::show(a) + " ; A does not contain its regularization " +
                      detail::show(decomposed));
    PGLPROP_CHECK(exactA.samePointSet(decomposed) == (holdsDecomposed && holdsA),
                  "A = " + detail::show(a) + " ; A|A=" + detail::show(decomposed) +
                      " ; A.samePointSet(A|A)=" + detail::show(exactA.samePointSet(decomposed)) +
                      " but A.contains(A|A)=" + detail::show(holdsDecomposed) +
                      " and (A|A).contains(A)=" + detail::show(holdsA));
    return held();
}

/**
 * @brief The self-operations collapse the way the algebra requires.
 *
 * @f$A \setminus A@f$ and @f$A \triangle A@f$ are empty, and @f$A \cup A@f$ and
 * @f$A \cap A@f$ are the same set — the regularization of `A`. Note what is
 * *not* claimed: that either equals `A`.
 */
inline Result selfBooleansCollapse(const AnyShape& a) {
    bool checked = false;

    if (const auto carved = attempt([&] { return a.template difference<Exact>(a); })) {
        checked = true;
        PGLPROP_CHECK(carved->empty(),
                      "A = " + detail::show(a) + " ; A-A is not empty but " +
                          detail::show(*carved));
    }
    if (const auto symmetric = attempt([&] { return a.template symmetricDifference<Exact>(a); })) {
        checked = true;
        PGLPROP_CHECK(symmetric->empty(),
                      "A = " + detail::show(a) + " ; A^A is not empty but " +
                          detail::show(*symmetric));
    }
    const auto united = attempt([&] { return a.template regularizedUnion<Exact>(a); });
    const auto met = attempt([&] { return a.template regularizedIntersection<Exact>(a); });
    if (united && met) {
        checked = true;
        PGLPROP_CHECK(*united == *met,
                      "A = " + detail::show(a) + " ; A|A=" + detail::show(*united) + " but A&A=" +
                          detail::show(*met));
    }
    return checked ? held() : skipped();
}

// -------------------------------------------------- pointwise boolean oracle

/**
 * @brief Every lattice point of the generation grid, grown by one on each side.
 *
 * For the properties whose per-point work is an integer predicate rather than an
 * exact `contains` against a construction, so the whole grid is affordable — and
 * needed, since the unbounded alternatives have no bounding box to sample
 * against.
 */
inline const std::vector<PointShape>& gridPoints() {
    static const std::vector<PointShape> points = [] {
        std::vector<PointShape> result;
        for (Coord x = -7; x <= 7; ++x) {
            for (Coord y = -7; y <= 7; ++y) {
                result.emplace_back(x, y);
            }
        }
        return result;
    }();
    return points;
}

/**
 * @brief Lattice points to test membership at, for one pair of operands.
 *
 * The lattice of the two bounding boxes together, grown by one on each side so
 * that a point just outside both shapes is always included and the "in neither
 * operand" half of each implication is exercised rather than being vacuously
 * true. Growing by one is enough: the shapes have lattice vertices, so one step
 * out of the joint box is outside both.
 *
 * Bounded to the operands rather than to the whole generation grid because each
 * point costs an exact `contains` against four `PolygonSet` results in
 * `Rational<BigInt>`, and the far corners of the grid only ever repeat the same
 * "outside everything" answer. An operand with no bounding box yields no points,
 * and the caller skips.
 */
inline std::vector<PointShape> samplePoints(const AnyShape& a, const AnyShape& b) {
    if (!hasBoundingBox(a) || !hasBoundingBox(b)) {
        return {};
    }
    const AnyShape boxA = a.bbox();
    const AnyShape boxB = b.bbox();
    const Coord minX = std::min(boxA.bbox().min().x(), boxB.bbox().min().x()) - 1;
    const Coord minY = std::min(boxA.bbox().min().y(), boxB.bbox().min().y()) - 1;
    const Coord maxX = std::max(boxA.bbox().max().x(), boxB.bbox().max().x()) + 1;
    const Coord maxY = std::max(boxA.bbox().max().y(), boxB.bbox().max().y()) + 1;

    std::vector<PointShape> result;
    for (Coord x = minX; x <= maxX; ++x) {
        for (Coord y = minY; y <= maxY; ++y) {
            result.emplace_back(x, y);
        }
    }
    return result;
}

/**
 * @brief The boolean results agree with their operands point by point.
 *
 * The area identities in this group are sums, and a sum forgives a
 * classification error that another cell's error cancels: two cells swapped
 * between @f$A \cap B@f$ and @f$A \setminus B@f$ leave
 * @f$|A \cap B| + |A \setminus B| = |A|@f$ standing. Membership does not
 * forgive it, and a lattice point is a witness that says which cell went wrong.
 *
 * Every implication below is stated so that regularization cannot break it —
 * each one tests the *interior* on whichever side the closure might have added a
 * lower-dimensional piece, so a point on a boundary or a slit is never the
 * deciding case:
 *
 *  - interior to both operands @f$\Rightarrow@f$ in @f$A \cap B@f$ and in
 *    @f$A \cup B@f$;
 *  - interior to @f$A \cap B@f$ @f$\Rightarrow@f$ in both operands;
 *  - interior to @f$A@f$ and outside @f$B@f$ @f$\Rightarrow@f$ in
 *    @f$A \setminus B@f$ and in @f$A \triangle B@f$;
 *  - interior to @f$A \setminus B@f$ @f$\Rightarrow@f$ in @f$A@f$ and not
 *    interior to @f$B@f$.
 */
inline Result booleansAgreePointwise(const AnyShape& a, const AnyShape& b) {
    const auto united = attempt([&] { return a.template regularizedUnion<Exact>(b); });
    const auto met = attempt([&] { return a.template regularizedIntersection<Exact>(b); });
    const auto carved = attempt([&] { return a.template difference<Exact>(b); });
    const auto symmetric = attempt([&] { return a.template symmetricDifference<Exact>(b); });
    if (!united || !met || !carved || !symmetric) {
        return skipped();
    }
    const std::vector<PointShape> samples = samplePoints(a, b);
    if (samples.empty()) {
        return skipped();
    }

    for (const PointShape& p : samples) {
        const ExactPoint exact(p);
        const bool inA = a.contains(p);
        const bool inB = b.contains(p);
        const bool insideA = a.interiorContains(p);
        const bool insideB = b.interiorContains(p);
        const std::string where = pair(a, b) + " ; at the point " + detail::show(p) + ": ";

        if (insideA && insideB) {
            PGLPROP_CHECK(met->contains(exact),
                          where + "interior to both operands, but outside A&B = " +
                              detail::show(*met));
            PGLPROP_CHECK(united->contains(exact),
                          where + "interior to both operands, but outside A|B = " +
                              detail::show(*united));
        }
        if (met->interiorContains(exact)) {
            PGLPROP_CHECK(inA && inB,
                          where + "interior to A&B = " + detail::show(*met) +
                              ", but in A is " + detail::show(inA) + " and in B is " +
                              detail::show(inB));
        }
        if (insideA && !inB) {
            PGLPROP_CHECK(carved->contains(exact),
                          where + "interior to A and outside B, but outside A-B = " +
                              detail::show(*carved));
            PGLPROP_CHECK(symmetric->contains(exact),
                          where + "interior to A and outside B, but outside A^B = " +
                              detail::show(*symmetric));
        }
        if (carved->interiorContains(exact)) {
            PGLPROP_CHECK(inA, where + "interior to A-B = " + detail::show(*carved) +
                                   ", but not in A");
            PGLPROP_CHECK(!insideB, where + "interior to A-B = " + detail::show(*carved) +
                                        ", but also interior to B");
        }
    }
    return held();
}

/**
 * @brief A point interior to both operands makes their interiors intersect.
 *
 * The predicate group derives `interiorsIntersect` from its siblings; this gives
 * it an oracle instead. A lattice point interior to both is a witness that the
 * intersection of the interiors is non-empty, which is the definition, so the
 * predicate has no room to disagree — and unlike the derivations it needs no
 * other predicate to be right.
 */
inline Result interiorWitnessMeetsInteriors(const AnyShape& a, const AnyShape& b) {
    for (const PointShape& p : gridPoints()) {
        if (a.interiorContains(p) && b.interiorContains(p)) {
            PGLPROP_CHECK(a.interiorsIntersect(b),
                          pair(a, b) + " ; both interiors hold " + detail::show(p) +
                              " but interiorsIntersect is false");
            PGLPROP_CHECK(a.intersects(b),
                          pair(a, b) + " ; both interiors hold " + detail::show(p) +
                              " but intersects is false");
            return held();
        }
    }
    return skipped();
}

// ------------------------------------------------------------- Minkowski sums

/**
 * @brief The Minkowski sum of two convex shapes holds every vertex sum and does
 *        not depend on the order.
 *
 * Restricted to `Convex` operands, where the sum of two lattice polygons is a
 * lattice polygon and the check stays in integers. @f$A \oplus B@f$ contains
 * @f$u + v@f$ for every @f$u \in A, v \in B@f$; testing it at the vertices is
 * the strongest finite instance of that.
 */
inline Result minkowskiSumCoversVertexSums(const AnyShape& a, const AnyShape& b) {
    const auto* convexA = a.getIfHoldsConvex();
    const auto* convexB = b.getIfHoldsConvex();
    if (convexA == nullptr || convexB == nullptr || convexA->empty() || convexB->empty()) {
        return skipped();
    }

    const AnyShape sum(convexA->minkowskiSum(*convexB));
    for (const PointShape& u : *convexA) {
        for (const PointShape& v : *convexB) {
            const PointShape translated = u + v;
            PGLPROP_CHECK(sum.contains(translated),
                          pair(a, b) + " ; the Minkowski sum " + detail::show(sum) +
                              " omits the vertex sum " + detail::show(translated));
        }
    }

    const AnyShape reversed(convexB->minkowskiSum(*convexA));
    PGLPROP_CHECK(sum == reversed,
                  pair(a, b) + " ; A+B=" + detail::show(sum) + " but B+A=" +
                      detail::show(reversed));
    return held();
}

// --------------------------------------------------------------- convex hull

/**
 * @brief The convex hull encloses its shape, and adds nothing for a convex one.
 *
 * Two statements, and the second is what gives the first its teeth. `contains`
 * alone is satisfied by a hull that is far too large — the whole point of a hull
 * is that it is the *smallest* convex superset — so a convex operand, whose hull
 * must come back as the same point set, pins the other end. Together they say
 * the hull of a convex shape is that shape and the hull of any shape covers it.
 *
 * `Shape::convexHull` is documented to throw for the alternatives that have
 * none: the empty shape, the unbounded ones and a `Disk`, whose hull is not a
 * polygon.
 */
inline Result convexHullEnclosesItsShape(const AnyShape& a) {
    // The hull must be held at the exact vertex type: a `HalfplaneIntersection`
    // has rational vertices, and wrapping it in an integral `Shape` would round
    // them and test a different polygon.
    const auto hull = attempt([&] { return ExactShape(a.template convexHull<Exact>()); });
    if (!hull) {
        return skipped();
    }
    const ExactShape exact = toExact(a);
    PGLPROP_CHECK(hull->contains(exact),
                  "A = " + detail::show(a) + " ; its convex hull " + detail::show(*hull) +
                      " does not contain it");

    const bool convex = a.holdsTriangle() || a.holdsRectangle() || a.holdsConvex();
    if (convex) {
        PGLPROP_CHECK(exact.contains(*hull),
                      "A = " + detail::show(a) + " is convex, but its hull " +
                          detail::show(*hull) + " is strictly larger");
    }
    return held();
}

/**
 * @brief Eroding by what was used to dilate gives back at least the original.
 *
 * @f$(A \oplus B) \ominus B \supseteq A@f$ holds for any two sets, and with
 * equality exactly when `A` is the opening of itself by `B` — so containment,
 * not equality, is the statement. It is the only relation available that runs a
 * Minkowski sum back through the erosion, which is otherwise checked by nothing:
 * the sum's own property compares it against vertex sums, which the erosion
 * never sees.
 *
 * Both operands are `Convex`, where the sum of two lattice polygons is again a
 * lattice polygon and the whole round trip stays exact.
 */
inline Result erosionUndoesDilation(const AnyShape& a, const AnyShape& b) {
    const auto* convexA = a.getIfHoldsConvex();
    const auto* convexB = b.getIfHoldsConvex();
    if (convexA == nullptr || convexB == nullptr || convexA->empty() || convexB->empty()) {
        return skipped();
    }
    const auto dilated = attempt([&] { return convexA->minkowskiSum(*convexB); });
    if (!dilated) {
        return skipped();
    }
    const auto eroded = attempt([&] { return AnyShape(dilated->minkowskiErosion(*convexB)); });
    if (!eroded) {
        return skipped();
    }
    PGLPROP_CHECK(eroded->contains(a),
                  pair(a, b) + " ; (A+B)-B = " + detail::show(*eroded) +
                      " does not contain A, though eroding by what dilated it cannot lose a point");
    return held();
}

}  // namespace props

/** @brief Adds the construction properties to a registry. */
inline void registerConstructionProperties(Registry& registry) {
    registry.unary.push_back({"bounding", "bbox-contains-shape", kNoTag,
                              props::boundingBoxContainsShape});
    registry.binary.push_back({"bounding", "bbox-bounds-intersection", kNoTag,
                               props::boundingBoxesBoundIntersection});
    registry.binary.push_back({"bounding", "bbox-bounds-containment", kNoTag,
                               props::boundingBoxesBoundContainment});
    registry.binary.push_back({"intersection", "intersection-agrees-with-predicate", kNoTag,
                               props::intersectionAgreesWithPredicate});
    registry.binary.push_back({"intersection", "intersection-sits-inside-both-operands", kNoTag,
                               props::intersectionSitsInsideBothOperands});
    registry.binary.push_back({"boolean", "operations-share-a-domain", kRegion,
                               props::booleanOperationsShareADomain});
    registry.binary.push_back({"boolean", "union-and-intersection-areas-add-up", kRegion,
                               props::unionAndIntersectionAreasAddUp});
    registry.binary.push_back({"boolean", "difference-partitions-area", kRegion,
                               props::differencePartitionsArea});
    registry.binary.push_back({"boolean", "symmetric-difference-area", kRegion,
                               props::symmetricDifferenceArea});
    registry.binary.push_back({"boolean", "symmetric-difference-is-union-of-differences", kRegion,
                               props::symmetricDifferenceIsUnionOfDifferences});
    registry.binary.push_back({"boolean", "booleans-are-commutative", kRegion,
                               props::booleansAreCommutative});
    registry.binary.push_back({"boolean", "boolean-lattice-orders-operands", kRegion,
                               props::booleanLatticeOrdersOperands});
    registry.binary.push_back({"boolean", "difference-avoids-what-was-removed", kRegion,
                               props::differenceAvoidsWhatWasRemoved});
    registry.unary.push_back({"boolean", "self-booleans-collapse", kRegion,
                              props::selfBooleansCollapse});
    registry.unary.push_back({"boolean", "regularization-agrees-with-predicates", kRegion,
                              props::regularizationAgreesWithPredicates});
    registry.binary.push_back({"boolean", "booleans-agree-pointwise", kRegion,
                               props::booleansAgreePointwise});
    registry.binary.push_back({"predicates", "interior-witness-meets-interiors", kNoTag,
                               props::interiorWitnessMeetsInteriors});
    registry.unary.push_back({"bounding", "convex-hull-encloses-its-shape", kNoTag,
                              props::convexHullEnclosesItsShape});
    registry.binary.push_back({"minkowski", "erosion-undoes-dilation", kConvexAlternative,
                               props::erosionUndoesDilation});
    registry.binary.push_back({"minkowski", "minkowski-sum-covers-vertex-sums", kConvexAlternative,
                               props::minkowskiSumCoversVertexSums});
}

}  // namespace pglprop
