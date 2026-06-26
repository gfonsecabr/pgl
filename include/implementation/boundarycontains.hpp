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
    // A non-degenerate area never lies wholly on a 1D boundary; defer to the
    // convex view (false for >2 vertices, exact for the degenerate cases).
    return boundaryContains(other.asConvex());
}

template <class PointType, class LabelType>
template<TriangleConcept OtherTriangle>
constexpr bool Triangle<PointType, LabelType>::boundaryContains(const OtherTriangle& other) const {
    // See boundaryContains(Rectangle): defer to the convex view.
    return boundaryContains(other.asConvex());
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
    // A non-degenerate area never lies wholly on a 1D boundary; defer to the
    // convex view (false for >2 vertices, exact for the degenerate cases).
    return boundaryContains(other.asConvex());
}

template <class PointType, class LabelType>
template<TriangleConcept OtherTriangle>
constexpr bool Rectangle<PointType, LabelType>::boundaryContains(const OtherTriangle& other) const {
    // See boundaryContains(Rectangle): defer to the convex view.
    return boundaryContains(other.asConvex());
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

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Disk<PointType, LabelType>::boundaryContains(const OtherPoint& point) const {
    if (isDegenerate()) {
        return Line<PointType>(a(), c()).contains(point);
    }

    return inCircleSign(a(), b(), c(), point) == std::partial_ordering::equivalent;
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Disk<PointType, LabelType>::boundaryContains(const OtherSegment& other) const {
    if (isDegenerate()) {
        return Line<PointType>(a(), c()).contains(other);
    }
    return other.isDegenerate() && boundaryContains(other.min());
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Disk<PointType, LabelType>::boundaryContains(const OtherOrientedSegment& other) const {
    if (isDegenerate()) {
        return Line<PointType>(a(), c()).contains(other);
    }
    return other.isDegenerate() && boundaryContains(other.source());
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Disk<PointType, LabelType>::boundaryContains(const OtherLine& other) const {
    if (isDegenerate()) {
        return Line<PointType>(a(), c()).contains(other);
    }
    return other.isDegenerate() && boundaryContains(other.min());
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Disk<PointType, LabelType>::boundaryContains(const OtherOrientedLine& other) const {
    if (isDegenerate()) {
        return Line<PointType>(a(), c()).contains(other);
    }
    return other.isDegenerate() && boundaryContains(other.source());
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool Disk<PointType, LabelType>::boundaryContains(const OtherRay& other) const {
    if (isDegenerate()) {
        return Line<PointType>(a(), c()).contains(other);
    }
    return other.isDegenerate() && boundaryContains(other.source());
}

template <class PointType, class LabelType>
template<HalfplaneConcept OtherHalfplane>
constexpr bool Disk<PointType, LabelType>::boundaryContains(const OtherHalfplane& other) const {
    if (isDegenerate()) {
        return Line<PointType>(a(), c()).contains(other);
    }
    return other.isDegenerate() && boundaryContains(other.source());
}

template <class PointType, class LabelType>
template<TriangleConcept OtherTriangle>
constexpr bool Disk<PointType, LabelType>::boundaryContains(const OtherTriangle& other) const {
    if (isDegenerate()) {
        return Line<PointType>(a(), c()).contains(other);
    }

    const bool is_point = other.a() == other.b() && other.a() == other.c();
    return is_point && boundaryContains(other.a());
}

template <class PointType, class LabelType>
template<RectangleConcept OtherRectangle>
constexpr bool Disk<PointType, LabelType>::boundaryContains(const OtherRectangle& other) const {
    if (isDegenerate()) {
        return Line<PointType>(a(), c()).contains(other);
    }

    const bool is_point = other.min() == other.max();
    return is_point && boundaryContains(other.min());
}

template <class PointType, class LabelType>
template<ConvexConcept OtherConvex>
constexpr bool Disk<PointType, LabelType>::boundaryContains(const OtherConvex& other) const {
    if (isDegenerate()) {
        return Line<PointType>(a(), c()).contains(other);
    }

    return other.size() == 1 && boundaryContains(other[0]);
}

template <class PointType, class LabelType>
template<DiskConcept OtherDisk>
constexpr bool Disk<PointType, LabelType>::boundaryContains(const OtherDisk& other) const {
    if (other.isDegenerate()) {
        return boundaryContains(Segment<typename OtherDisk::PointType>(other.a(), other.c()));
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
        return false;
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
    // A non-degenerate area never lies wholly on a 1D boundary; defer to the
    // convex view (false for >2 vertices, exact for the degenerate cases).
    return boundaryContains(other.asConvex());
}

template <class PointType, class LabelType>
template<TriangleConcept OtherTriangle>
constexpr bool Convex<PointType, LabelType>::boundaryContains(const OtherTriangle& other) const {
    // See boundaryContains(Rectangle): defer to the convex view.
    return boundaryContains(other.asConvex());
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
    for (const auto& edge : edges()) {
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
    // A non-degenerate area never lies wholly on a 1D boundary; defer to the
    // convex view (false for >2 vertices, exact for the degenerate cases).
    return boundaryContains(other.asConvex());
}

template <class PointType, class LabelType>
template<TriangleConcept OtherTriangle>
constexpr bool Polygon<PointType, LabelType>::boundaryContains(const OtherTriangle& other) const {
    // See boundaryContains(Rectangle): defer to the convex view.
    return boundaryContains(other.asConvex());
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

}  // namespace pgl
