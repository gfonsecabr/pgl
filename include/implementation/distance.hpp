#pragma once

/**
 * @file distance.hpp
 * @brief Distance and Hausdorff-style measurements between shapes.
 */

#include <algorithm>
#include <cmath>

#include "../pgl.hpp"

namespace pgl {

// -----------------------------------------------------------------------------
// Point

template <class Number, class Label>
template<PointConcept OtherPoint>
constexpr auto Point<Number, Label>::squaredDistance(const OtherPoint& other) const {
    const auto dx = x() - other.x();
    const auto dy = y() - other.y();
    return dx * dx + dy * dy;
}

template <class Number, class Label>
template <class ApproximateNumber, class OtherPoint>
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

template <class PointType>
template<PointConcept OtherPoint>
constexpr auto Segment<PointType>::squaredDistance(const OtherPoint& point) const {
    using ResultNumber = std::common_type_t<NumberType, typename OtherPoint::NumberType, double>;
    const Point<ResultNumber> a(min());
    const Point<ResultNumber> b(max());
    const Point<ResultNumber> p(point);

    const auto ab = b - a;
    const auto ap = p - a;
    const auto bp = p - b;
    const auto squared_length = ab * ab;

    if (squared_length == ResultNumber{}) {
        return static_cast<ResultNumber>(a.squaredDistance(p));
    }

    const ResultNumber projection_numerator = ap * ab;
    if (projection_numerator <= ResultNumber{}) {
        return ap * ap;
    }

    if (projection_numerator >= squared_length) {
        return bp * bp;
    }

    const ResultNumber twice_triangle_area = ab.x() * ap.y() - ab.y() * ap.x();
    return (twice_triangle_area * twice_triangle_area) / squared_length;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr auto Segment<PointType>::squaredDistance(const Segment<OtherPoint>& other) const {
    if (intersects(other)) {
        using ResultNumber = std::common_type_t<NumberType, typename OtherPoint::NumberType, double>;
        return ResultNumber{};
    }

    const auto this_min_to_other = other.squaredDistance(min());
    const auto this_max_to_other = other.squaredDistance(max());
    const auto other_min_to_this = squaredDistance(other.min());
    const auto other_max_to_this = squaredDistance(other.max());

    const auto best_from_this = this_min_to_other < this_max_to_other ? this_min_to_other : this_max_to_other;
    const auto best_from_other = other_min_to_this < other_max_to_this ? other_min_to_this : other_max_to_this;

    return best_from_this < best_from_other ? best_from_this : best_from_other;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr auto Segment<PointType>::squaredHausdorffDistance(const Segment<OtherPoint>& other) const {
    const auto this_min_to_other = other.squaredDistance(min());
    const auto this_max_to_other = other.squaredDistance(max());
    const auto other_min_to_this = squaredDistance(other.min());
    const auto other_max_to_this = squaredDistance(other.max());

    const auto worst_from_this = this_min_to_other > this_max_to_other ? this_min_to_other : this_max_to_other;
    const auto worst_from_other = other_min_to_this > other_max_to_this ? other_min_to_this : other_max_to_this;

    return worst_from_this > worst_from_other ? worst_from_this : worst_from_other;
}

// -----------------------------------------------------------------------------
// OrientedSegment

template <class PointType>
template<PointConcept OtherPoint>
constexpr auto OrientedSegment<PointType>::squaredDistance(const OtherPoint& point) const {
    return static_cast<Segment<PointType>>(*this).squaredDistance(point);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr auto OrientedSegment<PointType>::squaredDistance(const Segment<OtherPoint>& other) const {
    return static_cast<Segment<PointType>>(*this).squaredDistance(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr auto OrientedSegment<PointType>::squaredDistance(const OrientedSegment<OtherPoint>& other) const {
    return static_cast<Segment<PointType>>(*this).squaredDistance(static_cast<Segment<OtherPoint>>(other));
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr auto OrientedSegment<PointType>::squaredHausdorffDistance(const Segment<OtherPoint>& other) const {
    return static_cast<Segment<PointType>>(*this).squaredHausdorffDistance(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr auto OrientedSegment<PointType>::squaredHausdorffDistance(const OrientedSegment<OtherPoint>& other) const {
    return static_cast<Segment<PointType>>(*this).squaredHausdorffDistance(
        static_cast<Segment<OtherPoint>>(other));
}

// -----------------------------------------------------------------------------
// Line

template <class PointType>
template<PointConcept OtherPoint>
constexpr auto Line<PointType>::squaredDistance(const OtherPoint& point) const {
    using ResultNumber = std::common_type_t<NumberType, typename OtherPoint::NumberType, double>;

    if (isDegenerate()) {
        return static_cast<ResultNumber>(min().squaredDistance(point));
    }

    const Point<ResultNumber> a(min());
    const Point<ResultNumber> b(max());
    const Point<ResultNumber> p(point);
    const auto ab = b - a;
    const auto ap = p - a;
    const ResultNumber twice_triangle_area = ab.x() * ap.y() - ab.y() * ap.x();
    const ResultNumber squared_length = ab * ab;
    return (twice_triangle_area * twice_triangle_area) / squared_length;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr auto Line<PointType>::squaredDistance(const Line<OtherPoint>& other) const {
    using ResultNumber = std::common_type_t<NumberType, typename OtherPoint::NumberType, double>;

    if (intersects(other)) {
        return ResultNumber{};
    }

    return static_cast<ResultNumber>(other.squaredDistance(min()));
}

// -----------------------------------------------------------------------------
// OrientedLine

template <class PointType>
template<PointConcept OtherPoint>
constexpr auto OrientedLine<PointType>::squaredDistance(const OtherPoint& point) const {
    return this->asLine().squaredDistance(point);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr auto OrientedLine<PointType>::squaredDistance(const Line<OtherPoint>& other) const {
    return this->asLine().squaredDistance(other);
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr auto OrientedLine<PointType>::squaredDistance(const OrientedLine<OtherPoint>& other) const {
    return this->asLine().squaredDistance(other.asLine());
}

// -----------------------------------------------------------------------------
// Ray

template <class PointType>
template<PointConcept OtherPoint>
constexpr auto Ray<PointType>::squaredDistance(const OtherPoint& point) const {
    using ResultNumber = std::common_type_t<NumberType, typename OtherPoint::NumberType, double>;

    if (isDegenerate()) {
        return static_cast<ResultNumber>(source().squaredDistance(point));
    }

    const Point<ResultNumber> a(source());
    const Point<ResultNumber> b(target());
    const Point<ResultNumber> p(point);
    const auto ab = b - a;
    const auto ap = p - a;
    const auto squared_length = ab * ab;

    if (squared_length == ResultNumber{}) {
        return ap * ap;
    }

    const ResultNumber projection_numerator = ap * ab;
    if (projection_numerator <= ResultNumber{}) {
        return ap * ap;
    }

    const ResultNumber twice_triangle_area = ab.x() * ap.y() - ab.y() * ap.x();
    return (twice_triangle_area * twice_triangle_area) / squared_length;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr auto Ray<PointType>::squaredDistance(const Line<OtherPoint>& other) const {
    using ResultNumber = std::common_type_t<NumberType, typename OtherPoint::NumberType, double>;

    if (intersects(other)) {
        return ResultNumber{};
    }

    return static_cast<ResultNumber>(other.squaredDistance(source()));
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr auto Ray<PointType>::squaredDistance(const OrientedLine<OtherPoint>& other) const {
    using ResultNumber = std::common_type_t<NumberType, typename OtherPoint::NumberType, double>;

    if (intersects(other)) {
        return ResultNumber{};
    }

    return static_cast<ResultNumber>(other.squaredDistance(source()));
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr auto Ray<PointType>::squaredDistance(const Segment<OtherPoint>& other) const {
    if (intersects(other)) {
        using ResultNumber = std::common_type_t<NumberType, typename OtherPoint::NumberType, double>;
        return ResultNumber{};
    }

    const auto source_to_other = other.squaredDistance(source());
    const auto other_min_to_this = squaredDistance(other.min());
    const auto other_max_to_this = squaredDistance(other.max());
    const auto best_from_segment = other_min_to_this < other_max_to_this ? other_min_to_this : other_max_to_this;
    return source_to_other < best_from_segment ? source_to_other : best_from_segment;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr auto Ray<PointType>::squaredDistance(const OrientedSegment<OtherPoint>& other) const {
    if (intersects(other)) {
        using ResultNumber = std::common_type_t<NumberType, typename OtherPoint::NumberType, double>;
        return ResultNumber{};
    }

    const auto source_to_other = other.squaredDistance(source());
    const auto other_source_to_this = squaredDistance(other.source());
    const auto other_target_to_this = squaredDistance(other.target());
    const auto best_from_segment = other_source_to_this < other_target_to_this ? other_source_to_this : other_target_to_this;
    return source_to_other < best_from_segment ? source_to_other : best_from_segment;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr auto Ray<PointType>::squaredDistance(const Ray<OtherPoint>& other) const {
    using ResultNumber = std::common_type_t<NumberType, typename OtherPoint::NumberType, double>;

    if (intersects(other)) {
        return ResultNumber{};
    }

    const auto this_source_to_other = other.squaredDistance(source());
    const auto other_source_to_this = squaredDistance(other.source());
    return this_source_to_other < other_source_to_this ? this_source_to_other : other_source_to_this;
}

// -----------------------------------------------------------------------------
// Rectangle

template <class PointType>
template <class Left, class Right>
constexpr auto Rectangle<PointType>::axisDistance(const Left& first_min, const Left& first_max, const Right& second_min, const Right& second_max)
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

template <class PointType>
template<PointConcept OtherPoint>
constexpr auto Rectangle<PointType>::maxVertexDistanceTo(const Rectangle<OtherPoint>& other) const {
    const auto rectangle_vertices = vertices();
    auto worst_distance = other.squaredDistance(rectangle_vertices[0]);

    for (std::size_t index = 1; index < rectangle_vertices.size(); ++index) {
        const auto current_distance = other.squaredDistance(rectangle_vertices[index]);
        if (worst_distance < current_distance) {
            worst_distance = current_distance;
        }
    }

    return worst_distance;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr auto Rectangle<PointType>::squaredDistance(const OtherPoint& point) const {
    const auto dx = axisDistance(min().x(), max().x(), point.x(), point.x());
    const auto dy = axisDistance(min().y(), max().y(), point.y(), point.y());
    return dx * dx + dy * dy;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr auto Rectangle<PointType>::squaredDistance(const Line<OtherPoint>& other) const {
    using ResultNumber = std::common_type_t<NumberType, typename OtherPoint::NumberType, double>;

    if (intersects(other)) {
        return ResultNumber{};
    }

    const auto rectangle_vertices = vertices();
    auto best_distance = static_cast<ResultNumber>(other.squaredDistance(rectangle_vertices[0]));
    for (std::size_t index = 1; index < rectangle_vertices.size(); ++index) {
        const auto current_distance = static_cast<ResultNumber>(other.squaredDistance(rectangle_vertices[index]));
        if (current_distance < best_distance) {
            best_distance = current_distance;
        }
    }

    return best_distance;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr auto Rectangle<PointType>::squaredDistance(const OrientedLine<OtherPoint>& other) const {
    return squaredDistance(other.asLine());
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr auto Rectangle<PointType>::squaredDistance(const Segment<OtherPoint>& other) const {
    using ResultNumber = std::common_type_t<NumberType, typename OtherPoint::NumberType, double>;

    if (intersects(other)) {
        return ResultNumber{};
    }

    auto best_distance = static_cast<ResultNumber>(squaredDistance(other.min()));
    const auto other_max_distance = static_cast<ResultNumber>(squaredDistance(other.max()));
    if (other_max_distance < best_distance) {
        best_distance = other_max_distance;
    }

    const auto rectangle_edges = edges();
    for (const auto& edge : rectangle_edges) {
        const auto current_distance = static_cast<ResultNumber>(edge.squaredDistance(other));
        if (current_distance < best_distance) {
            best_distance = current_distance;
        }
    }

    return best_distance;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr auto Rectangle<PointType>::squaredDistance(const OrientedSegment<OtherPoint>& other) const {
    return squaredDistance(static_cast<Segment<OtherPoint>>(other));
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr auto Rectangle<PointType>::squaredDistance(const Ray<OtherPoint>& other) const {
    using ResultNumber = std::common_type_t<NumberType, typename OtherPoint::NumberType, double>;

    if (intersects(other)) {
        return ResultNumber{};
    }

    auto best_distance = static_cast<ResultNumber>(squaredDistance(other.source()));
    const auto rectangle_edges = edges();
    for (const auto& edge : rectangle_edges) {
        const auto current_distance = static_cast<ResultNumber>(other.squaredDistance(edge));
        if (current_distance < best_distance) {
            best_distance = current_distance;
        }
    }

    return best_distance;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr auto Rectangle<PointType>::squaredDistance(const Rectangle<OtherPoint>& other) const {
    const auto dx = axisDistance(min().x(), max().x(), other.min().x(), other.max().x());
    const auto dy = axisDistance(min().y(), max().y(), other.min().y(), other.max().y());
    return dx * dx + dy * dy;
}

template <class PointType>
template<PointConcept OtherPoint>
constexpr auto Rectangle<PointType>::squaredHausdorffDistance(const Rectangle<OtherPoint>& other) const {
    const auto worst_from_this = maxVertexDistanceTo(other);
    const auto worst_from_other = other.maxVertexDistanceTo(*this);
    return worst_from_this < worst_from_other ? worst_from_other : worst_from_this;
}

}  // namespace pgl
