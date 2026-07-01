#pragma once

#include "implementation/intersection.hpp"
#include "shape/convex.hpp"

/**
 * @file distance.hpp
 * @brief Distance and Hausdorff-style measurements between shapes.
 *
 * Every `squaredDistance` / `squaredHausdorffDistance` overload is templated on
 * a `ResultNumber` coordinate type that defaults to the calling shape's
 * `NumberType`, mirroring the `intersection` API. Overloads whose closest point
 * lies in a shape's interior divide by a squared length; with an integer
 * `ResultNumber` that division truncates, so callers that need an accurate
 * result must request a floating-point or pgl::Rational `ResultNumber`.
 */

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <ranges>


namespace pgl {

// -----------------------------------------------------------------------------
// Point

template <class Number, class Label>
template <class ResultNumber, PointConcept OtherPoint>
constexpr auto Point<Number, Label>::squaredDistance(const OtherPoint& other) const {
    const ResultNumber dx = static_cast<ResultNumber>(x()) - static_cast<ResultNumber>(other.x());
    const ResultNumber dy = static_cast<ResultNumber>(y()) - static_cast<ResultNumber>(other.y());
    return dx * dx + dy * dy;
}

template <class Number, class Label>
template <class ApproximateNumber, PointConcept OtherPoint>
ApproximateNumber Point<Number, Label>::distance(const OtherPoint& other) const {
    return std::sqrt(static_cast<ApproximateNumber>(squaredDistance(other)));
}

template <class Number, class Label>
template<PointConcept OtherPoint>
constexpr auto Point<Number, Label>::distanceL1(const OtherPoint& other) const {
    return pgl::detail::abs(x() - other.x()) + pgl::detail::abs(y() - other.y());
}

template <class Number, class Label>
template<PointConcept OtherPoint>
constexpr auto Point<Number, Label>::distanceLInf(const OtherPoint& other) const {
    return std::max(pgl::detail::abs(x() - other.x()), pgl::detail::abs(y() - other.y()));
}

// -----------------------------------------------------------------------------
// Segment

template <class PointType, class LabelType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr auto Segment<PointType, LabelType>::squaredDistance(const OtherPoint& point) const {
    const Point<ResultNumber> a(min());
    const Point<ResultNumber> b(max());
    const Point<ResultNumber> p(point);

    const auto ab = b - a;
    const auto ap = p - a;
    const auto bp = p - b;
    const auto squared_length = ab * ab;

    if (squared_length == ResultNumber{}) {
        return a.template squaredDistance<ResultNumber>(p);
    }

    const ResultNumber projection_numerator = ap * ab;
    if (projection_numerator <= ResultNumber{}) {
        return static_cast<ResultNumber>(ap * ap);
    }

    if (projection_numerator >= squared_length) {
        return static_cast<ResultNumber>(bp * bp);
    }

    const ResultNumber twice_triangle_area = ab.x() * ap.y() - ab.y() * ap.x();
    return static_cast<ResultNumber>((twice_triangle_area * twice_triangle_area) / squared_length);
}

template <class PointType, class LabelType>
template <class ResultNumber, SegmentConcept OtherSegment>
constexpr auto Segment<PointType, LabelType>::squaredDistance(const OtherSegment& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }

    const auto this_min_to_other = other.template squaredDistance<ResultNumber>(min());
    const auto this_max_to_other = other.template squaredDistance<ResultNumber>(max());
    const auto other_min_to_this = this->template squaredDistance<ResultNumber>(other.min());
    const auto other_max_to_this = this->template squaredDistance<ResultNumber>(other.max());

    const auto best_from_this = this_min_to_other < this_max_to_other ? this_min_to_other : this_max_to_other;
    const auto best_from_other = other_min_to_this < other_max_to_this ? other_min_to_this : other_max_to_this;

    return best_from_this < best_from_other ? best_from_this : best_from_other;
}

template <class PointType, class LabelType>
template <class ResultNumber, SegmentConcept OtherSegment>
constexpr auto Segment<PointType, LabelType>::squaredHausdorffDistance(const OtherSegment& other) const {
    const auto this_min_to_other = other.template squaredDistance<ResultNumber>(min());
    const auto this_max_to_other = other.template squaredDistance<ResultNumber>(max());
    const auto other_min_to_this = this->template squaredDistance<ResultNumber>(other.min());
    const auto other_max_to_this = this->template squaredDistance<ResultNumber>(other.max());

    const auto worst_from_this = this_min_to_other > this_max_to_other ? this_min_to_other : this_max_to_other;
    const auto worst_from_other = other_min_to_this > other_max_to_this ? other_min_to_this : other_max_to_this;

    return worst_from_this > worst_from_other ? worst_from_this : worst_from_other;
}

// -----------------------------------------------------------------------------
// OrientedSegment

template <class PointType, class LabelType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr auto OrientedSegment<PointType, LabelType>::squaredDistance(const OtherPoint& point) const {
    return static_cast<Segment<PointType>>(*this).template squaredDistance<ResultNumber>(point);
}

template <class PointType, class LabelType>
template <class ResultNumber, SegmentConcept OtherSegment>
constexpr auto OrientedSegment<PointType, LabelType>::squaredDistance(const OtherSegment& other) const {
    return static_cast<Segment<PointType>>(*this).template squaredDistance<ResultNumber>(other);
}

template <class PointType, class LabelType>
template <class ResultNumber, OrientedSegmentConcept OtherOrientedSegment>
constexpr auto OrientedSegment<PointType, LabelType>::squaredDistance(const OtherOrientedSegment& other) const {
    return static_cast<Segment<PointType>>(*this).template squaredDistance<ResultNumber>(
        static_cast<Segment<typename OtherOrientedSegment::PointType>>(other));
}

