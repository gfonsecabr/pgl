#pragma once

/**
 * @file predicates_helpers.hpp
 * @brief Small dispatch traits and geometry helpers reused by the implementations.
 */

#include "../pgl.hpp"

namespace pgl {

namespace detail {

/**
 * @name Shape Category Traits
 * predicates.hpp uses these traits to route generic 'Shape' overloads toward
 * the correct helper at compile time.
 */

template <class T>
struct is_segment : std::false_type {};

template <class PointType>
struct is_segment<Segment<PointType>> : std::true_type {};

template <class T>
inline constexpr bool is_segment_v = is_segment<std::remove_cvref_t<T>>::value;

template <class T>
struct is_oriented_segment : std::false_type {};

template <class PointType>
struct is_oriented_segment<OrientedSegment<PointType>> : std::true_type {};

template <class T>
inline constexpr bool is_oriented_segment_v = is_oriented_segment<std::remove_cvref_t<T>>::value;

template <class T>
struct is_line : std::false_type {};

template <class PointType>
struct is_line<Line<PointType>> : std::true_type {};

template <class T>
inline constexpr bool is_line_v = is_line<std::remove_cvref_t<T>>::value;

template <class T>
struct is_oriented_line : std::false_type {};

template <class PointType>
struct is_oriented_line<OrientedLine<PointType>> : std::true_type {};

template <class T>
inline constexpr bool is_oriented_line_v = is_oriented_line<std::remove_cvref_t<T>>::value;

template <class T>
struct is_ray : std::false_type {};

template <class PointType>
struct is_ray<Ray<PointType>> : std::true_type {};

template <class T>
inline constexpr bool is_ray_v = is_ray<std::remove_cvref_t<T>>::value;

template <class T>
struct is_rectangle : std::false_type {};

template <class PointType>
struct is_rectangle<Rectangle<PointType>> : std::true_type {};

template <class T>
inline constexpr bool is_rectangle_v = is_rectangle<std::remove_cvref_t<T>>::value;

template <class T>
struct is_triangle : std::false_type {};

template <class PointType>
struct is_triangle<Triangle<PointType>> : std::true_type {};

template <class T>
inline constexpr bool is_triangle_v = is_triangle<std::remove_cvref_t<T>>::value;

template <class T>
struct is_halfplane : std::false_type {};

template <class PointType>
struct is_halfplane<Halfplane<PointType>> : std::true_type {};

template <class T>
inline constexpr bool is_halfplane_v = is_halfplane<std::remove_cvref_t<T>>::value;

template <class T>
struct is_area_cut_target : std::false_type {};

template <class PointType>
struct is_area_cut_target<Rectangle<PointType>> : std::true_type {};

template <class PointType>
struct is_area_cut_target<Triangle<PointType>> : std::true_type {};

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

}  // namespace detail

}  // namespace pgl
