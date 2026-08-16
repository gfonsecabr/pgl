#pragma once

#include "implementation/predicates.hpp"

/**
 * @file samepointset.hpp
 * @brief Exact equality of the point sets represented by arbitrary shapes.
 *
 * `A.samePointSet(B)` asks whether `A` and `B` cover the same points, so it
 * agrees with `A.contains(B) && B.contains(A)` — but it is almost never worth
 * computing that way. Every pair of shape kinds gets its own definition here,
 * each written around three observations:
 *
 * - **Most pairs cannot agree at all.** A defined line is unbounded and a
 *   segment is not; a disk with area has a curved boundary and a convex polygon
 *   does not; a rectangle with area has four corners and a triangle three. Such
 *   a pair is `false` in O(1) once each operand that has collapsed below its
 *   natural dimension is reduced onto the point or segment it covers, which is
 *   the only way the two can meet.
 * - **Shapes are stored canonically.** A `Segment` orders its endpoints, a
 *   `Rectangle` its corners, and `Rectangle`, `Triangle`, `Convex` and
 *   `Polygon` all present their ring counterclockwise from its
 *   lexicographically smallest vertex — a vertex that is a function of the
 *   point set alone, never of how the shape was built. A cached bounding box
 *   and that one vertex settle two independently built rings in O(1), which is
 *   what makes the expected cost of this predicate constant rather than linear.
 *   Rings that agree on both are compared vertex for vertex, and only rings
 *   that disagree there are walked corner by corner, skipping the vertices that
 *   merely subdivide a straight edge — the one freedom two equal rings have
 *   left.
 * - **No definition sums an area.** Twice the area of a ring overflows an
 *   integral coordinate type long before its vertices do, and it costs a full
 *   pass over a shape whose first vertex would have answered the question.
 *   Degeneracy is therefore read off `isPoint` / `isSegment`, which stop at the
 *   first vertex that proves the shape has area.
 *
 * The one shared step is the empty set, in @ref pgl::detail::samePointSetAny:
 * an operand covering no point equals exactly the operands covering no point,
 * whatever the two kinds are. Every definition below may assume both operands
 * are non-empty.
 */

#include <algorithm>
#include <compare>
#include <concepts>
#include <cstddef>
#include <optional>
#include <ranges>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace pgl::detail {

/**
 * @brief Point-set equality for any two shapes, wrappers and empty sets
 * included.
 *
 * Declared here and defined at the end of the file: every definition below
 * reaches back through it to compare the carriers it has reduced an operand to.
 */
template <AnyShapeConcept First, AnyShapeConcept Second>
constexpr bool samePointSetAny(const First& first, const Second& second);

// ---------------------------------------------------------------------------
// Carriers of collapsed shapes

/** @brief The point type of a shape; a @ref pgl::Point is its own. */
template <class TShape>
struct shape_point {
    using type = typename TShape::PointType;
};

template <class Number, class Label>
struct shape_point<Point<Number, Label>> {
    using type = Point<Number, Label>;
};

template <class TShape>
using shape_point_t = typename shape_point<std::remove_cvref_t<TShape>>::type;

/**
 * @brief The single point a shape covers, if it covers exactly one.
 *
 * Every shape that can collapse onto a point answers `getIfPoint`, except the
 * two containers whose collapsed state lives in a ring they own: a region is a
 * point when its outer boundary is, and a set when its one component is.
 * Shapes that cannot collapse at all (a line, a ray, a half-plane: their
 * degenerate forms are undefined) answer an empty optional.
 */
template <class TShape>
constexpr auto collapsedPoint(const TShape& shape) {
    using Shape = std::remove_cvref_t<TShape>;
    if constexpr (PolygonWithHolesConcept<Shape>) {
        return shape.outer().getIfPoint();
    } else if constexpr (PolygonSetConcept<Shape>) {
        using Result = std::optional<typename Shape::PointType>;
        if (shape.componentCount() != 1) {
            return Result{};
        }
        return Result(shape.component(0).outer().getIfPoint());
    } else if constexpr (requires { shape.getIfPoint(); }) {
        return shape.getIfPoint();
    } else {
        return std::optional<shape_point_t<Shape>>{};
    }
}

/**
 * @brief The single segment of positive length a shape covers, if it covers
 * exactly one. The companion of @ref collapsedPoint.
 */
template <class TShape>
constexpr auto collapsedSegment(const TShape& shape) {
    using Shape = std::remove_cvref_t<TShape>;
    if constexpr (PolygonWithHolesConcept<Shape>) {
        return shape.outer().getIfSegment();
    } else if constexpr (PolygonSetConcept<Shape>) {
        using Result = std::optional<Segment<typename Shape::PointType>>;
        if (shape.componentCount() != 1) {
            return Result{};
        }
        return Result(shape.component(0).outer().getIfSegment());
    } else if constexpr (requires { shape.getIfSegment(); }) {
        return shape.getIfSegment();
    } else {
        return std::optional<Segment<shape_point_t<Shape>>>{};
    }
}

/**
 * @brief Tests whether a shape has collapsed below two dimensions.
 *
 * Cheaper than it looks: both probes stop at the first vertex that is off the
 * line through the first two, so a shape with area normally costs O(1) and
 * never costs an area sum.
 */
template <class TShape>
constexpr bool collapsedBelowArea(const TShape& shape) {
    if constexpr (requires { shape.isPoint(); shape.isSegment(); }) {
        return shape.isPoint() || shape.isSegment();
    } else if constexpr (requires { shape.isPoint(); }) {
        return shape.isPoint();
    } else {
        return false;
    }
}

/**
 * @brief Evaluates @p predicate on the point or segment @p shape has collapsed
 * onto, and answers `false` when it has not collapsed.
 *
 * This is the shape of every "these two kinds cannot agree" definition below:
 * the full-dimensional form is out of the question, so the whole answer is what
 * the carrier says. The recursion terminates because a carrier is a `Point` or
 * a `Segment`, and those two are compared directly against everything.
 */
template <class TShape, class Predicate>
constexpr bool reduceCollapsed(const TShape& shape, Predicate predicate) {
    if (const auto vertex = collapsedPoint(shape)) {
        return predicate(*vertex);
    }
    if (const auto carrier = collapsedSegment(shape)) {
        return predicate(*carrier);
    }
    return false;
}

/**
 * @brief The answer for a pair in which @p first can only agree with @p second
 * once it has collapsed onto a point or a segment.
 */
template <class First, class Second>
constexpr bool collapsedMatches(const First& first, const Second& second) {
    return reduceCollapsed(first, [&second](const auto& carrier) {
        return samePointSetAny(carrier, second);
    });
}

/**
 * @brief Evaluates @p predicate on the point, segment, ray or line a
 * half-plane intersection has collapsed onto.
 *
 * The empty-interior states of a region are exactly those four. The
 * full-dimensional states — a half-plane, a wedge, a strip, the plane, a
 * bounded region — have no lower-dimensional carrier and answer `false`.
 */
template <HalfplaneIntersectionConcept Region, class Predicate>
constexpr bool reduceRegionCarrier(const Region& region, Predicate predicate) {
    if (const auto vertex = region.getIfPoint()) {
        return predicate(*vertex);
    }
    if (const auto carrier = region.getIfSegment()) {
        return predicate(*carrier);
    }
    if (const auto carrier = region.getIfRay()) {
        return predicate(*carrier);
    }
    if (const auto carrier = region.getIfLine()) {
        return predicate(*carrier);
    }
    return false;
}

// ---------------------------------------------------------------------------
// The straight primitives

/** @brief Equality of two segments, which store their endpoints in order. */
template <class First, class Second>
constexpr bool sameSegmentPointSet(const First& first, const Second& second) {
    return first.min() == second.min() && first.max() == second.max();
}

/**
 * @brief Equality of two lines: each defining point of one is on the other.
 *
 * Two orientation tests, whatever the two lines store and in whichever
 * coordinate types.
 */
template <class First, class Second>
constexpr bool sameLinePointSet(const First& first, const Second& second) {
    return collinear(first.min(), first.max(), second.min()) &&
           collinear(first.min(), first.max(), second.max());
}

