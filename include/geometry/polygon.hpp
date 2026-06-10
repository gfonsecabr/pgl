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

namespace pgl {

template <class PointType = Point<>>
struct Polygon;

Polygon() -> Polygon<Point<>>;

template <std::ranges::input_range Range>
requires detail::is_point_v<std::ranges::range_value_t<Range>>
Polygon(Range&&) -> Polygon<std::remove_cvref_t<std::ranges::range_value_t<Range>>>;

template <class Number>
requires (!detail::is_point_v<Number>)
Polygon(std::initializer_list<Number>) -> Polygon<Point<Number>>;

template <class Number>
requires (!detail::is_point_v<Number>)
Polygon(std::initializer_list<Number>, bool) -> Polygon<Point<Number>>;


/**
 * @brief A simple polygon stored by its vertices plus a translation.
 *
 * `Polygon` mirrors the storage layout of @ref Convex — a vector of vertices
 * and a `translation_` applied lazily on access — but makes no convexity
 * assumption. The boundary is the closed polyline through the vertices in the
 * stored order, with the last vertex joined back to the first.
 *
 * The constructor normalizes the vertex sequence to a canonical form: it is
 * oriented counterclockwise and rotated so the lexicographically smallest
 * vertex (smallest x, ties broken by smallest y) comes first. Because a
 * constant translation preserves both orientation and lexicographic order,
 * `operator==`/`operator<=>` give a translation-consistent geometric equality.
 *
 * @tparam PointType_ The vertex point type.
 */
template <class PointType_>
struct Polygon {
    using PointType = PointType_;
    using NumberType = PointType::NumberType;
    static_assert(detail::is_point_v<PointType>, "Polygon requires pgl::Point vertices");

    template <bool Oriented>
    using BoundaryType = std::conditional_t<Oriented, OrientedSegment<PointType>, Segment<PointType>>;

    template <bool Oriented>
    class BoundaryIterator;

    using EdgeIterator = BoundaryIterator<false>;
    using OrientedEdgeIterator = BoundaryIterator<true>;

    /**
     * @brief Creates a polygon with no vertex.
     */
    constexpr Polygon() = default;

    /**
     * @brief Creates a polygon from a range of points.
     *
     * The points must be given in the order they appear along the boundary.
     * Unless @p trusted is set, the vertices are normalized to the canonical
     * form (counterclockwise, lexicographically smallest vertex first).
     *
     * @tparam Range Input range whose elements can be converted to @ref PointType.
     * @param points Range of boundary points in order.
     * @param trusted Set to true if the points are already in canonical form.
     */
    template<std::ranges::input_range Range = std::initializer_list<PointType>>
    requires std::ranges::common_range<Range> &&
             std::convertible_to<std::ranges::range_value_t<Range>, PointType>
    constexpr explicit Polygon(Range&& points, bool trusted = false) {
        for (const auto& p : points) {
            points_.push_back(p);
        }
        if (!trusted) {
            normalize();
        }
    }

    /**
     * @brief Creates a polygon from a flat list of coordinates.
     *
     * The values are consumed in pairs `(x0, y0, x1, y1, …)`, each pair forming
     * one boundary vertex in order, so the list must hold an even number of
     * values. Unless @p trusted is set, the vertices are normalized to the
     * canonical form (counterclockwise, lexicographically smallest vertex first).
     *
     * @param coords Interleaved x/y coordinates of the boundary vertices.
     * @param trusted Set to true if the points are already in canonical form.
     */
    constexpr explicit Polygon(std::initializer_list<NumberType> coords, bool trusted = false) {
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
    }

    /**
     * @brief Converts a polygon with compatible vertex type.
     *
     * The source is already canonical and a translation/type conversion
     * preserves that, so no renormalization is needed.
     *
     * @tparam OtherPointType Source vertex type.
     * @param other Source polygon.
     */
    template<PointConcept OtherPointType>
        requires(std::constructible_from<PointType, const OtherPointType&>)
    constexpr Polygon(const Polygon<OtherPointType>& other) : points_(other.begin(), other.end()) {}

