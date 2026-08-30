#pragma once

#include "shape/monotonechain.hpp"

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
#include <type_traits>
#include <utility>


namespace pgl {

template <class PointType = Point<>, class Label>
struct Polyline;

Polyline() -> Polyline<Point<>, NoLabel>;

template <std::ranges::input_range Range>
requires detail::is_point_v<std::ranges::range_value_t<Range>>
Polyline(Range&&) -> Polyline<std::remove_cvref_t<std::ranges::range_value_t<Range>>, NoLabel>;

template <class Number>
requires (!detail::is_point_v<Number>)
Polyline(std::initializer_list<Number>) -> Polyline<Point<Number>, NoLabel>;


/**
 * @brief An open polygonal chain stored by its vertices in traversal order
 * plus a translation.
 *
 * `Polyline` mirrors the storage layout of @ref Polygon — a vector of vertices
 * and a `translation_` applied lazily on access — but the vertices form an
 * open chain: n vertices are joined by n - 1 edges and there is no closing
 * edge back to the first vertex. Unlike @ref MonotoneChain, the vertices keep
 * the order they were given in, so the chain may bend backwards, revisit
 * points, and self-intersect; @ref isSimple reports whether it does not.
 *
 * The constructor stores the sequence exactly as given: no vertex is
 * reordered or removed, and the traversal direction is kept, so a polyline
 * built backwards iterates backwards. A polyline traversed backwards is still
 * the same set of points, so `operator==`/`operator<=>` and `std::hash` read
 * the vertices in a canonical direction instead of the stored one, giving
 * `Polyline({a, b, c}) == Polyline({c, b, a})` without ever flipping the
 * stored sequence. Repeated vertices are kept as given. A repeated
 * *consecutive* vertex produces a zero-length edge; like other degenerate
 * inputs in the library, such a polyline is outside the contract of the
 * geometric predicates (@ref isSimple reports `false` for it, and
 * @ref isDegenerate is `true` only when *all* vertices are equal).
 *
 * As a 1-dimensional manifold with boundary, the polyline's boundary is its
 * two extreme vertices and its relative interior is everything else (matching
 * the convention of @ref Segment and @ref MonotoneChain). Note that for a
 * closed polyline (first vertex equal to the last) this convention still
 * subtracts that vertex from the interior.
 *
 * @tparam PointType_ The vertex point type.
 */
template <class PointType_, class TLabel>
struct Polyline {
    using PointType = PointType_;
    using NumberType = PointType::NumberType;
    using LabelType = TLabel;
    static_assert(detail::is_point_v<PointType>, "Polyline requires pgl::Point vertices");

    template <bool Oriented>
    using BoundaryType = std::conditional_t<Oriented, OrientedSegment<PointType>, Segment<PointType>>;

    template <bool Oriented>
    class BoundaryIterator;

    using EdgeIterator = BoundaryIterator<false>;
    using OrientedEdgeIterator = BoundaryIterator<true>;

    /**
     * @brief Creates a polyline with no vertex.
     */
    constexpr Polyline() = default;

    /**
     * @brief Creates a polyline from a range of points.
     *
     * The points are linked and stored in the given order: nothing is changed —
     * no sorting, no deduplication, no reversal.
     *
     * @tparam Range Input range whose elements can be converted to @ref PointType.
     * @param points Range of vertices in traversal order.
     */
    template<std::ranges::input_range Range = std::initializer_list<PointType>>
    requires std::ranges::common_range<Range> &&
             std::convertible_to<std::ranges::range_value_t<Range>, PointType>
    constexpr explicit Polyline(Range&& points) {
        for (const auto& p : points) {
            points_.push_back(p);
        }
    }

    /**
     * @brief Creates a polyline from a flat list of coordinates.
     *
     * The values are consumed in pairs `(x0, y0, x1, y1, …)`, each pair forming
     * one vertex, so the list must hold an even number of values. The vertices
     * are stored in the given order (see the range constructor).
     *
     * @param coords Interleaved x/y coordinates of the vertices in traversal order.
     */
    constexpr explicit Polyline(std::initializer_list<NumberType> coords) {
        assert(coords.size() % 2 == 0);
        points_.reserve(coords.size() / 2);
        for (auto it = coords.begin(); it != coords.end(); ) {
            NumberType x = *it++;
            NumberType y = *it++;
            points_.emplace_back(x, y);
        }
    }

    /**
     * @brief Converts a polyline with compatible vertex type.
     *
     * The traversal order of the source is preserved.
     *
     * @tparam OtherPointType Source vertex type.
     * @param other Source polyline.
     */
    template<PointConcept OtherPointType, class OtherLabelType>
        requires std::constructible_from<PointType, const OtherPointType&>
    constexpr Polyline(const Polyline<OtherPointType, OtherLabelType>& other)
        : points_(other.begin(), other.end()), label_(detail::copyLabel<LabelType>(other)) {}

    /**
     * @brief Returns the polyline label.
     *
     * The label is mutable even through a const polyline: it is metadata that
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
     * @brief Accesses a vertex by index (in traversal order).
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
     * Unlike @ref Polygon the polyline is not cyclic — edges never wrap around
     * — but `get` still reduces the index modulo @ref size() (Euclidean, so
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
     * @brief Replaces the vertex at the given index.
     *
     * The stored vertices are kept relative to a lazy translation, so the
     * given absolute position has that translation subtracted before it is
     * stored; the rest of the polyline is untouched. Drops the cached bounding
     * box and hash.
     *
     * Complexity: O(1).
     *
     * @tparam OtherPoint Type of the new vertex.
     * @param index The index of the vertex to replace.
     * @param point The new vertex position.
     */
    template <PointConcept OtherPoint>
    constexpr void set(std::size_t index, const OtherPoint& point) {
        assert(index < size());
        PointType stored(point);
        stored -= translation_;
        points_[index] = stored;
        resetCache();
    }

    /**
     * @brief Returns the smallest index `i` with `(*this)[i] == point`, or
     * `-1` if `point` is not a vertex.
     *
     * Complexity: O(n) for n vertices (linear scan; the vertices follow the
     * traversal order, not a searchable order).
     *
     * @param point The vertex to locate.
     * @return The vertex index, or `-1` if `point` is not a vertex.
     */
    constexpr std::ptrdiff_t index(const PointType& point) const {
        for (std::ptrdiff_t i = 0; i < static_cast<std::ptrdiff_t>(size()); ++i) {
            if ((*this)[static_cast<std::size_t>(i)] == point) {
                return i;
            }
        }
        return -1;
    }

    /**
     * @brief Inserts a vertex at the given index, shifting the later vertices
     * back.
     *
     * The vertex becomes `(*this)[index]`; passing @ref size() appends. The
     * two edges that met at the old `index` are replaced by three, so the
     * polyline may stop being simple.
     *
     * Complexity: O(n) for n vertices.
     *
     * @tparam OtherPoint Type of the inserted point.
     * @param index Position the new vertex takes, in `[0, size()]`.
     * @param point Vertex to insert.
     */
    template <PointConcept OtherPoint>
    constexpr void insert(std::size_t index, const OtherPoint& point) {
        assert(index <= size());
        PointType stored(point);
        stored -= translation_;
        points_.insert(points_.begin() + static_cast<std::ptrdiff_t>(index), stored);
        resetCache();
    }

    /**
     * @brief Inserts a range of vertices at the given index, shifting the later
     * vertices back.
     *
     * The range keeps its order: its first point becomes `(*this)[index]`.
     * Passing @ref size() appends. The points are collected before the
     * insertion, so a range that reads from this polyline (e.g. its own
     * @ref vertices) is safe.
     *
     * Complexity: O(n + m) for n vertices and m inserted points.
     *
     * @tparam Range Input range whose elements can be converted to @ref PointType.
     * @param index Position the first new vertex takes, in `[0, size()]`.
     * @param points Vertices to insert, in traversal order.
     */
    template <std::ranges::input_range Range>
        requires (!detail::is_point_v<std::remove_cvref_t<Range>>) &&
                 std::convertible_to<std::ranges::range_value_t<Range>, PointType>
    constexpr void insert(std::size_t index, Range&& points) {
        assert(index <= size());
        std::vector<PointType> incoming;
        for (const auto& p : points) {
            PointType stored(p);
            stored -= translation_;
            incoming.push_back(stored);
        }
        points_.insert(points_.begin() + static_cast<std::ptrdiff_t>(index), incoming.begin(),
                       incoming.end());
        resetCache();
    }

    /**
     * @brief Appends a vertex, extending the polyline by one edge.
     *
     * Same as `insert(size(), point)`.
     *
     * Complexity: amortized O(1).
     *
     * @tparam OtherPoint Type of the appended point.
     * @param point Vertex to append.
     */
    template <PointConcept OtherPoint>
    constexpr void pushBack(const OtherPoint& point) {
        insert(size(), point);
    }

    /**
     * @brief Appends a range of vertices in order.
     *
     * Same as `insert(size(), points)`.
     *
     * Complexity: O(m) for m appended points.
     *
     * @tparam Range Input range whose elements can be converted to @ref PointType.
     * @param points Vertices to append, in traversal order.
     */
    template <std::ranges::input_range Range>
        requires (!detail::is_point_v<std::remove_cvref_t<Range>>) &&
                 std::convertible_to<std::ranges::range_value_t<Range>, PointType>
    constexpr void pushBack(Range&& points) {
        insert(size(), std::forward<Range>(points));
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
        return Iterator(points_.cbegin(), translation_);
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
        return Iterator(points_.cend(), translation_);
    }

    /**
     * @brief Compares two polylines by their canonical vertex sequences.
     *
     * The stored direction is irrelevant: each side is read through @ref
     * canonicalAt, so the order is the one induced by the lexicographically
     * smaller of each sequence and its reversal.
     *
     * Complexity: O(n) for n vertices.
     */
    constexpr auto operator<=>(const Polyline& other) const {
        if (auto cmp = size() <=> other.size(); cmp != 0) {
            return cmp;
        }
        const bool reversed = !storedIsCanonical();
        const bool otherReversed = !other.storedIsCanonical();
        for (std::size_t i = 0; i < size(); ++i) {
            if (auto cmp = canonicalAt(i, reversed) <=> other.canonicalAt(i, otherReversed);
                cmp != 0) {
                return cmp;
            }
        }
        return std::strong_ordering::equal;
    }

