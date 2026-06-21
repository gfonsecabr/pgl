#pragma once

#include "shape/disk.hpp"

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
#include <type_traits>
#include <utility>


namespace pgl::detail {
/**
 * @brief Finds the iterator pointing to the maximum element of a cyclic unimodal range.
 *
 * A range is cyclic unimodal if some rotation of it is unimodal: weakly increasing
 * up to a single maximum and then weakly decreasing. The rotation amount is unknown
 * (and may be zero), so the maximum can sit anywhere in the stored range.
 *
 * The binary search picks the half that must contain the maximum. A naive peak
 * search (follow the local slope) is not enough: when the rotation puts the
 * descending-to-ascending seam inside the search window, the local slope points
 * the wrong way. Comparing the two window endpoints with the probed values
 * disambiguates which side the global maximum lies on.
 *
 * @note Correct for plateaus of length up to two (e.g. a polygon edge parallel to
 * the query direction). Arbitrarily long plateaus — a spike inside a flat region —
 * cannot be located in O(log n) and are outside the contract.
 *
 * @tparam RandomIt Random access iterator type.
 * @tparam Key Key extraction function type.
 * @param first Iterator to the beginning of the range.
 * @param last Iterator to the end of the range.
 * @param key Optional key function applied to elements.
 * @return Iterator pointing to a maximum element, or last if the range is empty.
 * @complexity O(log n)
 */
template <class RandomIt, class Key = std::identity>
constexpr RandomIt cyclicMax(RandomIt first, RandomIt last, Key key = {}) {
    using diff_t = typename std::iterator_traits<RandomIt>::difference_type;
    const diff_t n = last - first;

    if (n == 0)
        return last;

    diff_t lo = 0;
    diff_t hi = n - 1;

    while (lo < hi) {
        const diff_t mid = lo + (hi - lo) / 2;
        const auto k_lo = key(first[lo]);
        const auto k_hi = key(first[hi]);
        const auto k_mid = key(first[mid]);
        const auto k_next = key(first[mid + 1]);

        if (k_mid < k_next) {
            // Ascending at mid: head right, unless the left endpoint dominates both
            // the ongoing ascent and the right endpoint (then the peak is on the left).
            if (k_next > k_lo || k_hi > k_lo)
                lo = mid + 1;
            else
                hi = mid;
        } else {
            // Descending at mid: head left, unless the right endpoint dominates both
            // mid and the left endpoint (then the peak is on the right).
            if (k_mid > k_hi || k_lo > k_hi)
                hi = mid;
            else
                lo = mid + 1;
        }
    }

    return first + lo;
}

/**
 * @brief Finds a maximum element of a cyclic unimodal range, returning early on a positive key.
 *
 * Identical search to @ref cyclicMax (correct for any rotation), with one shortcut:
 * if any probed element has a strictly positive key, that element is returned right
 * away. Callers that only need to know whether *some* element is positive (e.g. a
 * convex vertex strictly to one side of a line) can therefore stop early; when no
 * positive key exists the function still returns a maximum (which is then <= 0).
 *
 * @tparam RandomIt Random access iterator type.
 * @tparam Key Key extraction function type.
 * @param first Iterator to the beginning of the range.
 * @param last Iterator to the end of the range.
 * @param key Optional key function applied to elements.
 * @return Iterator to an element with positive key if one is probed, otherwise to a
 *         maximum element. Returns @p last if the range is empty.
 * @complexity O(log n)
 */
template <class RandomIt, class Key = std::identity>
constexpr RandomIt cyclicMaxOrPositive(RandomIt first, RandomIt last, Key key = {}) {
    using diff_t = typename std::iterator_traits<RandomIt>::difference_type;
    const diff_t n = last - first;

    if (n == 0)
        return last;

    diff_t lo = 0;
    diff_t hi = n - 1;

    while (lo < hi) {
        const diff_t mid = lo + (hi - lo) / 2;
        const auto k_lo = key(first[lo]);
        if (k_lo > 0) return first + lo;
        const auto k_hi = key(first[hi]);
        if (k_hi > 0) return first + hi;
        const auto k_mid = key(first[mid]);
        if (k_mid > 0) return first + mid;
        const auto k_next = key(first[mid + 1]);
        if (k_next > 0) return first + mid + 1;

        if (k_mid < k_next) {
            // Ascending at mid: head right, unless the left endpoint dominates both
            // the ongoing ascent and the right endpoint (then the peak is on the left).
            if (k_next > k_lo || k_hi > k_lo)
                lo = mid + 1;
            else
                hi = mid;
        } else {
            // Descending at mid: head left, unless the right endpoint dominates both
            // mid and the left endpoint (then the peak is on the right).
            if (k_mid > k_hi || k_lo > k_hi)
                hi = mid;
            else
                lo = mid + 1;
        }
    }

    return first + lo;
}


} // namespace pgl::detail


namespace pgl {

template <class PointType = Point<>, class Label>
struct Convex;

Convex() -> Convex<Point<>, NoLabel>;

template <std::ranges::input_range Range>
requires detail::is_point_v<std::ranges::range_value_t<Range>>
Convex(Range&&) -> Convex<std::remove_cvref_t<std::ranges::range_value_t<Range>>, NoLabel>;

template <class Number>
requires (!detail::is_point_v<Number>)
Convex(std::initializer_list<Number>) -> Convex<Point<Number>, NoLabel>;

template <class Number>
requires (!detail::is_point_v<Number>)
Convex(std::initializer_list<Number>, bool) -> Convex<Point<Number>, NoLabel>;


template <class PointType_, class TLabel>
struct Convex {
    using PointType = PointType_;
    using NumberType = PointType::NumberType;
    using LabelType = TLabel;
    static_assert(detail::is_point_v<PointType>, "Convex requires pgl::Point vertices");

    template <bool Oriented>
    using BoundaryType = std::conditional_t<Oriented, OrientedSegment<PointType>, Segment<PointType>>;

    template <bool Oriented>
    class BoundaryIterator;

    using EdgeIterator = BoundaryIterator<false>;
    using OrientedEdgeIterator = BoundaryIterator<true>;

    /**
     * @brief Creates a convex with no vertex.
     */
    constexpr Convex() = default;

    /**
     * @brief Creates a convex from a range of points.
     *
     * @tparam Range Input range whose elements can be converted to @ref PointType.
     * @param points Range of points to enclose.
     * @param trusted Set to true if the points are already convex hull vertices starting from the leftmost and ccw.
     */
    template<std::ranges::input_range Range = std::initializer_list<PointType>>
    requires std::ranges::common_range<Range> &&
             std::convertible_to<std::ranges::range_value_t<Range>, PointType>
    constexpr explicit Convex(Range&& points, bool trusted = false) {
        if (trusted) {
            points_.reserve(points.size());
            for (const auto &p : points) {
                points_.push_back(p);
            }
        }
        else {
            points_ = grahamScan(points);
        }
    }

    /**
     * @brief Creates a convex from a flat list of coordinates.
     *
     * The values are consumed in pairs `(x0, y0, x1, y1, …)`, each pair forming
     * one point, so the list must hold an even number of values.
     *
     * @param coords Interleaved x/y coordinates of the points to enclose.
     * @param trusted Set to true if the points are already convex hull vertices starting from the leftmost and ccw.
     */
    constexpr explicit Convex(std::initializer_list<NumberType> coords, bool trusted = false) {
        assert(coords.size() % 2 == 0);
        std::vector<PointType> points;
        points.reserve(coords.size() / 2);
        for (auto it = coords.begin(); it != coords.end(); ) {
            NumberType x = *it++;
            NumberType y = *it++;
            points.emplace_back(x, y);
        }
        if (trusted) {
            points_ = std::move(points);
        }
        else {
            points_ = grahamScan(points);
        }
    }

