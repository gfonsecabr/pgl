#pragma once

#include "implementation/duality.hpp"

#include <algorithm>
#include <cstddef>
#include <ranges>
#include <set>
#include <utility>
#include <vector>

/**
 * @file predicates_helpers.hpp
 * @brief Small dispatch traits and geometry helpers reused by the implementations.
 */


namespace pgl {

namespace detail {

/**
 * @brief Tests the vertices of a convex polygon that decide its containment in a triangle.
 *
 * A triangle contains (or interior-contains) a convex polygon iff it does so
 * for every vertex. For a non-degenerate, counterclockwise triangle every vertex
 * is on the inner side of an edge iff the vertex deepest on its outer side is,
 * and that vertex maximizes a linear functional over the polygon, so the
 * cyclic extreme-vertex search finds it. Only those three vertices go through
 * @p vertexTest, which keeps the answer identical to testing all of them. A
 * polygon of at most three vertices, or a degenerate triangle, has at most
 * three vertices tested directly: a valid polygon with three or more vertices is
 * not collinear, so it never fits in the segment a degenerate triangle is.
 *
 * Complexity: O(log n) for a polygon of n vertices, plus the cost of three
 * @p vertexTest calls.
 */
template <class TriangleType, class ConvexType, class VertexTest>
constexpr bool triangleContainsConvexVertices(
    const TriangleType& triangle, const ConvexType& polygon, VertexTest vertexTest) {
    const std::ptrdiff_t n = static_cast<std::ptrdiff_t>(polygon.size());
    if (n <= 3 || triangle.isDegenerate()) {
        for (std::ptrdiff_t k = 0; k < std::min<std::ptrdiff_t>(n, 3); ++k) {
            if (!vertexTest(polygon[k])) {
                return false;
            }
        }
        return true;
    }
    const auto indices = std::views::iota(std::ptrdiff_t{0}, n);
    const auto testDeepestOutside = [&](const auto& from, const auto& to) {
        // Deepest on the right of from->to = maximum of the negated determinant.
        const std::ptrdiff_t deepest = *cyclicMax(indices.begin(), indices.end(),
            [&](std::ptrdiff_t k) { return -orientationDeterminant(from, to, polygon[k]); });
        return vertexTest(polygon[deepest]);
    };
    return testDeepestOutside(triangle.a(), triangle.b()) &&
           testDeepestOutside(triangle.b(), triangle.c()) &&
           testDeepestOutside(triangle.c(), triangle.a());
}

/**
 * @brief Visits every pair of closed boxes that intersect, stopping when the
 *        visitor asks to.
 *
 * Boxes are swept left to right; the boxes the sweep line crosses are kept by
 * the ranks of their y-ranges, both in a segment tree (for the ones reaching
 * down past a new box's bottom) and in an ordered set by bottom (for the ones
 * starting inside its y-range), so every intersecting pair is reported once,
 * when the second of the two boxes enters. Empty boxes meet nothing and are
 * skipped. A handful of boxes goes through the plain pair scan instead.
 *
 * Complexity: O(k log k + B) for k boxes, B of whose pairs intersect, plus the
 * visitor's calls.
 *
 * @param count Number of boxes k.
 * @param boxOf Maps an index in `[0, count)` to its box (a Rectangle).
 * @param visit Called as `visit(i, j)` with `i < j` for each intersecting
 *        pair; returning `true` stops the scan.
 * @return `true` if some call to @p visit returned `true`.
 */
template <class BoxOf, class Visit>
bool anyIntersectingBoxPair(std::size_t count, BoxOf boxOf, Visit visit) {
    constexpr std::size_t plainScanLimit = 32;
    if (count < plainScanLimit) {
        for (std::size_t i = 0; i < count; ++i) {
            for (std::size_t j = i + 1; j < count; ++j) {
                if (boxOf(i).intersects(boxOf(j)) && visit(i, j)) {
                    return true;
                }
            }
        }
        return false;
    }
    std::vector<std::size_t> live;
    live.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        if (!boxOf(i).empty()) {
            live.push_back(i);
        }
    }
    const std::size_t k = live.size();
    if (k < 2) {
        return false;
    }
    using Number = std::remove_cvref_t<decltype(boxOf(0).min().y())>;

