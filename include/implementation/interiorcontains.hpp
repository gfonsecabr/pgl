#pragma once

#include "implementation/crosses.hpp"

/**
 * @file interiorcontains.hpp
 * @brief Implementations of the 'interiorContains' predicate.
 **/

#include <limits>
#include "predicates_helpers.hpp"


namespace pgl {

/**
 * @section predicates-point Point
 * Point equality and the point-vs-shape predicates. This section also contains
 * the cases where removing a point disconnects a 1D primitive.
 */

template <class Number, class Label>
template<PointConcept OtherPoint>
constexpr bool Point<Number, Label>::interiorContains(const OtherPoint& other) const {
    return contains(other);
}

template <class Number, class Label>
template<SegmentConcept OtherSegment>
constexpr bool Point<Number, Label>::interiorContains(const OtherSegment& other) const {
    return contains(other);
}

template <class Number, class Label>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Point<Number, Label>::interiorContains(const OtherOrientedSegment& other) const {
    return contains(other);
}

template <class Number, class Label>
template<LineConcept OtherLine>
constexpr bool Point<Number, Label>::interiorContains(const OtherLine& other) const {
    return contains(other);
}

template <class Number, class Label>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Point<Number, Label>::interiorContains(const OtherOrientedLine& other) const {
    return contains(other);
}

template <class Number, class Label>
template<RayConcept OtherRay>
constexpr bool Point<Number, Label>::interiorContains(const OtherRay& other) const {
    return contains(other);
}

template <class Number, class Label>
template<HalfplaneConcept OtherHalfplane>
constexpr bool Point<Number, Label>::interiorContains(const OtherHalfplane& other) const {
    return contains(other);
}

template <class Number, class Label>
template<RectangleConcept OtherRectangle>
constexpr bool Point<Number, Label>::interiorContains(const OtherRectangle& other) const {
    return contains(other);
}

template <class Number, class Label>
template<TriangleConcept OtherTriangle>
constexpr bool Point<Number, Label>::interiorContains(const OtherTriangle& other) const {
    return contains(other);
}

template <class Number, class Label>
template<ConvexConcept OtherConvex>
constexpr bool Point<Number, Label>::interiorContains(const OtherConvex& other) const {
    return contains(other);
}

template <class Number, class Label>
template<PolygonConcept OtherPolygon>
constexpr bool Point<Number, Label>::interiorContains(const OtherPolygon& other) const {
    return contains(other);
}

template <class Number, class Label>
template<DiskConcept OtherDisk>
constexpr bool Point<Number, Label>::interiorContains(const OtherDisk& other) const {
    // The interior of a point is the point itself, so this matches contains:
    // it holds only for a disk that degenerates to this very point.
    return other.a() == other.b() && other.b() == other.c() && contains(other.a());
}

/**
 * @section predicates-segment Segment
 * Segment endpoint, boundary, containment, collinearity, intersection, and
 * topological predicates, including the generic `separates` / `crosses`
 * dispatch used against 1D and area targets.
 */

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Segment<PointType, LabelType>::interiorContains(const OtherPoint& point) const {
    return !boundaryContains(point) && contains(point);
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Segment<PointType, LabelType>::interiorContains(const OtherSegment& other) const {
    return interiorContains(other.min()) && interiorContains(other.max());
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Segment<PointType, LabelType>::interiorContains(const OtherOrientedSegment& other) const {
    return interiorContains(other.source()) && interiorContains(other.target());
}

template <class PointType, class LabelType>
template<TriangleConcept OtherTriangle>
constexpr bool Segment<PointType, LabelType>::interiorContains(const OtherTriangle& other) const {
    return interiorContains(other.a()) && interiorContains(other.b()) && interiorContains(other.c());
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Segment<PointType, LabelType>::interiorContains(const Shape<OtherPoint>& other) const {
    return std::visit(
        [this](const auto& value) {
            return interiorContains(value);
        },
        other.variant());
}

/**
 * @section predicates-triangle Triangle
 * Triangle boundary, containment, intersection, and cut predicates, including
 * triangle-vs-rectangle and triangle-vs-triangle topological cases.
 */

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Triangle<PointType, LabelType>::interiorContains(const OtherPoint& point) const {
    return !isDegenerate() && contains(point) && !boundaryContains(point);
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Triangle<PointType, LabelType>::interiorContains(const OtherSegment& other) const {
    return interiorContains(other.min()) && interiorContains(other.max());
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Triangle<PointType, LabelType>::interiorContains(const OtherOrientedSegment& other) const {
    return interiorContains(other.source()) && interiorContains(other.target());
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Triangle<PointType, LabelType>::interiorContains(const OtherLine& other) const {
    return other.isDegenerate() && interiorContains(other.min());
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Triangle<PointType, LabelType>::interiorContains(const OtherOrientedLine& other) const {
    return other.isDegenerate() && interiorContains(other.source());
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool Triangle<PointType, LabelType>::interiorContains(const OtherRay& other) const {
    return other.isDegenerate() && interiorContains(other.source());
}

template <class PointType, class LabelType>
template<HalfplaneConcept OtherHalfplane>
constexpr bool Triangle<PointType, LabelType>::interiorContains(const OtherHalfplane& other) const {
    return other.isDegenerate() && interiorContains(other.source());
}

template <class PointType, class LabelType>
template<RectangleConcept OtherRectangle>
constexpr bool Triangle<PointType, LabelType>::interiorContains(const OtherRectangle& other) const {
    const auto vertices = other.vertices();
    for (const auto& vertex : vertices) {
        if (!interiorContains(vertex)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<TriangleConcept OtherTriangle>
constexpr bool Triangle<PointType, LabelType>::interiorContains(const OtherTriangle& other) const {
    return interiorContains(other.a()) && interiorContains(other.b()) && interiorContains(other.c());
}

template <class PointType, class LabelType>
template<ConvexConcept OtherConvex>
constexpr bool Triangle<PointType, LabelType>::interiorContains(const OtherConvex& other) const {
    if (other.size() == 0) {
        return true;
    }
    if (!bbox().interiorContains(other.bbox())) {
        return false;
    }
    // The triangle is convex, so it interior-contains the convex iff it
    // interior-contains every vertex.
    for (std::size_t i = 0; i < other.size(); ++i) {
        if (!interiorContains(other[i])) {
            return false;
        }
    }
    return true;
}

/**
 * @section predicates-oriented-segment OrientedSegment
 * Oriented-segment predicates. Most topology delegates to the unoriented
 * segment view, with local methods kept for orientation-sensitive behavior.
 */

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool OrientedSegment<PointType, LabelType>::interiorContains(const OtherPoint& point) const {
    return !boundaryContains(point) && contains(point);
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool OrientedSegment<PointType, LabelType>::interiorContains(const OtherSegment& other) const {
    return interiorContains(other.min()) && interiorContains(other.max());
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool OrientedSegment<PointType, LabelType>::interiorContains(const OtherOrientedSegment& other) const {
    return interiorContains(other.source()) && interiorContains(other.target());
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool OrientedSegment<PointType, LabelType>::interiorContains(const OtherLine& other) const {
    return other.isDegenerate() && interiorContains(other.min());
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool OrientedSegment<PointType, LabelType>::interiorContains(const OtherOrientedLine& other) const {
    return other.isDegenerate() && interiorContains(other.source());
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool OrientedSegment<PointType, LabelType>::interiorContains(const OtherRay& other) const {
    return other.isDegenerate() && interiorContains(other.source());
}

template <class PointType, class LabelType>
template<HalfplaneConcept OtherHalfplane>
constexpr bool OrientedSegment<PointType, LabelType>::interiorContains(const OtherHalfplane& other) const {
    return other.isDegenerate() && interiorContains(other.source());
}

template <class PointType, class LabelType>
template<RectangleConcept OtherRectangle>
constexpr bool OrientedSegment<PointType, LabelType>::interiorContains(const OtherRectangle& other) const {
    // min/max alone would miss the other two corners; defer to the convex view.
    return interiorContains(other.asConvex());
}

template <class PointType, class LabelType>
template<TriangleConcept OtherTriangle>
constexpr bool OrientedSegment<PointType, LabelType>::interiorContains(const OtherTriangle& other) const {
    return interiorContains(other.a()) && interiorContains(other.b()) && interiorContains(other.c());
}

template <class PointType, class LabelType>
template<ConvexConcept OtherConvex>
constexpr bool OrientedSegment<PointType, LabelType>::interiorContains(const OtherConvex& other) const {
    // A convex with area (more than two vertices) cannot fit in a 1D interior.
    // Otherwise the segment is convex, so it interior-contains the convex iff it
    // interior-contains every vertex.
    if (other.size() > 2) {
        return false;
    }
    for (std::size_t i = 0; i < other.size(); ++i) {
        if (!interiorContains(other[i])) {
            return false;
        }
    }
    return true;
}

/**
 * @section predicates-line Line
 * Geometric line predicates: geometric equality/order, containment,
 * intersection against 1D and 2D shapes, and generic separation dispatch.
 */

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Line<PointType, LabelType>::interiorContains(const OtherPoint& point) const {
    return contains(point);
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Line<PointType, LabelType>::interiorContains(const OtherLine& other) const {
    return contains(other);
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Line<PointType, LabelType>::interiorContains(const OtherSegment& other) const {
    return contains(other);
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Line<PointType, LabelType>::interiorContains(const OtherOrientedSegment& other) const {
    return contains(other);
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Line<PointType, LabelType>::interiorContains(const OtherOrientedLine& other) const {
    return contains(other);
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool Line<PointType, LabelType>::interiorContains(const OtherRay& other) const {
    return contains(other);
}

template <class PointType, class LabelType>
template<HalfplaneConcept OtherHalfplane>
constexpr bool Line<PointType, LabelType>::interiorContains(const OtherHalfplane& other) const {
    return contains(other);
}

template <class PointType, class LabelType>
template<RectangleConcept OtherRectangle>
constexpr bool Line<PointType, LabelType>::interiorContains(const OtherRectangle& other) const {
    return contains(other);
}

template <class PointType, class LabelType>
template<TriangleConcept OtherTriangle>
constexpr bool Line<PointType, LabelType>::interiorContains(const OtherTriangle& other) const {
    return contains(other);
}

template <class PointType, class LabelType>
template<ConvexConcept OtherConvex>
constexpr bool Line<PointType, LabelType>::interiorContains(const OtherConvex& other) const {
    // A line has no boundary, so its interior is the whole line.
    return contains(other);
}

/**
 * @section predicates-oriented-line OrientedLine
 * Oriented-line predicates. Shared topology is mostly delegated to the
 * unoriented line view, while orientation-specific methods stay local here.
 */

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool OrientedLine<PointType, LabelType>::interiorContains(const OtherPoint& point) const {
    return contains(point);
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool OrientedLine<PointType, LabelType>::interiorContains(const OtherLine& other) const {
    return contains(other);
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool OrientedLine<PointType, LabelType>::interiorContains(const OtherOrientedLine& other) const {
    return contains(other);
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool OrientedLine<PointType, LabelType>::interiorContains(const OtherSegment& other) const {
    return contains(other);
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool OrientedLine<PointType, LabelType>::interiorContains(const OtherOrientedSegment& other) const {
    return contains(other);
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool OrientedLine<PointType, LabelType>::interiorContains(const OtherRay& other) const {
    return contains(other);
}

template <class PointType, class LabelType>
template<HalfplaneConcept OtherHalfplane>
constexpr bool OrientedLine<PointType, LabelType>::interiorContains(const OtherHalfplane& other) const {
    return contains(other);
}

template <class PointType, class LabelType>
template<RectangleConcept OtherRectangle>
constexpr bool OrientedLine<PointType, LabelType>::interiorContains(const OtherRectangle& other) const {
    return contains(other);
}

template <class PointType, class LabelType>
template<TriangleConcept OtherTriangle>
constexpr bool OrientedLine<PointType, LabelType>::interiorContains(const OtherTriangle& other) const {
    return contains(other);
}

template <class PointType, class LabelType>
template<ConvexConcept OtherConvex>
constexpr bool OrientedLine<PointType, LabelType>::interiorContains(const OtherConvex& other) const {
    // A line has no boundary, so its interior is the whole line.
    return contains(other);
}

/**
 * @section predicates-ray Ray
 * Ray-specific containment, intersection, and topological predicates. This is
 * where the asymmetric behavior of a half-infinite 1D primitive is implemented.
 */

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Ray<PointType, LabelType>::interiorContains(const OtherPoint& point) const {
    return contains(point) && !boundaryContains(point);
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Ray<PointType, LabelType>::interiorContains(const OtherLine& other) const {
    return other.isDegenerate() && interiorContains(other.min());
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Ray<PointType, LabelType>::interiorContains(const OtherOrientedLine& other) const {
    return other.isDegenerate() && interiorContains(other.source());
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Ray<PointType, LabelType>::interiorContains(const OtherSegment& other) const {
    return interiorContains(other.min()) && interiorContains(other.max());
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Ray<PointType, LabelType>::interiorContains(const OtherOrientedSegment& other) const {
    return interiorContains(other.source()) && interiorContains(other.target());
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool Ray<PointType, LabelType>::interiorContains(const OtherRay& other) const {
    return interiorContains(other.source()) && contains(other.target());
}

template <class PointType, class LabelType>
template<HalfplaneConcept OtherHalfplane>
constexpr bool Ray<PointType, LabelType>::interiorContains(const OtherHalfplane& other) const {
    return other.isDegenerate() && interiorContains(other.source());
}

template <class PointType, class LabelType>
template<RectangleConcept OtherRectangle>
constexpr bool Ray<PointType, LabelType>::interiorContains(const OtherRectangle& other) const {
    // min/max alone would miss the other two corners; defer to the convex view.
    return interiorContains(other.asConvex());
}

template <class PointType, class LabelType>
template<TriangleConcept OtherTriangle>
constexpr bool Ray<PointType, LabelType>::interiorContains(const OtherTriangle& other) const {
    return interiorContains(other.a()) &&
           interiorContains(other.b()) &&
           interiorContains(other.c());
}

template <class PointType, class LabelType>
template<ConvexConcept OtherConvex>
constexpr bool Ray<PointType, LabelType>::interiorContains(const OtherConvex& other) const {
    // A convex with area (more than two vertices) cannot fit in a 1D interior.
    // Otherwise the ray is convex, so it interior-contains the convex iff it
    // interior-contains every vertex.
    if (other.size() > 2) {
        return false;
    }
    for (std::size_t i = 0; i < other.size(); ++i) {
        if (!interiorContains(other[i])) {
            return false;
        }
    }
    return true;
}

/**
 * @section predicates-rectangle Rectangle
 * Axis-aligned rectangle predicates plus the rectangle-local clipping helpers
 * used to answer strict interior and separation questions.
 */


template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Rectangle<PointType, LabelType>::interiorContains(const OtherPoint& point) const {
    return contains(point) && !boundaryContains(point);
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Rectangle<PointType, LabelType>::interiorContains(const OtherLine& other) const {
    return other.isDegenerate() && interiorContains(other.min());
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Rectangle<PointType, LabelType>::interiorContains(const OtherOrientedLine& other) const {
    return other.isDegenerate() && interiorContains(other.source());
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Rectangle<PointType, LabelType>::interiorContains(const OtherSegment& other) const {
    return interiorContains(other.min()) && interiorContains(other.max());
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Rectangle<PointType, LabelType>::interiorContains(const OtherOrientedSegment& other) const {
    return interiorContains(other.source()) && interiorContains(other.target());
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool Rectangle<PointType, LabelType>::interiorContains(const OtherRay& other) const {
    return other.isDegenerate() && interiorContains(other.source());
}

template <class PointType, class LabelType>
template<HalfplaneConcept OtherHalfplane>
constexpr bool Rectangle<PointType, LabelType>::interiorContains(const OtherHalfplane& other) const {
    return other.isDegenerate() && interiorContains(other.source());
}

template <class PointType, class LabelType>
template<RectangleConcept OtherRectangle>
constexpr bool Rectangle<PointType, LabelType>::interiorContains(const OtherRectangle& other) const {
    return interiorContains(other.min()) && interiorContains(other.max());
}

template <class PointType, class LabelType>
template<TriangleConcept OtherTriangle>
constexpr bool Rectangle<PointType, LabelType>::interiorContains(const OtherTriangle& other) const {
    return interiorContains(other.a()) && interiorContains(other.b()) && interiorContains(other.c());
}

template <class PointType, class LabelType>
template<ConvexConcept OtherConvex>
constexpr bool Rectangle<PointType, LabelType>::interiorContains(const OtherConvex& other) const {
    // For an axis-aligned rectangle, containing the convex strictly is equivalent
    // to containing its (axis-aligned) bounding box strictly.
    return other.size() == 0 || interiorContains(other.bbox());
}

/**
 * @section predicates-halfplane Halfplane
 * Half-plane containment, intersection, and topological predicates, together
 * with the helper routines used for strict side/interior tests.
 */

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Halfplane<PointType, LabelType>::interiorContains(const OtherPoint& point) const {
    return contains(point) && !boundaryContains(point);
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Halfplane<PointType, LabelType>::interiorContains(const OtherLine& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return other.isDegenerate() && interiorContains(other.min());
    }
    const auto direction_side =
        orientationDeterminant(source(), target(), other.max()) -
        orientationDeterminant(source(), target(), other.min());
    return direction_side == decltype(direction_side){} && interiorContains(other.min());
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Halfplane<PointType, LabelType>::interiorContains(const OtherOrientedLine& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return other.isDegenerate() && interiorContains(other.source());
    }
    const auto direction_side =
        orientationDeterminant(source(), target(), other.target()) -
        orientationDeterminant(source(), target(), other.source());
    return direction_side == decltype(direction_side){} && interiorContains(other.source());
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Halfplane<PointType, LabelType>::interiorContains(const OtherSegment& other) const {
    if (isDegenerate()) {
        return false;
    }
    return interiorContains(other.min()) && interiorContains(other.max());
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Halfplane<PointType, LabelType>::interiorContains(const OtherOrientedSegment& other) const {
    if (isDegenerate()) {
        return false;
    }
    return interiorContains(other.source()) && interiorContains(other.target());
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool Halfplane<PointType, LabelType>::interiorContains(const OtherRay& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }
    const auto source_side = orientationDeterminant(source(), target(), other.source());
    const auto direction_side =
        orientationDeterminant(source(), target(), other.target()) -
        orientationDeterminant(source(), target(), other.source());
    const auto zero = decltype(source_side){};
    return zero < source_side && !(direction_side < zero);
}

template <class PointType, class LabelType>
template<RectangleConcept OtherRectangle>
constexpr bool Halfplane<PointType, LabelType>::interiorContains(const OtherRectangle& other) const {
    if (isDegenerate()) {
        return false;
    }
    const auto vertices = other.vertices();
    for (const auto& vertex : vertices) {
        if (!interiorContains(vertex)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<ConvexConcept OtherConvex>
constexpr bool Halfplane<PointType, LabelType>::interiorContains(const OtherConvex& other) const {
    if (other.size() == 0) {
        return true;
    }

    return interiorContains(other[0]) && !static_cast<Line<PointType>>(*this).intersects(other);
}

template <class PointType, class LabelType>
template<DiskConcept OtherDisk>
constexpr bool Halfplane<PointType, LabelType>::interiorContains(const OtherDisk& other) const {
    // The closed disk lies in the open half-plane iff the boundary line does not
    // touch the closed disk at all (so the disk is strictly off the boundary) and
    // the disk is on the interior side (a point strictly inside the disk is in
    // the open half-plane). Both tests are exact and division-free, avoiding the
    // disk's rational center and radius; this is the strict version of
    // contains(Disk), which only excludes the boundary line piercing the open
    // disk.
    return !asLine().intersects(other) && other.pointInsideInteriorContainedIn(*this);
}

template <class PointType, class LabelType>
template<HalfplaneConcept OtherHalfplane>
constexpr bool Halfplane<PointType, LabelType>::interiorContains(const OtherHalfplane& other) const {
    return other.isDegenerate() && interiorContains(other.source());
}

template <class PointType, class LabelType>
template<TriangleConcept OtherTriangle>
constexpr bool Halfplane<PointType, LabelType>::interiorContains(const OtherTriangle& other) const {
    return interiorContains(other.a()) && interiorContains(other.b()) && interiorContains(other.c());
}


// ---------------------------------------------------------------------------
// Convex

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Convex<PointType, LabelType>::interiorContains(const OtherPoint& point) const {
    // A point is in the strict interior iff it is contained but not on
    // the boundary. Both predicates are O(log n) and together they handle
    // every edge case (including points on vertical edges at extreme x,
    // where edgesAtX returns the horizontal extent rather than the
    // vertical edge itself).
    return contains(point) && !boundaryContains(point);
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Convex<PointType, LabelType>::interiorContains(const OtherSegment& other) const {
    return interiorContains(other[0]) && interiorContains(other[1]);
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Convex<PointType, LabelType>::interiorContains(const OtherOrientedSegment& other) const {
    return interiorContains(other[0]) && interiorContains(other[1]);
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Convex<PointType, LabelType>::interiorContains(const OtherLine&) const {
    return false;
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Convex<PointType, LabelType>::interiorContains(const OtherOrientedLine&) const {
    return false;
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool Convex<PointType, LabelType>::interiorContains(const OtherRay&) const {
    return false;
}

template <class PointType, class LabelType>
template<HalfplaneConcept OtherHalfplane>
constexpr bool Convex<PointType, LabelType>::interiorContains(const OtherHalfplane&) const {
    return false;
}

template <class PointType, class LabelType>
template<RectangleConcept OtherRectangle>
constexpr bool Convex<PointType, LabelType>::interiorContains(const OtherRectangle& other) const {
    if (!bbox().interiorContains(other)) {
        return false;
    }
    for (size_t i = 0; i < 4; ++i) {
        if (!interiorContains(other[i])) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<TriangleConcept OtherTriangle>
constexpr bool Convex<PointType, LabelType>::interiorContains(const OtherTriangle& other) const {
    if (!bbox().interiorContains(other)) {
        return false;
    }
    for (size_t i = 0; i < 3; ++i) {
        if (!interiorContains(other[i])) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<ConvexConcept OtherConvex>
constexpr bool Convex<PointType, LabelType>::interiorContains(const OtherConvex& other) const {
    if (size() <= 2) {
        return false;
    }
    if (other.size() == 0) {
        return true;
    }
    if (!bbox().interiorContains(other.bbox())) {
        return false;
    }
    if (other.size() == 1) {
        return interiorContains(other[0]);
    }
    if (other.size() == 2) {
        return interiorContains(Segment<typename OtherConvex::PointType>(other[0], other[1]));
    }

    if (other.size() <= 2*size()) {
        for (size_t i = 0; i < other.size(); ++i) {
            if (!interiorContains(other[i])) {
                return false;
            }
        }
    } else {
        for (const auto& edge : orientedEdgesView()) {
            if (!edge.leftHalfplane().interiorContains(other)) {
                return false;
            }
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<DiskConcept OtherDisk>
constexpr bool Convex<PointType, LabelType>::interiorContains(const OtherDisk& other) const {
    for (const auto& edge : orientedEdgesView()) {
        if (!edge.leftHalfplane().interiorContains(other)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template <PointConcept OtherPoint>
constexpr bool Convex<PointType, LabelType>::interiorContains(const Shape<OtherPoint>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->interiorContains(value);
        },
        other.variant());
}

// ---------------------------------------------------------------------------
// Polygon

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Polygon<PointType, LabelType>::interiorContains(const OtherPoint& point) const {
    // Strictly interior iff contained but not on the boundary (mirrors
    // Convex::interiorContains). A polygon with fewer than three vertices has
    // every contained point on its boundary, so this yields false there.
    return contains(point) && !boundaryContains(point);
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Polygon<PointType, LabelType>::interiorContains(const OtherSegment& other) const {
    if (size() < 3) {
        return false;
    }
    // Both endpoints strictly inside, plus no boundary contact anywhere: a
    // segment that grazes or crosses the boundary (e.g. through a reflex notch)
    // is rejected even when both endpoints are interior.
    if (!interiorContains(other.min()) || !interiorContains(other.max())) {
        return false;
    }
    for (const auto& edge : edgesView()) {
        if (other.intersects(edge)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Polygon<PointType, LabelType>::interiorContains(const OtherOrientedSegment& other) const {
    return interiorContains(Segment<typename OtherOrientedSegment::PointType>(other.source(), other.target()));
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Polygon<PointType, LabelType>::interiorContains(const OtherLine& other) const {
    return other.isDegenerate() && interiorContains(other.min());
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Polygon<PointType, LabelType>::interiorContains(const OtherOrientedLine& other) const {
    return other.isDegenerate() && interiorContains(other.source());
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool Polygon<PointType, LabelType>::interiorContains(const OtherRay& other) const {
    return other.isDegenerate() && interiorContains(other.source());
}

template <class PointType, class LabelType>
template<HalfplaneConcept OtherHalfplane>
constexpr bool Polygon<PointType, LabelType>::interiorContains(const OtherHalfplane& other) const {
    return other.isDegenerate() && interiorContains(other.source());
}

// For a simple polygon (no holes) the region overloads reduce to an
// edge-by-edge interior check, mirroring the contains() overloads.
template <class PointType, class LabelType>
template<RectangleConcept OtherRectangle>
constexpr bool Polygon<PointType, LabelType>::interiorContains(const OtherRectangle& other) const {
    for (std::size_t i = 0; i < other.size(); ++i) {
        if (!interiorContains(Segment<typename OtherRectangle::PointType>(other[i], other[(i + 1) % other.size()]))) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<TriangleConcept OtherTriangle>
constexpr bool Polygon<PointType, LabelType>::interiorContains(const OtherTriangle& other) const {
    for (std::size_t i = 0; i < other.size(); ++i) {
        if (!interiorContains(Segment<typename OtherTriangle::PointType>(other[i], other[(i + 1) % other.size()]))) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<ConvexConcept OtherConvex>
constexpr bool Polygon<PointType, LabelType>::interiorContains(const OtherConvex& other) const {
    return interiorContains(other.asPolygon());
}

template <class PointType, class LabelType>
template<PolygonConcept OtherPolygon>
constexpr bool Polygon<PointType, LabelType>::interiorContains(const OtherPolygon& other) const {
    if (other.size() == 0) {
        return true;
    }
    if (!bbox().interiorContains(other.bbox())) {
        return false;
    }
    if (other.size() == 1) {
        return interiorContains(other[0]);
    }

    if (!interiorContains(other[0])) {
        return false;
    }

    return !boundariesIntersect(other);
}

// ---------------------------------------------------------------------------
// Disk

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Disk<PointType, LabelType>::interiorContains(const OtherPoint& point) const {
    return inCircleSign(a(), b(), c(), point) == std::partial_ordering::greater;
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Disk<PointType, LabelType>::interiorContains(const OtherSegment& other) const {
    return interiorContains(other.min()) && interiorContains(other.max());
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Disk<PointType, LabelType>::interiorContains(const OtherOrientedSegment& other) const {
    return interiorContains(other.source()) && interiorContains(other.target());
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Disk<PointType, LabelType>::interiorContains(const OtherLine&) const {
    return false;
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Disk<PointType, LabelType>::interiorContains(const OtherOrientedLine&) const {
    return false;
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool Disk<PointType, LabelType>::interiorContains(const OtherRay&) const {
    return false;
}

template <class PointType, class LabelType>
template<HalfplaneConcept OtherHalfplane>
constexpr bool Disk<PointType, LabelType>::interiorContains(const OtherHalfplane&) const {
    return false;
}

template <class PointType, class LabelType>
template<TriangleConcept OtherTriangle>
constexpr bool Disk<PointType, LabelType>::interiorContains(const OtherTriangle& other) const {
    return interiorContains(other.a()) && interiorContains(other.b()) && interiorContains(other.c());
}

template <class PointType, class LabelType>
template<RectangleConcept OtherRectangle>
constexpr bool Disk<PointType, LabelType>::interiorContains(const OtherRectangle& other) const {
    const auto vertices = other.vertices();
    return interiorContains(vertices[0]) && interiorContains(vertices[1]) &&
           interiorContains(vertices[2]) && interiorContains(vertices[3]);
}

template <class PointType, class LabelType>
template<ConvexConcept OtherConvex>
constexpr bool Disk<PointType, LabelType>::interiorContains(const OtherConvex& other) const {
    for (const auto& point : other) {
        if (!interiorContains(point)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<DiskConcept OtherDisk>
constexpr bool Disk<PointType, LabelType>::interiorContains(const OtherDisk& other) const {
    // The circumcenter and squared circumradius are generally rational, so they
    // must be evaluated exactly (truncating to an integer type gives wrong
    // answers for three-point disks); mirror intersects(Disk) and contains(Disk).
    using R = std::conditional_t<
        std::is_floating_point_v<NumberType> ||
            std::is_floating_point_v<typename OtherDisk::NumberType>,
        long double,
        Rational<BigInt>>;

    const R r1_sq = squaredRadius<R>();
    const R r2_sq = other.template squaredRadius<R>();
    if (r1_sq <= r2_sq) {
        return false;
    }

    const R d2 = center<R>().squaredDistance(other.template center<R>());
    const R A = d2 - r1_sq - r2_sq;
    return A < R{} && A * A > R{4} * r1_sq * r2_sq;
}

template <class PointType, class LabelType>
template<PolygonConcept OtherPolygon>
constexpr bool Convex<PointType, LabelType>::interiorContains(const OtherPolygon& other) const {
    if (size() <= 2) {
        return false;
    }
    for (const auto& vertex : other) {
        if (!interiorContains(vertex)) {
            return false;
        }
    }
    return true;
}


// --- asymmetric Disk/Polygon containment ---
//
// Oriented 1D shapes forward to their unoriented view. The remaining 1D shapes
// can only interior-contain a disk that has degenerated to a single point, or a
// polygon whose vertices all lie on the (relative) interior. The 2D convex
// shapes (Triangle, Rectangle, Disk, Halfplane) reuse the convex containment
// logic: a convex set interior-contains a polygon iff it interior-contains
// every vertex, and Triangle/Rectangle defer the disk case to their Convex view.

template <class PointType, class LabelType>
template<DiskConcept OtherDisk>
constexpr bool OrientedSegment<PointType, LabelType>::interiorContains(const OtherDisk& other) const {
    return asSegment().interiorContains(other);
}

template <class PointType, class LabelType>
template<PolygonConcept OtherPolygon>
constexpr bool OrientedSegment<PointType, LabelType>::interiorContains(const OtherPolygon& other) const {
    return asSegment().interiorContains(other);
}

template <class PointType, class LabelType>
template<DiskConcept OtherDisk>
constexpr bool Line<PointType, LabelType>::interiorContains(const OtherDisk& other) const {
    // A line is 1D, so it interior-contains a disk only when the disk
    // degenerates to a single point lying on the line.
    return other.a() == other.b() && other.b() == other.c() && contains(other.a());
}

template <class PointType, class LabelType>
template<PolygonConcept OtherPolygon>
constexpr bool Line<PointType, LabelType>::interiorContains(const OtherPolygon& other) const {
    // A line has no boundary, so its interior is the whole line.
    return contains(other);
}

template <class PointType, class LabelType>
template<DiskConcept OtherDisk>
constexpr bool OrientedLine<PointType, LabelType>::interiorContains(const OtherDisk& other) const {
    return asLine().interiorContains(other);
}

template <class PointType, class LabelType>
template<PolygonConcept OtherPolygon>
constexpr bool OrientedLine<PointType, LabelType>::interiorContains(const OtherPolygon& other) const {
    return asLine().interiorContains(other);
}

template <class PointType, class LabelType>
template<DiskConcept OtherDisk>
constexpr bool Ray<PointType, LabelType>::interiorContains(const OtherDisk& other) const {
    // A ray is 1D, so it interior-contains a disk only when the disk
    // degenerates to a single point in the ray's interior.
    return other.a() == other.b() && other.b() == other.c() && interiorContains(other.a());
}

template <class PointType, class LabelType>
template<PolygonConcept OtherPolygon>
constexpr bool Ray<PointType, LabelType>::interiorContains(const OtherPolygon& other) const {
    // A ray is 1D: a polygon fits in its interior only when degenerate (all
    // vertices collinear on the ray), which the per-vertex test captures.
    for (const auto& vertex : other) {
        if (!interiorContains(vertex)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<PolygonConcept OtherPolygon>
constexpr bool Halfplane<PointType, LabelType>::interiorContains(const OtherPolygon& other) const {
    if (isDegenerate()) {
        return false;
    }
    // The half-plane is convex, so it interior-contains the polygon iff it
    // interior-contains every vertex.
    for (const auto& vertex : other) {
        if (!interiorContains(vertex)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<DiskConcept OtherDisk>
constexpr bool Rectangle<PointType, LabelType>::interiorContains(const OtherDisk& other) const {
    return asConvex().interiorContains(other);
}

template <class PointType, class LabelType>
template<PolygonConcept OtherPolygon>
constexpr bool Rectangle<PointType, LabelType>::interiorContains(const OtherPolygon& other) const {
    return asConvex().interiorContains(other);
}

template <class PointType, class LabelType>
template<DiskConcept OtherDisk>
constexpr bool Triangle<PointType, LabelType>::interiorContains(const OtherDisk& other) const {
    return asConvex().interiorContains(other);
}

template <class PointType, class LabelType>
template<PolygonConcept OtherPolygon>
constexpr bool Triangle<PointType, LabelType>::interiorContains(const OtherPolygon& other) const {
    return asConvex().interiorContains(other);
}

template <class PointType, class LabelType>
template<PolygonConcept OtherPolygon>
constexpr bool Disk<PointType, LabelType>::interiorContains(const OtherPolygon& other) const {
    // The disk is convex, so it interior-contains the polygon iff it
    // interior-contains every vertex.
    for (const auto& vertex : other) {
        if (!interiorContains(vertex)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<DiskConcept OtherDisk>
constexpr bool Polygon<PointType, LabelType>::interiorContains(const OtherDisk& other) const {
    if (other.isDegenerate()) {
        return interiorContains(other.a());
    }

    if (!interiorContains(other.a())) {
        return false;
    }
    for (const auto& edge : edgesView()) {
        if (other.intersects(edge)) {
            return false;
        }
    }
    return true;
}

/**
 * @section predicates-monotonechain MonotoneChain
 * Weakly x-monotone chain predicates: the relative interior of a chain is the
 * chain minus its two extreme vertices, matching the convention of Segment.
 */

template <class PointType, class LabelType, class Storage>
template<PointConcept OtherPoint>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::interiorContains(const OtherPoint& point) const {
    return !boundaryContains(point) && contains(point);
}

template <class PointType, class LabelType, class Storage>
template<SegmentConcept OtherSegment>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::interiorContains(const OtherSegment& other) const {
    // Containment already puts every point of the segment on the chain; the
    // extreme chain vertices have degree one, so a contained segment avoids
    // the chain's boundary iff its endpoints do.
    return contains(other) && !boundaryContains(other.min()) && !boundaryContains(other.max());
}

template <class PointType, class LabelType, class Storage>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::interiorContains(const OtherOrientedSegment& other) const {
    return interiorContains(static_cast<Segment<typename OtherOrientedSegment::PointType>>(other));
}

template <class PointType, class LabelType, class Storage>
template<LineConcept OtherLine>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::interiorContains(const OtherLine& other) const {
    return other.isDegenerate() && interiorContains(other.min());
}

template <class PointType, class LabelType, class Storage>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::interiorContains(const OtherOrientedLine& other) const {
    return other.isDegenerate() && interiorContains(other.source());
}

template <class PointType, class LabelType, class Storage>
template<RayConcept OtherRay>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::interiorContains(const OtherRay& other) const {
    return other.isDegenerate() && interiorContains(other.source());
}

template <class PointType, class LabelType, class Storage>
template<HalfplaneConcept OtherHalfplane>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::interiorContains(const OtherHalfplane& other) const {
    return other.isDegenerate() && interiorContains(other.source());
}

template <class PointType, class LabelType, class Storage>
template<TriangleConcept OtherTriangle>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::interiorContains(const OtherTriangle& other) const {
    if (!other.isDegenerate()) {
        return false;
    }
    if (other.a() == other.c()) {
        return interiorContains(other.a());
    }
    return interiorContains(Segment<typename OtherTriangle::PointType>(other.a(), other.c()));
}

template <class PointType, class LabelType, class Storage>
template<MonotoneChainConcept OtherChain>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::interiorContains(const OtherChain& other) const {
    if (other.empty()) {
        return true;
    }
    // A contained chain is a connected subset of this chain, so it can only
    // reach this chain's extreme vertices through its own extremes.
    return contains(other) && !boundaryContains(other[0]) &&
           !boundaryContains(other[other.size() - 1]);
}

template <class PointType, class LabelType, class Storage>
template<PointConcept OtherPoint>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::interiorContains(const Shape<OtherPoint>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->interiorContains(value);
        },
        other.variant());
}

template <class Number, class Label>
template<MonotoneChainConcept OtherChain>
constexpr bool Point<Number, Label>::interiorContains(const OtherChain& other) const {
    return contains(other);
}

// The interiors below are convex sets, so they contain the chain iff they
// contain all of its vertices.

template <class PointType, class LabelType>
template<MonotoneChainConcept OtherChain>
constexpr bool Segment<PointType, LabelType>::interiorContains(const OtherChain& other) const {
    for (const auto& vertex : other) {
        if (!interiorContains(vertex)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<MonotoneChainConcept OtherChain>
constexpr bool OrientedSegment<PointType, LabelType>::interiorContains(const OtherChain& other) const {
    return asSegment().interiorContains(other);
}

template <class PointType, class LabelType>
template<MonotoneChainConcept OtherChain>
constexpr bool Line<PointType, LabelType>::interiorContains(const OtherChain& other) const {
    return contains(other);
}

template <class PointType, class LabelType>
template<MonotoneChainConcept OtherChain>
constexpr bool OrientedLine<PointType, LabelType>::interiorContains(const OtherChain& other) const {
    return asLine().interiorContains(other);
}

template <class PointType, class LabelType>
template<MonotoneChainConcept OtherChain>
constexpr bool Ray<PointType, LabelType>::interiorContains(const OtherChain& other) const {
    for (const auto& vertex : other) {
        if (!interiorContains(vertex)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<MonotoneChainConcept OtherChain>
constexpr bool Halfplane<PointType, LabelType>::interiorContains(const OtherChain& other) const {
    if (isDegenerate()) {
        return false;
    }
    for (const auto& vertex : other) {
        if (!interiorContains(vertex)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<MonotoneChainConcept OtherChain>
constexpr bool Rectangle<PointType, LabelType>::interiorContains(const OtherChain& other) const {
    return asConvex().interiorContains(other);
}

template <class PointType, class LabelType>
template<MonotoneChainConcept OtherChain>
constexpr bool Triangle<PointType, LabelType>::interiorContains(const OtherChain& other) const {
    return asConvex().interiorContains(other);
}

template <class PointType, class LabelType>
template<MonotoneChainConcept OtherChain>
constexpr bool Disk<PointType, LabelType>::interiorContains(const OtherChain& other) const {
    for (const auto& vertex : other) {
        if (!interiorContains(vertex)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<MonotoneChainConcept OtherChain>
constexpr bool Convex<PointType, LabelType>::interiorContains(const OtherChain& other) const {
    if (size() <= 2) {
        return false;
    }
    for (const auto& vertex : other) {
        if (!interiorContains(vertex)) {
            return false;
        }
    }
    return true;
}

// A polygon's interior is generally not convex, so it must interior-contain
// every chain edge.
template <class PointType, class LabelType>
template<MonotoneChainConcept OtherChain>
constexpr bool Polygon<PointType, LabelType>::interiorContains(const OtherChain& other) const {
    if (other.empty()) {
        return true;
    }
    if (other.size() == 1) {
        return interiorContains(other[0]);
    }
    for (std::size_t i = 0; i + 1 < other.size(); ++i) {
        if (!interiorContains(Segment<typename OtherChain::PointType>(other[i], other[i + 1]))) {
            return false;
        }
    }
    return true;
}

}  // namespace pgl
