#pragma once

#include "implementation/interiorsintersect.hpp"

/**
 * @file intersects.hpp
 * @brief Implementations of the 'intersects' predicate.
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
constexpr bool Point<Number, Label>::intersects(const OtherPoint& other) const {
    return contains(other);
}


template <class Number, class Label>
constexpr bool Point<Number, Label>::intersects(const Shape<Point<Number, Label>>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->intersects(value);
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
constexpr bool Segment<PointType, LabelType>::intersects(const OtherPoint& other) const {
    return contains(other);
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Segment<PointType, LabelType>::intersects(const OtherSegment& other) const {
    if constexpr (is_Rational_v<NumberType> || is_Rational_v<typename OtherSegment::NumberType>) {
        int cross = boundingBoxesCross(other);
        if (cross == 0) {
            return false;
        }
        if (cross == 2) {
            return true;
        }
    }
    else if (!boundingBoxesOverlap(other)) {
        return false;
    }
    const auto d1 = orientationSign(min(), max(), other.min());
    if (d1 == 0 && containsCollinear(other.min())) {
        return true;
    }
    const auto d2 = orientationSign(min(), max(), other.max());
    if (d2 == 0 && containsCollinear(other.max())) {
        return true;
    }
    const auto d3 = orientationSign(other.min(), other.max(), min());
    if (d3 == 0 && other.containsCollinear(min())) {
        return true;
    }
    const auto d4 = orientationSign(other.min(), other.max(), max());
    if (d4 == 0 && other.containsCollinear(max())) {
        return true;
    }
    if (d1 == 0 || d2 == 0 || d3 == 0 || d4 == 0) {
        return false;
    }
    return d1 != d2 && d3 != d4;
}


template <class PointType, class LabelType>
constexpr bool Segment<PointType, LabelType>::intersects(const Shape<PointType>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->intersects(value);
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
constexpr bool Triangle<PointType, LabelType>::intersects(const OtherPoint& other) const {
    return contains(other);
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Triangle<PointType, LabelType>::intersects(const OtherLine& other) const {
    if (other.isDegenerate()) {
        return contains(other.min());
    }
    // The line meets the closed triangle unless all three vertices lie
    // strictly on one side of it.
    bool positive = false, negative = false;
    for (auto p : *this) {
        auto o = orientationSign(other[0], other[1], p);
        if (o == 0) {
            return true;
        }
        negative = (negative || o < 0);
        positive = (positive || o > 0);
        if (positive && negative) {
            return true;
        }
    }
    return false;
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Triangle<PointType, LabelType>::intersects(const OtherOrientedLine& other) const {
    return intersects(static_cast<Line<typename OtherOrientedLine::PointType>>(other));
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Triangle<PointType, LabelType>::intersects(const OtherSegment& other) const {
    // Either an endpoint lies in the closed triangle, or the segment crosses
    // one of the triangle's edges.
    if (contains(other.min()) || contains(other.max())) {
        return true;
    }
    for (const auto& edge : edges()) {
        if (edge.intersects(other)) {
            return true;
        }
    }
    return false;
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Triangle<PointType, LabelType>::intersects(const OtherOrientedSegment& other) const {
    return intersects(static_cast<Segment<typename OtherOrientedSegment::PointType>>(other));
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool Triangle<PointType, LabelType>::intersects(const OtherRay& other) const {
    // Either the source lies in the closed triangle, or the ray crosses an edge.
    if (contains(other.source())) {
        return true;
    }
    for (const auto& edge : edges()) {
        if (edge.intersects(other)) {
            return true;
        }
    }
    return false;
}

template <class PointType, class LabelType>
template<HalfplaneConcept OtherHalfplane>
constexpr bool Triangle<PointType, LabelType>::intersects(const OtherHalfplane& other) const {
    if (other.contains(a()) || other.contains(b()) || other.contains(c())) {
        return true;
    }
    for (const auto& edge : edges()) {
        if (other.intersects(edge)) {
            return true;
        }
    }
    return false;
}

template <class PointType, class LabelType>
template<RectangleConcept OtherRectangle>
constexpr bool Triangle<PointType, LabelType>::intersects(const OtherRectangle& other) const {
    if (other.contains(a()) || other.contains(b()) || other.contains(c())) {
        return true;
    }
    for (const auto& vertex : other.vertices()) {
        if (contains(vertex)) {
            return true;
        }
    }
    for (const auto& left : other.edges()) {
        for (const auto& right : edges()) {
            if (left.intersects(right)) {
                return true;
            }
        }
    }
    return false;
}

template <class PointType, class LabelType>
template<TriangleConcept OtherTriangle>
constexpr bool Triangle<PointType, LabelType>::intersects(const OtherTriangle& other) const {
    if (contains(other.a()) || contains(other.b()) || contains(other.c()) ||
        other.contains(a()) || other.contains(b()) || other.contains(c())) {
        return true;
    }
    const auto this_edges = edges();
    const auto other_edges = other.edges();
    for (const auto& left : this_edges) {
        for (const auto& right : other_edges) {
            if (left.intersects(right)) {
                return true;
            }
        }
    }
    return false;
}

template <class PointType, class LabelType>
constexpr bool Triangle<PointType, LabelType>::intersects(const Shape<PointType>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->intersects(value);
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
constexpr bool OrientedSegment<PointType, LabelType>::intersects(const OtherPoint& other) const {
    return static_cast<Segment<PointType>>(*this).intersects(other);
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool OrientedSegment<PointType, LabelType>::intersects(const OtherSegment& other) const {
    return static_cast<Segment<PointType>>(*this).intersects(other);
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool OrientedSegment<PointType, LabelType>::intersects(const OtherOrientedSegment& other) const {
    return static_cast<Segment<PointType>>(*this).intersects(static_cast<Segment<typename OtherOrientedSegment::PointType>>(other));
}


template <class PointType, class LabelType>
constexpr bool OrientedSegment<PointType, LabelType>::intersects(const Shape<PointType>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->intersects(value);
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
constexpr bool Line<PointType, LabelType>::intersects(const OtherPoint& other) const {
    return contains(other);
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Line<PointType, LabelType>::intersects(const OtherLine& other) const {
    if (isDegenerate()) {
        return other.contains(min());
    }
    if (other.isDegenerate()) {
        return contains(other.min());
    }
    return !parallel(other) || contains(other.min());
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Line<PointType, LabelType>::intersects(const OtherSegment& other) const {
    if (other.isDegenerate()) {
        return contains(other.min());
    }
    const auto first = orientationSign(min(), max(), other.min());
    const auto second = orientationSign(min(), max(), other.max());
    return first == std::partial_ordering::equivalent ||
           second == std::partial_ordering::equivalent ||
           first != second;
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Line<PointType, LabelType>::intersects(const OtherOrientedSegment& other) const {
    return intersects(static_cast<Segment<typename OtherOrientedSegment::PointType>>(other));
}

template <class PointType, class LabelType>
constexpr bool Line<PointType, LabelType>::intersects(const Shape<PointType>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->intersects(value);
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
constexpr bool OrientedLine<PointType, LabelType>::intersects(const OtherPoint& other) const {
    return this->asLine().intersects(other);
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool OrientedLine<PointType, LabelType>::intersects(const OtherLine& other) const {
    return this->asLine().intersects(other);
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool OrientedLine<PointType, LabelType>::intersects(const OtherOrientedLine& other) const {
    return this->asLine().intersects(other.asLine());
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool OrientedLine<PointType, LabelType>::intersects(const OtherSegment& other) const {
    return this->asLine().intersects(other);
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool OrientedLine<PointType, LabelType>::intersects(const OtherOrientedSegment& other) const {
    return this->asLine().intersects(other);
}

template <class PointType, class LabelType>
constexpr bool OrientedLine<PointType, LabelType>::intersects(const Shape<PointType>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->intersects(value);
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
constexpr bool Ray<PointType, LabelType>::intersects(const OtherPoint& other) const {
    return contains(other);
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Ray<PointType, LabelType>::intersects(const OtherLine& other) const {
    if (other.isDegenerate()) {
        return contains(other.min());
    }
    const auto source_side = orientationSign(other.min(), other.max(), source());
    if (source_side == 0) {
        return true;  // source lies on the line
    }
    // As the ray runs to infinity it tends to the side given by its direction.
    // It meets the line exactly when that side is opposite the source's (a
    // forward crossing); parallel (equivalent) or same-side rays never reach it.
    const auto direction_side =
        orientationSign(other.min(), other.max(), other.min() + (target() - source()));
    return direction_side != 0 && direction_side != source_side;
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Ray<PointType, LabelType>::intersects(const OtherOrientedLine& other) const {
    return intersects(other.asLine());
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Ray<PointType, LabelType>::intersects(const OtherSegment& other) const {
    if (other.isDegenerate()) {
        return contains(other.min());
    }

    // A segment collinear with the ray satisfies both line-vs-line tests below
    // even when it does not overlap the ray, so handle it as a 1D overlap: they
    // meet iff the ray reaches an endpoint of the segment, or the segment covers
    // the ray's source.
    if (orientationSign(source(), target(), other.min()) == 0 &&
        orientationSign(source(), target(), other.max()) == 0) {
        return contains(other.min()) || contains(other.max()) || other.contains(source());
    }

    return intersects(other.asLine()) &&
           this->asLine().intersects(other);
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Ray<PointType, LabelType>::intersects(const OtherOrientedSegment& other) const {
    return intersects(static_cast<Segment<typename OtherOrientedSegment::PointType>>(other));
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool Ray<PointType, LabelType>::intersects(const OtherRay& other) const {
    if (isDegenerate()) {
        return other.contains(source());
    }
    if (other.isDegenerate()) {
        return contains(other.source());
    }
    // Collinear rays both satisfy the line-vs-line test below even when they do
    // not overlap, so handle them as a 1D overlap: the rays meet iff one reaches
    // the other's source.
    if (orientationSign(source(), target(), other.source()) == 0 &&
        orientationSign(source(), target(), other.target()) == 0) {
        return contains(other.source()) || other.contains(source());
    }
    // Otherwise the supporting lines meet in at most one point; the rays meet
    // iff that point lies on both (each ray reaches the other's line).
    return intersects(other.asLine()) &&
           this->asLine().intersects(other);
}

template <class PointType, class LabelType>
constexpr bool Ray<PointType, LabelType>::intersects(const Shape<PointType>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->intersects(value);
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
constexpr bool Rectangle<PointType, LabelType>::intersects(const OtherPoint& other) const {
    return contains(other);
}

template <class PointType, class LabelType>
template<RectangleConcept OtherRectangle>
constexpr bool Rectangle<PointType, LabelType>::intersects(const OtherRectangle& other) const {
    return intervalsOverlap(min().x(), max().x(), other.min().x(), other.max().x()) &&
           intervalsOverlap(min().y(), max().y(), other.min().y(), other.max().y());
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Rectangle<PointType, LabelType>::intersects(const OtherLine& other) const {
    if (other.isDegenerate()) {
        return contains(other.min());
    }
    return detail::lineIntersectsRectangle(*this, other.min(), other.max());
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Rectangle<PointType, LabelType>::intersects(const OtherOrientedLine& other) const {
    if (other.isDegenerate()) {
        return contains(other.source());
    }
    return detail::lineIntersectsRectangle(*this, other.source(), other.target());
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Rectangle<PointType, LabelType>::intersects(const OtherSegment& other) const {
    if (contains(other.min()) || contains(other.max())) {
        return true;
    }
    const auto rectangle_edges = edges();
    for (const auto& edge : rectangle_edges) {
        if (edge.intersects(other)) {
            return true;
        }
    }
    return false;
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Rectangle<PointType, LabelType>::intersects(const OtherOrientedSegment& other) const {
    if (contains(other.source()) || contains(other.target())) {
        return true;
    }
    const auto rectangle_edges = edges();
    for (const auto& edge : rectangle_edges) {
        if (other.intersects(edge)) {
            return true;
        }
    }
    return false;
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool Rectangle<PointType, LabelType>::intersects(const OtherRay& other) const {
    if (contains(other.source())) {
        return true;
    }
    const auto rectangle_edges = edges();
    for (const auto& edge : rectangle_edges) {
        if (other.intersects(edge)) {
            return true;
        }
    }
    return false;
}

template <class PointType, class LabelType>
template<HalfplaneConcept OtherHalfplane>
constexpr bool Rectangle<PointType, LabelType>::intersects(const OtherHalfplane& other) const {
    if (other.isDegenerate()) {
        return contains(other.source());
    }
    const auto rectangle_vertices = vertices();
    for (const auto& vertex : rectangle_vertices) {
        if (other.contains(vertex)) {
            return true;
        }
    }
    return false;
}

template <class PointType, class LabelType>
constexpr bool Rectangle<PointType, LabelType>::intersects(const Shape<PointType>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->intersects(value);
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
constexpr bool Halfplane<PointType, LabelType>::intersects(const OtherPoint& other) const {
    return contains(other);
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Halfplane<PointType, LabelType>::intersects(const OtherLine& other) const {
    if (isDegenerate()) {
        return other.contains(source());
    }
    if (other.isDegenerate()) {
        return contains(other.min());
    }
    const auto direction_side =
        orientationDeterminant(source(), target(), other.max()) -
        orientationDeterminant(source(), target(), other.min());
    return direction_side != decltype(direction_side){} || contains(other.min());
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Halfplane<PointType, LabelType>::intersects(const OtherOrientedLine& other) const {
    if (isDegenerate()) {
        return other.contains(source());
    }
    if (other.isDegenerate()) {
        return contains(other.source());
    }
    const auto direction_side =
        orientationDeterminant(source(), target(), other.target()) -
        orientationDeterminant(source(), target(), other.source());
    return direction_side != decltype(direction_side){} || contains(other.source());
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Halfplane<PointType, LabelType>::intersects(const OtherSegment& other) const {
    if (isDegenerate()) {
        return other.contains(source());
    }
    return contains(other.min()) || contains(other.max());
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Halfplane<PointType, LabelType>::intersects(const OtherOrientedSegment& other) const {
    if (isDegenerate()) {
        return other.contains(source());
    }
    return contains(other.source()) || contains(other.target());
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool Halfplane<PointType, LabelType>::intersects(const OtherRay& other) const {
    if (isDegenerate()) {
        return other.contains(source());
    }
    if (other.isDegenerate()) {
        return contains(other.source());
    }
    const auto source_side = orientationDeterminant(source(), target(), other.source());
    const auto direction_side =
        orientationDeterminant(source(), target(), other.target()) -
        orientationDeterminant(source(), target(), other.source());
    const auto zero = decltype(source_side){};
    return !(source_side < zero) || zero < direction_side;
}

template <class PointType, class LabelType>
template<HalfplaneConcept OtherHalfplane>
constexpr bool Halfplane<PointType, LabelType>::intersects(const OtherHalfplane& other) const {
    return contains(other.source()) || contains(other.target()) || other.contains(source()) || other.contains(target());
}

template <class PointType, class LabelType>
constexpr bool Halfplane<PointType, LabelType>::intersects(const Shape<PointType>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->intersects(value);
        },
        other.variant());
}


// ---------------------------------------------------------------------------
// Convex

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Convex<PointType, LabelType>::intersects(const OtherSegment& other) const {
    if (size() == 0 || !bbox().intersects(other.bbox())) {
        return false;
    }
    if (contains(other.min()) || contains(other.max())) {
        return true;
    }
    auto translatedOther = other - translation_;
    auto it1 = detail::cyclicMaxOrPositive(points_.begin(), points_.end(), [&translatedOther](const PointType& a) {
        return orientationDeterminant(translatedOther[0], translatedOther[1], a);
    });
    auto it2 = detail::cyclicMaxOrPositive(points_.begin(), points_.end(), [&translatedOther](const PointType& a) {
        return orientationDeterminant(translatedOther[1], translatedOther[0], a);
    });
    Segment<PointType> s(*it1, *it2);
    return s.intersects(translatedOther);
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Convex<PointType, LabelType>::intersects(const OtherOrientedSegment& other) const {
    return intersects(Segment<typename OtherOrientedSegment::PointType>(other[0], other[1]));
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Convex<PointType, LabelType>::intersects(const OtherLine& other) const {
    auto translatedOther = other - translation_;
    auto it1 = detail::cyclicMaxOrPositive(points_.begin(), points_.end(), [&translatedOther](const PointType& a) {
        return orientationDeterminant(translatedOther[0], translatedOther[1], a);
    });
    auto it2 = detail::cyclicMaxOrPositive(points_.begin(), points_.end(), [&translatedOther](const PointType& a) {
        return orientationDeterminant(translatedOther[1], translatedOther[0], a);
    });
    Segment<PointType> s(*it1, *it2);
    return s.intersects(translatedOther);
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Convex<PointType, LabelType>::intersects(const OtherOrientedLine& other) const {
    return intersects(Line<typename OtherOrientedLine::PointType>(other[0], other[1]));
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool Convex<PointType, LabelType>::intersects(const OtherRay& other) const {
    if (contains(other.source())) {
        return true;
    }

    auto translatedOther = other - translation_;
    auto it1 = detail::cyclicMaxOrPositive(points_.begin(), points_.end(), [&translatedOther](const PointType& a) {
        return orientationDeterminant(translatedOther[0], translatedOther[1], a);
    });
    auto it2 = detail::cyclicMaxOrPositive(points_.begin(), points_.end(), [&translatedOther](const PointType& a) {
        return orientationDeterminant(translatedOther[1], translatedOther[0], a);
    });
    Segment<PointType> s(*it1, *it2);
    return s.intersects(translatedOther);
}

template <class PointType, class LabelType>
template<RectangleConcept OtherRectangle>
constexpr bool Convex<PointType, LabelType>::intersects(const OtherRectangle& other) const {
    if (size() == 0 || !bbox().intersects(other)) {
        return false;
    }
    if (bbox().separates(other) || other.separates(bbox())) {
        return true;
    }

    if (other.contains(points_[0])) {
        return true;
    }

    for (auto &edge : other.edges()) {
        if (intersects(edge)) {
            return true;
        }
    }

    return false;
}

template <class PointType, class LabelType>
template<TriangleConcept OtherTriangle>
constexpr bool Convex<PointType, LabelType>::intersects(const OtherTriangle& other) const {
    if (size() == 0 || !bbox().intersects(other)) {
        return false;
    }
    if (bbox().separates(other.bbox()) || other.bbox().separates(bbox())) {
        return true;
    }
    if (other.contains(points_[0])) {
        return true;
    }

    for (auto &edge : other.edges()) {
        if (intersects(edge)) {
            return true;
        }
    }

    return false;
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Convex<PointType, LabelType>::intersects(const OtherPoint& other) const {
    if (size() == 0) {
        return false;
    }
    return bbox().contains(other) && contains(other);
}

template <class PointType, class LabelType>
template<HalfplaneConcept OtherHalfplane>
constexpr bool Convex<PointType, LabelType>::intersects(const OtherHalfplane& other) const {
    if (points_.empty()) {
        return false;
    }
    if (other.isDegenerate()) {
        return contains(other.source());
    }
    // The vertex maximizing the inward orientation is the deepest into
    // the half-plane; if it is not in the closed half-plane, no other
    // vertex is either. The orientation determinant is unimodal along
    // the convex polygon's vertex sequence.
    const auto it = detail::cyclicMaxOrPositive(points_.begin(), points_.end(),
        [&other, this](const PointType& a) {
            return orientationDeterminant(other.source(), other.target(), a + translation_);
        });
    return other.contains(*it + translation_);
}

template <class PointType, class LabelType>
template<ConvexConcept OtherConvex>
constexpr bool Convex<PointType, LabelType>::intersects(const OtherConvex& other) const {
    if (size() > other.size()) {
        return other.intersects(*this);
    }
    if (size() == 0 || other.size() == 0) {
        return false;
    }
    if (!bbox().intersects(other.bbox())) {
        return false;
    }
    if (bbox().separates(other.bbox()) || other.bbox().separates(this->bbox())) {
        return true;
    }

    if (contains(other[0]) || other.contains((*this)[0])) {
        return true;
    }

    for (const auto& edge : edges()) {
        if (other.intersects(edge)) {
            return true;
        }
    }

    return false;
}

template <class PointType, class LabelType>
template<DiskConcept OtherDisk>
constexpr bool Convex<PointType, LabelType>::intersects(const OtherDisk& other) const {
    if (contains(other[0])) {
        return true;
    }
    for (auto &edge : edges()) {
        if (edge.intersects(other)) {
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// Disk

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Disk<PointType, LabelType>::intersects(const OtherSegment& other) const {
    if (contains(other.min()) || contains(other.max())) {
        return true;
    }

    // Both endpoints lie strictly outside the closed disk, so the segment meets
    // the disk exactly when the perpendicular foot from the centre falls on the
    // segment *and* reaches inside it. This is decided exactly and without any
    // division (no centre/radius needed): writing power(p) = |p-center|^2 - r^2
    // for the in-circle determinant inCircleDeterminant(a,b,c,p) = -A*power(p),
    // with A = 2*signedArea(a,b,c), the quadratic power along the segment is
    // pinned by its endpoint values, so the centre never appears.
    //
    //   A  = orientation determinant of the three boundary points
    //   J0 = inCircleDeterminant(a,b,c, min), J1 = ... (a,b,c, max)
    //   L  = |max - min|^2,   M = L * A
    // Foot on the segment (t* in [0,1]): |(J0 - J1)*A| <= M*A   (M*A >= 0).
    // Foot reaches the disk (f(t*) <= 0): (J0 + J1 + M)^2 >= 4*J0*J1.
    // Reordering a,b,c flips A and every J together, leaving both tests intact.
    using W = detail::promoted_number_t<
        decltype(inCircleDeterminant(a(), b(), c(), other.min()))>;

    const W det = static_cast<W>(orientationDeterminant(a(), b(), c()));
    const W j0 = static_cast<W>(inCircleDeterminant(a(), b(), c(), other.min()));
    const W j1 = static_cast<W>(inCircleDeterminant(a(), b(), c(), other.max()));
    const W squared_length = static_cast<W>(other.min().squaredDistance(other.max()));
    if (squared_length == W{}) {
        return false;  // Degenerate (point) segment whose sole point is outside.
    }
    const W m = squared_length * det;

    const W projection = (j0 - j1) * det;          // (J0 - J1) * A
    const W half_span = m * det;                   // M * A = L * A^2 >= 0
    const W discriminant_base = j0 + j1 + m;       // J0 + J1 + M

    const bool foot_on_segment = projection >= -half_span && projection <= half_span;
    const bool reaches_disk = discriminant_base * discriminant_base >= W{4} * j0 * j1;

    return foot_on_segment && reaches_disk;
}

template <class PointType, class LabelType>
template<DiskConcept OtherDisk>
constexpr bool Disk<PointType, LabelType>::intersects(const OtherDisk& other) const {
    using R = std::conditional_t<
        std::is_floating_point_v<NumberType> ||
            std::is_floating_point_v<typename OtherDisk::NumberType>,
        long double,
        Rational<BigInt>>;

    const R d2 = center<R>().squaredDistance(other.template center<R>());
    const R r1_sq = squaredRadius<R>();
    const R r2_sq = other.template squaredRadius<R>();

    const R A = d2 - r1_sq - r2_sq;
    return A <= R{} || A * A <= R{4} * r1_sq * r2_sq;
}

template <class PointType, class LabelType>
template <PointConcept OtherPoint>
constexpr bool Convex<PointType, LabelType>::intersects(const Shape<OtherPoint>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->intersects(value);
        },
        other.variant());
}


// ---------------------------------------------------------------------------
// Polygon

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Polygon<PointType, LabelType>::intersects(const OtherPoint& other) const {
    return contains(other);
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Polygon<PointType, LabelType>::intersects(const OtherSegment& other) const {
    // Either an endpoint lies in the closed polygon, or the segment crosses
    // a boundary edge.
    if (contains(other.min()) || contains(other.max())) {
        return true;
    }
    for (const auto& edge : edges()) {
        if (edge.intersects(other)) {
            return true;
        }
    }
    return false;
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Polygon<PointType, LabelType>::intersects(const OtherOrientedSegment& other) const {
    return intersects(static_cast<Segment<typename OtherOrientedSegment::PointType>>(other));
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Polygon<PointType, LabelType>::intersects(const OtherLine& other) const {
    if (other.isDegenerate()) {
        return contains(other.min());
    }
    // The line meets the closed polygon unless every vertex is strictly on one
    // side: the boundary is connected, so vertices straddling the line force an
    // edge crossing (and a vertex on the line is itself an intersection).
    bool positive = false, negative = false;
    for (const auto& vertex : vertices()) {
        const auto o = orientationSign(other.min(), other.max(), vertex);
        if (o == 0) {
            return true;
        }
        negative = negative || o < 0;
        positive = positive || o > 0;
        if (positive && negative) {
            return true;
        }
    }
    return false;
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Polygon<PointType, LabelType>::intersects(const OtherOrientedLine& other) const {
    return intersects(static_cast<Line<typename OtherOrientedLine::PointType>>(other));
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool Polygon<PointType, LabelType>::intersects(const OtherRay& other) const {
    if (contains(other.source())) {
        return true;
    }
    for (const auto& edge : edges()) {
        if (edge.intersects(other)) {
            return true;
        }
    }
    return false;
}

template <class PointType, class LabelType>
template<HalfplaneConcept OtherHalfplane>
constexpr bool Polygon<PointType, LabelType>::intersects(const OtherHalfplane& other) const {
    if (other.isDegenerate()) {
        return contains(other.source());
    }
    // The half-plane is convex, so it meets the polygon iff some vertex lies in
    // it; if every vertex is strictly outside, the whole polygon is too.
    for (const auto& vertex : vertices()) {
        if (other.contains(vertex)) {
            return true;
        }
    }
    return false;
}

// Two filled simple polygons meet iff a vertex of one lies in the other or a
// pair of their edges cross. The region overloads below all share this shape.
template <class PointType, class LabelType>
template<RectangleConcept OtherRectangle>
constexpr bool Polygon<PointType, LabelType>::intersects(const OtherRectangle& other) const {
    if (size() == 0) {
        return false;
    }
    if (!bbox().intersects(other.bbox())) {
        return false;
    }
    if (bbox().separates(other.bbox()) || other.bbox().separates(bbox())) {
        return true;
    }
    if (other.contains((*this)[0]) || contains(other[0])) {
        return true;
    }
    for (const auto& edge : edges()) {
        if (other.intersects(edge)) {
            return true;
        }
    }
    return false;
}

template <class PointType, class LabelType>
template<TriangleConcept OtherTriangle>
constexpr bool Polygon<PointType, LabelType>::intersects(const OtherTriangle& other) const {
    if (size() == 0) {
        return false;
    }
    if (!bbox().intersects(other.bbox())) {
        return false;
    }
    if (bbox().separates(other.bbox()) || other.bbox().separates(bbox())) {
        return true;
    }
    if (other.contains((*this)[0]) || contains(other[0])) {
        return true;
    }
    for (const auto& edge : edges()) {
        if (other.intersects(edge)) {
            return true;
        }
    }
    return false;
}

template <class PointType, class LabelType>
template<ConvexConcept OtherConvex>
constexpr bool Polygon<PointType, LabelType>::intersects(const OtherConvex& other) const {
    if (size() == 0 || other.size() == 0) {
        return false;
    }
    if (!bbox().intersects(other.bbox())) {
        return false;
    }
    if (bbox().separates(other.bbox()) || other.bbox().separates(this->bbox())) {
        return true;
    }
    if (other.contains((*this)[0]) || contains(other[0])) {
        return true;
    }
    for (const auto& edge : edges()) {
        if (other.intersects(edge)) {
            return true;
        }
    }
    return false;
}

template <class PointType, class LabelType>
template<PolygonConcept OtherPolygon>
constexpr bool Polygon<PointType, LabelType>::intersects(const OtherPolygon& other) const {
    if (size() == 0 || other.size() == 0) {
        return false;
    }
    if (!bbox().intersects(other.bbox())) {
        return false;
    }
    if (bbox().separates(other.bbox()) || other.bbox().separates(bbox())) {
        return true;
    }

    // The boundaries touching or crossing settles the closed intersection.
    if (boundariesIntersect(other)) {
        return true;
    }

    // Disjoint boundaries: the polygons are either separate or one lies wholly
    // inside the other, so a single point-in-polygon test each way settles it
    // (every vertex of the inner polygon is contained in the outer one).
    return contains(other.get(0)) || other.contains(get(0));
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Polygon<PointType, LabelType>::intersects(const Shape<OtherPoint>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->intersects(value);
        },
        other.variant());
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Disk<PointType, LabelType>::intersects(const OtherPoint& point) const {
    return contains(point);
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Disk<PointType, LabelType>::intersects(const OtherOrientedSegment& other) const {
    return intersects(other.asSegment());
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Disk<PointType, LabelType>::intersects(const OtherLine& other) const {
    // A line has no endpoints, so there is no containment shortcut: it meets the
    // closed disk exactly when the centre lies within one radius of the line,
    // i.e. when the power quadratic along the line has real roots (discriminant
    // >= 0). Same exact, division-free in-circle formulation as
    // intersects(Segment), evaluated on the two points that define the line and
    // without the [0,1] parameter restriction.
    //
    //   A  = orientation determinant of the three boundary points
    //   J0 = inCircleDeterminant(a,b,c, other[0]), J1 = ... (a,b,c, other[1])
    //   L  = |other[1] - other[0]|^2,   M = L * A
    // Line reaches the closed disk: (J0 + J1 + M)^2 >= 4*J0*J1.
    using W = detail::promoted_number_t<
        decltype(inCircleDeterminant(a(), b(), c(), other[0]))>;

    const W det = static_cast<W>(orientationDeterminant(a(), b(), c()));
    const W j0 = static_cast<W>(inCircleDeterminant(a(), b(), c(), other[0]));
    const W j1 = static_cast<W>(inCircleDeterminant(a(), b(), c(), other[1]));
    const W squared_length = static_cast<W>(other[0].squaredDistance(other[1]));
    if (squared_length == W{}) {
        return false;  // Degenerate line.
    }
    const W discriminant_base = j0 + j1 + squared_length * det;  // J0 + J1 + M

    return discriminant_base * discriminant_base >= W{4} * j0 * j1;
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Disk<PointType, LabelType>::intersects(const OtherOrientedLine& other) const {
    return intersects(other.asLine());
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool Disk<PointType, LabelType>::intersects(const OtherRay& other) const {
    // The source in the closed disk settles it. Otherwise the ray meets the
    // closed disk exactly when its supporting line does (discriminant >= 0) and
    // the contact lies ahead of the source (the perpendicular foot has positive
    // parameter). Same division-free in-circle formulation as
    // intersects(Segment); the source is parameter 0 and the target parameter 1.
    if (contains(other.source())) {
        return true;
    }

    //   A  = orientation determinant of the three boundary points
    //   J0 = inCircleDeterminant(a,b,c, source), J1 = ... (a,b,c, target)
    //   L  = |target - source|^2,   M = L * A
    // Supporting line reaches the closed disk: (J0 + J1 + M)^2 >= 4*J0*J1.
    // Contact ahead of the source (foot parameter t* > 0): (J0 - J1)*A < M*A.
    using W = detail::promoted_number_t<
        decltype(inCircleDeterminant(a(), b(), c(), other.source()))>;

    const W det = static_cast<W>(orientationDeterminant(a(), b(), c()));
    const W j0 = static_cast<W>(inCircleDeterminant(a(), b(), c(), other.source()));
    const W j1 = static_cast<W>(inCircleDeterminant(a(), b(), c(), other.target()));
    const W squared_length = static_cast<W>(other.source().squaredDistance(other.target()));
    const W m = squared_length * det;

    const W projection = (j0 - j1) * det;          // (J0 - J1) * A
    const W half_span = m * det;                   // M * A = L * A^2 >= 0
    const W discriminant_base = j0 + j1 + m;       // J0 + J1 + M

    const bool reaches_disk = discriminant_base * discriminant_base >= W{4} * j0 * j1;
    const bool contact_ahead = projection < half_span;  // foot parameter t* > 0

    return reaches_disk && contact_ahead;
}

template <class PointType, class LabelType>
template<HalfplaneConcept OtherHalfplane>
constexpr bool Disk<PointType, LabelType>::intersects(const OtherHalfplane& other) const {
    return other.contains((*this)[0]) || intersects(other.asLine());
}

template <class PointType, class LabelType>
template<RectangleConcept OtherRectangle>
constexpr bool Disk<PointType, LabelType>::intersects(const OtherRectangle& other) const {
    // A rectangle edge meeting the closed disk covers every case except the disk
    // lying entirely inside the rectangle: an edge meets the disk whenever it
    // crosses it or has an endpoint in it, so a rectangle corner inside the disk
    // is already caught by its incident edges. The remaining case is detected by
    // a disk boundary point lying inside the rectangle.
    for (const auto& edge : other.edges()) {
        if (intersects(edge)) {
            return true;
        }
    }
    return other.contains((*this)[0]);
}

template <class PointType, class LabelType>
template<TriangleConcept OtherTriangle>
constexpr bool Disk<PointType, LabelType>::intersects(const OtherTriangle& other) const {
    // A triangle edge meeting the closed disk covers every case except the disk
    // lying entirely inside the triangle: an edge meets the disk whenever it
    // crosses it or has an endpoint in it, so a triangle vertex inside the disk
    // is already caught by its incident edges. The remaining case is detected by
    // a disk boundary point lying inside the triangle.
    for (const auto& edge : other.edges()) {
        if (intersects(edge)) {
            return true;
        }
    }
    return other.contains((*this)[0]);
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Disk<PointType, LabelType>::intersects(const Shape<OtherPoint>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->intersects(value);
        },
        other.variant());
}


template <class PointType, class LabelType>
template<DiskConcept OtherDisk>
constexpr bool Polygon<PointType, LabelType>::intersects(const OtherDisk& other) const {
    // If the disk meets the polygon without crossing its boundary, the disk lies
    // wholly inside the polygon, so a boundary point of the disk is contained.
    // Every other configuration -- a boundary crossing, or the polygon sitting
    // inside the disk -- is witnessed by some edge meeting the closed disk.
    if (contains(other.a())) {
        return true;
    }
    for (const auto& edge : edges()) {
        if (edge.intersects(other)) {
            return true;
        }
    }
    return false;
}

/**
 * @section predicates-monotonechain MonotoneChain
 * Weakly x-monotone chain predicates: intersection tests restricted by binary
 * search to the edges whose x-range meets the other shape's, and the
 * chain-vs-chain merge sweep.
 */