/** @brief Equality of two rays: same source, and the same direction from it. */
template <RayConcept First, RayConcept Second>
constexpr bool sameRayPointSet(const First& first, const Second& second) {
    return first.source() == second.source() &&
           collinear(first.source(), first.target(), second.target()) &&
           dotSign(first.source(), first.target(), second.source(), second.target()) > 0;
}

/**
 * @brief Equality of two half-planes: the same boundary line, kept on the same
 * side of it.
 *
 * A half-plane is stored as the oriented line it lies to the left of, so the
 * two boundaries have to be collinear *and* run the same way round; parallel
 * boundaries pointing opposite ways bound complementary half-planes.
 */
template <class First, class Second>
constexpr bool sameHalfplanePointSet(const First& first, const Second& second) {
    return collinear(first.source(), first.target(), second.source()) &&
           collinear(first.source(), first.target(), second.target()) &&
           dotSign(first.source(), first.target(), second.source(), second.target()) > 0;
}

/**
 * @brief Tests whether a half-plane is the one bounded by the directed edge
 * `tail -> head`, with the region on its left.
 *
 * The half-plane form of @ref sameHalfplanePointSet, for a caller that holds
 * the boundary as two points rather than as a `Halfplane`.
 */
template <class HalfplaneType, class TailPoint, class HeadPoint>
constexpr bool halfplaneBoundedByEdge(const HalfplaneType& halfplane,
                                      const TailPoint& tail,
                                      const HeadPoint& head) {
    return collinear(halfplane.source(), halfplane.target(), tail) &&
           collinear(halfplane.source(), halfplane.target(), head) &&
           dotSign(halfplane.source(), halfplane.target(), tail, head) > 0;
}

// ---------------------------------------------------------------------------
// Bounded shapes

/** @brief Compares the bounding boxes of two bounded shapes. */
template <class First, class Second>
constexpr bool sameBoundingBox(const First& first, const Second& second) {
    const auto& firstBox = first.bbox();
    const auto& secondBox = second.bbox();
    return firstBox.min() == secondBox.min() && firstBox.max() == secondBox.max();
}

// ---------------------------------------------------------------------------
// Polygonal rings

/**
 * @brief Tests whether two shapes of the same kind store the very same
 * representation, which is the cheapest sufficient reason to answer `true`.
 *
 * A shape's representation determines its point set, so equal representations
 * are equal sets; unequal ones prove nothing and fall through to the geometry.
 * This is worth a test where the geometric walk is linear: two shapes that came
 * from the same source agree here in one pass over the raw vertices, without
 * paying for the accessors that rebuild each vertex, which for rational
 * coordinates is arithmetic per vertex. Shapes of different types, or of
 * different coordinate types, have no `operator==` to ask and skip the test.
 */
template <class First, class Second>
constexpr bool sameRepresentation(const First& first, const Second& second) {
    if constexpr (requires { { first == second } -> std::convertible_to<bool>; }) {
        return first == second;
    } else {
        return false;
    }
}

/**
 * @brief Tests whether a ring vertex only subdivides the straight edge around
 * it, so that dropping it leaves the same point set.
 *
 * Collinearity alone would also accept a vertex where the boundary doubles
 * back, which a simple ring does not have but a hand-built one might, so the
 * vertex is also required to lie between its neighbours. That betweenness needs
 * no arithmetic: the lexicographic order is monotone along any line, so a point
 * of a straight edge is between its ends exactly when it compares between them.
 */
template <class Ring>
constexpr bool subdividingVertex(const Ring& ring, std::size_t index) {
    const auto position = static_cast<std::ptrdiff_t>(index);
    const auto previous = ring.get(position - 1);
    const auto current = ring[index];
    const auto next = ring.get(position + 1);
    if (!collinear(previous, current, next)) {
        return false;
    }
    return (previous < current && current < next) || (next < current && current < previous);
}

/**
 * @brief The index of the next ring corner after @p index, wrapping around.
 *
 * Vertex `0` is always a corner: it is the lexicographically smallest vertex,
 * and a vertex that subdivides an edge compares strictly between its
 * neighbours, so it can never be the smallest. That is what lets the walk use
 * `0` as its stopping point.
 */
template <class Ring>
constexpr std::size_t nextRingCorner(const Ring& ring, std::size_t index) {
    const std::size_t count = ring.size();
    do {
        index = index + 1 == count ? 0 : index + 1;
    } while (index != 0 && subdividingVertex(ring, index));
    return index;
}

/**
 * @brief Compares two rings that both enclose area.
 *
 * `Rectangle`, `Triangle`, `Convex`, `Polygon` and the rings of a region all
 * present their vertices counterclockwise from the lexicographically smallest
 * one. Equal point sets share that vertex — the smallest point of a closed
 * polygonal set is a corner of it — so a single comparison rejects rings that
 * differ, in O(1). Rings that pass it are compared vertex for vertex, which is
 * already the answer whenever the two were built the same way; only rings that
 * disagree there are walked corner by corner, since the one freedom two equal
 * rings have left is where they subdivide their straight edges.
 */
template <class First, class Second>
constexpr bool sameRingPointSet(const First& first, const Second& second) {
    if (first.size() < 3 || second.size() < 3) {
        return false;  // no area to enclose
    }
    if (sameRepresentation(first, second)) {
        return true;
    }
    if (!(first[0] == second[0])) {
        return false;
    }
    if (first.size() == second.size()) {
        std::size_t index = 1;
        while (index < first.size() && first[index] == second[index]) {
            ++index;
        }
        if (index == first.size()) {
            return true;
        }
    }
    std::size_t here = 0;
    std::size_t there = 0;
    for (;;) {
        here = nextRingCorner(first, here);
        there = nextRingCorner(second, there);
        if (here == 0 || there == 0) {
            return here == there;  // one ring ran out of corners before the other
        }
        if (!(first[here] == second[there])) {
            return false;
        }
    }
}

/**
 * @brief Compares two shapes that are single polygonal rings, either of which
 * may have collapsed onto a point or a segment.
 */
template <class First, class Second>
constexpr bool sameRingShapes(const First& first, const Second& second) {
    if (sameRepresentation(first, second)) {
        return true;
    }
    if (!sameBoundingBox(first, second)) {
        // Four coordinates read straight out of the cached boxes, with no
        // vertex rebuilt and nothing to overflow.
        return false;
    }
    if (!(first[0] == second[0])) {
        // The first vertex is the lexicographically smallest one whether the
        // ring encloses area or has collapsed onto a segment or a point, and
        // the smallest point of a set is a property of the set. That settles
        // the rings a shared box let through, before anything asks how either
        // one is shaped.
        return false;
    }
    if (collapsedBelowArea(first)) {
        return collapsedMatches(first, second);
    }
    if (collapsedBelowArea(second)) {
        return collapsedMatches(second, first);
    }
    return sameRingPointSet(first, second);
}

/**
 * @brief Matches two equally sized collections of rings that carry no canonical
 * order between them.
 *
 * The holes of a region and the components of a set are kept sorted, but by a
 * representational order that counts vertices, so subdividing one hole can move
 * it past another. Equal collections nearly always still line up, so that is
 * tried first and costs one pass; only when it fails is each ring looked up by
 * geometry. The lookup needs no bookkeeping: distinct holes, and distinct
 * components, are distinct point sets, so a match is unique and an injection
 * between equally sized collections is a bijection.
 */