template <class PointType, class LabelType>
template <class ResultNumber, SegmentConcept OtherSegment>
constexpr auto OrientedSegment<PointType, LabelType>::squaredHausdorffDistance(const OtherSegment& other) const {
    return static_cast<Segment<PointType>>(*this).template squaredHausdorffDistance<ResultNumber>(other);
}

template <class PointType, class LabelType>
template <class ResultNumber, OrientedSegmentConcept OtherOrientedSegment>
constexpr auto OrientedSegment<PointType, LabelType>::squaredHausdorffDistance(const OtherOrientedSegment& other) const {
    return static_cast<Segment<PointType>>(*this).template squaredHausdorffDistance<ResultNumber>(
        static_cast<Segment<typename OtherOrientedSegment::PointType>>(other));
}

// -----------------------------------------------------------------------------
// Line

template <class PointType, class LabelType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr auto Line<PointType, LabelType>::squaredDistance(const OtherPoint& point) const {
    if (isDegenerate()) {
        return min().template squaredDistance<ResultNumber>(point);
    }

    const Point<ResultNumber> a(min());
    const Point<ResultNumber> b(max());
    const Point<ResultNumber> p(point);
    const auto ab = b - a;
    const auto ap = p - a;
    const ResultNumber twice_triangle_area = ab.x() * ap.y() - ab.y() * ap.x();
    const ResultNumber squared_length = ab * ab;
    return static_cast<ResultNumber>((twice_triangle_area * twice_triangle_area) / squared_length);
}

template <class PointType, class LabelType>
template <class ResultNumber, LineConcept OtherLine>
constexpr auto Line<PointType, LabelType>::squaredDistance(const OtherLine& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }

    return other.template squaredDistance<ResultNumber>(min());
}

template <class PointType, class LabelType>
template <class ResultNumber, SegmentConcept OtherSegment>
constexpr auto Line<PointType, LabelType>::squaredDistance(const OtherSegment& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }

    const auto source_distance = this->template squaredDistance<ResultNumber>(other.min());
    const auto target_distance = this->template squaredDistance<ResultNumber>(other.max());
    return source_distance < target_distance ? source_distance : target_distance;
}

template <class PointType, class LabelType>
template <class ResultNumber, OrientedSegmentConcept OtherOrientedSegment>
constexpr auto Line<PointType, LabelType>::squaredDistance(const OtherOrientedSegment& other) const {
    return this->template squaredDistance<ResultNumber>(static_cast<Segment<typename OtherOrientedSegment::PointType>>(other));
}

// -----------------------------------------------------------------------------
// OrientedLine

template <class PointType, class LabelType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr auto OrientedLine<PointType, LabelType>::squaredDistance(const OtherPoint& point) const {
    return this->asLine().template squaredDistance<ResultNumber>(point);
}

template <class PointType, class LabelType>
template <class ResultNumber, LineConcept OtherLine>
constexpr auto OrientedLine<PointType, LabelType>::squaredDistance(const OtherLine& other) const {
    return this->asLine().template squaredDistance<ResultNumber>(other);
}

template <class PointType, class LabelType>
template <class ResultNumber, OrientedLineConcept OtherOrientedLine>
constexpr auto OrientedLine<PointType, LabelType>::squaredDistance(const OtherOrientedLine& other) const {
    return this->asLine().template squaredDistance<ResultNumber>(other.asLine());
}

template <class PointType, class LabelType>
template <class ResultNumber, SegmentConcept OtherSegment>
constexpr auto OrientedLine<PointType, LabelType>::squaredDistance(const OtherSegment& other) const {
    return this->asLine().template squaredDistance<ResultNumber>(other);
}

template <class PointType, class LabelType>
template <class ResultNumber, OrientedSegmentConcept OtherOrientedSegment>
constexpr auto OrientedLine<PointType, LabelType>::squaredDistance(const OtherOrientedSegment& other) const {
    return this->asLine().template squaredDistance<ResultNumber>(other);
}

// -----------------------------------------------------------------------------
// Ray

template <class PointType, class LabelType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr auto Ray<PointType, LabelType>::squaredDistance(const OtherPoint& point) const {
    if (isDegenerate()) {
        return source().template squaredDistance<ResultNumber>(point);
    }

    const Point<ResultNumber> a(source());
    const Point<ResultNumber> b(target());
    const Point<ResultNumber> p(point);
    const auto ab = b - a;
    const auto ap = p - a;
    const auto squared_length = ab * ab;

    if (squared_length == ResultNumber{}) {
        return static_cast<ResultNumber>(ap * ap);
    }

    const ResultNumber projection_numerator = ap * ab;
    if (projection_numerator <= ResultNumber{}) {
        return static_cast<ResultNumber>(ap * ap);
    }

    const ResultNumber twice_triangle_area = ab.x() * ap.y() - ab.y() * ap.x();
    return static_cast<ResultNumber>((twice_triangle_area * twice_triangle_area) / squared_length);
}

template <class PointType, class LabelType>
template <class ResultNumber, LineConcept OtherLine>
constexpr auto Ray<PointType, LabelType>::squaredDistance(const OtherLine& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }

    return other.template squaredDistance<ResultNumber>(source());
}

template <class PointType, class LabelType>
template <class ResultNumber, OrientedLineConcept OtherOrientedLine>
constexpr auto Ray<PointType, LabelType>::squaredDistance(const OtherOrientedLine& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }

    return other.template squaredDistance<ResultNumber>(source());
}

template <class PointType, class LabelType>
template <class ResultNumber, SegmentConcept OtherSegment>
constexpr auto Ray<PointType, LabelType>::squaredDistance(const OtherSegment& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }

    const auto source_to_other = other.template squaredDistance<ResultNumber>(source());
    const auto other_min_to_this = this->template squaredDistance<ResultNumber>(other.min());
    const auto other_max_to_this = this->template squaredDistance<ResultNumber>(other.max());
    const auto best_from_segment = other_min_to_this < other_max_to_this ? other_min_to_this : other_max_to_this;
    return source_to_other < best_from_segment ? source_to_other : best_from_segment;
}