    /**
     * @brief Checks equality of two polylines.
     *
     * Complexity: O(n) for n vertices.
     *
     * @return True if both polylines have the same vertices in the same order,
     *         up to the traversal direction: a polyline equals its reversal.
     */
    constexpr bool operator==(const Polyline& other) const {
        if (size() != other.size()) {
            return false;
        }
        const bool reversed = !storedIsCanonical();
        const bool otherReversed = !other.storedIsCanonical();
        for (std::size_t i = 0; i < size(); ++i) {
            if (canonicalAt(i, reversed) != other.canonicalAt(i, otherReversed)) {
                return false;
            }
        }
        return true;
    }

    /** @brief Tests whether another shape defines exactly the same point set. */
    template<AnyShapeConcept OtherShape>
    [[nodiscard]] constexpr bool samePointSet(const OtherShape& other) const;

    /**
     * @brief Returns the number of vertices in the polyline.
     */
    constexpr std::size_t size() const {
        return points_.size();
    }

    /**
     * @brief Checks whether the polyline has no vertex.
     */
    constexpr bool empty() const {
        return points_.empty();
    }

    /**
     * @brief Checks if the polyline is degenerate (all vertices are equal, so
     * it covers at most a single point).
     *
     * An empty or single-vertex polyline is degenerate. Note the contrast with
     * @ref MonotoneChain::isDegenerate: a polyline may repeat one point many
     * times and still be a single geometric point.
     *
     * Complexity: O(n).
     */
    constexpr bool isDegenerate() const {
        return std::adjacent_find(points_.begin(), points_.end(), std::not_equal_to<>{}) ==
               points_.end();
    }

    /**
     * @brief Checks whether the polyline covers exactly one point.
     *
     * Same as @ref isDegenerate except for the empty polyline, which has no
     * defining vertex and so is not a point.
     *
     * Complexity: O(n), returning at the first differing vertex.
     */
    [[nodiscard]] constexpr bool isPoint() const {
        return detail::allPointsEqual(points_);
    }

    /**
     * @brief Returns the point the polyline collapses to, if it does.
     *
     * Complexity: O(n), returning at the first differing vertex.
     *
     * @return The common vertex if @ref isPoint, `std::nullopt` otherwise.
     */
    [[nodiscard]] constexpr std::optional<PointType> getIfPoint() const {
        if (!isPoint()) {
            return std::nullopt;
        }
        return points_.front() + translation_;
    }

    /**
     * @brief Checks whether the polyline covers exactly one segment of positive
     * length.
     *
     * True when the vertices are collinear but not all equal. The polyline is
     * connected, so collinear vertices make its edges cover the single segment
     * spanning them, however many times they retrace it.
     *
     * Complexity: O(n), returning at the first non-collinear vertex.
     */
    [[nodiscard]] constexpr bool isSegment() const {
        return detail::pointsSpanSegment(points_);
    }

    /**
     * @brief Returns the segment the polyline collapses to, if it does.
     *
     * Complexity: O(n).
     *
     * @return The spanned segment if @ref isSegment, `std::nullopt` otherwise.
     */
    [[nodiscard]] constexpr std::optional<BoundaryType<false>> getIfSegment() const {
        if (!isSegment()) {
            return std::nullopt;
        }
        return detail::spannedSegment<BoundaryType<false>>(points_) + translation_;
    }

    /**
     * @brief Checks whether the polyline is degenerate without covering a point
     * or a segment.
     *
     * True only for the empty polyline, which has no defining vertex: any other
     * degenerate polyline has all vertices equal and is therefore a point.
     *
     * Complexity: O(1).
     */
    [[nodiscard]] constexpr bool isUndefined() const {
        return empty();
    }

    /**
     * @brief Tests whether the polyline is simple (it does not touch or cross
     * itself).
     *
     * Simple means no two non-adjacent edges share a point and consecutive
     * edges meet only at their shared vertex; in an open chain the first and
     * last edges are *not* adjacent, so a closed polyline (first vertex equal
     * to the last) is not simple. A zero-length edge (repeated consecutive
     * vertex) also makes the polyline not simple. A polyline with fewer than
     * two vertices is vacuously simple.
     *
     * Uses a brute-force pairwise edge test in O(n^2) for few edges (n <= 8)
     * or floating-point coordinates, and the Bentley-Ottmann sweep
     * (O(n log n)) for larger exact (integer or rational) polylines.
     *
     * @tparam Rational Exact rational type used by the sweep for large polylines.
     * @return `true` if no two edges meet except consecutive edges at their
     *         shared vertex.
     */
    template <class Rational = pgl::Rational<pgl::BigInt>>
    [[nodiscard]] bool isSimple() const;

    /**
     * @brief Returns a segment realizing the diameter (the farthest vertex pair).
     *
     * The farthest pair of vertices lies on the convex hull of the vertex set,
     * so this builds a @ref Convex from the polyline vertices and returns that
     * hull's @ref Convex::diameter(). Distances are compared exactly via
     * squared length.
     *
     * @return A longest segment between two vertices (degenerate if fewer than
     *         two distinct vertices).
     */
    constexpr Segment<PointType> diameter() const {
        return Convex<PointType>(vertices()).diameter();
    }

    /**
     * @brief Returns the convex hull of the polyline's vertices.
     */
    constexpr Convex<PointType> convexHull() const {
        return Convex<PointType>(vertices());
    }

    /**
     * @brief Computes the bounding box of the polyline.
     *
     * The result is computed on the first call and cached in @ref bbox_; later
     * calls return the stored value. Any operation that modifies the polyline
     * resets the cache.
     *
     * Complexity: O(n) for n vertices on the first call, O(1) thereafter.
     *
     * @return A constant reference to the rectangle bounding the polyline.
     */
    constexpr const Rectangle<PointType>& bbox() const;

    /**
     * @brief Returns the integer points the polyline contains.
     *
     * The lattice points of the edges, one edge at a time in traversal order,
     * with each point kept the first time the polyline reaches it: a vertex
     * shared by two edges appears once, and so does a point the polyline
     * crosses or retraces. A single vertex answers for itself.
     *
     * @tparam ResultNumber Integer coordinate type of the points: the shape's
     *         own coordinate type when that is a signed integer, the integer a
     *         pgl::Rational is built on, and `int64_t` for anything else.
     * @return The lattice points, each of them once, carrying no label.
     * @throws std::logic_error If a coordinate is not finite, or a lattice
     *         point of the shape does not fit @p ResultNumber.
     * @throws std::length_error If there are more of them than a vector holds.
     */
    template <class ResultNumber = grid_number_t<typename PointType_::NumberType>>
        requires(detail::extended_integral<ResultNumber> || std::same_as<ResultNumber, BigInt>)
    [[nodiscard]] std::vector<Point<ResultNumber, typename PointType::LabelType>>
    latticePoints() const;

    /**
     * @brief Computes the floating-point bounding box of the polyline.
     * @tparam ResultNumber The floating-point type for the result.
     * @return A rectangle with floating-point coordinates representing the bounding box.
     */
    template <std::floating_point ResultNumber = double>
    constexpr Rectangle<Point<ResultNumber>> fbox() const;

    /**
     * @brief Returns the vertices of the polyline (translation applied).
     */
    constexpr std::vector<PointType> vertices() const {
        std::vector<PointType> ret(points_.begin(), points_.end());
        for (auto& vertex : ret) {
            vertex += translation_;
        }
        return ret;
    }

