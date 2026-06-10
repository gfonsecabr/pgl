#pragma once

/**
 * @file interiorcontains.hpp
 * @brief Implementations of the 'interiorContains' predicate.
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
constexpr bool Point<Number, Label>::interiorContains(const OtherPoint& other) const {
    return contains(other);
}

template <class Number, class Label>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool Point<Number, Label>::interiorContains(const Segment<OtherPoint, OtherLabel>& other) const {
    return contains(other);
}

template <class Number, class Label>
template<PointConcept OtherPoint>
constexpr bool Point<Number, Label>::interiorContains(const OrientedSegment<OtherPoint>& other) const {
    return contains(other);
}

template <class Number, class Label>
template<PointConcept OtherPoint>
constexpr bool Point<Number, Label>::interiorContains(const Line<OtherPoint>& other) const {
    return contains(other);
}

template <class Number, class Label>
template<PointConcept OtherPoint>
constexpr bool Point<Number, Label>::interiorContains(const OrientedLine<OtherPoint>& other) const {
    return contains(other);
}

template <class Number, class Label>
template<PointConcept OtherPoint>
constexpr bool Point<Number, Label>::interiorContains(const Ray<OtherPoint>& other) const {
    return contains(other);
}

template <class Number, class Label>
template<PointConcept OtherPoint>
constexpr bool Point<Number, Label>::interiorContains(const Halfplane<OtherPoint>& other) const {
    return contains(other);
}

template <class Number, class Label>
template<PointConcept OtherPoint>
constexpr bool Point<Number, Label>::interiorContains(const Rectangle<OtherPoint>& other) const {
    return contains(other);
}

template <class Number, class Label>
template<PointConcept OtherPoint>
constexpr bool Point<Number, Label>::interiorContains(const Triangle<OtherPoint>& other) const {
    return contains(other);
}

template <class Number, class Label>
template<PointConcept OtherPoint>
constexpr bool Point<Number, Label>::interiorContains(const Convex<OtherPoint>& other) const {
    return contains(other);
}

template <class Number, class Label>
template<PointConcept OtherPoint>
constexpr bool Point<Number, Label>::interiorContains(const Polygon<OtherPoint>& other) const {
    return contains(other);
}

template <class Number, class Label>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool Point<Number, Label>::interiorContains(const Disk<OtherPoint, OtherLabel>& other) const {
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
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool Segment<PointType, LabelType>::interiorContains(const Segment<OtherPoint, OtherLabel>& other) const {
    return interiorContains(other.min()) && interiorContains(other.max());
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Segment<PointType, LabelType>::interiorContains(const OrientedSegment<OtherPoint>& other) const {
    return interiorContains(other.source()) && interiorContains(other.target());
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Segment<PointType, LabelType>::interiorContains(const Triangle<OtherPoint>& other) const {
    return interiorContains(other.a()) && interiorContains(other.b()) && interiorContains(other.c());
}

template <class PointType, class LabelType>
template<class S>
    requires(!detail::is_point_v<S>)
constexpr bool Segment<PointType, LabelType>::interiorContains(const S&) const {
    return false;
}

/**
 * @section predicates-triangle Triangle
 * Triangle boundary, containment, intersection, and cut predicates, including
 * triangle-vs-rectangle and triangle-vs-triangle topological cases.
 */

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Triangle<PointType>::interiorContains(const OtherPoint& point) const {
    return !isDegenerate() && contains(point) && !boundaryContains(point);
}