template <class PointType, class LabelType>
template <class ResultNumber, OrientedSegmentConcept OtherOrientedSegment>
constexpr auto Ray<PointType, LabelType>::squaredDistance(const OtherOrientedSegment& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }

    const auto source_to_other = other.template squaredDistance<ResultNumber>(source());
    const auto other_source_to_this = this->template squaredDistance<ResultNumber>(other.source());
    const auto other_target_to_this = this->template squaredDistance<ResultNumber>(other.target());
    const auto best_from_segment = other_source_to_this < other_target_to_this ? other_source_to_this : other_target_to_this;
    return source_to_other < best_from_segment ? source_to_other : best_from_segment;
}

template <class PointType, class LabelType>
template <class ResultNumber, RayConcept OtherRay>
constexpr auto Ray<PointType, LabelType>::squaredDistance(const OtherRay& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }

    const auto this_source_to_other = other.template squaredDistance<ResultNumber>(source());
    const auto other_source_to_this = this->template squaredDistance<ResultNumber>(other.source());
    return this_source_to_other < other_source_to_this ? this_source_to_other : other_source_to_this;
}

// -----------------------------------------------------------------------------
// Halfplane

template <class PointType, class LabelType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr auto Halfplane<PointType, LabelType>::squaredDistance(const OtherPoint& point) const {
    if (intersects(point)) {
        return ResultNumber{};
    }
    return asLine().template squaredDistance<ResultNumber>(point);
}

template <class PointType, class LabelType>
template <class ResultNumber, SegmentConcept OtherSegment>
constexpr auto Halfplane<PointType, LabelType>::squaredDistance(const OtherSegment& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return asLine().template squaredDistance<ResultNumber>(other);
}

template <class PointType, class LabelType>
template <class ResultNumber, OrientedSegmentConcept OtherOrientedSegment>
constexpr auto Halfplane<PointType, LabelType>::squaredDistance(const OtherOrientedSegment& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return asLine().template squaredDistance<ResultNumber>(other);
}

template <class PointType, class LabelType>
template <class ResultNumber, LineConcept OtherLine>
constexpr auto Halfplane<PointType, LabelType>::squaredDistance(const OtherLine& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return asLine().template squaredDistance<ResultNumber>(other);
}

template <class PointType, class LabelType>
template <class ResultNumber, OrientedLineConcept OtherOrientedLine>
constexpr auto Halfplane<PointType, LabelType>::squaredDistance(const OtherOrientedLine& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return asLine().template squaredDistance<ResultNumber>(other);
}

template <class PointType, class LabelType>
template <class ResultNumber, RayConcept OtherRay>
constexpr auto Halfplane<PointType, LabelType>::squaredDistance(const OtherRay& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return asLine().template squaredDistance<ResultNumber>(other);
}

template <class PointType, class LabelType>
template <class ResultNumber, HalfplaneConcept OtherHalfplane>
constexpr auto Halfplane<PointType, LabelType>::squaredDistance(const OtherHalfplane& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return asLine().template squaredDistance<ResultNumber>(other.asLine());
}

// -----------------------------------------------------------------------------
// Rectangle

template <class PointType, class LabelType>
template <class Left, class Right>
constexpr auto Rectangle<PointType, LabelType>::axisDistance(const Left& first_min, const Left& first_max, const Right& second_min, const Right& second_max)
    -> std::common_type_t<Left, Right> {
    using Distance = std::common_type_t<Left, Right>;

    if (first_max < second_min) {
        return static_cast<Distance>(second_min) - static_cast<Distance>(first_max);
    }

    if (second_max < first_min) {
        return static_cast<Distance>(first_min) - static_cast<Distance>(second_max);
    }

    return Distance{};
}

template <class PointType, class LabelType>
template <class ResultNumber, RectangleConcept OtherRectangle>
constexpr auto Rectangle<PointType, LabelType>::maxVertexDistanceTo(const OtherRectangle& other) const {
    const auto rectangle_vertices = vertices();
    auto worst_distance = other.template squaredDistance<ResultNumber>(rectangle_vertices[0]);

    for (std::size_t index = 1; index < rectangle_vertices.size(); ++index) {
        const auto current_distance = other.template squaredDistance<ResultNumber>(rectangle_vertices[index]);
        if (worst_distance < current_distance) {
            worst_distance = current_distance;
        }
    }

    return worst_distance;
}

template <class PointType, class LabelType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr auto Rectangle<PointType, LabelType>::squaredDistance(const OtherPoint& point) const {
    const ResultNumber dx = static_cast<ResultNumber>(axisDistance(min().x(), max().x(), point.x(), point.x()));
    const ResultNumber dy = static_cast<ResultNumber>(axisDistance(min().y(), max().y(), point.y(), point.y()));
    return dx * dx + dy * dy;
}

template <class PointType, class LabelType>
template <class ResultNumber, LineConcept OtherLine>
constexpr auto Rectangle<PointType, LabelType>::squaredDistance(const OtherLine& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }

    const auto rectangle_vertices = vertices();
    auto best_distance = other.template squaredDistance<ResultNumber>(rectangle_vertices[0]);
    for (std::size_t index = 1; index < rectangle_vertices.size(); ++index) {
        const auto current_distance = other.template squaredDistance<ResultNumber>(rectangle_vertices[index]);
        if (current_distance < best_distance) {
            best_distance = current_distance;
        }
    }

    return best_distance;
}