template <class FirstRange, class SecondRange, class Compare>
constexpr bool sameRingCollection(const FirstRange& first, const SecondRange& second, Compare same) {
    const std::size_t count = first.size();
    bool aligned = true;
    for (std::size_t i = 0; i < count; ++i) {
        if (!same(first[i], second[i])) {
            aligned = false;
            break;
        }
    }
    if (aligned) {
        return true;
    }
    for (const auto& ring : first) {
        bool found = false;
        for (const auto& candidate : second) {
            if (same(ring, candidate)) {
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

/**
 * @brief Compares two holed regions ring by ring.
 *
 * This is complete, not a fast path: the rings of a valid region are a function
 * of its point set. The outer ring bounds the unbounded component of the
 * complement and each hole bounds a bounded one, and a ring that merged two of
 * those, or split one, would have to visit a point twice and would not be a
 * simple polygon.
 */
template <PolygonWithHolesConcept First, PolygonWithHolesConcept Second>
constexpr bool sameRegionRings(const First& first, const Second& second) {
    if (sameRepresentation(first, second)) {
        return true;
    }
    if (!sameRingPointSet(first.outer(), second.outer())) {
        return false;
    }
    if (first.holeCount() != second.holeCount()) {
        return false;
    }
    return sameRingCollection(first.holes(), second.holes(),
                              [](const auto& one, const auto& other) {
                                  return sameRingPointSet(one, other);
                              });
}

// ---------------------------------------------------------------------------
// Boundaries

/**
 * @brief Tests whether a segment is covered by the union of a range of edges.
 *
 * Several collinear edges may share the work, so the overlaps are collected and
 * merged rather than tested one by one. No arithmetic is involved: an overlap
 * of two collinear segments is delimited by two of their four endpoints, and
 * along a line the lexicographic order is the order of the line, so the
 * overlaps are ordinary intervals in that order.
 */
template <class SegmentType, class EdgeRange>
constexpr bool segmentCoveredByEdges(const SegmentType& segment, const EdgeRange& edges) {
    using EdgeType = std::ranges::range_value_t<EdgeRange>;
    using CommonPoint =
        Point<std::common_type_t<typename SegmentType::PointType::NumberType,
                                 typename EdgeType::PointType::NumberType>>;

    const CommonPoint low(segment.min());
    const CommonPoint high(segment.max());
    if (low == high) {
        for (const auto& edge : edges) {
            if (edge.contains(low)) {
                return true;
            }
        }
        return false;
    }

    std::vector<std::pair<CommonPoint, CommonPoint>> overlaps;
    for (const auto& edge : edges) {
        if (!collinear(low, high, edge.min()) || !collinear(low, high, edge.max())) {
            continue;  // the edge leaves the segment's supporting line
        }
        const CommonPoint lo = (edge.min() < low) ? low : CommonPoint(edge.min());
        const CommonPoint hi = (high < edge.max()) ? high : CommonPoint(edge.max());
        if (hi < lo) {
            continue;  // collinear but disjoint from the segment
        }
        overlaps.emplace_back(lo, hi);
    }
    std::sort(overlaps.begin(), overlaps.end());

    CommonPoint covered = low;
    for (const auto& [lo, hi] : overlaps) {
        if (covered < lo) {
            return false;  // the part between covered and lo is off the edges
        }
        if (covered < hi) {
            covered = hi;
        }
    }
    return !(covered < high);
}

/**
 * @brief Compares the boundaries of two polygonal regions or sets as point
 * sets, edge by edge in both directions.
 *
 * Equal boundaries mean equal sets here. Both operands are closed, bounded and
 * carry area, so each is the union of some faces of the arrangement of that
 * common boundary; a face taken by one and not the other would have to be
 * separated from its neighbours by a curve the two disagree on, which is a
 * curve in the common boundary of neither. This is what settles the pairs the
 * rings cannot: a region whose interior is pinched apart at isolated points is
 * the same point set as the set of the pieces it falls into, and there the two
 * ring structures have nothing to do with each other.
 */
template <class First, class Second>
constexpr bool sameBoundaryPointSet(const First& first, const Second& second) {
    const auto firstEdges = first.edges();
    const auto secondEdges = second.edges();
    for (const auto& edge : firstEdges) {
        if (!segmentCoveredByEdges(edge, secondEdges)) {
            return false;
        }
    }
    for (const auto& edge : secondEdges) {
        if (!segmentCoveredByEdges(edge, firstEdges)) {
            return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// Half-plane intersections

/**
 * @brief Compares a half-plane intersection with a polygonal ring.
 *
 * A convex region with area is determined by its non-redundant supporting
 * half-planes, and those are exactly the left half-planes of the ring's corner
 * edges. Both sequences run counterclockwise — the region's sorted by boundary
 * direction, the ring's by construction — so they agree up to where they start,
 * which one scan locates. Comparing half-planes rather than vertices keeps the
 * whole test exact and division-free: a region over integer half-planes has
 * rational vertices.
 */
template <HalfplaneIntersectionConcept Region, class Ring>
constexpr bool sameConvexRegionAndRing(const Region& region, const Ring& ring) {
    if (region.isDegenerate()) {
        return reduceRegionCarrier(region, [&ring](const auto& carrier) {
            return samePointSetAny(carrier, ring);
        });
    }
    if (collapsedBelowArea(ring) || !region.isBounded()) {
        // The region has interior and is unbounded or the ring has neither.
        return false;
    }
    const std::size_t constraints = region.size();
    if (constraints < 3) {
        return false;  // fewer constraints than that bound nothing
    }

    // Where the two cyclic sequences start relative to each other is the only
    // unknown, and the ring's first corner edge locates it.
    const std::size_t behind = nextRingCorner(ring, 0);
    std::size_t start = constraints;
    for (std::size_t i = 0; i < constraints; ++i) {
        if (halfplaneBoundedByEdge(region[i], ring[0], ring[behind])) {
            start = i;
            break;
        }
    }
    if (start == constraints) {
        return false;
    }

    // Walking the corners against the constraints also counts them: the ring
    // has to close after exactly as many corners as there are constraints.
    std::size_t here = 0;
    for (std::size_t i = 0; i < constraints; ++i) {
        const std::size_t next = nextRingCorner(ring, here);
        if (!halfplaneBoundedByEdge(region[(start + i) % constraints], ring[here], ring[next])) {
            return false;
        }
        here = next;
    }
    return here == 0;
}

// ---------------------------------------------------------------------------
// Chains

/**
 * @brief The index of the next corner of an open chain after @p index, stopping
 * at the last vertex.
 */
template <class Chain>
constexpr std::size_t nextChainCorner(const Chain& chain, std::size_t index) {
    const std::size_t last = chain.size() - 1;
    do {
        ++index;
    } while (index < last && collinear(chain[index - 1], chain[index], chain[index + 1]));
    return index;
}

/**
 * @brief Compares two monotone chains.
 *
 * A chain stores its vertices in increasing lexicographic order with duplicates
 * removed, so the point set fixes everything but the vertices that subdivide a
 * straight run — those are collinear with their two neighbours, and being
 * sorted they lie between them. Chains of equal length are compared vertex for
 * vertex first, as two rings are; the corner walk is what is left over.
 */
template <MonotoneChainConcept First, MonotoneChainConcept Second>
constexpr bool sameChainPointSet(const First& first, const Second& second) {
    if (sameRepresentation(first, second)) {
        return true;
    }
    const std::size_t firstCount = first.size();
    const std::size_t secondCount = second.size();
    if (!(first[0] == second[0])) {
        return false;
    }
    if (firstCount == secondCount) {
        std::size_t index = 1;
        while (index < firstCount && first[index] == second[index]) {
            ++index;
        }
        if (index == firstCount) {
            return true;
        }
    }
    std::size_t i = 0;
    std::size_t j = 0;
    for (;;) {
        if (i + 1 == firstCount || j + 1 == secondCount) {
            return i + 1 == firstCount && j + 1 == secondCount;
        }
        i = nextChainCorner(first, i);
        j = nextChainCorner(second, j);
        if (!(first[i] == second[j])) {
            return false;
        }
    }
}

/**
 * @brief Compares two shapes made of edges in a given order, at least one of
 * which is a polyline.
 *
 * A polyline is the one shape with no canonical form at all: it may be given in
 * either direction, may repeat a vertex, and may retrace a part of itself, so
 * two of them cover the same points without any correspondence between their
 * vertices. The cheap agreements are tried first — the same vertices in the
 * same or the opposite order, and a common bounding box, which both shapes
 * cache — and what is left is the definition, each covering the other.
 */
template <class First, class Second>
constexpr bool samePolylinePointSet(const First& first, const Second& second) {
    if (!sameBoundingBox(first, second)) {
        // Both shapes cache their box, and having no canonical first vertex to
        // compare, this is the O(1) rejection they do have.
        return false;
    }
    if (sameRepresentation(first, second)) {
        return true;
    }
    if (const auto carrier = collapsedSegment(first)) {
        return samePointSetAny(*carrier, second);
    }
    if (const auto carrier = collapsedPoint(first)) {
        return samePointSetAny(*carrier, second);
    }
    if (first.size() == second.size()) {
        const std::size_t count = first.size();
        bool forward = true;
        bool backward = true;
        for (std::size_t i = 0; i < count; ++i) {
            forward = forward && first[i] == second[i];
            backward = backward && first[i] == second[count - 1 - i];
        }
        if (forward || backward) {
            return true;
        }
    }
    return first.contains(second) && second.contains(first);
}

// ---------------------------------------------------------------------------

/**
 * @section samepointset-point Point
 * A point is a single point, so every pair here asks whether the other shape
 * has collapsed onto that same point.
 */

/** @brief Two points agree exactly when their coordinates do. */
template <PointConcept First, PointConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return first == second;
}

/** @brief A segment is a point exactly when its two endpoints coincide. */
template <PointConcept First, SegmentConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return second.min() == second.max() && first == second.min();
}

/** @brief A segment is a point exactly when its two endpoints coincide. */
template <PointConcept First, OrientedSegmentConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return second.min() == second.max() && first == second.min();
}

/** @brief A defined line is unbounded; a point is not. */
template <PointConcept First, LineConcept Second>
constexpr bool samePointSet(const First&, const Second&) {
    return false;
}

/** @brief A defined oriented line is unbounded; a point is not. */
template <PointConcept First, OrientedLineConcept Second>
constexpr bool samePointSet(const First&, const Second&) {
    return false;
}

/** @brief A defined ray is unbounded; a point is not. */
template <PointConcept First, RayConcept Second>
constexpr bool samePointSet(const First&, const Second&) {
    return false;
}

/** @brief A defined half-plane is unbounded; a point is not. */
template <PointConcept First, HalfplaneConcept Second>
constexpr bool samePointSet(const First&, const Second&) {
    return false;
}

/** @brief A rectangle is a point only once both its extents have collapsed. */
template <PointConcept First, RectangleConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return collapsedMatches(second, first);
}

/** @brief A triangle is a point only when its three vertices coincide. */
template <PointConcept First, TriangleConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return collapsedMatches(second, first);
}

/** @brief A disk is a point only when its radius is zero. */
template <PointConcept First, DiskConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return collapsedMatches(second, first);
}

/** @brief A convex polygon is a point only when it holds one vertex. */
template <PointConcept First, ConvexConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return collapsedMatches(second, first);
}

