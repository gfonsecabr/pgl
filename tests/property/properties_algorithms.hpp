#pragma once

/**
 * @file properties_algorithms.hpp
 * @brief Whole-algorithm properties, driven by a random point set.
 *
 * The predicates get checked against each other; the algorithms mostly cannot
 * be, so each one here is checked against whatever independent oracle it has:
 *
 *  - the **convex hull** against its own definition (contains the input, is
 *    convex, is idempotent, invents no vertices);
 *  - the **segment sweep** against brute force, which is the classic pairing and
 *    the only one where a quadratic reference implementation is both available
 *    and obviously correct;
 *  - the **triangulation** against the area of what it triangulates, and against
 *    the @f$n - 2@f$ triangle count that a simple polygon forces;
 *  - the **arrangement** against the internal consistency of its own DCEL, which
 *    is not an oracle so much as a set of structural identities that any correct
 *    half-edge structure satisfies and a subtly wrong one does not;
 *  - **`sortAround`** against the simplicity of the ring it documents itself to
 *    produce.
 *
 * Point sets shrink with exactly the same machinery as shapes, so a
 * counterexample to any of these arrives minimized too.
 */

#include "properties_common.hpp"

#include <algorithm>
#include <vector>

namespace pglprop {

namespace props {

/** @brief Renders a point list for a failure detail. */
inline std::string showPoints(const std::vector<PointShape>& points) {
    std::string text = "{";
    for (std::size_t i = 0; i < points.size(); ++i) {
        if (i > 0) {
            text += " ";
        }
        text += detail::show(points[i]);
    }
    return text + "}";
}

/** @brief Pairs consecutive points into segments, dropping the degenerate ones. */
inline std::vector<detail::SegmentShape> segmentsFrom(const std::vector<PointShape>& points) {
    std::vector<detail::SegmentShape> segments;
    for (std::size_t i = 0; i + 1 < points.size(); i += 2) {
        const detail::SegmentShape segment(points[i], points[i + 1]);
        if (!segment.isUndefined()) {
            segments.push_back(segment);
        }
    }
    // The sweep is specified over a set of distinct segments; duplicates are a
    // different question from the one being asked here.
    std::sort(segments.begin(), segments.end());
    segments.erase(std::unique(segments.begin(), segments.end()), segments.end());
    return segments;
}

// ---------------------------------------------------------------- convex hull

/** @brief The hull contains every point it was built from. */
inline Result hullContainsItsInput(const std::vector<PointShape>& points) {
    const detail::ConvexShape hull(points);
    if (hull.empty()) {
        return skipped();
    }
    for (const PointShape& point : points) {
        PGLPROP_CHECK(hull.contains(point),
                      "points " + showPoints(points) + " ; the hull " + detail::show(hull) +
                          " does not contain the input point " + detail::show(point));
    }
    return held();
}

/** @brief Hulling a hull changes nothing. */
inline Result hullIsIdempotent(const std::vector<PointShape>& points) {
    const detail::ConvexShape hull(points);
    if (hull.empty()) {
        return skipped();
    }
    const std::vector<PointShape> vertices(hull.begin(), hull.end());
    const detail::ConvexShape again(vertices);
    PGLPROP_CHECK(again == hull,
                  "points " + showPoints(points) + " ; hulling the hull " + detail::show(hull) +
                      " gives the different " + detail::show(again));
    return held();
}

/** @brief The hull invents no vertices. */
inline Result hullVerticesComeFromTheInput(const std::vector<PointShape>& points) {
    const detail::ConvexShape hull(points);
    if (hull.empty()) {
        return skipped();
    }
    for (const PointShape& vertex : hull) {
        PGLPROP_CHECK(std::find(points.begin(), points.end(), vertex) != points.end(),
                      "points " + showPoints(points) + " ; the hull " + detail::show(hull) +
                          " has the vertex " + detail::show(vertex) + ", which is not an input point");
    }
    return held();
}

/**
 * @brief The hull turns one way all the way round.
 *
 * Checked with the same exact orientation predicate the hull is built from, so
 * this is not fully independent — but a Graham scan that leaves one reflex
 * vertex behind still fails it, and that is the failure mode.
 */
inline Result hullIsConvex(const std::vector<PointShape>& points) {
    const detail::ConvexShape hull(points);
    const std::size_t count = hull.size();
    if (count < 3) {
        return skipped();
    }
    for (std::size_t i = 0; i < count; ++i) {
        const PointShape& previous = hull[i];
        const PointShape& current = hull[(i + 1) % count];
        const PointShape& next = hull[(i + 2) % count];
        PGLPROP_CHECK(pgl::orientationSign(previous, current, next) >= 0,
                      "points " + showPoints(points) + " ; the hull " + detail::show(hull) +
                          " turns clockwise at " + detail::show(current));
    }
    return held();
}

// ------------------------------------------------------------- segment sweep

/**
 * @brief Bentley-Ottmann reports exactly what brute force reports.
 *
 * Both the crossing pairs and the full intersection pairs, and the boolean
 * `detectCrossings` short-circuit alongside them — a sweep that agrees on the
 * list but disagrees on whether the list is empty has still gone wrong.
 */
inline Result sweepMatchesBruteForce(const std::vector<PointShape>& points) {
    const std::vector<detail::SegmentShape> segments = segmentsFrom(points);
    if (segments.size() < 2) {
        return skipped();
    }

    auto sweptCrossings = pgl::findCrossings(segments);
    auto bruteCrossings = pgl::bruteForceCrossings(segments);
    std::sort(sweptCrossings.begin(), sweptCrossings.end());
    std::sort(bruteCrossings.begin(), bruteCrossings.end());
    PGLPROP_CHECK(sweptCrossings == bruteCrossings,
                  "segments from " + showPoints(points) + " ; findCrossings reports " +
                      std::to_string(sweptCrossings.size()) + " pairs, bruteForceCrossings " +
                      std::to_string(bruteCrossings.size()));

    auto sweptIntersections = pgl::findIntersections(segments);
    auto bruteIntersections = pgl::bruteForceIntersections(segments);
    std::sort(sweptIntersections.begin(), sweptIntersections.end());
    std::sort(bruteIntersections.begin(), bruteIntersections.end());
    PGLPROP_CHECK(sweptIntersections == bruteIntersections,
                  "segments from " + showPoints(points) + " ; findIntersections reports " +
                      std::to_string(sweptIntersections.size()) + " pairs, bruteForceIntersections " +
                      std::to_string(bruteIntersections.size()));

    PGLPROP_CHECK(pgl::detectCrossings(segments) == !bruteCrossings.empty(),
                  "segments from " + showPoints(points) + " ; detectCrossings disagrees with the " +
                      std::to_string(bruteCrossings.size()) + " crossings found");
    return held();
}

// -------------------------------------------------------------- triangulation

/**
 * @brief A triangulated polygon is covered exactly once.
 *
 * The triangle areas sum to the polygon's area — so nothing is missed and
 * nothing is double-covered — and a simple polygon of `n` vertices with no
 * interior vertices falls into exactly `n - 2` triangles. The hull is used as
 * the polygon so that simplicity is structural and every input vertex is on the
 * boundary, which is what makes the count exact.
 */
inline Result triangulationCoversItsPolygon(const std::vector<PointShape>& points) {
    const detail::ConvexShape hull(points);
    if (hull.size() < 3 || hull.isDegenerate()) {
        return skipped();
    }
    const detail::PolygonShape polygon = hull.asPolygon();

    const pgl::Triangulation<detail::TriangleShape> triangulation(polygon);
    const std::vector<detail::TriangleShape> triangles = triangulation.triangles();

    Coord total = 0;
    for (const detail::TriangleShape& triangle : triangles) {
        total += triangle.twiceArea();
    }
    PGLPROP_CHECK(total == polygon.twiceArea(),
                  "hull of " + showPoints(points) + " ; the " + std::to_string(triangles.size()) +
                      " triangles have twiceArea " + detail::show(total) + " but the polygon " +
                      detail::show(polygon) + " has " + detail::show(polygon.twiceArea()));
    PGLPROP_CHECK(triangles.size() + 2 == polygon.size(),
                  "hull of " + showPoints(points) + " ; a simple polygon of " +
                      std::to_string(polygon.size()) + " vertices must give " +
                      std::to_string(polygon.size() - 2) + " triangles, not " +
                      std::to_string(triangles.size()));
    return held();
}

// ---------------------------------------------------------------- arrangement

/**
 * @brief The arrangement's half-edge structure satisfies its own identities.
 *
 * Every DCEL obeys these: `twin` is an involution without fixed points, walking
 * `next` stays on one face, and consecutive half-edges meet head to tail. They
 * are cheap, they need no oracle, and a boundary cycle that got stitched to the
 * wrong face breaks one of them.
 */
inline Result arrangementDcelIsConsistent(const std::vector<PointShape>& points) {
    const std::vector<detail::SegmentShape> segments = segmentsFrom(points);
    if (segments.empty()) {
        return skipped();
    }

    using Arrangement = pgl::Arrangement<>;
    const Arrangement arrangement(segments);
    const std::size_t count = arrangement.halfedgeCount();

    for (std::size_t i = 0; i < count; ++i) {
        const Arrangement::HalfedgeId halfedge(static_cast<std::uint32_t>(i));
        const Arrangement::HalfedgeId twin = arrangement.twin(halfedge);
        const Arrangement::HalfedgeId next = arrangement.next(halfedge);
        const std::string where = "segments from " + showPoints(points) + " ; halfedge " +
                                  std::to_string(i);

        PGLPROP_CHECK(twin != halfedge, where + " is its own twin");
        PGLPROP_CHECK(arrangement.twin(twin) == halfedge, where + " has twin(twin(h)) != h");
        PGLPROP_CHECK(arrangement.target(halfedge) == arrangement.source(twin),
                      where + " does not end where its twin starts");
        PGLPROP_CHECK(arrangement.source(next) == arrangement.target(halfedge),
                      where + " does not meet its successor head to tail");
        PGLPROP_CHECK(arrangement.face(next) == arrangement.face(halfedge),
                      where + " and its successor lie on different faces");
    }
    return held();
}

// ----------------------------------------------------------- point ordering

/**
 * @brief The Hilbert sort permutes the points and nothing more.
 *
 * A spatial sort has no cheap oracle for its *order*, but losing or duplicating
 * a point is the failure that matters and this catches it.
 */
inline Result hilbertSortIsAPermutation(const std::vector<PointShape>& points) {
    if (points.empty()) {
        return skipped();
    }
    std::vector<PointShape> sorted = points;
    pgl::hilbertSort(sorted);

    std::vector<PointShape> before = points;
    std::sort(before.begin(), before.end());
    std::vector<PointShape> after = sorted;
    std::sort(after.begin(), after.end());

    PGLPROP_CHECK(before == after,
                  "points " + showPoints(points) + " ; hilbertSort returned " +
                      showPoints(sorted) + ", which is not a permutation of the input");
    return held();
}

/**
 * @brief `sortAround` traces a simple polygon, as it documents.
 *
 * The first point is taken as the center and the rest are sorted around it; the
 * documented guarantee is that connecting them in that order gives a simple,
 * star-shaped ring whose kernel holds the center. Duplicates and the center
 * itself are removed first — the guarantee is about a set of distinct points —
 * and a ring that comes out with zero area is skipped, a fully collapsed ring
 * having no simplicity to speak of.
 */
inline Result sortAroundTracesASimpleRing(const std::vector<PointShape>& points) {
    if (points.size() < 4) {
        return skipped();
    }
    const PointShape center = points[0];

    std::vector<PointShape> rest(points.begin() + 1, points.end());
    std::sort(rest.begin(), rest.end());
    rest.erase(std::unique(rest.begin(), rest.end()), rest.end());
    rest.erase(std::remove(rest.begin(), rest.end(), center), rest.end());
    if (rest.size() < 3) {
        return skipped();
    }

    pgl::sortAround(rest, center);
    const detail::PolygonShape ring(rest, true);  // Trust the order: that is the claim.
    if (ring.isDegenerate()) {
        return skipped();
    }
    PGLPROP_CHECK(ring.isSimple(),
                  "center " + detail::show(center) + " and points " + showPoints(rest) +
                      " ; the ring " + detail::show(ring) + " traced by sortAround is not simple");
    return held();
}

}  // namespace props

/** @brief Adds the algorithm-level properties to a registry. */
inline void registerAlgorithmProperties(Registry& registry) {
    registry.pointSet.push_back({"hull", "hull-contains-its-input", 1, 8,
                                 props::hullContainsItsInput});
    registry.pointSet.push_back({"hull", "hull-is-idempotent", 1, 8, props::hullIsIdempotent});
    registry.pointSet.push_back({"hull", "hull-vertices-come-from-the-input", 1, 8,
                                 props::hullVerticesComeFromTheInput});
    registry.pointSet.push_back({"hull", "hull-is-convex", 3, 8, props::hullIsConvex});
    registry.pointSet.push_back({"sweep", "sweep-matches-brute-force", 4, 16,
                                 props::sweepMatchesBruteForce});
    registry.pointSet.push_back({"triangulation", "triangulation-covers-its-polygon", 3, 8,
                                 props::triangulationCoversItsPolygon});
    registry.pointSet.push_back({"arrangement", "arrangement-dcel-is-consistent", 2, 12,
                                 props::arrangementDcelIsConsistent});
    registry.pointSet.push_back({"sorting", "hilbert-sort-is-a-permutation", 1, 12,
                                 props::hilbertSortIsAPermutation});
    registry.pointSet.push_back({"sorting", "sort-around-traces-a-simple-ring", 4, 10,
                                 props::sortAroundTracesASimpleRing});
}

}  // namespace pglprop
