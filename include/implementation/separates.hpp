#pragma once

/**
 * @file separates.hpp
 * @brief Implementations of the 'separates' predicate.
 **/

#include <cstddef>
#include <stdexcept>
#include <limits>
#include "../pgl.hpp"
#include "geometry/orientedline.hpp"
#include "geometry/orientedsegment.hpp"
#include "implementation/orientation.hpp"
#include "predicates_helpers.hpp"


namespace pgl {

/**
 * @section predicates-point Point
 * Point equality and the point-vs-shape predicates. This section also contains
 * the cases where removing a point disconnects a 1D primitive.
 */

template <class Number, class Label>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool Point<Number, Label>::separates(const Segment<OtherPoint, OtherLabel>& other) const {
    return other.interiorContains(*this);
}

template <class Number, class Label>
template<PointConcept OtherPoint>
constexpr bool Point<Number, Label>::separates(const OrientedSegment<OtherPoint>& other) const {
    return static_cast<Segment<OtherPoint>>(other).interiorContains(*this);
}

template <class Number, class Label>
template<PointConcept OtherPoint>
constexpr bool Point<Number, Label>::separates(const Line<OtherPoint>& other) const {
    return other.contains(*this);
}

template <class Number, class Label>
template<PointConcept OtherPoint>
constexpr bool Point<Number, Label>::separates(const OrientedLine<OtherPoint>& other) const {
    return other.asLine().contains(*this);
}

template <class Number, class Label>
template<PointConcept OtherPoint>
constexpr bool Point<Number, Label>::separates(const Ray<OtherPoint>& other) const {
    return other.interiorContains(*this);
}

/**
 * @section predicates-segment Segment
 * Segment endpoint, boundary, containment, collinearity, intersection, and
 * topological predicates, including the generic `separates` / `crosses`
 * dispatch used against 1D and area targets.
 */

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Segment<PointType, LabelType>::separates(const OtherPoint&) const {
    return false;
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool Segment<PointType, LabelType>::separates(const Segment<OtherPoint, OtherLabel>& other) const {
    const int cross = boundingBoxesCross(other);
    if (cross == 0) {
        return false;
    }
    if (cross == 2) {
        return true;
    }
    const auto& a = min();
    const auto& b = max();
    const auto& c = other.min();
    const auto& d = other.max();
    const auto d1 = orientationSign(a, b, c);
    const auto d2 = orientationSign(a, b, d);
    if (d1 == 0 && d2 == 0) {
        return other.interiorContains(a) && other.interiorContains(b);
    }
    const bool other_endpoints_are_on_strictly_opposite_sides =
        d1 != 0 && d2 != 0 && d1 != d2;
    if (!other_endpoints_are_on_strictly_opposite_sides) {
        return false;
    }
    const auto d3 = orientationSign(c, d, a);
    if (d3 == 0 && other.containsCollinear(a)) {
        return true;
    }
    const auto d4 = orientationSign(c, d, b);
    if (d4 == 0 && other.containsCollinear(b)) {
        return true;
    }
    return d3 != 0 && d4 != 0 && d3 != d4;
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Segment<PointType, LabelType>::separates(const OrientedSegment<OtherPoint>& other) const {
    return separates(other.asSegment());
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Segment<PointType, LabelType>::separates(const Line<OtherPoint>& other) const {
    return intersects(other);
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Segment<PointType, LabelType>::separates(const OrientedLine<OtherPoint>& other) const {
    return intersects(other);
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Segment<PointType, LabelType>::separates(const Ray<OtherPoint>& other) const {
    // The segment splits the ray only when it meets the ray ahead of the
    // source: a piece then survives between the source and the segment, and
    // another runs to infinity. If the source lies on the segment, the near
    // piece is empty and the ray stays connected.
    return intersects(other) && !contains(other.source());
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Segment<PointType, LabelType>::separates(const Rectangle<OtherPoint>& other) const {
    return other.interiorsIntersect(*this) && !other.interiorContains(min()) && !other.interiorContains(max());
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Segment<PointType, LabelType>::separates(const Halfplane<OtherPoint>& other) const {
    return other.separates(*this);
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Segment<PointType, LabelType>::separates(const Triangle<OtherPoint>& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }
    if (other.interiorContains(min()) || other.interiorContains(max())) {
        return false;
    }

    const auto triangle_edges = other.edges();
    for (const auto& edge : triangle_edges) {
        if (parallel(edge) &&
            (edge.contains(min()) ||
             edge.contains(max()) ||
             contains(edge.min()) ||
             contains(edge.max()))) {
            return false;
        }
    }

    int boundary_contact_count = 0;
    if (other.boundaryContains(min())) {
        ++boundary_contact_count;
    }
    if (other.boundaryContains(max()) && max() != min()) {
        ++boundary_contact_count;
    }

    const auto triangle_vertices = other.vertices();
    for (const auto& vertex : triangle_vertices) {
        if (contains(vertex) && vertex != min() && vertex != max()) {
            ++boundary_contact_count;
        }
    }

    for (const auto& edge : triangle_edges) {
        if (interiorsIntersect(edge)) {
            ++boundary_contact_count;
        }
    }

    return boundary_contact_count >= 2;
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Segment<PointType, LabelType>::separates(const Convex<OtherPoint>& other) const {
    // The segment separates the polygon iff its intersection with the polygon
    // is a true chord through the interior — neither endpoint lies strictly
    // inside (otherwise the segment ends midway and leaves a slit, not a
    // split) and the segment actually crosses the interior (otherwise it
    // lies along a single boundary edge). Both checks are O(log n).
    return !isDegenerate() && !other.isDegenerate()
           && !other.interiorContains(min())
           && !other.interiorContains(max())
           && other.interiorsIntersect(*this);
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Segment<PointType, LabelType>::separates(const Polygon<OtherPoint>& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }

    const int interior_count = other.interiorContains(min()) + other.interiorContains(max());

    const std::size_t n = other.size();
    int cross_count = 0;
    for (std::size_t i = 0; i < n; ++i) {
        const auto v = other[i];
        const auto w = other[(i + 1) % n];
        bool cv = contains(v);
        bool cw = contains(w);

        if (cv && !cw) {
            if (++cross_count >= 2 + interior_count) {
                return true;
            }
        }
        else if (!cv && !cw && separates(Segment<OtherPoint>(v, w))) {
            if (++cross_count >= 2 + interior_count) {
                return true;
            }
        }
    }

    return false;
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool Segment<PointType, LabelType>::separates(const Disk<OtherPoint, OtherLabel>& other) const {
    // Removing the segment disconnects the disk iff the segment carries a
    // complete chord through the interior: the line must genuinely cross the
    // circle (two distinct intersections) and both of those intersection points
    // must lie within the segment span.
    using R = detail::promoted_number_t<std::common_type_t<
        decltype(other.squaredRadius()), typename PointType::NumberType>>;
    const auto center_point = other.template center<R>();
    const R squared_radius = other.template squaredRadius<R>();

    // v = max - min (segment direction), w = centre - min.
    const R vx = static_cast<R>(max().x()) - static_cast<R>(min().x());
    const R vy = static_cast<R>(max().y()) - static_cast<R>(min().y());
    const R wx = static_cast<R>(center_point.x()) - static_cast<R>(min().x());
    const R wy = static_cast<R>(center_point.y()) - static_cast<R>(min().y());

    const R squared_length = vx * vx + vy * vy;
    const R foot_projection = wx * vx + wy * vy;
    const R cross = vx * wy - vy * wx;

    // discriminant = r^2 |v|^2 - cross^2 is proportional to the squared
    // half-chord length; it must be strictly positive for a real chord (zero
    // means tangent, negative means the line misses the disk).
    const R discriminant = squared_radius * squared_length - cross * cross;
    if (!(discriminant > R{})) {
        return false;
    }

    // The chord spans foot_projection +/- sqrt(discriminant) in the (scaled)
    // projection parameter; it lies within [0, |v|^2] exactly when the near end
    // is >= 0 and the far end <= |v|^2. Both bounds are squared to stay exact.
    const R tail = squared_length - foot_projection;
    return foot_projection >= R{} && foot_projection * foot_projection >= discriminant
        && tail >= R{} && tail * tail >= discriminant;
}

template <class PointType, class LabelType>
constexpr bool Segment<PointType, LabelType>::separates(const Shape<PointType>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->separates(value);
        },
        other.variant());
}

/**
 * @section predicates-triangle Triangle
 * Triangle boundary, containment, intersection, and cut predicates, including
 * triangle-vs-rectangle and triangle-vs-triangle topological cases.
 */

template <class PointType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool Triangle<PointType>::separates(const Segment<OtherPoint, OtherLabel>& other) const {
    return !other.isDegenerate() && !contains(other.min()) && !contains(other.max()) && intersects(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Triangle<PointType>::separates(const OtherPoint&) const {
    return false;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Triangle<PointType>::separates(const OrientedSegment<OtherPoint>& other) const {
    return separates(static_cast<Segment<OtherPoint>>(other));
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Triangle<PointType>::separates(const Line<OtherPoint>& other) const {
    return !other.isDegenerate() && intersects(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Triangle<PointType>::separates(const OrientedLine<OtherPoint>& other) const {
    return separates(other.asLine());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Triangle<PointType>::separates(const Ray<OtherPoint>& other) const {
    // Removing the triangle splits the ray iff the ray reaches it with its
    // source outside: the far end runs to infinity (always outside), so any
    // contact -- including a tangential touch of the boundary -- leaves a piece
    // on each side.
    return !other.isDegenerate() && !isDegenerate()
           && !contains(other.source()) && intersects(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Triangle<PointType>::separates(const Halfplane<OtherPoint>&) const {
    return false;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Triangle<PointType>::separates(const Rectangle<OtherPoint>& other) const {
    if (other.isDegenerate()) {
        return false;
    }

    const auto target_edges = other.edges();
    if ((separates(target_edges[0]) && separates(target_edges[2])) ||
        (separates(target_edges[1]) && separates(target_edges[3]))) {
        return true;
    }

    const auto target_vertices = other.vertices();
    if ((contains(target_vertices[0]) && contains(target_vertices[2])) ||
        (contains(target_vertices[1]) && contains(target_vertices[3]))) {
        return true;
    }

    return (contains(target_vertices[0]) &&
            (separates(target_edges[1]) || separates(target_edges[2]))) ||
           (contains(target_vertices[1]) &&
            (separates(target_edges[2]) || separates(target_edges[3]))) ||
           (contains(target_vertices[2]) &&
            (separates(target_edges[3]) || separates(target_edges[0]))) ||
           (contains(target_vertices[3]) &&
            (separates(target_edges[0]) || separates(target_edges[1])));
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Triangle<PointType>::separates(const Triangle<OtherPoint>& other) const {
    if (other.isDegenerate()) {
        return false;
    }

    const auto target_edges = other.edges();
    if (contains(other.a()) && separates(target_edges[1])) {
        return true;
    }
    if (contains(other.b()) && separates(target_edges[2])) {
        return true;
    }
    if (contains(other.c()) && separates(target_edges[0])) {
        return true;
    }

    int separated_edges = 0;
    for (const auto& edge : target_edges) {
        separated_edges += separates(edge) ? 1 : 0;
    }
    return separated_edges >= 2;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Triangle<PointType>::separates(const Convex<OtherPoint>& other) const {
    if (other.isDegenerate()) {
        return false;
    }

    int interior_vertex_count = 0;
    for (const auto& vertex : vertices()) {
        if (other.interiorContains(vertex)) {
            interior_vertex_count++;
            if (interior_vertex_count >= 2) {
                return false;
            }
        }
    }
    if (interior_vertex_count == 1) {
        for (const auto& edge : edges()) {
            if (edge.separates(other)) {
                return true;
            }
        }
        return false;
    }

    int separating_edge_count = 0;
    for (const auto& edge : edges()) {
        if (edge.separates(other)) {
            separating_edge_count++;
        }
    }
    return separating_edge_count >= 2;
}

template <class PointType>
constexpr bool Triangle<PointType>::separates(const Shape<PointType>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->separates(value);
        },
        other.variant());
}

/**
 * @section predicates-oriented-segment OrientedSegment
 * Oriented-segment predicates. Most topology delegates to the unoriented
 * segment view, with local methods kept for orientation-sensitive behavior.
 */

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedSegment<PointType>::separates(const OtherPoint& other) const {
    return this->asSegment().separates(other);
}

template <class PointType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool OrientedSegment<PointType>::separates(const Segment<OtherPoint, OtherLabel>& other) const {
    return this->asSegment().separates(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedSegment<PointType>::separates(const OrientedSegment<OtherPoint>& other) const {
    return this->asSegment().separates(other.asSegment());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedSegment<PointType>::separates(const Line<OtherPoint>& other) const {
    return this->asSegment().separates(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedSegment<PointType>::separates(const OrientedLine<OtherPoint>& other) const {
    return this->asSegment().separates(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedSegment<PointType>::separates(const Ray<OtherPoint>& other) const {
    return this->asSegment().separates(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedSegment<PointType>::separates(const Rectangle<OtherPoint>& other) const {
    return this->asSegment().separates(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedSegment<PointType>::separates(const Triangle<OtherPoint>& other) const {
    return this->asSegment().separates(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedSegment<PointType>::separates(const Halfplane<OtherPoint>& other) const {
    return this->asSegment().separates(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedSegment<PointType>::separates(const Convex<OtherPoint>& other) const {
    return this->asSegment().separates(other);
}

template <class PointType>
constexpr bool OrientedSegment<PointType>::separates(const Shape<PointType>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->separates(value);
        },
        other.variant());
}

/**
 * @section predicates-line Line
 * Geometric line predicates: geometric equality/order, containment,
 * intersection against 1D and 2D shapes, and generic separation dispatch.
 */

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Line<PointType>::separates(const OtherPoint&) const {
    return false;
}

template <class PointType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool Line<PointType>::separates(const Segment<OtherPoint, OtherLabel>& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }
    const auto first_side = orientationSign(min(), max(), other.min());
    const auto second_side = orientationSign(min(), max(), other.max());
    return first_side != 0 && second_side != 0 && first_side != second_side;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Line<PointType>::separates(const OrientedSegment<OtherPoint>& other) const {
    return separates(other.asSegment());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Line<PointType>::separates(const Line<OtherPoint>& other) const {
    return !isDegenerate() && !other.isDegenerate() &&
           intersects(other) && !collinear(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Line<PointType>::separates(const OrientedLine<OtherPoint>& other) const {
    return separates(other.asLine());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Line<PointType>::separates(const Ray<OtherPoint>& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }
    const auto source_side = orientationSign(min(), max(), other.source());
    const auto target_side = orientationSign(min(), max(), other.target());
    if (source_side == 0) {
        return false;
    }
    return target_side == 0 || target_side != source_side;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Line<PointType>::separates(const Rectangle<OtherPoint>& other) const {
    return other.interiorsIntersect(*this);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Line<PointType>::separates(const Triangle<OtherPoint>& other) const {
    if (isDegenerate()) {
        return false;
    }
    return other.interiorsIntersect(*this);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Line<PointType>::separates(const Halfplane<OtherPoint>& other) const {
    return other.separates(*this);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Line<PointType>::separates(const Convex<OtherPoint>& other) const {
    if (isDegenerate()) {
        return false;
    }
    return other.interiorsIntersect(*this);
}

template <class PointType>
constexpr bool Line<PointType>::separates(const Shape<PointType>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->separates(value);
        },
        other.variant());
}

/**
 * @section predicates-oriented-line OrientedLine
 * Oriented-line predicates. Shared topology is mostly delegated to the
 * unoriented line view, while orientation-specific methods stay local here.
 */

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedLine<PointType>::separates(const OtherPoint& other) const {
    return this->asLine().separates(other);
}

template <class PointType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool OrientedLine<PointType>::separates(const Segment<OtherPoint, OtherLabel>& other) const {
    return this->asLine().separates(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedLine<PointType>::separates(const OrientedSegment<OtherPoint>& other) const {
    return this->asLine().separates(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedLine<PointType>::separates(const Line<OtherPoint>& other) const {
    return this->asLine().separates(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedLine<PointType>::separates(const OrientedLine<OtherPoint>& other) const {
    return this->asLine().separates(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedLine<PointType>::separates(const Ray<OtherPoint>& other) const {
    return this->asLine().separates(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedLine<PointType>::separates(const Rectangle<OtherPoint>& other) const {
    return this->asLine().separates(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedLine<PointType>::separates(const Triangle<OtherPoint>& other) const {
    return this->asLine().separates(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedLine<PointType>::separates(const Halfplane<OtherPoint>& other) const {
    return this->asLine().separates(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedLine<PointType>::separates(const Convex<OtherPoint>& other) const {
    return this->asLine().separates(other);
}

template <class PointType>
constexpr bool OrientedLine<PointType>::separates(const Shape<PointType>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->separates(value);
        },
        other.variant());
}

/**
 * @section predicates-ray Ray
 * Ray-specific containment, intersection, and topological predicates. This is
 * where the asymmetric behavior of a half-infinite 1D primitive is implemented.
 */

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Ray<PointType>::separates(const OtherPoint&) const {
    return false;
}

template <class PointType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool Ray<PointType>::separates(const Segment<OtherPoint, OtherLabel>& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }

    const auto first_side = orientationSign(source(), target(), other.min());
    const auto second_side = orientationSign(source(), target(), other.max());
    if (first_side == 0 && second_side == 0) {
        return false;
    }
    if (other.interiorContains(source())) {
        return true;
    }
    if (first_side == 0 || second_side == 0 || first_side == second_side) {
        return false;
    }

    // The segment crosses the ray's supporting line at a point interior to the
    // segment; it cuts the segment in two exactly when that crossing lies on the
    // ray itself (anywhere from the source onward, not just up to the target).
    return intersects(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Ray<PointType>::separates(const OrientedSegment<OtherPoint>& other) const {
    return separates(other.asSegment());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Ray<PointType>::separates(const Line<OtherPoint>& other) const {
    return !isDegenerate() && !other.isDegenerate() &&
           intersects(other) && !collinear(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Ray<PointType>::separates(const OrientedLine<OtherPoint>& other) const {
    return separates(other.asLine());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Ray<PointType>::separates(const Ray<OtherPoint>& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }
    if (collinear(other)) {
        return false;
    }
    if (other.interiorContains(source())) {
        return true;
    }

    const auto other_source_side = orientationSign(source(), target(), other.source());
    const auto other_target_side = orientationSign(source(), target(), other.target());
    if (other_source_side == 0) {
        return false;
    }
    if (other_target_side != 0 && other_target_side == other_source_side) {
        return false;
    }

    const auto source_side = orientationSign(other.source(), other.target(), source());
    const auto target_side = orientationSign(other.source(), other.target(), target());
    return source_side != 0 && (target_side == 0 || target_side != source_side);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Ray<PointType>::separates(const Halfplane<OtherPoint>& other) const {
    return other.separates(*this);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Ray<PointType>::separates(const Rectangle<OtherPoint>& other) const {
    return !other.interiorContains(source()) && other.interiorsIntersect(*this);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Ray<PointType>::separates(const Triangle<OtherPoint>& other) const {
    if (isDegenerate() || other.isDegenerate() || other.interiorContains(source())) {
        return false;
    }

    const auto triangle_edges = other.edges();
    for (const auto& edge : triangle_edges) {
        if (parallel(edge) && (contains(edge.min()) || contains(edge.max()))) {
            return false;
        }
    }

    int boundary_contact_count = 0;
    if (other.boundaryContains(source())) {
        ++boundary_contact_count;
    }
    const auto triangle_vertices = other.vertices();
    for (const auto& vertex : triangle_vertices) {
        if (contains(vertex) && vertex != source()) {
            ++boundary_contact_count;
        }
    }

    for (const auto& edge : triangle_edges) {
        if (interiorsIntersect(edge)) {
            ++boundary_contact_count;
        }
    }

    return boundary_contact_count >= 2;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Ray<PointType>::separates(const Convex<OtherPoint>& other) const {
    if (other.isDegenerate()) {
        return false;
    }

    if (other.interiorContains(source())) {
        return false;
    }
    return other.interiorsIntersect(*this);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Ray<PointType>::separates(const Polygon<OtherPoint>& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }

    // Only the ray's source is a finite end that can lie strictly inside the
    // polygon and leave a slit; the far end runs to infinity, always outside,
    // so the threshold counts just the source.
    const int interior_count = other.interiorContains(source());

    const std::size_t n = other.size();
    int cross_count = 0;
    for (std::size_t i = 0; i < n; ++i) {
        const auto v = other[i];
        const auto w = other[(i + 1) % n];
        const bool cv = contains(v);
        const bool cw = contains(w);

        if (cv && !cw) {
            if (++cross_count >= 2 + interior_count) {
                return true;
            }
        }
        else if (!cv && !cw && separates(Segment<OtherPoint>(v, w))) {
            if (++cross_count >= 2 + interior_count) {
                return true;
            }
        }
    }
    return false;
}

template <class PointType>
constexpr bool Ray<PointType>::separates(const Shape<PointType>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->separates(value);
        },
        other.variant());
}

/**
 * @section predicates-rectangle Rectangle
 * Axis-aligned rectangle predicates plus the rectangle-local clipping helpers
 * used to answer strict interior and separation questions.
 */

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Rectangle<PointType>::separates(const Rectangle<OtherPoint>& other) const {
    const bool splits_horizontally =
        other.min().x() < min().x() &&
        max().x() < other.max().x() &&
        !(other.min().y() < min().y()) &&
        !(max().y() < other.max().y());
    if (splits_horizontally) {
        return true;
    }
    const bool splits_vertically =
        other.min().y() < min().y() &&
        max().y() < other.max().y() &&
        !(other.min().x() < min().x()) &&
        !(max().x() < other.max().x());
    return splits_vertically;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Rectangle<PointType>::separates(const OtherPoint&) const {
    return false;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Rectangle<PointType>::separates(const Line<OtherPoint>& other) const {
    return intersects(other) && !contains(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Rectangle<PointType>::separates(const OrientedLine<OtherPoint>& other) const {
    return separates(other.asLine());
}

template <class PointType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool Rectangle<PointType>::separates(const Segment<OtherPoint, OtherLabel>& other) const {
    // Both endpoints outside the closed rectangle and the segment touching it
    // anywhere (boundary contact included) leave a piece on each side.
    return !other.isDegenerate() && !contains(other.min()) && !contains(other.max()) && intersects(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Rectangle<PointType>::separates(const OrientedSegment<OtherPoint>& other) const {
    return separates(static_cast<Segment<OtherPoint>>(other));
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Rectangle<PointType>::separates(const Ray<OtherPoint>& other) const {
    return !other.isDegenerate() &&
           !contains(other.source()) &&
           intersects(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Rectangle<PointType>::separates(const Halfplane<OtherPoint>& other) const {
    (void)other;
    return false;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Rectangle<PointType>::separates(const Triangle<OtherPoint>& other) const {
    if (other.isDegenerate()) {
        return false;
    }

    const auto target_edges = other.edges();
    if (contains(other.a()) && separates(target_edges[1])) {
        return true;
    }
    if (contains(other.b()) && separates(target_edges[2])) {
        return true;
    }
    if (contains(other.c()) && separates(target_edges[0])) {
        return true;
    }

    int separated_edges = 0;
    for (const auto& edge : target_edges) {
        separated_edges += separates(edge) ? 1 : 0;
    }
    return separated_edges >= 2;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Rectangle<PointType>::separates(const Convex<OtherPoint>& other) const {
    return asConvex().separates(other);
}

template <class PointType>
constexpr bool Rectangle<PointType>::separates(const Shape<PointType>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->separates(value);
        },
        other.variant());
}

/**
 * @section predicates-halfplane Halfplane
 * Half-plane containment, intersection, and topological predicates, together
 * with the helper routines used for strict side/interior tests.
 */

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Halfplane<PointType>::separates(const OtherPoint&) const {
    return false;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Halfplane<PointType>::separates(const Line<OtherPoint>& other) const {
    (void)other;
    return false;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Halfplane<PointType>::separates(const OrientedLine<OtherPoint>& other) const {
    (void)other;
    return false;
}

template <class PointType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool Halfplane<PointType>::separates(const Segment<OtherPoint, OtherLabel>& other) const {
    (void)other;
    return false;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Halfplane<PointType>::separates(const OrientedSegment<OtherPoint>& other) const {
    (void)other;
    return false;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Halfplane<PointType>::separates(const Ray<OtherPoint>& other) const {
    (void)other;
    return false;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Halfplane<PointType>::separates(const Rectangle<OtherPoint>& other) const {
    (void)other;
    return false;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Halfplane<PointType>::separates(const Halfplane<OtherPoint>& other) const {
    (void)other;
    return false;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Halfplane<PointType>::separates(const Triangle<OtherPoint>& other) const {
    (void)other;
    return false;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Halfplane<PointType>::separates(const Convex<OtherPoint>& other) const {
    if (other.isDegenerate()) {
        return false;
    }
    const auto otherEdges = other.edges();
    const std::size_t m = other.size();
    int sep = 0;
    std::size_t cut_idx = 0;
    for (std::size_t i = 0; i < m; ++i) {
        if (separates(otherEdges[i])) {
            ++sep;
            cut_idx = i;
            if (sep >= 2) {
                return true;
            }
        }
    }
    if (sep == 0) {
        return false;
    }
    const std::size_t skip_a = cut_idx;
    const std::size_t skip_b = (cut_idx + 1) % m;
    for (std::size_t i = 0; i < m; ++i) {
        if (i == skip_a || i == skip_b) {
            continue;
        }
        if (contains(other[i])) {
            return true;
        }
    }
    return false;
}

template <class PointType>
constexpr bool Halfplane<PointType>::separates(const Shape<PointType>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->separates(value);
        },
        other.variant());
}


// ---------------------------------------------------------------------------
// Convex

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Convex<PointType>::separates(const OtherPoint&) const {
    return false;
}

template <class PointType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool Convex<PointType>::separates(const Segment<OtherPoint, OtherLabel>& other) const {
    // A contact anywhere on the convex polygon -- boundary touch included --
    // leaves a piece of the segment on each side when both endpoints are
    // outside, so this gates on closed intersection, not interior crossing.
    return !other.isDegenerate() && !contains(other.min()) && !contains(other.max()) && intersects(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Convex<PointType>::separates(const OrientedSegment<OtherPoint>& other) const {
    return separates(static_cast<Segment<OtherPoint>>(other));
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Convex<PointType>::separates(const Line<OtherPoint>& other) const {
    return !other.isDegenerate() && intersects(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Convex<PointType>::separates(const OrientedLine<OtherPoint>& other) const {
    return separates(static_cast<Line<OtherPoint>>(other));
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Convex<PointType>::separates(const Ray<OtherPoint>& other) const {
    return !other.isDegenerate() && !isDegenerate() &&
           !contains(other.source()) && intersects(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Convex<PointType>::separates(const Halfplane<OtherPoint>&) const {
    return false;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Convex<PointType>::separates(const Rectangle<OtherPoint>& other) const {
    return separates(other.asConvex());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Convex<PointType>::separates(const Triangle<OtherPoint>& other) const {
    return separates(other.asConvex());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Convex<PointType>::separates(const Convex<OtherPoint>& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }
    if (!bbox().intersects(other.bbox())) {
        return false;
    }

    if (other.size() <= 2*size()) {
        const ptrdiff_t m = other.size();
        int arcs = 0;
        bool prev_in = contains(other[m - 1]);
        for (ptrdiff_t i = 0; i < m; ++i) {
            const bool cur_in = contains(other[i]);
            if (!prev_in && !cur_in) {
                // Both endpoints are out, but maybe the edge went through this
                Segment<OtherPoint> edge(other.get(i-1),other[i]);
                if (intersects(edge)) {
                    ++arcs;
                    if (arcs >= 2) {
                        return true;
                    }
                }
            }
            else if (prev_in && !cur_in) {
                // Just went outside
                ++arcs;
                if (arcs >= 2) {
                    return true;
                }
            }
            
            prev_in = cur_in;
        }
    }
    else {
        const ptrdiff_t n = size();
        int arcs = 0;
        bool prev_in = other.interiorContains((*this)[n - 1]);
        for (ptrdiff_t i = 0; i < n; ++i) {
            const bool cur_in = other.interiorContains((*this)[i]);
            if (!prev_in && !cur_in) {
                // Both endpoints are out, but maybe the edge went through this
                Segment<OtherPoint> edge(get(i-1),(*this)[i]);
                if (edge.separates(other)) {
                    ++arcs;
                    if (arcs >= 2) {
                        return true;
                    }
                }
            }
            else if (prev_in && !cur_in) {
                // Just went outside
                ++arcs;
                if (arcs >= 2) {
                    return true;
                }
            }
            
            prev_in = cur_in;
        }
    }

    return false;
}

template <class PointType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool Convex<PointType>::separates(const Disk<OtherPoint, OtherLabel>& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }

    int sep_count = 0;
    Segment<PointType> separator;
    for (const auto& edge : edges()) {
        if (edge.separates(other)) {
            ++sep_count;
            if (sep_count >= 2) {
                return true;
            }
            separator = edge;
        }

    }

    if (sep_count == 1) {
        for (const auto& vertex : vertices()) {
            if (!separator.verticesContain(vertex) && other.contains(vertex)) {
                return true;
            }
        }
    }

    return false;
}

/**
 * @section predicates-disk Disk
 * A disk separates a segment when it crosses the segment's interior, leaving a surviving piece on each side.
 */

template <class PointType, class LabelType>
template <PointConcept OtherPoint, class OtherLabel>
constexpr bool Disk<PointType, LabelType>::separates(const Segment<OtherPoint, OtherLabel>& other) const {
    if (other.isDegenerate() || contains(other.min()) || contains(other.max())) {
        return false;
    }

    using R = detail::promoted_number_t<std::common_type_t<
        decltype(squaredRadius()), typename OtherPoint::NumberType>>;
    const auto center_point = center<R>();
    const R squared_radius = squaredRadius<R>();

    // v = max - min (segment direction), w = centre - min.
    const R vx = static_cast<R>(other.max().x()) - static_cast<R>(other.min().x());
    const R vy = static_cast<R>(other.max().y()) - static_cast<R>(other.min().y());
    const R wx = static_cast<R>(center_point.x()) - static_cast<R>(other.min().x());
    const R wy = static_cast<R>(center_point.y()) - static_cast<R>(other.min().y());

    const R squared_length = vx * vx + vy * vy;
    const R foot_projection = wx * vx + wy * vy;
    const R cross = vx * wy - vy * wx;

    // Both endpoints lie outside the closed disk, so the segment is cut in two
    // exactly when the perpendicular foot from the centre falls strictly inside
    // the segment and the centre lies within (or at) one radius of the
    // supporting line -- a tangent touch removes one boundary point and still
    // splits the segment in two.
    return foot_projection > R{} && foot_projection < squared_length && cross * cross <= squared_radius * squared_length;
}

template <class PointType, class LabelType>
template <PointConcept OtherPoint>
constexpr bool Disk<PointType, LabelType>::separates(const OrientedSegment<OtherPoint>& other) const {
    return separates(other.asSegment());
}

template <class PointType, class LabelType>
template <PointConcept OtherPoint>
constexpr bool Disk<PointType, LabelType>::separates(const Line<OtherPoint>& other) const {
    if (other.isDegenerate()) {
        return false;
    }

    using R = detail::promoted_number_t<std::common_type_t<
        decltype(squaredRadius()), typename OtherPoint::NumberType>>;
    const auto center_point = center<R>();
    const R squared_radius = squaredRadius<R>();

    // v = direction along the line, w = centre - point on the line.
    const R vx = static_cast<R>(other.max().x()) - static_cast<R>(other.min().x());
    const R vy = static_cast<R>(other.max().y()) - static_cast<R>(other.min().y());
    const R wx = static_cast<R>(center_point.x()) - static_cast<R>(other.min().x());
    const R wy = static_cast<R>(center_point.y()) - static_cast<R>(other.min().y());
    const R cross = vx * wy - vy * wx;

    // An infinite line is cut into two rays as soon as it reaches the closed
    // disk: a tangent line removes one boundary point and still splits in two.
    return cross * cross <= squared_radius * (vx * vx + vy * vy);
}

template <class PointType, class LabelType>
template <PointConcept OtherPoint>
constexpr bool Disk<PointType, LabelType>::separates(const OrientedLine<OtherPoint>& other) const {
    return separates(other.asLine());
}

template <class PointType, class LabelType>
template <PointConcept OtherPoint>
constexpr bool Disk<PointType, LabelType>::separates(const Convex<OtherPoint>& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }

    const ptrdiff_t m = other.size();
    int arcs = 0;
    bool prev_in = contains(other[m - 1]);
    for (ptrdiff_t i = 0; i < m; ++i) {
        const bool cur_in = contains(other[i]);
        if (!prev_in && !cur_in) {
            // Both endpoints are out, but maybe the edge went through this
            Segment<OtherPoint> edge(other.get(i-1),other[i]);
            if (intersects(edge)) {
                ++arcs;
                if (arcs >= 2) {
                    return true;
                }
            }
        }
        else if (prev_in && !cur_in) {
            // Just went outside
            ++arcs;
            if (arcs >= 2) {
                return true;
            }
        }
        
        prev_in = cur_in;
    }
    return false;
}

template <class PointType>
template <PointConcept OtherPoint>
constexpr bool Convex<PointType>::separates(const Shape<OtherPoint>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->separates(value);
        },
        other.variant());
}

/**
 * @section predicates-polygon Polygon
 * Polygon-vs-shape cut predicates.
 */

template <class PointType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool Polygon<PointType>::separates(const Segment<OtherPoint, OtherLabel>& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }

    std::optional<pgl::OrientedSegment<PointType>> minSeg, maxSeg;

    auto crossingOrder = [](const pgl::Segment<OtherPoint> &s, const pgl::OrientedSegment<PointType> &a, const pgl::OrientedSegment<PointType> &b) {
        bool gte = pgl::Triangle<PointType>(s[0],a[0],a[1]).interiorsIntersect(pgl::Triangle<PointType>(s[1],b[0],b[1]));
        bool lte = pgl::Triangle<PointType>(s[0],b[0],b[1]).interiorsIntersect(pgl::Triangle<PointType>(s[1],a[0],a[1]));
        if (gte && !lte) return std::partial_ordering::greater;
        if (lte && !gte) return std::partial_ordering::less;
        return std::partial_ordering::equivalent;
    };

    for (ptrdiff_t i = 0; i < (ptrdiff_t) size(); ++i) {
        pgl::OrientedSegment<PointType> edge(get(i), get(i+1));
        if (edge.separates(other) && !edge.collinear(other)) {
            auto h = edge.leftHalfplane();
            if (h.contains(other[0])) {
                if (!maxSeg || crossingOrder(other, *maxSeg, edge) < 0) {
                    if (other.contains(edge[0])) {
                        pgl::OrientedSegment<PointType> previous(get(i-1), get(i));
                        if (previous.collinear(other) && !h.contains(previous)) continue;
                        auto hprevious = previous.leftHalfplane();
                        if (!previous.collinear(other) && hprevious.contains(other[1]) && !hprevious.contains(edge[1])) continue;
                    }
                    else if (other.contains(edge[1])) {
                        pgl::OrientedSegment<PointType> next(get(i+1), get(i+2));
                        if (next.collinear(other) && !h.contains(next)) continue;
                        auto hnext = next.leftHalfplane();
                        if (!next.collinear(other) && hnext.contains(other[1]) && !hnext.contains(edge[0])) continue;
                    }
                    maxSeg = edge;
                    if (minSeg && maxSeg && crossingOrder(other, *minSeg, *maxSeg) <= 0) {
                        return true;
                    }
                }
            }
            else {
                if (!minSeg || crossingOrder(other, *minSeg, edge) > 0) {
                    if (other.contains(edge[0])) {
                        pgl::OrientedSegment<PointType> previous(get(i-1), get(i));
                        if (previous.collinear(other) && !h.contains(previous)) continue;
                        auto hprevious = previous.leftHalfplane();
                        if (!previous.collinear(other) && hprevious.contains(other[0]) && !hprevious.contains(edge[1])) continue;
                    }
                    else if (other.contains(edge[1])) {
                        pgl::OrientedSegment<PointType> next(get(i+1), get(i+2));
                        if (next.collinear(other) && !h.contains(next)) continue;
                        auto hnext = next.leftHalfplane();
                        if (!next.collinear(other) && hnext.contains(other[0]) && !hnext.contains(edge[0])) continue;
                    }
                    minSeg = edge;
                    if (minSeg && maxSeg && crossingOrder(other, *minSeg, *maxSeg) <= 0) {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Polygon<PointType>::separates(const Ray<OtherPoint>& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }

    pgl::OrientedLine<OtherPoint> ol(other[0],other[1]);

    for (ptrdiff_t i = 0; i < (ptrdiff_t) size(); ++i) {
        pgl::OrientedSegment<PointType> edge(get(i), get(i+1));
        if (edge.separates(other) && !edge.collinear(other)) {
            auto h = edge.leftHalfplane();
            if (!h.contains(other.source())) {
                if (other.contains(edge[0])) {
                    pgl::OrientedSegment<PointType> previous(get(i-1), get(i));
                    if (previous.collinear(other) && !h.contains(previous)) continue;
                    auto hprevious = previous.leftHalfplane();
                    if (!previous.collinear(other) && hprevious.contains(other[0]) && !hprevious.contains(edge[1])) continue;
                }
                else if (other.contains(edge[1])) {
                    pgl::OrientedSegment<PointType> next(get(i+1), get(i+2));
                    if (next.collinear(other) && !h.contains(next)) continue;
                    auto hnext = next.leftHalfplane();
                    if (!next.collinear(other) && hnext.contains(other[0]) && !hnext.contains(edge[0])) continue;
                }
                return true;
            }
        }
    }
    return false;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Polygon<PointType>::separates(const Line<OtherPoint>& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }
    return intersects(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Polygon<PointType>::separates(const OrientedSegment<OtherPoint>& other) const {
    return separates(other.asSegment());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Polygon<PointType>::separates(const OrientedLine<OtherPoint>& other) const {
    return separates(other.asLine());
}


template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedSegment<PointType>::separates(const Polygon<OtherPoint>& other) const {
    return this->asSegment().separates(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedLine<PointType>::separates(const Polygon<OtherPoint>& other) const {
    return this->asLine().separates(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Line<PointType>::separates(const Polygon<OtherPoint>& other) const {
    if (isDegenerate()) {
        return false;
    }
    return other.interiorsIntersect(*this);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Rectangle<PointType>::separates(const Polygon<OtherPoint>& other) const {
    return asConvex().separates(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Triangle<PointType>::separates(const Polygon<OtherPoint>& other) const {
    return asConvex().separates(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Halfplane<PointType>::separates(const Polygon<OtherPoint>&) const {
    throw std::runtime_error(
        "pgl: Halfplane::separates(Polygon) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Convex<PointType>::separates(const Polygon<OtherPoint>&) const {
    throw std::runtime_error(
        "pgl: Convex::separates(Polygon) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
}


// --- asymmetric not-yet-implemented stubs ---

template <class Number, class Label>
template<PointConcept OtherPoint>
constexpr bool Point<Number, Label>::separates(const OtherPoint&) const {
    throw std::runtime_error(
        "pgl: Point::separates(Point) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
}

template <class Number, class Label>
template<PointConcept OtherPoint>
constexpr bool Point<Number, Label>::separates(const Halfplane<OtherPoint>&) const {
    throw std::runtime_error(
        "pgl: Point::separates(Halfplane) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
}

template <class Number, class Label>
template<PointConcept OtherPoint>
constexpr bool Point<Number, Label>::separates(const Rectangle<OtherPoint>&) const {
    throw std::runtime_error(
        "pgl: Point::separates(Rectangle) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
}

template <class Number, class Label>
template<PointConcept OtherPoint>
constexpr bool Point<Number, Label>::separates(const Triangle<OtherPoint>&) const {
    throw std::runtime_error(
        "pgl: Point::separates(Triangle) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
}

template <class Number, class Label>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool Point<Number, Label>::separates(const Disk<OtherPoint, OtherLabel>&) const {
    throw std::runtime_error(
        "pgl: Point::separates(Disk) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
}

template <class Number, class Label>
template<PointConcept OtherPoint>
constexpr bool Point<Number, Label>::separates(const Convex<OtherPoint>&) const {
    throw std::runtime_error(
        "pgl: Point::separates(Convex) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
}

template <class Number, class Label>
template<PointConcept OtherPoint>
constexpr bool Point<Number, Label>::separates(const Polygon<OtherPoint>&) const {
    throw std::runtime_error(
        "pgl: Point::separates(Polygon) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Disk<PointType, LabelType>::separates(const OtherPoint&) const {
    throw std::runtime_error(
        "pgl: Disk::separates(Point) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Disk<PointType, LabelType>::separates(const Ray<OtherPoint>&) const {
    throw std::runtime_error(
        "pgl: Disk::separates(Ray) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Disk<PointType, LabelType>::separates(const Halfplane<OtherPoint>&) const {
    throw std::runtime_error(
        "pgl: Disk::separates(Halfplane) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Disk<PointType, LabelType>::separates(const Rectangle<OtherPoint>&) const {
    throw std::runtime_error(
        "pgl: Disk::separates(Rectangle) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Disk<PointType, LabelType>::separates(const Triangle<OtherPoint>&) const {
    throw std::runtime_error(
        "pgl: Disk::separates(Triangle) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool Disk<PointType, LabelType>::separates(const Disk<OtherPoint, OtherLabel>&) const {
    throw std::runtime_error(
        "pgl: Disk::separates(Disk) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Disk<PointType, LabelType>::separates(const Polygon<OtherPoint>&) const {
    throw std::runtime_error(
        "pgl: Disk::separates(Polygon) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Polygon<PointType>::separates(const OtherPoint&) const {
    throw std::runtime_error(
        "pgl: Polygon::separates(Point) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Polygon<PointType>::separates(const Halfplane<OtherPoint>&) const {
    throw std::runtime_error(
        "pgl: Polygon::separates(Halfplane) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Polygon<PointType>::separates(const Rectangle<OtherPoint>&) const {
    throw std::runtime_error(
        "pgl: Polygon::separates(Rectangle) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Polygon<PointType>::separates(const Triangle<OtherPoint>&) const {
    throw std::runtime_error(
        "pgl: Polygon::separates(Triangle) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
}

template <class PointType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool Polygon<PointType>::separates(const Disk<OtherPoint, OtherLabel>&) const {
    throw std::runtime_error(
        "pgl: Polygon::separates(Disk) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Polygon<PointType>::separates(const Convex<OtherPoint>&) const {
    throw std::runtime_error(
        "pgl: Polygon::separates(Convex) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Polygon<PointType>::separates(const Polygon<OtherPoint>&) const {
    throw std::runtime_error(
        "pgl: Polygon::separates(Polygon) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
}

}  // namespace pgl