/** @brief A chain is a point only when all its vertices coincide. */
template <PointConcept First, MonotoneChainConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return collapsedMatches(second, first);
}

/** @brief A polyline is a point only when all its vertices coincide. */
template <PointConcept First, PolylineConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return collapsedMatches(second, first);
}

/** @brief A polygon is a point only when all its vertices coincide. */
template <PointConcept First, PolygonConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return collapsedMatches(second, first);
}

/** @brief A half-plane intersection is a point only when its constraints meet in one. */
template <PointConcept First, HalfplaneIntersectionConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return collapsedMatches(second, first);
}

/** @brief A region is a point only when its outer boundary is one. */
template <PointConcept First, PolygonWithHolesConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return collapsedMatches(second, first);
}

/** @brief A set is a point only when it holds one component that is one. */
template <PointConcept First, PolygonSetConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return collapsedMatches(second, first);
}

/**
 * @section samepointset-segment Segment
 * A segment covers a bounded straight piece, so it agrees with an unbounded
 * shape never, and with a shape of higher dimension only once that shape has
 * collapsed onto a segment of its own.
 */

/** @brief Both segments store their endpoints in lexicographic order. */
template <SegmentConcept First, SegmentConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return sameSegmentPointSet(first, second);
}

/** @brief Orientation is representation, not geometry: the endpoints decide. */
template <SegmentConcept First, OrientedSegmentConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return sameSegmentPointSet(first, second);
}

/** @brief A defined line is unbounded; a segment is not. */
template <SegmentConcept First, LineConcept Second>
constexpr bool samePointSet(const First&, const Second&) {
    return false;
}

/** @brief A defined oriented line is unbounded; a segment is not. */
template <SegmentConcept First, OrientedLineConcept Second>
constexpr bool samePointSet(const First&, const Second&) {
    return false;
}

/** @brief A defined ray is unbounded; a segment is not. */
template <SegmentConcept First, RayConcept Second>
constexpr bool samePointSet(const First&, const Second&) {
    return false;
}

/** @brief A defined half-plane is unbounded; a segment is not. */
template <SegmentConcept First, HalfplaneConcept Second>
constexpr bool samePointSet(const First&, const Second&) {
    return false;
}

/** @brief Only a rectangle with one collapsed extent is a segment. */
template <SegmentConcept First, RectangleConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return collapsedMatches(second, first);
}

/** @brief Only a triangle with collinear vertices is a segment. */
template <SegmentConcept First, TriangleConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return collapsedMatches(second, first);
}

/** @brief A disk is never a segment: it is a point or it has area. */
template <SegmentConcept First, DiskConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return collapsedMatches(second, first);
}

/** @brief Only a convex polygon holding two vertices is a segment. */
template <SegmentConcept First, ConvexConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return collapsedMatches(second, first);
}

/** @brief A chain covers a segment exactly when its vertices are collinear. */
template <SegmentConcept First, MonotoneChainConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return collapsedMatches(second, first);
}

/**
 * @brief A polyline covers a segment exactly when its vertices are collinear:
 * it is connected, so its edges then fill the segment they span.
 */
template <SegmentConcept First, PolylineConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return collapsedMatches(second, first);
}

/** @brief Only a polygon with collinear vertices is a segment. */
template <SegmentConcept First, PolygonConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return collapsedMatches(second, first);
}

/** @brief Only a region collapsed by touching constraints is a segment. */
template <SegmentConcept First, HalfplaneIntersectionConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return collapsedMatches(second, first);
}

/** @brief A region is a segment only when its outer boundary is one. */
template <SegmentConcept First, PolygonWithHolesConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return collapsedMatches(second, first);
}

/** @brief A set is a segment only when it holds one component that is one. */
template <SegmentConcept First, PolygonSetConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return collapsedMatches(second, first);
}

/**
 * @section samepointset-orientedsegment OrientedSegment
 * The same set as a `Segment`, with the endpoints kept in the caller's order.
 */

/** @brief Orientation is representation, not geometry: the endpoints decide. */
template <OrientedSegmentConcept First, OrientedSegmentConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return sameSegmentPointSet(first, second);
}

/** @brief A defined line is unbounded; a segment is not. */
template <OrientedSegmentConcept First, LineConcept Second>
constexpr bool samePointSet(const First&, const Second&) {
    return false;
}

/** @brief A defined oriented line is unbounded; a segment is not. */
template <OrientedSegmentConcept First, OrientedLineConcept Second>
constexpr bool samePointSet(const First&, const Second&) {
    return false;
}

/** @brief A defined ray is unbounded; a segment is not. */
template <OrientedSegmentConcept First, RayConcept Second>
constexpr bool samePointSet(const First&, const Second&) {
    return false;
}

/** @brief A defined half-plane is unbounded; a segment is not. */
template <OrientedSegmentConcept First, HalfplaneConcept Second>
constexpr bool samePointSet(const First&, const Second&) {
    return false;
}

/** @brief Only a rectangle with one collapsed extent is a segment. */
template <OrientedSegmentConcept First, RectangleConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return collapsedMatches(second, first);
}

/** @brief Only a triangle with collinear vertices is a segment. */
template <OrientedSegmentConcept First, TriangleConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return collapsedMatches(second, first);
}

/** @brief A disk is never a segment: it is a point or it has area. */
template <OrientedSegmentConcept First, DiskConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return collapsedMatches(second, first);
}

