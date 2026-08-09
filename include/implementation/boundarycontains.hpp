#pragma once

#include "implementation/predicates_helpers.hpp"

/**
 * @file boundarycontains.hpp
 * @brief Implementations of the 'boundaryContains' predicate.
 **/

#include <cstddef>
#include <limits>
#include "shape/segment.hpp"
#include "predicates_helpers.hpp"


namespace pgl {

/**
 * @section predicates-point Point
 * Point equality and the point-vs-shape predicates. This section also contains
 * the cases where removing a point disconnects a 1D primitive.
 */

template <class Number, class Label>
template<PointConcept OtherPoint>
constexpr bool Point<Number, Label>::boundaryContains(const OtherPoint&) const {
    return false;
}

template <class Number, class Label>
template<SegmentConcept OtherSegment>
constexpr bool Point<Number, Label>::boundaryContains(const OtherSegment&) const {
    return false;
}

template <class Number, class Label>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Point<Number, Label>::boundaryContains(const OtherOrientedSegment&) const {
    return false;
}

template <class Number, class Label>
template<LineConcept OtherLine>
constexpr bool Point<Number, Label>::boundaryContains(const OtherLine&) const {
    return false;
}

template <class Number, class Label>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Point<Number, Label>::boundaryContains(const OtherOrientedLine&) const {
    return false;
}

template <class Number, class Label>
template<RayConcept OtherRay>
constexpr bool Point<Number, Label>::boundaryContains(const OtherRay&) const {
    return false;
}

template <class Number, class Label>
template<HalfplaneConcept OtherHalfplane>
constexpr bool Point<Number, Label>::boundaryContains(const OtherHalfplane&) const {
    return false;
}

template <class Number, class Label>
template<RectangleConcept OtherRectangle>
constexpr bool Point<Number, Label>::boundaryContains(const OtherRectangle&) const {
    return false;
}

template <class Number, class Label>
template<TriangleConcept OtherTriangle>
constexpr bool Point<Number, Label>::boundaryContains(const OtherTriangle&) const {
    return false;
}

template <class Number, class Label>
template<ConvexConcept OtherConvex>
constexpr bool Point<Number, Label>::boundaryContains(const OtherConvex&) const {
    return false;
}

template <class Number, class Label>
template<PolygonConcept OtherPolygon>
constexpr bool Point<Number, Label>::boundaryContains(const OtherPolygon&) const {
    return false;
}

template <class Number, class Label>
template<DiskConcept OtherDisk>
constexpr bool Point<Number, Label>::boundaryContains(const OtherDisk&) const {
    return false;
}

/**
 * @section predicates-segment Segment
 * Segment endpoint, boundary, containment, collinearity, intersection, and
 * topological predicates, including the generic `separates` / `crosses`
 * dispatch used against 1D and area targets.
 */

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Segment<PointType, LabelType>::boundaryContains(const OtherPoint& point) const {
    return verticesContain(point);
}


/**
 * @section predicates-triangle Triangle
 * Triangle boundary, containment, intersection, and cut predicates, including
 * triangle-vs-rectangle and triangle-vs-triangle topological cases.
 */

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Triangle<PointType, LabelType>::boundaryContains(const OtherPoint& point) const {
    const auto boundary = edges();
    return boundary[0].contains(point) || boundary[1].contains(point) || boundary[2].contains(point);
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Triangle<PointType, LabelType>::boundaryContains(const OtherSegment& other) const {
    return detail::polygonBoundaryContainsSegment(*this, other);
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Triangle<PointType, LabelType>::boundaryContains(const OtherOrientedSegment& other) const {
    return boundaryContains(static_cast<Segment<typename OtherOrientedSegment::PointType>>(other));
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Triangle<PointType, LabelType>::boundaryContains(const OtherLine& other) const {
    return other.isDegenerate() && boundaryContains(other.min());
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Triangle<PointType, LabelType>::boundaryContains(const OtherOrientedLine& other) const {
    return other.isDegenerate() && boundaryContains(other.source());
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool Triangle<PointType, LabelType>::boundaryContains(const OtherRay& other) const {
    return other.isDegenerate() && boundaryContains(other.source());
}

template <class PointType, class LabelType>
template<HalfplaneConcept OtherHalfplane>
constexpr bool Triangle<PointType, LabelType>::boundaryContains(const OtherHalfplane& other) const {
    return other.isDegenerate() && boundaryContains(other.source());
}

template <class PointType, class LabelType>
template<RectangleConcept OtherRectangle>
constexpr bool Triangle<PointType, LabelType>::boundaryContains(const OtherRectangle& other) const {
    if (!other.isDegenerate()) {
        return false;
    }
    if (other.min() == other.max()) {
        return boundaryContains(other.min());
    }
    return boundaryContains(Segment<typename OtherRectangle::PointType>(other.min(), other.max()));
}

template <class PointType, class LabelType>
template<TriangleConcept OtherTriangle>
constexpr bool Triangle<PointType, LabelType>::boundaryContains(const OtherTriangle& other) const {
    if (!other.isDegenerate()) {
        return false;
    }
    if (other.a() == other.c()) {
        return boundaryContains(other.a());
    }
    return boundaryContains(Segment<typename OtherTriangle::PointType>(other.a(), other.c()));
}

template <class PointType, class LabelType>
template<ConvexConcept OtherConvex>
constexpr bool Triangle<PointType, LabelType>::boundaryContains(const OtherConvex& other) const {
    if (other.size() == 0) {
        return true;
    }
    if (other.size() == 1) {
        return boundaryContains(other[0]);
    }
    if (other.size() == 2) {
        return boundaryContains(Segment<typename OtherConvex::PointType>(other[0], other[1]));
    }
    return false;
}

/**
 * @section predicates-oriented-segment OrientedSegment
 * Oriented-segment predicates. Most topology delegates to the unoriented
 * segment view, with local methods kept for orientation-sensitive behavior.
 */

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool OrientedSegment<PointType, LabelType>::boundaryContains(const OtherPoint& point) const {
    return verticesContain(point);
}

/**
 * @section predicates-line Line
 * Geometric line predicates: geometric equality/order, containment,
 * intersection against 1D and 2D shapes, and generic separation dispatch.
 */

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Line<PointType, LabelType>::boundaryContains(const OtherPoint& point) const {
    (void)point;
    return false;
}

/**
 * @section predicates-oriented-line OrientedLine
 * Oriented-line predicates. Shared topology is mostly delegated to the
 * unoriented line view, while orientation-specific methods stay local here.
 */

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool OrientedLine<PointType, LabelType>::boundaryContains(const OtherPoint& point) const {
    (void)point;
    return false;
}

/**
 * @section predicates-ray Ray
 * Ray-specific containment, intersection, and topological predicates. This is
 * where the asymmetric behavior of a half-infinite 1D primitive is implemented.
 */

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Ray<PointType, LabelType>::boundaryContains(const OtherPoint& point) const {
    return point == source();
}

/**
 * @section predicates-rectangle Rectangle
 * Axis-aligned rectangle predicates plus the rectangle-local clipping helpers
 * used to answer strict interior and separation questions.
 */


template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Rectangle<PointType, LabelType>::boundaryContains(const OtherPoint& point) const {
    if (!contains(point)) {
        return false;
    }
    return point.x() == min().x() ||
           point.x() == max().x() ||
           point.y() == min().y() ||
           point.y() == max().y();
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Rectangle<PointType, LabelType>::boundaryContains(const OtherSegment& other) const {
    return detail::polygonBoundaryContainsSegment(*this, other);
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Rectangle<PointType, LabelType>::boundaryContains(const OtherOrientedSegment& other) const {
    return boundaryContains(static_cast<Segment<typename OtherOrientedSegment::PointType>>(other));
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Rectangle<PointType, LabelType>::boundaryContains(const OtherLine& other) const {
    return other.isDegenerate() && boundaryContains(other.min());
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Rectangle<PointType, LabelType>::boundaryContains(const OtherOrientedLine& other) const {
    return other.isDegenerate() && boundaryContains(other.source());
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool Rectangle<PointType, LabelType>::boundaryContains(const OtherRay& other) const {
    return other.isDegenerate() && boundaryContains(other.source());
}

template <class PointType, class LabelType>
template<HalfplaneConcept OtherHalfplane>
constexpr bool Rectangle<PointType, LabelType>::boundaryContains(const OtherHalfplane& other) const {
    return other.isDegenerate() && boundaryContains(other.source());
}

template <class PointType, class LabelType>
template<RectangleConcept OtherRectangle>
constexpr bool Rectangle<PointType, LabelType>::boundaryContains(const OtherRectangle& other) const {
    if (!other.isDegenerate()) {
        return false;
    }
    if (other.min() == other.max()) {
        return boundaryContains(other.min());
    }
    return boundaryContains(Segment<typename OtherRectangle::PointType>(other.min(), other.max()));
}

template <class PointType, class LabelType>
template<TriangleConcept OtherTriangle>
constexpr bool Rectangle<PointType, LabelType>::boundaryContains(const OtherTriangle& other) const {
    if (!other.isDegenerate()) {
        return false;
    }
    if (other.a() == other.c()) {
        return boundaryContains(other.a());
    }
    return boundaryContains(Segment<typename OtherTriangle::PointType>(other.a(), other.c()));
}

template <class PointType, class LabelType>
template<ConvexConcept OtherConvex>
constexpr bool Rectangle<PointType, LabelType>::boundaryContains(const OtherConvex& other) const {
    if (other.size() == 0) {
        return true;
    }
    if (other.size() == 1) {
        return boundaryContains(other[0]);
    }
    if (other.size() == 2) {
        return boundaryContains(Segment<typename OtherConvex::PointType>(other[0], other[1]));
    }
    return false;
}

/**
 * @section predicates-halfplane Halfplane
 * Half-plane containment, intersection, and topological predicates, together
 * with the helper routines used for strict side/interior tests.
 */

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Halfplane<PointType, LabelType>::boundaryContains(const OtherPoint& point) const {
    if (isDegenerate()) {
        return point == source();
    }
    return pgl::collinear(source(), target(), point);
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Halfplane<PointType, LabelType>::boundaryContains(const OtherSegment& other) const {
    return boundaryContains(other.min()) && boundaryContains(other.max());
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Halfplane<PointType, LabelType>::boundaryContains(const OtherLine& other) const {
    return boundaryContains(other.min()) && boundaryContains(other.max());
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Halfplane<PointType, LabelType>::boundaryContains(const OtherOrientedSegment& other) const {
    return boundaryContains(other.source()) && boundaryContains(other.target());
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Halfplane<PointType, LabelType>::boundaryContains(const OtherOrientedLine& other) const {
    return boundaryContains(other.source()) && boundaryContains(other.target());
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool Halfplane<PointType, LabelType>::boundaryContains(const OtherRay& other) const {
    return boundaryContains(other.source()) && boundaryContains(other.target());
}

template <class PointType, class LabelType>
template<RectangleConcept OtherRectangle>
constexpr bool Halfplane<PointType, LabelType>::boundaryContains(const OtherRectangle& other) const {
    return other.isDegenerate() &&
           boundaryContains(other.min()) &&
           boundaryContains(other.max());
}

template <class PointType, class LabelType>
template<TriangleConcept OtherTriangle>
constexpr bool Halfplane<PointType, LabelType>::boundaryContains(const OtherTriangle& other) const {
    return other.isDegenerate() &&
           boundaryContains(other.a()) &&
           boundaryContains(other.b()) &&
           boundaryContains(other.c());
}

template <class PointType, class LabelType>
template<HalfplaneConcept OtherHalfplane>
constexpr bool Halfplane<PointType, LabelType>::boundaryContains(const OtherHalfplane& other) const {
    return other.isDegenerate() &&
           boundaryContains(other.source()) &&
           boundaryContains(other.target());
}

// -----------------------------------------------------------------------------
// Disk
//
// Every overload opens with the two degenerate readings. A disk of radius zero
// is the point a() — never a segment (doc/raw/shapes.md) — and a shape that has
// dropped below its natural dimension is entirely boundary, so boundaryContains
// coincides with contains there. Testing a() == b() alone settles it without
// ever reading c(): a() == b() == c() is that radius-zero disk, and a() == b()
// with c() elsewhere is undefined, so answering as if it were the point a() is
// one of the answers the contract allows. A disk whose three defining points
// are collinear but distinct determines no circle and is likewise undefined;
// reading it as the line through them is another terminating answer. That line
// is degenerate when a() == c(), and then holds every point of the plane —
// still an answer, still terminating, and only ever reached on undefined input.

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Disk<PointType, LabelType>::boundaryContains(const OtherPoint& point) const {
    if (a() == b()) {
        return contains(point);
    }
    if (isDegenerate()) {
        return Line<PointType>(a(), c()).contains(point);
    }

    return inCircleSign(a(), b(), c(), point) == std::partial_ordering::equivalent;
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Disk<PointType, LabelType>::boundaryContains(const OtherSegment& other) const {
    if (a() == b()) {
        return contains(other);
    }
    if (isDegenerate()) {
        return Line<PointType>(a(), c()).contains(other);
    }
    return other.isDegenerate() && boundaryContains(other.min());
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Disk<PointType, LabelType>::boundaryContains(const OtherOrientedSegment& other) const {
    if (a() == b()) {
        return contains(other);
    }
    if (isDegenerate()) {
        return Line<PointType>(a(), c()).contains(other);
    }
    return other.isDegenerate() && boundaryContains(other.source());
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Disk<PointType, LabelType>::boundaryContains(const OtherLine& other) const {
    if (a() == b()) {
        return contains(other);
    }
    if (isDegenerate()) {
        return Line<PointType>(a(), c()).contains(other);
    }
    return other.isDegenerate() && boundaryContains(other.min());
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Disk<PointType, LabelType>::boundaryContains(const OtherOrientedLine& other) const {
    if (a() == b()) {
        return contains(other);
    }
    if (isDegenerate()) {
        return Line<PointType>(a(), c()).contains(other);
    }
    return other.isDegenerate() && boundaryContains(other.source());
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool Disk<PointType, LabelType>::boundaryContains(const OtherRay& other) const {
    if (a() == b()) {
        return contains(other);
    }
    if (isDegenerate()) {
        return Line<PointType>(a(), c()).contains(other);
    }
    return other.isDegenerate() && boundaryContains(other.source());
}

template <class PointType, class LabelType>
template<HalfplaneConcept OtherHalfplane>
constexpr bool Disk<PointType, LabelType>::boundaryContains(const OtherHalfplane& other) const {
    if (a() == b()) {
        return contains(other);
    }
    if (isDegenerate()) {
        return Line<PointType>(a(), c()).contains(other);
    }
    return other.isDegenerate() && boundaryContains(other.source());
}

template <class PointType, class LabelType>
template<TriangleConcept OtherTriangle>
constexpr bool Disk<PointType, LabelType>::boundaryContains(const OtherTriangle& other) const {
    if (a() == b()) {
        return contains(other);
    }
    if (isDegenerate()) {
        return Line<PointType>(a(), c()).contains(other);
    }

    const bool is_point = other.a() == other.b() && other.a() == other.c();
    return is_point && boundaryContains(other.a());
}

template <class PointType, class LabelType>
template<RectangleConcept OtherRectangle>
constexpr bool Disk<PointType, LabelType>::boundaryContains(const OtherRectangle& other) const {
    if (a() == b()) {
        return contains(other);
    }
    if (isDegenerate()) {
        return Line<PointType>(a(), c()).contains(other);
    }

    const bool is_point = other.min() == other.max();
    return is_point && boundaryContains(other.min());
}

template <class PointType, class LabelType>
template<ConvexConcept OtherConvex>
constexpr bool Disk<PointType, LabelType>::boundaryContains(const OtherConvex& other) const {
    if (a() == b()) {
        return contains(other);
    }
    if (isDegenerate()) {
        return Line<PointType>(a(), c()).contains(other);
    }

    return other.size() == 1 && boundaryContains(other[0]);
}

template <class PointType, class LabelType>
template<DiskConcept OtherDisk>
constexpr bool Disk<PointType, LabelType>::boundaryContains(const OtherDisk& other) const {
    // A collapsed disk is the point a(), never a segment; its boundary circle
    // has shrunk to that same point.
    if (other.a() == other.b()) {
        return boundaryContains(other.a());
    }
    if (isDegenerate()) {
        return false;
    }

    return boundaryContains(other.a()) && boundaryContains(other.b()) && boundaryContains(other.c());
}

template <class PointType, class LabelType>
constexpr bool Disk<PointType, LabelType>::boundaryContains(const Shape<PointType>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->boundaryContains(value);
        },
        other.variant());
}


// ---------------------------------------------------------------------------
// Convex

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Convex<PointType, LabelType>::boundaryContains(const OtherPoint& point) const {
    if (points_.empty()) {
        return false;
    }
    if (!bbox().contains(point)) {
        return false;
    }

    using CommonNumberType = std::common_type_t<NumberType, typename OtherPoint::NumberType>;
    Point<CommonNumberType> translatedPoint = static_cast<Point<CommonNumberType>>(point) -
                                              static_cast<Point<CommonNumberType>>(translation_);

    if (points_.size() == 1) {
        return translatedPoint == points_[0];
    }
    if (points_.size() == 2) {
        return Segment<PointType>(points_[0], points_[1]).contains(translatedPoint);
    }
    if (points_.size() == 3) {
        return Segment<PointType>(points_[0], points_[1]).contains(translatedPoint) ||
               Segment<PointType>(points_[1], points_[2]).contains(translatedPoint) ||
               Segment<PointType>(points_[2], points_[0]).contains(translatedPoint);
    }

    const size_t max_i = maxIndex();

    auto o = orientationSign(points_[0], points_[max_i], translatedPoint);

    if (o < 0) {
        auto it_end = points_.begin() + max_i + 1;
        auto it = std::lower_bound(points_.begin(), it_end, translatedPoint, lexLessCrossType);
        if (it == it_end) {
            return false;
        }
        if (it == points_.begin()) {
            return *it == translatedPoint;
        }

        return Segment<PointType>(*(it - 1), *it).contains(translatedPoint);
    }

    if (o > 0) {
        auto it_begin = std::make_reverse_iterator(points_.end());
        auto it_end = std::make_reverse_iterator(points_.begin() + max_i);

        auto it = std::lower_bound(it_begin, it_end, translatedPoint, lexLessCrossType);

        if (it == it_end) {
            return false;
        }
        if (it == it_begin) { // Check the edge between the first and last vertices
            return Segment<PointType>(*it, points_[0]).contains(translatedPoint);
        }   

        return Segment<PointType>(*(it - 1), *it).contains(translatedPoint);
    }

    // o == 0: the point is collinear with the two x-extreme vertices. That
    // line is an actual boundary edge only when one hull degenerates to the
    // single edge v[0]-v[max_i] (the extremes are cyclically adjacent);
    // otherwise it is an interior diagonal and only the endpoints qualify.
    if (max_i == 1 || max_i + 1 == points_.size()) {
        return Segment<PointType>(points_[0], points_[max_i]).contains(translatedPoint);
    }
    return translatedPoint == points_[0] || translatedPoint == points_[max_i];
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Convex<PointType, LabelType>::boundaryContains(const OtherSegment& other) const {
    if (other.isDegenerate()) {
        return boundaryContains(other.min());
    }
    if (isDegenerate()) {
        // A hull with empty interior is entirely boundary, so boundary
        // containment coincides with containment.
        return contains(other);
    }
    for (const auto &edgePair : {edgesAtX(other.min().x()), edgesAtX(other.max().x())}) {
        if (!edgePair) {
            return false;
        }
        if (other.isVertical()) {
            Segment<PointType> edge1(get(0),get(-1));
            if (edge1.contains(other)) {
                return true;
            }

            ptrdiff_t i = maxIndex();
            Segment<PointType> edge2(get(i),get(i-1));
            if (edge2.contains(other)) {
                return true;
            }
            return false;
        }
        for (const auto &edge : *edgePair) {
            if (edge.contains(other)) {
                return true;
            }            
        }
    }
    return false;
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Convex<PointType, LabelType>::boundaryContains(const OtherOrientedSegment& other) const {
    return boundaryContains(static_cast<Segment<typename OtherOrientedSegment::PointType>>(other));
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Convex<PointType, LabelType>::boundaryContains(const OtherLine& other) const {
    return other.isDegenerate() && boundaryContains(other.min());
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Convex<PointType, LabelType>::boundaryContains(const OtherOrientedLine& other) const {
    return other.isDegenerate() && boundaryContains(other.source());
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool Convex<PointType, LabelType>::boundaryContains(const OtherRay& other) const {
    return other.isDegenerate() && boundaryContains(other.source());
}

template <class PointType, class LabelType>
template<HalfplaneConcept OtherHalfplane>
constexpr bool Convex<PointType, LabelType>::boundaryContains(const OtherHalfplane& other) const {
    return other.isDegenerate() && boundaryContains(other.source());
}

template <class PointType, class LabelType>
template<RectangleConcept OtherRectangle>
constexpr bool Convex<PointType, LabelType>::boundaryContains(const OtherRectangle& other) const {
    if (!other.isDegenerate()) {
        return false;
    }
    if (other.min() == other.max()) {
        return boundaryContains(other.min());
    }
    return boundaryContains(Segment<typename OtherRectangle::PointType>(other.min(), other.max()));
}

template <class PointType, class LabelType>
template<TriangleConcept OtherTriangle>
constexpr bool Convex<PointType, LabelType>::boundaryContains(const OtherTriangle& other) const {
    if (!other.isDegenerate()) {
        return false;
    }
    if (other.a() == other.c()) {
        return boundaryContains(other.a());
    }
    return boundaryContains(Segment<typename OtherTriangle::PointType>(other.a(), other.c()));
}

template <class PointType, class LabelType>
template<ConvexConcept OtherConvex>
constexpr bool Convex<PointType, LabelType>::boundaryContains(const OtherConvex& other) const {
    if (other.size() == 0) {
        return true;
    }
    if (other.size() == 1) {
        return boundaryContains(other[0]);
    }
    if (other.size() == 2) {
        return boundaryContains(Segment<typename OtherConvex::PointType>(other[0], other[1]));
    }
    return false;
}

template <class PointType, class LabelType>
template<DiskConcept OtherDisk>
constexpr bool Convex<PointType, LabelType>::boundaryContains(const OtherDisk& other) const {
    if (other[0] == other[1] && other[0] == other[2]) {
        return boundaryContains(other[0]);
    }
    return false;
}

template <class PointType, class LabelType>
template <PointConcept OtherPoint>
constexpr bool Convex<PointType, LabelType>::boundaryContains(const Shape<OtherPoint>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->boundaryContains(value);
        },
        other.variant());
}


// ---------------------------------------------------------------------------
// Polygon

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Polygon<PointType, LabelType>::boundaryContains(const OtherPoint& point) const {
    const std::size_t n = size();
    if (n == 0) {
        return false;
    }
    // Containment is translation-invariant, so test against the raw points_ in
    // the polygon's untranslated frame (cf. contains). A single-vertex polygon
    // is handled by its degenerate edge (a point), so no special case is needed.
    const auto p = point - translation_;
    for (std::size_t i = 0; i < n; ++i) {
        if (Segment<PointType>(points_[i], points_[(i + 1) % n]).contains(p)) {
            return true;
        }
    }
    return false;
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Polygon<PointType, LabelType>::boundaryContains(const OtherSegment& other) const {
    if (other.isDegenerate()) {
        return boundaryContains(other.min());
    }
    // A straight segment on the boundary of a simple polygon (one without
    // straight-angle vertices) lies within a single edge, mirroring the
    // single-edge containment used by Convex::boundaryContains.
    for (const auto& edge : edgesView()) {
        if (edge.contains(other)) {
            return true;
        }
    }
    return false;
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Polygon<PointType, LabelType>::boundaryContains(const OtherOrientedSegment& other) const {
    return boundaryContains(static_cast<Segment<typename OtherOrientedSegment::PointType>>(other));
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Polygon<PointType, LabelType>::boundaryContains(const OtherLine& other) const {
    return other.isDegenerate() && boundaryContains(other.min());
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Polygon<PointType, LabelType>::boundaryContains(const OtherOrientedLine& other) const {
    return other.isDegenerate() && boundaryContains(other.source());
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool Polygon<PointType, LabelType>::boundaryContains(const OtherRay& other) const {
    return other.isDegenerate() && boundaryContains(other.source());
}

template <class PointType, class LabelType>
template<HalfplaneConcept OtherHalfplane>
constexpr bool Polygon<PointType, LabelType>::boundaryContains(const OtherHalfplane& other) const {
    return other.isDegenerate() && boundaryContains(other.source());
}

template <class PointType, class LabelType>
template<RectangleConcept OtherRectangle>
constexpr bool Polygon<PointType, LabelType>::boundaryContains(const OtherRectangle& other) const {
    if (!other.isDegenerate()) {
        return false;
    }
    if (other.min() == other.max()) {
        return boundaryContains(other.min());
    }
    return boundaryContains(Segment<typename OtherRectangle::PointType>(other.min(), other.max()));
}

template <class PointType, class LabelType>
template<TriangleConcept OtherTriangle>
constexpr bool Polygon<PointType, LabelType>::boundaryContains(const OtherTriangle& other) const {
    if (!other.isDegenerate()) {
        return false;
    }
    if (other.a() == other.c()) {
        return boundaryContains(other.a());
    }
    return boundaryContains(Segment<typename OtherTriangle::PointType>(other.a(), other.c()));
}

template <class PointType, class LabelType>
template<ConvexConcept OtherConvex>
constexpr bool Polygon<PointType, LabelType>::boundaryContains(const OtherConvex& other) const {
    if (other.size() == 0) {
        return true;
    }
    if (other.size() == 1) {
        return boundaryContains(other[0]);
    }
    if (other.size() == 2) {
        return boundaryContains(Segment<typename OtherConvex::PointType>(other[0], other[1]));
    }
    return false;
}

template <class PointType, class LabelType>
template<PolygonConcept OtherPolygon>
constexpr bool Polygon<PointType, LabelType>::boundaryContains(const OtherPolygon& other) const {
    if (other.size() == 0) {
        return true;
    }
    if (other.size() == 1) {
        return boundaryContains(other[0]);
    }
    if (other.size() == 2) {
        return boundaryContains(Segment<typename OtherPolygon::PointType>(other[0], other[1]));
    }
    return false;
}

template <class PointType, class LabelType>
template<DiskConcept OtherDisk>
constexpr bool Polygon<PointType, LabelType>::boundaryContains(const OtherDisk& other) const {
    if (other[0] == other[1] && other[0] == other[2]) {
        return boundaryContains(other[0]);
    }
    return false;
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Polygon<PointType, LabelType>::boundaryContains(const Shape<OtherPoint>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->boundaryContains(value);
        },
        other.variant());
}

template <class PointType, class LabelType>
template<PolygonConcept OtherPolygon>
constexpr bool Convex<PointType, LabelType>::boundaryContains(const OtherPolygon& other) const {
    if (other.size() == 0) {
        return true;
    }
    if (other.size() == 1) {
        return boundaryContains(other[0]);
    }
    if (other.size() == 2) {
        return boundaryContains(Segment<typename OtherPolygon::PointType>(other[0], other[1]));
    }
    return false;
}

template <class PointType, class LabelType>
template<PolygonConcept OtherPolygon>
constexpr bool Disk<PointType, LabelType>::boundaryContains(const OtherPolygon& other) const {
    if (a() == b()) {
        return contains(other);
    }
    if (isDegenerate()) {
        return Line<PointType>(a(), c()).contains(other);
    }
    return other.size() == 1 && boundaryContains(other[0]);
}


// --- asymmetric Disk/Polygon boundary containment ---
//
// A triangle and a rectangle share the boundary of their convex-polygon view,
// so they defer to it. The Convex implementation captures the only ways a 2D
// shape can lie on a 1D boundary: a disk degenerated to a single boundary point,
// or a polygon of zero/one/two vertices reducing to a point or boundary segment.

template <class PointType, class LabelType>
template<DiskConcept OtherDisk>
constexpr bool Triangle<PointType, LabelType>::boundaryContains(const OtherDisk& other) const {
    return other[0] == other[1] && other[1]==other[2] && boundaryContains(other[0]);
}

template <class PointType, class LabelType>
template<PolygonConcept OtherPolygon>
constexpr bool Triangle<PointType, LabelType>::boundaryContains(const OtherPolygon& other) const {
    return asConvex().boundaryContains(other);
}

template <class PointType, class LabelType>
template<PolygonConcept OtherPolygon>
constexpr bool Rectangle<PointType, LabelType>::boundaryContains(const OtherPolygon& other) const {
    return asConvex().boundaryContains(other);
}

template <class PointType, class LabelType>
template<DiskConcept OtherDisk>
constexpr bool Rectangle<PointType, LabelType>::boundaryContains(const OtherDisk& other) const {
    return other[0] == other[1] && other[1]==other[2] && boundaryContains(other[0]);
}

// ---------------------------------------------------------------------------
// boundaryContains(Shape): runtime dispatch over the wrapped alternative, for
// the shapes that did not previously expose a Shape overload.
// ---------------------------------------------------------------------------

template <class Number, class Label>
constexpr bool Point<Number, Label>::boundaryContains(const Shape<Point<Number, Label>>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->boundaryContains(value);
        },
        other.variant());
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Segment<PointType, LabelType>::boundaryContains(const Shape<OtherPoint>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->boundaryContains(value);
        },
        other.variant());
}

template <class PointType, class LabelType>
constexpr bool OrientedSegment<PointType, LabelType>::boundaryContains(const Shape<PointType>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->boundaryContains(value);
        },
        other.variant());
}

template <class PointType, class LabelType>
constexpr bool Line<PointType, LabelType>::boundaryContains(const Shape<PointType>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->boundaryContains(value);
        },
        other.variant());
}

template <class PointType, class LabelType>
constexpr bool OrientedLine<PointType, LabelType>::boundaryContains(const Shape<PointType>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->boundaryContains(value);
        },
        other.variant());
}

template <class PointType, class LabelType>
constexpr bool Ray<PointType, LabelType>::boundaryContains(const Shape<PointType>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->boundaryContains(value);
        },
        other.variant());
}

template <class PointType, class LabelType>
constexpr bool Halfplane<PointType, LabelType>::boundaryContains(const Shape<PointType>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->boundaryContains(value);
        },
        other.variant());
}

template <class PointType, class LabelType>
constexpr bool Rectangle<PointType, LabelType>::boundaryContains(const Shape<PointType>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->boundaryContains(value);
        },
        other.variant());
}

template <class PointType, class LabelType>
constexpr bool Triangle<PointType, LabelType>::boundaryContains(const Shape<PointType>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->boundaryContains(value);
        },
        other.variant());
}

/**
 * @section predicates-monotonechain MonotoneChain
 * Weakly x-monotone chain predicates: the boundary of a chain is its two
 * extreme vertices, matching the endpoint convention of Segment.
 */

template <class PointType, class LabelType, class Storage>
template<PointConcept OtherPoint>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::boundaryContains(const OtherPoint& point) const {
    if (points_.empty()) {
        return false;
    }
    return point == points_.front() + translation_ || point == points_.back() + translation_;
}

template <class PointType, class LabelType, class Storage>
template<PointConcept OtherPoint>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::boundaryContains(const Shape<OtherPoint>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->boundaryContains(value);
        },
        other.variant());
}

template <class Number, class Label>
template<MonotoneChainConcept OtherChain>
constexpr bool Point<Number, Label>::boundaryContains(const OtherChain&) const {
    return false;
}

template <class PointType, class LabelType>
template<MonotoneChainConcept OtherChain>
constexpr bool Halfplane<PointType, LabelType>::boundaryContains(const OtherChain& other) const {
    // The boundary is a line (a convex set), so it contains the chain iff it
    // contains every vertex.
    for (const auto& vertex : other) {
        if (!boundaryContains(vertex)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<MonotoneChainConcept OtherChain>
constexpr bool Rectangle<PointType, LabelType>::boundaryContains(const OtherChain& other) const {
    return asConvex().boundaryContains(other);
}

template <class PointType, class LabelType>
template<MonotoneChainConcept OtherChain>
constexpr bool Triangle<PointType, LabelType>::boundaryContains(const OtherChain& other) const {
    return asConvex().boundaryContains(other);
}

template <class PointType, class LabelType>
template<MonotoneChainConcept OtherChain>
constexpr bool Disk<PointType, LabelType>::boundaryContains(const OtherChain& other) const {
    if (a() == b()) {
        return contains(other);
    }
    if (isDegenerate()) {
        return Line<PointType>(a(), c()).contains(other);
    }
    // Chain edges are straight, so only a single vertex can lie on the circle.
    return other.empty() || (other.size() == 1 && boundaryContains(other[0]));
}

template <class PointType, class LabelType>
template<MonotoneChainConcept OtherChain>
constexpr bool Convex<PointType, LabelType>::boundaryContains(const OtherChain& other) const {
    // A chain may run along the convex boundary through many collinear
    // vertices, so fold the edges rather than counting vertices.
    if (other.empty()) {
        return true;
    }
    if (other.size() == 1) {
        return boundaryContains(other[0]);
    }
    for (std::size_t i = 0; i + 1 < other.size(); ++i) {
        if (!boundaryContains(Segment<typename OtherChain::PointType>(other[i], other[i + 1]))) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<MonotoneChainConcept OtherChain>
constexpr bool Polygon<PointType, LabelType>::boundaryContains(const OtherChain& other) const {
    if (other.empty()) {
        return true;
    }
    if (other.size() == 1) {
        return boundaryContains(other[0]);
    }
    for (std::size_t i = 0; i + 1 < other.size(); ++i) {
        if (!boundaryContains(Segment<typename OtherChain::PointType>(other[i], other[i + 1]))) {
            return false;
        }
    }
    return true;
}

/**
 * @section predicates-polyline Polyline
 * Open polygonal chain predicates: the boundary of a polyline is its two
 * extreme vertices, matching the endpoint convention of Segment.
 */

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Polyline<PointType, LabelType>::boundaryContains(const OtherPoint& point) const {
    if (points_.empty()) {
        return false;
    }
    return point == points_.front() + translation_ || point == points_.back() + translation_;
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Polyline<PointType, LabelType>::boundaryContains(const Shape<OtherPoint>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->boundaryContains(value);
        },
        other.variant());
}

template <class Number, class Label>
template<PolylineConcept OtherPolyline>
constexpr bool Point<Number, Label>::boundaryContains(const OtherPolyline&) const {
    return false;
}

template <class PointType, class LabelType>
template<PolylineConcept OtherPolyline>
constexpr bool Halfplane<PointType, LabelType>::boundaryContains(const OtherPolyline& other) const {
    // The boundary is a line (a convex set), so it contains the polyline iff
    // it contains every vertex.
    for (const auto& vertex : other) {
        if (!boundaryContains(vertex)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<PolylineConcept OtherPolyline>
constexpr bool Rectangle<PointType, LabelType>::boundaryContains(const OtherPolyline& other) const {
    return asConvex().boundaryContains(other);
}

template <class PointType, class LabelType>
template<PolylineConcept OtherPolyline>
constexpr bool Triangle<PointType, LabelType>::boundaryContains(const OtherPolyline& other) const {
    return asConvex().boundaryContains(other);
}

template <class PointType, class LabelType>
template<PolylineConcept OtherPolyline>
constexpr bool Disk<PointType, LabelType>::boundaryContains(const OtherPolyline& other) const {
    if (a() == b()) {
        return contains(other);
    }
    if (isDegenerate()) {
        return Line<PointType>(a(), c()).contains(other);
    }
    // Polyline edges are straight, so only a polyline covering a single point
    // can lie on the circle.
    return other.empty() || (other.isDegenerate() && boundaryContains(other[0]));
}

template <class PointType, class LabelType>
template<PolylineConcept OtherPolyline>
constexpr bool Convex<PointType, LabelType>::boundaryContains(const OtherPolyline& other) const {
    // A polyline may run along the convex boundary through many collinear
    // vertices, so fold the edges rather than counting vertices.
    if (other.empty()) {
        return true;
    }
    if (other.size() == 1) {
        return boundaryContains(other[0]);
    }
    for (std::size_t i = 0; i + 1 < other.size(); ++i) {
        if (!boundaryContains(Segment<typename OtherPolyline::PointType>(other[i], other[i + 1]))) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<PolylineConcept OtherPolyline>
constexpr bool Polygon<PointType, LabelType>::boundaryContains(const OtherPolyline& other) const {
    if (other.empty()) {
        return true;
    }
    if (other.size() == 1) {
        return boundaryContains(other[0]);
    }
    for (std::size_t i = 0; i + 1 < other.size(); ++i) {
        if (!boundaryContains(Segment<typename OtherPolyline::PointType>(other[i], other[i + 1]))) {
            return false;
        }
    }
    return true;
}


// ---------------------------------------------------------------------------
// HalfplaneIntersection

template <class PointType, class LabelType>
template <PointConcept OtherPoint>
constexpr bool HalfplaneIntersection<PointType, LabelType>::boundaryContains(const OtherPoint& point) const {
    // A degenerate region equals its own boundary, and every point of a
    // degenerate region lies on some constraint boundary, so the status test
    // covers that case with no special handling.
    return pointStatus(point) == 0;
}

template <class PointType, class LabelType>
template <SegmentConcept OtherSegment>
constexpr bool HalfplaneIntersection<PointType, LabelType>::boundaryContains(const OtherSegment& other) const {
    if (isEmpty()) {
        return false;
    }
    // A segment on the boundary lies on a single constraint's boundary line
    // (at most two stored constraints are parallel to it), inside the region.
    // A degenerate region is its own boundary, and it always stores its
    // carrier line's constraints, so the same test applies.
    const Halfplane<typename OtherSegment::PointType> along(other.min(), other.max());
    if (along.isDegenerate()) {
        return boundaryContains(other.min());
    }
    for (const std::ptrdiff_t idx : {sameDirectionIndex(along), sameDirectionIndex(along.opposite())}) {
        if (idx >= 0 && constraintSide(static_cast<std::size_t>(idx), other.min()) == 0 &&
            constraintSide(static_cast<std::size_t>(idx), other.max()) == 0) {
            return contains(other.min()) && contains(other.max());
        }
    }
    return false;
}

template <class PointType, class LabelType>
template <OrientedSegmentConcept OtherOrientedSegment>
constexpr bool HalfplaneIntersection<PointType, LabelType>::boundaryContains(const OtherOrientedSegment& other) const {
    return boundaryContains(Segment<typename OtherOrientedSegment::PointType>(other[0], other[1]));
}

template <class PointType, class LabelType>
template <LineConcept OtherLine>
constexpr bool HalfplaneIntersection<PointType, LabelType>::boundaryContains(const OtherLine& other) const {
    // A full line on the boundary must be a constraint's entire boundary
    // line, and the region must contain it (so no other constraint clips it).
    if (isEmpty() || !contains(other)) {
        return false;
    }
    for (const auto& halfplane : halfplanes_) {
        if (halfplane.boundaryContains(other)) {
            return true;
        }
    }
    return false;
}

template <class PointType, class LabelType>
template <OrientedLineConcept OtherOrientedLine>
constexpr bool HalfplaneIntersection<PointType, LabelType>::boundaryContains(const OtherOrientedLine& other) const {
    return boundaryContains(other.asLine());
}

template <class PointType, class LabelType>
template <RayConcept OtherRay>
constexpr bool HalfplaneIntersection<PointType, LabelType>::boundaryContains(const OtherRay& other) const {
    // The ray must lie inside the region and on some constraint's boundary
    // line; a degenerate region containing the ray stores its carrier line's
    // constraints, so the boundary-line scan below finds it there too.
    if (isEmpty() || !contains(other)) {
        return false;
    }
    const Halfplane<typename OtherRay::PointType> along(other.source(), other.target());
    for (const std::ptrdiff_t idx : {sameDirectionIndex(along), sameDirectionIndex(along.opposite())}) {
        if (idx >= 0 && halfplanes_[static_cast<std::size_t>(idx)].boundaryContains(other)) {
            return true;
        }
    }
    return false;
}

template <class PointType, class LabelType>
template <HalfplaneConcept OtherHalfplane>
constexpr bool HalfplaneIntersection<PointType, LabelType>::boundaryContains(const OtherHalfplane&) const {
    // The boundary of the region is at most one-dimensional (and empty for
    // the whole plane), so it never contains a two-dimensional half-plane.
    return false;
}

template <class PointType, class LabelType>
template <RectangleConcept OtherRectangle>
constexpr bool HalfplaneIntersection<PointType, LabelType>::boundaryContains(const OtherRectangle& other) const {
    // Only a degenerate rectangle — the segment between its corners — can lie
    // on the at most one-dimensional boundary.
    if (!other.isDegenerate()) {
        return false;
    }
    return boundaryContains(Segment<typename OtherRectangle::PointType>(other.min(), other.max()));
}

template <class PointType, class LabelType>
template <TriangleConcept OtherTriangle>
constexpr bool HalfplaneIntersection<PointType, LabelType>::boundaryContains(const OtherTriangle&) const {
    // A (non-degenerate) triangle is two-dimensional and the boundary of the
    // region is at most one-dimensional.
    return false;
}

template <class PointType, class LabelType>
template <DiskConcept OtherDisk>
constexpr bool HalfplaneIntersection<PointType, LabelType>::boundaryContains(const OtherDisk& other) const {
    // Only a degenerate disk — a single point — can lie on the boundary.
    if (!other.isDegenerate()) {
        return false;
    }
    return boundaryContains(other.template center<typename OtherDisk::NumberType>());
}

template <class PointType, class LabelType>
template <ConvexConcept OtherConvex>
constexpr bool HalfplaneIntersection<PointType, LabelType>::boundaryContains(const OtherConvex& other) const {
    // Only the empty polygon and a degenerate one — a point or the segment
    // between its extremes — can lie on the at most one-dimensional boundary.
    if (other.size() == 0) {
        return true;
    }
    if (!other.isDegenerate()) {
        return false;
    }
    if (other.size() == 1) {
        return boundaryContains(other[0]);
    }
    return boundaryContains(Segment<typename OtherConvex::PointType>(other[0], other[other.size() - 1]));
}

template <class PointType, class LabelType>
template <MonotoneChainConcept OtherChain>
constexpr bool HalfplaneIntersection<PointType, LabelType>::boundaryContains(const OtherChain& other) const {
    if (other.size() == 0) {
        return true;
    }
    if (other.size() == 1) {
        return boundaryContains(other[0]);
    }
    for (const auto& edge : other.edgesView()) {
        if (!boundaryContains(edge)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template <PolylineConcept OtherPolyline>
constexpr bool HalfplaneIntersection<PointType, LabelType>::boundaryContains(const OtherPolyline& other) const {
    if (other.size() == 0) {
        return true;
    }
    if (other.size() == 1) {
        return boundaryContains(other[0]);
    }
    for (const auto& edge : other.edgesView()) {
        if (!boundaryContains(edge)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template <PolygonConcept OtherPolygon>
constexpr bool HalfplaneIntersection<PointType, LabelType>::boundaryContains(const OtherPolygon& other) const {
    // Only the empty polygon and a degenerate (zero-area) one — the union of
    // its edges — can lie on the at most one-dimensional boundary.
    if (other.size() == 0) {
        return true;
    }
    if (other.size() == 1) {
        return boundaryContains(other[0]);
    }
    if (!other.isDegenerate()) {
        return false;
    }
    for (const auto& edge : other.edgesView()) {
        if (!boundaryContains(edge)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template <HalfplaneIntersectionConcept OtherRegion>
constexpr bool HalfplaneIntersection<PointType, LabelType>::boundaryContains(const OtherRegion& other) const {
    // The empty region lies on every boundary; a full-dimensional region
    // never fits in the at most one-dimensional boundary; a degenerate region
    // reduces to its carrier shape.
    if (other.isEmpty()) {
        return true;
    }
    if (!other.isDegenerate()) {
        return false;
    }
    return std::visit([this](const auto& carrier) { return this->boundaryContains(carrier); },
                      detail::degenerateRegionCarrier(other));
}

template <class PointType, class LabelType>
template <PointConcept OtherPoint>
constexpr bool HalfplaneIntersection<PointType, LabelType>::boundaryContains(const Shape<OtherPoint>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->boundaryContains(value);
        },
        other.variant());
}


// ---------------------------------------------------------------------------
// Reverse direction: lower-ranked shapes' boundaries containing a
// HalfplaneIntersection.
//
// The empty region is a subset of every boundary; a full-dimensional region
// is never contained in an at most one-dimensional boundary; a degenerate
// region reduces to its carrier shape.

namespace detail {

// Dispatches boundaryContains(carrier) over the degenerate region's carrier,
// with per-alternative availability: a bounded shape's boundary never
// contains a ray or a line, so those alternatives short-circuit to false
// unless the shape declares the corresponding overload.
template <class Shape2, class Region>
constexpr bool boundaryContainsDegenerateRegion(const Shape2& shape, const Region& region) {
    return std::visit(
        [&shape](const auto& carrier) {
            if constexpr (requires { shape.boundaryContains(carrier); }) {
                return shape.boundaryContains(carrier);
            } else {
                (void)carrier;
                return false;  // no overload: geometrically impossible containment
            }
        },
        degenerateRegionCarrier(region));
}

}  // namespace detail

template <class Number, class Label>
template <HalfplaneIntersectionConcept OtherRegion>
constexpr bool Point<Number, Label>::boundaryContains(const OtherRegion& other) const {
    if (other.isEmpty()) {
        return true;
    }
    if (!other.isDegenerate()) {
        return false;
    }
    return detail::boundaryContainsDegenerateRegion(*this, other);
}

template <class PointType, class LabelType>
template <HalfplaneIntersectionConcept OtherRegion>
constexpr bool Segment<PointType, LabelType>::boundaryContains(const OtherRegion& other) const {
    if (other.isEmpty()) {
        return true;
    }
    if (!other.isDegenerate()) {
        return false;
    }
    return detail::boundaryContainsDegenerateRegion(*this, other);
}

template <class PointType, class LabelType>
template <HalfplaneIntersectionConcept OtherRegion>
constexpr bool OrientedSegment<PointType, LabelType>::boundaryContains(const OtherRegion& other) const {
    return asSegment().boundaryContains(other);
}

template <class PointType, class LabelType>
template <HalfplaneIntersectionConcept OtherRegion>
constexpr bool Line<PointType, LabelType>::boundaryContains(const OtherRegion& other) const {
    if (other.isEmpty()) {
        return true;
    }
    if (!other.isDegenerate()) {
        return false;
    }
    return detail::boundaryContainsDegenerateRegion(*this, other);
}

template <class PointType, class LabelType>
template <HalfplaneIntersectionConcept OtherRegion>
constexpr bool OrientedLine<PointType, LabelType>::boundaryContains(const OtherRegion& other) const {
    return asLine().boundaryContains(other);
}

template <class PointType, class LabelType>
template <HalfplaneIntersectionConcept OtherRegion>
constexpr bool Ray<PointType, LabelType>::boundaryContains(const OtherRegion& other) const {
    if (other.isEmpty()) {
        return true;
    }
    if (!other.isDegenerate()) {
        return false;
    }
    return detail::boundaryContainsDegenerateRegion(*this, other);
}

template <class PointType, class LabelType>
template <HalfplaneIntersectionConcept OtherRegion>
constexpr bool Halfplane<PointType, LabelType>::boundaryContains(const OtherRegion& other) const {
    if (other.isEmpty()) {
        return true;
    }
    if (!other.isDegenerate()) {
        return false;
    }
    return detail::boundaryContainsDegenerateRegion(*this, other);
}

template <class PointType, class LabelType>
template <HalfplaneIntersectionConcept OtherRegion>
constexpr bool Rectangle<PointType, LabelType>::boundaryContains(const OtherRegion& other) const {
    if (other.isEmpty()) {
        return true;
    }
    if (!other.isDegenerate()) {
        return false;
    }
    return detail::boundaryContainsDegenerateRegion(*this, other);
}

template <class PointType, class LabelType>
template <HalfplaneIntersectionConcept OtherRegion>
constexpr bool Triangle<PointType, LabelType>::boundaryContains(const OtherRegion& other) const {
    if (other.isEmpty()) {
        return true;
    }
    if (!other.isDegenerate()) {
        return false;
    }
    return detail::boundaryContainsDegenerateRegion(*this, other);
}

template <class PointType, class LabelType>
template <HalfplaneIntersectionConcept OtherRegion>
constexpr bool Disk<PointType, LabelType>::boundaryContains(const OtherRegion& other) const {
    if (other.isEmpty()) {
        return true;
    }
    if (!other.isDegenerate()) {
        return false;
    }
    // The circle contains no straight piece of positive length, so only a
    // point-carrier region can lie on it.
    return std::visit(
        [this](const auto& carrier) {
            using Carrier = std::remove_cvref_t<decltype(carrier)>;
            if constexpr (detail::is_point_v<Carrier>) {
                return this->boundaryContains(carrier);
            } else {
                (void)carrier;
                return false;
            }
        },
        detail::degenerateRegionCarrier(other));
}

template <class PointType, class LabelType>
template <HalfplaneIntersectionConcept OtherRegion>
constexpr bool Convex<PointType, LabelType>::boundaryContains(const OtherRegion& other) const {
    if (other.isEmpty()) {
        return true;
    }
    if (!other.isDegenerate()) {
        return false;
    }
    return detail::boundaryContainsDegenerateRegion(*this, other);
}

template <class PointType, class LabelType, class Storage>
template <HalfplaneIntersectionConcept OtherRegion>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::boundaryContains(const OtherRegion& other) const {
    if (other.isEmpty()) {
        return true;
    }
    if (!other.isDegenerate()) {
        return false;
    }
    return detail::boundaryContainsDegenerateRegion(*this, other);
}

template <class PointType, class LabelType>
template <HalfplaneIntersectionConcept OtherRegion>
constexpr bool Polyline<PointType, LabelType>::boundaryContains(const OtherRegion& other) const {
    if (other.isEmpty()) {
        return true;
    }
    if (!other.isDegenerate()) {
        return false;
    }
    return detail::boundaryContainsDegenerateRegion(*this, other);
}

template <class PointType, class LabelType>
template <HalfplaneIntersectionConcept OtherRegion>
constexpr bool Polygon<PointType, LabelType>::boundaryContains(const OtherRegion& other) const {
    if (other.isEmpty()) {
        return true;
    }
    if (!other.isDegenerate()) {
        return false;
    }
    return detail::boundaryContainsDegenerateRegion(*this, other);
}


// ---------------------------------------------------------------------------
// PolygonWithHoles

template <class PointType, class LabelType>
template <PointConcept OtherPoint>
constexpr bool PolygonWithHoles<PointType, LabelType>::boundaryContains(const OtherPoint& point) const {
    if (outer_.boundaryContains(point)) {
        return true;
    }
    for (const auto& hole : holes_) {
        if (hole.boundaryContains(point)) {
            return true;
        }
    }
    return false;
}

template <class PointType, class LabelType>
template <SegmentConcept OtherSegment>
constexpr bool PolygonWithHoles<PointType, LabelType>::boundaryContains(const OtherSegment& other) const {
    if (other.isDegenerate()) {
        return boundaryContains(other.min());
    }
    // ∂A = A ∖ A°, so a segment lies on the boundary exactly when the region
    // contains it while its relative interior never reaches the region
    // interior. Testing it this way rather than edge by edge also accepts a
    // segment covered jointly by several collinear ring edges.
    return contains(other) && !interiorsIntersect(other);
}

template <class PointType, class LabelType>
template <OrientedSegmentConcept OtherOrientedSegment>
constexpr bool PolygonWithHoles<PointType, LabelType>::boundaryContains(const OtherOrientedSegment& other) const {
    return boundaryContains(other.asSegment());
}

// The boundary of a bounded region is bounded, so an unbounded operand fits on
// it only after collapsing to a point.
template <class PointType, class LabelType>
template <LineConcept OtherLine>
constexpr bool PolygonWithHoles<PointType, LabelType>::boundaryContains(const OtherLine& other) const {
    return other.isDegenerate() && boundaryContains(other.min());
}

template <class PointType, class LabelType>
template <OrientedLineConcept OtherOrientedLine>
constexpr bool PolygonWithHoles<PointType, LabelType>::boundaryContains(const OtherOrientedLine& other) const {
    return other.isDegenerate() && boundaryContains(other.source());
}

template <class PointType, class LabelType>
template <RayConcept OtherRay>
constexpr bool PolygonWithHoles<PointType, LabelType>::boundaryContains(const OtherRay& other) const {
    return other.isDegenerate() && boundaryContains(other.source());
}

template <class PointType, class LabelType>
template <HalfplaneConcept OtherHalfplane>
constexpr bool PolygonWithHoles<PointType, LabelType>::boundaryContains(const OtherHalfplane& other) const {
    return other.isDegenerate() && boundaryContains(other.source());
}

// ∂A is a finite union of segments and so has no area, which rules out any
// operand that has some. A collapsed operand is exactly the union of its edges,
// so the segment overload settles it one edge at a time — and that overload
// already accepts an edge covered jointly by several collinear ring edges.
template <class PointType, class LabelType>
template <class OtherArea>
constexpr bool PolygonWithHoles<PointType, LabelType>::areaBoundaryContains(const OtherArea& other) const {
    if (!other.isDegenerate()) {
        return false;
    }
    for (const auto& edge : other.edges()) {
        if (!boundaryContains(edge)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template <RectangleConcept OtherRectangle>
constexpr bool PolygonWithHoles<PointType, LabelType>::boundaryContains(const OtherRectangle& other) const {
    return areaBoundaryContains(other);
}

template <class PointType, class LabelType>
template <TriangleConcept OtherTriangle>
constexpr bool PolygonWithHoles<PointType, LabelType>::boundaryContains(const OtherTriangle& other) const {
    return areaBoundaryContains(other);
}

template <class PointType, class LabelType>
template <ConvexConcept OtherConvex>
constexpr bool PolygonWithHoles<PointType, LabelType>::boundaryContains(const OtherConvex& other) const {
    return areaBoundaryContains(other);
}

template <class PointType, class LabelType>
template <PolygonConcept OtherPolygon>
constexpr bool PolygonWithHoles<PointType, LabelType>::boundaryContains(const OtherPolygon& other) const {
    return areaBoundaryContains(other);
}

template <class PointType, class LabelType>
template <PolygonWithHolesConcept OtherRegion>
constexpr bool PolygonWithHoles<PointType, LabelType>::boundaryContains(const OtherRegion& other) const {
    return areaBoundaryContains(other);
}

// A chain is the union of its edges, so it lies on ∂A exactly when every edge
// does (see @ref chainRelation).
template <class PointType, class LabelType>
template <MonotoneChainConcept OtherChain>
constexpr bool PolygonWithHoles<PointType, LabelType>::boundaryContains(const OtherChain& other) const {
    return chainRelation(other, true,
                         [this](const auto& edge) { return this->boundaryContains(edge); });
}

template <class PointType, class LabelType>
template <PolylineConcept OtherPolyline>
constexpr bool PolygonWithHoles<PointType, LabelType>::boundaryContains(const OtherPolyline& other) const {
    return chainRelation(other, true,
                         [this](const auto& edge) { return this->boundaryContains(edge); });
}

// ∂A is a finite union of segments, so it holds no disk with any area; a
// degenerate disk is the point a() (radius zero) or undefined.
template <class PointType, class LabelType>
template <DiskConcept OtherDisk>
constexpr bool PolygonWithHoles<PointType, LabelType>::boundaryContains(const OtherDisk& other) const {
    if (!other.isDegenerate()) {
        return false;
    }
    return boundaryContains(other.a());
}

template <class PointType, class LabelType>
template <HalfplaneIntersectionConcept OtherIntersection>
constexpr bool PolygonWithHoles<PointType, LabelType>::boundaryContains(const OtherIntersection& other) const {
    if (other.isEmpty()) {
        return true;
    }
    if (!other.isDegenerate()) {
        return false;  // it has area, and ∂A has none
    }
    return degenerateIntersectionRelation(
        other, [this](const auto& carrier) { return this->boundaryContains(carrier); });
}


// ---------------------------------------------------------------------------
// Reverse direction: lower-ranked shapes' boundaries containing a
// PolygonWithHoles.
//
// One rewriting settles every shape here, because it asks nothing of the
// shape at all. A boundary is at most one-dimensional, so it can hold the
// region only when the region has no area — and a region with no area is
// exactly the union of its ring edges (detail::everyHoledRegionEdge). So the
// question is edge by edge, and each edge goes to the shape's own
// boundaryContains(Segment).
//
// Note what this does *not* do: forward to the outer polygon. The outer
// polygon of a zero-area region need not be zero-area itself — a hole may
// cover it entirely, leaving the ring as the whole region — and a boundary
// that holds the ring does not hold the polygon the ring bounds.

namespace detail {

// ∂shape ⊇ region, for any shape offering boundaryContains(Segment).
template <class Shape2, class HoledRegion>
constexpr bool boundaryContainsHoledRegion(const Shape2& shape, const HoledRegion& region) {
    if (!region.isDegenerate()) {
        return false;  // the region has area; the boundary has none
    }
    return everyHoledRegionEdge(
        region, [&shape](const auto& edge) { return shape.boundaryContains(edge); });
}

}  // namespace detail

template <class Number, class Label>
template <PolygonWithHolesConcept OtherRegion>
constexpr bool Point<Number, Label>::boundaryContains(const OtherRegion& other) const {
    return detail::boundaryContainsHoledRegion(*this, other);
}

template <class PointType, class LabelType>
template <PolygonWithHolesConcept OtherRegion>
constexpr bool Segment<PointType, LabelType>::boundaryContains(const OtherRegion& other) const {
    return detail::boundaryContainsHoledRegion(*this, other);
}

template <class PointType, class LabelType>
template <PolygonWithHolesConcept OtherRegion>
constexpr bool OrientedSegment<PointType, LabelType>::boundaryContains(const OtherRegion& other) const {
    return asSegment().boundaryContains(other);
}

template <class PointType, class LabelType>
template <PolygonWithHolesConcept OtherRegion>
constexpr bool Line<PointType, LabelType>::boundaryContains(const OtherRegion& other) const {
    return detail::boundaryContainsHoledRegion(*this, other);
}

template <class PointType, class LabelType>
template <PolygonWithHolesConcept OtherRegion>
constexpr bool OrientedLine<PointType, LabelType>::boundaryContains(const OtherRegion& other) const {
    return asLine().boundaryContains(other);
}

template <class PointType, class LabelType>
template <PolygonWithHolesConcept OtherRegion>
constexpr bool Ray<PointType, LabelType>::boundaryContains(const OtherRegion& other) const {
    return detail::boundaryContainsHoledRegion(*this, other);
}

template <class PointType, class LabelType>
template <PolygonWithHolesConcept OtherRegion>
constexpr bool Halfplane<PointType, LabelType>::boundaryContains(const OtherRegion& other) const {
    return detail::boundaryContainsHoledRegion(*this, other);
}

template <class PointType, class LabelType>
template <PolygonWithHolesConcept OtherRegion>
constexpr bool Rectangle<PointType, LabelType>::boundaryContains(const OtherRegion& other) const {
    return detail::boundaryContainsHoledRegion(*this, other);
}

template <class PointType, class LabelType>
template <PolygonWithHolesConcept OtherRegion>
constexpr bool Triangle<PointType, LabelType>::boundaryContains(const OtherRegion& other) const {
    return detail::boundaryContainsHoledRegion(*this, other);
}

template <class PointType, class LabelType>
template <PolygonWithHolesConcept OtherRegion>
constexpr bool Disk<PointType, LabelType>::boundaryContains(const OtherRegion& other) const {
    return detail::boundaryContainsHoledRegion(*this, other);
}

template <class PointType, class LabelType>
template <PolygonWithHolesConcept OtherRegion>
constexpr bool Convex<PointType, LabelType>::boundaryContains(const OtherRegion& other) const {
    return detail::boundaryContainsHoledRegion(*this, other);
}

template <class PointType, class LabelType, class Storage>
template <PolygonWithHolesConcept OtherRegion>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::boundaryContains(const OtherRegion& other) const {
    return detail::boundaryContainsHoledRegion(*this, other);
}

template <class PointType, class LabelType>
template <PolygonWithHolesConcept OtherRegion>
constexpr bool Polyline<PointType, LabelType>::boundaryContains(const OtherRegion& other) const {
    return detail::boundaryContainsHoledRegion(*this, other);
}

template <class PointType, class LabelType>
template <PolygonWithHolesConcept OtherRegion>
constexpr bool Polygon<PointType, LabelType>::boundaryContains(const OtherRegion& other) const {
    return detail::boundaryContainsHoledRegion(*this, other);
}

template <class PointType, class LabelType>
template <PolygonWithHolesConcept OtherHoledRegion>
constexpr bool HalfplaneIntersection<PointType, LabelType>::boundaryContains(const OtherHoledRegion& other) const {
    return detail::boundaryContainsHoledRegion(*this, other);
}

// ---------------------------------------------------------------------------
// Runtime Shape argument: unwrap the stored alternative and re-dispatch. Every
// alternative has a per-shape overload above, so no fallback is needed.

template <class PointType, class LabelType>
template <PointConcept OtherPoint>
constexpr bool PolygonWithHoles<PointType, LabelType>::boundaryContains(const Shape<OtherPoint>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->boundaryContains(value);
        },
        other.variant());
}


// ---------------------------------------------------------------------------
// PolygonSet
//
// `∂A = ⋃ ∂Aᵢ`: a point on a component's boundary is in the set's interior only
// if another component fills the far side of it, which would mean a shared
// stretch of edge — what PolygonSet::isValid rules out. The union has no area,
// so only a collapsed operand can lie on it, and the one-dimensional case is
// the same one PolygonSet::contains has: a segment may run from one component's
// boundary onto another's through a point where the two touch.

template <class PointType, class LabelType>
template <detail::SetOperandConcept OtherShape>
bool PolygonSet<PointType, LabelType>::boundaryContains(const OtherShape& other) const {
    if constexpr (PointConcept<OtherShape>) {
        return anyComponent([&](const ComponentType& c) { return c.boundaryContains(other); });
    } else if constexpr (LineConcept<OtherShape>) {
        return other.isDegenerate() && boundaryContains(other.min());
    } else if constexpr (OrientedLineConcept<OtherShape>) {
        return other.isDegenerate() && boundaryContains(other.source());
    } else if constexpr (RayConcept<OtherShape>) {
        return other.isDegenerate() && boundaryContains(other.source());
    } else if constexpr (HalfplaneConcept<OtherShape>) {
        // A half-plane is unbounded unless it has collapsed onto its own
        // boundary line, which the line path then settles.
        return other.isDegenerate() && boundaryContains(other.source());
    } else if constexpr (SegmentConcept<OtherShape> || OrientedSegmentConcept<OtherShape>) {
        if (anyComponent([&](const ComponentType& c) { return c.boundaryContains(other); })) {
            return true;
        }
        if (!isPinched() || !intersects(other)) {
            return false;
        }
        return segmentIn(other, /*boundaryOnly=*/true);
    } else if constexpr (MonotoneChainConcept<OtherShape> || PolylineConcept<OtherShape>) {
        if (anyComponent([&](const ComponentType& c) { return c.boundaryContains(other); })) {
            return true;
        }
        if (!isPinched()) {
            return false;
        }
        return chainIn(other, /*boundaryOnly=*/true);
    } else {
        if (anyComponent([&](const ComponentType& c) { return c.boundaryContains(other); })) {
            return true;
        }
        return detail::reduceDegenerate(
            other, [this](const auto& carrier) { return boundaryContains(carrier); });
    }
}

template <class PointType, class LabelType>
template <PolygonSetConcept OtherSet>
bool PolygonSet<PointType, LabelType>::boundaryContains(const OtherSet& other) const {
    for (const auto& component : other) {
        if (!boundaryContains(component)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template <PointConcept OtherPoint>
bool PolygonSet<PointType, LabelType>::boundaryContains(const Shape<OtherPoint>& other) const {
    return std::visit([this](const auto& value) { return this->boundaryContains(value); },
                      other.variant());
}

}  // namespace pgl
