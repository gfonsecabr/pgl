#pragma once

#include "algorithm/mindisk.hpp"

/**
 * @file closestpair.hpp
 * @brief Closest pair of points by divide and conquer.
 *
 * Algorithm headers sit above the shape API and express reusable geometry
 * procedures in terms of the public primitives.
 */

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <iterator>
#include <type_traits>
#include <utility>
#include <vector>


namespace pgl {

namespace detail {

template <class Container>
using closest_pair_input_point_t =
    std::remove_cvref_t<decltype(*std::begin(std::declval<const Container&>()))>;

/**
 * @brief Coordinate type the closest-pair comparisons are evaluated in.
 *
 * One promotion above the coordinate type, exactly like the sign predicates:
 * squared distances are degree two in the coordinates, so the promoted type
 * keeps the comparisons exact for integer input.
 */
template <class PointType>
using closest_pair_coordinate_t = promoted_number_t<typename PointType::NumberType>;

template <class PointType>
using closest_pair_distance_t =
    decltype(std::declval<const PointType&>()
                 .template squaredDistance<closest_pair_coordinate_t<PointType>>(
                     std::declval<const PointType&>()));

/** @brief A candidate closest pair and the squared distance between its points. */
template <class PointType>
struct ClosestPairCandidate {
    PointType first;
    PointType second;
    closest_pair_distance_t<PointType> squaredDistance;
};

/**
 * @brief Range size at or below which brute force beats splitting again.
 *
 * Brute force costs `k(k-1)/2` squared distances on a range of `k` points, while
 * recursing costs a strip sort and a strip scan at every node below. Which one
 * wins at a given `k` turns on one ratio: how dear the coordinate type makes a
 * squared distance *relative to* a comparison, since the recursion's own work is
 * almost entirely comparisons and moves. The dearer the multiplication, the
 * sooner the quadratic term bites and the lower the crossover — which is why
 * this is a per-coordinate-type constant and not one number.
 *
 * The values are measured by `sandbox/closestpair_threshold.cpp`, which sweeps
 * the threshold over whole closest-pair runs, interleaving the candidates and
 * taking medians. Note that the parameter is the type the arithmetic happens in
 * — @ref closest_pair_coordinate_t, one promotion above the coordinate — so that
 * a coordinate whose products land in a dearer type is charged for it.
 *
 *   - **Primitive coordinates: 6.** A comparison here is a single instruction
 *     and a strip is a handful of points, so a node costs almost nothing and
 *     brute force runs out of advantage almost immediately.
 *   - **Rational over a primitive: 12.** The highest of the four, because every
 *     comparison in the strip sort is a cross-multiplication: the recursion's
 *     own work is dearer here relative to a squared distance than it is for any
 *     other family, so brute force stays competitive longest.
 *   - **BigInt and Rational over BigInt: 4.** A squared distance allocates,
 *     several times over, and a Rational one also runs a gcd — while the
 *     comparisons the recursion lives on allocate nothing. Brute force is beaten
 *     at once. Both curves are flat to within 1% anywhere in 3-6, so for these
 *     types the cutoff buys essentially nothing and only needs to stay small.
 *
 * These are much lower than they would be for a recursion that filtered its
 * whole range to find each strip: with @ref closestPairGatherStrip walking out
 * from the split index instead, a node costs what its strip costs, and there is
 * far less per-node work left for brute force to displace.
 *
 * Specialize this for a coordinate type whose cost ratio differs from its
 * family. Any value `>= 3` is correct; only the speed changes.
 */
template <class Coordinate>
inline constexpr std::size_t closestPairBruteForceThreshold =
    arbitraryPrecision<Coordinate> ? 4 : (is_Rational_v<Coordinate> ? 12 : 6);

/** @brief Orders points bottom to top, ties broken left to right. */
template <class PointType>
constexpr bool closestPairLessY(const PointType& a, const PointType& b) {
    if (a.y() < b.y())
        return true;
    if (b.y() < a.y())
        return false;
    return a.x() < b.x();
}

/**
 * @brief Improves @p best with every pair of a range small enough to settle so.
 *
 * @p best is the running answer over everything examined so far, not just this
 * range, so it only ever moves down.
 */
template <class PointType>
void closestPairBaseCase(const PointType* points, std::size_t count,
                         ClosestPairCandidate<PointType>& best) {
    using Coordinate = closest_pair_coordinate_t<PointType>;
    for (std::size_t i = 0; i < count; ++i) {
        for (std::size_t j = i + 1; j < count; ++j) {
            const auto squared = points[i].template squaredDistance<Coordinate>(points[j]);
            if (squared < best.squaredDistance) {
                best = {points[i], points[j], squared};
            }
        }
    }
}

/**
 * @brief The run `[low, high)` of a range holding the points within `best` of
 *        the split line.
 *
 * A pair closer than the best so far has both of its points within that
 * distance of the split line, so only this strip can still improve on it.
 *
 * The range is in x-order and stays that way, so the strip is a *contiguous* run
 * of it around the split index: this walks outwards from that index and stops on
 * each side at the first point already outside, rather than testing all @p count
 * points. The node therefore costs what its strip costs and not what its range
 * costs — which matters most exactly where it is cheapest, since a node whose
 * strip is a handful of points now does a handful of work regardless of how many
 * points it spans.
 *
 * @param points Range to gather from, sorted lexicographically (x, then y).
 * @param count Number of points in the range.
 * @param half Index the range was split at; `points[half].x()` is @p splitX.
 * @param splitX Abscissa of the split line.
 * @param best Running best pair, whose distance is the strip's half-width.
 * @return The bounds of the strip, as indices into @p points.
 */
template <class PointType>
std::pair<std::size_t, std::size_t> closestPairGatherStrip(
    const PointType* points, std::size_t count, std::size_t half,
    closest_pair_coordinate_t<PointType> splitX, const ClosestPairCandidate<PointType>& best) {
    using Coordinate = closest_pair_coordinate_t<PointType>;

    // Squared throughout, so the half-width is never rooted and the test stays
    // exact: |x - splitX| < d becomes (x - splitX)^2 < d^2. A best of zero
    // admits nothing, which is right — no pair can beat a coincident one.
    const auto inside = [&](std::size_t index) {
        const Coordinate dx = detail::asNumber<Coordinate>(points[index].x()) - splitX;
        return dx * dx < best.squaredDistance;
    };

    std::size_t low = half;
    while (low > 0 && inside(low - 1)) {
        --low;
    }
    std::size_t high = half;
    while (high < count && inside(high)) {
        ++high;
    }

    return {low, high};
}

/**
 * @brief The rank of every input point in the y-order of the whole input,
 *        computed the first time a strip needs it.
 *
 * The order is the one @ref closestPairLessY gives, its ties among coincident
 * points broken by the x-order the input is kept in, so every rank is distinct.
 */
template <class PointType>
struct ClosestPairRanks {
    const PointType* points;  ///< The whole input, sorted lexicographically.
    std::size_t count;        ///< Number of points in the whole input.
    std::vector<std::size_t> rankOf;  ///< Rank of each point; empty until used.
    std::vector<std::size_t> order;   ///< Index of the point of each rank.
    std::vector<std::size_t> keys;    ///< Scratch: the ranks of one strip.
    std::vector<std::size_t> buffer;  ///< Scratch for the radix passes.