template <class PointType, class LabelType>
template <class ResultNumber, OrientedLineConcept OtherOrientedLine>
constexpr auto Rectangle<PointType, LabelType>::squaredDistance(const OtherOrientedLine& other) const {
    return this->template squaredDistance<ResultNumber>(other.asLine());
}

template <class PointType, class LabelType>
template <class ResultNumber, SegmentConcept OtherSegment>
constexpr auto Rectangle<PointType, LabelType>::squaredDistance(const OtherSegment& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }

    auto best_distance = this->template squaredDistance<ResultNumber>(other.min());
    const auto other_max_distance = this->template squaredDistance<ResultNumber>(other.max());
    if (other_max_distance < best_distance) {
        best_distance = other_max_distance;
    }

    const auto rectangle_edges = edges();
    for (const auto& edge : rectangle_edges) {
        const auto current_distance = edge.template squaredDistance<ResultNumber>(other);
        if (current_distance < best_distance) {
            best_distance = current_distance;
        }
    }

    return best_distance;
}

template <class PointType, class LabelType>
template <class ResultNumber, OrientedSegmentConcept OtherOrientedSegment>
constexpr auto Rectangle<PointType, LabelType>::squaredDistance(const OtherOrientedSegment& other) const {
    return this->template squaredDistance<ResultNumber>(static_cast<Segment<typename OtherOrientedSegment::PointType>>(other));
}

template <class PointType, class LabelType>
template <class ResultNumber, RayConcept OtherRay>
constexpr auto Rectangle<PointType, LabelType>::squaredDistance(const OtherRay& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }

    auto best_distance = this->template squaredDistance<ResultNumber>(other.source());
    const auto rectangle_edges = edges();
    for (const auto& edge : rectangle_edges) {
        const auto current_distance = other.template squaredDistance<ResultNumber>(edge);
        if (current_distance < best_distance) {
            best_distance = current_distance;
        }
    }

    return best_distance;
}

template <class PointType, class LabelType>
template <class ResultNumber, HalfplaneConcept OtherHalfplane>
constexpr auto Rectangle<PointType, LabelType>::squaredDistance(const OtherHalfplane& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template squaredDistance<ResultNumber>(other.asLine());
}

template <class PointType, class LabelType>
template <class ResultNumber, RectangleConcept OtherRectangle>
constexpr auto Rectangle<PointType, LabelType>::squaredDistance(const OtherRectangle& other) const {
    const ResultNumber dx = static_cast<ResultNumber>(axisDistance(min().x(), max().x(), other.min().x(), other.max().x()));
    const ResultNumber dy = static_cast<ResultNumber>(axisDistance(min().y(), max().y(), other.min().y(), other.max().y()));
    return dx * dx + dy * dy;
}

template <class PointType, class LabelType>
template <class ResultNumber, RectangleConcept OtherRectangle>
constexpr auto Rectangle<PointType, LabelType>::squaredHausdorffDistance(const OtherRectangle& other) const {
    const auto worst_from_this = this->template maxVertexDistanceTo<ResultNumber>(other);
    const auto worst_from_other = other.template maxVertexDistanceTo<ResultNumber>(*this);
    return worst_from_this < worst_from_other ? worst_from_other : worst_from_this;
}

// -----------------------------------------------------------------------------
// Triangle

template <class PointType, class LabelType>
template <class ResultNumber, class OtherShape>
constexpr ResultNumber Triangle<PointType, LabelType>::edgeMinSquaredDistance(const OtherShape& other) const {
    const auto triangle_edges = edges();
    auto best = triangle_edges[0].template squaredDistance<ResultNumber>(other);
    for (std::size_t index = 1; index < triangle_edges.size(); ++index) {
        const auto current = triangle_edges[index].template squaredDistance<ResultNumber>(other);
        if (current < best) {
            best = current;
        }
    }
    return best;
}

template <class PointType, class LabelType>
template <class ResultNumber, class OtherShape>
constexpr ResultNumber Triangle<PointType, LabelType>::vertexMinSquaredDistance(const OtherShape& other) const {
    const auto triangle_vertices = vertices();
    auto best = other.template squaredDistance<ResultNumber>(triangle_vertices[0]);
    for (std::size_t index = 1; index < triangle_vertices.size(); ++index) {
        const auto current = other.template squaredDistance<ResultNumber>(triangle_vertices[index]);
        if (current < best) {
            best = current;
        }
    }
    return best;
}

template <class PointType, class LabelType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr auto Triangle<PointType, LabelType>::squaredDistance(const OtherPoint& point) const {
    if (intersects(point)) {
        return ResultNumber{};
    }
    return this->template edgeMinSquaredDistance<ResultNumber>(point);
}

template <class PointType, class LabelType>
template <class ResultNumber, SegmentConcept OtherSegment>
constexpr auto Triangle<PointType, LabelType>::squaredDistance(const OtherSegment& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template edgeMinSquaredDistance<ResultNumber>(other);
}

template <class PointType, class LabelType>
template <class ResultNumber, OrientedSegmentConcept OtherOrientedSegment>
constexpr auto Triangle<PointType, LabelType>::squaredDistance(const OtherOrientedSegment& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template edgeMinSquaredDistance<ResultNumber>(other);
}

template <class PointType, class LabelType>
template <class ResultNumber, LineConcept OtherLine>
constexpr auto Triangle<PointType, LabelType>::squaredDistance(const OtherLine& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template vertexMinSquaredDistance<ResultNumber>(other);
}

template <class PointType, class LabelType>
template <class ResultNumber, OrientedLineConcept OtherOrientedLine>
constexpr auto Triangle<PointType, LabelType>::squaredDistance(const OtherOrientedLine& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template vertexMinSquaredDistance<ResultNumber>(other);
}

