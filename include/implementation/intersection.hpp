#pragma once

#ifndef PGL_HPP_INCLUDED
#error "Do not include this Pangolin header directly; include \"pgl.hpp\" instead."
#endif

/**
 * @file intersection.hpp
 * @brief Implementations of the 'intersection' predicate.
 */

#include <algorithm>
#include <cassert>
#include <map>
#include <set>


namespace pgl {

// -----------------------------------------------------------------------------
// Point

template <class Number, class Label>
template <class ResultNumber, PointConcept OtherPoint>
constexpr std::optional<Point<ResultNumber, Label>>
Point<Number, Label>::intersection(const OtherPoint& other) const {
    if (contains(other)) {
        return Point<ResultNumber, Label>(*this);
    }
    return {};
}

// -----------------------------------------------------------------------------
// Segment

template <class PointType, class LabelType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr std::optional<Point<ResultNumber, typename PointType::LabelType>>
Segment<PointType, LabelType>::intersection(const OtherPoint& other) const {
    if (contains(other)) {
        return Point<ResultNumber, typename PointType::LabelType>(other);
    }
    return {};
}

template <class PointType, class LabelType>
template <class ResultNumber, SegmentConcept OtherSegment>
constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>>>
Segment<PointType, LabelType>::intersection(const OtherSegment& other) const {
    using ResultPoint = Point<ResultNumber, typename PointType::LabelType>;
    using ResultSegment = Segment<ResultPoint>;

    if (!boundingBoxesOverlap(other)) {
        return {};
    }

    const auto d1 = orientationSign(min(), max(), other.min());
    const auto d2 = orientationSign(min(), max(), other.max());

    if (d1 == 0 && d2 == 0) {
        // Both segments are collinear: compare endpoints in ResultPoint so the
        // ternary and ordering stay well-typed when PointType and the other
        // segment's point type
        // differ (e.g. Convex chord clipped from int into Rational).
        const ResultPoint a_min(min());
        const ResultPoint a_max(max());
        const ResultPoint b_min(other.min());
        const ResultPoint b_max(other.max());
        const ResultPoint pminmax = a_max < b_max ? a_max : b_max;
        const ResultPoint pmaxmin = a_min < b_min ? b_min : a_min;

        if (pminmax < pmaxmin) {
            return {};
        }
        if (pminmax == pmaxmin) {
            return pminmax;
        }

        return ResultSegment(pminmax, pmaxmin);
    }

    if (d1 == 0 && containsCollinear(other.min())) {
        return Point<ResultNumber>(other.min());
    }
    if (d2 == 0 && containsCollinear(other.max())) {
        return Point<ResultNumber>(other.max());
    }

    const auto d3 = orientationSign(other.min(), other.max(), min());
    if (d3 == 0 && other.containsCollinear(min())) {
        return Point<ResultNumber>(min());
    }

    const auto d4 = orientationSign(other.min(), other.max(), max());
    if (d4 == 0 && other.containsCollinear(max())) {
        return Point<ResultNumber>(max());
    }

    if (d1 == 0 || d2 == 0 || d3 == 0 || d4 == 0) {
        return {};
    }

    if (d1 != d2 && d3 != d4) {
        const auto x1 = static_cast<ResultNumber>(min().x());
        const auto y1 = static_cast<ResultNumber>(min().y());
        const auto x2 = static_cast<ResultNumber>(max().x());
        const auto y2 = static_cast<ResultNumber>(max().y());
        const auto x3 = static_cast<ResultNumber>(other.min().x());
        const auto y3 = static_cast<ResultNumber>(other.min().y());
        const auto x4 = static_cast<ResultNumber>(other.max().x());
        const auto y4 = static_cast<ResultNumber>(other.max().y());

        const ResultNumber determinant = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);
        const ResultNumber line1 = x1 * y2 - y1 * x2;
        const ResultNumber line2 = x3 * y4 - y3 * x4;

        const ResultNumber px = (line1 * (x3 - x4) - (x1 - x2) * line2) / determinant;
        const ResultNumber py = (line1 * (y3 - y4) - (y1 - y2) * line2) / determinant;

        return Point<ResultNumber, typename PointType::LabelType>(px, py);
    }

    return {};
}

// -----------------------------------------------------------------------------
// OrientedSegment

template <class PointType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr std::optional<Point<ResultNumber, typename PointType::LabelType>>
OrientedSegment<PointType>::intersection(const OtherPoint& other) const {
    return static_cast<Segment<PointType>>(*this).template intersection<ResultNumber>(other);
}

template <class PointType>
template <class ResultNumber, PointConcept OtherPoint, class OtherLabel>
constexpr auto OrientedSegment<PointType>::intersection(const Segment<OtherPoint, OtherLabel>& other) const {
    return static_cast<Segment<PointType>>(*this).template intersection<ResultNumber>(other);
}

template <class PointType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr auto OrientedSegment<PointType>::intersection(const OrientedSegment<OtherPoint>& other) const {
    return static_cast<Segment<PointType>>(*this).template intersection<ResultNumber>(
        static_cast<Segment<OtherPoint>>(other));
}

// -----------------------------------------------------------------------------
// Line

template <class PointType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr std::optional<Point<ResultNumber, typename PointType::LabelType>>
Line<PointType>::intersection(const OtherPoint& other) const {
    if (contains(other)) {
        return Point<ResultNumber, typename PointType::LabelType>(other);
    }
    return {};
}

template <class PointType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr std::optional<
    std::variant<
        Point<ResultNumber, typename PointType::LabelType>,
        Line<Point<ResultNumber, typename PointType::LabelType>>>>
Line<PointType>::intersection(const Line<OtherPoint>& other) const {
    using ResultPoint = Point<ResultNumber, typename PointType::LabelType>;
    using ResultLine = Line<ResultPoint>;

    if (isDegenerate()) {
        if (other.contains(min())) {
            return ResultPoint(min());
        }
        return {};
    }

    if (other.isDegenerate()) {
        if (contains(other.min())) {
            return ResultPoint(other.min());
        }
        return {};
    }

    if (contains(other)) {
        return ResultLine(ResultPoint(min()), ResultPoint(max()));
    }

    if (parallel(other)) {
        return {};
    }

    const auto x1 = static_cast<ResultNumber>(min().x());
    const auto y1 = static_cast<ResultNumber>(min().y());
    const auto x2 = static_cast<ResultNumber>(max().x());
    const auto y2 = static_cast<ResultNumber>(max().y());
    const auto x3 = static_cast<ResultNumber>(other.min().x());
    const auto y3 = static_cast<ResultNumber>(other.min().y());
    const auto x4 = static_cast<ResultNumber>(other.max().x());
    const auto y4 = static_cast<ResultNumber>(other.max().y());

    const ResultNumber determinant = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);
    const ResultNumber line1 = x1 * y2 - y1 * x2;
    const ResultNumber line2 = x3 * y4 - y3 * x4;

    const ResultNumber px = (line1 * (x3 - x4) - (x1 - x2) * line2) / determinant;
    const ResultNumber py = (line1 * (y3 - y4) - (y1 - y2) * line2) / determinant;

    return ResultPoint(px, py);
}

template <class PointType>
template <class ResultNumber, PointConcept OtherPoint, class OtherLabel>
constexpr auto Line<PointType>::intersection(const Segment<OtherPoint, OtherLabel>& other) const {
    using ResultPoint = Point<ResultNumber, typename PointType::LabelType>;
    using ResultSegment = Segment<ResultPoint>;
    using ResultType = std::optional<std::variant<ResultPoint, ResultSegment>>;

    const auto line_intersection = intersection<ResultNumber>(Line<OtherPoint>(other.min(), other.max()));
    if (!line_intersection) {
        return ResultType{};
    }
    if (std::holds_alternative<ResultPoint>(*line_intersection)) {
        const auto& point = std::get<ResultPoint>(*line_intersection);
        if (other.contains(point)) {
            return ResultType(point);
        }
        return ResultType{};
    }
    return ResultType(ResultSegment(ResultPoint(other.min()), ResultPoint(other.max())));
}

template <class PointType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr auto Line<PointType>::intersection(const OrientedSegment<OtherPoint>& other) const {
    return intersection<ResultNumber>(static_cast<Segment<OtherPoint>>(other));
}

// -----------------------------------------------------------------------------
// OrientedLine

template <class PointType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr std::optional<Point<ResultNumber, typename PointType::LabelType>>
OrientedLine<PointType>::intersection(const OtherPoint& other) const {
    return this->asLine().template intersection<ResultNumber>(other);
}

template <class PointType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr std::optional<
    std::variant<
        Point<ResultNumber, typename PointType::LabelType>,
        Line<Point<ResultNumber, typename PointType::LabelType>>>>
OrientedLine<PointType>::intersection(const Line<OtherPoint>& other) const {
    return this->asLine().template intersection<ResultNumber>(other);
}

template <class PointType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr std::optional<
    std::variant<
        Point<ResultNumber, typename PointType::LabelType>,
        Line<Point<ResultNumber, typename PointType::LabelType>>>>
OrientedLine<PointType>::intersection(const OrientedLine<OtherPoint>& other) const {
    return this->asLine().template intersection<ResultNumber>(
        other.asLine());
}

template <class PointType>
template <class ResultNumber, PointConcept OtherPoint, class OtherLabel>
constexpr auto OrientedLine<PointType>::intersection(const Segment<OtherPoint, OtherLabel>& other) const {
    return this->asLine().template intersection<ResultNumber>(other);
}

template <class PointType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr auto OrientedLine<PointType>::intersection(const OrientedSegment<OtherPoint>& other) const {
    return this->asLine().template intersection<ResultNumber>(other);
}

// -----------------------------------------------------------------------------
// Ray

template <class PointType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr std::optional<Point<ResultNumber, typename PointType::LabelType>>
Ray<PointType>::intersection(const OtherPoint& other) const {
    if (contains(other)) {
        return Point<ResultNumber, typename PointType::LabelType>(other);
    }
    return {};
}

template <class PointType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr std::optional<
    std::variant<
        Point<ResultNumber, typename PointType::LabelType>,
        Ray<Point<ResultNumber, typename PointType::LabelType>>>>
