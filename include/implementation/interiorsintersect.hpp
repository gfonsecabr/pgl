#pragma once

/**
 * @file interiorsintersect.hpp
 * @brief Implementations of the 'interiorsIntersect' predicate.
 **/

#include <limits>
#include "../pgl.hpp"
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
template<PointConcept OtherPoint>
constexpr bool Segment<PointType, LabelType>::interiorsIntersect(const Segment<OtherPoint>& other) const {
    if constexpr (is_Rational_v<NumberType> || is_Rational_v<typename OtherPoint::NumberType>) {
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
    return interiorContains(c) ||
           interiorContains(d) ||
           other.interiorContains(a) ||
           other.interiorContains(b);
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

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Triangle<PointType>::interiorsIntersect(const OtherPoint& other) const {
    // A point's interior is the point itself, so this matches interiorContains.
    return interiorContains(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Triangle<PointType>::interiorsIntersect(const Line<OtherPoint>& other) const {
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

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Triangle<PointType>::interiorsIntersect(const OrientedLine<OtherPoint>& other) const {
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

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Triangle<PointType>::interiorsIntersect(const Segment<OtherPoint>& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }
    return interiorContains(other.min()) ||
           interiorContains(other.max()) ||
           other.separates(*this);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Triangle<PointType>::interiorsIntersect(const OrientedSegment<OtherPoint>& other) const {
    return interiorsIntersect(static_cast<Segment<OtherPoint>>(other));
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Triangle<PointType>::interiorsIntersect(const Ray<OtherPoint>& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }
    return interiorContains(other.source()) || other.separates(*this);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Triangle<PointType>::interiorsIntersect(const Halfplane<OtherPoint>& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }
    return other.interiorContains(a()) || other.interiorContains(b()) || other.interiorContains(c());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Triangle<PointType>::interiorsIntersect(const Rectangle<OtherPoint>& other) const {
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

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Triangle<PointType>::interiorsIntersect(const Triangle<OtherPoint>& other) const {
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

template <class PointType>
constexpr bool Triangle<PointType>::interiorsIntersect(const Shape<PointType>& other) const {
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

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedSegment<PointType>::interiorsIntersect(const OtherPoint& other) const {
    return this->asSegment().interiorsIntersect(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedSegment<PointType>::interiorsIntersect(const Segment<OtherPoint>& other) const {
    return this->asSegment().interiorsIntersect(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedSegment<PointType>::interiorsIntersect(const OrientedSegment<OtherPoint>& other) const {
    return this->asSegment().interiorsIntersect(other.asSegment());
}

template <class PointType>
constexpr bool OrientedSegment<PointType>::interiorsIntersect(const Shape<PointType>& other) const {
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

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Line<PointType>::interiorsIntersect(const OtherPoint& other) const {
    // A point's interior is the point itself, so this matches interiorContains.
    return interiorContains(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Line<PointType>::interiorsIntersect(const Line<OtherPoint>& other) const {
    return intersects(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Line<PointType>::interiorsIntersect(const Segment<OtherPoint>& other) const {
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

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Line<PointType>::interiorsIntersect(const OrientedSegment<OtherPoint>& other) const {
    return interiorsIntersect(static_cast<Segment<OtherPoint>>(other));
}

template <class PointType>
constexpr bool Line<PointType>::interiorsIntersect(const Shape<PointType>& other) const {
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

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedLine<PointType>::interiorsIntersect(const OtherPoint& other) const {
    return this->asLine().interiorsIntersect(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedLine<PointType>::interiorsIntersect(const Line<OtherPoint>& other) const {
    return intersects(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedLine<PointType>::interiorsIntersect(const OrientedLine<OtherPoint>& other) const {
    return intersects(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedLine<PointType>::interiorsIntersect(const Segment<OtherPoint>& other) const {
    return this->asLine().interiorsIntersect(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedLine<PointType>::interiorsIntersect(const OrientedSegment<OtherPoint>& other) const {
    return this->asLine().interiorsIntersect(other);
}

template <class PointType>
constexpr bool OrientedLine<PointType>::interiorsIntersect(const Shape<PointType>& other) const {
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

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Ray<PointType>::interiorsIntersect(const OtherPoint& other) const {
    // A point's interior is the point itself, so this matches interiorContains.
    return interiorContains(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Ray<PointType>::interiorsIntersect(const Line<OtherPoint>& other) const {
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

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Ray<PointType>::interiorsIntersect(const OrientedLine<OtherPoint>& other) const {
    return interiorsIntersect(other.asLine());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Ray<PointType>::interiorsIntersect(const Segment<OtherPoint>& other) const {
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

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Ray<PointType>::interiorsIntersect(const OrientedSegment<OtherPoint>& other) const {
    return interiorsIntersect(static_cast<Segment<OtherPoint>>(other));
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Ray<PointType>::interiorsIntersect(const Ray<OtherPoint>& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }

    const auto other_source_side = orientationSign(source(), target(), other.source());
    const auto other_target_side = orientationSign(source(), target(), other.target());

    if (other_source_side == std::partial_ordering::equivalent &&
        other_target_side == std::partial_ordering::equivalent) {
        return interiorContains(other.source()) || other.interiorContains(source());
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

template <class PointType>
constexpr bool Ray<PointType>::interiorsIntersect(const Shape<PointType>& other) const {
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

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Rectangle<PointType>::interiorsIntersect(const OtherPoint& other) const {
    // A point's interior is the point itself, so this matches interiorContains.
    return interiorContains(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Rectangle<PointType>::interiorsIntersect(const Rectangle<OtherPoint>& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }
    return intervalsOverlapStrict(min().x(), max().x(), other.min().x(), other.max().x()) &&
           intervalsOverlapStrict(min().y(), max().y(), other.min().y(), other.max().y());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Rectangle<PointType>::interiorsIntersect(const Line<OtherPoint>& other) const {
    if (isDegenerate()) {
        return false;
    }
    if (other.isDegenerate()) {
        return interiorContains(other.min());
    }
    return detail::lineIntersectsRectangleInterior(*this, other.min(), other.max());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Rectangle<PointType>::interiorsIntersect(const OrientedLine<OtherPoint>& other) const {
    if (isDegenerate()) {
        return false;
    }
    if (other.isDegenerate()) {
        return interiorContains(other.source());
    }
    return detail::lineIntersectsRectangleInterior(*this, other.source(), other.target());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Rectangle<PointType>::interiorsIntersect(const Segment<OtherPoint>& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }
    return detail::segmentIntersectsRectangleInteriorExact(*this, other.min(), other.max());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Rectangle<PointType>::interiorsIntersect(const OrientedSegment<OtherPoint>& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }
    return detail::segmentIntersectsRectangleInteriorExact(*this, other.source(), other.target());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Rectangle<PointType>::interiorsIntersect(const Ray<OtherPoint>& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }
    if (interiorContains(other.source())) {
        return true;
    }
    return Segment<PointType>((*this)[0], (*this)[2]).interiorsIntersect(other) ||
           Segment<PointType>((*this)[1], (*this)[3]).interiorsIntersect(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Rectangle<PointType>::interiorsIntersect(const Halfplane<OtherPoint>& other) const {
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

template <class PointType>
constexpr bool Rectangle<PointType>::interiorsIntersect(const Shape<PointType>& other) const {
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

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Halfplane<PointType>::interiorsIntersect(const OtherPoint& other) const {
    // A point's interior is the point itself, so this matches interiorContains.
    return interiorContains(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Halfplane<PointType>::interiorsIntersect(const Line<OtherPoint>& other) const {
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

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Halfplane<PointType>::interiorsIntersect(const OrientedLine<OtherPoint>& other) const {
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

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Halfplane<PointType>::interiorsIntersect(const Segment<OtherPoint>& other) const {
    if (isDegenerate()) {
        return false;
    }
    const auto first_side = orientationDeterminant(source(), target(), other.min());
    const auto second_side = orientationDeterminant(source(), target(), other.max());
    const auto zero = decltype(first_side){};
    return zero < first_side || zero < second_side;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Halfplane<PointType>::interiorsIntersect(const OrientedSegment<OtherPoint>& other) const {
    if (isDegenerate()) {
        return false;
    }
    const auto first_side = orientationDeterminant(source(), target(), other.source());
    const auto second_side = orientationDeterminant(source(), target(), other.target());
    const auto zero = decltype(first_side){};
    return zero < first_side || zero < second_side;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Halfplane<PointType>::interiorsIntersect(const Ray<OtherPoint>& other) const {
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

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Halfplane<PointType>::interiorsIntersect(const Halfplane<OtherPoint>& other) const {
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
    const auto other_oriented_boundary = static_cast<OrientedLine<OtherPoint>>(other);
    return this_oriented_boundary == other_oriented_boundary;
}

template <class PointType>
constexpr bool Halfplane<PointType>::interiorsIntersect(const Shape<PointType>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->interiorsIntersect(value);
        },
        other.variant());
}


// ---------------------------------------------------------------------------
// Convex

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Convex<PointType>::interiorsIntersect(const OtherPoint& other) const {
    // A point's interior is the point itself, so this matches interiorContains.
    return interiorContains(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Convex<PointType>::interiorsIntersect(const Line<OtherPoint>& other) const {
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

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Convex<PointType>::interiorsIntersect(const OrientedLine<OtherPoint>& other) const {
    return interiorsIntersect(static_cast<Line<OtherPoint>>(other));
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Convex<PointType>::interiorsIntersect(const Segment<OtherPoint>& other) const {
    if (isDegenerate() || other.isDegenerate()) {
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

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Convex<PointType>::interiorsIntersect(const OrientedSegment<OtherPoint>& other) const {
    return interiorsIntersect(static_cast<Segment<OtherPoint>>(other));
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Convex<PointType>::interiorsIntersect(const Ray<OtherPoint>& other) const {
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

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Convex<PointType>::interiorsIntersect(const Halfplane<OtherPoint>& other) const {
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

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Convex<PointType>::interiorsIntersect(const Rectangle<OtherPoint>& other) const {
    return interiorsIntersect(other.asConvex());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Convex<PointType>::interiorsIntersect(const Triangle<OtherPoint>& other) const {
    return interiorsIntersect(other.asConvex());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Convex<PointType>::interiorsIntersect(const Convex<OtherPoint>& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }
    if (size() > other.size()) {
        return other.interiorsIntersect(*this);
    }
    if (!bbox().intersects(other.bbox())) {
        return false;
    }

    // If this contains 3 vertices of other, the interiors intersect,
    // even if the vertices are on the boundary.
    if (contains(other[0]) && contains(other[1]) && contains(other[2])) {
        return true;
    }

    // Here we know that other is not inside this, so if the interiors intersect,
    // then there is an edge of this that intersects the interior of other.

    for (const auto& edge : edges()) {
        if (other.interiorsIntersect(edge)) {
            return true;
        }
    }

    return false;
}

template <class PointType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool Convex<PointType>::interiorsIntersect(const Disk<OtherPoint, OtherLabel>& other) const {
    if (interiorContains(other[0])) {
        return true;
    }
    for (auto &edge : edges()) {
        if (edge.interiorsIntersect(other)) {
            return true;
        }
    }
    return false;
}

template <class PointType>
template <PointConcept OtherPoint>
constexpr bool Convex<PointType>::interiorsIntersect(const Shape<OtherPoint>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->interiorsIntersect(value);
        },
        other.variant());
}


// ---------------------------------------------------------------------------
// Polygon

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Polygon<PointType>::interiorsIntersect(const OtherPoint& other) const {
    // A point's interior is the point itself, so this matches interiorContains.
    return interiorContains(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Polygon<PointType>::interiorsIntersect(const Line<OtherPoint>& other) const {
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

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Polygon<PointType>::interiorsIntersect(const OrientedLine<OtherPoint>& other) const {
    return interiorsIntersect(static_cast<Line<OtherPoint>>(other));
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Polygon<PointType>::interiorsIntersect(const Segment<OtherPoint>& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }
    // A transversal crossing of any boundary edge places an interior point on
    // the open segment.
    for (const auto& edge : edges()) {
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
    using C = std::common_type_t<NumberType, typename OtherPoint::NumberType>;
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

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Polygon<PointType>::interiorsIntersect(const OrientedSegment<OtherPoint>& other) const {
    return interiorsIntersect(static_cast<Segment<OtherPoint>>(other));
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Polygon<PointType>::interiorsIntersect(const Ray<OtherPoint>& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }
    for (const auto& edge : edges()) {
        if (edge.crosses(other)) {
            return true;
        }
    }
    // As with the segment, the remaining boundary contacts are the ray's source
    // and the polygon vertices on the ray; split there and test midpoints. The
    // ray exits the bounded polygon for good past its last contact.
    using C = std::common_type_t<NumberType, typename OtherPoint::NumberType>;
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

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Polygon<PointType>::interiorsIntersect(const Halfplane<OtherPoint>& other) const {
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

// Two filled simple polygons have intersecting interiors iff a vertex of one is
// strictly inside the other, or a pair of their edges properly cross. (Exact
// coincidence is caught separately for the polygon-polygon overload.)
template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Polygon<PointType>::interiorsIntersect(const Rectangle<OtherPoint>& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }
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
    for (const auto& edge : edges()) {
        for (const auto& otherEdge : other.edges()) {
            if (edge.crosses(otherEdge)) {
                return true;
            }
        }
    }
    return false;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Polygon<PointType>::interiorsIntersect(const Triangle<OtherPoint>& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }
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
    for (const auto& edge : edges()) {
        for (const auto& otherEdge : other.edges()) {
            if (edge.crosses(otherEdge)) {
                return true;
            }
        }
    }
    return false;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Polygon<PointType>::interiorsIntersect(const Convex<OtherPoint>& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }
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
    for (const auto& edge : edges()) {
        for (const auto& otherEdge : other.edges()) {
            if (edge.crosses(otherEdge)) {
                return true;
            }
        }
    }
    return false;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Polygon<PointType>::interiorsIntersect(const Polygon<OtherPoint>& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return false;
    }
    // Exact coincidence shares the whole interior but exposes no strictly-inside
    // vertex or crossing edge, so test it directly (only when comparable).
    if constexpr (std::same_as<OtherPoint, PointType>) {
        if (*this == other) {
            return true;
        }
    }
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
    for (const auto& edge : edges()) {
        for (const auto& otherEdge : other.edges()) {
            if (edge.crosses(otherEdge)) {
                return true;
            }
        }
    }
    return false;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Convex<PointType>::interiorsIntersect(const Polygon<OtherPoint>& other) const {
    return other.interiorsIntersect(*this);
}


// --- Disk symmetric-trio stubs (not yet implemented) + Shape dispatch ---

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Disk<PointType, LabelType>::interiorsIntersect(const OtherPoint& other) const {
    // A point's interior is the point itself, so this matches interiorContains.
    return interiorContains(other);
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Disk<PointType, LabelType>::interiorsIntersect(const Segment<OtherPoint>&) const {
    throw std::runtime_error(
        "pgl: Disk::interiorsIntersect(Segment) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Disk<PointType, LabelType>::interiorsIntersect(const OrientedSegment<OtherPoint>&) const {
    throw std::runtime_error(
        "pgl: Disk::interiorsIntersect(OrientedSegment) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Disk<PointType, LabelType>::interiorsIntersect(const Line<OtherPoint>&) const {
    throw std::runtime_error(
        "pgl: Disk::interiorsIntersect(Line) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Disk<PointType, LabelType>::interiorsIntersect(const OrientedLine<OtherPoint>&) const {
    throw std::runtime_error(
        "pgl: Disk::interiorsIntersect(OrientedLine) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Disk<PointType, LabelType>::interiorsIntersect(const Ray<OtherPoint>&) const {
    throw std::runtime_error(
        "pgl: Disk::interiorsIntersect(Ray) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Disk<PointType, LabelType>::interiorsIntersect(const Halfplane<OtherPoint>&) const {
    throw std::runtime_error(
        "pgl: Disk::interiorsIntersect(Halfplane) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Disk<PointType, LabelType>::interiorsIntersect(const Rectangle<OtherPoint>&) const {
    throw std::runtime_error(
        "pgl: Disk::interiorsIntersect(Rectangle) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Disk<PointType, LabelType>::interiorsIntersect(const Triangle<OtherPoint>&) const {
    throw std::runtime_error(
        "pgl: Disk::interiorsIntersect(Triangle) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool Disk<PointType, LabelType>::interiorsIntersect(const Disk<OtherPoint, OtherLabel>&) const {
    throw std::runtime_error(
        "pgl: Disk::interiorsIntersect(Disk) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
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


template <class PointType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool Polygon<PointType>::interiorsIntersect(const Disk<OtherPoint, OtherLabel>&) const {
    throw std::runtime_error(
        "pgl: Polygon::interiorsIntersect(Disk) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
}

}  // namespace pgl
