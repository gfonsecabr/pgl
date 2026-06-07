#pragma once

#include <algorithm>
#include <vector>
#include <cassert>
#include <compare>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <ostream>
#include <type_traits>
#include <utility>

#include "../pgl.hpp"

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

template <class PointType = Point<>>
struct Convex;

Convex() -> Convex<Point<>>;

template <std::ranges::input_range Range>
requires detail::is_point_v<std::ranges::range_value_t<Range>>
Convex(Range&&) -> Convex<std::remove_cvref_t<std::ranges::range_value_t<Range>>>;

template <class Number>
requires (!detail::is_point_v<Number>)
Convex(std::initializer_list<Number>) -> Convex<Point<Number>>;

template <class Number>
requires (!detail::is_point_v<Number>)
Convex(std::initializer_list<Number>, bool) -> Convex<Point<Number>>;


template <class PointType_>
struct Convex {
    using PointType = PointType_;
    using NumberType = PointType::NumberType;
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
    template<PointConcept OtherPointType>
        requires(std::constructible_from<PointType, const OtherPointType&>)
    constexpr Convex(const Convex<OtherPointType>& other) : points_(other.begin(), other.end()) {}

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
     * Complexity: O(log n) for n vertices.
     * 
     * @return A rectangle representing the bounding box.
     */
    constexpr Rectangle<PointType> bbox() const;

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
     * @warning Uses division by 3 and twice the area, so may not be exact for integral types.
     */
    template <class ResultNumber = NumberType>
    constexpr Point<ResultNumber> centroid() const;

    /**
     * @brief Computes the centroid of the vertex set.
     * @tparam ResultNumber The number type for the result.
     * @return The centroid of the vertex set.
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
     * @warning For polygons with collinear vertices, the point may be on the boundary. Uses division by 4, so may not be exact for integral types.
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
     * @brief Checks if the boundary of the convex polygon contains the given point.
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
     * @brief Checks if the boundary of the convex polygon contains the given segment.
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
    template<PointConcept OtherPoint>
    constexpr bool boundaryContains(const Segment<OtherPoint>& other) const;

    /**
     * @brief Checks if the boundary of the convex polygon contains the given oriented segment.
     *
     * Complexity: O(log n) for n vertices.
     */
    template<PointConcept OtherPoint>
    constexpr bool boundaryContains(const OrientedSegment<OtherPoint>& other) const;

    /**
     * @brief Degenerate lines are on the boundary iff their unique point is.
     *
     * Complexity: O(log n) for n vertices.
     */
    template<PointConcept OtherPoint>
    constexpr bool boundaryContains(const Line<OtherPoint>& other) const;

    /**
     * @brief Degenerate oriented lines are on the boundary iff their unique point is.
     *
     * Complexity: O(log n) for n vertices.
     */
    template<PointConcept OtherPoint>
    constexpr bool boundaryContains(const OrientedLine<OtherPoint>& other) const;

    /**
     * @brief Degenerate rays are on the boundary iff their source point is.
     *
     * Complexity: O(log n) for n vertices.
     */
    template<PointConcept OtherPoint>
    constexpr bool boundaryContains(const Ray<OtherPoint>& other) const;

    /**
     * @brief Degenerate half-planes are on the boundary iff their source point is.
     *
     * Complexity: O(log n) for n vertices.
     */
    template<PointConcept OtherPoint>
    constexpr bool boundaryContains(const Halfplane<OtherPoint>& other) const;

    /**
     * @brief Checks if every rectangle edge lies on the boundary of the convex polygon.
     *
     * Complexity: O(log n) for n vertices (four edge checks).
     */
    template<PointConcept OtherPoint>
    constexpr bool boundaryContains(const Rectangle<OtherPoint>& other) const;

    /**
     * @brief Checks if every triangle edge lies on the boundary of the convex polygon.
     *
     * Complexity: O(log n) for n vertices (three edge checks).
     */
    template<PointConcept OtherPoint>
    constexpr bool boundaryContains(const Triangle<OtherPoint>& other) const;

    /**
     * @brief Checks if every edge of the other convex polygon lies on the boundary.
     *
     * Returns false unless the convex polygon has at most two vertices.
     * Complexity: O(log n) for n vertices on this convex polygon.
     */
    template<PointConcept OtherPoint>
    constexpr bool boundaryContains(const Convex<OtherPoint>& other) const;

