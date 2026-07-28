#pragma once

#include "implementation/interiorcontains.hpp"

/**
 * @file interiorsintersect.hpp
 * @brief Implementations of the 'interiorsIntersect' predicate.
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
constexpr bool Point<Number, Label>::interiorsIntersect(const OtherPoint& other) const {
    // A point's interior is the point itself, so interiors intersect exactly
    // when its interior contains the other shape.
    return interiorContains(other);
}

/**
 * @section predicates-segment Segment
 * Segment endpoint, boundary, containment, collinearity, intersection, and
 * topological predicates, including the generic `separates` / `crosses`
 * dispatch used against 1D and area targets.
 */

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Segment<PointType, LabelType>::interiorsIntersect(const OtherPoint& other) const {
    // A point's interior is the point itself, so this matches interiorContains.
    return interiorContains(other);
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Segment<PointType, LabelType>::interiorsIntersect(const OtherSegment& other) const {
    if constexpr (is_Rational_v<NumberType> || is_Rational_v<typename OtherSegment::NumberType>) {
        const int cross = boundingBoxesCross(other);
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
    if (min() == max() || other.min() == other.max()) {
        return false;
    }
    const auto& a = min();
    const auto& b = max();
    const auto& c = other.min();
    const auto& d = other.max();
    const auto d1 = orientationSign(a, b, c);
    const auto d2 = orientationSign(a, b, d);
    const auto d3 = orientationSign(c, d, a);
    const auto d4 = orientationSign(c, d, b);
    const bool no_endpoint_is_collinear = d1 != 0 && d2 != 0 && d3 != 0 && d4 != 0;
    const bool this_segment_straddles_other = (d1 > 0) != (d2 > 0);
    const bool other_segment_straddles_this = (d3 > 0) != (d4 > 0);
    const bool proper_cross =
        no_endpoint_is_collinear &&
        this_segment_straddles_other &&
        other_segment_straddles_this;
    if (proper_cross) {
        return true;
    }
    if (d1 != 0 || d2 != 0) {
        return false;
    }
    // Collinear: the relative interiors meet iff the two overlap along a
    // sub-segment of positive length, which happens iff an endpoint of one lies
    // strictly inside the other — or the two coincide, the single overlapping
    // configuration in which neither holds an endpoint of the other strictly
    // inside (any other positive-length overlap pushes one endpoint in).
    return interiorContains(c) ||
           interiorContains(d) ||
           other.interiorContains(a) ||
           other.interiorContains(b) ||
           (a == c && b == d);
}

template <class PointType, class LabelType>
constexpr bool Segment<PointType, LabelType>::interiorsIntersect(const Shape<PointType>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->interiorsIntersect(value);
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
constexpr bool Triangle<PointType, LabelType>::interiorsIntersect(const OtherPoint& other) const {
    // A point's interior is the point itself, so this matches interiorContains.
    return interiorContains(other);
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Triangle<PointType, LabelType>::interiorsIntersect(const OtherLine& other) const {
    if (isDegenerate()) {
        return false;
    }
    if (other.isDegenerate()) {
        return interiorContains(other.min());
    }
    bool has_positive = false;
    bool has_negative = false;
    const auto triangle_vertices = vertices();
    for (const auto& vertex : triangle_vertices) {
        const auto side = orientationSign(other.min(), other.max(), vertex);
        has_positive = has_positive || side == std::partial_ordering::greater;
        has_negative = has_negative || side == std::partial_ordering::less;
        if (has_positive && has_negative) {
            return true;
        }
    }
    return false;
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Triangle<PointType, LabelType>::interiorsIntersect(const OtherOrientedLine& other) const {
    if (isDegenerate()) {
        return false;
    }
    if (other.isDegenerate()) {
        return interiorContains(other.source());
    }
    bool has_positive = false;
    bool has_negative = false;
    const auto triangle_vertices = vertices();
    for (const auto& vertex : triangle_vertices) {
        const auto side = orientationSign(other.source(), other.target(), vertex);
        has_positive = has_positive || side == std::partial_ordering::greater;
        has_negative = has_negative || side == std::partial_ordering::less;
        if (has_positive && has_negative) {
            return true;
        }
    }
    return false;
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Triangle<PointType, LabelType>::interiorsIntersect(const OtherSegment& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }
    return interiorContains(other.min()) ||
           interiorContains(other.max()) ||
           other.separates(*this);
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Triangle<PointType, LabelType>::interiorsIntersect(const OtherOrientedSegment& other) const {
    return interiorsIntersect(static_cast<Segment<typename OtherOrientedSegment::PointType>>(other));
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool Triangle<PointType, LabelType>::interiorsIntersect(const OtherRay& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }
    return interiorContains(other.source()) || other.separates(*this);
}

template <class PointType, class LabelType>
template<HalfplaneConcept OtherHalfplane>
constexpr bool Triangle<PointType, LabelType>::interiorsIntersect(const OtherHalfplane& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }
    return other.interiorContains(a()) || other.interiorContains(b()) || other.interiorContains(c());
}

template <class PointType, class LabelType>
template<RectangleConcept OtherRectangle>
constexpr bool Triangle<PointType, LabelType>::interiorsIntersect(const OtherRectangle& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }
    if (other.interiorContains(a()) || other.interiorContains(b()) || other.interiorContains(c())) {
        return true;
    }
    const auto rectangle_vertices = other.vertices();
    for (const auto& vertex : rectangle_vertices) {
        if (interiorContains(vertex)) {
            return true;
        }
    }
    const auto rectangle_edges = other.edges();
    for (const auto& edge : rectangle_edges) {
        if (edge.separates(*this)) {
            return true;
        }
    }
    const auto triangle_edges = edges();
    for (const auto& edge : triangle_edges) {
        if (edge.separates(other)) {
            return true;
        }
    }
    return false;
}

template <class PointType, class LabelType>
template<TriangleConcept OtherTriangle>
constexpr bool Triangle<PointType, LabelType>::interiorsIntersect(const OtherTriangle& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }

    if (!bbox().intersects(other.bbox())) {
        return false;
    }

    for (const auto& thisEdge : edges()) {
        if (thisEdge.interiorsIntersect(other)) {
            return true;
        }
    }

    for (const auto& otherEdge : other.edges()) {
        if (otherEdge.interiorsIntersect(*this)) {
            return true;
        }
    }

    return other == *this;
}

template <class PointType, class LabelType>
constexpr bool Triangle<PointType, LabelType>::interiorsIntersect(const Shape<PointType>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->interiorsIntersect(value);
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
constexpr bool OrientedSegment<PointType, LabelType>::interiorsIntersect(const OtherPoint& other) const {
    return this->asSegment().interiorsIntersect(other);
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool OrientedSegment<PointType, LabelType>::interiorsIntersect(const OtherSegment& other) const {
    return this->asSegment().interiorsIntersect(other);
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool OrientedSegment<PointType, LabelType>::interiorsIntersect(const OtherOrientedSegment& other) const {
    return this->asSegment().interiorsIntersect(other.asSegment());
}

template <class PointType, class LabelType>
constexpr bool OrientedSegment<PointType, LabelType>::interiorsIntersect(const Shape<PointType>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->interiorsIntersect(value);
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
constexpr bool Line<PointType, LabelType>::interiorsIntersect(const OtherPoint& other) const {
    // A point's interior is the point itself, so this matches interiorContains.
    return interiorContains(other);
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Line<PointType, LabelType>::interiorsIntersect(const OtherLine& other) const {
    return intersects(other);
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Line<PointType, LabelType>::interiorsIntersect(const OtherSegment& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }
    const auto first_side = orientationSign(min(), max(), other.min());
    const auto second_side = orientationSign(min(), max(), other.max());
    if (first_side == std::partial_ordering::equivalent &&
        second_side == std::partial_ordering::equivalent) {
        return true;
    }
    if (first_side == std::partial_ordering::equivalent ||
        second_side == std::partial_ordering::equivalent) {
        return false;
    }
    return first_side != second_side;
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Line<PointType, LabelType>::interiorsIntersect(const OtherOrientedSegment& other) const {
    return interiorsIntersect(static_cast<Segment<typename OtherOrientedSegment::PointType>>(other));
}

template <class PointType, class LabelType>
constexpr bool Line<PointType, LabelType>::interiorsIntersect(const Shape<PointType>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->interiorsIntersect(value);
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
constexpr bool OrientedLine<PointType, LabelType>::interiorsIntersect(const OtherPoint& other) const {
    return this->asLine().interiorsIntersect(other);
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool OrientedLine<PointType, LabelType>::interiorsIntersect(const OtherLine& other) const {
    return intersects(other);
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool OrientedLine<PointType, LabelType>::interiorsIntersect(const OtherOrientedLine& other) const {
    return intersects(other);
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool OrientedLine<PointType, LabelType>::interiorsIntersect(const OtherSegment& other) const {
    return this->asLine().interiorsIntersect(other);
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool OrientedLine<PointType, LabelType>::interiorsIntersect(const OtherOrientedSegment& other) const {
    return this->asLine().interiorsIntersect(other);
}

template <class PointType, class LabelType>
constexpr bool OrientedLine<PointType, LabelType>::interiorsIntersect(const Shape<PointType>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->interiorsIntersect(value);
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
constexpr bool Ray<PointType, LabelType>::interiorsIntersect(const OtherPoint& other) const {
    // A point's interior is the point itself, so this matches interiorContains.
    return interiorContains(other);
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Ray<PointType, LabelType>::interiorsIntersect(const OtherLine& other) const {
    if (other.isDegenerate() || isDegenerate()) {
        return false;
    }

    const auto source_side = orientationSign(other.min(), other.max(), source());
    if (source_side == 0) {
        return other.contains(target());  // source lies on the line
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
constexpr bool Ray<PointType, LabelType>::interiorsIntersect(const OtherOrientedLine& other) const {
    return interiorsIntersect(other.asLine());
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Ray<PointType, LabelType>::interiorsIntersect(const OtherSegment& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }

    const auto ray_min_side = orientationSign(source(), target(), other.min());
    const auto ray_max_side = orientationSign(source(), target(), other.max());

    if (ray_min_side == std::partial_ordering::equivalent &&
        ray_max_side == std::partial_ordering::equivalent) {
        return interiorContains(other.min()) ||
               interiorContains(other.max()) ||
               other.interiorContains(source());
    }

    if (ray_min_side == std::partial_ordering::equivalent ||
        ray_max_side == std::partial_ordering::equivalent ||
        ray_min_side == ray_max_side) {
        return false;
    }

    // The segment straddles the ray's line, so the lines cross at a point
    // interior to the segment. The interiors meet iff that crossing is strictly
    // ahead of the ray's source: the source is off the segment's line and the
    // ray's direction tends to the opposite side of it.
    const auto source_side = orientationSign(other.min(), other.max(), source());
    if (source_side == std::partial_ordering::equivalent) {
        return false;
    }
    const auto direction_side =
        orientationSign(other.min(), other.max(), other.min() + (target() - source()));
    return direction_side != std::partial_ordering::equivalent &&
           direction_side != source_side;
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Ray<PointType, LabelType>::interiorsIntersect(const OtherOrientedSegment& other) const {
    return interiorsIntersect(static_cast<Segment<typename OtherOrientedSegment::PointType>>(other));
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool Ray<PointType, LabelType>::interiorsIntersect(const OtherRay& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }

    const auto other_source_side = orientationSign(source(), target(), other.source());
    const auto other_target_side = orientationSign(source(), target(), other.target());

    if (other_source_side == std::partial_ordering::equivalent &&
        other_target_side == std::partial_ordering::equivalent) {
        // Collinear, as in the segment case: one source lies strictly inside the
        // other ray, or the two rays coincide — same source, same direction —
        // which is the overlapping configuration neither test catches.
        return interiorContains(other.source()) || other.interiorContains(source()) ||
               (source() == other.source() && contains(other.target()));
    }

    if (other_source_side == other_target_side &&
        other_source_side != std::partial_ordering::equivalent) {
        return false;
    }

    const auto this_source_side = orientationSign(other.source(), other.target(), source());
    const auto this_target_side = orientationSign(other.source(), other.target(), target());

    if (this_source_side == this_target_side &&
        this_source_side != std::partial_ordering::equivalent) {
        return false;
    }

    return !boundaryContains(other.source()) && !other.boundaryContains(source());
}

template <class PointType, class LabelType>
constexpr bool Ray<PointType, LabelType>::interiorsIntersect(const Shape<PointType>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->interiorsIntersect(value);
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
constexpr bool Rectangle<PointType, LabelType>::interiorsIntersect(const OtherPoint& other) const {
    // A point's interior is the point itself, so this matches interiorContains.
    return interiorContains(other);
}

template <class PointType, class LabelType>
template<RectangleConcept OtherRectangle>
constexpr bool Rectangle<PointType, LabelType>::interiorsIntersect(const OtherRectangle& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }
    return intervalsOverlapStrict(min().x(), max().x(), other.min().x(), other.max().x()) &&
           intervalsOverlapStrict(min().y(), max().y(), other.min().y(), other.max().y());
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Rectangle<PointType, LabelType>::interiorsIntersect(const OtherLine& other) const {
    if (isDegenerate()) {
        return false;
    }
    if (other.isDegenerate()) {
        return interiorContains(other.min());
    }
    return detail::lineIntersectsRectangleInterior(*this, other.min(), other.max());
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Rectangle<PointType, LabelType>::interiorsIntersect(const OtherOrientedLine& other) const {
    if (isDegenerate()) {
        return false;
    }
    if (other.isDegenerate()) {
        return interiorContains(other.source());
    }
    return detail::lineIntersectsRectangleInterior(*this, other.source(), other.target());
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Rectangle<PointType, LabelType>::interiorsIntersect(const OtherSegment& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }
    return detail::segmentIntersectsRectangleInteriorExact(*this, other.min(), other.max());
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Rectangle<PointType, LabelType>::interiorsIntersect(const OtherOrientedSegment& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }
    return detail::segmentIntersectsRectangleInteriorExact(*this, other.source(), other.target());
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool Rectangle<PointType, LabelType>::interiorsIntersect(const OtherRay& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }
    if (interiorContains(other.source())) {
        return true;
    }
    return Segment<PointType>((*this)[0], (*this)[2]).interiorsIntersect(other) ||
           Segment<PointType>((*this)[1], (*this)[3]).interiorsIntersect(other);
}

template <class PointType, class LabelType>
template<HalfplaneConcept OtherHalfplane>
constexpr bool Rectangle<PointType, LabelType>::interiorsIntersect(const OtherHalfplane& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }
    const auto rectangle_vertices = vertices();
    for (const auto& vertex : rectangle_vertices) {
        if (other.interiorContains(vertex)) {
            return true;
        }
    }
    return false;
}

template <class PointType, class LabelType>
constexpr bool Rectangle<PointType, LabelType>::interiorsIntersect(const Shape<PointType>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->interiorsIntersect(value);
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
constexpr bool Halfplane<PointType, LabelType>::interiorsIntersect(const OtherPoint& other) const {
    // A point's interior is the point itself, so this matches interiorContains.
    return interiorContains(other);
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Halfplane<PointType, LabelType>::interiorsIntersect(const OtherLine& other) const {
    if (isDegenerate()) {
        return false;
    }
    if (other.isDegenerate()) {
        return interiorContains(other.min());
    }
    const auto direction_side =
        orientationDeterminant(source(), target(), other.max()) -
        orientationDeterminant(source(), target(), other.min());
    return direction_side != decltype(direction_side){} || interiorContains(other.min());
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Halfplane<PointType, LabelType>::interiorsIntersect(const OtherOrientedLine& other) const {
    if (isDegenerate()) {
        return false;
    }
    if (other.isDegenerate()) {
        return interiorContains(other.source());
    }
    const auto direction_side =
        orientationDeterminant(source(), target(), other.target()) -
        orientationDeterminant(source(), target(), other.source());
    return direction_side != decltype(direction_side){} || interiorContains(other.source());
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Halfplane<PointType, LabelType>::interiorsIntersect(const OtherSegment& other) const {
    if (isDegenerate() || other.isPoint()) {
        // A segment collapsed to a point is all boundary and has no interior.
        return false;
    }
    const auto first_side = orientationDeterminant(source(), target(), other.min());
    const auto second_side = orientationDeterminant(source(), target(), other.max());
    const auto zero = decltype(first_side){};
    return zero < first_side || zero < second_side;
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Halfplane<PointType, LabelType>::interiorsIntersect(const OtherOrientedSegment& other) const {
    if (isDegenerate() || other.isPoint()) {
        // A segment collapsed to a point is all boundary and has no interior.
        return false;
    }
    const auto first_side = orientationDeterminant(source(), target(), other.source());
    const auto second_side = orientationDeterminant(source(), target(), other.target());
    const auto zero = decltype(first_side){};
    return zero < first_side || zero < second_side;
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool Halfplane<PointType, LabelType>::interiorsIntersect(const OtherRay& other) const {
    if (isDegenerate()) {
        return false;
    }
    if (other.isDegenerate()) {
        return interiorContains(other.source());
    }
    const auto source_side = orientationDeterminant(source(), target(), other.source());
    const auto direction_side =
        orientationDeterminant(source(), target(), other.target()) -
        orientationDeterminant(source(), target(), other.source());
    const auto zero = decltype(source_side){};
    return zero < source_side || zero < direction_side;
}

template <class PointType, class LabelType>
template<HalfplaneConcept OtherHalfplane>
constexpr bool Halfplane<PointType, LabelType>::interiorsIntersect(const OtherHalfplane& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }

    const auto this_boundary = this->asLine();
    const auto other_boundary = other.asLine();
    if (!this_boundary.parallel(other_boundary)) {
        return true;
    }

    const auto side_of_other_source = orientationDeterminant(source(), target(), other.source());
    const auto zero = decltype(side_of_other_source){};
    if (zero < side_of_other_source) {
        return true;
    }

    const auto side_of_this_source = orientationDeterminant(other.source(), other.target(), source());
    if (zero < side_of_this_source) {
        return true;
    }

    const auto this_oriented_boundary = static_cast<OrientedLine<PointType>>(*this);
    const auto other_oriented_boundary = static_cast<OrientedLine<typename OtherHalfplane::PointType>>(other);
    return this_oriented_boundary == other_oriented_boundary;
}

template <class PointType, class LabelType>
constexpr bool Halfplane<PointType, LabelType>::interiorsIntersect(const Shape<PointType>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->interiorsIntersect(value);
        },
        other.variant());
}


// ---------------------------------------------------------------------------
// Convex

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Convex<PointType, LabelType>::interiorsIntersect(const OtherPoint& other) const {
    // A point's interior is the point itself, so this matches interiorContains.
    return interiorContains(other);
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Convex<PointType, LabelType>::interiorsIntersect(const OtherLine& other) const {
    if (isDegenerate()) {
        return false;
    }
    if (other.isDegenerate()) {
        return interiorContains(other.min());
    }
    // The line cuts the interior iff the convex polygon has vertices strictly on
    // both sides. Find the vertex with maximum signed distance and the
    // one with minimum (= max of the negated key) in O(log n) each.
    const auto max_it = detail::cyclicMax(points_.begin(), points_.end(),
        [&other, this](const PointType& a) {
            return orientationDeterminant(other.min(), other.max(), a + translation_);
        });
    const auto min_it = detail::cyclicMax(points_.begin(), points_.end(),
        [&other, this](const PointType& a) {
            return -orientationDeterminant(other.min(), other.max(), a + translation_);
        });
    const auto max_val = orientationDeterminant(other.min(), other.max(), *max_it + translation_);
    const auto min_val = orientationDeterminant(other.min(), other.max(), *min_it + translation_);
    return max_val > 0 && min_val < 0;
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Convex<PointType, LabelType>::interiorsIntersect(const OtherOrientedLine& other) const {
    return interiorsIntersect(static_cast<Line<typename OtherOrientedLine::PointType>>(other));
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Convex<PointType, LabelType>::interiorsIntersect(const OtherSegment& other) const {
    if (isDegenerate() || other.isDegenerate() || !bbox().intersects(other.bbox())) {
        return false;
    }
    if (interiorContains(other.min()) || interiorContains(other.max())) {
        return true;
    }
    auto translatedOther = other - translation_;
    auto it1 = detail::cyclicMaxOrPositive(points_.begin(), points_.end(), [&translatedOther](const PointType& a) {
        return orientationDeterminant(translatedOther[0], translatedOther[1], a);
    });
    auto it2 = detail::cyclicMaxOrPositive(points_.begin(), points_.end(), [&translatedOther](const PointType& a) {
        return orientationDeterminant(translatedOther[1], translatedOther[0], a);
    });
    auto i3 = it1 - points_.begin() - 1;
    i3 = i3 < 0 ? points_.size()-1 : i3;
    if (points_[i3] == *it2) {
        i3 = (i3+2) % points_.size();
    }

    Triangle<PointType> tri(*it1, *it2, points_[i3]);
    return tri.interiorsIntersect(translatedOther);
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Convex<PointType, LabelType>::interiorsIntersect(const OtherOrientedSegment& other) const {
    return interiorsIntersect(static_cast<Segment<typename OtherOrientedSegment::PointType>>(other));
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool Convex<PointType, LabelType>::interiorsIntersect(const OtherRay& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }
    if (interiorContains(other.source())) {
        return true;
    }

    auto translatedOther = other - translation_;
    auto it1 = detail::cyclicMaxOrPositive(points_.begin(), points_.end(), [&translatedOther](const PointType& a) {
        return orientationDeterminant(translatedOther[0], translatedOther[1], a);
    });
    auto it2 = detail::cyclicMaxOrPositive(points_.begin(), points_.end(), [&translatedOther](const PointType& a) {
        return orientationDeterminant(translatedOther[1], translatedOther[0], a);
    });
    auto i3 = it1 - points_.begin() - 1;
    i3 = i3 < 0 ? points_.size()-1 : i3;
    if (points_[i3] == *it2) {
        i3 = (i3+2) % points_.size();
    }

    Triangle<PointType> tri(*it1, *it2, points_[i3]);
    return tri.interiorsIntersect(translatedOther);
}

template <class PointType, class LabelType>
template<HalfplaneConcept OtherHalfplane>
constexpr bool Convex<PointType, LabelType>::interiorsIntersect(const OtherHalfplane& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }
    // The vertex maximizing the inward orientation is the deepest into
    // the half-plane. If it is not strictly inside, no vertex is.
    const auto it = detail::cyclicMaxOrPositive(points_.begin(), points_.end(),
        [&other, this](const PointType& a) {
            return orientationDeterminant(other.source(), other.target(), a + translation_);
        });
    return other.interiorContains(*it + translation_);
}

template <class PointType, class LabelType>
template<RectangleConcept OtherRectangle>
constexpr bool Convex<PointType, LabelType>::interiorsIntersect(const OtherRectangle& other) const {
    return interiorsIntersect(other.asConvex());
}

template <class PointType, class LabelType>
template<TriangleConcept OtherTriangle>
constexpr bool Convex<PointType, LabelType>::interiorsIntersect(const OtherTriangle& other) const {
    return interiorsIntersect(other.asConvex());
}

template <class PointType, class LabelType>
template<ConvexConcept OtherConvex>
constexpr bool Convex<PointType, LabelType>::interiorsIntersect(const OtherConvex& other) const {
    if (isDegenerate() || other.isDegenerate() || !bbox().interiorsIntersect(other.bbox())) {
        return false;
    }
    if (bbox().crosses(other.bbox())) {
        return true;
    }
    if (size() > other.size()) {
        return other.interiorsIntersect(*this);
    }

    // If this contains 3 vertices of other, the interiors intersect,
    // even if the vertices are on the boundary.
    if (contains(other[0]) && contains(other[1]) && contains(other[2])) {
        return true;
    }

    // Here we know that other is not inside this, so if the interiors intersect,
    // then there is an edge of this that intersects the interior of other.


    for (const auto& edge : edgesView()) {
        if (other.interiorsIntersect(edge)) {
            return true;
        }
    }

    return false;
}

template <class PointType, class LabelType>
template<DiskConcept OtherDisk>
constexpr bool Convex<PointType, LabelType>::interiorsIntersect(const OtherDisk& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        // Either operand collapsed to a lower dimension has empty interior.
        return false;
    }
    // Interiors meet when a convex edge passes through the open disk, or the
    // disk lies inside the convex (a point strictly inside the disk is in the
    // convex's interior). The latter uses a disk-interior point as the witness:
    // a disk tangent to an edge from inside still overlaps the interior.
    for (const auto& edge : edgesView()) {
        if (edge.interiorsIntersect(other)) {
            return true;
        }
    }
    return other.pointInsideInteriorContainedIn(*this);
}

template <class PointType, class LabelType>
template <PointConcept OtherPoint>
constexpr bool Convex<PointType, LabelType>::interiorsIntersect(const Shape<OtherPoint>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->interiorsIntersect(value);
        },
        other.variant());
}


// ---------------------------------------------------------------------------
// Polygon

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Polygon<PointType, LabelType>::interiorsIntersect(const OtherPoint& other) const {
    // A point's interior is the point itself, so this matches interiorContains.
    return interiorContains(other);
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Polygon<PointType, LabelType>::interiorsIntersect(const OtherLine& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }
    // The line meets the open interior iff vertices lie strictly on both sides:
    // the boundary is connected, so a straddle forces a transversal edge
    // crossing, which puts an interior point on the line. A polygon entirely on
    // one closed side only touches the line on its boundary.
    bool positive = false, negative = false;
    for (const auto& vertex : vertices()) {
        const auto side = orientationSign(other.min(), other.max(), vertex);
        positive = positive || side > 0;
        negative = negative || side < 0;
        if (positive && negative) {
            return true;
        }
    }
    return false;
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Polygon<PointType, LabelType>::interiorsIntersect(const OtherOrientedLine& other) const {
    return interiorsIntersect(static_cast<Line<typename OtherOrientedLine::PointType>>(other));
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Polygon<PointType, LabelType>::interiorsIntersect(const OtherSegment& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }
    // A transversal crossing of any boundary edge places an interior point on
    // the open segment.
    for (const auto& edge : edgesView()) {
        if (edge.crosses(other)) {
            return true;
        }
    }
    // No transversal crossings: the segment touches the boundary only at
    // vertices or along collinear overlaps, so every boundary contact is a
    // polygon vertex or a segment endpoint. Split the segment at those contacts
    // and classify each piece by its midpoint. Doubling the polygon keeps the
    // midpoint test exact and division-free: (p+q)/2 is interior iff (p+q) is
    // interior to 2*polygon.
    using C = std::common_type_t<NumberType, typename OtherSegment::NumberType>;
    using V = Point<C>;
    std::vector<V> contacts{static_cast<V>(other.min()), static_cast<V>(other.max())};
    for (const auto& vertex : vertices()) {
        if (other.contains(vertex)) {
            contacts.push_back(static_cast<V>(vertex));
        }
    }
    const auto base = other.min();
    const auto dir = other.max() - other.min();
    const auto along = [&](const V& p) {
        return (p.x() - base.x()) * dir.x() + (p.y() - base.y()) * dir.y();
    };
    std::sort(contacts.begin(), contacts.end(),
              [&](const V& p, const V& q) { return along(p) < along(q); });
    const auto doubled = (*this) * NumberType(2);
    for (std::size_t i = 1; i < contacts.size(); ++i) {
        if (contacts[i - 1] == contacts[i]) {
            continue;
        }
        if (doubled.interiorContains(contacts[i - 1] + contacts[i])) {
            return true;
        }
    }
    return false;
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Polygon<PointType, LabelType>::interiorsIntersect(const OtherOrientedSegment& other) const {
    return interiorsIntersect(static_cast<Segment<typename OtherOrientedSegment::PointType>>(other));
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool Polygon<PointType, LabelType>::interiorsIntersect(const OtherRay& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }
    for (const auto& edge : edgesView()) {
        if (edge.crosses(other)) {
            return true;
        }
    }
    // As with the segment, the remaining boundary contacts are the ray's source
    // and the polygon vertices on the ray; split there and test midpoints. The
    // ray exits the bounded polygon for good past its last contact.
    using C = std::common_type_t<NumberType, typename OtherRay::NumberType>;
    using V = Point<C>;
    std::vector<V> contacts{static_cast<V>(other.source())};
    for (const auto& vertex : vertices()) {
        if (other.contains(vertex)) {
            contacts.push_back(static_cast<V>(vertex));
        }
    }
    const auto base = other.source();
    const auto dir = other.target() - other.source();
    const auto along = [&](const V& p) {
        return (p.x() - base.x()) * dir.x() + (p.y() - base.y()) * dir.y();
    };
    std::sort(contacts.begin(), contacts.end(),
              [&](const V& p, const V& q) { return along(p) < along(q); });
    const auto doubled = (*this) * NumberType(2);
    for (std::size_t i = 1; i < contacts.size(); ++i) {
        if (contacts[i - 1] == contacts[i]) {
            continue;
        }
        if (doubled.interiorContains(contacts[i - 1] + contacts[i])) {
            return true;
        }
    }
    return false;
}

template <class PointType, class LabelType>
template<HalfplaneConcept OtherHalfplane>
constexpr bool Polygon<PointType, LabelType>::interiorsIntersect(const OtherHalfplane& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }
    // A vertex strictly inside the open half-plane has interior points of the
    // polygon near it that are also in the half-plane; if no vertex is strictly
    // inside, the whole polygon lies in the closed complement.
    for (const auto& vertex : vertices()) {
        if (other.interiorContains(vertex)) {
            return true;
        }
    }
    return false;
}

namespace detail {

// Interiors of a filled simple polygon and another filled convex region (a
// rectangle, triangle, or convex polygon).
//
// An edge of the area whose relative interior reaches into the polygon's interior
// settles it: a vertex of the area strictly inside, a properly crossing pair of
// edges, and an area sitting snugly in a corner of the polygon (every vertex on
// the boundary, no edge crossing) all put one there. If no edge of the area does,
// then the area's whole boundary misses the polygon's interior, which — the
// interior being connected — leaves it wholly inside the area, coincident regions
// included; an interior witness point of either then decides.
template <class Poly, class Area>
constexpr bool polygonAreaInteriorsIntersect(const Poly& poly, const Area& area) {
    if (area.isDegenerate() || poly.isDegenerate()) {
        return false;
    }
    auto abbox = area.bbox();
    if (!poly.bbox().interiorsIntersect(abbox)) {
        return false;
    }
    if (poly.bbox().separates(abbox) || abbox.separates(poly.bbox())) {
        return true;
    }

    for (const auto& edge : area.edges()) {
        if (edge.interiorsIntersect(poly)) {
            return true;
        }
    }

    // Nested (or coincident) with the boundaries in contact: one region's own
    // interior witness point decides, exactly and without scanning a boundary.
    return area.pointInsideInteriorContainedIn(poly) || poly.pointInsideInteriorContainedIn(area);
}

}  // namespace detail

template <class PointType, class LabelType>
template<RectangleConcept OtherRectangle>
constexpr bool Polygon<PointType, LabelType>::interiorsIntersect(const OtherRectangle& other) const {
    return detail::polygonAreaInteriorsIntersect(*this, other);
}

template <class PointType, class LabelType>
template<TriangleConcept OtherTriangle>
constexpr bool Polygon<PointType, LabelType>::interiorsIntersect(const OtherTriangle& other) const {
    return detail::polygonAreaInteriorsIntersect(*this, other);
}

template <class PointType, class LabelType>
template<ConvexConcept OtherConvex>
constexpr bool Polygon<PointType, LabelType>::interiorsIntersect(const OtherConvex& other) const {
    return detail::polygonAreaInteriorsIntersect(*this, other);
}

template <class PointType, class LabelType>
template<PolygonConcept OtherPolygon>
constexpr bool Polygon<PointType, LabelType>::boundariesIntersect(const OtherPolygon& other) const {
    // A polygon collapsed to a single point is all boundary, and its boundary
    // has no lexicographic break for BoundaryChains to split on, so reduce it
    // to that point first.
    if (const auto vertex = getIfPoint()) {
        return other.boundaryContains(*vertex);
    }
    if (const auto vertex = other.getIfPoint()) {
        return boundaryContains(*vertex);
    }
    // Produce both boundary decompositions in lockstep, testing each new chain
    // against every already-produced chain of the other polygon before building
    // the next, and stop at the first shared point. See BoundaryChains.
    BoundaryChains<Polygon> mine(*this);
    BoundaryChains<OtherPolygon> theirs(other);
    while (!mine.exhausted() || !theirs.exhausted()) {
        if (!mine.exhausted()) {
            const auto& chain = mine.produceNext();
            for (const auto& their : theirs.produced()) {
                if (chain.intersects(their)) {
                    return true;
                }
            }
        }
        if (!theirs.exhausted()) {
            const auto& chain = theirs.produceNext();
            for (const auto& my : mine.produced()) {
                if (chain.intersects(my)) {
                    return true;
                }
            }
        }
    }
    return false;
}

template <class PointType, class LabelType>
template<PolygonConcept OtherPolygon>
constexpr bool Polygon<PointType, LabelType>::boundariesStrongCross(const OtherPolygon& other) const {
    // A polygon collapsed to a single point has no edges to cross with, and
    // its boundary offers BoundaryChains no lexicographic break to split on.
    if (isPoint() || other.isPoint()) {
        return false;
    }
    // Produce both boundary decompositions in lockstep, testing each new chain
    // against every already-produced chain of the other polygon before building
    // the next, and stop at the first shared point. See BoundaryChains.
    BoundaryChains<Polygon> mine(*this);
    BoundaryChains<OtherPolygon> theirs(other);
    while (!mine.exhausted() || !theirs.exhausted()) {
        if (!mine.exhausted()) {
            const auto& chain = mine.produceNext();
            for (const auto& their : theirs.produced()) {
                if (chain.edgesCross(their)) {
                    return true;
                }
            }
        }
        if (!theirs.exhausted()) {
            const auto& chain = theirs.produceNext();
            for (const auto& my : mine.produced()) {
                if (chain.edgesCross(my)) {
                    return true;
                }
            }
        }
    }
    return false;
}

template <class PointType, class LabelType>
template<PolygonConcept OtherPolygon>
constexpr bool Polygon<PointType, LabelType>::interiorsIntersect(const OtherPolygon& other) const {
    // Cheap bounding-box reject: if the closed boxes are disjoint the interiors
    // cannot meet, so the machinery below is skipped for distant pairs.
    if (size() == 0 || other.size() == 0 || !bbox().intersects(other.bbox())) {
        return false;
    }
    if (bbox().crosses(other.bbox())) {
        return true;
    }
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }
    if (*this == other) {
        return true;
    }

    // When the boundaries are disjoint the polygons are either separate or one
    // is nested in the other, so the interiors meet iff one polygon contains the
    // other — which, with no boundary contact, reduces to a single interior
    // point-in-polygon test each way (every vertex of the inner polygon lies in
    // the outer interior). This short-circuits the quadratic scan below, which
    // is then only reached when the boundaries actually touch.
    // Similarly, if the boundaries strongly cross, then the interiors intersect

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
                        return true;
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
                        return true;
                    }
                }
            }
        }
    }

    if (!boundaries_intersect) {
        return interiorContains(other.get(0)) || other.interiorContains(get(0));
    }

    // Boundaries touch but do not have crossing chains:
    // distinguish a mere boundary contact from a real interior
    // overlap that went through a vertex between monotone chains.

    for (const auto& vertex : other.vertices()) {
        if (interiorContains(vertex)) {
            return true;
        }
    }
    for (const auto& vertex : vertices()) {
        if (other.interiorContains(vertex)) {
            return true;
        }
    }
    for (const auto& edge : edgesView()) {
        if (edge.separates(other)) {
            return true;
        }
    }
    for (const auto& edge : other.edgesView()) {
        if (edge.separates(*this)) {
            return true;
        }
    }
    return false;
}


template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Polygon<PointType, LabelType>::interiorsIntersect(const Shape<OtherPoint>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->interiorsIntersect(value);
        },
        other.variant());
}

template <class Number, class Label>
constexpr bool Point<Number, Label>::interiorsIntersect(const Shape<Point<Number, Label>>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->interiorsIntersect(value);
        },
        other.variant());
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Disk<PointType, LabelType>::interiorsIntersect(const OtherPoint& other) const {
    // A point's interior is the point itself, so this matches interiorContains.
    return interiorContains(other);
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Disk<PointType, LabelType>::interiorsIntersect(const OtherSegment& other) const {
    // A degenerate disk has empty interior and a degenerate segment has empty
    // relative interior, so neither can contribute an interior intersection.
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }

    // An endpoint strictly inside the open disk drags the adjacent open segment
    // inside with it, so the relative interiors already meet.
    if (interiorContains(other.min()) || interiorContains(other.max())) {
        return true;
    }

    // Neither endpoint is strictly inside, so the interiors meet exactly when
    // the segment pierces the open disk. This is the same exact, division-free
    // in-circle formulation as intersects(), only with strict inequalities so a
    // mere boundary tangency does not count: writing power(p) = |p-center|^2-r^2
    // and inCircleDeterminant(a,b,c,p) = -A*power(p) with A = 2*signedArea, the
    // value h(t) = A*inCircleDeterminant along the segment is a concave quadratic
    // (leading coefficient -L*A^2 < 0) that is positive exactly where the point
    // is strictly inside the disk, so the open disk is pierced iff h has an
    // interior maximum above zero.
    //
    //   A  = orientation determinant of the three boundary points
    //   J0 = inCircleDeterminant(a,b,c, min), J1 = ... (a,b,c, max)
    //   L  = |max - min|^2,   M = L * A
    // Foot on the segment (t* in [0,1]): |(J0 - J1)*A| <= M*A   (M*A >= 0).
    // Line cuts the circle in two distinct points: (J0 + J1 + M)^2 > 4*J0*J1.
    using W = detail::promoted_number_t<
        decltype(inCircleDeterminant(a(), b(), c(), other.min()))>;

    const W det = static_cast<W>(orientationDeterminant(a(), b(), c()));
    const W j0 = static_cast<W>(inCircleDeterminant(a(), b(), c(), other.min()));
    const W j1 = static_cast<W>(inCircleDeterminant(a(), b(), c(), other.max()));
    const W squared_length = static_cast<W>(other.min().squaredDistance(other.max()));
    const W m = squared_length * det;

    const W projection = (j0 - j1) * det;          // (J0 - J1) * A
    const W half_span = m * det;                   // M * A = L * A^2 >= 0
    const W discriminant_base = j0 + j1 + m;       // J0 + J1 + M

    const bool foot_on_segment = projection >= -half_span && projection <= half_span;
    const bool pierces_disk = discriminant_base * discriminant_base > W{4} * j0 * j1;

    return foot_on_segment && pierces_disk;
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Disk<PointType, LabelType>::interiorsIntersect(const OtherOrientedSegment& other) const {
    return interiorsIntersect(other.asSegment());
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Disk<PointType, LabelType>::interiorsIntersect(const OtherLine& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }
    // A line meets the open disk exactly when it is a strict secant: it cuts the
    // boundary circle in two distinct points (discriminant > 0). A tangent line
    // touches only the boundary and does not count. Same division-free in-circle
    // formulation as interiorsIntersect(Segment), without a parameter range.
    //
    //   A  = orientation determinant of the three boundary points
    //   J0 = inCircleDeterminant(a,b,c, other[0]), J1 = ... (a,b,c, other[1])
    //   L  = |other[1] - other[0]|^2,   M = L * A
    // Line is a strict secant: (J0 + J1 + M)^2 > 4*J0*J1.
    using W = detail::promoted_number_t<
        decltype(inCircleDeterminant(a(), b(), c(), other[0]))>;

    const W det = static_cast<W>(orientationDeterminant(a(), b(), c()));
    const W j0 = static_cast<W>(inCircleDeterminant(a(), b(), c(), other[0]));
    const W j1 = static_cast<W>(inCircleDeterminant(a(), b(), c(), other[1]));
    const W squared_length = static_cast<W>(other[0].squaredDistance(other[1]));
    const W discriminant_base = j0 + j1 + squared_length * det;  // J0 + J1 + M

    return discriminant_base * discriminant_base > W{4} * j0 * j1;
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Disk<PointType, LabelType>::interiorsIntersect(const OtherOrientedLine& other) const {
    return interiorsIntersect(other.asLine());
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool Disk<PointType, LabelType>::interiorsIntersect(const OtherRay& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }
    // A source strictly inside the open disk drags the adjacent ray inside.
    if (interiorContains(other.source())) {
        return true;
    }
    // Otherwise the interiors meet exactly when the supporting line is a strict
    // secant (discriminant > 0) AND the pierced span lies ahead of the source
    // (the perpendicular foot has positive parameter). Same division-free
    // in-circle formulation as interiorsIntersect(Segment); the source is
    // parameter 0 and the target parameter 1.
    //
    //   A  = orientation determinant of the three boundary points
    //   J0 = inCircleDeterminant(a,b,c, source), J1 = ... (a,b,c, target)
    //   L  = |target - source|^2,   M = L * A
    // Strict secant: (J0 + J1 + M)^2 > 4*J0*J1.
    // Pierced span ahead of the source (foot t* > 0): (J0 - J1)*A < M*A.
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

    const bool strict_secant = discriminant_base * discriminant_base > W{4} * j0 * j1;
    const bool contact_ahead = projection < half_span;  // foot parameter t* > 0

    return strict_secant && contact_ahead;
}

template <class PointType, class LabelType>
template<HalfplaneConcept OtherHalfplane>
constexpr bool Disk<PointType, LabelType>::interiorsIntersect(const OtherHalfplane& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        // A radius-zero disk is a point and has empty interior.
        return false;
    }
    return interiorsIntersect(other.asLine()) || pointInsideInteriorContainedIn(other);
}

template <class PointType, class LabelType>
template<RectangleConcept OtherRectangle>
constexpr bool Disk<PointType, LabelType>::interiorsIntersect(const OtherRectangle& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }
    // Interiors meet when a rectangle edge passes through the open disk, or the
    // disk lies inside the rectangle (a point strictly inside the disk is in the
    // rectangle's interior). The latter uses a disk-interior point as the
    // witness: a disk tangent to an edge from inside still overlaps the interior.
    for (const auto& edge : other.edges()) {
        if (interiorsIntersect(edge)) {
            return true;
        }
    }
    return pointInsideInteriorContainedIn(other);
}

template <class PointType, class LabelType>
template<TriangleConcept OtherTriangle>
constexpr bool Disk<PointType, LabelType>::interiorsIntersect(const OtherTriangle& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }
    // Interiors meet when a triangle edge passes through the open disk, or the
    // disk lies inside the triangle (a point strictly inside the disk is in the
    // triangle's interior). The latter uses a disk-interior point as the
    // witness: a disk tangent to an edge from inside still overlaps the interior.
    for (const auto& edge : other.edges()) {
        if (interiorsIntersect(edge)) {
            return true;
        }
    }
    return pointInsideInteriorContainedIn(other);
}

template <class PointType, class LabelType>
template<DiskConcept OtherDisk>
constexpr bool Disk<PointType, LabelType>::interiorsIntersect(const OtherDisk& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        // A radius-zero disk is a point and has empty interior.
        return false;
    }
    // Open disks overlap iff the centre distance is strictly less than the sum
    // of the radii, so externally tangent disks do not count. With
    // A = d^2 - r1^2 - r2^2 that is A < 0 or A^2 < 4 r1^2 r2^2 (the strict form
    // of the closed-disk test in intersects).
    using R = std::conditional_t<
        std::is_floating_point_v<NumberType> ||
            std::is_floating_point_v<typename OtherDisk::NumberType>,
        long double,
        Rational<BigInt>>;
    const R d2 = center<R>().squaredDistance(other.template center<R>());
    const R r1_sq = squaredRadius<R>();
    const R r2_sq = other.template squaredRadius<R>();
    const R A = d2 - r1_sq - r2_sq;
    return A < R{} || A * A < R{4} * r1_sq * r2_sq;
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Disk<PointType, LabelType>::interiorsIntersect(const Shape<OtherPoint>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->interiorsIntersect(value);
        },
        other.variant());
}


template <class PointType, class LabelType>
template<DiskConcept OtherDisk>
constexpr bool Polygon<PointType, LabelType>::interiorsIntersect(const OtherDisk& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        // Either operand collapsed to a lower dimension has empty interior.
        return false;
    }
    // The open interiors meet when a boundary edge passes through the open disk
    // (this also covers the polygon lying inside the disk, whose edges are then
    // inside it), or when the disk lies inside the polygon -- witnessed by a point
    // strictly inside the disk falling in the polygon's strict interior. The
    // interior witness matters: a disk tangent to an edge from inside still
    // overlaps the interior.
    for (const auto& edge : edgesView()) {
        if (edge.interiorsIntersect(other)) {
            return true;
        }
    }
    return other.pointInsideInteriorContainedIn(*this);
}

/**
 * @section predicates-monotonechain MonotoneChain
 * Weakly x-monotone chain predicates. The chain's relative interior is the
 * chain minus its two extreme vertices, so non-extreme vertices count as
 * interior points alongside the open edges.
 */

template <class PointType, class LabelType, class Storage>
template<PointConcept OtherPoint>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::interiorsIntersect(const OtherPoint& other) const {
    // A point's interior is the point itself, so this matches interiorContains.
    return interiorContains(other);
}

template <class PointType, class LabelType, class Storage>
template<SegmentConcept OtherSegment>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::interiorsIntersect(const OtherSegment& other) const {
    if (points_.size() < 2) {
        // A chain without an edge has an empty relative interior.
        return false;
    }
    const auto window = edgeWindow(other.min().x(), other.max().x());
    if (!window) {
        return false;
    }
    for (std::size_t i = window->first; i <= window->second; ++i) {
        if (this->template boundaryAt<false>(i).interiorsIntersect(other)) {
            return true;
        }
    }
    // The open edges miss the chain's own vertices, but the non-extreme ones
    // are interior points of the chain: a segment whose open part passes
    // exactly through such a vertex still meets the chain's interior.
    const std::size_t firstVertex = std::max<std::size_t>(window->first, 1);
    const std::size_t lastVertex = std::min(window->second + 1, points_.size() - 2);
    for (std::size_t v = firstVertex; v <= lastVertex; ++v) {
        if (other.interiorContains((*this)[v])) {
            return true;
        }
    }
    return false;
}

template <class PointType, class LabelType, class Storage>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::interiorsIntersect(const OtherOrientedSegment& other) const {
    return interiorsIntersect(static_cast<Segment<typename OtherOrientedSegment::PointType>>(other));
}

namespace detail {

// Shared body of MonotoneChain::interiorsIntersect against a shape whose
// interior test is `other.interiorContains(point)`: the chain's relative
// interior is its open edges plus its non-extreme vertices, so it meets the
// other interior iff an open edge does or such a vertex lies inside it.
template <class Chain, class OtherShape>
constexpr bool chainInteriorsIntersect(const Chain& chain, const OtherShape& other) {
    const std::size_t n = chain.size();
    if (n < 2) {
        // A chain without an edge has an empty relative interior.
        return false;
    }
    for (std::size_t i = 0; i + 1 < n; ++i) {
        if (Segment<typename Chain::PointType>(chain[i], chain[i + 1]).interiorsIntersect(other)) {
            return true;
        }
    }
    for (std::size_t v = 1; v + 1 < n; ++v) {
        if (other.interiorContains(chain[v])) {
            return true;
        }
    }
    return false;
}

}  // namespace detail

template <class PointType, class LabelType, class Storage>
template<LineConcept OtherLine>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::interiorsIntersect(const OtherLine& other) const {
    return detail::chainInteriorsIntersect(*this, other);
}

template <class PointType, class LabelType, class Storage>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::interiorsIntersect(const OtherOrientedLine& other) const {
    return detail::chainInteriorsIntersect(*this, other);
}

template <class PointType, class LabelType, class Storage>
template<RayConcept OtherRay>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::interiorsIntersect(const OtherRay& other) const {
    return detail::chainInteriorsIntersect(*this, other);
}

template <class PointType, class LabelType, class Storage>
template<HalfplaneConcept OtherHalfplane>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::interiorsIntersect(const OtherHalfplane& other) const {
    return detail::chainInteriorsIntersect(*this, other);
}

template <class PointType, class LabelType, class Storage>
template<RectangleConcept OtherRectangle>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::interiorsIntersect(const OtherRectangle& other) const {
    return detail::chainInteriorsIntersect(*this, other);
}

template <class PointType, class LabelType, class Storage>
template<TriangleConcept OtherTriangle>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::interiorsIntersect(const OtherTriangle& other) const {
    return detail::chainInteriorsIntersect(*this, other);
}

template <class PointType, class LabelType, class Storage>
template<ConvexConcept OtherConvex>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::interiorsIntersect(const OtherConvex& other) const {
    return detail::chainInteriorsIntersect(*this, other);
}

template <class PointType, class LabelType, class Storage>
template<DiskConcept OtherDisk>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::interiorsIntersect(const OtherDisk& other) const {
    return detail::chainInteriorsIntersect(*this, other);
}

template <class PointType, class LabelType, class Storage>
template<MonotoneChainConcept OtherChain>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::interiorsIntersect(const OtherChain& other) const {
    if (size() < 2 || other.size() < 2) {
        return false;
    }
    // Open-edge pairs via the same merge sweep as intersects().
    const std::size_t iEnd = size() - 1;
    const std::size_t jEnd = other.size() - 1;
    // Seed past the leading edges left of the shared x-window (see the sweep in
    // MonotoneChain::intersects): indexAtX locates the first candidate in
    // O(log n); a disengaged result means the x-ranges are disjoint, so the
    // merge has no work (the vertex loops below still run and correctly find
    // nothing, as no vertex of one chain can lie on the disjoint other).
    using XType = std::common_type_t<NumberType, typename OtherChain::PointType::NumberType>;
    const XType xlo = std::max<XType>((*this)[0].x(), other[0].x());
    const auto iSeed = indexAtX(xlo);
    const auto jSeed = other.indexAtX(xlo);
    // Back up one edge: the edge whose right endpoint sits exactly on xlo can
    // still meet the other chain there, yet indexAtX returns the next edge.
    std::size_t i = (iSeed && jSeed) ? (*iSeed > 0 ? *iSeed - 1 : 0) : iEnd;
    std::size_t j = (iSeed && jSeed) ? (*jSeed > 0 ? *jSeed - 1 : 0) : jEnd;
    while (i < iEnd && j < jEnd) {
        const Segment<PointType> mine((*this)[i], (*this)[i + 1]);
        const Segment<typename OtherChain::PointType> theirs(other[j], other[j + 1]);
        if (!(mine.max().x() < theirs.min().x() || theirs.max().x() < mine.min().x()) &&
            mine.interiorsIntersect(theirs)) {
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
    // The open edges miss the chains' own vertices, but the non-extreme ones
    // are interior, whichever chain they belong to.
    for (std::size_t v = 1; v + 1 < size(); ++v) {
        if (other.interiorContains((*this)[v])) {
            return true;
        }
    }
    for (std::size_t v = 1; v + 1 < other.size(); ++v) {
        if (interiorContains(other[v])) {
            return true;
        }
    }
    return false;
}

template <class PointType, class LabelType, class Storage>
template<PointConcept OtherPoint>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::interiorsIntersect(const Shape<OtherPoint>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->interiorsIntersect(value);
        },
        other.variant());
}

template <class PointType, class LabelType>
template<MonotoneChainConcept OtherChain>
constexpr bool Polygon<PointType, LabelType>::interiorsIntersect(const OtherChain& other) const {
    return detail::chainInteriorsIntersect(other, *this);
}

/**
 * @section predicates-polyline Polyline
 * Open polygonal chain predicates. The polyline's relative interior is the
 * polyline minus its two extreme *points* (a self-intersecting polyline may
 * pass through an extreme again mid-chain, and that point is still excluded),
 * so the tests work with closed edges and explicitly discard meeting points
 * that coincide with an excluded extreme or endpoint.
 */

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Polyline<PointType, LabelType>::interiorsIntersect(const OtherPoint& other) const {
    // A point's interior is the point itself, so this matches interiorContains.
    return interiorContains(other);
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Polyline<PointType, LabelType>::interiorsIntersect(const OtherSegment& other) const {
    if (size() < 2) {
        // A polyline covering at most one point has an empty relative interior.
        return false;
    }
    // The wanted set is (A ∖ {front, back}) ∩ (S ∖ {S.min, S.max}): the closed
    // intersection minus at most four excluded points. A positive-length
    // collinear overlap survives the removal of finitely many points; a single
    // meeting point survives iff it is not one of the four. Working with
    // closed edges also covers a segment passing exactly through a non-extreme
    // vertex without crossing any open edge.
    const PointType front = (*this)[0];
    const PointType back = (*this)[size() - 1];
    for (const auto& edge : edgesView()) {
        if (!edge.intersects(other)) {
            continue;
        }
        if (edge.collinear(other) && edge.min() < other.max() && other.min() < edge.max()) {
            return true;  // positive-length overlap
        }
        // The intersection is a single point; it counts unless it is one of
        // the excluded extremes/endpoints (tested division-free: the unique
        // common point equals x iff x lies on both segments).
        const bool excluded = (edge.contains(front) && other.contains(front)) ||
                              (edge.contains(back) && other.contains(back)) ||
                              edge.contains(other.min()) || edge.contains(other.max());
        if (!excluded) {
            return true;
        }
    }
    return false;
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Polyline<PointType, LabelType>::interiorsIntersect(const OtherOrientedSegment& other) const {
    return interiorsIntersect(static_cast<Segment<typename OtherOrientedSegment::PointType>>(other));
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Polyline<PointType, LabelType>::interiorsIntersect(const OtherLine& other) const {
    if (size() < 2) {
        return false;
    }
    // The wanted set is (A ∖ {front, back}) ∩ line: the closed intersection
    // minus at most two excluded points. Since the line is infinite, an edge
    // meets it either along the whole edge (edge on the line) or in a single
    // point.
    const PointType front = (*this)[0];
    const PointType back = (*this)[size() - 1];
    for (const auto& edge : edgesView()) {
        if (!edge.intersects(other)) {
            continue;
        }
        if (other.contains(edge.min()) && other.contains(edge.max()) &&
            edge.min() != edge.max()) {
            return true;  // positive-length overlap survives losing two points
        }
        // Single meeting point; it counts unless it is an excluded extreme
        // (tested division-free: the unique common point equals x iff x lies
        // on both operands).
        const bool excluded = (edge.contains(front) && other.contains(front)) ||
                              (edge.contains(back) && other.contains(back));
        if (!excluded) {
            return true;
        }
    }
    return false;
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Polyline<PointType, LabelType>::interiorsIntersect(const OtherOrientedLine& other) const {
    return interiorsIntersect(other.asLine());
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool Polyline<PointType, LabelType>::interiorsIntersect(const OtherRay& other) const {
    if (size() < 2) {
        return false;
    }
    // The ray's relative interior is the ray minus its source, so the excluded
    // points are the polyline's extremes and the source.
    const PointType front = (*this)[0];
    const PointType back = (*this)[size() - 1];
    const auto supporting = other.asLine();
    for (const auto& edge : edgesView()) {
        if (!edge.intersects(other)) {
            continue;
        }
        if (supporting.contains(edge.min()) && supporting.contains(edge.max())) {
            // Collinear contact: the overlap with the ray is an interval whose
            // far end is an edge endpoint on the ray.
            const bool minOnRay = other.contains(edge.min());
            const bool maxOnRay = other.contains(edge.max());
            if (minOnRay && maxOnRay && edge.min() != edge.max()) {
                return true;  // positive-length overlap
            }
            if (minOnRay != maxOnRay) {
                // Overlap [source, endpoint-on-ray]; positive length iff that
                // endpoint is not the source itself.
                const PointType onRay = minOnRay ? edge.min() : edge.max();
                if (onRay != other.source()) {
                    return true;
                }
                continue;  // the single meeting point is the excluded source
            }
            // Zero-length edge on the ray: fall through to the single-point test.
        }
        const bool excluded = (edge.contains(front) && other.contains(front)) ||
                              (edge.contains(back) && other.contains(back)) ||
                              edge.contains(other.source());
        if (!excluded) {
            return true;
        }
    }
    return false;
}

// The shapes below have open two-dimensional interiors, so whenever such an
// interior meets the polyline at all it also meets a polyline point other
// than the two excluded extremes (an open neighborhood of the meeting point
// contains further polyline points); the chain helper is therefore exact here
// even though the polyline may revisit an extreme mid-sequence.

template <class PointType, class LabelType>
template<HalfplaneConcept OtherHalfplane>
constexpr bool Polyline<PointType, LabelType>::interiorsIntersect(const OtherHalfplane& other) const {
    return detail::chainInteriorsIntersect(*this, other);
}

template <class PointType, class LabelType>
template<RectangleConcept OtherRectangle>
constexpr bool Polyline<PointType, LabelType>::interiorsIntersect(const OtherRectangle& other) const {
    return detail::chainInteriorsIntersect(*this, other);
}

template <class PointType, class LabelType>
template<TriangleConcept OtherTriangle>
constexpr bool Polyline<PointType, LabelType>::interiorsIntersect(const OtherTriangle& other) const {
    return detail::chainInteriorsIntersect(*this, other);
}

template <class PointType, class LabelType>
template<ConvexConcept OtherConvex>
constexpr bool Polyline<PointType, LabelType>::interiorsIntersect(const OtherConvex& other) const {
    return detail::chainInteriorsIntersect(*this, other);
}

template <class PointType, class LabelType>
template<DiskConcept OtherDisk>
constexpr bool Polyline<PointType, LabelType>::interiorsIntersect(const OtherDisk& other) const {
    return detail::chainInteriorsIntersect(*this, other);
}

template <class PointType, class LabelType>
template<MonotoneChainConcept OtherChain>
constexpr bool Polyline<PointType, LabelType>::interiorsIntersect(const OtherChain& other) const {
    if (size() < 2 || other.size() < 2) {
        return false;
    }
    // Same closed-edge scheme as the polyline overload, with the four excluded
    // points being the extremes of the polyline and of the chain.
    const PointType front = (*this)[0];
    const PointType back = (*this)[size() - 1];
    const auto otherFront = other[0];
    const auto otherBack = other[other.size() - 1];
    for (const auto& mine : edgesView()) {
        for (const auto& theirs : other.edgesView()) {
            if (!mine.intersects(theirs)) {
                continue;
            }
            if (mine.collinear(theirs) && mine.min() < theirs.max() && theirs.min() < mine.max()) {
                return true;  // positive-length overlap
            }
            const bool excluded =
                (mine.contains(front) && theirs.contains(front)) ||
                (mine.contains(back) && theirs.contains(back)) ||
                (mine.contains(otherFront) && theirs.contains(otherFront)) ||
                (mine.contains(otherBack) && theirs.contains(otherBack));
            if (!excluded) {
                return true;
            }
        }
    }
    return false;
}

template <class PointType, class LabelType>
template<PolylineConcept OtherPolyline>
constexpr bool Polyline<PointType, LabelType>::interiorsIntersect(const OtherPolyline& other) const {
    if (size() < 2 || other.size() < 2) {
        return false;
    }
    // Same closed-edge scheme as the segment overload, with the four excluded
    // points being the extremes of the two polylines.
    const PointType front = (*this)[0];
    const PointType back = (*this)[size() - 1];
    const auto otherFront = other[0];
    const auto otherBack = other[other.size() - 1];
    for (const auto& mine : edgesView()) {
        for (const auto& theirs : other.edgesView()) {
            if (!mine.intersects(theirs)) {
                continue;
            }
            if (mine.collinear(theirs) && mine.min() < theirs.max() && theirs.min() < mine.max()) {
                return true;  // positive-length overlap
            }
            const bool excluded =
                (mine.contains(front) && theirs.contains(front)) ||
                (mine.contains(back) && theirs.contains(back)) ||
                (mine.contains(otherFront) && theirs.contains(otherFront)) ||
                (mine.contains(otherBack) && theirs.contains(otherBack));
            if (!excluded) {
                return true;
            }
        }
    }
    return false;
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Polyline<PointType, LabelType>::interiorsIntersect(const Shape<OtherPoint>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->interiorsIntersect(value);
        },
        other.variant());
}

// The polygon's interior is open and two-dimensional, so the chain helper is
// exact for a polyline too (see the polyline section note above).
template <class PointType, class LabelType>
template<PolylineConcept OtherPolyline>
constexpr bool Polygon<PointType, LabelType>::interiorsIntersect(const OtherPolyline& other) const {
    return detail::chainInteriorsIntersect(other, *this);
}


// ---------------------------------------------------------------------------
// HalfplaneIntersection
//
// The other shape's interior is its relative interior (a point is itself, a
// segment is the open segment, ...), matching the conventions used by the
// other shapes. The region's interior is its topological interior, which is
// empty when the region is degenerate.

template <class PointType, class LabelType>
template <PointConcept OtherPoint>
constexpr bool HalfplaneIntersection<PointType, LabelType>::interiorsIntersect(const OtherPoint& other) const {
    // A point's interior is the point itself, so this matches interiorContains.
    return interiorContains(other);
}

template <class PointType, class LabelType>
template <SegmentConcept OtherSegment>
constexpr bool HalfplaneIntersection<PointType, LabelType>::interiorsIntersect(const OtherSegment& other) const {
    if (isDegenerate()) {
        return false;
    }
    if (halfplanes_.empty()) {
        return true;
    }
    if (other.isDegenerate()) {
        return interiorContains(other.min());
    }
    // The open clip interval must overlap the open parameter window (0, 1).
    const Halfplane<typename OtherSegment::PointType> along(other.min(), other.max());
    const auto clip = clipLine(along);
    if (clip.empty || clip.onParallelBoundary || !clipHasLength(clip, along)) {
        return false;
    }
    if (clip.entry >= 0 && !(constraintSide(static_cast<std::size_t>(clip.entry), other.max()) > 0)) {
        return false;
    }
    if (clip.exit >= 0 && !(constraintSide(static_cast<std::size_t>(clip.exit), other.min()) > 0)) {
        return false;
    }
    return true;
}

template <class PointType, class LabelType>
template <OrientedSegmentConcept OtherOrientedSegment>
constexpr bool HalfplaneIntersection<PointType, LabelType>::interiorsIntersect(const OtherOrientedSegment& other) const {
    return interiorsIntersect(Segment<typename OtherOrientedSegment::PointType>(other[0], other[1]));
}

template <class PointType, class LabelType>
template <LineConcept OtherLine>
constexpr bool HalfplaneIntersection<PointType, LabelType>::interiorsIntersect(const OtherLine& other) const {
    if (isDegenerate()) {
        return false;
    }
    if (halfplanes_.empty()) {
        return true;
    }
    const Halfplane<typename OtherLine::PointType> along(other[0], other[1]);
    const auto clip = clipLine(along);
    return !clip.empty && !clip.onParallelBoundary && clipHasLength(clip, along);
}

template <class PointType, class LabelType>
template <OrientedLineConcept OtherOrientedLine>
constexpr bool HalfplaneIntersection<PointType, LabelType>::interiorsIntersect(const OtherOrientedLine& other) const {
    return interiorsIntersect(other.asLine());
}

template <class PointType, class LabelType>
template <RayConcept OtherRay>
constexpr bool HalfplaneIntersection<PointType, LabelType>::interiorsIntersect(const OtherRay& other) const {
    if (isDegenerate()) {
        return false;
    }
    if (halfplanes_.empty()) {
        return true;
    }
    // The open clip interval must overlap the open window (0, +inf).
    const Halfplane<typename OtherRay::PointType> along(other.source(), other.target());
    const auto clip = clipLine(along);
    if (clip.empty || clip.onParallelBoundary || !clipHasLength(clip, along)) {
        return false;
    }
    return clip.exit < 0 || constraintSide(static_cast<std::size_t>(clip.exit), other.source()) > 0;
}

template <class PointType, class LabelType>
template <HalfplaneConcept OtherHalfplane>
constexpr bool HalfplaneIntersection<PointType, LabelType>::interiorsIntersect(const OtherHalfplane& other) const {
    if (isDegenerate()) {
        return false;
    }
    if (halfplanes_.empty()) {
        return true;
    }
    // The infimum of the half-plane's normal functional must lie strictly
    // below its boundary value; a full-dimensional region then has interior
    // points arbitrarily close to any witness.
    const SupStatus infimum = supStatus(other.opposite());
    return infimum == SupStatus::unbounded || infimum == SupStatus::above;
}

template <class PointType, class LabelType>
template <RectangleConcept OtherRectangle>
constexpr bool HalfplaneIntersection<PointType, LabelType>::interiorsIntersect(const OtherRectangle& other) const {
    // The open interiors meet exactly when the region intersected with the
    // rectangle is full-dimensional.
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }
    return !intersection(other).isDegenerate();
}

template <class PointType, class LabelType>
template <TriangleConcept OtherTriangle>
constexpr bool HalfplaneIntersection<PointType, LabelType>::interiorsIntersect(const OtherTriangle& other) const {
    if (isDegenerate()) {
        return false;
    }
    return !intersection(other).isDegenerate();
}

template <class PointType, class LabelType>
template <ConvexConcept OtherConvex>
constexpr bool HalfplaneIntersection<PointType, LabelType>::interiorsIntersect(const OtherConvex& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }
    return !intersection(other).isDegenerate();
}

template <class PointType, class LabelType>
template <DiskConcept OtherDisk>
constexpr bool HalfplaneIntersection<PointType, LabelType>::interiorsIntersect(const OtherDisk& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }
    if (halfplanes_.empty()) {
        return true;
    }
    using E = detail::region_exact_number_t<NumberType>;
    const auto clipped = detail::regionClippedToBox(*this, other.bbox());
    if (clipped.isDegenerate()) {
        return false;
    }
    return clipped.template asConvex<E>().interiorsIntersect(other);
}

template <class PointType, class LabelType>
template <MonotoneChainConcept OtherChain>
constexpr bool HalfplaneIntersection<PointType, LabelType>::interiorsIntersect(const OtherChain& other) const {
    // A chain is one-dimensional: its interior meets the region's interior
    // exactly when some edge's relative interior enters the open region.
    if (isDegenerate() || other.size() < 2) {
        return false;
    }
    for (const auto& edge : other.edgesView()) {
        if (interiorsIntersect(edge)) {
            return true;
        }
    }
    return false;
}

template <class PointType, class LabelType>
template <PolylineConcept OtherPolyline>
constexpr bool HalfplaneIntersection<PointType, LabelType>::interiorsIntersect(const OtherPolyline& other) const {
    if (isDegenerate() || other.size() < 2) {
        return false;
    }
    for (const auto& edge : other.edgesView()) {
        if (interiorsIntersect(edge)) {
            return true;
        }
    }
    return false;
}

template <class PointType, class LabelType>
template <PolygonConcept OtherPolygon>
constexpr bool HalfplaneIntersection<PointType, LabelType>::interiorsIntersect(const OtherPolygon& other) const {
    if (isDegenerate() || other.isDegenerate() || other.size() < 3) {
        return false;
    }
    if (halfplanes_.empty()) {
        return true;
    }
    // Clip to the polygon's box, then defer to the polygon's own interior-
    // intersection test against the resulting bounded convex region.
    using E = detail::region_exact_number_t<NumberType>;
    const auto clipped = detail::regionClippedToBox(*this, other.bbox());
    if (clipped.isDegenerate()) {
        return false;
    }
    return other.interiorsIntersect(clipped.template asConvex<E>());
}

template <class PointType, class LabelType>
template <HalfplaneIntersectionConcept OtherRegion>
constexpr bool HalfplaneIntersection<PointType, LabelType>::interiorsIntersect(const OtherRegion& other) const {
    // The interiors are intersections of open half-planes, and a finite family
    // of open half-planes has a common point exactly when the corresponding
    // closed intersection is full-dimensional.
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }
    return !intersection(other).isDegenerate();
}

template <class PointType, class LabelType>
template <PointConcept OtherPoint>
constexpr bool HalfplaneIntersection<PointType, LabelType>::interiorsIntersect(const Shape<OtherPoint>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->interiorsIntersect(value);
        },
        other.variant());
}


// ---------------------------------------------------------------------------
// PolygonWithHoles

template <class PointType, class LabelType>
template <class OtherLinear, class ContactNumber>
constexpr bool PolygonWithHoles<PointType, LabelType>::linearInteriorsIntersect(
    const OtherLinear& other,
    std::vector<Point<ContactNumber>> contacts,
    const Point<ContactNumber>& base,
    const Point<ContactNumber>& direction) const {
    // The split points: the operand's own ends, already in `contacts`, then
    // every ring vertex on the operand. Every other contact between the operand
    // and ∂A is a transversal crossing — a non-collinear pair meets in at most
    // one point, which is a crossing unless it is an endpoint of one of them,
    // and a collinear overlap begins and ends at such a point.
    using V = Point<ContactNumber>;
    const std::size_t firstRingVertex = contacts.size();
    for (const auto& vertex : outer_) {
        if (other.contains(vertex)) {
            contacts.push_back(static_cast<V>(vertex));
        }
    }
    for (const auto& hole : holes_) {
        for (const auto& vertex : hole) {
            if (other.contains(vertex)) {
                contacts.push_back(static_cast<V>(vertex));
            }
        }
    }
    // A transversal crossing of a ring edge normally settles it: the operand
    // passes from one open side of the edge to the other, and one of those sides
    // is region interior — inside the outer ring for an outer edge, outside the
    // hole for a hole edge.
    //
    // The exception is a crossing where the region pinches shut, i.e. where two
    // rings meet, because there both sides of the crossing are outside the
    // region interior. That happens in two ways, and neither needs the crossing
    // point itself:
    //
    //  - the rings touch at an isolated point, which has to be a vertex of one
    //    of them (meeting anywhere else would make their edges cross or
    //    overlap), so it is already among the split points collected above;
    //  - the rings run along one another, so the crossed edge is covered twice.
    //    A second crossed edge collinear with the first meets the operand at the
    //    same point — both crossing points are `line(edge) ∩ line(operand)` —
    //    so collinearity among the crossed edges detects exactly this.
    //
    // Skipping a pinched crossing is safe and needs no split point of its own:
    // the region is locally one-dimensional there, so the piece carrying it
    // stays outside the region interior on both sides and its midpoint says so.
    std::vector<EdgeType> crossed;
    anyBoundaryEdge([&](const auto& edge) {
        if (edge.crosses(other)) {
            crossed.push_back(edge);
        }
        return false;
    });
    for (std::size_t i = 0; i < crossed.size(); ++i) {
        bool pinched = false;
        for (std::size_t v = firstRingVertex; v < contacts.size() && !pinched; ++v) {
            pinched = crossed[i].contains(contacts[v]);
        }
        for (std::size_t j = 0; j < crossed.size() && !pinched; ++j) {
            pinched = j != i && crossed[i].collinear(crossed[j]);
        }
        if (!pinched) {
            return true;
        }
    }
    // Every remaining contact is a split point, so the open pieces between
    // consecutive split points each lie wholly inside or wholly outside the
    // region and their midpoints classify them. Doubling the region keeps the
    // midpoint test exact and division-free: (p+q)/2 is interior to A iff (p+q)
    // is interior to 2A. The unbounded pieces past the extreme split points need
    // no test: the region is bounded, and a piece that escapes it for good is
    // one whose far end left through an unpinched crossing, already answered
    // above.
    const auto along = [&](const V& p) {
        return (p.x() - base.x()) * direction.x() + (p.y() - base.y()) * direction.y();
    };
    std::sort(contacts.begin(), contacts.end(),
              [&](const V& p, const V& q) { return along(p) < along(q); });
    const auto doubled = (*this) * NumberType(2);
    for (std::size_t i = 1; i < contacts.size(); ++i) {
        if (contacts[i - 1] == contacts[i]) {
            continue;
        }
        if (doubled.interiorContains(contacts[i - 1] + contacts[i])) {
            return true;
        }
    }
    return false;
}

template <class PointType, class LabelType>
template <SegmentConcept OtherSegment>
constexpr bool PolygonWithHoles<PointType, LabelType>::interiorsIntersect(const OtherSegment& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }
    // Both endpoints bound the segment, so both are split points.
    using C = std::common_type_t<NumberType, typename OtherSegment::NumberType>;
    using V = Point<C>;
    const V begin = static_cast<V>(other.min());
    const V end = static_cast<V>(other.max());
    return linearInteriorsIntersect(other, std::vector<V>{begin, end}, begin, end - begin);
}

template <class PointType, class LabelType>
template <OrientedSegmentConcept OtherOrientedSegment>
constexpr bool PolygonWithHoles<PointType, LabelType>::interiorsIntersect(const OtherOrientedSegment& other) const {
    return interiorsIntersect(other.asSegment());
}

template <class PointType, class LabelType>
template <LineConcept OtherLine>
constexpr bool PolygonWithHoles<PointType, LabelType>::interiorsIntersect(const OtherLine& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }
    // A line ends nowhere, so it contributes no split point of its own; the ring
    // vertices on it are the whole list. Unlike @ref Polygon, a straddle of the
    // line is not enough: a hole touching the outer ring at two points can hold
    // the entire chord between them, and then the line misses the region
    // interior even though vertices lie on both sides of it.
    using C = std::common_type_t<NumberType, typename OtherLine::NumberType>;
    using V = Point<C>;
    const V begin = static_cast<V>(other.min());
    const V end = static_cast<V>(other.max());
    return linearInteriorsIntersect(other, std::vector<V>{}, begin, end - begin);
}

template <class PointType, class LabelType>
template <OrientedLineConcept OtherOrientedLine>
constexpr bool PolygonWithHoles<PointType, LabelType>::interiorsIntersect(const OtherOrientedLine& other) const {
    return interiorsIntersect(other.asLine());
}

template <class PointType, class LabelType>
template <RayConcept OtherRay>
constexpr bool PolygonWithHoles<PointType, LabelType>::interiorsIntersect(const OtherRay& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }
    // The source is where the ray begins, hence a split point; the far end runs
    // out of the bounded region for good.
    using C = std::common_type_t<NumberType, typename OtherRay::NumberType>;
    using V = Point<C>;
    const V source = static_cast<V>(other.source());
    const V target = static_cast<V>(other.target());
    return linearInteriorsIntersect(other, std::vector<V>{source}, source, target - source);
}

template <class PointType, class LabelType>
template <HalfplaneConcept OtherHalfplane>
constexpr bool PolygonWithHoles<PointType, LabelType>::interiorsIntersect(const OtherHalfplane& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }
    // The open half-plane meets the open region exactly when some vertex of some
    // ring is both strictly inside the half-plane and two-dimensional for the
    // region (@ref isSolidVertex).
    //
    // (⇐) is immediate: the half-plane is open, so it holds a neighbourhood of
    // that vertex, and a two-dimensional vertex has region interior in every one
    // of its neighbourhoods.
    //
    // (⇒) Take a connected piece W of A° ∩ H°. Its boundary runs along ∂A and
    // along the half-plane's edge line, and the stretches along ∂A are covered
    // once — a doubly covered stretch has the region pinched shut against it and
    // no interior beside it. Those stretches cannot all lie on the edge line, or
    // W would be bounded by a line alone and hence unbounded, which it is not.
    // So one of them has a point strictly inside the half-plane, and since the
    // covering multiplicity along a ring edge only changes at ring vertices, the
    // singly covered stretch containing it extends to a ring vertex; the farther
    // of its two ends is strictly inside the half-plane too, and is a
    // two-dimensional vertex because a singly covered stretch reaches it.
    for (const auto& vertex : outer_) {
        if (other.interiorContains(vertex) && isSolidVertex(vertex)) {
            return true;
        }
    }
    for (const auto& hole : holes_) {
        for (const auto& vertex : hole) {
            if (other.interiorContains(vertex) && isSolidVertex(vertex)) {
                return true;
            }
        }
    }
    return false;
}

template <class PointType, class LabelType>
constexpr bool PolygonWithHoles<PointType, LabelType>::isSolidVertex(const PointType& vertex) const {
    if (holes_.empty()) {
        return true;  // a lone simple ring has interior along all of itself
    }
    bool solid = false;
    anyBoundaryEdge([&](const auto& edge) {
        if (!edge.contains(vertex)) {
            return false;
        }
        for (const auto& endpoint : {edge.min(), edge.max()}) {
            if (endpoint == vertex) {
                continue;  // the stretch on this side of the vertex is empty
            }
            // Shrink the stretch to the nearest ring vertex beyond `vertex`: the
            // covering multiplicity is constant between consecutive ring
            // vertices, since ring edges begin and end at them and never cross.
            PointType beyond = endpoint;
            const auto shrink = [&](const PointType& candidate) {
                if (candidate != vertex && EdgeType(vertex, beyond).contains(candidate)) {
                    beyond = candidate;
                }
            };
            for (const auto& candidate : outer_) {
                shrink(candidate);
            }
            for (const auto& hole : holes_) {
                for (const auto& candidate : hole) {
                    shrink(candidate);
                }
            }
            // Count the ring edges covering the stretch, reading multiplicity at
            // its midpoint. Doubling both keeps that exact: m is on an edge iff
            // 2m is on the doubled edge.
            const PointType doubledMidpoint = vertex + beyond;
            std::size_t covers = 0;
            anyBoundaryEdge([&](const auto& candidate) {
                if (EdgeType(candidate.min() + candidate.min(), candidate.max() + candidate.max())
                        .contains(doubledMidpoint)) {
                    ++covers;
                }
                return false;
            });
            if (covers == 1) {
                solid = true;
                return true;
            }
        }
        return false;
    });
    return solid;
}

// The area operands are where the region's shape finally costs something. For a
// simple polygon the textbook argument is "if neither boundary reaches the
// other's interior, one interior witness point decides", and it works because a
// simple polygon's interior is connected. A region's is not: a hole spanning it
// leaves two slabs, and a witness in one says nothing about the other. The
// counterexample is small — take `[0,4]² ∖ ((0,4)×(1,2))`, whose interior is two
// slabs, against the rectangle `[0,4]×[0,2]`. Every edge of the rectangle lies
// on ∂A, every ring vertex of the region lies on ∂(rectangle), and the
// rectangle's own centre falls on the hole's boundary — yet the lower slab is
// inside the rectangle and the interiors do meet.
//
// So the fallback is the triangulated domain, which is where the region already
// keeps its answer to "what part of me has area". Its triangles tile
// closure(A°), so A° ∩ B° ≠ ∅ exactly when some triangle's interior meets B°:
// a non-empty open intersection has positive area, the triangles cover it, and
// a triangle carrying positive area of it has interior in it.
//
// That test alone is complete. The edge scan before it is a fast path — an edge
// of B whose relative interior reaches A° settles the question without
// triangulating anything, and it is what answers a query that actually overlaps
// the region.
//
// The fast path costs one assumption, though: an edge reaching A° only implies
// that B° does when B has interior beside that edge, i.e. when B is the closure
// of its own interior. Every operand here is — except another region, which may
// carry a slit, a stretch of boundary with the region pinched shut against it.
// Two regions sharing a slit line would both report a hit with no interior
// anywhere near it, so region against region skips the scan and compares the
// two triangulated domains directly.
template <class PointType, class LabelType>
template <class OtherArea>
bool PolygonWithHoles<PointType, LabelType>::areaInteriorsIntersect(const OtherArea& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }
    if (!bbox().interiorsIntersect(other.bbox())) {
        return false;
    }
    if constexpr (PolygonWithHolesConcept<OtherArea>) {
        // A region without holes is its outer polygon, and that is regular, so
        // reducing either side to it brings the cheaper path back — and with it
        // one triangulation instead of two.
        if (!other.hasHoles()) {
            return areaInteriorsIntersect(other.outer());
        }
        if (holes_.empty()) {
            return other.interiorsIntersect(outer_);
        }
        const auto mine = triangulation();
        const auto theirs = other.triangulation();
        return mine.visitTriangles([&theirs](const auto& t) {
            return theirs.visitTriangles([&t](const auto& u) { return t.interiorsIntersect(u); });
        });
    } else {
        // Without holes the region is its outer polygon and the simply connected
        // argument applies unchanged.
        if (holes_.empty()) {
            return other.interiorsIntersect(outer_);
        }
        for (const auto& edge : other.edges()) {
            if (interiorsIntersect(edge)) {
                return true;
            }
        }
        return triangulation().visitTriangles(
            [&other](const auto& triangle) { return other.interiorsIntersect(triangle); });
    }
}

template <class PointType, class LabelType>
template <RectangleConcept OtherRectangle>
bool PolygonWithHoles<PointType, LabelType>::interiorsIntersect(const OtherRectangle& other) const {
    return areaInteriorsIntersect(other);
}

template <class PointType, class LabelType>
template <TriangleConcept OtherTriangle>
bool PolygonWithHoles<PointType, LabelType>::interiorsIntersect(const OtherTriangle& other) const {
    return areaInteriorsIntersect(other);
}

template <class PointType, class LabelType>
template <ConvexConcept OtherConvex>
bool PolygonWithHoles<PointType, LabelType>::interiorsIntersect(const OtherConvex& other) const {
    return areaInteriorsIntersect(other);
}

template <class PointType, class LabelType>
template <PolygonConcept OtherPolygon>
bool PolygonWithHoles<PointType, LabelType>::interiorsIntersect(const OtherPolygon& other) const {
    return areaInteriorsIntersect(other);
}

template <class PointType, class LabelType>
template <PolygonWithHolesConcept OtherRegion>
bool PolygonWithHoles<PointType, LabelType>::interiorsIntersect(const OtherRegion& other) const {
    return areaInteriorsIntersect(other);
}

// The chain's relative interior is its open edges together with the vertices
// between them, and the region's interior is open and two-dimensional — which is
// all the shared chain helper needs. (An open-edge point that has to be
// discarded, because a self-intersecting polyline passes through one of its own
// extremes there, is surrounded by edge points that do not; an open subset of
// the edge cannot consist of those two points alone.)
template <class PointType, class LabelType>
template <MonotoneChainConcept OtherChain>
constexpr bool PolygonWithHoles<PointType, LabelType>::interiorsIntersect(const OtherChain& other) const {
    return detail::chainInteriorsIntersect(other, *this);
}

template <class PointType, class LabelType>
template <PolylineConcept OtherPolyline>
constexpr bool PolygonWithHoles<PointType, LabelType>::interiorsIntersect(const OtherPolyline& other) const {
    return detail::chainInteriorsIntersect(other, *this);
}

// The disk brings no edges to scan, so the fast path the area operands use —
// an edge of the operand whose relative interior reaches A° — has no analogue
// here, and the triangulated domain does the work. It answers completely: the
// domain triangles tile closure(A°) and their interiors lie in A°, so A° ∩ D°
// is nonempty exactly when some triangle interior meets the open disk.
//
// Two cheap tests come first. A disk missing the closed region misses its
// interior too, and that is an O(n) edge scan; and a disk whose own interior
// witness falls in A° settles it outright, which is what a query that actually
// overlaps the region usually does.
template <class PointType, class LabelType>
template <DiskConcept OtherDisk>
bool PolygonWithHoles<PointType, LabelType>::interiorsIntersect(const OtherDisk& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }
    if (holes_.empty()) {
        return other.interiorsIntersect(outer_);
    }
    if (!intersects(other)) {
        return false;
    }
    if (other.pointInsideInteriorContainedIn(*this)) {
        return true;
    }
    return triangulation().visitTriangles(
        [&other](const auto& triangle) { return other.interiorsIntersect(triangle); });
}

// Clipping the operand to the region's bounding box loses nothing: A° is an open
// subset of that box, so it stays clear of the box boundary, and the clip is
// inflated past the box anyway. What it gains is a bounded operand — a convex
// polygon — which the area path handles.
template <class PointType, class LabelType>
template <HalfplaneIntersectionConcept OtherIntersection>
bool PolygonWithHoles<PointType, LabelType>::interiorsIntersect(const OtherIntersection& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }
    const auto clipped = detail::regionClippedToBox(other, bbox());
    if (clipped.isDegenerate()) {
        return false;
    }
    return areaInteriorsIntersect(asConvexOperand(clipped));
}


// ---------------------------------------------------------------------------
// Reverse direction: interiorsIntersect is symmetric, so the lower-ranked
// shapes' generic rank-guarded forwarders dispatch these here — no per-shape
// definitions are needed.

}  // namespace pgl
