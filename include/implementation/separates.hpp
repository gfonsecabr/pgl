#pragma once

#include "pgl.hpp"

/**
 * @file separates.hpp
 * @brief Implementations of the 'separates' predicate.
 **/

#include <algorithm>
#include <compare>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <limits>
#include <utility>
#include <vector>
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
template<SegmentConcept OtherSegment>
constexpr bool Point<Number, Label>::separates(const OtherSegment& other) const {
    return other.interiorContains(*this);
}

template <class Number, class Label>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Point<Number, Label>::separates(const OtherOrientedSegment& other) const {
    return static_cast<Segment<typename OtherOrientedSegment::PointType>>(other).interiorContains(*this);
}

template <class Number, class Label>
template<LineConcept OtherLine>
constexpr bool Point<Number, Label>::separates(const OtherLine& other) const {
    return other.contains(*this);
}

template <class Number, class Label>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Point<Number, Label>::separates(const OtherOrientedLine& other) const {
    return other.asLine().contains(*this);
}

template <class Number, class Label>
template<RayConcept OtherRay>
constexpr bool Point<Number, Label>::separates(const OtherRay& other) const {
    return other.interiorContains(*this);
}

template <class Number, class Label>
template<PointConcept OtherPoint>
constexpr bool Point<Number, Label>::separates(const OtherPoint&) const {
    return false;
}

template <class Number, class Label>
template<HalfplaneConcept OtherHalfplane>
constexpr bool Point<Number, Label>::separates(const OtherHalfplane&) const {
    return false;
}

template <class Number, class Label>
template<DiskConcept OtherDisk>
constexpr bool Point<Number, Label>::separates(const OtherDisk&) const {
    return false;
}

// A point can only separate a 2D region when that region is degenerate to a
// segment; its convex hull then has size 2, whose whole segment is the
// region's boundary, so the point separates it iff that segment contains it.
template <class Number, class Label>
template<ConvexConcept OtherConvex>
constexpr bool Point<Number, Label>::separates(const OtherConvex& other) const {
    return other.size() == 2 &&
           Segment<typename OtherConvex::PointType>(other[0], other[1]).contains(*this);
}

template <class Number, class Label>
template<RectangleConcept OtherRectangle>
constexpr bool Point<Number, Label>::separates(const OtherRectangle& other) const {
    return separates(static_cast<Convex<typename OtherRectangle::PointType>>(other));
}

template <class Number, class Label>
template<TriangleConcept OtherTriangle>
constexpr bool Point<Number, Label>::separates(const OtherTriangle& other) const {
    return separates(static_cast<Convex<typename OtherTriangle::PointType>>(other));
}

