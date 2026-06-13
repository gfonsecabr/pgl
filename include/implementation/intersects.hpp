 #pragma once

#ifndef PGL_HPP_INCLUDED
#error "Do not include this Pangolin header directly; include \"pgl.hpp\" instead."
#endif

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
            return intersects(value);
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
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool Segment<PointType, LabelType>::intersects(const Segment<OtherPoint, OtherLabel>& other) const {
    if constexpr (is_Rational_v<NumberType> || is_Rational_v<typename OtherPoint::NumberType>) {
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
            return intersects(value);
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
constexpr bool Triangle<PointType>::intersects(const OtherPoint& other) const {
    return contains(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Triangle<PointType>::intersects(const Line<OtherPoint>& other) const {
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

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Triangle<PointType>::intersects(const OrientedLine<OtherPoint>& other) const {
    return intersects(static_cast<Line<OtherPoint>>(other));
}

template <class PointType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool Triangle<PointType>::intersects(const Segment<OtherPoint, OtherLabel>& other) const {
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

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Triangle<PointType>::intersects(const OrientedSegment<OtherPoint>& other) const {
    return intersects(static_cast<Segment<OtherPoint>>(other));
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Triangle<PointType>::intersects(const Ray<OtherPoint>& other) const {
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

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Triangle<PointType>::intersects(const Halfplane<OtherPoint>& other) const {
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

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Triangle<PointType>::intersects(const Rectangle<OtherPoint>& other) const {
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

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Triangle<PointType>::intersects(const Triangle<OtherPoint>& other) const {
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

template <class PointType>
constexpr bool Triangle<PointType>::intersects(const Shape<PointType>& other) const {
    return std::visit(
        [this](const auto& value) {
            return intersects(value);
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
constexpr bool OrientedSegment<PointType>::intersects(const OtherPoint& other) const {
    return static_cast<Segment<PointType>>(*this).intersects(other);
}

template <class PointType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool OrientedSegment<PointType>::intersects(const Segment<OtherPoint, OtherLabel>& other) const {
    return static_cast<Segment<PointType>>(*this).intersects(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedSegment<PointType>::intersects(const OrientedSegment<OtherPoint>& other) const {
    return static_cast<Segment<PointType>>(*this).intersects(static_cast<Segment<OtherPoint>>(other));
}


template <class PointType>
constexpr bool OrientedSegment<PointType>::intersects(const Shape<PointType>& other) const {
    return std::visit(
        [this](const auto& value) {
            return intersects(value);
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
constexpr bool Line<PointType>::intersects(const OtherPoint& other) const {
    return contains(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Line<PointType>::intersects(const Line<OtherPoint>& other) const {
    if (isDegenerate()) {
        return other.contains(min());
    }
    if (other.isDegenerate()) {
        return contains(other.min());
    }
    return !parallel(other) || contains(other.min());
}

template <class PointType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool Line<PointType>::intersects(const Segment<OtherPoint, OtherLabel>& other) const {
    if (other.isDegenerate()) {
        return contains(other.min());
    }
    const auto first = orientationSign(min(), max(), other.min());
    const auto second = orientationSign(min(), max(), other.max());
    return first == std::partial_ordering::equivalent ||
           second == std::partial_ordering::equivalent ||
           first != second;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Line<PointType>::intersects(const OrientedSegment<OtherPoint>& other) const {
    return intersects(static_cast<Segment<OtherPoint>>(other));
}

template <class PointType>
constexpr bool Line<PointType>::intersects(const Shape<PointType>& other) const {
    return std::visit(
        [this](const auto& value) {
            return intersects(value);
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
constexpr bool OrientedLine<PointType>::intersects(const OtherPoint& other) const {
    return this->asLine().intersects(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedLine<PointType>::intersects(const Line<OtherPoint>& other) const {
    return this->asLine().intersects(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedLine<PointType>::intersects(const OrientedLine<OtherPoint>& other) const {
    return this->asLine().intersects(other.asLine());
}

template <class PointType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool OrientedLine<PointType>::intersects(const Segment<OtherPoint, OtherLabel>& other) const {
    return this->asLine().intersects(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedLine<PointType>::intersects(const OrientedSegment<OtherPoint>& other) const {
    return this->asLine().intersects(other);
}

template <class PointType>
constexpr bool OrientedLine<PointType>::intersects(const Shape<PointType>& other) const {
    return std::visit(
        [this](const auto& value) {
            return intersects(value);
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
constexpr bool Ray<PointType>::intersects(const OtherPoint& other) const {
    return contains(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Ray<PointType>::intersects(const Line<OtherPoint>& other) const {
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

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Ray<PointType>::intersects(const OrientedLine<OtherPoint>& other) const {
    return intersects(other.asLine());
}

template <class PointType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool Ray<PointType>::intersects(const Segment<OtherPoint, OtherLabel>& other) const {
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

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Ray<PointType>::intersects(const OrientedSegment<OtherPoint>& other) const {
    return intersects(static_cast<Segment<OtherPoint>>(other));
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Ray<PointType>::intersects(const Ray<OtherPoint>& other) const {
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

template <class PointType>
constexpr bool Ray<PointType>::intersects(const Shape<PointType>& other) const {
    return std::visit(
        [this](const auto& value) {
            return intersects(value);
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
constexpr bool Rectangle<PointType>::intersects(const OtherPoint& other) const {
    return contains(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Rectangle<PointType>::intersects(const Rectangle<OtherPoint>& other) const {
    return intervalsOverlap(min().x(), max().x(), other.min().x(), other.max().x()) &&
           intervalsOverlap(min().y(), max().y(), other.min().y(), other.max().y());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Rectangle<PointType>::intersects(const Line<OtherPoint>& other) const {
    if (other.isDegenerate()) {
        return contains(other.min());
    }
    return detail::lineIntersectsRectangle(*this, other.min(), other.max());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Rectangle<PointType>::intersects(const OrientedLine<OtherPoint>& other) const {
    if (other.isDegenerate()) {
        return contains(other.source());
    }
    return detail::lineIntersectsRectangle(*this, other.source(), other.target());
}

template <class PointType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool Rectangle<PointType>::intersects(const Segment<OtherPoint, OtherLabel>& other) const {
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

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Rectangle<PointType>::intersects(const OrientedSegment<OtherPoint>& other) const {
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

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Rectangle<PointType>::intersects(const Ray<OtherPoint>& other) const {
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

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Rectangle<PointType>::intersects(const Halfplane<OtherPoint>& other) const {
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

template <class PointType>
constexpr bool Rectangle<PointType>::intersects(const Shape<PointType>& other) const {
    return std::visit(
        [this](const auto& value) {
            return intersects(value);
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
constexpr bool Halfplane<PointType>::intersects(const OtherPoint& other) const {
    return contains(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Halfplane<PointType>::intersects(const Line<OtherPoint>& other) const {
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

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Halfplane<PointType>::intersects(const OrientedLine<OtherPoint>& other) const {
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

template <class PointType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool Halfplane<PointType>::intersects(const Segment<OtherPoint, OtherLabel>& other) const {
    if (isDegenerate()) {
        return other.contains(source());
    }
    return contains(other.min()) || contains(other.max());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Halfplane<PointType>::intersects(const OrientedSegment<OtherPoint>& other) const {
    if (isDegenerate()) {
        return other.contains(source());
    }
    return contains(other.source()) || contains(other.target());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Halfplane<PointType>::intersects(const Ray<OtherPoint>& other) const {
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

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Halfplane<PointType>::intersects(const Halfplane<OtherPoint>& other) const {
    return contains(other.source()) || contains(other.target()) || other.contains(source()) || other.contains(target());
}

template <class PointType>
constexpr bool Halfplane<PointType>::intersects(const Shape<PointType>& other) const {
    return std::visit(
        [this](const auto& value) {
            return intersects(value);
        },
        other.variant());
}


// ---------------------------------------------------------------------------
// Convex

template <class PointType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool Convex<PointType>::intersects(const Segment<OtherPoint, OtherLabel>& other) const {
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

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Convex<PointType>::intersects(const OrientedSegment<OtherPoint>& other) const {
    return intersects(Segment<OtherPoint>(other[0], other[1]));
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Convex<PointType>::intersects(const Line<OtherPoint>& other) const {
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

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Convex<PointType>::intersects(const OrientedLine<OtherPoint>& other) const {
    return intersects(Line<OtherPoint>(other[0], other[1]));
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Convex<PointType>::intersects(const Ray<OtherPoint>& other) const {
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

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Convex<PointType>::intersects(const Rectangle<OtherPoint>& other) const {
    if (other.contains(points_[0])) {
        return true;
    }

    if (!bbox().intersects(other)) {
        return false;
    }
    if (bbox().separates(other) || other.separates(bbox())) {
        return true;
    }

    for (auto &edge : other.edges()) {
        if (intersects(edge)) {
            return true;
        }
    }

    return false;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Convex<PointType>::intersects(const Triangle<OtherPoint>& other) const {
    if (other.contains(points_[0])) {
        return true;
    }
    if (!bbox().intersects(other)) {
        return false;
    }
    if (bbox().separates(other)) {
        return true;
    }

    for (auto &edge : other.edges()) {
        if (intersects(edge)) {
            return true;
        }
    }

    return false;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Convex<PointType>::intersects(const OtherPoint& other) const {
    return contains(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Convex<PointType>::intersects(const Halfplane<OtherPoint>& other) const {
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

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Convex<PointType>::intersects(const Convex<OtherPoint>& other) const {
    if (size() == 0 || other.size() == 0) {
        return false;
    }
    if (!bbox().intersects(other.bbox())) {
        return false;
    }
    if (bbox().crosses(other.bbox())) {
        return true;
    }
    if (size() > other.size()) {
        return other.intersects(*this);
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

template <class PointType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool Convex<PointType>::intersects(const Disk<OtherPoint, OtherLabel>& other) const {
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
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool Disk<PointType, LabelType>::intersects(const Segment<OtherPoint, OtherLabel>& other) const {
    if (contains(other.min()) || contains(other.max())) {
        return true;
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

    // Both endpoints lie outside the closed disk, so they meet exactly when the
    // perpendicular foot from the centre falls inside the segment and the centre
    // lies within one radius of the supporting line (tangency counts).
    return foot_projection > R{} && foot_projection < squared_length && cross * cross <= squared_radius * squared_length;
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool Disk<PointType, LabelType>::intersects(const Disk<OtherPoint, OtherLabel>& other) const {
    using R = detail::promoted_number_t<std::common_type_t<
        decltype(squaredRadius()),
        decltype(other.squaredRadius())>>;

    const R d2 = center<R>().squaredDistance(other.template center<R>());
    const R r1_sq = squaredRadius<R>();
    const R r2_sq = other.template squaredRadius<R>();

    const R A = d2 - r1_sq - r2_sq;
    return A <= R{} || A * A <= R{4} * r1_sq * r2_sq;
}

template <class PointType>
template <PointConcept OtherPoint>
constexpr bool Convex<PointType>::intersects(const Shape<OtherPoint>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->intersects(value);
        },
        other.variant());
}


// ---------------------------------------------------------------------------
// Polygon

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Polygon<PointType>::intersects(const OtherPoint& other) const {
    return contains(other);
}

template <class PointType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool Polygon<PointType>::intersects(const Segment<OtherPoint, OtherLabel>& other) const {
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

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Polygon<PointType>::intersects(const OrientedSegment<OtherPoint>& other) const {
    return intersects(static_cast<Segment<OtherPoint>>(other));
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Polygon<PointType>::intersects(const Line<OtherPoint>& other) const {
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

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Polygon<PointType>::intersects(const OrientedLine<OtherPoint>& other) const {
    return intersects(static_cast<Line<OtherPoint>>(other));
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Polygon<PointType>::intersects(const Ray<OtherPoint>& other) const {
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

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Polygon<PointType>::intersects(const Halfplane<OtherPoint>& other) const {
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
template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Polygon<PointType>::intersects(const Rectangle<OtherPoint>& other) const {
    for (const auto& vertex : other.vertices()) {
        if (contains(vertex)) {
            return true;
        }
    }
    for (const auto& vertex : vertices()) {
        if (other.contains(vertex)) {
            return true;
        }
    }
    for (const auto& edge : edges()) {
        for (const auto& otherEdge : other.edges()) {
            if (edge.intersects(otherEdge)) {
                return true;
            }
        }
    }
    return false;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Polygon<PointType>::intersects(const Triangle<OtherPoint>& other) const {
    for (const auto& vertex : other.vertices()) {
        if (contains(vertex)) {
            return true;
        }
    }
    for (const auto& vertex : vertices()) {
        if (other.contains(vertex)) {
            return true;
        }
    }
    for (const auto& edge : edges()) {
        for (const auto& otherEdge : other.edges()) {
            if (edge.intersects(otherEdge)) {
                return true;
            }
        }
    }
    return false;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Polygon<PointType>::intersects(const Convex<OtherPoint>& other) const {
    for (const auto& vertex : other.vertices()) {
        if (contains(vertex)) {
            return true;
        }
    }
    for (const auto& vertex : vertices()) {
        if (other.contains(vertex)) {
            return true;
        }
    }
    for (const auto& edge : edges()) {
        for (const auto& otherEdge : other.edges()) {
            if (edge.intersects(otherEdge)) {
                return true;
            }
        }
    }
    return false;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Polygon<PointType>::intersects(const Polygon<OtherPoint>& other) const {
    if (size() == 0 || other.size() == 0) {
        return false;
    }
    if (!bbox().intersects(other.bbox())) {
        return false;
    }
    if (bbox().separates(other.bbox()) || other.bbox().separates(bbox())) {
        return true;
    }

    for (const auto& vertex : other.vertices()) {
        if (contains(vertex)) {
            return true;
        }
    }
    for (const auto& vertex : vertices()) {
        if (other.contains(vertex)) {
            return true;
        }
    }
    for (const auto& edge : edges()) {
        for (const auto& otherEdge : other.edges()) {
            if (edge.intersects(otherEdge)) {
                return true;
            }
        }
    }
    return false;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Convex<PointType>::intersects(const Polygon<OtherPoint>& other) const {
    return other.intersects(*this);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Polygon<PointType>::intersects(const Shape<OtherPoint>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->intersects(value);
        },
        other.variant());
}


// --- Disk symmetric-trio stubs (not yet implemented) + Shape dispatch ---

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Disk<PointType, LabelType>::intersects(const OtherPoint& point) const {
    return contains(point);
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Disk<PointType, LabelType>::intersects(const OrientedSegment<OtherPoint>&) const {
    throw std::runtime_error(
        "pgl: Disk::intersects(OrientedSegment) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Disk<PointType, LabelType>::intersects(const Line<OtherPoint>&) const {
    throw std::runtime_error(
        "pgl: Disk::intersects(Line) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Disk<PointType, LabelType>::intersects(const OrientedLine<OtherPoint>&) const {
    throw std::runtime_error(
        "pgl: Disk::intersects(OrientedLine) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Disk<PointType, LabelType>::intersects(const Ray<OtherPoint>&) const {
    throw std::runtime_error(
        "pgl: Disk::intersects(Ray) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Disk<PointType, LabelType>::intersects(const Halfplane<OtherPoint>&) const {
    throw std::runtime_error(
        "pgl: Disk::intersects(Halfplane) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Disk<PointType, LabelType>::intersects(const Rectangle<OtherPoint>&) const {
    throw std::runtime_error(
        "pgl: Disk::intersects(Rectangle) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Disk<PointType, LabelType>::intersects(const Triangle<OtherPoint>&) const {
    throw std::runtime_error(
        "pgl: Disk::intersects(Triangle) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
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


template <class PointType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool Polygon<PointType>::intersects(const Disk<OtherPoint, OtherLabel>&) const {
    throw std::runtime_error(
        "pgl: Polygon::intersects(Disk) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
}

}  // namespace pgl