    /**
     * @brief Accesses a vertex by index.
     * @param index The index of the vertex.
     * @return The vertex at the given index.
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
     * @brief Returns the smallest index `i` with `(*this)[i] == point`, or
     * `-1` if `point` is not a vertex.
     *
     * Complexity: O(n) for n vertices (linear scan, since a simple polygon
     * has no monotone structure to binary-search).
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
     * @brief Compares two polygons by their canonical vertex sequences.
     */
    constexpr auto operator<=>(const Polygon& other) const {
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
     * @brief Checks equality of two polygons.
     * @return True if both polygons have the same vertices in the same order.
     */
    constexpr bool operator==(const Polygon& other) const {
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
     * @brief Returns the number of vertices in the polygon.
     */
    constexpr std::size_t size() const {
        return points_.size();
    }

    /**
     * @brief Computes twice the (unsigned) area of the polygon via the shoelace formula.
     * @return Twice the area, or zero for fewer than three vertices.
     */
    constexpr auto twiceArea() const {
        if (points_.size() < 3) {
            return NumberType(0);
        }
        return pgl::detail::abs(signedTwiceArea());
    }

    /**
     * @brief Computes the area of the polygon.
     * @warning Uses division by 2.
     */
    template <class ResultNumber = NumberType>
    constexpr auto area() const {
        ResultNumber result = static_cast<ResultNumber>(twiceArea());
        return result / ResultNumber(2);
    }

    /**
     * @brief Checks if the polygon is degenerate (has zero area).
     */
    constexpr bool isDegenerate() const {
        return twiceArea() == NumberType(0);
    }

    /**
     * @brief Returns a segment realizing the diameter (the farthest vertex pair).
     *
     * The farthest pair of vertices of a simple polygon lies on its convex
     * hull, so this builds a @ref Convex from the polygon vertices and returns
     * that hull's @ref Convex::diameter(). Distances are compared exactly via
     * squared length.
     *
     * @return A longest segment between two vertices (degenerate if fewer than
     *         two vertices).
     */
    constexpr Segment<PointType> diameter() const {
        return Convex<PointType>(vertices()).diameter();
    }

    /**
     * @brief Returns the vertices of the polygon (translation applied).
     */
    constexpr std::vector<PointType> vertices() const {
        auto ret = points_;
        for (auto& vertex : ret) {
            vertex += translation_;
        }
        return ret;
    }

    /**
     * @brief Returns the edges of the polygon.
     */
    constexpr std::vector<Segment<PointType>> edges() const {
        std::vector<Segment<PointType>> result;
        const auto translatedVertices = vertices();
        for (std::size_t i = 0; i < translatedVertices.size(); ++i) {
            const auto& p1 = translatedVertices[i];
            const auto& p2 = translatedVertices[(i + 1) % translatedVertices.size()];
            result.emplace_back(p1, p2);
        }
        return result;
    }

    /**
     * @brief Returns the oriented edges of the polygon.
     */
    constexpr std::vector<OrientedSegment<PointType>> orientedEdges() const {
        std::vector<OrientedSegment<PointType>> result;
        const auto translatedVertices = vertices();
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
     * @brief Computes the area-weighted centroid of the polygon.
     * @tparam ResultNumber The number type for the result.
     * @warning Uses division.
     */
    template <class ResultNumber = NumberType>
    constexpr Point<ResultNumber> centroid() const {
        if (points_.empty()) {
            return Point<ResultNumber>();
        }
        const auto areaTwice = signedTwiceArea();
        if (points_.size() < 3 || areaTwice == NumberType(0)) {
            return verticesCentroid<ResultNumber>();
        }
        ResultNumber cx = 0;
        ResultNumber cy = 0;
        const std::size_t n = points_.size();
        for (std::size_t i = 0; i < n; ++i) {
            const auto& p1 = points_[i];
            const auto& p2 = points_[(i + 1) % n];
            const ResultNumber cross = static_cast<ResultNumber>(p1.x()) * static_cast<ResultNumber>(p2.y())
                                     - static_cast<ResultNumber>(p2.x()) * static_cast<ResultNumber>(p1.y());
            cx += (static_cast<ResultNumber>(p1.x()) + static_cast<ResultNumber>(p2.x())) * cross;
            cy += (static_cast<ResultNumber>(p1.y()) + static_cast<ResultNumber>(p2.y())) * cross;
        }
        const ResultNumber denom = ResultNumber(3) * static_cast<ResultNumber>(areaTwice);
        return Point<ResultNumber>(cx / denom, cy / denom) + static_cast<Point<ResultNumber>>(translation_);
    }

    /**
     * @brief Computes the centroid of the vertex set (the average of the vertices).
     * @tparam ResultNumber The number type for the result.
     * @warning Uses division by the number of vertices.
     */
    template <class ResultNumber = NumberType>
    constexpr Point<ResultNumber> verticesCentroid() const {
        if (points_.empty()) {
            return Point<ResultNumber>();
        }
        ResultNumber cx = 0;
        ResultNumber cy = 0;
        for (const auto& vertex : points_) {
            cx += static_cast<ResultNumber>(vertex.x());
            cy += static_cast<ResultNumber>(vertex.y());
        }
        return Point<ResultNumber>(cx / static_cast<ResultNumber>(points_.size()),
                                   cy / static_cast<ResultNumber>(points_.size()))
               + static_cast<Point<ResultNumber>>(translation_);
    }

    /**
     * @brief Checks if the closed polygon contains the given point (boundary or interior).
     *
     * Uses an exact winding-number test, preceded by an explicit boundary
     * check so the closed boundary counts as contained.
     *
     * Complexity: O(n) for n vertices.
     */
    template<PointConcept OtherPoint>
    constexpr bool contains(const OtherPoint& point) const;

    /**
     * @brief Checks if the closed polygon contains the given segment.
     *
     * The segment is split at its boundary intersections and each piece is
     * classified by its midpoint, so the test is correct for non-convex
     * polygons (both endpoints inside does not suffice).
     *
     * Complexity: O(n log n) for n vertices.
     */
    template<PointConcept OtherPoint, class OtherLabel>
    constexpr bool contains(const Segment<OtherPoint, OtherLabel>& other) const;

    /**
     * @brief Checks if the closed polygon contains the given oriented segment.
     *
     * Complexity: O(n log n) for n vertices.
     */
    template<PointConcept OtherPoint>
    constexpr bool contains(const OrientedSegment<OtherPoint>& other) const;

    /**
     * @brief A bounded polygon contains a line only if the line is degenerate.
     */
    template<PointConcept OtherPoint>
    constexpr bool contains(const Line<OtherPoint>& other) const;

    /**
     * @brief A bounded polygon contains an oriented line only if it is degenerate.
     */
    template<PointConcept OtherPoint>
    constexpr bool contains(const OrientedLine<OtherPoint>& other) const;

    /**
     * @brief A bounded polygon contains a ray only if the ray is degenerate.
     */
    template<PointConcept OtherPoint>
    constexpr bool contains(const Ray<OtherPoint>& other) const;

    /**
     * @brief A bounded polygon contains a half-plane only if it is degenerate.
     */
    template<PointConcept OtherPoint>
    constexpr bool contains(const Halfplane<OtherPoint>& other) const;

    /**
     * @brief Checks if the polygon contains the given rectangle.
     *
     * Complexity: O(n log n) for n vertices.
     */
    template<PointConcept OtherPoint>
    constexpr bool contains(const Rectangle<OtherPoint>& other) const;

    /**
     * @brief Checks if the polygon contains the given triangle.
     *
     * Complexity: O(n log n) for n vertices.
     */
    template<PointConcept OtherPoint>
    constexpr bool contains(const Triangle<OtherPoint>& other) const;

    /**
     * @brief Checks if the polygon contains the given convex polygon.
     *
     * Complexity: O((n + m) log n) for n and m vertices.
     */
    template<PointConcept OtherPoint>
    constexpr bool contains(const Convex<OtherPoint>& other) const;

    /**
     * @brief Checks if the polygon contains the given polygon.
     *
     * For simple polygons (no holes) this holds iff every edge of @p other is
     * contained, which is what this checks.
     *
     * Complexity: O((n + m) log n) for n and m vertices.
     */
    template<PointConcept OtherPoint>
    constexpr bool contains(const Polygon<OtherPoint>& other) const;

    /**
     * @brief Checks if the polygon contains the wrapped shape.
     */
    constexpr bool contains(const Shape<PointType>& other) const;

    /**
     * @brief Checks if the open interior of the polygon contains the given point.
     *
     * True iff the point is contained but lies on no edge. A polygon with fewer
     * than three vertices has empty interior, so the result is always false.
     *
     * Complexity: O(n) for n vertices.
     */
    template<PointConcept OtherPoint>
    constexpr bool interiorContains(const OtherPoint& point) const;

    /**
     * @brief Checks if the open interior of the polygon contains the given segment.
     *
     * Requires both endpoints strictly inside and no contact with the boundary,
     * so a segment cannot dip out through a reflex notch and return.
     *
     * Complexity: O(n) for n vertices.
     */
    template<PointConcept OtherPoint, class OtherLabel>
    constexpr bool interiorContains(const Segment<OtherPoint, OtherLabel>& other) const;

    /**
     * @brief Checks if the open interior of the polygon contains the given oriented segment.
     *
     * Complexity: O(n) for n vertices.
     */
    template<PointConcept OtherPoint>
    constexpr bool interiorContains(const OrientedSegment<OtherPoint>& other) const;

    /**
     * @brief A bounded polygon's interior contains a line only if it is degenerate.
     */
    template<PointConcept OtherPoint>
    constexpr bool interiorContains(const Line<OtherPoint>& other) const;

    /**
     * @brief A bounded polygon's interior contains an oriented line only if it is degenerate.
     */
    template<PointConcept OtherPoint>
    constexpr bool interiorContains(const OrientedLine<OtherPoint>& other) const;

    /**
     * @brief A bounded polygon's interior contains a ray only if it is degenerate.
     */
    template<PointConcept OtherPoint>
    constexpr bool interiorContains(const Ray<OtherPoint>& other) const;

    /**
     * @brief A bounded polygon's interior contains a half-plane only if it is degenerate.
     */
    template<PointConcept OtherPoint>
    constexpr bool interiorContains(const Halfplane<OtherPoint>& other) const;

    /**
     * @brief Checks if the open interior of the polygon contains the given rectangle.
     *
     * Complexity: O(n log n) for n vertices.
     */
    template<PointConcept OtherPoint>
    constexpr bool interiorContains(const Rectangle<OtherPoint>& other) const;

    /**
     * @brief Checks if the open interior of the polygon contains the given triangle.
     *
     * Complexity: O(n log n) for n vertices.
     */
    template<PointConcept OtherPoint>
    constexpr bool interiorContains(const Triangle<OtherPoint>& other) const;

    /**
     * @brief Checks if the open interior of the polygon contains the given convex polygon.
     *
     * Complexity: O((n + m) log n) for n and m vertices.
     */
    template<PointConcept OtherPoint>
    constexpr bool interiorContains(const Convex<OtherPoint>& other) const;

    /**
     * @brief Checks if the open interior of the polygon contains the given polygon.
     *
     * Like @ref contains(const Polygon&), this reduces to an edge-by-edge check,
     * which is exact for simple polygons (no holes).
     *
     * Complexity: O((n + m) log n) for n and m vertices.
     */
    template<PointConcept OtherPoint>
    constexpr bool interiorContains(const Polygon<OtherPoint>& other) const;

    /**
     * @brief Checks if the boundary of the polygon contains the given point.
     *
     * Complexity: O(n) for n vertices.
     */
    template<PointConcept OtherPoint>
    constexpr bool boundaryContains(const OtherPoint& point) const;

    /**
     * @brief Checks if the boundary of the polygon contains the given segment.
     *
     * True iff the segment lies within a single boundary edge (the simple-polygon
     * model also used by @ref Convex::boundaryContains).
     *
     * Complexity: O(n) for n vertices.
     */
    template<PointConcept OtherPoint, class OtherLabel>
    constexpr bool boundaryContains(const Segment<OtherPoint, OtherLabel>& other) const;

    /** @brief Same as the segment overload, ignoring orientation. */
    template<PointConcept OtherPoint>
    constexpr bool boundaryContains(const OrientedSegment<OtherPoint>& other) const;

    /** @brief Degenerate lines are on the boundary iff their unique point is. */
    template<PointConcept OtherPoint>
    constexpr bool boundaryContains(const Line<OtherPoint>& other) const;

    /** @brief Degenerate oriented lines are on the boundary iff their unique point is. */
    template<PointConcept OtherPoint>
    constexpr bool boundaryContains(const OrientedLine<OtherPoint>& other) const;

    /** @brief Degenerate rays are on the boundary iff their source point is. */
    template<PointConcept OtherPoint>
    constexpr bool boundaryContains(const Ray<OtherPoint>& other) const;

    /** @brief Degenerate half-planes are on the boundary iff their source point is. */
    template<PointConcept OtherPoint>
    constexpr bool boundaryContains(const Halfplane<OtherPoint>& other) const;

    /**
     * @brief Checks if every rectangle edge lies on the polygon boundary.
     *
     * Complexity: O(n) per edge for n vertices.
     */
    template<PointConcept OtherPoint>
    constexpr bool boundaryContains(const Rectangle<OtherPoint>& other) const;

    /**
     * @brief Checks if every triangle edge lies on the polygon boundary.
     *
     * Complexity: O(n) per edge for n vertices.
     */
    template<PointConcept OtherPoint>
    constexpr bool boundaryContains(const Triangle<OtherPoint>& other) const;

    /**
     * @brief A convex polygon lies on the boundary only if it has at most two vertices.
     */
    template<PointConcept OtherPoint>
    constexpr bool boundaryContains(const Convex<OtherPoint>& other) const;

    /**
     * @brief A polygon lies on the boundary only if it has at most two vertices.
     */
    template<PointConcept OtherPoint>
    constexpr bool boundaryContains(const Polygon<OtherPoint>& other) const;

    /**
     * @brief A disk lies on the boundary only if it is a single point on the boundary.
     */
    template<PointConcept OtherPoint, class OtherLabel>
    constexpr bool boundaryContains(const Disk<OtherPoint, OtherLabel>& other) const;

    /**
     * @brief Checks if the polygon boundary contains the wrapped shape.
     */
    template<PointConcept OtherPoint>
    constexpr bool boundaryContains(const Shape<OtherPoint>& other) const;

    // --- not-yet-implemented predicate pairs (throw); see implementation ---
    template<PointConcept OtherPoint, class OtherLabel>
    [[nodiscard]] constexpr bool interiorContains(const Disk<OtherPoint, OtherLabel>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool separates(const OtherPoint& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool separates(const Halfplane<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool separates(const Rectangle<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool separates(const Triangle<OtherPoint>& other) const;

    template<PointConcept OtherPoint, class OtherLabel>
    [[nodiscard]] constexpr bool separates(const Disk<OtherPoint, OtherLabel>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool separates(const Convex<OtherPoint>& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool separates(const Polygon<OtherPoint>& other) const;


    /**
     * @brief Checks if the polygon intersects the given point.
     *
     * Complexity: O(n) for n vertices.
     */
    template<PointConcept OtherPoint>
    constexpr bool intersects(const OtherPoint& other) const;

    /**
     * @brief Checks if the polygon intersects the given segment.
     *
     * Complexity: O(n) for n vertices.
     */
    template<PointConcept OtherPoint, class OtherLabel>
    constexpr bool intersects(const Segment<OtherPoint, OtherLabel>& other) const;

    /**
     * @brief Checks if the polygon intersects the given oriented segment.
     *
     * Complexity: O(n) for n vertices.
     */
    template<PointConcept OtherPoint>
    constexpr bool intersects(const OrientedSegment<OtherPoint>& other) const;

    /**
     * @brief Checks if the polygon intersects the given line.
     *
     * Complexity: O(n) for n vertices.
     */
    template<PointConcept OtherPoint>
    constexpr bool intersects(const Line<OtherPoint>& other) const;

    /**
     * @brief Checks if the polygon intersects the given oriented line.
     *
     * Complexity: O(n) for n vertices.
     */
    template<PointConcept OtherPoint>
    constexpr bool intersects(const OrientedLine<OtherPoint>& other) const;

    /**
     * @brief Checks if the polygon intersects the given ray.
     *
     * Complexity: O(n) for n vertices.
     */
    template<PointConcept OtherPoint>
    constexpr bool intersects(const Ray<OtherPoint>& other) const;

    /**
     * @brief Checks if the polygon intersects the given half-plane.
     *
     * Complexity: O(n) for n vertices.
     */
    template<PointConcept OtherPoint>
    constexpr bool intersects(const Halfplane<OtherPoint>& other) const;

    /**
     * @brief Checks if the polygon intersects the given rectangle.
     *
     * Complexity: O(n m) for polygons with n and m vertices.
     */
    template<PointConcept OtherPoint>
    constexpr bool intersects(const Rectangle<OtherPoint>& other) const;

    /**
     * @brief Checks if the polygon intersects the given triangle.
     *
     * Complexity: O(n m) for polygons with n and m vertices.
     */
    template<PointConcept OtherPoint>
    constexpr bool intersects(const Triangle<OtherPoint>& other) const;

    /**
     * @brief Checks if the polygon intersects the given convex polygon.
     *
     * Complexity: O(n m) for polygons with n and m vertices.
     */
    template<PointConcept OtherPoint>
    constexpr bool intersects(const Convex<OtherPoint>& other) const;

    /**
     * @brief Checks if the polygon intersects the given polygon.
     *
     * Complexity: O(n m) for polygons with n and m vertices.
     */
    template<PointConcept OtherPoint>
    constexpr bool intersects(const Polygon<OtherPoint>& other) const;

    /** @brief Not yet supported; throws (Polygon cannot yet `intersects` a disk). */
    template<PointConcept OtherPoint, class OtherLabel>
    constexpr bool intersects(const Disk<OtherPoint, OtherLabel>& other) const;

    /**
     * @brief Checks if the polygon interior strictly contains the given point.
     *
     * Complexity: O(n) for n vertices.
     */
    template<PointConcept OtherPoint>
    constexpr bool interiorsIntersect(const OtherPoint& other) const;

    /**
     * @brief Checks if the polygon interior meets the given line.
     *
     * Complexity: O(n) for n vertices.
     */
    template<PointConcept OtherPoint>
    constexpr bool interiorsIntersect(const Line<OtherPoint>& other) const;

    /**
     * @brief Checks if the polygon interior meets the given oriented line.
     *
     * Complexity: O(n) for n vertices.
     */
    template<PointConcept OtherPoint>
    constexpr bool interiorsIntersect(const OrientedLine<OtherPoint>& other) const;

    /**
     * @brief Checks if the polygon interior meets the given segment.
     *
     * Complexity: O(n^2) for n vertices.
     */
    template<PointConcept OtherPoint, class OtherLabel>
    constexpr bool interiorsIntersect(const Segment<OtherPoint, OtherLabel>& other) const;

    /**
     * @brief Checks if the polygon interior meets the given oriented segment.
     *
     * Complexity: O(n^2) for n vertices.
     */
    template<PointConcept OtherPoint>
    constexpr bool interiorsIntersect(const OrientedSegment<OtherPoint>& other) const;

    /**
     * @brief Checks if the polygon interior meets the given ray.
     *
     * Complexity: O(n^2) for n vertices.
     */
    template<PointConcept OtherPoint>
    constexpr bool interiorsIntersect(const Ray<OtherPoint>& other) const;

    /**
     * @brief Checks if the polygon interior meets the given half-plane.
     *
     * Complexity: O(n) for n vertices.
     */
    template<PointConcept OtherPoint>
    constexpr bool interiorsIntersect(const Halfplane<OtherPoint>& other) const;

    /**
     * @brief Checks if the polygon interior meets the given rectangle interior.
     *
     * Complexity: O(n m) for polygons with n and m vertices.
     */
    template<PointConcept OtherPoint>
    constexpr bool interiorsIntersect(const Rectangle<OtherPoint>& other) const;

    /**
     * @brief Checks if the polygon interior meets the given triangle interior.
     *
     * Complexity: O(n m) for polygons with n and m vertices.
     */
    template<PointConcept OtherPoint>
    constexpr bool interiorsIntersect(const Triangle<OtherPoint>& other) const;

    /**
     * @brief Checks if the polygon interior meets the given convex polygon interior.
     *
     * Complexity: O(n m) for polygons with n and m vertices.
     */
    template<PointConcept OtherPoint>
    constexpr bool interiorsIntersect(const Convex<OtherPoint>& other) const;

    /**
     * @brief Checks if the polygon interior meets the given polygon interior.
     *
     * Complexity: O(n m) for polygons with n and m vertices.
     */
    template<PointConcept OtherPoint>
    constexpr bool interiorsIntersect(const Polygon<OtherPoint>& other) const;

    /** @brief Not yet supported; throws (Polygon cannot yet `interiorsIntersect` a disk). */
    template<PointConcept OtherPoint, class OtherLabel>
    constexpr bool interiorsIntersect(const Disk<OtherPoint, OtherLabel>& other) const;

    /**
     * @brief Checks if removing the polygon disconnects the given segment.
     *
     * True iff some boundary edge cuts transversally through the segment's
     * interior while the segment does not lie on the boundary, so the polygon's
     * body interrupts the segment.
     *
     * Complexity: O(n) for n vertices.
     */
    template<PointConcept OtherPoint, class OtherLabel>
    constexpr bool separates(const Segment<OtherPoint, OtherLabel>& other) const;

    /** @brief Same as the segment overload, ignoring orientation. */
    template<PointConcept OtherPoint>
    constexpr bool separates(const OrientedSegment<OtherPoint>& other) const;

    /**
     * @brief Checks if removing the polygon disconnects the given ray.
     *
     * Like the segment overload, but a ray has a single finite end (its
     * source); its far end runs to infinity, always outside the bounded
     * polygon, so only the source can lie inside.
     *
     * Complexity: O(n) for n vertices.
     */
    template<PointConcept OtherPoint>
    constexpr bool separates(const Ray<OtherPoint>& other) const;

    /**
     * @brief Checks if removing the polygon disconnects the given line.
     * Complexity: O(n) for n vertices.
     */
    template<PointConcept OtherPoint>
    constexpr bool separates(const Line<OtherPoint>& other) const;

    /** @brief Same as the line overload, ignoring orientation. */
    template<PointConcept OtherPoint>
    constexpr bool separates(const OrientedLine<OtherPoint>& other) const;

    /** @brief A polygon never crosses a point. */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool crosses(const OtherPoint&) const;

    /** @brief Tests whether the polygon and segment mutually separate each other. */
    template<PointConcept OtherPoint, class OtherLabel>
    [[nodiscard]] constexpr bool crosses(const Segment<OtherPoint, OtherLabel>& other) const;

    /** @brief Same as the segment overload, ignoring orientation. */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool crosses(const OrientedSegment<OtherPoint>& other) const;

    /** @brief Tests whether the polygon and ray mutually separate each other. */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool crosses(const Ray<OtherPoint>& other) const;

    /** @brief Tests whether the polygon and line mutually separate each other. */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool crosses(const Line<OtherPoint>& other) const;

    /** @brief Same as the line overload, ignoring orientation. */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool crosses(const OrientedLine<OtherPoint>& other) const;

    /** @brief A polygon never crosses a half-plane in this API. */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool crosses(const Halfplane<OtherPoint>&) const;

    /** @brief Not yet supported; always false (Polygon cannot yet `separates` this shape). */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool crosses(const Rectangle<OtherPoint>&) const;

    /** @brief Not yet supported; always false (Polygon cannot yet `separates` this shape). */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool crosses(const Triangle<OtherPoint>&) const;

    /** @brief Not yet supported; always false (Polygon cannot yet `separates` this shape). */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool crosses(const Convex<OtherPoint>&) const;

    /** @brief Not yet supported; always false (Polygon cannot yet `separates` this shape). */
    template<PointConcept OtherPoint, class OtherLabel>
    [[nodiscard]] constexpr bool crosses(const Disk<OtherPoint, OtherLabel>&) const;

    /** @brief Not yet supported; always false (Polygon cannot yet `separates` this shape). */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool crosses(const Polygon<OtherPoint>&) const;

    /** @brief Dispatches crossing to the wrapped shape alternative. */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool crosses(const Shape<OtherPoint>& other) const;

    /**
     * @brief Returns the polygon rotated by 90k degrees around the origin.
     *
     * @param k Number of 90-degree CCW rotations (may be negative).
     * @return Rotated polygon.
     */
    [[nodiscard]] constexpr Polygon rotated90(int k = 1) const;

    /**
     * @brief Rotates the polygon by 90k degrees around the origin in place.
     *
     * @param k Number of 90-degree CCW rotations (may be negative).
     */
    constexpr void rotate90(int k = 1);

    /** @brief Returns the polygon with its x-coordinates multiplied by a factor. */
    template <class OtherNumber>
    [[nodiscard]] constexpr Polygon scaledUpX(const OtherNumber scalar) const;

    /** @brief Multiplies the polygon's x-coordinates by a factor in place. */
    template <class OtherNumber>
    constexpr void scaleUpX(const OtherNumber scalar);

    /** @brief Returns the polygon with its y-coordinates multiplied by a factor. */
    template <class OtherNumber>
    [[nodiscard]] constexpr Polygon scaledUpY(const OtherNumber scalar) const;

    /** @brief Multiplies the polygon's y-coordinates by a factor in place. */
    template <class OtherNumber>
    constexpr void scaleUpY(const OtherNumber scalar);

    /** @brief Returns the polygon with its x-coordinates divided by a divisor. */
    template <class OtherNumber>
    [[nodiscard]] constexpr Polygon scaledDownX(const OtherNumber scalar) const;

    /** @brief Divides the polygon's x-coordinates by a divisor in place. */
    template <class OtherNumber>
    constexpr void scaleDownX(const OtherNumber scalar);

    /** @brief Returns the polygon with its y-coordinates divided by a divisor. */
    template <class OtherNumber>
    [[nodiscard]] constexpr Polygon scaledDownY(const OtherNumber scalar) const;

    /** @brief Divides the polygon's y-coordinates by a divisor in place. */
    template <class OtherNumber>
    constexpr void scaleDownY(const OtherNumber scalar);

    /**
     * @brief Translates the polygon by the given point.
     *
     * Complexity: O(1).
     */
    template<PointConcept OtherPoint>
    constexpr Polygon& operator+=(const OtherPoint& translation) {
        translation_ += translation;
        return *this;
    }

    /**
     * @brief Translates the polygon by the negation of the given point.
     *
     * Complexity: O(1).
     */
    template<PointConcept OtherPoint>
    constexpr Polygon& operator-=(const OtherPoint& translation) {
        translation_ -= translation;
        return *this;
    }

    /**
     * @brief Scales the polygon by the given scalar.
     *
     * Complexity: O(n) for n vertices. Scaling by a negative factor flips the
     * orientation, so the polygon is renormalized to stay canonical.
     */
    template <class Scalar>
        requires(!detail::is_point_v<Scalar>)
    constexpr Polygon& operator*=(const Scalar& scalar) {
        for (auto& vertex : points_) {
            vertex *= scalar;
        }
        translation_ *= scalar;
        normalize();
        return *this;
    }

    /**
     * @brief Divides the polygon by the given scalar.
     *
     * Complexity: O(n) for n vertices.
     */
    template <class Scalar>
        requires(!detail::is_point_v<Scalar>)
    constexpr Polygon& operator/=(const Scalar& scalar) {
        for (auto& vertex : points_) {
            vertex /= scalar;
        }
        translation_ /= scalar;
        normalize();
        return *this;
    }

    /**
     * @brief Forward iterator over the (optionally oriented) boundary edges.
     *
     * Edge `i` joins vertex `i` to vertex `i + 1` (cyclically), so a polygon
     * with `size()` vertices has `size()` edges.
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
            assert(polygon != nullptr);
            return polygon->template boundaryAt<Oriented>(index);
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
        friend struct Polygon;

        constexpr BoundaryIterator(const Polygon* polygon_arg, std::size_t index_arg)
            : polygon(polygon_arg), index(index_arg) {}

        const Polygon* polygon = nullptr;
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

    /**
     * @brief Twice the signed area (shoelace) of the untranslated vertices.
     *
     * Positive for a counterclockwise boundary, negative for clockwise. The
     * translation is irrelevant to orientation, so it is ignored here.
     */
    constexpr NumberType signedTwiceArea() const {
        NumberType sum = 0;
        const std::size_t n = points_.size();
        for (std::size_t i = 0; i < n; ++i) {
            const auto& p1 = points_[i];
            const auto& p2 = points_[(i + 1) % n];
            sum += p1.x() * p2.y() - p2.x() * p1.y();
        }
        return sum;
    }

    /**
     * @brief Brings the stored vertices to canonical form: counterclockwise,
     * with the lexicographically smallest vertex first.
     */
    constexpr void normalize() {
        if (points_.empty()) {
            return;
        }
        if (points_.size() >= 3 && signedTwiceArea() < NumberType(0)) {
            std::reverse(points_.begin(), points_.end());
        }
        const auto minIt = std::min_element(points_.begin(), points_.end());
        std::rotate(points_.begin(), minIt, points_.end());
    }

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
}; // struct Polygon

template <class PointType, class TranslationNumber, class TranslationLabel>
constexpr auto operator+(const Polygon<PointType>& polygon, const Point<TranslationNumber, TranslationLabel>& translation) {
    return translation + polygon;
}

template <class TranslationNumber, class TranslationLabel, class PointType>
constexpr auto operator+(const Point<TranslationNumber, TranslationLabel>& translation, const Polygon<PointType>& polygon) {
    using ResultPointType = Point<TranslationNumber, typename PointType::LabelType>;
    Polygon<ResultPointType> result(polygon);
    result += translation;
    return result;
}

template <class PointType, class TranslationNumber, class TranslationLabel>
constexpr auto operator-(const Polygon<PointType>& polygon, const Point<TranslationNumber, TranslationLabel>& translation) {
    return polygon + (-translation);
}

template <class PointType, class Scalar>
    requires(!detail::is_point_v<Scalar>)
constexpr auto operator*(const Polygon<PointType>& polygon, const Scalar& scalar) {
    using ResultPointType = Point<decltype(std::declval<PointType>().x() * scalar), typename PointType::LabelType>;
    Polygon<ResultPointType> result(polygon);
    result *= scalar;
    return result;
}

template <class Scalar, class PointType>
    requires(!detail::is_point_v<Scalar>)
constexpr auto operator*(const Scalar& scalar, const Polygon<PointType>& polygon) {
    return polygon * scalar;
}

template <class PointType, class Scalar>
    requires(!detail::is_point_v<Scalar>)
constexpr auto operator/(const Polygon<PointType>& polygon, const Scalar& scalar) {
    using ResultPointType = Point<decltype(std::declval<PointType>().x() / scalar), typename PointType::LabelType>;
    Polygon<ResultPointType> result(polygon);
    result /= scalar;
    return result;
}

template <class PointType>
std::ostream& operator<<(std::ostream& stream, const Polygon<PointType>& polygon) {
    stream << "Polygon[";
    for (std::size_t i = 0; i < polygon.size(); ++i) {
        if (i > 0) {
            stream << ",";
        }
        stream << polygon[i];
    }
    stream << "]";
    return stream;
}

}  // namespace pgl