    /**
     * @brief Returns the edges of the polyline.
     *
     * A polyline with n vertices has n - 1 edges (none for an empty or
     * single-vertex polyline); there is no closing edge back to the first
     * vertex.
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
     * @brief Returns the oriented edges of the polyline, each directed from
     * vertex i to vertex i + 1 in traversal order.
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
     * @brief Returns a lazy view over the vertices, translating each on the fly
     * instead of allocating a vector.
     *
     * Same traversal-order vertex sequence as @ref vertices(), with no heap
     * allocation. The polyline is itself a vertex range, so this is just
     * @ref begin() and @ref end() packaged as a view for callers that take one.
     */
    constexpr auto verticesView() const {
        return std::ranges::subrange(begin(), end());
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
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     *
     * Complexity: O(n) for n vertices.
     *
     * @tparam OtherPoint Type of the point.
     * @param point Point to test.
     * @return `true` if the point lies on the polyline.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool contains(const OtherPoint& point) const;

    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     *
     * A self-intersecting polyline can cover a segment with several,
     * possibly non-consecutive collinear edges, so the segment is contained
     * exactly when the union of its overlaps with the polyline edges covers
     * it with no gap.
     *
     * Complexity: O(n log n) for n vertices (collecting and sorting the
     * overlaps).
     *
     * @tparam OtherSegment Type of the other segment.
     * @param other Segment to test.
     * @return `true` if every point of the segment lies on the polyline.
     */
    template<SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool contains(const OtherSegment& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template<OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool contains(const OtherOrientedSegment& other) const;

    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     * @return `true` if the line is degenerate and its unique point lies on the polyline.
     */
    template<LineConcept OtherLine>
    [[nodiscard]] constexpr bool contains(const OtherLine& other) const;

    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     * @return `true` if the oriented line is degenerate and its unique point lies on the polyline.
     */
    template<OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool contains(const OtherOrientedLine& other) const;

    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     * @return `true` if the ray is degenerate and its unique point lies on the polyline.
     */
    template<RayConcept OtherRay>
    [[nodiscard]] constexpr bool contains(const OtherRay& other) const;

    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     * @return `true` if the halfplane is degenerate and its unique point lies on the polyline.
     */
    template<HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool contains(const OtherHalfplane& other) const;

    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     * @return `true` if the rectangle is degenerate and its diagonal lies on the polyline.
     */
    template<RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool contains(const OtherRectangle& other) const;

    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     * @return `true` if the triangle is degenerate and its spanning segment lies on the polyline.
     */
    template<TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr bool contains(const OtherTriangle& other) const;

    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     * @return `true` if the convex has at most two vertices and they span a subset of the polyline.
     */
    template<ConvexConcept OtherConvex>
    [[nodiscard]] constexpr bool contains(const OtherConvex& other) const;

    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     *
     * A polygon lies on the 1-dimensional polyline exactly when all of its
     * edges do (its interior is then empty), so this folds @ref contains over
     * the polygon's edges.
     */
    template<PolygonConcept OtherPolygon>
    [[nodiscard]] constexpr bool contains(const OtherPolygon& other) const;

    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     * @return `true` if all three boundary points of `other` are equal and the polyline contains that point.
     */
    template<DiskConcept OtherDisk>
    [[nodiscard]] constexpr bool contains(const OtherDisk& other) const;

    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     *
     * A chain is exactly the union of its edges, so this folds the
     * (gap-sweeping) segment containment over them.
     */
    template<MonotoneChainConcept OtherChain>
    [[nodiscard]] constexpr bool contains(const OtherChain& other) const;

    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     *
     * A polyline contains another polyline exactly when it contains every
     * edge of the other (and its vertex for a degenerate other).
     *
     * Complexity: O(m n log n) for m vertices of the other polyline.
     */
    template<PolylineConcept OtherPolyline>
    [[nodiscard]] constexpr bool contains(const OtherPolyline& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool contains(const EmptyShape<EmptyPoint>&) const {
        return true;
    }

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool contains(const Shape<OtherPoint>& other) const;

    /**
     * @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B).
     *
     * The boundary of a polyline is its two extreme vertices (matching the
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

    // The boundary of a polyline is exactly its two extreme vertices, a finite
    // point set, so it contains no positive-length or two-dimensional shape.
    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool boundaryContains(const OtherSegment& other) const {
        return detail::reduceDegenerateToPoint(
            other, [this](const auto& vertex) { return this->boundaryContains(vertex); });
    }
    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool boundaryContains(const OtherOrientedSegment& other) const {
        return detail::reduceDegenerateToPoint(
            other, [this](const auto& vertex) { return this->boundaryContains(vertex); });
    }
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
    [[nodiscard]] constexpr bool boundaryContains(const OtherRectangle& other) const {
        return detail::reduceDegenerateToPoint(
            other, [this](const auto& vertex) { return this->boundaryContains(vertex); });
    }
    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr bool boundaryContains(const OtherTriangle& other) const {
        return detail::reduceDegenerateToPoint(
            other, [this](const auto& vertex) { return this->boundaryContains(vertex); });
    }
    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<ConvexConcept OtherConvex>
    [[nodiscard]] constexpr bool boundaryContains(const OtherConvex& other) const {
        return detail::reduceDegenerateToPoint(
            other, [this](const auto& vertex) { return this->boundaryContains(vertex); });
    }
    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<PolygonConcept OtherPolygon>
    [[nodiscard]] constexpr bool boundaryContains(const OtherPolygon& other) const {
        return detail::reduceDegenerateToPoint(
            other, [this](const auto& vertex) { return this->boundaryContains(vertex); });
    }
    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<DiskConcept OtherDisk>
    [[nodiscard]] constexpr bool boundaryContains(const OtherDisk& other) const {
        return detail::reduceDegenerateToPoint(
            other, [this](const auto& vertex) { return this->boundaryContains(vertex); });
    }
    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<MonotoneChainConcept OtherChain>
    [[nodiscard]] constexpr bool boundaryContains(const OtherChain& other) const {
        // The boundary is the two extreme vertices, so only a chain without an
        // edge fits inside it.
        return other.empty() || (other.size() == 1 && boundaryContains(other[0]));
    }
    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<PolylineConcept OtherPolyline>
    [[nodiscard]] constexpr bool boundaryContains(const OtherPolyline& other) const {
        // The boundary is the two extreme vertices, so only a polyline
        // covering at most one point fits inside it.
        return other.empty() || (other.isDegenerate() && boundaryContains(other[0]));
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
     * The relative interior of a polyline is the polyline minus its two
     * extreme vertices.
     *
     * Complexity: O(n) for n vertices.
     *
     * @tparam OtherPoint Type of the point.
     * @param point Point to test.
     * @return `true` if the point lies on the polyline and is not an extreme vertex.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorContains(const OtherPoint& point) const;

    /**
     * @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B).
     *
     * Complexity: O(n log n) for n vertices (see @ref contains).
     *
     * @tparam OtherSegment Type of the other segment.
     * @param other Segment to test.
     * @return `true` if the polyline contains the segment and the segment
     *         avoids both extreme vertices of the polyline.
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

    // A polyline is one-dimensional: its relative interior cannot contain any
    // unbounded or two-dimensional shape.
    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template<RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool interiorContains(const OtherRectangle& other) const {
        return detail::reduceDegenerateGuarded(
            other, [this](const auto& carrier) { return this->interiorContains(carrier); });
    }
    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template<ConvexConcept OtherConvex>
    [[nodiscard]] constexpr bool interiorContains(const OtherConvex& other) const {
        return detail::reduceDegenerateGuarded(
            other, [this](const auto& carrier) { return this->interiorContains(carrier); });
    }
    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template<PolygonConcept OtherPolygon>
    [[nodiscard]] constexpr bool interiorContains(const OtherPolygon& other) const {
        return detail::reduceDegenerate(
            other, [this](const auto& carrier) { return this->interiorContains(carrier); });
    }
    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template<DiskConcept OtherDisk>
    [[nodiscard]] constexpr bool interiorContains(const OtherDisk& other) const {
        return detail::reduceDegenerate(
            other, [this](const auto& carrier) { return this->interiorContains(carrier); });
    }

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template<MonotoneChainConcept OtherChain>
    [[nodiscard]] constexpr bool interiorContains(const OtherChain& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template<PolylineConcept OtherPolyline>
    [[nodiscard]] constexpr bool interiorContains(const OtherPolyline& other) const;
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
     * Complexity: O(n) for n vertices.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool intersects(const OtherPoint& other) const;

    /**
     * @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅).
     *
     * Every edge is tested against the segment; a self-intersecting polyline
     * has no monotone structure to prune the scan.
     *
     * Complexity: O(n) for n vertices.
     *
     * @tparam OtherSegment Type of the other segment.
     * @param other Segment to test.
     * @return `true` if the polyline and the segment share at least one point.
     */
    template<SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool intersects(const OtherSegment& other) const;

    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template<OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool intersects(const OtherOrientedSegment& other) const;
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
    /**
     * @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅).
     *
     * Folds the chain's own (pruned) segment test over the polyline edges.
     *
     * Complexity: O(n (log m + k)) for a polyline with n vertices and a chain
     * with m vertices.
     */
    template<MonotoneChainConcept OtherChain>
    [[nodiscard]] constexpr bool intersects(const OtherChain& other) const;

    /**
     * @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅).
     *
     * All-pairs edge test, preceded by a bounding-box cull.
     *
     * Complexity: O(n m) for polylines with n and m vertices.
     *
     * @tparam OtherPolyline Type of the other polyline.
     * @param other Polyline to test.
     * @return `true` if the polylines share at least one point.
     */
    template<PolylineConcept OtherPolyline>
    [[nodiscard]] constexpr bool intersects(const OtherPolyline& other) const;

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
        requires (!PointConcept<OtherShape> && detail::shapeRank<OtherShape> > detail::shapeRank<Polyline>)
    [[nodiscard]] constexpr bool intersects(const OtherShape& other) const {
        return other.intersects(*this);
    }

    /**
     * @brief Tests whether the interiors of the shapes intersect (A° ∩ B° ≠ ∅).
     *
     * A point's interior is the point itself, so this matches @ref interiorContains.
     *
     * Complexity: O(n) for n vertices.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherPoint& other) const;

    /**
     * @brief Tests whether the interiors of the shapes intersect (A° ∩ B° ≠ ∅).
     *
     * The polyline's relative interior is the polyline minus its extreme
     * vertices, so a polyline vertex other than the extremes counts as
     * interior: a segment whose open part passes exactly through such a vertex
     * engages this predicate even though it crosses no open edge.
     *
     * Complexity: O(n) for n vertices.
     *
     * @tparam OtherSegment Type of the other segment.
     * @param other Segment to test.
     * @return `true` if the polyline minus its extremes meets the open segment.
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
    /** @brief Tests whether the interiors of the shapes intersect (A° ∩ B° ≠ ∅). */
    template<MonotoneChainConcept OtherChain>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherChain& other) const;

    /**
     * @brief Tests whether the interiors of the shapes intersect (A° ∩ B° ≠ ∅).
     *
     * All-pairs edge test plus the crossing-at-a-non-extreme-vertex checks in
     * both directions.
     *
     * Complexity: O(n m) for polylines with n and m vertices.
     */
    template<PolylineConcept OtherPolyline>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherPolyline& other) const;

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
        requires (!PointConcept<OtherShape> && detail::shapeRank<OtherShape> > detail::shapeRank<Polyline>)
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
     * True when the segment minus the polyline has at least two connected
     * components. Because the polyline may self-intersect, the pieces are
     * collected as a set and joined through every shared point that survives
     * the removal (see `detail::separates1DSet`).
     *
     * Complexity: O(n^2) exact piece tests for n vertices.
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

    // --- 2-dimensional targets: the region minus the polyline is disconnected
    // iff the polyline (clipped to the region) together with the region's
    // boundary closes a cycle through the interior. Unlike a monotone chain, a
    // self-intersecting polyline can seal a pocket with a loop that never
    // leaves the interior, so the traversal-order crosscut scan is replaced by
    // a cycle search on the polyline's self-intersection arrangement (see
    // detail::polylineSeparatesConvexRegion). ---
    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
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
    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template<PolygonConcept OtherPolygon>
    [[nodiscard]] constexpr bool separates(const OtherPolygon& other) const;
    /**
     * @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected).
     *
     * The chain is a 1-dimensional set like the polyline, so its free pieces
     * are joined geometrically (see `detail::separates1DSet`).
     */
    template<MonotoneChainConcept OtherChain>
    [[nodiscard]] constexpr bool separates(const OtherChain& other) const;
    /**
     * @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected).
     *
     * Set semantics: the other polyline's free pieces may reconnect through
     * its own self-intersections, so removing an interior point of a closed
     * polyline does not disconnect it.
     *
     * Complexity: O((n m)^2) piece tests for polylines with n and m vertices.
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

    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     *
     * The polyline is the one shape whose complement can come apart, so it does
     * not answer a region by that region's outer polygon: having no area
     * itself, it holds only a region with no area, and then edge by edge. See
     * implementation/contains.hpp.
     */
    template<PolygonWithHolesConcept OtherRegion>
    [[nodiscard]] constexpr bool contains(const OtherRegion& other) const;

    /**
     * @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B).
     *
     * A boundary has no area, so it holds only a region with no area — which is
     * exactly the union of that region's ring edges.
     */
    template<PolygonWithHolesConcept OtherRegion>
    [[nodiscard]] constexpr bool boundaryContains(const OtherRegion& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template<PolygonWithHolesConcept OtherRegion>
    [[nodiscard]] constexpr bool interiorContains(const OtherRegion& other) const;

    /**
     * @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected).
     *
     * The region is settled by the cell engine of implementation/separates.hpp;
     * see the notes on pgl::PolygonWithHoles::separates for what a region
     * admits that a simply connected target does not.
     */
    template<PolygonWithHolesConcept OtherRegion>
    [[nodiscard]] bool separates(const OtherRegion& other) const;

    // -------------------------------------------------------------------------
    // A set of regions
    //
    // It outranks every other shape, so the symmetric relations reach it through
    // the rank-based forwarders and only the asymmetric ones are answered here.
    // A set is the union of its components, so it is contained exactly when
    // every component is — no matter what this shape is.

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template<PolygonSetConcept OtherSet>
    [[nodiscard]] constexpr bool contains(const OtherSet& other) const {
        for (const auto& component : other) {
            if (!contains(component)) {
                return false;
            }
        }
        return true;
    }

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<PolygonSetConcept OtherSet>
    [[nodiscard]] constexpr bool boundaryContains(const OtherSet& other) const {
        for (const auto& component : other) {
            if (!boundaryContains(component)) {
                return false;
            }
        }
        return true;
    }

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template<PolygonSetConcept OtherSet>
    [[nodiscard]] constexpr bool interiorContains(const OtherSet& other) const {
        for (const auto& component : other) {
            if (!interiorContains(component)) {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected).
     *
     * A set of regions is the one target that may already be in several pieces
     * before anything is removed, so this neither folds over its components nor
     * answers false for a remover that misses it. See
     * implementation/separates.hpp.
     */
    template<PolygonSetConcept OtherSet>
    [[nodiscard]] bool separates(const OtherSet& other) const;
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
    template<PolylineConcept OtherPolyline>
    [[nodiscard]] constexpr bool crosses(const OtherPolyline& other) const;
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
        requires (!PointConcept<OtherShape> && detail::shapeRank<OtherShape> > detail::shapeRank<Polyline>)
    [[nodiscard]] constexpr bool crosses(const OtherShape& other) const {
        return other.crosses(*this);
    }

    /** @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint. */
    template <class ResultNumber = NumberType, PointConcept OtherPoint>
    [[nodiscard]] constexpr std::optional<Point<ResultNumber, typename PointType::LabelType>>
    intersection(const OtherPoint& other) const;

    /**
     * @brief Returns the intersection with a one-dimensional or convex shape
     * (A ∩ B), a sequence of points and segments sorted by lexicographic order.
     *
     * Folds the shape over the polyline edges, delegating each edge to the
     * segment-vs-shape intersection, then coalesces the pieces like
     * @ref intersection(const OtherPolyline&) const.
     *
     * Complexity: O(n) segment intersections for n vertices, plus coalescing
     * the resulting pieces (quadratic in their count).
     *
     * @tparam ResultNumber Number type of the returned coordinates.
     * @return Vector of points and segments forming the intersection.
     * @warning Divides coordinates after casting to ResultNumber.
     */
    template <class ResultNumber = division_result_t<NumberType>, SegmentConcept OtherSegment>
    [[nodiscard]] constexpr std::vector<std::variant<Point<ResultNumber, typename PointType::LabelType>,
                                                     Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherSegment& other) const;
    /** @copydoc intersection(const OtherSegment&) const */
    template <class ResultNumber = division_result_t<NumberType>, OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr std::vector<std::variant<Point<ResultNumber, typename PointType::LabelType>,
                                                     Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherOrientedSegment& other) const;
    /** @copydoc intersection(const OtherSegment&) const */
    template <class ResultNumber = division_result_t<NumberType>, LineConcept OtherLine>
    [[nodiscard]] constexpr std::vector<std::variant<Point<ResultNumber, typename PointType::LabelType>,
                                                     Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherLine& other) const;
    /** @copydoc intersection(const OtherSegment&) const */
    template <class ResultNumber = division_result_t<NumberType>, OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr std::vector<std::variant<Point<ResultNumber, typename PointType::LabelType>,
                                                     Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherOrientedLine& other) const;
    /** @copydoc intersection(const OtherSegment&) const */
    template <class ResultNumber = division_result_t<NumberType>, RayConcept OtherRay>
    [[nodiscard]] constexpr std::vector<std::variant<Point<ResultNumber, typename PointType::LabelType>,
                                                     Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherRay& other) const;
    /** @copydoc intersection(const OtherSegment&) const */
    template <class ResultNumber = division_result_t<NumberType>, HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr std::vector<std::variant<Point<ResultNumber, typename PointType::LabelType>,
                                                     Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherHalfplane& other) const;
    /** @copydoc intersection(const OtherSegment&) const */
    template <class ResultNumber = division_result_t<NumberType>, RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr std::vector<std::variant<Point<ResultNumber, typename PointType::LabelType>,
                                                     Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherRectangle& other) const;
    /** @copydoc intersection(const OtherSegment&) const */
    template <class ResultNumber = division_result_t<NumberType>, TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr std::vector<std::variant<Point<ResultNumber, typename PointType::LabelType>,
                                                     Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherTriangle& other) const;
    /** @copydoc intersection(const OtherSegment&) const */
    template <class ResultNumber = division_result_t<NumberType>, ConvexConcept OtherConvex>
    [[nodiscard]] constexpr std::vector<std::variant<Point<ResultNumber, typename PointType::LabelType>,
                                                     Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherConvex& other) const;

    /**
     * @brief Returns the intersection with a monotone chain (A ∩ B), a
     * sequence of points and segments sorted by lexicographic order.
     *
     * All-pairs edge test, preceded by a bounding-box cull; the pieces are
     * coalesced like @ref intersection(const OtherPolyline&) const.
     *
     * Complexity: O(n m) for a polyline with n vertices and a chain with m
     * vertices, plus coalescing the resulting pieces.
     *
     * @tparam ResultNumber Number type of the returned coordinates.
     * @return Vector of points and segments forming the intersection.
     * @warning Divides coordinates after casting to ResultNumber.
     */
    template <class ResultNumber = division_result_t<NumberType>, MonotoneChainConcept OtherChain>
    [[nodiscard]] constexpr std::vector<std::variant<Point<ResultNumber, typename PointType::LabelType>,
                                                     Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherChain& other) const;

    /**
     * @brief Returns the intersection of the two polylines (A ∩ B), a sequence
     * of points and segments sorted by lexicographic order.
     *
     * Two polylines can overlap along collinear sub-segments, so the result is
     * a vector of point-or-segment variants. Pieces are maximal: collinear
     * touching overlaps are coalesced into single segments — even when they
     * come from non-consecutive edges of a self-intersecting polyline — and
     * points covered by a reported segment or repeated by several edge pairs
     * are dropped. Computed by an all-pairs edge test with a bounding-box cull.
     *
     * Complexity: O(n m) for polylines with n and m vertices, plus coalescing
     * the resulting pieces (quadratic in their count).
     *
     * @tparam ResultNumber Number type of the returned coordinates.
     * @tparam OtherPolyline Type of the other polyline.
     * @param other Polyline to intersect with.
     * @return Vector of points and segments forming the intersection.
     * @warning Divides coordinates after casting to ResultNumber.
     */
    template <class ResultNumber = division_result_t<NumberType>, PolylineConcept OtherPolyline>
    [[nodiscard]] constexpr std::vector<std::variant<Point<ResultNumber, typename PointType::LabelType>,
                                                     Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherPolyline& other) const;

    /**
     * @brief Returns the intersection with a polygon or a region (A ∩ B), a
     * sequence of points and segments sorted by lexicographic order.
     *
     * The area may be non-convex — and, for a @ref PolygonWithHoles, not simply
     * connected — so its intersection with a single edge can already split into
     * several disjoint pieces; each edge is clipped against the area and the
     * pieces are coalesced like @ref intersection(const OtherPolyline&) const.
     *
     * @note This is the shared implementation of the area–polyline clip, not an
     * `intersection` overload: @ref Polygon and @ref PolygonWithHoles both
     * outrank @ref Polyline, so
     * @ref Polygon::intersection(const OtherPolyline&) const and
     * @ref PolygonWithHoles::intersection(const OtherPolyline&) const own the
     * pairs and call this helper (and `polyline.intersection(area)` reaches it by
     * forwarding up). Keeping it here reuses the polyline's coalescing and labels
     * the pieces with the polyline's label.
     *
     * Complexity: O(n) area-vs-segment clips for n vertices, plus coalescing the
     * resulting pieces.
     *
     * @tparam ResultNumber Number type of the returned coordinates.
     * @tparam OtherArea Type of the polygon or region.
     * @param other Polygon or region to intersect with.
     * @return Vector of points and segments forming the intersection.
     * @warning Divides coordinates after casting to ResultNumber.
     */
    template <class ResultNumber = division_result_t<NumberType>, class OtherArea>
        requires(PolygonConcept<OtherArea> || PolygonWithHolesConcept<OtherArea>)
    [[nodiscard]] constexpr std::vector<std::variant<Point<ResultNumber, typename PointType::LabelType>,
                                                     Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    polygonIntersection(const OtherArea& other) const;

    /** @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint. @warning Divides coordinates after casting to ResultNumber. */
    template <class ResultNumber = division_result_t<NumberType>, typename OtherShape>
        requires (!PointConcept<OtherShape>
                  && (detail::shapeRank<OtherShape> > detail::shapeRank<Polyline>)
                  && requires(const OtherShape& o, const Polyline& self) {
                         o.template intersection<ResultNumber>(self);
                     })
    [[nodiscard]] constexpr auto intersection(const OtherShape& other) const {
        return other.template intersection<ResultNumber>(*this);
    }

    /** @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint. */
    template <class ResultNumber = NumberType, class EmptyPoint>
    [[nodiscard]] constexpr EmptyShape<EmptyPoint> intersection(const EmptyShape<EmptyPoint>&) const {
        return {};
    }

    /**
     * @brief Returns the squared Euclidean distance to the given shape.
     *
     * Zero when the shapes intersect, otherwise the minimum over the polyline
     * edges. The polyline must have at least one edge.
     *
     * Complexity: O(n) edge queries for n vertices, plus the intersection test.
     *
     * @tparam ResultNumber Coordinate type of the returned distance (default: @ref division_result_t).
     *
     * @warning With an integer @p ResultNumber the exact squared distance is
     *          generally a fraction, so the internal division truncates and the
     *          result is inexact. Request a floating-point or pgl::Rational
     *          result type, e.g. `squaredDistance<double>(point)`, for an
     *          accurate value.
     */
    template <class ResultNumber = division_result_t<NumberType>, PointConcept OtherPoint>
    [[nodiscard]] constexpr auto squaredDistance(const OtherPoint& point) const;
    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = division_result_t<NumberType>, SegmentConcept OtherSegment>
    [[nodiscard]] constexpr auto squaredDistance(const OtherSegment& other) const;
    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = division_result_t<NumberType>, OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr auto squaredDistance(const OtherOrientedSegment& other) const;
    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = division_result_t<NumberType>, LineConcept OtherLine>
    [[nodiscard]] constexpr auto squaredDistance(const OtherLine& other) const;
    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = division_result_t<NumberType>, OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr auto squaredDistance(const OtherOrientedLine& other) const;
    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = division_result_t<NumberType>, RayConcept OtherRay>
    [[nodiscard]] constexpr auto squaredDistance(const OtherRay& other) const;
    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = division_result_t<NumberType>, HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr auto squaredDistance(const OtherHalfplane& other) const;
    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = division_result_t<NumberType>, RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr auto squaredDistance(const OtherRectangle& other) const;
    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = division_result_t<NumberType>, TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr auto squaredDistance(const OtherTriangle& other) const;
    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = division_result_t<NumberType>, ConvexConcept OtherConvex>
    [[nodiscard]] constexpr auto squaredDistance(const OtherConvex& other) const;
    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = division_result_t<NumberType>, MonotoneChainConcept OtherChain>
    [[nodiscard]] constexpr auto squaredDistance(const OtherChain& other) const;
    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = division_result_t<NumberType>, PolylineConcept OtherPolyline>
    [[nodiscard]] constexpr auto squaredDistance(const OtherPolyline& other) const;

    /**
     * @brief Returns the squared Euclidean distance to a disk.
     *
     * Forwards to @ref Disk::squaredDistance's model. Reports in `detail::floating_result_t<ResultNumber>`: the gap to a
     * circle is generally irrational, so a floating-point `ResultNumber` is
     * honoured as asked and any other request falls back to `double`.
     */
    template <class ResultNumber = double, class DiskPointType, class DiskLabel>
    [[nodiscard]] detail::floating_result_t<ResultNumber> squaredDistance(
        const Disk<DiskPointType, DiskLabel>& disk) const;

    /**
     * @brief Returns the squared Euclidean distance to the given shape.
     *
     * Forwards to the other shape's implementation so that each unordered pair
     * needs `squaredDistance` defined only once, on the higher-ranked shape.
     */
    template <class ResultNumber = division_result_t<NumberType>, typename OtherShape>
        requires ((detail::shapeRank<OtherShape> > detail::shapeRank<Polyline>)
                  && requires(const OtherShape& o, const Polyline& self) {
                         o.template squaredDistance<ResultNumber>(self);
                     })
    [[nodiscard]] constexpr auto squaredDistance(const OtherShape& other) const {
        return other.template squaredDistance<ResultNumber>(*this);
    }

    /**
     * @brief Returns the Manhattan (L1) distance to the given shape.
     *
     * Zero when the shapes intersect, otherwise the minimum over the polyline
     * edges. The polyline must have at least one edge.
     *
     * @warning With an integer @p ResultNumber the exact distance is generally
     *          a fraction, so the internal division truncates. Request a
     *          floating-point or pgl::Rational result type for an accurate value.
     */
    template <class ResultNumber = division_result_t<NumberType>, PointConcept OtherPoint>
    [[nodiscard]] constexpr auto distanceL1(const OtherPoint& point) const;
    /** @copydoc distanceL1(const OtherPoint&) const */
    template <class ResultNumber = division_result_t<NumberType>, SegmentConcept OtherSegment>
    [[nodiscard]] constexpr auto distanceL1(const OtherSegment& other) const;
    /** @copydoc distanceL1(const OtherPoint&) const */
    template <class ResultNumber = division_result_t<NumberType>, OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr auto distanceL1(const OtherOrientedSegment& other) const;
    /** @copydoc distanceL1(const OtherPoint&) const */
    template <class ResultNumber = division_result_t<NumberType>, LineConcept OtherLine>
    [[nodiscard]] constexpr auto distanceL1(const OtherLine& other) const;
    /** @copydoc distanceL1(const OtherPoint&) const */
    template <class ResultNumber = division_result_t<NumberType>, OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr auto distanceL1(const OtherOrientedLine& other) const;
    /** @copydoc distanceL1(const OtherPoint&) const */
    template <class ResultNumber = division_result_t<NumberType>, RayConcept OtherRay>
    [[nodiscard]] constexpr auto distanceL1(const OtherRay& other) const;
    /** @copydoc distanceL1(const OtherPoint&) const */
    template <class ResultNumber = division_result_t<NumberType>, HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr auto distanceL1(const OtherHalfplane& other) const;
    /** @copydoc distanceL1(const OtherPoint&) const */
    template <class ResultNumber = division_result_t<NumberType>, RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr auto distanceL1(const OtherRectangle& other) const;
    /** @copydoc distanceL1(const OtherPoint&) const */
    template <class ResultNumber = division_result_t<NumberType>, TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr auto distanceL1(const OtherTriangle& other) const;
    /** @copydoc distanceL1(const OtherPoint&) const */
    template <class ResultNumber = division_result_t<NumberType>, ConvexConcept OtherConvex>
    [[nodiscard]] constexpr auto distanceL1(const OtherConvex& other) const;
    /** @copydoc distanceL1(const OtherPoint&) const */
    template <class ResultNumber = division_result_t<NumberType>, MonotoneChainConcept OtherChain>
    [[nodiscard]] constexpr auto distanceL1(const OtherChain& other) const;
    /** @copydoc distanceL1(const OtherPoint&) const */
    template <class ResultNumber = division_result_t<NumberType>, PolylineConcept OtherPolyline>
    [[nodiscard]] constexpr auto distanceL1(const OtherPolyline& other) const;

    /**
     * @brief Returns the Manhattan (L1) distance to the given shape.
     *
     * Forwards to the other shape's implementation so that each unordered pair
     * needs `distanceL1` defined only once, on the higher-ranked shape.
     */
    template <class ResultNumber = division_result_t<NumberType>, typename OtherShape>
        requires ((detail::shapeRank<OtherShape> > detail::shapeRank<Polyline>)
                  && requires(const OtherShape& o, const Polyline& self) {
                         o.template distanceL1<ResultNumber>(self);
                     })
    [[nodiscard]] constexpr auto distanceL1(const OtherShape& other) const {
        return other.template distanceL1<ResultNumber>(*this);
    }

    /**
     * @brief Returns the intersection of the two shapes (A ∩ B), re-dispatching
     *        through the wrapper's own `intersection`.
     *
     * An intersection is symmetric, so this just calls @p other's own
     * `intersection`, which visits its wrapped alternative and throws if the
     * pair is unsupported.
     *
     * The point type is deduced from @p other so a plain concrete shape cannot
     * reach this overload through an implicit conversion to `Shape`.
     *
     * @return The intersection wrapped in a `Shape`, rather than the tighter
     *   type the concrete pair would answer with: which alternative @p other
     *   holds is not known until run time, so neither is the result's.
     */
    template <class ResultNumber = division_result_t<NumberType>, PointConcept OtherPoint>
    [[nodiscard]] constexpr auto intersection(const Shape<OtherPoint>& other) const {
        return other.template intersection<ResultNumber>(*this);
    }

    /**
     * @brief Returns the distance to the given shape, using symmetry to
     * re-dispatch through the wrapper's own `distanceL1`.
     */
    template <class ResultNumber = double, PointConcept OtherPoint>
    [[nodiscard]] constexpr auto distanceL1(const Shape<OtherPoint>& other) const {
        return other.template distanceL1<ResultNumber>(*this);
    }

    /**
     * @brief Returns the Chebyshev (LInf) distance to the given shape.
     *
     * Zero when the shapes intersect, otherwise the minimum over the polyline
     * edges. The polyline must have at least one edge.
     *
     * @warning With an integer @p ResultNumber the exact distance is generally
     *          a fraction, so the internal division truncates. Request a
     *          floating-point or pgl::Rational result type for an accurate value.
     */
    template <class ResultNumber = division_result_t<NumberType>, PointConcept OtherPoint>
    [[nodiscard]] constexpr auto distanceLInf(const OtherPoint& point) const;
    /** @copydoc distanceLInf(const OtherPoint&) const */
    template <class ResultNumber = division_result_t<NumberType>, SegmentConcept OtherSegment>
    [[nodiscard]] constexpr auto distanceLInf(const OtherSegment& other) const;
    /** @copydoc distanceLInf(const OtherPoint&) const */
    template <class ResultNumber = division_result_t<NumberType>, OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr auto distanceLInf(const OtherOrientedSegment& other) const;
    /** @copydoc distanceLInf(const OtherPoint&) const */
    template <class ResultNumber = division_result_t<NumberType>, LineConcept OtherLine>
    [[nodiscard]] constexpr auto distanceLInf(const OtherLine& other) const;
    /** @copydoc distanceLInf(const OtherPoint&) const */
    template <class ResultNumber = division_result_t<NumberType>, OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr auto distanceLInf(const OtherOrientedLine& other) const;
    /** @copydoc distanceLInf(const OtherPoint&) const */
    template <class ResultNumber = division_result_t<NumberType>, RayConcept OtherRay>
    [[nodiscard]] constexpr auto distanceLInf(const OtherRay& other) const;
    /** @copydoc distanceLInf(const OtherPoint&) const */
    template <class ResultNumber = division_result_t<NumberType>, HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr auto distanceLInf(const OtherHalfplane& other) const;
    /** @copydoc distanceLInf(const OtherPoint&) const */
    template <class ResultNumber = division_result_t<NumberType>, RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr auto distanceLInf(const OtherRectangle& other) const;
    /** @copydoc distanceLInf(const OtherPoint&) const */
    template <class ResultNumber = division_result_t<NumberType>, TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr auto distanceLInf(const OtherTriangle& other) const;
    /** @copydoc distanceLInf(const OtherPoint&) const */
    template <class ResultNumber = division_result_t<NumberType>, ConvexConcept OtherConvex>
    [[nodiscard]] constexpr auto distanceLInf(const OtherConvex& other) const;
    /** @copydoc distanceLInf(const OtherPoint&) const */
    template <class ResultNumber = division_result_t<NumberType>, MonotoneChainConcept OtherChain>
    [[nodiscard]] constexpr auto distanceLInf(const OtherChain& other) const;
    /** @copydoc distanceLInf(const OtherPoint&) const */
    template <class ResultNumber = division_result_t<NumberType>, PolylineConcept OtherPolyline>
    [[nodiscard]] constexpr auto distanceLInf(const OtherPolyline& other) const;

    /**
     * @brief Returns the Chebyshev (LInf) distance to the given shape.
     *
     * Forwards to the other shape's implementation so that each unordered pair
     * needs `distanceLInf` defined only once, on the higher-ranked shape.
     */
    template <class ResultNumber = division_result_t<NumberType>, typename OtherShape>
        requires ((detail::shapeRank<OtherShape> > detail::shapeRank<Polyline>)
                  && requires(const OtherShape& o, const Polyline& self) {
                         o.template distanceLInf<ResultNumber>(self);
                     })
    [[nodiscard]] constexpr auto distanceLInf(const OtherShape& other) const {
        return other.template distanceLInf<ResultNumber>(*this);
    }

    /**
     * @brief Returns the distance to the given shape, using symmetry to
     * re-dispatch through the wrapper's own `distanceLInf`.
     */
    template <class ResultNumber = double, PointConcept OtherPoint>
    [[nodiscard]] constexpr auto distanceLInf(const Shape<OtherPoint>& other) const {
        return other.template distanceLInf<ResultNumber>(*this);
    }

    /**
     * @brief Computes the Euclidean length of the polyline (the sum of its edge lengths).
     *
     * A self-overlapping polyline counts every traversal of a repeated part.
     *
     * @tparam ApproximateNumber The floating-point type for the result.
     */
    template <class ApproximateNumber = double>
    ApproximateNumber length() const;

    /** @brief Computes the Manhattan (L1) length of the polyline. */
    constexpr auto lengthL1() const;

    /** @brief Computes the Chebyshev (LInf) length of the polyline. */
    constexpr auto lengthLInf() const;

    /**
     * @brief Returns a point inside the polyline.
     *
     * This is the point inside the segment formed by the first two
     * vertices, i.e. the midpoint of that edge.
     *
     * @tparam ResultNumber Coordinate type of the result.
     * @warning Divides coordinates by 2. Inexact for odd integer coordinates.
     */
    template <class ResultNumber = division_result_t<NumberType>>
    [[nodiscard]] constexpr Point<ResultNumber> pointInside() const;

    /**
     * @brief Returns the polyline rotated by 90k degrees around the origin.
     *
     * The vertex sequence, including its direction, is preserved (rotation is
     * a rigid motion).
     *
     * @param k Number of 90-degree CCW rotations (may be negative).
     * @return Rotated polyline.
     */
    [[nodiscard]] constexpr Polyline rotated90(int k = 1) const;

    /**
     * @brief Rotates the polyline by 90k degrees around the origin in place.
     *
     * @param k Number of 90-degree CCW rotations (may be negative).
     */
    constexpr void rotate90(int k = 1);

    /** @brief Returns the polyline with its x-coordinates multiplied by a factor. */
    template <class OtherNumber>
    [[nodiscard]] constexpr Polyline scaledUpX(const OtherNumber scalar) const;

    /** @brief Multiplies the polyline's x-coordinates by a factor in place. */
    template <class OtherNumber>
    constexpr void scaleUpX(const OtherNumber scalar);

    /** @brief Returns the polyline with its y-coordinates multiplied by a factor. */
    template <class OtherNumber>
    [[nodiscard]] constexpr Polyline scaledUpY(const OtherNumber scalar) const;

    /** @brief Multiplies the polyline's y-coordinates by a factor in place. */
    template <class OtherNumber>
    constexpr void scaleUpY(const OtherNumber scalar);

    /** @brief Returns the polyline with its x-coordinates divided by a divisor. */
    template <class OtherNumber>
    [[nodiscard]] constexpr Polyline scaledDownX(const OtherNumber scalar) const;

    /** @brief Divides the polyline's x-coordinates by a divisor in place. */
    template <class OtherNumber>
    constexpr void scaleDownX(const OtherNumber scalar);

    /** @brief Returns the polyline with its y-coordinates divided by a divisor. */
    template <class OtherNumber>
    [[nodiscard]] constexpr Polyline scaledDownY(const OtherNumber scalar) const;

    /** @brief Divides the polyline's y-coordinates by a divisor in place. */
    template <class OtherNumber>
    constexpr void scaleDownY(const OtherNumber scalar);

    /**
     * @brief Tests whether @p oldEdge can be flipped to @p newEdge.
     *
     * A *flip* removes one edge and adds one edge so that the result is still a
     * single open chain over the very same vertices (a Hamiltonian path move).
     * Removing an edge splits the chain into two sub-paths `A` (holding the
     * first vertex) and `B` (holding the last); the new edge rejoins them and
     * therefore has to connect an endpoint of `A` to an endpoint of `B`. With
     * `A = [p_0 .. p_i]` and `B = [p_{i+1} .. p_{n-1}]` the three non-trivial
     * reconnections are:
     * - add `(p_i, p_{n-1})` — reverses the suffix `B`;
     * - add `(p_0, p_{i+1})` — reverses the prefix `A`;
     * - add `(p_0, p_{n-1})` — reverses both.
     *
     * The flip is possible when @p oldEdge equals some edge of the polyline,
     * @p newEdge is non-degenerate, and @p newEdge is one of the reconnections
     * above (in particular @p newEdge differs from @p oldEdge — re-adding the
     * removed edge is not a flip). Both edges are compared as unordered vertex
     * pairs. In a self-intersecting polyline @p oldEdge may match several edges;
     * the first (smallest-index) edge admitting @p newEdge as a valid
     * reconnection is used.
     *
     * Complexity: O(n) for n vertices.
     *
     * @tparam OldSegment Type of the removed edge.
     * @tparam NewSegment Type of the added edge.
     * @param oldEdge Edge to remove (an existing polyline edge).
     * @param newEdge Edge to add in its place.
     * @return `true` if the flip yields a path over the same vertices.
     */
    template <SegmentConcept OldSegment, SegmentConcept NewSegment>
    [[nodiscard]] constexpr bool flippable(const OldSegment& oldEdge, const NewSegment& newEdge) const;

    /**
     * @brief Returns the polyline with @p oldEdge flipped to @p newEdge.
     *
     * See @ref flippable for the exact semantics. The flip must be possible.
     *
     * Complexity: O(n) for n vertices.
     *
     * @tparam OldSegment Type of the removed edge.
     * @tparam NewSegment Type of the added edge.
     * @param oldEdge Edge to remove (an existing polyline edge).
     * @param newEdge Edge to add in its place.
     * @return The flipped polyline.
     * @pre `flippable(oldEdge, newEdge)`.
     */
    template <SegmentConcept OldSegment, SegmentConcept NewSegment>
    [[nodiscard]] constexpr Polyline flipped(const OldSegment& oldEdge, const NewSegment& newEdge) const;

    /**
     * @brief Flips @p oldEdge to @p newEdge in place.
     *
     * See @ref flippable for the exact semantics. The flip must be possible.
     * The label is preserved.
     *
     * Complexity: O(n) for n vertices.
     *
     * @tparam OldSegment Type of the removed edge.
     * @tparam NewSegment Type of the added edge.
     * @param oldEdge Edge to remove (an existing polyline edge).
     * @param newEdge Edge to add in its place.
     * @pre `flippable(oldEdge, newEdge)`.
     */
    template <SegmentConcept OldSegment, SegmentConcept NewSegment>
    constexpr void flip(const OldSegment& oldEdge, const NewSegment& newEdge);

    /**
     * @brief Returns the Minkowski sum of this shape and another (A ⊕ B).
     *
     * The sum is the point set `{a + b : a ∈ A, b ∈ B}`. Summing with a
     * `Point` is a translation, so it returns this shape's own type; two
     * bounded convex shapes sum to a @ref Convex, or to a @ref Rectangle when
     * both are rectangles. See @ref MinkowskiSummableConcept for the pairs a
     * Minkowski sum is defined for.
     *
     * @tparam OtherShape Type of the other shape.
     * @param other Shape to sum with.
     * @return The Minkowski sum, in the tightest type that represents it.
     */
    template <class OtherShape>
        requires MinkowskiSummableConcept<Polyline<PointType_, TLabel>, OtherShape>
    [[nodiscard]] constexpr auto minkowskiSum(const OtherShape& other) const;

    /**
     * @brief Returns the Minkowski erosion of this shape by another (A ⊖ B).
     *
     * The erosion is the point set `{x : x ⊕ B ⊆ A}`, the translations of
     * @p other that keep it inside this shape -- equivalently
     * `⋂ {A - b : b ∈ B}`. It is the morphological dual of
     * @ref minkowskiSum and is defined for the same pairs, but it is **not**
     * commutative.
     *
     * Eroding by a `Point` is the translation by its negation, so it returns
     * this shape's own type; the other pairs come back as the convex region
     * they are, a @ref HalfplaneIntersection, which holds a lower-dimensional
     * erosion and the empty one as readily as a two-dimensional one.
     * A polyline has no area, so the erosion of one has none either.
     *
     * Eroding by a shape that covers no point is the whole plane, which a
     * @ref HalfplaneIntersection returns and the tighter result types cannot.
     *
     * @tparam OtherShape Type of the shape to erode by.
     * @param other Shape to erode by.
     * @return The erosion, in the tightest type that represents it.
     */
    template <class OtherShape>
        requires MinkowskiSummableConcept<Polyline<PointType_, TLabel>, OtherShape>
    [[nodiscard]] constexpr auto minkowskiErosion(const OtherShape& other) const;

    /**
     * @brief Returns the regularized Minkowski erosion of this shape by a
     *        bounded polygonal one (A ⊖ B), as a set of regions.
     *
     * The pairs @ref MinkowskiSummableConcept turns away, which for this
     * receiver is every bounded operand: the erosion of a shape that is not
     * convex is no more convex than it was, and it is not even connected --
     * a dumbbell eroded by anything wider than its handle is two regions, for
     * operands that are in no way degenerate. That is why this returns a
     * @ref PolygonSet where @ref minkowskiSum returns one
     * @ref PolygonWithHoles, and the difference is structural rather than a
     * missing guarantee.
     *
     * The result is **regularized**, `closure((A ⊖ B)°)`, as the sum and the
     * boolean operations are: an erosion produces thin material readily -- a
     * corridor exactly as wide as the operand erodes to a curve -- and a set of
     * regions holds none of it. A receiver with no area erodes to the empty set
     * for the same reason.
     *
     * A convex receiver is answered by its own constraints in `O(a·b)`;
     * everything else pays for a complement, a sum and a difference. See
     * `implementation/minkowskierosion.hpp` for both constructions and their
     * cost.
     *
     * @tparam ResultNumber Coordinate type of the result.
     * @tparam OtherShape Type of the shape to erode by.
     * @param other Shape to erode by.
     * @return The erosion, as a @ref PolygonSet.
     * @throws std::logic_error when @p other covers no point: that erosion is
     *         the whole plane, which no set of bounded regions represents.
     */
    template <class ResultNumber = division_result_t<NumberType>, class OtherShape>
        requires (!MinkowskiSummableConcept<Polyline<PointType_, TLabel>, OtherShape> &&
                  BoundedPolygonalConcept<OtherShape>)
    [[nodiscard]] PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
    minkowskiErosion(const OtherShape& other) const;

    /**
     * @brief Returns the regularized Minkowski sum of the two shapes (A ⊕ B).
     *
     * The sum is `{p + q : p ∈ A, q ∈ B}`, regularized to `closure((A ⊕ B)°)`
     * and returned as a region with holes. A
     * polyline has no area of its own, but sweeping another shape along it has
     * some: the chain is one-dimensional and may bend back on itself, so the
     * swept material can close over a hole — a closed chain is the plainest
     * example, and an open one whose ends come within the summand's reach of each
     * other does it too — and no other shape in the library can say so.
     *
     * **The operand must be a body** — the closure of a connected, non-empty
     * interior — and it is the operand that has to be one, since the receiver
     * never is. A body thickens the whole polyline: the sum covers
     * `⋃_{a ∈ A} (a + B°)`, which is connected along the chain and open, and the
     * sum is its closure. So the answer is one region. A degenerate operand — a
     * `Rectangle` collapsed to a segment, say — is not on this contract: the
     * regularization can then leave the sum in pieces, of which one comes back.
     * Sum with a @ref Segment instead for the pair that is contracted for two
     * thin operands and returns every piece. An empty or wholly flat regularized
     * sum is the empty region.
     *
     * Distinguish this from @ref minkowskiSum(const OtherShape&) const, which
     * sums a `Point` — a translation, giving back a `Polyline` — and nothing
     * else: a polyline is not convex, so @ref MinkowskiSummableConcept rejects
     * every other pair. A second `Polyline` is not an operand.
     *
     * Complexity: one convex merge per edge of the polyline, then a constrained
     * triangulation over the arrangement of all of them.
     *
     * @tparam ResultNumber The number type for the result.
     * @param other The shape to sum with.
     * @return The sum, as one region.
     * @pre The operand is a body.
     * @note Every vertex of every piece sum is a sum of two input vertices, so
     *       the pieces are exact; only their union can put a vertex at a
     *       crossing, and that arrangement is built over exact rationals and
     *       converted to @p ResultNumber only at the end.
     */
    template <class ResultNumber = division_result_t<NumberType>, TriangleConcept OtherTriangle>
    [[nodiscard]] PolygonWithHoles<Point<ResultNumber, typename PointType::LabelType>>
    minkowskiSum(const OtherTriangle& other) const;

    /** @brief Returns the regularized Minkowski sum of the two shapes (A ⊕ B). */
    template <class ResultNumber = division_result_t<NumberType>, RectangleConcept OtherRectangle>
    [[nodiscard]] PolygonWithHoles<Point<ResultNumber, typename PointType::LabelType>>
    minkowskiSum(const OtherRectangle& other) const;

    /** @brief Returns the regularized Minkowski sum of the two shapes (A ⊕ B). */
    template <class ResultNumber = division_result_t<NumberType>, ConvexConcept OtherConvex>
    [[nodiscard]] PolygonWithHoles<Point<ResultNumber, typename PointType::LabelType>>
    minkowskiSum(const OtherConvex& other) const;

    /**
     * @brief Returns the regularized Minkowski sum of the two shapes (A ⊕ B).
     *
     * The one non-convex summand a chain takes: a `Polygon` reaches around the
     * chain as readily as the chain bends around it, so here either operand's
     * concavity can strand a cavity, where with a convex summand only the chain's
     * turns can. It is decomposed the way the region-valued sums decompose it —
     * the triangles of its triangulated domain, or its edges when it has no area —
     * while the chain contributes its edges.
     *
     * Complexity: one convex merge per pair of chain edge and operand triangle,
     * then a constrained triangulation over the arrangement of all of them.
     */
    template <class ResultNumber = division_result_t<NumberType>, PolygonConcept OtherPolygon>
    [[nodiscard]] PolygonWithHoles<Point<ResultNumber, typename PointType::LabelType>>
    minkowskiSum(const OtherPolygon& other) const;

    /**
     * @brief Returns the regularized Minkowski sum of the two shapes (A ⊕ B).
     *
     * A region summand brings what it brings to every sum: its holes need no
     * handling, being simply where its decomposition has no piece, while its
     * **slits** do — a stretch of boundary two of its rings share carries no area
     * beside it and yet sweeps out area along the chain exactly as a triangle
     * does, so it joins the decomposition.
     *
     * The one case worth knowing is a slit the chain drags *along the slit's own
     * direction*: that sweep is a segment, so the regularization drops it and the
     * answer is strictly smaller than `A ⊕ B`. It is the same rule that makes a
     * summand with no area at all come back empty, and it is reachable here from a
     * perfectly ordinary rectilinear region and chain.
     *
     * That case is also the one that can cost a region its **body**, which is
     * what this overload's single-region result is contracted on: a slit cutting
     * the operand's interior in two leaves the sum in as many pieces, of which
     * one comes back. A region whose slits leave its interior connected — or
     * that has none — is a body and is on the contract.
     *
     * Complexity: one convex merge per pair of chain edge and operand piece, then
     * a constrained triangulation over the arrangement of all of them.
     */
    template <class ResultNumber = division_result_t<NumberType>, PolygonWithHolesConcept OtherRegion>
    [[nodiscard]] PolygonWithHoles<Point<ResultNumber, typename PointType::LabelType>>
    minkowskiSum(const OtherRegion& other) const;

    /**
     * @brief Returns the regularized Minkowski sum of the two shapes (A ⊕ B).
     *
     * The one operand here with no area of its own, and the sum still has some:
     * an edge of the chain summed with the segment is the parallelogram they
     * span, which is flat only where the two are parallel. So a chain that bends
     * sweeps a band of material along itself whichever direction the segment
     * points, and that band closes over a cavity exactly as a summand with area
     * does — a closed chain still comes back as a region with a hole.
     *
     * This is the pair where the regularization is easiest to trip over, because
     * the receiver's own edges are what sweep. An edge *parallel* to the segment
     * sweeps a segment, which the regularization drops: a straight chain summed
     * along its own direction comes back empty — as does any chain against a
     * summand collapsed to a point — and a closed rectilinear chain summed with an
     * axis-parallel segment comes back as **two disjoint regions**, the sweeps of
     * the two edges across it, with what joined them dropped. Every one of those
     * is `A ⊕ B` losing what has no area, not the sum being unsupported.
     *
     * Complexity: one convex merge per edge of the chain, then a constrained
     * triangulation over the arrangement of all of them.
     */
    template <class ResultNumber = division_result_t<NumberType>, SegmentConcept OtherSegment>
    [[nodiscard]] PolygonSet<Point<ResultNumber, typename PointType::LabelType>>
    minkowskiSum(const OtherSegment& other) const;

    /**
     * @brief Returns the regularized Minkowski sum of the two shapes (A ⊕ B).
     *
     * An orientation is not part of a point set, so this is the sum with the
     * underlying segment, vertex for vertex.
     */
    template <class ResultNumber = division_result_t<NumberType>, OrientedSegmentConcept OtherSegment>
    [[nodiscard]] PolygonSet<Point<ResultNumber, typename PointType::LabelType>>
    minkowskiSum(const OtherSegment& other) const;

    /**
     * @brief Returns the regularized Minkowski sum of the two chains (A ⊕ B).
     *
     * Two chains, and between them not a scrap of area — and the sum has some
     * anyway, for the reason a segment operand does: an edge of each spans a
     * parallelogram unless the two are parallel. It is the same construction as
     * every other sum here, with *both* operands contributing their edges as
     * their decomposition, and the same regularized, set-valued contract that
     * @ref minkowskiSum(const OtherSegment&) const has — a segment operand being
     * the one-edge case of this one.
     *
     * Nothing about a valid pair of chains keeps the answer in one piece, so
     * there is no body here and no precondition to observe: two chains that
     * share a direction sweep out nothing along it, and what is left can be in
     * pieces or empty. Two parallel segments spelled as chains are the smallest
     * example, and they come back empty.
     *
     * Complexity: one convex merge per pair of edges, then a constrained
     * triangulation over the arrangement of all of them.
     *
     * @tparam ResultNumber The number type for the result.
     * @param other The chain to sum with.
     * @return The pieces of the sum, in canonical order.
     */
    template <class ResultNumber = division_result_t<NumberType>, PolylineConcept OtherPolyline>
    [[nodiscard]] PolygonSet<Point<ResultNumber, typename PointType::LabelType>>
    minkowskiSum(const OtherPolyline& other) const;

    /**
     * @brief Returns the regularized Minkowski sum of the two chains (A ⊕ B).
     *
     * A @ref MonotoneChain operand is a polyline that happens to be sorted, and
     * it sums here exactly as one: its monotonicity buys nothing against a
     * receiver that may bend back on itself. A polyline outranks a chain and so
     * owns the pair, which the chain's own forwarder reaches.
     */
    template <class ResultNumber = division_result_t<NumberType>, MonotoneChainConcept OtherChain>
    [[nodiscard]] PolygonSet<Point<ResultNumber, typename PointType::LabelType>>
    minkowskiSum(const OtherChain& other) const;

    /**
     * @brief Returns the regularized Minkowski sum of the two shapes (A ⊕ B),
     *        as a set of regions.
     *
     * A @ref PolygonSet operand is the one whose answer needs a set whatever the
     * other operand is: its components are disjoint, and a sum small relative to
     * the gaps between them leaves them so. The set outranks every shape here
     * and owns the pair, so this is the mirror spelling of
     * @ref PolygonSet::minkowskiSum, and the same call.
     */
    template <class ResultNumber = division_result_t<NumberType>, PolygonSetConcept OtherSet>
    [[nodiscard]] PolygonSet<Point<ResultNumber, typename PointType::LabelType>>
    minkowskiSum(const OtherSet& other) const;

    /**
     * @brief Translates the polyline by the given point.
     *
     * Complexity: O(1).
     */
    template<PointConcept OtherPoint>
    constexpr Polyline& operator+=(const OtherPoint& translation) {
        translation_ += translation;
        // A pure translation merely shifts the bounding box, so update the
        // cached bbox in place rather than discarding it. The hash, however,
        // depends on the absolute vertex positions, so it must be invalidated.
        if (!bbox_.empty()) {
            bbox_ += translation;
        }
        hash_ = hashUnset_;
        return *this;
    }

    /**
     * @brief Translates the polyline by the negation of the given point.
     *
     * Complexity: O(1).
     */
    template<PointConcept OtherPoint>
    constexpr Polyline& operator-=(const OtherPoint& translation) {
        translation_ -= translation;
        if (!bbox_.empty()) {
            bbox_ -= translation;
        }
        hash_ = hashUnset_;
        return *this;
    }

    /**
     * @brief Scales the polyline by the given scalar.
     *
     * Complexity: O(n) for n vertices. The traversal order is preserved, even
     * for a negative factor, which merely reverses the lexicographic order of
     * the extremes.
     */
    template <class Scalar>
        requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
    constexpr Polyline& operator*=(const Scalar& scalar) {
        for (auto& vertex : points_) {
            vertex *= scalar;
        }
        translation_ *= scalar;
        resetCache();
        return *this;
    }

    /**
     * @brief Divides the polyline by the given scalar.
     *
     * Complexity: O(n) for n vertices; preserves the traversal order like
     * @ref operator*=.
     */
    template <class Scalar>
        requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
    constexpr Polyline& operator/=(const Scalar& scalar) {
        for (auto& vertex : points_) {
            vertex /= scalar;
        }
        translation_ /= scalar;
        resetCache();
        return *this;
    }

    /**
     * @brief Forward iterator over the (optionally oriented) polyline edges.
     *
     * Edge `i` joins vertex `i` to vertex `i + 1`, so a polyline with `size()`
     * vertices has `size() - 1` edges (and none when it has fewer than two
     * vertices).
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
            assert(polyline != nullptr);
            return polyline->template boundaryAt<Oriented>(index);
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
        friend struct Polyline;

        constexpr BoundaryIterator(const Polyline* polyline_arg, std::size_t index_arg)
            : polyline(polyline_arg), index(index_arg) {}

        const Polyline* polyline = nullptr;
        std::size_t index = 0;
    };

  private:
    std::vector<PointType> points_{};
    [[no_unique_address]] mutable LabelType label_{};
    PointType translation_{};
    // Lazily computed bounding box, invalidated by resetCache() on every
    // mutation. The empty rectangle doubles as "not computed yet": a shape
    // whose box is genuinely empty has no vertices, so bbox() re-derives it
    // with one size check rather than any real work.
    mutable Rectangle<PointType> bbox_{};

    // Memoized hash, computed lazily by std::hash<Polyline>. hashUnset_
    // means "not yet computed"; SIZE_MAX is chosen as the sentinel because it
    // is a rare hash output, and the one true hash that would collide with it
    // is remapped to hashUnset_ - 1 so the sentinel is never stored as a real
    // value. Unlike the bbox, the hash is not translation-invariant, so
    // operator+=/-= reset it.
    static constexpr std::size_t hashUnset_ = pgl::detail::numeric_limits<std::size_t>::max();
    mutable std::size_t hash_ = hashUnset_;
    friend struct std::hash<Polyline>;

    // Drops the memoized caches; call after any operation that mutates the
    // polyline's vertices. A pure translation does not need to drop bbox_ (it
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
     * @brief Minimum of the given distance over the polyline edges.
     *
     * The distance to a shape the polyline does not intersect is attained on
     * some edge, so the fold is exact. Requires at least one edge; the caller
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
     * @brief Folds a shape's segment intersection over the polyline edges and
     * coalesces the pieces (shared by the single-piece `intersection` overloads).
     */
    template <class ResultNumber, class OtherShape>
    constexpr std::vector<std::variant<Point<ResultNumber, typename PointType::LabelType>,
                                       Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    edgeFoldIntersection(const OtherShape& other) const;

    /**
     * @brief Coalesces raw intersection pieces: collinear touching segments
     * merge, and points covered by a segment or by an equal point are dropped;
     * the survivors are sorted by lexicographic minimum (shared by every
     * piecewise `intersection`).
     *
     * Unlike MonotoneChain::coalescePieces, the pieces of a self-intersecting
     * polyline need not lie on one monotone arc, so a piece's absorber may sit
     * anywhere in the list, not just next to it in sorted order; every piece is
     * therefore tested against every kept segment (quadratic in the piece count).
     */
    template <class ResultNumber>
    static constexpr std::vector<std::variant<Point<ResultNumber, typename PointType::LabelType>,
                                              Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    coalescePieces(std::vector<std::variant<Point<ResultNumber, typename PointType::LabelType>,
                                            Segment<Point<ResultNumber, typename PointType::LabelType>>>> pieces);

    template <bool Oriented>
    constexpr BoundaryType<Oriented> boundaryAt(std::size_t index) const {
        assert(index + 1 < size());
        return BoundaryType<Oriented>((*this)[index], (*this)[index + 1]);
    }

    /**
     * @brief Computes the vertex sequence resulting from flipping @p oldEdge to
     * @p newEdge, or `std::nullopt` when the flip is not possible.
     *
     * Shared by @ref flippable, @ref flipped and @ref flip; see @ref flippable
     * for the semantics. Vertices are returned with the translation applied.
     */
    template <SegmentConcept OldSegment, SegmentConcept NewSegment>
    constexpr std::optional<std::vector<PointType>> flipVertices(const OldSegment& oldEdge,
                                                                 const NewSegment& newEdge) const;

    /**
     * @brief Tests whether the stored vertex sequence is in canonical
     * direction: not lexicographically larger than its reversal.
     *
     * The stored sequence keeps the user's traversal order, so this is not an
     * invariant but a question asked on demand by the direction-agnostic
     * comparators and by std::hash, which read the vertices through
     * @ref canonicalAt.
     *
     * Complexity: O(n) for n vertices.
     */
    constexpr bool storedIsCanonical() const {
        if (points_.size() < 2) {
            return true;
        }
        const auto cmp = points_.front() <=> points_.back();
        if (cmp < 0) {
            return true;
        }
        if (cmp > 0) {
            return false;
        }
        // Equal extremes: the tie is broken by the full sequences.
        return !std::lexicographical_compare(points_.rbegin(), points_.rend(),
                                             points_.begin(), points_.end());
    }

    /**
     * @brief Reads vertex @p index in canonical direction.
     *
     * @param index Index in the canonical sequence.
     * @param reversed `!storedIsCanonical()`, computed once by the caller so a
     *        full comparison stays linear.
     */
    constexpr PointType canonicalAt(std::size_t index, bool reversed) const {
        return (*this)[reversed ? size() - 1 - index : index];
    }

    class Iterator {
    private:
        using BaseIterator = std::vector<PointType>::const_iterator;
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
        auto operator<=>(const Iterator& other) const {
            return it <=> other.it;
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
}; // struct Polyline

// --- asPolyline conversions, defined here now that Polyline is complete ---

template <class TPoint, class TLabel>
constexpr Polyline<typename Segment<TPoint, TLabel>::PointType>
Segment<TPoint, TLabel>::asPolyline() const {
    // The polyline traverses the segment from min() to max().
    return Polyline<PointType>(vertices());
}

template <class PointType_, class TLabel, class Storage>
constexpr Polyline<typename MonotoneChain<PointType_, TLabel, Storage>::PointType>
MonotoneChain<PointType_, TLabel, Storage>::asPolyline() const {
    // The polyline traverses the chain in its lexicographic vertex order.
    return Polyline<PointType>(vertices());
}

template <class PointType, class LabelType, class TranslationNumber, class TranslationLabel>
constexpr auto operator-(const Polyline<PointType, LabelType>& polyline, const Point<TranslationNumber, TranslationLabel>& translation) {
    return polyline + (-translation);
}

template <class PointType, class LabelType, class Scalar>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr auto operator*(const Polyline<PointType, LabelType>& polyline, const Scalar& scalar) {
    using ResultPointType = Point<decltype(std::declval<PointType>().x() * scalar), typename PointType::LabelType>;
    Polyline<ResultPointType, LabelType> result(polyline);
    result *= scalar;
    if constexpr (detail::has_label_v<LabelType>) {
        result.label() = LabelType{};
    }
    return result;
}

template <class Scalar, class PointType, class LabelType>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr auto operator*(const Scalar& scalar, const Polyline<PointType, LabelType>& polyline) {
    return polyline * scalar;
}

template <class PointType, class LabelType, class Scalar>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr auto operator/(const Polyline<PointType, LabelType>& polyline, const Scalar& scalar) {
    using ResultPointType = Point<decltype(std::declval<PointType>().x() / scalar), typename PointType::LabelType>;
    Polyline<ResultPointType, LabelType> result(polyline);
    result /= scalar;
    if constexpr (detail::has_label_v<LabelType>) {
        result.label() = LabelType{};
    }
    return result;
}

template <class PointType, class LabelType>
std::ostream& operator<<(std::ostream& stream, const Polyline<PointType, LabelType>& polyline);

}  // namespace pgl