    /** @brief Polygon overload; see the Convex overload. */
    template<PointConcept OtherPoint>
    constexpr bool boundaryContains(const Polygon<OtherPoint>& other) const;

    /**
     * @brief Checks if the disk lies on the boundary of this convex polygon.
     *
     * Returns false unless all disk points are the same and that point is on the boundary of this convex polygon.
     * Complexity: O(log n) for n vertices on this convex polygon.
     */
    template<PointConcept OtherPoint>
    constexpr bool boundaryContains(const Disk<OtherPoint>& other) const;

    /**
     * @brief Checks if the boundary of the convex polygon contains the wrapped shape.
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
     * @brief Checks if the convex polygon contains the given point (boundary or interior).
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
     * @brief Checks if the convex polygon contains the given segment.
     * 
     * Complexity: O(log n) for n vertices.
     * 
     * @tparam OtherPoint The point type of the segment.
     * @param other The segment to check.
     * @return True if the convex polygon contains the segment, false otherwise.
     */
    template<PointConcept OtherPoint>
    constexpr bool contains(const Segment<OtherPoint>& other) const;

    /**
     * @brief Checks if the convex polygon contains the given oriented segment.
     * 
     * Complexity: O(log n) for n vertices.
     * 
     * @tparam OtherPoint The point type of the oriented segment.
     * @param other The oriented segment to check.
     * @return True if the convex polygon contains the oriented segment, false otherwise.
     */
    template<PointConcept OtherPoint>
    constexpr bool contains(const OrientedSegment<OtherPoint>& other) const;

    /**
     * @brief Checks if the convex polygon contains the given line.
     * @tparam OtherPoint The point type of the line.
     * @param other The line to check.
     * @return Always returns false since a line cannot be fully contained in A convex polygon.
     */
    template<PointConcept OtherPoint>
    constexpr bool contains(const Line<OtherPoint>&) const;

    /**
     * @brief Checks if the convex polygon contains the given oriented line.
     * @tparam OtherPoint The point type of the oriented line.
     * @param other The oriented line to check.
     * @return Always returns false since an oriented line cannot be fully contained in A convex polygon.
     */
    template<PointConcept OtherPoint>
    constexpr bool contains(const OrientedLine<OtherPoint>&) const;

    /**
     * @brief Checks if the convex polygon contains the given ray.
     * @tparam OtherPoint The point type of the ray.
     * @param other The ray to check.
     * @return Always returns false since a ray cannot be fully contained in A convex polygon.
     */
    template<PointConcept OtherPoint>
    constexpr bool contains(const Ray<OtherPoint>&) const;

    /**
     * @brief Checks if the convex polygon contains the given halfplane.
     * @tparam OtherPoint The point type of the halfplane.
     * @param other The halfplane to check.
     * @return Always returns false since a halfplane cannot be fully contained in A convex polygon.
     */
    template<PointConcept OtherPoint>
    constexpr bool contains(const Halfplane<OtherPoint>&) const;

    /**
     * @brief Checks if the convex polygon contains the given rectangle.
     * 
     * Complexity: O(log n) for n vertices.
     * 
     * @tparam OtherPoint The point type of the rectangle.
     * @param other The rectangle to check.
     * @return True if the convex polygon contains the rectangle, false otherwise.
     */
    template<PointConcept OtherPoint>
    constexpr bool contains(const Rectangle<OtherPoint>& other) const;


    /**
     * @brief Checks if the convex polygon contains the given triangle.
     * 
     * Complexity: O(log n) for n vertices.
     * 
     * @tparam OtherPoint The point type of the triangle.
     * @param other The triangle to check.
     * @return True if the convex polygon contains the triangle, false otherwise.
     */
    template<PointConcept OtherPoint>
    constexpr bool contains(const Triangle<OtherPoint>& other) const;

    /**
     * @brief Checks if the convex polygon contains the given convex polygon.
     * 
     * Complexity: O(min(n,m) log (n+m)) for convex polygons with n and m vertices.
     * 
     * @tparam OtherPoint The point type of the convex polygon.
     * @param other The convex polygon to check.
     * @return True if the convex polygon contains the convex polygon, false otherwise.
     * @todo Optimize with bounding boxes and early exits.
     */
    template<PointConcept OtherPoint>
    constexpr bool contains(const Convex<OtherPoint>& other) const;

    /** @brief Polygon overload; see the Convex overload. */
    template<PointConcept OtherPoint>
    constexpr bool contains(const Polygon<OtherPoint>& other) const;
  

