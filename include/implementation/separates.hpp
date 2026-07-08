#pragma once

#include "implementation/intersects.hpp"

/**
 * @file separates.hpp
 * @brief Implementations of the 'separates' predicate.
 **/

#include <algorithm>
#include <array>
#include <compare>
#include <cstddef>
#include <map>
#include <optional>
#include <stdexcept>
#include <set>
#include <limits>
#include <type_traits>
#include <utility>
#include <vector>
#include "shape/orientedline.hpp"
#include "shape/orientedsegment.hpp"
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
    // A chord that splits the polygon runs through its interior, so the open
    // segment must meet the open polygon. This necessary condition also rejects
    // an exterior chord between two boundary vertices — e.g. a segment along the
    // mouth of a notch — that the boundary-crossing count below would otherwise
    // miscount as a cut (its endpoints touch the boundary tangentially without
    // the segment ever entering the interior).
    if (isDegenerate() || other.isDegenerate() || !other.interiorsIntersect(*this)) {
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
    return    !other.interiorContains(min())
           && !other.interiorContains(max())
           && other.interiorsIntersect(*this);
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
    // A non-degenerate rectangle is its convex view; use the general convex-body
    // algorithm so the result matches separates(Convex) exactly. The previous
    // bespoke edge/vertex formula diverged from it.
    return separates(other.asConvex());
}