template <class PointType, class LabelType, class Storage>
template<PointConcept OtherPoint>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::intersects(const OtherPoint& other) const {
    return contains(other);
}

template <class PointType, class LabelType, class Storage>
template<SegmentConcept OtherSegment>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::intersects(const OtherSegment& other) const {
    if (points_.empty()) {
        return false;
    }
    if (points_.size() == 1) {
        return other.contains((*this)[0]);
    }
    const auto window = edgeWindow(other.min().x(), other.max().x());
    if (!window) {
        return false;
    }
    for (std::size_t i = window->first; i <= window->second; ++i) {
        if (this->template boundaryAt<false>(i).intersects(other)) {
            return true;
        }
    }
    return false;
}

template <class PointType, class LabelType, class Storage>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::intersects(const OtherOrientedSegment& other) const {
    return intersects(static_cast<Segment<typename OtherOrientedSegment::PointType>>(other));
}

template <class PointType, class LabelType, class Storage>
template<LineConcept OtherLine>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::intersects(const OtherLine& other) const {
    if (empty()) {
        return false;
    }
    if (size() == 1) {
        return other.contains((*this)[0]);
    }
    for (std::size_t i = 0; i + 1 < size(); ++i) {
        if (this->template boundaryAt<false>(i).intersects(other)) {
            return true;
        }
    }
    return false;
}

