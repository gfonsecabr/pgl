#pragma once

#include "pgl.hpp"

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

}  // namespace pgl
