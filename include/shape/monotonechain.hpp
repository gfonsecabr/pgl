#pragma once

#include "shape/convex.hpp"

#include <algorithm>
#include <vector>
#include <cassert>
#include <compare>
#include <concepts>
#include <cstddef>
#include <functional>
#include <iterator>
#include <limits>
#include <optional>
#include <ostream>
#include <ranges>
#include <span>
#include <type_traits>
#include <utility>
#include <variant>


namespace pgl {

template <class PointType = Point<>, class Label, class Storage>
struct MonotoneChain;

namespace detail {
/**
 * @brief True when a chain storage type owns its vertices (can be grown and
 * sorted in place), as opposed to a non-owning view like `std::span`.
 *
 * Both the in-class declaration and the out-of-line definition of every
 * mutating member spell this concept identically, so it gates owning-only
 * operations (insert, in-place scaling/rotation, `operator*=`, …) out of the
 * overload set for view instantiations.
 */
template <class Storage, class PointType>
concept ownsChainStorage = requires(Storage& s, const PointType& p) {
    s.push_back(p);
};

/**
 * @brief True when every point of a non-empty range is the same point.
 *
 * Backs @ref MonotoneChain::isPoint and, further down the include chain,
 * @ref Polyline::isPoint and @ref Polygon::isPoint. Stops at the first
 * differing point, so a non-degenerate shape usually costs O(1).
 *
 * @return `false` for an empty range: a point needs a defining vertex.
 */
template <std::ranges::forward_range Range>
constexpr bool allPointsEqual(const Range& points) {
    auto first = std::ranges::begin(points);
    const auto last = std::ranges::end(points);
    if (first == last) {
        return false;
    }
    return std::find_if(std::next(first), last,
                        [&](const auto& p) { return p != *first; }) == last;
}

/**
 * @brief True when the points of a range are collinear and not all equal, so
 * they span a segment of positive length.
 *
 * Backs the `isSegment` predicates of @ref MonotoneChain, @ref Polyline and
 * @ref Polygon. Stops at the first non-collinear point, so a shape with real
 * area usually costs O(1).
 */
template <std::ranges::forward_range Range>
constexpr bool pointsSpanSegment(const Range& points) {
    const auto first = std::ranges::begin(points);
    const auto last = std::ranges::end(points);
    if (first == last) {
        return false;
    }
    // Anchor the line on the first point and the first one differing from it.
    const auto second = std::find_if(std::next(first), last,
                                     [&](const auto& p) { return p != *first; });
    if (second == last) {
        return false;  // All equal: a point, not a segment.
    }
    return std::all_of(std::next(second), last,
                       [&](const auto& p) { return collinear(*first, *second, p); });
}

/**
 * @brief Returns the segment spanned by a range of collinear points.
 *
 * @pre `pointsSpanSegment(points)` holds, so the lexicographic extremes are the
 *      endpoints of the spanned segment.
 */
template <class SegmentType, std::ranges::forward_range Range>
constexpr SegmentType spannedSegment(const Range& points) {
    const auto [low, high] = std::ranges::minmax_element(points);
    return SegmentType(*low, *high);
}
}  // namespace detail

MonotoneChain() -> MonotoneChain<Point<>, NoLabel>;

template <std::ranges::input_range Range>
requires detail::is_point_v<std::ranges::range_value_t<Range>>
MonotoneChain(Range&&) -> MonotoneChain<std::remove_cvref_t<std::ranges::range_value_t<Range>>, NoLabel>;

template <std::ranges::input_range Range>
requires detail::is_point_v<std::ranges::range_value_t<Range>>
MonotoneChain(Range&&, bool) -> MonotoneChain<std::remove_cvref_t<std::ranges::range_value_t<Range>>, NoLabel>;

template <class Number>
requires (!detail::is_point_v<Number>)
MonotoneChain(std::initializer_list<Number>) -> MonotoneChain<Point<Number>, NoLabel>;

template <class Number>
requires (!detail::is_point_v<Number>)
MonotoneChain(std::initializer_list<Number>, bool) -> MonotoneChain<Point<Number>, NoLabel>;


/**
 * @brief A weakly x-monotone polyline stored by its sorted vertices plus a translation.
 *
 * `MonotoneChain` mirrors the storage layout of @ref Polygon — a vector of
 * vertices and a `translation_` applied lazily on access — but the vertices
 * form an open chain, not a closed boundary, and they obey a monotonicity
 * invariant: the stored sequence is strictly increasing in the lexicographic
 * point order (smaller x first, ties broken by smaller y). Consecutive
 * vertices may therefore share an x-coordinate, producing a vertical edge, so
 * the chain is *weakly* x-monotone; @ref isStrictlyMonotone reports whether
 * every x appears at most once (the chain is the graph of a function).
 *
 * The constructor normalizes any input to this canonical form by sorting the
 * points lexicographically and removing duplicates. The input is thus treated
 * as a **point set**, not as a pre-linked chain: shuffled input yields the
 * same object. Because the order is unique, the chain is automatically simple
 * (edges meet only at shared endpoints), and `operator==`/`operator<=>` give a
 * translation-consistent geometric equality.
 *
 * As a 1-dimensional manifold with boundary, the chain's boundary is its two
 * extreme vertices and its relative interior is everything else (matching the
 * convention of @ref Segment).
 *
 * @tparam PointType_ The vertex point type.
 */
template <class PointType_, class TLabel, class Storage>
struct MonotoneChain {
    using PointType = PointType_;
    using NumberType = PointType::NumberType;
    using LabelType = TLabel;
    using StorageType = Storage;
    // The owning counterpart of this chain: the same vertex and label type
    // backed by an owned vector. Value-returning transformations produce this
    // type so that even a view (which cannot own vertices) yields a real chain.
    using OwningChain = MonotoneChain<PointType_, TLabel, std::vector<PointType_>>;
    static_assert(detail::is_point_v<PointType>, "MonotoneChain requires pgl::Point vertices");

    template <bool Oriented>
    using BoundaryType = std::conditional_t<Oriented, OrientedSegment<PointType>, Segment<PointType>>;

    template <bool Oriented>
    class BoundaryIterator;

    using EdgeIterator = BoundaryIterator<false>;
    using OrientedEdgeIterator = BoundaryIterator<true>;

    /**
     * @brief Creates a chain with no vertex.
     */
    constexpr MonotoneChain() = default;

    /**
     * @brief Creates a chain from a range of points.
     *
     * The points are treated as a set: unless @p trusted is set, they are
     * sorted lexicographically and duplicates are removed, producing the
     * canonical weakly x-monotone chain through them.
     *
     * @tparam Range Input range whose elements can be converted to @ref PointType.
     * @param points Range of vertices in any order.
     * @param trusted Set to true if the points are already sorted and unique.
     */
    template<std::ranges::input_range Range = std::initializer_list<PointType>>
    requires std::ranges::common_range<Range> &&
             std::convertible_to<std::ranges::range_value_t<Range>, PointType> &&
             detail::ownsChainStorage<Storage, PointType>
    constexpr explicit MonotoneChain(Range&& points, bool trusted = false) {
        for (const auto& p : points) {
            points_.push_back(p);
        }
        if (!trusted) {
            normalize();
        }
        assert(std::is_sorted(points_.begin(), points_.end()) &&
               std::adjacent_find(points_.begin(), points_.end()) == points_.end());
    }

    /**
     * @brief Creates a non-owning chain viewing an external contiguous range of
     * vertices (view instantiations only, e.g. @ref MonotoneChainView).
     *
     * The view cannot sort or deduplicate memory it does not own, so the input
     * must already be in canonical form — sorted lexicographically with no
     * duplicates (the @p trusted contract of the owning constructors). The
     * caller retains ownership of the underlying storage and is responsible for
     * keeping it alive for the lifetime of the view.
     *
     * @tparam Range Contiguous range of vertices convertible to @ref Storage.
     * @param points Canonical range of vertices to view.
     */
    template<std::ranges::contiguous_range Range>
    requires (!detail::ownsChainStorage<Storage, PointType>) &&
             std::constructible_from<Storage, Range&&>
    constexpr explicit MonotoneChain(Range&& points, bool /*trusted*/ = true)
        : points_(std::forward<Range>(points)) {
        assert(std::is_sorted(points_.begin(), points_.end()) &&
               std::adjacent_find(points_.begin(), points_.end()) == points_.end());
    }

    /**
     * @brief Creates a chain from a flat list of coordinates.
     *
     * The values are consumed in pairs `(x0, y0, x1, y1, …)`, each pair forming
     * one vertex, so the list must hold an even number of values. Unless
     * @p trusted is set, the vertices are sorted lexicographically and
     * duplicates are removed.
     *
     * @param coords Interleaved x/y coordinates of the vertices.
     * @param trusted Set to true if the points are already sorted and unique.
     */
    constexpr explicit MonotoneChain(std::initializer_list<NumberType> coords, bool trusted = false)
        requires detail::ownsChainStorage<Storage, PointType>
    {
        assert(coords.size() % 2 == 0);
        points_.reserve(coords.size() / 2);
        for (auto it = coords.begin(); it != coords.end(); ) {
            NumberType x = *it++;
            NumberType y = *it++;
            points_.emplace_back(x, y);
        }
        if (!trusted) {
            normalize();
        }
        assert(std::is_sorted(points_.begin(), points_.end()) &&
               std::adjacent_find(points_.begin(), points_.end()) == points_.end());
    }

    /**
     * @brief Converts a chain with compatible vertex type.
     *
     * The source is already canonical; a translation or a non-narrowing type
     * conversion preserves that, so no renormalization is needed.
     *
     * @tparam OtherPointType Source vertex type.
     * @param other Source chain.
     */
    template<PointConcept OtherPointType, class OtherLabelType, class OtherStorage>
        requires(std::constructible_from<PointType, const OtherPointType&> &&
                 detail::ownsChainStorage<Storage, PointType>)
    constexpr MonotoneChain(const MonotoneChain<OtherPointType, OtherLabelType, OtherStorage>& other)
        : points_(other.begin(), other.end()), label_(detail::copyLabel<LabelType>(other)) {}