template <class PointType, class LabelType>
template<TriangleConcept OtherTriangle>
constexpr bool Triangle<PointType, LabelType>::separates(const OtherTriangle& other) const {
    // See separates(Rectangle): defer to the general convex-body algorithm.
    return separates(other.asConvex());
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

// Disk: specialized rather than delegated to Convex. For a triangle, removing it
// disconnects the disk iff the boundary crosses the circle >= 4 times. With k =
// vertices strictly inside the disk and c = edges carrying a full chord, the
// crossing count is fully determined by (k, c): a triangle can never weave in and
// out four times via single crossings, so the answer is a closed-form table:
//   k == 0 -> separates iff c >= 2
//   k == 1 -> separates iff c >= 1
//   k >= 2 -> never (the disk bulges out on a single arc)
// This avoids building a Convex and short-circuits on k before any chord test.
// Degenerate inputs are undefined here; we report false.
template <class PointType, class LabelType>
template<DiskConcept OtherDisk>
constexpr bool Triangle<PointType, LabelType>::separates(const OtherDisk& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }

    int interior_vertex_count = 0;
    for (const auto& vertex : vertices()) {
        interior_vertex_count += other.interiorContains(vertex) ? 1 : 0;
    }
    if (interior_vertex_count >= 2) {
        return false;
    }

    const int needed = interior_vertex_count == 0 ? 2 : 1;
    int separating_edge_count = 0;
    for (const auto& edge : edges()) {
        if (edge.separates(other) && ++separating_edge_count >= needed) {
            return true;
        }
    }
    return false;
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
template<DiskConcept OtherDisk>
constexpr bool OrientedSegment<PointType, LabelType>::separates(const OtherDisk& other) const {
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
template<DiskConcept OtherDisk>
constexpr bool Line<PointType, LabelType>::separates(const OtherDisk& other) const {
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
template<DiskConcept OtherDisk>
constexpr bool OrientedLine<PointType, LabelType>::separates(const OtherDisk& other) const {
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
template<DiskConcept OtherDisk>
constexpr bool Ray<PointType, LabelType>::separates(const OtherDisk& other) const {
    // The disk is convex, so removing the ray disconnects it exactly when the ray
    // runs clear through as a full chord: the source must not be strictly inside
    // (an interior source leaves only a slit, which stays connected) and the ray's
    // interior must reach the disk's interior, after which the half-infinite far
    // end guarantees a second boundary crossing.
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

// Disk: specialized rather than delegated to Convex (see Triangle's overload for
// the reasoning). A rectangle's four corners are concyclic, so a disk can never
// contain exactly the two corners of a diagonal; hence the answer is again a
// closed-form table in k = corners strictly inside and c = full-chord edges:
//   k == 0      -> separates iff c >= 2
//   k in {1, 2} -> separates iff c >= 1   (k == 2 is always two *adjacent* corners)
//   k >= 3      -> never
// Degenerate inputs are undefined here; we report false.
template <class PointType, class LabelType>
template<DiskConcept OtherDisk>
constexpr bool Rectangle<PointType, LabelType>::separates(const OtherDisk& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }

    int interior_vertex_count = 0;
    for (const auto& vertex : vertices()) {
        interior_vertex_count += other.interiorContains(vertex) ? 1 : 0;
    }
    if (interior_vertex_count >= 3) {
        return false;
    }

    const int needed = interior_vertex_count == 0 ? 2 : 1;
    int separating_edge_count = 0;
    for (const auto& edge : edges()) {
        if (edge.separates(other) && ++separating_edge_count >= needed) {
            return true;
        }
    }
    return false;
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
constexpr bool Halfplane<PointType, LabelType>::separates(const OtherConvex&) const {
    return false;
}

template <class PointType, class LabelType>
template<DiskConcept OtherDisk>
constexpr bool Halfplane<PointType, LabelType>::separates(const OtherDisk& other) const {
    // Removing a closed half-plane from a disk leaves the circular segment on the
    // far side of the boundary line, which is always a single connected piece, so
    // a half-plane never disconnects a (convex) disk.
    (void)other;
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

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Convex<PointType, LabelType>::separates(const OtherPoint&) const {
    return false;
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Convex<PointType, LabelType>::separates(const OtherSegment& other) const {
    // A contact anywhere on the convex polygon -- boundary touch included --
    // leaves a piece of the segment on each side when both endpoints are
    // outside, so this gates on closed intersection, not interior crossing.
    return !other.isDegenerate() && !contains(other.min()) && !contains(other.max()) && intersects(other);
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Convex<PointType, LabelType>::separates(const OtherOrientedSegment& other) const {
    return separates(static_cast<Segment<typename OtherOrientedSegment::PointType>>(other));
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Convex<PointType, LabelType>::separates(const OtherLine& other) const {
    return !other.isDegenerate() && intersects(other);
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Convex<PointType, LabelType>::separates(const OtherOrientedLine& other) const {
    return separates(static_cast<Line<typename OtherOrientedLine::PointType>>(other));
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool Convex<PointType, LabelType>::separates(const OtherRay& other) const {
    return !other.isDegenerate() && !isDegenerate() &&
           !contains(other.source()) && intersects(other);
}

template <class PointType, class LabelType>
template<HalfplaneConcept OtherHalfplane>
constexpr bool Convex<PointType, LabelType>::separates(const OtherHalfplane&) const {
    return false;
}

template <class PointType, class LabelType>
template<RectangleConcept OtherRectangle>
constexpr bool Convex<PointType, LabelType>::separates(const OtherRectangle& other) const {
    return separates(other.asConvex());
}

template <class PointType, class LabelType>
template<TriangleConcept OtherTriangle>
constexpr bool Convex<PointType, LabelType>::separates(const OtherTriangle& other) const {
    return separates(other.asConvex());
}

template <class PointType, class LabelType>
template<ConvexConcept OtherConvex>
constexpr bool Convex<PointType, LabelType>::separates(const OtherConvex& other) const {
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

template <class PointType, class LabelType>
template<DiskConcept OtherDisk>
constexpr bool Convex<PointType, LabelType>::separates(const OtherDisk& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }

    // Removing the polygon disconnects the disk iff its boundary crosses the
    // circle at least four times (>= 2 in/out arcs). Each edge contributes:
    //   - one endpoint inside, one outside -> 1 crossing,
    //   - both endpoints outside and the edge carries a full chord -> 2,
    //   - otherwise 0.
    // (A segment with both endpoints outside meets the circle 0 or 2 times, so
    // an interior crossing is exactly a full chord.) Counting per edge avoids
    // the earlier scheme that ignored crossings split across single-crossing
    // edges -- which under-reported, e.g. a thin rhombus laid across the disk.
    int crossings = 0;
    for (const auto& edge : edges()) {
        const bool min_inside = other.interiorContains(edge.min());
        const bool max_inside = other.interiorContains(edge.max());
        if (min_inside != max_inside) {
            ++crossings;
        } else if (!min_inside && edge.separates(other)) {
            crossings += 2;
        }
        if (crossings >= 4) {
            return true;
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
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }
    return !contains(other.min()) && !contains(other.max()) && intersects(other);
}

template <class PointType, class LabelType>
template <OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Disk<PointType, LabelType>::separates(const OtherOrientedSegment& other) const {
    return separates(other.asSegment());
}

template <class PointType, class LabelType>
template <LineConcept OtherLine>
constexpr bool Disk<PointType, LabelType>::separates(const OtherLine& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }
    return intersects(other);
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
    for (int i = 0; i < 4; i++) {
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

template <class PointType, class LabelType>
template <PointConcept OtherPoint>
constexpr bool Convex<PointType, LabelType>::separates(const Shape<OtherPoint>& other) const {
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

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Polygon<PointType, LabelType>::separates(const OtherPoint&) const {
    return false;
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Polygon<PointType, LabelType>::separates(const OtherSegment& other) const {
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

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool Polygon<PointType, LabelType>::separates(const OtherRay& other) const {
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

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Polygon<PointType, LabelType>::separates(const OtherLine& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }
    return intersects(other);
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Polygon<PointType, LabelType>::separates(const OtherOrientedSegment& other) const {
    return separates(other.asSegment());
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Polygon<PointType, LabelType>::separates(const OtherOrientedLine& other) const {
    return separates(other.asLine());
}

template <class PointType, class LabelType>
template<HalfplaneConcept OtherHalfplane>
constexpr bool Polygon<PointType, LabelType>::separates(const OtherHalfplane&) const {
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

template <class PointType, class LabelType>
template<PolygonConcept OtherPolygon>
constexpr bool Convex<PointType, LabelType>::separates(const OtherPolygon& other) const {
    // Removing a convex body from a polygon is the same cut question as for a
    // general polygon; route through the unified Polygon::separates(Polygon).
    return asPolygon().separates(other);
}

template <class PointType, class LabelType>
template<PolygonConcept OtherPolygon>
constexpr bool Disk<PointType, LabelType>::separates(const OtherPolygon& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }

    // Convex fast path -- same arc count as Disk::separates(Convex): removing
    // the disk disconnects a convex polygon iff its boundary leaves the disk in
    // >= 2 separate arcs. Each vertex is inside or outside the disk (an exact
    // squared-distance test), and an edge with both endpoints outside still
    // contributes an arc if it touches the disk.
    if (other.isConvex()) {
        const std::ptrdiff_t m = static_cast<std::ptrdiff_t>(other.size());
        int arcs = 0;
        bool prev_in = contains(other[m - 1]);
        for (std::ptrdiff_t i = 0; i < m; ++i) {
            const bool cur_in = contains(other[i]);
            if (!prev_in && !cur_in) {
                const Segment<typename OtherPolygon::PointType> edge(other.get(i - 1), other[i]);
                if (intersects(edge)) {
                    ++arcs;
                }
            } else if (prev_in && !cur_in) {
                ++arcs;
            }
            if (arcs >= 2) {
                return true;
            }
            prev_in = cur_in;
        }
        return false;
    }

    // General simple polygon. The arc count above over-reports here: a reflex
    // polygon can wrap around the disk and reconnect two of its boundary arcs,
    // so connectivity of P \ D is decided directly -- still without ever
    // constructing the (irrational) circle crossings. Triangulate P and glue
    // the convex per-triangle answers:
    //   - Within one triangle T, the components of T \ D correspond one-to-one
    //     to the maximal runs of pieces of dT outside the disk that are not
    //     interrupted by a contact of the (closed) disk with dT: the disk's
    //     bite off the convex T disconnects it exactly at those contacts.
    //   - A triangulation edge keeps at most two pieces outside the disk (the
    //     whole edge, or the parts adjacent to each endpoint), and which of
    //     them exist depends only on the edge, never on the incident triangle.
    //     Both triangles sharing the edge therefore resolve a surviving piece
    //     to the same slot, and any crossing between adjacent triangles goes
    //     through such a piece (the complement of the closed disk is open
    //     along the edge, so a crossing point sits in a piece of positive
    //     length).
    // Union-find over the pieces then yields the components of P \ D: the disk
    // separates the polygon iff at least two classes remain. No pieces at all
    // means the polygon is swallowed by the disk, which does not separate it.
    using Seg = Segment<typename OtherPolygon::PointType>;

    std::vector<std::size_t> parent;
    const auto findRoot = [&parent](std::size_t x) {
        while (parent[x] != x) {
            parent[x] = parent[parent[x]];
            x = parent[x];
        }
        return x;
    };
    const auto unite = [&](std::size_t a, std::size_t b) {
        parent[findRoot(a)] = findRoot(b);
    };

    enum : int { WHOLE = 0, NEAR_MIN = 1, NEAR_MAX = 2 };
    std::map<Seg, std::array<std::ptrdiff_t, 3>> pieceSlots;
    const auto slotOf = [&](const Seg& s, int piece) {
        auto [it, inserted] =
            pieceSlots.try_emplace(s, std::array<std::ptrdiff_t, 3>{-1, -1, -1});
        std::ptrdiff_t& slot = it->second[static_cast<std::size_t>(piece)];
        if (slot < 0) {
            slot = static_cast<std::ptrdiff_t>(parent.size());
            parent.push_back(parent.size());
        }
        return static_cast<std::size_t>(slot);
    };

    constexpr std::size_t NO_PIECE = std::numeric_limits<std::size_t>::max();
    // Layering: Triangulation lives in algorithm/triangulation.hpp, which
    // pgl.hpp includes after this header. Reach it only through the dependent
    // call other.triangulation(), never by naming the class here.
    for (const auto& tri : other.triangulation().triangles()) {
        // Walk dT once, emitting its outside pieces in boundary order and
        // recording every disk contact between them; gap-free consecutive
        // pieces (wrap-around included) bound the same component of T \ D.
        std::size_t firstPiece = NO_PIECE;
        std::size_t lastPiece = NO_PIECE;
        bool gapBeforeFirst = false;
        bool gap = false;  // contact seen since the last emitted piece
        const auto emit = [&](std::size_t slot) {
            if (lastPiece == NO_PIECE) {
                firstPiece = slot;
                gapBeforeFirst = gap;
            } else if (!gap) {
                unite(lastPiece, slot);
            }
            lastPiece = slot;
            gap = false;
        };

        const bool in[3] = {contains(tri[0]), contains(tri[1]), contains(tri[2])};
        for (int i = 0; i < 3; ++i) {
            const auto& a = tri.get(i);
            const auto& b = tri.get(i + 1);
            const Seg s(a, b);
            const int nearA = a == s.min() ? NEAR_MIN : NEAR_MAX;
            const int nearB = nearA == NEAR_MIN ? NEAR_MAX : NEAR_MIN;
            if (in[i] && in[(i + 1) % 3]) {  // edge swallowed by the disk
                gap = true;
            } else if (in[i]) {              // walk leaves the disk mid-edge
                gap = true;
                emit(slotOf(s, nearB));
            } else if (in[(i + 1) % 3]) {    // walk enters the disk mid-edge
                emit(slotOf(s, nearA));
                gap = true;
            } else if (intersects(s)) {      // contact strictly inside the edge
                emit(slotOf(s, nearA));
                gap = true;
                emit(slotOf(s, nearB));
            } else {                         // edge clear of the disk
                emit(slotOf(s, WHOLE));
            }
        }
        if (firstPiece != NO_PIECE && !gap && !gapBeforeFirst) {
            unite(lastPiece, firstPiece);
        }
    }

    std::size_t classes = 0;
    for (std::size_t i = 0; i < parent.size(); ++i) {
        if (parent[i] == i && ++classes >= 2) {
            return true;
        }
    }
    return false;
}

template <class PointType, class LabelType>
template<RectangleConcept OtherRectangle>
constexpr bool Polygon<PointType, LabelType>::separates(const OtherRectangle& other) const {
    return separates(other.asPolygon());
}

template <class PointType, class LabelType>
template<TriangleConcept OtherTriangle>
constexpr bool Polygon<PointType, LabelType>::separates(const OtherTriangle& other) const {
    return separates(other.asPolygon());
}

template <class PointType, class LabelType>
template<DiskConcept OtherDisk>
constexpr bool Polygon<PointType, LabelType>::separates(const OtherDisk& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }

    // Convex fast path -- same crossing count as Convex::separates(Disk):
    // removing a convex polygon disconnects the disk when the polygon boundary
    // crosses the circle >= 4 times (>= 2 in/out arcs). Per edge:
    //   - one endpoint inside, one outside -> 1 crossing,
    //   - both endpoints outside and the edge carries a full chord -> 2,
    //   - otherwise 0.
    if (isConvex()) {
        int crossings = 0;
        for (const auto& edge : edges()) {
            const bool min_inside = other.interiorContains(edge.min());
            const bool max_inside = other.interiorContains(edge.max());
            if (min_inside != max_inside) {
                ++crossings;
            } else if (!min_inside && edge.separates(other)) {
                crossings += 2;
            }
            if (crossings >= 4) {
                return true;
            }
        }
        return false;
    }

    // General simple polygon P. The crossing count above over-counts here: a
    // non-convex P can take several separate bites out of the disk D, and the
    // bites' crossings do not disconnect D \ P. Connectivity is decided
    // directly instead -- still without constructing the (irrational) circle
    // crossings:
    //   - Enclose both shapes in a box polygon and build its constrained
    //     Delaunay triangulation with P's edges as constraints. Every triangle
    //     then lies entirely inside or entirely outside P; the sides are told
    //     apart by a flood fill from a box corner that flips whenever it steps
    //     over a constrained edge.
    //   - For an outside triangle T, the piece D ∩ T \ P is convex minus a
    //     part of its own boundary, hence connected. Two pieces adjacent
    //     across a triangulation edge e belong to the same component of D \ P
    //     iff (D ∩ e) \ P is nonempty. Only e's endpoints can lie in P (a P
    //     vertex; box corners are outside D by construction and e's relative
    //     interior never meets P), so that reduces to: the open disk meets e,
    //     or the closed disk touches e in a single interior point.
    // Union-find over the outside triangles then yields the components of
    // D \ P; the polygon separates the disk iff pieces of D \ P fall into at
    // least two classes. Pieces pinched at a P vertex on the circle stay
    // apart, because the pinch point itself belongs to P.
    using Common = std::common_type_t<typename PointType::NumberType,
                                      typename OtherDisk::NumberType>;
    using CPoint = Point<Common>;
    using CSeg = Segment<CPoint>;

    // Bounding box of both shapes, inflated so the box strictly contains them.
    const auto dbox = other.bbox();
    Common xlo = static_cast<Common>(dbox.min().x());
    Common ylo = static_cast<Common>(dbox.min().y());
    Common xhi = static_cast<Common>(dbox.max().x());
    Common yhi = static_cast<Common>(dbox.max().y());
    for (std::size_t i = 0; i < size(); ++i) {
        const auto v = (*this)[i];
        const Common vx = static_cast<Common>(v.x());
        const Common vy = static_cast<Common>(v.y());
        xlo = vx < xlo ? vx : xlo;
        ylo = vy < ylo ? vy : ylo;
        xhi = vx > xhi ? vx : xhi;
        yhi = vy > yhi ? vy : yhi;
    }
    xlo = xlo - Common{1};
    ylo = ylo - Common{1};
    xhi = xhi + Common{1};
    yhi = yhi + Common{1};
    const Polygon<CPoint> box(std::vector<CPoint>{
        CPoint(xlo, ylo), CPoint(xhi, ylo), CPoint(xhi, yhi), CPoint(xlo, yhi)});

    // P's edges become the constraints; the set tells the flood fill where the
    // inside/outside classification flips.
    std::vector<CSeg> constraints;
    constraints.reserve(size());
    std::set<CSeg> pBoundary;
    for (std::size_t i = 0; i < size(); ++i) {
        const auto u = (*this)[i];
        const auto w = get(static_cast<std::ptrdiff_t>(i) + 1);
        const CSeg s(CPoint(static_cast<Common>(u.x()), static_cast<Common>(u.y())),
                     CPoint(static_cast<Common>(w.x()), static_cast<Common>(w.y())));
        constraints.push_back(s);
        pBoundary.insert(s);
    }

    // Layering: Triangulation lives in algorithm/triangulation.hpp, which
    // pgl.hpp includes after this header. Reach it only through the dependent
    // call box.triangulation(constraints), never by naming the class here.
    const auto mesh = box.triangulation(constraints);
    const auto tris = mesh.triangles();
    const std::size_t n = tris.size();
    std::map<typename std::decay_t<decltype(tris)>::value_type, std::size_t> indexOf;
    for (std::size_t i = 0; i < n; ++i) {
        indexOf.emplace(tris[i], i);
    }

    // Flood fill from a box corner: -1 unvisited, 0 inside P, 1 outside P.
    std::vector<signed char> state(n, -1);
    std::vector<std::size_t> pending;
    for (const auto& t : mesh.incidentTriangles(CPoint(xlo, ylo))) {
        const std::size_t i = indexOf.at(t);
        if (state[i] < 0) {
            state[i] = 1;
            pending.push_back(i);
        }
    }
    while (!pending.empty()) {
        const std::size_t i = pending.back();
        pending.pop_back();
        for (int k = 0; k < 3; ++k) {
            const CSeg e(tris[i].get(k), tris[i].get(k + 1));
            const auto nb = mesh.otherTriangle(tris[i], e);
            if (!nb) {
                continue;
            }
            const std::size_t j = indexOf.at(*nb);
            if (state[j] < 0) {
                state[j] = pBoundary.count(e)
                               ? static_cast<signed char>(1 - state[i])
                               : state[i];
                pending.push_back(j);
            }
        }
    }

    std::vector<std::size_t> parent(n);
    for (std::size_t i = 0; i < n; ++i) {
        parent[i] = i;
    }
    const auto findRoot = [&parent](std::size_t x) {
        while (parent[x] != x) {
            parent[x] = parent[parent[x]];
            x = parent[x];
        }
        return x;
    };

    for (std::size_t i = 0; i < n; ++i) {
        if (state[i] != 1) {
            continue;
        }
        for (int k = 0; k < 3; ++k) {
            const CSeg e(tris[i].get(k), tris[i].get(k + 1));
            const auto nb = mesh.otherTriangle(tris[i], e);
            if (!nb) {
                continue;
            }
            const std::size_t j = indexOf.at(*nb);
            if (j < i || state[j] != 1) {
                continue;  // shared edge handled from the smaller index only
            }
            if (other.intersects(e) &&
                (other.interiorsIntersect(e) ||
                 (!other.contains(e.min()) && !other.contains(e.max())))) {
                parent[findRoot(i)] = findRoot(j);
            }
        }
    }

    // D \ P has no isolated points and no pinches, so every component holds a
    // triangle whose open interior meets the open disk; count those classes.
    std::size_t classes = 0;
    std::vector<char> counted(n, 0);
    for (std::size_t i = 0; i < n; ++i) {
        if (state[i] == 1 && other.interiorsIntersect(tris[i])) {
            const std::size_t root = findRoot(i);
            if (!counted[root]) {
                counted[root] = 1;
                if (++classes >= 2) {
                    return true;
                }
            }
        }
    }
    return false;
}

template <class PointType, class LabelType>
template<ConvexConcept OtherConvex>
constexpr bool Polygon<PointType, LabelType>::separates(const OtherConvex& other) const {
    return separates(other.asPolygon());
}

template <class PointType, class LabelType>
template<PolygonConcept OtherPolygon>
constexpr bool Polygon<PointType, LabelType>::separates(const OtherPolygon& other) const {
    // Let A = *this and B = other. A.separates(B) asks whether B \ A is
    // disconnected. For a (topological) disk B this happens exactly when some
    // connected component of A ∩ B meets ∂B in two or more pieces -- a
    // "crosscut". A single bite (one contact arc) or an interior island (a hole,
    // zero contact arcs) leaves B connected, and two disjoint bites belong to
    // different components, so neither cuts. This is the same criterion the
    // Convex overload uses, generalized to a non-convex A: A may dip into B
    // through several separate pockets, so the count is per A ∩ B component, not
    // a raw boundary-arc tally.
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }
    if (!bbox().intersects(other.bbox())) {
        return false;
    }

    // Exact rationals: the components of A ∩ B are polygons whose vertices are
    // the intersections of the two boundaries, which are rational in general.
    using Number = Rational<BigInt>;  // exact, overflow-free (ERational's backing)
    using RPoint = Point<Number, typename OtherPolygon::PointType::LabelType>;
    using RPolygon = Polygon<RPoint>;
    using RSegment = Segment<RPoint>;

    const auto toR = [](const auto& p) {
        return RPoint(Number(p.x()), Number(p.y()));
    };

    // The 2-D components of A ∩ B. Lower-dimensional pieces (shared boundary
    // segments or touch points) cannot disconnect the 2-D body of B, so only the
    // polygon pieces matter.
    std::vector<RPolygon> pieces;
    for (const auto& piece : other.template intersection<Number>(*this)) {
        if (std::holds_alternative<RPolygon>(piece)) {
            pieces.push_back(std::get<RPolygon>(piece));
        }
    }
    if (pieces.empty()) {
        return false;
    }

    // The intersection returns pinched faces (two faces meeting at a single
    // vertex) as separate polygons, but such faces form one connected component
    // of the closed set A ∩ B. Union those that share a vertex so a bowtie
    // crosscut is counted as one component touching ∂B twice.
    const int k = static_cast<int>(pieces.size());
    std::vector<int> parent(k);
    for (int i = 0; i < k; ++i) {
        parent[i] = i;
    }
    const auto find = [&parent](int x) {
        while (parent[x] != x) {
            parent[x] = parent[parent[x]];
            x = parent[x];
        }
        return x;
    };
    std::vector<std::pair<RPoint, int>> vertexOwner;
    for (int i = 0; i < k; ++i) {
        for (const auto& v : pieces[i].vertices()) {
            vertexOwner.emplace_back(v, i);
        }
    }
    std::sort(vertexOwner.begin(), vertexOwner.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
    for (std::size_t i = 1; i < vertexOwner.size(); ++i) {
        if (vertexOwner[i].first == vertexOwner[i - 1].first) {
            parent[find(vertexOwner[i].second)] = find(vertexOwner[i - 1].second);
        }
    }

    // Component roots present at a point q of ∂B: every A ∩ B component whose
    // (closed) region contains q.
    const auto rootsAt = [&](const RPoint& q) {
        std::vector<int> roots;
        for (int i = 0; i < k; ++i) {
            if (pieces[i].contains(q)) {
                const int r = find(i);
                if (std::find(roots.begin(), roots.end(), r) == roots.end()) {
                    roots.push_back(r);
                }
            }
        }
        return roots;
    };

    // All piece vertices: these are exactly the points on ∂B where component
    // membership can change (crossings of ∂A with ∂B and shared-boundary ends),
    // so subdividing each edge of B at them makes every sub-edge uniformly
    // inside or outside a given component.
    std::vector<RPoint> breakpoints;
    for (const auto& piece : pieces) {
        for (const auto& v : piece.vertices()) {
            breakpoints.push_back(v);
        }
    }

    // Walk ∂B once, cyclically, sampling: each vertex, each interior crossing,
    // and the midpoint of each resulting sub-edge. Membership is constant between
    // consecutive breakpoints, so these samples label every arc (including
    // isolated point contacts, caught at the vertex/crossing samples).
    std::vector<std::vector<int>> elements;
    const std::ptrdiff_t n = static_cast<std::ptrdiff_t>(other.size());
    for (std::ptrdiff_t i = 0; i < n; ++i) {
        const RPoint a = toR(other.get(i));
        const RPoint b = toR(other.get(i + 1));

        std::vector<RPoint> cuts;
        const RSegment edge(a, b);
        for (const auto& bp : breakpoints) {
            if (edge.interiorContains(bp)) {
                cuts.push_back(bp);
            }
        }
        std::sort(cuts.begin(), cuts.end(), [&](const RPoint& u, const RPoint& v) {
            return (u.x() - a.x()) * (b.x() - a.x()) + (u.y() - a.y()) * (b.y() - a.y())
                 < (v.x() - a.x()) * (b.x() - a.x()) + (v.y() - a.y()) * (b.y() - a.y());
        });
        cuts.erase(std::unique(cuts.begin(), cuts.end()), cuts.end());

        std::vector<RPoint> chain;
        chain.push_back(a);
        chain.insert(chain.end(), cuts.begin(), cuts.end());
        chain.push_back(b);

        elements.push_back(rootsAt(a));  // the vertex of B
        for (std::size_t j = 0; j + 1 < chain.size(); ++j) {
            const RPoint mid((chain[j].x() + chain[j + 1].x()) / Number(2),
                             (chain[j].y() + chain[j + 1].y()) / Number(2));
            elements.push_back(rootsAt(mid));                  // the sub-edge
            if (j + 1 + 1 < chain.size()) {
                elements.push_back(rootsAt(chain[j + 1]));     // an interior crossing
            }
        }
    }

    // A component cuts B iff its contacts with ∂B fall into >= 2 maximal cyclic
    // runs. Count, per root, the runs as rising edges around the cycle.
    const std::size_t total = elements.size();
    const auto has = [](const std::vector<int>& s, int r) {
        return std::find(s.begin(), s.end(), r) != s.end();
    };
    std::vector<int> roots;
    for (const auto& s : elements) {
        for (int r : s) {
            if (std::find(roots.begin(), roots.end(), r) == roots.end()) {
                roots.push_back(r);
            }
        }
    }
    for (int r : roots) {
        int runs = 0;
        for (std::size_t i = 0; i < total; ++i) {
            const std::size_t prev = (i + total - 1) % total;
            if (has(elements[i], r) && !has(elements[prev], r)) {
                if (++runs >= 2) {
                    return true;
                }
            }
        }
    }
    return false;
}

/**
 * @section predicates-monotonechain MonotoneChain
 * Cut predicates involving a weakly x-monotone chain. The chain is an arc, so
 * whether a removal disconnects it reduces to a component walk over its edges
 * (see detail::separatesChain); the chain disconnecting a one-dimensional
 * shape is the dual walk, and the chain disconnecting a two-dimensional
 * region is the crosscut scan of separatesTwoDimensional. The
 * chain-removing-chain and polygon-removing-chain cases both reduce to the
 * same arc-cut criterion, since a chain's arc order is its lexicographic
 * vertex order: some ordered a < b < c on the chain has b covered by the
 * remover and a, c uncovered.
 */

template <class PointType, class LabelType, class Storage>
template <class OtherShape, class TouchesBoundary>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::separatesOneDimensional(
    const OtherShape& other, TouchesBoundary touchesBoundary) const {
    if (points_.empty()) {
        return false;
    }
    if (points_.size() == 1) {
        // A single vertex disconnects `other` iff it is an interior point of it.
        return other.interiorContains((*this)[0]);
    }
    // Walk the connected components of (this ∩ other): consecutive edge pieces
    // belong to one component iff the shared chain vertex lies on `other`
    // (`other` is convex, so each edge meets it in one connected piece). A
    // component that avoids every boundary point of `other` disconnects it.
    bool active = false;
    bool touched = false;
    for (std::size_t i = 0; i + 1 < points_.size(); ++i) {
        const auto edge = this->template boundaryAt<false>(i);
        if (edge.intersects(other)) {
            const bool connected = active && other.contains((*this)[i]);
            if (!connected) {
                if (active && !touched) {
                    return true;
                }
                touched = false;
            }
            active = true;
            if (touchesBoundary(edge)) {
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

template <class PointType, class LabelType, class Storage>
template <bool OtherIsConvex, class OtherShape>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::separatesTwoDimensional(const OtherShape& other) const {
    if (points_.size() <= 1) {
        return false;
    }

    // Removing the chain disconnects the region iff it removes a crosscut:
    // either a single edge already disconnects the region, or the chain has
    // points p < x < q in chain order with x in the region's interior and
    // p, q both non-interior (boundary or beyond) -- the piece of the chain
    // around x then runs boundary-to-boundary through the interior.
    //
    // The scan looks for the p/x/q pattern at the vertices. That is complete
    // for a convex region: an edge between two interior vertices stays
    // interior, so a mid-edge p, x, or q always has a vertex witness on the
    // correct side (an edge whose non-interior endpoints sandwich an interior
    // point is caught as a separating edge). A non-convex region breaks the
    // first rule -- an edge can leave the interior between two interior
    // vertices -- so OtherIsConvex == false additionally tests such edges,
    // each excursion acting as one more non-interior stop between its
    // endpoints. The last vertex pre-seeds the final non-interior stop, so
    // the scan can stop as soon as the interior stop is confirmed.
    bool a_check = false;  // saw a non-interior stop ...
    bool b_check = false;  // ... then an interior stop ...
    bool c_check = !other.interiorContains((*this)[size() - 1]);  // ... then a non-interior stop
    const auto pattern_found = [&](bool stop_is_interior) {
        if (!a_check) {
            a_check = !stop_is_interior;
        }
        else if (!b_check) {
            b_check = stop_is_interior;
        }
        else if (!c_check) {
            c_check = !stop_is_interior;
        }
        return a_check && b_check && c_check;
    };

    bool prev_interior = other.interiorContains((*this)[0]);
    pattern_found(prev_interior);
    for (std::size_t i = 1; i < size(); ++i) {
        const Segment<PointType> edge((*this)[i - 1], (*this)[i]);
        if (edge.separates(other)) {
            return true;
        }
        const bool cur_interior = other.interiorContains((*this)[i]);
        if constexpr (!OtherIsConvex) {
            if (prev_interior && cur_interior && !other.interiorContains(edge) &&
                pattern_found(false)) {
                return true;
            }
        }
        if (pattern_found(cur_interior)) {
            return true;
        }
        prev_interior = cur_interior;
    }
    return false;
}

template <class PointType, class LabelType, class Storage>
template<SegmentConcept OtherSegment>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::separates(const OtherSegment& other) const {
    return separatesOneDimensional(other, [&other](const auto& edge) {
        return edge.contains(other.min()) || edge.contains(other.max());
    });
}

template <class PointType, class LabelType, class Storage>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::separates(const OtherOrientedSegment& other) const {
    return separates(other.asSegment());
}

template <class PointType, class LabelType, class Storage>
template<LineConcept OtherLine>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::separates(const OtherLine& other) const {
    // A line has no boundary, so any nonempty intersection with the bounded
    // chain disconnects it.
    return intersects(other);
}

template <class PointType, class LabelType, class Storage>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::separates(const OtherOrientedLine& other) const {
    return intersects(other);
}

template <class PointType, class LabelType, class Storage>
template<RayConcept OtherRay>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::separates(const OtherRay& other) const {
    return separatesOneDimensional(other, [&other](const auto& edge) {
        return edge.contains(other.source());
    });
}

template <class PointType, class LabelType, class Storage>
template<HalfplaneConcept OtherHalfplane>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::separates(const OtherHalfplane& other) const {
    // No straight edge can disconnect a halfplane (Segment::separates is
    // always false there), but the chain can: bending through the interior
    // between two non-interior stops seals off a pocket against the boundary
    // line, which is exactly the crosscut pattern of the vertex scan.
    return separatesTwoDimensional(other);
}

template <class PointType, class LabelType, class Storage>
template<RectangleConcept OtherRectangle>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::separates(const OtherRectangle& other) const {
    if (!bbox().intersects(other)) {
        return false;
    }
    if (bbox().separates(other)) {
        return true;
    }
    return separatesTwoDimensional(other);
}

template <class PointType, class LabelType, class Storage>
template<TriangleConcept OtherTriangle>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::separates(const OtherTriangle& other) const {
    if (!bbox().intersects(other.bbox())) {
        return false;
    }
    if (bbox().separates(other.bbox())) {
        return true;
    }
    return separatesTwoDimensional(other);
}

template <class PointType, class LabelType, class Storage>
template<DiskConcept OtherDisk>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::separates(const OtherDisk& other) const {
    if (!bbox().intersects(other.bbox())) {
        return false;
    }
    return separatesTwoDimensional(other);
}

template <class PointType, class LabelType, class Storage>
template<ConvexConcept OtherConvex>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::separates(const OtherConvex& other) const {
    if (!bbox().intersects(other.bbox())) {
        return false;
    }
    if (bbox().separates(other.bbox())) {
        return true;
    }
    return separatesTwoDimensional(other);
}

template <class PointType, class LabelType, class Storage>
template<PolygonConcept OtherPolygon>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::separates(const OtherPolygon& other) const {
    if (!bbox().intersects(other.bbox())) {
        return false;
    }
    if (bbox().separates(other.bbox())) {
        return true;
    }
    // The polygon may be non-convex, so the scan must also spot excursion
    // edges (see separatesTwoDimensional).
    return separatesTwoDimensional<false>(other);
}

template <class PointType, class LabelType, class Storage>
template<MonotoneChainConcept OtherChain>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::separates(const OtherChain& other) const {
    if (!bbox().intersects(other.bbox())) {
        return false;
    }
    if (bbox().separates(other.bbox())) {
        return true;
    }

    // Polyline this separates polyline other if there exist
    // three ordered points a,b,c in other, such that
    // !this->contains(a), this->contains(b), !this->contains(c)
    // These points a,b,c may not be vertices, but if they are on
    // the same edge, than the edge is separated. If they are not
    // on the same edge, then there are two edges that are not contained
    // in this with an intermediate point b that is contained

    bool a_check = false;
    bool b_check = false;
    bool c_check = !contains(other.get(-1));

    for (std::size_t i = 1; i < other.size(); ++i) {
        Segment<typename OtherChain::PointType> edge(other[i-1], other[i]);
        if (separates(edge)) {
            return true; // If edge is separated, then a,b,c are in edge
        }
        if (!a_check) { // We did not find point a st !this->contains(a) yet
            if (!contains(edge)) {
                a_check = true; // point of edge not contained in this is a
                b_check = contains(other[i]); // maybe b st this->contains(b) is in edge
            }
        }
        else if (!b_check) { // We did not find point b > a st this->contains(b) yet
            b_check = intersects(edge); // If the edge intersects this, we found b
        }
        else if (!c_check) { // We did not find point c > b st !this->contains(c) yet
            c_check = !contains(edge); // If edge is not contained, we found c
        }

        if (a_check && b_check && c_check) { // All points found
            return true;
        }
    }

    // There are no such a,b,c
    return false;
}

template <class PointType, class LabelType, class Storage>
template<PointConcept OtherPoint>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::separates(const Shape<OtherPoint>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->separates(value);
        },
        other.variant());
}

template <class Number, class Label>
template<MonotoneChainConcept OtherChain>
constexpr bool Point<Number, Label>::separates(const OtherChain& other) const {
    // Removing a point disconnects the arc iff it is a relative-interior point.
    return other.interiorContains(*this);
}

template <class PointType, class LabelType>
template<MonotoneChainConcept OtherChain>
constexpr bool Segment<PointType, LabelType>::separates(const OtherChain& other) const {
    return detail::separatesChain(*this, other);
}

template <class PointType, class LabelType>
template<MonotoneChainConcept OtherChain>
constexpr bool OrientedSegment<PointType, LabelType>::separates(const OtherChain& other) const {
    return detail::separatesChain(asSegment(), other);
}

template <class PointType, class LabelType>
template<MonotoneChainConcept OtherChain>
constexpr bool Line<PointType, LabelType>::separates(const OtherChain& other) const {
    return detail::separatesChain(*this, other);
}

template <class PointType, class LabelType>
template<MonotoneChainConcept OtherChain>
constexpr bool OrientedLine<PointType, LabelType>::separates(const OtherChain& other) const {
    return detail::separatesChain(*this, other);
}

template <class PointType, class LabelType>
template<MonotoneChainConcept OtherChain>
constexpr bool Ray<PointType, LabelType>::separates(const OtherChain& other) const {
    return detail::separatesChain(*this, other);
}

template <class PointType, class LabelType>
template<MonotoneChainConcept OtherChain>
constexpr bool Halfplane<PointType, LabelType>::separates(const OtherChain& other) const {
    return detail::separatesChain(*this, other);
}

template <class PointType, class LabelType>
template<MonotoneChainConcept OtherChain>
constexpr bool Rectangle<PointType, LabelType>::separates(const OtherChain& other) const {
    return detail::separatesChain(*this, other);
}

template <class PointType, class LabelType>
template<MonotoneChainConcept OtherChain>
constexpr bool Triangle<PointType, LabelType>::separates(const OtherChain& other) const {
    return detail::separatesChain(*this, other);
}

template <class PointType, class LabelType>
template<MonotoneChainConcept OtherChain>
constexpr bool Disk<PointType, LabelType>::separates(const OtherChain& other) const {
    return detail::separatesChain(*this, other);
}

template <class PointType, class LabelType>
template<MonotoneChainConcept OtherChain>
constexpr bool Convex<PointType, LabelType>::separates(const OtherChain& other) const {
    return detail::separatesChain(*this, other);
}

template <class PointType, class LabelType>
template<MonotoneChainConcept OtherChain>
constexpr bool Polygon<PointType, LabelType>::separates(const OtherChain& other) const {
    if (!bbox().intersects(other.bbox())) {
        return false;
    }
    if (bbox().separates(other.bbox())) {
        return true;
    }

    // Polygon this separates polyline other if there exist
    // three ordered points a,b,c in other, such that
    // !this->contains(a), this->contains(b), !this->contains(c)
    // These points a,b,c may not be vertices, but if they are on
    // the same edge, than the edge is separated. If they are not
    // on the same edge, then there are two edges that are not contained
    // in this with an intermediate point b that is contained

    bool a_check = false;
    bool b_check = false;
    bool c_check = !contains(other.get(-1));

    for (std::size_t i = 1; i < other.size(); ++i) {
        Segment<typename OtherChain::PointType> edge(other[i-1], other[i]);
        if (separates(edge)) {
            return true; // If edge is separated, then a,b,c are in edge
        }
        if (!a_check) { // We did not find point a st !this->contains(a) yet
            if (!contains(edge)) {
                a_check = true; // point of edge not contained in this is a
                b_check = contains(other[i]); // maybe b st this->contains(b) is in edge
            }
        }
        else if (!b_check) { // We did not find point b > a st this->contains(b) yet
            b_check = intersects(edge); // If the edge intersects this, we found b
        }
        else if (!c_check) { // We did not find point c > b st !this->contains(c) yet
            c_check = !contains(edge); // If edge is not contained, we found c
        }

        if (a_check && b_check && c_check) { // All points found
            return true;
        }
    }

    // There are no such a,b,c
    return false;
}

}  // namespace pgl