template <class PointType, class LabelType, class Storage>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::intersects(const OtherOrientedLine& other) const {
    if (empty()) {
        return false;
    }
    if (size() == 1) {
        return other.contains((*this)[0]);
    }
    for (std::size_t i = 0; i + 1 < size(); ++i) {
        if (this->template boundaryAt<false>(i).intersects(other)) {
            return true;
        }
    }
    return false;
}

template <class PointType, class LabelType, class Storage>
template<RayConcept OtherRay>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::intersects(const OtherRay& other) const {
    if (empty()) {
        return false;
    }
    if (size() == 1) {
        return other.contains((*this)[0]);
    }
    for (std::size_t i = 0; i + 1 < size(); ++i) {
        if (this->template boundaryAt<false>(i).intersects(other)) {
            return true;
        }
    }
    return false;
}

template <class PointType, class LabelType, class Storage>
template<HalfplaneConcept OtherHalfplane>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::intersects(const OtherHalfplane& other) const {
    if (empty()) {
        return false;
    }
    if (size() == 1) {
        return other.contains((*this)[0]);
    }
    for (std::size_t i = 0; i + 1 < size(); ++i) {
        if (this->template boundaryAt<false>(i).intersects(other)) {
            return true;
        }
    }
    return false;
}

template <class PointType, class LabelType, class Storage>
template<RectangleConcept OtherRectangle>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::intersects(const OtherRectangle& other) const {
    if (empty()) {
        return false;
    }
    if (size() == 1) {
        return other.contains((*this)[0]);
    }
    // A rectangle's x-extent is available directly, so only the chain edges in
    // that window can meet it.
    const auto window = edgeWindow(other.min().x(), other.max().x());
    if (!window) {
        return false;
    }
    for (std::size_t i = window->first; i <= window->second; ++i) {
        if (this->template boundaryAt<false>(i).intersects(other)) {
            return true;
        }
    }
    return false;
}