Ray<PointType>::intersection(const Line<OtherPoint>& other) const {
    using ResultPoint = Point<ResultNumber, typename PointType::LabelType>;
    if (isDegenerate()) {
        if (other.contains(source())) {
            return ResultPoint(source());
        }
        return {};
    }

    const auto line_intersection = this->asLine().template intersection<ResultNumber>(other);
    if (!line_intersection) {
        return {};
    }

    if (std::holds_alternative<Line<ResultPoint>>(*line_intersection)) {
        return Ray<ResultPoint>(ResultPoint(source()), ResultPoint(target()));
    }

    const auto& point = std::get<ResultPoint>(*line_intersection);
    if (contains(point)) {
        return point;
    }

    return {};
}

template <class PointType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr std::optional<
    std::variant<
        Point<ResultNumber, typename PointType::LabelType>,
        Ray<Point<ResultNumber, typename PointType::LabelType>>>>
Ray<PointType>::intersection(const OrientedLine<OtherPoint>& other) const {
    return intersection<ResultNumber>(other.asLine());
}

template <class PointType>
template <class ResultNumber, PointConcept OtherPoint, class OtherLabel>
constexpr std::optional<
    std::variant<
        Point<ResultNumber, typename PointType::LabelType>,
        Segment<Point<ResultNumber, typename PointType::LabelType>>>>
Ray<PointType>::intersection(const Segment<OtherPoint, OtherLabel>& other) const {
    using ResultPoint = Point<ResultNumber, typename PointType::LabelType>;
    using ResultSegment = Segment<ResultPoint>;
    if (isDegenerate()) {
        if (other.contains(source())) {
            return ResultPoint(source());
        }
        return {};
    }

    const auto line_intersection =
        this->asLine().template intersection<ResultNumber>(
            Line<OtherPoint>(other.min(), other.max()));
    if (!line_intersection) {
        return {};
    }

    if (std::holds_alternative<ResultPoint>(*line_intersection)) {
        const auto& point = std::get<ResultPoint>(*line_intersection);
        if (contains(point) && other.contains(point)) {
            return point;
        }
        return {};
    }

    const ResultPoint ray_source(source());
    const bool min_on_ray = contains(other.min());
    const bool max_on_ray = contains(other.max());

    if (min_on_ray && max_on_ray) {
        return ResultSegment(ResultPoint(other.min()), ResultPoint(other.max()));
    }

    if (min_on_ray) {
        const auto point = ResultPoint(other.min());
        if (other.contains(ray_source) && point != ray_source) {
            return ResultSegment(point, ray_source);
        }
        return point;
    }

    if (max_on_ray) {
        const auto point = ResultPoint(other.max());
        if (other.contains(ray_source) && point != ray_source) {
            return ResultSegment(point, ray_source);
        }
        return point;
    }

    if (other.contains(ray_source)) {
        return ray_source;
    }

    return {};
}

template <class PointType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr std::optional<
    std::variant<
        Point<ResultNumber, typename PointType::LabelType>,
        Segment<Point<ResultNumber, typename PointType::LabelType>>>>
Ray<PointType>::intersection(const OrientedSegment<OtherPoint>& other) const {
    return intersection<ResultNumber>(static_cast<Segment<OtherPoint>>(other));
}

template <class PointType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr std::optional<
    std::variant<
        Point<ResultNumber, typename PointType::LabelType>,
        Segment<Point<ResultNumber, typename PointType::LabelType>>,
        Ray<Point<ResultNumber, typename PointType::LabelType>>>>
Ray<PointType>::intersection(const Ray<OtherPoint>& other) const {
    using ResultPoint = Point<ResultNumber, typename PointType::LabelType>;
    using ResultSegment = Segment<ResultPoint>;
    using ResultRay = Ray<ResultPoint>;
    if (isDegenerate()) {
        if (other.contains(source())) {
            return ResultPoint(source());
        }
        return {};
    }

    if (other.isDegenerate()) {
        if (contains(other.source())) {
            return ResultPoint(other.source());
        }
        return {};
    }

    const auto line_intersection =
        this->asLine().template intersection<ResultNumber>(
            other.asLine());
    if (!line_intersection) {
        return {};
    }

    if (std::holds_alternative<ResultPoint>(*line_intersection)) {
        const auto& point = std::get<ResultPoint>(*line_intersection);
        if (contains(point) && other.contains(point)) {
            return point;
        }
        return {};
    }

    if (source() == other.source()) {
        if (contains(other.target())) {
            return ResultRay(ResultPoint(source()), ResultPoint(target()));
        }
        return ResultPoint(source());
    }

    const bool this_contains_other_source = contains(other.source());
    const bool other_contains_this_source = other.contains(source());

    if (this_contains_other_source && other_contains_this_source) {
        return ResultSegment(ResultPoint(source()), ResultPoint(other.source()));
    }

    if (this_contains_other_source) {
        return ResultRay(ResultPoint(other.source()), ResultPoint(other.target()));
    }

    if (other_contains_this_source) {
        return ResultRay(ResultPoint(source()), ResultPoint(target()));
    }

    return {};
}

// -----------------------------------------------------------------------------
// Halfplane

namespace detail {

template <class ResultPoint>
constexpr ResultPoint extendRayAlongLine(const ResultPoint& intersection, const ResultPoint& first, const ResultPoint& second) {
    return intersection + (second - first);
}

}  // namespace detail

template <class PointType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr std::optional<Point<ResultNumber, typename PointType::LabelType>>
Halfplane<PointType>::intersection(const OtherPoint& other) const {
    if (contains(other)) {
        return Point<ResultNumber, typename PointType::LabelType>(other);
    }
    return {};
}

template <class PointType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr std::optional<
    std::variant<
        Point<ResultNumber, typename PointType::LabelType>,
        Line<Point<ResultNumber, typename PointType::LabelType>>,
        Ray<Point<ResultNumber, typename PointType::LabelType>>>>
Halfplane<PointType>::intersection(const Line<OtherPoint>& other) const {
    using ResultPoint = Point<ResultNumber, typename PointType::LabelType>;
    using ResultLine = Line<ResultPoint>;
    using ResultRay = Ray<ResultPoint>;

    if (isDegenerate()) {
        if (other.contains(source())) {
            return ResultPoint(source());
        }
        return {};
    }

    if (other.isDegenerate()) {
        if (contains(other.min())) {
            return ResultPoint(other.min());
        }
        return {};
    }

    const auto boundary_intersection =
        this->asLine().template intersection<ResultNumber>(other);

    if (!boundary_intersection) {
        if (contains(other.min())) {
            return ResultLine(ResultPoint(other.min()), ResultPoint(other.max()));
        }
        return {};
    }

    if (std::holds_alternative<ResultPoint>(*boundary_intersection)) {
        const auto& point = std::get<ResultPoint>(*boundary_intersection);
        const ResultPoint first(other.min());
        const ResultPoint second(other.max());
        const auto direction_side =
            orientationDeterminant(source(), target(), other.max()) -
            orientationDeterminant(source(), target(), other.min());
        const auto zero = decltype(direction_side){};

        if (zero < direction_side) {
            return ResultRay(point, detail::extendRayAlongLine(point, first, second));
        }

        return ResultRay(point, detail::extendRayAlongLine(point, second, first));
    }

    return ResultLine(ResultPoint(other.min()), ResultPoint(other.max()));
}

template <class PointType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr std::optional<
    std::variant<
        Point<ResultNumber, typename PointType::LabelType>,
        Line<Point<ResultNumber, typename PointType::LabelType>>,
        Ray<Point<ResultNumber, typename PointType::LabelType>>>>
Halfplane<PointType>::intersection(const OrientedLine<OtherPoint>& other) const {
    return intersection<ResultNumber>(other.asLine());
}

template <class PointType>
template <class ResultNumber, PointConcept OtherPoint, class OtherLabel>
constexpr std::optional<
    std::variant<
        Point<ResultNumber, typename PointType::LabelType>,
        Segment<Point<ResultNumber, typename PointType::LabelType>>>>
Halfplane<PointType>::intersection(const Segment<OtherPoint, OtherLabel>& other) const {
    using ResultPoint = Point<ResultNumber, typename PointType::LabelType>;
    using ResultSegment = Segment<ResultPoint>;
    using ResultLine = Line<ResultPoint>;
    using ResultRay = Ray<ResultPoint>;

    if (isDegenerate()) {
        if (other.contains(source())) {
            return ResultPoint(source());
        }
        return {};
    }

    if (other.isDegenerate()) {
        if (contains(other.min())) {
            return ResultPoint(other.min());
        }
        return {};
    }

    const auto supporting_intersection =
        intersection<ResultNumber>(Line<OtherPoint>(other.min(), other.max()));
    if (!supporting_intersection) {
        return {};
    }

    if (std::holds_alternative<ResultPoint>(*supporting_intersection)) {
        const auto& point = std::get<ResultPoint>(*supporting_intersection);
        if (other.contains(point)) {
            return point;
        }
        return {};
    }

    if (std::holds_alternative<ResultLine>(*supporting_intersection)) {
        return ResultSegment(ResultPoint(other.min()), ResultPoint(other.max()));
    }

    return std::get<ResultRay>(*supporting_intersection).template intersection<ResultNumber>(other);
}

template <class PointType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr std::optional<
    std::variant<
        Point<ResultNumber, typename PointType::LabelType>,
        Segment<Point<ResultNumber, typename PointType::LabelType>>>>
Halfplane<PointType>::intersection(const OrientedSegment<OtherPoint>& other) const {
    return intersection<ResultNumber>(static_cast<Segment<OtherPoint>>(other));
}

template <class PointType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr std::optional<
    std::variant<
        Point<ResultNumber, typename PointType::LabelType>,
        Segment<Point<ResultNumber, typename PointType::LabelType>>,
        Ray<Point<ResultNumber, typename PointType::LabelType>>>>