    /**
     * @brief Checks if the disk is contained in this convex polygon.
     *
     * Complexity: O(n) for n vertices on this convex polygon.
     */
    template<PointConcept OtherPoint>
    constexpr bool contains(const Disk<OtherPoint>& other) const;

    /**
     * @brief Checks if the convex polygon contains the wrapped shape.
     */
    template<PointConcept OtherPoint>
    constexpr bool contains(const Shape<OtherPoint>& other) const;

    /**
     * @brief Checks if the interior of the convex polygon contains the given point.
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
     * @brief Checks if the interior of the convex polygon contains the given segment.
     * 
     * Complexity: O(log n) for n vertices.
     * 
     * @tparam OtherPoint The point type of the segment.
     * @param other The segment to check.
     * @return True if the interior of the convex polygon contains the segment, false otherwise.
     */
    template<PointConcept OtherPoint>
    constexpr bool interiorContains(const Segment<OtherPoint>& other) const;

    /**
     * @brief Checks if the interior of the convex polygon contains the given oriented segment.
     * 
     * Complexity: O(log n) for n vertices.
     * 
     * @tparam OtherPoint The point type of the oriented segment.
     * @param other The oriented segment to check.
     * @return True if the interior of the convex polygon contains the oriented segment, false otherwise.
     */
    template<PointConcept OtherPoint>
    constexpr bool interiorContains(const OrientedSegment<OtherPoint>& other) const;

    /**
     * @brief Checks if the interior of the convex polygon contains the given line.
     * @tparam OtherPoint The point type of the line.
     * @param other The line to check.
     * @return Always returns false since a line cannot be fully contained in A convex polygon.
     */
    template<PointConcept OtherPoint>
    constexpr bool interiorContains(const Line<OtherPoint>&) const;

    /**
     * @brief Checks if the interior of the convex polygon contains the given oriented line.
     * @tparam OtherPoint The point type of the oriented line.
     * @param other The oriented line to check.
     * @return Always returns false since an oriented line cannot be fully contained in A convex polygon.
     */
    template<PointConcept OtherPoint>
    constexpr bool interiorContains(const OrientedLine<OtherPoint>&) const;

    /**
     * @brief Checks if the interior of the convex polygon contains the given ray.
     * @tparam OtherPoint The point type of the ray.
     * @param other The ray to check.
     * @return Always returns false since a ray cannot be fully contained in A convex polygon.
     */
    template<PointConcept OtherPoint>
    constexpr bool interiorContains(const Ray<OtherPoint>&) const;

    /**
     * @brief Checks if the interior of the convex polygon contains the given halfplane.
     * @tparam OtherPoint The point type of the halfplane.
     * @param other The halfplane to check.
     * @return Always returns false since a halfplane cannot be fully contained in A convex polygon.
     */
    template<PointConcept OtherPoint>
    constexpr bool interiorContains(const Halfplane<OtherPoint>&) const;

    /**
     * @brief Checks if the interior of the convex polygon contains the given rectangle.
     * 
     * Complexity: O(log n) for n vertices.
     * 
     * @tparam OtherPoint The point type of the rectangle.
     * @param other The rectangle to check.
     * @return True if the interior of the convex polygon contains the rectangle, false otherwise.
     */
    template<PointConcept OtherPoint>
    constexpr bool interiorContains(const Rectangle<OtherPoint>& other) const;

    /**
     * @brief Checks if the interior of the convex polygon contains the given triangle.
     * 
     * Complexity: O(log n) for n vertices.
     * 
     * @tparam OtherPoint The point type of the triangle.
     * @param other The triangle to check.
     * @return True if the interior of the convex polygon contains the triangle, false otherwise.
     */
    template<PointConcept OtherPoint>
    constexpr bool interiorContains(const Triangle<OtherPoint>& other) const;

    /**
     * @brief Checks if the interior of the convex polygon contains the given convex polygon.
     * 
     * Complexity: O(min(n,m) log (m+n)) for polygons with n and m vertices
     * 
     * @tparam OtherPoint The point type of the convex polygon.
     * @param other The convex polygon to check.
     * @return True if the interior of the convex polygon contains the convex polygon, false otherwise.
     * @todo Optimize with bounding boxes and early exits.
     */
    template<PointConcept OtherPoint>
    constexpr bool interiorContains(const Convex<OtherPoint>& other) const;

    /** @brief Polygon overload; see the Convex overload. */
    template<PointConcept OtherPoint>
    constexpr bool interiorContains(const Polygon<OtherPoint>& other) const;