    /** @brief Computes the ranks unless an earlier strip already did. */
    void prepare() {
        if (!rankOf.empty()) {
            return;
        }
        order.resize(count);
        for (std::size_t i = 0; i < count; ++i) {
            order[i] = i;
        }
        // The points are in (x, y) order, so a stable sort by ordinate alone
        // breaks its ties by abscissa and then by position.
        using Number = typename PointType::NumberType;
        bool ordered = false;
        if constexpr (RadixSortable<std::size_t, Number>) {
            constexpr std::size_t radixThreshold = 256;
            if (count >= radixThreshold) {
                const PointType* all = points;
                radixSort(order, buffer, [all](std::size_t i) { return radixKey(all[i].y()); });
                ordered = true;
            }
        }
        if (!ordered) {
            std::sort(order.begin(), order.end(), [this](std::size_t a, std::size_t b) {
                if (points[a].y() < points[b].y()) {
                    return true;
                }
                if (points[b].y() < points[a].y()) {
                    return false;
                }
                return a < b;
            });
        }
        rankOf.resize(count);
        for (std::size_t rank = 0; rank < count; ++rank) {
            rankOf[order[rank]] = rank;
        }
    }
};

/**
 * @brief Strips longer than this are ordered by rank rather than compared.
 *
 * Comparing a strip of `s <= 256` points into order costs `O(s log 256)`
 * comparisons, so either way a strip costs `O(s)`.
 */
inline constexpr std::size_t closestPairComparedStripLimit = 256;

/**
 * @brief Copies `points[low, high)` into @p strip in y-order.
 *
 * A short strip is sorted by @ref closestPairLessY. A long one is radix sorted by
 * the ranks of @p ranks, whose word-sized keys take a fixed number of linear
 * passes, so a strip of `s` points costs `O(s)` whatever its length, plus the
 * `O(n log n)` of computing the ranks the first time.
 *
 * @return The number of points written, `high - low`.
 */
template <class PointType>
std::size_t closestPairOrderStrip(const PointType* points, std::size_t low, std::size_t high,
                                  ClosestPairRanks<PointType>& ranks, PointType* strip) {
    const std::size_t stripCount = high - low;
    if (stripCount <= closestPairComparedStripLimit) {
        std::copy(points + low, points + high, strip);
        std::sort(strip, strip + stripCount, closestPairLessY<PointType>);
        return stripCount;
    }
    ranks.prepare();
    const auto base = static_cast<std::size_t>(points - ranks.points);
    ranks.keys.assign(ranks.rankOf.begin() + static_cast<std::ptrdiff_t>(base + low),
                      ranks.rankOf.begin() + static_cast<std::ptrdiff_t>(base + high));
    radixSort(ranks.keys, ranks.buffer, [](std::size_t rank) { return rank; });
    for (std::size_t k = 0; k < stripCount; ++k) {
        strip[k] = ranks.points[ranks.order[ranks.keys[k]]];
    }
    return stripCount;
}

/**
 * @brief Improves @p best with the closest pair found inside a y-sorted strip.
 *
 * Inside the strip every point of a half is at least `best` away from the other
 * points of that half. A ball of that radius therefore holds a bounded number
 * of strip points, so the inner loop stops after a constant number of steps.
 */
template <class PointType>
void closestPairScanStrip(const PointType* strip, std::size_t stripCount,
                          ClosestPairCandidate<PointType>& best) {
    using Coordinate = closest_pair_coordinate_t<PointType>;
    for (std::size_t i = 0; i < stripCount; ++i) {
        for (std::size_t j = i + 1; j < stripCount; ++j) {
            const Coordinate dy =
                detail::asNumber<Coordinate>(strip[j].y()) - detail::asNumber<Coordinate>(strip[i].y());
            if (dy * dy >= best.squaredDistance) {
                break;
            }
            const auto squared = strip[i].template squaredDistance<Coordinate>(strip[j]);
            if (squared < best.squaredDistance) {
                best = {strip[i], strip[j], squared};
            }
        }
    }
}

/**
 * @brief Improves @p best with the closest pair within `points[0, count)`.
 *
 * The range is sorted by x on entry and keeps that order throughout: the only
 * thing ever put in y-order is the strip around the split line, right before it
 * is scanned, by @ref closestPairOrderStrip in time linear in the strip. A node
 * therefore costs what its strip costs, and the recursion `O(n log n)` however
 * much of a range its strips hold. The textbook alternative carries a y-order
 * up the recursion instead, merging the two orders its children left behind;
 * it has the same bound but pays for the whole range at every node, which on
 * ordinary inputs, where strips are a handful of points, costs several times
 * more. Keeping the x-order is also what lets @ref closestPairGatherStrip find
 * the strip by walking out from the split index instead of filtering the range.
 *
 * @p best is the running answer over everything examined so far, anywhere in the
 * input — not the answer for this range. Threading it down rather than combining
 * two subtree answers on the way up means the left half's discovery narrows the
 * right half's strips, and every strip below is measured against the best the
 * whole traversal has found rather than against its own subtree's best. The
 * result is unchanged and the strips are never wider.
 *
 * @param points Range to search, sorted lexicographically (x, then y). Taken by
 *        const pointer on purpose: the abscissa order is a precondition of every
 *        level below, so nothing here may reorder the range, and the type says
 *        so rather than leaving it to be noticed. All the reordering happens in
 *        @p scratch. (The merging formulation of this algorithm does the
 *        opposite — it leaves each range sorted by ordinate for its parent to
 *        merge — which is precisely why it cannot find its strip by walking out
 *        from the split index.)
 * @param count Number of points in the range; at least two.
 * @param scratch Uninitialized-or-stale buffer of at least @p count points, not
 *        overlapping @p points.
 * @param ranks The y-order ranks of the whole input that @p points is part of.
 * @param best Running best pair, improved in place.
 * @tparam Threshold Range size to stop recursing at; see
 *         @ref closestPairBruteForceThreshold. At least 3, so that a range that
 *         is still split has halves of at least two points. A template parameter
 *         rather than an argument: it is a constant of the coordinate type, so
 *         the base-case test folds against an immediate instead of threading a
 *         register down every call.
 */
template <std::size_t Threshold, class PointType>
void closestPairRecursive(const PointType* points, std::size_t count, PointType* scratch,
                          ClosestPairRanks<PointType>& ranks,
                          ClosestPairCandidate<PointType>& best) {
    using Coordinate = closest_pair_coordinate_t<PointType>;
    static_assert(Threshold >= 3,
                  "a range above the threshold must split into halves of at least two points");
    assert(count >= 2);

    // Nothing above needs a y-order, so brute force leaves the range untouched.
    if (count <= Threshold) {
        closestPairBaseCase(points, count, best);
        return;
    }

    // The vertical line through points[half] separates the halves: everything
    // left of it has a smaller-or-equal abscissa, everything right of it a
    // greater-or-equal one.
    const std::size_t half = count / 2;
    const Coordinate splitX = static_cast<Coordinate>(points[half].x());

    closestPairRecursive<Threshold>(points, half, scratch, ranks, best);
    closestPairRecursive<Threshold>(points + half, count - half, scratch + half, ranks, best);

    // The range is in x-order, so the strip is found in x-order too and has to
    // be put in y-order before it can be scanned.
    const auto [low, high] = closestPairGatherStrip(points, count, half, splitX, best);
    const std::size_t stripCount = closestPairOrderStrip(points, low, high, ranks, scratch);
    closestPairScanStrip(scratch, stripCount, best);
}

/**
 * @brief Copies the input, sorts it by abscissa, and runs the recursion on it.
 *
 * The running best starts as a real pair — the first two points of the sorted
 * copy — so the very first strip is already measured against something finite.
 */
template <std::size_t Threshold, class Container>
[[nodiscard]] Segment<closest_pair_input_point_t<Container>>
closestPairDriver(const Container& input) {
    using InputPoint = closest_pair_input_point_t<Container>;
    using Coordinate = closest_pair_coordinate_t<InputPoint>;
    static_assert(PointConcept<InputPoint>,
                  "closestPair requires a container of pgl::Point values");

    std::vector<InputPoint> points(std::begin(input), std::end(input));
    assert(points.size() >= 2);

    sortPoints(points);
    std::vector<InputPoint> scratch(points.size());

    ClosestPairCandidate<InputPoint> best{
        points[0], points[1],
        points[0].template squaredDistance<Coordinate>(points[1])};
    ClosestPairRanks<InputPoint> ranks{points.data(), points.size(), {}, {}, {}, {}};
    closestPairRecursive<Threshold>(points.data(), points.size(), scratch.data(), ranks, best);

    return Segment<InputPoint>(best.first, best.second);
}

}  // namespace detail

/**
 * @brief Computes a closest pair of points by divide and conquer.
 *
 * The points are sorted by abscissa and split in half by a vertical line. Each
 * half is solved recursively, and only the points lying within the better of the
 * two half-solutions of the split line can still form a closer pair; putting
 * that strip in ordinate order and scanning it settles them in time linear in
 * the strip.
 *
 * The recursion stops and tries every pair once a range gets small enough that
 * brute force is the cheaper of the two; where that is depends on the coordinate
 * type, see @ref detail::closestPairBruteForceThreshold.
 *
 * Only squared distances are compared, in the promoted coordinate type, so the
 * result is exact for integer coordinates. Ties are broken arbitrarily. The
 * returned segment keeps the input point type, labels included, and orders its
 * endpoints like any other @ref Segment.
 *
 * @tparam Container Container of pgl points.
 * @param input Input points; they are copied and the container is not modified.
 * @pre @p input holds at least two points; fewer is undefined behavior.
 * @return Segment joining two points at minimum distance from each other.
 *
 * @complexity O(n log n) time. O(n) additional space.
 */
template <class Container>
[[nodiscard]] Segment<detail::closest_pair_input_point_t<Container>>
closestPair(const Container& input) {
    using Coordinate =
        detail::closest_pair_coordinate_t<detail::closest_pair_input_point_t<Container>>;
    return detail::closestPairDriver<detail::closestPairBruteForceThreshold<Coordinate>>(input);
}

}  // namespace pgl