    /**
     * @brief Returns the chain label.
     *
     * The label is mutable even through a const chain: it is metadata that
     * does not participate in equality, hashing, or geometric predicates.
     *
     * @return Reference to the stored label.
     */
    template <class A = LabelType>
        requires(detail::has_label_v<A>)
    constexpr A& label() const {
        return label_;
    }

    /**
     * @brief Accesses a vertex by index (in lexicographic order).
     * @param index The index of the vertex.
     * @return The vertex at the given index.
     */
    constexpr const PointType operator[](std::size_t index) const {
        assert(index < size());
        return points_[index] + translation_;
    }

    /**
     * @brief Accesses a vertex by index modulo the vertex count.
     *
     * Unlike @ref Polygon the chain is not cyclic — edges never wrap around —
     * but `get` still reduces the index modulo @ref size() (Euclidean, so
     * negative indices count from the back) to satisfy the common vertex
     * access interface of @ref Shape.
     *
     * @param index The index of the vertex, reduced modulo the vertex count.
     * @return The vertex at the reduced index.
     */
    constexpr PointType get(std::ptrdiff_t index) const {
        const std::ptrdiff_t n = static_cast<std::ptrdiff_t>(size());
        return (*this)[static_cast<std::size_t>(((index % n) + n) % n)];
    }

    /**
     * @brief Returns the smallest index `i` with `(*this)[i] == point`, or
     * `-1` if `point` is not a vertex.
     *
     * Complexity: O(log n) for n vertices (binary search on the sorted
     * vertex sequence).
     *
     * @param point The vertex to locate.
     * @return The vertex index, or `-1` if `point` is not a vertex.
     */
    constexpr std::ptrdiff_t index(const PointType& point) const {
        const PointType query = point - translation_;
        const auto it = std::lower_bound(points_.begin(), points_.end(), query);
        if (it != points_.end() && *it == query) {
            return it - points_.begin();
        }
        return -1;
    }

    /**
     * @brief Returns a constant iterator to the first vertex.
     */
    constexpr auto begin() const {
        return Iterator(points_.begin(), translation_);
    }

    /**
     * @brief Returns a constant iterator to the first vertex.
     */
    constexpr auto cbegin() const {
        return Iterator(std::ranges::begin(points_), translation_);
    }

    /**
     * @brief Returns a constant iterator past the last vertex.
     */
    constexpr auto end() const {
        return Iterator(points_.end(), translation_);
    }

    /**
     * @brief Returns a constant iterator past the last vertex.
     */
    constexpr auto cend() const {
        return Iterator(std::ranges::end(points_), translation_);
    }

    /**
     * @brief Compares two chains by their canonical vertex sequences.
     *
     * Templated on the other chain's storage so an owning chain and a view over
     * the same vertices compare equal; the vertices are read through the public
     * (translation-applied) accessors, so no cross-instantiation access is needed.
     */
    template <class OtherStorage>
    constexpr auto operator<=>(const MonotoneChain<PointType_, TLabel, OtherStorage>& other) const {
        if (auto cmp = size() <=> other.size(); cmp != 0) {
            return cmp;
        }
        for (std::size_t i = 0; i < size(); ++i) {
            if (auto cmp = (*this)[i] <=> other[i]; cmp != 0) {
                return cmp;
            }
        }
        return std::strong_ordering::equal;
    }