/** @brief Only a convex polygon holding two vertices is a segment. */
template <OrientedSegmentConcept First, ConvexConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return collapsedMatches(second, first);
}

/** @brief A chain covers a segment exactly when its vertices are collinear. */
template <OrientedSegmentConcept First, MonotoneChainConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return collapsedMatches(second, first);
}

/** @brief A polyline covers a segment exactly when its vertices are collinear. */
template <OrientedSegmentConcept First, PolylineConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return collapsedMatches(second, first);
}

/** @brief Only a polygon with collinear vertices is a segment. */
template <OrientedSegmentConcept First, PolygonConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return collapsedMatches(second, first);
}

/** @brief Only a region collapsed by touching constraints is a segment. */
template <OrientedSegmentConcept First, HalfplaneIntersectionConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return collapsedMatches(second, first);
}

/** @brief A region is a segment only when its outer boundary is one. */
template <OrientedSegmentConcept First, PolygonWithHolesConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return collapsedMatches(second, first);
}

/** @brief A set is a segment only when it holds one component that is one. */
template <OrientedSegmentConcept First, PolygonSetConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return collapsedMatches(second, first);
}

/**
 * @section samepointset-line Line
 * A line is unbounded in both directions and has no area, which leaves only
 * another line — however it is spelled — to agree with it.
 */

/** @brief Two lines agree when each holds the other's defining points. */
template <LineConcept First, LineConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return sameLinePointSet(first, second);
}

/** @brief Orientation is representation: the supporting line decides. */
template <LineConcept First, OrientedLineConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return sameLinePointSet(first, second);
}

/** @brief A ray stops at its source; a line does not stop. */
template <LineConcept First, RayConcept Second>
constexpr bool samePointSet(const First&, const Second&) {
    return false;
}

/** @brief A half-plane has interior area; a line has none. */
template <LineConcept First, HalfplaneConcept Second>
constexpr bool samePointSet(const First&, const Second&) {
    return false;
}

/** @brief A rectangle is bounded; a defined line is not. */
template <LineConcept First, RectangleConcept Second>
constexpr bool samePointSet(const First&, const Second&) {
    return false;
}

/** @brief A triangle is bounded; a defined line is not. */
template <LineConcept First, TriangleConcept Second>
constexpr bool samePointSet(const First&, const Second&) {
    return false;
}

/** @brief A disk is bounded; a defined line is not. */
template <LineConcept First, DiskConcept Second>
constexpr bool samePointSet(const First&, const Second&) {
    return false;
}

/** @brief A convex polygon is bounded; a defined line is not. */
template <LineConcept First, ConvexConcept Second>
constexpr bool samePointSet(const First&, const Second&) {
    return false;
}

/** @brief A chain has finitely many vertices, so it is bounded. */
template <LineConcept First, MonotoneChainConcept Second>
constexpr bool samePointSet(const First&, const Second&) {
    return false;
}

/** @brief A polyline has finitely many vertices, so it is bounded. */
template <LineConcept First, PolylineConcept Second>
constexpr bool samePointSet(const First&, const Second&) {
    return false;
}

/** @brief A polygon is bounded; a defined line is not. */
template <LineConcept First, PolygonConcept Second>
constexpr bool samePointSet(const First&, const Second&) {
    return false;
}

/** @brief A region of half-planes is a line only when it has one to give. */
template <LineConcept First, HalfplaneIntersectionConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    const auto carrier = second.getIfLine();
    return carrier && sameLinePointSet(first, *carrier);
}

/** @brief A region with holes is bounded; a defined line is not. */
template <LineConcept First, PolygonWithHolesConcept Second>
constexpr bool samePointSet(const First&, const Second&) {
    return false;
}

/** @brief A set of regions is bounded; a defined line is not. */
template <LineConcept First, PolygonSetConcept Second>
constexpr bool samePointSet(const First&, const Second&) {
    return false;
}

/**
 * @section samepointset-orientedline OrientedLine
 * The same set as a `Line`, with a direction the point set does not see.
 */

/** @brief Orientation is representation: the supporting line decides. */
template <OrientedLineConcept First, OrientedLineConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return sameLinePointSet(first, second);
}

/** @brief A ray stops at its source; a line does not stop. */
template <OrientedLineConcept First, RayConcept Second>
constexpr bool samePointSet(const First&, const Second&) {
    return false;
}

/** @brief A half-plane has interior area; a line has none. */
template <OrientedLineConcept First, HalfplaneConcept Second>
constexpr bool samePointSet(const First&, const Second&) {
    return false;
}

/** @brief A rectangle is bounded; a defined line is not. */
template <OrientedLineConcept First, RectangleConcept Second>
constexpr bool samePointSet(const First&, const Second&) {
    return false;
}

/** @brief A triangle is bounded; a defined line is not. */
template <OrientedLineConcept First, TriangleConcept Second>
constexpr bool samePointSet(const First&, const Second&) {
    return false;
}

/** @brief A disk is bounded; a defined line is not. */
template <OrientedLineConcept First, DiskConcept Second>
constexpr bool samePointSet(const First&, const Second&) {
    return false;
}

/** @brief A convex polygon is bounded; a defined line is not. */
template <OrientedLineConcept First, ConvexConcept Second>
constexpr bool samePointSet(const First&, const Second&) {
    return false;
}

/** @brief A chain has finitely many vertices, so it is bounded. */
template <OrientedLineConcept First, MonotoneChainConcept Second>
constexpr bool samePointSet(const First&, const Second&) {
    return false;
}

/** @brief A polyline has finitely many vertices, so it is bounded. */
template <OrientedLineConcept First, PolylineConcept Second>
constexpr bool samePointSet(const First&, const Second&) {
    return false;
}

/** @brief A polygon is bounded; a defined line is not. */
template <OrientedLineConcept First, PolygonConcept Second>
constexpr bool samePointSet(const First&, const Second&) {
    return false;
}

/** @brief A region of half-planes is a line only when it has one to give. */
template <OrientedLineConcept First, HalfplaneIntersectionConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    const auto carrier = second.getIfLine();
    return carrier && sameLinePointSet(first, *carrier);
}

/** @brief A region with holes is bounded; a defined line is not. */
template <OrientedLineConcept First, PolygonWithHolesConcept Second>
constexpr bool samePointSet(const First&, const Second&) {
    return false;
}

/** @brief A set of regions is bounded; a defined line is not. */
template <OrientedLineConcept First, PolygonSetConcept Second>
constexpr bool samePointSet(const First&, const Second&) {
    return false;
}

/**
 * @section samepointset-ray Ray
 * A ray is unbounded in one direction and has no area.
 */

/** @brief Two rays agree when they share a source and a direction. */
template <RayConcept First, RayConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return sameRayPointSet(first, second);
}

/** @brief A half-plane has interior area; a ray has none. */
template <RayConcept First, HalfplaneConcept Second>
constexpr bool samePointSet(const First&, const Second&) {
    return false;
}

/** @brief A rectangle is bounded; a defined ray is not. */
template <RayConcept First, RectangleConcept Second>
constexpr bool samePointSet(const First&, const Second&) {
    return false;
}

/** @brief A triangle is bounded; a defined ray is not. */
template <RayConcept First, TriangleConcept Second>
constexpr bool samePointSet(const First&, const Second&) {
    return false;
}

/** @brief A disk is bounded; a defined ray is not. */
template <RayConcept First, DiskConcept Second>
constexpr bool samePointSet(const First&, const Second&) {
    return false;
}

/** @brief A convex polygon is bounded; a defined ray is not. */
template <RayConcept First, ConvexConcept Second>
constexpr bool samePointSet(const First&, const Second&) {
    return false;
}

/** @brief A chain has finitely many vertices, so it is bounded. */
template <RayConcept First, MonotoneChainConcept Second>
constexpr bool samePointSet(const First&, const Second&) {
    return false;
}

/** @brief A polyline has finitely many vertices, so it is bounded. */
template <RayConcept First, PolylineConcept Second>
constexpr bool samePointSet(const First&, const Second&) {
    return false;
}