template <class PointType, class LabelType>
template <class ResultNumber, RayConcept OtherRay>
constexpr auto Triangle<PointType, LabelType>::squaredDistance(const OtherRay& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template edgeMinSquaredDistance<ResultNumber>(other);
}

template <class PointType, class LabelType>
template <class ResultNumber, HalfplaneConcept OtherHalfplane>
constexpr auto Triangle<PointType, LabelType>::squaredDistance(const OtherHalfplane& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template squaredDistance<ResultNumber>(other.asLine());
}

template <class PointType, class LabelType>
template <class ResultNumber, RectangleConcept OtherRectangle>
constexpr auto Triangle<PointType, LabelType>::squaredDistance(const OtherRectangle& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template edgeMinSquaredDistance<ResultNumber>(other);
}

template <class PointType, class LabelType>
template <class ResultNumber, TriangleConcept OtherTriangle>
constexpr auto Triangle<PointType, LabelType>::squaredDistance(const OtherTriangle& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template edgeMinSquaredDistance<ResultNumber>(other);
}

// Convex

template <class PointType_, class LabelType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr auto Convex<PointType_, LabelType>::squaredDistance(const OtherPoint& point) const {
    using Num = NumberType;
    if (contains(point)) {
        return ResultNumber{};
    }
    if (size() <= 32) {
        auto edgeVector = edges();
        ResultNumber best = edgeVector[0].template squaredDistance<ResultNumber>(point);
        for (auto & e : edgeVector) {
            const ResultNumber current = e.template squaredDistance<ResultNumber>(point);
            if (current < best) {
                best = current;
            }
        }
        return best;
    }

    const std::ptrdiff_t n = static_cast<std::ptrdiff_t>(size());
    const auto V = [this](std::ptrdiff_t i) { return get(i); };
    const auto facing = [&](std::ptrdiff_t i) {
        return orientationDeterminant(V(i), V(i + 1), point) < 0;
    };

    // Reference point c = (v0 + v1 + v2) / 3, strictly interior. To stay exact we
    // work with vectors scaled by 3 (a positive factor, so signs are preserved).
    const Num sx = V(0).x() + V(1).x() + V(2).x();
    const Num sy = V(0).y() + V(1).y() + V(2).y();
    const Num bx = Num{3} * point.x() - sx;   // 3 * (q - c)
    const Num by = Num{3} * point.y() - sy;

    // Sign of (V(k) - c) x (q - c). Around the boundary this changes sign exactly
    // twice: at the edge the ray c->q exits through (which faces q) and at the
    // opposite edge (which does not).
    const auto dSign = [&](std::ptrdiff_t k) -> int {
        const Num ax = Num{3} * V(k).x() - sx;   // 3 * (V(k) - c)
        const Num ay = Num{3} * V(k).y() - sy;
        const Num cross = ax * by - ay * bx;
        return (cross > Num{0}) - (cross < Num{0});
    };
    // D(k) above is linear in V(k) with gradient (by, -bx); its argmax/argmin are
    // the genuinely unimodal support function, so they bracket the two sign
    // changes and are found in O(log n) with the existing cyclic search.
    const auto gDot = [&](std::ptrdiff_t k) { return V(k).x() * by - V(k).y() * bx; };
    const auto indices = std::views::iota(std::ptrdiff_t{0}, n);
    const std::ptrdiff_t mPos = *detail::cyclicMax(
        indices.begin(), indices.end(), [&](std::ptrdiff_t k) { return gDot(k); });
    const std::ptrdiff_t mNeg = *detail::cyclicMax(
        indices.begin(), indices.end(), [&](std::ptrdiff_t k) { return -gDot(k); });

    const auto wrap = [n](std::ptrdiff_t a) { return ((a % n) + n) % n; };
    // Largest offset in [0, len] for which pred(from + step * offset) holds; the
    // predicate is true...false over the bracket, so a binary search locates the
    // transition.
    const auto lastWhile =
        [&](std::ptrdiff_t from, std::ptrdiff_t step, std::ptrdiff_t len, auto pred) {
            std::ptrdiff_t lo = 0, hi = len;
            while (lo < hi) {
                const std::ptrdiff_t mid = (lo + hi + 1) / 2;
                if (pred(from + step * mid)) {
                    lo = mid;
                } else {
                    hi = mid - 1;
                }
            }
            return lo;
        };

    // Exit edge (faces q): the + -> - sign change between the two D extrema.
    const std::ptrdiff_t anchor = mPos + lastWhile(mPos, 1, wrap(mNeg - mPos),
        [&](std::ptrdiff_t k) { return dSign(k) > 0; });
    // Back edge (does not face q): the - -> + sign change on the other side.
    const std::ptrdiff_t back = mNeg + lastWhile(mNeg, 1, wrap(mPos - mNeg),
        [&](std::ptrdiff_t k) { return dSign(k) < 0; });

    // Grow the contiguous chain of q-facing edges around the anchor.
    const std::ptrdiff_t hiEdge = anchor + lastWhile(anchor, 1, wrap(back - anchor), facing);
    const std::ptrdiff_t loEdge = anchor - lastWhile(anchor, -1, wrap(anchor - back), facing);

    // Over the facing chain the per-edge distance is unimodal: pick the closest.
    const auto edgeDist = [&](std::ptrdiff_t i) {
        return Segment<PointType>(V(i), V(i + 1)).template squaredDistance<ResultNumber>(point);
    };
    std::ptrdiff_t lo = 0, hi = hiEdge - loEdge;
    while (lo < hi) {
        const std::ptrdiff_t mid = (lo + hi) / 2;
        if (edgeDist(loEdge + mid) < edgeDist(loEdge + mid + 1)) {
            hi = mid;
        } else {
            lo = mid + 1;
        }
    }
    return edgeDist(loEdge + lo);
}