    /**
     * @brief Checks equality of two chains.
     * @return True if both chains have the same vertices.
     */
    template <class OtherStorage>
    constexpr bool operator==(const MonotoneChain<PointType_, TLabel, OtherStorage>& other) const {
        if (size() != other.size()) {
            return false;
        }
        for (std::size_t i = 0; i < size(); ++i) {
            if ((*this)[i] != other[i]) {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief Returns the number of vertices in the chain.
     */
    constexpr std::size_t size() const {
        return points_.size();
    }

    /**
     * @brief Checks whether the chain has no vertex.
     */
    constexpr bool empty() const {
        return points_.empty();
    }

    /**
     * @brief Checks if the chain is degenerate (fewer than two vertices, so it
     * has no edge).
     */
    constexpr bool isDegenerate() const {
        return points_.size() < 2;
    }

    /**
     * @brief Checks whether the chain covers exactly one point.
     *
     * Unlike @ref isDegenerate, this is about the point set rather than the
     * edge count: a chain repeating one vertex many times is a single point.
     *
     * Complexity: O(n), returning at the first differing vertex.
     *
     * @return `true` if the chain has at least one vertex and all are equal.
     */
    [[nodiscard]] constexpr bool isPoint() const {
        return detail::allPointsEqual(points_);
    }

    /**
     * @brief Returns the point the chain collapses to, if it does.
     *
     * Complexity: O(n), returning at the first differing vertex.
     *
     * @return The common vertex if @ref isPoint, `std::nullopt` otherwise.
     */
    [[nodiscard]] constexpr std::optional<PointType> getIfPoint() const {
        if (!isPoint()) {
            return std::nullopt;
        }
        return points_.front();
    }

    /**
     * @brief Checks whether the chain covers exactly one segment of positive
     * length.
     *
     * True when the vertices are collinear but not all equal. The chain is
     * connected, so collinear vertices make its edges cover the single segment
     * spanning them.
     *
     * Complexity: O(n), returning at the first non-collinear vertex.
     */
    [[nodiscard]] constexpr bool isSegment() const {
        return detail::pointsSpanSegment(points_);
    }

    /**
     * @brief Returns the segment the chain collapses to, if it does.
     *
     * Complexity: O(n).
     *
     * @return The spanned segment if @ref isSegment, `std::nullopt` otherwise.
     */
    [[nodiscard]] constexpr std::optional<BoundaryType<false>> getIfSegment() const {
        if (!isSegment()) {
            return std::nullopt;
        }
        return detail::spannedSegment<BoundaryType<false>>(points_);
    }

    /**
     * @brief Checks whether the chain is degenerate without covering a point or
     * a segment.
     *
     * True only for the empty chain, which has no defining vertex: a chain with
     * a single vertex is a point, and any chain with an edge covers at least a
     * segment.
     *
     * Complexity: O(1).
     */
    [[nodiscard]] constexpr bool isUndefined() const {
        return empty();
    }

    /**
     * @brief Tests whether the chain is strictly x-monotone.
     *
     * True when no two vertices share an x-coordinate (equivalently, the chain
     * has no vertical edge), so the chain is the graph of a function of x. A
     * chain with fewer than two vertices is trivially strict.
     *
     * Complexity: O(n).
     *
     * @return `true` if every x-coordinate appears at most once.
     */
    [[nodiscard]] constexpr bool isStrictlyMonotone() const {
        for (std::size_t i = 1; i < points_.size(); ++i) {
            if (points_[i - 1].x() == points_[i].x()) {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief Returns a segment realizing the diameter (the farthest vertex pair).
     *
     * The farthest pair of vertices lies on the convex hull of the vertex set,
     * so this builds a @ref Convex from the chain vertices and returns that
     * hull's @ref Convex::diameter(). Distances are compared exactly via
     * squared length.
     *
     * @return A longest segment between two vertices (degenerate if fewer than
     *         two vertices).
     */
    constexpr Segment<PointType> diameter() const {
        return Convex<PointType>(vertices()).diameter();
    }

    /**
     * @brief Computes the bounding box of the chain.
     *
     * The x-extent is free (first and last vertex), but the y-extent requires
     * a scan, so the result is computed on the first call and cached in
     * @ref bbox_; later calls return the stored value. Any operation that
     * modifies the chain resets the cache.
     *
     * Complexity: O(n) for n vertices on the first call, O(1) thereafter.
     *
     * @return A constant reference to the rectangle bounding the chain.
     */
    constexpr const Rectangle<PointType>& bbox() const;

    /**
     * @brief Computes the floating-point bounding box of the chain.
     * @tparam ResultNumber The floating-point type for the result.
     * @return A rectangle with floating-point coordinates representing the bounding box.
     */
    template <std::floating_point ResultNumber = double>
    constexpr Rectangle<Point<ResultNumber>> fbox() const;

    /**
     * @brief Returns the vertices of the chain (translation applied).
     */
    constexpr std::vector<PointType> vertices() const {
        std::vector<PointType> ret(points_.begin(), points_.end());
        for (auto& vertex : ret) {
            vertex += translation_;
        }
        return ret;
    }

    /**
     * @brief Returns the chain as a polyline traversing its vertices in
     * lexicographic order.
     *
     * The chain's vertices are already sorted lexicographically, which is the
     * canonical polyline direction, so no renormalization is needed. The
     * chain's own label is not carried over.
     *
     * @return Polyline through the chain's vertices.
     */
    [[nodiscard]] constexpr Polyline<PointType> asPolyline() const;

    /**
     * @brief Returns the edges of the chain.
     *
     * A chain with n vertices has n - 1 edges (none for a degenerate chain);
     * there is no closing edge back to the first vertex.
     */
    constexpr std::vector<Segment<PointType>> edges() const {
        std::vector<Segment<PointType>> result;
        const auto translatedVertices = vertices();
        for (std::size_t i = 0; i + 1 < translatedVertices.size(); ++i) {
            result.emplace_back(translatedVertices[i], translatedVertices[i + 1]);
        }
        return result;
    }

    /**
     * @brief Returns the oriented edges of the chain, each directed from the
     * lexicographically smaller to the larger endpoint.
     */
    constexpr std::vector<OrientedSegment<PointType>> orientedEdges() const {
        std::vector<OrientedSegment<PointType>> result;
        const auto translatedVertices = vertices();
        for (std::size_t i = 0; i + 1 < translatedVertices.size(); ++i) {
            result.emplace_back(translatedVertices[i], translatedVertices[i + 1]);
        }
        return result;
    }

    /**
     * @brief Returns a lazy view over the edges, materializing each @ref
     * Segment on the fly instead of allocating a vector.
     *
     * Same edge sequence as @ref edges() (n - 1 edges, no closing edge) but
     * with no heap allocation, so it is preferable when the edges are only
     * iterated once — e.g. inside predicate loops.
     */
    constexpr auto edgesView() const {
        return std::ranges::subrange(edgesBegin(), edgesEnd());
    }

    /**
     * @brief Lazy view counterpart of @ref orientedEdges(); see @ref
     * edgesView().
     */
    constexpr auto orientedEdgesView() const {
        return std::ranges::subrange(orientedEdgesBegin(), orientedEdgesEnd());
    }

    /**
     * @brief Returns an iterator to the first unoriented edge.
     * @return Iterator to edge `(vertex 0, vertex 1)`.
     */
    constexpr EdgeIterator edgesBegin() const {
        return EdgeIterator(this, 0);
    }

    /**
     * @brief Returns an iterator past the last unoriented edge.
     * @return Sentinel iterator for @ref edgesBegin().
     */
    constexpr EdgeIterator edgesEnd() const {
        return EdgeIterator(this, edgeCount());
    }

    /**
     * @brief Returns an iterator to the first oriented edge.
     * @return Iterator to edge `vertex 0 -> vertex 1`.
     */
    constexpr OrientedEdgeIterator orientedEdgesBegin() const {
        return OrientedEdgeIterator(this, 0);
    }

    /**
     * @brief Returns an iterator past the last oriented edge.
     * @return Sentinel iterator for @ref orientedEdgesBegin().
     */
    constexpr OrientedEdgeIterator orientedEdgesEnd() const {
        return OrientedEdgeIterator(this, edgeCount());
    }

    /**
     * @brief Extends the chain to contain the given point as a vertex.
     *
     * Inserts the point at its lexicographic position; a point that is already
     * a vertex leaves the chain unchanged. Note that inserting a point whose
     * x-coordinate lies inside an existing edge's x-range *reroutes* the chain
     * through the new vertex (the vertices are a point set, §constructor).
     *
     * Complexity: O(log n) comparisons plus O(n) vector shift; amortized O(1)
     * shift when appending at either end.
     *
     * @param point The vertex to add.
     */
    constexpr void insert(const PointType& point)
        requires detail::ownsChainStorage<Storage, PointType>
    {
        const PointType query = point - translation_;
        const auto it = std::lower_bound(points_.begin(), points_.end(), query);
        if (it != points_.end() && *it == query) {
            return;
        }
        points_.insert(it, query);
        resetCache();
    }

    /**
     * @brief Extends the chain to contain all the given points as vertices.
     *
     * Equivalent to inserting each point of the range, but sorts the new
     * points once and merges, so it is cheaper for bulk insertion.
     *
     * Complexity: O((n + k) log(n + k)) for k inserted points.
     *
     * @param points Range of vertices to add, in any order.
     */
    template<std::ranges::input_range Range>
    requires std::ranges::common_range<Range> &&
             std::convertible_to<std::ranges::range_value_t<Range>, PointType> &&
             detail::ownsChainStorage<Storage, PointType>
    constexpr void insert(Range&& points) {
        const std::size_t oldSize = points_.size();
        for (const auto& p : points) {
            points_.push_back(PointType(p) - translation_);
        }
        if (points_.size() == oldSize) {
            return;
        }
        std::sort(points_.begin() + static_cast<std::ptrdiff_t>(oldSize), points_.end());
        std::inplace_merge(points_.begin(), points_.begin() + static_cast<std::ptrdiff_t>(oldSize), points_.end());
        points_.erase(std::unique(points_.begin(), points_.end()), points_.end());
        resetCache();
    }

    /**
     * @brief Locates the vertex or edge of the chain at a given x-coordinate.
     *
     * Returns the smallest index `i` such that `(*this)[i].x() == x`, or, when
     * no vertex has that x-coordinate, the unique `i` with
     * `(*this)[i].x() < x < (*this)[i+1].x()`. Empty when `x` lies outside the
     * chain's x-extent or the chain is empty. At a vertical edge the returned
     * index is the edge's bottom vertex.
     *
     * Complexity: O(log n) for n vertices.
     *
     * @tparam OtherNumber Query x-coordinate type.
     * @param x Query x-coordinate.
     * @return The located index, or empty when `x` is outside the x-extent.
     */
    template <class OtherNumber>
    [[nodiscard]] constexpr std::optional<std::size_t> indexAtX(const OtherNumber& x) const;

    /**
     * @brief Evaluates the y-coordinate of the chain at a given x-coordinate.
     *
     * The value is returned only when `x` lies within the chain's x-extent.
     * Vertices are handled exactly. When `x` falls on a vertical edge (or on a
     * vertex that starts one), the y of the edge's *bottom* vertex is
     * returned; @ref isStrictlyMonotone is the precondition for `yAtX` to be
     * the unique value of the chain at every x.
     *
     * Complexity: O(log n) for n vertices.
     *
     * @tparam ResultNumber Return coordinate type.
     * @tparam OtherNumber Query x-coordinate type.
     * @param x Query x-coordinate.
     * @return Interpolated y-coordinate, or empty when `x` is outside the x-extent.
     * @warning Divides coordinates after casting to ResultNumber.
     */
    template <class ResultNumber = NumberType, class OtherNumber>
    [[nodiscard]] constexpr std::optional<ResultNumber> yAtX(const OtherNumber& x) const;

    /**
     * @brief Tests whether the whole chain lies strictly below a point at its x.
     *
     * Engaged iff, at `point.x()`, every part of the chain is strictly below
     * @p point — i.e. the top of the chain's vertical run there is below the
     * point. A point lying on the chain (including inside a vertical edge)
     * counts as neither below nor above, so @ref isStrictlyBelow and
     * @ref isStrictlyAbove are mutually exclusive, and both are empty when the
     * point is on the chain or `point.x()` is outside the chain's x-extent.
     * (Contrast the weak @ref isBelow, which a point on the chain satisfies.)
     * The engaged value is the index @ref indexAtX returns for `point.x()`.
     *
     * Complexity: O(log n) for n vertices. Exact (division-free).
     *
     * @param point The query point.
     * @return The index at `point.x()` when the chain is strictly below,
     *         otherwise empty.
     */
    template <PointConcept OtherPoint>
    [[nodiscard]] constexpr std::optional<std::size_t> isStrictlyBelow(const OtherPoint& point) const;

    /**
     * @brief Tests whether the whole chain lies strictly above a point at its x.
     *
     * Engaged iff, at `point.x()`, every part of the chain is strictly above
     * @p point — i.e. the bottom of the chain's vertical run there is above the
     * point. A point lying on the chain (including inside a vertical edge)
     * counts as neither; see @ref isStrictlyBelow.
     *
     * Complexity: O(log n) for n vertices. Exact (division-free).
     *
     * @param point The query point.
     * @return The index at `point.x()` when the chain is strictly above,
     *         otherwise empty.
     */
    template <PointConcept OtherPoint>
    [[nodiscard]] constexpr std::optional<std::size_t> isStrictlyAbove(const OtherPoint& point) const;


    /**
     * @brief Tests whether the chain passes weakly below a point.
     *
     * Engaged iff a ray shot straight *down* from @p point intersects the
     * chain (a point on the chain counts). The engaged value is the index
     * @ref indexAtX returns for `point.x()`.
     *
     * Note that @ref isBelow and @ref isAbove are not complementary: both are
     * engaged when the point lies on the chain, and both are empty when
     * `point.x()` is outside the chain's x-extent.
     *
     * Complexity: O(log n) for n vertices. Exact (division-free).
     *
     * @param point The query point.
     * @return The index at `point.x()` when the downward ray hits the chain,
     *         otherwise empty.
     */
    template <PointConcept OtherPoint>
    [[nodiscard]] constexpr std::optional<std::size_t> isBelow(const OtherPoint& point) const;

    /**
     * @brief Tests whether the chain passes weakly above a point.
     *
     * Engaged iff a ray shot straight *up* from @p point intersects the chain
     * (a point on the chain counts). The engaged value is the index
     * @ref indexAtX returns for `point.x()`.
     *
     * Complexity: O(log n) for n vertices. Exact (division-free).
     *
     * @param point The query point.
     * @return The index at `point.x()` when the upward ray hits the chain,
     *         otherwise empty.
     */
    template <PointConcept OtherPoint>
    [[nodiscard]] constexpr std::optional<std::size_t> isAbove(const OtherPoint& point) const;

    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     *
     * Complexity: O(log n) for n vertices.
     *
     * @tparam OtherPoint Type of the point.
     * @param point Point to test.
     * @return `true` if the point lies on the chain.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool contains(const OtherPoint& point) const;

    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     *
     * A chain contains a segment exactly when the segment is a straight
     * sub-path of the chain: both endpoints lie on the chain and every chain
     * vertex between them is collinear with the segment. The scan exits at the
     * first bend.
     *
     * Complexity: O(log n + k) for n vertices, where k is the number of chain
     * vertices spanned by the segment's x-range.
     *
     * @tparam OtherSegment Type of the other segment.
     * @param other Segment to test.
     * @return `true` if every point of the segment lies on the chain.
     */
    template<SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool contains(const OtherSegment& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template<OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool contains(const OtherOrientedSegment& other) const;

    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     * @return `true` if the line is degenerate and its unique point lies on the chain.
     */
    template<LineConcept OtherLine>
    [[nodiscard]] constexpr bool contains(const OtherLine& other) const;

    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     * @return `true` if the oriented line is degenerate and its unique point lies on the chain.
     */
    template<OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool contains(const OtherOrientedLine& other) const;

    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     * @return `true` if the ray is degenerate and its unique point lies on the chain.
     */
    template<RayConcept OtherRay>
    [[nodiscard]] constexpr bool contains(const OtherRay& other) const;

    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     * @return `true` if the halfplane is degenerate and its unique point lies on the chain.
     */
    template<HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool contains(const OtherHalfplane& other) const;

    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     * @return `true` if the rectangle is degenerate and its diagonal is a sub-path of the chain.
     */
    template<RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool contains(const OtherRectangle& other) const;

    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     * @return `true` if the triangle is degenerate and its spanning segment is a sub-path of the chain.
     */
    template<TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr bool contains(const OtherTriangle& other) const;

    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     * @return `true` if the convex has at most two vertices and they span a sub-path of the chain.
     */
    template<ConvexConcept OtherConvex>
    [[nodiscard]] constexpr bool contains(const OtherConvex& other) const;

    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     *
     * A polygon lies on the 1-dimensional chain exactly when all of its edges
     * do (its interior is then empty), so this folds @ref contains over the
     * polygon's edges.
     */
    template<PolygonConcept OtherPolygon>
    [[nodiscard]] constexpr bool contains(const OtherPolygon& other) const;

    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     * @return `true` if all three boundary points of `other` are equal and the chain contains that point.
     */
    template<DiskConcept OtherDisk>
    [[nodiscard]] constexpr bool contains(const OtherDisk& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool contains(const EmptyShape<EmptyPoint>&) const {
        return true;
    }

    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     *
     * A chain contains another chain exactly when every edge of the other is a
     * straight sub-path of this chain (and its vertices for a degenerate
     * other).
     *
     * Complexity: O(m (log n + k)) for m vertices of the other chain.
     */
    template<MonotoneChainConcept OtherChain>
    [[nodiscard]] constexpr bool contains(const OtherChain& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool contains(const Shape<OtherPoint>& other) const;

    /**
     * @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B).
     *
     * The boundary of a chain is its two extreme vertices (matching the
     * endpoint convention of @ref Segment).
     *
     * Complexity: O(1).
     *
     * @tparam OtherPoint Type of the point.
     * @param point Point to test.
     * @return `true` if the point equals the first or the last vertex.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool boundaryContains(const OtherPoint& point) const;

    // The boundary of a chain is exactly its two extreme vertices, a finite
    // point set, so it contains no positive-length or two-dimensional shape.
    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool boundaryContains(const OtherSegment&) const { return false; }
    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool boundaryContains(const OtherOrientedSegment&) const { return false; }
    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<LineConcept OtherLine>
    [[nodiscard]] constexpr bool boundaryContains(const OtherLine&) const { return false; }
    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool boundaryContains(const OtherOrientedLine&) const { return false; }
    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<RayConcept OtherRay>
    [[nodiscard]] constexpr bool boundaryContains(const OtherRay&) const { return false; }
    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool boundaryContains(const OtherHalfplane&) const { return false; }
    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool boundaryContains(const OtherRectangle&) const { return false; }
    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr bool boundaryContains(const OtherTriangle&) const { return false; }
    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<ConvexConcept OtherConvex>
    [[nodiscard]] constexpr bool boundaryContains(const OtherConvex&) const { return false; }
    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<PolygonConcept OtherPolygon>
    [[nodiscard]] constexpr bool boundaryContains(const OtherPolygon&) const { return false; }
    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<DiskConcept OtherDisk>
    [[nodiscard]] constexpr bool boundaryContains(const OtherDisk&) const { return false; }
    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<MonotoneChainConcept OtherChain>
    [[nodiscard]] constexpr bool boundaryContains(const OtherChain& other) const {
        // The boundary is the two extreme vertices, so only a chain without an
        // edge fits inside it.
        return other.empty() || (other.size() == 1 && boundaryContains(other[0]));
    }
    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool boundaryContains(const EmptyShape<EmptyPoint>&) const {
        return true;
    }
    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool boundaryContains(const Shape<OtherPoint>& other) const;

    /**
     * @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B).
     *
     * The relative interior of a chain is the chain minus its two extreme
     * vertices.
     *
     * Complexity: O(log n) for n vertices.
     *
     * @tparam OtherPoint Type of the point.
     * @param point Point to test.
     * @return `true` if the point lies on the chain and is not an extreme vertex.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorContains(const OtherPoint& point) const;

    /**
     * @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B).
     *
     * Complexity: O(log n + k) for n vertices, where k is the number of chain
     * vertices spanned by the segment's x-range.
     *
     * @tparam OtherSegment Type of the other segment.
     * @param other Segment to test.
     * @return `true` if the chain contains the segment and neither endpoint is
     *         an extreme vertex of the chain.
     */
    template<SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool interiorContains(const OtherSegment& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template<OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool interiorContains(const OtherOrientedSegment& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template<LineConcept OtherLine>
    [[nodiscard]] constexpr bool interiorContains(const OtherLine& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template<OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool interiorContains(const OtherOrientedLine& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template<RayConcept OtherRay>
    [[nodiscard]] constexpr bool interiorContains(const OtherRay& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template<HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool interiorContains(const OtherHalfplane& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template<TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr bool interiorContains(const OtherTriangle& other) const;

    // A chain is one-dimensional: its relative interior cannot contain any
    // unbounded or two-dimensional shape.
    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template<RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool interiorContains(const OtherRectangle&) const { return false; }
    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template<ConvexConcept OtherConvex>
    [[nodiscard]] constexpr bool interiorContains(const OtherConvex&) const { return false; }
    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template<PolygonConcept OtherPolygon>
    [[nodiscard]] constexpr bool interiorContains(const OtherPolygon&) const { return false; }
    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template<DiskConcept OtherDisk>
    [[nodiscard]] constexpr bool interiorContains(const OtherDisk&) const { return false; }
    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template<MonotoneChainConcept OtherChain>
    [[nodiscard]] constexpr bool interiorContains(const OtherChain& other) const;
    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool interiorContains(const EmptyShape<EmptyPoint>&) const {
        return true;
    }
    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorContains(const Shape<OtherPoint>& other) const;

    /**
     * @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅).
     *
     * Complexity: O(log n) for n vertices.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool intersects(const OtherPoint& other) const;

    /**
     * @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅).
     *
     * Only the chain edges whose x-range meets the segment's x-range are
     * tested, located by binary search.
     *
     * Complexity: O(log n + k) for n vertices, where k is the number of chain
     * edges overlapping the segment's x-range (worst case O(n)).
     *
     * @tparam OtherSegment Type of the other segment.
     * @param other Segment to test.
     * @return `true` if the chain and the segment share at least one point.
     */
    template<SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool intersects(const OtherSegment& other) const;

    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template<OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool intersects(const OtherOrientedSegment& other) const;

    /**
     * @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅).
     *
     * Merge sweep over the two sorted vertex sequences: the pointers advance by
     * lexicographically smaller edge right endpoint, and only edge pairs whose
     * x-ranges overlap are tested.
     *
     * Complexity: O(n + m) for chains with n and m vertices.
     *
     * @tparam OtherChain Type of the other chain.
     * @param other Chain to test.
     * @return `true` if the chains share at least one point.
     */
    template<MonotoneChainConcept OtherChain>
    [[nodiscard]] constexpr bool intersects(const OtherChain& other) const;

    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template<LineConcept OtherLine>
    [[nodiscard]] constexpr bool intersects(const OtherLine& other) const;
    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template<OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool intersects(const OtherOrientedLine& other) const;
    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template<RayConcept OtherRay>
    [[nodiscard]] constexpr bool intersects(const OtherRay& other) const;
    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template<HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool intersects(const OtherHalfplane& other) const;
    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template<RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool intersects(const OtherRectangle& other) const;
    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template<TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr bool intersects(const OtherTriangle& other) const;
    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template<ConvexConcept OtherConvex>
    [[nodiscard]] constexpr bool intersects(const OtherConvex& other) const;
    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template<DiskConcept OtherDisk>
    [[nodiscard]] constexpr bool intersects(const OtherDisk& other) const;

    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool intersects(const EmptyShape<EmptyPoint>&) const {
        return false;
    }

    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool intersects(const Shape<OtherPoint>& other) const;

    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template<typename OtherShape>
        requires (!PointConcept<OtherShape> && detail::shapeRank<OtherShape> > detail::shapeRank<MonotoneChain>)
    [[nodiscard]] constexpr bool intersects(const OtherShape& other) const {
        return other.intersects(*this);
    }

    /**
     * @brief Tests whether the interiors of the shapes intersect (A° ∩ B° ≠ ∅).
     *
     * A point's interior is the point itself, so this matches @ref interiorContains.
     *
     * Complexity: O(log n) for n vertices.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherPoint& other) const;

    /**
     * @brief Tests whether the interiors of the shapes intersect (A° ∩ B° ≠ ∅).
     *
     * The chain's relative interior is the chain minus its extreme vertices,
     * so a chain vertex other than the extremes counts as interior: a segment
     * whose open part passes exactly through such a vertex engages this
     * predicate even though it crosses no open edge.
     *
     * Complexity: O(log n + k) for n vertices, where k is the number of chain
     * edges overlapping the segment's x-range (worst case O(n)).
     *
     * @tparam OtherSegment Type of the other segment.
     * @param other Segment to test.
     * @return `true` if the chain minus its extremes meets the open segment.
     */
    template<SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherSegment& other) const;

    /** @brief Tests whether the interiors of the shapes intersect (A° ∩ B° ≠ ∅). */
    template<OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherOrientedSegment& other) const;

    /** @brief Tests whether the interiors of the shapes intersect (A° ∩ B° ≠ ∅). */
    template<LineConcept OtherLine>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherLine& other) const;
    /** @brief Tests whether the interiors of the shapes intersect (A° ∩ B° ≠ ∅). */
    template<OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherOrientedLine& other) const;
    /** @brief Tests whether the interiors of the shapes intersect (A° ∩ B° ≠ ∅). */
    template<RayConcept OtherRay>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherRay& other) const;
    /** @brief Tests whether the interiors of the shapes intersect (A° ∩ B° ≠ ∅). */
    template<HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherHalfplane& other) const;
    /** @brief Tests whether the interiors of the shapes intersect (A° ∩ B° ≠ ∅). */
    template<RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherRectangle& other) const;
    /** @brief Tests whether the interiors of the shapes intersect (A° ∩ B° ≠ ∅). */
    template<TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherTriangle& other) const;
    /** @brief Tests whether the interiors of the shapes intersect (A° ∩ B° ≠ ∅). */
    template<ConvexConcept OtherConvex>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherConvex& other) const;
    /** @brief Tests whether the interiors of the shapes intersect (A° ∩ B° ≠ ∅). */
    template<DiskConcept OtherDisk>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherDisk& other) const;
    /**
     * @brief Tests whether the interiors of the shapes intersect (A° ∩ B° ≠ ∅).
     *
     * Merge sweep over the open edge pairs plus the crossing-at-a-non-extreme-
     * vertex checks in both directions.
     *
     * Complexity: O(n + m log n) for chains with n and m vertices.
     */
    template<MonotoneChainConcept OtherChain>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherChain& other) const;

    /** @brief Tests whether the interiors of the shapes intersect (A° ∩ B° ≠ ∅). */
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool interiorsIntersect(const EmptyShape<EmptyPoint>&) const {
        return false;
    }

    /** @brief Tests whether the interiors of the shapes intersect (A° ∩ B° ≠ ∅). */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorsIntersect(const Shape<OtherPoint>& other) const;

    /** @brief Tests whether the interiors of the shapes intersect (A° ∩ B° ≠ ∅). */
    template<typename OtherShape>
        requires (!PointConcept<OtherShape> && detail::shapeRank<OtherShape> > detail::shapeRank<MonotoneChain>)
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherShape& other) const {
        return other.interiorsIntersect(*this);
    }

    /**
     * @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected).
     *
     * Removing anything from a single point never disconnects it.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool separates(const OtherPoint&) const {
        return false;
    }
    /**
     * @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected).
     *
     * True when some connected component of the intersection with the segment
     * avoids both segment endpoints. Exact and division-free: the component
     * walk only uses edge intersection and containment predicates.
     *
     * Complexity: O(n) for n vertices.
     */
    template<SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool separates(const OtherSegment& other) const;
    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template<OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool separates(const OtherOrientedSegment& other) const;
    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template<LineConcept OtherLine>
    [[nodiscard]] constexpr bool separates(const OtherLine& other) const;
    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template<OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool separates(const OtherOrientedLine& other) const;
    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template<RayConcept OtherRay>
    [[nodiscard]] constexpr bool separates(const OtherRay& other) const;

    // --- 2-dimensional targets: the crosscut scan of separatesTwoDimensional
    // (an edge disconnects the region by itself, or the chain runs
    // outside-interior-outside) ---
    /**
     * @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected).
     *
     * No straight edge can disconnect a halfplane, but a chain bending
     * through the interior between two boundary contacts seals off a pocket
     * against the boundary line.
     */
    template<HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool separates(const OtherHalfplane& other) const;
    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template<RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool separates(const OtherRectangle& other) const;
    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template<TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr bool separates(const OtherTriangle& other) const;
    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template<DiskConcept OtherDisk>
    [[nodiscard]] constexpr bool separates(const OtherDisk& other) const;
    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template<ConvexConcept OtherConvex>
    [[nodiscard]] constexpr bool separates(const OtherConvex& other) const;
    /**
     * @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected).
     *
     * The polygon may be non-convex, so the scan also spots edges that leave
     * the interior between two interior vertices (see
     * @ref separatesTwoDimensional).
     */
    template<PolygonConcept OtherPolygon>
    [[nodiscard]] constexpr bool separates(const OtherPolygon& other) const;
    /**
     * @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected).
     *
     * Both chains are arcs whose arc order matches lexicographic order, so
     * removing this cuts the other exactly when the other has ordered points
     * a < b < c with b on this and a, c off it (an edge carrying all three is
     * a separated edge; otherwise a and c straddle a covered vertex or edge).
     *
     * Complexity: O(n + m) for n and m vertices.
     */
    template<MonotoneChainConcept OtherChain>
    [[nodiscard]] constexpr bool separates(const OtherChain& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template<PolylineConcept OtherPolyline>
    [[nodiscard]] constexpr bool contains(const OtherPolyline& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<PolylineConcept OtherPolyline>
    [[nodiscard]] constexpr bool boundaryContains(const OtherPolyline& other) const {
        // The boundary is the two extreme vertices, so only a polyline
        // covering at most one point fits inside it.
        return other.empty() || (other.isDegenerate() && boundaryContains(other[0]));
    }

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template<PolylineConcept OtherPolyline>
    [[nodiscard]] constexpr bool interiorContains(const OtherPolyline& other) const;

    /**
     * @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected).
     *
     * Set semantics: the polyline's free pieces may reconnect through its own
     * self-intersections (see `detail::separates1DSet`).
     */
    template<PolylineConcept OtherPolyline>
    [[nodiscard]] constexpr bool separates(const OtherPolyline& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template<HalfplaneIntersectionConcept OtherRegion>
    [[nodiscard]] constexpr bool contains(const OtherRegion& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<HalfplaneIntersectionConcept OtherRegion>
    [[nodiscard]] constexpr bool boundaryContains(const OtherRegion& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template<HalfplaneIntersectionConcept OtherRegion>
    [[nodiscard]] constexpr bool interiorContains(const OtherRegion& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template<HalfplaneIntersectionConcept OtherRegion>
    [[nodiscard]] constexpr bool separates(const OtherRegion& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool separates(const EmptyShape<EmptyPoint>&) const {
        return false;
    }
    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool separates(const Shape<OtherPoint>& other) const;

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool crosses(const OtherPoint&) const {
        return false;
    }
    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template<SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool crosses(const OtherSegment& other) const;
    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template<OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool crosses(const OtherOrientedSegment& other) const;
    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template<LineConcept OtherLine>
    [[nodiscard]] constexpr bool crosses(const OtherLine& other) const;
    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template<OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool crosses(const OtherOrientedLine& other) const;
    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template<RayConcept OtherRay>
    [[nodiscard]] constexpr bool crosses(const OtherRay& other) const;
    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template<HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool crosses(const OtherHalfplane& other) const;
    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template<RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool crosses(const OtherRectangle& other) const;
    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template<TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr bool crosses(const OtherTriangle& other) const;
    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template<DiskConcept OtherDisk>
    [[nodiscard]] constexpr bool crosses(const OtherDisk& other) const;
    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template<ConvexConcept OtherConvex>
    [[nodiscard]] constexpr bool crosses(const OtherConvex& other) const;
    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template<MonotoneChainConcept OtherChain>
    [[nodiscard]] constexpr bool crosses(const OtherChain& other) const;
    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool crosses(const EmptyShape<EmptyPoint>&) const {
        return false;
    }
    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool crosses(const Shape<OtherPoint>& other) const;
    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template<typename OtherShape>
        requires (!PointConcept<OtherShape> && detail::shapeRank<OtherShape> > detail::shapeRank<MonotoneChain>)
    [[nodiscard]] constexpr bool crosses(const OtherShape& other) const {
        return other.crosses(*this);
    }

    /**
     * @brief Tests whether the two chains have edges that cross.
     *
     * True iff some edge of this chain and some edge of @p other cross:
     * their interiors meet at a single point.
     *
     * Both edge sequences are sorted by x-interval, so the proper-crossing pair
     * is found by a merge sweep in O(n + m) for chains with n and m vertices.
     *
     * @tparam OtherChain Type of the other chain.
     * @param other The other chain.
     * @return `true` if an edge of this chain crosses an edge of @p other.
     */
    template<MonotoneChainConcept OtherChain>
    [[nodiscard]] constexpr bool edgesCross(const OtherChain& other) const;

    /** @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint. */
    template <class ResultNumber = NumberType, PointConcept OtherPoint>
    [[nodiscard]] constexpr std::optional<Point<ResultNumber, typename PointType::LabelType>>
    intersection(const OtherPoint& other) const;

    /**
     * @brief Returns the intersection with a one-dimensional or convex shape
     * (A ∩ B), a sequence of points and segments sorted by lexicographic order.
     *
     * Folds the shape over the chain edges, delegating each edge to the
     * segment-vs-shape intersection, then coalesces the pieces like
     * @ref intersection(const OtherChain&) const.
     *
     * @tparam ResultNumber Number type of the returned coordinates.
     * @return Vector of points and segments forming the intersection.
     * @warning Divides coordinates after casting to ResultNumber.
     */
    template <class ResultNumber = NumberType, SegmentConcept OtherSegment>
    [[nodiscard]] constexpr std::vector<std::variant<Point<ResultNumber, typename PointType::LabelType>,
                                                     Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherSegment& other) const;
    /** @copydoc intersection(const OtherSegment&) const */
    template <class ResultNumber = NumberType, OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr std::vector<std::variant<Point<ResultNumber, typename PointType::LabelType>,
                                                     Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherOrientedSegment& other) const;
    /** @copydoc intersection(const OtherSegment&) const */
    template <class ResultNumber = NumberType, LineConcept OtherLine>
    [[nodiscard]] constexpr std::vector<std::variant<Point<ResultNumber, typename PointType::LabelType>,
                                                     Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherLine& other) const;
    /** @copydoc intersection(const OtherSegment&) const */
    template <class ResultNumber = NumberType, OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr std::vector<std::variant<Point<ResultNumber, typename PointType::LabelType>,
                                                     Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherOrientedLine& other) const;
    /** @copydoc intersection(const OtherSegment&) const */
    template <class ResultNumber = NumberType, RayConcept OtherRay>
    [[nodiscard]] constexpr std::vector<std::variant<Point<ResultNumber, typename PointType::LabelType>,
                                                     Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherRay& other) const;
    /** @copydoc intersection(const OtherSegment&) const */
    template <class ResultNumber = NumberType, HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr std::vector<std::variant<Point<ResultNumber, typename PointType::LabelType>,
                                                     Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherHalfplane& other) const;
    /** @copydoc intersection(const OtherSegment&) const */
    template <class ResultNumber = NumberType, RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr std::vector<std::variant<Point<ResultNumber, typename PointType::LabelType>,
                                                     Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherRectangle& other) const;
    /** @copydoc intersection(const OtherSegment&) const */
    template <class ResultNumber = NumberType, TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr std::vector<std::variant<Point<ResultNumber, typename PointType::LabelType>,
                                                     Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherTriangle& other) const;
    /** @copydoc intersection(const OtherSegment&) const */
    template <class ResultNumber = NumberType, ConvexConcept OtherConvex>
    [[nodiscard]] constexpr std::vector<std::variant<Point<ResultNumber, typename PointType::LabelType>,
                                                     Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherConvex& other) const;

    /** @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint. @warning Divides coordinates after casting to ResultNumber. */
    template <class ResultNumber = NumberType, typename OtherShape>
        requires (!PointConcept<OtherShape>
                  && (detail::shapeRank<OtherShape> > detail::shapeRank<MonotoneChain>)
                  && requires(const OtherShape& o, const MonotoneChain& self) {
                         o.template intersection<ResultNumber>(self);
                     })
    [[nodiscard]] constexpr auto intersection(const OtherShape& other) const {
        return other.template intersection<ResultNumber>(*this);
    }

    /**
     * @brief Returns the intersection of the two chains (A ∩ B), a sequence of
     * points and segments sorted by lexicographic order.
     *
     * Two chains can overlap along collinear sub-segments, so the result is a
     * vector of point-or-segment variants. Pieces are maximal: adjacent
     * collinear overlaps are coalesced into single segments, and points
     * covered by a reported segment are dropped. Computed by the same merge
     * sweep as @ref intersects(const OtherChain&).
     *
     * Complexity: O(n + m) intersection tests for chains with n and m
     * vertices, plus sorting the resulting pieces.
     *
     * @tparam ResultNumber Number type of the returned coordinates.
     * @tparam OtherChain Type of the other chain.
     * @param other Chain to intersect with.
     * @return Vector of points and segments forming the intersection.
     * @warning Divides coordinates after casting to ResultNumber.
     */
    template <class ResultNumber = NumberType, MonotoneChainConcept OtherChain>
    [[nodiscard]] constexpr std::vector<std::variant<Point<ResultNumber, typename PointType::LabelType>,
                                                     Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherChain& other) const;

