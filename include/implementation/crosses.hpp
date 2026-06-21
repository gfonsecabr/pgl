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
constexpr bool Polygon<PointType, LabelType>::crosses(const OtherDisk&) const {
    throw std::runtime_error(
        "pgl: Polygon::crosses(Disk) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
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


// --- Disk symmetric-trio stubs (not yet implemented) + Shape dispatch ---

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
constexpr bool Disk<PointType, LabelType>::crosses(const OtherOrientedSegment&) const {
    throw std::runtime_error(
        "pgl: Disk::crosses(OrientedSegment) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
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
constexpr bool Disk<PointType, LabelType>::crosses(const OtherRay&) const {
    throw std::runtime_error(
        "pgl: Disk::crosses(Ray) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
}

template <class PointType, class LabelType>
template<HalfplaneConcept OtherHalfplane>
constexpr bool Disk<PointType, LabelType>::crosses(const OtherHalfplane&) const {
    throw std::runtime_error(
        "pgl: Disk::crosses(Halfplane) is not implemented yet for this shape pair");
    return false;  // unreachable; satisfies constexpr return requirement
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

}  // namespace pgl