template <class Number, class Label>
template<PolygonConcept OtherPolygon>
constexpr bool Point<Number, Label>::separates(const OtherPolygon& other) const {
    return separates(Convex<typename OtherPolygon::PointType>(other.vertices()));
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
template<SegmentConcept OtherSegment>
constexpr bool Segment<PointType, LabelType>::separates(const OtherSegment& other) const {
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
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Segment<PointType, LabelType>::separates(const OtherOrientedSegment& other) const {
    return separates(other.asSegment());
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Segment<PointType, LabelType>::separates(const OtherLine& other) const {
    return intersects(other);
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Segment<PointType, LabelType>::separates(const OtherOrientedLine& other) const {
    return intersects(other);
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool Segment<PointType, LabelType>::separates(const OtherRay& other) const {
    // The segment splits the ray only when it meets the ray ahead of the
    // source: a piece then survives between the source and the segment, and
    // another runs to infinity. If the source lies on the segment, the near
    // piece is empty and the ray stays connected.
    return intersects(other) && !contains(other.source());
}

template <class PointType, class LabelType>
template<RectangleConcept OtherRectangle>
constexpr bool Segment<PointType, LabelType>::separates(const OtherRectangle& other) const {
    return other.interiorsIntersect(*this) && !other.interiorContains(min()) && !other.interiorContains(max());
}

template <class PointType, class LabelType>
template<HalfplaneConcept OtherHalfplane>
constexpr bool Segment<PointType, LabelType>::separates(const OtherHalfplane& other) const {
    return other.separates(*this);
}

template <class PointType, class LabelType>
template<TriangleConcept OtherTriangle>
constexpr bool Segment<PointType, LabelType>::separates(const OtherTriangle& other) const {
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
template<ConvexConcept OtherConvex>
constexpr bool Segment<PointType, LabelType>::separates(const OtherConvex& other) const {
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
template<PolygonConcept OtherPolygon>
constexpr bool Segment<PointType, LabelType>::separates(const OtherPolygon& other) const {
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
        else if (!cv && !cw && separates(Segment<typename OtherPolygon::PointType>(v, w))) {
            if (++cross_count >= 2 + interior_count) {
                return true;
            }
        }
    }

    return false;
}

template <class PointType, class LabelType>
template<DiskConcept OtherDisk>
constexpr bool Segment<PointType, LabelType>::separates(const OtherDisk& other) const {
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

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Triangle<PointType, LabelType>::separates(const OtherSegment& other) const {
    return !other.isDegenerate() && !contains(other.min()) && !contains(other.max()) && intersects(other);
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Triangle<PointType, LabelType>::separates(const OtherPoint&) const {
    return false;
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Triangle<PointType, LabelType>::separates(const OtherOrientedSegment& other) const {
    return separates(static_cast<Segment<typename OtherOrientedSegment::PointType>>(other));
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Triangle<PointType, LabelType>::separates(const OtherLine& other) const {
    return !other.isDegenerate() && intersects(other);
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Triangle<PointType, LabelType>::separates(const OtherOrientedLine& other) const {
    return separates(other.asLine());
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool Triangle<PointType, LabelType>::separates(const OtherRay& other) const {
    // Removing the triangle splits the ray iff the ray reaches it with its
    // source outside: the far end runs to infinity (always outside), so any
    // contact -- including a tangential touch of the boundary -- leaves a piece
    // on each side.
    return !other.isDegenerate() && !isDegenerate()
           && !contains(other.source()) && intersects(other);
}

template <class PointType, class LabelType>
template<HalfplaneConcept OtherHalfplane>
constexpr bool Triangle<PointType, LabelType>::separates(const OtherHalfplane&) const {
    return false;
}

template <class PointType, class LabelType>
template<RectangleConcept OtherRectangle>
constexpr bool Triangle<PointType, LabelType>::separates(const OtherRectangle& other) const {
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

template <class PointType, class LabelType>
template<TriangleConcept OtherTriangle>
constexpr bool Triangle<PointType, LabelType>::separates(const OtherTriangle& other) const {
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

template <class PointType, class LabelType>
template<ConvexConcept OtherConvex>
constexpr bool Triangle<PointType, LabelType>::separates(const OtherConvex& other) const {
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

template <class PointType, class LabelType>
constexpr bool Triangle<PointType, LabelType>::separates(const Shape<PointType>& other) const {
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

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool OrientedSegment<PointType, LabelType>::separates(const OtherPoint& other) const {
    return this->asSegment().separates(other);
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool OrientedSegment<PointType, LabelType>::separates(const OtherSegment& other) const {
    return this->asSegment().separates(other);
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool OrientedSegment<PointType, LabelType>::separates(const OtherOrientedSegment& other) const {
    return this->asSegment().separates(other.asSegment());
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool OrientedSegment<PointType, LabelType>::separates(const OtherLine& other) const {
    return this->asSegment().separates(other);
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool OrientedSegment<PointType, LabelType>::separates(const OtherOrientedLine& other) const {
    return this->asSegment().separates(other);
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool OrientedSegment<PointType, LabelType>::separates(const OtherRay& other) const {
    return this->asSegment().separates(other);
}

template <class PointType, class LabelType>
template<RectangleConcept OtherRectangle>
constexpr bool OrientedSegment<PointType, LabelType>::separates(const OtherRectangle& other) const {
    return this->asSegment().separates(other);
}

template <class PointType, class LabelType>
template<TriangleConcept OtherTriangle>
constexpr bool OrientedSegment<PointType, LabelType>::separates(const OtherTriangle& other) const {
    return this->asSegment().separates(other);
}

template <class PointType, class LabelType>
template<HalfplaneConcept OtherHalfplane>
constexpr bool OrientedSegment<PointType, LabelType>::separates(const OtherHalfplane& other) const {
    return this->asSegment().separates(other);
}

template <class PointType, class LabelType>
template<ConvexConcept OtherConvex>
constexpr bool OrientedSegment<PointType, LabelType>::separates(const OtherConvex& other) const {
    return this->asSegment().separates(other);
}

template <class PointType, class LabelType>
constexpr bool OrientedSegment<PointType, LabelType>::separates(const Shape<PointType>& other) const {
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

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Line<PointType, LabelType>::separates(const OtherPoint&) const {
    return false;
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Line<PointType, LabelType>::separates(const OtherSegment& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }
    const auto first_side = orientationSign(min(), max(), other.min());
    const auto second_side = orientationSign(min(), max(), other.max());
    return first_side != 0 && second_side != 0 && first_side != second_side;
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Line<PointType, LabelType>::separates(const OtherOrientedSegment& other) const {
    return separates(other.asSegment());
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Line<PointType, LabelType>::separates(const OtherLine& other) const {
    return !isDegenerate() && !other.isDegenerate() &&
           intersects(other) && !collinear(other);
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Line<PointType, LabelType>::separates(const OtherOrientedLine& other) const {
    return separates(other.asLine());
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool Line<PointType, LabelType>::separates(const OtherRay& other) const {
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

template <class PointType, class LabelType>
template<RectangleConcept OtherRectangle>
constexpr bool Line<PointType, LabelType>::separates(const OtherRectangle& other) const {
    return other.interiorsIntersect(*this);
}

template <class PointType, class LabelType>
template<TriangleConcept OtherTriangle>
constexpr bool Line<PointType, LabelType>::separates(const OtherTriangle& other) const {
    if (isDegenerate()) {
        return false;
    }
    return other.interiorsIntersect(*this);
}

template <class PointType, class LabelType>
template<HalfplaneConcept OtherHalfplane>
constexpr bool Line<PointType, LabelType>::separates(const OtherHalfplane& other) const {
    return other.separates(*this);
}

template <class PointType, class LabelType>
template<ConvexConcept OtherConvex>
constexpr bool Line<PointType, LabelType>::separates(const OtherConvex& other) const {
    if (isDegenerate()) {
        return false;
    }
    return other.interiorsIntersect(*this);
}

template <class PointType, class LabelType>
constexpr bool Line<PointType, LabelType>::separates(const Shape<PointType>& other) const {
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

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool OrientedLine<PointType, LabelType>::separates(const OtherPoint& other) const {
    return this->asLine().separates(other);
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool OrientedLine<PointType, LabelType>::separates(const OtherSegment& other) const {
    return this->asLine().separates(other);
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool OrientedLine<PointType, LabelType>::separates(const OtherOrientedSegment& other) const {
    return this->asLine().separates(other);
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool OrientedLine<PointType, LabelType>::separates(const OtherLine& other) const {
    return this->asLine().separates(other);
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool OrientedLine<PointType, LabelType>::separates(const OtherOrientedLine& other) const {
    return this->asLine().separates(other);
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool OrientedLine<PointType, LabelType>::separates(const OtherRay& other) const {
    return this->asLine().separates(other);
}

template <class PointType, class LabelType>
template<RectangleConcept OtherRectangle>
constexpr bool OrientedLine<PointType, LabelType>::separates(const OtherRectangle& other) const {
    return this->asLine().separates(other);
}

template <class PointType, class LabelType>
template<TriangleConcept OtherTriangle>
constexpr bool OrientedLine<PointType, LabelType>::separates(const OtherTriangle& other) const {
    return this->asLine().separates(other);
}

template <class PointType, class LabelType>
template<HalfplaneConcept OtherHalfplane>
constexpr bool OrientedLine<PointType, LabelType>::separates(const OtherHalfplane& other) const {
    return this->asLine().separates(other);
}

template <class PointType, class LabelType>
template<ConvexConcept OtherConvex>
constexpr bool OrientedLine<PointType, LabelType>::separates(const OtherConvex& other) const {
    return this->asLine().separates(other);
}

template <class PointType, class LabelType>
constexpr bool OrientedLine<PointType, LabelType>::separates(const Shape<PointType>& other) const {
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

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Ray<PointType, LabelType>::separates(const OtherPoint&) const {
    return false;
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Ray<PointType, LabelType>::separates(const OtherSegment& other) const {
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

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Ray<PointType, LabelType>::separates(const OtherOrientedSegment& other) const {
    return separates(other.asSegment());
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Ray<PointType, LabelType>::separates(const OtherLine& other) const {
    return !isDegenerate() && !other.isDegenerate() &&
           intersects(other) && !collinear(other);
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Ray<PointType, LabelType>::separates(const OtherOrientedLine& other) const {
    return separates(other.asLine());
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool Ray<PointType, LabelType>::separates(const OtherRay& other) const {
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

template <class PointType, class LabelType>
template<HalfplaneConcept OtherHalfplane>
constexpr bool Ray<PointType, LabelType>::separates(const OtherHalfplane& other) const {
    return other.separates(*this);
}

template <class PointType, class LabelType>
template<RectangleConcept OtherRectangle>
constexpr bool Ray<PointType, LabelType>::separates(const OtherRectangle& other) const {
    return !other.interiorContains(source()) && other.interiorsIntersect(*this);
}

template <class PointType, class LabelType>
template<TriangleConcept OtherTriangle>
constexpr bool Ray<PointType, LabelType>::separates(const OtherTriangle& other) const {
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

template <class PointType, class LabelType>
template<ConvexConcept OtherConvex>
constexpr bool Ray<PointType, LabelType>::separates(const OtherConvex& other) const {
    if (other.isDegenerate()) {
        return false;
    }

    if (other.interiorContains(source())) {
        return false;
    }
    return other.interiorsIntersect(*this);
}

template <class PointType, class LabelType>
template<PolygonConcept OtherPolygon>
constexpr bool Ray<PointType, LabelType>::separates(const OtherPolygon& other) const {
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
        else if (!cv && !cw && separates(Segment<typename OtherPolygon::PointType>(v, w))) {
            if (++cross_count >= 2 + interior_count) {
                return true;
            }
        }
    }
    return false;
}

template <class PointType, class LabelType>
constexpr bool Ray<PointType, LabelType>::separates(const Shape<PointType>& other) const {
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

template <class PointType, class LabelType>
template<RectangleConcept OtherRectangle>
constexpr bool Rectangle<PointType, LabelType>::separates(const OtherRectangle& other) const {
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

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Rectangle<PointType, LabelType>::separates(const OtherPoint&) const {
    return false;
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Rectangle<PointType, LabelType>::separates(const OtherLine& other) const {
    return intersects(other) && !contains(other);
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Rectangle<PointType, LabelType>::separates(const OtherOrientedLine& other) const {
    return separates(other.asLine());
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Rectangle<PointType, LabelType>::separates(const OtherSegment& other) const {
    // Both endpoints outside the closed rectangle and the segment touching it
    // anywhere (boundary contact included) leave a piece on each side.
    return !other.isDegenerate() && !contains(other.min()) && !contains(other.max()) && intersects(other);
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Rectangle<PointType, LabelType>::separates(const OtherOrientedSegment& other) const {
    return separates(static_cast<Segment<typename OtherOrientedSegment::PointType>>(other));
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool Rectangle<PointType, LabelType>::separates(const OtherRay& other) const {
    return !other.isDegenerate() &&
           !contains(other.source()) &&
           intersects(other);
}

template <class PointType, class LabelType>
template<HalfplaneConcept OtherHalfplane>
constexpr bool Rectangle<PointType, LabelType>::separates(const OtherHalfplane& other) const {
    (void)other;
    return false;
}

template <class PointType, class LabelType>
template<TriangleConcept OtherTriangle>
constexpr bool Rectangle<PointType, LabelType>::separates(const OtherTriangle& other) const {
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

template <class PointType, class LabelType>
template<ConvexConcept OtherConvex>
constexpr bool Rectangle<PointType, LabelType>::separates(const OtherConvex& other) const {
    return asConvex().separates(other);
}

template <class PointType, class LabelType>
constexpr bool Rectangle<PointType, LabelType>::separates(const Shape<PointType>& other) const {
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

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Halfplane<PointType, LabelType>::separates(const OtherPoint&) const {
    return false;
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Halfplane<PointType, LabelType>::separates(const OtherLine& other) const {
    (void)other;
    return false;
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Halfplane<PointType, LabelType>::separates(const OtherOrientedLine& other) const {
    (void)other;
    return false;
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Halfplane<PointType, LabelType>::separates(const OtherSegment& other) const {
    (void)other;
    return false;
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Halfplane<PointType, LabelType>::separates(const OtherOrientedSegment& other) const {
    (void)other;
    return false;
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool Halfplane<PointType, LabelType>::separates(const OtherRay& other) const {
    (void)other;
    return false;
}

template <class PointType, class LabelType>
template<RectangleConcept OtherRectangle>
constexpr bool Halfplane<PointType, LabelType>::separates(const OtherRectangle& other) const {
    (void)other;
    return false;
}

template <class PointType, class LabelType>
template<HalfplaneConcept OtherHalfplane>
constexpr bool Halfplane<PointType, LabelType>::separates(const OtherHalfplane& other) const {
    (void)other;
    return false;
}

template <class PointType, class LabelType>
template<TriangleConcept OtherTriangle>
constexpr bool Halfplane<PointType, LabelType>::separates(const OtherTriangle& other) const {
    (void)other;
    return false;
}

template <class PointType, class LabelType>
template<ConvexConcept OtherConvex>
constexpr bool Halfplane<PointType, LabelType>::separates(const OtherConvex& other) const {
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

template <class PointType, class LabelType>
template<PolygonConcept OtherPolygon>
constexpr bool Halfplane<PointType, LabelType>::separates(const OtherPolygon& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }

    if (other.size() < 3) {
        return false;
    }

    const ptrdiff_t m = other.size();

    ptrdiff_t start = 0;
    for (; start < m && !contains(other[start]) ; ++start) {
    }

    if (start == m) {
        return false; // No vertex in the halfplane
    }
    // Now we know that other[start] is in the halfplane

    int arcs = 0;
    pgl::OrientedLine<PointType> boundary = asOrientedLine();
    pgl::Line<typename OtherPolygon::PointType> leaving;
    start++; // Now other[start-1] is in the halfplane
    bool prev_in = true;

    for (ptrdiff_t i = start; i < start+m; ++i) {
        const bool cur_in = contains(other.get(i));
        if (prev_in && !cur_in) {
            // Just went outside
            leaving = pgl::Line<typename OtherPolygon::PointType>(other.get(i), other.get(i-1));
        }
        if (cur_in && !prev_in) {
            // Just came inside, must check order
            pgl::Line<typename OtherPolygon::PointType> entering(other.get(i), other.get(i-1));
            if (boundary.crossingOrder(entering, leaving) >= 0) {
                ++arcs;
                if (arcs >= 2) {
                    return true;
                }
            }
        }

        prev_in = cur_in;
    }
    return false;

}

template <class PointType, class LabelType>
constexpr bool Halfplane<PointType, LabelType>::separates(const Shape<PointType>& other) const {
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
template<SegmentConcept OtherSegment>
constexpr bool Convex<PointType>::separates(const OtherSegment& other) const {
    // A contact anywhere on the convex polygon -- boundary touch included --
    // leaves a piece of the segment on each side when both endpoints are
    // outside, so this gates on closed intersection, not interior crossing.
    return !other.isDegenerate() && !contains(other.min()) && !contains(other.max()) && intersects(other);
}

template <class PointType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Convex<PointType>::separates(const OtherOrientedSegment& other) const {
    return separates(static_cast<Segment<typename OtherOrientedSegment::PointType>>(other));
}

template <class PointType>
template<LineConcept OtherLine>
constexpr bool Convex<PointType>::separates(const OtherLine& other) const {
    return !other.isDegenerate() && intersects(other);
}

template <class PointType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Convex<PointType>::separates(const OtherOrientedLine& other) const {
    return separates(static_cast<Line<typename OtherOrientedLine::PointType>>(other));
}

template <class PointType>
template<RayConcept OtherRay>
constexpr bool Convex<PointType>::separates(const OtherRay& other) const {
    return !other.isDegenerate() && !isDegenerate() &&
           !contains(other.source()) && intersects(other);
}

template <class PointType>
template<HalfplaneConcept OtherHalfplane>
constexpr bool Convex<PointType>::separates(const OtherHalfplane&) const {
    return false;
}

template <class PointType>
template<RectangleConcept OtherRectangle>
constexpr bool Convex<PointType>::separates(const OtherRectangle& other) const {
    return separates(other.asConvex());
}

template <class PointType>
template<TriangleConcept OtherTriangle>
constexpr bool Convex<PointType>::separates(const OtherTriangle& other) const {
    return separates(other.asConvex());
}

template <class PointType>
template<ConvexConcept OtherConvex>
constexpr bool Convex<PointType>::separates(const OtherConvex& other) const {
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
                Segment<typename OtherConvex::PointType> edge(other.get(i-1),other[i]);
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
                Segment<typename OtherConvex::PointType> edge(get(i-1),(*this)[i]);
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
template<DiskConcept OtherDisk>
constexpr bool Convex<PointType>::separates(const OtherDisk& other) const {
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
template<PointConcept OtherPoint>
constexpr bool Disk<PointType, LabelType>::separates(const OtherPoint&) const {
    return false;
}

template <class PointType, class LabelType>
template <SegmentConcept OtherSegment>
constexpr bool Disk<PointType, LabelType>::separates(const OtherSegment& other) const {
    if (other.isDegenerate() || contains(other.min()) || contains(other.max())) {
        return false;
    }

    using R = detail::promoted_number_t<std::common_type_t<
        decltype(squaredRadius()), typename OtherSegment::NumberType>>;
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
template <OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Disk<PointType, LabelType>::separates(const OtherOrientedSegment& other) const {
    return separates(other.asSegment());
}

template <class PointType, class LabelType>
template <LineConcept OtherLine>
constexpr bool Disk<PointType, LabelType>::separates(const OtherLine& other) const {
    if (other.isDegenerate()) {
        return false;
    }

    using R = detail::promoted_number_t<std::common_type_t<
        decltype(squaredRadius()), typename OtherLine::NumberType>>;
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
template <OrientedLineConcept OtherOrientedLine>
constexpr bool Disk<PointType, LabelType>::separates(const OtherOrientedLine& other) const {
    return separates(other.asLine());
}

template <class PointType, class LabelType>
template <ConvexConcept OtherConvex>
constexpr bool Disk<PointType, LabelType>::separates(const OtherConvex& other) const {
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
            Segment<typename OtherConvex::PointType> edge(other.get(i-1),other[i]);
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

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool Disk<PointType, LabelType>::separates(const OtherRay& other) const {
    return !contains(other.source()) && intersects(other);
}

template <class PointType, class LabelType>
template<HalfplaneConcept OtherHalfplane>
constexpr bool Disk<PointType, LabelType>::separates(const OtherHalfplane&) const {
    return false;  // A disk never separates a halfplane
}

template <class PointType, class LabelType>
template<RectangleConcept OtherRectangle>
constexpr bool Disk<PointType, LabelType>::separates(const OtherRectangle& other) const {
    int count = 0;
    for (int i = 0; i < 3; i++) {
        pgl::Segment<typename OtherRectangle::PointType> edge(other.get(i),other.get(i+1));
        if (separates(edge)) {
            count++;
            typename OtherRectangle::PointType opposite1 = other.get(i+2);
            typename OtherRectangle::PointType opposite2 = other.get(i+3);
            if (contains(opposite1) || contains(opposite2)) {
                return true;
            }
        }
    }
    return count >= 2;
}

template <class PointType, class LabelType>
template<TriangleConcept OtherTriangle>
constexpr bool Disk<PointType, LabelType>::separates(const OtherTriangle& other) const {
    int count = 0;
    for (int i = 0; i < 3; i++) {
        pgl::Segment<typename OtherTriangle::PointType> edge(other.get(i),other.get(i+1));
        if (separates(edge)) {
            count++;
            typename OtherTriangle::PointType opposite = other.get(i+2);
            if (contains(opposite)) {
                return true;
            }
        }
    }
    return count >= 2;
}

template <class PointType, class LabelType>
template<DiskConcept OtherDisk>
constexpr bool Disk<PointType, LabelType>::separates(const OtherDisk&) const {
    return false;  // a disk never separates another disk
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
template<PointConcept OtherPoint>
constexpr bool Polygon<PointType>::separates(const OtherPoint&) const {
    return false;
}

template <class PointType>
template<SegmentConcept OtherSegment>
constexpr bool Polygon<PointType>::separates(const OtherSegment& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }

    std::optional<pgl::OrientedSegment<PointType>> minSeg, maxSeg;

    auto crossingOrder = [](const pgl::Segment<typename OtherSegment::PointType> &s, const pgl::OrientedSegment<PointType> &a, const pgl::OrientedSegment<PointType> &b) {
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
template<RayConcept OtherRay>
constexpr bool Polygon<PointType>::separates(const OtherRay& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }

    pgl::OrientedLine<typename OtherRay::PointType> ol(other[0],other[1]);

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
template<LineConcept OtherLine>
constexpr bool Polygon<PointType>::separates(const OtherLine& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }
    return intersects(other);
}

template <class PointType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Polygon<PointType>::separates(const OtherOrientedSegment& other) const {
    return separates(other.asSegment());
}

template <class PointType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Polygon<PointType>::separates(const OtherOrientedLine& other) const {
    return separates(other.asLine());
}

template <class PointType>
template<HalfplaneConcept OtherHalfplane>
constexpr bool Polygon<PointType>::separates(const OtherHalfplane&) const {
    return false;  // A polygon never separates a halfplane
}


template <class PointType, class LabelType>
template<PolygonConcept OtherPolygon>
constexpr bool OrientedSegment<PointType, LabelType>::separates(const OtherPolygon& other) const {
    return this->asSegment().separates(other);
}

template <class PointType, class LabelType>
template<PolygonConcept OtherPolygon>
constexpr bool OrientedLine<PointType, LabelType>::separates(const OtherPolygon& other) const {
    return this->asLine().separates(other);
}

template <class PointType, class LabelType>
template<PolygonConcept OtherPolygon>
constexpr bool Line<PointType, LabelType>::separates(const OtherPolygon& other) const {
    if (isDegenerate()) {
        return false;
    }
    return other.interiorsIntersect(*this);
}

template <class PointType, class LabelType>
template<PolygonConcept OtherPolygon>
constexpr bool Rectangle<PointType, LabelType>::separates(const OtherPolygon& other) const {
    return asConvex().separates(other);
}

template <class PointType, class LabelType>
template<PolygonConcept OtherPolygon>
constexpr bool Triangle<PointType, LabelType>::separates(const OtherPolygon& other) const {
    return asConvex().separates(other);
}

template <class PointType>
template<PolygonConcept OtherPolygon>
constexpr bool Convex<PointType>::separates(const OtherPolygon& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }
    if (!bbox().intersects(other.bbox())) {
        return false;
    }

    // Removing the convex body C from the polygon P disconnects P iff some
    // connected component of C ∩ P touches ∂P in two or more pieces. Counting
    // boundary arcs (as the Convex overload does) is not enough here: a reflex
    // polygon can dip into C through several separate pockets while P \ C
    // stays connected. Instead, walk around ∂C: P is cut exactly when some
    // maximal arc of ∂C through the open interior of P joins two contacts
    // belonging to *different* components of ∂P ∩ C. This also catches a
    // convex body inside P that pinches ∂P at two isolated touch points.
    //
    // Events are the contacts of ∂P with ∂C, each labelled with the component
    // of ∂P ∩ C it belongs to and located on its convex edge by a line that
    // crosses the edge at the contact, so no intersection point is ever
    // constructed. Each event also records the local feature of ∂P at the
    // contact (the crossing edge, or the wedge at a polygon vertex), so that
    // whether the ∂C arc leaving a contact enters the interior of P is decided
    // by orientation signs at that contact alone. Stretches where the two
    // boundaries overlap are bounded by events of the same component and can
    // therefore never fire the test.

    using CommonNumber = std::common_type_t<typename PointType::NumberType,
                                            typename OtherPolygon::NumberType>;
    using CommonPoint = Point<CommonNumber>;
    using PosLine = Line<CommonPoint>;

    const std::ptrdiff_t n = static_cast<std::ptrdiff_t>(other.size());
    const std::ptrdiff_t m = static_cast<std::ptrdiff_t>(size());

    const auto common = [](const auto& p) {
        return CommonPoint(static_cast<CommonNumber>(p.x()),
                           static_cast<CommonNumber>(p.y()));
    };

    // A line through `at` perpendicular to convex edge k; it crosses that
    // edge exactly at `at`, placing a contact point that is known explicitly.
    const auto perpendicularAt = [&](const CommonPoint& at, std::ptrdiff_t k) {
        const CommonPoint a = common(get(k));
        const CommonPoint b = common(get(k + 1));
        return PosLine(at, CommonPoint(at.x() - (b.y() - a.y()),
                                       at.y() + (b.x() - a.x())));
    };

    struct Event {
        std::ptrdiff_t cedge;  // convex edge carrying the contact
        PosLine where;         // crosses that edge at the contact point
        int component;         // component of ∂P ∩ C the contact belongs to
        CommonPoint a, b, c;   // ∂P at the contact: edge a->b, or wedge a->b->c
        bool atVertex;         // contact is the polygon vertex b
    };
    std::vector<Event> events;

    // The first (mode first) or last contact of polygon edge pa->pb with ∂C.
    // Contacts are ordered along pa->pb by where the convex edge's line
    // crosses it; that line is parallel to pa->pb only for a collinear
    // overlap, whose near and far ends are explicit endpoints. A contact
    // landing exactly on the run's vertex inside C is dropped when requested:
    // the vertex's own wedge events describe that contact better.
    const auto extremeContact = [&](const typename OtherPolygon::PointType& pa, const typename OtherPolygon::PointType& pb,
                                    bool first, bool skipAtRunVertex)
        -> std::optional<std::pair<std::ptrdiff_t, PosLine>> {
        const CommonPoint a = common(pa);
        const CommonPoint b = common(pb);
        const Segment<CommonPoint> s(a, b);
        const OrientedLine<CommonPoint> axis(a, b);
        std::optional<std::pair<std::ptrdiff_t, PosLine>> best;
        std::optional<PosLine> bestAlong;
        const auto consider = [&](std::ptrdiff_t k, const PosLine& where,
                                  const PosLine& along) {
            if (bestAlong) {
                const auto order = axis.crossingOrder(along, *bestAlong);
                const bool improves =
                    first ? order == std::partial_ordering::less
                          : order == std::partial_ordering::greater;
                if (!improves) {
                    return;
                }
            }
            best.emplace(k, where);
            bestAlong = along;
        };
        for (std::ptrdiff_t k = 0; k < m; ++k) {
            const Segment<CommonPoint> edge(common(get(k)), common(get(k + 1)));
            if (!edge.intersects(s)) {
                continue;
            }
            if (s.parallel(edge)) {
                const CommonPoint lo = std::max(s.min(), edge.min());
                const CommonPoint hi = std::min(s.max(), edge.max());
                const CommonPoint at = (first == (a < b)) ? lo : hi;
                const PosLine across = perpendicularAt(at, k);
                consider(k, across, across);
            } else {
                consider(k, PosLine(a, b),
                         PosLine(common(get(k)), common(get(k + 1))));
            }
        }
        if (best && skipAtRunVertex) {
            const CommonPoint inVertex = first ? b : a;
            const PosLine atInVertex(
                inVertex, CommonPoint(inVertex.x() - (b.y() - a.y()),
                                      inVertex.y() + (b.x() - a.x())));
            if (axis.crossingOrder(*bestAlong, atInVertex) ==
                std::partial_ordering::equivalent) {
                return std::nullopt;
            }
        }
        return best;
    };

    std::vector<char> in(static_cast<std::size_t>(n));
    std::ptrdiff_t start = -1;
    for (std::ptrdiff_t i = 0; i < n; ++i) {
        in[static_cast<std::size_t>(i)] = contains(other.get(i));
        if (!in[static_cast<std::size_t>(i)]) {
            start = i;
        }
    }
    if (start < 0) {
        return false;  // the whole polygon lies inside the convex body
    }

    int component = 0;

    // A polygon vertex inside C that lies on ∂C is a contact in its own
    // right; the pair of wedge events splits the ∂C arcs it sits between
    // (the two-touch pinch case) without affecting any other arc.
    const auto addVertexContact = [&](std::ptrdiff_t i) {
        const typename OtherPolygon::PointType vertex = other.get(i);
        if (!boundaryContains(vertex)) {
            return;
        }
        const CommonPoint v = common(vertex);
        for (std::ptrdiff_t k = 0; k < m; ++k) {
            const Segment<CommonPoint> edge(common(get(k)), common(get(k + 1)));
            if (edge.contains(v)) {
                const PosLine across = perpendicularAt(v, k);
                const Event event{k, across, component,
                                  common(other.get(i - 1)), v,
                                  common(other.get(i + 1)), true};
                events.push_back(event);
                events.push_back(event);
                return;
            }
        }
    };

    bool previous = false;  // in[start] is false
    for (std::ptrdiff_t i = start + 1; i <= start + n; ++i) {
        const bool current = in[static_cast<std::size_t>(i % n)];
        const auto u = other.get(i - 1);
        const auto v = other.get(i);
        if (current && !previous) {
            ++component;
            if (const auto contact = extremeContact(u, v, true, true)) {
                events.push_back({contact->first, contact->second, component,
                                  common(u), common(v), common(v), false});
            }
        } else if (!current && previous) {
            if (const auto contact = extremeContact(u, v, false, true)) {
                events.push_back({contact->first, contact->second, component,
                                  common(u), common(v), common(v), false});
            }
        } else if (!current && intersects(Segment<typename OtherPolygon::PointType>(u, v))) {
            // Both endpoints outside: the edge meets the convex body in a
            // single sub-segment or touch point, a component of its own.
            ++component;
            if (const auto contact = extremeContact(u, v, true, false)) {
                events.push_back({contact->first, contact->second, component,
                                  common(u), common(v), common(v), false});
            }
            if (const auto contact = extremeContact(u, v, false, false)) {
                events.push_back({contact->first, contact->second, component,
                                  common(u), common(v), common(v), false});
            }
        }
        if (current) {
            addVertexContact(i);
        }
        previous = current;
    }

    if (events.empty()) {
        return false;  // the boundaries never meet: nested or disjoint
    }

    std::sort(events.begin(), events.end(),
              [&](const Event& x, const Event& y) {
        if (x.cedge != y.cedge) {
            return x.cedge < y.cedge;
        }
        const OrientedLine<CommonPoint> edge(common(get(x.cedge)),
                                             common(get(x.cedge + 1)));
        return edge.crossingOrder(x.where, y.where) ==
               std::partial_ordering::less;
    });

    // Whether ∂C heads strictly into the interior of P as it leaves the
    // event's contact point, decided against the recorded ∂P feature there.
    const auto entersInterior = [&](const Event& event) {
        const std::ptrdiff_t k = event.cedge;
        const OrientedLine<CommonPoint> edgeLine(common(get(k)),
                                                 common(get(k + 1)));
        const bool atEdgeEnd =
            edgeLine.crossingOrder(event.where,
                                   perpendicularAt(common(get(k + 1)), k)) ==
            std::partial_ordering::equivalent;
        const CommonPoint from = common(get(k + (atEdgeEnd ? 1 : 0)));
        const CommonPoint to = common(get(k + (atEdgeEnd ? 2 : 1)));
        const CommonNumber dx = to.x() - from.x();
        const CommonNumber dy = to.y() - from.y();
        if (!event.atVertex) {
            // Contact interior to polygon edge a->b: P's interior is strictly
            // to its left, so the arc dives inside iff it heads left.
            return orientationSign(event.a, event.b,
                                   CommonPoint(event.a.x() + dx,
                                               event.a.y() + dy)) > 0;
        }
        // Contact at polygon vertex b: the direction must point strictly into
        // the interior wedge between the edges a->b and b->c.
        const CommonPoint probe(event.b.x() + dx, event.b.y() + dy);
        const auto sideIn = orientationSign(event.a, event.b, probe);
        const auto sideOut = orientationSign(event.b, event.c, probe);
        return orientationSign(event.a, event.b, event.c) >= 0
                   ? (sideIn > 0 && sideOut > 0)
                   : (sideIn > 0 || sideOut > 0);
    };

    const std::size_t count = events.size();
    for (std::size_t t = 0; t < count; ++t) {
        const Event& current = events[t];
        const Event& next = events[(t + 1) % count];
        if (current.component != next.component && entersInterior(current)) {
            return true;
        }
    }
    return false;
}

template <class PointType, class LabelType>
template<PolygonConcept OtherPolygon>
constexpr bool Disk<PointType, LabelType>::separates(const OtherPolygon&) const {
    throw std::runtime_error(
        "pgl: Disk::separates(Polygon) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
}

template <class PointType>
template<RectangleConcept OtherRectangle>
constexpr bool Polygon<PointType>::separates(const OtherRectangle&) const {
    throw std::runtime_error(
        "pgl: Polygon::separates(Rectangle) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
}

template <class PointType>
template<TriangleConcept OtherTriangle>
constexpr bool Polygon<PointType>::separates(const OtherTriangle&) const {
    throw std::runtime_error(
        "pgl: Polygon::separates(Triangle) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
}

template <class PointType>
template<DiskConcept OtherDisk>
constexpr bool Polygon<PointType>::separates(const OtherDisk&) const {
    throw std::runtime_error(
        "pgl: Polygon::separates(Disk) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
}

template <class PointType>
template<ConvexConcept OtherConvex>
constexpr bool Polygon<PointType>::separates(const OtherConvex&) const {
    throw std::runtime_error(
        "pgl: Polygon::separates(Convex) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
}

template <class PointType>
template<PolygonConcept OtherPolygon>
constexpr bool Polygon<PointType>::separates(const OtherPolygon&) const {
    throw std::runtime_error(
        "pgl: Polygon::separates(Polygon) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
}

}  // namespace pgl
