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

}  // namespace detail

}  // namespace pgl