template <class PointType_, class LabelType>
template <class ResultNumber, SegmentConcept OtherSegment>
constexpr auto Convex<PointType_, LabelType>::squaredDistance(const OtherSegment& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }

    if (size() <= 32) {
        auto edgeVector = edges();
        ResultNumber best = edgeVector[0].template squaredDistance<ResultNumber>(other);
        for (auto& e : edgeVector) {
            const ResultNumber current = e.template squaredDistance<ResultNumber>(other);
            if (current < best) {
                best = current;
            }
        }
        return best;
    }

    // Disjoint, large polygon. Two witnesses come from the segment endpoints; the
    // third (polygon vertex closest to the segment interior) lies on a vertex
    // extremal along the segment normal, found in O(log n).
    const std::ptrdiff_t n = static_cast<std::ptrdiff_t>(size());

    ResultNumber best = this->template squaredDistance<ResultNumber>(other.min());
    const ResultNumber from_max = this->template squaredDistance<ResultNumber>(other.max());
    if (from_max < best) {
        best = from_max;
    }

    // Support functional along the segment normal. The orientation determinant of
    // (segment.min, segment.max, vertex) is the signed-distance functional — it is
    // linear, so it is cyclic-unimodal over the CCW vertices and cyclicMax locates
    // its extrema; those two vertices are the polygon's nearest to the segment's
    // supporting line from either side, the only interior-witness candidates. The
    // orientation primitive keeps the coordinate promotion overflow-safe.
    const PointType a(other.min());
    const PointType b(other.max());
    const auto support = [&](std::ptrdiff_t i) {
        return orientationDeterminant(a, b, get(i));
    };
    const auto indices = std::views::iota(std::ptrdiff_t{0}, n);
    const std::ptrdiff_t hi = *detail::cyclicMax(
        indices.begin(), indices.end(), [&](std::ptrdiff_t k) { return support(k); });
    const std::ptrdiff_t lo = *detail::cyclicMax(
        indices.begin(), indices.end(), [&](std::ptrdiff_t k) { return -support(k); });

    const ResultNumber from_hi = other.template squaredDistance<ResultNumber>(get(hi));
    if (from_hi < best) {
        best = from_hi;
    }
    const ResultNumber from_lo = other.template squaredDistance<ResultNumber>(get(lo));
    if (from_lo < best) {
        best = from_lo;
    }

    return best;
}

template <class PointType_, class LabelType>
template <class ResultNumber, OrientedSegmentConcept OtherOrientedSegment>
constexpr auto Convex<PointType_, LabelType>::squaredDistance(const OtherOrientedSegment& other) const {
    return this->template squaredDistance<ResultNumber>(
        static_cast<Segment<typename OtherOrientedSegment::PointType>>(other));
}

template <class PointType_, class LabelType>
template <class ResultNumber, ConvexConcept OtherConvex>
constexpr auto Convex<PointType_, LabelType>::squaredDistance(const OtherConvex& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }

    // Disjoint convex polygons: the closest point of one lies on its boundary, so
    // the minimum over the edges of either polygon of the edge-to-polygon distance
    // is the polygon-to-polygon distance. Query the edges of the polygon with
    // fewer vertices to minimize the number of O(log n) segment searches.
    const auto minOverEdges = [](const auto& source, const auto& target) {
        const auto edgeVector = source.edges();
        ResultNumber best = target.template squaredDistance<ResultNumber>(edgeVector[0]);
        for (const auto& edge : edgeVector) {
            const ResultNumber current = target.template squaredDistance<ResultNumber>(edge);
            if (current < best) {
                best = current;
            }
        }
        return best;
    };

    if (size() <= other.size()) {
        return minOverEdges(*this, other);
    }
    return minOverEdges(other, *this);
}

template <class PointType_, class LabelType>
template <class ResultNumber, TriangleConcept OtherTriangle>
constexpr auto Convex<PointType_, LabelType>::squaredDistance(const OtherTriangle& other) const {
    return this->template squaredDistance<ResultNumber>(other.asConvex());
}

template <class PointType_, class LabelType>
template <class ResultNumber, RectangleConcept OtherRectangle>
constexpr auto Convex<PointType_, LabelType>::squaredDistance(const OtherRectangle& other) const {
    return this->template squaredDistance<ResultNumber>(other.asConvex());
}

template <class PointType_, class LabelType>
template <class ResultNumber, LineConcept OtherLine>
constexpr auto Convex<PointType_, LabelType>::squaredDistance(const OtherLine& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }

    if (size() <= 8) {
        ResultNumber best = other.template squaredDistance<ResultNumber>(get(0));
        for (std::ptrdiff_t i = 1; i < static_cast<std::ptrdiff_t>(size()); ++i) {
            const ResultNumber current = other.template squaredDistance<ResultNumber>(get(i));
            if (current < best) {
                best = current;
            }
        }
        return best;
    }

    // Disjoint line: the whole polygon lies on one side, so its closest point to
    // the line is the vertex extremal along the line normal toward the line. The
    // orientation determinant of (line.min, line.max, vertex) is the signed
    // distance functional — linear, hence cyclic-unimodal over the CCW vertices.
    // cyclicMax locates its two extrema (the polygon's nearest and farthest
    // vertices from the line), the only closest-vertex candidates. Using the
    // orientation primitive keeps the coordinate promotion overflow-safe.
    const std::ptrdiff_t n = static_cast<std::ptrdiff_t>(size());
    const PointType a(other.min());
    const PointType b(other.max());
    const auto support = [&](std::ptrdiff_t i) {
        return orientationDeterminant(a, b, get(i));
    };
    const auto indices = std::views::iota(std::ptrdiff_t{0}, n);
    const std::ptrdiff_t hi = *detail::cyclicMax(
        indices.begin(), indices.end(), [&](std::ptrdiff_t k) { return support(k); });
    const std::ptrdiff_t lo = *detail::cyclicMax(
        indices.begin(), indices.end(), [&](std::ptrdiff_t k) { return -support(k); });

    const ResultNumber from_hi = other.template squaredDistance<ResultNumber>(get(hi));
    const ResultNumber from_lo = other.template squaredDistance<ResultNumber>(get(lo));
    return from_hi < from_lo ? from_hi : from_lo;
}