    /**
     * @brief Checks if the disk is contained in the interior of this convex polygon.
     *
     * Complexity: O(n) for n vertices on this convex polygon.
     */
    template<PointConcept OtherPoint>
    constexpr bool interiorContains(const Disk<OtherPoint>& other) const;

    /**
     * @brief Checks if the interior of the convex polygon contains the wrapped shape.
     */
    template<PointConcept OtherPoint>
    constexpr bool interiorContains(const Shape<OtherPoint>& other) const;
    
    /**
     * @brief Checks if the convex polygon intersects the given segment.
     * 
     * Complexity: O(log n) for n vertices.
     * 
     * @tparam OtherPoint The point type of the segment.
     * @param other The segment to check intersection with.
     * @return True if the convex polygon and segment intersect, false otherwise.
     */
    template<PointConcept OtherPoint>
    constexpr bool intersects(const Segment<OtherPoint>& other) const;

    /**
     * @brief Checks if the convex polygon intersects the given oriented segment.
     * 
     * Complexity: O(log n) for n vertices.
     * 
     * @tparam OtherPoint The point type of the oriented segment.
     * @param other The oriented segment to check intersection with.
     * @return True if the convex polygon and oriented segment intersect, false otherwise.
     */
    template<PointConcept OtherPoint>
    constexpr bool intersects(const OrientedSegment<OtherPoint>& other) const;

    /**
     * @brief Checks if the convex polygon intersects the given line.
     * 
     * Complexity: O(log n) for n vertices.
     * 
     * @tparam OtherPoint The point type of the line.
     * @param other The line to check intersection with.
     * @return True if the convex polygon and line intersect, false otherwise.
     */
    template<PointConcept OtherPoint>
    constexpr bool intersects(const Line<OtherPoint>& other) const;

    /**
     * @brief Checks if the convex polygon intersects the given oriented line.
     * 
     * Complexity: O(log n) for n vertices.
     *
     * @tparam OtherPoint The point type of the oriented line.
     * @param other The oriented line to check intersection with.
     * @return True if the convex polygon and oriented line intersect, false otherwise.
     */
    template<PointConcept OtherPoint>
    constexpr bool intersects(const OrientedLine<OtherPoint>& other) const;

    /**
     * @brief Checks if the convex polygon intersects the given ray.
     * 
     * Complexity: O(log n) for n vertices.
     * 
     * @tparam OtherPoint The point type of the ray.
     * @param other The ray to check intersection with.
     * @return True if the convex polygon and ray intersect, false otherwise.
     */
    template<PointConcept OtherPoint>
    constexpr bool intersects(const Ray<OtherPoint>& other) const;

    /**
     * @brief Checks if the convex polygon intersects the given rectangle.
     * 
     * Complexity: O(log n) for n vertices.
     * 
     * @tparam OtherPoint The point type of the rectangle.
     * @param other The rectangle to check intersection with.
     * @return True if the convex polygon and rectangle intersect, false otherwise.
     */
    template<PointConcept OtherPoint>
    constexpr bool intersects(const Rectangle<OtherPoint>& other) const;

    /**
     * @brief Checks if the convex polygon intersects the given triangle.
     * 
     * Complexity: O(log n) for n vertices.
     * 
     * @tparam OtherPoint The point type of the triangle.
     * @param other The triangle to check intersection with.
     * @return True if the convex polygon and triangle intersect, false otherwise.
     */
    template<PointConcept OtherPoint>
    constexpr bool intersects(const Triangle<OtherPoint>& other) const;

    /**
     * @brief Checks if the convex polygon intersects the given point.
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
     * @brief Checks if the convex polygon intersects the given half-plane.
     *
     * Complexity: O(log n) for n vertices.
     *
     * @tparam OtherPoint The point type of the half-plane.
     * @param other The half-plane to check intersection with.
     * @return True if any polygon vertex lies in the closed half-plane.
     */
    template<PointConcept OtherPoint>
    constexpr bool intersects(const Halfplane<OtherPoint>& other) const;

    /**
     * @brief Checks if the convex polygon intersects the given convex polygon.
     *
     * Complexity: O(min(n,m) log(n+m)) for convex polygons with n and m vertices.
     * Cheap bounding-box check filters out disjoint cases in O(1).
     *
     * @tparam OtherPoint The point type of the other convex polygon.
     * @param other The convex polygon to check intersection with.
     * @return True if the convex polygons share any point.
     */
    template<PointConcept OtherPoint>
    constexpr bool intersects(const Convex<OtherPoint>& other) const;

