#pragma once

#ifndef PGL_HPP_INCLUDED
#error "Do not include this Pangolin header directly; include \"pgl.hpp\" instead."
#endif

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
template<PointConcept OtherPoint>
constexpr bool Point<Number, Label>::contains(const OrientedSegment<OtherPoint>& other) const {
    return other.isDegenerate() && contains(other.source());
}

template <class Number, class Label>
template<PointConcept OtherPoint>
constexpr bool Point<Number, Label>::contains(const Line<OtherPoint>& other) const {
    return other.isDegenerate() && contains(other.min());
}

template <class Number, class Label>
template<PointConcept OtherPoint>
constexpr bool Point<Number, Label>::contains(const OrientedLine<OtherPoint>& other) const {
    return other.isDegenerate() && contains(other.source());
}

template <class Number, class Label>
template<PointConcept OtherPoint>
constexpr bool Point<Number, Label>::contains(const Ray<OtherPoint>& other) const {
    return other.isDegenerate() && contains(other.source());
}

template <class Number, class Label>
template<PointConcept OtherPoint>
constexpr bool Point<Number, Label>::contains(const Halfplane<OtherPoint>& other) const {
    return other.isDegenerate() && contains(other.source());
}

template <class Number, class Label>
template<PointConcept OtherPoint>
constexpr bool Point<Number, Label>::contains(const Rectangle<OtherPoint>& other) const {
    return contains(other.min()) && contains(other.max());
}

template <class Number, class Label>
template<PointConcept OtherPoint>
constexpr bool Point<Number, Label>::contains(const Triangle<OtherPoint>& other) const {
    const auto vertices = other.vertices();
    return contains(vertices[0]) && contains(vertices[1]) && contains(vertices[2]);
}

template <class Number, class Label>
template<PointConcept OtherPoint>
constexpr bool Point<Number, Label>::contains(const Convex<OtherPoint>& other) const {
    return other.size()== 0 || (other.size() == 1 && other[0]==*this);
}

