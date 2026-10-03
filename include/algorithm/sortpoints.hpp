#pragma once

#include "algorithm/redbluesweep.hpp"

/**
 * @file sortpoints.hpp
 * @brief Reorderings of a list of points: lexicographic, angular, spatial.
 *
 * Algorithm headers sit above the shape API and express reusable geometry
 * procedures in terms of the public primitives.
 */

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <vector>


namespace pgl {

/**
 * @brief Sorts points in place, lexicographically by `(x, y)`.
 *
 * Points sharing both coordinates are interchangeable to this order, and which
 * of them ends up first is unspecified -- it may differ between coordinate
 * types, and labels do not break the tie.
 *
 * An integral coordinate is ordered by its bits rather than by comparisons: a
 * radix sort on `y` and then a stable one on `x` leaves the lexicographic
 * order, in a number of linear passes fixed by the width of the coordinate.
 * Over a few hundred points and up this is several times faster than comparing
 * them, and it is what makes @ref convexHull's sort a fifth of its cost rather
 * than most of it. Every other coordinate type is compared: an exact
 * rational's order is not a function of its representation.
 *
 * @tparam Number Coordinate type of the points being sorted.
 * @tparam Label Label type of the points being sorted.
 * @param points Points to reorder in place.
 */
template <class Number, class Label>
void sortPoints(std::vector<Point<Number, Label>>& points) {
    using PointType = Point<Number, Label>;
    // Below this many points the radix passes cost more than they save: they
    // open with a histogram sweep and a second buffer that a short list never
    // earns back. Measured against a comparison sort that is not handed the
    // same input twice -- repeat a small sort and the branch predictor learns
    // it outright, which flatters the comparisons several times over and puts
    // this threshold an order of magnitude too high.
    constexpr std::size_t radixThreshold = 256;
    if constexpr (detail::RadixSortable<PointType, Number>) {
        if (points.size() >= radixThreshold) {
            std::vector<PointType> scratch;
            detail::radixSort(points, scratch,
                              [](const PointType& q) { return detail::radixKey(q.y()); });
            detail::radixSort(points, scratch,
                              [](const PointType& q) { return detail::radixKey(q.x()); });
            return;
        }
    }
    std::sort(points.begin(), points.end());
}

/**
 * @brief Sorts points in place lexicographically and drops the duplicates.
 *
 * As @ref sortPoints, then erasing every point whose coordinates repeat the one
 * before it, so the survivors are distinct. Which of a run of coincident points
 * survives is unspecified, and with it which label survives.
 *
 * @tparam Number Coordinate type of the points being sorted.
 * @tparam Label Label type of the points being sorted.
 * @param points Points to reorder and deduplicate in place.
 */
template <class Number, class Label>
void sortDistinctPoints(std::vector<Point<Number, Label>>& points) {
    sortPoints(points);
    points.erase(std::unique(points.begin(), points.end()), points.end());
}

/**
 * @brief Sorts points counterclockwise around a center point.
 *
 * The angular order starts from the lexicographically smallest point in
 * @p points (its direction from @p p defines angle zero) and proceeds
 * counterclockwise around @p p. Points that share an angular direction are
 * tied; the tie is broken by putting the points that are farther from @p p
 * first. When @p p lies strictly inside the convex hull of @p points,
 * connecting them in that order traces a simple, star-shaped polygon whose
 * kernel contains @p p. That hypothesis is exactly what keeps every gap
 * between consecutive directions under half a turn, so each edge stays within
 * its own wedge and each ray from @p p meets the ring once; with @p p outside
 * the hull no order at all can satisfy the conclusion, since a polygon lies
 * inside the convex hull of its own vertices. On the hull boundary the largest
 * gap is a half turn exactly, and the ring closes along the supporting line —
 * simple unless a third point sits on that line between the two it joins.
 * Points equal to @p p have no direction to sort by and end up last.
 *
 * The comparison relies only on the exact @ref orientationSign predicate and
 * squared distances, so it stays exact for integer coordinates.
 *
 * The points are first split by the horizontal line through @p p, which costs
 * two coordinate comparisons each. Each part then spans half a turn, where the
 * orientation sign alone already orders the directions, so the sort spends one
 * orientation predicate per comparison instead of the three that comparing
 * angles measured from the reference direction would take.
 *
 * @tparam Number Coordinate type of the points being sorted.
 * @tparam Label Label type of the points being sorted.
 * @tparam CenterNumber Coordinate type of the center point.
 * @tparam CenterLabel Label type of the center point.
 * @param points Points to reorder in place.
 * @param p Center the points are sorted around.
 */
template <class Number, class Label, class CenterNumber, class CenterLabel>
void sortAround(std::vector<Point<Number, Label>>& points,
                const Point<CenterNumber, CenterLabel>& p) {
    if (points.size() < 2)
        return;

    // A point equal to p has no direction around it, and would compare tied
    // with every other point. Park those at the end and sort the rest.
    const auto first = points.begin();
    const auto last = std::partition(first, points.end(),
                                     [&p](const auto& q) { return q != p; });
    if (last - first < 2)
        return;

    // The smallest point defines the reference direction (angle zero).
    const auto reference = *std::min_element(first, last);

    // Splits off the directions in the half turn [0, pi) measured
    // counterclockwise from the +x direction, that is the points above the
    // horizontal line through p, ties on it broken by the side of p they fall
    // on. The coordinates are promoted the way the orientation predicate
    // promotes them, so the split and the sort agree on every direction.
    using Compare = std::common_type_t<Number, CenterNumber>;
    const auto firstHalf = [&p](const auto& q) {
        const auto vertical = detail::strongOrder(detail::asNumber<Compare>(q.y()),
                                                  detail::asNumber<Compare>(p.y()));
        if (vertical != 0)
            return vertical > 0;
        return detail::strongOrder(detail::asNumber<Compare>(q.x()),
                                   detail::asNumber<Compare>(p.x())) > 0;
    };
    const auto middle = std::partition(first, last, firstHalf);

    // The center is an operand of every orientation sign the sort takes —
    // O(n log n) of them — so its coordinates are converted for the filter once
    // here rather than once per comparison. The two points being compared are
    // different ones each time and are converted as they come.
    using SignCoordinate = detail::orientation_coordinate_t<CenterNumber, Number, Number>;
    const auto center = detail::filtered<SignCoordinate>(p);

    // Neither part spans more than half a turn, so within one of them the
    // orientation sign is a consistent order on the directions.
    const auto less = [&p, &center](const auto& a, const auto& b) {
        const auto turn = detail::orientationSignOf(
            center, detail::filtered<SignCoordinate>(a), detail::filtered<SignCoordinate>(b))
            .value();
        if (turn > 0)
            return true;   // b is counterclockwise of a, so a has the smaller angle.
        if (turn < 0)
            return false;

        // Same direction from p: the farther point comes first. The distance
        // is taken in the promoted type, so points carrying more precision
        // than the center still compare exactly.
        return p.template squaredDistance<Compare>(a) >
               p.template squaredDistance<Compare>(b);
    };
    std::sort(first, middle, less);
    std::sort(middle, last, less);

    // The points now run counterclockwise from the +x direction: rotate the
    // block sharing the reference direction to the front. The search compares
    // directions alone, so it lands on the first point of that block rather
    // than on the reference itself, which the distance tie-break may have put
    // behind others pointing the same way.
    const auto above = firstHalf(reference);
    const auto start = std::lower_bound(
        above ? first : middle, above ? middle : last, reference,
        [&center](const auto& a, const auto& b) {
            return detail::orientationSignOf(center,
                                             detail::filtered<SignCoordinate>(a),
                                             detail::filtered<SignCoordinate>(b))
                       .value() > 0;
        });
    std::rotate(first, start, last);
}

namespace detail {

// Recursive median Hilbert sort (CGAL's median policy): split the range into the
// four quadrants the Hilbert curve visits, using nested medians, then recurse
// into each quadrant with the rotated/reflected curve state. `xAxis` selects the
// primary split axis; `upX`/`upY` give the curve's direction along each axis.
// Only the two coordinate comparators are used, so it is exact for any numeric
// type and never constructs intermediate coordinates.
template <class RandomIt, class LessX, class LessY>
void hilbertSortMedian(RandomIt begin, RandomIt end, bool xAxis, bool upX, bool upY,
                       const LessX& lessX, const LessY& lessY) {
    if (end - begin <= 1) {
        return;
    }

    const RandomIt m0 = begin, m4 = end;
    const RandomIt m2 = m0 + (m4 - m0) / 2;  // primary-axis median of the whole range
    const RandomIt m1 = m0 + (m2 - m0) / 2;  // secondary-axis median of the lower half
    const RandomIt m3 = m2 + (m4 - m2) / 2;  // secondary-axis median of the upper half

    // Rearranges [first,last) so *mid is its median under `less`, ascending when
    // `up`, descending otherwise.
    const auto split = [](RandomIt first, RandomIt mid, RandomIt last, const auto& less,
                          bool up) {
        if (up) {
            std::nth_element(first, mid, last, less);
        } else {
            std::nth_element(first, mid, last,
                             [&less](const auto& a, const auto& b) { return less(b, a); });
        }
    };

    if (xAxis) {
        split(m0, m2, m4, lessX, upX);
        split(m0, m1, m2, lessY, upY);
        split(m2, m3, m4, lessY, !upY);
    } else {
        split(m0, m2, m4, lessY, upY);
        split(m0, m1, m2, lessX, upX);
        split(m2, m3, m4, lessX, !upX);
    }

    hilbertSortMedian(m0, m1, !xAxis, upY, upX, lessX, lessY);
    hilbertSortMedian(m1, m2, xAxis, upX, upY, lessX, lessY);
    hilbertSortMedian(m2, m3, xAxis, upX, upY, lessX, lessY);
    hilbertSortMedian(m3, m4, !xAxis, !upY, !upX, lessX, lessY);
}

// Spreads the low 16 bits of x to the even bit positions.
constexpr std::uint32_t spreadBits16(std::uint32_t x) {
    x = (x | (x << 8)) & 0x00FF00FFu;
    x = (x | (x << 4)) & 0x0F0F0F0Fu;
    x = (x | (x << 2)) & 0x33333333u;
    x = (x | (x << 1)) & 0x55555555u;
    return x;
}

// Position of cell (x, y) along the Hilbert curve through a 2^16 x 2^16 grid,
// computed branch-free as a parallel prefix over the bit pairs.
constexpr std::uint32_t hilbertIndex16(std::uint32_t x, std::uint32_t y) {
    std::uint32_t A, B, C, D;
    {
        const std::uint32_t a = x ^ y, b = 0xFFFFu ^ a, c = 0xFFFFu ^ (x | y), d = x & (y ^ 0xFFFFu);
        A = a | (b >> 1);
        B = (a >> 1) ^ a;
        C = ((c >> 1) ^ (b & (d >> 1))) ^ c;
        D = ((a & (c >> 1)) ^ (d >> 1)) ^ d;
    }
    for (int shift : {2, 4}) {
        const std::uint32_t a = A, b = B, c = C, d = D;
        A = (a & (a >> shift)) ^ (b & (b >> shift));
        B = (a & (b >> shift)) ^ (b & ((a ^ b) >> shift));
        C ^= (a & (c >> shift)) ^ (b & (d >> shift));
        D ^= (b & (c >> shift)) ^ ((a ^ b) & (d >> shift));
    }
    {
        const std::uint32_t a = A, b = B, c = C, d = D;
        C ^= (a & (c >> 8)) ^ (b & (d >> 8));
        D ^= (b & (c >> 8)) ^ ((a ^ b) & (d >> 8));
    }
    const std::uint32_t a = C ^ (C >> 1), b = D ^ (D >> 1);
    const std::uint32_t i0 = x ^ y, i1 = b | (0xFFFFu ^ (i0 | a));
    return (spreadBits16(i1) << 1) | spreadBits16(i0);
}

// Reorders order[begin, end) — indices into xy — along the Hilbert curve drawn
// over their bounding box at 2^16 cells a side. A run of indices that falls in
// one cell is drawn again over its own box, so clustered input keeps its
// locality however far apart the clusters lie; a run too short to be worth
// that, or whose box is a single point, is put in (x, y, exact point, index)
// order instead, which leaves equal points next to each other with the
// earliest first.
template <class PointType>
void hilbertKeyOrderRange(const std::vector<PointType>& points,
                          const std::vector<std::array<double, 2>>& xy,
                          std::vector<std::uint32_t>& order, std::size_t begin, std::size_t end,
                          std::vector<std::uint64_t>& records, std::vector<std::uint64_t>& scratch) {
    constexpr std::size_t leaf = 32;
    const auto leafOrder = [&](std::size_t from, std::size_t to) {
        std::sort(order.begin() + static_cast<std::ptrdiff_t>(from),
                  order.begin() + static_cast<std::ptrdiff_t>(to),
                  [&](std::uint32_t i, std::uint32_t j) {
                      if (xy[i][0] != xy[j][0]) return xy[i][0] < xy[j][0];
                      if (xy[i][1] != xy[j][1]) return xy[i][1] < xy[j][1];
                      if (const auto c = points[i] <=> points[j]; c != 0) return c < 0;
                      return i < j;
                  });
    };
    double minX = std::numeric_limits<double>::infinity(), minY = minX;
    double maxX = -minX, maxY = -minX;
    for (std::size_t k = begin; k < end; ++k) {
        const auto& p = xy[order[k]];
        if (std::isfinite(p[0]) && std::isfinite(p[1])) {
            minX = std::min(minX, p[0]);
            maxX = std::max(maxX, p[0]);
            minY = std::min(minY, p[1]);
            maxY = std::max(maxY, p[1]);
        }
    }
    const double span = std::max(maxX - minX, maxY - minY);
    if (end - begin <= leaf || !(span > 0) || !std::isfinite(span)) {
        leafOrder(begin, end);
        return;
    }
    const double scale = 65535.0 / span;
    const auto cell = [scale](double v, double low) -> std::uint32_t {
        const double q = (v - low) * scale;
        return q >= 0 ? (q < 65535.0 ? static_cast<std::uint32_t>(q) : 65535u) : 0u;
    };
    records.resize(end - begin);
    for (std::size_t k = begin; k < end; ++k) {
        const std::uint32_t i = order[k];
        records[k - begin] =
            (std::uint64_t{hilbertIndex16(cell(xy[i][0], minX), cell(xy[i][1], minY))} << 32) | i;
    }
    // Stable LSD radix sort on the key, eleven bits a pass.
    scratch.resize(records.size());
    for (int shift = 32; shift < 64; shift += 11) {
        std::array<std::size_t, 2049> count{};
        for (const std::uint64_t r : records) {
            ++count[((r >> shift) & 2047u) + 1];
        }
        for (std::size_t d = 0; d < 2048; ++d) {
            count[d + 1] += count[d];
        }
        for (const std::uint64_t r : records) {
            scratch[count[(r >> shift) & 2047u]++] = r;
        }
        records.swap(scratch);
    }
    std::vector<std::pair<std::size_t, std::size_t>> runs;
    for (std::size_t k = 0; k < records.size();) {
        std::size_t j = k + 1;
        while (j < records.size() && (records[j] >> 32) == (records[k] >> 32)) {
            ++j;
        }
        if (j - k > 1) {
            runs.emplace_back(begin + k, begin + j);
        }
        k = j;
    }
    for (std::size_t k = 0; k < records.size(); ++k) {
        order[begin + k] = static_cast<std::uint32_t>(records[k]);
    }
    for (const auto& [from, to] : runs) {
        hilbertKeyOrderRange(points, xy, order, from, to, records, scratch);
    }
}

// The indices of points in an order along a Hilbert curve, with every set of
// equal points consecutive and in index order. Unlike hilbertSort this reads
// the coordinates as doubles, which is a heuristic only: the order serves
// locality, never correctness, and its cost is a few linear passes.
template <class PointType>
std::vector<std::uint32_t> hilbertKeyOrder(const std::vector<PointType>& points) {
    std::vector<std::array<double, 2>> xy(points.size());
    for (std::size_t i = 0; i < points.size(); ++i) {
        xy[i] = {approximate(points[i].x()).value, approximate(points[i].y()).value};
    }
    std::vector<std::uint32_t> order(points.size());
    for (std::size_t i = 0; i < order.size(); ++i) {
        order[i] = static_cast<std::uint32_t>(i);
    }
    std::vector<std::uint64_t> records, scratch;
    hilbertKeyOrderRange(points, xy, order, 0, order.size(), records, scratch);
    return order;
}

}  // namespace detail

/**
 * @brief Sorts points along a Hilbert space-filling curve.
 *
 * Reorders @p points in place so that points close together in the plane are
 * close together in the sequence, which makes the order a useful insertion
 * order for incremental algorithms that locate each point by walking.
 *
 * The order is produced by the median policy: the set is recursively split into
 * four quadrants by nested medians, following the Hilbert curve's recursive
 * structure. Only coordinate comparisons are used, so the result is exact for
 * integer coordinates and well defined for any numeric type.
 *
 * @tparam Number Coordinate type of the points.
 * @tparam Label Label type of the points.
 * @param points Points to reorder in place.
 */
template <class Number, class Label>
void hilbertSort(std::vector<Point<Number, Label>>& points) {
    const auto lessX = [](const Point<Number, Label>& a, const Point<Number, Label>& b) {
        return a.x() < b.x();
    };
    const auto lessY = [](const Point<Number, Label>& a, const Point<Number, Label>& b) {
        return a.y() < b.y();
    };
    detail::hilbertSortMedian(points.begin(), points.end(), true, false, false, lessX, lessY);
}

}  // namespace pgl