    /** @brief Polygon overload; see the Convex overload. */
    template<PointConcept OtherPoint>
    constexpr bool intersects(const Polygon<OtherPoint>& other) const;

    /**
     * @brief Checks if the disk intersects this convex polygon.
     *
     * Complexity: O(n) for n vertices on this convex polygon.
     */
    template<PointConcept OtherPoint>
    constexpr bool intersects(const Disk<OtherPoint>& other) const;

    /**
     * @brief Checks if the convex polygon intersects the wrapped shape.
     */
    template<PointConcept OtherPoint>
    constexpr bool intersects(const Shape<OtherPoint>& other) const;

    /**
     * @brief Checks if the interior of the convex polygon intersects a point.
     *
     * Complexity: O(log n) for n vertices.
     */
    template<PointConcept OtherPoint>
    constexpr bool interiorsIntersect(const OtherPoint& other) const;

    /**
     * @brief Checks if the convex polygon interior intersects a line.
     *
     * Complexity: O(log n) for n vertices.
     */
    template<PointConcept OtherPoint>
    constexpr bool interiorsIntersect(const Line<OtherPoint>& other) const;

    /**
     * @brief Checks if the convex polygon interior intersects an oriented line.
     *
     * Complexity: O(log n) for n vertices.
     */
    template<PointConcept OtherPoint>
    constexpr bool interiorsIntersect(const OrientedLine<OtherPoint>& other) const;

    /**
     * @brief Checks if the segment interior intersects the convex polygon interior.
     *
     * Complexity: O(log n) for n vertices.
     *
     * Uses @ref intersection to obtain the convex polygon-segment intersect exactly
     * in O(log n); the interiors meet iff the intersect is a non-degenerate
     * chord not lying on the convex polygon boundary.
     */
    template<PointConcept OtherPoint>
    constexpr bool interiorsIntersect(const Segment<OtherPoint>& other) const;

    /**
     * @brief Checks if the oriented segment interior intersects the convex polygon interior.
     *
     * Complexity: O(log n) for n vertices.
     */
    template<PointConcept OtherPoint>
    constexpr bool interiorsIntersect(const OrientedSegment<OtherPoint>& other) const;

    /**
     * @brief Checks if the ray interior intersects the convex polygon interior.
     *
     * Complexity: O(log n) for n vertices.
     *
     * Uses @ref intersection to obtain the convex polygon-ray intersect exactly in
     * O(log n); the interiors meet iff the intersect is a non-degenerate
     * chord not lying on the convex polygon boundary.
     */
    template<PointConcept OtherPoint>
    constexpr bool interiorsIntersect(const Ray<OtherPoint>& other) const;

    /**
     * @brief Checks if a half-plane interior intersects the convex polygon interior.
     *
     * Complexity: O(log n) for n vertices.
     */
    template<PointConcept OtherPoint>
    constexpr bool interiorsIntersect(const Halfplane<OtherPoint>& other) const;

    /**
     * @brief Checks if a rectangle interior intersects the convex polygon interior.
     *
     * Complexity: O(log n) for n vertices.
     *
     */
    template<PointConcept OtherPoint>
    constexpr bool interiorsIntersect(const Rectangle<OtherPoint>& other) const;

    /**
     * @brief Checks if a triangle interior intersects the convex polygon interior.
     *
     * Complexity: O(log n) for n vertices.
     *
     */
    template<PointConcept OtherPoint>
    constexpr bool interiorsIntersect(const Triangle<OtherPoint>& other) const;

    /**
     * @brief Checks if two convex polygon interiors intersect.
     *
     * Complexity: O(min(n,m) log(m+n)) for polygons with n and m vertices.
     * A bounding-box test filters disjoint inputs in O(1).
     */
    template<PointConcept OtherPoint>
    constexpr bool interiorsIntersect(const Convex<OtherPoint>& other) const;

    /** @brief Polygon overload; see the Convex overload. */
    template<PointConcept OtherPoint>
    constexpr bool interiorsIntersect(const Polygon<OtherPoint>& other) const;

    /**
     * @brief Checks if the interior of the convex polygon intersects a disk.
     *
     * Complexity: O(n) for n vertices on this convex polygon.
     */
    template<PointConcept OtherPoint>
    constexpr bool interiorsIntersect(const Disk<OtherPoint>& other) const;

