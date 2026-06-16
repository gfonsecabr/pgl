#pragma once

#include "pgl.hpp"

/**
 * @file bounding.hpp
 * @brief Bounding-box and rectangle-boundary operations.
 */

#include "geometry/rectangle.hpp"

namespace pgl {

// -----------------------------------------------------------------------------
// Point

template <class Number, class Label>
constexpr Rectangle<Point<Number, Label>> Point<Number, Label>::bbox() const {
    return Rectangle<Point<Number, Label>>(*this, *this, true);
}

template <class Number, class Label>
template <std::floating_point ResultNumber>
constexpr Rectangle<Point<ResultNumber>> Point<Number, Label>::fbox() const {
    return Rectangle<Point<ResultNumber>>(
        detail::lowerFloatingBound<ResultNumber>(x()),
        detail::lowerFloatingBound<ResultNumber>(y()),
        detail::upperFloatingBound<ResultNumber>(x()),
        detail::upperFloatingBound<ResultNumber>(y()),
        true);
}

template <class Number, class Label>
constexpr std::array<Point<Number, Label>, 1> Point<Number, Label>::vertices() const {
    return {*this};
}

template <class Number, class Label>
constexpr std::array<Segment<Point<Number, Label>>, 0> Point<Number, Label>::edges() const {
    return {};
}

template <class Number, class Label>
constexpr std::array<OrientedSegment<Point<Number, Label>>, 0> Point<Number, Label>::orientedEdges() const {
    return {};
}

// -----------------------------------------------------------------------------
// Segment

template <class PointType, class LabelType>
template <std::floating_point ResultNumber, class Value>
constexpr ResultNumber Segment<PointType, LabelType>::lowerCoordinateBound(const Value& value) {
    if constexpr (requires { value.template lowerBound<ResultNumber>(); }) {
        return value.template lowerBound<ResultNumber>();
    } else {
        return static_cast<ResultNumber>(value);
    }
}

template <class PointType, class LabelType>
template <std::floating_point ResultNumber, class Value>
constexpr ResultNumber Segment<PointType, LabelType>::upperCoordinateBound(const Value& value) {
    if constexpr (requires { value.template upperBound<ResultNumber>(); }) {
        return value.template upperBound<ResultNumber>();
    } else {
        return static_cast<ResultNumber>(value);
    }
}

template <class PointType, class LabelType>
constexpr Rectangle<PointType> Segment<PointType, LabelType>::bbox() const {
    if (min().y() < max().y()) {
        return Rectangle<PointType>(min().x(), min().y(), max().x(), max().y(), true);
    }
    return Rectangle<PointType>(min().x(), max().y(), max().x(), min().y(), true);
}

template <class PointType, class LabelType>
template <std::floating_point ResultNumber>
constexpr Rectangle<Point<ResultNumber>> Segment<PointType, LabelType>::fbox() const {
    ResultNumber xmin = lowerCoordinateBound<ResultNumber>(min().x());
    ResultNumber ymin = lowerCoordinateBound<ResultNumber>(min().y());
    ResultNumber xmax = upperCoordinateBound<ResultNumber>(max().x());
    ResultNumber ymax = upperCoordinateBound<ResultNumber>(max().y());
    if (ymin > ymax) {
        std::swap(ymin, ymax);
    }
    return Rectangle<Point<ResultNumber>>(xmin, ymin, xmax, ymax, true);
}

template <class PointType, class LabelType>
constexpr std::array<PointType, 2> Segment<PointType, LabelType>::vertices() const {
    return {min(), max()};
}

template <class PointType, class LabelType>
constexpr std::array<Segment<PointType, LabelType>, 1> Segment<PointType, LabelType>::edges() const {
    return {*this};
}

template <class PointType, class LabelType>
constexpr std::array<OrientedSegment<PointType>, 1> Segment<PointType, LabelType>::orientedEdges() const {
    return {OrientedSegment<PointType>(min(), max())};
}

// -----------------------------------------------------------------------------
// OrientedSegment

template <class PointType, class LabelType>
constexpr Rectangle<PointType> OrientedSegment<PointType, LabelType>::bbox() const {
    return static_cast<Segment<PointType>>(*this).bbox();
}

template <class PointType, class LabelType>
template <std::floating_point ResultNumber>
constexpr Rectangle<Point<ResultNumber>> OrientedSegment<PointType, LabelType>::fbox() const {
    return static_cast<Segment<PointType>>(*this).template fbox<ResultNumber>();
}

template <class PointType, class LabelType>
constexpr std::array<PointType, 2> OrientedSegment<PointType, LabelType>::vertices() const {
    return {source(), target()};
}

template <class PointType, class LabelType>
constexpr std::array<Segment<PointType>, 1> OrientedSegment<PointType, LabelType>::edges() const {
    return {static_cast<Segment<PointType>>(*this)};
}

template <class PointType, class LabelType>
constexpr std::array<OrientedSegment<PointType, LabelType>, 1> OrientedSegment<PointType, LabelType>::orientedEdges() const {
    return {*this};
}

// -----------------------------------------------------------------------------
// Rectangle

template <class PointType>
constexpr Rectangle<PointType> Rectangle<PointType>::bbox() const {
    return *this;
}

template <class PointType>
template <std::floating_point ResultNumber>
constexpr Rectangle<Point<ResultNumber>> Rectangle<PointType>::fbox() const {
    return Rectangle<Point<ResultNumber>>(
        detail::lowerFloatingBound<ResultNumber>(min().x()),
        detail::lowerFloatingBound<ResultNumber>(min().y()),
        detail::upperFloatingBound<ResultNumber>(max().x()),
        detail::upperFloatingBound<ResultNumber>(max().y()),
        true);
}

template <class PointType>
constexpr typename Rectangle<PointType>::PointType Rectangle<PointType>::bottomRight() const {
    return makeCorner(max().x(), min().y());
}

template <class PointType>
constexpr typename Rectangle<PointType>::PointType Rectangle<PointType>::topLeft() const {
    return makeCorner(min().x(), max().y());
}

template <class PointType>
template <bool Oriented>
constexpr typename Rectangle<PointType>::template BoundaryType<Oriented> Rectangle<PointType>::boundaryAt(std::size_t index) const {
    assert(index < edgeCount);

    const auto bottom_left = min();
    const auto bottom_right = bottomRight();
    const auto top_right = max();
    const auto top_left = topLeft();

    switch (index) {
    case 0:
        return BoundaryType<Oriented>(bottom_left, bottom_right);
    case 1:
        return BoundaryType<Oriented>(bottom_right, top_right);
    case 2:
        return BoundaryType<Oriented>(top_right, top_left);
    default:
        return BoundaryType<Oriented>(top_left, bottom_left);
    }
}

template <class PointType>
constexpr std::array<typename Rectangle<PointType>::PointType, 4> Rectangle<PointType>::vertices() const {
    return {
        min(),
        bottomRight(),
        max(),
        topLeft(),
    };
}

template <class PointType>
constexpr std::array<Segment<PointType>, 4> Rectangle<PointType>::edges() const {
    return {
        boundaryAt<false>(0),
        boundaryAt<false>(1),
        boundaryAt<false>(2),
        boundaryAt<false>(3),
    };
}

template <class PointType>
constexpr std::array<OrientedSegment<PointType>, 4> Rectangle<PointType>::orientedEdges() const {
    return {
        boundaryAt<true>(0),
        boundaryAt<true>(1),
        boundaryAt<true>(2),
        boundaryAt<true>(3),
    };
}

// -----------------------------------------------------------------------------
// Triangle

template <class PointType>
constexpr Rectangle<PointType> Triangle<PointType>::bbox() const {
    return Rectangle<PointType>(points_);
}

template <class PointType>
template <std::floating_point ResultNumber>
constexpr Rectangle<Point<ResultNumber>> Triangle<PointType>::fbox() const {
    return bbox().template fbox<ResultNumber>();
}

template <class PointType>
constexpr std::array<PointType, 3> Triangle<PointType>::vertices() const {
    return points_;
}

template <class PointType>
constexpr std::array<Segment<PointType>, 3> Triangle<PointType>::edges() const {
    return {
        Segment<PointType>(a(), b()),
        Segment<PointType>(b(), c()),
        Segment<PointType>(c(), a()),
    };
}

template <class PointType>
constexpr std::array<OrientedSegment<PointType>, 3> Triangle<PointType>::orientedEdges() const {
    return {
        OrientedSegment<PointType>(a(), b()),
        OrientedSegment<PointType>(b(), c()),
        OrientedSegment<PointType>(c(), a()),
    };
}

// template <class PointType>
// template <std::ranges::input_range Range>
//     requires std::constructible_from<PointType, std::ranges::range_reference_t<Range>>
// constexpr void Rectangle<PointType>::assignBoundingBox(Range&& points) {
//     auto iterator = std::ranges::begin(points);
//     const auto sentinel = std::ranges::end(points);
//
//     if (iterator == sentinel) {
//         throw std::invalid_argument("Rectangle bounding box requires at least one point");
//     }
//
//     PointType min_corner(*iterator);
//     PointType max_corner(*iterator);
//     std::optional<PointType> exact_min_corner(min_corner);
//     std::optional<PointType> exact_max_corner(max_corner);
//     ++iterator;
//
//     for (; iterator != sentinel; ++iterator) {
//         const PointType point(*iterator);
//         const PointType previous_min = min_corner;
//         const PointType previous_max = max_corner;
//
//         if (point.x() < min_corner.x()) {
//             min_corner = makeCorner(point.x(), min_corner.y());
//         }
//         if (point.y() < min_corner.y()) {
//             min_corner = makeCorner(min_corner.x(), point.y());
//         }
//         if (max_corner.x() < point.x()) {
//             max_corner = makeCorner(point.x(), max_corner.y());
//         }
//         if (max_corner.y() < point.y()) {
//             max_corner = makeCorner(max_corner.x(), point.y());
//         }
//
//         if (point.x() == min_corner.x() && point.y() == min_corner.y()) {
//             exact_min_corner = point;
//         } else if (previous_min != min_corner) {
//             exact_min_corner.reset();
//         }
//
//         if (point.x() == max_corner.x() && point.y() == max_corner.y()) {
//             exact_max_corner = point;
//         } else if (previous_max != max_corner) {
//             exact_max_corner.reset();
//         }
//     }
//
//     points_[0] = exact_min_corner.has_value() ? std::move(*exact_min_corner) : std::move(min_corner);
//     points_[1] = exact_max_corner.has_value() ? std::move(*exact_max_corner) : std::move(max_corner);
// }

template <class PointType>
template <PointConcept OtherPoint>
constexpr void Rectangle<PointType>::insert(const OtherPoint& point) {
    const PointType old_min = min();
    const PointType old_max = max();

    const NumberType x = static_cast<NumberType>(point.x());
    const NumberType y = static_cast<NumberType>(point.y());

    NumberType min_x = old_min.x();
    NumberType min_y = old_min.y();
    NumberType max_x = old_max.x();
    NumberType max_y = old_max.y();

    if (x < min_x) {
        min_x = x;
    }
    if (y < min_y) {
        min_y = y;
    }
    if (max_x < x) {
        max_x = x;
    }
    if (max_y < y) {
        max_y = y;
    }

    points_[0] = (min_x == old_min.x() && min_y == old_min.y())
        ? old_min
        : makeCorner(min_x, min_y);
    points_[1] = (max_x == old_max.x() && max_y == old_max.y())
        ? old_max
        : makeCorner(max_x, max_y);
}

template <class PointType>
template <RectangleConcept OtherRectangle>
constexpr void Rectangle<PointType>::insert(const OtherRectangle& other) {
    insert(other.min());
    insert(other.max());
}

template <class PointType>
template<std::ranges::input_range Range>
requires std::ranges::common_range<Range> &&
         std::convertible_to<std::ranges::range_value_t<Range>, PointType>
constexpr void Rectangle<PointType>::insert(Range&& range) {
    for (const auto& point : range) {
        insert(point);
    }
}

template <class PointType>
template <class TShape>
    requires(!detail::is_point_v<TShape> && requires(const TShape& shape) { shape.bbox(); })
constexpr void Rectangle<PointType>::insert(const TShape& shape) {
    const auto box = shape.bbox();
    insert(box.min());
    insert(box.max());
}

template <class PointType>
template <std::ranges::input_range Range>
    requires(!detail::is_point_v<typename std::ranges::range_value_t<Range>> && requires(const typename std::ranges::range_value_t<Range>& shape) { shape.bbox(); })
constexpr void Rectangle<PointType>::insert(Range&& range) {
    for (const auto& shape : range) {
        insert(shape);
    }
}


// ---------------------------------------------------------------------------
// Convex

template <class PointType>
constexpr const Rectangle<PointType>& Convex<PointType>::bbox() const {
    if (bbox_) {
        return *bbox_;
    }
    if (points_.empty()) {
        return bbox_.emplace();
    }
    if (points_.size() <= 6) {
        return bbox_.emplace(Rectangle<PointType>(points_) + translation_);
    }

    // Find the peak of a plain unimodal range [first, last): the keys ascend
    // weakly to a single maximum, then descend weakly. O(log n) binary search.
    const auto unimodalMax = [](auto first, auto last, auto key) {
        auto lo = first;
        auto hi = last - 1;
        while (lo < hi) {
            const auto mid = lo + (hi - lo) / 2;
            if (key(*mid) < key(*(mid + 1)))
                lo = mid + 1;
            else
                hi = mid;
        }
        return lo;
    };

    // The boundary splits at its leftmost (points_[0]) and rightmost (maxIndex())
    // vertices into two x-monotone chains. On each chain y is plain unimodal (the
    // split removes the rotation cyclicMax would otherwise have to handle). The
    // lower chain [0, mi] is contiguous; the upper chain [mi, n) wraps back to
    // points_[0], so that vertex is folded into the max_y candidate explicitly.
    const NumberType min_x = points_[0].x();
    const auto mi = maxIndex();
    const NumberType max_x = points_[mi].x();
    const NumberType min_y = unimodalMax(points_.begin(), points_.begin() + mi + 1,
        [](const PointType& p) { return -p.y(); })->y();
    const NumberType max_y = std::max(
        unimodalMax(points_.begin() + mi, points_.end(),
            [](const PointType& p) { return p.y(); })->y(),
        points_[0].y());

    return bbox_.emplace(Rectangle<PointType>(min_x, min_y, max_x, max_y, true) + translation_);
}

template <class PointType>
template <std::floating_point ResultNumber>
constexpr Rectangle<Point<ResultNumber>> Convex<PointType>::fbox() const {
    return bbox().template fbox<ResultNumber>();
}

// ---------------------------------------------------------------------------
// Polygon

template <class PointType>
constexpr const Rectangle<PointType>& Polygon<PointType>::bbox() const {
    if (bbox_) {
        return *bbox_;
    }
    if (points_.empty()) {
        return bbox_.emplace();
    }
    return bbox_.emplace(Rectangle<PointType>(points_) + translation_);
}

template <class PointType>
template <std::floating_point ResultNumber>
constexpr Rectangle<Point<ResultNumber>> Polygon<PointType>::fbox() const {
    return bbox().template fbox<ResultNumber>();
}

}  // namespace pgl