Halfplane<PointType>::intersection(const Ray<OtherPoint>& other) const {
    using ResultPoint = Point<ResultNumber, typename PointType::LabelType>;
    using ResultRay = Ray<ResultPoint>;

    if (isDegenerate()) {
        if (other.contains(source())) {
            return ResultPoint(source());
        }
        return {};
    }

    if (other.isDegenerate()) {
        if (contains(other.source())) {
            return ResultPoint(other.source());
        }
        return {};
    }

    const auto supporting_intersection =
        intersection<ResultNumber>(other.asLine());
    if (!supporting_intersection) {
        return {};
    }

    if (std::holds_alternative<ResultPoint>(*supporting_intersection)) {
        const auto& point = std::get<ResultPoint>(*supporting_intersection);
        if (other.contains(point)) {
            return point;
        }
        return {};
    }

    if (std::holds_alternative<Line<ResultPoint>>(*supporting_intersection)) {
        return ResultRay(ResultPoint(other.source()), ResultPoint(other.target()));
    }

    return std::get<ResultRay>(*supporting_intersection).template intersection<ResultNumber>(other);
}

// -----------------------------------------------------------------------------
// Rectangle

namespace detail {

template <class PointType, std::size_t Capacity>
struct UniqueIntersectionPoints {
    constexpr void add(const PointType& point) {
        for (std::size_t index = 0; index < size; ++index) {
            if (points[index] == point) {
                return;
            }
        }

        points[size] = point;
        ++size;
    }

    constexpr const PointType& operator[](std::size_t index) const {
        return points[index];
    }

    constexpr PointType minPoint() const {
        PointType result = points[0];
        for (std::size_t index = 1; index < size; ++index) {
            if (points[index] < result) {
                result = points[index];
            }
        }
        return result;
    }

    constexpr PointType maxPoint() const {
        PointType result = points[0];
        for (std::size_t index = 1; index < size; ++index) {
            if (result < points[index]) {
                result = points[index];
            }
        }
        return result;
    }

    std::array<PointType, Capacity> points{};
    std::size_t size = 0;
};

template <class SegmentType, class PointSet>
constexpr void addSegmentEndpoints(const SegmentType& segment, PointSet& points) {
    points.add(segment.min());
    points.add(segment.max());
}

template <class PointSet>
constexpr auto pointsToSegmentIntersection(const PointSet& points) {
    using PointType = std::remove_cvref_t<decltype(points[0])>;
    using SegmentType = Segment<PointType>;
    using ResultType = std::optional<std::variant<PointType, SegmentType>>;

    if (points.size == 0) {
        return ResultType{};
    }

    if (points.size == 1) {
        return ResultType(points[0]);
    }

    const auto first = points.minPoint();
    const auto second = points.maxPoint();
    if (first == second) {
        return ResultType(first);
    }

    return ResultType(SegmentType(first, second));
}

template <class ResultNumber, class LabelType, class RectangleType, class LineType>
constexpr auto rectangleLineIntersection(const RectangleType& rectangle, const LineType& line) {
    using ResultPoint = Point<ResultNumber, LabelType>;
    UniqueIntersectionPoints<ResultPoint, 8> points;

    if (line.isDegenerate()) {
        if (rectangle.contains(line.min())) {
            points.add(ResultPoint(line.min()));
        }
        return pointsToSegmentIntersection(points);
    }

    const auto rectangle_edges = rectangle.edges();
    for (const auto& edge : rectangle_edges) {
        const Line<typename RectangleType::PointType> edge_line(edge.min(), edge.max());
        const auto intersection = edge_line.template intersection<ResultNumber>(line);
        if (!intersection) {
            continue;
        }

        if (std::holds_alternative<ResultPoint>(*intersection)) {
            const auto& point = std::get<ResultPoint>(*intersection);
            if (edge.contains(point)) {
                points.add(point);
            }
        } else {
            const auto& overlap = std::get<Line<ResultPoint>>(*intersection);
            static_cast<void>(overlap);
            addSegmentEndpoints(Segment<ResultPoint>(ResultPoint(edge.min()), ResultPoint(edge.max())), points);
        }
    }

    return pointsToSegmentIntersection(points);
}

template <class ResultNumber, class LabelType, class RectangleType, class SegmentType>
constexpr auto rectangleSegmentIntersection(const RectangleType& rectangle, const SegmentType& segment) {
    using ResultPoint = Point<ResultNumber, LabelType>;
    UniqueIntersectionPoints<ResultPoint, 10> points;

    if (rectangle.contains(segment.min())) {
        points.add(ResultPoint(segment.min()));
    }
    if (rectangle.contains(segment.max())) {
        points.add(ResultPoint(segment.max()));
    }

    const auto rectangle_edges = rectangle.edges();
    for (const auto& edge : rectangle_edges) {
        const auto intersection = edge.template intersection<ResultNumber>(segment);
        if (!intersection) {
            continue;
        }

        if (std::holds_alternative<ResultPoint>(*intersection)) {
            points.add(std::get<ResultPoint>(*intersection));
        } else {
            addSegmentEndpoints(std::get<Segment<ResultPoint>>(*intersection), points);
        }
    }

    return pointsToSegmentIntersection(points);
}

template <class ResultNumber, class LabelType, class RectangleType, class RayType>
constexpr auto rectangleRayIntersection(const RectangleType& rectangle, const RayType& ray) {
    using ResultPoint = Point<ResultNumber, LabelType>;
    UniqueIntersectionPoints<ResultPoint, 10> points;

    if (rectangle.contains(ray.source())) {
        points.add(ResultPoint(ray.source()));
    }

    if (ray.isDegenerate()) {
        return pointsToSegmentIntersection(points);
    }

    const Line<Point<ResultNumber, LabelType>> ray_line(ResultPoint(ray.source()), ResultPoint(ray.target()));
    const auto rectangle_edges = rectangle.edges();
    for (const auto& edge : rectangle_edges) {
        const Line<typename RectangleType::PointType> edge_line(edge.min(), edge.max());
        const auto intersection = edge_line.template intersection<ResultNumber>(ray_line);
        if (!intersection) {
            continue;
        }

        if (std::holds_alternative<ResultPoint>(*intersection)) {
            const auto& point = std::get<ResultPoint>(*intersection);
            if (edge.contains(point) && ray.contains(point)) {
                points.add(point);
            }
        } else {
            if (ray.contains(edge.min())) {
                points.add(ResultPoint(edge.min()));
            }
            if (ray.contains(edge.max())) {
                points.add(ResultPoint(edge.max()));
            }
        }
    }

    return pointsToSegmentIntersection(points);
}

template <class ResultNumber, class LabelType, class TriangleType, class LineType>
constexpr auto triangleLineIntersection(const TriangleType& triangle, const LineType& line) {
    using ResultPoint = Point<ResultNumber, LabelType>;
    UniqueIntersectionPoints<ResultPoint, 8> points;

    if (line.isDegenerate()) {
        if (triangle.contains(line.min())) {
            points.add(ResultPoint(line.min()));
        }
        return pointsToSegmentIntersection(points);
    }

    const auto triangle_edges = triangle.edges();
    for (const auto& edge : triangle_edges) {
        const Line<typename TriangleType::PointType> edge_line(edge.min(), edge.max());
        const auto intersection = edge_line.template intersection<ResultNumber>(line);
        if (!intersection) {
            continue;
        }

        if (std::holds_alternative<ResultPoint>(*intersection)) {
            const auto& point = std::get<ResultPoint>(*intersection);
            if (edge.contains(point)) {
                points.add(point);
            }
        } else {
            addSegmentEndpoints(Segment<ResultPoint>(ResultPoint(edge.min()), ResultPoint(edge.max())), points);
        }
    }

    return pointsToSegmentIntersection(points);
}

template <class ResultNumber, class LabelType, class TriangleType, class SegmentType>
constexpr auto triangleSegmentIntersection(const TriangleType& triangle, const SegmentType& segment) {
    using ResultPoint = Point<ResultNumber, LabelType>;
    UniqueIntersectionPoints<ResultPoint, 10> points;

    if (triangle.contains(segment.min())) {
        points.add(ResultPoint(segment.min()));
    }
    if (triangle.contains(segment.max())) {
        points.add(ResultPoint(segment.max()));
    }

    const auto triangle_edges = triangle.edges();
    for (const auto& edge : triangle_edges) {
        const auto intersection = edge.template intersection<ResultNumber>(segment);
        if (!intersection) {
            continue;
        }

        if (std::holds_alternative<ResultPoint>(*intersection)) {
            points.add(std::get<ResultPoint>(*intersection));
        } else {
            addSegmentEndpoints(std::get<Segment<ResultPoint>>(*intersection), points);
        }
    }

    return pointsToSegmentIntersection(points);
}

template <class ResultNumber, class LabelType, class TriangleType, class RayType>
constexpr auto triangleRayIntersection(const TriangleType& triangle, const RayType& ray) {
    using ResultPoint = Point<ResultNumber, LabelType>;
    UniqueIntersectionPoints<ResultPoint, 10> points;

    if (triangle.contains(ray.source())) {
        points.add(ResultPoint(ray.source()));
    }

    if (ray.isDegenerate()) {
        return pointsToSegmentIntersection(points);
    }

    const Line<Point<ResultNumber, LabelType>> ray_line(ResultPoint(ray.source()), ResultPoint(ray.target()));
    const auto triangle_edges = triangle.edges();
    for (const auto& edge : triangle_edges) {
        const Line<typename TriangleType::PointType> edge_line(edge.min(), edge.max());
        const auto intersection = edge_line.template intersection<ResultNumber>(ray_line);
        if (!intersection) {
            continue;
        }

        if (std::holds_alternative<ResultPoint>(*intersection)) {
            const auto& point = std::get<ResultPoint>(*intersection);
            if (edge.contains(point) && ray.contains(point)) {
                points.add(point);
            }
        } else {
            if (ray.contains(edge.min())) {
                points.add(ResultPoint(edge.min()));
            }
            if (ray.contains(edge.max())) {
                points.add(ResultPoint(edge.max()));
            }
        }
    }

    return pointsToSegmentIntersection(points);
}

}  // namespace detail

template <class PointType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr std::optional<Point<ResultNumber, typename PointType::LabelType>>
Rectangle<PointType>::intersection(const OtherPoint& other) const {
    if (contains(other)) {
        return Point<ResultNumber, typename PointType::LabelType>(other);
    }
    return {};
}