    /**
     * @brief Returns the squared Euclidean distance to the given shape.
     *
     * Zero when the shapes intersect, otherwise the minimum over the chain
     * edges. The chain must have at least one edge.
     *
     * Complexity: O(n) edge queries for n vertices, plus the intersection test.
     *
     * @tparam ResultNumber Coordinate type of the returned distance (default: NumberType).
     *
     * @warning With an integer @p ResultNumber the exact squared distance is
     *          generally a fraction, so the internal division truncates and the
     *          result is inexact. Request a floating-point or pgl::Rational
     *          result type, e.g. `squaredDistance<double>(point)`, for an
     *          accurate value.
     */
    template <class ResultNumber = NumberType, PointConcept OtherPoint>
    [[nodiscard]] constexpr auto squaredDistance(const OtherPoint& point) const;
    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, SegmentConcept OtherSegment>
    [[nodiscard]] constexpr auto squaredDistance(const OtherSegment& other) const;
    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr auto squaredDistance(const OtherOrientedSegment& other) const;
    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, LineConcept OtherLine>
    [[nodiscard]] constexpr auto squaredDistance(const OtherLine& other) const;
    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr auto squaredDistance(const OtherOrientedLine& other) const;
    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, RayConcept OtherRay>
    [[nodiscard]] constexpr auto squaredDistance(const OtherRay& other) const;
    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr auto squaredDistance(const OtherHalfplane& other) const;
    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr auto squaredDistance(const OtherRectangle& other) const;
    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr auto squaredDistance(const OtherTriangle& other) const;
    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, ConvexConcept OtherConvex>
    [[nodiscard]] constexpr auto squaredDistance(const OtherConvex& other) const;
    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, MonotoneChainConcept OtherChain>
    [[nodiscard]] constexpr auto squaredDistance(const OtherChain& other) const;