template <class PointType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool Triangle<PointType>::interiorContains(const Segment<OtherPoint, OtherLabel>& other) const {
    return interiorContains(other.min()) && interiorContains(other.max());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Triangle<PointType>::interiorContains(const OrientedSegment<OtherPoint>& other) const {
    return interiorContains(other.source()) && interiorContains(other.target());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Triangle<PointType>::interiorContains(const Line<OtherPoint>& other) const {
    return other.isDegenerate() && interiorContains(other.min());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Triangle<PointType>::interiorContains(const OrientedLine<OtherPoint>& other) const {
    return other.isDegenerate() && interiorContains(other.source());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Triangle<PointType>::interiorContains(const Ray<OtherPoint>& other) const {
    return other.isDegenerate() && interiorContains(other.source());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Triangle<PointType>::interiorContains(const Halfplane<OtherPoint>& other) const {
    return other.isDegenerate() && interiorContains(other.source());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Triangle<PointType>::interiorContains(const Rectangle<OtherPoint>& other) const {
    const auto vertices = other.vertices();
    for (const auto& vertex : vertices) {
        if (!interiorContains(vertex)) {
            return false;
        }
    }
    return true;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Triangle<PointType>::interiorContains(const Triangle<OtherPoint>& other) const {
    return interiorContains(other.a()) && interiorContains(other.b()) && interiorContains(other.c());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Triangle<PointType>::interiorContains(const Convex<OtherPoint>& other) const {
    if (other.size() == 0) {
        return true;
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

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedSegment<PointType>::interiorContains(const OtherPoint& point) const {
    return !boundaryContains(point) && contains(point);
}

template <class PointType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool OrientedSegment<PointType>::interiorContains(const Segment<OtherPoint, OtherLabel>& other) const {
    return interiorContains(other.min()) && interiorContains(other.max());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedSegment<PointType>::interiorContains(const OrientedSegment<OtherPoint>& other) const {
    return interiorContains(other.source()) && interiorContains(other.target());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedSegment<PointType>::interiorContains(const Line<OtherPoint>& other) const {
    return other.isDegenerate() && interiorContains(other.min());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedSegment<PointType>::interiorContains(const OrientedLine<OtherPoint>& other) const {
    return other.isDegenerate() && interiorContains(other.source());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedSegment<PointType>::interiorContains(const Ray<OtherPoint>& other) const {
    return other.isDegenerate() && interiorContains(other.source());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedSegment<PointType>::interiorContains(const Halfplane<OtherPoint>& other) const {
    return other.isDegenerate() && interiorContains(other.source());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedSegment<PointType>::interiorContains(const Rectangle<OtherPoint>& other) const {
    return interiorContains(other.min()) && interiorContains(other.max());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedSegment<PointType>::interiorContains(const Triangle<OtherPoint>& other) const {
    return interiorContains(other.a()) && interiorContains(other.b()) && interiorContains(other.c());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedSegment<PointType>::interiorContains(const Convex<OtherPoint>& other) const {
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

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Line<PointType>::interiorContains(const OtherPoint& point) const {
    return contains(point);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Line<PointType>::interiorContains(const Line<OtherPoint>& other) const {
    return contains(other);
}

template <class PointType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool Line<PointType>::interiorContains(const Segment<OtherPoint, OtherLabel>& other) const {
    return contains(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Line<PointType>::interiorContains(const OrientedSegment<OtherPoint>& other) const {
    return contains(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Line<PointType>::interiorContains(const OrientedLine<OtherPoint>& other) const {
    return contains(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Line<PointType>::interiorContains(const Ray<OtherPoint>& other) const {
    return contains(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Line<PointType>::interiorContains(const Halfplane<OtherPoint>& other) const {
    return contains(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Line<PointType>::interiorContains(const Rectangle<OtherPoint>& other) const {
    return contains(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Line<PointType>::interiorContains(const Triangle<OtherPoint>& other) const {
    return contains(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Line<PointType>::interiorContains(const Convex<OtherPoint>& other) const {
    // A line has no boundary, so its interior is the whole line.
    return contains(other);
}

/**
 * @section predicates-oriented-line OrientedLine
 * Oriented-line predicates. Shared topology is mostly delegated to the
 * unoriented line view, while orientation-specific methods stay local here.
 */

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedLine<PointType>::interiorContains(const OtherPoint& point) const {
    return contains(point);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedLine<PointType>::interiorContains(const Line<OtherPoint>& other) const {
    return contains(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedLine<PointType>::interiorContains(const OrientedLine<OtherPoint>& other) const {
    return contains(other);
}

template <class PointType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool OrientedLine<PointType>::interiorContains(const Segment<OtherPoint, OtherLabel>& other) const {
    return contains(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedLine<PointType>::interiorContains(const OrientedSegment<OtherPoint>& other) const {
    return contains(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedLine<PointType>::interiorContains(const Ray<OtherPoint>& other) const {
    return contains(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedLine<PointType>::interiorContains(const Halfplane<OtherPoint>& other) const {
    return contains(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedLine<PointType>::interiorContains(const Rectangle<OtherPoint>& other) const {
    return contains(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedLine<PointType>::interiorContains(const Triangle<OtherPoint>& other) const {
    return contains(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedLine<PointType>::interiorContains(const Convex<OtherPoint>& other) const {
    // A line has no boundary, so its interior is the whole line.
    return contains(other);
}

/**
 * @section predicates-ray Ray
 * Ray-specific containment, intersection, and topological predicates. This is
 * where the asymmetric behavior of a half-infinite 1D primitive is implemented.
 */

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Ray<PointType>::interiorContains(const OtherPoint& point) const {
    return contains(point) && !boundaryContains(point);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Ray<PointType>::interiorContains(const Line<OtherPoint>& other) const {
    return other.isDegenerate() && interiorContains(other.min());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Ray<PointType>::interiorContains(const OrientedLine<OtherPoint>& other) const {
    return other.isDegenerate() && interiorContains(other.source());
}

template <class PointType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool Ray<PointType>::interiorContains(const Segment<OtherPoint, OtherLabel>& other) const {
    return interiorContains(other.min()) && interiorContains(other.max());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Ray<PointType>::interiorContains(const OrientedSegment<OtherPoint>& other) const {
    return interiorContains(other.source()) && interiorContains(other.target());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Ray<PointType>::interiorContains(const Ray<OtherPoint>& other) const {
    return interiorContains(other.source()) && contains(other.target());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Ray<PointType>::interiorContains(const Halfplane<OtherPoint>& other) const {
    return other.isDegenerate() && interiorContains(other.source());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Ray<PointType>::interiorContains(const Rectangle<OtherPoint>& other) const {
    return interiorContains(other.min()) && interiorContains(other.max());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Ray<PointType>::interiorContains(const Triangle<OtherPoint>& other) const {
    return interiorContains(other.a()) &&
           interiorContains(other.b()) &&
           interiorContains(other.c());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Ray<PointType>::interiorContains(const Convex<OtherPoint>& other) const {
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


template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Rectangle<PointType>::interiorContains(const OtherPoint& point) const {
    return contains(point) && !boundaryContains(point);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Rectangle<PointType>::interiorContains(const Line<OtherPoint>& other) const {
    return other.isDegenerate() && interiorContains(other.min());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Rectangle<PointType>::interiorContains(const OrientedLine<OtherPoint>& other) const {
    return other.isDegenerate() && interiorContains(other.source());
}

template <class PointType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool Rectangle<PointType>::interiorContains(const Segment<OtherPoint, OtherLabel>& other) const {
    return interiorContains(other.min()) && interiorContains(other.max());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Rectangle<PointType>::interiorContains(const OrientedSegment<OtherPoint>& other) const {
    return interiorContains(other.source()) && interiorContains(other.target());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Rectangle<PointType>::interiorContains(const Ray<OtherPoint>& other) const {
    return other.isDegenerate() && interiorContains(other.source());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Rectangle<PointType>::interiorContains(const Halfplane<OtherPoint>& other) const {
    return other.isDegenerate() && interiorContains(other.source());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Rectangle<PointType>::interiorContains(const Rectangle<OtherPoint>& other) const {
    return interiorContains(other.min()) && interiorContains(other.max());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Rectangle<PointType>::interiorContains(const Triangle<OtherPoint>& other) const {
    return interiorContains(other.a()) && interiorContains(other.b()) && interiorContains(other.c());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Rectangle<PointType>::interiorContains(const Convex<OtherPoint>& other) const {
    // For an axis-aligned rectangle, containing the convex strictly is equivalent
    // to containing its (axis-aligned) bounding box strictly.
    return other.size() == 0 || interiorContains(other.bbox());
}

/**
 * @section predicates-halfplane Halfplane
 * Half-plane containment, intersection, and topological predicates, together
 * with the helper routines used for strict side/interior tests.
 */

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Halfplane<PointType>::interiorContains(const OtherPoint& point) const {
    return contains(point) && !boundaryContains(point);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Halfplane<PointType>::interiorContains(const Line<OtherPoint>& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return other.isDegenerate() && interiorContains(other.min());
    }
    const auto direction_side =
        orientationDeterminant(source(), target(), other.max()) -
        orientationDeterminant(source(), target(), other.min());
    return direction_side == decltype(direction_side){} && interiorContains(other.min());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Halfplane<PointType>::interiorContains(const OrientedLine<OtherPoint>& other) const {
    if (isDegenerate() || other.isDegenerate()) {
        return other.isDegenerate() && interiorContains(other.source());
    }
    const auto direction_side =
        orientationDeterminant(source(), target(), other.target()) -
        orientationDeterminant(source(), target(), other.source());
    return direction_side == decltype(direction_side){} && interiorContains(other.source());
}

template <class PointType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool Halfplane<PointType>::interiorContains(const Segment<OtherPoint, OtherLabel>& other) const {
    if (isDegenerate()) {
        return false;
    }
    return interiorContains(other.min()) && interiorContains(other.max());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Halfplane<PointType>::interiorContains(const OrientedSegment<OtherPoint>& other) const {
    if (isDegenerate()) {
        return false;
    }
    return interiorContains(other.source()) && interiorContains(other.target());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Halfplane<PointType>::interiorContains(const Ray<OtherPoint>& other) const {
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

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Halfplane<PointType>::interiorContains(const Rectangle<OtherPoint>& other) const {
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

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Halfplane<PointType>::interiorContains(const Convex<OtherPoint>& other) const {
    if (other.size() == 0) {
        return true;
    }

    return interiorContains(other[0]) && !static_cast<Line<PointType>>(*this).intersects(other);
}

template <class PointType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool Halfplane<PointType>::interiorContains(const Disk<OtherPoint, OtherLabel>& other) const {
    // The closed disk lies in the open half-plane iff its center is strictly on
    // the interior side and its distance to the boundary line exceeds the
    // radius. Squaring keeps the test exact: with the boundary direction AB and
    // cross = det(AB, A->center) = |AB| * signedDistance(center), the condition
    // signedDistance > radius becomes cross > 0 and cross^2 > radius^2 * |AB|^2.
    const auto cross = orientationDeterminant(source(), target(), other.center());
    const auto zero = decltype(cross){};
    if (!(zero < cross)) {
        return false;
    }
    const auto squaredLength = source().squaredDistance(target());
    return cross * cross > other.squaredRadius() * squaredLength;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Halfplane<PointType>::interiorContains(const Halfplane<OtherPoint>& other) const {
    return other.isDegenerate() && interiorContains(other.source());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Halfplane<PointType>::interiorContains(const Triangle<OtherPoint>& other) const {
    return interiorContains(other.a()) && interiorContains(other.b()) && interiorContains(other.c());
}


// ---------------------------------------------------------------------------
// Convex

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Convex<PointType>::interiorContains(const OtherPoint& point) const {
    // A point is in the strict interior iff it is contained but not on
    // the boundary. Both predicates are O(log n) and together they handle
    // every edge case (including points on vertical edges at extreme x,
    // where edgesAtX returns the horizontal extent rather than the
    // vertical edge itself).
    return contains(point) && !boundaryContains(point);
}

template <class PointType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool Convex<PointType>::interiorContains(const Segment<OtherPoint, OtherLabel>& other) const {
    return interiorContains(other[0]) && interiorContains(other[1]);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Convex<PointType>::interiorContains(const OrientedSegment<OtherPoint>& other) const {
    return interiorContains(other[0]) && interiorContains(other[1]);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Convex<PointType>::interiorContains(const Line<OtherPoint>&) const {
    return false;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Convex<PointType>::interiorContains(const OrientedLine<OtherPoint>&) const {
    return false;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Convex<PointType>::interiorContains(const Ray<OtherPoint>&) const {
    return false;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Convex<PointType>::interiorContains(const Halfplane<OtherPoint>&) const {
    return false;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Convex<PointType>::interiorContains(const Rectangle<OtherPoint>& other) const {
    for (size_t i = 0; i < 4; ++i) {
        if (!interiorContains(other[i])) {
            return false;
        }
    }
    return true;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Convex<PointType>::interiorContains(const Triangle<OtherPoint>& other) const {
    for (size_t i = 0; i < 3; ++i) {
        if (!interiorContains(other[i])) {
            return false;
        }
    }
    return true;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Convex<PointType>::interiorContains(const Convex<OtherPoint>& other) const {
    if (size() <= 2) {
        return false;
    }
    if (other.size() == 0) {
        return true;
    }
    if (other.size() == 1) {
        return interiorContains(other[0]);
    }
    if (other.size() == 2) {
        return interiorContains(Segment<OtherPoint>(other[0], other[1]));
    }

    if (other.size() <= 2*size()) {
        for (size_t i = 0; i < other.size(); ++i) {
            if (!interiorContains(other[i])) {
                return false;
            }
        }
    } else {
        for (auto &edge : orientedEdges()) {
            if (!edge.leftHalfplane().interiorContains(other)) {
                return false;
            }
        }
    }
    return true;
}

template <class PointType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool Convex<PointType>::interiorContains(const Disk<OtherPoint, OtherLabel>& other) const {
    for (auto &edge : orientedEdges()) {
        if (!edge.leftHalfplane().interiorContains(other)) {
            return false;
        }
    }
    return true;
}

template <class PointType>
template <PointConcept OtherPoint>
constexpr bool Convex<PointType>::interiorContains(const Shape<OtherPoint>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->interiorContains(value);
        },
        other.variant());
}

// ---------------------------------------------------------------------------
// Polygon

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Polygon<PointType>::interiorContains(const OtherPoint& point) const {
    // Strictly interior iff contained but not on the boundary (mirrors
    // Convex::interiorContains). A polygon with fewer than three vertices has
    // every contained point on its boundary, so this yields false there.
    return contains(point) && !boundaryContains(point);
}

template <class PointType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool Polygon<PointType>::interiorContains(const Segment<OtherPoint, OtherLabel>& other) const {
    if (size() < 3) {
        return false;
    }
    // Both endpoints strictly inside, plus no boundary contact anywhere: a
    // segment that grazes or crosses the boundary (e.g. through a reflex notch)
    // is rejected even when both endpoints are interior.
    if (!interiorContains(other.min()) || !interiorContains(other.max())) {
        return false;
    }
    for (const auto& edge : edges()) {
        if (other.intersects(edge)) {
            return false;
        }
    }
    return true;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Polygon<PointType>::interiorContains(const OrientedSegment<OtherPoint>& other) const {
    return interiorContains(Segment<OtherPoint>(other.source(), other.target()));
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Polygon<PointType>::interiorContains(const Line<OtherPoint>& other) const {
    return other.isDegenerate() && interiorContains(other.min());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Polygon<PointType>::interiorContains(const OrientedLine<OtherPoint>& other) const {
    return other.isDegenerate() && interiorContains(other.source());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Polygon<PointType>::interiorContains(const Ray<OtherPoint>& other) const {
    return other.isDegenerate() && interiorContains(other.source());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Polygon<PointType>::interiorContains(const Halfplane<OtherPoint>& other) const {
    return other.isDegenerate() && interiorContains(other.source());
}

// For a simple polygon (no holes) the region overloads reduce to an
// edge-by-edge interior check, mirroring the contains() overloads.
template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Polygon<PointType>::interiorContains(const Rectangle<OtherPoint>& other) const {
    for (std::size_t i = 0; i < other.size(); ++i) {
        if (!interiorContains(Segment<OtherPoint>(other[i], other[(i + 1) % other.size()]))) {
            return false;
        }
    }
    return true;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Polygon<PointType>::interiorContains(const Triangle<OtherPoint>& other) const {
    for (std::size_t i = 0; i < other.size(); ++i) {
        if (!interiorContains(Segment<OtherPoint>(other[i], other[(i + 1) % other.size()]))) {
            return false;
        }
    }
    return true;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Polygon<PointType>::interiorContains(const Convex<OtherPoint>& other) const {
    if (other.size() == 0) {
        return true;
    }
    if (other.size() == 1) {
        return interiorContains(other[0]);
    }
    for (std::size_t i = 0; i < other.size(); ++i) {
        if (!interiorContains(Segment<OtherPoint>(other[i], other[(i + 1) % other.size()]))) {
            return false;
        }
    }
    return true;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Polygon<PointType>::interiorContains(const Polygon<OtherPoint>& other) const {
    if (other.size() == 0) {
        return true;
    }
    if (other.size() == 1) {
        return interiorContains(other[0]);
    }
    for (std::size_t i = 0; i < other.size(); ++i) {
        if (!interiorContains(Segment<OtherPoint>(other[i], other[(i + 1) % other.size()]))) {
            return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// Disk

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Disk<PointType, LabelType>::interiorContains(const OtherPoint& point) const {
    return inCircleSign(a(), b(), c(), point) == std::partial_ordering::greater;
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool Disk<PointType, LabelType>::interiorContains(const Segment<OtherPoint, OtherLabel>& other) const {
    return interiorContains(other.min()) && interiorContains(other.max());
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Disk<PointType, LabelType>::interiorContains(const OrientedSegment<OtherPoint>& other) const {
    return interiorContains(other.source()) && interiorContains(other.target());
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Disk<PointType, LabelType>::interiorContains(const Line<OtherPoint>&) const {
    return false;
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Disk<PointType, LabelType>::interiorContains(const OrientedLine<OtherPoint>&) const {
    return false;
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Disk<PointType, LabelType>::interiorContains(const Ray<OtherPoint>&) const {
    return false;
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Disk<PointType, LabelType>::interiorContains(const Halfplane<OtherPoint>&) const {
    return false;
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Disk<PointType, LabelType>::interiorContains(const Triangle<OtherPoint>& other) const {
    return interiorContains(other.a()) && interiorContains(other.b()) && interiorContains(other.c());
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Disk<PointType, LabelType>::interiorContains(const Rectangle<OtherPoint>& other) const {
    const auto vertices = other.vertices();
    return interiorContains(vertices[0]) && interiorContains(vertices[1]) &&
           interiorContains(vertices[2]) && interiorContains(vertices[3]);
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Disk<PointType, LabelType>::interiorContains(const Convex<OtherPoint>& other) const {
    for (const auto& point : other) {
        if (!interiorContains(point)) {
            return false;
        }
    }
    return true;
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool Disk<PointType, LabelType>::interiorContains(const Disk<OtherPoint, OtherLabel>& other) const {
    using R = detail::promoted_number_t<std::common_type_t<
        decltype(squaredRadius()),
        decltype(other.squaredRadius())>>;

    const R r1_sq = squaredRadius<R>();
    const R r2_sq = other.template squaredRadius<R>();
    if (r1_sq <= r2_sq) {
        return false;
    }

    const R d2 = center<R>().squaredDistance(other.template center<R>());
    const R A = d2 - r1_sq - r2_sq;
    return A < R{} && A * A > R{4} * r1_sq * r2_sq;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Convex<PointType>::interiorContains(const Polygon<OtherPoint>& other) const {
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


// --- asymmetric not-yet-implemented stubs ---

template <class PointType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool OrientedSegment<PointType>::interiorContains(const Disk<OtherPoint, OtherLabel>&) const {
    throw std::runtime_error(
        "pgl: OrientedSegment::interiorContains(Disk) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedSegment<PointType>::interiorContains(const Polygon<OtherPoint>&) const {
    throw std::runtime_error(
        "pgl: OrientedSegment::interiorContains(Polygon) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
}

template <class PointType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool Line<PointType>::interiorContains(const Disk<OtherPoint, OtherLabel>&) const {
    throw std::runtime_error(
        "pgl: Line::interiorContains(Disk) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Line<PointType>::interiorContains(const Polygon<OtherPoint>&) const {
    throw std::runtime_error(
        "pgl: Line::interiorContains(Polygon) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
}

template <class PointType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool OrientedLine<PointType>::interiorContains(const Disk<OtherPoint, OtherLabel>&) const {
    throw std::runtime_error(
        "pgl: OrientedLine::interiorContains(Disk) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedLine<PointType>::interiorContains(const Polygon<OtherPoint>&) const {
    throw std::runtime_error(
        "pgl: OrientedLine::interiorContains(Polygon) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
}

template <class PointType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool Ray<PointType>::interiorContains(const Disk<OtherPoint, OtherLabel>&) const {
    throw std::runtime_error(
        "pgl: Ray::interiorContains(Disk) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Ray<PointType>::interiorContains(const Polygon<OtherPoint>&) const {
    throw std::runtime_error(
        "pgl: Ray::interiorContains(Polygon) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Halfplane<PointType>::interiorContains(const Polygon<OtherPoint>&) const {
    throw std::runtime_error(
        "pgl: Halfplane::interiorContains(Polygon) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
}

template <class PointType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool Rectangle<PointType>::interiorContains(const Disk<OtherPoint, OtherLabel>&) const {
    throw std::runtime_error(
        "pgl: Rectangle::interiorContains(Disk) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Rectangle<PointType>::interiorContains(const Polygon<OtherPoint>&) const {
    throw std::runtime_error(
        "pgl: Rectangle::interiorContains(Polygon) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
}

template <class PointType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool Triangle<PointType>::interiorContains(const Disk<OtherPoint, OtherLabel>&) const {
    throw std::runtime_error(
        "pgl: Triangle::interiorContains(Disk) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Triangle<PointType>::interiorContains(const Polygon<OtherPoint>&) const {
    throw std::runtime_error(
        "pgl: Triangle::interiorContains(Polygon) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Disk<PointType, LabelType>::interiorContains(const Polygon<OtherPoint>&) const {
    throw std::runtime_error(
        "pgl: Disk::interiorContains(Polygon) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
}

template <class PointType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool Polygon<PointType>::interiorContains(const Disk<OtherPoint, OtherLabel>&) const {
    throw std::runtime_error(
        "pgl: Polygon::interiorContains(Disk) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
}

}  // namespace pgl