/** @brief A polygon is bounded; a defined ray is not. */
template <RayConcept First, PolygonConcept Second>
constexpr bool samePointSet(const First&, const Second&) {
    return false;
}

/** @brief A region of half-planes is a ray only when it has one to give. */
template <RayConcept First, HalfplaneIntersectionConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    const auto carrier = second.getIfRay();
    return carrier && sameRayPointSet(first, *carrier);
}

/** @brief A region with holes is bounded; a defined ray is not. */
template <RayConcept First, PolygonWithHolesConcept Second>
constexpr bool samePointSet(const First&, const Second&) {
    return false;
}

/** @brief A set of regions is bounded; a defined ray is not. */
template <RayConcept First, PolygonSetConcept Second>
constexpr bool samePointSet(const First&, const Second&) {
    return false;
}

/**
 * @section samepointset-halfplane Halfplane
 * A half-plane is the one unbounded shape below with area, so only a region of
 * half-planes that kept a single constraint can match it.
 */

/** @brief Two half-planes agree on the same boundary line and the same side. */
template <HalfplaneConcept First, HalfplaneConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return sameHalfplanePointSet(first, second);
}

/** @brief A rectangle is bounded; a defined half-plane is not. */
template <HalfplaneConcept First, RectangleConcept Second>
constexpr bool samePointSet(const First&, const Second&) {
    return false;
}

/** @brief A triangle is bounded; a defined half-plane is not. */
template <HalfplaneConcept First, TriangleConcept Second>
constexpr bool samePointSet(const First&, const Second&) {
    return false;
}

/** @brief A disk is bounded; a defined half-plane is not. */
template <HalfplaneConcept First, DiskConcept Second>
constexpr bool samePointSet(const First&, const Second&) {
    return false;
}

/** @brief A convex polygon is bounded; a defined half-plane is not. */
template <HalfplaneConcept First, ConvexConcept Second>
constexpr bool samePointSet(const First&, const Second&) {
    return false;
}

/** @brief A chain has finitely many vertices, so it is bounded. */
template <HalfplaneConcept First, MonotoneChainConcept Second>
constexpr bool samePointSet(const First&, const Second&) {
    return false;
}

/** @brief A polyline has finitely many vertices, so it is bounded. */
template <HalfplaneConcept First, PolylineConcept Second>
constexpr bool samePointSet(const First&, const Second&) {
    return false;
}

/** @brief A polygon is bounded; a defined half-plane is not. */
template <HalfplaneConcept First, PolygonConcept Second>
constexpr bool samePointSet(const First&, const Second&) {
    return false;
}

/**
 * @brief A region is a half-plane exactly when one constraint survived: a
 * half-plane contains another only along a shared boundary direction, and
 * `insert` keeps the tighter of those rather than both.
 */
template <HalfplaneConcept First, HalfplaneIntersectionConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    const auto carrier = second.getIfHalfplane();
    return carrier && sameHalfplanePointSet(first, *carrier);
}

/** @brief A region with holes is bounded; a defined half-plane is not. */
template <HalfplaneConcept First, PolygonWithHolesConcept Second>
constexpr bool samePointSet(const First&, const Second&) {
    return false;
}

/** @brief A set of regions is bounded; a defined half-plane is not. */
template <HalfplaneConcept First, PolygonSetConcept Second>
constexpr bool samePointSet(const First&, const Second&) {
    return false;
}

/**
 * @section samepointset-rectangle Rectangle
 * A rectangle with area is a ring of four corners, stored as the two corners
 * that determine it.
 */

/** @brief The stored corners are canonical, so they answer directly. */
template <RectangleConcept First, RectangleConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return first.min() == second.min() && first.max() == second.max();
}

/**
 * @brief A rectangle with area has four corners and a triangle three, so the
 * two can only meet once the rectangle has collapsed.
 */
template <RectangleConcept First, TriangleConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return collapsedMatches(first, second);
}

/** @brief A disk with area has a curved boundary; a rectangle's is straight. */
template <RectangleConcept First, DiskConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return collapsedMatches(second, first);
}

/** @brief Both are polygonal rings; a convex polygon may well be a rectangle. */
template <RectangleConcept First, ConvexConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return sameRingShapes(first, second);
}

/** @brief A chain has no area, so only a collapsed rectangle can match it. */
template <RectangleConcept First, MonotoneChainConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return collapsedMatches(first, second);
}

/** @brief A polyline has no area, so only a collapsed rectangle can match it. */
template <RectangleConcept First, PolylineConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return collapsedMatches(first, second);
}

/** @brief Both are polygonal rings; a polygon may well be a rectangle. */
template <RectangleConcept First, PolygonConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return sameRingShapes(first, second);
}

/** @brief A rectangle is a bounded convex region, which is what a region of half-planes may be. */
template <RectangleConcept First, HalfplaneIntersectionConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return sameConvexRegionAndRing(second, first);
}

/**
 * @brief A hole has area, so it leaves a bounded piece of the plane outside the
 * region that a rectangle has nowhere to put.
 */
template <RectangleConcept First, PolygonWithHolesConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    if (second.hasHoles()) {
        return false;
    }
    return sameRingShapes(first, second.outer());
}

/**
 * @brief Two components meet in finitely many points at most, so their union
 * has disconnected interior, which a rectangle has not.
 */
template <RectangleConcept First, PolygonSetConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    if (second.componentCount() != 1) {
        return false;
    }
    return samePointSetAny(first, second.component(0));
}

/**
 * @section samepointset-triangle Triangle
 * A triangle with area is a ring of exactly three corners.
 */

/**
 * @brief Both triangles keep their vertices from the lexicographically smallest
 * one, counterclockwise, so equal point sets are equal vertex triples.
 */
template <TriangleConcept First, TriangleConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    if (first.isDegenerate() || second.isDegenerate()) {
        return sameRingShapes(first, second);
    }
    return first[0] == second[0] && first[1] == second[1] && first[2] == second[2];
}

/** @brief A disk with area has a curved boundary; a triangle's is straight. */
template <TriangleConcept First, DiskConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return collapsedMatches(second, first);
}

/** @brief Both are polygonal rings; a convex polygon may well be a triangle. */
template <TriangleConcept First, ConvexConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return sameRingShapes(first, second);
}

/** @brief A chain has no area, so only a collapsed triangle can match it. */
template <TriangleConcept First, MonotoneChainConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return collapsedMatches(first, second);
}

/** @brief A polyline has no area, so only a collapsed triangle can match it. */
template <TriangleConcept First, PolylineConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return collapsedMatches(first, second);
}

/** @brief Both are polygonal rings; a polygon may well be a triangle. */
template <TriangleConcept First, PolygonConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return sameRingShapes(first, second);
}

/** @brief A triangle is a bounded convex region, which is what a region of half-planes may be. */
template <TriangleConcept First, HalfplaneIntersectionConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return sameConvexRegionAndRing(second, first);
}

/** @brief A hole leaves a bounded piece of the plane outside the region; a triangle has none. */
template <TriangleConcept First, PolygonWithHolesConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    if (second.hasHoles()) {
        return false;
    }
    return sameRingShapes(first, second.outer());
}

/** @brief Several components have disconnected interior; a triangle has not. */
template <TriangleConcept First, PolygonSetConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    if (second.componentCount() != 1) {
        return false;
    }
    return samePointSetAny(first, second.component(0));
}

/**
 * @section samepointset-disk Disk
 * A disk with positive radius is the only shape here whose boundary curves, so
 * it agrees with nothing else unless it has collapsed onto its centre.
 */

/**
 * @brief Two disks agree when the three boundary points of one lie on the
 * other's circle, which three non-collinear points determine.
 */
template <DiskConcept First, DiskConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    if (first.isPoint() || second.isPoint()) {
        return first.isPoint() && second.isPoint() && first.a() == second.a();
    }
    return inCircleSign(first.a(), first.b(), first.c(), second.a()) == 0 &&
           inCircleSign(first.a(), first.b(), first.c(), second.b()) == 0 &&
           inCircleSign(first.a(), first.b(), first.c(), second.c()) == 0;
}