    /**
     * @brief Returns the squared Euclidean distance to a disk.
     *
     * Forwards to @ref Disk::squaredDistance's model: a disk's exterior
     * distance is irrational, so it always returns `double`.
     */
    template <class DiskPointType, class DiskLabel>
    [[nodiscard]] double squaredDistance(const Disk<DiskPointType, DiskLabel>& disk) const;

    /**
     * @brief Returns the squared Euclidean distance to the given shape.
     *
     * Forwards to the other shape's implementation so that each unordered pair
     * needs `squaredDistance` defined only once, on the higher-ranked shape.
     */
    template <class ResultNumber = NumberType, typename OtherShape>
        requires ((detail::shapeRank<OtherShape> > detail::shapeRank<MonotoneChain>)
                  && requires(const OtherShape& o, const MonotoneChain& self) {
                         o.template squaredDistance<ResultNumber>(self);
                     })
    [[nodiscard]] constexpr auto squaredDistance(const OtherShape& other) const {
        return other.template squaredDistance<ResultNumber>(*this);
    }

    /**
     * @brief Returns the Manhattan (L1) distance to the given shape.
     *
     * Zero when the shapes intersect, otherwise the minimum over the chain
     * edges. The chain must have at least one edge.
     *
     * @warning With an integer @p ResultNumber the exact distance is generally
     *          a fraction, so the internal division truncates. Request a
     *          floating-point or pgl::Rational result type for an accurate value.
     */
    template <class ResultNumber = NumberType, PointConcept OtherPoint>
    [[nodiscard]] constexpr auto distanceL1(const OtherPoint& point) const;
    /** @copydoc distanceL1(const OtherPoint&) const */
    template <class ResultNumber = NumberType, SegmentConcept OtherSegment>
    [[nodiscard]] constexpr auto distanceL1(const OtherSegment& other) const;
    /** @copydoc distanceL1(const OtherPoint&) const */
    template <class ResultNumber = NumberType, OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr auto distanceL1(const OtherOrientedSegment& other) const;
    /** @copydoc distanceL1(const OtherPoint&) const */
    template <class ResultNumber = NumberType, LineConcept OtherLine>
    [[nodiscard]] constexpr auto distanceL1(const OtherLine& other) const;
    /** @copydoc distanceL1(const OtherPoint&) const */
    template <class ResultNumber = NumberType, OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr auto distanceL1(const OtherOrientedLine& other) const;
    /** @copydoc distanceL1(const OtherPoint&) const */
    template <class ResultNumber = NumberType, RayConcept OtherRay>
    [[nodiscard]] constexpr auto distanceL1(const OtherRay& other) const;
    /** @copydoc distanceL1(const OtherPoint&) const */
    template <class ResultNumber = NumberType, HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr auto distanceL1(const OtherHalfplane& other) const;
    /** @copydoc distanceL1(const OtherPoint&) const */
    template <class ResultNumber = NumberType, RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr auto distanceL1(const OtherRectangle& other) const;
    /** @copydoc distanceL1(const OtherPoint&) const */
    template <class ResultNumber = NumberType, TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr auto distanceL1(const OtherTriangle& other) const;
    /** @copydoc distanceL1(const OtherPoint&) const */
    template <class ResultNumber = NumberType, ConvexConcept OtherConvex>
    [[nodiscard]] constexpr auto distanceL1(const OtherConvex& other) const;
    /** @copydoc distanceL1(const OtherPoint&) const */
    template <class ResultNumber = NumberType, MonotoneChainConcept OtherChain>
    [[nodiscard]] constexpr auto distanceL1(const OtherChain& other) const;