    /**
     * @brief Converts a convex with compatible vertex type.
     *
     * @tparam OtherPointType Source vertex type.
     * @param other Source convex.
     */
    template<PointConcept OtherPointType, class OtherLabelType>
        requires(std::constructible_from<PointType, const OtherPointType&>)
    constexpr Convex(const Convex<OtherPointType, OtherLabelType>& other)
        : points_(other.begin(), other.end()), label_(detail::copyLabel<LabelType>(other)) {}

    /**
     * @brief Returns the convex-polygon label.
     *
     * The label is mutable even through a const convex polygon: it is metadata
     * that does not participate in equality, hashing, or geometric predicates.
     *
     * @return Reference to the stored label.
     */
    template <class A = LabelType>
        requires(detail::has_label_v<A>)
    constexpr A& label() const {
        return label_;
    }

    /**
     * @brief Accesses a vertex by index.
     * @param index The index of the vertex.
     * @return A constant reference to the vertex at the given index.
     */
    constexpr const PointType operator[](std::size_t index) const {
        assert(index < size());
        return points_[index] + translation_;
    }

    /**
     * @brief Cyclic access: same as @ref operator[] but `index` is taken
     * modulo @ref size(); negative indices wrap from the end. Useful for
     * iterating polygon edges where the last edge wraps around.
     */
    constexpr PointType get(std::ptrdiff_t index) const {
        const std::ptrdiff_t n = static_cast<std::ptrdiff_t>(size());
        return (*this)[static_cast<std::size_t>(((index % n) + n) % n)];
    }

    /**
     * @brief Returns a constant iterator to the beginning of vertices.
     * @return Constant iterator to the first vertex.
     */
    constexpr auto begin() const {
        return Iterator(points_.begin(), translation_);
    }

    /**
     * @brief Returns a constant iterator to the beginning of vertices.
     * @return Constant iterator to the first vertex.
     */
    constexpr auto cbegin() const {
        return Iterator(points_.cbegin(), translation_);
    }

    /**
     * @brief Returns a constant iterator to the end of vertices.
     * @return Constant iterator past the last vertex.
     */
    constexpr auto end() const {
        return Iterator(points_.end(), translation_);
    }

    /**
     * @brief Returns a constant iterator to the end of vertices.
     * @return Constant iterator past the last vertex.
     */
    constexpr auto cend() const {
        return Iterator(points_.cend(), translation_);
    }

    /**
     * @brief Compares two convex polygons.
     * @param other The other convex polygon to compare with.
     * @return The three-way comparison result.
     */
    constexpr auto operator<=>(const Convex& other) const {
        if (auto cmp = points_.size() <=> other.points_.size(); cmp != 0) {
            return cmp;
        }
        for (std::size_t i = 0; i < points_.size(); ++i) {
            if (auto cmp = points_[i] + translation_ <=> other.points_[i] + other.translation_; cmp != 0) {
                return cmp;
            }
        }
        return std::strong_ordering::equal;
    }