template <class PointType_, class LabelType>
template <class ResultNumber, OrientedLineConcept OtherOrientedLine>
constexpr auto Convex<PointType_, LabelType>::squaredDistance(const OtherOrientedLine& other) const {
    return this->template squaredDistance<ResultNumber>(other.asLine());
}

template <class PointType_, class LabelType>
template <class ResultNumber, RayConcept OtherRay>
constexpr auto Convex<PointType_, LabelType>::squaredDistance(const OtherRay& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }

    if (size() <= 16) {
        auto edgeVector = edges();
        ResultNumber best = other.template squaredDistance<ResultNumber>(edgeVector[0]);
        for (const auto& e : edgeVector) {
            const ResultNumber current = other.template squaredDistance<ResultNumber>(e);
            if (current < best) {
                best = current;
            }
        }
        return best;
    }

    // Disjoint, large polygon. One witness comes from the ray's source; the other
    // (polygon vertex closest to the ray interior) lies on a vertex extremal along
    // the ray normal, found in O(log n). The orientation determinant of
    // (ray.source, ray.target, vertex) is the signed-distance functional — linear,
    // hence cyclic-unimodal over the CCW vertices — and stays overflow-safe.
    const std::ptrdiff_t n = static_cast<std::ptrdiff_t>(size());

    ResultNumber best = this->template squaredDistance<ResultNumber>(other.source());

    const PointType a(other.source());
    const PointType b(other.target());
    const auto support = [&](std::ptrdiff_t i) {
        return orientationDeterminant(a, b, get(i));
    };
    const auto indices = std::views::iota(std::ptrdiff_t{0}, n);
    const std::ptrdiff_t hi = *detail::cyclicMax(
        indices.begin(), indices.end(), [&](std::ptrdiff_t k) { return support(k); });
    const std::ptrdiff_t lo = *detail::cyclicMax(
        indices.begin(), indices.end(), [&](std::ptrdiff_t k) { return -support(k); });

    const ResultNumber from_hi = other.template squaredDistance<ResultNumber>(get(hi));
    if (from_hi < best) {
        best = from_hi;
    }
    const ResultNumber from_lo = other.template squaredDistance<ResultNumber>(get(lo));
    if (from_lo < best) {
        best = from_lo;
    }

    return best;
}

template <class PointType_, class LabelType>
template <class ResultNumber, HalfplaneConcept OtherHalfplane>
constexpr auto Convex<PointType_, LabelType>::squaredDistance(const OtherHalfplane& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }

    // Disjoint: the polygon lies entirely on the far side of the boundary line,
    // so the closest point of the half-plane is on its boundary.
    return this->template squaredDistance<ResultNumber>(other.asLine());
}

// -----------------------------------------------------------------------------
// Disk

namespace detail {

/**
 * @brief Squared distance from a disk to a disjoint shape.
 *
 * The caller has already established that the disk and @p other do not
 * intersect, so the nearest point of the disk lies on its circle and the
 * distance is `distance(center, other) - radius`. The squared distance is
 * generally irrational, hence the fixed `double` result. Evaluate the gap
 * directly as `sqrt(dc2) - radius`; this well-conditioned form avoids the
 * catastrophic cancellation of the algebraically equal
 * `dc2 + r2 - 2*sqrt(dc2 * r2)`.
 */
template <class DiskType, class OtherShape>
double diskExteriorSquaredDistance(const DiskType& disk, const OtherShape& other) {
    const double gap =
        std::sqrt(other.template squaredDistance<double>(disk.template center<double>()))
        - disk.template radius<double>();
    return gap * gap;
}

}  // namespace detail

template <class PointType_, class LabelType>
template <DiskConcept OtherDisk>
double Convex<PointType_, LabelType>::squaredDistance(const OtherDisk& other) const {
    if (intersects(other)) {
        return 0.0;
    }
    return detail::diskExteriorSquaredDistance(other, *this);
}

template <class PointType_, class TLabel>
template <PointConcept OtherPoint>
double Disk<PointType_, TLabel>::squaredDistance(const OtherPoint& point) const {
    if (contains(point)) {
        return 0.0;
    }

    // Exterior point: the nearest point of the disk is on the circle, so the
    // distance is |point - center| - radius. The squared distance is generally
    // irrational, hence the fixed double result. Evaluate the gap directly as
    // sqrt(dc2) - sqrt(r2); this well-conditioned form avoids the catastrophic
    // cancellation of the algebraically equal dc2 + r2 - 2*sqrt(dc2 * r2).
    const double gap = center<double>().template distance<double>(point) - radius<double>();
    return gap * gap;
}

template <class PointType_, class TLabel>
template <SegmentConcept OtherSegment>
double Disk<PointType_, TLabel>::squaredDistance(const OtherSegment& other) const {
    if (intersects(other)) {
        return 0.0;
    }
    return detail::diskExteriorSquaredDistance(*this, other);
}

template <class PointType_, class TLabel>
template <OrientedSegmentConcept OtherOrientedSegment>
double Disk<PointType_, TLabel>::squaredDistance(const OtherOrientedSegment& other) const {
    if (intersects(other)) {
        return 0.0;
    }
    return detail::diskExteriorSquaredDistance(*this, other);
}