    /**
     * @brief Checks if the interior of the convex polygon intersects the interior of the wrapped shape.
     */
    template<PointConcept OtherPoint>
    constexpr bool interiorsIntersect(const Shape<OtherPoint>& other) const;

    /**
     * @brief A convex polygon never separates a point.
     *
     * Complexity: O(1).
     */
    template<PointConcept OtherPoint>
    constexpr bool separates(const OtherPoint&) const;

    /**
     * @brief Checks if removing the convex polygon from a segment disconnects it.
     *
     * Complexity: O(log n) for n vertices.
     *
     * A segment collinear with one of the convex polygon's boundary edges is not
     * separated by the convex polygon — the convex polygon only touches the segment along
     * the boundary, not through its interior, so the two outside extensions
     * are not produced by removing the convex polygon's body. We therefore require
     * the segment to cross the convex polygon interior, not just intersect it.
     */
    template<PointConcept OtherPoint>
    constexpr bool separates(const Segment<OtherPoint>& other) const;

    /**
     * @brief Checks if removing the convex polygon from an oriented segment disconnects it.
     *
     * Complexity: O(log n) for n vertices.
     */
    template<PointConcept OtherPoint>
    constexpr bool separates(const OrientedSegment<OtherPoint>& other) const;

    /**
     * @brief Checks if removing the convex polygon from a line disconnects it.
     *
     * Complexity: O(log n) for n vertices.
     */
    template<PointConcept OtherPoint>
    constexpr bool separates(const Line<OtherPoint>& other) const;

    /**
     * @brief Checks if removing the convex polygon from an oriented line disconnects it.
     *
     * Complexity: O(log n) for n vertices.
     */
    template<PointConcept OtherPoint>
    constexpr bool separates(const OrientedLine<OtherPoint>& other) const;

    /**
     * @brief Checks if removing the convex polygon from a ray disconnects it.
     *
     * Complexity: O(log n) for n vertices.
     *
     * The ray is split iff its source lies outside the closed polygon (so
     * the leading piece survives) and the ray actually intersects the convex polygon.
     */
    template<PointConcept OtherPoint>
    constexpr bool separates(const Ray<OtherPoint>& other) const;

    /**
     * @brief Polygons never separate half-planes.
     *
     * Complexity: O(1).
     */
    template<PointConcept OtherPoint>
    constexpr bool separates(const Halfplane<OtherPoint>&) const;

    /**
     * @brief Checks if the convex polygon separates a rectangle.
     *
     * Complexity: O(log n) for n vertices (four edge checks).
     */
    template<PointConcept OtherPoint>
    constexpr bool separates(const Rectangle<OtherPoint>& other) const;

    /**
     * @brief Checks if the convex polygon separates a triangle.
     *
     * Complexity: O(log n) for n vertices (three edge checks).
     */
    template<PointConcept OtherPoint>
    constexpr bool separates(const Triangle<OtherPoint>& other) const;

    /**
     * @brief Checks if the convex polygon separates another convex polygon.
     *
     * Complexity: O(min(n,m) log(m+n)) for convex polygons with n vertices and m vertices.
     */
    template<PointConcept OtherPoint>
    constexpr bool separates(const Convex<OtherPoint>& other) const;

    /** @brief Polygon overload; see the Convex overload. */
    template<PointConcept OtherPoint>
    constexpr bool separates(const Polygon<OtherPoint>& other) const;

    /**
     * @brief Checks if the convex polygon separates a disk.
     *
     * Complexity: O(n) for n vertices on this convex polygon.
     */
    template<PointConcept OtherPoint>
    constexpr bool separates(const Disk<OtherPoint>& other) const;

    /**
     * @brief Checks if the convex polygon separates the wrapped shape.
     */
    template<PointConcept OtherPoint>
    constexpr bool separates(const Shape<OtherPoint>& other) const;

    /**
     * @brief A convex polygon never crosses a point.
     *
     * Complexity: O(1).
     */
    template<PointConcept OtherPoint>
    constexpr bool crosses(const OtherPoint&) const;

    /**
     * @brief Checks if the convex polygon and segment cross.
     *
     * Complexity: O(log n) for n vertices.
     */
    template<PointConcept OtherPoint>
    constexpr bool crosses(const Segment<OtherPoint>& other) const;