template <class PointType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr std::optional<Rectangle<Point<ResultNumber, typename PointType::LabelType>>>
Rectangle<PointType>::intersection(const Rectangle<OtherPoint>& other) const {
    using ResultPoint = Point<ResultNumber, typename PointType::LabelType>;
    using ResultRectangle = Rectangle<ResultPoint>;

    if (!intersects(other)) {
        return {};
    }

    const auto min_x = min().x() < other.min().x() ? other.min().x() : min().x();
    const auto min_y = min().y() < other.min().y() ? other.min().y() : min().y();
    const auto max_x = max().x() < other.max().x() ? max().x() : other.max().x();
    const auto max_y = max().y() < other.max().y() ? max().y() : other.max().y();

    return ResultRectangle(
        ResultPoint(static_cast<ResultNumber>(min_x), static_cast<ResultNumber>(min_y)),
        ResultPoint(static_cast<ResultNumber>(max_x), static_cast<ResultNumber>(max_y)));
}

template <class PointType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>>>
Rectangle<PointType>::intersection(const Line<OtherPoint>& other) const {
    return detail::rectangleLineIntersection<ResultNumber, typename PointType::LabelType>(*this, other);
}

template <class PointType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>>>
Rectangle<PointType>::intersection(const OrientedLine<OtherPoint>& other) const {
    return intersection<ResultNumber>(other.asLine());
}

template <class PointType>
template <class ResultNumber, PointConcept OtherPoint, class OtherLabel>
constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>>>
Rectangle<PointType>::intersection(const Segment<OtherPoint, OtherLabel>& other) const {
    return detail::rectangleSegmentIntersection<ResultNumber, typename PointType::LabelType>(*this, other);
}

template <class PointType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>>>
Rectangle<PointType>::intersection(const OrientedSegment<OtherPoint>& other) const {
    return intersection<ResultNumber>(static_cast<Segment<OtherPoint>>(other));
}

template <class PointType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>>>
Rectangle<PointType>::intersection(const Ray<OtherPoint>& other) const {
    return detail::rectangleRayIntersection<ResultNumber, typename PointType::LabelType>(*this, other);
}

template <class PointType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr auto Rectangle<PointType>::intersection(const Halfplane<OtherPoint>& other) const {
    using ResultPoint = Point<ResultNumber, typename PointType::LabelType>;
    using ResultSegment = Segment<ResultPoint>;
    if (!other.intersects(*this)) {
        return std::optional<std::variant<ResultPoint, ResultSegment>>{};
    }
    const auto rectangle_edges = edges();
    for (const auto& edge : rectangle_edges) {
        const auto edge_intersection = other.template intersection<ResultNumber>(edge);
        if (edge_intersection) {
            return edge_intersection;
        }
    }
    return std::optional<std::variant<ResultPoint, ResultSegment>>(ResultPoint(min()));
}

// -----------------------------------------------------------------------------
// Triangle

template <class PointType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr std::optional<Point<ResultNumber, typename PointType::LabelType>>
Triangle<PointType>::intersection(const OtherPoint& other) const {
    if (contains(other)) {
        return Point<ResultNumber, typename PointType::LabelType>(other);
    }
    return {};
}

template <class PointType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>>>
Triangle<PointType>::intersection(const Line<OtherPoint>& other) const {
    return detail::triangleLineIntersection<ResultNumber, typename PointType::LabelType>(*this, other);
}

template <class PointType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>>>
Triangle<PointType>::intersection(const OrientedLine<OtherPoint>& other) const {
    return intersection<ResultNumber>(other.asLine());
}

template <class PointType>
template <class ResultNumber, PointConcept OtherPoint, class OtherLabel>
constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>>>
Triangle<PointType>::intersection(const Segment<OtherPoint, OtherLabel>& other) const {
    return detail::triangleSegmentIntersection<ResultNumber, typename PointType::LabelType>(*this, other);
}

template <class PointType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>>>
Triangle<PointType>::intersection(const OrientedSegment<OtherPoint>& other) const {
    return intersection<ResultNumber>(static_cast<Segment<OtherPoint>>(other));
}

template <class PointType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>>>
Triangle<PointType>::intersection(const Ray<OtherPoint>& other) const {
    return detail::triangleRayIntersection<ResultNumber, typename PointType::LabelType>(*this, other);
}

template <class PointType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr auto Triangle<PointType>::intersection(const Halfplane<OtherPoint>& other) const {
    using ResultPoint = Point<ResultNumber, typename PointType::LabelType>;
    using ResultSegment = Segment<ResultPoint>;
    if (!other.intersects(*this)) {
        return std::optional<std::variant<ResultPoint, ResultSegment>>{};
    }
    const auto triangle_edges = edges();
    for (const auto& edge : triangle_edges) {
        const auto edge_intersection = other.template intersection<ResultNumber>(edge);
        if (edge_intersection) {
            return edge_intersection;
        }
    }
    return std::optional<std::variant<ResultPoint, ResultSegment>>(ResultPoint(a()));
}

template <class PointType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr auto Triangle<PointType>::intersection(const Rectangle<OtherPoint>& other) const {
    using ResultPoint = Point<ResultNumber, typename PointType::LabelType>;
    using ResultSegment = Segment<ResultPoint>;
    if (!other.intersects(*this)) {
        return std::optional<std::variant<ResultPoint, ResultSegment>>{};
    }
    const auto triangle_edges = edges();
    for (const auto& edge : triangle_edges) {
        const auto edge_intersection = other.template intersection<ResultNumber>(edge);
        if (edge_intersection) {
            return edge_intersection;
        }
    }
    return std::optional<std::variant<ResultPoint, ResultSegment>>(ResultPoint(a()));
}

template <class PointType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr auto Triangle<PointType>::intersection(const Triangle<OtherPoint>& other) const {
    using ResultPoint = Point<ResultNumber, typename PointType::LabelType>;
    using ResultSegment = Segment<ResultPoint>;
    if (!intersects(other)) {
        return std::optional<std::variant<ResultPoint, ResultSegment>>{};
    }
    const auto other_edges = other.edges();
    for (const auto& edge : other_edges) {
        const auto edge_intersection = intersection<ResultNumber>(edge);
        if (edge_intersection) {
            return edge_intersection;
        }
    }
    return std::optional<std::variant<ResultPoint, ResultSegment>>(ResultPoint(other.a()));
}


// ---------------------------------------------------------------------------
// Convex

template <class PointType>
template <class ResultNumber, PointConcept OtherPoint, class OtherLabel>
constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>>> Convex<PointType>::intersection(const Segment<OtherPoint, OtherLabel>& other) const {
    auto isec = this->template intersection<ResultNumber>(Line<OtherPoint>(other));
    if (!isec) {
        return {};
    }
    size_t index = isec->index();
    if (index == 0) {
        const auto& p = std::get<0>(*isec);
        if (other.containsCollinear(p)) {
            return p;
        }
        return {};
    }
    const auto& seg = std::get<1>(*isec);
    return seg.template intersection<ResultNumber>(other);
}

template <class PointType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>>> Convex<PointType>::intersection(const OrientedSegment<OtherPoint>& other) const {
    return intersection<ResultNumber>(Segment<OtherPoint>(other[0], other[1]));
}