    /**
     * @brief Returns the Manhattan (L1) distance to the given shape.
     *
     * Forwards to the other shape's implementation so that each unordered pair
     * needs `distanceL1` defined only once, on the higher-ranked shape.
     */
    template <class ResultNumber = NumberType, typename OtherShape>
        requires ((detail::shapeRank<OtherShape> > detail::shapeRank<MonotoneChain>)
                  && requires(const OtherShape& o, const MonotoneChain& self) {
                         o.template distanceL1<ResultNumber>(self);
                     })
    [[nodiscard]] constexpr auto distanceL1(const OtherShape& other) const {
        return other.template distanceL1<ResultNumber>(*this);
    }

    /**
     * @brief Returns the distance to the given shape, using symmetry to
     * re-dispatch through the wrapper's own `distanceL1`.
     */
    template <class ResultNumber = NumberType, PointConcept OtherPoint>
    [[nodiscard]] constexpr auto distanceL1(const Shape<OtherPoint>& other) const {
        return other.template distanceL1<ResultNumber>(*this);
    }

    /**
     * @brief Returns the Chebyshev (LInf) distance to the given shape.
     *
     * Zero when the shapes intersect, otherwise the minimum over the chain
     * edges. The chain must have at least one edge.
     *
     * @warning With an integer @p ResultNumber the exact distance is generally
     *          a fraction, so the internal division truncates. Request a
     *          floating-point or pgl::Rational result type for an accurate value.
     */
    template <class ResultNumber = NumberType, PointConcept OtherPoint>
    [[nodiscard]] constexpr auto distanceLInf(const OtherPoint& point) const;
    /** @copydoc distanceLInf(const OtherPoint&) const */
    template <class ResultNumber = NumberType, SegmentConcept OtherSegment>
    [[nodiscard]] constexpr auto distanceLInf(const OtherSegment& other) const;
    /** @copydoc distanceLInf(const OtherPoint&) const */
    template <class ResultNumber = NumberType, OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr auto distanceLInf(const OtherOrientedSegment& other) const;
    /** @copydoc distanceLInf(const OtherPoint&) const */
    template <class ResultNumber = NumberType, LineConcept OtherLine>
    [[nodiscard]] constexpr auto distanceLInf(const OtherLine& other) const;
    /** @copydoc distanceLInf(const OtherPoint&) const */
    template <class ResultNumber = NumberType, OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr auto distanceLInf(const OtherOrientedLine& other) const;
    /** @copydoc distanceLInf(const OtherPoint&) const */
    template <class ResultNumber = NumberType, RayConcept OtherRay>
    [[nodiscard]] constexpr auto distanceLInf(const OtherRay& other) const;
    /** @copydoc distanceLInf(const OtherPoint&) const */
    template <class ResultNumber = NumberType, HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr auto distanceLInf(const OtherHalfplane& other) const;
    /** @copydoc distanceLInf(const OtherPoint&) const */
    template <class ResultNumber = NumberType, RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr auto distanceLInf(const OtherRectangle& other) const;
    /** @copydoc distanceLInf(const OtherPoint&) const */
    template <class ResultNumber = NumberType, TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr auto distanceLInf(const OtherTriangle& other) const;
    /** @copydoc distanceLInf(const OtherPoint&) const */
    template <class ResultNumber = NumberType, ConvexConcept OtherConvex>
    [[nodiscard]] constexpr auto distanceLInf(const OtherConvex& other) const;
    /** @copydoc distanceLInf(const OtherPoint&) const */
    template <class ResultNumber = NumberType, MonotoneChainConcept OtherChain>
    [[nodiscard]] constexpr auto distanceLInf(const OtherChain& other) const;

