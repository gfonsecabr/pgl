#pragma once

#ifndef PGL_HPP_INCLUDED
#error "Do not include this Pangolin header directly; include \"pgl.hpp\" instead."
#endif

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
template<PointConcept OtherPoint>
constexpr bool Segment<PointType, LabelType>::collinear(const OrientedSegment<OtherPoint>& other) const {
    return collinear(other.source()) && collinear(other.target());
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Segment<PointType, LabelType>::collinear(const Line<OtherPoint>& other) const {
    return collinear(other.min()) && collinear(other.max());
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Segment<PointType, LabelType>::collinear(const OrientedLine<OtherPoint>& other) const {
    return collinear(other.source()) && collinear(other.target());
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Segment<PointType, LabelType>::collinear(const Ray<OtherPoint>& other) const {
    return collinear(other.source()) && collinear(other.target());
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Segment<PointType, LabelType>::parallel(const OrientedSegment<OtherPoint>& other) const {
    return sameDirection(min(), max(), other.source(), other.target());
}

/**
 * @section predicates-triangle Triangle
 * Triangle boundary, containment, intersection, and cut predicates, including
 * triangle-vs-rectangle and triangle-vs-triangle topological cases.
 */

template <class PointType>
constexpr bool Triangle<PointType>::isDegenerate() const {
    return twiceArea() == decltype(twiceArea()){};
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Triangle<PointType>::verticesContain(const OtherPoint& point) const {
    return a().contains(point) || b().contains(point) || c().contains(point);
}

/**
 * @section predicates-oriented-segment OrientedSegment
 * Oriented-segment predicates. Most topology delegates to the unoriented
 * segment view, with local methods kept for orientation-sensitive behavior.
 */

template <class PointType>
constexpr bool OrientedSegment<PointType>::isDegenerate() const {
    return source() == target();
}

template <class PointType>
constexpr bool OrientedSegment<PointType>::isVertical() const {
    return source().x() == target().x();
}

template <class PointType>
constexpr bool OrientedSegment<PointType>::isHorizontal() const {
    return source().y() == target().y();
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedSegment<PointType>::verticesContain(const OtherPoint& point) const {
    return point == source() || point == target();
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedSegment<PointType>::containsEndpoint(const OtherPoint& point) const {
    return verticesContain(point);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedSegment<PointType>::containsCollinear(const OtherPoint& point) const {
    return !(point < min() || max() < point);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedSegment<PointType>::collinear(const OtherPoint& point) const {
    if (isDegenerate()) {
        return point == source();
    }
    return pgl::collinear(source(), target(), point);
}

template <class PointType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool OrientedSegment<PointType>::collinear(const Segment<OtherPoint, OtherLabel>& other) const {
    return collinear(other.min()) && collinear(other.max());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedSegment<PointType>::collinear(const OrientedSegment<OtherPoint>& other) const {
    return collinear(other.source()) && collinear(other.target());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedSegment<PointType>::collinear(const Line<OtherPoint>& other) const {
    return collinear(other.min()) && collinear(other.max());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedSegment<PointType>::collinear(const OrientedLine<OtherPoint>& other) const {
    return collinear(other.source()) && collinear(other.target());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedSegment<PointType>::collinear(const Ray<OtherPoint>& other) const {
    return collinear(other.source()) && collinear(other.target());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr std::partial_ordering OrientedSegment<PointType>::orientation(const OtherPoint& point) const {
    return orientationSign(source(), target(), point);
}

template <class PointType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool OrientedSegment<PointType>::parallel(const Segment<OtherPoint, OtherLabel>& other) const {
    return static_cast<Segment<PointType>>(*this).parallel(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedSegment<PointType>::parallel(const OrientedSegment<OtherPoint>& other) const {
    return static_cast<Segment<PointType>>(*this).parallel(static_cast<Segment<OtherPoint>>(other));
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedSegment<PointType>::parallel(const Line<OtherPoint>& other) const {
    return other.parallel(*this);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedSegment<PointType>::parallel(const OrientedLine<OtherPoint>& other) const {
    return other.parallel(*this);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedSegment<PointType>::parallel(const Ray<OtherPoint>& other) const {
    return other.parallel(*this);
}

template <class PointType>
constexpr Halfplane<PointType> OrientedSegment<PointType>::rightHalfplane() const {
    return Halfplane<PointType>(target(), source());
}

template <class PointType>
constexpr Halfplane<PointType> OrientedSegment<PointType>::leftHalfplane() const {
    return Halfplane<PointType>(source(), target());
}

/**
 * @section predicates-line Line
 * Geometric line predicates: geometric equality/order, containment,
 * intersection against 1D and 2D shapes, and generic separation dispatch.
 */

template <class PointType>
constexpr bool Line<PointType>::operator==(const Line& other) const {
    return contains(other);
}

template <class PointType>
constexpr auto Line<PointType>::operator<=>(const Line& other) const {
    if (isVertical()) {
        return detail::strongOrder(min().y(), max().y());
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

template <class PointType>
constexpr bool Line<PointType>::isDegenerate() const {
    return min() == max();
}

template <class PointType>
constexpr bool Line<PointType>::isVertical() const {
    return min().x() == max().x();
}

template <class PointType>
constexpr bool Line<PointType>::isHorizontal() const {
    return min().y() == max().y();
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Segment<PointType, LabelType>::parallel(const Line<OtherPoint>& other) const {
    return other.parallel(*this);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Line<PointType>::verticesContain(const OtherPoint& point) const {
    return point == min() || point == max();
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Line<PointType>::collinear(const OtherPoint& point) const {
    return contains(point);
}

template <class PointType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool Line<PointType>::collinear(const Segment<OtherPoint, OtherLabel>& other) const {
    return contains(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Line<PointType>::collinear(const OrientedSegment<OtherPoint>& other) const {
    return contains(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Line<PointType>::collinear(const Line<OtherPoint>& other) const {
    return contains(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Line<PointType>::parallel(const Line<OtherPoint>& other) const {
    return sameDirection(min(), max(), other.min(), other.max());
}

template <class PointType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool Line<PointType>::parallel(const Segment<OtherPoint, OtherLabel>& other) const {
    return sameDirection(min(), max(), other.min(), other.max());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Line<PointType>::parallel(const OrientedSegment<OtherPoint>& other) const {
    return sameDirection(min(), max(), other.source(), other.target());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Line<PointType>::parallel(const OrientedLine<OtherPoint>& other) const {
    return parallel(other.asLine());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Line<PointType>::parallel(const Ray<OtherPoint>& other) const {
    return parallel(other.asLine());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Line<PointType>::collinear(const OrientedLine<OtherPoint>& other) const {
    return collinear(other.asLine());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Line<PointType>::collinear(const Ray<OtherPoint>& other) const {
    return collinear(other.asLine());
}

template <class PointType>
constexpr Halfplane<PointType> Line<PointType>::halfplaneAbove() const {
    return Halfplane<PointType>(max(), min());
}

template <class PointType>
constexpr Halfplane<PointType> Line<PointType>::halfplaneBelow() const {
    return Halfplane<PointType>(min(), max());
}

/**
 * @section predicates-oriented-line OrientedLine
 * Oriented-line predicates. Shared topology is mostly delegated to the
 * unoriented line view, while orientation-specific methods stay local here.
 */

template <class PointType>
constexpr bool OrientedLine<PointType>::operator==(const OrientedLine& other) const {
    return contains(other) && ((source() <=> target()) == (other.source() <=> other.target()));
}

template <class PointType>
constexpr auto OrientedLine<PointType>::operator<=>(const OrientedLine& other) const {
    if (source() < target() && other.source() > other.target()) {
        return std::strong_ordering::less;
    }
    return this->asLine() <=> other.asLine();
}

template <class PointType>
constexpr bool OrientedLine<PointType>::isDegenerate() const {
    return source() == target();
}

template <class PointType>
constexpr bool OrientedLine<PointType>::isVertical() const {
    return source().x() == target().x();
}

template <class PointType>
constexpr bool OrientedLine<PointType>::isHorizontal() const {
    return source().y() == target().y();
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Segment<PointType, LabelType>::parallel(const OrientedLine<OtherPoint>& other) const {
    return other.parallel(*this);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedLine<PointType>::verticesContain(const OtherPoint& point) const {
    return point == source() || point == target();
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedLine<PointType>::collinear(const OtherPoint& point) const {
    return contains(point);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedLine<PointType>::collinear(const Line<OtherPoint>& other) const {
    return contains(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedLine<PointType>::collinear(const OrientedLine<OtherPoint>& other) const {
    return contains(other);
}

template <class PointType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool OrientedLine<PointType>::collinear(const Segment<OtherPoint, OtherLabel>& other) const {
    return contains(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedLine<PointType>::collinear(const OrientedSegment<OtherPoint>& other) const {
    return contains(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedLine<PointType>::collinear(const Ray<OtherPoint>& other) const {
    return collinear(other.asLine());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr std::partial_ordering OrientedLine<PointType>::orientation(const OtherPoint& point) const {
    return orientationSign(source(), target(), point);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedLine<PointType>::parallel(const Line<OtherPoint>& other) const {
    return sameDirection(source(), target(), other.min(), other.max());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedLine<PointType>::parallel(const OrientedLine<OtherPoint>& other) const {
    return sameDirection(source(), target(), other.source(), other.target());
}

template <class PointType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool OrientedLine<PointType>::parallel(const Segment<OtherPoint, OtherLabel>& other) const {
    return sameDirection(source(), target(), other.min(), other.max());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool OrientedLine<PointType>::parallel(const OrientedSegment<OtherPoint>& other) const {
    return sameDirection(source(), target(), other.source(), other.target());
}

template <class PointType>
constexpr Halfplane<PointType> OrientedLine<PointType>::halfplaneAbove() const {
    return Halfplane<PointType>(max(), min());
}

template <class PointType>
constexpr Halfplane<PointType> OrientedLine<PointType>::halfplaneBelow() const {
    return Halfplane<PointType>(min(), max());
}

template <class PointType>
constexpr Halfplane<PointType> OrientedLine<PointType>::rightHalfplane() const {
    return Halfplane<PointType>(target(), source());
}

template <class PointType>
constexpr Halfplane<PointType> OrientedLine<PointType>::leftHalfplane() const {
    return Halfplane<PointType>(source(), target());
}

/**
 * @section predicates-ray Ray
 * Ray-specific containment, intersection, and topological predicates. This is
 * where the asymmetric behavior of a half-infinite 1D primitive is implemented.
 */

template <class PointType>
constexpr bool Ray<PointType>::operator==(const Ray& other) const {
    return source() == other.source() && contains(other);
}

template <class PointType>
constexpr auto Ray<PointType>::operator<=>(const Ray& other) const {
    if (source() != other.source()) {
        return source() <=> other.source();
    }
    using otherPointType = std::remove_cvref_t<decltype(other.source())>;
    return static_cast<OrientedLine<PointType>>(*this) <=> static_cast<OrientedLine<otherPointType>>(other);
}

template <class PointType>
constexpr bool Ray<PointType>::isDegenerate() const {
    return source() == target();
}

template <class PointType>
constexpr bool Ray<PointType>::isVertical() const {
    return source().x() == target().x();
}

template <class PointType>
constexpr bool Ray<PointType>::isHorizontal() const {
    return source().y() == target().y();
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Ray<PointType>::verticesContain(const OtherPoint& point) const {
    return point == source() || point == target();
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Ray<PointType>::containsCollinear(const OtherPoint& point) const {
    if (isDegenerate()) {
        return point == source();
    }
    if (source() < target()) {
        return !(point < source());
    }
    return !(source() < point);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Ray<PointType>::collinear(const OtherPoint& point) const {
    return this->asLine().contains(point);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Ray<PointType>::collinear(const Line<OtherPoint>& other) const {
    return this->asLine().contains(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Ray<PointType>::collinear(const OrientedLine<OtherPoint>& other) const {
    return this->asLine().contains(other.asLine());
}

template <class PointType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool Ray<PointType>::collinear(const Segment<OtherPoint, OtherLabel>& other) const {
    return this->asLine().contains(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Ray<PointType>::collinear(const OrientedSegment<OtherPoint>& other) const {
    return this->asLine().contains(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Ray<PointType>::collinear(const Ray<OtherPoint>& other) const {
    return this->asLine().contains(other.asLine());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr std::partial_ordering Ray<PointType>::orientation(const OtherPoint& point) const {
    return orientationSign(source(), target(), point);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Ray<PointType>::parallel(const Line<OtherPoint>& other) const {
    return sameDirection(source(), target(), other.min(), other.max());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Ray<PointType>::parallel(const OrientedLine<OtherPoint>& other) const {
    return sameDirection(source(), target(), other.source(), other.target());
}

template <class PointType>
template<PointConcept OtherPoint, class OtherLabel>
constexpr bool Ray<PointType>::parallel(const Segment<OtherPoint, OtherLabel>& other) const {
    return sameDirection(source(), target(), other.min(), other.max());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Ray<PointType>::parallel(const OrientedSegment<OtherPoint>& other) const {
    return sameDirection(source(), target(), other.source(), other.target());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Ray<PointType>::parallel(const Ray<OtherPoint>& other) const {
    return sameDirection(source(), target(), other.source(), other.target());
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Segment<PointType, LabelType>::parallel(const Ray<OtherPoint>& other) const {
    return other.parallel(*this);
}

template <class PointType>
constexpr Halfplane<PointType> Ray<PointType>::halfplaneAbove() const {
    return Halfplane<PointType>(max(), min());
}

template <class PointType>
constexpr Halfplane<PointType> Ray<PointType>::halfplaneBelow() const {
    return Halfplane<PointType>(min(), max());
}

template <class PointType>
constexpr Halfplane<PointType> Ray<PointType>::rightHalfplane() const {
    return Halfplane<PointType>(target(), source());
}

template <class PointType>
constexpr Halfplane<PointType> Ray<PointType>::leftHalfplane() const {
    return Halfplane<PointType>(source(), target());
}

/**
 * @section predicates-rectangle Rectangle
 * Axis-aligned rectangle predicates plus the rectangle-local clipping helpers
 * used to answer strict interior and separation questions.
 */

template <class PointType>
constexpr bool Rectangle<PointType>::isDegenerate() const {
    return !(min().x() < max().x()) || !(min().y() < max().y());
}


template <class PointType>
template <class Left, class Right>
constexpr bool Rectangle<PointType>::intervalsOverlap(const Left& first_min, const Left& first_max, const Right& second_min, const Right& second_max) {
    return !(first_max < second_min) && !(second_max < first_min);
}

template <class PointType>
template <class Left, class Right>
constexpr bool Rectangle<PointType>::intervalsOverlapStrict(const Left& first_min, const Left& first_max, const Right& second_min, const Right& second_max) {
    return first_min < second_max && second_min < first_max;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Rectangle<PointType>::verticesContain(const OtherPoint& point) const {
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


template <class PointType>
constexpr bool Halfplane<PointType>::operator==(const Halfplane& other) const {
    using otherPointType = std::remove_cvref_t<decltype(other.source())>;
    return static_cast<OrientedLine<PointType>>(*this) == static_cast<OrientedLine<otherPointType>>(other);
}

template <class PointType>
constexpr auto Halfplane<PointType>::operator<=>(const Halfplane& other) const {
    using otherPointType = std::remove_cvref_t<decltype(other.source())>;
    return static_cast<OrientedLine<PointType>>(*this) <=> static_cast<OrientedLine<otherPointType>>(other);
}

template <class PointType>
constexpr bool Halfplane<PointType>::isDegenerate() const {
    return source() == target();
}

template <class PointType>
constexpr bool Halfplane<PointType>::isVertical() const {
    return source().x() == target().x();
}

template <class PointType>
constexpr bool Halfplane<PointType>::isHorizontal() const {
    return source().y() == target().y();
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Halfplane<PointType>::verticesContain(const OtherPoint& point) const {
    return point == source() || point == target();
}


// ---------------------------------------------------------------------------
// Convex

template <class PointType>
constexpr bool Convex<PointType>::isDegenerate() const {
    return size() < 3;
}

template <class PointType>
constexpr size_t Convex<PointType>::maxIndex() const {
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

template <class PointType>
template<PointConcept OtherPoint>
constexpr bool Convex<PointType>::verticesContain(const OtherPoint& point) const {
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

template <class PointType>
constexpr std::ptrdiff_t Convex<PointType>::index(const PointType& point) const {
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

}  // namespace pgl