template <class PointType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>>> Convex<PointType>::intersection(const Line<OtherPoint>& other) const {
    if (points_.empty()) {
        return {};
    }

    using CommonNumberType = std::common_type_t<NumberType, typename OtherPoint::NumberType>;
    using CommonPoint = Point<CommonNumberType, typename PointType::LabelType>;

    const CommonPoint local_p0(static_cast<CommonNumberType>(other[0].x()) - static_cast<CommonNumberType>(translation_.x()),
                               static_cast<CommonNumberType>(other[0].y()) - static_cast<CommonNumberType>(translation_.y()));
    const CommonPoint local_p1(static_cast<CommonNumberType>(other[1].x()) - static_cast<CommonNumberType>(translation_.x()),
                               static_cast<CommonNumberType>(other[1].y()) - static_cast<CommonNumberType>(translation_.y()));

    if (local_p0 == local_p1) {
        return {};
    }

    const Line<CommonPoint> localLine(local_p0, local_p1);

    auto orientationValue = [&](size_t index) {
        return orientationDeterminant(localLine[0], localLine[1], points_[index]);
    };

    if (points_.size() == 1) {
        if (orientationValue(0) == CommonNumberType(0)) {
            Point<ResultNumber, typename PointType::LabelType> result =
                static_cast<Point<ResultNumber, typename PointType::LabelType>>(points_[0]);
            return result + static_cast<Point<ResultNumber, typename PointType::LabelType>>(translation_);
        }
        return {};
    }

    if (points_.size() == 2) {
        auto local_segment = Segment<PointType>(points_[0], points_[1]);
        auto isec = local_segment.template intersection<ResultNumber>(localLine);
        if (!isec) {
            return {};
        }
        const auto shift = static_cast<Point<ResultNumber, typename PointType::LabelType>>(translation_);
        if (std::holds_alternative<Point<ResultNumber, typename PointType::LabelType>>(*isec)) {
            return std::get<Point<ResultNumber, typename PointType::LabelType>>(*isec) + shift;
        }
        auto seg = std::get<Segment<Point<ResultNumber, typename PointType::LabelType>>>(*isec);
        return Segment<Point<ResultNumber, typename PointType::LabelType>>(seg[0] + shift, seg[1] + shift);
    }

    auto max_it = detail::cyclicMax(points_.begin(), points_.end(),
                                    [&](const PointType& a) {
                                        return orientationDeterminant(localLine[0], localLine[1], a);
                                    });
    auto min_it = detail::cyclicMax(points_.begin(), points_.end(),
                                    [&](const PointType& a) {
                                        return orientationDeterminant(localLine[1], localLine[0], a);
                                    });

    const size_t n = points_.size();
    const size_t i_max = static_cast<size_t>(std::distance(points_.begin(), max_it));
    const size_t i_min = static_cast<size_t>(std::distance(points_.begin(), min_it));

    const CommonNumberType max_value = orientationValue(i_max);
    const CommonNumberType min_value = orientationValue(i_min);

    if (max_value < CommonNumberType(0) || min_value > CommonNumberType(0)) {
        return {};
    }

    auto translatePoint = [&](const Point<ResultNumber, typename PointType::LabelType>& point) {
        return point + static_cast<Point<ResultNumber, typename PointType::LabelType>>(translation_);
    };

    auto translateSegment = [&](const Segment<Point<ResultNumber, typename PointType::LabelType>>& segment) {
        return Segment<Point<ResultNumber, typename PointType::LabelType>>(
            translatePoint(segment[0]), translatePoint(segment[1]));
    };

    if (max_value == CommonNumberType(0) && min_value == CommonNumberType(0)) {
        const CommonPoint direction(localLine[1].x() - localLine[0].x(),
                                    localLine[1].y() - localLine[0].y());

        auto extremal_high = detail::cyclicMax(points_.begin(), points_.end(),
            [&](const PointType& a) {
                return static_cast<CommonNumberType>(a.x()) * direction.x() +
                       static_cast<CommonNumberType>(a.y()) * direction.y();
            });

        auto extremal_low = detail::cyclicMax(points_.begin(), points_.end(),
            [&](const PointType& a) {
                return -(static_cast<CommonNumberType>(a.x()) * direction.x() +
                         static_cast<CommonNumberType>(a.y()) * direction.y());
            });

        Segment<PointType> support(*extremal_low, *extremal_high);
        auto isec = support.template intersection<ResultNumber>(localLine);
        if (!isec) {
            return {};
        }
        if (std::holds_alternative<Point<ResultNumber, typename PointType::LabelType>>(*isec)) {
            return translatePoint(std::get<Point<ResultNumber, typename PointType::LabelType>>(*isec));
        }
        return translateSegment(std::get<Segment<Point<ResultNumber, typename PointType::LabelType>>>(*isec));
    }

    auto forwardDist = [&](size_t from, size_t to) {
        return (to + n - from) % n;
    };

    auto findBoundaryForward = [&](size_t start, size_t length) -> size_t {
        size_t lo = 1;
        size_t hi = length;
        size_t result = length + 1;
        while (lo <= hi) {
            size_t mid = lo + (hi - lo) / 2;
            if (orientationValue((start + mid) % n) <= CommonNumberType(0)) {
                result = mid;
                hi = mid - 1;
            } else {
                lo = mid + 1;
            }
        }
        return result;
    };

    auto findBoundaryBackward = [&](size_t start, size_t length) -> size_t {
        size_t lo = 1;
        size_t hi = length;
        size_t result = length + 1;
        while (lo <= hi) {
            size_t mid = lo + (hi - lo) / 2;
            size_t index = (start + n - mid) % n;
            if (orientationValue(index) <= CommonNumberType(0)) {
                result = mid;
                hi = mid - 1;
            } else {
                lo = mid + 1;
            }
        }
        return result;
    };

    const size_t forward_len = forwardDist(i_max, i_min);
    const size_t backward_len = forwardDist(i_min, i_max);

    const size_t boundary_forward = findBoundaryForward(i_max, forward_len);
    const size_t boundary_backward = findBoundaryBackward(i_max, backward_len);

    if (boundary_forward > forward_len || boundary_backward > backward_len) {
        return {};
    }

    const size_t b1 = (i_max + boundary_forward) % n;
    const size_t b1_prev = (b1 + n - 1) % n;
    const size_t b2 = (i_max + n - boundary_backward) % n;
    const size_t b2_next = (b2 + 1) % n;

    auto collectPoints = [&](const Segment<PointType>& edge,
                             std::vector<Point<ResultNumber, typename PointType::LabelType>>& points) {
        auto edge_isec = edge.template intersection<ResultNumber>(localLine);
        if (!edge_isec) {
            return;
        }
        if (std::holds_alternative<Point<ResultNumber, typename PointType::LabelType>>(*edge_isec)) {
            points.push_back(std::get<Point<ResultNumber, typename PointType::LabelType>>(*edge_isec));
        } else {
            auto seg = std::get<Segment<Point<ResultNumber, typename PointType::LabelType>>>(*edge_isec);
            points.push_back(seg[0]);
            points.push_back(seg[1]);
        }
    };

    std::vector<Point<ResultNumber, typename PointType::LabelType>> points;
    points.reserve(4);
    collectPoints(Segment<PointType>(points_[b1_prev], points_[b1]), points);
    collectPoints(Segment<PointType>(points_[b2], points_[b2_next]), points);

    if (points.empty()) {
        return {};
    }

    const CommonPoint direction(localLine[1].x() - localLine[0].x(),
                                localLine[1].y() - localLine[0].y());

    auto project = [&](const Point<ResultNumber, typename PointType::LabelType>& point) {
        return (static_cast<CommonNumberType>(point.x()) - localLine[0].x()) * direction.x() +
               (static_cast<CommonNumberType>(point.y()) - localLine[0].y()) * direction.y();
    };

    std::sort(points.begin(), points.end(), [&](auto const& a, auto const& b) {
        return project(a) < project(b);
    });
    points.erase(std::unique(points.begin(), points.end()), points.end());

    if (points.empty()) {
        return {};
    }
    if (points.size() == 1) {
        return translatePoint(points[0]);
    }

    return Segment<Point<ResultNumber, typename PointType::LabelType>>(translatePoint(points.front()),
                                                                      translatePoint(points.back()));
}

template <class PointType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>>> Convex<PointType>::intersection(const OrientedLine<OtherPoint>& other) const {
    return intersection<ResultNumber>(Line<OtherPoint>(other[0], other[1]));
}

template <class PointType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>>> Convex<PointType>::intersection(const Ray<OtherPoint>& other) const {
    auto line_isec =  intersection<ResultNumber>(Line<OtherPoint>(other[0], other[1]));
    if (!line_isec) {
        return {};
    }
    size_t index = line_isec->index();
    if (index == 0) {
        if (other.containsCollinear(std::get<0>(*line_isec))) {
            return std::get<0>(*line_isec);
        }
        return {};
    }
    // index == 1
    auto seg = std::get<1>(*line_isec);
    return seg.template intersection<ResultNumber>(other);
}

template <class PointType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>, Convex<Point<ResultNumber, typename PointType::LabelType>>>> Convex<PointType>::intersection(const Rectangle<OtherPoint>& other) const {
    return intersection<ResultNumber>(other.asConvex());
}

template <class PointType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>, Convex<Point<ResultNumber, typename PointType::LabelType>>>> Convex<PointType>::intersection(const Triangle<OtherPoint>& other) const {
    return intersection<ResultNumber>(other.asConvex());
}

template <class PointType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>, Convex<Point<ResultNumber, typename PointType::LabelType>>>> Convex<PointType>::intersection(const Convex<OtherPoint>& other) const {
    if (size() == 0 || other.size() == 0 || !intersects(other)) {
        return {};
    }

    using ResultPointType = Point<ResultNumber, typename PointType::LabelType>;

    if (size() == 1) {
        ResultPointType point = static_cast<ResultPointType>(points_[0] + translation_);
        if (other.contains(point)) {
            return point;
        }
        return {};
    }

    if (other.size() == 1) {
        ResultPointType point = static_cast<ResultPointType>(other[0]);
        if (contains(point)) {
            return point;
        }
        return {};
    }

    std::vector<ResultPointType> isecPoints;

    for (auto &edge : other.edges()) {
        auto isec = intersection<ResultNumber>(edge);
        if (isec) {
            if (std::holds_alternative<Point<ResultNumber, typename PointType::LabelType>>(*isec)) {
                isecPoints.push_back(std::get<Point<ResultNumber, typename PointType::LabelType>>(*isec));
            } else {
                auto seg = std::get<Segment<Point<ResultNumber, typename PointType::LabelType>>>(*isec);
                isecPoints.push_back(seg[0]);
                isecPoints.push_back(seg[1]);
            }
        }
    }
    for (auto &edge : edges()) {
        auto isec = other.template intersection<ResultNumber>(edge);
        if (isec) {
            if (std::holds_alternative<ResultPointType>(*isec)) {
                isecPoints.push_back(std::get<ResultPointType>(*isec));
            } else {
                auto seg = std::get<Segment<ResultPointType>>(*isec);
                isecPoints.push_back(seg[0]);
                isecPoints.push_back(seg[1]);
            }
        }
    }

    if (isecPoints.empty()) {
        return {};
    }

    Convex<ResultPointType> convex(std::move(isecPoints));

    if (convex.size() == 1) {
        return convex[0];
    }
    if (convex.size() == 2) {
        return Segment<ResultPointType>(convex[0], convex[1]);
    }

    return convex;
}

// ---------------------------------------------------------------------------
// Polygon