    /**
     * @brief Returns the Chebyshev (LInf) distance to the given shape.
     *
     * Forwards to the other shape's implementation so that each unordered pair
     * needs `distanceLInf` defined only once, on the higher-ranked shape.
     */
    template <class ResultNumber = NumberType, typename OtherShape>
        requires ((detail::shapeRank<OtherShape> > detail::shapeRank<MonotoneChain>)
                  && requires(const OtherShape& o, const MonotoneChain& self) {
                         o.template distanceLInf<ResultNumber>(self);
                     })
    [[nodiscard]] constexpr auto distanceLInf(const OtherShape& other) const {
        return other.template distanceLInf<ResultNumber>(*this);
    }

    /**
     * @brief Returns the distance to the given shape, using symmetry to
     * re-dispatch through the wrapper's own `distanceLInf`.
     */
    template <class ResultNumber = NumberType, PointConcept OtherPoint>
    [[nodiscard]] constexpr auto distanceLInf(const Shape<OtherPoint>& other) const {
        return other.template distanceLInf<ResultNumber>(*this);
    }

    /**
     * @brief Computes the Euclidean length of the chain (the sum of its edge lengths).
     * @tparam ApproximateNumber The floating-point type for the result.
     */
    template <class ApproximateNumber = double>
    ApproximateNumber length() const;

    /** @brief Computes the Manhattan (L1) length of the chain. */
    constexpr auto lengthL1() const;

    /** @brief Computes the Chebyshev (LInf) length of the chain. */
    constexpr auto lengthLInf() const;

    /**
     * @brief Returns a point inside the chain.
     *
     * This is the point inside the segment formed by the first two
     * vertices, i.e. the midpoint of that edge.
     *
     * @tparam ResultNumber Coordinate type of the result.
     * @warning Divides coordinates by 2. Inexact for odd integer coordinates.
     */
    template <class ResultNumber = NumberType>
    [[nodiscard]] constexpr Point<ResultNumber> pointInside() const;

    /**
     * @brief Tests whether some point in this shape's relative interior lies in
     *        the strict interior of @p shape.
     *
     * Uses @ref pointInside as the witness. When integer truncation rounds that
     * witness onto or outside the boundary, this shape and @p shape are scaled
     * so the witness is exact, leaving the containment relation unchanged.
     */
    template <class OtherShape>
    [[nodiscard]] constexpr bool pointInsideInteriorContainedIn(const OtherShape& shape) const;

    /**
     * @brief Returns the chain rotated by 90k degrees around the origin.
     *
     * An odd number of rotations turns an x-monotone chain into a y-monotone
     * one, so the result is renormalized: it is the canonical chain on the
     * rotated *point set*, which generally links the vertices in a different
     * order than the source chain.
     *
     * @param k Number of 90-degree CCW rotations (may be negative).
     * @return Rotated chain.
     */
    [[nodiscard]] constexpr OwningChain rotated90(int k = 1) const;

    /**
     * @brief Rotates the chain by 90k degrees around the origin in place.
     *
     * @param k Number of 90-degree CCW rotations (may be negative).
     */
    constexpr void rotate90(int k = 1)
        requires detail::ownsChainStorage<Storage, PointType>;

    /** @brief Returns the chain with its x-coordinates multiplied by a factor. */
    template <class OtherNumber>
    [[nodiscard]] constexpr OwningChain scaledUpX(const OtherNumber scalar) const;

    /** @brief Multiplies the chain's x-coordinates by a factor in place. */
    template <class OtherNumber>
    constexpr void scaleUpX(const OtherNumber scalar)
        requires detail::ownsChainStorage<Storage, PointType>;

    /** @brief Returns the chain with its y-coordinates multiplied by a factor. */
    template <class OtherNumber>
    [[nodiscard]] constexpr OwningChain scaledUpY(const OtherNumber scalar) const;

    /** @brief Multiplies the chain's y-coordinates by a factor in place. */
    template <class OtherNumber>
    constexpr void scaleUpY(const OtherNumber scalar)
        requires detail::ownsChainStorage<Storage, PointType>;

    /** @brief Returns the chain with its x-coordinates divided by a divisor. */
    template <class OtherNumber>
    [[nodiscard]] constexpr OwningChain scaledDownX(const OtherNumber scalar) const;

    /** @brief Divides the chain's x-coordinates by a divisor in place. */
    template <class OtherNumber>
    constexpr void scaleDownX(const OtherNumber scalar)
        requires detail::ownsChainStorage<Storage, PointType>;

    /** @brief Returns the chain with its y-coordinates divided by a divisor. */
    template <class OtherNumber>
    [[nodiscard]] constexpr OwningChain scaledDownY(const OtherNumber scalar) const;

    /** @brief Divides the chain's y-coordinates by a divisor in place. */
    template <class OtherNumber>
    constexpr void scaleDownY(const OtherNumber scalar)
        requires detail::ownsChainStorage<Storage, PointType>;

    /**
     * @brief Translates the chain by the given point.
     *
     * Complexity: O(1).
     */
    template<PointConcept OtherPoint>
    constexpr MonotoneChain& operator+=(const OtherPoint& translation) {
        translation_ += translation;
        // A pure translation merely shifts the bounding box, so update the
        // cached bbox in place rather than discarding it. The hash, however,
        // depends on the absolute vertex positions, so it must be invalidated.
        if (bbox_) {
            *bbox_ += translation;
        }
        hash_ = hashUnset_;
        return *this;
    }

    /**
     * @brief Translates the chain by the negation of the given point.
     *
     * Complexity: O(1).
     */
    template<PointConcept OtherPoint>
    constexpr MonotoneChain& operator-=(const OtherPoint& translation) {
        translation_ -= translation;
        if (bbox_) {
            *bbox_ -= translation;
        }
        hash_ = hashUnset_;
        return *this;
    }

    /**
     * @brief Scales the chain by the given scalar.
     *
     * Complexity: O(n log n) for n vertices. Scaling by a negative factor
     * reverses the lexicographic order (and by zero collapses the chain to a
     * point), so the chain is renormalized to stay canonical.
     */
    template <class Scalar>
        requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar> &&
                 detail::ownsChainStorage<Storage, PointType>)
    constexpr MonotoneChain& operator*=(const Scalar& scalar) {
        for (auto& vertex : points_) {
            vertex *= scalar;
        }
        translation_ *= scalar;
        normalize();
        resetCache();
        return *this;
    }

