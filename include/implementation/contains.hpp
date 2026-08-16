#pragma once

#include "implementation/boundarycontains.hpp"

/**
 * @file contains.hpp
 * @brief Implementations of the 'contains' predicate.
 **/

#include <limits>
#include "implementation/orientation.hpp"
#include "predicates_helpers.hpp"


namespace pgl {

/**
 * @section predicates-point Point
 * Point equality and the point-vs-shape predicates. This section also contains
 * the cases where removing a point disconnects a 1D primitive.
 */

template <class Number, class Label>
template<PointConcept OtherPoint>
constexpr bool Point<Number, Label>::contains(const OtherPoint& other) const {
    using Compare = std::common_type_t<Number, typename OtherPoint::NumberType>;
    return static_cast<Compare>(x()) == static_cast<Compare>(other.x()) &&
           static_cast<Compare>(y()) == static_cast<Compare>(other.y());
}

template <class Number, class Label>
template<SegmentConcept OtherSegment>
constexpr bool Point<Number, Label>::contains(const OtherSegment& other) const {
    return other.isDegenerate() && contains(other.min());
}

template <class Number, class Label>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Point<Number, Label>::contains(const OtherOrientedSegment& other) const {
    return other.isDegenerate() && contains(other.source());
}

template <class Number, class Label>
template<LineConcept OtherLine>
constexpr bool Point<Number, Label>::contains(const OtherLine& other) const {
    return other.isDegenerate() && contains(other.min());
}

template <class Number, class Label>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Point<Number, Label>::contains(const OtherOrientedLine& other) const {
    return other.isDegenerate() && contains(other.source());
}

template <class Number, class Label>
template<RayConcept OtherRay>
constexpr bool Point<Number, Label>::contains(const OtherRay& other) const {
    return other.isDegenerate() && contains(other.source());
}

template <class Number, class Label>
template<HalfplaneConcept OtherHalfplane>
constexpr bool Point<Number, Label>::contains(const OtherHalfplane& other) const {
    return other.isDegenerate() && contains(other.source());
}

template <class Number, class Label>
template<RectangleConcept OtherRectangle>
constexpr bool Point<Number, Label>::contains(const OtherRectangle& other) const {
    if (other.empty()) {
        // The empty set is a subset of every shape, its boundary and its
        // interior alike.
        return true;
    }
    return contains(other.min()) && contains(other.max());
}

template <class Number, class Label>
template<TriangleConcept OtherTriangle>
constexpr bool Point<Number, Label>::contains(const OtherTriangle& other) const {
    const auto vertices = other.vertices();
    return contains(vertices[0]) && contains(vertices[1]) && contains(vertices[2]);
}

template <class Number, class Label>
template<ConvexConcept OtherConvex>
constexpr bool Point<Number, Label>::contains(const OtherConvex& other) const {
    return other.size()== 0 || (other.size() == 1 && other[0]==*this);
}

template <class Number, class Label>
template<DiskConcept OtherDisk>
constexpr bool Point<Number, Label>::contains(const OtherDisk& other) const {
    // A non-degenerate disk has positive area, so a single point can only
    // contain a disk that has collapsed to a point, which is the case a() == b()
    // (a disk is never a segment). A disk with a() == b() != c() is undefined,
    // and reading it as the point a() is one answer the contract allows.
    return other.a() == other.b() && contains(other.a());
}

template <class Number, class Label>
constexpr bool Point<Number, Label>::contains(const Shape<Point<Number, Label>>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->contains(value);
        },
        other.variant());
}

