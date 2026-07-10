#pragma once

#include "implementation/contains.hpp"

/**
 * @file crosses.hpp
 * @brief Implementations of the 'crosses' predicate.
 **/

#include <limits>
#include <stdexcept>
#include "predicates_helpers.hpp"


namespace pgl {

/**
 * @section predicates-point Point
 * Point equality and the point-vs-shape predicates. This section also contains
 * the cases where removing a point disconnects a 1D primitive.
 */

template <class Number, class Label>
template<PointConcept OtherPoint>
constexpr bool Point<Number, Label>::crosses(const OtherPoint&) const {
    return false;
}

/**
 * @section predicates-segment Segment
 * Segment endpoint, boundary, containment, collinearity, intersection, and
 * topological predicates, including the generic `separates` / `crosses`
 * dispatch used against 1D and area targets.
 */

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Segment<PointType, LabelType>::crosses(const OtherSegment& other) const {
    if constexpr (is_Rational_v<NumberType> || is_Rational_v<typename OtherSegment::NumberType>) {
        const int cross = boundingBoxesCross(other);
        if (cross == 0) {
            return false;
        }
        if (cross == 2) {
            return true;
        }
    }
    const auto d1 = orientationSign(min(), max(), other.min());
    if (d1 == 0) {
        return false;
    }
    const auto d2 = orientationSign(min(), max(), other.max());
    if (d1 == d2 || d2 == 0) {
        return false;
    }
    const auto d3 = orientationSign(other.min(), other.max(), min());
    if (d3 == 0) {
        return false;
    }
    const auto d4 = orientationSign(other.min(), other.max(), max());
    if (d4 == 0) {
        return false;
    }
    return d3 != d4;
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Segment<PointType, LabelType>::crosses(const OtherPoint&) const {
    return false;
}

template <class PointType, class LabelType>
constexpr bool Segment<PointType, LabelType>::crosses(const Shape<PointType>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->crosses(value);
        },
        other.variant());
}