template <class PointType_, class TLabel>
template <LineConcept OtherLine>
double Disk<PointType_, TLabel>::squaredDistance(const OtherLine& other) const {
    if (intersects(other)) {
        return 0.0;
    }
    return detail::diskExteriorSquaredDistance(*this, other);
}

template <class PointType_, class TLabel>
template <OrientedLineConcept OtherOrientedLine>
double Disk<PointType_, TLabel>::squaredDistance(const OtherOrientedLine& other) const {
    if (intersects(other)) {
        return 0.0;
    }
    return detail::diskExteriorSquaredDistance(*this, other);
}

template <class PointType_, class TLabel>
template <RayConcept OtherRay>
double Disk<PointType_, TLabel>::squaredDistance(const OtherRay& other) const {
    if (intersects(other)) {
        return 0.0;
    }
    return detail::diskExteriorSquaredDistance(*this, other);
}

template <class PointType_, class TLabel>
template <HalfplaneConcept OtherHalfplane>
double Disk<PointType_, TLabel>::squaredDistance(const OtherHalfplane& other) const {
    if (intersects(other)) {
        return 0.0;
    }
    return detail::diskExteriorSquaredDistance(*this, other);
}

template <class PointType_, class TLabel>
template <RectangleConcept OtherRectangle>
double Disk<PointType_, TLabel>::squaredDistance(const OtherRectangle& other) const {
    if (intersects(other)) {
        return 0.0;
    }
    return detail::diskExteriorSquaredDistance(*this, other);
}

template <class PointType_, class TLabel>
template <TriangleConcept OtherTriangle>
double Disk<PointType_, TLabel>::squaredDistance(const OtherTriangle& other) const {
    if (intersects(other)) {
        return 0.0;
    }
    return detail::diskExteriorSquaredDistance(*this, other);
}

template <class PointType_, class TLabel>
template <DiskConcept OtherDisk>
double Disk<PointType_, TLabel>::squaredDistance(const OtherDisk& other) const {
    if (intersects(other)) {
        return 0.0;
    }

    // Disjoint disks: the nearest points lie on the two circles along the line
    // through the centers, so the distance is the center separation minus both
    // radii. Subtracting the radii from the directly computed center distance is
    // well-conditioned (no catastrophic cancellation in the squared form).
    const double gap = center<double>().template distance<double>(other.template center<double>())
                       - radius<double>() - other.template radius<double>();
    return gap * gap;
}

// -----------------------------------------------------------------------------
// Polygon

template <class PointType_, class TLabel>
template <class ResultNumber, class OtherShape>
constexpr ResultNumber Polygon<PointType_, TLabel>::edgeMinSquaredDistance(const OtherShape& other) const {
    const auto boundaryEdges = edges();
    ResultNumber best = boundaryEdges[0].template squaredDistance<ResultNumber>(other);
    for (std::size_t index = 1; index < boundaryEdges.size(); ++index) {
        const ResultNumber current = boundaryEdges[index].template squaredDistance<ResultNumber>(other);
        if (current < best) {
            best = current;
        }
    }
    return best;
}

template <class PointType_, class TLabel>
template <class ResultNumber, PointConcept OtherPoint>
constexpr auto Polygon<PointType_, TLabel>::squaredDistance(const OtherPoint& point) const {
    if (intersects(point)) {
        return ResultNumber{};
    }
    return this->template edgeMinSquaredDistance<ResultNumber>(point);
}

template <class PointType_, class TLabel>
template <class ResultNumber, SegmentConcept OtherSegment>
constexpr auto Polygon<PointType_, TLabel>::squaredDistance(const OtherSegment& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template edgeMinSquaredDistance<ResultNumber>(other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, OrientedSegmentConcept OtherOrientedSegment>
constexpr auto Polygon<PointType_, TLabel>::squaredDistance(const OtherOrientedSegment& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template edgeMinSquaredDistance<ResultNumber>(other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, LineConcept OtherLine>
constexpr auto Polygon<PointType_, TLabel>::squaredDistance(const OtherLine& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template edgeMinSquaredDistance<ResultNumber>(other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, OrientedLineConcept OtherOrientedLine>
constexpr auto Polygon<PointType_, TLabel>::squaredDistance(const OtherOrientedLine& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template edgeMinSquaredDistance<ResultNumber>(other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, RayConcept OtherRay>
constexpr auto Polygon<PointType_, TLabel>::squaredDistance(const OtherRay& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template edgeMinSquaredDistance<ResultNumber>(other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, HalfplaneConcept OtherHalfplane>
constexpr auto Polygon<PointType_, TLabel>::squaredDistance(const OtherHalfplane& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template edgeMinSquaredDistance<ResultNumber>(other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, RectangleConcept OtherRectangle>
constexpr auto Polygon<PointType_, TLabel>::squaredDistance(const OtherRectangle& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template edgeMinSquaredDistance<ResultNumber>(other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, TriangleConcept OtherTriangle>
constexpr auto Polygon<PointType_, TLabel>::squaredDistance(const OtherTriangle& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template edgeMinSquaredDistance<ResultNumber>(other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, ConvexConcept OtherConvex>
constexpr auto Polygon<PointType_, TLabel>::squaredDistance(const OtherConvex& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template edgeMinSquaredDistance<ResultNumber>(other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, PolygonConcept OtherPolygon>
constexpr auto Polygon<PointType_, TLabel>::squaredDistance(const OtherPolygon& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template edgeMinSquaredDistance<ResultNumber>(other);
}

template <class PointType_, class TLabel>
template <class DiskPointType, class DiskLabel>
double Polygon<PointType_, TLabel>::squaredDistance(const Disk<DiskPointType, DiskLabel>& disk) const {
    if (intersects(disk)) {
        return 0.0;
    }
    return detail::diskExteriorSquaredDistance(disk, *this);
}

}  // namespace pgl