template <class Number, class Label>
constexpr bool Point<Number, Label>::contains(const Shape<Point<Number, Label>>& other) const {
    return std::visit(
        [this](const auto& value) {
            return contains(value);
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
template<PointConcept OtherPoint>
constexpr bool Segment<PointType, LabelType>::contains(const OrientedSegment<OtherPoint>& other) const {
    return contains(other.source()) && contains(other.target());
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Segment<PointType, LabelType>::contains(const Line<OtherPoint>& other) const {
    return other.isDegenerate() && contains(other.min());
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Segment<PointType, LabelType>::contains(const OrientedLine<OtherPoint>& other) const {
    return other.isDegenerate() && contains(other.source());
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Segment<PointType, LabelType>::contains(const Ray<OtherPoint>& other) const {
    return other.isDegenerate() && contains(other.source());
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Segment<PointType, LabelType>::contains(const Halfplane<OtherPoint>& other) const {
    return other.isDegenerate() && contains(other.source());
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Segment<PointType, LabelType>::contains(const Rectangle<OtherPoint>& other) const {
    return contains(other.min()) && contains(other.max());
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Segment<PointType, LabelType>::contains(const Triangle<OtherPoint>& other) const {
    return contains(other.a()) && contains(other.b()) && contains(other.c());
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Segment<PointType, LabelType>::contains(const Convex<OtherPoint>& other) const {
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
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool Segment<PointType, LabelType>::contains(const Disk<OtherPoint, OtherLabel>& other) const {
    return other.a() == other.b() && other.b() == other.c() && contains(other.a());
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Segment<PointType, LabelType>::contains(const Shape<OtherPoint>& other) const {
    return std::visit(
        [this](const auto& value) {
            return contains(value);
        },
        other.variant());
}

/**
 * @section predicates-triangle Triangle
 * Triangle boundary, containment, intersection, and cut predicates, including
 * triangle-vs-rectangle and triangle-vs-triangle topological cases.
 */

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Triangle<PointType>::contains(const OtherPoint& point) const {
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

template <class PointType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool Triangle<PointType>::contains(const Segment<OtherPoint, OtherLabel>& other) const {
    return contains(other.min()) && contains(other.max());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Triangle<PointType>::contains(const OrientedSegment<OtherPoint>& other) const {
    return contains(other.source()) && contains(other.target());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Triangle<PointType>::contains(const Line<OtherPoint>& other) const {
    return other.isDegenerate() && contains(other.min());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Triangle<PointType>::contains(const OrientedLine<OtherPoint>& other) const {
    return other.isDegenerate() && contains(other.source());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Triangle<PointType>::contains(const Ray<OtherPoint>& other) const {
    return other.isDegenerate() && contains(other.source());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Triangle<PointType>::contains(const Halfplane<OtherPoint>& other) const {
    return other.isDegenerate() && contains(other.source());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Triangle<PointType>::contains(const Rectangle<OtherPoint>& other) const {
    const auto vertices = other.vertices();
    for (const auto& vertex : vertices) {
        if (!contains(vertex)) {
            return false;
        }
    }
    return true;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Triangle<PointType>::contains(const Triangle<OtherPoint>& other) const {
    return contains(other.a()) && contains(other.b()) && contains(other.c());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Triangle<PointType>::contains(const Convex<OtherPoint>& other) const {
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

template <class PointType>
constexpr bool Triangle<PointType>::contains(const Shape<PointType>& other) const {
    return std::visit(
        [this](const auto& value) {
            return contains(value);
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
constexpr bool OrientedSegment<PointType>::contains(const OtherPoint& point) const {
    if (point < min() || max() < point) {
        return false;
    }
    return pgl::collinear(source(), target(), point);
}

template <class PointType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool OrientedSegment<PointType>::contains(const Segment<OtherPoint, OtherLabel>& other) const {
    return contains(other.min()) && contains(other.max());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedSegment<PointType>::contains(const OrientedSegment<OtherPoint>& other) const {
    return contains(other.source()) && contains(other.target());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedSegment<PointType>::contains(const Line<OtherPoint>& other) const {
    return other.isDegenerate() && contains(other.min());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedSegment<PointType>::contains(const OrientedLine<OtherPoint>& other) const {
    return other.isDegenerate() && contains(other.source());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedSegment<PointType>::contains(const Ray<OtherPoint>& other) const {
    return other.isDegenerate() && contains(other.source());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedSegment<PointType>::contains(const Halfplane<OtherPoint>& other) const {
    return other.isDegenerate() && contains(other.source());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedSegment<PointType>::contains(const Rectangle<OtherPoint>& other) const {
    return contains(other.min()) && contains(other.max());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedSegment<PointType>::contains(const Triangle<OtherPoint>& other) const {
    return contains(other.a()) && contains(other.b()) && contains(other.c());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedSegment<PointType>::contains(const Convex<OtherPoint>& other) const {
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

template <class PointType>
constexpr bool OrientedSegment<PointType>::contains(const Shape<PointType>& other) const {
    return std::visit(
        [this](const auto& value) {
            return contains(value);
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
constexpr bool Line<PointType>::contains(const OtherPoint& point) const {
    if (isDegenerate()) {
        return point == min();
    }
    return pgl::collinear(min(), max(), point);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Line<PointType>::contains(const Line<OtherPoint>& other) const {
    return contains(other.min()) && contains(other.max());
}

template <class PointType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool Line<PointType>::contains(const Segment<OtherPoint, OtherLabel>& other) const {
    return contains(other.min()) && contains(other.max());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Line<PointType>::contains(const OrientedSegment<OtherPoint>& other) const {
    return contains(other.source()) && contains(other.target());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Line<PointType>::contains(const OrientedLine<OtherPoint>& other) const {
    return contains(other.asLine());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Line<PointType>::contains(const Ray<OtherPoint>& other) const {
    return contains(other.source()) && contains(other.target());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Line<PointType>::contains(const Halfplane<OtherPoint>& other) const {
    return other.isDegenerate() && contains(other.source());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Line<PointType>::contains(const Rectangle<OtherPoint>& other) const {
    return contains(other.min()) && contains(other.max());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Line<PointType>::contains(const Triangle<OtherPoint>& other) const {
    return contains(other.a()) && contains(other.b()) && contains(other.c());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Line<PointType>::contains(const Convex<OtherPoint>& other) const {
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

template <class PointType>
constexpr bool Line<PointType>::contains(const Shape<PointType>& other) const {
    return std::visit(
        [this](const auto& value) {
            return contains(value);
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
constexpr bool OrientedLine<PointType>::contains(const OtherPoint& point) const {
    return this->asLine().contains(point);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedLine<PointType>::contains(const Line<OtherPoint>& other) const {
    return this->asLine().contains(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedLine<PointType>::contains(const OrientedLine<OtherPoint>& other) const {
    return this->asLine().contains(other.asLine());
}

template <class PointType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool OrientedLine<PointType>::contains(const Segment<OtherPoint, OtherLabel>& other) const {
    return this->asLine().contains(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedLine<PointType>::contains(const OrientedSegment<OtherPoint>& other) const {
    return this->asLine().contains(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedLine<PointType>::contains(const Ray<OtherPoint>& other) const {
    return this->asLine().contains(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedLine<PointType>::contains(const Halfplane<OtherPoint>& other) const {
    return this->asLine().contains(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedLine<PointType>::contains(const Rectangle<OtherPoint>& other) const {
    return this->asLine().contains(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedLine<PointType>::contains(const Triangle<OtherPoint>& other) const {
    return this->asLine().contains(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedLine<PointType>::contains(const Convex<OtherPoint>& other) const {
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

template <class PointType>
constexpr bool OrientedLine<PointType>::contains(const Shape<PointType>& other) const {
    return std::visit(
        [this](const auto& value) {
            return contains(value);
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
constexpr bool Ray<PointType>::contains(const OtherPoint& point) const {
    if (isDegenerate()) {
        return point == source();
    }
    return pgl::collinear(source(), target(), point) && containsCollinear(point);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Ray<PointType>::contains(const Line<OtherPoint>& other) const {
    return other.isDegenerate() && contains(other.min());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Ray<PointType>::contains(const OrientedLine<OtherPoint>& other) const {
    return other.isDegenerate() && contains(other.source());
}

template <class PointType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool Ray<PointType>::contains(const Segment<OtherPoint, OtherLabel>& other) const {
    return contains(other.min()) && contains(other.max());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Ray<PointType>::contains(const OrientedSegment<OtherPoint>& other) const {
    return contains(other.source()) && contains(other.target());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Ray<PointType>::contains(const Ray<OtherPoint>& other) const {
    return contains(other.source()) && contains(other.target());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Ray<PointType>::contains(const Halfplane<OtherPoint>& other) const {
    return other.isDegenerate() && contains(other.source());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Ray<PointType>::contains(const Rectangle<OtherPoint>& other) const {
    return contains(other.min()) && contains(other.max());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Ray<PointType>::contains(const Triangle<OtherPoint>& other) const {
    return contains(other.a()) && contains(other.b()) && contains(other.c());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Ray<PointType>::contains(const Convex<OtherPoint>& other) const {
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

template <class PointType>
constexpr bool Ray<PointType>::contains(const Shape<PointType>& other) const {
    return std::visit(
        [this](const auto& value) {
            return contains(value);
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
constexpr bool Rectangle<PointType>::contains(const OtherPoint& point) const {
    return !(point.x() < min().x()) &&
           !(max().x() < point.x()) &&
           !(point.y() < min().y()) &&
           !(max().y() < point.y());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Rectangle<PointType>::contains(const Line<OtherPoint>& other) const {
    return other.isDegenerate() && contains(other.min());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Rectangle<PointType>::contains(const OrientedLine<OtherPoint>& other) const {
    return other.isDegenerate() && contains(other.source());
}

template <class PointType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool Rectangle<PointType>::contains(const Segment<OtherPoint, OtherLabel>& other) const {
    return contains(other.min()) && contains(other.max());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Rectangle<PointType>::contains(const OrientedSegment<OtherPoint>& other) const {
    return contains(other.source()) && contains(other.target());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Rectangle<PointType>::contains(const Ray<OtherPoint>& other) const {
    return other.isDegenerate() && contains(other.source());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Rectangle<PointType>::contains(const Halfplane<OtherPoint>& other) const {
    return other.isDegenerate() && contains(other.source());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Rectangle<PointType>::contains(const Rectangle<OtherPoint>& other) const {
    return contains(other.min()) && contains(other.max());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Rectangle<PointType>::contains(const Triangle<OtherPoint>& other) const {
    return contains(other.a()) && contains(other.b()) && contains(other.c());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Rectangle<PointType>::contains(const Convex<OtherPoint>& other) const {
    return other.size()==0 || contains(other.bbox());
}

template <class PointType>
constexpr bool Rectangle<PointType>::contains(const Shape<PointType>& other) const {
    return std::visit(
        [this](const auto& value) {
            return contains(value);
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
constexpr bool Halfplane<PointType>::contains(const OtherPoint& point) const {
    if (isDegenerate()) {
        return point == source();
    }
    const auto side = orientationSign(source(), target(), point);
    return side == std::partial_ordering::greater || side == std::partial_ordering::equivalent;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Halfplane<PointType>::contains(const Line<OtherPoint>& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return other.isDegenerate() && contains(other.min());
    }
    return contains(other.min()) && this->asLine().parallel(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Halfplane<PointType>::contains(const OrientedLine<OtherPoint>& other) const {
    return contains(other.asLine());
}

template <class PointType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool Halfplane<PointType>::contains(const Segment<OtherPoint, OtherLabel>& other) const {
    if (isDegenerate()) {
        return other.isDegenerate() && contains(other.min());
    }
    return contains(other.min()) && contains(other.max());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Halfplane<PointType>::contains(const OrientedSegment<OtherPoint>& other) const {
    if (isDegenerate()) {
        return other.isDegenerate() && contains(other.source());
    }
    return contains(other.source()) && contains(other.target());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Halfplane<PointType>::contains(const Ray<OtherPoint>& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return other.isDegenerate() && contains(other.source());
    }
    const auto source_side = orientationDeterminant(source(), target(), other.source());
    const auto zero = decltype(source_side){};
    if (source_side < zero) {
        return false;
    }
    const auto direction_side =
        orientationDeterminant(source(), target(), other.target()) -
        source_side;
    return !(direction_side < zero);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Halfplane<PointType>::contains(const Rectangle<OtherPoint>& other) const {
    const auto vertices = other.vertices();
    for (const auto& vertex : vertices) {
        if (!contains(vertex)) {
            return false;
        }
    }
    return true;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Halfplane<PointType>::contains(const Halfplane<OtherPoint>& other) const {
    return other.isDegenerate() && contains(other.source());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Halfplane<PointType>::contains(const Triangle<OtherPoint>& other) const {
    return contains(other.a()) && contains(other.b()) && contains(other.c());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Halfplane<PointType>::contains(const Convex<OtherPoint>& other) const {
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

template <class PointType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool Halfplane<PointType>::contains(const Disk<OtherPoint, OtherLabel>& other) const {
    // The closed disk lies in the closed half-plane iff its center is on the
    // interior side and its distance to the boundary line is at least the
    // radius. With cross = det(AB, A->center) = |AB| * signedDistance(center),
    // the condition signedDistance >= radius becomes cross > 0 and
    // cross^2 >= radius^2 * |AB|^2 (boundary tangency is allowed).
    const auto cross = orientationDeterminant(source(), target(), other.center());
    const auto zero = decltype(cross){};
    if (!(zero < cross)) {
        return false;
    }
    const auto squaredLength = source().squaredDistance(target());
    return cross * cross >= other.squaredRadius() * squaredLength;
}

template <class PointType>
constexpr bool Halfplane<PointType>::contains(const Shape<PointType>& other) const {
    return std::visit(
        [this](const auto& value) {
            return contains(value);
        },
        other.variant());
}

// -----------------------------------------------------------------------------
// Disk

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Disk<PointType, LabelType>::contains(const OtherPoint& point) const {
    if (isDegenerate()) {
        return Segment<PointType>(a(), c()).contains(point);
    }

    return inCircleSign(a(), b(), c(), point) != std::partial_ordering::less;
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool Disk<PointType, LabelType>::contains(const Segment<OtherPoint, OtherLabel>& other) const {
    return contains(other.min()) && contains(other.max());
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Disk<PointType, LabelType>::contains(const OrientedSegment<OtherPoint>& other) const {
    return contains(other.source()) && contains(other.target());
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Disk<PointType, LabelType>::contains(const Line<OtherPoint>& other) const {
    return other.isDegenerate() && contains(other.min());
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Disk<PointType, LabelType>::contains(const OrientedLine<OtherPoint>& other) const {
    return other.isDegenerate() && contains(other.source());
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Disk<PointType, LabelType>::contains(const Ray<OtherPoint>& other) const {
    return other.isDegenerate() && contains(other.source());
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Disk<PointType, LabelType>::contains(const Halfplane<OtherPoint>& other) const {
    return other.isDegenerate() && contains(other.source());
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Disk<PointType, LabelType>::contains(const Triangle<OtherPoint>& other) const {
    return contains(other.a()) && contains(other.b()) && contains(other.c());
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Disk<PointType, LabelType>::contains(const Rectangle<OtherPoint>& other) const {
    const auto vertices = other.vertices();
    return contains(vertices[0]) && contains(vertices[1]) && contains(vertices[2]) && contains(vertices[3]);
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Disk<PointType, LabelType>::contains(const Convex<OtherPoint>& other) const {
    for (const auto& point : other) {
        if (!contains(point)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool Disk<PointType, LabelType>::contains(const Disk<OtherPoint, OtherLabel>& other) const {
    if (other.isDegenerate()) {
        return contains(Segment<OtherPoint>(other.a(), other.c()));
    }
    if (isDegenerate()) {
        return false;
    }

    using R = detail::promoted_number_t<std::common_type_t<
        decltype(squaredRadius()),
        decltype(other.squaredRadius())>>;

    const R r1_sq = squaredRadius<R>();
    const R r2_sq = other.template squaredRadius<R>();
    if (r1_sq < r2_sq) {
        return false;
    }

    const R d2 = center<R>().squaredDistance(other.template center<R>());
    const R A = d2 - r1_sq - r2_sq;
    return A <= R{} && A * A >= R{4} * r1_sq * r2_sq;
}

template <class PointType, class LabelType>
constexpr bool Disk<PointType, LabelType>::contains(const Shape<PointType>& other) const {
    return std::visit(
        [this](const auto& value) {
            return contains(value);
        },
        other.variant());
}


// ---------------------------------------------------------------------------
// Convex

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Convex<PointType>::contains(const OtherPoint& point) const {
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

template <class PointType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool Convex<PointType>::contains(const Segment<OtherPoint, OtherLabel>& other) const {
    return contains(other[0]) && contains(other[1]);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Convex<PointType>::contains(const OrientedSegment<OtherPoint>& other) const {
    return contains(other[0]) && contains(other[1]);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Convex<PointType>::contains(const Line<OtherPoint>&) const {
    return false;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Convex<PointType>::contains(const OrientedLine<OtherPoint>&) const {
    return false;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Convex<PointType>::contains(const Ray<OtherPoint>&) const {
    return false;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Convex<PointType>::contains(const Halfplane<OtherPoint>&) const {
    return false;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Convex<PointType>::contains(const Rectangle<OtherPoint>& other) const {
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

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Convex<PointType>::contains(const Triangle<OtherPoint>& other) const {
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

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Convex<PointType>::contains(const Convex<OtherPoint>& other) const {
    if (size() == 0) {
        return false;
    }
    if (other.size() == 0) {
        return true;
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
        return Segment<PointType>((*this)[0], (*this)[1]).contains(Segment<OtherPoint>(other[0], other[1]));
    }

    if (other.size() <= 2*size()) {
        for (size_t i = 0; i < other.size(); ++i) {
            if (!contains(other[i])) {
                return false;
            }
        }
    } else {
        for (auto &edge : orientedEdges()) {
            if (!edge.leftHalfplane().contains(other)) {
                return false;
            }
        }
    }
    return true;
}

template <class PointType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool Convex<PointType>::contains(const Disk<OtherPoint, OtherLabel>& other) const {
    for (auto &edge : orientedEdges()) {
        if (!edge.leftHalfplane().contains(other)) {
            return false;
        }
    }
    return true;
}


// ---------------------------------------------------------------------------
// Polygon

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Polygon<PointType>::contains(const OtherPoint& point) const {
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

template <class PointType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool Polygon<PointType>::contains(const Segment<OtherPoint, OtherLabel>& other) const {
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

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Polygon<PointType>::contains(const OrientedSegment<OtherPoint>& other) const {
    return contains(Segment<OtherPoint>(other.source(), other.target()));
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Polygon<PointType>::contains(const Line<OtherPoint>& other) const {
    return other.isDegenerate() && contains(other.min());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Polygon<PointType>::contains(const OrientedLine<OtherPoint>& other) const {
    return other.isDegenerate() && contains(other.source());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Polygon<PointType>::contains(const Ray<OtherPoint>& other) const {
    return other.isDegenerate() && contains(other.source());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Polygon<PointType>::contains(const Halfplane<OtherPoint>& other) const {
    return other.isDegenerate() && contains(other.source());
}

// For a simple polygon (no holes) a bounded shape is contained iff every one
// of its edges is contained, so the region overloads reduce to edge checks.
template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Polygon<PointType>::contains(const Rectangle<OtherPoint>& other) const {
    for (std::size_t i = 0; i < other.size(); ++i) {
        if (!contains(Segment<OtherPoint>(other[i], other[(i + 1) % other.size()]))) {
            return false;
        }
    }
    return true;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Polygon<PointType>::contains(const Triangle<OtherPoint>& other) const {
    for (std::size_t i = 0; i < other.size(); ++i) {
        if (!contains(Segment<OtherPoint>(other[i], other[(i + 1) % other.size()]))) {
            return false;
        }
    }
    return true;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Polygon<PointType>::contains(const Convex<OtherPoint>& other) const {
    if (other.size() == 0) {
        return true;
    }
    if (other.size() == 1) {
        return contains(other[0]);
    }
    for (std::size_t i = 0; i < other.size(); ++i) {
        if (!contains(Segment<OtherPoint>(other[i], other[(i + 1) % other.size()]))) {
            return false;
        }
    }
    return true;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Polygon<PointType>::contains(const Polygon<OtherPoint>& other) const {
    if (other.size() == 0) {
        return true;
    }
    if (!bbox().contains(other.bbox())) {
        return false;
    }
    if (other.size() == 1) {
        return contains(other[0]);
    }
    for (std::size_t i = 0; i < other.size(); ++i) {
        if (!contains(Segment<OtherPoint>(other[i], other[(i + 1) % other.size()]))) {
            return false;
        }
    }
    return true;
}

template <class PointType>
constexpr bool Polygon<PointType>::contains(const Shape<PointType>& other) const {
    return std::visit(
        [this](const auto& value) {
            return contains(value);
        },
        other.variant());
}

template <class PointType>
template <PointConcept OtherPoint>
constexpr bool Convex<PointType>::contains(const Shape<OtherPoint>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->contains(value);
        },
        other.variant());
}

template <class Number, class Label>
template<PointConcept OtherPoint>
constexpr bool Point<Number, Label>::contains(const Polygon<OtherPoint>& other) const {
    for (const auto& vertex : other) {
        if (!contains(vertex)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Segment<PointType, LabelType>::contains(const Polygon<OtherPoint>& other) const {
    for (const auto& vertex : other) {
        if (!contains(vertex)) {
            return false;
        }
    }
    return true;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedSegment<PointType>::contains(const Polygon<OtherPoint>& other) const {
    for (const auto& vertex : other) {
        if (!contains(vertex)) {
            return false;
        }
    }
    return true;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Line<PointType>::contains(const Polygon<OtherPoint>& other) const {
    for (const auto& vertex : other) {
        if (!contains(vertex)) {
            return false;
        }
    }
    return true;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedLine<PointType>::contains(const Polygon<OtherPoint>& other) const {
    for (const auto& vertex : other) {
        if (!contains(vertex)) {
            return false;
        }
    }
    return true;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Ray<PointType>::contains(const Polygon<OtherPoint>& other) const {
    for (const auto& vertex : other) {
        if (!contains(vertex)) {
            return false;
        }
    }
    return true;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Halfplane<PointType>::contains(const Polygon<OtherPoint>& other) const {
    for (const auto& vertex : other) {
        if (!contains(vertex)) {
            return false;
        }
    }
    return true;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Rectangle<PointType>::contains(const Polygon<OtherPoint>& other) const {
    for (const auto& vertex : other) {
        if (!contains(vertex)) {
            return false;
        }
    }
    return true;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Triangle<PointType>::contains(const Polygon<OtherPoint>& other) const {
    for (const auto& vertex : other) {
        if (!contains(vertex)) {
            return false;
        }
    }
    return true;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Convex<PointType>::contains(const Polygon<OtherPoint>& other) const {
    for (const auto& vertex : other) {
        if (!contains(vertex)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Disk<PointType, LabelType>::contains(const Polygon<OtherPoint>& other) const {
    for (const auto& vertex : other) {
        if (!contains(vertex)) {
            return false;
        }
    }
    return true;
}

}  // namespace pgl