/**
 * @section predicates-triangle Triangle
 * Triangle boundary, containment, intersection, and cut predicates, including
 * triangle-vs-rectangle and triangle-vs-triangle topological cases.
 */

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Triangle<PointType, LabelType>::crosses(const OtherSegment& other) const {
    return separates(other) && other.separates(*this);
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Triangle<PointType, LabelType>::crosses(const OtherPoint&) const {
    return false;
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Triangle<PointType, LabelType>::crosses(const OtherOrientedSegment& other) const {
    return separates(other) && other.separates(*this);
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Triangle<PointType, LabelType>::crosses(const OtherLine& other) const {
    return separates(other) && other.separates(*this);
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Triangle<PointType, LabelType>::crosses(const OtherOrientedLine& other) const {
    return separates(other) && other.separates(*this);
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool Triangle<PointType, LabelType>::crosses(const OtherRay& other) const {
    return separates(other) && other.separates(*this);
}

template <class PointType, class LabelType>
template<HalfplaneConcept OtherHalfplane>
constexpr bool Triangle<PointType, LabelType>::crosses(const OtherHalfplane&) const {
    return false;
}

template <class PointType, class LabelType>
template<RectangleConcept OtherRectangle>
constexpr bool Triangle<PointType, LabelType>::crosses(const OtherRectangle& other) const {
    return separates(other) && other.separates(*this);
}

template <class PointType, class LabelType>
template<TriangleConcept OtherTriangle>
constexpr bool Triangle<PointType, LabelType>::crosses(const OtherTriangle& other) const {
    return separates(other) && other.separates(*this);
}

template <class PointType, class LabelType>
constexpr bool Triangle<PointType, LabelType>::crosses(const Shape<PointType>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->crosses(value);
        },
        other.variant());
}

/**
 * @section predicates-oriented-segment OrientedSegment
 * Oriented-segment predicates. Most topology delegates to the unoriented
 * segment view, with local methods kept for orientation-sensitive behavior.
 */

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool OrientedSegment<PointType, LabelType>::crosses(const OtherSegment& other) const {
    return this->asSegment().crosses(other);
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool OrientedSegment<PointType, LabelType>::crosses(const OtherOrientedSegment& other) const {
    return this->asSegment().crosses(other.asSegment());
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool OrientedSegment<PointType, LabelType>::crosses(const OtherPoint& other) const {
    return this->asSegment().crosses(other);
}

template <class PointType, class LabelType>
constexpr bool OrientedSegment<PointType, LabelType>::crosses(const Shape<PointType>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->crosses(value);
        },
        other.variant());
}

/**
 * @section predicates-line Line
 * Geometric line predicates: geometric equality/order, containment,
 * intersection against 1D and 2D shapes, and generic separation dispatch.
 */

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Line<PointType, LabelType>::crosses(const OtherLine& other) const {
    if (isDegenerate() || other.isDegenerate() || *this == other) {
        return false;
    }
    return !parallel(other);
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Line<PointType, LabelType>::crosses(const OtherPoint&) const {
    return false;
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Line<PointType, LabelType>::crosses(const OtherSegment& other) const {
    return separates(other) && other.separates(*this);
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Line<PointType, LabelType>::crosses(const OtherOrientedSegment& other) const {
    return crosses(other.asSegment());
}

template <class PointType, class LabelType>
constexpr bool Line<PointType, LabelType>::crosses(const Shape<PointType>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->crosses(value);
        },
        other.variant());
}

/**
 * @section predicates-oriented-line OrientedLine
 * Oriented-line predicates. Shared topology is mostly delegated to the
 * unoriented line view, while orientation-specific methods stay local here.
 */

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool OrientedLine<PointType, LabelType>::crosses(const OtherLine& other) const {
    return this->asLine().crosses(other);
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool OrientedLine<PointType, LabelType>::crosses(const OtherOrientedLine& other) const {
    return this->asLine().crosses(other.asLine());
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool OrientedLine<PointType, LabelType>::crosses(const OtherPoint& other) const {
    return this->asLine().crosses(other);
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool OrientedLine<PointType, LabelType>::crosses(const OtherSegment& other) const {
    return this->asLine().crosses(other);
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool OrientedLine<PointType, LabelType>::crosses(const OtherOrientedSegment& other) const {
    return this->asLine().crosses(other);
}

template <class PointType, class LabelType>
constexpr bool OrientedLine<PointType, LabelType>::crosses(const Shape<PointType>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->crosses(value);
        },
        other.variant());
}

template <class PointType, class LabelType>
template <LineConcept OtherLine>
constexpr std::partial_ordering
OrientedLine<PointType, LabelType>::crossingOrder(const OtherLine& first,
                                       const OtherLine& second) const {
    // A point source() + t*(target() - source()) lies on a line L = (P, Q) where
    // the affine function orientationDeterminant(P, Q, .) = p + t*(q - p)
    // vanishes, i.e. at parameter t = p / (p - q). The denominator p - q is the
    // cross product of the two line directions and is zero exactly when L is
    // parallel to this oriented line.
    const auto crossing = [&](const OtherLine& line) {
        const auto p = orientationDeterminant(line[0], line[1], source());
        const auto q = orientationDeterminant(line[0], line[1], target());
        return std::pair{p, p - q};   // crossing parameter is p / (p - q)
    };
    const auto [p1, d1] = crossing(first);
    const auto [p2, d2] = crossing(second);
    if (d1 == 0 || d2 == 0) {
        return std::partial_ordering::unordered;   // a line is parallel to this one
    }

    // Order p1/d1 against p2/d2 without forming the difference: compare the
    // cross products, then undo the sign of the denominators d1*d2. The products
    // are ~coordinate^4, so promote the operands first to widen the headroom.
    using Wide = detail::promoted_number_t<std::remove_cvref_t<decltype(p1)>>;
    const auto cross = detail::threeWay(static_cast<Wide>(p1) * static_cast<Wide>(d2),
                                        static_cast<Wide>(p2) * static_cast<Wide>(d1));
    if (cross == 0) {
        return std::partial_ordering::equivalent;  // both cross at the same point
    }
    const bool denominators_agree = (d1 < 0) == (d2 < 0);
    const bool first_is_earlier = (cross < 0) == denominators_agree;
    return first_is_earlier ? std::partial_ordering::less : std::partial_ordering::greater;
}

/**
 * @section predicates-ray Ray
 * Ray-specific containment, intersection, and topological predicates. This is
 * where the asymmetric behavior of a half-infinite 1D primitive is implemented.
 */

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Ray<PointType, LabelType>::crosses(const OtherLine& other) const {
    return separates(other) && other.separates(*this);
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Ray<PointType, LabelType>::crosses(const OtherOrientedLine& other) const {
    return separates(other) && other.separates(*this);
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Ray<PointType, LabelType>::crosses(const OtherSegment& other) const {
    return separates(other) && other.separates(*this);
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Ray<PointType, LabelType>::crosses(const OtherOrientedSegment& other) const {
    return separates(other) && other.separates(*this);
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool Ray<PointType, LabelType>::crosses(const OtherRay& other) const {
    return separates(other) && other.separates(*this);
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Ray<PointType, LabelType>::crosses(const OtherPoint&) const {
    return false;
}

template <class PointType, class LabelType>
constexpr bool Ray<PointType, LabelType>::crosses(const Shape<PointType>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->crosses(value);
        },
        other.variant());
}

/**
 * @section predicates-rectangle Rectangle
 * Axis-aligned rectangle predicates plus the rectangle-local clipping helpers
 * used to answer strict interior and separation questions.
 */

template <class PointType, class LabelType>
template<RectangleConcept OtherRectangle>
constexpr bool Rectangle<PointType, LabelType>::crosses(const OtherRectangle& other) const {
    const bool hor_ver =
        min().x() < other.min().x() &&
        other.max().x() < max().x() &&
        other.min().y() < min().y() &&
        max().y() < other.max().y();
    if (hor_ver) {
        return true;
    }
    const bool ver_hor =
        other.min().x() < min().x() &&
        max().x() < other.max().x() &&
        min().y() < other.min().y() &&
        other.max().y() < max().y();
    return ver_hor;
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Rectangle<PointType, LabelType>::crosses(const OtherPoint&) const {
    return false;
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Rectangle<PointType, LabelType>::crosses(const OtherLine& other) const {
    return separates(other) && other.separates(*this);
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Rectangle<PointType, LabelType>::crosses(const OtherOrientedLine& other) const {
    return separates(other) && other.separates(*this);
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Rectangle<PointType, LabelType>::crosses(const OtherSegment& other) const {
    return separates(other) && other.separates(*this);
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Rectangle<PointType, LabelType>::crosses(const OtherOrientedSegment& other) const {
    return separates(other) && other.separates(*this);
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool Rectangle<PointType, LabelType>::crosses(const OtherRay& other) const {
    return separates(other) && other.separates(*this);
}

template <class PointType, class LabelType>
template<HalfplaneConcept OtherHalfplane>
constexpr bool Rectangle<PointType, LabelType>::crosses(const OtherHalfplane& other) const {
    (void)other;
    return false;
}

template <class PointType, class LabelType>
constexpr bool Rectangle<PointType, LabelType>::crosses(const Shape<PointType>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->crosses(value);
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
constexpr bool Halfplane<PointType, LabelType>::crosses(const OtherPoint&) const {
    return false;
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Halfplane<PointType, LabelType>::crosses(const OtherLine& other) const {
    (void)other;
    return false;
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Halfplane<PointType, LabelType>::crosses(const OtherOrientedLine& other) const {
    (void)other;
    return false;
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Halfplane<PointType, LabelType>::crosses(const OtherSegment& other) const {
    (void)other;
    return false;
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Halfplane<PointType, LabelType>::crosses(const OtherOrientedSegment& other) const {
    (void)other;
    return false;
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool Halfplane<PointType, LabelType>::crosses(const OtherRay& other) const {
    (void)other;
    return false;
}

template <class PointType, class LabelType>
template<HalfplaneConcept OtherHalfplane>
constexpr bool Halfplane<PointType, LabelType>::crosses(const OtherHalfplane& other) const {
    (void)other;
    return false;
}

template <class PointType, class LabelType>
constexpr bool Halfplane<PointType, LabelType>::crosses(const Shape<PointType>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->crosses(value);
        },
        other.variant());
}


// ---------------------------------------------------------------------------
// Convex

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Convex<PointType, LabelType>::crosses(const OtherPoint&) const {
    return false;
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Convex<PointType, LabelType>::crosses(const OtherSegment& other) const {
    return separates(other) && other.separates(*this);
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Convex<PointType, LabelType>::crosses(const OtherOrientedSegment& other) const {
    return crosses(static_cast<Segment<typename OtherOrientedSegment::PointType>>(other));
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Convex<PointType, LabelType>::crosses(const OtherLine& other) const {
    return separates(other) && other.separates(*this);
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Convex<PointType, LabelType>::crosses(const OtherOrientedLine& other) const {
    return crosses(static_cast<Line<typename OtherOrientedLine::PointType>>(other));
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool Convex<PointType, LabelType>::crosses(const OtherRay& other) const {
    return separates(other) && other.separates(*this);
}

template <class PointType, class LabelType>
template<HalfplaneConcept OtherHalfplane>
constexpr bool Convex<PointType, LabelType>::crosses(const OtherHalfplane&) const {
    return false;
}

template <class PointType, class LabelType>
template<RectangleConcept OtherRectangle>
constexpr bool Convex<PointType, LabelType>::crosses(const OtherRectangle& other) const {
    return crosses(other.asConvex());
}

template <class PointType, class LabelType>
template<TriangleConcept OtherTriangle>
constexpr bool Convex<PointType, LabelType>::crosses(const OtherTriangle& other) const {
    return crosses(other.asConvex());
}

template <class PointType, class LabelType>
template<ConvexConcept OtherConvex>
constexpr bool Convex<PointType, LabelType>::crosses(const OtherConvex& other) const {
    if (bbox().crosses(other.bbox())) {
        return true;
    }
    return separates(other) && other.separates(*this);
}

template <class PointType, class LabelType>
template<DiskConcept OtherDisk>
constexpr bool Convex<PointType, LabelType>::crosses(const OtherDisk& other) const {
    return (separates(other) && other.separates(*this));
}

template <class PointType, class LabelType>
template <PointConcept OtherPoint>
constexpr bool Convex<PointType, LabelType>::crosses(const Shape<OtherPoint>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->crosses(value);
        },
        other.variant());
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Polygon<PointType, LabelType>::crosses(const OtherPoint&) const {
    return false;
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Polygon<PointType, LabelType>::crosses(const OtherSegment& other) const {
    return separates(other) && other.separates(*this);
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Polygon<PointType, LabelType>::crosses(const OtherOrientedSegment& other) const {
    return crosses(other.asSegment());
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool Polygon<PointType, LabelType>::crosses(const OtherRay& other) const {
    return separates(other) && other.separates(*this);
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Polygon<PointType, LabelType>::crosses(const OtherLine& other) const {
    return separates(other) && other.separates(*this);
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Polygon<PointType, LabelType>::crosses(const OtherOrientedLine& other) const {
    return separates(other) && other.separates(*this);
}

template <class PointType, class LabelType>
template<HalfplaneConcept OtherHalfplane>
constexpr bool Polygon<PointType, LabelType>::crosses(const OtherHalfplane&) const {
    return false;
}

template <class PointType, class LabelType>
template<RectangleConcept OtherRectangle>
constexpr bool Polygon<PointType, LabelType>::crosses(const OtherRectangle&other) const {
    return separates(other) && other.separates(*this);
}

template <class PointType, class LabelType>
template<TriangleConcept OtherTriangle>
constexpr bool Polygon<PointType, LabelType>::crosses(const OtherTriangle& other) const {
    return separates(other) && other.separates(*this);
}

template <class PointType, class LabelType>
template<ConvexConcept OtherConvex>
constexpr bool Polygon<PointType, LabelType>::crosses(const OtherConvex& other) const {
    return separates(other) && other.separates(*this);
}

template <class PointType, class LabelType>
template<DiskConcept OtherDisk>
constexpr bool Polygon<PointType, LabelType>::crosses(const OtherDisk& other) const {
    return separates(other) && other.separates(*this);
}

template <class PointType, class LabelType>
template<PolygonConcept OtherPolygon>
constexpr bool Polygon<PointType, LabelType>::crosses(const OtherPolygon& other) const {
    return separates(other) && other.separates(*this);
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Polygon<PointType, LabelType>::crosses(const Shape<OtherPoint>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->crosses(value);
        },
        other.variant());
}

template <class Number, class Label>
constexpr bool Point<Number, Label>::crosses(const Shape<Point<Number, Label>>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->crosses(value);
        },
        other.variant());
}


// --- Disk crosses overloads (via separates) + Shape dispatch ---

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Disk<PointType, LabelType>::crosses(const OtherPoint&) const {
    return false;
}

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Disk<PointType, LabelType>::crosses(const OtherSegment& other) const {
    return separates(other) && other.separates(*this);
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Disk<PointType, LabelType>::crosses(const OtherOrientedSegment& other) const {
    return crosses(other.asSegment());
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Disk<PointType, LabelType>::crosses(const OtherLine& other) const {
    return separates(other) && other.separates(*this);
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Disk<PointType, LabelType>::crosses(const OtherOrientedLine& other) const {
    return separates(other) && other.separates(*this);
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool Disk<PointType, LabelType>::crosses(const OtherRay& other) const {
    return separates(other) && other.separates(*this);
}

template <class PointType, class LabelType>
template<HalfplaneConcept OtherHalfplane>
constexpr bool Disk<PointType, LabelType>::crosses(const OtherHalfplane& other) const {
    return separates(other) && other.separates(*this);
}

template <class PointType, class LabelType>
template<RectangleConcept OtherRectangle>
constexpr bool Disk<PointType, LabelType>::crosses(const OtherRectangle& other) const {
    return separates(other) && other.separates(*this);
}

template <class PointType, class LabelType>
template<TriangleConcept OtherTriangle>
constexpr bool Disk<PointType, LabelType>::crosses(const OtherTriangle& other) const {
    return separates(other) && other.separates(*this);
}

template <class PointType, class LabelType>
template<DiskConcept OtherDisk>
constexpr bool Disk<PointType, LabelType>::crosses(const OtherDisk& other) const {
    return separates(other) && other.separates(*this);
}

template <class PointType, class LabelType>
template<PointConcept OtherPoint>
constexpr bool Disk<PointType, LabelType>::crosses(const Shape<OtherPoint>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->crosses(value);
        },
        other.variant());
}

/**
 * @section predicates-monotonechain MonotoneChain
 * Mutual-cut predicates for a weakly x-monotone chain: `a.crosses(b)` is
 * `a.separates(b) && b.separates(a)`, so the pairs whose `separates` is not
 * implemented yet throw through it.
 */

template <class PointType, class LabelType, class Storage>
template<SegmentConcept OtherSegment>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::crosses(const OtherSegment& other) const {
    return separates(other) && other.separates(*this);
}

template <class PointType, class LabelType, class Storage>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::crosses(const OtherOrientedSegment& other) const {
    return crosses(other.asSegment());
}

template <class PointType, class LabelType, class Storage>
template<LineConcept OtherLine>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::crosses(const OtherLine& other) const {
    return separates(other) && other.separates(*this);
}

template <class PointType, class LabelType, class Storage>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::crosses(const OtherOrientedLine& other) const {
    return separates(other) && other.separates(*this);
}

template <class PointType, class LabelType, class Storage>
template<RayConcept OtherRay>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::crosses(const OtherRay& other) const {
    return separates(other) && other.separates(*this);
}

template <class PointType, class LabelType, class Storage>
template<HalfplaneConcept OtherHalfplane>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::crosses(const OtherHalfplane& other) const {
    return separates(other) && other.separates(*this);
}

template <class PointType, class LabelType, class Storage>
template<RectangleConcept OtherRectangle>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::crosses(const OtherRectangle& other) const {
    return separates(other) && other.separates(*this);
}

template <class PointType, class LabelType, class Storage>
template<TriangleConcept OtherTriangle>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::crosses(const OtherTriangle& other) const {
    return separates(other) && other.separates(*this);
}

template <class PointType, class LabelType, class Storage>
template<DiskConcept OtherDisk>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::crosses(const OtherDisk& other) const {
    return separates(other) && other.separates(*this);
}

template <class PointType, class LabelType, class Storage>
template<ConvexConcept OtherConvex>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::crosses(const OtherConvex& other) const {
    return separates(other) && other.separates(*this);
}

template <class PointType, class LabelType, class Storage>
template<MonotoneChainConcept OtherChain>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::crosses(const OtherChain& other) const {
    return separates(other) && other.separates(*this);
}

template <class PointType, class LabelType, class Storage>
template<PointConcept OtherPoint>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::crosses(const Shape<OtherPoint>& other) const {
    return std::visit(
        [this](const auto& value) {
            return this->crosses(value);
        },
        other.variant());
}

template <class PointType, class LabelType, class Storage>
template<MonotoneChainConcept OtherChain>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::edgesCross(const OtherChain& other) const {
    if (size() < 2 || other.size() < 2) {
        return false;
    }
    // A strong (robust, transversal) crossing is exactly a properly crossing
    // edge pair: two edges whose interiors meet with the chains passing to
    // opposite sides (Segment::crosses). A mere touch, a collinear overlap, or a
    // vertical edge that only brackets the other chain at a shared boundary x
    // are not proper crossings, so they do not count -- unlike "some vertex
    // above and some below", they cannot be slid apart into a non-crossing by an
    // arbitrarily small perturbation. Both edge sequences are sorted by
    // x-interval, so a merge sweep advancing the edge with the smaller right
    // endpoint visits every pair whose x-ranges overlap in O(n + m).
    const std::size_t iEnd = size() - 1;
    const std::size_t jEnd = other.size() - 1;
    // Seed past the leading edges left of the shared x-window (see the sweep in
    // MonotoneChain::intersects): indexAtX locates the first candidate in
    // O(log n); a disengaged result means the x-ranges are disjoint, so no pair
    // can properly cross.
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
            mine.crosses(theirs)) {
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
constexpr bool Polygon<PointType, LabelType>::crosses(const OtherChain& other) const {
    return separates(other) && other.separates(*this);
}

/**
 * @section predicates-polyline Polyline
 * Mutual-cut predicates for an open polygonal chain: `a.crosses(b)` is
 * `a.separates(b) && b.separates(a)`.
 */

template <class PointType, class LabelType>
template<SegmentConcept OtherSegment>
constexpr bool Polyline<PointType, LabelType>::crosses(const OtherSegment& other) const {
    return separates(other) && other.separates(*this);
}

template <class PointType, class LabelType>
template<OrientedSegmentConcept OtherOrientedSegment>
constexpr bool Polyline<PointType, LabelType>::crosses(const OtherOrientedSegment& other) const {
    return crosses(other.asSegment());
}

template <class PointType, class LabelType>
template<LineConcept OtherLine>
constexpr bool Polyline<PointType, LabelType>::crosses(const OtherLine& other) const {
    return separates(other) && other.separates(*this);
}

template <class PointType, class LabelType>
template<OrientedLineConcept OtherOrientedLine>
constexpr bool Polyline<PointType, LabelType>::crosses(const OtherOrientedLine& other) const {
    return separates(other) && other.separates(*this);
}

template <class PointType, class LabelType>
template<RayConcept OtherRay>
constexpr bool Polyline<PointType, LabelType>::crosses(const OtherRay& other) const {
    return separates(other) && other.separates(*this);
}

template <class PointType, class LabelType>
template<HalfplaneConcept OtherHalfplane>
constexpr bool Polyline<PointType, LabelType>::crosses(const OtherHalfplane& other) const {
    return separates(other) && other.separates(*this);
}

template <class PointType, class LabelType>
template<RectangleConcept OtherRectangle>
constexpr bool Polyline<PointType, LabelType>::crosses(const OtherRectangle& other) const {
    return separates(other) && other.separates(*this);
}

template <class PointType, class LabelType>
template<TriangleConcept OtherTriangle>
constexpr bool Polyline<PointType, LabelType>::crosses(const OtherTriangle& other) const {
    return separates(other) && other.separates(*this);
}

template <class PointType, class LabelType>
template<DiskConcept OtherDisk>
constexpr bool Polyline<PointType, LabelType>::crosses(const OtherDisk& other) const {
    return separates(other) && other.separates(*this);
}

template <class PointType, class LabelType>
template<ConvexConcept OtherConvex>
constexpr bool Polyline<PointType, LabelType>::crosses(const OtherConvex& other) const {
    return separates(other) && other.separates(*this);
}

template <class PointType, class LabelType>
template<MonotoneChainConcept OtherChain>
constexpr bool Polyline<PointType, LabelType>::crosses(const OtherChain& other) const {
    return separates(other) && other.separates(*this);
}

template <class PointType, class LabelType>
template<PolylineConcept OtherPolyline>
constexpr bool Polyline<PointType, LabelType>::crosses(const OtherPolyline& other) const {
    return separates(other) && other.separates(*this);
}

template <class PointType, class LabelType>
template<PolylineConcept OtherPolyline>
constexpr bool Polygon<PointType, LabelType>::crosses(const OtherPolyline& other) const {
    return separates(other) && other.separates(*this);
}

}  // namespace pgl