    /**
     * @brief Checks equality of two convex polygons.
     * @param other The other convex polygon to compare with.
     * @return True if both polygons have the same vertices in the same order.
     */
    constexpr bool operator==(const Convex& other) const {
        if (points_.size() != other.points_.size()) {
            return false;
        }
        for (std::size_t i = 0; i < points_.size(); ++i) {
            if (points_[i] + translation_ != other.points_[i] + other.translation_) {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief Computes twice the area of the convex polygon.
     * @return Twice the area of the convex polygon.
     */
    constexpr auto twiceArea() const;

    /**
     * @brief Computes the area of the convex polygon.
     * @return The area of the convex polygon.
     * @warning Uses division by 2.
     */
    template <class ResultNumber = NumberType>  
    constexpr auto area() const;

    /**
     * @brief Checks if the convex polygon is degenerate (has zero area).
     * @return True if the convex polygon is degenerate, false otherwise.
     */
    constexpr bool isDegenerate() const;

    /**
     * @brief Computes the bounding box of the convex polygon.
     *
     * The result is computed on the first call and cached in @ref bbox_; later
     * calls return the stored value. Any operation that modifies the convex
     * resets the cache.
     *
     * Complexity: O(log n) for n vertices on the first call, O(1) thereafter.
     *
     * @return A constant reference to the rectangle representing the bounding box.
     */
    constexpr const Rectangle<PointType>& bbox() const;

    /**
     * @brief Computes the floating-point bounding box of the convex polygon.
     * @tparam ResultNumber The floating-point type for the result.
     * @return A rectangle with floating-point coordinates representing the bounding box.
     */
    template <std::floating_point ResultNumber = double>
    constexpr Rectangle<Point<ResultNumber>> fbox() const;

    /**
     * @brief Returns the vertices of the convex polygon.
     * @return A vector of vertices.
     */
    constexpr const std::vector<PointType> vertices() const {
        auto ret = points_;
        for (auto& vertex : ret) {
            vertex += translation_;
        }
        return ret;
    }

    /**
     * @brief Returns the edges of the convex polygon.
     * @return A vector of segments representing the edges.
     */
    constexpr std::vector<Segment<PointType>> edges() const {
        std::vector<Segment<PointType>> result;
        auto translatedVertices = vertices();
        for (std::size_t i = 0; i < translatedVertices.size(); ++i) {
            const auto& p1 = translatedVertices[i];
            const auto& p2 = translatedVertices[(i + 1) % translatedVertices.size()];
            result.emplace_back(p1, p2);
        }
        return result;
    }

    /**
     * @brief Returns the oriented edges of the convex polygon.
     * @return A vector of oriented segments representing the edges.
     */
    constexpr std::vector<OrientedSegment<PointType>> orientedEdges() const {
        std::vector<OrientedSegment<PointType>> result;
        auto translatedVertices = vertices();
        for (std::size_t i = 0; i < translatedVertices.size(); ++i) {
            const auto& p1 = translatedVertices[i];
            const auto& p2 = translatedVertices[(i + 1) % translatedVertices.size()];
            result.emplace_back(p1, p2);
        }
        return result;
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
        return EdgeIterator(this, size());
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
        return OrientedEdgeIterator(this, size());
    }

    /**
     * @brief Converts the convex polygon to a simple polygon.
     *
     * The vertices already follow the canonical polygon order
     * (counterclockwise, lexicographically smallest first).
     *
     * @return Polygon with the same vertices.
     */
    [[nodiscard]] constexpr explicit operator Polygon<PointType>() const {
        return Polygon<PointType>(*this, !isDegenerate());
    }

    /**
     * @brief Returns the convex polygon as a simple polygon.
     *
     * @return Polygon with the same vertices.
     */
    [[nodiscard]] constexpr Polygon<PointType> asPolygon() const {
        return static_cast<Polygon<PointType>>(*this);
    }

    /**
     * @brief Computes the centroid of the convex polygon.
     * @tparam ResultNumber The number type for the result.
     * @return The centroid point.
     * @warning Uses division by 3 and twice the area, so the result may be inexact even for floating-point types.
     */
    template <class ResultNumber = NumberType>
    constexpr Point<ResultNumber> centroid() const;

    /**
     * @brief Computes the centroid of the vertex set.
     * @tparam ResultNumber The number type for the result.
     * @return The centroid of the vertex set.
     * @warning Uses division by the number of vertices, so the result may be inexact even for floating-point types.
     */
    template <class ResultNumber = NumberType>
    constexpr Point<ResultNumber> verticesCentroid() const;


    /**
     * @brief Returns a point inside the convex polygon.
     * 
     * Complexity: O(1).
     * 
     * @tparam ResultNumber The number type for the result.
     * @return A point guaranteed to be inside the convex polygon.
     * @warning Divides coordinates by 4. Inexact for integer coordinates not divisible by 4.
     */
    template <class ResultNumber = NumberType>
    constexpr Point<ResultNumber> pointInside() const;

    /**
     * @brief Returns the number of vertices in the convex polygon.
     * @return The number of vertices.
     */
    size_t size() const {
        return points_.size();
    }

    /**
     * @brief Returns the index of the maximum vertex (rightmost and highest in case of ties).
     * 
     * Complexity: O(log n) for n vertices.
     * 
     * @return The maximum vertex index.
     */
    constexpr size_t maxIndex() const;

    /**
     * @brief Checks if the vertices list contains the given point.
     * 
     * Complexity: O(log n) for n vertices.
     * 
     * @tparam OtherPoint The point type to check.
     * @param point The point to check.
     * @return True if the point is a vertex, false otherwise.
     */
    template<PointConcept OtherPoint>
    constexpr bool verticesContain(const OtherPoint& point) const;

    /**
     * @brief Returns the smallest index `i` with `(*this)[i] == point`, or
     * `-1` if `point` is not a vertex.
     *
     * Complexity: O(log n) for n vertices, via binary search over the two
     * monotone boundary chains (mirrors @ref verticesContain).
     *
     * @param point The vertex to locate.
     * @return The vertex index, or `-1` if `point` is not a vertex.
     */
    constexpr std::ptrdiff_t index(const PointType& point) const;

    /**
     * @brief Returns every antipodal vertex-index pair, via rotating calipers.
     *
     * A pair of vertices is *antipodal* when the polygon admits two parallel
     * supporting lines, one through each vertex. The pairs are produced with a
     * single rotating-calipers sweep over the CCW boundary. Each unordered pair
     * is reported once as `{i, j}` with `i < j`, where the indices refer to the
     * CCW vertex order exposed by @ref operator[]. A convex polygon has at most
     * `3n/2` antipodal pairs.
     *
     * Complexity: O(n) for n vertices.
     *
     * @return Vector of antipodal index pairs (empty for fewer than two vertices).
     */
    constexpr std::vector<std::pair<std::size_t, std::size_t>>
    antipodalPairs() const;

    /**
     * @brief Returns a segment realizing the diameter (the farthest vertex pair).
     *
     * The farthest pair of vertices of a convex polygon is always antipodal, so
     * the diameter is the longest segment over @ref antipodalPairs(). Distances
     * are compared exactly via squared length.
     *
     * Complexity: O(n) for n vertices.
     *
     * @return A longest segment between two vertices (degenerate if fewer than
     *         two vertices).
     */
    constexpr Segment<PointType> diameter() const;


    /**
     * @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B).
     * 
     * Complexity: O(log n) for n vertices.
     * 
     * @tparam OtherPoint The point type to check.
     * @param point The point to check.
     * @return True if the point is on the boundary, false otherwise.
     */
    template<PointConcept OtherPoint>
    constexpr bool boundaryContains(const OtherPoint& point) const;

    /**
     * @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B).
     *
     * Complexity: O(log n) for n vertices.
     *
     * Uses three O(log n) point-membership queries: both endpoints must lie
     * on the boundary, and so must the midpoint. For a convex polygon, a
     * segment with all three on the boundary necessarily lies on a single
     * edge — otherwise it would be a chord passing through the interior.
     *
     * @tparam OtherPoint The point type of the segment.
     * @param other The segment to check.
     * @return True if the segment lies on the convex polygon boundary.
     */
    template<SegmentConcept OtherSegment>
    constexpr bool boundaryContains(const OtherSegment& other) const;

    /**
     * @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B).
     *
     * Complexity: O(log n) for n vertices.
     */
    template<OrientedSegmentConcept OtherOrientedSegment>
    constexpr bool boundaryContains(const OtherOrientedSegment& other) const;

    /**
     * @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B).
     *
     * Complexity: O(log n) for n vertices.
     */
    template<LineConcept OtherLine>
    constexpr bool boundaryContains(const OtherLine& other) const;

    /**
     * @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B).
     *
     * Complexity: O(log n) for n vertices.
     */
    template<OrientedLineConcept OtherOrientedLine>
    constexpr bool boundaryContains(const OtherOrientedLine& other) const;

    /**
     * @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B).
     *
     * Complexity: O(log n) for n vertices.
     */
    template<RayConcept OtherRay>
    constexpr bool boundaryContains(const OtherRay& other) const;

    /**
     * @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B).
     *
     * Complexity: O(log n) for n vertices.
     */
    template<HalfplaneConcept OtherHalfplane>
    constexpr bool boundaryContains(const OtherHalfplane& other) const;

    /**
     * @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B).
     *
     * Complexity: O(log n) for n vertices (four edge checks).
     */
    template<RectangleConcept OtherRectangle>
    constexpr bool boundaryContains(const OtherRectangle& other) const;

    /**
     * @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B).
     *
     * Complexity: O(log n) for n vertices (three edge checks).
     */
    template<TriangleConcept OtherTriangle>
    constexpr bool boundaryContains(const OtherTriangle& other) const;

    /**
     * @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B).
     *
     * Returns false unless the convex polygon has at most two vertices.
     * Complexity: O(log n) for n vertices on this convex polygon.
     */
    template<ConvexConcept OtherConvex>
    constexpr bool boundaryContains(const OtherConvex& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<PolygonConcept OtherPolygon>
    constexpr bool boundaryContains(const OtherPolygon& other) const;

    /**
     * @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B).
     *
     * Returns false unless all disk points are the same and that point is on the boundary of this convex polygon.
     * Complexity: O(log n) for n vertices on this convex polygon.
     */
    template<DiskConcept OtherDisk>
    constexpr bool boundaryContains(const OtherDisk& other) const;

    /**
     * @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B).
     */
    template<PointConcept OtherPoint>
    constexpr bool boundaryContains(const Shape<OtherPoint>& other) const;


    /**
     * @brief Returns two edges of the convex polygon that intersect with the vertical line at x.
     * 
     * If the vertical line at x does not intersect the polygon, then std::nullopt is returned.
     * If the vertical line at x intersects the polygon, then it returns an edge of the strict
     * upper convex boundary and an edge of the strict lower convex boundary. By strict I mean
     * that the edges returned are not vertical. If the intersection happens at a vertex between
     * two non-vertical esges, then either one may be returned.
     * 
     * Complexity: O(log n) for n vertices.
     * 
     * @param x The x-coordinate of the vertical line.
     * @return An optional array containing the intersecting edges, or std::nullopt if none.
     */
    template<class OtherNumberType>
    constexpr std::optional<std::array<Segment<PointType>, 2>> edgesAtX(OtherNumberType x) const;

    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     * 
     * Complexity: O(log n) for n vertices.
     * 
     * @tparam OtherPoint The point type to check.
     * @param point The point to check.
     * @return True if the point is on the boundary or inside the convex polygon, false otherwise.
     */
    template<PointConcept OtherPoint>
    constexpr bool contains(const OtherPoint& point) const;

    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     * 
     * Complexity: O(log n) for n vertices.
     * 
     * @tparam OtherPoint The point type of the segment.
     * @param other The segment to check.
     * @return True if the convex polygon contains the segment, false otherwise.
     */
    template<SegmentConcept OtherSegment>
    constexpr bool contains(const OtherSegment& other) const;

    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     * 
     * Complexity: O(log n) for n vertices.
     * 
     * @tparam OtherPoint The point type of the oriented segment.
     * @param other The oriented segment to check.
     * @return True if the convex polygon contains the oriented segment, false otherwise.
     */
    template<OrientedSegmentConcept OtherOrientedSegment>
    constexpr bool contains(const OtherOrientedSegment& other) const;

    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     * @tparam OtherPoint The point type of the line.
     * @param other The line to check.
     * @return Always returns false since a line cannot be fully contained in A convex polygon.
     */
    template<LineConcept OtherLine>
    constexpr bool contains(const OtherLine&) const;

    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     * @tparam OtherPoint The point type of the oriented line.
     * @param other The oriented line to check.
     * @return Always returns false since an oriented line cannot be fully contained in A convex polygon.
     */
    template<OrientedLineConcept OtherOrientedLine>
    constexpr bool contains(const OtherOrientedLine&) const;

    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     * @tparam OtherPoint The point type of the ray.
     * @param other The ray to check.
     * @return Always returns false since a ray cannot be fully contained in A convex polygon.
     */
    template<RayConcept OtherRay>
    constexpr bool contains(const OtherRay&) const;

    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     * @tparam OtherPoint The point type of the halfplane.
     * @param other The halfplane to check.
     * @return Always returns false since a halfplane cannot be fully contained in A convex polygon.
     */
    template<HalfplaneConcept OtherHalfplane>
    constexpr bool contains(const OtherHalfplane&) const;

    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     * 
     * Complexity: O(log n) for n vertices.
     * 
     * @tparam OtherPoint The point type of the rectangle.
     * @param other The rectangle to check.
     * @return True if the convex polygon contains the rectangle, false otherwise.
     */
    template<RectangleConcept OtherRectangle>
    constexpr bool contains(const OtherRectangle& other) const;


    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     * 
     * Complexity: O(log n) for n vertices.
     * 
     * @tparam OtherPoint The point type of the triangle.
     * @param other The triangle to check.
     * @return True if the convex polygon contains the triangle, false otherwise.
     */
    template<TriangleConcept OtherTriangle>
    constexpr bool contains(const OtherTriangle& other) const;

    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     * 
     * Complexity: O(min(n,m) log (n+m)) for convex polygons with n and m vertices.
     * 
     * @tparam OtherPoint The point type of the convex polygon.
     * @param other The convex polygon to check.
     * @return True if the convex polygon contains the convex polygon, false otherwise.
     * @todo Optimize with bounding boxes and early exits.
     */
    template<ConvexConcept OtherConvex>
    constexpr bool contains(const OtherConvex& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template<PolygonConcept OtherPolygon>
    constexpr bool contains(const OtherPolygon& other) const;
  

    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     *
     * Complexity: O(n) for n vertices on this convex polygon.
     */
    template<DiskConcept OtherDisk>
    constexpr bool contains(const OtherDisk& other) const;

    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     */
    template<PointConcept OtherPoint>
    constexpr bool contains(const Shape<OtherPoint>& other) const;

    // The empty set is a subset of every shape, so its containment relations are
    // true. separates has no symmetric fallback, so it gets an explicit overload
    // too; the symmetric intersection/crossing predicates instead reach the
    // empty set through the generic OtherShape fallbacks declared below.
    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool contains(const EmptyShape<EmptyPoint>&) const {
        return true;
    }
    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool boundaryContains(const EmptyShape<EmptyPoint>&) const {
        return true;
    }
    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool interiorContains(const EmptyShape<EmptyPoint>&) const {
        return true;
    }
    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool separates(const EmptyShape<EmptyPoint>&) const {
        return false;
    }

    /**
     * @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B).
     *
     * Complexity: O(log n) for n vertices.
     *
     * @tparam OtherPoint The point type to check.
     * @param point The point to check.
     * @return True if the point is strictly inside the convex polygon, false otherwise.
     */
    template<PointConcept OtherPoint>
    constexpr bool interiorContains(const OtherPoint& point) const;

    /**
     * @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B).
     * 
     * Complexity: O(log n) for n vertices.
     * 
     * @tparam OtherPoint The point type of the segment.
     * @param other The segment to check.
     * @return True if the interior of the convex polygon contains the segment, false otherwise.
     */
    template<SegmentConcept OtherSegment>
    constexpr bool interiorContains(const OtherSegment& other) const;

    /**
     * @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B).
     * 
     * Complexity: O(log n) for n vertices.
     * 
     * @tparam OtherPoint The point type of the oriented segment.
     * @param other The oriented segment to check.
     * @return True if the interior of the convex polygon contains the oriented segment, false otherwise.
     */
    template<OrientedSegmentConcept OtherOrientedSegment>
    constexpr bool interiorContains(const OtherOrientedSegment& other) const;

    /**
     * @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B).
     * @tparam OtherPoint The point type of the line.
     * @param other The line to check.
     * @return Always returns false since a line cannot be fully contained in A convex polygon.
     */
    template<LineConcept OtherLine>
    constexpr bool interiorContains(const OtherLine&) const;

    /**
     * @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B).
     * @tparam OtherPoint The point type of the oriented line.
     * @param other The oriented line to check.
     * @return Always returns false since an oriented line cannot be fully contained in A convex polygon.
     */
    template<OrientedLineConcept OtherOrientedLine>
    constexpr bool interiorContains(const OtherOrientedLine&) const;

    /**
     * @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B).
     * @tparam OtherPoint The point type of the ray.
     * @param other The ray to check.
     * @return Always returns false since a ray cannot be fully contained in A convex polygon.
     */
    template<RayConcept OtherRay>
    constexpr bool interiorContains(const OtherRay&) const;

    /**
     * @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B).
     * @tparam OtherPoint The point type of the halfplane.
     * @param other The halfplane to check.
     * @return Always returns false since a halfplane cannot be fully contained in A convex polygon.
     */
    template<HalfplaneConcept OtherHalfplane>
    constexpr bool interiorContains(const OtherHalfplane&) const;

    /**
     * @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B).
     * 
     * Complexity: O(log n) for n vertices.
     * 
     * @tparam OtherPoint The point type of the rectangle.
     * @param other The rectangle to check.
     * @return True if the interior of the convex polygon contains the rectangle, false otherwise.
     */
    template<RectangleConcept OtherRectangle>
    constexpr bool interiorContains(const OtherRectangle& other) const;

    /**
     * @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B).
     * 
     * Complexity: O(log n) for n vertices.
     * 
     * @tparam OtherPoint The point type of the triangle.
     * @param other The triangle to check.
     * @return True if the interior of the convex polygon contains the triangle, false otherwise.
     */
    template<TriangleConcept OtherTriangle>
    constexpr bool interiorContains(const OtherTriangle& other) const;

    /**
     * @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B).
     * 
     * Complexity: O(min(n,m) log (m+n)) for polygons with n and m vertices
     * 
     * @tparam OtherPoint The point type of the convex polygon.
     * @param other The convex polygon to check.
     * @return True if the interior of the convex polygon contains the convex polygon, false otherwise.
     * @todo Optimize with bounding boxes and early exits.
     */
    template<ConvexConcept OtherConvex>
    constexpr bool interiorContains(const OtherConvex& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template<PolygonConcept OtherPolygon>
    constexpr bool interiorContains(const OtherPolygon& other) const;

    /**
     * @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B).
     *
     * Complexity: O(n) for n vertices on this convex polygon.
     */
    template<DiskConcept OtherDisk>
    constexpr bool interiorContains(const OtherDisk& other) const;

    /**
     * @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B).
     */
    template<PointConcept OtherPoint>
    constexpr bool interiorContains(const Shape<OtherPoint>& other) const;
    
    /**
     * @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅).
     * 
     * Complexity: O(log n) for n vertices.
     * 
     * @tparam OtherPoint The point type of the segment.
     * @param other The segment to check intersection with.
     * @return True if the convex polygon and segment intersect, false otherwise.
     */
    template<SegmentConcept OtherSegment>
    constexpr bool intersects(const OtherSegment& other) const;

    /**
     * @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅).
     * 
     * Complexity: O(log n) for n vertices.
     * 
     * @tparam OtherPoint The point type of the oriented segment.
     * @param other The oriented segment to check intersection with.
     * @return True if the convex polygon and oriented segment intersect, false otherwise.
     */
    template<OrientedSegmentConcept OtherOrientedSegment>
    constexpr bool intersects(const OtherOrientedSegment& other) const;

    /**
     * @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅).
     * 
     * Complexity: O(log n) for n vertices.
     * 
     * @tparam OtherPoint The point type of the line.
     * @param other The line to check intersection with.
     * @return True if the convex polygon and line intersect, false otherwise.
     */
    template<LineConcept OtherLine>
    constexpr bool intersects(const OtherLine& other) const;

    /**
     * @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅).
     * 
     * Complexity: O(log n) for n vertices.
     *
     * @tparam OtherPoint The point type of the oriented line.
     * @param other The oriented line to check intersection with.
     * @return True if the convex polygon and oriented line intersect, false otherwise.
     */
    template<OrientedLineConcept OtherOrientedLine>
    constexpr bool intersects(const OtherOrientedLine& other) const;

    /**
     * @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅).
     * 
     * Complexity: O(log n) for n vertices.
     * 
     * @tparam OtherPoint The point type of the ray.
     * @param other The ray to check intersection with.
     * @return True if the convex polygon and ray intersect, false otherwise.
     */
    template<RayConcept OtherRay>
    constexpr bool intersects(const OtherRay& other) const;

    /**
     * @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅).
     * 
     * Complexity: O(log n) for n vertices.
     * 
     * @tparam OtherPoint The point type of the rectangle.
     * @param other The rectangle to check intersection with.
     * @return True if the convex polygon and rectangle intersect, false otherwise.
     */
    template<RectangleConcept OtherRectangle>
    constexpr bool intersects(const OtherRectangle& other) const;

    /**
     * @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅).
     * 
     * Complexity: O(log n) for n vertices.
     * 
     * @tparam OtherPoint The point type of the triangle.
     * @param other The triangle to check intersection with.
     * @return True if the convex polygon and triangle intersect, false otherwise.
     */
    template<TriangleConcept OtherTriangle>
    constexpr bool intersects(const OtherTriangle& other) const;

    /**
     * @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅).
     *
     * Complexity: O(log n) for n vertices.
     *
     * @tparam OtherPoint The point type to check.
     * @param other The point to check.
     * @return True if the point is contained in the convex polygon.
     */
    template<PointConcept OtherPoint>
    constexpr bool intersects(const OtherPoint& other) const;

    /**
     * @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅).
     *
     * Complexity: O(log n) for n vertices.
     *
     * @tparam OtherPoint The point type of the half-plane.
     * @param other The half-plane to check intersection with.
     * @return True if any polygon vertex lies in the closed half-plane.
     */
    template<HalfplaneConcept OtherHalfplane>
    constexpr bool intersects(const OtherHalfplane& other) const;

    /**
     * @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅).
     *
     * Complexity: O(min(n,m) log(n+m)) for convex polygons with n and m vertices.
     * Cheap bounding-box check filters out disjoint cases in O(1).
     *
     * @tparam OtherPoint The point type of the other convex polygon.
     * @param other The convex polygon to check intersection with.
     * @return True if the convex polygons share any point.
     */
    template<ConvexConcept OtherConvex>
    constexpr bool intersects(const OtherConvex& other) const;

    /**
     * @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅).
     *
     * Complexity: O(n) for n vertices on this convex polygon.
     */
    template<DiskConcept OtherDisk>
    constexpr bool intersects(const OtherDisk& other) const;

    /**
     * @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅).
     */
    template<PointConcept OtherPoint>
    constexpr bool intersects(const Shape<OtherPoint>& other) const;

    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template<typename OtherShape>
        requires (!PointConcept<OtherShape> && detail::shapeRank<OtherShape> > detail::shapeRank<Convex>)
    [[nodiscard]] constexpr bool intersects(const OtherShape& other) const {
        return other.intersects(*this);
    }

    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool intersects(const EmptyShape<EmptyPoint>&) const {
        return false;
    }

    /**
     * @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅).
     *
     * Complexity: O(log n) for n vertices.
     */
    template<PointConcept OtherPoint>
    constexpr bool interiorsIntersect(const OtherPoint& other) const;

    /**
     * @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅).
     *
     * Complexity: O(log n) for n vertices.
     */
    template<LineConcept OtherLine>
    constexpr bool interiorsIntersect(const OtherLine& other) const;

    /**
     * @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅).
     *
     * Complexity: O(log n) for n vertices.
     */
    template<OrientedLineConcept OtherOrientedLine>
    constexpr bool interiorsIntersect(const OtherOrientedLine& other) const;

    /**
     * @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅).
     *
     * Complexity: O(log n) for n vertices.
     *
     * Uses @ref intersection to obtain the convex polygon-segment intersect exactly
     * in O(log n); the interiors meet iff the intersect is a non-degenerate
     * chord not lying on the convex polygon boundary.
     */
    template<SegmentConcept OtherSegment>
    constexpr bool interiorsIntersect(const OtherSegment& other) const;

    /**
     * @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅).
     *
     * Complexity: O(log n) for n vertices.
     */
    template<OrientedSegmentConcept OtherOrientedSegment>
    constexpr bool interiorsIntersect(const OtherOrientedSegment& other) const;

    /**
     * @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅).
     *
     * Complexity: O(log n) for n vertices.
     *
     * Uses @ref intersection to obtain the convex polygon-ray intersect exactly in
     * O(log n); the interiors meet iff the intersect is a non-degenerate
     * chord not lying on the convex polygon boundary.
     */
    template<RayConcept OtherRay>
    constexpr bool interiorsIntersect(const OtherRay& other) const;

    /**
     * @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅).
     *
     * Complexity: O(log n) for n vertices.
     */
    template<HalfplaneConcept OtherHalfplane>
    constexpr bool interiorsIntersect(const OtherHalfplane& other) const;

    /**
     * @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅).
     *
     * Complexity: O(log n) for n vertices.
     *
     */
    template<RectangleConcept OtherRectangle>
    constexpr bool interiorsIntersect(const OtherRectangle& other) const;

    /**
     * @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅).
     *
     * Complexity: O(log n) for n vertices.
     *
     */
    template<TriangleConcept OtherTriangle>
    constexpr bool interiorsIntersect(const OtherTriangle& other) const;

    /**
     * @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅).
     *
     * Complexity: O(min(n,m) log(m+n)) for polygons with n and m vertices.
     * A bounding-box test filters disjoint inputs in O(1).
     */
    template<ConvexConcept OtherConvex>
    constexpr bool interiorsIntersect(const OtherConvex& other) const;

    /**
     * @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅).
     *
     * Complexity: O(n) for n vertices on this convex polygon.
     */
    template<DiskConcept OtherDisk>
    constexpr bool interiorsIntersect(const OtherDisk& other) const;

    /**
     * @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅).
     */
    template<PointConcept OtherPoint>
    constexpr bool interiorsIntersect(const Shape<OtherPoint>& other) const;

    /** @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅). */
    template<typename OtherShape>
        requires (!PointConcept<OtherShape> && detail::shapeRank<OtherShape> > detail::shapeRank<Convex>)
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherShape& other) const {
        return other.interiorsIntersect(*this);
    }

    /** @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅). */
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool interiorsIntersect(const EmptyShape<EmptyPoint>&) const {
        return false;
    }

    /**
     * @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected).
     *
     * Complexity: O(1).
     */
    template<PointConcept OtherPoint>
    constexpr bool separates(const OtherPoint&) const;

    /**
     * @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected).
     *
     * Complexity: O(log n) for n vertices.
     *
     * A segment collinear with one of the convex polygon's boundary edges is not
     * separated by the convex polygon — the convex polygon only touches the segment along
     * the boundary, not through its interior, so the two outside extensions
     * are not produced by removing the convex polygon's body. We therefore require
     * the segment to cross the convex polygon interior, not just intersect it.
     */
    template<SegmentConcept OtherSegment>
    constexpr bool separates(const OtherSegment& other) const;

    /**
     * @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected).
     *
     * Complexity: O(log n) for n vertices.
     */
    template<OrientedSegmentConcept OtherOrientedSegment>
    constexpr bool separates(const OtherOrientedSegment& other) const;

    /**
     * @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected).
     *
     * Complexity: O(log n) for n vertices.
     */
    template<LineConcept OtherLine>
    constexpr bool separates(const OtherLine& other) const;

    /**
     * @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected).
     *
     * Complexity: O(log n) for n vertices.
     */
    template<OrientedLineConcept OtherOrientedLine>
    constexpr bool separates(const OtherOrientedLine& other) const;

    /**
     * @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected).
     *
     * Complexity: O(log n) for n vertices.
     *
     * The ray is split iff its source lies outside the closed polygon (so
     * the leading piece survives) and the ray actually intersects the convex polygon.
     */
    template<RayConcept OtherRay>
    constexpr bool separates(const OtherRay& other) const;

    /**
     * @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected).
     *
     * Complexity: O(1).
     */
    template<HalfplaneConcept OtherHalfplane>
    constexpr bool separates(const OtherHalfplane&) const;

    /**
     * @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected).
     *
     * Complexity: O(log n) for n vertices (four edge checks).
     */
    template<RectangleConcept OtherRectangle>
    constexpr bool separates(const OtherRectangle& other) const;

    /**
     * @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected).
     *
     * Complexity: O(log n) for n vertices (three edge checks).
     */
    template<TriangleConcept OtherTriangle>
    constexpr bool separates(const OtherTriangle& other) const;

    /**
     * @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected).
     *
     * Complexity: O(min(n,m) log(m+n)) for convex polygons with n vertices and m vertices.
     */
    template<ConvexConcept OtherConvex>
    constexpr bool separates(const OtherConvex& other) const;

    /**
     * @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected).
     *
     * Unlike the Convex overload, a reflex polygon can dip into the convex
     * body through several separate pockets while staying connected, so
     * counting boundary arcs is not enough. The polygon is cut iff some
     * connected component of the intersection touches the polygon boundary in
     * two or more pieces, detected by walking the contacts around the convex
     * boundary; this includes a convex body inside the polygon that pinches
     * the boundary at two touch points.
     *
     * Complexity: O(n log m + c m + c log c) for n polygon vertices, m convex
     * polygon vertices, and c boundary contacts.
     */
    template<PolygonConcept OtherPolygon>
    constexpr bool separates(const OtherPolygon& other) const;

    /**
     * @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected).
     *
     * Complexity: O(n) for n vertices on this convex polygon.
     */
    template<DiskConcept OtherDisk>
    constexpr bool separates(const OtherDisk& other) const;

    /**
     * @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected).
     */
    template<PointConcept OtherPoint>
    constexpr bool separates(const Shape<OtherPoint>& other) const;

    /**
     * @brief Tests whether the two shapes mutually separate each other (each disconnects the other).
     *
     * Complexity: O(1).
     */
    template<PointConcept OtherPoint>
    constexpr bool crosses(const OtherPoint&) const;

    /**
     * @brief Tests whether the two shapes mutually separate each other (each disconnects the other).
     *
     * Complexity: O(log n) for n vertices.
     */
    template<SegmentConcept OtherSegment>
    constexpr bool crosses(const OtherSegment& other) const;

    /**
     * @brief Tests whether the two shapes mutually separate each other (each disconnects the other).
     *
     * Complexity: O(log n) for n vertices.
     */
    template<OrientedSegmentConcept OtherOrientedSegment>
    constexpr bool crosses(const OtherOrientedSegment& other) const;

    /**
     * @brief Tests whether the two shapes mutually separate each other (each disconnects the other).
     *
     * Complexity: O(log n) for n vertices.
     */
    template<LineConcept OtherLine>
    constexpr bool crosses(const OtherLine& other) const;

    /**
     * @brief Tests whether the two shapes mutually separate each other (each disconnects the other).
     *
     * Complexity: O(log n) for n vertices.
     */
    template<OrientedLineConcept OtherOrientedLine>
    constexpr bool crosses(const OtherOrientedLine& other) const;

    /**
     * @brief Tests whether the two shapes mutually separate each other (each disconnects the other).
     *
     * Complexity: O(log n) for n vertices.
     */
    template<RayConcept OtherRay>
    constexpr bool crosses(const OtherRay& other) const;

    /**
     * @brief Tests whether the two shapes mutually separate each other (each disconnects the other).
     *
     * Complexity: O(1).
     */
    template<HalfplaneConcept OtherHalfplane>
    constexpr bool crosses(const OtherHalfplane&) const;

    /**
     * @brief Tests whether the two shapes mutually separate each other (each disconnects the other).
     *
     * Complexity: O(n) for n vertices, dominated by interiorsIntersect.
     */
    template<RectangleConcept OtherRectangle>
    constexpr bool crosses(const OtherRectangle& other) const;

    /**
     * @brief Tests whether the two shapes mutually separate each other (each disconnects the other).
     *
     * Complexity: O(n) for n vertices, dominated by interiorsIntersect.
     */
    template<TriangleConcept OtherTriangle>
    constexpr bool crosses(const OtherTriangle& other) const;

    /**
     * @brief Tests whether the two shapes mutually separate each other (each disconnects the other).
     *
     * Complexity: O(n log m + m log n) for this polygon with n vertices and
     * the other with m vertices.
     */
    template<ConvexConcept OtherConvex>
    constexpr bool crosses(const OtherConvex& other) const;

    /**
     * @brief Tests whether the two shapes mutually separate each other (each disconnects the other).
     *
     * Complexity: O(n) for n vertices on this convex polygon.
     */
    template<DiskConcept OtherDisk>
    constexpr bool crosses(const OtherDisk& other) const;

    /**
     * @brief Tests whether the two shapes mutually separate each other (each disconnects the other).
     */
    template<PointConcept OtherPoint>
    constexpr bool crosses(const Shape<OtherPoint>& other) const;

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template<typename OtherShape>
        requires (!PointConcept<OtherShape> && detail::shapeRank<OtherShape> > detail::shapeRank<Convex>)
    [[nodiscard]] constexpr bool crosses(const OtherShape& other) const {
        return other.crosses(*this);
    }

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool crosses(const EmptyShape<EmptyPoint>&) const {
        return false;
    }

    /**
     * @brief Returns the squared Euclidean distance to a point.
     *
     * Zero when the convex polygon contains the point (boundary included);
     * otherwise the smallest squared distance from the point to an edge.
     *
     * For an exterior point the closest boundary point lies on the contiguous
     * chain of edges that face the point. That chain is located in logarithmic
     * time: an interior reference point and two support-function searches bracket
     * the edge the ray to the query exits through (which faces the query) and the
     * opposite edge; a binary search then grows the facing chain, over which the
     * per-edge distance is unimodal, and a final binary search picks the closest
     * edge. The per-vertex distances themselves are *not* unimodal, so a direct
     * search over vertices does not work.
     *
     * Complexity: O(log n) for n vertices.
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

    /**
     * @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint.
     *
     * Complexity: O(log n) for n vertices.
     *
     * @tparam ResultNumber The number type for the result.
     * @tparam OtherPoint The point type of the segment.
     * @param other The segment to intersect with.
     * @return An optional variant containing either a point or segment representing the intersection, or empty if no intersection.
     * @warning Divides coordinates after casting to ResultNumber.
     */
    template <class ResultNumber = NumberType, SegmentConcept OtherSegment>
    constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherSegment& other) const;

    /**
     * @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint.
     * 
     * Complexity: O(log n) for n vertices.
     * 
     * @tparam ResultNumber The number type for the result.
     * @tparam OtherPoint The point type of the oriented segment.
     * @param other The oriented segment to intersect with.
     * @return An optional variant containing either a point or segment representing the intersection, or empty if no intersection.
     * @warning Divides coordinates after casting to ResultNumber.
     */
    template <class ResultNumber = NumberType, OrientedSegmentConcept OtherOrientedSegment>
    constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherOrientedSegment& other) const;

    /**
     * @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint.
     * 
     * Complexity: O(log n) for n vertices.
     *
     * @tparam ResultNumber The number type for the result.
     * @tparam OtherPoint The point type of the line.
     * @param other The line to intersect with.
     * @return An optional variant containing either a point or segment representing the intersection, or empty if no intersection.
     * @warning Divides coordinates after casting to ResultNumber.
     */
    template <class ResultNumber = NumberType, LineConcept OtherLine>
    constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherLine& other) const;

    /**
     * @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint.
     * 
     * Complexity: O(log n) for n vertices.
     * 
     * @tparam ResultNumber The number type for the result.
     * @tparam OtherPoint The point type of the oriented line.
     * @param other The oriented line to intersect with.
     * @return An optional variant containing either a point or segment representing the intersection, or empty if no intersection.
     * @warning Divides coordinates after casting to ResultNumber.
     */
    template <class ResultNumber = NumberType, OrientedLineConcept OtherOrientedLine>
    constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherOrientedLine& other) const;

    /**
     * @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint.
     * 
     * Complexity: O(log n) for n vertices.
     * 
     * @tparam ResultNumber The number type for the result.
     * @tparam OtherPoint The point type of the ray.
     * @param other The ray to intersect with.
     * @return An optional variant containing either a point or segment representing the intersection, or empty if no intersection.
     * @warning Divides coordinates after casting to ResultNumber.
     */
    template <class ResultNumber = NumberType, RayConcept OtherRay>
    constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherRay& other) const;