    /**
     * @brief Checks if the convex polygon and oriented segment cross.
     *
     * Complexity: O(log n) for n vertices.
     */
    template<PointConcept OtherPoint>
    constexpr bool crosses(const OrientedSegment<OtherPoint>& other) const;

    /**
     * @brief Checks if the convex polygon and line cross.
     *
     * Complexity: O(log n) for n vertices.
     */
    template<PointConcept OtherPoint>
    constexpr bool crosses(const Line<OtherPoint>& other) const;

    /**
     * @brief Checks if the convex polygon and oriented line cross.
     *
     * Complexity: O(log n) for n vertices.
     */
    template<PointConcept OtherPoint>
    constexpr bool crosses(const OrientedLine<OtherPoint>& other) const;

    /**
     * @brief Checks if the convex polygon and ray cross.
     *
     * Complexity: O(log n) for n vertices.
     */
    template<PointConcept OtherPoint>
    constexpr bool crosses(const Ray<OtherPoint>& other) const;

    /**
     * @brief Convex polygons never cross half-planes.
     *
     * Complexity: O(1).
     */
    template<PointConcept OtherPoint>
    constexpr bool crosses(const Halfplane<OtherPoint>&) const;

    /**
     * @brief Checks if the convex polygon crosses a rectangle.
     *
     * Complexity: O(n) for n vertices, dominated by interiorsIntersect.
     */
    template<PointConcept OtherPoint>
    constexpr bool crosses(const Rectangle<OtherPoint>& other) const;

    /**
     * @brief Checks if the convex polygon crosses a triangle.
     *
     * Complexity: O(n) for n vertices, dominated by interiorsIntersect.
     */
    template<PointConcept OtherPoint>
    constexpr bool crosses(const Triangle<OtherPoint>& other) const;

    /**
     * @brief Checks if the convex polygon crosses another convex polygon.
     *
     * Complexity: O(n log m + m log n) for this polygon with n vertices and
     * the other with m vertices.
     */
    template<PointConcept OtherPoint>
    constexpr bool crosses(const Convex<OtherPoint>& other) const;

    /** @brief Polygon overload; see the Convex overload. */
    template<PointConcept OtherPoint>
    constexpr bool crosses(const Polygon<OtherPoint>& other) const;

    /**
     * @brief Checks if the convex polygon crosses a disk.
     *
     * Complexity: O(n) for n vertices on this convex polygon.
     */
    template<PointConcept OtherPoint>
    constexpr bool crosses(const Disk<OtherPoint>& other) const;

    /**
     * @brief Checks if the convex polygon crosses the wrapped shape.
     */
    template<PointConcept OtherPoint>
    constexpr bool crosses(const Shape<OtherPoint>& other) const;