/**
 * @section predicates-segment Segment
 * Segment endpoint, boundary, containment, collinearity, intersection, and
 * topological predicates, including the generic `separates` / `crosses`
 * dispatch used against 1D and area targets.
 */

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Segment<PointType, LabelType>::contains(const OtherPoint& point) const {
    if (point < min() || max() < point) {
        return false;
    }
    return pgl::collinear(min(), max(), point);
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Segment<PointType, LabelType>::contains(const OtherSegment& other) const {
    return contains(other.min()) && contains(other.max());
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Segment<PointType, LabelType>::contains(const OtherOrientedSegment& other) const {
    return contains(other.source()) && contains(other.target());
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Segment<PointType, LabelType>::contains(const OtherLine& other) const {
    return other.isDegenerate() && contains(other.min());
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Segment<PointType, LabelType>::contains(const OtherOrientedLine& other) const {
    return other.isDegenerate() && contains(other.source());
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool Segment<PointType, LabelType>::contains(const OtherRay& other) const {
    return other.isDegenerate() && contains(other.source());
}

template <class PointType, class LabelType>
template<HalfplaneConcept OtherHalfplane>
constexpr bool Segment<PointType, LabelType>::contains(const OtherHalfplane& other) const {
    return other.isDegenerate() && contains(other.source());
}

template <class PointType, class LabelType>
template<RectangleConcept OtherRectangle>
constexpr bool Segment<PointType, LabelType>::contains(const OtherRectangle& other) const {
    if (other.empty()) {
        // The empty set is a subset of every shape, its boundary and its
        // interior alike.
        return true;
    }
    if (!other.isDegenerate()) {
        return false;
    }
    if (other.min() == other.max()) {
        return contains(other.min());
    }
    return contains(Segment<typename OtherRectangle::PointType>(other.min(), other.max()));
}

template <class PointType, class LabelType>
template<TriangleConcept OtherTriangle>
constexpr bool Segment<PointType, LabelType>::contains(const OtherTriangle& other) const {
    return contains(other.a()) && contains(other.b()) && contains(other.c());
}

template <class PointType, class LabelType>
template<ConvexConcept OtherConvex>
constexpr bool Segment<PointType, LabelType>::contains(const OtherConvex& other) const {
    if (other.size() > 2) {
        return false;
    }
    for (std::size_t i = 0; i < other.size(); ++i) {
        if (!contains(other[i])) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<DiskConcept OtherDisk>
constexpr bool Segment<PointType, LabelType>::contains(const OtherDisk& other) const {
    return other.a() == other.b() && other.b() == other.c() && contains(other.a());
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Segment<PointType, LabelType>::contains(const Shape<OtherPoint>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->contains(value);
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
constexpr bool Triangle<PointType, LabelType>::contains(const OtherPoint& point) const {
    if (isDegenerate()) {
        return boundaryContains(point);
    }
    const auto o1 = orientationSign(a(), b(), point);
    const auto o2 = orientationSign(b(), c(), point);
    const auto o3 = orientationSign(c(), a(), point);
    const bool has_negative =
        o1 == std::partial_ordering::less ||
        o2 == std::partial_ordering::less ||
        o3 == std::partial_ordering::less;
    const bool has_positive =
        o1 == std::partial_ordering::greater ||
        o2 == std::partial_ordering::greater ||
        o3 == std::partial_ordering::greater;
    return !(has_negative && has_positive);
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Triangle<PointType, LabelType>::contains(const OtherSegment& other) const {
    return contains(other.min()) && contains(other.max());
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Triangle<PointType, LabelType>::contains(const OtherOrientedSegment& other) const {
    return contains(other.source()) && contains(other.target());
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Triangle<PointType, LabelType>::contains(const OtherLine& other) const {
    return other.isDegenerate() && contains(other.min());
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Triangle<PointType, LabelType>::contains(const OtherOrientedLine& other) const {
    return other.isDegenerate() && contains(other.source());
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool Triangle<PointType, LabelType>::contains(const OtherRay& other) const {
    return other.isDegenerate() && contains(other.source());
}

template <class PointType, class LabelType>
template<HalfplaneConcept OtherHalfplane>
constexpr bool Triangle<PointType, LabelType>::contains(const OtherHalfplane& other) const {
    return other.isDegenerate() && contains(other.source());
}

template <class PointType, class LabelType>
template<RectangleConcept OtherRectangle>
constexpr bool Triangle<PointType, LabelType>::contains(const OtherRectangle& other) const {
    if (other.empty()) {
        // The empty set is a subset of every shape, its boundary and its
        // interior alike.
        return true;
    }
    const auto vertices = other.vertices();
    for (const auto& vertex : vertices) {
        if (!contains(vertex)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<TriangleConcept OtherTriangle>
constexpr bool Triangle<PointType, LabelType>::contains(const OtherTriangle& other) const {
    return contains(other.a()) && contains(other.b()) && contains(other.c());
}

template <class PointType, class LabelType>
template<ConvexConcept OtherConvex>
constexpr bool Triangle<PointType, LabelType>::contains(const OtherConvex& other) const {
    if (other.size() == 0) {
        return true;
    }
    if (!bbox().contains(other.bbox())) {
        return false;
    }
    for (std::size_t i = 0; i < other.size(); ++i) {
        if (!contains(other[i])) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<DiskConcept OtherDisk>
constexpr bool Triangle<PointType, LabelType>::contains(const OtherDisk& other) const {
    if (const auto center = other.getIfPoint()) {
        // A radius-zero disk is its center, and contains(Point) already reads a
        // degenerate triangle as its carrier segment.
        return contains(*center);
    }
    // A non-degenerate triangle behaves exactly like its convex view.
    return asConvex().contains(other);
}

template <class PointType, class LabelType>
constexpr bool Triangle<PointType, LabelType>::contains(const Shape<PointType>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->contains(value);
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
constexpr bool OrientedSegment<PointType, LabelType>::contains(const OtherPoint& point) const {
    return asSegment().contains(point);
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool OrientedSegment<PointType, LabelType>::contains(const OtherSegment& other) const {
    return asSegment().contains(other);
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool OrientedSegment<PointType, LabelType>::contains(const OtherOrientedSegment& other) const {
    return asSegment().contains(other);
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool OrientedSegment<PointType, LabelType>::contains(const OtherLine& other) const {
    return asSegment().contains(other);
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool OrientedSegment<PointType, LabelType>::contains(const OtherOrientedLine& other) const {
    return asSegment().contains(other);
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool OrientedSegment<PointType, LabelType>::contains(const OtherRay& other) const {
    return asSegment().contains(other);
}

template <class PointType, class LabelType>
template<HalfplaneConcept OtherHalfplane>
constexpr bool OrientedSegment<PointType, LabelType>::contains(const OtherHalfplane& other) const {
    return asSegment().contains(other);
}

template <class PointType, class LabelType>
template<RectangleConcept OtherRectangle>
constexpr bool OrientedSegment<PointType, LabelType>::contains(const OtherRectangle& other) const {
    if (other.empty()) {
        // The empty set is a subset of every shape, its boundary and its
        // interior alike.
        return true;
    }
    return asSegment().contains(other);
}

template <class PointType, class LabelType>
template<TriangleConcept OtherTriangle>
constexpr bool OrientedSegment<PointType, LabelType>::contains(const OtherTriangle& other) const {
    return asSegment().contains(other);
}

template <class PointType, class LabelType>
template<ConvexConcept OtherConvex>
constexpr bool OrientedSegment<PointType, LabelType>::contains(const OtherConvex& other) const {
    return asSegment().contains(other);
}

template <class PointType, class LabelType>
template<DiskConcept OtherDisk>
constexpr bool OrientedSegment<PointType, LabelType>::contains(const OtherDisk& other) const {
    return asSegment().contains(other);
}

template <class PointType, class LabelType>
constexpr bool OrientedSegment<PointType, LabelType>::contains(const Shape<PointType>& other) const {
    return asSegment().contains(other);
}

/**
 * @section predicates-line Line
 * Geometric line predicates: geometric equality/order, containment,
 * intersection against 1D and 2D shapes, and generic separation dispatch.
 */

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Line<PointType, LabelType>::contains(const OtherPoint& point) const {
    if (isDegenerate()) {
        return point == min();
    }
    return pgl::collinear(min(), max(), point);
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Line<PointType, LabelType>::contains(const OtherLine& other) const {
    return contains(other.min()) && contains(other.max());
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Line<PointType, LabelType>::contains(const OtherSegment& other) const {
    return contains(other.min()) && contains(other.max());
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Line<PointType, LabelType>::contains(const OtherOrientedSegment& other) const {
    return contains(other.source()) && contains(other.target());
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Line<PointType, LabelType>::contains(const OtherOrientedLine& other) const {
    return contains(other.asLine());
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool Line<PointType, LabelType>::contains(const OtherRay& other) const {
    return contains(other.source()) && contains(other.target());
}

template <class PointType, class LabelType>
template<HalfplaneConcept OtherHalfplane>
constexpr bool Line<PointType, LabelType>::contains(const OtherHalfplane& other) const {
    return other.isDegenerate() && contains(other.source());
}

template <class PointType, class LabelType>
template<RectangleConcept OtherRectangle>
constexpr bool Line<PointType, LabelType>::contains(const OtherRectangle& other) const {
    if (other.empty()) {
        // The empty set is a subset of every shape, its boundary and its
        // interior alike.
        return true;
    }
    if (!other.isDegenerate()) {
        return false;
    }
    return contains(other.min()) && contains(other.max());
}

template <class PointType, class LabelType>
template<TriangleConcept OtherTriangle>
constexpr bool Line<PointType, LabelType>::contains(const OtherTriangle& other) const {
    return contains(other.a()) && contains(other.b()) && contains(other.c());
}

template <class PointType, class LabelType>
template<ConvexConcept OtherConvex>
constexpr bool Line<PointType, LabelType>::contains(const OtherConvex& other) const {
    if (other.size() > 2) {
        return false;
    }
    for (std::size_t i = 0; i < other.size(); ++i) {
        if (!contains(other[i])) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<DiskConcept OtherDisk>
constexpr bool Line<PointType, LabelType>::contains(const OtherDisk& other) const {
    // A non-degenerate disk has positive area; only a disk collapsed to a point,
    // the case a() == b(), can lie in a one-dimensional line. See
    // Point::contains(Disk) for the undefined a() == b() != c() case.
    return other.a() == other.b() && contains(other.a());
}

template <class PointType, class LabelType>
constexpr bool Line<PointType, LabelType>::contains(const Shape<PointType>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->contains(value);
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
constexpr bool OrientedLine<PointType, LabelType>::contains(const OtherPoint& point) const {
    return this->asLine().contains(point);
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool OrientedLine<PointType, LabelType>::contains(const OtherLine& other) const {
    return this->asLine().contains(other);
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool OrientedLine<PointType, LabelType>::contains(const OtherOrientedLine& other) const {
    return this->asLine().contains(other.asLine());
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool OrientedLine<PointType, LabelType>::contains(const OtherSegment& other) const {
    return this->asLine().contains(other);
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool OrientedLine<PointType, LabelType>::contains(const OtherOrientedSegment& other) const {
    return this->asLine().contains(other);
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool OrientedLine<PointType, LabelType>::contains(const OtherRay& other) const {
    return this->asLine().contains(other);
}

template <class PointType, class LabelType>
template<HalfplaneConcept OtherHalfplane>
constexpr bool OrientedLine<PointType, LabelType>::contains(const OtherHalfplane& other) const {
    return this->asLine().contains(other);
}

template <class PointType, class LabelType>
template<RectangleConcept OtherRectangle>
constexpr bool OrientedLine<PointType, LabelType>::contains(const OtherRectangle& other) const {
    if (other.empty()) {
        // The empty set is a subset of every shape, its boundary and its
        // interior alike.
        return true;
    }
    return this->asLine().contains(other);
}

template <class PointType, class LabelType>
template<TriangleConcept OtherTriangle>
constexpr bool OrientedLine<PointType, LabelType>::contains(const OtherTriangle& other) const {
    return this->asLine().contains(other);
}

template <class PointType, class LabelType>
template<ConvexConcept OtherConvex>
constexpr bool OrientedLine<PointType, LabelType>::contains(const OtherConvex& other) const {
    if (other.size() > 2) {
        return false;
    }
    for (std::size_t i = 0; i < other.size(); ++i) {
        if (!contains(other[i])) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<DiskConcept OtherDisk>
constexpr bool OrientedLine<PointType, LabelType>::contains(const OtherDisk& other) const {
    return this->asLine().contains(other);
}

template <class PointType, class LabelType>
constexpr bool OrientedLine<PointType, LabelType>::contains(const Shape<PointType>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->contains(value);
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
constexpr bool Ray<PointType, LabelType>::contains(const OtherPoint& point) const {
    if (isDegenerate()) {
        return point == source();
    }
    return pgl::collinear(source(), target(), point) && containsCollinear(point);
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Ray<PointType, LabelType>::contains(const OtherLine& other) const {
    return other.isDegenerate() && contains(other.min());
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Ray<PointType, LabelType>::contains(const OtherOrientedLine& other) const {
    return other.isDegenerate() && contains(other.source());
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Ray<PointType, LabelType>::contains(const OtherSegment& other) const {
    return contains(other.min()) && contains(other.max());
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Ray<PointType, LabelType>::contains(const OtherOrientedSegment& other) const {
    return contains(other.source()) && contains(other.target());
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool Ray<PointType, LabelType>::contains(const OtherRay& other) const {
    return contains(other.source()) && contains(other.target());
}

template <class PointType, class LabelType>
template<HalfplaneConcept OtherHalfplane>
constexpr bool Ray<PointType, LabelType>::contains(const OtherHalfplane& other) const {
    return other.isDegenerate() && contains(other.source());
}

template <class PointType, class LabelType>
template<RectangleConcept OtherRectangle>
constexpr bool Ray<PointType, LabelType>::contains(const OtherRectangle& other) const {
    if (other.empty()) {
        // The empty set is a subset of every shape, its boundary and its
        // interior alike.
        return true;
    }
    if (!other.isDegenerate()) {
        return false;
    }
    return contains(other.min()) && contains(other.max());
}

template <class PointType, class LabelType>
template<TriangleConcept OtherTriangle>
constexpr bool Ray<PointType, LabelType>::contains(const OtherTriangle& other) const {
    return contains(other.a()) && contains(other.b()) && contains(other.c());
}

template <class PointType, class LabelType>
template<ConvexConcept OtherConvex>
constexpr bool Ray<PointType, LabelType>::contains(const OtherConvex& other) const {
    if (other.size() > 2) {
        return false;
    }
    for (std::size_t i = 0; i < other.size(); ++i) {
        if (!contains(other[i])) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<DiskConcept OtherDisk>
constexpr bool Ray<PointType, LabelType>::contains(const OtherDisk& other) const {
    // A non-degenerate disk has positive area; only a disk collapsed to a point,
    // the case a() == b(), can lie in a one-dimensional ray. See
    // Point::contains(Disk) for the undefined a() == b() != c() case.
    return other.a() == other.b() && contains(other.a());
}

template <class PointType, class LabelType>
constexpr bool Ray<PointType, LabelType>::contains(const Shape<PointType>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->contains(value);
        },
        other.variant());
}

/**
 * @section predicates-rectangle Rectangle
 * Axis-aligned rectangle predicates plus the rectangle-local clipping helpers
 * used to answer strict interior and separation questions.
 */

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Rectangle<PointType, LabelType>::contains(const OtherPoint& point) const {
    // The empty set contains no point, and it needs no case of its own: its
    // corners are inverted, so the pair of tests on the inverted axis cannot
    // both hold and the answer is already false.
    return !(point.x() < min().x()) &&
           !(max().x() < point.x()) &&
           !(point.y() < min().y()) &&
           !(max().y() < point.y());
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Rectangle<PointType, LabelType>::contains(const OtherLine& other) const {
    if (empty()) {
        // The empty set is a subset of itself and of nothing else.
        return detail::coversNoPoint(other);
    }
    return other.isDegenerate() && contains(other.min());
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Rectangle<PointType, LabelType>::contains(const OtherOrientedLine& other) const {
    if (empty()) {
        // The empty set is a subset of itself and of nothing else.
        return detail::coversNoPoint(other);
    }
    return other.isDegenerate() && contains(other.source());
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Rectangle<PointType, LabelType>::contains(const OtherSegment& other) const {
    if (empty()) {
        // The empty set is a subset of itself and of nothing else.
        return detail::coversNoPoint(other);
    }
    return contains(other.min()) && contains(other.max());
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Rectangle<PointType, LabelType>::contains(const OtherOrientedSegment& other) const {
    if (empty()) {
        // The empty set is a subset of itself and of nothing else.
        return detail::coversNoPoint(other);
    }
    return contains(other.source()) && contains(other.target());
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool Rectangle<PointType, LabelType>::contains(const OtherRay& other) const {
    if (empty()) {
        // The empty set is a subset of itself and of nothing else.
        return detail::coversNoPoint(other);
    }
    return other.isDegenerate() && contains(other.source());
}

template <class PointType, class LabelType>
template<HalfplaneConcept OtherHalfplane>
constexpr bool Rectangle<PointType, LabelType>::contains(const OtherHalfplane& other) const {
    if (empty()) {
        // The empty set is a subset of itself and of nothing else.
        return detail::coversNoPoint(other);
    }
    return other.isDegenerate() && contains(other.source());
}

template <class PointType, class LabelType>
template<RectangleConcept OtherRectangle>
constexpr bool Rectangle<PointType, LabelType>::contains(const OtherRectangle& other) const {
    // The empty set is a subset of every shape, so an empty operand is
    // contained whatever its inverted corners do to the tests below. An empty
    // *this needs no case of its own: it covers no point, so the corner tests
    // already answer false, which is the right answer for every non-empty
    // operand. The emptiness test trails the geometry because containment is
    // usually decided without it.
    return (contains(other.min()) && contains(other.max())) || other.empty();
}

template <class PointType, class LabelType>
template<TriangleConcept OtherTriangle>
constexpr bool Rectangle<PointType, LabelType>::contains(const OtherTriangle& other) const {
    if (empty()) {
        // The empty set is a subset of itself and of nothing else.
        return detail::coversNoPoint(other);
    }
    return contains(other.a()) && contains(other.b()) && contains(other.c());
}

template <class PointType, class LabelType>
template<ConvexConcept OtherConvex>
constexpr bool Rectangle<PointType, LabelType>::contains(const OtherConvex& other) const {
    if (empty()) {
        // The empty set is a subset of itself and of nothing else.
        return detail::coversNoPoint(other);
    }
    return other.size()==0 || contains(other.bbox());
}

template <class PointType, class LabelType>
template<DiskConcept OtherDisk>
constexpr bool Rectangle<PointType, LabelType>::contains(const OtherDisk& other) const {
    if (empty()) {
        // The empty set is a subset of itself and of nothing else.
        return detail::coversNoPoint(other);
    }
    if (const auto center = other.getIfPoint()) {
        // A radius-zero disk is its center; it has no interior point for the
        // witness test below to find, which would reject it on the boundary.
        return contains(*center);
    }
    // The closed rectangle contains the closed disk iff no edge passes through
    // the open disk (the disk does not poke across the boundary) and the disk
    // lies on the inside (a point strictly inside the disk is interior to the
    // rectangle). Both tests are exact in integer arithmetic, avoiding the
    // disk's center and radius, which are generally rational.
    for (const auto& edge : edges()) {
        if (edge.interiorsIntersect(other)) {
            return false;
        }
    }
    return other.pointInsideInteriorContainedIn(*this);
}

template <class PointType, class LabelType>
constexpr bool Rectangle<PointType, LabelType>::contains(const Shape<PointType>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->contains(value);
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
constexpr bool Halfplane<PointType, LabelType>::contains(const OtherPoint& point) const {
    if (isDegenerate()) {
        return point == source();
    }
    const auto side = orientationSign(source(), target(), point);
    return side == std::partial_ordering::greater || side == std::partial_ordering::equivalent;
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Halfplane<PointType, LabelType>::contains(const OtherLine& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return other.isDegenerate() && contains(other.min());
    }
    return contains(other.min()) && this->asLine().parallel(other);
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Halfplane<PointType, LabelType>::contains(const OtherOrientedLine& other) const {
    return contains(other.asLine());
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Halfplane<PointType, LabelType>::contains(const OtherSegment& other) const {
    if (isDegenerate()) {
        return other.isDegenerate() && contains(other.min());
    }
    return contains(other.min()) && contains(other.max());
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Halfplane<PointType, LabelType>::contains(const OtherOrientedSegment& other) const {
    if (isDegenerate()) {
        return other.isDegenerate() && contains(other.source());
    }
    return contains(other.source()) && contains(other.target());
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool Halfplane<PointType, LabelType>::contains(const OtherRay& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return other.isDegenerate() && contains(other.source());
    }
    if (orientationSign(source(), target(), other.source()) < 0) {
        return false;
    }
    // The ray runs on into the half-plane exactly when its direction does not
    // turn away from the boundary. That is the sign of the difference of the
    // two endpoint determinants, which is one cross product of the two
    // directions -- no need to evaluate either determinant.
    return !(crossSign(source(), target(), other.source(), other.target()) < 0);
}

template <class PointType, class LabelType>
template<RectangleConcept OtherRectangle>
constexpr bool Halfplane<PointType, LabelType>::contains(const OtherRectangle& other) const {
    if (other.empty()) {
        // The empty set is a subset of every shape, its boundary and its
        // interior alike.
        return true;
    }
    const auto vertices = other.vertices();
    for (const auto& vertex : vertices) {
        if (!contains(vertex)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<HalfplaneConcept OtherHalfplane>
constexpr bool Halfplane<PointType, LabelType>::contains(const OtherHalfplane& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }

    // Boundaries must be parallel AND face the same interior side; otherwise
    // part of `other` always lies outside `*this`. `parallel` is orientation
    // insensitive, so the dot-product sign disambiguates same vs opposite
    // facing. Then `other` is the nested half-plane iff its boundary line lies
    // on the closed side of `*this`, which contains(other.source()) tests
    // exactly (one point stands in for the whole parallel line).
    if (!asLine().parallel(other.asLine()) ||
        dotSign(target() - source(), other.target() - other.source()) !=
            std::partial_ordering::greater) {
        return false;
    }
    return contains(other.source());
}

template <class PointType, class LabelType>
template<TriangleConcept OtherTriangle>
constexpr bool Halfplane<PointType, LabelType>::contains(const OtherTriangle& other) const {
    return contains(other.a()) && contains(other.b()) && contains(other.c());
}

template <class PointType, class LabelType>
template<ConvexConcept OtherConvex>
constexpr bool Halfplane<PointType, LabelType>::contains(const OtherConvex& other) const {
    if (other.size() == 0) {
        return true;
    }
    if (other.size() == 1) {
        return contains(other[0]);
    }
    if (other.size() == 2) {
        return contains(other[0]) && contains(other[1]);
    }

    return contains(other[0]) && contains(other[1]) && contains(other[2]) && !static_cast<Line<PointType>>(*this).interiorsIntersect(other);
}

template <class PointType, class LabelType>
template<DiskConcept OtherDisk>
constexpr bool Halfplane<PointType, LabelType>::contains(const OtherDisk& other) const {
    if (const auto center = other.getIfPoint()) {
        // A radius-zero disk is its center; it has no interior point for the
        // witness test below to find, which would reject it on the boundary.
        // This mirrors the same guard in interiorContains(Disk).
        return contains(*center);
    }
    return !asLine().interiorsIntersect(other) && other.pointInsideInteriorContainedIn(*this);
}

template <class PointType, class LabelType>
constexpr bool Halfplane<PointType, LabelType>::contains(const Shape<PointType>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->contains(value);
        },
        other.variant());
}

// -----------------------------------------------------------------------------
// Disk

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Disk<PointType, LabelType>::contains(const OtherPoint& point) const {
    // A disk that has collapsed covers the single point a(); it is never a
    // segment. The remaining degenerate disks -- three collinear points that
    // determine no circle -- are undefined, and fall through to the in-circle
    // determinant, which is division-free and terminates on any input.
    if (a() == b()) {
        return a() == point;
    }

    return inCircleSign(a(), b(), c(), point) != std::partial_ordering::less;
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Disk<PointType, LabelType>::contains(const OtherSegment& other) const {
    return contains(other.min()) && contains(other.max());
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Disk<PointType, LabelType>::contains(const OtherOrientedSegment& other) const {
    return contains(other.source()) && contains(other.target());
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Disk<PointType, LabelType>::contains(const OtherLine& other) const {
    return other.isDegenerate() && contains(other.min());
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Disk<PointType, LabelType>::contains(const OtherOrientedLine& other) const {
    return other.isDegenerate() && contains(other.source());
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool Disk<PointType, LabelType>::contains(const OtherRay& other) const {
    return other.isDegenerate() && contains(other.source());
}

template <class PointType, class LabelType>
template<HalfplaneConcept OtherHalfplane>
constexpr bool Disk<PointType, LabelType>::contains(const OtherHalfplane& other) const {
    return other.isDegenerate() && contains(other.source());
}

template <class PointType, class LabelType>
template<TriangleConcept OtherTriangle>
constexpr bool Disk<PointType, LabelType>::contains(const OtherTriangle& other) const {
    return contains(other.a()) && contains(other.b()) && contains(other.c());
}

template <class PointType, class LabelType>
template<RectangleConcept OtherRectangle>
constexpr bool Disk<PointType, LabelType>::contains(const OtherRectangle& other) const {
    if (other.empty()) {
        // The empty set is a subset of every shape, its boundary and its
        // interior alike.
        return true;
    }
    const auto vertices = other.vertices();
    return contains(vertices[0]) && contains(vertices[1]) && contains(vertices[2]) && contains(vertices[3]);
}

template <class PointType, class LabelType>
template<ConvexConcept OtherConvex>
constexpr bool Disk<PointType, LabelType>::contains(const OtherConvex& other) const {
    for (const auto& point : other) {
        if (!contains(point)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<DiskConcept OtherDisk>
constexpr bool Disk<PointType, LabelType>::contains(const OtherDisk& other) const {
    // A collapsed disk is the point a(), never a segment.
    if (other.a() == other.b()) {
        return contains(other.a());
    }
    if (isDegenerate()) {
        return false;
    }

    using R = std::conditional_t<
        std::is_floating_point_v<NumberType> ||
            std::is_floating_point_v<typename OtherDisk::NumberType>,
        long double,
        Rational<BigInt>>;

    const R r1_sq = squaredRadius<R>();
    const R r2_sq = other.template squaredRadius<R>();
    if (r1_sq < r2_sq) {
        return false;
    }

    const R d2 = center<R>().template squaredDistance<R>(other.template center<R>());
    const R A = d2 - r1_sq - r2_sq;
    return A <= R{} && A * A >= R{4} * r1_sq * r2_sq;
}

template <class PointType, class LabelType>
constexpr bool Disk<PointType, LabelType>::contains(const Shape<PointType>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->contains(value);
        },
        other.variant());
}


// ---------------------------------------------------------------------------
// Convex

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Convex<PointType, LabelType>::contains(const OtherPoint& point) const {
    if (isDegenerate()) {
        // Fewer than three vertices: there is no lower/upper chain for
        // edgesAtX to split, so test the carrier point or segment directly.
        if (const auto vertex = getIfPoint()) {
            return *vertex == point;
        }
        if (const auto carrier = getIfSegment()) {
            return carrier->contains(point);
        }
        return false;  // no vertices at all
    }
    if (!bbox().contains(point)) {
        return false;
    }
    auto edges = edgesAtX(point.x());
    if (!edges) {
        return false;
    }
    const auto& lower = (*edges)[0];
    const auto& upper = (*edges)[1];
    // Both edges are non-vertical and stored left-to-right, so the interior
    // lies above the lower edge and below the upper edge (boundary included).
    return orientationSign(lower[0], lower[1], point) >= 0 &&
           orientationSign(upper[0], upper[1], point) <= 0;
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Convex<PointType, LabelType>::contains(const OtherSegment& other) const {
    return contains(other[0]) && contains(other[1]);
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Convex<PointType, LabelType>::contains(const OtherOrientedSegment& other) const {
    return contains(other[0]) && contains(other[1]);
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Convex<PointType, LabelType>::contains(const OtherLine&) const {
    return false;
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Convex<PointType, LabelType>::contains(const OtherOrientedLine&) const {
    return false;
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool Convex<PointType, LabelType>::contains(const OtherRay&) const {
    return false;
}

template <class PointType, class LabelType>
template<HalfplaneConcept OtherHalfplane>
constexpr bool Convex<PointType, LabelType>::contains(const OtherHalfplane&) const {
    return false;
}

template <class PointType, class LabelType>
template<RectangleConcept OtherRectangle>
constexpr bool Convex<PointType, LabelType>::contains(const OtherRectangle& other) const {
    if (other.empty()) {
        // The empty set is a subset of every shape, its boundary and its
        // interior alike.
        return true;
    }
    if (!bbox().contains(other)) {
        return false;
    }
    for (size_t i = 0; i < 4; ++i) {
        if (!contains(other[i])) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<TriangleConcept OtherTriangle>
constexpr bool Convex<PointType, LabelType>::contains(const OtherTriangle& other) const {
    if (!bbox().contains(other)) {
        return false;
    }
    for (size_t i = 0; i < 3; ++i) {
        if (!contains(other[i])) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<ConvexConcept OtherConvex>
constexpr bool Convex<PointType, LabelType>::contains(const OtherConvex& other) const {
    // The empty set is a subset of every shape, so it is tested first: an empty
    // polygon does contain another empty one.
    if (other.empty()) {
        return true;
    }
    if (empty()) {
        return false;
    }
    if (!bbox().contains(other.bbox())) {
        return false;
    }

    if (size() == 1) {
        return other.size() == 1 && (*this)[0] == other[0];
    }
    if (size() == 2 && other.size() == 1) {
        return Segment<PointType>((*this)[0], (*this)[1]).contains(other[0]);
    }
    if (size() == 2 && other.size() == 2) {
        return Segment<PointType>((*this)[0], (*this)[1]).contains(Segment<typename OtherConvex::PointType>(other[0], other[1]));
    }

    if (other.size() <= 2*size()) {
        for (size_t i = 0; i < other.size(); ++i) {
            if (!contains(other[i])) {
                return false;
            }
        }
    } else {
        for (const auto& edge : orientedEdgesView()) {
            if (!edge.leftHalfplane().contains(other)) {
                return false;
            }
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<DiskConcept OtherDisk>
constexpr bool Convex<PointType, LabelType>::contains(const OtherDisk& other) const {
    if (const auto center = other.getIfPoint()) {
        // A radius-zero disk is its center. The edge loop below cannot answer
        // for it: a degenerate convex hull has no pair of bounding half-planes
        // to cut the carrier line down to the point or segment it really is,
        // whereas contains(Point) reads that carrier directly.
        return contains(*center);
    }
    for (const auto& edge : orientedEdgesView()) {
        if (!edge.leftHalfplane().contains(other)) {
            return false;
        }
    }
    return true;
}


// ---------------------------------------------------------------------------
// Polygon

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Polygon<PointType, LabelType>::contains(const OtherPoint& point) const {
    const std::size_t n = size();
    if (n == 0) {
        return false;
    }

    // Orientation signs, lexicographic order, and Segment::contains are all
    // translation-invariant, so translate the query once into the polygon's
    // untranslated frame and compare against the raw points_ — instead of
    // adding the translation to every vertex (cheaper, notably for rationals).
    const auto p = point - translation_;

    if (n == 1) {
        return points_[0] == p;
    }
    if (n == 2) {
        return Segment<PointType>(points_[0], points_[1]).contains(p);
    }

    // The closed polygon includes its boundary, so an explicit edge check
    // comes first (the winding number is unreliable on the boundary).
    for (std::size_t i = 0; i < n; ++i) {
        if (Segment<PointType>(points_[i], points_[(i + 1) % n]).contains(p)) {
            return true;
        }
    }

    // Interior test via the winding number, using exact orientation signs for
    // the left/right classification of each upward/downward crossing edge.
    int winding = 0;
    for (std::size_t i = 0; i < n; ++i) {
        const PointType& a = points_[i];
        const PointType& b = points_[(i + 1) % n];
        if (!(a.y() > p.y())) {                // a.y() <= p.y()
            if (b.y() > p.y()) {               // upward crossing
                if (orientationSign(a, b, p) > 0) {
                    ++winding;                 // p strictly left of the edge
                }
            }
        } else {                               // a.y() > p.y()
            if (!(b.y() > p.y())) {             // downward crossing
                if (orientationSign(a, b, p) < 0) {
                    --winding;                 // p strictly right of the edge
                }
            }
        }
    }
    return winding != 0;
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Polygon<PointType, LabelType>::contains(const OtherSegment& other) const {
    if (size() == 0) {
        return false;
    }
    if (!contains(other.min()) || !contains(other.max())) {
        return false;
    }
    if (other.isDegenerate()) {
        return contains(other.min());
    }

    const std::size_t n = size();

    // Translate the segment once into the polygon's untranslated frame and
    // work against the raw points_, rather than adding the translation to
    // every vertex; all the predicates below are translation-invariant.
    const auto a = other.min() - translation_;
    const auto b = other.max() - translation_;

    // Does the ray from polygon vertex p[i] toward q head into the closed
    // polygon? The interior at a vertex is the intersection of the two edge
    // half-planes when the vertex is convex, and their union when reflex
    // (the polygon is CCW, so the interior lies to the left of each edge).
    auto entersClosedAtVertex = [&](std::size_t i, const auto& q) -> bool {
        const PointType& u = points_[(i + n - 1) % n];
        const PointType& w = points_[i];
        const PointType& x = points_[(i + 1) % n];
        const auto incoming = orientationSign(u, w, q);
        const auto outgoing = orientationSign(w, x, q);
        const auto turn = orientationSign(u, w, x);
        if (turn > 0) {                      // convex vertex: between both edges
            return incoming >= 0 && outgoing >= 0;
        }
        if (turn < 0) {                      // reflex vertex: outside the small wedge
            return incoming >= 0 || outgoing >= 0;
        }
        return incoming >= 0;                // straight pass-through
    };

    for (std::size_t i = 0; i < n; ++i) {
        const PointType& c = points_[i];
        const PointType& d = points_[(i + 1) % n];

        const auto cSide = orientationSign(a, b, c);
        const auto dSide = orientationSign(a, b, d);
        const auto aSide = orientationSign(c, d, a);
        const auto bSide = orientationSign(c, d, b);

        // (A) The edge interior crosses the segment transversally, so the
        // segment passes from one side of the boundary to the other.
        const bool straddleEdge = (cSide < 0 && dSide > 0) || (cSide > 0 && dSide < 0);
        const bool straddleSeg  = (aSide < 0 && bSide > 0) || (aSide > 0 && bSide < 0);
        if (straddleEdge && straddleSeg) {
            return false;
        }

        // (B) Vertex c lies strictly inside the open segment: the segment
        // passes straight through it and must stay inside on both sides.
        if (collinear(a, b, c) && a < c && c < b) {
            if (!entersClosedAtVertex(i, a) || !entersClosedAtVertex(i, b)) {
                return false;
            }
        }

        // (C) A segment endpoint lies strictly inside this edge: the segment
        // must continue to the interior (left) side, not the exterior.
        if (aSide == 0 && a != c && a != d && Segment<PointType>(c, d).contains(a)) {
            if (bSide < 0) {
                return false;
            }
        }
        if (bSide == 0 && b != c && b != d && Segment<PointType>(c, d).contains(b)) {
            if (aSide < 0) {
                return false;
            }
        }

        // (D) A segment endpoint coincides with vertex c: the segment must
        // leave the vertex into the closed polygon.
        if (a == c && !entersClosedAtVertex(i, b)) {
            return false;
        }
        if (b == c && !entersClosedAtVertex(i, a)) {
            return false;
        }
    }

    return true;
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Polygon<PointType, LabelType>::contains(const OtherOrientedSegment& other) const {
    return contains(Segment<typename OtherOrientedSegment::PointType>(other.source(), other.target()));
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Polygon<PointType, LabelType>::contains(const OtherLine& other) const {
    return other.isDegenerate() && contains(other.min());
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Polygon<PointType, LabelType>::contains(const OtherOrientedLine& other) const {
    return other.isDegenerate() && contains(other.source());
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool Polygon<PointType, LabelType>::contains(const OtherRay& other) const {
    return other.isDegenerate() && contains(other.source());
}

template <class PointType, class LabelType>
template<HalfplaneConcept OtherHalfplane>
constexpr bool Polygon<PointType, LabelType>::contains(const OtherHalfplane& other) const {
    return other.isDegenerate() && contains(other.source());
}

// For a simple polygon (no holes) a bounded shape is contained iff every one
// of its edges is contained, so the region overloads reduce to edge checks.
template <class PointType, class LabelType>
template<RectangleConcept OtherRectangle>
constexpr bool Polygon<PointType, LabelType>::contains(const OtherRectangle& other) const {
    if (other.empty()) {
        // The empty set is a subset of every shape, its boundary and its
        // interior alike.
        return true;
    }
    for (std::size_t i = 0; i < other.size(); ++i) {
        if (!contains(Segment<typename OtherRectangle::PointType>(other[i], other[(i + 1) % other.size()]))) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<TriangleConcept OtherTriangle>
constexpr bool Polygon<PointType, LabelType>::contains(const OtherTriangle& other) const {
    for (std::size_t i = 0; i < other.size(); ++i) {
        if (!contains(Segment<typename OtherTriangle::PointType>(other[i], other[(i + 1) % other.size()]))) {
            return false;
        }
    }
    return true;
}

// A convex polygon's boundary is exactly two lex-monotone chains — its lower and
// upper hull — so we can run the edgesCross fast path of contains(Polygon)
// without building a BoundaryChains decomposition (or an asPolygon copy) of the
// convex: just test this polygon's chains against those two known hull chains.
template <class PointType, class LabelType>
template<ConvexConcept OtherConvex>
constexpr bool Polygon<PointType, LabelType>::contains(const OtherConvex& other) const {
    using OtherPoint = typename OtherConvex::PointType;
    if (other.size() == 0) {
        return true;
    }
    if (!bbox().contains(other.bbox())) {
        return false;
    }
    if (other.size() == 1) {
        return contains(other[0]);
    }

    if (!contains(other[0])) {
        return false;
    }

    const MonotoneChain<OtherPoint> lower = other.lowerHull();
    const MonotoneChain<OtherPoint> upper = other.upperHull();

    bool boundaries_intersect = false;
    BoundaryChains<Polygon> mine(*this);
    while (!mine.exhausted()) {
        const auto& chain = mine.produceNext();
        for (const MonotoneChain<OtherPoint>* their : {&lower, &upper}) {
            if (chain.intersects(*their)) {
                boundaries_intersect = true;
                if (chain.edgesCross(*their)) {
                    return false;
                }
            }
        }
    }

    if (!boundaries_intersect) {
        return true;
    }

    // The boundaries intersect without crossing, check everything quadratically
    for (std::size_t i = 0; i < other.size(); ++i) {
        if (!contains(Segment<OtherPoint>(other[i], other[(i + 1) % other.size()]))) {
            return false;
        }
    }

    return true;
}

template <class PointType, class LabelType>
template<PolygonConcept OtherPolygon>
constexpr bool Polygon<PointType, LabelType>::contains(const OtherPolygon& other) const {
    if (other.size() == 0) {
        return true;
    }
    if (!bbox().contains(other.bbox())) {
        return false;
    }
    if (other.size() == 1) {
        return contains(other[0]);
    }
    // A polygon collapsed to a single point is exactly that point, and its
    // boundary has no lexicographic break for BoundaryChains to split on.
    if (const auto vertex = other.getIfPoint()) {
        return contains(*vertex);
    }
    if (isPoint()) {
        // `other` has two distinct vertices by the test above, so a single
        // point cannot contain it.
        return false;
    }

    if (!contains(other.get(0))) {
        return false;
    }

    bool boundaries_intersect = false;

    BoundaryChains<Polygon> mine(*this);
    BoundaryChains<OtherPolygon> theirs(other);
    while (!mine.exhausted() || !theirs.exhausted()) {
        if (!mine.exhausted()) {
            const auto& chain = mine.produceNext();
            for (const auto& their : theirs.produced()) {
                if (chain.intersects(their)) {
                    boundaries_intersect = true;
                    if (chain.edgesCross(their)) {
                        return false;
                    }
                }
            }
        }
        if (!theirs.exhausted()) {
            const auto& chain = theirs.produceNext();
            for (const auto& my : mine.produced()) {
                if (chain.intersects(my)) {
                    boundaries_intersect = true;
                    if (chain.edgesCross(my)) {
                        return false;
                    }
                }
            }
        }
    }

    if (!boundaries_intersect) {
        return true;
    }

    // The boundaries intersect without crossing, check everything quadratically
    for (std::size_t i = 0; i < other.size(); ++i) {
        if (!contains(Segment<typename OtherPolygon::PointType>(other[i], other[(i + 1) % other.size()]))) {
            return false;
        }
    }

    return true;
}

template <class PointType, class LabelType>
template<DiskConcept OtherDisk>
constexpr bool Polygon<PointType, LabelType>::contains(const OtherDisk& other) const {
    if (other.isDegenerate()) {
        return contains(other.a());
    }

    if (!other.pointInsideInteriorContainedIn(*this)) {
        return false;
    }

    for (const auto& edge : edgesView()) {
        if (other.interiorsIntersect(edge)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
constexpr bool Polygon<PointType, LabelType>::contains(const Shape<PointType>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->contains(value);
        },
        other.variant());
}

template <class PointType, class LabelType>
template <PointConcept OtherPoint>
constexpr bool Convex<PointType, LabelType>::contains(const Shape<OtherPoint>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->contains(value);
        },
        other.variant());
}

template <class Number, class Label>
template<PolygonConcept OtherPolygon>
constexpr bool Point<Number, Label>::contains(const OtherPolygon& other) const {
    for (const auto& vertex : other) {
        if (!contains(vertex)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<PolygonConcept OtherPolygon>
constexpr bool Segment<PointType, LabelType>::contains(const OtherPolygon& other) const {
    for (const auto& vertex : other) {
        if (!contains(vertex)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<PolygonConcept OtherPolygon>
constexpr bool OrientedSegment<PointType, LabelType>::contains(const OtherPolygon& other) const {
    return asSegment().contains(other);
}

template <class PointType, class LabelType>
template<PolygonConcept OtherPolygon>
constexpr bool Line<PointType, LabelType>::contains(const OtherPolygon& other) const {
    for (const auto& vertex : other) {
        if (!contains(vertex)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<PolygonConcept OtherPolygon>
constexpr bool OrientedLine<PointType, LabelType>::contains(const OtherPolygon& other) const {
    for (const auto& vertex : other) {
        if (!contains(vertex)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<PolygonConcept OtherPolygon>
constexpr bool Ray<PointType, LabelType>::contains(const OtherPolygon& other) const {
    for (const auto& vertex : other) {
        if (!contains(vertex)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<PolygonConcept OtherPolygon>
constexpr bool Halfplane<PointType, LabelType>::contains(const OtherPolygon& other) const {
    for (const auto& vertex : other) {
        if (!contains(vertex)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<PolygonConcept OtherPolygon>
constexpr bool Rectangle<PointType, LabelType>::contains(const OtherPolygon& other) const {
    if (empty()) {
        // The empty set is a subset of itself and of nothing else.
        return detail::coversNoPoint(other);
    }
    for (const auto& vertex : other) {
        if (!contains(vertex)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<PolygonConcept OtherPolygon>
constexpr bool Triangle<PointType, LabelType>::contains(const OtherPolygon& other) const {
    for (const auto& vertex : other) {
        if (!contains(vertex)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<PolygonConcept OtherPolygon>
constexpr bool Convex<PointType, LabelType>::contains(const OtherPolygon& other) const {
    for (const auto& vertex : other) {
        if (!contains(vertex)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<PolygonConcept OtherPolygon>
constexpr bool Disk<PointType, LabelType>::contains(const OtherPolygon& other) const {
    for (const auto& vertex : other) {
        if (!contains(vertex)) {
            return false;
        }
    }
    return true;
}

/**
 * @section predicates-monotonechain MonotoneChain
 * Weakly x-monotone chain predicates: point location by binary search on x,
 * straight sub-path containment, and degenerate reductions for the shapes a
 * 1-dimensional bounded set can contain.
 */

template <class PointType, class LabelType, class Storage>
template<PointConcept OtherPoint>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::contains(const OtherPoint& point) const {
    // The slice of the chain at any x is a single vertical segment (possibly a
    // point), so the point is on the chain iff the chain passes both weakly
    // below and weakly above it.
    return isBelow(point).has_value() && isAbove(point).has_value();
}

template <class PointType, class LabelType, class Storage>
template<SegmentConcept OtherSegment>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::contains(const OtherSegment& other) const {
    if (!contains(other.min()) || !contains(other.max())) {
        return false;
    }
    // Both endpoints lie on the chain, which is a monotone arc in the
    // lexicographic order, so the sub-arc between them equals the segment iff
    // it never bends: every chain vertex lexicographically between the
    // endpoints must be collinear with the segment. The scan stops at the
    // first bend or at the far endpoint.
    const auto first = std::upper_bound(
        points_.begin(), points_.end(), other.min(),
        [this](const auto& value, const PointType& p) { return value < p + translation_; });
    for (auto it = first; it != points_.end(); ++it) {
        const PointType vertex = *it + translation_;
        if (!(vertex < other.max())) {
            break;
        }
        if (orientationSign(other.min(), other.max(), vertex) != 0) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType, class Storage>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::contains(const OtherOrientedSegment& other) const {
    return contains(static_cast<Segment<typename OtherOrientedSegment::PointType>>(other));
}

template <class PointType, class LabelType, class Storage>
template<LineConcept OtherLine>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::contains(const OtherLine& other) const {
    return other.isDegenerate() && contains(other.min());
}

template <class PointType, class LabelType, class Storage>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::contains(const OtherOrientedLine& other) const {
    return other.isDegenerate() && contains(other.source());
}

template <class PointType, class LabelType, class Storage>
template<RayConcept OtherRay>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::contains(const OtherRay& other) const {
    return other.isDegenerate() && contains(other.source());
}

template <class PointType, class LabelType, class Storage>
template<HalfplaneConcept OtherHalfplane>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::contains(const OtherHalfplane& other) const {
    return other.isDegenerate() && contains(other.source());
}

template <class PointType, class LabelType, class Storage>
template<RectangleConcept OtherRectangle>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::contains(const OtherRectangle& other) const {
    if (other.empty()) {
        // The empty set is a subset of every shape, its boundary and its
        // interior alike.
        return true;
    }
    if (!other.isDegenerate()) {
        return false;
    }
    if (other.min() == other.max()) {
        return contains(other.min());
    }
    return contains(Segment<typename OtherRectangle::PointType>(other.min(), other.max()));
}

template <class PointType, class LabelType, class Storage>
template<TriangleConcept OtherTriangle>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::contains(const OtherTriangle& other) const {
    if (!other.isDegenerate()) {
        return false;
    }
    if (other.a() == other.c()) {
        return contains(other.a());
    }
    return contains(Segment<typename OtherTriangle::PointType>(other.a(), other.c()));
}

template <class PointType, class LabelType, class Storage>
template<ConvexConcept OtherConvex>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::contains(const OtherConvex& other) const {
    if (other.size() == 0) {
        return true;
    }
    if (other.size() == 1) {
        return contains(other[0]);
    }
    if (other.size() == 2) {
        return contains(Segment<typename OtherConvex::PointType>(other[0], other[1]));
    }
    return false;
}

template <class PointType, class LabelType, class Storage>
template<PolygonConcept OtherPolygon>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::contains(const OtherPolygon& other) const {
    // A polygon with area is never on the 1-dimensional chain. Without area it
    // is exactly the union of its edges, so the edge fold decides -- and it has
    // to be the edges: a bent chain may pass through every vertex without
    // containing the edges between them, so a vertex fold would not be enough.
    if (!other.isDegenerate()) {
        return false;
    }
    if (other.size() == 0) {
        return true;
    }
    if (other.size() == 1) {
        return contains(other[0]);
    }
    for (const auto& edge : other.edgesView()) {
        if (!contains(edge)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType, class Storage>
template<DiskConcept OtherDisk>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::contains(const OtherDisk& other) const {
    return other.a() == other.b() && other.b() == other.c() && contains(other.a());
}

template <class PointType, class LabelType, class Storage>
template<MonotoneChainConcept OtherChain>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::contains(const OtherChain& other) const {
    if (other.empty()) {
        return true;
    }
    if (other.size() == 1) {
        return contains(other[0]);
    }
    // The other chain is exactly the union of its edges, and each edge must be
    // a straight sub-path of this chain.
    for (std::size_t i = 0; i + 1 < other.size(); ++i) {
        if (!contains(Segment<typename OtherChain::PointType>(other[i], other[i + 1]))) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType, class Storage>
template<PointConcept OtherPoint>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::contains(const Shape<OtherPoint>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->contains(value);
        },
        other.variant());
}

// Every shape below is a convex point set, so it contains the chain iff it
// contains all of the chain's vertices (an empty chain is trivially contained).

template <class Number, class Label>
template<MonotoneChainConcept OtherChain>
constexpr bool Point<Number, Label>::contains(const OtherChain& other) const {
    for (const auto& vertex : other) {
        if (!contains(vertex)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<MonotoneChainConcept OtherChain>
constexpr bool Segment<PointType, LabelType>::contains(const OtherChain& other) const {
    for (const auto& vertex : other) {
        if (!contains(vertex)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<MonotoneChainConcept OtherChain>
constexpr bool OrientedSegment<PointType, LabelType>::contains(const OtherChain& other) const {
    return asSegment().contains(other);
}

template <class PointType, class LabelType>
template<MonotoneChainConcept OtherChain>
constexpr bool Line<PointType, LabelType>::contains(const OtherChain& other) const {
    for (const auto& vertex : other) {
        if (!contains(vertex)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<MonotoneChainConcept OtherChain>
constexpr bool OrientedLine<PointType, LabelType>::contains(const OtherChain& other) const {
    return asLine().contains(other);
}

template <class PointType, class LabelType>
template<MonotoneChainConcept OtherChain>
constexpr bool Ray<PointType, LabelType>::contains(const OtherChain& other) const {
    for (const auto& vertex : other) {
        if (!contains(vertex)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<MonotoneChainConcept OtherChain>
constexpr bool Halfplane<PointType, LabelType>::contains(const OtherChain& other) const {
    for (const auto& vertex : other) {
        if (!contains(vertex)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<MonotoneChainConcept OtherChain>
constexpr bool Rectangle<PointType, LabelType>::contains(const OtherChain& other) const {
    if (empty()) {
        // The empty set is a subset of itself and of nothing else.
        return detail::coversNoPoint(other);
    }
    for (const auto& vertex : other) {
        if (!contains(vertex)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<MonotoneChainConcept OtherChain>
constexpr bool Triangle<PointType, LabelType>::contains(const OtherChain& other) const {
    for (const auto& vertex : other) {
        if (!contains(vertex)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<MonotoneChainConcept OtherChain>
constexpr bool Disk<PointType, LabelType>::contains(const OtherChain& other) const {
    for (const auto& vertex : other) {
        if (!contains(vertex)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<MonotoneChainConcept OtherChain>
constexpr bool Convex<PointType, LabelType>::contains(const OtherChain& other) const {
    for (const auto& vertex : other) {
        if (!contains(vertex)) {
            return false;
        }
    }
    return true;
}

// A polygon is generally not convex, so it must contain every chain edge.
template <class PointType, class LabelType>
template<MonotoneChainConcept OtherChain>
constexpr bool Polygon<PointType, LabelType>::contains(const OtherChain& other) const {
    if (other.empty()) {
        return true;
    }
    if (other.size() == 1) {
        return contains(other[0]);
    }
    for (std::size_t i = 0; i + 1 < other.size(); ++i) {
        if (!contains(Segment<typename OtherChain::PointType>(other[i], other[i + 1]))) {
            return false;
        }
    }
    return true;
}

/**
 * @section predicates-polyline Polyline
 * Open polygonal chain predicates: the polyline keeps its vertices in
 * traversal order and may self-intersect, so there is no monotone structure
 * to exploit — point location scans the edges and segment containment sweeps
 * the union of collinear edge overlaps.
 */

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Polyline<PointType, LabelType>::contains(const OtherPoint& point) const {
    if (empty()) {
        return false;
    }
    if (size() == 1) {
        return (*this)[0] == point;
    }
    for (const auto& edge : edgesView()) {
        if (edge.contains(point)) {
            return true;
        }
    }
    return false;
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Polyline<PointType, LabelType>::contains(const OtherSegment& other) const {
    if (other.min() == other.max()) {
        return contains(other.min());
    }
    if (size() < 2) {
        // Without an edge the polyline cannot cover a positive-length segment.
        return false;
    }
    // A self-intersecting polyline can cover the segment with several,
    // possibly non-consecutive collinear edges, so collect the overlap of
    // every edge lying on the segment's supporting line and check that the
    // union leaves no gap. Collinear points compare consistently along the
    // line in the lexicographic point order, so the overlaps are ordinary
    // closed intervals in that order.
    using CommonPoint =
        Point<std::common_type_t<NumberType, typename OtherSegment::PointType::NumberType>>;
    std::vector<std::pair<CommonPoint, CommonPoint>> overlaps;
    for (const auto& edge : edgesView()) {
        if (orientationSign(other.min(), other.max(), edge.min()) != 0 ||
            orientationSign(other.min(), other.max(), edge.max()) != 0) {
            continue;  // the edge leaves the segment's supporting line
        }
        const CommonPoint lo =
            (edge.min() < other.min()) ? CommonPoint(other.min()) : CommonPoint(edge.min());
        const CommonPoint hi =
            (other.max() < edge.max()) ? CommonPoint(other.max()) : CommonPoint(edge.max());
        if (hi < lo) {
            continue;  // collinear but disjoint from the segment
        }
        overlaps.emplace_back(lo, hi);
    }
    std::sort(overlaps.begin(), overlaps.end());
    CommonPoint covered(other.min());
    for (const auto& [lo, hi] : overlaps) {
        if (covered < lo) {
            return false;  // the part between covered and lo is off the polyline
        }
        if (covered < hi) {
            covered = hi;
        }
    }
    return !(covered < CommonPoint(other.max()));
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Polyline<PointType, LabelType>::contains(const OtherOrientedSegment& other) const {
    return contains(static_cast<Segment<typename OtherOrientedSegment::PointType>>(other));
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Polyline<PointType, LabelType>::contains(const OtherLine& other) const {
    return other.isDegenerate() && contains(other.min());
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Polyline<PointType, LabelType>::contains(const OtherOrientedLine& other) const {
    return other.isDegenerate() && contains(other.source());
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool Polyline<PointType, LabelType>::contains(const OtherRay& other) const {
    return other.isDegenerate() && contains(other.source());
}

template <class PointType, class LabelType>
template<HalfplaneConcept OtherHalfplane>
constexpr bool Polyline<PointType, LabelType>::contains(const OtherHalfplane& other) const {
    return other.isDegenerate() && contains(other.source());
}

template <class PointType, class LabelType>
template<RectangleConcept OtherRectangle>
constexpr bool Polyline<PointType, LabelType>::contains(const OtherRectangle& other) const {
    if (other.empty()) {
        // The empty set is a subset of every shape, its boundary and its
        // interior alike.
        return true;
    }
    if (!other.isDegenerate()) {
        return false;
    }
    if (other.min() == other.max()) {
        return contains(other.min());
    }
    return contains(Segment<typename OtherRectangle::PointType>(other.min(), other.max()));
}

template <class PointType, class LabelType>
template<TriangleConcept OtherTriangle>
constexpr bool Polyline<PointType, LabelType>::contains(const OtherTriangle& other) const {
    if (!other.isDegenerate()) {
        return false;
    }
    if (other.a() == other.c()) {
        return contains(other.a());
    }
    return contains(Segment<typename OtherTriangle::PointType>(other.a(), other.c()));
}

template <class PointType, class LabelType>
template<ConvexConcept OtherConvex>
constexpr bool Polyline<PointType, LabelType>::contains(const OtherConvex& other) const {
    if (other.size() == 0) {
        return true;
    }
    if (other.size() == 1) {
        return contains(other[0]);
    }
    if (other.size() == 2) {
        return contains(Segment<typename OtherConvex::PointType>(other[0], other[1]));
    }
    return false;
}

template <class PointType, class LabelType>
template<PolygonConcept OtherPolygon>
constexpr bool Polyline<PointType, LabelType>::contains(const OtherPolygon& other) const {
    // A polygon with area is never on the 1-dimensional polyline, however much
    // of its boundary is: a polyline tracing the whole boundary still misses
    // everything inside it. Without area the polygon is exactly the union of
    // its edges, and the fold decides.
    if (!other.isDegenerate()) {
        return false;
    }
    if (other.size() == 0) {
        return true;
    }
    if (other.size() == 1) {
        return contains(other[0]);
    }
    for (const auto& edge : other.edgesView()) {
        if (!contains(edge)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<DiskConcept OtherDisk>
constexpr bool Polyline<PointType, LabelType>::contains(const OtherDisk& other) const {
    return other.a() == other.b() && other.b() == other.c() && contains(other.a());
}

template <class PointType, class LabelType>
template<MonotoneChainConcept OtherChain>
constexpr bool Polyline<PointType, LabelType>::contains(const OtherChain& other) const {
    if (other.empty()) {
        return true;
    }
    if (other.size() == 1) {
        return contains(other[0]);
    }
    // The chain is exactly the union of its edges.
    for (std::size_t i = 0; i + 1 < other.size(); ++i) {
        if (!contains(Segment<typename OtherChain::PointType>(other[i], other[i + 1]))) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<PolylineConcept OtherPolyline>
constexpr bool Polyline<PointType, LabelType>::contains(const OtherPolyline& other) const {
    if (other.empty()) {
        return true;
    }
    if (other.size() == 1) {
        return contains(other[0]);
    }
    // The other polyline is exactly the union of its edges.
    for (std::size_t i = 0; i + 1 < other.size(); ++i) {
        if (!contains(Segment<typename OtherPolyline::PointType>(other[i], other[i + 1]))) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Polyline<PointType, LabelType>::contains(const Shape<OtherPoint>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->contains(value);
        },
        other.variant());
}

// A point and a segment are convex point sets, so they contain the polyline
// iff they contain all of its vertices (an empty polyline is trivially
// contained).

template <class Number, class Label>
template<PolylineConcept OtherPolyline>
constexpr bool Point<Number, Label>::contains(const OtherPolyline& other) const {
    for (const auto& vertex : other) {
        if (!contains(vertex)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<PolylineConcept OtherPolyline>
constexpr bool Segment<PointType, LabelType>::contains(const OtherPolyline& other) const {
    for (const auto& vertex : other) {
        if (!contains(vertex)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<PolylineConcept OtherPolyline>
constexpr bool OrientedSegment<PointType, LabelType>::contains(const OtherPolyline& other) const {
    return asSegment().contains(other);
}

// Every shape below is a convex point set, so it contains the polyline iff it
// contains all of the polyline's vertices (an empty polyline is trivially
// contained).

template <class PointType, class LabelType>
template<PolylineConcept OtherPolyline>
constexpr bool Line<PointType, LabelType>::contains(const OtherPolyline& other) const {
    for (const auto& vertex : other) {
        if (!contains(vertex)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<PolylineConcept OtherPolyline>
constexpr bool OrientedLine<PointType, LabelType>::contains(const OtherPolyline& other) const {
    return asLine().contains(other);
}

template <class PointType, class LabelType>
template<PolylineConcept OtherPolyline>
constexpr bool Ray<PointType, LabelType>::contains(const OtherPolyline& other) const {
    for (const auto& vertex : other) {
        if (!contains(vertex)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<PolylineConcept OtherPolyline>
constexpr bool Halfplane<PointType, LabelType>::contains(const OtherPolyline& other) const {
    for (const auto& vertex : other) {
        if (!contains(vertex)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<PolylineConcept OtherPolyline>
constexpr bool Rectangle<PointType, LabelType>::contains(const OtherPolyline& other) const {
    if (empty()) {
        // The empty set is a subset of itself and of nothing else.
        return detail::coversNoPoint(other);
    }
    for (const auto& vertex : other) {
        if (!contains(vertex)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<PolylineConcept OtherPolyline>
constexpr bool Triangle<PointType, LabelType>::contains(const OtherPolyline& other) const {
    for (const auto& vertex : other) {
        if (!contains(vertex)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<PolylineConcept OtherPolyline>
constexpr bool Disk<PointType, LabelType>::contains(const OtherPolyline& other) const {
    for (const auto& vertex : other) {
        if (!contains(vertex)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<PolylineConcept OtherPolyline>
constexpr bool Convex<PointType, LabelType>::contains(const OtherPolyline& other) const {
    for (const auto& vertex : other) {
        if (!contains(vertex)) {
            return false;
        }
    }
    return true;
}

// A monotone chain is 1-dimensional and generally bent, so it must contain
// every polyline edge as a straight sub-path.
template <class PointType, class LabelType, class Storage>
template<PolylineConcept OtherPolyline>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::contains(const OtherPolyline& other) const {
    if (other.empty()) {
        return true;
    }
    if (other.size() == 1) {
        return contains(other[0]);
    }
    for (std::size_t i = 0; i + 1 < other.size(); ++i) {
        if (!contains(Segment<typename OtherPolyline::PointType>(other[i], other[i + 1]))) {
            return false;
        }
    }
    return true;
}

// A polygon is generally not convex, so it must contain every polyline edge.
template <class PointType, class LabelType>
template<PolylineConcept OtherPolyline>
constexpr bool Polygon<PointType, LabelType>::contains(const OtherPolyline& other) const {
    if (other.empty()) {
        return true;
    }
    if (other.size() == 1) {
        return contains(other[0]);
    }
    for (std::size_t i = 0; i + 1 < other.size(); ++i) {
        if (!contains(Segment<typename OtherPolyline::PointType>(other[i], other[i + 1]))) {
            return false;
        }
    }
    return true;
}


// ---------------------------------------------------------------------------
// HalfplaneIntersection

template <class PointType, class LabelType>
template <PointConcept OtherPoint>
constexpr bool HalfplaneIntersection<PointType, LabelType>::contains(const OtherPoint& point) const {
    return pointStatus(point) >= 0;
}

template <class PointType, class LabelType>
template <SegmentConcept OtherSegment>
constexpr bool HalfplaneIntersection<PointType, LabelType>::contains(const OtherSegment& other) const {
    // The region is convex, so containing both endpoints contains the segment.
    return contains(other[0]) && contains(other[1]);
}

template <class PointType, class LabelType>
template <OrientedSegmentConcept OtherOrientedSegment>
constexpr bool HalfplaneIntersection<PointType, LabelType>::contains(const OtherOrientedSegment& other) const {
    return contains(other[0]) && contains(other[1]);
}

template <class PointType, class LabelType>
template <LineConcept OtherLine>
constexpr bool HalfplaneIntersection<PointType, LabelType>::contains(const OtherLine& other) const {
    // A half-plane contains a line only when its boundary is parallel to it,
    // and the canonical form stores at most one half-plane per direction, so
    // at most two constraints (one per orientation) can contain a line.
    if (empty() || size() > 2) {
        return false;
    }
    for (const auto& halfplane : halfplanes_) {
        if (!halfplane.contains(other)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template <OrientedLineConcept OtherOrientedLine>
constexpr bool HalfplaneIntersection<PointType, LabelType>::contains(const OtherOrientedLine& other) const {
    return contains(other.asLine());
}

template <class PointType, class LabelType>
template <RayConcept OtherRay>
constexpr bool HalfplaneIntersection<PointType, LabelType>::contains(const OtherRay& other) const {
    // Containing the source and the ray's direction lying in the recession
    // cone keeps the whole ray inside (the distance to each boundary line is
    // affine along the ray and stays nonnegative).
    if (empty()) {
        return false;
    }
    if (!contains(other.source())) {
        return false;
    }
    const Halfplane<typename OtherRay::PointType> forward(other.source(), other.target());
    return recessionContains(forward);
}

template <class PointType, class LabelType>
template <HalfplaneConcept OtherHalfplane>
constexpr bool HalfplaneIntersection<PointType, LabelType>::contains(const OtherHalfplane& other) const {
    // Every stored constraint must contain the half-plane, which requires the
    // same boundary direction; the canonical form therefore admits at most one
    // stored constraint (or none: the whole plane).
    if (empty() || size() > 1) {
        return false;
    }
    return halfplanes_.empty() || halfplanes_[0].contains(other);
}

template <class PointType, class LabelType>
template <RectangleConcept OtherRectangle>
constexpr bool HalfplaneIntersection<PointType, LabelType>::contains(const OtherRectangle& other) const {
    if (other.empty()) {
        // The empty set is a subset of every shape, its boundary and its
        // interior alike.
        return true;
    }
    // The region is convex, so containing the vertices contains the rectangle.
    const auto vertices = other.vertices();
    for (const auto& vertex : vertices) {
        if (!contains(vertex)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template <TriangleConcept OtherTriangle>
constexpr bool HalfplaneIntersection<PointType, LabelType>::contains(const OtherTriangle& other) const {
    return contains(other.a()) && contains(other.b()) && contains(other.c());
}

template <class PointType, class LabelType>
template <DiskConcept OtherDisk>
constexpr bool HalfplaneIntersection<PointType, LabelType>::contains(const OtherDisk& other) const {
    // The region contains the disk exactly when every stored constraint does.
    if (empty()) {
        return false;
    }
    if (const auto center = other.getIfPoint()) {
        // A radius-zero disk is its center, and contains(Point) is the
        // authoritative membership test for the region.
        return contains(*center);
    }
    for (const auto& halfplane : halfplanes_) {
        if (!halfplane.contains(other)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template <ConvexConcept OtherConvex>
constexpr bool HalfplaneIntersection<PointType, LabelType>::contains(const OtherConvex& other) const {
    // The region is convex, so containing the vertices contains the polygon.
    for (std::size_t i = 0; i < other.size(); ++i) {
        if (!contains(other[i])) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template <MonotoneChainConcept OtherChain>
constexpr bool HalfplaneIntersection<PointType, LabelType>::contains(const OtherChain& other) const {
    // The region is convex, so containing the vertices contains the chain.
    for (std::size_t i = 0; i < other.size(); ++i) {
        if (!contains(other[i])) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template <PolylineConcept OtherPolyline>
constexpr bool HalfplaneIntersection<PointType, LabelType>::contains(const OtherPolyline& other) const {
    // The region is convex, so containing the vertices contains the polyline.
    for (std::size_t i = 0; i < other.size(); ++i) {
        if (!contains(other[i])) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template <PolygonConcept OtherPolygon>
constexpr bool HalfplaneIntersection<PointType, LabelType>::contains(const OtherPolygon& other) const {
    // The region is convex, so containing the vertices contains the polygon
    // (its region is inside the vertices' convex hull).
    for (std::size_t i = 0; i < other.size(); ++i) {
        if (!contains(other[i])) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template <HalfplaneIntersectionConcept OtherRegion>
constexpr bool HalfplaneIntersection<PointType, LabelType>::contains(const OtherRegion& other) const {
    // The region contains the other region exactly when every stored
    // constraint does; the whole plane (no constraints) contains everything.
    if (other.empty()) {
        return true;
    }
    if (empty()) {
        return false;
    }
    for (const auto& halfplane : halfplanes_) {
        if (!halfplane.contains(other)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template <PointConcept OtherPoint>
constexpr bool HalfplaneIntersection<PointType, LabelType>::contains(const Shape<OtherPoint>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->contains(value);
        },
        other.variant());
}


// ---------------------------------------------------------------------------
// Reverse direction: lower-ranked shapes containing a HalfplaneIntersection.
//
// The empty region is a subset of every shape. A degenerate region reduces to
// its carrier shape (point, segment, ray, or line, with exact coordinates); a
// full-dimensional region can only be contained in two-dimensional shapes,
// where it reduces to half-plane redundancy tests or, for a bounded region,
// to its convex-polygon form.

template <class Number, class Label>
template <HalfplaneIntersectionConcept OtherRegion>
constexpr bool Point<Number, Label>::contains(const OtherRegion& other) const {
    // The region is inside the point exactly when it is inside all four
    // axis-aligned half-planes through it.
    const Point right(x() + Number(1), y());
    const Point up(x(), y() + Number(1));
    return detail::regionInsideHalfplane(other, Halfplane<Point>(*this, right)) &&
           detail::regionInsideHalfplane(other, Halfplane<Point>(right, *this)) &&
           detail::regionInsideHalfplane(other, Halfplane<Point>(*this, up)) &&
           detail::regionInsideHalfplane(other, Halfplane<Point>(up, *this));
}

template <class PointType, class LabelType>
template <HalfplaneIntersectionConcept OtherRegion>
constexpr bool Segment<PointType, LabelType>::contains(const OtherRegion& other) const {
    if (min() == max()) {
        // The clamps below are built from the segment's own direction, which a
        // collapsed segment does not have: every one of them would come out
        // undefined, and `insert` drops an undefined half-plane instead of
        // cutting with it, so the region would test as inside all four. The
        // point the segment covers answers the question with axis-aligned
        // clamps of its own.
        return min().contains(other);
    }
    // Inside the supporting line's slab and the two perpendicular clamps
    // through the endpoints.
    const auto a = min();
    const auto b = max();
    const auto dx = b.x() - a.x();
    const auto dy = b.y() - a.y();
    const PointType aClamp(a.x() + dy, a.y() - dx);
    const PointType bClamp(b.x() - dy, b.y() + dx);
    return detail::regionInsideHalfplane(other, Halfplane<PointType>(a, b)) &&
           detail::regionInsideHalfplane(other, Halfplane<PointType>(b, a)) &&
           detail::regionInsideHalfplane(other, Halfplane<PointType>(a, aClamp)) &&
           detail::regionInsideHalfplane(other, Halfplane<PointType>(b, bClamp));
}

template <class PointType, class LabelType>
template <HalfplaneIntersectionConcept OtherRegion>
constexpr bool OrientedSegment<PointType, LabelType>::contains(const OtherRegion& other) const {
    return asSegment().contains(other);
}

template <class PointType, class LabelType>
template <HalfplaneIntersectionConcept OtherRegion>
constexpr bool Line<PointType, LabelType>::contains(const OtherRegion& other) const {
    return detail::regionInsideHalfplane(other, Halfplane<PointType>((*this)[0], (*this)[1])) &&
           detail::regionInsideHalfplane(other, Halfplane<PointType>((*this)[1], (*this)[0]));
}

template <class PointType, class LabelType>
template <HalfplaneIntersectionConcept OtherRegion>
constexpr bool OrientedLine<PointType, LabelType>::contains(const OtherRegion& other) const {
    return asLine().contains(other);
}

template <class PointType, class LabelType>
template <HalfplaneIntersectionConcept OtherRegion>
constexpr bool Ray<PointType, LabelType>::contains(const OtherRegion& other) const {
    // Inside the supporting line's slab and the perpendicular clamp through
    // the source.
    const auto a = source();
    const auto b = target();
    const auto dx = b.x() - a.x();
    const auto dy = b.y() - a.y();
    const PointType clamp(a.x() + dy, a.y() - dx);
    return detail::regionInsideHalfplane(other, Halfplane<PointType>(a, b)) &&
           detail::regionInsideHalfplane(other, Halfplane<PointType>(b, a)) &&
           detail::regionInsideHalfplane(other, Halfplane<PointType>(a, clamp));
}

template <class PointType, class LabelType>
template <HalfplaneIntersectionConcept OtherRegion>
constexpr bool Halfplane<PointType, LabelType>::contains(const OtherRegion& other) const {
    return detail::regionInsideHalfplane(other, *this);
}

template <class PointType, class LabelType>
template <HalfplaneIntersectionConcept OtherRegion>
constexpr bool Rectangle<PointType, LabelType>::contains(const OtherRegion& other) const {
    if (empty()) {
        // The empty set is a subset of itself and of nothing else.
        return detail::coversNoPoint(other);
    }
    if (min() == max()) {
        return Point<typename PointType::NumberType>(min().x(), min().y()).contains(other);
    }
    if (isDegenerate()) {
        return Segment<PointType>(min(), max()).contains(other);
    }
    const PointType lo(min());
    const PointType hi(max());
    const PointType lohi(lo.x(), hi.y());
    const PointType hilo(hi.x(), lo.y());
    return detail::regionInsideHalfplane(other, Halfplane<PointType>(lo, hilo)) &&
           detail::regionInsideHalfplane(other, Halfplane<PointType>(hilo, hi)) &&
           detail::regionInsideHalfplane(other, Halfplane<PointType>(hi, lohi)) &&
           detail::regionInsideHalfplane(other, Halfplane<PointType>(lohi, lo));
}

template <class PointType, class LabelType>
template <HalfplaneIntersectionConcept OtherRegion>
constexpr bool Triangle<PointType, LabelType>::contains(const OtherRegion& other) const {
    if (other.empty()) {
        return true;
    }
    if (const auto vertex = getIfPoint()) {
        return vertex->contains(other);
    }
    if (const auto carrier = getIfSegment()) {
        return carrier->contains(other);
    }
    // Vertices are counterclockwise when non-degenerate, so each edge's
    // half-plane has the interior on its left.
    return detail::regionInsideHalfplane(other, Halfplane<PointType>(a(), b())) &&
           detail::regionInsideHalfplane(other, Halfplane<PointType>(b(), c())) &&
           detail::regionInsideHalfplane(other, Halfplane<PointType>(c(), a()));
}

template <class PointType, class LabelType>
template <HalfplaneIntersectionConcept OtherRegion>
constexpr bool Disk<PointType, LabelType>::contains(const OtherRegion& other) const {
    if (other.empty()) {
        return true;
    }
    if (!other.isBounded()) {
        return false;
    }
    // The disk is convex and the bounded region is the hull of its vertices.
    using E = detail::region_exact_number_t<typename OtherRegion::NumberType>;
    const auto vertices = other.template vertices<E>();
    for (const auto& vertex : vertices) {
        if (!contains(vertex)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template <HalfplaneIntersectionConcept OtherRegion>
constexpr bool Convex<PointType, LabelType>::contains(const OtherRegion& other) const {
    if (size() == 0) {
        return other.empty();
    }
    if (size() == 1) {
        return Point<typename PointType::NumberType>((*this)[0].x(), (*this)[0].y()).contains(other);
    }
    if (isDegenerate()) {
        return Segment<PointType>((*this)[0], (*this)[size() - 1]).contains(other);
    }
    for (std::size_t i = 0; i < size(); ++i) {
        if (!detail::regionInsideHalfplane(other, Halfplane<PointType>((*this)[i], get(static_cast<std::ptrdiff_t>(i) + 1)))) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType, class Storage>
template <HalfplaneIntersectionConcept OtherRegion>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::contains(const OtherRegion& other) const {
    if (other.empty()) {
        return true;
    }
    if (!other.isDegenerate()) {
        return false;  // a chain is one-dimensional
    }
    return std::visit(
        [this](const auto& carrier) {
            using Carrier = std::remove_cvref_t<decltype(carrier)>;
            if constexpr (detail::is_point_v<Carrier> || detail::is_segment_v<Carrier>) {
                return this->contains(carrier);
            } else {
                return false;  // a bounded chain never contains a ray or a line
            }
        },
        detail::degenerateRegionCarrier(other));
}

template <class PointType, class LabelType>
template <HalfplaneIntersectionConcept OtherRegion>
constexpr bool Polyline<PointType, LabelType>::contains(const OtherRegion& other) const {
    if (other.empty()) {
        return true;
    }
    if (!other.isDegenerate()) {
        return false;  // a polyline is one-dimensional
    }
    return std::visit(
        [this](const auto& carrier) {
            using Carrier = std::remove_cvref_t<decltype(carrier)>;
            if constexpr (detail::is_point_v<Carrier> || detail::is_segment_v<Carrier>) {
                return this->contains(carrier);
            } else {
                return false;  // a bounded polyline never contains a ray or a line
            }
        },
        detail::degenerateRegionCarrier(other));
}

template <class PointType, class LabelType>
template <HalfplaneIntersectionConcept OtherRegion>
constexpr bool Polygon<PointType, LabelType>::contains(const OtherRegion& other) const {
    if (other.empty()) {
        return true;
    }
    if (!other.isBounded()) {
        return false;
    }
    using E = detail::region_exact_number_t<typename OtherRegion::NumberType>;
    if (other.isDegenerate()) {
        return std::visit(
            [this](const auto& carrier) {
                using Carrier = std::remove_cvref_t<decltype(carrier)>;
                if constexpr (detail::is_point_v<Carrier> || detail::is_segment_v<Carrier>) {
                    return this->contains(carrier);
                } else {
                    return false;
                }
            },
            detail::degenerateRegionCarrier(other));
    }
    return contains(other.template asConvex<E>());
}


// ---------------------------------------------------------------------------
// PolygonWithHoles

template <class PointType, class LabelType>
template <PointConcept OtherPoint>
constexpr bool PolygonWithHoles<PointType, LabelType>::contains(const OtherPoint& point) const {
    if (!outer_.contains(point)) {
        return false;
    }
    // A hole removes only its interior, so a point on a hole boundary stays in
    // the region; only a strictly interior one is carved out.
    for (const auto& hole : holes_) {
        if (hole.interiorContains(point)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template <SegmentConcept OtherSegment>
constexpr bool PolygonWithHoles<PointType, LabelType>::contains(const OtherSegment& other) const {
    // A segment collapsed to a point has no relative interior for the hole test
    // below to see, so it goes through the point path.
    if (other.isDegenerate()) {
        return contains(other.min());
    }
    if (!outer_.contains(other)) {
        return false;
    }
    // A hole removes only its interior. A segment that reaches the open hole at
    // all reaches it along its own relative interior — the hole interior is
    // open, so a contact at an endpoint drags the neighbouring segment points in
    // with it — which is exactly what interiorsIntersect detects.
    for (const auto& hole : holes_) {
        if (hole.interiorsIntersect(other)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template <OrientedSegmentConcept OtherOrientedSegment>
constexpr bool PolygonWithHoles<PointType, LabelType>::contains(const OtherOrientedSegment& other) const {
    return contains(other.asSegment());
}

// The region is bounded, so it swallows an unbounded operand only when that
// operand has collapsed to a point.
template <class PointType, class LabelType>
template <LineConcept OtherLine>
constexpr bool PolygonWithHoles<PointType, LabelType>::contains(const OtherLine& other) const {
    return other.isDegenerate() && contains(other.min());
}

template <class PointType, class LabelType>
template <OrientedLineConcept OtherOrientedLine>
constexpr bool PolygonWithHoles<PointType, LabelType>::contains(const OtherOrientedLine& other) const {
    return other.isDegenerate() && contains(other.source());
}

template <class PointType, class LabelType>
template <RayConcept OtherRay>
constexpr bool PolygonWithHoles<PointType, LabelType>::contains(const OtherRay& other) const {
    return other.isDegenerate() && contains(other.source());
}

template <class PointType, class LabelType>
template <HalfplaneConcept OtherHalfplane>
constexpr bool PolygonWithHoles<PointType, LabelType>::contains(const OtherHalfplane& other) const {
    return other.isDegenerate() && contains(other.source());
}

// Containment of a bounded operand splits cleanly in two along the operand's
// own boundary.
//
// Everything the boundary can reach is settled edge by edge, and that is more
// than it looks. Leaving the outer polygon is caught even when no edge does:
// a simple polygon leaves a connected unbounded complement, so a path from an
// escaped point of the operand to infinity stays outside the outer polygon and
// has to cross the operand's boundary on its way out.
//
// What the edges cannot reach is a hole swallowed whole. Its boundary belongs
// to the region, its interior does not, so an operand closing over one holds
// points the region lacks while every one of its edges still lies in the
// region. That is the second test — and once the edge scan has passed, the
// hole's interior misses ∂B, so it lies wholly inside the operand or wholly
// outside it and one witness point of the hole decides.
//
// A collapsed operand is exactly the union of its edges and has no interior to
// swallow anything with, so the first half alone answers it.
template <class PointType, class LabelType>
template <class OtherArea>
constexpr bool PolygonWithHoles<PointType, LabelType>::areaContains(const OtherArea& other) const {
    // Without holes the region is exactly its outer polygon, which answers
    // directly — and for a convex or a simple-polygon operand it has a
    // boundary-chain fast path the edge scan below does not. Polygon::contains
    // has no overload for a region operand (that direction is a later phase), so
    // that one keeps going edge by edge.
    if constexpr (!PolygonWithHolesConcept<OtherArea>) {
        if (holes_.empty()) {
            return outer_.contains(other);
        }
    }
    for (const auto& edge : other.edges()) {
        if (!contains(edge)) {
            return false;
        }
    }
    if (other.isDegenerate()) {
        return true;
    }
    for (const auto& hole : holes_) {
        if (hole.pointInsideInteriorContainedIn(other)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template <RectangleConcept OtherRectangle>
constexpr bool PolygonWithHoles<PointType, LabelType>::contains(const OtherRectangle& other) const {
    if (other.empty()) {
        // The empty set is a subset of every shape, its boundary and its
        // interior alike.
        return true;
    }
    return areaContains(other);
}

template <class PointType, class LabelType>
template <TriangleConcept OtherTriangle>
constexpr bool PolygonWithHoles<PointType, LabelType>::contains(const OtherTriangle& other) const {
    return areaContains(other);
}

template <class PointType, class LabelType>
template <ConvexConcept OtherConvex>
constexpr bool PolygonWithHoles<PointType, LabelType>::contains(const OtherConvex& other) const {
    return areaContains(other);
}

template <class PointType, class LabelType>
template <PolygonConcept OtherPolygon>
constexpr bool PolygonWithHoles<PointType, LabelType>::contains(const OtherPolygon& other) const {
    return areaContains(other);
}

template <class PointType, class LabelType>
template <PolygonWithHolesConcept OtherRegion>
constexpr bool PolygonWithHoles<PointType, LabelType>::contains(const OtherRegion& other) const {
    return areaContains(other);
}

// A chain — monotone or not — is exactly the union of its edges, so every
// set-level relation between it and the region is the conjunction (or, for
// intersects, the disjunction) of the same relation over the edges. The segment
// overloads carry all the hole bookkeeping already. A chain covering a single
// point becomes a degenerate segment, which those overloads route to the point
// path themselves.
template <class PointType, class LabelType>
template <class OtherChain, class EdgeRelation>
constexpr bool PolygonWithHoles<PointType, LabelType>::chainRelation(const OtherChain& other,
                                                                     bool all,
                                                                     EdgeRelation&& relation) const {
    using ChainSegment = Segment<typename OtherChain::PointType>;
    if (other.empty()) {
        // The empty set is contained in everything and meets nothing.
        return all;
    }
    if (other.size() == 1) {
        return relation(ChainSegment(other[0], other[0]));
    }
    for (std::size_t i = 0; i + 1 < other.size(); ++i) {
        const bool holds = relation(ChainSegment(other[i], other[i + 1]));
        if (holds != all) {
            return holds;  // an edge that fails a conjunction, or carries a disjunction
        }
    }
    return all;
}

template <class PointType, class LabelType>
template <MonotoneChainConcept OtherChain>
constexpr bool PolygonWithHoles<PointType, LabelType>::contains(const OtherChain& other) const {
    return chainRelation(other, true, [this](const auto& edge) { return this->contains(edge); });
}

template <class PointType, class LabelType>
template <PolylineConcept OtherPolyline>
constexpr bool PolygonWithHoles<PointType, LabelType>::contains(const OtherPolyline& other) const {
    return chainRelation(other, true, [this](const auto& edge) { return this->contains(edge); });
}

// A non-degenerate disk is the closure of its own interior, which is exactly
// what the area operands could not be relied on to be (§3): a disk reaching a
// hole interior at all reaches it with interior of its own, so the per-hole
// rewriting of A = outer ∖ ⋃ hole° applies directly and no edge scan is needed.
//
// A degenerate disk is either a disk of radius zero, which is the point a(), or
// undefined — three collinear points determine no circle. Reading it as a() is
// exact in the first case and an arbitrary but terminating answer in the second,
// which is all the contract asks for.
template <class PointType, class LabelType>
template <DiskConcept OtherDisk>
constexpr bool PolygonWithHoles<PointType, LabelType>::contains(const OtherDisk& other) const {
    if (other.isDegenerate()) {
        return contains(other.a());
    }
    if (!outer_.contains(other)) {
        return false;
    }
    for (const auto& hole : holes_) {
        if (hole.interiorsIntersect(other)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template <class OtherIntersection>
constexpr auto PolygonWithHoles<PointType, LabelType>::asConvexOperand(const OtherIntersection& other) {
    using E = detail::region_exact_number_t<typename OtherIntersection::NumberType>;
    return other.template asConvex<E>();
}

template <class PointType, class LabelType>
template <class OtherIntersection, class Relation>
constexpr bool PolygonWithHoles<PointType, LabelType>::degenerateIntersectionRelation(
    const OtherIntersection& other, Relation&& relation) const {
    return std::visit([&relation](const auto& carrier) { return relation(carrier); },
                      detail::degenerateRegionCarrier(other));
}

// A bounded half-plane intersection with area is a convex polygon and goes to
// the area path; a degenerate one is a point, a segment, a ray or a line, and
// goes to the overload for that carrier — which, for the two unbounded ones,
// answers false because the region is bounded.
template <class PointType, class LabelType>
template <HalfplaneIntersectionConcept OtherIntersection>
constexpr bool PolygonWithHoles<PointType, LabelType>::contains(const OtherIntersection& other) const {
    if (other.empty()) {
        return true;
    }
    if (other.isDegenerate()) {
        return degenerateIntersectionRelation(
            other, [this](const auto& carrier) { return this->contains(carrier); });
    }
    if (!other.isBounded()) {
        return false;
    }
    return areaContains(asConvexOperand(other));
}


// ---------------------------------------------------------------------------
// Reverse direction: lower-ranked shapes containing a PolygonWithHoles.
//
// Every shape here but a Polyline answers the region exactly as it answers the
// region's *outer polygon*, holes and all:
//
//   B ⊇ A  ⟺  B ⊇ outer.
//
// (⇐) is free, since A ⊆ outer. For (⇒), start from A ⊇ ∂outer: a hole
// interior touching ∂outer would carry points beyond it, which the closed
// containment of a hole in the outer polygon forbids, so the whole outer ring
// belongs to the region however the rings meet. Then split on the ring:
//
//  - if the outer polygon has no area it *is* ∂outer, and it has no room for a
//    hole either (zero-area rings are dropped at construction), so A = outer
//    and the two questions are the same one;
//
//  - otherwise ∂outer is a Jordan curve. A shape that is at most
//    one-dimensional cannot hold it and cannot hold the polygon either, so
//    both sides are false. A two-dimensional B holding it holds everything
//    inside it too: every shape in the library is closed with a *connected*
//    complement, so a point of outer° outside B would reach infinity along a
//    path avoiding B — hence avoiding ∂outer ⊆ B — while going from inside the
//    curve to outside it.
//
// A Polyline is the one exception, because it is the one shape here that can
// close a loop, and then its complement is not connected. It gets the
// zero-area rewriting of detail::everyHoledRegionEdge instead: a
// one-dimensional shape holds the region only when the region has no area, and
// then the region is exactly the union of its ring edges. The region whose
// hole equals its outer ring — material with no area anywhere — is precisely
// the case the two readings disagree on.

template <class Number, class Label>
template <PolygonWithHolesConcept OtherRegion>
constexpr bool Point<Number, Label>::contains(const OtherRegion& other) const {
    return contains(other.outer());
}

template <class PointType, class LabelType>
template <PolygonWithHolesConcept OtherRegion>
constexpr bool Segment<PointType, LabelType>::contains(const OtherRegion& other) const {
    return contains(other.outer());
}

template <class PointType, class LabelType>
template <PolygonWithHolesConcept OtherRegion>
constexpr bool OrientedSegment<PointType, LabelType>::contains(const OtherRegion& other) const {
    return asSegment().contains(other);
}

template <class PointType, class LabelType>
template <PolygonWithHolesConcept OtherRegion>
constexpr bool Line<PointType, LabelType>::contains(const OtherRegion& other) const {
    return contains(other.outer());
}

template <class PointType, class LabelType>
template <PolygonWithHolesConcept OtherRegion>
constexpr bool OrientedLine<PointType, LabelType>::contains(const OtherRegion& other) const {
    return asLine().contains(other);
}

template <class PointType, class LabelType>
template <PolygonWithHolesConcept OtherRegion>
constexpr bool Ray<PointType, LabelType>::contains(const OtherRegion& other) const {
    return contains(other.outer());
}

template <class PointType, class LabelType>
template <PolygonWithHolesConcept OtherRegion>
constexpr bool Halfplane<PointType, LabelType>::contains(const OtherRegion& other) const {
    return contains(other.outer());
}

template <class PointType, class LabelType>
template <PolygonWithHolesConcept OtherRegion>
constexpr bool Rectangle<PointType, LabelType>::contains(const OtherRegion& other) const {
    if (empty()) {
        // The empty set is a subset of itself and of nothing else.
        return detail::coversNoPoint(other);
    }
    return contains(other.outer());
}

template <class PointType, class LabelType>
template <PolygonWithHolesConcept OtherRegion>
constexpr bool Triangle<PointType, LabelType>::contains(const OtherRegion& other) const {
    return contains(other.outer());
}

template <class PointType, class LabelType>
template <PolygonWithHolesConcept OtherRegion>
constexpr bool Disk<PointType, LabelType>::contains(const OtherRegion& other) const {
    return contains(other.outer());
}

template <class PointType, class LabelType>
template <PolygonWithHolesConcept OtherRegion>
constexpr bool Convex<PointType, LabelType>::contains(const OtherRegion& other) const {
    return contains(other.outer());
}

template <class PointType, class LabelType, class Storage>
template <PolygonWithHolesConcept OtherRegion>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::contains(const OtherRegion& other) const {
    return contains(other.outer());
}

// The exception; see the note above.
template <class PointType, class LabelType>
template <PolygonWithHolesConcept OtherRegion>
constexpr bool Polyline<PointType, LabelType>::contains(const OtherRegion& other) const {
    if (!other.isDegenerate()) {
        return false;  // the region has area; the polyline has none
    }
    return detail::everyHoledRegionEdge(
        other, [this](const auto& edge) { return this->contains(edge); });
}

template <class PointType, class LabelType>
template <PolygonWithHolesConcept OtherRegion>
constexpr bool Polygon<PointType, LabelType>::contains(const OtherRegion& other) const {
    return contains(other.outer());
}

template <class PointType, class LabelType>
template <PolygonWithHolesConcept OtherHoledRegion>
constexpr bool HalfplaneIntersection<PointType, LabelType>::contains(const OtherHoledRegion& other) const {
    return contains(other.outer());
}

// ---------------------------------------------------------------------------
// Runtime Shape argument: unwrap the stored alternative and re-dispatch. Every
// alternative has a per-shape overload above, so no fallback is needed.

template <class PointType, class LabelType>
template <PointConcept OtherPoint>
constexpr bool PolygonWithHoles<PointType, LabelType>::contains(const Shape<OtherPoint>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->contains(value);
        },
        other.variant());
}


// ---------------------------------------------------------------------------
// PolygonSet
//
// One definition per relation, stated over every operand the set outranks: the
// argument for `A = ⋃ Aᵢ` does not vary from one operand family to the next, and
// where it does — the one-dimensional operands — the difference is a branch
// inside, not an overload of its own.

template <class PointType, class LabelType>
bool PolygonSet<PointType, LabelType>::isPinched() const {
    if (pinched_ >= 0) {
        return pinched_ != 0;
    }
    pinched_ = 0;
    for (std::size_t i = 0; i < components_.size() && pinched_ == 0; ++i) {
        for (std::size_t j = i + 1; j < components_.size(); ++j) {
            if (!components_[i].bbox().intersects(components_[j].bbox())) {
                continue;  // the boxes prefilter the quadratic scan
            }
            if (components_[i].intersects(components_[j])) {
                pinched_ = 1;
                break;
            }
        }
    }
    return pinched_ != 0;
}

template <class PointType, class LabelType>
template <class OtherSegment>
bool PolygonSet<PointType, LabelType>::segmentIn(const OtherSegment& segment,
                                                 bool boundaryOnly) const {
    using ExactNumber = detail::Exact1DNumber<NumberType, typename OtherSegment::NumberType>;
    using ExactPoint = Point<ExactNumber>;
    using ExactSegment = Segment<ExactPoint>;

    const ExactSegment probe(ExactPoint(segment.min()), ExactPoint(segment.max()));
    const auto holds = [this, boundaryOnly](const ExactPoint& point) {
        return anyComponent([&](const ComponentType& component) {
            return boundaryOnly ? component.boundaryContains(point) : component.contains(point);
        });
    };
    if (probe.min() == probe.max()) {
        return holds(probe.min());
    }

    std::vector<ExactPoint> cuts{probe.min(), probe.max()};
    for (const auto& component : components_) {
        for (const auto& edge : component.edges()) {
            const auto piece = probe.template intersection<ExactNumber>(
                ExactSegment(ExactPoint(edge.min()), ExactPoint(edge.max())));
            if (!piece) {
                continue;
            }
            if (const auto* touch = std::get_if<0>(&*piece)) {
                cuts.push_back(*touch);
            } else {
                const auto& overlap = std::get<1>(*piece);
                cuts.push_back(overlap.min());
                cuts.push_back(overlap.max());
            }
        }
    }
    // All the cuts lie on one line, so the lexicographic point order is the
    // linear order along it — the same argument splitCutSegments uses.
    std::sort(cuts.begin(), cuts.end());
    cuts.erase(std::unique(cuts.begin(), cuts.end()), cuts.end());

    for (const ExactPoint& cut : cuts) {
        if (!holds(cut)) {
            return false;
        }
    }
    const ExactNumber two(2);
    for (std::size_t i = 0; i + 1 < cuts.size(); ++i) {
        const ExactPoint middle((cuts[i].x() + cuts[i + 1].x()) / two,
                                (cuts[i].y() + cuts[i + 1].y()) / two);
        if (!holds(middle)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template <class OtherChain>
bool PolygonSet<PointType, LabelType>::chainIn(const OtherChain& chain, bool boundaryOnly) const {
    if (chain.size() == 0) {
        return true;  // an empty chain is contained in everything
    }
    if (chain.size() == 1) {
        return anyComponent([&](const ComponentType& component) {
            return boundaryOnly ? component.boundaryContains(chain[0]) : component.contains(chain[0]);
        });
    }
    for (const auto& edge : chain.edgesView()) {
        if (!segmentIn(edge, boundaryOnly)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template <detail::SetOperandConcept OtherShape>
bool PolygonSet<PointType, LabelType>::contains(const OtherShape& other) const {
    if constexpr (PointConcept<OtherShape>) {
        return anyComponent([&](const ComponentType& c) { return c.contains(other); });
    } else if constexpr (LineConcept<OtherShape>) {
        return other.isDegenerate() && contains(other.min());
    } else if constexpr (OrientedLineConcept<OtherShape>) {
        return other.isDegenerate() && contains(other.source());
    } else if constexpr (RayConcept<OtherShape>) {
        return other.isDegenerate() && contains(other.source());
    } else if constexpr (HalfplaneConcept<OtherShape>) {
        // A half-plane is unbounded unless it has collapsed onto its own
        // boundary line, which the line path then settles.
        return other.isDegenerate() && contains(other.source());
    } else if constexpr (SegmentConcept<OtherShape> || OrientedSegmentConcept<OtherShape>) {
        // The one-dimensional case: a single component need not hold the whole
        // segment even when the set does.
        if (anyComponent([&](const ComponentType& c) { return c.contains(other); })) {
            return true;
        }
        if (!isPinched() || !intersects(other)) {
            return false;
        }
        return segmentIn(other, /*boundaryOnly=*/false);
    } else if constexpr (MonotoneChainConcept<OtherShape> || PolylineConcept<OtherShape>) {
        if (anyComponent([&](const ComponentType& c) { return c.contains(other); })) {
            return true;
        }
        if (!isPinched()) {
            return false;
        }
        return chainIn(other, /*boundaryOnly=*/false);
    } else {
        // Every remaining operand stays connected when finitely many points are
        // removed from it, and the components meet at finitely many points at
        // most, so it cannot be shared between two of them: one component holds
        // it or none does. A collapsed one is one-dimensional again and reduces
        // to its carrier.
        if (anyComponent([&](const ComponentType& c) { return c.contains(other); })) {
            return true;
        }
        return detail::reduceDegenerate(other,
                                        [this](const auto& carrier) { return this->contains(carrier); });
    }
}

template <class PointType, class LabelType>
template <PolygonSetConcept OtherSet>
bool PolygonSet<PointType, LabelType>::contains(const OtherSet& other) const {
    // A set operand is the one that need not be connected, so it goes in one
    // component at a time.
    for (const auto& component : other) {
        if (!contains(component)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template <PointConcept OtherPoint>
bool PolygonSet<PointType, LabelType>::contains(const Shape<OtherPoint>& other) const {
    return std::visit([this](const auto& value) { return this->contains(value); }, other.variant());
}

}  // namespace pgl