    /**
     * @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint.
     * 
     * Complexity: O(log n) for n vertices.
     * 
     * @tparam ResultNumber The number type for the result.
     * @tparam OtherPoint The point type of the rectangle.
     * @param other The rectangle to intersect with.
     * @return An optional variant containing either a point or segment or convex polygon representing the intersection, or empty if no intersection.
     * @warning Divides coordinates after casting to ResultNumber.
     */
    template <class ResultNumber = NumberType, RectangleConcept OtherRectangle>
    constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>, Convex<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherRectangle& other) const;


    /**
     * @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint.
     * 
     * Complexity: O(log n) for n vertices.
     * 
     * @tparam ResultNumber The number type for the result.
     * @tparam OtherPoint The point type of the triangle.
     * @param other The triangle to intersect with.
     * @return An optional variant containing either a point or segment or convex polygon representing the intersection, or empty if no intersection.
     * @warning Divides coordinates after casting to ResultNumber.
     */
    template <class ResultNumber = NumberType, TriangleConcept OtherTriangle>
    constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>, Convex<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherTriangle& other) const;

    /**
     * @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint.
     * 
     * Complexity: O((n+m) log(n+m)) for n and m vertices.
     * 
     * @tparam ResultNumber The number type for the result.
     * @tparam OtherPoint The point type of the other convex polygon.
     * @param other The other convex polygon to intersect with.
     * @return An optional variant containing either a point or segment or convex polygon representing the intersection, or empty if no intersection.
     * @warning Divides coordinates after casting to ResultNumber.
     */
    template <class ResultNumber = NumberType, ConvexConcept OtherConvex>
    constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>, Convex<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherConvex& other) const;

    /** @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint. */
    template <class ResultNumber = NumberType, class EmptyPoint>
    [[nodiscard]] constexpr EmptyShape<EmptyPoint> intersection(const EmptyShape<EmptyPoint>&) const {
        return {};
    }

    /**
     * @brief Translates the convex polygon by the given point.
     * 
     * Complexity: O(1).
     * 
     * @tparam OtherPoint The point type of the translation vector.
     * @param translation The translation vector.
     * @return A reference to the modified polygon.
     */
    /**
     * @brief Returns the convex polygon rotated by 90k degrees around the origin.
     *
     * @param k Number of 90-degree CCW rotations (may be negative).
     * @return Rotated convex polygon.
     */
    [[nodiscard]] constexpr Convex rotated90(int k = 1) const;

    /**
     * @brief Rotates the convex polygon by 90k degrees around the origin in place.
     *
     * @param k Number of 90-degree CCW rotations (may be negative).
     */
    constexpr void rotate90(int k = 1);

    /** @brief Returns the convex polygon with its x-coordinates multiplied by a factor. */
    template <class OtherNumber>
    [[nodiscard]] constexpr Convex scaledUpX(const OtherNumber scalar) const;

    /** @brief Multiplies the convex polygon's x-coordinates by a factor in place. */
    template <class OtherNumber>
    constexpr void scaleUpX(const OtherNumber scalar);

    /** @brief Returns the convex polygon with its y-coordinates multiplied by a factor. */
    template <class OtherNumber>
    [[nodiscard]] constexpr Convex scaledUpY(const OtherNumber scalar) const;

    /** @brief Multiplies the convex polygon's y-coordinates by a factor in place. */
    template <class OtherNumber>
    constexpr void scaleUpY(const OtherNumber scalar);

    /** @brief Returns the convex polygon with its x-coordinates divided by a divisor. */
    template <class OtherNumber>
    [[nodiscard]] constexpr Convex scaledDownX(const OtherNumber scalar) const;

    /** @brief Divides the convex polygon's x-coordinates by a divisor in place. */
    template <class OtherNumber>
    constexpr void scaleDownX(const OtherNumber scalar);

    /** @brief Returns the convex polygon with its y-coordinates divided by a divisor. */
    template <class OtherNumber>
    [[nodiscard]] constexpr Convex scaledDownY(const OtherNumber scalar) const;

    /** @brief Divides the convex polygon's y-coordinates by a divisor in place. */
    template <class OtherNumber>
    constexpr void scaleDownY(const OtherNumber scalar);

    template<PointConcept OtherPoint>
    constexpr Convex& operator+=(const OtherPoint& translation);

    /**
     * @brief Translates the convex polygon by the negation of the given point.
     * 
     * Complexity: O(1).
     * 
     * @tparam OtherPoint The point type of the translation vector.
     * @param translation The translation vector to negate.
     * @return A reference to the modified polygon.
     */
    template<PointConcept OtherPoint>
    constexpr Convex& operator-=(const OtherPoint& translation);

    /**
     * @brief Scales the convex polygon by the given scalar.
     * 
     * Complexity: O(n) for n vertices since we need to apply the scaling to each vertex.
     * 
     * @tparam Scalar The scalar type.
     * @param scalar The scaling factor.
     * @return A reference to the modified polygon.
     */
    template <class Scalar>
        requires(!detail::is_point_v<Scalar>)
    constexpr Convex& operator*=(const Scalar& scalar);

    /**
     * @brief Divides the convex polygon by the given scalar.
     * 
     * Complexity: O(n) for n vertices since we need to apply the division to each vertex.
     * 
     * @tparam Scalar The scalar type.
     * @param scalar The divisor.
     * @return A reference to the modified polygon.
     */
    template <class Scalar>
        requires(!detail::is_point_v<Scalar>)
    constexpr Convex& operator/=(const Scalar& scalar);

    /**
     * @brief Forward iterator over the (optionally oriented) boundary edges.
     *
     * Edge `i` joins vertex `i` to vertex `i + 1` (cyclically), so a convex
     * polygon with `size()` vertices has `size()` edges.
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
            assert(convex != nullptr);
            return convex->template boundaryAt<Oriented>(index);
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
        friend struct Convex;

        constexpr BoundaryIterator(const Convex* convex_arg, std::size_t index_arg)
            : convex(convex_arg), index(index_arg) {}

        const Convex* convex = nullptr;
        std::size_t index = 0;
    };

  private:
    std::vector<PointType> points_{};
    [[no_unique_address]] mutable LabelType label_{};
    PointType translation_{};
    // Lazily computed caches, invalidated by resetCache() on every mutation.
    // bbox_ is empty and maxIndex_ is -1 until first computed.
    mutable std::optional<Rectangle<PointType>> bbox_{};
    mutable std::ptrdiff_t maxIndex_ = -1;

    // Memoized hash, computed lazily by std::hash<Convex>. hashUnset_ means "not
    // yet computed"; SIZE_MAX is chosen as the sentinel because it is a rare hash
    // output, and the one true hash that would collide with it is remapped to
    // hashUnset_ - 1 so the sentinel is never stored as a real value. Unlike the
    // bbox, the hash is not translation-invariant, so operator+=/-= reset it.
    static constexpr std::size_t hashUnset_ = std::numeric_limits<std::size_t>::max();
    mutable std::size_t hash_ = hashUnset_;
    friend struct std::hash<Convex>;

    // Drops every memoized value; call after any operation that mutates the
    // polygon's vertices. A pure translation does not need to drop bbox_/maxIndex_
    // (maxIndex_ stays valid and the bbox shifts in place, see operator+=), but it
    // must still reset hash_, which depends on the absolute vertex positions.
    constexpr void resetCache() const {
        bbox_ = {};
        maxIndex_ = -1;
        hash_ = hashUnset_;
    }

    template <bool Oriented>
    constexpr BoundaryType<Oriented> boundaryAt(std::size_t index) const {
        const auto i = static_cast<std::ptrdiff_t>(index);
        return BoundaryType<Oriented>(get(i), get(i + 1));
    }

    // Lexicographic less-than that promotes mixed numeric types to their
    // common type before comparing. Lets std::lower_bound / std::binary_search
    // search points_ with a key whose NumberType differs from PointType's.
    struct LexLessCrossType {
        template <class A, class B>
        constexpr bool operator()(const A& a, const B& b) const {
            using AX = std::remove_cvref_t<decltype(a.x())>;
            using BX = std::remove_cvref_t<decltype(b.x())>;
            using C = std::common_type_t<AX, BX>;
            const auto ax = static_cast<C>(a.x());
            const auto bx = static_cast<C>(b.x());
            if (ax < bx) return true;
            if (bx < ax) return false;
            return static_cast<C>(a.y()) < static_cast<C>(b.y());
        }
    };
    static constexpr LexLessCrossType lexLessCrossType{};

    class Iterator {
    private:
        std::vector<PointType>::const_iterator it;
        PointType x;

    public:
        using iterator_category = std::random_access_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = PointType;
        using pointer = PointType*;
        using reference = PointType&;

        Iterator() = default;
        Iterator(std::vector<PointType>::const_iterator it, PointType x) : it(it), x(x) {}

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
}; // class Convex

template <class PointType, class LabelType, class TranslationNumber, class TranslationLabel>
constexpr auto operator+(const Convex<PointType, LabelType>& convex, const Point<TranslationNumber, TranslationLabel>& translation) {
    return translation + convex;
}

template <class TranslationNumber, class TranslationLabel, class PointType, class LabelType>
constexpr auto operator+(const Point<TranslationNumber, TranslationLabel>& translation, const Convex<PointType, LabelType>& convex) {
    using ResultPointType = Point<TranslationNumber, typename PointType::LabelType>;
    Convex<ResultPointType, LabelType> result(convex);
    result += translation;
    if constexpr (detail::has_label_v<LabelType>) {
        result.label() = LabelType{};
    }
    return result;
}

template <class PointType, class LabelType, class TranslationNumber, class TranslationLabel>
constexpr auto operator-(const Convex<PointType, LabelType>& convex, const Point<TranslationNumber, TranslationLabel>& translation) {
    return convex + (-translation);
}

template <class PointType, class LabelType, class Scalar>
    requires(!detail::is_point_v<Scalar>)
constexpr auto operator*(const Convex<PointType, LabelType>& convex, const Scalar& scalar) {
    using ResultPointType = Point<decltype(std::declval<PointType>().x() * scalar), typename PointType::LabelType>;
    Convex<ResultPointType, LabelType> result(convex);
    result *= scalar;
    if constexpr (detail::has_label_v<LabelType>) {
        result.label() = LabelType{};
    }
    return result;
}

template <class Scalar, class PointType, class LabelType>
    requires(!detail::is_point_v<Scalar>)
constexpr auto operator*(const Scalar& scalar, const Convex<PointType, LabelType>& convex) {
    return convex * scalar;
}

template <class PointType, class LabelType, class Scalar>
    requires(!detail::is_point_v<Scalar>)
constexpr auto operator/(const Convex<PointType, LabelType>& convex, const Scalar& scalar) {
    using ResultPointType = Point<decltype(std::declval<PointType>().x() / scalar), typename PointType::LabelType>;
    Convex<ResultPointType, LabelType> result(convex);
    result /= scalar;
    if constexpr (detail::has_label_v<LabelType>) {
        result.label() = LabelType{};
    }
    return result;
}

template <class PointType, class LabelType>
std::ostream& operator<<(std::ostream& stream, const Convex<PointType, LabelType>& convex);

}  // namespace pgl