    // Ranks of the y-coordinates, so the tree can be built over positions.
    std::vector<Number> ys;
    ys.reserve(2 * k);
    for (const std::size_t i : live) {
        ys.push_back(boxOf(i).min().y());
        ys.push_back(boxOf(i).max().y());
    }
    std::sort(ys.begin(), ys.end());
    ys.erase(std::unique(ys.begin(), ys.end()), ys.end());
    const auto rankOf = [&ys](const Number& y) {
        return static_cast<std::size_t>(std::lower_bound(ys.begin(), ys.end(), y) - ys.begin());
    };
    std::vector<std::size_t> low(k);
    std::vector<std::size_t> high(k);
    for (std::size_t s = 0; s < k; ++s) {
        low[s] = rankOf(boxOf(live[s]).min().y());
        high[s] = rankOf(boxOf(live[s]).max().y());
    }
    const std::size_t leaves = ys.size();

    // Segment tree over the ranks: each box sits in the O(log k) nodes that
    // cover its y-range canonically, so a root-to-leaf path meets it at most
    // once. Each node entry remembers its slot in the box's cover list and
    // vice versa, which makes removal a swap with the node's last entry.
    struct Entry {
        std::size_t box;
        std::size_t coverSlot;
    };
    struct Placement {
        std::size_t node;
        std::size_t position;
    };
    std::vector<std::vector<Entry>> nodes(4 * leaves);
    std::vector<std::vector<Placement>> cover(k);
    const auto insertBox = [&](std::size_t box) {
        const auto place = [&](const auto& self, std::size_t node, std::size_t from,
                               std::size_t to) -> void {
            if (high[box] < from || to < low[box]) {
                return;
            }
            if (low[box] <= from && to <= high[box]) {
                cover[box].push_back(Placement{node, nodes[node].size()});
                nodes[node].push_back(Entry{box, cover[box].size() - 1});
                return;
            }
            const std::size_t middle = from + (to - from) / 2;
            self(self, 2 * node + 1, from, middle);
            self(self, 2 * node + 2, middle + 1, to);
        };
        place(place, 0, 0, leaves - 1);
    };
    const auto removeBox = [&](std::size_t box) {
        for (const Placement& placement : cover[box]) {
            auto& list = nodes[placement.node];
            list[placement.position] = list.back();
            cover[list[placement.position].box][list[placement.position].coverSlot].position =
                placement.position;
            list.pop_back();
        }
        cover[box].clear();
    };
    std::set<std::pair<std::size_t, std::size_t>> byLow;

    // Enter events before leave events at one x: closed boxes that only touch
    // along a vertical line still intersect.
    std::vector<std::pair<std::size_t, bool>> events;  // (box, leaves)
    events.reserve(2 * k);
    for (std::size_t s = 0; s < k; ++s) {
        events.emplace_back(s, false);
        events.emplace_back(s, true);
    }
    std::vector<Number> xs;
    xs.reserve(2 * k);
    for (std::size_t s = 0; s < k; ++s) {
        xs.push_back(boxOf(live[s]).min().x());
        xs.push_back(boxOf(live[s]).max().x());
    }
    std::sort(events.begin(), events.end(), [&xs](const auto& first, const auto& second) {
        const Number& x1 = xs[2 * first.first + (first.second ? 1 : 0)];
        const Number& x2 = xs[2 * second.first + (second.second ? 1 : 0)];
        if (x1 < x2) return true;
        if (x2 < x1) return false;
        return first.second < second.second;
    });

