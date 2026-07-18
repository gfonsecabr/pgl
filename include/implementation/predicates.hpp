#pragma once

#include "implementation/separates.hpp"

/**
 * @file predicates.hpp
 * @brief Method definitions for the shapes.
 **/

#include <limits>
#include "predicates_helpers.hpp"
#include "boundarycontains.hpp"
#include "contains.hpp"
#include "crosses.hpp"
#include "interiorcontains.hpp"
#include "interiorsintersect.hpp"
#include "intersects.hpp"
#include "separates.hpp"

namespace pgl {

/**
 * @section predicates-point Point
 * Point equality and the point-vs-shape predicates. This section also contains
 * the cases where removing a point disconnects a 1D primitive.
 */

template <class Number, class Label>
template<PointConcept OtherPoint>
constexpr bool Point<Number, Label>::operator==(const OtherPoint& other) const {
    using Compare = std::common_type_t<Number, typename OtherPoint::NumberType>;
    return static_cast<Compare>(x()) == static_cast<Compare>(other.x()) &&
           static_cast<Compare>(y()) == static_cast<Compare>(other.y());
}

template <class Number, class Label>
template<PointConcept OtherPoint>
constexpr std::strong_ordering Point<Number, Label>::operator<=>(const OtherPoint& other) const {
    using Compare = std::common_type_t<Number, typename OtherPoint::NumberType>;
    if (auto cmp = detail::strongOrder(static_cast<Compare>(x()), static_cast<Compare>(other.x())); cmp != 0) {
        return cmp;
    }
    return detail::strongOrder(static_cast<Compare>(y()), static_cast<Compare>(other.y()));
}

/**
 * @section predicates-segment Segment
 * Segment endpoint, boundary, containment, collinearity, intersection, and
 * topological predicates, including the generic `separates` / `crosses`
 * dispatch used against 1D and area targets.
 */

template <class PointType, class LabelType>
constexpr bool Segment<PointType, LabelType>::isDegenerate() const {
    return min() == max();
}

template <class PointType, class LabelType>
constexpr bool Segment<PointType, LabelType>::isPoint() const {
    return min() == max();
}

template <class PointType, class LabelType>
constexpr std::optional<PointType> Segment<PointType, LabelType>::getIfPoint() const {
    if (!isPoint()) {
        return std::nullopt;
    }
    return min();
}

template <class PointType, class LabelType>
constexpr bool Segment<PointType, LabelType>::isUndefined() const {
    return false;
}

template <class PointType, class LabelType>
constexpr bool Segment<PointType, LabelType>::isVertical() const {
    return min().x() == max().x();
}

template <class PointType, class LabelType>
constexpr bool Segment<PointType, LabelType>::isHorizontal() const {
    return min().y() == max().y();
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Segment<PointType, LabelType>::boundingBoxesOverlap(const OtherSegment& other) const {
    using Compare = std::common_type_t<NumberType, typename OtherSegment::NumberType>;
    const auto& a = min();
    const auto& b = max();
    const auto& c = other.min();
    const auto& d = other.max();
    const Compare thisMinX = static_cast<Compare>(a.x());
    const Compare thisMaxX = static_cast<Compare>(b.x());
    const Compare otherMinX = static_cast<Compare>(c.x());
    const Compare otherMaxX = static_cast<Compare>(d.x());
    if (thisMaxX < otherMinX || otherMaxX < thisMinX) {
        return false;
    }
    const Compare thisY1 = static_cast<Compare>(a.y());
    const Compare thisY2 = static_cast<Compare>(b.y());
    const Compare otherY1 = static_cast<Compare>(c.y());
    const Compare otherY2 = static_cast<Compare>(d.y());
    const Compare thisMinY = thisY1 < thisY2 ? thisY1 : thisY2;
    const Compare thisMaxY = thisY1 < thisY2 ? thisY2 : thisY1;
    const Compare otherMinY = otherY1 < otherY2 ? otherY1 : otherY2;
    const Compare otherMaxY = otherY1 < otherY2 ? otherY2 : otherY1;
    return !(thisMaxY < otherMinY || otherMaxY < thisMinY);
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr int Segment<PointType, LabelType>::boundingBoxesCross(const OtherSegment& other) const {
    using Compare = std::common_type_t<NumberType, typename OtherSegment::NumberType>;
    const auto& a = min();
    const auto& b = max();
    const auto& c = other.min();
    const auto& d = other.max();
    const Compare thisMinX = static_cast<Compare>(a.x());
    const Compare thisMaxX = static_cast<Compare>(b.x());
    const Compare otherMinX = static_cast<Compare>(c.x());
    const Compare otherMaxX = static_cast<Compare>(d.x());
    if (thisMaxX < otherMinX || otherMaxX < thisMinX) {
        return 0;
    }
    const Compare thisY1 = static_cast<Compare>(a.y());
    const Compare thisY2 = static_cast<Compare>(b.y());
    const Compare otherY1 = static_cast<Compare>(c.y());
    const Compare otherY2 = static_cast<Compare>(d.y());
    const Compare thisMinY = thisY1 < thisY2 ? thisY1 : thisY2;
    const Compare thisMaxY = thisY1 < thisY2 ? thisY2 : thisY1;
    const Compare otherMinY = otherY1 < otherY2 ? otherY1 : otherY2;
    const Compare otherMaxY = otherY1 < otherY2 ? otherY2 : otherY1;
    if (thisMaxY < otherMinY || otherMaxY < thisMinY) {
        return 0;
    }
    if (thisMinX < otherMinX && otherMaxX < thisMaxX && otherMinY < thisMinY && thisMaxY < otherMaxY ) {
        return 2;
    }
    if (otherMinX < thisMinX && thisMaxX < otherMaxX && thisMinY < otherMinY && otherMaxY < thisMaxY ) {
        return 2;
    }
    return 1;
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Segment<PointType, LabelType>::verticesContain(const OtherPoint& point) const {
    return point == min() || point == max();
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Segment<PointType, LabelType>::containsEndpoint(const OtherPoint& point) const {
    return verticesContain(point);
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Segment<PointType, LabelType>::containsCollinear(const OtherPoint& point) const {
    return !(point < min() || max() < point);
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Segment<PointType, LabelType>::collinear(const OtherPoint& point) const {
    if (isDegenerate()) {
        return point == min();
    }
    return pgl::collinear(min(), max(), point);
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Segment<PointType, LabelType>::collinear(const OtherSegment& other) const {
    return collinear(other.min()) && collinear(other.max());
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Segment<PointType, LabelType>::parallel(const OtherSegment& other) const {
    return sameDirection(min(), max(), other.min(), other.max());
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Segment<PointType, LabelType>::collinear(const OtherOrientedSegment& other) const {
    return collinear(other.source()) && collinear(other.target());
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Segment<PointType, LabelType>::collinear(const OtherLine& other) const {
    return collinear(other.min()) && collinear(other.max());
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Segment<PointType, LabelType>::collinear(const OtherOrientedLine& other) const {
    return collinear(other.source()) && collinear(other.target());
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool Segment<PointType, LabelType>::collinear(const OtherRay& other) const {
    return collinear(other.source()) && collinear(other.target());
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Segment<PointType, LabelType>::parallel(const OtherOrientedSegment& other) const {
    return sameDirection(min(), max(), other.source(), other.target());
}

/**
 * @section predicates-triangle Triangle
 * Triangle boundary, containment, intersection, and cut predicates, including
 * triangle-vs-rectangle and triangle-vs-triangle topological cases.
 */

template <class PointType, class LabelType>
constexpr bool Triangle<PointType, LabelType>::isDegenerate() const {
    return twiceArea() == decltype(twiceArea()){};
}

template <class PointType, class LabelType>
constexpr bool Triangle<PointType, LabelType>::isPoint() const {
    return a() == b() && b() == c();
}

template <class PointType, class LabelType>
constexpr std::optional<PointType> Triangle<PointType, LabelType>::getIfPoint() const {
    if (!isPoint()) {
        return std::nullopt;
    }
    return a();
}

template <class PointType, class LabelType>
constexpr bool Triangle<PointType, LabelType>::isSegment() const {
    return isDegenerate() && !isPoint();
}

template <class PointType, class LabelType>
constexpr std::optional<typename Triangle<PointType, LabelType>::template BoundaryType<false>>
Triangle<PointType, LabelType>::getIfSegment() const {
    if (!isSegment()) {
        return std::nullopt;
    }
    // The vertices are collinear, so the lexicographic extremes span them all.
    return BoundaryType<false>(std::min({a(), b(), c()}), std::max({a(), b(), c()}));
}

template <class PointType, class LabelType>
constexpr bool Triangle<PointType, LabelType>::isUndefined() const {
    return false;
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Triangle<PointType, LabelType>::verticesContain(const OtherPoint& point) const {
    return a().contains(point) || b().contains(point) || c().contains(point);
}

/**
 * @section predicates-oriented-segment OrientedSegment
 * Oriented-segment predicates. Most topology delegates to the unoriented
 * segment view, with local methods kept for orientation-sensitive behavior.
 */

template <class PointType, class LabelType>
constexpr bool OrientedSegment<PointType, LabelType>::isDegenerate() const {
    return source() == target();
}

template <class PointType, class LabelType>
constexpr bool OrientedSegment<PointType, LabelType>::isPoint() const {
    return source() == target();
}

template <class PointType, class LabelType>
constexpr std::optional<PointType> OrientedSegment<PointType, LabelType>::getIfPoint() const {
    if (!isPoint()) {
        return std::nullopt;
    }
    return source();
}

template <class PointType, class LabelType>
constexpr bool OrientedSegment<PointType, LabelType>::isUndefined() const {
    return false;
}

template <class PointType, class LabelType>
constexpr bool OrientedSegment<PointType, LabelType>::isVertical() const {
    return source().x() == target().x();
}

template <class PointType, class LabelType>
constexpr bool OrientedSegment<PointType, LabelType>::isHorizontal() const {
    return source().y() == target().y();
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool OrientedSegment<PointType, LabelType>::verticesContain(const OtherPoint& point) const {
    return point == source() || point == target();
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool OrientedSegment<PointType, LabelType>::containsEndpoint(const OtherPoint& point) const {
    return verticesContain(point);
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool OrientedSegment<PointType, LabelType>::containsCollinear(const OtherPoint& point) const {
    return !(point < min() || max() < point);
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool OrientedSegment<PointType, LabelType>::collinear(const OtherPoint& point) const {
    if (isDegenerate()) {
        return point == source();
    }
    return pgl::collinear(source(), target(), point);
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool OrientedSegment<PointType, LabelType>::collinear(const OtherSegment& other) const {
    return collinear(other.min()) && collinear(other.max());
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool OrientedSegment<PointType, LabelType>::collinear(const OtherOrientedSegment& other) const {
    return collinear(other.source()) && collinear(other.target());
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool OrientedSegment<PointType, LabelType>::collinear(const OtherLine& other) const {
    return collinear(other.min()) && collinear(other.max());
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool OrientedSegment<PointType, LabelType>::collinear(const OtherOrientedLine& other) const {
    return collinear(other.source()) && collinear(other.target());
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool OrientedSegment<PointType, LabelType>::collinear(const OtherRay& other) const {
    return collinear(other.source()) && collinear(other.target());
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr std::partial_ordering OrientedSegment<PointType, LabelType>::orientation(const OtherPoint& point) const {
    return orientationSign(source(), target(), point);
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool OrientedSegment<PointType, LabelType>::parallel(const OtherSegment& other) const {
    return static_cast<Segment<PointType>>(*this).parallel(other);
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool OrientedSegment<PointType, LabelType>::parallel(const OtherOrientedSegment& other) const {
    return static_cast<Segment<PointType>>(*this).parallel(static_cast<Segment<typename OtherOrientedSegment::PointType>>(other));
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool OrientedSegment<PointType, LabelType>::parallel(const OtherLine& other) const {
    return other.parallel(*this);
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool OrientedSegment<PointType, LabelType>::parallel(const OtherOrientedLine& other) const {
    return other.parallel(*this);
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool OrientedSegment<PointType, LabelType>::parallel(const OtherRay& other) const {
    return other.parallel(*this);
}

template <class PointType, class LabelType>
constexpr Halfplane<PointType> OrientedSegment<PointType, LabelType>::rightHalfplane() const {
    return Halfplane<PointType>(target(), source());
}

template <class PointType, class LabelType>
constexpr Halfplane<PointType> OrientedSegment<PointType, LabelType>::leftHalfplane() const {
    return Halfplane<PointType>(source(), target());
}

/**
 * @section predicates-line Line
 * Geometric line predicates: geometric equality/order, containment,
 * intersection against 1D and 2D shapes, and generic separation dispatch.
 */

template <class PointType, class LabelType>
constexpr bool Line<PointType, LabelType>::operator==(const Line& other) const {
    return contains(other);
}

template <class PointType, class LabelType>
constexpr auto Line<PointType, LabelType>::operator<=>(const Line& other) const {
    // Vertical lines have no dual point, so they sort before every
    // non-vertical line and among themselves by their x-coordinate.
    if (isVertical() || other.isVertical()) {
        if (!other.isVertical()) {
            return std::strong_ordering::less;
        }
        if (!isVertical()) {
            return std::strong_ordering::greater;
        }
        return detail::strongOrder(min().x(), other.min().x());
    }
    using otherNumber = std::remove_cvref_t<decltype(other.min().x())>;
    using Coordinate = detail::promoted_number_t<std::common_type_t<NumberType, otherNumber>>;
    const auto [anum, bnum, den] = dualCoordinates<Coordinate>();
    const auto [other_anum, other_bnum, other_den] = other.template dualCoordinates<Coordinate>();
    if (auto cmp = detail::strongOrder(anum * other_den, other_anum * den); cmp != 0) {
        return cmp;
    }
    return detail::strongOrder(bnum * other_den, other_bnum * den);
}

template <class PointType, class LabelType>
constexpr bool Line<PointType, LabelType>::isDegenerate() const {
    return min() == max();
}

template <class PointType, class LabelType>
constexpr bool Line<PointType, LabelType>::isVertical() const {
    return min().x() == max().x();
}

template <class PointType, class LabelType>
constexpr bool Line<PointType, LabelType>::isHorizontal() const {
    return min().y() == max().y();
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Segment<PointType, LabelType>::parallel(const OtherLine& other) const {
    return other.parallel(*this);
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Line<PointType, LabelType>::verticesContain(const OtherPoint& point) const {
    return point == min() || point == max();
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Line<PointType, LabelType>::collinear(const OtherPoint& point) const {
    return contains(point);
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Line<PointType, LabelType>::collinear(const OtherSegment& other) const {
    return contains(other);
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Line<PointType, LabelType>::collinear(const OtherOrientedSegment& other) const {
    return contains(other);
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Line<PointType, LabelType>::collinear(const OtherLine& other) const {
    return contains(other);
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Line<PointType, LabelType>::parallel(const OtherLine& other) const {
    return sameDirection(min(), max(), other.min(), other.max());
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Line<PointType, LabelType>::parallel(const OtherSegment& other) const {
    return sameDirection(min(), max(), other.min(), other.max());
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Line<PointType, LabelType>::parallel(const OtherOrientedSegment& other) const {
    return sameDirection(min(), max(), other.source(), other.target());
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Line<PointType, LabelType>::parallel(const OtherOrientedLine& other) const {
    return parallel(other.asLine());
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool Line<PointType, LabelType>::parallel(const OtherRay& other) const {
    return parallel(other.asLine());
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Line<PointType, LabelType>::collinear(const OtherOrientedLine& other) const {
    return collinear(other.asLine());
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool Line<PointType, LabelType>::collinear(const OtherRay& other) const {
    return collinear(other.asLine());
}

template <class PointType, class LabelType>
constexpr Halfplane<PointType> Line<PointType, LabelType>::halfplaneAbove() const {
    return Halfplane<PointType>(min(), max());
}

template <class PointType, class LabelType>
constexpr Halfplane<PointType> Line<PointType, LabelType>::halfplaneBelow() const {
    return Halfplane<PointType>(max(), min());
}

/**
 * @section predicates-oriented-line OrientedLine
 * Oriented-line predicates. Shared topology is mostly delegated to the
 * unoriented line view, while orientation-specific methods stay local here.
 */

template <class PointType, class LabelType>
constexpr bool OrientedLine<PointType, LabelType>::operator==(const OrientedLine& other) const {
    return contains(other) && ((source() <=> target()) == (other.source() <=> other.target()));
}

template <class PointType, class LabelType>
constexpr auto OrientedLine<PointType, LabelType>::operator<=>(const OrientedLine& other) const {
    // Order by orientation first (ascending before descending), then by the
    // underlying line, so opposite orientations of the same line are ordered
    // consistently from both sides.
    const bool ascending = source() < target();
    const bool otherAscending = other.source() < other.target();
    if (ascending != otherAscending) {
        return ascending ? std::strong_ordering::less : std::strong_ordering::greater;
    }
    return this->asLine() <=> other.asLine();
}

template <class PointType, class LabelType>
constexpr bool OrientedLine<PointType, LabelType>::isDegenerate() const {
    return source() == target();
}

template <class PointType, class LabelType>
constexpr bool OrientedLine<PointType, LabelType>::isVertical() const {
    return source().x() == target().x();
}

template <class PointType, class LabelType>
constexpr bool OrientedLine<PointType, LabelType>::isHorizontal() const {
    return source().y() == target().y();
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Segment<PointType, LabelType>::parallel(const OtherOrientedLine& other) const {
    return other.parallel(*this);
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool OrientedLine<PointType, LabelType>::verticesContain(const OtherPoint& point) const {
    return point == source() || point == target();
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool OrientedLine<PointType, LabelType>::collinear(const OtherPoint& point) const {
    return contains(point);
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool OrientedLine<PointType, LabelType>::collinear(const OtherLine& other) const {
    return contains(other);
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool OrientedLine<PointType, LabelType>::collinear(const OtherOrientedLine& other) const {
    return contains(other);
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool OrientedLine<PointType, LabelType>::collinear(const OtherSegment& other) const {
    return contains(other);
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool OrientedLine<PointType, LabelType>::collinear(const OtherOrientedSegment& other) const {
    return contains(other);
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool OrientedLine<PointType, LabelType>::collinear(const OtherRay& other) const {
    return collinear(other.asLine());
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr std::partial_ordering OrientedLine<PointType, LabelType>::orientation(const OtherPoint& point) const {
    return orientationSign(source(), target(), point);
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool OrientedLine<PointType, LabelType>::parallel(const OtherLine& other) const {
    return sameDirection(source(), target(), other.min(), other.max());
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool OrientedLine<PointType, LabelType>::parallel(const OtherOrientedLine& other) const {
    return sameDirection(source(), target(), other.source(), other.target());
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool OrientedLine<PointType, LabelType>::parallel(const OtherSegment& other) const {
    return sameDirection(source(), target(), other.min(), other.max());
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool OrientedLine<PointType, LabelType>::parallel(const OtherOrientedSegment& other) const {
    return sameDirection(source(), target(), other.source(), other.target());
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool OrientedLine<PointType, LabelType>::parallel(const OtherRay& other) const {
    return other.parallel(*this);
}

template <class PointType, class LabelType>
constexpr Halfplane<PointType> OrientedLine<PointType, LabelType>::halfplaneAbove() const {
    return Halfplane<PointType>(min(), max());
}

template <class PointType, class LabelType>
constexpr Halfplane<PointType> OrientedLine<PointType, LabelType>::halfplaneBelow() const {
    return Halfplane<PointType>(max(), min());
}

template <class PointType, class LabelType>
constexpr Halfplane<PointType> OrientedLine<PointType, LabelType>::rightHalfplane() const {
    return Halfplane<PointType>(target(), source());
}

template <class PointType, class LabelType>
constexpr Halfplane<PointType> OrientedLine<PointType, LabelType>::leftHalfplane() const {
    return Halfplane<PointType>(source(), target());
}

/**
 * @section predicates-ray Ray
 * Ray-specific containment, intersection, and topological predicates. This is
 * where the asymmetric behavior of a half-infinite 1D primitive is implemented.
 */

template <class PointType, class LabelType>
constexpr bool Ray<PointType, LabelType>::operator==(const Ray& other) const {
    return source() == other.source() && contains(other);
}

template <class PointType, class LabelType>
constexpr auto Ray<PointType, LabelType>::operator<=>(const Ray& other) const {
    if (source() != other.source()) {
        return source() <=> other.source();
    }
    using otherPointType = std::remove_cvref_t<decltype(other.source())>;
    return static_cast<OrientedLine<PointType>>(*this) <=> static_cast<OrientedLine<otherPointType>>(other);
}

template <class PointType, class LabelType>
constexpr bool Ray<PointType, LabelType>::isDegenerate() const {
    return source() == target();
}

template <class PointType, class LabelType>
constexpr bool Ray<PointType, LabelType>::isVertical() const {
    return source().x() == target().x();
}

template <class PointType, class LabelType>
constexpr bool Ray<PointType, LabelType>::isHorizontal() const {
    return source().y() == target().y();
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Ray<PointType, LabelType>::verticesContain(const OtherPoint& point) const {
    return point == source() || point == target();
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Ray<PointType, LabelType>::containsCollinear(const OtherPoint& point) const {
    if (isDegenerate()) {
        return point == source();
    }
    if (source() < target()) {
        return !(point < source());
    }
    return !(source() < point);
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Ray<PointType, LabelType>::collinear(const OtherPoint& point) const {
    return this->asLine().contains(point);
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Ray<PointType, LabelType>::collinear(const OtherLine& other) const {
    return this->asLine().contains(other);
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Ray<PointType, LabelType>::collinear(const OtherOrientedLine& other) const {
    return this->asLine().contains(other.asLine());
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Ray<PointType, LabelType>::collinear(const OtherSegment& other) const {
    return this->asLine().contains(other);
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Ray<PointType, LabelType>::collinear(const OtherOrientedSegment& other) const {
    return this->asLine().contains(other);
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool Ray<PointType, LabelType>::collinear(const OtherRay& other) const {
    return this->asLine().contains(other.asLine());
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr std::partial_ordering Ray<PointType, LabelType>::orientation(const OtherPoint& point) const {
    return orientationSign(source(), target(), point);
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Ray<PointType, LabelType>::parallel(const OtherLine& other) const {
    return sameDirection(source(), target(), other.min(), other.max());
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Ray<PointType, LabelType>::parallel(const OtherOrientedLine& other) const {
    return sameDirection(source(), target(), other.source(), other.target());
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Ray<PointType, LabelType>::parallel(const OtherSegment& other) const {
    return sameDirection(source(), target(), other.min(), other.max());
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Ray<PointType, LabelType>::parallel(const OtherOrientedSegment& other) const {
    return sameDirection(source(), target(), other.source(), other.target());
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool Ray<PointType, LabelType>::parallel(const OtherRay& other) const {
    return sameDirection(source(), target(), other.source(), other.target());
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool Segment<PointType, LabelType>::parallel(const OtherRay& other) const {
    return other.parallel(*this);
}

template <class PointType, class LabelType>
constexpr Halfplane<PointType> Ray<PointType, LabelType>::halfplaneAbove() const {
    return Halfplane<PointType>(min(), max());
}

template <class PointType, class LabelType>
constexpr Halfplane<PointType> Ray<PointType, LabelType>::halfplaneBelow() const {
    return Halfplane<PointType>(max(), min());
}

template <class PointType, class LabelType>
constexpr Halfplane<PointType> Ray<PointType, LabelType>::rightHalfplane() const {
    return Halfplane<PointType>(target(), source());
}

template <class PointType, class LabelType>
constexpr Halfplane<PointType> Ray<PointType, LabelType>::leftHalfplane() const {
    return Halfplane<PointType>(source(), target());
}

/**
 * @section predicates-rectangle Rectangle
 * Axis-aligned rectangle predicates plus the rectangle-local clipping helpers
 * used to answer strict interior and separation questions.
 */

template <class PointType, class LabelType>
constexpr bool Rectangle<PointType, LabelType>::isDegenerate() const {
    return !(min().x() < max().x()) || !(min().y() < max().y());
}

template <class PointType, class LabelType>
constexpr bool Rectangle<PointType, LabelType>::isPoint() const {
    return min() == max();
}

template <class PointType, class LabelType>
constexpr std::optional<PointType> Rectangle<PointType, LabelType>::getIfPoint() const {
    if (!isPoint()) {
        return std::nullopt;
    }
    return min();
}

template <class PointType, class LabelType>
constexpr bool Rectangle<PointType, LabelType>::isSegment() const {
    return isDegenerate() && !isPoint();
}

template <class PointType, class LabelType>
constexpr std::optional<typename Rectangle<PointType, LabelType>::template BoundaryType<false>>
Rectangle<PointType, LabelType>::getIfSegment() const {
    if (!isSegment()) {
        return std::nullopt;
    }
    return BoundaryType<false>(min(), max());
}

template <class PointType, class LabelType>
constexpr bool Rectangle<PointType, LabelType>::isUndefined() const {
    return false;
}


template <class PointType, class LabelType>
template <class Left, class Right>
constexpr bool Rectangle<PointType, LabelType>::intervalsOverlap(const Left& first_min, const Left& first_max, const Right& second_min, const Right& second_max) {
    return !(first_max < second_min) && !(second_max < first_min);
}

template <class PointType, class LabelType>
template <class Left, class Right>
constexpr bool Rectangle<PointType, LabelType>::intervalsOverlapStrict(const Left& first_min, const Left& first_max, const Right& second_min, const Right& second_max) {
    return first_min < second_max && second_min < first_max;
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Rectangle<PointType, LabelType>::verticesContain(const OtherPoint& point) const {
    return point == min() ||
           point == bottomRight() ||
           point == max() ||
           point == topLeft();
}

/**
 * @section predicates-halfplane Halfplane
 * Half-plane containment, intersection, and topological predicates, together
 * with the helper routines used for strict side/interior tests.
 */


template <class PointType, class LabelType>
constexpr bool Halfplane<PointType, LabelType>::operator==(const Halfplane& other) const {
    using otherPointType = std::remove_cvref_t<decltype(other.source())>;
    return static_cast<OrientedLine<PointType>>(*this) == static_cast<OrientedLine<otherPointType>>(other);
}

template <class PointType, class LabelType>
constexpr auto Halfplane<PointType, LabelType>::operator<=>(const Halfplane& other) const {
    using otherPointType = std::remove_cvref_t<decltype(other.source())>;
    return static_cast<OrientedLine<PointType>>(*this) <=> static_cast<OrientedLine<otherPointType>>(other);
}

template <class PointType, class LabelType>
constexpr bool Halfplane<PointType, LabelType>::isDegenerate() const {
    return source() == target();
}

template <class PointType, class LabelType>
constexpr bool Halfplane<PointType, LabelType>::isVertical() const {
    return source().x() == target().x();
}

template <class PointType, class LabelType>
constexpr bool Halfplane<PointType, LabelType>::isHorizontal() const {
    return source().y() == target().y();
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Halfplane<PointType, LabelType>::verticesContain(const OtherPoint& point) const {
    return point == source() || point == target();
}


// ---------------------------------------------------------------------------
// Convex

template <class PointType, class LabelType>
constexpr bool Convex<PointType, LabelType>::isDegenerate() const {
    return size() < 3;
}

template <class PointType, class LabelType>
constexpr bool Convex<PointType, LabelType>::isPoint() const {
    // grahamScan drops duplicates, so size() == 2 implies distinct vertices;
    // the equality test only matters for a `trusted` polygon built by hand.
    return size() == 1 || (size() == 2 && (*this)[0] == (*this)[1]);
}

template <class PointType, class LabelType>
constexpr std::optional<PointType> Convex<PointType, LabelType>::getIfPoint() const {
    if (!isPoint()) {
        return std::nullopt;
    }
    return (*this)[0];
}

template <class PointType, class LabelType>
constexpr bool Convex<PointType, LabelType>::isSegment() const {
    return size() == 2 && (*this)[0] != (*this)[1];
}

template <class PointType, class LabelType>
constexpr std::optional<typename Convex<PointType, LabelType>::template BoundaryType<false>>
Convex<PointType, LabelType>::getIfSegment() const {
    if (!isSegment()) {
        return std::nullopt;
    }
    return BoundaryType<false>((*this)[0], (*this)[1]);
}

template <class PointType, class LabelType>
constexpr bool Convex<PointType, LabelType>::isUndefined() const {
    return size() == 0;
}

template <class PointType, class LabelType>
constexpr size_t Convex<PointType, LabelType>::maxIndex() const {
    assert(size() != 0);
    if (maxIndex_ >= 0) {
        return static_cast<size_t>(maxIndex_);
    }

    const size_t n = size();
    size_t lo = 0;
    size_t hi = n - 1;

    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;

        // Compare consecutive x values.
        // If increasing, maximum is to the right.
        if (points_[mid] < points_[mid + 1]) {
            lo = mid + 1;
        }
        else {
            hi = mid;
        }
    }

    maxIndex_ = static_cast<std::ptrdiff_t>(lo);
    return lo;
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Convex<PointType, LabelType>::verticesContain(const OtherPoint& point) const {
    if (points_.empty()) {
        return false;
    }
    using CommonNumberType = std::common_type_t<NumberType, typename OtherPoint::NumberType>;
    Point<CommonNumberType> translatedPoint = static_cast<Point<CommonNumberType>>(point) -
                                              static_cast<Point<CommonNumberType>>(translation_);

    if (points_.size() == 1) {
        return translatedPoint == points_[0];
    }
    if (points_.size() == 2) {
        return translatedPoint == points_[0] || translatedPoint == points_[1];
    }
    if (points_.size() == 3) {
        return translatedPoint == points_[0] || translatedPoint == points_[1] || translatedPoint == points_[2];
    }

    const size_t max_i = maxIndex();

    auto o = orientationSign(points_[0], points_[max_i], translatedPoint);
    if (o < 0) {
        return std::binary_search(points_.begin(), points_.begin() + max_i + 1, translatedPoint, lexLessCrossType);
    }
    if (o > 0) {
        return std::binary_search(std::make_reverse_iterator(points_.end()),
                                  std::make_reverse_iterator(points_.begin() + max_i),
                                  translatedPoint, lexLessCrossType);
    }

    return translatedPoint == points_[0] || translatedPoint == points_[max_i];
}

template <class PointType, class LabelType>
constexpr std::ptrdiff_t Convex<PointType, LabelType>::index(const PointType& point) const {
    const std::ptrdiff_t n = static_cast<std::ptrdiff_t>(points_.size());
    if (n == 0) {
        return -1;
    }

    const PointType translatedPoint = point - translation_;

    // For few vertices a linear scan already runs in O(1) and naturally yields
    // the smallest matching index.
    if (n <= 3) {
        for (std::ptrdiff_t i = 0; i < n; ++i) {
            if (points_[static_cast<std::size_t>(i)] == translatedPoint) {
                return i;
            }
        }
        return -1;
    }

    const std::size_t max_i = maxIndex();

    // The boundary splits at the lex-min vertex (index 0) and the lex-max
    // vertex (index max_i) into two chains, each monotone in lexicographic
    // order. The orientation of the query point relative to the line through
    // those two vertices selects the chain to binary-search.
    const auto o = orientationSign(points_[0], points_[max_i], translatedPoint);
    if (o < 0) {
        // Lower chain: points_[0 .. max_i], ascending lexicographically.
        const auto first = points_.begin();
        const auto last = points_.begin() + max_i + 1;
        const auto it = std::lower_bound(first, last, translatedPoint, lexLessCrossType);
        if (it != last && *it == translatedPoint) {
            return it - points_.begin();
        }
        return -1;
    }
    if (o > 0) {
        // Upper chain: points_[max_i .. n-1] viewed in reverse, which is
        // ascending lexicographically.
        const auto rfirst = std::make_reverse_iterator(points_.end());
        const auto rlast = std::make_reverse_iterator(points_.begin() + max_i);
        const auto rit = std::lower_bound(rfirst, rlast, translatedPoint, lexLessCrossType);
        if (rit != rlast && *rit == translatedPoint) {
            return (rit.base() - 1) - points_.begin();
        }
        return -1;
    }

    // o == 0: the point lies on the supporting line, so it can only be one of
    // the two shared chain endpoints. Return the smaller matching index.
    if (translatedPoint == points_[0]) {
        return 0;
    }
    if (translatedPoint == points_[max_i]) {
        return static_cast<std::ptrdiff_t>(max_i);
    }
    return -1;
}

/**
 * @section predicates-halfplane-intersection HalfplaneIntersection
 * Shape-recognition predicates: which lower- or full-dimensional shape the
 * region actually is. Together with `isEmpty`, `isPlane` and the ray case,
 * these cover every region a half-plane intersection can describe.
 */

template <class PointType, class LabelType>
constexpr bool HalfplaneIntersection<PointType, LabelType>::isHalfplane() const {
    return !isEmpty() && size() == 1;
}

template <class PointType, class LabelType>
constexpr std::optional<typename HalfplaneIntersection<PointType, LabelType>::HalfplaneType>
HalfplaneIntersection<PointType, LabelType>::getIfHalfplane() const {
    if (!isHalfplane()) {
        return std::nullopt;
    }
    return (*this)[0];
}

template <class PointType, class LabelType>
constexpr bool HalfplaneIntersection<PointType, LabelType>::isLine() const {
    // Among the degenerate regions only a line has no vertex: a point and a
    // segment are bounded (so every consecutive pair turns), and a ray turns at
    // its source.
    return !isEmpty() && isDegenerate() && vertexCount() == 0;
}

template <class PointType, class LabelType>
constexpr std::optional<Line<PointType>> HalfplaneIntersection<PointType, LabelType>::getIfLine() const {
    if (!isLine()) {
        return std::nullopt;
    }
    // Every stored constraint of a line region is bounded by that line: one
    // whose boundary differed would cut it and leave a ray or less.
    return (*this)[0].asLine();
}

template <class PointType, class LabelType>
constexpr bool HalfplaneIntersection<PointType, LabelType>::isPoint() const {
    if (isEmpty() || !isDegenerate()) {
        return false;
    }
    using ExactPoint = Point<detail::region_exact_number_t<NumberType>, typename PointType::LabelType>;
    return std::holds_alternative<ExactPoint>(detail::degenerateRegionCarrier(*this));
}

template <class PointType, class LabelType>
template <class ResultNumber>
constexpr std::optional<Point<ResultNumber, typename PointType::LabelType>>
HalfplaneIntersection<PointType, LabelType>::getIfPoint() const {
    if (!isPoint()) {
        return std::nullopt;
    }
    // Every vertex of a point region is that point.
    return this->template vertices<ResultNumber>().front();
}

template <class PointType, class LabelType>
constexpr bool HalfplaneIntersection<PointType, LabelType>::isSegment() const {
    if (isEmpty() || !isDegenerate()) {
        return false;
    }
    using ExactPoint = Point<detail::region_exact_number_t<NumberType>, typename PointType::LabelType>;
    return std::holds_alternative<Segment<ExactPoint>>(detail::degenerateRegionCarrier(*this));
}

template <class PointType, class LabelType>
template <class ResultNumber>
constexpr std::optional<Segment<Point<ResultNumber, typename PointType::LabelType>>>
HalfplaneIntersection<PointType, LabelType>::getIfSegment() const {
    if (!isSegment()) {
        return std::nullopt;
    }
    // The vertices are the (partly coincident) endpoints; the segment spans the
    // lexicographic extremes among them.
    using ResultPoint = Point<ResultNumber, typename PointType::LabelType>;
    const auto verts = this->template vertices<ResultNumber>();
    const auto [low, high] = std::ranges::minmax_element(verts);
    return Segment<ResultPoint>(*low, *high);
}

}  // namespace pgl