/** @brief A circle meets a straight boundary in at most two points per edge. */
template <DiskConcept First, ConvexConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return collapsedMatches(first, second);
}

/** @brief A chain has no area; a disk has area unless it is a point. */
template <DiskConcept First, MonotoneChainConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return collapsedMatches(first, second);
}

/** @brief A polyline has no area; a disk has area unless it is a point. */
template <DiskConcept First, PolylineConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return collapsedMatches(first, second);
}

/** @brief A polygon's boundary is straight; a disk's curves. */
template <DiskConcept First, PolygonConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return collapsedMatches(first, second);
}

/** @brief A region of finitely many half-planes has a straight boundary. */
template <DiskConcept First, HalfplaneIntersectionConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return collapsedMatches(first, second);
}

/** @brief A region's boundary is straight; a disk's curves. */
template <DiskConcept First, PolygonWithHolesConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return collapsedMatches(first, second);
}

/** @brief A set's boundary is straight; a disk's curves. */
template <DiskConcept First, PolygonSetConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return collapsedMatches(first, second);
}

/**
 * @section samepointset-convex Convex
 * A convex polygon is a ring, possibly with vertices subdividing its hull
 * edges.
 */

/** @brief Both rings run counterclockwise from their smallest vertex. */
template <ConvexConcept First, ConvexConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return sameRingShapes(first, second);
}

/** @brief A chain has no area, so only a collapsed polygon can match it. */
template <ConvexConcept First, MonotoneChainConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return collapsedMatches(first, second);
}

/** @brief A polyline has no area, so only a collapsed polygon can match it. */
template <ConvexConcept First, PolylineConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return collapsedMatches(first, second);
}

/** @brief Both are polygonal rings; a polygon may well be convex. */
template <ConvexConcept First, PolygonConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return sameRingShapes(first, second);
}

/** @brief A convex polygon is a bounded convex region, as a region of half-planes may be. */
template <ConvexConcept First, HalfplaneIntersectionConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return sameConvexRegionAndRing(second, first);
}

/** @brief A hole leaves a bounded piece of the plane outside the region, which no ring does. */
template <ConvexConcept First, PolygonWithHolesConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    if (second.hasHoles()) {
        return false;
    }
    return sameRingShapes(first, second.outer());
}

/** @brief Several components have disconnected interior; a convex polygon has not. */
template <ConvexConcept First, PolygonSetConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    if (second.componentCount() != 1) {
        return false;
    }
    return samePointSetAny(first, second.component(0));
}

/**
 * @section samepointset-monotonechain MonotoneChain
 * A chain is an open curve with sorted vertices, so it is canonical up to the
 * vertices that subdivide a straight run.
 */

/** @brief Both chains are sorted, so their corners line up one for one. */
template <MonotoneChainConcept First, MonotoneChainConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return sameChainPointSet(first, second);
}

/** @brief A polyline may cover a chain in any order, so it is compared as one. */
template <MonotoneChainConcept First, PolylineConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return samePolylinePointSet(second, first);
}

/** @brief A polygon with area is two-dimensional; a chain is not. */
template <MonotoneChainConcept First, PolygonConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return collapsedMatches(second, first);
}

/** @brief A region of half-planes with interior is two-dimensional; a chain is not. */
template <MonotoneChainConcept First, HalfplaneIntersectionConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return collapsedMatches(second, first);
}

/** @brief A region with area is two-dimensional; a chain is not. */
template <MonotoneChainConcept First, PolygonWithHolesConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return collapsedMatches(second, first);
}

/** @brief A set with area is two-dimensional; a chain is not. */
template <MonotoneChainConcept First, PolygonSetConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return collapsedMatches(second, first);
}

/**
 * @section samepointset-polyline Polyline
 * A polyline is the one shape with no canonical form, so its own pairs are the
 * only ones here that fall back on mutual containment.
 */

/** @brief Two polylines may cover the same points with unrelated vertices. */
template <PolylineConcept First, PolylineConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return samePolylinePointSet(first, second);
}

/** @brief A polygon with area is two-dimensional; a polyline is not. */
template <PolylineConcept First, PolygonConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return collapsedMatches(second, first);
}

/** @brief A region of half-planes with interior is two-dimensional; a polyline is not. */
template <PolylineConcept First, HalfplaneIntersectionConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return collapsedMatches(second, first);
}

/** @brief A region with area is two-dimensional; a polyline is not. */
template <PolylineConcept First, PolygonWithHolesConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return collapsedMatches(second, first);
}

/** @brief A set with area is two-dimensional; a polyline is not. */
template <PolylineConcept First, PolygonSetConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return collapsedMatches(second, first);
}

/**
 * @section samepointset-polygon Polygon
 * A simple polygon is a ring whose interior is connected.
 */

/** @brief Both rings run counterclockwise from their smallest vertex. */
template <PolygonConcept First, PolygonConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return sameRingShapes(first, second);
}

/**
 * @brief A polygon matches a region of half-planes only by being convex, which
 * the constraint-by-constraint comparison decides on its own.
 */
template <PolygonConcept First, HalfplaneIntersectionConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    return sameConvexRegionAndRing(second, first);
}

/** @brief A hole leaves a bounded piece of the plane outside the region, which no simple ring does. */
template <PolygonConcept First, PolygonWithHolesConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    if (second.hasHoles()) {
        return false;
    }
    return sameRingShapes(first, second.outer());
}

/** @brief Several components have disconnected interior; a simple polygon has not. */
template <PolygonConcept First, PolygonSetConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    if (second.componentCount() != 1) {
        return false;
    }
    return samePointSetAny(first, second.component(0));
}

/**
 * @section samepointset-halfplaneintersection HalfplaneIntersection
 * A region of half-planes is convex and may be empty, lower-dimensional,
 * bounded or unbounded, so it is the one shape that meets nearly every other.
 */

/**
 * @brief Full-dimensional regions store a canonical constraint list — sorted by
 * boundary direction, one per direction, none redundant — so they compare
 * element by element. A region with empty interior is compared through its
 * carrier instead, since its stored constraints are not unique.
 */
template <HalfplaneIntersectionConcept First, HalfplaneIntersectionConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    if (first.isDegenerate() || second.isDegenerate()) {
        return reduceRegionCarrier(first, [&second](const auto& carrier) {
            return samePointSetAny(carrier, second);
        });
    }
    if (first.size() != second.size()) {
        return false;
    }
    for (std::size_t i = 0; i < first.size(); ++i) {
        if (!sameHalfplanePointSet(first[i], second[i])) {
            return false;
        }
    }
    return true;
}

/**
 * @brief A hole has area, and a region that is missing an interior disk of the
 * plane is not convex: the chord of the outer boundary through a hole point
 * leaves the region.
 */
template <HalfplaneIntersectionConcept First, PolygonWithHolesConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    if (second.hasHoles()) {
        return false;
    }
    return sameConvexRegionAndRing(first, second.outer());
}

/** @brief Several components have disconnected interior, so their union is not convex. */
template <HalfplaneIntersectionConcept First, PolygonSetConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    if (second.componentCount() != 1) {
        return false;
    }
    return samePointSetAny(first, second.component(0));
}

/**
 * @section samepointset-polygonwithholes PolygonWithHoles
 * A region is a ring minus the interiors of its hole rings, and those rings are
 * a function of the point set.
 */

/** @brief Outer ring against outer ring, then the holes matched up. */
template <PolygonWithHolesConcept First, PolygonWithHolesConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    if (sameRepresentation(first, second)) {
        return true;
    }
    if (!sameBoundingBox(first, second) || !(first.outer()[0] == second.outer()[0])) {
        // A region's box and smallest vertex are its outer ring's, and reject
        // it for the same reasons a ring is rejected.
        return false;
    }
    if (collapsedBelowArea(first)) {
        return collapsedMatches(first, second);
    }
    if (collapsedBelowArea(second)) {
        return collapsedMatches(second, first);
    }
    return sameRegionRings(first, second);
}