template <class PointType, class LabelType, class Storage>
template<TriangleConcept OtherTriangle>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::intersects(const OtherTriangle& other) const {
    if (empty()) {
        return false;
    }
    if (size() == 1) {
        return other.contains((*this)[0]);
    }
    for (std::size_t i = 0; i + 1 < size(); ++i) {
        if (this->template boundaryAt<false>(i).intersects(other)) {
            return true;
        }
    }
    return false;
}

template <class PointType, class LabelType, class Storage>
template<ConvexConcept OtherConvex>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::intersects(const OtherConvex& other) const {
    if (empty()) {
        return false;
    }
    if (size() == 1) {
        return other.contains((*this)[0]);
    }
    for (std::size_t i = 0; i + 1 < size(); ++i) {
        if (this->template boundaryAt<false>(i).intersects(other)) {
            return true;
        }
    }
    return false;
}

template <class PointType, class LabelType, class Storage>
template<DiskConcept OtherDisk>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::intersects(const OtherDisk& other) const {
    if (empty()) {
        return false;
    }
    if (size() == 1) {
        return other.contains((*this)[0]);
    }
    for (std::size_t i = 0; i + 1 < size(); ++i) {
        if (this->template boundaryAt<false>(i).intersects(other)) {
            return true;
        }
    }
    return false;
}

