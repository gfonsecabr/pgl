#pragma once

#include "core/hash.hpp"

/**
 * @file lattice.hpp
 * @brief Enumeration of the integer grid points a shape contains.
 *
 * A lattice point is a point whose two coordinates are whole numbers. The
 * shapes only declare the operation; the arithmetic that decides which of them
 * a shape passes through -- exact for every coordinate type, and never a
 * rounding -- is kept together here.
 */

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <ranges>
#include <stdexcept>
#include <type_traits>
#include <unordered_set>
#include <vector>

namespace pgl {

namespace detail {

/** @brief Whether a coordinate is exactly a whole number. */
template <class Number>
[[nodiscard]] bool isWholeCoordinate(const Number& value) {
    if constexpr (is_Rational_v<Number>) {
        return value.isInteger();
    } else if constexpr (std::is_floating_point_v<Number>) {
        return std::isfinite(value) && value == std::floor(value);
    } else {
        return true;
    }
}

/**
 * @brief Whether an exact integer value fits @p Int.
 *
 * The range test itself is skipped where the value's own type is too narrow to
 * leave the range: comparing against a bound it cannot even spell would narrow
 * the bound instead of the value.
 */
template <class Int, class Integer>
[[nodiscard]] bool latticeFits(const Integer& value) {
    if constexpr (std::same_as<Int, Integer>) {
        return true;
    } else if constexpr (numeric_limits<Integer>::is_specialized
                         && numeric_limits<Integer>::digits < numeric_limits<Int>::digits) {
        return true;
    } else {
        return representableAs<Int>(value);
    }
}

/**
 * @brief Throws unless an exact integer value fits @p Int.
 *
 * A lattice point is named by integers of the result type, so one the type
 * cannot hold is refused rather than wrapped into a different point.
 */
template <class Int, class Integer>
void requireLatticeFits(const Integer& value) {
    if (!latticeFits<Int>(value)) {
        throw std::logic_error("pgl::latticePoints: a lattice point does not fit the result type");
    }
}

/** @brief Converts an exact integer value to @p Int, or throws if it does not fit. */
template <class Int, class Integer>
[[nodiscard]] Int latticeNarrow(const Integer& value) {
    requireLatticeFits<Int>(value);
    return narrowTo<Int>(value);
}

/** @brief Converts a whole floating-point value to @p Int, or throws. */
template <class Int, class Float>
[[nodiscard]] Int latticeFromFloat(const Float& value) {
    if (!std::isfinite(value)) {
        throw std::logic_error("pgl::latticePoints: a coordinate is not finite");
    }
    if constexpr (numeric_limits<Int>::is_bounded) {
        // The bound is a power of two, so it and its negation are both exact in
        // any binary floating-point type: this range test never rounds.
        const Float low = static_cast<Float>(numeric_limits<Int>::min());
        if (value < low || value >= -low) {
            throw std::logic_error("pgl::latticePoints: a lattice point does not fit the result type");
        }
        return static_cast<Int>(value);
    } else {
        return Int(value);
    }
}

/** @brief The largest integer at most @p value, as an @p Int. */
template <class Int, class Number>
[[nodiscard]] Int latticeFloor(const Number& value) {
    if constexpr (is_Rational_v<Number>) {
        // Both parts are wanted, and a deferred fraction reduces itself anew on
        // each read, so reduce once and read the reduced form twice.
        const Number reduced = value.simplified();
        using Integer = rational_int_t<Number>;
        const Integer n = reduced.numerator();
        const Integer d = reduced.denominator();   // positive
        return latticeNarrow<Int>(n >= Integer(0) ? n / d : -((-n + d - Integer(1)) / d));
    } else if constexpr (std::is_floating_point_v<Number>) {
        return latticeFromFloat<Int>(std::floor(value));
    } else {
        return latticeNarrow<Int>(value);
    }
}

/** @brief The smallest integer at least @p value, as an @p Int. */
template <class Int, class Number>
[[nodiscard]] Int latticeCeil(const Number& value) {
    if constexpr (is_Rational_v<Number>) {
        const Number reduced = value.simplified();
        using Integer = rational_int_t<Number>;
        const Integer n = reduced.numerator();
        const Integer d = reduced.denominator();   // positive
        return latticeNarrow<Int>(n > Integer(0) ? (n + d - Integer(1)) / d : -((-n) / d));
    } else if constexpr (std::is_floating_point_v<Number>) {
        return latticeFromFloat<Int>(std::ceil(value));
    } else {
        return latticeNarrow<Int>(value);
    }
}

/**
 * @brief The coordinate as an exact fraction over ::pgl::BigInt.
 *
 * The denominator is positive and the pair is exactly the coordinate: a
 * floating-point value is the dyadic fraction it actually stands for, however
 * large its exponent, not a rounding of it.
 */
template <class Number>
[[nodiscard]] std::array<BigInt, 2> exactFraction(const Number& value) {
    if constexpr (is_Rational_v<Number>) {
        const Number reduced = value.simplified();
        return {BigInt(reduced.numerator()), BigInt(reduced.denominator())};
    } else if constexpr (std::is_floating_point_v<Number>) {
        if (!std::isfinite(value)) {
            throw std::logic_error("pgl::latticePoints: a coordinate is not finite");
        }
        // A finite float is exactly significand * 2^shift, with the significand
        // a whole number of at most `digits` bits.
        int exponent = 0;
        const Number fraction = std::frexp(value, &exponent);
        const int digits = numeric_limits<Number>::digits;
        const BigInt significand(std::ldexp(fraction, digits));
        const int shift = exponent - digits;
        if (shift >= 0) {
            return {significand * pow2(shift), BigInt(1)};
        }
        return {significand, pow2(-shift)};
    } else {
        return {BigInt(value), BigInt(1)};
    }
}

/** @brief The number of integers in `[first, last]`, or throws if no vector holds them. */
template <class Integer>
[[nodiscard]] std::size_t latticeCount(const Integer& first, const Integer& last) {
    // Half the addressable range: a bound no allocation clears anyway, and one
    // both this conversion and a 32-bit std::size_t are exact for.
    constexpr std::int64_t limit =
        static_cast<std::int64_t>(std::numeric_limits<std::size_t>::max() / 2);
    const Integer count = last - first + Integer(1);
    if (!latticeFits<std::int64_t>(count) || narrowTo<std::int64_t>(count) > limit) {
        throw std::length_error("pgl::latticePoints: too many lattice points");
    }
    return static_cast<std::size_t>(narrowTo<std::int64_t>(count));
}

/** @brief The largest integer strictly below @p value, as an @p Int. */
template <class Int, class Number>
[[nodiscard]] Int strictlyBelow(const Number& value) {
    const Int below = latticeFloor<Int>(value);
    return isWholeCoordinate(value) ? Int(below - Int(1)) : below;
}

/** @brief An integer column or row index, read as the coordinate type it indexes. */
template <class Number, class Int>
[[nodiscard]] Number coordinateAt(const Int& index) {
    if constexpr (std::same_as<Number, Int>) {
        return index;
    } else if constexpr (is_Rational_v<Number>) {
        return Number(narrowTo<rational_int_t<Number>>(index));
    } else if constexpr (std::is_floating_point_v<Number>) {
        return static_cast<Number>(narrowTo<std::int64_t>(index));
    } else {
        return narrowTo<Number>(index);
    }
}

/**
 * @brief The row below where a non-vertical edge meets the column at @p column.
 *
 * The floor of the crossing, which is all a parity sweep needs: for an integer
 * row, a crossing lies below the row exactly when its floor does, so the sweep
 * never has to compare two crossings as fractions. Exact over integer
 * coordinates, where the crossing is one floor division of a product the
 * coordinates' promoted type holds, as an orientation determinant is.
 */
template <class ResultNumber, class SegmentType>
[[nodiscard]] ResultNumber crossingFloor(const SegmentType& edge, const ResultNumber& column) {
    using Number = typename SegmentType::NumberType;
    const auto& lower = edge.min();   // lower.x() < upper.x(): the edge is not vertical
    const auto& upper = edge.max();
    if constexpr (extended_integral<Number> || std::same_as<Number, BigInt>) {
        using Wide = promoted_number_t<Number>;
        const Wide run = Wide(upper.x()) - Wide(lower.x());   // positive
        const Wide rise = Wide(upper.y()) - Wide(lower.y());
        const Wide offset = (narrowTo<Wide>(column) - Wide(lower.x())) * rise;
        const Wide quotient = offset >= Wide(0) ? offset / run
                                                : -((-offset + run - Wide(1)) / run);
        return latticeNarrow<ResultNumber>(Wide(lower.y()) + quotient);
    } else {
        const Number crossing =
            lower.y() + (coordinateAt<Number>(column) - lower.x()) * (upper.y() - lower.y())
                            / (upper.x() - lower.x());
        return latticeFloor<ResultNumber>(crossing);
    }
}

/**
 * @brief The lattice points of a region, given every edge of its boundary.
 *
 * Two passes answer two parts of the region. The boundary points come from the
 * edges themselves, each answering as a segment. The interior points come from
 * a sweep over the columns the edges span: the crossings of a column, sorted,
 * pair up into the runs of it that are inside, and a run's integer rows are the
 * points it contributes. A point the sweep and the edges both reach is reported
 * once, and a point either of them reaches is in the region, so the two passes
 * together are exactly its lattice points -- the sweep may misplace a point
 * *on* the boundary, which is why the edges answer for those themselves.
 *
 * Only the edges a column actually crosses are examined, kept in an active list
 * as the sweep advances, so a shape costs one pass over its edges plus the
 * crossings it really has.
 */
template <class ResultPoint, class EdgeRange>
[[nodiscard]] std::vector<ResultPoint> regionLatticePoints(const EdgeRange& edges) {
    using ResultNumber = typename ResultPoint::NumberType;
    using EdgeType = std::ranges::range_value_t<EdgeRange>;

    /** @brief An edge together with the columns that cross it. */
    struct Crossed {
        EdgeType edge;
        ResultNumber first;
        ResultNumber last;
    };

    std::vector<Crossed> crossed;
    std::vector<ResultPoint> boundary;
    for (const EdgeType& edge : edges) {
        const std::vector<ResultPoint> own = edge.template latticePoints<ResultNumber>();
        boundary.insert(boundary.end(), own.begin(), own.end());
        if (edge.isVertical()) {
            continue;   // no column crosses it; it is all boundary anyway
        }
        // Half-open in x, so a vertex shared by two edges is crossed by one of
        // them: that is what makes the parity right where the boundary turns.
        const ResultNumber first = latticeCeil<ResultNumber>(edge.min().x());
        const ResultNumber last = strictlyBelow<ResultNumber>(edge.max().x());
        if (last < first) {
            continue;
        }
        crossed.push_back(Crossed{edge, first, last});
    }
    std::sort(boundary.begin(), boundary.end());
    boundary.erase(std::unique(boundary.begin(), boundary.end()), boundary.end());
    if (crossed.empty()) {
        return boundary;   // nothing has an interior: a point, a segment, no shape at all
    }
    std::sort(crossed.begin(), crossed.end(),
              [](const Crossed& left, const Crossed& right) { return left.first < right.first; });

    std::vector<ResultPoint> inside;
    std::vector<const Crossed*> active;
    std::vector<ResultNumber> crossings;
    std::size_t pending = 0;
    ResultNumber column = crossed.front().first;
    while (pending < crossed.size() || !active.empty()) {
        if (active.empty() && pending < crossed.size() && column < crossed[pending].first) {
            column = crossed[pending].first;   // no edge here: on to the next one that has some
        }
        while (pending < crossed.size() && !(column < crossed[pending].first)) {
            active.push_back(&crossed[pending++]);
        }
        std::erase_if(active, [&](const Crossed* edge) { return edge->last < column; });
        if (active.empty()) {
            continue;
        }
        crossings.clear();
        for (const Crossed* edge : active) {
            crossings.push_back(crossingFloor<ResultNumber>(edge->edge, column));
        }
        std::sort(crossings.begin(), crossings.end());
        for (std::size_t i = 0; i + 1 < crossings.size(); i += 2) {
            for (ResultNumber row = crossings[i] + ResultNumber(1); !(crossings[i + 1] < row); ++row) {
                inside.push_back(ResultPoint(column, row));
            }
        }
        ++column;
    }

    std::vector<ResultPoint> points;
    points.reserve(inside.size() + boundary.size());
    std::set_union(inside.begin(), inside.end(), boundary.begin(), boundary.end(),
                   std::back_inserter(points));
    return points;
}

/** @brief Appends every boundary edge of a region, its holes included. */
template <class Region, class SegmentVector>
void appendRegionEdges(const Region& region, SegmentVector& edges) {
    for (const auto& edge : region.outer().edgesView()) {
        edges.push_back(edge);
    }
    for (const auto& hole : region.holes()) {
        for (const auto& edge : hole.edgesView()) {
            edges.push_back(edge);
        }
    }
}

/**
 * @brief The lattice points of a chain of vertices, in traversal order.
 *
 * One edge at a time, in the order the chain walks them, and each point kept
 * only the first time it is reached: consecutive edges share a vertex, and a
 * chain that crosses or retraces itself reaches other points twice as well.
 */
template <class ResultPoint, class Chain>
[[nodiscard]] std::vector<ResultPoint> chainLatticePoints(const Chain& chain) {
    using ResultNumber = typename ResultPoint::NumberType;
    if (chain.size() == 1) {
        // No edge to walk, so the single vertex answers for itself.
        return Segment<typename Chain::PointType>(chain[0], chain[0])
            .template latticePoints<ResultNumber>();
    }
    std::vector<ResultPoint> points;
    std::unordered_set<ResultPoint> reached;
    for (const auto& edge : chain.orientedEdgesView()) {
        for (const ResultPoint& point : edge.template latticePoints<ResultNumber>()) {
            if (reached.insert(point).second) {
                points.push_back(point);
            }
        }
    }
    return points;
}

}  // namespace detail

// -----------------------------------------------------------------------------
// Segment

// The parameter is named as the declaration spells it: the return type reaches
// through it for a nested type, and MSVC matches such a definition to its
// declaration by spelling rather than by parameter position.
template <class PointType, class LabelType>
template <class ResultNumber>
    requires(detail::extended_integral<ResultNumber> || std::same_as<ResultNumber, BigInt>)
std::vector<Point<ResultNumber, typename PointType::LabelType>>
Segment<PointType, LabelType>::latticePoints() const {
    using ResultPoint = Point<ResultNumber, typename PointType::LabelType>;
    // The walk from one lattice point to the next runs one type wider than the
    // points themselves, so the step off the last one cannot overflow.
    using Step = detail::promoted_number_t<ResultNumber>;

    std::vector<ResultPoint> points;

    const NumberType& x1 = min().x();
    const NumberType& y1 = min().y();
    const NumberType& x2 = max().x();
    const NumberType& y2 = max().y();

    // The three paths below agree on this much: the answer is `count` points
    // from `first`, each a fixed step further along the segment.
    ResultNumber firstX{}, firstY{};
    Step stepX{}, stepY{};
    std::size_t count = 0;

    if (isVertical() || isHorizontal()) {
        // One coordinate is constant and the other sweeps an interval, so the
        // lattice points are the integers of that interval -- no direction to
        // reduce, and no fraction to solve. A single point takes this path too.
        const bool vertical = isVertical();
        const NumberType& fixed = vertical ? x1 : y1;
        const NumberType& low = vertical ? y1 : x1;
        const NumberType& high = vertical ? y2 : x2;
        if (!detail::isWholeCoordinate(fixed)) {
            return points;   // the whole supporting line lies between two grid lines
        }
        const ResultNumber constant = detail::latticeFloor<ResultNumber>(fixed);
        const ResultNumber lowest = detail::latticeCeil<ResultNumber>(low);
        const ResultNumber highest = detail::latticeFloor<ResultNumber>(high);
        if (highest < lowest) {
            return points;   // the interval holds no integer
        }
        count = detail::latticeCount(Step(lowest), Step(highest));
        firstX = vertical ? constant : lowest;
        firstY = vertical ? lowest : constant;
        stepX = vertical ? Step(0) : Step(1);
        stepY = vertical ? Step(1) : Step(0);
    } else if constexpr (detail::extended_integral<NumberType> || std::same_as<NumberType, BigInt>) {
        // Integer endpoints are lattice points themselves, so the progression
        // starts at one of them: it steps by the primitive direction, which is
        // the difference divided by its own gcd, and lands on the other end.
        using Wide = detail::promoted_number_t<NumberType>;
        const Wide deltaX = Wide(x2) - Wide(x1);
        const Wide deltaY = Wide(y2) - Wide(y1);
        const Wide steps = detail::gcd(detail::abs(deltaX), detail::abs(deltaY));
        firstX = detail::latticeNarrow<ResultNumber>(x1);
        firstY = detail::latticeNarrow<ResultNumber>(y1);
        detail::requireLatticeFits<ResultNumber>(x2);   // the far end is a lattice point too
        detail::requireLatticeFits<ResultNumber>(y2);
        count = detail::latticeCount(Wide(0), steps);
        stepX = detail::narrowTo<Step>(deltaX / steps);
        stepY = detail::narrowTo<Step>(deltaY / steps);
    } else {
        // Fractional endpoints: the supporting line carries a progression of
        // lattice points -- or none at all -- and the segment holds the part of
        // it inside its own x range. Both come out of an exact integral line.
        const auto line = [&] {
            if constexpr (is_Rational_v<NumberType>) {
                return OrientedLine<PointType>(min(), max()).template integralLine<BigInt>();
            } else {
                using ExactPoint = Point<Rational<BigInt>>;
                const auto exactPoint = [](const NumberType& x, const NumberType& y) {
                    const std::array<BigInt, 2> fx = detail::exactFraction(x);
                    const std::array<BigInt, 2> fy = detail::exactFraction(y);
                    return ExactPoint(Rational<BigInt>(fx[0], fx[1]), Rational<BigInt>(fy[0], fy[1]));
                };
                return OrientedLine<ExactPoint>(exactPoint(x1, y1), exactPoint(x2, y2))
                    .template integralLine<BigInt>();
            }
        }();
        if (!line) {
            return points;   // the supporting line misses the grid entirely
        }
        const BigInt baseX = line->source().x();
        const BigInt baseY = line->source().y();
        const BigInt directionX = line->target().x() - baseX;   // > 0: the segment is not vertical
        const BigInt directionY = line->target().y() - baseY;

        // The lattice points of the line are base + t * direction, and x grows
        // with t, so the segment keeps the t whose x it spans. Both bounds are
        // exact fractions, which turns each into one rounded integer division.
        const auto boundIndex = [&](const NumberType& x, bool upwards) {
            const std::array<BigInt, 2> fraction = detail::exactFraction(x);
            const BigInt numerator = fraction[0] - baseX * fraction[1];
            const BigInt denominator = fraction[1] * directionX;   // positive
            if (upwards) {
                return numerator > BigInt(0)
                           ? (numerator + denominator - BigInt(1)) / denominator
                           : -((-numerator) / denominator);
            }
            return numerator >= BigInt(0)
                       ? numerator / denominator
                       : -((-numerator + denominator - BigInt(1)) / denominator);
        };
        const BigInt lowest = boundIndex(x1, true);
        const BigInt highest = boundIndex(x2, false);
        if (highest < lowest) {
            return points;   // the line meets the grid, but not within this segment
        }
        firstX = detail::latticeNarrow<ResultNumber>(baseX + lowest * directionX);
        firstY = detail::latticeNarrow<ResultNumber>(baseY + lowest * directionY);
        detail::requireLatticeFits<ResultNumber>(baseX + highest * directionX);
        detail::requireLatticeFits<ResultNumber>(baseY + highest * directionY);
        count = detail::latticeCount(lowest, highest);
        if (count > 1) {
            // With two points in range the step is shorter than the span, which
            // the endpoints above already fit; a single point never steps.
            stepX = detail::narrowTo<Step>(directionX);
            stepY = detail::narrowTo<Step>(directionY);
        }
    }

    points.reserve(count);
    Step x(firstX), y(firstY);
    for (std::size_t i = 0; i < count; ++i) {
        points.push_back(ResultPoint(detail::narrowTo<ResultNumber>(x),
                                     detail::narrowTo<ResultNumber>(y)));
        x += stepX;
        y += stepY;
    }
    return points;
}


// -----------------------------------------------------------------------------
// OrientedSegment

template <class PointType, class LabelType>
template <class ResultNumber>
    requires(detail::extended_integral<ResultNumber> || std::same_as<ResultNumber, BigInt>)
std::vector<Point<ResultNumber, typename PointType::LabelType>>
OrientedSegment<PointType, LabelType>::latticePoints() const {
    auto points = static_cast<Segment<PointType>>(*this).template latticePoints<ResultNumber>();
    if (target() < source()) {
        std::reverse(points.begin(), points.end());   // the segment answered the other way round
    }
    return points;
}

// -----------------------------------------------------------------------------
// MonotoneChain

template <class PointType, class LabelType, class Storage>
template <class ResultNumber>
    requires(detail::extended_integral<ResultNumber> || std::same_as<ResultNumber, BigInt>)
std::vector<Point<ResultNumber, typename PointType::LabelType>>
MonotoneChain<PointType, LabelType, Storage>::latticePoints() const {
    return detail::chainLatticePoints<Point<ResultNumber, typename PointType::LabelType>>(*this);
}

// -----------------------------------------------------------------------------
// Polyline

template <class PointType, class LabelType>
template <class ResultNumber>
    requires(detail::extended_integral<ResultNumber> || std::same_as<ResultNumber, BigInt>)
std::vector<Point<ResultNumber, typename PointType::LabelType>>
Polyline<PointType, LabelType>::latticePoints() const {
    return detail::chainLatticePoints<Point<ResultNumber, typename PointType::LabelType>>(*this);
}


// -----------------------------------------------------------------------------
// Rectangle

template <class PointType, class LabelType>
template <class ResultNumber>
    requires(detail::extended_integral<ResultNumber> || std::same_as<ResultNumber, BigInt>)
std::vector<Point<ResultNumber, typename PointType::LabelType>>
Rectangle<PointType, LabelType>::latticePoints() const {
    using ResultPoint = Point<ResultNumber, typename PointType::LabelType>;
    std::vector<ResultPoint> points;
    const ResultNumber firstX = detail::latticeCeil<ResultNumber>(min().x());
    const ResultNumber lastX = detail::latticeFloor<ResultNumber>(max().x());
    const ResultNumber firstY = detail::latticeCeil<ResultNumber>(min().y());
    const ResultNumber lastY = detail::latticeFloor<ResultNumber>(max().y());
    if (lastX < firstX || lastY < firstY) {
        return points;   // a side spans no integer, so neither does the box
    }
    // The two sides are independent: their integers are the whole answer, with
    // no direction to reduce and no crossing to sort. They are counted and
    // walked one type wider than the points, so a side that reaches the edge
    // of the coordinate range neither wraps its count nor steps past its end.
    using Step = detail::promoted_number_t<ResultNumber>;
    const std::size_t columns = detail::latticeCount(Step(firstX), Step(lastX));
    const std::size_t rows = detail::latticeCount(Step(firstY), Step(lastY));
    if (columns > std::numeric_limits<std::size_t>::max() / rows) {
        throw std::length_error("pgl::latticePoints: too many lattice points");
    }
    points.reserve(columns * rows);
    for (Step x = Step(firstX); !(Step(lastX) < x); ++x) {
        for (Step y = Step(firstY); !(Step(lastY) < y); ++y) {
            points.push_back(ResultPoint(detail::narrowTo<ResultNumber>(x),
                                         detail::narrowTo<ResultNumber>(y)));
        }
    }
    return points;
}

// -----------------------------------------------------------------------------
// Triangle

template <class PointType, class LabelType>
template <class ResultNumber>
    requires(detail::extended_integral<ResultNumber> || std::same_as<ResultNumber, BigInt>)
std::vector<Point<ResultNumber, typename PointType::LabelType>>
Triangle<PointType, LabelType>::latticePoints() const {
    return detail::regionLatticePoints<Point<ResultNumber, typename PointType::LabelType>>(edges());
}

// -----------------------------------------------------------------------------
// Disk

template <class PointType, class LabelType>
template <class ResultNumber>
    requires(detail::extended_integral<ResultNumber> || std::same_as<ResultNumber, BigInt>)
std::vector<Point<ResultNumber, typename PointType::LabelType>>
Disk<PointType, LabelType>::latticePoints() const {
    using ResultPoint = Point<ResultNumber, typename PointType::LabelType>;
    if (const std::optional<PointType> single = getIfPoint()) {
        // No circle to sweep, and no centre to derive: the point answers alone.
        return Segment<PointType>(*single, *single).template latticePoints<ResultNumber>();
    }
    std::vector<ResultPoint> points;
    const Rectangle<PointType> box = bbox();
    const ResultNumber firstX = detail::latticeCeil<ResultNumber>(box.min().x());
    const ResultNumber lastX = detail::latticeFloor<ResultNumber>(box.max().x());
    if (lastX < firstX) {
        return points;
    }
    // A column meets the disk in an interval centred on the centre's own row, so
    // when it holds a lattice point at all, one of the rows around that centre
    // is among them: that is the seed the interval grows from, and every step of
    // the growth is the disk's own exact predicate.
    const ResultNumber middle =
        detail::latticeFloor<ResultNumber>(center<division_result_t<NumberType>>().y());
    // Walked one type wider than the points, so a disk that reaches the edge
    // of the coordinate range does not step past its last column.
    using Step = detail::promoted_number_t<ResultNumber>;
    for (Step column = Step(firstX); !(Step(lastX) < column); ++column) {
        const ResultNumber x = detail::narrowTo<ResultNumber>(column);
        ResultNumber seed = middle;
        bool inside = contains(ResultPoint(x, seed));
        for (int step = -1; !inside && step <= 1; step += 2) {
            seed = middle + ResultNumber(step);
            inside = contains(ResultPoint(x, seed));
        }
        if (!inside) {
            continue;   // the column passes beside the disk, or between two rows
        }
        ResultNumber low = seed;
        ResultNumber high = seed;
        while (contains(ResultPoint(x, low - ResultNumber(1)))) {
            --low;
        }
        while (contains(ResultPoint(x, high + ResultNumber(1)))) {
            ++high;
        }
        for (ResultNumber y = low; !(high < y); ++y) {
            points.push_back(ResultPoint(x, y));
        }
    }
    return points;
}

// -----------------------------------------------------------------------------
// Convex

template <class PointType, class LabelType>
template <class ResultNumber>
    requires(detail::extended_integral<ResultNumber> || std::same_as<ResultNumber, BigInt>)
std::vector<Point<ResultNumber, typename PointType::LabelType>>
Convex<PointType, LabelType>::latticePoints() const {
    return detail::regionLatticePoints<Point<ResultNumber, typename PointType::LabelType>>(
        edgesView());
}

// -----------------------------------------------------------------------------
// Polygon

template <class PointType, class LabelType>
template <class ResultNumber>
    requires(detail::extended_integral<ResultNumber> || std::same_as<ResultNumber, BigInt>)
std::vector<Point<ResultNumber, typename PointType::LabelType>>
Polygon<PointType, LabelType>::latticePoints() const {
    return detail::regionLatticePoints<Point<ResultNumber, typename PointType::LabelType>>(
        edgesView());
}

// -----------------------------------------------------------------------------
// HalfplaneIntersection

template <class PointType, class LabelType>
template <class ResultNumber>
    requires(detail::extended_integral<ResultNumber> || std::same_as<ResultNumber, BigInt>)
std::vector<Point<ResultNumber, typename PointType::LabelType>>
HalfplaneIntersection<PointType, LabelType>::latticePoints() const {
    // The vertices are crossings of the constraints, so they need not be on the
    // grid and generally are not: the exact convex polygon they form is what the
    // sweep reads, and it refuses an unbounded region as every vertex list does.
    return asConvex<division_result_t<NumberType>>().template latticePoints<ResultNumber>();
}

// -----------------------------------------------------------------------------
// PolygonWithHoles

template <class PointType, class LabelType>
template <class ResultNumber>
    requires(detail::extended_integral<ResultNumber> || std::same_as<ResultNumber, BigInt>)
std::vector<Point<ResultNumber, typename PointType::LabelType>>
PolygonWithHoles<PointType, LabelType>::latticePoints() const {
    // A hole's ring crosses every column that runs through it exactly as the
    // outer ring does, so counting both makes the parity odd only where the
    // region is: the sweep needs no separate notion of a hole.
    std::vector<Segment<PointType>> edges;
    detail::appendRegionEdges(*this, edges);
    return detail::regionLatticePoints<Point<ResultNumber, typename PointType::LabelType>>(edges);
}

// -----------------------------------------------------------------------------
// PolygonSet

template <class PointType, class LabelType>
template <class ResultNumber>
    requires(detail::extended_integral<ResultNumber> || std::same_as<ResultNumber, BigInt>)
std::vector<Point<ResultNumber, typename PointType::LabelType>>
PolygonSet<PointType, LabelType>::latticePoints() const {
    // The components have disjoint interiors, so one sweep over all their rings
    // answers for the set: a column crosses each component an even number of
    // times, which leaves the parity of the one it is inside.
    std::vector<Segment<PointType>> edges;
    for (const auto& component : components()) {
        detail::appendRegionEdges(component, edges);
    }
    return detail::regionLatticePoints<Point<ResultNumber, typename PointType::LabelType>>(edges);
}

}  // namespace pgl