    /**
     * @brief Computes the intersection of the convex polygon with a segment.
     *
     * Complexity: O(log n) for n vertices.
     *
     * @tparam ResultNumber The number type for the result.
     * @tparam OtherPoint The point type of the segment.
     * @param other The segment to intersect with.
     * @return An optional variant containing either a point or segment representing the intersection, or empty if no intersection.
     */
    template <class ResultNumber = NumberType, class OtherPoint>
    constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const Segment<OtherPoint>& other) const;

    /**
     * @brief Computes the intersection of the convex polygon with an oriented segment.
     * 
     * Complexity: O(log n) for n vertices.
     * 
     * @tparam ResultNumber The number type for the result.
     * @tparam OtherPoint The point type of the oriented segment.
     * @param other The oriented segment to intersect with.
     * @return An optional variant containing either a point or segment representing the intersection, or empty if no intersection.
     */
    template <class ResultNumber = NumberType, class OtherPoint>
    constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OrientedSegment<OtherPoint>& other) const;

    /**
     * @brief Computes the intersection of the convex polygon with a line.
     * 
     * Complexity: O(log n) for n vertices.
     *
     * @tparam ResultNumber The number type for the result.
     * @tparam OtherPoint The point type of the line.
     * @param other The line to intersect with.
     * @return An optional variant containing either a point or segment representing the intersection, or empty if no intersection.
     */
    template <class ResultNumber = NumberType, class OtherPoint>
    constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const Line<OtherPoint>& other) const;

    /**
     * @brief Computes the intersection of the convex polygon with an oriented line.
     * 
     * Complexity: O(log n) for n vertices.
     * 
     * @tparam ResultNumber The number type for the result.
     * @tparam OtherPoint The point type of the oriented line.
     * @param other The oriented line to intersect with.
     * @return An optional variant containing either a point or segment representing the intersection, or empty if no intersection.
     */
    template <class ResultNumber = NumberType, class OtherPoint>
    constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OrientedLine<OtherPoint>& other) const;

    /**
     * @brief Computes the intersection of the convex polygon with a ray.
     * 
     * Complexity: O(log n) for n vertices.
     * 
     * @tparam ResultNumber The number type for the result.
     * @tparam OtherPoint The point type of the ray.
     * @param other The ray to intersect with.
     * @return An optional variant containing either a point or segment representing the intersection, or empty if no intersection.
     */
    template <class ResultNumber = NumberType, class OtherPoint>
    constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const Ray<OtherPoint>& other) const;

    /**
     * @brief Computes the intersection of the convex polygon with a rectangle.
     * 
     * Complexity: O(log n) for n vertices.
     * 
     * @tparam ResultNumber The number type for the result.
     * @tparam OtherPoint The point type of the rectangle.
     * @param other The rectangle to intersect with.
     * @return An optional variant containing either a point or segment or convex polygon representing the intersection, or empty if no intersection.
     */
    template <class ResultNumber = NumberType, class OtherPoint>
    constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>, Convex<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const Rectangle<OtherPoint>& other) const;


    /**
     * @brief Computes the intersection of the convex polygon with a triangle.
     * 
     * Complexity: O(log n) for n vertices.
     * 
     * @tparam ResultNumber The number type for the result.
     * @tparam OtherPoint The point type of the triangle.
     * @param other The triangle to intersect with.
     * @return An optional variant containing either a point or segment or convex polygon representing the intersection, or empty if no intersection.
     */
    template <class ResultNumber = NumberType, class OtherPoint>
    constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>, Convex<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const Triangle<OtherPoint>& other) const;

    /**
     * @brief Computes the intersection of the convex polygon with another convex polygon.
     * 
     * Complexity: O((n+m) log(n+m)) for n and m vertices.
     * 
     * @tparam ResultNumber The number type for the result.
     * @tparam OtherPoint The point type of the other convex polygon.
     * @param other The other convex polygon to intersect with.
     * @return An optional variant containing either a point or segment or convex polygon representing the intersection, or empty if no intersection.
     */
    template <class ResultNumber = NumberType, class OtherPoint>
    constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>, Convex<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const Convex<OtherPoint>& other) const;

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
    PointType translation_{};

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

template <class PointType, class TranslationNumber, class TranslationLabel>
constexpr auto operator+(const Convex<PointType>& convex, const Point<TranslationNumber, TranslationLabel>& translation) {
    return translation + convex;
}

template <class TranslationNumber, class TranslationLabel, class PointType>
constexpr auto operator+(const Point<TranslationNumber, TranslationLabel>& translation, const Convex<PointType>& convex) {
    using ResultPointType = Point<TranslationNumber, typename PointType::LabelType>;
    Convex<ResultPointType> result(convex);
    result += translation;
    return result;
}

template <class PointType, class TranslationNumber, class TranslationLabel>
constexpr auto operator-(const Convex<PointType>& convex, const Point<TranslationNumber, TranslationLabel>& translation) {
    return convex + (-translation);
}

template <class PointType, class Scalar>
    requires(!detail::is_point_v<Scalar>)
constexpr auto operator*(const Convex<PointType>& convex, const Scalar& scalar) {
    using ResultPointType = Point<decltype(std::declval<PointType>().x() * scalar), typename PointType::LabelType>;
    Convex<ResultPointType> result(convex);
    result *= scalar;
    return result;
}

template <class Scalar, class PointType>
    requires(!detail::is_point_v<Scalar>)
constexpr auto operator*(const Scalar& scalar, const Convex<PointType>& convex) {
    return convex * scalar;
}

template <class PointType, class Scalar>
    requires(!detail::is_point_v<Scalar>)
constexpr auto operator/(const Convex<PointType>& convex, const Scalar& scalar) {
    using ResultPointType = Point<decltype(std::declval<PointType>().x() / scalar), typename PointType::LabelType>;
    Convex<ResultPointType> result(convex);
    result /= scalar;
    return result;
}

template <class PointType>
std::ostream& operator<<(std::ostream& stream, const Convex<PointType>& convex) {
    stream << "Convex[";
    for (size_t i = 0; i < convex.size(); ++i) {
        if (i > 0) {
            stream << ",";
        }
        stream << convex[i];
    }
    stream << "]";
    return stream;
}

}  // namespace pgl