template <class PointType>
template <class ResultNumber, PointConcept OtherPoint, class OtherLabel>
constexpr std::vector<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>>>
Polygon<PointType>::intersection(const Segment<OtherPoint, OtherLabel>& other) const {
    using ResultPoint = Point<ResultNumber, typename PointType::LabelType>;
    using ResultSegment = Segment<ResultPoint>;
    using Piece = std::variant<ResultPoint, ResultSegment>;

    std::vector<Piece> result;
    const std::size_t n = size();
    if (n == 0) {
        return result;
    }

    const ResultPoint a(other.min());
    const ResultPoint b(other.max());

    // A degenerate segment is just its single point.
    if (other.isDegenerate()) {
        if (contains(a)) {
            result.emplace_back(a);
        }
        return result;
    }

    // Parametrize the supporting line by t = (p - a) . (b - a): t runs from 0 at
    // `a` to T at `b`, orders points exactly (no division), and -- since every
    // point handled here lies on that line -- identifies each point uniquely.
    const ResultPoint direction = b - a;
    const ResultNumber T = direction * direction;
    auto tOf = [&](const ResultPoint& p) -> ResultNumber { return (p - a) * direction; };

    // Signed side of a vertex w.r.t. the line (CCW positive, 0 on the line).
    auto sideOf = [&](const ResultPoint& v) -> int {
        const auto o = orientationSign(a, b, v);
        if (o > 0) return 1;
        if (o < 0) return -1;
        return 0;
    };

    // Closed pieces of (line ∩ polygon), as intervals [lo, hi] in t with the
    // matching endpoints; a degenerate interval (lo == hi) is a single point.
    struct Span { ResultNumber lo, hi; ResultPoint plo, phi; };
    auto makeSpan = [](ResultNumber t1, ResultNumber t2, const ResultPoint& p1, const ResultPoint& p2) -> Span {
        return (t1 <= t2) ? Span{t1, t2, p1, p2} : Span{t2, t1, p2, p1};
    };
    std::vector<Span> spans;

    // Boundary crossings of the *whole* line, used to recover inside/outside by
    // ray parity. A vertex on the line is treated as if perturbed to the -1
    // side, which counts vertex touches and collinear edges consistently: each
    // edge whose perturbed endpoint signs differ contributes one crossing.
    std::vector<std::pair<ResultNumber, ResultPoint>> crossings;

    for (std::size_t i = 0; i < n; ++i) {
        const ResultPoint u = static_cast<ResultPoint>((*this)[i]);
        const ResultPoint w = static_cast<ResultPoint>((*this)[(i + 1) % n]);
        const int su = sideOf(u);
        const int sw = sideOf(w);

        if (su == 0 && sw == 0) {
            // Edge collinear with the line: a boundary overlap, always closed.
            spans.push_back(makeSpan(tOf(u), tOf(w), u, w));
            continue;
        }
        if (su == 0) {
            // Vertex on the line: a boundary touch point (recorded once, here,
            // as the starting vertex of its edge).
            spans.push_back({tOf(u), tOf(u), u, u});
        }

        if (su != 0 && sw != 0 && su != sw) {
            // Transversal crossing through the edge interior.
            const Line<ResultPoint> line(a, b);
            const auto isec = line.template intersection<ResultNumber>(
                Segment<PointType>((*this)[i], (*this)[(i + 1) % n]));
            if (isec && std::holds_alternative<ResultPoint>(*isec)) {
                const ResultPoint c = std::get<ResultPoint>(*isec);
                spans.push_back({tOf(c), tOf(c), c, c});
                crossings.emplace_back(tOf(c), c);
            }
        } else {
            // Perturbed crossing located at whichever endpoint is on the line.
            const int eu = (su != 0) ? su : -1;
            const int ew = (sw != 0) ? sw : -1;
            if (eu != ew) {
                const ResultPoint& onLine = (su == 0) ? u : w;
                crossings.emplace_back(tOf(onLine), onLine);
            }
        }
    }

    // Walk the crossings in order; the open cell to the right of a crossing is
    // inside the polygon exactly when an odd number of crossings lie to its
    // left. Each maximal inside cell becomes a closed interval (its endpoints
    // are boundary crossings, hence in the closed region too).
    std::sort(crossings.begin(), crossings.end(),
              [](const auto& x, const auto& y) { return x.first < y.first; });
    std::size_t idx = 0;
    int parity = 0;
    while (idx < crossings.size()) {
        const ResultNumber tcur = crossings[idx].first;
        const ResultPoint pcur = crossings[idx].second;
        std::size_t next = idx;
        while (next < crossings.size() && crossings[next].first == tcur) {
            ++next;
        }
        parity += static_cast<int>(next - idx);
        if ((parity & 1) && next < crossings.size()) {
            spans.push_back({tcur, crossings[next].first, pcur, crossings[next].second});
        }
        idx = next;
    }

    // Union the spans (sort by lo, merge touching/overlapping), then clip to the
    // segment range [0, T] and emit each piece in order along the segment.
    std::sort(spans.begin(), spans.end(),
              [](const Span& x, const Span& y) { return x.lo != y.lo ? x.lo < y.lo : x.hi < y.hi; });
    std::vector<Span> merged;
    for (const Span& s : spans) {
        if (merged.empty() || s.lo > merged.back().hi) {
            merged.push_back(s);
        } else if (s.hi > merged.back().hi) {
            merged.back().hi = s.hi;
            merged.back().phi = s.phi;
        }
    }

    for (const Span& s : merged) {
        if (s.hi < ResultNumber(0) || s.lo > T) {
            continue;
        }
        const ResultPoint lo = (s.lo < ResultNumber(0)) ? a : s.plo;
        const ResultPoint hi = (s.hi > T) ? b : s.phi;
        if (lo == hi) {
            result.emplace_back(lo);
        } else {
            result.emplace_back(ResultSegment(lo, hi));
        }
    }

    return result;
}

template <class PointType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr std::vector<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>>>
Polygon<PointType>::intersection(const OrientedSegment<OtherPoint>& other) const {
    return intersection<ResultNumber>(Segment<OtherPoint>(other[0], other[1]));
}

template <class PointType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr std::vector<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>>>
Polygon<PointType>::intersection(const Line<OtherPoint>& other) const {
    using ResultPoint = Point<ResultNumber, typename PointType::LabelType>;
    using ResultSegment = Segment<ResultPoint>;
    using Piece = std::variant<ResultPoint, ResultSegment>;

    std::vector<Piece> result;
    const std::size_t n = size();
    if (n == 0) {
        return result;
    }

    const ResultPoint a(other.min());
    const ResultPoint b(other.max());

    // A degenerate line is a single point.
    if (other.isDegenerate()) {
        if (contains(a)) {
            result.emplace_back(a);
        }
        return result;
    }

    // Parametrize the line by t = (p - a) . (b - a): t orders points along the
    // line exactly (no division), and -- since every point handled here lies on
    // that line -- identifies each point uniquely.
    const ResultPoint direction = b - a;
    auto tOf = [&](const ResultPoint& p) -> ResultNumber { return (p - a) * direction; };

    // Signed side of a vertex w.r.t. the line (CCW positive, 0 on the line).
    auto sideOf = [&](const ResultPoint& v) -> int {
        const auto o = orientationSign(a, b, v);
        if (o > 0) return 1;
        if (o < 0) return -1;
        return 0;
    };

    // Closed pieces of (line ∩ polygon), as intervals [lo, hi] in t with the
    // matching endpoints; a degenerate interval (lo == hi) is a single point.
    struct Span { ResultNumber lo, hi; ResultPoint plo, phi; };
    auto makeSpan = [](ResultNumber t1, ResultNumber t2, const ResultPoint& p1, const ResultPoint& p2) -> Span {
        return (t1 <= t2) ? Span{t1, t2, p1, p2} : Span{t2, t1, p2, p1};
    };
    std::vector<Span> spans;

    // Boundary crossings of the line, used to recover inside/outside by ray
    // parity. A vertex on the line is treated as if perturbed to the -1 side,
    // which counts vertex touches and collinear edges consistently: each edge
    // whose perturbed endpoint signs differ contributes one crossing.
    std::vector<std::pair<ResultNumber, ResultPoint>> crossings;

    for (std::size_t i = 0; i < n; ++i) {
        const ResultPoint u = static_cast<ResultPoint>((*this)[i]);
        const ResultPoint w = static_cast<ResultPoint>((*this)[(i + 1) % n]);
        const int su = sideOf(u);
        const int sw = sideOf(w);

        if (su == 0 && sw == 0) {
            // Edge collinear with the line: a boundary overlap, always closed.
            spans.push_back(makeSpan(tOf(u), tOf(w), u, w));
            continue;
        }
        if (su == 0) {
            // Vertex on the line: a boundary touch point (recorded once, here,
            // as the starting vertex of its edge).
            spans.push_back({tOf(u), tOf(u), u, u});
        }

        if (su != 0 && sw != 0 && su != sw) {
            // Transversal crossing through the edge interior.
            const Line<ResultPoint> line(a, b);
            const auto isec = line.template intersection<ResultNumber>(
                Segment<PointType>((*this)[i], (*this)[(i + 1) % n]));
            if (isec && std::holds_alternative<ResultPoint>(*isec)) {
                const ResultPoint c = std::get<ResultPoint>(*isec);
                spans.push_back({tOf(c), tOf(c), c, c});
                crossings.emplace_back(tOf(c), c);
            }
        } else {
            // Perturbed crossing located at whichever endpoint is on the line.
            const int eu = (su != 0) ? su : -1;
            const int ew = (sw != 0) ? sw : -1;
            if (eu != ew) {
                const ResultPoint& onLine = (su == 0) ? u : w;
                crossings.emplace_back(tOf(onLine), onLine);
            }
        }
    }

    // Walk the crossings in order; the open cell to the right of a crossing is
    // inside the polygon exactly when an odd number of crossings lie to its
    // left. Each maximal inside cell becomes a closed interval (its endpoints
    // are boundary crossings, hence in the closed region too).
    std::sort(crossings.begin(), crossings.end(),
              [](const auto& x, const auto& y) { return x.first < y.first; });
    std::size_t idx = 0;
    int parity = 0;
    while (idx < crossings.size()) {
        const ResultNumber tcur = crossings[idx].first;
        const ResultPoint pcur = crossings[idx].second;
        std::size_t next = idx;
        while (next < crossings.size() && crossings[next].first == tcur) {
            ++next;
        }
        parity += static_cast<int>(next - idx);
        if ((parity & 1) && next < crossings.size()) {
            spans.push_back({tcur, crossings[next].first, pcur, crossings[next].second});
        }
        idx = next;
    }

    // Union the spans (sort by lo, merge touching/overlapping) and emit each
    // piece in order along the line -- no clipping, since the line is infinite.
    std::sort(spans.begin(), spans.end(),
              [](const Span& x, const Span& y) { return x.lo != y.lo ? x.lo < y.lo : x.hi < y.hi; });
    std::vector<Span> merged;
    for (const Span& s : spans) {
        if (merged.empty() || s.lo > merged.back().hi) {
            merged.push_back(s);
        } else if (s.hi > merged.back().hi) {
            merged.back().hi = s.hi;
            merged.back().phi = s.phi;
        }
    }

    for (const Span& s : merged) {
        if (s.lo == s.hi) {
            result.emplace_back(s.plo);
        } else {
            result.emplace_back(ResultSegment(s.plo, s.phi));
        }
    }

    return result;
}

template <class PointType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr std::vector<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>>>
Polygon<PointType>::intersection(const OrientedLine<OtherPoint>& other) const {
    return intersection<ResultNumber>(other.asLine());
}

