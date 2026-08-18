#pragma once

#include "algorithm/intervaltree.hpp"

/**
 * @file xysweep.hpp
 * @brief Bounding-box sweep over pairs of segments.
 *
 * A vertical line sweeps the bounding-box abscissas of the input while an
 * @ref IntervalTree over the y-extents holds the segments the line currently
 * meets. A segment entering the sweep is queried against that active set, so
 * the only pairs ever examined are those whose bounding boxes overlap. The
 * pair-reporting algorithms built on it (@ref xyCrossings and
 * @ref xyIntersections) report what the brute-force scans report, and the
 * simplicity tests of @ref Polygon and @ref Polyline use it for the coordinate
 * types the exact sweep line cannot take.
 */

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>


namespace pgl::detail {

/**
 * @brief Segment count above which the bounding-box sweep pays for itself.
 *
 * The quadratic scan tests a pair with a couple of orientation predicates and
 * nothing else, which is several times cheaper per pair than reaching one
 * through the sweep. Measured on floating-point polygons the two meet at about
 * this many edges; below it the scan wins, above it the sweep pulls away.
 */
inline constexpr std::size_t xySweepMinSegments = 128;

/**
 * @brief Visits every pair of segments whose bounding boxes overlap.
 *
 * The visitor is called once per such pair, as `visit(entering, active)`, with
 * indices into @p segments: `entering` is the segment the sweep just reached
 * and `active` one the sweep line already meets. Pairs whose bounding boxes are
 * disjoint are never visited, and a pair of segments that intersect always has
 * overlapping bounding boxes, so a visitor looking for intersecting pairs sees
 * all of them. A visitor returning `true` stops the sweep; a `void` visitor
 * runs it to the end.
 *
 * Sorting the 2n bounding-box abscissas costs `O(n log n)` and each of the k
 * visited pairs is reached through one interval-tree report, for
 * `O((n + k) log n)` overall. In the worst case every pair of bounding boxes
 * overlaps and k is quadratic, but k is the number of pairs the caller would
 * have to test anyway.
 *
 * The bounding-box bounds are copies of the endpoint coordinates, so the filter
 * introduces no arithmetic of its own and stays exact for every coordinate
 * type, floating point included.
 *
 * @tparam Segment Segment-like type stored in @p segments.
 * @tparam Visitor Callable taking two indices and returning `bool` or `void`.
 * @param segments Segments to sweep.
 * @param visit Visitor called once per bounding-box-overlapping pair.
 * @return `true` if the visitor stopped the sweep.
 */
template <SegmentConcept Segment, class Visitor>
bool visitXYSweepPairs(const std::vector<Segment>& segments, Visitor visit) {
    using PointType = typename Segment::PointType;
    using NumberType = typename PointType::NumberType;
    using IndexedSegment = pgl::Segment<PointType, std::uint32_t>;

    const std::size_t count = segments.size();
    if (count < 2) {
        return false;  // no pair to visit
    }
    if (count > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
        throw std::length_error("xy sweep exceeds its 32-bit segment capacity");
    }

    // The index travels with the segment as its label, so a segment reported by
    // the tree names its position in the input.
    std::vector<IndexedSegment> indexed;
    indexed.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        indexed.emplace_back(segments[i].min(), segments[i].max(),
                             static_cast<std::uint32_t>(i));
    }

    struct Event {
        NumberType x;
        bool closing;
        std::uint32_t index;
    };
    std::vector<Event> events;
    events.reserve(2 * count);
    for (const IndexedSegment& segment : indexed) {
        const auto box = segment.bbox();
        events.push_back({box.min().x(), false, segment.label()});
        events.push_back({box.max().x(), true, segment.label()});
    }
    // Openings come before closings at a shared abscissa, so two boxes meeting
    // in a single vertical line are still compared, and a vertical segment is
    // queried before the same abscissa closes it.
    std::sort(events.begin(), events.end(), [](const Event& a, const Event& b) {
        if (a.x < b.x) {
            return true;
        }
        if (b.x < a.x) {
            return false;
        }
        return !a.closing && b.closing;
    });

    pgl::IntervalTree<IndexedSegment, pgl::ProjectionAxis::y> active;
    for (const Event& event : events) {
        const IndexedSegment& segment = indexed[event.index];
        if (event.closing) {
            active.erase(segment);
            continue;
        }
        // Everything the tree holds already overlaps this segment in x, so the
        // shapes it reports for the y-extent are exactly the boxes that overlap.
        const bool stopped =
            active.visitProjectionsIntersecting(segment, [&](const IndexedSegment& other) {
                return visit(static_cast<std::size_t>(event.index),
                             static_cast<std::size_t>(other.label()));
            });
        if (stopped) {
            return true;
        }
        active.insert(segment);
    }
    return false;
}

}  // namespace pgl::detail


