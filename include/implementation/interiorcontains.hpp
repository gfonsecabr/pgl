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
    if (other.isEmpty()) {
        // The empty set is a subset of every shape, its boundary and its
        // interior alike.
        return true;
    }
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
    if (other.isEmpty()) {
        // The empty set is a subset of every shape, its boundary and its
        // interior alike.
        return true;
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
    if (other.isEmpty()) {
        // The empty set is a subset of every shape, its boundary and its
        // interior alike.
        return true;
    }
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
    if (other.isEmpty()) {
        // The empty set is a subset of every shape, its boundary and its
        // interior alike.
        return true;
    }
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
    if (other.isEmpty()) {
        // The empty set is a subset of every shape, its boundary and its
        // interior alike.
        return true;
    }
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
    if (other.isEmpty()) {
        // The empty set is a subset of every shape, its boundary and its
        // interior alike.
        return true;
    }
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
    // The empty set has no interior, and it needs no case of its own: it
    // contains no point, so the test below is already false for it.
    return contains(point) && !boundaryContains(point);
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Rectangle<PointType, LabelType>::interiorContains(const OtherLine& other) const {
    if (isEmpty()) {
        // The empty set is a subset of itself and of nothing else.
        return detail::coversNoPoint(other);
    }
    return other.isDegenerate() && interiorContains(other.min());
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Rectangle<PointType, LabelType>::interiorContains(const OtherOrientedLine& other) const {
    if (isEmpty()) {
        // The empty set is a subset of itself and of nothing else.
        return detail::coversNoPoint(other);
    }
    return other.isDegenerate() && interiorContains(other.source());
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Rectangle<PointType, LabelType>::interiorContains(const OtherSegment& other) const {
    if (isEmpty()) {
        // The empty set is a subset of itself and of nothing else.
        return detail::coversNoPoint(other);
    }
    return interiorContains(other.min()) && interiorContains(other.max());
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Rectangle<PointType, LabelType>::interiorContains(const OtherOrientedSegment& other) const {
    if (isEmpty()) {
        // The empty set is a subset of itself and of nothing else.
        return detail::coversNoPoint(other);
    }
    return interiorContains(other.source()) && interiorContains(other.target());
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool Rectangle<PointType, LabelType>::interiorContains(const OtherRay& other) const {
    if (isEmpty()) {
        // The empty set is a subset of itself and of nothing else.
        return detail::coversNoPoint(other);
    }
    return other.isDegenerate() && interiorContains(other.source());
}

template <class PointType, class LabelType>
template<HalfplaneConcept OtherHalfplane>
constexpr bool Rectangle<PointType, LabelType>::interiorContains(const OtherHalfplane& other) const {
    if (isEmpty()) {
        // The empty set is a subset of itself and of nothing else.
        return detail::coversNoPoint(other);
    }
    return other.isDegenerate() && interiorContains(other.source());
}

template <class PointType, class LabelType>
template<RectangleConcept OtherRectangle>
constexpr bool Rectangle<PointType, LabelType>::interiorContains(const OtherRectangle& other) const {
    // The empty set is a subset of every interior, so an empty operand is
    // contained whatever its inverted corners do to the tests below. An empty
    // *this needs no case of its own: it covers no point, so the corner tests
    // already answer false, which is the right answer for every non-empty
    // operand. The emptiness test trails the geometry because containment is
    // usually decided without it.
    return (interiorContains(other.min()) && interiorContains(other.max())) || other.isEmpty();
}

template <class PointType, class LabelType>
template<TriangleConcept OtherTriangle>
constexpr bool Rectangle<PointType, LabelType>::interiorContains(const OtherTriangle& other) const {
    if (isEmpty()) {
        // The empty set is a subset of itself and of nothing else.
        return detail::coversNoPoint(other);
    }
    return interiorContains(other.a()) && interiorContains(other.b()) && interiorContains(other.c());
}

template <class PointType, class LabelType>
template<ConvexConcept OtherConvex>
constexpr bool Rectangle<PointType, LabelType>::interiorContains(const OtherConvex& other) const {
    if (isEmpty()) {
        // The empty set is a subset of itself and of nothing else.
        return detail::coversNoPoint(other);
    }
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
    if (other.isEmpty()) {
        // The empty set is a subset of every shape, its boundary and its
        // interior alike.
        return true;
    }
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
    if (const auto center = other.getIfPoint()) {
        // A radius-zero disk is its center; it has no interior point for the
        // witness test below to find.
        return interiorContains(*center);
    }
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
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }
    // Mirrors contains(other): the boundaries must be parallel and face the
    // same side, and then the whole parallel boundary line of `other` must lie
    // strictly inside, which interiorContains(other.source()) tests exactly.
    if (!asLine().parallel(other.asLine()) ||
        dotSign(target() - source(), other.target() - other.source()) !=
            std::partial_ordering::greater) {
        return false;
    }
    return interiorContains(other.source());
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
    if (other.isEmpty()) {
        // The empty set is a subset of every shape, its boundary and its
        // interior alike.
        return true;
    }
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
    if (other.isEmpty()) {
        // The empty set is a subset of every shape, its boundary and its
        // interior alike.
        return true;
    }
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

// A convex polygon's boundary is exactly two lex-monotone chains — its lower and
// upper hull — so we can run the interiorContains(Polygon) criterion (one vertex
// strictly inside and the boundaries fully disjoint) without building a
// BoundaryChains decomposition (or an asPolygon copy) of the convex: just test
// this polygon's chains against those two known hull chains for any shared point.
template <class PointType, class LabelType>
template<ConvexConcept OtherConvex>
constexpr bool Polygon<PointType, LabelType>::interiorContains(const OtherConvex& other) const {
    using OtherPoint = typename OtherConvex::PointType;
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

    const MonotoneChain<OtherPoint> lower = other.lowerHull();
    const MonotoneChain<OtherPoint> upper = other.upperHull();

    BoundaryChains<Polygon> mine(*this);
    while (!mine.exhausted()) {
        const auto& chain = mine.produceNext();
        for (const MonotoneChain<OtherPoint>* their : {&lower, &upper}) {
            if (chain.intersects(*their)) {
                return false;
            }
        }
    }

    return true;
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
    if (other.isEmpty()) {
        // The empty set is a subset of every shape, its boundary and its
        // interior alike.
        return true;
    }
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

    const R d2 = center<R>().template squaredDistance<R>(other.template center<R>());
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
    if (isEmpty()) {
        // The empty set is a subset of itself and of nothing else.
        return detail::coversNoPoint(other);
    }
    return asConvex().interiorContains(other);
}

template <class PointType, class LabelType>
template<PolygonConcept OtherPolygon>
constexpr bool Rectangle<PointType, LabelType>::interiorContains(const OtherPolygon& other) const {
    if (isEmpty()) {
        // The empty set is a subset of itself and of nothing else.
        return detail::coversNoPoint(other);
    }
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
    if (isEmpty()) {
        // The empty set is a subset of itself and of nothing else.
        return detail::coversNoPoint(other);
    }
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

/**
 * @section predicates-polyline Polyline
 * Open polygonal chain predicates. The relative interior of a polyline is the
 * polyline minus its two extreme vertices as a point set: a self-intersecting
 * polyline may pass through an extreme vertex again mid-chain, and that point
 * is still excluded.
 */

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Polyline<PointType, LabelType>::interiorContains(const OtherPoint& point) const {
    return !boundaryContains(point) && contains(point);
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Polyline<PointType, LabelType>::interiorContains(const OtherSegment& other) const {
    // The interior is the polyline minus the two extreme points, so a
    // contained segment lies in it iff the segment avoids those points
    // entirely -- not just with its endpoints: the polyline may revisit an
    // extreme vertex in the middle of the segment.
    return contains(other) && !other.contains((*this)[0]) &&
           !other.contains((*this)[size() - 1]);
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Polyline<PointType, LabelType>::interiorContains(const OtherOrientedSegment& other) const {
    return interiorContains(static_cast<Segment<typename OtherOrientedSegment::PointType>>(other));
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Polyline<PointType, LabelType>::interiorContains(const OtherLine& other) const {
    return other.isDegenerate() && interiorContains(other.min());
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Polyline<PointType, LabelType>::interiorContains(const OtherOrientedLine& other) const {
    return other.isDegenerate() && interiorContains(other.source());
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool Polyline<PointType, LabelType>::interiorContains(const OtherRay& other) const {
    return other.isDegenerate() && interiorContains(other.source());
}

template <class PointType, class LabelType>
template<HalfplaneConcept OtherHalfplane>
constexpr bool Polyline<PointType, LabelType>::interiorContains(const OtherHalfplane& other) const {
    return other.isDegenerate() && interiorContains(other.source());
}

template <class PointType, class LabelType>
template<TriangleConcept OtherTriangle>
constexpr bool Polyline<PointType, LabelType>::interiorContains(const OtherTriangle& other) const {
    if (!other.isDegenerate()) {
        return false;
    }
    if (other.a() == other.c()) {
        return interiorContains(other.a());
    }
    return interiorContains(Segment<typename OtherTriangle::PointType>(other.a(), other.c()));
}

template <class PointType, class LabelType>
template<MonotoneChainConcept OtherChain>
constexpr bool Polyline<PointType, LabelType>::interiorContains(const OtherChain& other) const {
    if (other.empty()) {
        return true;
    }
    // Same set subtraction as the segment overload: the contained chain must
    // avoid both extreme points of this polyline entirely.
    return contains(other) && !other.contains((*this)[0]) &&
           !other.contains((*this)[size() - 1]);
}

template <class PointType, class LabelType>
template<PolylineConcept OtherPolyline>
constexpr bool Polyline<PointType, LabelType>::interiorContains(const OtherPolyline& other) const {
    if (other.empty()) {
        return true;
    }
    // Same set subtraction as the segment overload: the contained polyline
    // must avoid both extreme points of this polyline entirely.
    return contains(other) && !other.contains((*this)[0]) &&
           !other.contains((*this)[size() - 1]);
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Polyline<PointType, LabelType>::interiorContains(const Shape<OtherPoint>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->interiorContains(value);
        },
        other.variant());
}

template <class Number, class Label>
template<PolylineConcept OtherPolyline>
constexpr bool Point<Number, Label>::interiorContains(const OtherPolyline& other) const {
    return contains(other);
}

// A segment's relative interior is convex, so it contains the polyline iff it
// contains all of its vertices.
template <class PointType, class LabelType>
template<PolylineConcept OtherPolyline>
constexpr bool Segment<PointType, LabelType>::interiorContains(const OtherPolyline& other) const {
    for (const auto& vertex : other) {
        if (!interiorContains(vertex)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<PolylineConcept OtherPolyline>
constexpr bool OrientedSegment<PointType, LabelType>::interiorContains(const OtherPolyline& other) const {
    return asSegment().interiorContains(other);
}

template <class PointType, class LabelType>
template<PolylineConcept OtherPolyline>
constexpr bool Line<PointType, LabelType>::interiorContains(const OtherPolyline& other) const {
    return contains(other);
}

template <class PointType, class LabelType>
template<PolylineConcept OtherPolyline>
constexpr bool OrientedLine<PointType, LabelType>::interiorContains(const OtherPolyline& other) const {
    return asLine().interiorContains(other);
}

// The interiors below are convex sets, so they contain the polyline iff they
// contain all of its vertices.

template <class PointType, class LabelType>
template<PolylineConcept OtherPolyline>
constexpr bool Ray<PointType, LabelType>::interiorContains(const OtherPolyline& other) const {
    for (const auto& vertex : other) {
        if (!interiorContains(vertex)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<PolylineConcept OtherPolyline>
constexpr bool Halfplane<PointType, LabelType>::interiorContains(const OtherPolyline& other) const {
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
template<PolylineConcept OtherPolyline>
constexpr bool Rectangle<PointType, LabelType>::interiorContains(const OtherPolyline& other) const {
    if (isEmpty()) {
        // The empty set is a subset of itself and of nothing else.
        return detail::coversNoPoint(other);
    }
    return asConvex().interiorContains(other);
}

template <class PointType, class LabelType>
template<PolylineConcept OtherPolyline>
constexpr bool Triangle<PointType, LabelType>::interiorContains(const OtherPolyline& other) const {
    return asConvex().interiorContains(other);
}

template <class PointType, class LabelType>
template<PolylineConcept OtherPolyline>
constexpr bool Disk<PointType, LabelType>::interiorContains(const OtherPolyline& other) const {
    for (const auto& vertex : other) {
        if (!interiorContains(vertex)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<PolylineConcept OtherPolyline>
constexpr bool Convex<PointType, LabelType>::interiorContains(const OtherPolyline& other) const {
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

// A contained polyline is a subset of the chain, so it can only reach the
// chain's extreme vertices as a point set; subtract them explicitly (the
// polyline may revisit such a point mid-sequence).
template <class PointType, class LabelType, class Storage>
template<PolylineConcept OtherPolyline>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::interiorContains(const OtherPolyline& other) const {
    if (other.empty()) {
        return true;
    }
    return contains(other) && !other.contains((*this)[0]) &&
           !other.contains((*this)[size() - 1]);
}

// A polygon's interior is generally not convex, so it must interior-contain
// every polyline edge.
template <class PointType, class LabelType>
template<PolylineConcept OtherPolyline>
constexpr bool Polygon<PointType, LabelType>::interiorContains(const OtherPolyline& other) const {
    if (other.empty()) {
        return true;
    }
    if (other.size() == 1) {
        return interiorContains(other[0]);
    }
    for (std::size_t i = 0; i + 1 < other.size(); ++i) {
        if (!interiorContains(Segment<typename OtherPolyline::PointType>(other[i], other[i + 1]))) {
            return false;
        }
    }
    return true;
}


// ---------------------------------------------------------------------------
// HalfplaneIntersection

template <class PointType, class LabelType>
template <PointConcept OtherPoint>
constexpr bool HalfplaneIntersection<PointType, LabelType>::interiorContains(const OtherPoint& point) const {
    // A degenerate region has empty interior, and then no point tests
    // strictly inside all constraints, so no special handling is needed.
    return pointStatus(point) > 0;
}

template <class PointType, class LabelType>
template <SegmentConcept OtherSegment>
constexpr bool HalfplaneIntersection<PointType, LabelType>::interiorContains(const OtherSegment& other) const {
    // The interior of the region is convex, so containing both endpoints
    // contains the segment.
    return interiorContains(other[0]) && interiorContains(other[1]);
}

template <class PointType, class LabelType>
template <OrientedSegmentConcept OtherOrientedSegment>
constexpr bool HalfplaneIntersection<PointType, LabelType>::interiorContains(const OtherOrientedSegment& other) const {
    return interiorContains(other[0]) && interiorContains(other[1]);
}

template <class PointType, class LabelType>
template <LineConcept OtherLine>
constexpr bool HalfplaneIntersection<PointType, LabelType>::interiorContains(const OtherLine& other) const {
    // Only constraints parallel to the line can strictly contain it, and the
    // canonical form stores at most one constraint per direction.
    if (isEmpty() || size() > 2) {
        return false;
    }
    for (const auto& halfplane : halfplanes_) {
        if (!halfplane.interiorContains(other)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template <OrientedLineConcept OtherOrientedLine>
constexpr bool HalfplaneIntersection<PointType, LabelType>::interiorContains(const OtherOrientedLine& other) const {
    return interiorContains(other.asLine());
}

template <class PointType, class LabelType>
template <RayConcept OtherRay>
constexpr bool HalfplaneIntersection<PointType, LabelType>::interiorContains(const OtherRay& other) const {
    // Starting strictly inside and pointing into the recession cone keeps the
    // ray strictly inside: the distance to each boundary line is affine and
    // nonincreasing distances would eventually leave the region, so along a
    // recession direction each distance is nondecreasing.
    if (isEmpty()) {
        return false;
    }
    if (!interiorContains(other.source())) {
        return false;
    }
    const Halfplane<typename OtherRay::PointType> forward(other.source(), other.target());
    return recessionContains(forward);
}

template <class PointType, class LabelType>
template <HalfplaneConcept OtherHalfplane>
constexpr bool HalfplaneIntersection<PointType, LabelType>::interiorContains(const OtherHalfplane& other) const {
    // As with contains: only a same-direction constraint can contain a
    // half-plane, so at most one constraint may be stored.
    if (isEmpty() || size() > 1) {
        return false;
    }
    return halfplanes_.empty() || halfplanes_[0].interiorContains(other);
}

template <class PointType, class LabelType>
template <RectangleConcept OtherRectangle>
constexpr bool HalfplaneIntersection<PointType, LabelType>::interiorContains(const OtherRectangle& other) const {
    if (other.isEmpty()) {
        // The empty set is a subset of every shape, its boundary and its
        // interior alike.
        return true;
    }
    // The interior of the region is convex, so containing the vertices
    // contains the rectangle.
    const auto vertices = other.vertices();
    for (const auto& vertex : vertices) {
        if (!interiorContains(vertex)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template <TriangleConcept OtherTriangle>
constexpr bool HalfplaneIntersection<PointType, LabelType>::interiorContains(const OtherTriangle& other) const {
    return interiorContains(other.a()) && interiorContains(other.b()) && interiorContains(other.c());
}

template <class PointType, class LabelType>
template <DiskConcept OtherDisk>
constexpr bool HalfplaneIntersection<PointType, LabelType>::interiorContains(const OtherDisk& other) const {
    // The interior of the region is the intersection of the constraints'
    // open interiors.
    if (isEmpty()) {
        return false;
    }
    for (const auto& halfplane : halfplanes_) {
        if (!halfplane.interiorContains(other)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template <ConvexConcept OtherConvex>
constexpr bool HalfplaneIntersection<PointType, LabelType>::interiorContains(const OtherConvex& other) const {
    for (std::size_t i = 0; i < other.size(); ++i) {
        if (!interiorContains(other[i])) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template <MonotoneChainConcept OtherChain>
constexpr bool HalfplaneIntersection<PointType, LabelType>::interiorContains(const OtherChain& other) const {
    for (std::size_t i = 0; i < other.size(); ++i) {
        if (!interiorContains(other[i])) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template <PolylineConcept OtherPolyline>
constexpr bool HalfplaneIntersection<PointType, LabelType>::interiorContains(const OtherPolyline& other) const {
    for (std::size_t i = 0; i < other.size(); ++i) {
        if (!interiorContains(other[i])) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template <PolygonConcept OtherPolygon>
constexpr bool HalfplaneIntersection<PointType, LabelType>::interiorContains(const OtherPolygon& other) const {
    // The interior of the region is convex, so containing the vertices
    // contains the polygon.
    for (std::size_t i = 0; i < other.size(); ++i) {
        if (!interiorContains(other[i])) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template <HalfplaneIntersectionConcept OtherRegion>
constexpr bool HalfplaneIntersection<PointType, LabelType>::interiorContains(const OtherRegion& other) const {
    // The interior of the region is the intersection of the constraints' open
    // half-planes, so it contains the other region exactly when every
    // constraint's interior does. A degenerate region has two antiparallel
    // constraints with disjoint interiors, so it correctly contains only the
    // empty region.
    if (other.isEmpty()) {
        return true;
    }
    if (isEmpty()) {
        return false;
    }
    for (const auto& halfplane : halfplanes_) {
        if (!halfplane.interiorContains(other)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template <PointConcept OtherPoint>
constexpr bool HalfplaneIntersection<PointType, LabelType>::interiorContains(const Shape<OtherPoint>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->interiorContains(value);
        },
        other.variant());
}


// ---------------------------------------------------------------------------
// Reverse direction: lower-ranked shapes' interiors containing a
// HalfplaneIntersection.
//
// The empty region is a subset of every interior. A degenerate region reduces
// to its carrier shape; a full-dimensional region can only be inside the
// interior of a two-dimensional shape, where it reduces to strict half-plane
// containment tests or the region's convex-polygon form.

namespace detail {

// Dispatches interiorContains(carrier) over the degenerate region's carrier,
// treating alternatives without a matching overload as geometrically
// impossible.
template <class Shape2, class Region>
constexpr bool interiorContainsDegenerateRegion(const Shape2& shape, const Region& region) {
    return std::visit(
        [&shape](const auto& carrier) {
            if constexpr (requires { shape.interiorContains(carrier); }) {
                return shape.interiorContains(carrier);
            } else {
                (void)carrier;
                return false;
            }
        },
        degenerateRegionCarrier(region));
}

}  // namespace detail

template <class Number, class Label>
template <HalfplaneIntersectionConcept OtherRegion>
constexpr bool Point<Number, Label>::interiorContains(const OtherRegion& other) const {
    // The interior of a point is the point itself.
    return contains(other);
}

template <class PointType, class LabelType>
template <HalfplaneIntersectionConcept OtherRegion>
constexpr bool Segment<PointType, LabelType>::interiorContains(const OtherRegion& other) const {
    if (other.isEmpty()) {
        return true;
    }
    if (!other.isDegenerate()) {
        return false;
    }
    return detail::interiorContainsDegenerateRegion(*this, other);
}

template <class PointType, class LabelType>
template <HalfplaneIntersectionConcept OtherRegion>
constexpr bool OrientedSegment<PointType, LabelType>::interiorContains(const OtherRegion& other) const {
    return asSegment().interiorContains(other);
}

template <class PointType, class LabelType>
template <HalfplaneIntersectionConcept OtherRegion>
constexpr bool Line<PointType, LabelType>::interiorContains(const OtherRegion& other) const {
    // The interior of a line is the line itself.
    return contains(other);
}

template <class PointType, class LabelType>
template <HalfplaneIntersectionConcept OtherRegion>
constexpr bool OrientedLine<PointType, LabelType>::interiorContains(const OtherRegion& other) const {
    return asLine().interiorContains(other);
}

template <class PointType, class LabelType>
template <HalfplaneIntersectionConcept OtherRegion>
constexpr bool Ray<PointType, LabelType>::interiorContains(const OtherRegion& other) const {
    if (other.isEmpty()) {
        return true;
    }
    if (!other.isDegenerate()) {
        return false;
    }
    return detail::interiorContainsDegenerateRegion(*this, other);
}

template <class PointType, class LabelType>
template <HalfplaneIntersectionConcept OtherRegion>
constexpr bool Halfplane<PointType, LabelType>::interiorContains(const OtherRegion& other) const {
    return detail::regionInsideHalfplaneInterior(other, *this);
}

template <class PointType, class LabelType>
template <HalfplaneIntersectionConcept OtherRegion>
constexpr bool Rectangle<PointType, LabelType>::interiorContains(const OtherRegion& other) const {
    if (isEmpty()) {
        // The empty set is a subset of itself and of nothing else.
        return detail::coversNoPoint(other);
    }
    if (other.isEmpty()) {
        return true;
    }
    if (isDegenerate()) {
        return false;  // a degenerate rectangle has empty interior
    }
    const PointType lo(min());
    const PointType hi(max());
    const PointType lohi(lo.x(), hi.y());
    const PointType hilo(hi.x(), lo.y());
    return detail::regionInsideHalfplaneInterior(other, Halfplane<PointType>(lo, hilo)) &&
           detail::regionInsideHalfplaneInterior(other, Halfplane<PointType>(hilo, hi)) &&
           detail::regionInsideHalfplaneInterior(other, Halfplane<PointType>(hi, lohi)) &&
           detail::regionInsideHalfplaneInterior(other, Halfplane<PointType>(lohi, lo));
}

template <class PointType, class LabelType>
template <HalfplaneIntersectionConcept OtherRegion>
constexpr bool Triangle<PointType, LabelType>::interiorContains(const OtherRegion& other) const {
    return detail::regionInsideHalfplaneInterior(other, Halfplane<PointType>(a(), b())) &&
           detail::regionInsideHalfplaneInterior(other, Halfplane<PointType>(b(), c())) &&
           detail::regionInsideHalfplaneInterior(other, Halfplane<PointType>(c(), a()));
}

template <class PointType, class LabelType>
template <HalfplaneIntersectionConcept OtherRegion>
constexpr bool Disk<PointType, LabelType>::interiorContains(const OtherRegion& other) const {
    if (other.isEmpty()) {
        return true;
    }
    if (!other.isBounded() || isDegenerate()) {
        return false;
    }
    // The open disk is convex and the bounded region is the hull of its
    // vertices.
    using E = detail::region_exact_number_t<typename OtherRegion::NumberType>;
    const auto vertices = other.template vertices<E>();
    for (const auto& vertex : vertices) {
        if (!interiorContains(vertex)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template <HalfplaneIntersectionConcept OtherRegion>
constexpr bool Convex<PointType, LabelType>::interiorContains(const OtherRegion& other) const {
    if (other.isEmpty()) {
        return true;
    }
    if (isDegenerate()) {
        return false;  // a degenerate polygon has empty interior
    }
    for (std::size_t i = 0; i < size(); ++i) {
        if (!detail::regionInsideHalfplaneInterior(
                other, Halfplane<PointType>((*this)[i], get(static_cast<std::ptrdiff_t>(i) + 1)))) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType, class Storage>
template <HalfplaneIntersectionConcept OtherRegion>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::interiorContains(const OtherRegion& other) const {
    if (other.isEmpty()) {
        return true;
    }
    if (!other.isDegenerate()) {
        return false;
    }
    return detail::interiorContainsDegenerateRegion(*this, other);
}

template <class PointType, class LabelType>
template <HalfplaneIntersectionConcept OtherRegion>
constexpr bool Polyline<PointType, LabelType>::interiorContains(const OtherRegion& other) const {
    if (other.isEmpty()) {
        return true;
    }
    if (!other.isDegenerate()) {
        return false;
    }
    return detail::interiorContainsDegenerateRegion(*this, other);
}

template <class PointType, class LabelType>
template <HalfplaneIntersectionConcept OtherRegion>
constexpr bool Polygon<PointType, LabelType>::interiorContains(const OtherRegion& other) const {
    if (other.isEmpty()) {
        return true;
    }
    if (!other.isBounded()) {
        return false;
    }
    using E = detail::region_exact_number_t<typename OtherRegion::NumberType>;
    if (other.isDegenerate()) {
        return std::visit(
            [this](const auto& carrier) {
                if constexpr (requires { this->interiorContains(carrier); }) {
                    return this->interiorContains(carrier);
                } else {
                    (void)carrier;
                    return false;
                }
            },
            detail::degenerateRegionCarrier(other));
    }
    return interiorContains(other.template asConvex<E>());
}


// ---------------------------------------------------------------------------
// PolygonWithHoles

template <class PointType, class LabelType>
template <PointConcept OtherPoint>
constexpr bool PolygonWithHoles<PointType, LabelType>::interiorContains(const OtherPoint& point) const {
    if (!outer_.interiorContains(point)) {
        return false;
    }
    // The whole closed hole is out of the region's interior: its boundary is
    // part of the region's boundary, not of its interior.
    for (const auto& hole : holes_) {
        if (hole.contains(point)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template <SegmentConcept OtherSegment>
constexpr bool PolygonWithHoles<PointType, LabelType>::interiorContains(const OtherSegment& other) const {
    if (other.isDegenerate()) {
        return interiorContains(other.min());
    }
    if (!outer_.interiorContains(other)) {
        return false;
    }
    // Unlike contains, no part of a hole survives here — the hole boundary is
    // region boundary — so any contact at all disqualifies the segment.
    for (const auto& hole : holes_) {
        if (hole.intersects(other)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template <OrientedSegmentConcept OtherOrientedSegment>
constexpr bool PolygonWithHoles<PointType, LabelType>::interiorContains(const OtherOrientedSegment& other) const {
    return interiorContains(other.asSegment());
}

// Unbounded operands again: only a degenerate one fits inside a bounded region.
template <class PointType, class LabelType>
template <LineConcept OtherLine>
constexpr bool PolygonWithHoles<PointType, LabelType>::interiorContains(const OtherLine& other) const {
    return other.isDegenerate() && interiorContains(other.min());
}

template <class PointType, class LabelType>
template <OrientedLineConcept OtherOrientedLine>
constexpr bool PolygonWithHoles<PointType, LabelType>::interiorContains(const OtherOrientedLine& other) const {
    return other.isDegenerate() && interiorContains(other.source());
}

template <class PointType, class LabelType>
template <RayConcept OtherRay>
constexpr bool PolygonWithHoles<PointType, LabelType>::interiorContains(const OtherRay& other) const {
    return other.isDegenerate() && interiorContains(other.source());
}

template <class PointType, class LabelType>
template <HalfplaneConcept OtherHalfplane>
constexpr bool PolygonWithHoles<PointType, LabelType>::interiorContains(const OtherHalfplane& other) const {
    return other.isDegenerate() && interiorContains(other.source());
}

// Same argument as outerContains, applied to the open outer polygon: its
// complement — the outer boundary together with the exterior — is closed,
// connected and unbounded, so a bounded shape whose boundary is strictly inside
// is strictly inside.
template <class PointType, class LabelType>
template <class OtherArea>
constexpr bool PolygonWithHoles<PointType, LabelType>::outerInteriorContains(const OtherArea& other) const {
    if constexpr (PolygonWithHolesConcept<OtherArea>) {
        for (const auto& edge : other.edges()) {
            if (!outer_.interiorContains(edge)) {
                return false;
            }
        }
        return true;
    } else {
        return outer_.interiorContains(other);
    }
}

// A° = outer° ∖ ⋃ hole, with the *closed* holes removed: a point of outer° off
// every closed hole has a ball around it inside outer° and clear of the finitely
// many closed holes, so it is interior to the region, and conversely a hole
// boundary point is region boundary. That identity is about point sets, so
// unlike contains it needs no case for a collapsed operand.
template <class PointType, class LabelType>
template <class OtherArea>
constexpr bool PolygonWithHoles<PointType, LabelType>::areaInteriorContains(const OtherArea& other) const {
    if (!outerInteriorContains(other)) {
        return false;
    }
    for (const auto& hole : holes_) {
        if (other.intersects(hole)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template <RectangleConcept OtherRectangle>
constexpr bool PolygonWithHoles<PointType, LabelType>::interiorContains(const OtherRectangle& other) const {
    if (other.isEmpty()) {
        // The empty set is a subset of every shape, its boundary and its
        // interior alike.
        return true;
    }
    return areaInteriorContains(other);
}

template <class PointType, class LabelType>
template <TriangleConcept OtherTriangle>
constexpr bool PolygonWithHoles<PointType, LabelType>::interiorContains(const OtherTriangle& other) const {
    return areaInteriorContains(other);
}

template <class PointType, class LabelType>
template <ConvexConcept OtherConvex>
constexpr bool PolygonWithHoles<PointType, LabelType>::interiorContains(const OtherConvex& other) const {
    return areaInteriorContains(other);
}

template <class PointType, class LabelType>
template <PolygonConcept OtherPolygon>
constexpr bool PolygonWithHoles<PointType, LabelType>::interiorContains(const OtherPolygon& other) const {
    return areaInteriorContains(other);
}

template <class PointType, class LabelType>
template <PolygonWithHolesConcept OtherRegion>
constexpr bool PolygonWithHoles<PointType, LabelType>::interiorContains(const OtherRegion& other) const {
    return areaInteriorContains(other);
}

// A chain is the union of its edges, so the open region holds it exactly when
// it holds every edge (see @ref chainRelation).
template <class PointType, class LabelType>
template <MonotoneChainConcept OtherChain>
constexpr bool PolygonWithHoles<PointType, LabelType>::interiorContains(const OtherChain& other) const {
    return chainRelation(other, true,
                         [this](const auto& edge) { return this->interiorContains(edge); });
}

template <class PointType, class LabelType>
template <PolylineConcept OtherPolyline>
constexpr bool PolygonWithHoles<PointType, LabelType>::interiorContains(const OtherPolyline& other) const {
    return chainRelation(other, true,
                         [this](const auto& edge) { return this->interiorContains(edge); });
}

// A° = outer° ∖ ⋃ hole, with the holes removed closed. That is an identity
// between point sets, so — unlike contains — it asks nothing of the operand and
// applies to the disk as it stands.
template <class PointType, class LabelType>
template <DiskConcept OtherDisk>
constexpr bool PolygonWithHoles<PointType, LabelType>::interiorContains(const OtherDisk& other) const {
    if (other.isDegenerate()) {
        return interiorContains(other.a());  // radius zero, or undefined
    }
    if (!outer_.interiorContains(other)) {
        return false;
    }
    for (const auto& hole : holes_) {
        if (other.intersects(hole)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template <HalfplaneIntersectionConcept OtherIntersection>
constexpr bool PolygonWithHoles<PointType, LabelType>::interiorContains(const OtherIntersection& other) const {
    if (other.isEmpty()) {
        return true;
    }
    if (other.isDegenerate()) {
        return degenerateIntersectionRelation(
            other, [this](const auto& carrier) { return this->interiorContains(carrier); });
    }
    if (!other.isBounded()) {
        return false;
    }
    return areaInteriorContains(asConvexOperand(other));
}


// ---------------------------------------------------------------------------
// Reverse direction: lower-ranked shapes' interiors containing a
// PolygonWithHoles.
//
// The argument of the forward block in contains.hpp carries over verbatim with
// B replaced by B°, so again every shape but a Polyline answers the region as
// it answers the region's outer polygon:
//
//   B° ⊇ A  ⟺  B° ⊇ outer.
//
// A ⊆ outer gives (⇐). For (⇒), A ⊇ ∂outer; a zero-area outer polygon *is*
// ∂outer and carries no hole, so the two questions coincide; and otherwise
// ∂outer is a Jordan curve, which an at most one-dimensional interior cannot
// hold (both sides false) and a two-dimensional one holds together with
// everything inside it, since the complement of B° is closed and connected for
// every shape here.
//
// A Polyline is the exception for the same reason as there — it can close a
// loop — and takes the same zero-area rewriting, edge by edge against its own
// relative interior.

template <class Number, class Label>
template <PolygonWithHolesConcept OtherRegion>
constexpr bool Point<Number, Label>::interiorContains(const OtherRegion& other) const {
    // The interior of a point is the point itself.
    return contains(other);
}

template <class PointType, class LabelType>
template <PolygonWithHolesConcept OtherRegion>
constexpr bool Segment<PointType, LabelType>::interiorContains(const OtherRegion& other) const {
    return interiorContains(other.outer());
}

template <class PointType, class LabelType>
template <PolygonWithHolesConcept OtherRegion>
constexpr bool OrientedSegment<PointType, LabelType>::interiorContains(const OtherRegion& other) const {
    return asSegment().interiorContains(other);
}

template <class PointType, class LabelType>
template <PolygonWithHolesConcept OtherRegion>
constexpr bool Line<PointType, LabelType>::interiorContains(const OtherRegion& other) const {
    // The interior of a line is the line itself.
    return contains(other);
}

template <class PointType, class LabelType>
template <PolygonWithHolesConcept OtherRegion>
constexpr bool OrientedLine<PointType, LabelType>::interiorContains(const OtherRegion& other) const {
    return asLine().interiorContains(other);
}

template <class PointType, class LabelType>
template <PolygonWithHolesConcept OtherRegion>
constexpr bool Ray<PointType, LabelType>::interiorContains(const OtherRegion& other) const {
    return interiorContains(other.outer());
}

template <class PointType, class LabelType>
template <PolygonWithHolesConcept OtherRegion>
constexpr bool Halfplane<PointType, LabelType>::interiorContains(const OtherRegion& other) const {
    return interiorContains(other.outer());
}

template <class PointType, class LabelType>
template <PolygonWithHolesConcept OtherRegion>
constexpr bool Rectangle<PointType, LabelType>::interiorContains(const OtherRegion& other) const {
    if (isEmpty()) {
        // The empty set is a subset of itself and of nothing else.
        return detail::coversNoPoint(other);
    }
    return interiorContains(other.outer());
}

template <class PointType, class LabelType>
template <PolygonWithHolesConcept OtherRegion>
constexpr bool Triangle<PointType, LabelType>::interiorContains(const OtherRegion& other) const {
    return interiorContains(other.outer());
}

template <class PointType, class LabelType>
template <PolygonWithHolesConcept OtherRegion>
constexpr bool Disk<PointType, LabelType>::interiorContains(const OtherRegion& other) const {
    return interiorContains(other.outer());
}

template <class PointType, class LabelType>
template <PolygonWithHolesConcept OtherRegion>
constexpr bool Convex<PointType, LabelType>::interiorContains(const OtherRegion& other) const {
    return interiorContains(other.outer());
}

template <class PointType, class LabelType, class Storage>
template <PolygonWithHolesConcept OtherRegion>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::interiorContains(const OtherRegion& other) const {
    return interiorContains(other.outer());
}

// The exception; see the note above.
template <class PointType, class LabelType>
template <PolygonWithHolesConcept OtherRegion>
constexpr bool Polyline<PointType, LabelType>::interiorContains(const OtherRegion& other) const {
    if (!other.isDegenerate()) {
        return false;  // the region has area; the polyline has none
    }
    return detail::everyHoledRegionEdge(
        other, [this](const auto& edge) { return this->interiorContains(edge); });
}

template <class PointType, class LabelType>
template <PolygonWithHolesConcept OtherRegion>
constexpr bool Polygon<PointType, LabelType>::interiorContains(const OtherRegion& other) const {
    return interiorContains(other.outer());
}

template <class PointType, class LabelType>
template <PolygonWithHolesConcept OtherHoledRegion>
constexpr bool HalfplaneIntersection<PointType, LabelType>::interiorContains(const OtherHoledRegion& other) const {
    return interiorContains(other.outer());
}

// ---------------------------------------------------------------------------
// Runtime Shape argument: unwrap the stored alternative and re-dispatch. Every
// alternative has a per-shape overload above, so no fallback is needed.

template <class PointType, class LabelType>
template <PointConcept OtherPoint>
constexpr bool PolygonWithHoles<PointType, LabelType>::interiorContains(const Shape<OtherPoint>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->interiorContains(value);
        },
        other.variant());
}


// ---------------------------------------------------------------------------
// PolygonSet
//
// The component interiors are open and pairwise disjoint, so their union — the
// set's interior, by the no-shared-edge clause of PolygonSet::isValid — holds a
// connected shape only by holding it in one of them. Every operand but another
// set is connected, so this is exact componentwise throughout, with none of the
// one-dimensional trouble PolygonSet::contains has.

template <class PointType, class LabelType>
template <detail::SetOperandConcept OtherShape>
bool PolygonSet<PointType, LabelType>::interiorContains(const OtherShape& other) const {
    return anyComponent([&](const ComponentType& component) {
        return component.interiorContains(other);
    });
}

template <class PointType, class LabelType>
template <PolygonSetConcept OtherSet>
bool PolygonSet<PointType, LabelType>::interiorContains(const OtherSet& other) const {
    for (const auto& component : other) {
        if (!interiorContains(component)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template <PointConcept OtherPoint>
bool PolygonSet<PointType, LabelType>::interiorContains(const Shape<OtherPoint>& other) const {
    return std::visit([this](const auto& value) { return this->interiorContains(value); },
                      other.variant());
}

}  // namespace pgl
