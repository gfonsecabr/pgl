#pragma once

#include "pgl.hpp"

/**
 * @file atxy.hpp
 * @brief Coordinate-evaluation helpers for linear primitives.
 */


namespace pgl {

// -----------------------------------------------------------------------------
// Segment

/**
 * @brief Evaluates the y-coordinate of a segment at a given x-coordinate.
 *
 * The value is returned only when `x` lies within the segment's horizontal
 * extent. Endpoints are handled exactly.
 *
 * @tparam ResultNumber Return coordinate type.
 * @tparam OtherNumber Query x-coordinate type.
 * @param x Query x-coordinate.
 * @return Interpolated y-coordinate, or empty when `x` is outside the segment.
 */
template <class PointType, class LabelType>
template <class ResultNumber, class OtherNumber>
constexpr std::optional<ResultNumber>
Segment<PointType, LabelType>::yAtX(const OtherNumber &x) const {
    if (x < min().x() || x > max().x()) {
        return {};
    }
    if (x == min().x())
        return static_cast<ResultNumber>(min().y());
    if (x == max().x())
        return static_cast<ResultNumber>(max().y());

    const ResultNumber minX = static_cast<ResultNumber>(min().x());
    const ResultNumber minY = static_cast<ResultNumber>(min().y());
    const ResultNumber deltaX = static_cast<ResultNumber>(max().x() - min().x());
    const ResultNumber deltaY = static_cast<ResultNumber>(max().y() - min().y());

    const ResultNumber y = minY + (static_cast<ResultNumber>(x) - minX) * deltaY / deltaX;
    return y;
}

/**
 * @brief Evaluates the x-coordinate of a segment at a given y-coordinate.
 *
 * The value is returned only when `y` lies within the segment's vertical
 * extent. Endpoints are handled exactly.
 *
 * @tparam ResultNumber Return coordinate type.
 * @tparam OtherNumber Query y-coordinate type.
 * @param y Query y-coordinate.
 * @return Interpolated x-coordinate, or empty when `y` is outside the segment.
 */
template <class PointType, class LabelType>
template <class ResultNumber, class OtherNumber>
constexpr std::optional<ResultNumber> Segment<PointType, LabelType>::xAtY(const OtherNumber &y) const {
    const auto min_y = min().y() < max().y() ? min().y() : max().y();
    const auto max_y = max().y() < min().y() ? min().y() : max().y();

    if (y < min_y || y > max_y) {
        return {};
    }
    if (y == min().y()) {
        return static_cast<ResultNumber>(min().x());
    }
    if (y == max().y()) {
        return static_cast<ResultNumber>(max().x());
    }

    const ResultNumber min_x = static_cast<ResultNumber>(min().x());
    const ResultNumber min_y_value = static_cast<ResultNumber>(min().y());
    const ResultNumber delta_x = static_cast<ResultNumber>(max().x() - min().x());
    const ResultNumber delta_y = static_cast<ResultNumber>(max().y() - min().y());

    return min_x + (static_cast<ResultNumber>(y) - min_y_value) * delta_x / delta_y;
}

// -----------------------------------------------------------------------------
// OrientedSegment

/**
 * @brief Evaluates the y-coordinate of the supporting unordered segment.
 *
 * The orientation does not affect the returned geometric point, so this
 * delegates to the corresponding unordered segment.
 */
template <class PointType, class LabelType>
template <class ResultNumber, class OtherNumber>
constexpr std::optional<ResultNumber>
OrientedSegment<PointType, LabelType>::yAtX(const OtherNumber &x) const {
    return static_cast<Segment<PointType>>(*this).template yAtX<ResultNumber>(x);
}

/**
 * @brief Evaluates the x-coordinate of the supporting unordered segment.
 *
 * The orientation does not affect the returned geometric point, so this
 * delegates to the corresponding unordered segment.
 */
template <class PointType, class LabelType>
template <class ResultNumber, class OtherNumber>
constexpr std::optional<ResultNumber>
OrientedSegment<PointType, LabelType>::xAtY(const OtherNumber &y) const {
    return static_cast<Segment<PointType>>(*this).template xAtY<ResultNumber>(y);
}

// -----------------------------------------------------------------------------
// Line

/**
 * @brief Evaluates the y-coordinate of a line at a given x-coordinate.
 *
 * Non-vertical lines always define a unique result. For vertical lines, the
 * query succeeds only at the line x-coordinate, where the stored `min().y()`
 * is returned
 *
 * @tparam ResultNumber Return coordinate type.
 * @tparam OtherNumber Query x-coordinate type.
 * @param x Query x-coordinate.
 * @return Corresponding y-coordinate, or empty when undefined.
 */
template <class PointType, class LabelType>
template <class ResultNumber, class OtherNumber>
constexpr std::optional<ResultNumber>
Line<PointType, LabelType>::yAtX(const OtherNumber& x) const {
    if (isVertical()) {
        if (x == min().x()) {
            return static_cast<ResultNumber>(min().y());
        }
        return {};
    }

    if (x == min().x()) {
        return static_cast<ResultNumber>(min().y());
    }
    if (x == max().x()) {
        return static_cast<ResultNumber>(max().y());
    }

    const ResultNumber min_x = static_cast<ResultNumber>(min().x());
    const ResultNumber min_y = static_cast<ResultNumber>(min().y());
    const ResultNumber delta_x = static_cast<ResultNumber>(max().x() - min().x());
    const ResultNumber delta_y = static_cast<ResultNumber>(max().y() - min().y());

    return min_y + (static_cast<ResultNumber>(x) - min_x) * delta_y / delta_x;
}

/**
 * @brief Evaluates the x-coordinate of a line at a given y-coordinate.
 *
 * Non-horizontal lines always define a unique result. For horizontal lines,
 * the query succeeds only at the line y-coordinate, where the stored `min().x()`
 * is returned
 *
 * @tparam ResultNumber Return coordinate type.
 * @tparam OtherNumber Query y-coordinate type.
 * @param y Query y-coordinate.
 * @return Corresponding x-coordinate, or empty when undefined.
 */
template <class PointType, class LabelType>
template <class ResultNumber, class OtherNumber>
constexpr std::optional<ResultNumber>
Line<PointType, LabelType>::xAtY(const OtherNumber& y) const {
    if (isHorizontal()) {
        if (y == min().y()) {
            return static_cast<ResultNumber>(min().x());
        }
        return {};
    }

    if (y == min().y()) {
        return static_cast<ResultNumber>(min().x());
    }
    if (y == max().y()) {
        return static_cast<ResultNumber>(max().x());
    }

    const ResultNumber min_x = static_cast<ResultNumber>(min().x());
    const ResultNumber min_y = static_cast<ResultNumber>(min().y());
    const ResultNumber delta_x = static_cast<ResultNumber>(max().x() - min().x());
    const ResultNumber delta_y = static_cast<ResultNumber>(max().y() - min().y());

    return min_x + (static_cast<ResultNumber>(y) - min_y) * delta_x / delta_y;
}

// -----------------------------------------------------------------------------
// OrientedLine

/**
 * @brief Evaluates the y-coordinate of the supporting line at a given x-coordinate.
 *
 * The stored orientation does not affect the returned geometric point, so this
 * delegates to the corresponding unoriented line.
 *
 * @tparam ResultNumber Return coordinate type.
 * @tparam OtherNumber Query x-coordinate type.
 * @param x Query x-coordinate.
 * @return Corresponding y-coordinate, or empty when undefined.
 */
template <class PointType, class LabelType>
template <class ResultNumber, class OtherNumber>
constexpr std::optional<ResultNumber>
OrientedLine<PointType, LabelType>::yAtX(const OtherNumber& x) const {
    return this->asLine().template yAtX<ResultNumber>(x);
}

/**
 * @brief Evaluates the x-coordinate of the supporting line at a given y-coordinate.
 *
 * The stored orientation does not affect the returned geometric point, so this
 * delegates to the corresponding unoriented line.
 *
 * @tparam ResultNumber Return coordinate type.
 * @tparam OtherNumber Query y-coordinate type.
 * @param y Query y-coordinate.
 * @return Corresponding x-coordinate, or empty when undefined.
 */
template <class PointType, class LabelType>
template <class ResultNumber, class OtherNumber>
constexpr std::optional<ResultNumber>
OrientedLine<PointType, LabelType>::xAtY(const OtherNumber& y) const {
    return this->asLine().template xAtY<ResultNumber>(y);
}

// -----------------------------------------------------------------------------
// Ray

/**
 * @brief Evaluates the y-coordinate of a ray at a given x-coordinate.
 *
 * The supporting line computes the candidate coordinate; the ray direction
 * decides whether that candidate lies on the half-line.
 */
template <class PointType, class LabelType>
template <class ResultNumber, class OtherNumber>
constexpr std::optional<ResultNumber>
Ray<PointType, LabelType>::yAtX(const OtherNumber &x) const {
    const Line<PointType> supporting_line(*this);
    const std::optional<ResultNumber> candidate_y = supporting_line.template yAtX<ResultNumber>(x);
    if (!candidate_y.has_value()) {
        return {};
    }

    const ResultNumber candidate_x = static_cast<ResultNumber>(x);
    const Point<ResultNumber, typename PointType::LabelType> candidate_point(candidate_x, *candidate_y);
    if (!contains(candidate_point)) {
        return {};
    }

    return candidate_y;
}

/**
 * @brief Evaluates the x-coordinate of a ray at a given y-coordinate.
 *
 * The supporting line computes the candidate coordinate; the ray direction
 * decides whether that candidate lies on the half-line.
 */
template <class PointType, class LabelType>
template <class ResultNumber, class OtherNumber>
constexpr std::optional<ResultNumber>
Ray<PointType, LabelType>::xAtY(const OtherNumber &y) const {
    const Line<PointType> supporting_line(*this);
    const std::optional<ResultNumber> candidate_x = supporting_line.template xAtY<ResultNumber>(y);
    if (!candidate_x.has_value()) {
        return {};
    }

    const ResultNumber candidate_y = static_cast<ResultNumber>(y);
    const Point<ResultNumber, typename PointType::LabelType> candidate_point(*candidate_x, candidate_y);
    if (!contains(candidate_point)) {
        return {};
    }

    return candidate_x;
}


// ---------------------------------------------------------------------------
// Convex

template <class PointType>
template<class OtherNumberType>
constexpr std::optional<std::array<Segment<PointType>, 2>> Convex<PointType>::edgesAtX(OtherNumberType x) const {
    const size_t n = points_.size();
    if (n < 3) {
        return {};
    }
    using CommonNumberType = std::common_type_t<NumberType, OtherNumberType>;
    const CommonNumberType target =
        static_cast<CommonNumberType>(x) - static_cast<CommonNumberType>(translation_.x());
    const size_t m = maxIndex();
    const CommonNumberType min_x = static_cast<CommonNumberType>(points_[0].x());
    const CommonNumberType max_x = static_cast<CommonNumberType>(points_[m].x());
    if (target < min_x || target > max_x) {
        return {};  // the vertical line misses the polygon
    }
    const bool at_max = (target == max_x);

    // Walk a monotone-in-x chain and return the non-vertical edge whose x-span
    // covers `target`. `at(s)` is the x of the chain's s-th vertex (non-decreasing);
    // `pos(s)` maps a chain position to a vertex index. The edge runs from pos(s-1)
    // to pos(s). Skipping to the first vertex strictly past `target` (or, at the
    // right extreme, the first vertex reaching it) lands on a non-vertical edge,
    // because a vertical edge can only sit at the leftmost/rightmost x.
    const auto edge = [&](auto at, auto pos, size_t count) {
        size_t lo = 1, hi = count - 1;
        while (lo < hi) {
            const size_t mid = lo + (hi - lo) / 2;
            const bool past = at_max ? (at(mid) >= target) : (at(mid) > target);
            if (past) {
                hi = mid;
            } else {
                lo = mid + 1;
            }
        }
        return Segment<PointType>(points_[pos(lo - 1)], points_[pos(lo)]) + translation_;
    };

    // Lower chain: indices 0..m (x non-decreasing).
    const auto lower = edge(
        [this](size_t i) { return static_cast<CommonNumberType>(points_[i].x()); },
        [](size_t i) { return i; },
        m + 1);
    // Upper chain read leftmost->rightmost: positions 0..(n-m) map to vertices
    // 0, n-1, n-2, ..., m, whose x is non-decreasing.
    const auto upper = edge(
        [this, n](size_t s) { return static_cast<CommonNumberType>(points_[(n - s) % n].x()); },
        [n](size_t s) { return (n - s) % n; },
        n - m + 1);

    return std::array<Segment<PointType>, 2>{lower, upper};
}

}  // namespace pgl
