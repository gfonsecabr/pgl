#pragma once

#include "implementation/duality.hpp"

/**
 * @file predicates_helpers.hpp
 * @brief Small dispatch traits and geometry helpers reused by the implementations.
 */


namespace pgl {

namespace detail {

/**
 * @name Shape Category Traits
 * predicates.hpp uses these traits to route generic 'Shape' overloads toward
 * the correct helper at compile time. The single-shape detectors (is_segment,
 * is_line, ...) live next to their shape in the geometry headers, alongside the
 * public XxxConcept they back; this file keeps only the composite detectors.
 */

template <class T>
struct is_area_cut_target : std::false_type {};

template <class PointType, class Label>
struct is_area_cut_target<Rectangle<PointType, Label>> : std::true_type {};

template <class PointType, class Label>
struct is_area_cut_target<Triangle<PointType, Label>> : std::true_type {};

template <class T>
inline constexpr bool is_area_cut_target_v = is_area_cut_target<std::remove_cvref_t<T>>::value;

/**
 * A segment is on the polygon boundary only if one polygon edge contains the
 * whole segment. This deliberately rejects chords whose endpoints are on
 * the boundary but whose interior cuts through the polygon.
 */

template <class Polygon, class OtherSegment>
constexpr bool polygonBoundaryContainsSegment(const Polygon& polygon, const OtherSegment& other) {
    const auto boundary = polygon.edges();
    for (const auto& edge : boundary) {
        if (edge.contains(other)) {
            return true;
        }
    }
    return false;
}

/**
 * Closed rectangle/line intersection.
 *
 * A line meets the rectangle iff the rectangle vertices are not all on the
 * same side of the line, or one vertex lies exactly on the line.
 */
template <class RectangleType, class FirstPoint, class SecondPoint>
constexpr bool lineIntersectsRectangle(const RectangleType& rectangle, const FirstPoint& first, const SecondPoint& second) {
    bool has_positive = false;
    bool has_negative = false;
    const auto vertices = rectangle.vertices();
    for (const auto& vertex : vertices) {
        const auto side = orientationSign(first, second, vertex);
        if (side == std::partial_ordering::equivalent) {
            return true;
        }
        has_positive = has_positive || side == std::partial_ordering::greater;
        has_negative = has_negative || side == std::partial_ordering::less;
    }
    return has_positive && has_negative;
}

/**
 * Strict rectangle-interior / line intersection.
 *
 * This uses the same side test as 'lineIntersectsRectangle', but a tangent line
 * that only touches the boundary does not count.
 */
template <class RectangleType, class FirstPoint, class SecondPoint>
constexpr bool lineIntersectsRectangleInterior(const RectangleType& rectangle, const FirstPoint& first, const SecondPoint& second) {
    bool has_positive = false;
    bool has_negative = false;
    const auto vertices = rectangle.vertices();
    for (const auto& vertex : vertices) {
        const auto side = orientationSign(first, second, vertex);
        has_positive = has_positive || side == std::partial_ordering::greater;
        has_negative = has_negative || side == std::partial_ordering::less;
    }
    return has_positive && has_negative;
}

/**
 * Strict rectangle-interior / segment intersection.
 *
 * After rejecting degenerate segments and accepting segments whose endpoint is
 * already inside, we require the supporting line to cross the rectangle
 * interior. We then count how many distinct rectangle sides the segment
 * touches. Corner hits touch two edges at once, so they are merged before the
 * final enter/exit test.
 */
template <class RectangleType, class FirstPoint, class SecondPoint>
constexpr bool segmentIntersectsRectangleInteriorExact(const RectangleType& rectangle, const FirstPoint& first, const SecondPoint& second) {
    using Coordinate = promoted_number_t<std::common_type_t<
        std::remove_cvref_t<decltype(rectangle.min().x())>,
        std::remove_cvref_t<decltype(first.x())>,
        std::remove_cvref_t<decltype(second.x())>>>;
    using SegmentPoint = Point<Coordinate>;
    using TestSegment = Segment<SegmentPoint>;

    const TestSegment segment{SegmentPoint(first), SegmentPoint(second)};
    if (segment.isDegenerate()) {
        return false;
    }

    if (rectangle.interiorContains(first) || rectangle.interiorContains(second)) {
        return true;
    }

    if (!lineIntersectsRectangleInterior(rectangle, first, second)) {
        return false;
    }

    const auto edges = rectangle.edges();
    std::array<bool, 4> edge_hits{};
    int distinct_boundary_contacts = 0;
    for (std::size_t i = 0; i < edges.size(); ++i) {
        edge_hits[i] = segment.intersects(edges[i]);
        distinct_boundary_contacts += edge_hits[i] ? 1 : 0;
    }

    if (distinct_boundary_contacts < 2) {
        return false;
    }

    const auto vertices = rectangle.vertices();
    if (segment.contains(vertices[0]) && edge_hits[0] && edge_hits[3]) {
        --distinct_boundary_contacts;
    }
    if (segment.contains(vertices[1]) && edge_hits[0] && edge_hits[1]) {
        --distinct_boundary_contacts;
    }
    if (segment.contains(vertices[2]) && edge_hits[1] && edge_hits[2]) {
        --distinct_boundary_contacts;
    }
    if (segment.contains(vertices[3]) && edge_hits[2] && edge_hits[3]) {
        --distinct_boundary_contacts;
    }

    return distinct_boundary_contacts >= 2;
}

/**
 * @brief Tests whether removing a convex shape disconnects a monotone chain.
 *
 * The chain is an arc, so removing `remover ∩ chain` disconnects it exactly
 * when some connected component of that intersection avoids both extreme
 * vertices. Because the remover is convex, its intersection with each chain
 * edge is connected, and consecutive edge pieces belong to one component iff
 * the shared vertex lies in the remover — so one pass over the edges tracks
 * the components exactly, division-free.
 *
 * @pre `remover` is a convex point set (every currently supported shape except
 *      Polygon and MonotoneChain).
 */
template <class Remover, MonotoneChainConcept Chain>
constexpr bool separatesChain(const Remover& remover, const Chain& chain) {
    const std::size_t n = chain.size();
    if (n < 2) {
        // Removing anything from at most one point cannot disconnect it.
        return false;
    }
    bool active = false;     // a component is in progress
    bool touched = false;    // ... and it touches an extreme vertex
    for (std::size_t i = 0; i + 1 < n; ++i) {
        const Segment<typename Chain::PointType> edge(chain[i], chain[i + 1]);
        if (edge.intersects(remover)) {
            const bool connected = active && remover.contains(chain[i]);
            if (!connected) {
                if (active && !touched) {
                    return true;
                }
                touched = false;
            }
            active = true;
            if (i == 0 && remover.contains(chain[0])) {
                touched = true;
            }
            if (i + 2 == n && remover.contains(chain[n - 1])) {
                touched = true;
            }
        } else {
            if (active && !touched) {
                return true;
            }
            active = false;
            touched = false;
        }
    }
    return active && !touched;
}

/**
 * @name HalfplaneIntersection helpers
 * Shared building blocks for predicates involving a half-plane intersection
 * region. A region is convex but possibly unbounded and possibly degenerate
 * (empty interior), so the helpers reduce those states to simpler shapes:
 * a degenerate region to its carrier (point, segment, ray, or line), and an
 * unbounded region — when only its part near a bounded target matters — to a
 * bounded region clipped by an enclosing box.
 */

/**
 * @brief Exact coordinate type for constructions on a half-plane intersection
 * (vertices, edges, chords): rational over BigInt unless the input
 * coordinates are floating point (mirroring detail::separates1DSet).
 */
template <class Number>
using region_exact_number_t =
    std::conditional_t<std::is_floating_point_v<Number>, double, Rational<BigInt>>;

/**
 * @brief Tests whether the region is contained in the half-plane.
 *
 * Uses the redundancy test of `insert` on a copy: the half-plane is discarded
 * exactly when the region is already inside it. The empty region is inside
 * every half-plane. Complexity: O(n) for the copy, O(log n) comparisons.
 */
template <class Region, HalfplaneConcept OtherHalfplane>
constexpr bool regionInsideHalfplane(const Region& region, const OtherHalfplane& halfplane) {
    std::remove_cvref_t<Region> copy(region);
    return !copy.insert(halfplane);
}

/**
 * @brief Tests whether the region is contained in the open interior of the
 * half-plane.
 */
template <class Region, HalfplaneConcept OtherHalfplane>
constexpr bool regionInsideHalfplaneInterior(const Region& region, const OtherHalfplane& halfplane) {
    return regionInsideHalfplane(region, halfplane) && !region.intersects(halfplane.asLine());
}

/**
 * @brief Tests whether the region has points strictly on both sides of the
 * oriented line `first -> second`.
 *
 * True for the whole plane; false for the empty region.
 */
template <class Region, PointConcept FirstPoint, PointConcept SecondPoint>
constexpr bool regionStrictlyOnBothSides(const Region& region, const FirstPoint& first, const SecondPoint& second) {
    const Halfplane<std::remove_cvref_t<FirstPoint>> left(first, second);
    return !regionInsideHalfplane(region, left) && !regionInsideHalfplane(region, left.opposite());
}

/**
 * @brief The point set of a degenerate (empty-interior, nonempty) region as a
 * typed shape with exact coordinates: a point, a segment, a ray, or a line.
 *
 * Precondition: `region.isDegenerate() && !region.isEmpty()`.
 */
template <class Region>
constexpr auto degenerateRegionCarrier(const Region& region) {
    using E = region_exact_number_t<typename Region::NumberType>;
    using EPoint = Point<E, typename Region::PointType::LabelType>;
    using Carrier = std::variant<EPoint, Segment<EPoint>, Ray<EPoint>, Line<EPoint>>;
    if (region.isBounded()) {
        // A point or a segment: the convex hull of the (partly coincident)
        // implicit vertices.
        const auto verts = region.template vertices<E>();
        EPoint lo = verts[0];
        EPoint hi = verts[0];
        for (const auto& v : verts) {
            if (v < lo) {
                lo = v;
            }
            if (hi < v) {
                hi = v;
            }
        }
        if (lo == hi) {
            return Carrier(lo);
        }
        return Carrier(Segment<EPoint>(lo, hi));
    }
    // An unbounded degenerate region is a ray or a line, and then some
    // boundary edge equals the whole region.
    for (std::size_t i = 0; i < region.size(); ++i) {
        const auto e = region.template edge<E>(i);
        if (const auto* ray = std::get_if<Ray<EPoint>>(&e)) {
            return Carrier(*ray);
        }
        if (const auto* line = std::get_if<Line<EPoint>>(&e)) {
            return Carrier(*line);
        }
    }
    // Unreachable for a degenerate region; keep a deterministic fallback.
    return Carrier(EPoint(region[0].source()));
}

/**
 * @brief The region clipped to an axis-aligned box strictly containing the
 * given bounding rectangle (inflated by a margin of two, which also absorbs
 * the toward-zero cast into the region's coordinate type).
 *
 * Predicates against a bounded target shape only depend on the part of the
 * region near the target, so they may be evaluated on the (bounded) clipped
 * region instead; see the call sites for the connectivity argument.
 */
template <class Region, RectangleConcept BoundingRectangle>
constexpr std::remove_cvref_t<Region> regionClippedToBox(const Region& region, const BoundingRectangle& bounds) {
    using Result = std::remove_cvref_t<Region>;
    using RegionPoint = typename Result::PointType;
    using N = typename Result::NumberType;
    using RegionHalfplane = typename Result::HalfplaneType;
    const N margin(2);
    const RegionPoint lo(N(bounds.min().x()) - margin, N(bounds.min().y()) - margin);
    const RegionPoint hi(N(bounds.max().x()) + margin, N(bounds.max().y()) + margin);
    const RegionPoint lohi(lo.x(), hi.y());
    const RegionPoint hilo(hi.x(), lo.y());
    Result result(region);
    result.insert(RegionHalfplane(lo, hilo));
    result.insert(RegionHalfplane(hilo, hi));
    result.insert(RegionHalfplane(hi, lohi));
    result.insert(RegionHalfplane(lohi, lo));
    return result;
}

}  // namespace detail

}  // namespace pgl