    const auto report = [&](std::size_t one, std::size_t two) {
        const std::size_t i = live[one];
        const std::size_t j = live[two];
        return i < j ? visit(i, j) : visit(j, i);
    };
    for (const auto& [box, leaving] : events) {
        if (leaving) {
            removeBox(box);
            byLow.erase({low[box], box});
            continue;
        }
        // Boxes starting inside this y-range...
        for (auto it = byLow.lower_bound({low[box], 0}); it != byLow.end() && it->first <= high[box]; ++it) {
            if (report(it->second, box)) {
                return true;
            }
        }
        // ...and boxes starting below it that reach its bottom.
        std::size_t node = 0;
        std::size_t from = 0;
        std::size_t to = leaves - 1;
        while (true) {
            for (const Entry& entry : nodes[node]) {
                if (low[entry.box] < low[box] && report(entry.box, box)) {
                    return true;
                }
            }
            if (from == to) {
                break;
            }
            const std::size_t middle = from + (to - from) / 2;
            if (low[box] <= middle) {
                node = 2 * node + 1;
                to = middle;
            } else {
                node = 2 * node + 2;
                from = middle + 1;
            }
        }
        insertBox(box);
        byLow.insert({low[box], box});
    }
    return false;
}

/**
 * @brief Exact coordinate type for a mixed pair, mirroring separates1DSet.
 *
 * The type the crossings of two shapes are held in: a rational over
 * arbitrary-precision integers unless an operand already brought floating-point
 * coordinates, in which case there is no exactness left to preserve.
 */
template <class ANumber, class BNumber>
using Exact1DNumber = std::conditional_t<
    std::is_floating_point_v<ANumber> || std::is_floating_point_v<BNumber>,
    double, Rational<BigInt>>;

/**
 * @name Shape Category Traits
 * predicates.hpp uses these traits to route generic 'Shape' overloads toward
 * the correct helper at compile time. The single-shape detectors (is_segment,
 * is_line, ...) live next to their shape in the geometry headers, alongside the
 * public XxxConcept they back; this file keeps only the composite detectors.
 */

template <class T>
struct is_area_cut_target : std::false_type {};

template <class PointType, class Label>
struct is_area_cut_target<Rectangle<PointType, Label>> : std::true_type {};

template <class PointType, class Label>
struct is_area_cut_target<Triangle<PointType, Label>> : std::true_type {};

template <class T>
inline constexpr bool is_area_cut_target_v = is_area_cut_target<std::remove_cvref_t<T>>::value;

/**
 * A segment is on the polygon boundary only if one polygon edge contains the
 * whole segment. This deliberately rejects chords whose endpoints are on
 * the boundary but whose interior cuts through the polygon.
 */

template <class Polygon, class OtherSegment>
constexpr bool polygonBoundaryContainsSegment(const Polygon& polygon, const OtherSegment& other) {
    const auto boundary = polygon.edges();
    for (const auto& edge : boundary) {
        if (edge.contains(other)) {
            return true;
        }
    }
    return false;
}

/**
 * Closed rectangle/line intersection.
 *
 * A line meets the rectangle iff the rectangle vertices are not all on the
 * same side of the line, or one vertex lies exactly on the line.
 */
template <class RectangleType, class FirstPoint, class SecondPoint>
constexpr bool lineIntersectsRectangle(const RectangleType& rectangle, const FirstPoint& first, const SecondPoint& second) {
    // The line's two points appear in all four signs, so filtering them once
    // converts each of their coordinates a single time instead of four.
    using Coordinate = sign_coordinate_t<typename FirstPoint::NumberType,
                                         typename SecondPoint::NumberType,
                                         typename RectangleType::NumberType>;
    const auto a = filtered<Coordinate>(first);
    const auto b = filtered<Coordinate>(second);
    bool has_positive = false;
    bool has_negative = false;
    const auto vertices = rectangle.vertices();
    for (const auto& vertex : vertices) {
        const auto side = orientationSignOf(a, b, filtered<Coordinate>(vertex)).value();
        if (side == std::partial_ordering::equivalent) {
            return true;
        }
        has_positive = has_positive || side == std::partial_ordering::greater;
        has_negative = has_negative || side == std::partial_ordering::less;
    }
    return has_positive && has_negative;
}

/**
 * Closed rectangle/segment intersection, over a non-empty rectangle.
 *
 * Both shapes are convex, so they miss each other exactly when one of their
 * edge normals separates them: the rectangle's two axes, and the segment's own.
 * The axes are the bounding-box overlap tested first, and the segment's normal
 * separates precisely when every rectangle vertex lies strictly on one side of
 * the line through the segment -- which is what 'lineIntersectsRectangle'
 * already decides. Taking the two together answers the predicate in a handful
 * of coordinate comparisons and four orientation signs, where walking the
 * rectangle's own edges would build four segments and run a general segment
 * intersection against each.
 *
 * The endpoints arrive in whatever order the caller holds them, so the segment
 * extent is sorted here rather than assumed.
 */
template <class RectangleType, class FirstPoint, class SecondPoint>
constexpr bool segmentIntersectsRectangle(const RectangleType& rectangle, const FirstPoint& first, const SecondPoint& second) {
    const auto& low = rectangle.min();
    const auto& high = rectangle.max();

    const bool rising_x = first.x() < second.x();
    if ((rising_x ? first.x() : second.x()) > high.x() ||
        (rising_x ? second.x() : first.x()) < low.x()) {
        return false;
    }
    const bool rising_y = first.y() < second.y();
    if ((rising_y ? first.y() : second.y()) > high.y() ||
        (rising_y ? second.y() : first.y()) < low.y()) {
        return false;
    }
    return lineIntersectsRectangle(rectangle, first, second);
}

/**
 * Strict rectangle-interior / line intersection.
 *
 * This uses the same side test as 'lineIntersectsRectangle', but a tangent line
 * that only touches the boundary does not count.
 */
template <class RectangleType, class FirstPoint, class SecondPoint>
constexpr bool lineIntersectsRectangleInterior(const RectangleType& rectangle, const FirstPoint& first, const SecondPoint& second) {
    bool has_positive = false;
    bool has_negative = false;
    const auto vertices = rectangle.vertices();
    for (const auto& vertex : vertices) {
        const auto side = orientationSign(first, second, vertex);
        has_positive = has_positive || side == std::partial_ordering::greater;
        has_negative = has_negative || side == std::partial_ordering::less;
    }
    return has_positive && has_negative;
}

/**
 * Strict rectangle-interior / segment intersection.
 *
 * After rejecting degenerate segments and accepting segments whose endpoint is
 * already inside, we require the supporting line to cross the rectangle
 * interior. We then count how many distinct rectangle sides the segment
 * touches. Corner hits touch two edges at once, so they are merged before the
 * final enter/exit test.
 */
template <class RectangleType, class FirstPoint, class SecondPoint>
constexpr bool segmentIntersectsRectangleInteriorExact(const RectangleType& rectangle, const FirstPoint& first, const SecondPoint& second) {
    using Coordinate = promoted_number_t<std::common_type_t<
        std::remove_cvref_t<decltype(rectangle.min().x())>,
        std::remove_cvref_t<decltype(first.x())>,
        std::remove_cvref_t<decltype(second.x())>>>;
    using SegmentPoint = Point<Coordinate>;
    using TestSegment = Segment<SegmentPoint>;

    const TestSegment segment{SegmentPoint(first), SegmentPoint(second)};
    if (segment.isDegenerate()) {
        return false;
    }

    if (rectangle.interiorContains(first) || rectangle.interiorContains(second)) {
        return true;
    }

    if (!lineIntersectsRectangleInterior(rectangle, first, second)) {
        return false;
    }

    const auto edges = rectangle.edges();
    std::array<bool, 4> edge_hits{};
    int distinct_boundary_contacts = 0;
    for (std::size_t i = 0; i < edges.size(); ++i) {
        edge_hits[i] = segment.intersects(edges[i]);
        distinct_boundary_contacts += edge_hits[i] ? 1 : 0;
    }

    if (distinct_boundary_contacts < 2) {
        return false;
    }

    const auto vertices = rectangle.vertices();
    if (segment.contains(vertices[0]) && edge_hits[0] && edge_hits[3]) {
        --distinct_boundary_contacts;
    }
    if (segment.contains(vertices[1]) && edge_hits[0] && edge_hits[1]) {
        --distinct_boundary_contacts;
    }
    if (segment.contains(vertices[2]) && edge_hits[1] && edge_hits[2]) {
        --distinct_boundary_contacts;
    }
    if (segment.contains(vertices[3]) && edge_hits[2] && edge_hits[3]) {
        --distinct_boundary_contacts;
    }

    return distinct_boundary_contacts >= 2;
}

/**
 * @brief Tests whether removing a convex shape disconnects a monotone chain.
 *
 * The chain is an arc, so removing `remover ∩ chain` disconnects it exactly
 * when some connected component of that intersection avoids both extreme
 * vertices. Because the remover is convex, its intersection with each chain
 * edge is connected, and consecutive edge pieces belong to one component iff
 * the shared vertex lies in the remover — so one pass over the edges tracks
 * the components exactly, division-free.
 *
 * @pre `remover` is a convex point set (every currently supported shape except
 *      Polygon and MonotoneChain).
 */
template <class Remover, MonotoneChainConcept Chain>
constexpr bool separatesChain(const Remover& remover, const Chain& chain) {
    const std::size_t n = chain.size();
    if (n < 2) {
        // Removing anything from at most one point cannot disconnect it.
        return false;
    }
    bool active = false;     // a component is in progress
    bool touched = false;    // ... and it touches an extreme vertex
    for (std::size_t i = 0; i + 1 < n; ++i) {
        const Segment<typename Chain::PointType> edge(chain[i], chain[i + 1]);
        if (edge.intersects(remover)) {
            const bool connected = active && remover.contains(chain[i]);
            if (!connected) {
                if (active && !touched) {
                    return true;
                }
                touched = false;
            }
            active = true;
            if (i == 0 && remover.contains(chain[0])) {
                touched = true;
            }
            if (i + 2 == n && remover.contains(chain[n - 1])) {
                touched = true;
            }
        } else {
            if (active && !touched) {
                return true;
            }
            active = false;
            touched = false;
        }
    }
    return active && !touched;
}

/**
 * @name HalfplaneIntersection helpers
 * Shared building blocks for predicates involving a half-plane intersection
 * region. A region is convex but possibly unbounded and possibly degenerate
 * (empty interior), so the helpers reduce those states to simpler shapes:
 * a degenerate region to its carrier (point, segment, ray, or line), and an
 * unbounded region — when only its part near a bounded target matters — to a
 * bounded region clipped by an enclosing box.
 */

/**
 * @brief Exact coordinate type for constructions on a half-plane intersection
 * (vertices, edges, chords): rational over BigInt unless the input
 * coordinates are floating point (mirroring detail::separates1DSet).
 */
template <class Number>
using region_exact_number_t =
    std::conditional_t<std::is_floating_point_v<Number>, double, Rational<BigInt>>;

/**
 * @brief Tests whether the region is contained in the half-plane.
 *
 * The redundancy test of `insert`, asked without inserting: the half-plane is
 * discarded exactly when the region is already inside it. The empty region is
 * inside every half-plane. Complexity: O(log n) for a region of `n` stored
 * half-planes.
 */
template <class Region, HalfplaneConcept OtherHalfplane>
constexpr bool regionInsideHalfplane(const Region& region, const OtherHalfplane& halfplane) {
    return !region.insertChanges(halfplane);
}

/**
 * @brief Tests whether the region is contained in the open interior of the
 * half-plane.
 */
template <class Region, HalfplaneConcept OtherHalfplane>
constexpr bool regionInsideHalfplaneInterior(const Region& region, const OtherHalfplane& halfplane) {
    return regionInsideHalfplane(region, halfplane) && !region.intersects(halfplane.asLine());
}

/**
 * @brief Tests whether the region has points strictly on both sides of the
 * oriented line `first -> second`.
 *
 * True for the whole plane; false for the empty region.
 */
template <class Region, PointConcept FirstPoint, PointConcept SecondPoint>
constexpr bool regionStrictlyOnBothSides(const Region& region, const FirstPoint& first, const SecondPoint& second) {
    const Halfplane<std::remove_cvref_t<FirstPoint>> left(first, second);
    return !regionInsideHalfplane(region, left) && !regionInsideHalfplane(region, left.opposite());
}

/**
 * @brief The point set of a degenerate (empty-interior, nonempty) region as a
 * typed shape with exact coordinates: a point, a segment, a ray, or a line.
 *
 * Precondition: `region.isDegenerate() && !region.empty()`.
 */
template <class Region>
constexpr auto degenerateRegionCarrier(const Region& region) {
    using E = region_exact_number_t<typename Region::NumberType>;
    using EPoint = Point<E, typename Region::PointType::LabelType>;
    using Carrier = std::variant<EPoint, Segment<EPoint>, Ray<EPoint>, Line<EPoint>>;
    if (region.isBounded()) {
        // A point or a segment: the convex hull of the (partly coincident)
        // implicit vertices.
        const auto verts = region.template vertices<E>();
        EPoint lo = verts[0];
        EPoint hi = verts[0];
        for (const auto& v : verts) {
            if (v < lo) {
                lo = v;
            }
            if (hi < v) {
                hi = v;
            }
        }
        if (lo == hi) {
            return Carrier(lo);
        }
        return Carrier(Segment<EPoint>(lo, hi));
    }
    // An unbounded degenerate region is a ray or a line, and then some
    // boundary edge equals the whole region.
    for (std::size_t i = 0; i < region.size(); ++i) {
        const auto e = region.template edge<E>(i);
        if (const auto* ray = std::get_if<Ray<EPoint>>(&e)) {
            return Carrier(*ray);
        }
        if (const auto* line = std::get_if<Line<EPoint>>(&e)) {
            return Carrier(*line);
        }
    }
    // Unreachable for a degenerate region; keep a deterministic fallback.
    return Carrier(EPoint(region[0].source()));
}

/**
 * @brief The region clipped to an axis-aligned box strictly containing the
 * given bounding rectangle (inflated by a margin of two, which also absorbs
 * the toward-zero cast into the region's coordinate type).
 *
 * Predicates against a bounded target shape only depend on the part of the
 * region near the target, so they may be evaluated on the (bounded) clipped
 * region instead; see the call sites for the connectivity argument.
 */
template <class Region, RectangleConcept BoundingRectangle>
constexpr std::remove_cvref_t<Region> regionClippedToBox(const Region& region, const BoundingRectangle& bounds) {
    using Result = std::remove_cvref_t<Region>;
    using RegionPoint = typename Result::PointType;
    using N = typename Result::NumberType;
    using RegionHalfplane = typename Result::HalfplaneType;
    const N margin(2);
    // A fractional bound is rounded away from the box before the margin is
    // added, so the corners are whole numbers whatever N is. The box only has
    // to contain @p bounds, and rounding outward keeps that; what it buys is
    // the depth of everything downstream. Clipping against a corner that is
    // itself a deep fraction — a disk's bounding box under rational
    // coordinates carries twelve-digit numerators over eight-digit
    // denominators — hands every clipped vertex those denominators, and the
    // degree-four predicates over them then run on numbers with hundreds of
    // digits. An integral corner leaves the clipped vertices as shallow as the
    // region's own. For an integral N this is what the toward-zero cast
    // already did.
    const auto roundOutward = [](const auto& value, bool up) -> N {
        if constexpr (pgl::is_Rational_v<std::remove_cvref_t<decltype(value)>>) {
            // Both parts are wanted, so reduce once and read them off that copy:
            // numerator() and denominator() each run their own gcd otherwise.
            const auto reduced = value.simplified();
            const auto numerator = reduced.numerator();
            const auto denominator = reduced.denominator();  // always positive
            auto quotient = numerator / denominator;        // truncates toward zero
            const bool exact = quotient * denominator == numerator;
            if (!exact && (numerator < 0) == !up) {
                quotient = up ? quotient + 1 : quotient - 1;
            }
            return N(quotient);
        } else {
            return N(value);
        }
    };
    const RegionPoint lo(roundOutward(bounds.min().x(), false) - margin,
                         roundOutward(bounds.min().y(), false) - margin);
    const RegionPoint hi(roundOutward(bounds.max().x(), true) + margin,
                         roundOutward(bounds.max().y(), true) + margin);
    const RegionPoint lohi(lo.x(), hi.y());
    const RegionPoint hilo(hi.x(), lo.y());
    Result result(region);
    result.insert(RegionHalfplane(lo, hilo));
    result.insert(RegionHalfplane(hilo, hi));
    result.insert(RegionHalfplane(hi, lohi));
    result.insert(RegionHalfplane(lohi, lo));
    return result;
}

/**
 * @brief Folds a predicate over every boundary edge of a holed region, outer
 * ring first, stopping at the first edge that fails.
 *
 * The union of these edges is `∂A`, and when the region has no area it is all
 * of `A`: a closed set of zero measure has empty interior, so `A ⊆ ∂A`, and
 * every ring lies in `A` to begin with. That identity is what the
 * reverse-direction containment predicates need — a target that is at most
 * one-dimensional (a boundary, a chain) can hold a region only when the region
 * has no area, and then edge by edge is exact.
 */
template <class HoledRegion, class EdgePredicate>
constexpr bool everyHoledRegionEdge(const HoledRegion& region, EdgePredicate&& predicate) {
    for (const auto& edge : region.outer().edgesView()) {
        if (!predicate(edge)) {
            return false;
        }
    }
    for (const auto& hole : region.holes()) {
        for (const auto& edge : hole.edgesView()) {
            if (!predicate(edge)) {
                return false;
            }
        }
    }
    return true;
}

}  // namespace detail

}  // namespace pgl