template <class PointType, class LabelType, class Storage>
template<PointConcept OtherPoint>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::intersects(const Shape<OtherPoint>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->intersects(value);
        },
        other.variant());
}

template <class PointType, class LabelType, class Storage>
template<MonotoneChainConcept OtherChain>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::intersects(const OtherChain& other) const {
    if (empty() || other.empty()) {
        return false;
    }
    if (size() == 1) {
        return other.contains((*this)[0]);
    }
    if (other.size() == 1) {
        return contains(other[0]);
    }
    if ((*this)[size() - 1].x() < other[0].x() || other[other.size() - 1].x() < (*this)[0].x()) {
        return false;
    }
    // Merge sweep: both edge sequences are sorted by x-interval, so advancing
    // the edge with the lexicographically smaller right endpoint visits every
    // pair whose x-ranges overlap. On a tie both advance: the skipped pairs
    // could only meet at that shared right endpoint, which belongs to the pair
    // just tested, so nothing is missed.
    std::size_t i = 0;
    std::size_t j = 0;
    const std::size_t iEnd = size() - 1;
    const std::size_t jEnd = other.size() - 1;
    while (i < iEnd && j < jEnd) {
        const Segment<PointType> mine((*this)[i], (*this)[i + 1]);
        const Segment<typename OtherChain::PointType> theirs(other[j], other[j + 1]);
        if (!(mine.max().x() < theirs.min().x() || theirs.max().x() < mine.min().x()) &&
            mine.intersects(theirs)) {
            return true;
        }
        const auto order = mine.max() <=> theirs.max();
        if (order <= 0) {
            ++i;
        }
        if (order >= 0) {
            ++j;
        }
    }
    return false;
}

template <class PointType, class LabelType>
template<MonotoneChainConcept OtherChain>
constexpr bool Polygon<PointType, LabelType>::intersects(const OtherChain& other) const {
    if (other.empty()) {
        return false;
    }
    if (other.size() == 1) {
        return intersects(other[0]);
    }
    for (std::size_t i = 0; i + 1 < other.size(); ++i) {
        if (intersects(Segment<typename OtherChain::PointType>(other[i], other[i + 1]))) {
            return true;
        }
    }
    return false;
}

}  // namespace pgl