    /**
     * @brief Divides the chain by the given scalar.
     *
     * Complexity: O(n log n) for n vertices; renormalizes like @ref operator*=.
     */
    template <class Scalar>
        requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar> &&
                 detail::ownsChainStorage<Storage, PointType>)
    constexpr MonotoneChain& operator/=(const Scalar& scalar) {
        for (auto& vertex : points_) {
            vertex /= scalar;
        }
        translation_ /= scalar;
        normalize();
        resetCache();
        return *this;
    }

    /**
     * @brief Forward iterator over the (optionally oriented) chain edges.
     *
     * Edge `i` joins vertex `i` to vertex `i + 1`, so a chain with `size()`
     * vertices has `size() - 1` edges (and none when degenerate).
     */
    template <bool Oriented>
    class BoundaryIterator {
      public:
        using iterator_category = std::forward_iterator_tag;
        using iterator_concept = std::forward_iterator_tag;
        using value_type = BoundaryType<Oriented>;
        using difference_type = std::ptrdiff_t;
        using reference = value_type;

        constexpr BoundaryIterator() = default;

        constexpr value_type operator*() const {
            assert(chain != nullptr);
            return chain->template boundaryAt<Oriented>(index);
        }

        constexpr BoundaryIterator& operator++() {
            ++index;
            return *this;
        }

        constexpr BoundaryIterator operator++(int) {
            BoundaryIterator copy(*this);
            ++(*this);
            return copy;
        }

        constexpr bool operator==(const BoundaryIterator& other) const = default;

      private:
        friend struct MonotoneChain;

        constexpr BoundaryIterator(const MonotoneChain* chain_arg, std::size_t index_arg)
            : chain(chain_arg), index(index_arg) {}

        const MonotoneChain* chain = nullptr;
        std::size_t index = 0;
    };

  private:
    Storage points_{};
    [[no_unique_address]] mutable LabelType label_{};
    PointType translation_{};
    // Lazily computed bounding box, invalidated by resetCache() on every
    // mutation. Empty until first computed.
    mutable std::optional<Rectangle<PointType>> bbox_{};

    // Memoized hash, computed lazily by std::hash<MonotoneChain>. hashUnset_
    // means "not yet computed"; SIZE_MAX is chosen as the sentinel because it
    // is a rare hash output, and the one true hash that would collide with it
    // is remapped to hashUnset_ - 1 so the sentinel is never stored as a real
    // value. Unlike the bbox, the hash is not translation-invariant, so
    // operator+=/-= reset it.
    static constexpr std::size_t hashUnset_ = pgl::detail::numeric_limits<std::size_t>::max();
    mutable std::size_t hash_ = hashUnset_;
    friend struct std::hash<MonotoneChain>;

    // Drops the memoized caches; call after any operation that mutates the
    // chain's vertices. A pure translation does not need to drop bbox_ (it
    // shifts in place, see operator+=), but it must still reset hash_, which
    // depends on the absolute vertex positions.
    constexpr void resetCache() const {
        bbox_ = {};
        hash_ = hashUnset_;
    }

    constexpr std::size_t edgeCount() const {
        return points_.empty() ? 0 : points_.size() - 1;
    }

    /**
     * @brief Locates the inclusive range of edge indices whose x-range can meet
     * `[xlo, xhi]`.
     *
     * Empty when the chain has no edge or the x-windows are disjoint. The range
     * is conservative by at most one edge on the left, which keeps the
     * touch-at-a-shared-vertex cases inside the window.
     *
     * Complexity: O(log n) for n vertices.
     */
    template <class LowNumber, class HighNumber>
    [[nodiscard]] constexpr std::optional<std::pair<std::size_t, std::size_t>>
    edgeWindow(const LowNumber& xlo, const HighNumber& xhi) const;

    /**
     * @brief Minimum of the given distance over the chain edges.
     *
     * The distance to a shape the chain does not intersect is attained on some
     * edge, so the fold is exact. Requires at least one edge; the caller
     * handles the intersecting (distance zero) case first. Requires
     * `Segment::squaredDistance(OtherShape)` (directly or via the segment's
     * forwarder).
     */
    template <class ResultNumber, class OtherShape>
    constexpr ResultNumber edgeMinSquaredDistance(const OtherShape& other) const;

    /** @copydoc edgeMinSquaredDistance */
    template <class ResultNumber, class OtherShape>
    constexpr ResultNumber edgeMinDistanceL1(const OtherShape& other) const;

    /** @copydoc edgeMinSquaredDistance */
    template <class ResultNumber, class OtherShape>
    constexpr ResultNumber edgeMinDistanceLInf(const OtherShape& other) const;

    /**
     * @brief Folds a shape's segment intersection over the chain edges and
     * coalesces the pieces (shared by the non-chain `intersection` overloads).
     */
    template <class ResultNumber, class OtherShape>
    constexpr std::vector<std::variant<Point<ResultNumber, typename PointType::LabelType>,
                                       Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    edgeFoldIntersection(const OtherShape& other) const;

    /**
     * @brief Sorts raw intersection pieces by lexicographic minimum and
     * coalesces them: duplicate touch points are dropped and collinear
     * touching segments merge (shared by every piecewise `intersection`).
     */
    template <class ResultNumber>
    static constexpr std::vector<std::variant<Point<ResultNumber, typename PointType::LabelType>,
                                              Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    coalescePieces(std::vector<std::variant<Point<ResultNumber, typename PointType::LabelType>,
                                            Segment<Point<ResultNumber, typename PointType::LabelType>>>> pieces);

    /**
     * @brief Component walk shared by the 1-dimensional `separates` overloads.
     *
     * Walks the chain edges tracking the connected components of the
     * intersection with @p other (consecutive edge pieces connect through a
     * shared vertex on @p other); returns true as soon as a component avoids
     * every boundary point of @p other, reported by @p touchesBoundary.
     */
    template <class OtherShape, class TouchesBoundary>
    constexpr bool separatesOneDimensional(const OtherShape& other, TouchesBoundary touchesBoundary) const;

    /**
     * @brief Crosscut scan shared by the 2-dimensional `separates` overloads.
     *
     * Returns true iff removing the chain disconnects the region @p other:
     * some edge disconnects it by itself, or the chain runs
     * outside-interior-outside (scanned at the vertices; @p OtherIsConvex set
     * to false additionally spots edges that leave the interior between two
     * interior vertices, which only a non-convex region allows).
     */
    template <bool OtherIsConvex = true, class OtherShape>
    constexpr bool separatesTwoDimensional(const OtherShape& other) const;

    template <bool Oriented>
    constexpr BoundaryType<Oriented> boundaryAt(std::size_t index) const {
        assert(index + 1 < size());
        return BoundaryType<Oriented>((*this)[index], (*this)[index + 1]);
    }

    /**
     * @brief Brings the stored vertices to canonical form: sorted
     * lexicographically with duplicates removed.
     */
    constexpr void normalize() {
        std::sort(points_.begin(), points_.end());
        points_.erase(std::unique(points_.begin(), points_.end()), points_.end());
    }

    class Iterator {
    private:
        using BaseIterator = std::ranges::iterator_t<const Storage>;
        BaseIterator it;
        PointType x;

    public:
        using iterator_category = std::random_access_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = PointType;
        using pointer = PointType*;
        using reference = PointType&;

        Iterator() = default;
        Iterator(BaseIterator it, PointType x) : it(it), x(x) {}

        // Dereference returns value + x
        PointType operator*() const {
            return *it + x;
        }

        // Pre-increment
        Iterator& operator++() {
            ++it;
            return *this;
        }

        // Post-increment
        Iterator operator++(int) {
            Iterator tmp = *this;
            ++it;
            return tmp;
        }

        // Pre-decrement
        Iterator& operator--() {
            --it;
            return *this;
        }

        // Post-decrement
        Iterator operator--(int) {
            Iterator tmp = *this;
            --it;
            return tmp;
        }

        // Equality comparison
        bool operator==(const Iterator& other) const {
            return it == other.it;
        }

        // Other comparisons
        std::strong_ordering operator<=>(const Iterator& other) const {
            if (it < other.it) {
                return std::strong_ordering::less;
            }
            if (it > other.it) {
                return std::strong_ordering::greater;
            }
            return std::strong_ordering::equal;
        }

        // Addition
        Iterator operator+(difference_type n) const {
            return Iterator(it + n, x);
        }

        // Subtraction
        Iterator operator-(difference_type n) const {
            return Iterator(it - n, x);
        }

        // Difference
        difference_type operator-(const Iterator& other) const {
            return it - other.it;
        }

        // Array subscript operator
        PointType operator[](difference_type n) const {
            return *(it + n) + x;
        }
    };
}; // struct MonotoneChain

/**
 * @brief A non-owning @ref MonotoneChain that views an external, already
 * canonical (sorted, duplicate-free) contiguous range of vertices.
 *
 * `MonotoneChainView` shares every read-only operation with the owning chain
 * — it is just @ref MonotoneChain instantiated over a `std::span` — but the
 * caller owns the underlying storage and is responsible for keeping it alive
 * and canonical for the view's lifetime. Mutating operations (insert, in-place
 * scaling/rotation, `operator*=`, …) are not available; transformations that
 * return a chain yield an owning @ref MonotoneChain instead.
 */
template <class PointType = Point<>, class Label = NoLabel>
using MonotoneChainView = MonotoneChain<PointType, Label, std::span<const PointType>>;

template <class PointType, class LabelType, class Storage, class TranslationNumber, class TranslationLabel>
constexpr auto operator+(const MonotoneChain<PointType, LabelType, Storage>& chain, const Point<TranslationNumber, TranslationLabel>& translation) {
    return translation + chain;
}

template <class TranslationNumber, class TranslationLabel, class PointType, class LabelType, class Storage>
constexpr auto operator+(const Point<TranslationNumber, TranslationLabel>& translation, const MonotoneChain<PointType, LabelType, Storage>& chain) {
    using ResultPointType = Point<TranslationNumber, typename PointType::LabelType>;
    MonotoneChain<ResultPointType, LabelType> result(chain);
    result += translation;
    if constexpr (detail::has_label_v<LabelType>) {
        result.label() = LabelType{};
    }
    return result;
}

template <class PointType, class LabelType, class Storage, class TranslationNumber, class TranslationLabel>
constexpr auto operator-(const MonotoneChain<PointType, LabelType, Storage>& chain, const Point<TranslationNumber, TranslationLabel>& translation) {
    return chain + (-translation);
}

template <class PointType, class LabelType, class Storage, class Scalar>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr auto operator*(const MonotoneChain<PointType, LabelType, Storage>& chain, const Scalar& scalar) {
    using ResultPointType = Point<decltype(std::declval<PointType>().x() * scalar), typename PointType::LabelType>;
    MonotoneChain<ResultPointType, LabelType> result(chain);
    result *= scalar;
    if constexpr (detail::has_label_v<LabelType>) {
        result.label() = LabelType{};
    }
    return result;
}

template <class Scalar, class PointType, class LabelType, class Storage>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr auto operator*(const Scalar& scalar, const MonotoneChain<PointType, LabelType, Storage>& chain) {
    return chain * scalar;
}

template <class PointType, class LabelType, class Storage, class Scalar>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr auto operator/(const MonotoneChain<PointType, LabelType, Storage>& chain, const Scalar& scalar) {
    using ResultPointType = Point<decltype(std::declval<PointType>().x() / scalar), typename PointType::LabelType>;
    MonotoneChain<ResultPointType, LabelType> result(chain);
    result /= scalar;
    if constexpr (detail::has_label_v<LabelType>) {
        result.label() = LabelType{};
    }
    return result;
}

template <class PointType, class LabelType, class Storage>
std::ostream& operator<<(std::ostream& stream, const MonotoneChain<PointType, LabelType, Storage>& chain);

}  // namespace pgl