/**
 * @brief A region and a set agree ring for ring when the set is one component,
 * and otherwise only through their boundaries.
 *
 * A region whose interior is pinched apart at isolated points — a square whose
 * diamond-shaped hole touches all four sides, say — is exactly the union of the
 * pieces it falls into, and the set spelling out those pieces has a ring
 * structure with no relation to the region's. Their boundaries still coincide,
 * and for closed sets with area that is the same question.
 */
template <PolygonWithHolesConcept First, PolygonSetConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    if (second.componentCount() == 1) {
        return samePointSetAny(first, second.component(0));
    }
    if (collapsedBelowArea(first)) {
        return false;  // several components with area against no area at all
    }
    return sameBoundingBox(first, second) && sameBoundaryPointSet(first, second);
}

/**
 * @section samepointset-polygonset PolygonSet
 * A set is a union of regions with disjoint interiors that meet in finitely
 * many points, which is the one structure below that its point set does not
 * determine.
 */

/**
 * @brief Component against component while the two decompositions agree, and
 * boundary against boundary when they do not.
 *
 * Equal sets need not be cut into components the same way: pieces that touch at
 * a point may be given as one region or as several, so a component count is not
 * even an invariant. The common case is that they do agree, which the
 * componentwise match settles in one pass.
 */
template <PolygonSetConcept First, PolygonSetConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    if (sameRepresentation(first, second)) {
        return true;
    }
    if (first.componentCount() == second.componentCount() &&
        sameRingCollection(first.components(), second.components(),
                           [](const auto& one, const auto& other) {
                               return sameRegionRings(one, other);
                           })) {
        return true;
    }
    if (collapsedBelowArea(first) || collapsedBelowArea(second)) {
        return collapsedMatches(first, second) || collapsedMatches(second, first);
    }
    return sameBoundingBox(first, second) && sameBoundaryPointSet(first, second);
}

// ---------------------------------------------------------------------------

/**
 * @brief Point-set equality for any two shapes, wrappers and empty sets
 * included.
 *
 * Three steps come before the pair definitions above, and only these three:
 *
 * - A polymorphic @ref pgl::Shape is visited down to the alternative it holds,
 *   on either side, without converting anything.
 * - The empty set is settled once for all pairs. An operand covering no point
 *   equals exactly the operands covering no point, so @ref pgl::EmptyShape and
 *   the empty states of the rectangle, the rings, the chains, the region and
 *   the set need no definition of their own.
 * - The pair is ordered by @ref pgl::detail::shapeRank, so each unordered pair
 *   is written once. This is the one place a definition is shared between two
 *   pairs, and it shares an answer that is symmetric by definition.
 */
template <AnyShapeConcept First, AnyShapeConcept Second>
constexpr bool samePointSetAny(const First& first, const Second& second) {
    if constexpr (ShapeConcept<First>) {
        return std::visit(
            [&second](const auto& value) { return samePointSetAny(value, second); },
            first.variant());
    } else if constexpr (ShapeConcept<Second>) {
        return std::visit(
            [&first](const auto& value) { return samePointSetAny(first, value); },
            second.variant());
    } else if constexpr (EmptyShapeConcept<First> || EmptyShapeConcept<Second>) {
        return coversNoPoint(first) && coversNoPoint(second);
    } else {
        const bool firstEmpty = coversNoPoint(first);
        const bool secondEmpty = coversNoPoint(second);
        if (firstEmpty || secondEmpty) {
            return firstEmpty && secondEmpty;
        }
        if constexpr (shapeRank<std::remove_cvref_t<First>> >
                      shapeRank<std::remove_cvref_t<Second>>) {
            return samePointSet(second, first);
        } else {
            return samePointSet(first, second);
        }
    }
}

}  // namespace pgl::detail

namespace pgl {

template <class Number, class Label>
template <AnyShapeConcept OtherShape>
constexpr bool Point<Number, Label>::samePointSet(const OtherShape& other) const {
    return detail::samePointSetAny(*this, other);
}

template <class PointType>
template <AnyShapeConcept OtherShape>
constexpr bool EmptyShape<PointType>::samePointSet(const OtherShape& other) const {
    return detail::samePointSetAny(*this, other);
}

template <class PointType, class LabelType>
template <AnyShapeConcept OtherShape>
constexpr bool Segment<PointType, LabelType>::samePointSet(const OtherShape& other) const {
    return detail::samePointSetAny(*this, other);
}

template <class PointType, class LabelType>
template <AnyShapeConcept OtherShape>
constexpr bool OrientedSegment<PointType, LabelType>::samePointSet(const OtherShape& other) const {
    return detail::samePointSetAny(*this, other);
}

template <class PointType, class LabelType>
template <AnyShapeConcept OtherShape>
constexpr bool Line<PointType, LabelType>::samePointSet(const OtherShape& other) const {
    return detail::samePointSetAny(*this, other);
}

template <class PointType, class LabelType>
template <AnyShapeConcept OtherShape>
constexpr bool OrientedLine<PointType, LabelType>::samePointSet(const OtherShape& other) const {
    return detail::samePointSetAny(*this, other);
}

template <class PointType, class LabelType>
template <AnyShapeConcept OtherShape>
constexpr bool Ray<PointType, LabelType>::samePointSet(const OtherShape& other) const {
    return detail::samePointSetAny(*this, other);
}

template <class PointType, class LabelType>
template <AnyShapeConcept OtherShape>
constexpr bool Halfplane<PointType, LabelType>::samePointSet(const OtherShape& other) const {
    return detail::samePointSetAny(*this, other);
}

template <class PointType, class LabelType>
template <AnyShapeConcept OtherShape>
constexpr bool Rectangle<PointType, LabelType>::samePointSet(const OtherShape& other) const {
    return detail::samePointSetAny(*this, other);
}

template <class PointType, class LabelType>
template <AnyShapeConcept OtherShape>
constexpr bool Triangle<PointType, LabelType>::samePointSet(const OtherShape& other) const {
    return detail::samePointSetAny(*this, other);
}

template <class PointType, class LabelType>
template <AnyShapeConcept OtherShape>
constexpr bool Disk<PointType, LabelType>::samePointSet(const OtherShape& other) const {
    return detail::samePointSetAny(*this, other);
}

template <class PointType, class LabelType>
template <AnyShapeConcept OtherShape>
constexpr bool Convex<PointType, LabelType>::samePointSet(const OtherShape& other) const {
    return detail::samePointSetAny(*this, other);
}

template <class PointType, class LabelType, class Storage>
template <AnyShapeConcept OtherShape>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::samePointSet(const OtherShape& other) const {
    return detail::samePointSetAny(*this, other);
}

template <class PointType, class LabelType>
template <AnyShapeConcept OtherShape>
constexpr bool Polyline<PointType, LabelType>::samePointSet(const OtherShape& other) const {
    return detail::samePointSetAny(*this, other);
}

template <class PointType, class LabelType>
template <AnyShapeConcept OtherShape>
constexpr bool Polygon<PointType, LabelType>::samePointSet(const OtherShape& other) const {
    return detail::samePointSetAny(*this, other);
}

template <class PointType, class LabelType>
template <AnyShapeConcept OtherShape>
constexpr bool HalfplaneIntersection<PointType, LabelType>::samePointSet(const OtherShape& other) const {
    return detail::samePointSetAny(*this, other);
}

template <class PointType, class LabelType>
template <AnyShapeConcept OtherShape>
constexpr bool PolygonWithHoles<PointType, LabelType>::samePointSet(const OtherShape& other) const {
    return detail::samePointSetAny(*this, other);
}

template <class PointType, class LabelType>
template <AnyShapeConcept OtherShape>
constexpr bool PolygonSet<PointType, LabelType>::samePointSet(const OtherShape& other) const {
    return detail::samePointSetAny(*this, other);
}

template <class PointType>
template <AnyShapeConcept OtherShape>
constexpr bool Shape<PointType>::samePointSet(const OtherShape& other) const {
    return detail::samePointSetAny(*this, other);
}

}  // namespace pgl