template <class PointType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr std::vector<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>>>
Polygon<PointType>::intersection(const Ray<OtherPoint>& other) const {
    using ResultPoint = Point<ResultNumber, typename PointType::LabelType>;
    using ResultSegment = Segment<ResultPoint>;
    using Piece = std::variant<ResultPoint, ResultSegment>;

    std::vector<Piece> result;
    const std::size_t n = size();
    if (n == 0) {
        return result;
    }

    const ResultPoint a(other.source());
    const ResultPoint b(other.target());

    // A degenerate ray is a single point.
    if (other.isDegenerate()) {
        if (contains(a)) {
            result.emplace_back(a);
        }
        return result;
    }

    // Parametrize the supporting line by t = (p - a) . (b - a): t is 0 at the
    // source and grows along the ray, ordering points exactly (no division) and
    // -- since every point handled here lies on that line -- identifying each
    // point uniquely. The ray is the half t >= 0.
    const ResultPoint direction = b - a;
    auto tOf = [&](const ResultPoint& p) -> ResultNumber { return (p - a) * direction; };

    // Signed side of a vertex w.r.t. the line (CCW positive, 0 on the line).
    auto sideOf = [&](const ResultPoint& v) -> int {
        const auto o = orientationSign(a, b, v);
        if (o > 0) return 1;
        if (o < 0) return -1;
        return 0;
    };

    // Closed pieces of (line ∩ polygon), as intervals [lo, hi] in t with the
    // matching endpoints; a degenerate interval (lo == hi) is a single point.
    struct Span { ResultNumber lo, hi; ResultPoint plo, phi; };
    auto makeSpan = [](ResultNumber t1, ResultNumber t2, const ResultPoint& p1, const ResultPoint& p2) -> Span {
        return (t1 <= t2) ? Span{t1, t2, p1, p2} : Span{t2, t1, p2, p1};
    };
    std::vector<Span> spans;

    // Boundary crossings of the line, used to recover inside/outside by ray
    // parity. A vertex on the line is treated as if perturbed to the -1 side,
    // which counts vertex touches and collinear edges consistently: each edge
    // whose perturbed endpoint signs differ contributes one crossing.
    std::vector<std::pair<ResultNumber, ResultPoint>> crossings;

    for (std::size_t i = 0; i < n; ++i) {
        const ResultPoint u = static_cast<ResultPoint>((*this)[i]);
        const ResultPoint w = static_cast<ResultPoint>((*this)[(i + 1) % n]);
        const int su = sideOf(u);
        const int sw = sideOf(w);

        if (su == 0 && sw == 0) {
            // Edge collinear with the line: a boundary overlap, always closed.
            spans.push_back(makeSpan(tOf(u), tOf(w), u, w));
            continue;
        }
        if (su == 0) {
            // Vertex on the line: a boundary touch point (recorded once, here,
            // as the starting vertex of its edge).
            spans.push_back({tOf(u), tOf(u), u, u});
        }

        if (su != 0 && sw != 0 && su != sw) {
            // Transversal crossing through the edge interior.
            const Line<ResultPoint> line(a, b);
            const auto isec = line.template intersection<ResultNumber>(
                Segment<PointType>((*this)[i], (*this)[(i + 1) % n]));
            if (isec && std::holds_alternative<ResultPoint>(*isec)) {
                const ResultPoint c = std::get<ResultPoint>(*isec);
                spans.push_back({tOf(c), tOf(c), c, c});
                crossings.emplace_back(tOf(c), c);
            }
        } else {
            // Perturbed crossing located at whichever endpoint is on the line.
            const int eu = (su != 0) ? su : -1;
            const int ew = (sw != 0) ? sw : -1;
            if (eu != ew) {
                const ResultPoint& onLine = (su == 0) ? u : w;
                crossings.emplace_back(tOf(onLine), onLine);
            }
        }
    }

    // Walk the crossings in order; the open cell to the right of a crossing is
    // inside the polygon exactly when an odd number of crossings lie to its
    // left. Each maximal inside cell becomes a closed interval (its endpoints
    // are boundary crossings, hence in the closed region too).
    std::sort(crossings.begin(), crossings.end(),
              [](const auto& x, const auto& y) { return x.first < y.first; });
    std::size_t idx = 0;
    int parity = 0;
    while (idx < crossings.size()) {
        const ResultNumber tcur = crossings[idx].first;
        const ResultPoint pcur = crossings[idx].second;
        std::size_t next = idx;
        while (next < crossings.size() && crossings[next].first == tcur) {
            ++next;
        }
        parity += static_cast<int>(next - idx);
        if ((parity & 1) && next < crossings.size()) {
            spans.push_back({tcur, crossings[next].first, pcur, crossings[next].second});
        }
        idx = next;
    }

    // Union the spans (sort by lo, merge touching/overlapping), then clip to the
    // ray's half-line t >= 0 and emit each piece in order from the source.
    std::sort(spans.begin(), spans.end(),
              [](const Span& x, const Span& y) { return x.lo != y.lo ? x.lo < y.lo : x.hi < y.hi; });
    std::vector<Span> merged;
    for (const Span& s : spans) {
        if (merged.empty() || s.lo > merged.back().hi) {
            merged.push_back(s);
        } else if (s.hi > merged.back().hi) {
            merged.back().hi = s.hi;
            merged.back().phi = s.phi;
        }
    }

    for (const Span& s : merged) {
        if (s.hi < ResultNumber(0)) {
            continue;
        }
        const ResultPoint lo = (s.lo < ResultNumber(0)) ? a : s.plo;
        if (lo == s.phi) {
            result.emplace_back(lo);
        } else {
            result.emplace_back(ResultSegment(lo, s.phi));
        }
    }

    return result;
}

template <class PointType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr std::vector<std::variant<Point<ResultNumber, typename PointType::LabelType>, Polyline<Point<ResultNumber, typename PointType::LabelType>>, Polygon<Point<ResultNumber, typename PointType::LabelType>>>>
Polygon<PointType>::intersection(const Polygon<OtherPoint>& other) const {
    using ResultPoint = Point<ResultNumber, typename PointType::LabelType>;
    using ResultSegment = Segment<ResultPoint>;
    using ResultPolyline = Polyline<ResultPoint>;
    using ResultPolygon = Polygon<ResultPoint>;
    using Piece = std::variant<ResultPoint, ResultPolyline, ResultPolygon>;

    // The boundary of A ∩ B is (∂A ∩ B) ∪ (∂B ∩ A): clip every edge of each
    // polygon against the other and collect the resulting boundary pieces. The
    // sets deduplicate shared boundary so each segment/point is stored once.
    std::set<ResultSegment> segments;
    std::set<ResultPoint> touchPoints;

    auto clipEdgesAgainst = [&](const auto& edgePolygon, const auto& clipPolygon) {
        for (const auto& edge : edgePolygon.edges()) {
            for (const auto& piece : clipPolygon.template intersection<ResultNumber>(edge)) {
                if (std::holds_alternative<ResultPoint>(piece)) {
                    touchPoints.insert(std::get<ResultPoint>(piece));
                } else {
                    segments.insert(std::get<ResultSegment>(piece));
                }
            }
        }
    };
    clipEdgesAgainst(*this, other);
    clipEdgesAgainst(other, *this);

    // Make the collected boundary a consistent planar graph: split any segment at
    // another segment's endpoint lying in its interior, then dedup, so partially
    // overlapping collinear (shared) boundary collapses onto common edges. Only
    // genuine pinch points keep degree > 2 afterwards.
    auto planarize = [](const std::set<ResultSegment>& input) {
        std::vector<ResultPoint> vertices;
        for (const auto& s : input) {
            vertices.push_back(s.min());
            vertices.push_back(s.max());
        }
        std::sort(vertices.begin(), vertices.end());
        vertices.erase(std::unique(vertices.begin(), vertices.end()), vertices.end());

        std::set<ResultSegment> out;
        for (const auto& s : input) {
            const ResultPoint a = s.min();
            const ResultPoint b = s.max();
            std::vector<ResultPoint> cut{a, b};
            for (const auto& v : vertices) {
                if (v == a || v == b || !collinear(a, b, v)) {
                    continue;
                }
                if (v.x() < std::min(a.x(), b.x()) || v.x() > std::max(a.x(), b.x()) ||
                    v.y() < std::min(a.y(), b.y()) || v.y() > std::max(a.y(), b.y())) {
                    continue;  // collinear but outside the segment
                }
                cut.push_back(v);
            }
            std::sort(cut.begin(), cut.end());
            cut.erase(std::unique(cut.begin(), cut.end()), cut.end());
            for (std::size_t i = 0; i + 1 < cut.size(); ++i) {
                out.insert(ResultSegment(cut[i], cut[i + 1]));
            }
        }
        return out;
    };
    segments = planarize(segments);

    // Build the undirected graph: nodes are endpoints, edges are the segments.
    std::map<ResultPoint, std::vector<ResultPoint>> adjacency;
    for (const auto& segment : segments) {
        adjacency[segment.min()].push_back(segment.max());
        adjacency[segment.max()].push_back(segment.min());
    }

    std::vector<Piece> result;

    // A touch point that no segment uses is an isolated intersection point.
    for (const auto& point : touchPoints) {
        if (adjacency.find(point) == adjacency.end()) {
            result.emplace_back(point);
        }
    }

    using Number = ResultNumber;
    auto degree = [&adjacency](const ResultPoint& p) {
        const auto it = adjacency.find(p);
        return it == adjacency.end() ? std::size_t(0) : it->second.size();
    };
    auto removeEdge = [&adjacency](const ResultPoint& u, const ResultPoint& v) {
        auto& au = adjacency[u];
        au.erase(std::find(au.begin(), au.end(), v));
        auto& av = adjacency[v];
        av.erase(std::find(av.begin(), av.end(), u));
    };

    // Peel the open boundary (degree-1 chains, and whiskers hanging off a cycle)
    // into polylines, removing their edges. What remains is a union of closed
    // cycles that may share articulation (pinch) vertices.
    while (true) {
        const ResultPoint* leaf = nullptr;
        for (const auto& [p, neighbors] : adjacency) {
            if (neighbors.size() == 1) {
                leaf = &p;
                break;
            }
        }
        if (!leaf) {
            break;
        }
        ResultPolyline path;
        ResultPoint current = *leaf;
        path.push_back(current);
        while (degree(current) == 1) {
            const ResultPoint next = adjacency.at(current)[0];
            removeEdge(current, next);
            path.push_back(next);
            current = next;
        }
        result.emplace_back(std::move(path));
    }

    // Trace the remaining cycles as faces of the planar graph: following, at each
    // vertex, the neighbour immediately clockwise from the edge we arrived on
    // visits every face once and hugs its interior. Counterclockwise faces are
    // the filled intersection pieces; the clockwise outer face is dropped. This
    // resolves the articulation (degree > 2) vertices into their separate cycles.
    auto rotationalNext = [&adjacency](const ResultPoint& at, const ResultPoint& from) {
        const auto& neighbors = adjacency.at(at);
        auto dx = [&](const ResultPoint& p) { return p.x() - at.x(); };
        auto dy = [&](const ResultPoint& p) { return p.y() - at.y(); };
        // 0 for directions in [0, 180) degrees, 1 for [180, 360); orders the circle.
        auto half = [&](const ResultPoint& p) {
            const Number y = dy(p);
            if (y > Number(0)) return 0;
            if (y < Number(0)) return 1;
            return dx(p) >= Number(0) ? 0 : 1;
        };
        auto ccwBefore = [&](const ResultPoint& u, const ResultPoint& w) {  // u strictly CCW-before w
            const int hu = half(u), hw = half(w);
            if (hu != hw) {
                return hu < hw;
            }
            return dx(u) * dy(w) - dy(u) * dx(w) > Number(0);
        };
        // The neighbour immediately clockwise from `from`: the CCW-largest one
        // that is CCW-before `from`, wrapping to the CCW-largest overall.
        const ResultPoint* best = nullptr;
        for (const auto& n : neighbors) {
            if (n == from || !ccwBefore(n, from)) {
                continue;
            }
            if (!best || ccwBefore(*best, n)) {
                best = &n;
            }
        }
        if (!best) {
            for (const auto& n : neighbors) {
                if (n != from && (!best || ccwBefore(*best, n))) {
                    best = &n;
                }
            }
        }
        return *best;
    };

    std::set<std::pair<ResultPoint, ResultPoint>> usedDart;
    for (const auto& [u, neighbors] : adjacency) {
        for (const auto& v : neighbors) {
            if (usedDart.count({u, v})) {
                continue;
            }
            std::vector<ResultPoint> cycle;
            ResultPoint a = u;
            ResultPoint b = v;
            do {
                usedDart.insert({a, b});
                cycle.push_back(a);
                const ResultPoint c = rotationalNext(b, a);
                a = b;
                b = c;
            } while (a != u || b != v);

            // Shoelace twice-area; keep counterclockwise (filled) faces only.
            Number twiceArea(0);
            for (std::size_t i = 0; i < cycle.size(); ++i) {
                const ResultPoint& p = cycle[i];
                const ResultPoint& q = cycle[(i + 1) % cycle.size()];
                twiceArea += p.x() * q.y() - q.x() * p.y();
            }
            if (twiceArea > Number(0)) {
                result.emplace_back(ResultPolygon(cycle));
            }
        }
    }

    return result;
}