namespace pgl {

/**
 * @brief Finds all crossing segment pairs with a bounding-box sweep.
 *
 * Reports the same pairs as @ref bruteForceCrossings, in the order the sweep
 * meets them rather than in input order, by testing only the pairs whose
 * bounding boxes overlap. Takes `O((n + k) log n)` time for `n` segments and
 * `k` bounding-box-overlapping pairs, so it is the faster of the two whenever
 * the boxes are not almost all overlapping, and it needs no exact arithmetic:
 * unlike @ref findCrossings it accepts floating-point coordinates.
 *
 * @tparam Rational Unused template parameter kept for API symmetry.
 * @tparam Container Container of segment-like values.
 * @param segments Input segment container.
 * @return Vector of crossing segment pairs.
 */
template<class Rational = pgl::Rational<pgl::BigInt>, class Container>
auto xyCrossings(const Container &segments) {
    using Point = Container::value_type::PointType;
    std::vector<pgl::Segment<Point>> v;
    for (const auto &s : segments) {
        pgl::Segment<Point> converted = s;
        v.push_back(converted);
    }

    std::vector<std::array<pgl::Segment<Point>,2>> ret;
    pgl::detail::visitXYSweepPairs(v, [&v,&ret](std::size_t i, std::size_t j) {
        pgl::Segment<Point> s1 = v[i];
        pgl::Segment<Point> s2 = v[j];
        if (s1.crosses(s2)) {
            if (s2 < s1)
                std::swap(s1,s2);
            ret.push_back({s1,s2});
        }
    });

    return ret;
}

/**
 * @brief Finds all intersecting segment pairs with a bounding-box sweep.
 *
 * Reports the same pairs as @ref bruteForceIntersections, in the order the
 * sweep meets them rather than in input order, by testing only the pairs whose
 * bounding boxes overlap. Takes `O((n + k) log n)` time for `n` segments and
 * `k` bounding-box-overlapping pairs, so it is the faster of the two whenever
 * the boxes are not almost all overlapping, and it needs no exact arithmetic:
 * unlike @ref findIntersections it accepts floating-point coordinates.
 *
 * @tparam Rational Unused template parameter kept for API symmetry.
 * @tparam Container Container of segment-like values.
 * @param segments Input segment container.
 * @return Vector of intersecting segment pairs.
 */
template<class Rational = pgl::Rational<pgl::BigInt>, class Container>
auto xyIntersections(const Container &segments) {
    using Point = Container::value_type::PointType;
    std::vector<pgl::Segment<Point>> v;
    for (const auto &s : segments) {
        pgl::Segment<Point> converted = s;
        v.push_back(converted);
    }

    std::vector<std::array<pgl::Segment<Point>,2>> ret;
    pgl::detail::visitXYSweepPairs(v, [&v,&ret](std::size_t i, std::size_t j) {
        pgl::Segment<Point> s1 = v[i];
        pgl::Segment<Point> s2 = v[j];
        if (s1.intersects(s2)) {
            if (s2 < s1)
                std::swap(s1,s2);
            ret.push_back({s1,s2});
        }
    });

    return ret;
}

/**
 * @brief Tests whether the polygon boundary is simple.
 *
 * Integer and rational coordinates go to the exact sweep line, which needs
 * them; floating-point ones go to the bounding-box sweep of
 * @ref detail::visitXYSweepPairs, which reaches the same verdict through the
 * same predicates as the pairwise scan while testing only the pairs that can
 * possibly meet. A polygon small enough for the pairwise scan to win takes it
 * directly.
 *
 * @tparam Rational Exact rational type used internally by the sweep line.
 * @return `true` if the edges only meet at the shared endpoints of consecutive
 *         edges.
 */
template <class PointType_, class LabelType>
template <class Rational>
bool Polygon<PointType_, LabelType>::isSimple() const {
    using Number = typename PointType::NumberType;
    const std::ptrdiff_t n = static_cast<std::ptrdiff_t>(size());
    if (n < 3) {
        return false;
    }

    std::vector<pgl::Segment<PointType>> edges;
    edges.reserve(static_cast<std::size_t>(n));
    for (std::ptrdiff_t i = 0; i < n; ++i) {
        pgl::Segment<PointType> edge(get(i), get(i + 1));
        if (edge.isDegenerate()) {
            return false;  // zero-length edge / repeated vertex
        }
        edges.push_back(edge);
    }

    // Exact sweep for large integer/rational polygons; the bounding-box sweep
    // for large floating-point ones, which the exact sweep cannot handle; brute
    // force for the small ones either way.
    if constexpr (!std::is_floating_point_v<Number>) {
        if (n > 8) {
            pgl::detail::BentleyOttmann<Rational, pgl::Segment<PointType>> bo;
            return bo.testPolygon(edges);
        }
    } else if (edges.size() > detail::xySweepMinSegments) {
        const std::size_t last = edges.size() - 1;
        bool notSimple = false;
        detail::visitXYSweepPairs(edges, [&edges, last, &notSimple](std::size_t a, std::size_t b) {
            // The ring closes, so the first and last edges are consecutive too.
            const bool adjacent = (a + 1 == b) || (b + 1 == a) ||
                                  (a == 0 && b == last) || (b == 0 && a == last);
            notSimple = adjacent ? edges[a].interiorsIntersect(edges[b])
                                 : edges[a].intersects(edges[b]);
            return notSimple;  // the first violation ends the sweep
        });
        return !notSimple;
    }

    for (std::ptrdiff_t i = 0; i < n; ++i) {
        for (std::ptrdiff_t j = i + 1; j < n; ++j) {
            const bool adjacent = (j == i + 1) || (i == 0 && j == n - 1);
            if (adjacent) {
                if (edges[i].interiorsIntersect(edges[j])) {
                    return false;  // consecutive edges overlap beyond the shared vertex
                }
            } else if (edges[i].intersects(edges[j])) {
                return false;  // non-adjacent edges must be disjoint
            }
        }
    }
    return true;
}

/**
 * @brief Tests whether the polyline is simple.
 *
 * Takes the same three paths as @ref Polygon::isSimple, over the chain's edges
 * rather than a ring's: the exact sweep line for integer and rational
 * coordinates, the bounding-box sweep for large floating-point ones, and the
 * pairwise scan for the small ones.
 *
 * @tparam Rational Exact rational type used internally by the sweep line.
 * @return `true` if the edges only meet at the shared endpoints of consecutive
 *         edges.
 */
template <class PointType_, class TLabel>
template <class Rational>
bool Polyline<PointType_, TLabel>::isSimple() const {
    using Number = typename PointType::NumberType;
    const std::ptrdiff_t n = static_cast<std::ptrdiff_t>(size());
    if (n < 2) {
        return true;  // no edge: vacuously simple
    }

    std::vector<pgl::Segment<PointType>> edges;
    edges.reserve(static_cast<std::size_t>(n - 1));
    for (std::ptrdiff_t i = 0; i + 1 < n; ++i) {
        pgl::Segment<PointType> edge(get(i), get(i + 1));
        if (edge.isDegenerate()) {
            return false;  // zero-length edge / repeated consecutive vertex
        }
        edges.push_back(edge);
    }

    // Exact sweep for large integer/rational polylines; the bounding-box sweep
    // for large floating-point ones, which the exact sweep cannot handle; brute
    // force for the small ones either way.
    if constexpr (!std::is_floating_point_v<Number>) {
        if (edges.size() > 8) {
            pgl::detail::BentleyOttmann<Rational, pgl::Segment<PointType>> bo;
            return bo.testPolyLine(edges);
        }
    } else if (edges.size() > detail::xySweepMinSegments) {
        bool notSimple = false;
        detail::visitXYSweepPairs(edges, [&edges, &notSimple](std::size_t a, std::size_t b) {
            // In an open chain the first and last edges are NOT adjacent, so a
            // closed polyline (first vertex equal to the last) is not simple.
            const bool adjacent = (a + 1 == b) || (b + 1 == a);
            notSimple = adjacent ? edges[a].interiorsIntersect(edges[b])
                                 : edges[a].intersects(edges[b]);
            return notSimple;  // the first violation ends the sweep
        });
        return !notSimple;
    }

    const std::ptrdiff_t m = static_cast<std::ptrdiff_t>(edges.size());
    for (std::ptrdiff_t i = 0; i < m; ++i) {
        for (std::ptrdiff_t j = i + 1; j < m; ++j) {
            // In an open chain the first and last edges are NOT adjacent, so a
            // closed polyline (first vertex equal to the last) is not simple.
            const bool adjacent = (j == i + 1);
            if (adjacent) {
                if (edges[i].interiorsIntersect(edges[j])) {
                    return false;  // consecutive edges overlap beyond the shared vertex
                }
            } else if (edges[i].intersects(edges[j])) {
                return false;  // non-adjacent edges must be disjoint
            }
        }
    }
    return true;
}

}  // namespace pgl