template <class PointType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr std::vector<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>, Polygon<Point<ResultNumber, typename PointType::LabelType>>>>
Polygon<PointType>::intersection(const Halfplane<OtherPoint>& other) const {
    using ResultPoint = Point<ResultNumber, typename PointType::LabelType>;
    using ResultSegment = Segment<ResultPoint>;
    using ResultPolygon = Polygon<ResultPoint>;
    using Piece = std::variant<ResultPoint, ResultSegment, ResultPolygon>;

    std::vector<Piece> result;
    if (size() == 0) {
        return result;
    }

    // A degenerate half-plane is a single point.
    if (other.isDegenerate()) {
        const ResultPoint p(other.source());
        if (contains(p)) {
            result.emplace_back(p);
        }
        return result;
    }

    // The boundary of P ∩ H is (∂P ∩ H) ∪ (∂H ∩ P): clip each polygon edge to
    // the closed half-plane, and clip the half-plane's boundary line to the
    // polygon. The sets deduplicate shared boundary.
    std::set<ResultSegment> segments;
    std::set<ResultPoint> touchPoints;

    auto collect = [&](const std::optional<std::variant<ResultPoint, ResultSegment>>& piece) {
        if (!piece) {
            return;
        }
        if (std::holds_alternative<ResultPoint>(*piece)) {
            touchPoints.insert(std::get<ResultPoint>(*piece));
        } else {
            segments.insert(std::get<ResultSegment>(*piece));
        }
    };

    for (const auto& edge : edges()) {
        collect(other.template intersection<ResultNumber>(edge));
    }
    for (const auto& piece : intersection<ResultNumber>(Line<OtherPoint>(other.source(), other.target()))) {
        if (std::holds_alternative<ResultPoint>(piece)) {
            touchPoints.insert(std::get<ResultPoint>(piece));
        } else {
            segments.insert(std::get<ResultSegment>(piece));
        }
    }

    // Make the collected boundary a consistent planar graph (see the polygon
    // overload): split each segment at any other segment's endpoint in its
    // interior, then dedup, so overlapping collinear shared boundary collapses.
    auto planarize = [](const std::set<ResultSegment>& input) {
        std::vector<ResultPoint> vertices;
        for (const auto& s : input) {
            vertices.push_back(s.min());
            vertices.push_back(s.max());
        }
        std::sort(vertices.begin(), vertices.end());
        vertices.erase(std::unique(vertices.begin(), vertices.end()), vertices.end());

        std::set<ResultSegment> out;
        for (const auto& s : input) {
            const ResultPoint a = s.min();
            const ResultPoint b = s.max();
            std::vector<ResultPoint> cut{a, b};
            for (const auto& v : vertices) {
                if (v == a || v == b || !collinear(a, b, v)) {
                    continue;
                }
                if (v.x() < std::min(a.x(), b.x()) || v.x() > std::max(a.x(), b.x()) ||
                    v.y() < std::min(a.y(), b.y()) || v.y() > std::max(a.y(), b.y())) {
                    continue;
                }
                cut.push_back(v);
            }
            std::sort(cut.begin(), cut.end());
            cut.erase(std::unique(cut.begin(), cut.end()), cut.end());
            for (std::size_t i = 0; i + 1 < cut.size(); ++i) {
                out.insert(ResultSegment(cut[i], cut[i + 1]));
            }
        }
        return out;
    };
    segments = planarize(segments);

    // Build the undirected graph: nodes are endpoints, edges are the segments.
    std::map<ResultPoint, std::vector<ResultPoint>> adjacency;
    for (const auto& segment : segments) {
        adjacency[segment.min()].push_back(segment.max());
        adjacency[segment.max()].push_back(segment.min());
    }

    // A touch point that no segment uses is an isolated intersection point.
    for (const auto& point : touchPoints) {
        if (adjacency.find(point) == adjacency.end()) {
            result.emplace_back(point);
        }
    }

    using Number = ResultNumber;
    auto degree = [&adjacency](const ResultPoint& p) {
        const auto it = adjacency.find(p);
        return it == adjacency.end() ? std::size_t(0) : it->second.size();
    };
    auto removeEdge = [&adjacency](const ResultPoint& u, const ResultPoint& v) {
        auto& au = adjacency[u];
        au.erase(std::find(au.begin(), au.end(), v));
        auto& av = adjacency[v];
        av.erase(std::find(av.begin(), av.end(), u));
    };

    // Peel the open boundary into segments. Every 1D piece lies on the
    // half-plane's straight boundary, so each peeled chain is collinear and is
    // returned as the single segment spanning its two ends.
    while (true) {
        const ResultPoint* leaf = nullptr;
        for (const auto& [p, neighbors] : adjacency) {
            if (neighbors.size() == 1) {
                leaf = &p;
                break;
            }
        }
        if (!leaf) {
            break;
        }
        const ResultPoint start = *leaf;
        ResultPoint current = start;
        while (degree(current) == 1) {
            const ResultPoint next = adjacency.at(current)[0];
            removeEdge(current, next);
            current = next;
        }
        result.emplace_back(ResultSegment(start, current));
    }

    // Trace the remaining cycles as faces; keep the counterclockwise (filled)
    // ones. The clockwise-next rule resolves any articulation (pinch) vertices.
    auto rotationalNext = [&adjacency](const ResultPoint& at, const ResultPoint& from) {
        const auto& neighbors = adjacency.at(at);
        auto dx = [&](const ResultPoint& p) { return p.x() - at.x(); };
        auto dy = [&](const ResultPoint& p) { return p.y() - at.y(); };
        auto half = [&](const ResultPoint& p) {
            const Number y = dy(p);
            if (y > Number(0)) return 0;
            if (y < Number(0)) return 1;
            return dx(p) >= Number(0) ? 0 : 1;
        };
        auto ccwBefore = [&](const ResultPoint& u, const ResultPoint& w) {
            const int hu = half(u), hw = half(w);
            if (hu != hw) {
                return hu < hw;
            }
            return dx(u) * dy(w) - dy(u) * dx(w) > Number(0);
        };
        const ResultPoint* best = nullptr;
        for (const auto& n : neighbors) {
            if (n == from || !ccwBefore(n, from)) {
                continue;
            }
            if (!best || ccwBefore(*best, n)) {
                best = &n;
            }
        }
        if (!best) {
            for (const auto& n : neighbors) {
                if (n != from && (!best || ccwBefore(*best, n))) {
                    best = &n;
                }
            }
        }
        return *best;
    };

    std::set<std::pair<ResultPoint, ResultPoint>> usedDart;
    for (const auto& [u, neighbors] : adjacency) {
        for (const auto& v : neighbors) {
            if (usedDart.count({u, v})) {
                continue;
            }
            std::vector<ResultPoint> cycle;
            ResultPoint a = u;
            ResultPoint b = v;
            do {
                usedDart.insert({a, b});
                cycle.push_back(a);
                const ResultPoint c = rotationalNext(b, a);
                a = b;
                b = c;
            } while (a != u || b != v);

            Number twiceArea(0);
            for (std::size_t i = 0; i < cycle.size(); ++i) {
                const ResultPoint& p = cycle[i];
                const ResultPoint& q = cycle[(i + 1) % cycle.size()];
                twiceArea += p.x() * q.y() - q.x() * p.y();
            }
            if (twiceArea > Number(0)) {
                result.emplace_back(ResultPolygon(cycle));
            }
        }
    }

    return result;
}

}  // namespace pgl
