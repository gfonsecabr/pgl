#pragma once

#include "shape/polyline.hpp"

#include <algorithm>
#include <vector>
#include <cassert>
#include <compare>
#include <concepts>
#include <cstddef>
#include <functional>
#include <iterator>
#include <ranges>
#include <limits>
#include <optional>
#include <ostream>
#include <span>
#include <type_traits>
#include <utility>


namespace pgl {

template <class PointType = Point<>, class Label>
struct Polygon;

Polygon() -> Polygon<Point<>, NoLabel>;

template <std::ranges::input_range Range>
requires detail::is_point_v<std::ranges::range_value_t<Range>>
Polygon(Range&&) -> Polygon<std::remove_cvref_t<std::ranges::range_value_t<Range>>, NoLabel>;

template <class Number>
requires (!detail::is_point_v<Number>)
Polygon(std::initializer_list<Number>) -> Polygon<Point<Number>, NoLabel>;

template <class Number>
requires (!detail::is_point_v<Number>)
Polygon(std::initializer_list<Number>, bool) -> Polygon<Point<Number>, NoLabel>;


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
template <class PointType_, class TLabel>
struct Polygon {
    using PointType = PointType_;
    using NumberType = PointType::NumberType;
    using LabelType = TLabel;
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
    template<PointConcept OtherPointType, class OtherLabelType>
        requires(std::constructible_from<PointType, const OtherPointType&>)
    constexpr Polygon(const Polygon<OtherPointType, OtherLabelType>& other)
        : points_(other.begin(), other.end()), label_(detail::copyLabel<LabelType>(other)) {}

    /**
     * @brief Returns the polygon label.
     *
     * The label is mutable even through a const polygon: it is metadata that
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
     * @brief Checks whether the polygon covers exactly one point.
     *
     * Complexity: O(n), returning at the first differing vertex.
     *
     * @return `true` if the polygon has at least one vertex and all are equal.
     */
    [[nodiscard]] constexpr bool isPoint() const {
        return detail::allPointsEqual(points_);
    }

    /**
     * @brief Returns the point the polygon collapses to, if it does.
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
     * @brief Checks whether the polygon covers exactly one segment of positive
     * length.
     *
     * True when the vertices are collinear but not all equal. The boundary is a
     * closed walk, so collinear vertices make it cover the single segment
     * spanning them.
     *
     * Complexity: O(n), returning at the first non-collinear vertex.
     */
    [[nodiscard]] constexpr bool isSegment() const {
        return detail::pointsSpanSegment(points_);
    }

    /**
     * @brief Returns the segment the polygon collapses to, if it does.
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
     * @brief Checks whether the polygon is degenerate without covering a point
     * or a segment.
     *
     * Zero area does not imply collinear vertices: a self-overlapping boundary
     * whose signed area cancels out (or one that retraces a non-straight path)
     * is degenerate yet covers more than a segment. Such a polygon, and the
     * empty one, are the undefined cases.
     *
     * Complexity: O(n).
     */
    [[nodiscard]] constexpr bool isUndefined() const {
        // Ordered so the cheap point/segment scans reject the common cases
        // before paying for the full area sum.
        return !isPoint() && !isSegment() && isDegenerate();
    }

    /**
     * @brief Tests whether the polygon is simple (its boundary does not
     *        touch or cross itself).
     *
     * Uses a brute-force pairwise edge test in O(n^2) for few vertices (n <= 8)
     * or floating-point coordinates, and the Bentley-Ottmann sweep (O(n log n))
     * for larger exact (integer or rational) polygons. A polygon with fewer than
     * three vertices, or a zero-length edge (a repeated consecutive vertex), is
     * not simple.
     *
     * @tparam Rational Exact rational type used by the sweep for large polygons.
     * @return `true` if no two edges meet except adjacent edges at their shared vertex.
     */
    template <class Rational = pgl::Rational<pgl::BigInt>>
    [[nodiscard]] bool isSimple() const;

    /**
     * @brief Tests whether the polygon is convex.
     *
     * True when every turn along the boundary has the same orientation, i.e.
     * there is no reflex vertex (collinear vertices are permitted). The answer
     * is only meaningful for a simple polygon (@ref isSimple); as elsewhere in
     * the library, a self-intersecting boundary is outside the contract. A
     * polygon with fewer than three vertices is reported as non-convex.
     *
     * Complexity: O(n).
     *
     * @return `true` if the polygon is convex.
     */
    [[nodiscard]] constexpr bool isConvex() const {
        const std::ptrdiff_t n = static_cast<std::ptrdiff_t>(size());
        if (n < 3) {
            return false;
        }
        bool sawPositive = false;
        bool sawNegative = false;
        for (std::ptrdiff_t i = 0; i < n; ++i) {
            const auto turn = orientationSign(get(i), get(i + 1), get(i + 2));
            if (turn > 0) {
                sawPositive = true;
            } else if (turn < 0) {
                sawNegative = true;
            }
            if (sawPositive && sawNegative) {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief Returns the kernel: the set of points that see the whole polygon.
     *
     * A point `p` of the polygon belongs to the kernel when the segment `pq`
     * stays inside the polygon for every point `q` of it. For a simple polygon
     * the kernel is exactly the intersection of the closed half-planes bounded
     * by the edge lines and lying on the interior side, hence convex — that
     * intersection is what is returned.
     *
     * The answer is only meaningful for a simple polygon (@ref isSimple); as
     * elsewhere in the library, a self-intersecting boundary is outside the
     * contract. Degenerate polygons are handled: one collapsed to a point or a
     * segment is its own kernel. An undefined polygon (@ref isUndefined, which
     * includes the vertexless one) yields `std::nullopt`.
     *
     * Complexity: O(n log n) for n vertices.
     *
     * @return The kernel, or `std::nullopt` when it is empty (the polygon is
     *         not star-shaped).
     */
    [[nodiscard]] constexpr std::optional<HalfplaneIntersection<PointType>> getStarShapedKernel() const;

    /**
     * @brief Tests whether the polygon is star-shaped.
     *
     * True when some point of the polygon sees all of it, i.e. when the kernel
     * (@ref getStarShapedKernel) is non-empty. Every convex polygon is
     * star-shaped; the converse does not hold.
     *
     * Complexity: O(n log n) for n vertices.
     */
    [[nodiscard]] constexpr bool isStarShaped() const {
        return getStarShapedKernel().has_value();
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
     * @brief Computes the bounding box of the polygon.
     *
     * Unlike @ref Convex::bbox, a simple polygon has no monotone boundary
     * structure to exploit, so the corners come from a linear scan. The result
     * is computed on the first call and cached in @ref bbox_; later calls return
     * the stored value. Any operation that modifies the polygon resets the cache.
     *
     * Complexity: O(n) for n vertices on the first call, O(1) thereafter.
     *
     * @return A constant reference to the rectangle bounding the polygon.
     */
    constexpr const Rectangle<PointType>& bbox() const;

    /**
     * @brief Computes the floating-point bounding box of the polygon.
     * @tparam ResultNumber The floating-point type for the result.
     * @return A rectangle with floating-point coordinates representing the bounding box.
     */
    template <std::floating_point ResultNumber = double>
    constexpr Rectangle<Point<ResultNumber>> fbox() const;

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
     * @brief Returns a lazy view over the edges, materializing each @ref
     * Segment on the fly instead of allocating a vector.
     *
     * Same edge sequence as @ref edges() (including the closing edge back to
     * vertex 0) but with no heap allocation, so it is preferable when the
     * edges are only iterated once — e.g. inside predicate loops.
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
     * @warning Uses division by 3 and the area, so the result may be inexact even for floating-point types.
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
     * @warning Uses division by the number of vertices, so the result may be inexact even for floating-point types.
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
     * @brief Returns a point strictly inside the (simple) polygon.
     *
     * Works for non-convex polygons. The lexicographically smallest vertex
     * `p0` (stored first in canonical form) is convex, so the triangle formed
     * by `p0` and its two boundary neighbours `a`, `b` lies locally inside the
     * polygon. If no other vertex falls inside that triangle it is an ear and
     * its interior point is returned; otherwise the lexicographically smallest
     * vertex `q` inside the triangle yields a valid diagonal `p0 q`, and its
     * midpoint is returned. Only meaningful for a simple polygon.
     *
     * Complexity: O(n).
     *
     * @tparam ResultNumber The number type for the result.
     * @return A point guaranteed to be inside the polygon.
     * @warning The ear branch calls @ref Triangle::pointInside and so divides
     *          coordinates by 4; the diagonal branch divides by 2. Inexact for
     *          integer coordinates not divisible by that factor.
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
     * @brief Builds the constrained Delaunay triangulation of this polygon.
     *
     * Equivalent to `Triangulation(*this)`. The polygon must be simple
     * (non-self-intersecting) and non-degenerate.
     *
     * @return A @ref Triangulation whose in-domain triangles cover the polygon.
     */
    auto triangulation() const;

    /**
     * @brief Builds the constrained Delaunay triangulation of this polygon with
     *        the given interior constraint segments.
     *
     * Equivalent to `Triangulation(*this, segments)`. The polygon must be
     * simple (non-self-intersecting) and non-degenerate, and the segments are
     * assumed to lie inside it (not checked).
     *
     * @return A @ref Triangulation whose in-domain triangles cover the polygon,
     *         with every segment present as a constrained edge.
     */
    template <class SegmentRange>
    auto triangulation(const SegmentRange& segments) const;

    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     *
     * Uses an exact winding-number test, preceded by an explicit boundary
     * check so the closed boundary counts as contained.
     *
     * Complexity: O(n) for n vertices.
     */
    template<PointConcept OtherPoint>
    constexpr bool contains(const OtherPoint& point) const;

    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     *
     * The segment is split at its boundary intersections and each piece is
     * classified by its midpoint, so the test is correct for non-convex
     * polygons (both endpoints inside does not suffice).
     *
     * Complexity: O(n log n) for n vertices.
     */
    template<SegmentConcept OtherSegment>
    constexpr bool contains(const OtherSegment& other) const;

    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     *
     * Complexity: O(n log n) for n vertices.
     */
    template<OrientedSegmentConcept OtherOrientedSegment>
    constexpr bool contains(const OtherOrientedSegment& other) const;

    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     */
    template<LineConcept OtherLine>
    constexpr bool contains(const OtherLine& other) const;

    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     */
    template<OrientedLineConcept OtherOrientedLine>
    constexpr bool contains(const OtherOrientedLine& other) const;

    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     */
    template<RayConcept OtherRay>
    constexpr bool contains(const OtherRay& other) const;

    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     */
    template<HalfplaneConcept OtherHalfplane>
    constexpr bool contains(const OtherHalfplane& other) const;

    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     *
     * Complexity: O(n log n) for n vertices.
     */
    template<RectangleConcept OtherRectangle>
    constexpr bool contains(const OtherRectangle& other) const;

    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     *
     * Complexity: O(n log n) for n vertices.
     */
    template<TriangleConcept OtherTriangle>
    constexpr bool contains(const OtherTriangle& other) const;

    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     *
     * Complexity: O((n + m) log n) for n and m vertices.
     */
    template<ConvexConcept OtherConvex>
    constexpr bool contains(const OtherConvex& other) const;

    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     *
     * For simple polygons (no holes) this holds iff every edge of @p other is
     * contained, which is what this checks.
     *
     * Complexity: O((n + m) log n) for n and m vertices.
     */
    template<PolygonConcept OtherPolygon>
    constexpr bool contains(const OtherPolygon& other) const;

    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     *
     * A non-degenerate closed disk lies in the closed polygon iff its center is
     * contained and no boundary edge cuts into the open disk (so the disk cannot
     * poke out through a reflex notch). A degenerate disk reduces to a segment.
     *
     * Complexity: O(n) for n vertices.
     */
    template<DiskConcept OtherDisk>
    constexpr bool contains(const OtherDisk& other) const;

    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     */
    constexpr bool contains(const Shape<PointType>& other) const;

    // The empty set is a subset of every shape, so its containment relations are
    // true; symmetric crossing reaches the empty set through the generic
    // OtherShape fallback declared below.
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

    /**
     * @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B).
     *
     * True iff the point is contained but lies on no edge. A polygon with fewer
     * than three vertices has empty interior, so the result is always false.
     *
     * Complexity: O(n) for n vertices.
     */
    template<PointConcept OtherPoint>
    constexpr bool interiorContains(const OtherPoint& point) const;

    /**
     * @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B).
     *
     * Requires both endpoints strictly inside and no contact with the boundary,
     * so a segment cannot dip out through a reflex notch and return.
     *
     * Complexity: O(n) for n vertices.
     */
    template<SegmentConcept OtherSegment>
    constexpr bool interiorContains(const OtherSegment& other) const;

    /**
     * @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B).
     *
     * Complexity: O(n) for n vertices.
     */
    template<OrientedSegmentConcept OtherOrientedSegment>
    constexpr bool interiorContains(const OtherOrientedSegment& other) const;

    /**
     * @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B).
     */
    template<LineConcept OtherLine>
    constexpr bool interiorContains(const OtherLine& other) const;

    /**
     * @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B).
     */
    template<OrientedLineConcept OtherOrientedLine>
    constexpr bool interiorContains(const OtherOrientedLine& other) const;

    /**
     * @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B).
     */
    template<RayConcept OtherRay>
    constexpr bool interiorContains(const OtherRay& other) const;

    /**
     * @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B).
     */
    template<HalfplaneConcept OtherHalfplane>
    constexpr bool interiorContains(const OtherHalfplane& other) const;

    /**
     * @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B).
     *
     * Complexity: O(n log n) for n vertices.
     */
    template<RectangleConcept OtherRectangle>
    constexpr bool interiorContains(const OtherRectangle& other) const;

    /**
     * @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B).
     *
     * Complexity: O(n log n) for n vertices.
     */
    template<TriangleConcept OtherTriangle>
    constexpr bool interiorContains(const OtherTriangle& other) const;

    /**
     * @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B).
     *
     * Complexity: O((n + m) log n) for n and m vertices.
     */
    template<ConvexConcept OtherConvex>
    constexpr bool interiorContains(const OtherConvex& other) const;

    /**
     * @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B).
     *
     * Like @ref contains(const Polygon&), this reduces to an edge-by-edge check,
     * which is exact for simple polygons (no holes).
     *
     * Complexity: O((n + m) log n) for n and m vertices.
     */
    template<PolygonConcept OtherPolygon>
    constexpr bool interiorContains(const OtherPolygon& other) const;

    /**
     * @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B).
     *
     * Complexity: O(n) for n vertices.
     */
    template<PointConcept OtherPoint>
    constexpr bool boundaryContains(const OtherPoint& point) const;

    /**
     * @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B).
     *
     * True iff the segment lies within a single boundary edge (the simple-polygon
     * model also used by @ref Convex::boundaryContains).
     *
     * Complexity: O(n) for n vertices.
     */
    template<SegmentConcept OtherSegment>
    constexpr bool boundaryContains(const OtherSegment& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<OrientedSegmentConcept OtherOrientedSegment>
    constexpr bool boundaryContains(const OtherOrientedSegment& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<LineConcept OtherLine>
    constexpr bool boundaryContains(const OtherLine& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<OrientedLineConcept OtherOrientedLine>
    constexpr bool boundaryContains(const OtherOrientedLine& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<RayConcept OtherRay>
    constexpr bool boundaryContains(const OtherRay& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<HalfplaneConcept OtherHalfplane>
    constexpr bool boundaryContains(const OtherHalfplane& other) const;

    /**
     * @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B).
     *
     * Complexity: O(n) per edge for n vertices.
     */
    template<RectangleConcept OtherRectangle>
    constexpr bool boundaryContains(const OtherRectangle& other) const;

    /**
     * @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B).
     *
     * Complexity: O(n) per edge for n vertices.
     */
    template<TriangleConcept OtherTriangle>
    constexpr bool boundaryContains(const OtherTriangle& other) const;

    /**
     * @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B).
     */
    template<ConvexConcept OtherConvex>
    constexpr bool boundaryContains(const OtherConvex& other) const;

    /**
     * @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B).
     */
    template<PolygonConcept OtherPolygon>
    constexpr bool boundaryContains(const OtherPolygon& other) const;

    /**
     * @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B).
     */
    template<DiskConcept OtherDisk>
    constexpr bool boundaryContains(const OtherDisk& other) const;

    /**
     * @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B).
     */
    template<PointConcept OtherPoint>
    constexpr bool boundaryContains(const Shape<OtherPoint>& other) const;

    // --- not-yet-implemented predicate pairs (throw); see implementation ---
    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template<DiskConcept OtherDisk>
    [[nodiscard]] constexpr bool interiorContains(const OtherDisk& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool separates(const OtherPoint& other) const;

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

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template<MonotoneChainConcept OtherChain>
    [[nodiscard]] constexpr bool contains(const OtherChain& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<MonotoneChainConcept OtherChain>
    [[nodiscard]] constexpr bool boundaryContains(const OtherChain& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template<MonotoneChainConcept OtherChain>
    [[nodiscard]] constexpr bool interiorContains(const OtherChain& other) const;

    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template<MonotoneChainConcept OtherChain>
    [[nodiscard]] constexpr bool intersects(const OtherChain& other) const;

    /** @brief Tests whether the interiors of the shapes intersect (A° ∩ B° ≠ ∅). */
    template<MonotoneChainConcept OtherChain>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherChain& other) const;

    /**
     * @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected).
     *
     * The chain is an arc whose arc order is its lexicographic vertex order, so
     * removing this polygon cuts the chain exactly when the chain has ordered
     * points a < b < c with b inside the polygon and a, c outside (an edge
     * carrying all three is a separated edge; otherwise a and c straddle a
     * covered vertex or edge).
     */
    template<MonotoneChainConcept OtherChain>
    [[nodiscard]] constexpr bool separates(const OtherChain& other) const;

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). @warning Relies on the not-yet-implemented `separates` and throws. */
    template<MonotoneChainConcept OtherChain>
    [[nodiscard]] constexpr bool crosses(const OtherChain& other) const;

    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, MonotoneChainConcept OtherChain>
    [[nodiscard]] constexpr auto squaredDistance(const OtherChain& other) const;

    /** @copydoc distanceL1(const OtherPoint&) const */
    template <class ResultNumber = NumberType, MonotoneChainConcept OtherChain>
    [[nodiscard]] constexpr auto distanceL1(const OtherChain& other) const;

    /** @copydoc distanceLInf(const OtherPoint&) const */
    template <class ResultNumber = NumberType, MonotoneChainConcept OtherChain>
    [[nodiscard]] constexpr auto distanceLInf(const OtherChain& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template<PolylineConcept OtherPolyline>
    [[nodiscard]] constexpr bool contains(const OtherPolyline& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<PolylineConcept OtherPolyline>
    [[nodiscard]] constexpr bool boundaryContains(const OtherPolyline& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template<PolylineConcept OtherPolyline>
    [[nodiscard]] constexpr bool interiorContains(const OtherPolyline& other) const;

    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template<PolylineConcept OtherPolyline>
    [[nodiscard]] constexpr bool intersects(const OtherPolyline& other) const;

    /** @brief Tests whether the interiors of the shapes intersect (A° ∩ B° ≠ ∅). */
    template<PolylineConcept OtherPolyline>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherPolyline& other) const;

    /**
     * @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected).
     *
     * Set semantics: the polyline's free pieces may reconnect through its own
     * self-intersections, so they are joined geometrically rather than in
     * traversal order (see `detail::separates1DSet`).
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

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template<PolylineConcept OtherPolyline>
    [[nodiscard]] constexpr bool crosses(const OtherPolyline& other) const;

    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, PolylineConcept OtherPolyline>
    [[nodiscard]] constexpr auto squaredDistance(const OtherPolyline& other) const;

    /** @copydoc distanceL1(const OtherPoint&) const */
    template <class ResultNumber = NumberType, PolylineConcept OtherPolyline>
    [[nodiscard]] constexpr auto distanceL1(const OtherPolyline& other) const;

    /** @copydoc distanceLInf(const OtherPoint&) const */
    template <class ResultNumber = NumberType, PolylineConcept OtherPolyline>
    [[nodiscard]] constexpr auto distanceLInf(const OtherPolyline& other) const;


    /**
     * @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅).
     *
     * Complexity: O(n) for n vertices.
     */
    template<PointConcept OtherPoint>
    constexpr bool intersects(const OtherPoint& other) const;

    /**
     * @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅).
     *
     * Complexity: O(n) for n vertices.
     */
    template<SegmentConcept OtherSegment>
    constexpr bool intersects(const OtherSegment& other) const;

    /**
     * @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅).
     *
     * Complexity: O(n) for n vertices.
     */
    template<OrientedSegmentConcept OtherOrientedSegment>
    constexpr bool intersects(const OtherOrientedSegment& other) const;

    /**
     * @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅).
     *
     * Complexity: O(n) for n vertices.
     */
    template<LineConcept OtherLine>
    constexpr bool intersects(const OtherLine& other) const;

    /**
     * @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅).
     *
     * Complexity: O(n) for n vertices.
     */
    template<OrientedLineConcept OtherOrientedLine>
    constexpr bool intersects(const OtherOrientedLine& other) const;

    /**
     * @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅).
     *
     * Complexity: O(n) for n vertices.
     */
    template<RayConcept OtherRay>
    constexpr bool intersects(const OtherRay& other) const;

    /**
     * @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅).
     *
     * Complexity: O(n) for n vertices.
     */
    template<HalfplaneConcept OtherHalfplane>
    constexpr bool intersects(const OtherHalfplane& other) const;

    /**
     * @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅).
     *
     * Complexity: O(n m) for polygons with n and m vertices.
     */
    template<RectangleConcept OtherRectangle>
    constexpr bool intersects(const OtherRectangle& other) const;

    /**
     * @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅).
     *
     * Complexity: O(n m) for polygons with n and m vertices.
     */
    template<TriangleConcept OtherTriangle>
    constexpr bool intersects(const OtherTriangle& other) const;

    /**
     * @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅).
     *
     * Complexity: O(n m) for polygons with n and m vertices.
     */
    template<ConvexConcept OtherConvex>
    constexpr bool intersects(const OtherConvex& other) const;

    /**
     * @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅).
     *
     * Decomposes both boundaries into monotone chains via @ref
     * boundariesIntersect; when the boundaries are disjoint a single
     * point-in-polygon test each way settles containment.
     */
    template<PolygonConcept OtherPolygon>
    constexpr bool intersects(const OtherPolygon& other) const;

    /**
     * @brief Tests whether the two polygon boundaries share at least one point
     * (∂A ∩ ∂B ≠ ∅).
     *
     * Decomposes each boundary into its maximal lexicographically monotone
     * chains — @ref MonotoneChainView spans into one buffer per polygon — and
     * tests them with @ref MonotoneChain's linear merge sweep. The two
     * decompositions are produced in lockstep and every newly produced chain is
     * tested against all already-produced chains of the other polygon, so all
     * computed pairs are covered before the next chain is built and the search
     * stops at the first shared point. This underlies both @ref intersects and
     * @ref interiorsIntersect, which add the interior reasoning on top.
     *
     * @return `true` if the boundaries touch or cross anywhere.
     */
    template<PolygonConcept OtherPolygon>
    [[nodiscard]] constexpr bool boundariesIntersect(const OtherPolygon& other) const;

    /**
     * @brief Tests whether the two polygon boundaries have mononotone chains that strong cross
     *
     * @return `true` if two monotone chains strong cross.
     */
    template<PolygonConcept OtherPolygon>
    [[nodiscard]] constexpr bool boundariesStrongCross(const OtherPolygon& other) const;


    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template<DiskConcept OtherDisk>
    constexpr bool intersects(const OtherDisk& other) const;

    /**
     * @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅).
     *
     * Complexity: O(n) for n vertices.
     */
    template<PointConcept OtherPoint>
    constexpr bool interiorsIntersect(const OtherPoint& other) const;

    /**
     * @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅).
     *
     * Complexity: O(n) for n vertices.
     */
    template<LineConcept OtherLine>
    constexpr bool interiorsIntersect(const OtherLine& other) const;

    /**
     * @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅).
     *
     * Complexity: O(n) for n vertices.
     */
    template<OrientedLineConcept OtherOrientedLine>
    constexpr bool interiorsIntersect(const OtherOrientedLine& other) const;

    /**
     * @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅).
     *
     * Complexity: O(n^2) for n vertices.
     */
    template<SegmentConcept OtherSegment>
    constexpr bool interiorsIntersect(const OtherSegment& other) const;

    /**
     * @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅).
     *
     * Complexity: O(n^2) for n vertices.
     */
    template<OrientedSegmentConcept OtherOrientedSegment>
    constexpr bool interiorsIntersect(const OtherOrientedSegment& other) const;

    /**
     * @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅).
     *
     * Complexity: O(n^2) for n vertices.
     */
    template<RayConcept OtherRay>
    constexpr bool interiorsIntersect(const OtherRay& other) const;

    /**
     * @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅).
     *
     * Complexity: O(n) for n vertices.
     */
    template<HalfplaneConcept OtherHalfplane>
    constexpr bool interiorsIntersect(const OtherHalfplane& other) const;

    /**
     * @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅).
     *
     * Complexity: O(n m) for polygons with n and m vertices.
     */
    template<RectangleConcept OtherRectangle>
    constexpr bool interiorsIntersect(const OtherRectangle& other) const;

    /**
     * @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅).
     *
     * Complexity: O(n m) for polygons with n and m vertices.
     */
    template<TriangleConcept OtherTriangle>
    constexpr bool interiorsIntersect(const OtherTriangle& other) const;

    /**
     * @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅).
     *
     * Complexity: O(n m) for polygons with n and m vertices.
     */
    template<ConvexConcept OtherConvex>
    constexpr bool interiorsIntersect(const OtherConvex& other) const;

    /**
     * @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅).
     *
     * Complexity: O(n m) for polygons with n and m vertices.
     */
    template<PolygonConcept OtherPolygon>
    constexpr bool interiorsIntersect(const OtherPolygon& other) const;

    /** @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅). */
    template<DiskConcept OtherDisk>
    constexpr bool interiorsIntersect(const OtherDisk& other) const;

    /**
     * @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected).
     *
     * True iff some boundary edge cuts transversally through the segment's
     * interior while the segment does not lie on the boundary, so the polygon's
     * body interrupts the segment.
     *
     * Complexity: O(n) for n vertices.
     */
    template<SegmentConcept OtherSegment>
    constexpr bool separates(const OtherSegment& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template<OrientedSegmentConcept OtherOrientedSegment>
    constexpr bool separates(const OtherOrientedSegment& other) const;

    /**
     * @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected).
     *
     * Like the segment overload, but a ray has a single finite end (its
     * source); its far end runs to infinity, always outside the bounded
     * polygon, so only the source can lie inside.
     *
     * Complexity: O(n) for n vertices.
     */
    template<RayConcept OtherRay>
    constexpr bool separates(const OtherRay& other) const;

    /**
     * @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected).
     */
    template<LineConcept OtherLine>
    constexpr bool separates(const OtherLine& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template<OrientedLineConcept OtherOrientedLine>
    constexpr bool separates(const OtherOrientedLine& other) const;

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool crosses(const OtherPoint&) const;

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template<SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool crosses(const OtherSegment& other) const;

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template<OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool crosses(const OtherOrientedSegment& other) const;

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template<RayConcept OtherRay>
    [[nodiscard]] constexpr bool crosses(const OtherRay& other) const;

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template<LineConcept OtherLine>
    [[nodiscard]] constexpr bool crosses(const OtherLine& other) const;

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template<OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool crosses(const OtherOrientedLine& other) const;

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template<HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool crosses(const OtherHalfplane&) const;

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template<RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool crosses(const OtherRectangle&) const;

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template<TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr bool crosses(const OtherTriangle&) const;

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template<ConvexConcept OtherConvex>
    [[nodiscard]] constexpr bool crosses(const OtherConvex&) const;

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template<DiskConcept OtherDisk>
    [[nodiscard]] constexpr bool crosses(const OtherDisk&) const;

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template<PolygonConcept OtherPolygon>
    [[nodiscard]] constexpr bool crosses(const OtherPolygon&) const;

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool crosses(const Shape<OtherPoint>& other) const;

    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool intersects(const Shape<OtherPoint>& other) const;

    /** @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅). */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorsIntersect(const Shape<OtherPoint>& other) const;

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template<typename OtherShape>
        requires (!PointConcept<OtherShape> && detail::shapeRank<OtherShape> > detail::shapeRank<Polygon>)
    [[nodiscard]] constexpr bool crosses(const OtherShape& other) const {
        return other.crosses(*this);
    }

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool crosses(const EmptyShape<EmptyPoint>&) const {
        return false;
    }

    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool intersects(const EmptyShape<EmptyPoint>&) const {
        return false;
    }

    /** @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅). */
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool interiorsIntersect(const EmptyShape<EmptyPoint>&) const {
        return false;
    }

    /**
     * @brief Returns the squared Euclidean distance to the given shape.
     *
     * Zero when the polygon's closed region intersects the other shape;
     * otherwise the smallest squared distance between the two shapes. When they
     * are disjoint the polygon's closest point lies on its boundary, so the
     * result is the minimum over the boundary edges of the edge-to-shape squared
     * distance.
     *
     * Complexity: O(n) edge queries for n vertices, each against the other shape.
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
    template <class ResultNumber = NumberType, PolygonConcept OtherPolygon>
    [[nodiscard]] constexpr auto squaredDistance(const OtherPolygon& other) const;

    /**
     * @brief Returns the squared Euclidean distance to a disk.
     *
     * Zero when the polygon's closed region intersects the disk; otherwise the
     * squared exterior gap. Not templated on a result type: a disk's exterior
     * distance is irrational, so it always returns `double`, mirroring
     * @ref Disk::squaredDistance.
     */
    template <class DiskPointType, class DiskLabel>
    [[nodiscard]] double squaredDistance(const Disk<DiskPointType, DiskLabel>& disk) const;

    /** @brief Returns the Manhattan (L1) distance to the given shape. */
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
    template <class ResultNumber = NumberType, PolygonConcept OtherPolygon>
    [[nodiscard]] constexpr auto distanceL1(const OtherPolygon& other) const;

    /**
     * @brief Returns the Manhattan (L1) distance to the given shape.
     *
     * Distance is symmetric, so this just calls @p other's own `distanceL1`,
     * which visits its wrapped alternative and throws if the pair is
     * unsupported.
     */
    template <class ResultNumber = NumberType, PointConcept OtherPoint>
    [[nodiscard]] constexpr auto distanceL1(const Shape<OtherPoint>& other) const {
        return other.template distanceL1<ResultNumber>(*this);
    }

    /** @brief Returns the Chebyshev (LInf) distance to the given shape. */
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
    template <class ResultNumber = NumberType, PolygonConcept OtherPolygon>
    [[nodiscard]] constexpr auto distanceLInf(const OtherPolygon& other) const;

    /** @copydoc distanceL1(const Shape<OtherPoint>&) const */
    template <class ResultNumber = NumberType, PointConcept OtherPoint>
    [[nodiscard]] constexpr auto distanceLInf(const Shape<OtherPoint>& other) const {
        return other.template distanceLInf<ResultNumber>(*this);
    }

    /**
     * @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint.
     *
     * @tparam ResultNumber Coordinate type of the returned point.
     * @tparam OtherPoint Point type.
     * @param other Point to intersect with.
     * @return The point when contained, otherwise empty.
     */
    template <class ResultNumber = NumberType, PointConcept OtherPoint>
    [[nodiscard]] constexpr std::optional<Point<ResultNumber, typename PointType::LabelType>>
    intersection(const OtherPoint& other) const;

    /**
     * @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint.
     *
     * Unlike @ref Convex::intersection, a simple polygon need not be convex, so
     * the intersection of its closed region with a segment can be several
     * disjoint pieces. The pieces are returned in order along the segment (from
     * its `min()` endpoint to its `max()` endpoint); each piece is either a
     * @ref Point (an isolated boundary touch) or a @ref Segment (a maximal
     * overlap with the closed region). An empty vector means no intersection.
     *
     * The supporting line is split at every boundary crossing and each cell is
     * classified by exact (division-free) ray parity, so the result is correct
     * for reflex polygons where both endpoints may lie inside yet the segment
     * dips out through a notch.
     *
     * Complexity: O(n log n) for n vertices.
     *
     * @tparam ResultNumber The number type for the result.
     * @tparam OtherPoint The point type of the segment.
     * @param other The segment to intersect with.
     * @return The disjoint intersection pieces in order along the segment.
     * @warning Divides coordinates after casting to ResultNumber.
     */
    template <class ResultNumber = NumberType, SegmentConcept OtherSegment>
    [[nodiscard]] constexpr std::vector<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherSegment& other) const;

    /**
     * @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint.
     *
     * Same as the @ref Segment overload, ignoring orientation.
     *
     * Complexity: O(n log n) for n vertices.
     * @warning Divides coordinates after casting to ResultNumber.
     */
    template <class ResultNumber = NumberType, OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr std::vector<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherOrientedSegment& other) const;

    /**
     * @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint.
     *
     * Since a polygon is bounded, the intersection of its closed region with an
     * infinite line is a bounded set of disjoint pieces: each is either a
     * @ref Point (an isolated boundary touch) or a @ref Segment (a maximal
     * chord), returned in order along the line. An empty vector means no
     * intersection.
     *
     * Uses the same exact, division-free ray-parity sweep as
     * @ref intersection(const Segment&), but without clipping to a finite range.
     *
     * Complexity: O(n log n) for n vertices.
     *
     * @tparam ResultNumber The number type for the result.
     * @tparam OtherPoint The point type of the line.
     * @param other The line to intersect with.
     * @return The disjoint intersection pieces in order along the line.
     * @warning Divides coordinates after casting to ResultNumber.
     */
    template <class ResultNumber = NumberType, LineConcept OtherLine>
    [[nodiscard]] constexpr std::vector<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherLine& other) const;

    /**
     * @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint.
     *
     * Same as the @ref Line overload, ignoring orientation.
     *
     * Complexity: O(n log n) for n vertices.
     * @warning Divides coordinates after casting to ResultNumber.
     */
    template <class ResultNumber = NumberType, OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr std::vector<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherOrientedLine& other) const;

    /**
     * @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint.
     *
     * A ray is its supporting line restricted to the half starting at the
     * source, so the result is the disjoint pieces of that half inside the
     * closed polygon: each is either a @ref Point (an isolated boundary touch)
     * or a @ref Segment (a maximal chord), returned in order from the source
     * outward. An empty vector means no intersection.
     *
     * Uses the same exact, division-free ray-parity sweep as
     * @ref intersection(const Line&), clipped to the ray's half-line.
     *
     * Complexity: O(n log n) for n vertices.
     *
     * @tparam ResultNumber The number type for the result.
     * @tparam OtherPoint The point type of the ray.
     * @param other The ray to intersect with.
     * @return The disjoint intersection pieces in order from the source outward.
     * @warning Divides coordinates after casting to ResultNumber.
     */
    template <class ResultNumber = NumberType, RayConcept OtherRay>
    [[nodiscard]] constexpr std::vector<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherRay& other) const;

    /**
     * @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint.
     *
     * The boundary of the intersection region `A ∩ B` is exactly
     * `(∂A ∩ B) ∪ (∂B ∩ A)`, so the method clips every edge of each polygon
     * against the other (via @ref intersection(const Segment&)) and collects the
     * resulting boundary pieces into a deduplicated set of points and segments.
     * The segments are assembled into a graph whose nodes are endpoints; in a
     * non-degenerate configuration every node has degree at most two (asserted),
     * so each connected component is an isolated node, a simple path, or a simple
     * cycle. These become a @ref Point, a @ref Polyline, and a @ref Polygon
     * respectively, returned in no particular order.
     *
     * Complexity: O(n m log(n + m)) for polygons with n and m vertices.
     *
     * @tparam ResultNumber The number type for the result.
     * @tparam OtherPoint The point type of the other polygon.
     * @param other The other polygon to intersect with.
     * @return The intersection components: points, polylines, and polygons.
     * @warning Divides coordinates after casting to ResultNumber.
     */
    template <class ResultNumber = NumberType, PolygonConcept OtherPolygon>
    [[nodiscard]] constexpr std::vector<std::variant<Point<ResultNumber, typename PointType::LabelType>, Polyline<Point<ResultNumber, typename PointType::LabelType>>, Polygon<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherPolygon& other) const;

    /**
     * @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint.
     *
     * Forwards to the @ref intersection(const Polygon&) overload via the convex
     * polygon's @ref Convex::asPolygon representation.
     *
     * @tparam ResultNumber The number type for the result.
     * @tparam OtherConvex The convex polygon type.
     * @param other The convex polygon to intersect with.
     * @return The intersection components: points, polylines, and polygons.
     */
    template <class ResultNumber = NumberType, ConvexConcept OtherConvex>
    [[nodiscard]] constexpr std::vector<std::variant<Point<ResultNumber, typename PointType::LabelType>, Polyline<Point<ResultNumber, typename PointType::LabelType>>, Polygon<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherConvex& other) const;

    /**
     * @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint.
     *
     * Forwards to the @ref intersection(const Convex&) overload via the
     * triangle's @ref Triangle::asConvex representation.
     *
     * @tparam ResultNumber The number type for the result.
     * @tparam OtherTriangle The triangle type.
     * @param other The triangle to intersect with.
     * @return The intersection components: points, polylines, and polygons.
     */
    template <class ResultNumber = NumberType, TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr std::vector<std::variant<Point<ResultNumber, typename PointType::LabelType>, Polyline<Point<ResultNumber, typename PointType::LabelType>>, Polygon<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherTriangle& other) const;

    /**
     * @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint.
     *
     * Forwards to the @ref intersection(const Convex&) overload via the
     * rectangle's @ref Rectangle::asConvex representation.
     *
     * @tparam ResultNumber The number type for the result.
     * @tparam OtherRectangle The rectangle type.
     * @param other The rectangle to intersect with.
     * @return The intersection components: points, polylines, and polygons.
     */
    template <class ResultNumber = NumberType, RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr std::vector<std::variant<Point<ResultNumber, typename PointType::LabelType>, Polyline<Point<ResultNumber, typename PointType::LabelType>>, Polygon<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherRectangle& other) const;

    /** @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint. */
    template <class ResultNumber = NumberType, class EmptyPoint>
    [[nodiscard]] constexpr EmptyShape<EmptyPoint> intersection(const EmptyShape<EmptyPoint>&) const {
        return {};
    }

    /**
     * @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint.
     *
     * The boundary of the intersection region `P ∩ H` is `(∂P ∩ H) ∪ (∂H ∩ P)`,
     * so the method clips every polygon edge to the closed half-plane and clips
     * the half-plane's boundary line to the polygon (via
     * @ref intersection(const Line&)), collecting the pieces into a deduplicated
     * set. They are assembled exactly as in @ref intersection(const Polygon&) --
     * a graph whose nodes have degree at most two (asserted) -- into isolated
     * @ref Point components, @ref Segment components, and @ref Polygon components,
     * returned in no particular order. Unlike the polygon overload the 1D pieces
     * are @ref Segment rather than @ref Polyline, because every 1D part of the
     * intersection lies on the half-plane's straight boundary and so is collinear.
     *
     * Complexity: O(n log n) for n vertices.
     *
     * @tparam ResultNumber The number type for the result.
     * @tparam OtherPoint The point type of the half-plane.
     * @param other The half-plane to intersect with.
     * @return The intersection components: points, segments, and polygons.
     * @warning Divides coordinates after casting to ResultNumber.
     */
    template <class ResultNumber = NumberType, HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr std::vector<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>, Polygon<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherHalfplane& other) const;

    /**
     * @brief Returns the intersection with an open polyline (A ∩ B), a sequence
     * of points and segments.
     *
     * A polyline is 1-dimensional, so the intersection with this polygon's
     * region is a set of points and segments. Polygon owns this pair (it
     * outranks @ref Polyline); the computation is the polyline clipped against
     * the polygon, delegated to @ref Polyline::polygonIntersection. The pieces
     * carry the polyline's label, matching `polyline.intersection(polygon)`.
     *
     * @tparam ResultNumber Number type of the returned coordinates.
     * @tparam OtherPolyline Type of the polyline.
     * @param other Polyline to intersect with.
     * @return Vector of points and segments forming the intersection.
     * @warning Divides coordinates after casting to ResultNumber.
     */
    template <class ResultNumber = NumberType, PolylineConcept OtherPolyline>
    [[nodiscard]] constexpr auto intersection(const OtherPolyline& other) const;

    /**
     * @brief Returns the intersection with a monotone chain (A ∩ B), a sequence
     * of points and segments.
     *
     * Same contract as @ref intersection(const OtherPolyline&) const: Polygon
     * outranks @ref MonotoneChain and owns the pair. The chain is viewed as a
     * @ref Polyline (its lexicographic vertex order is a valid traversal) and
     * clipped against the polygon.
     *
     * @tparam ResultNumber Number type of the returned coordinates.
     * @tparam OtherChain Type of the monotone chain.
     * @param other Monotone chain to intersect with.
     * @return Vector of points and segments forming the intersection.
     * @warning Divides coordinates after casting to ResultNumber.
     */
    template <class ResultNumber = NumberType, MonotoneChainConcept OtherChain>
    [[nodiscard]] constexpr auto intersection(const OtherChain& other) const;

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

    /**
     * @brief Makes the polygon simple in place by uncrossing its boundary.
     *
     * Repeatedly removes self-intersections until the boundary is a simple closed
     * curve. Two kinds of moves are applied:
     *
     * - **Flip (2-opt):** when two non-adjacent edges cross transversally, the
     *   sub-path between them is reversed, turning the crossing pair
     *   `(v_i,v_{i+1}),(v_j,v_{j+1})` into the uncrossed `(v_i,v_j),(v_{i+1},v_{j+1})`.
     *   By the triangle inequality each flip strictly shortens the perimeter, so no
     *   vertex-set polygonalization can repeat.
     * - **Vertex removal:** a transversal flip is impossible when the offending
     *   edges only touch or overlap collinearly (a vertex lying on a non-incident
     *   edge, coincident vertices, or a zero-length edge). One such vertex is
     *   redundant for simplicity and is deleted, which also strictly decreases the
     *   vertex count.
     *
     * Because every move either shortens the perimeter at a fixed vertex count or
     * drops a vertex, the process terminates, and on return the polygon is simple
     * (@ref isSimple). The surviving vertices are a subset of the originals with
     * their positions unchanged, then renormalized to canonical form. A polygon
     * with fewer than three vertices is left untouched.
     *
     * @warning Relies on exact orientation predicates; use an exact coordinate
     * type. Termination is not guaranteed for floating-point coordinates.
     *
     * Complexity: O(n^3) worst case per move for n vertices.
     */
    constexpr void untangle();

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
     * @brief Translates the polygon by the negation of the given point.
     *
     * Complexity: O(1).
     */
    template<PointConcept OtherPoint>
    constexpr Polygon& operator-=(const OtherPoint& translation) {
        translation_ -= translation;
        if (bbox_) {
            *bbox_ -= translation;
        }
        hash_ = hashUnset_;
        return *this;
    }

    /**
     * @brief Scales the polygon by the given scalar.
     *
     * Complexity: O(n) for n vertices. Scaling by a negative factor flips the
     * orientation, so the polygon is renormalized to stay canonical.
     */
    template <class Scalar>
        requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
    constexpr Polygon& operator*=(const Scalar& scalar) {
        for (auto& vertex : points_) {
            vertex *= scalar;
        }
        translation_ *= scalar;
        normalize();
        resetCache();
        return *this;
    }

    /**
     * @brief Divides the polygon by the given scalar.
     *
     * Complexity: O(n) for n vertices.
     */
    template <class Scalar>
        requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
    constexpr Polygon& operator/=(const Scalar& scalar) {
        for (auto& vertex : points_) {
            vertex /= scalar;
        }
        translation_ /= scalar;
        normalize();
        resetCache();
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
    [[no_unique_address]] mutable LabelType label_{};
    PointType translation_{};
    // Lazily computed bounding box, invalidated by resetCache() on every
    // mutation. Empty until first computed.
    mutable std::optional<Rectangle<PointType>> bbox_{};

    // Memoized hash, computed lazily by std::hash<Polygon>. hashUnset_ means "not
    // yet computed"; SIZE_MAX is chosen as the sentinel because it is a rare hash
    // output, and the one true hash that would collide with it is remapped to
    // hashUnset_ - 1 so the sentinel is never stored as a real value. Unlike the
    // bbox, the hash is not translation-invariant, so operator+=/-= reset it.
    static constexpr std::size_t hashUnset_ = pgl::detail::numeric_limits<std::size_t>::max();
    mutable std::size_t hash_ = hashUnset_;
    friend struct std::hash<Polygon>;

    // Drops the memoized caches; call after any operation that mutates the
    // polygon's vertices. A pure translation does not need to drop bbox_ (it
    // shifts in place, see operator+=), but it must still reset hash_, which
    // depends on the absolute vertex positions.
    constexpr void resetCache() const {
        bbox_ = {};
        hash_ = hashUnset_;
    }

    template <bool Oriented>
    constexpr BoundaryType<Oriented> boundaryAt(std::size_t index) const {
        const auto i = static_cast<std::ptrdiff_t>(index);
        return BoundaryType<Oriented>(get(i), get(i + 1));
    }

    /**
     * @brief Smallest squared distance from a boundary edge to a disjoint shape.
     *
     * Used when the polygon does not intersect @p other and its closest point
     * therefore lies on the boundary. Requires the edge segment to support
     * `squaredDistance(OtherShape)` (directly or via the shape's forwarder).
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

    /**
     * @brief Lazily decomposes a simple polygon's boundary into its maximal
     * lexicographically monotone chains, exposed as @ref MonotoneChainView spans.
     *
     * Consecutive boundary vertices are distinct, so every edge runs strictly
     * lex-up or lex-down (never level); the boundary breaks into chains exactly
     * at its lexicographic local extrema — the vertices where that direction
     * reverses. A cyclic boundary cannot run one direction the whole way around,
     * so such a break always exists and the boundary splits into at least two
     * chains (exactly two for a convex polygon).
     *
     * Each chain's vertices, read in ascending lexicographic order, retrace its
     * boundary arc exactly (a lex-monotone arc is weakly x-monotone with no
     * y-backtracking vertical edge), which is @ref MonotoneChain's canonical
     * form. A boundary arc is, however, either descending or wraps the
     * vertex-array seam, so it cannot in general be a span straight into the
     * polygon's own vertices. Instead every run is unrolled — reversed when
     * descending — into one shared buffer reserved up front (its length is
     * `n + #chains <= 2n`, so appends never reallocate), and each chain is a
     * `std::span` into that buffer. The whole decomposition therefore costs a
     * single allocation, not one per chain.
     *
     * Chains are materialized on demand via @ref produceNext so a caller can
     * stop at the first crossing without unrolling the rest. @p Poly is the
     * source polygon type, so this serves both operands of a mixed comparison.
     */
    template <class Poly>
    class BoundaryChains {
      public:
        using PT = typename Poly::PointType;
        using ChainView = MonotoneChainView<PT>;

        explicit BoundaryChains(const Poly& poly) : verts_(poly.vertices()) {
            n_ = verts_.size();
            buffer_.reserve(2 * n_);

            // Edge i (verts_[i] -> verts_[i+1]) ascends lexicographically.
            const auto ascends = [&](std::size_t i) { return verts_[i] < verts_[(i + 1) % n_]; };

            // Anchor at a break vertex (its incoming edge reverses) so no run
            // straddles index 0 ambiguously; record each run's first vertex and
            // direction. The run's last vertex is the next run's first.
            std::size_t start = 0;
            bool broke = false;
            for (std::size_t j = 0; j < n_; ++j) {
                if (ascends((j + n_ - 1) % n_) != ascends(j)) {
                    start = j;
                    broke = true;
                    break;
                }
            }
            if (!broke) {
                // Every edge is level, so all vertices coincide: the boundary
                // is a single point and decomposes into no monotone chain. A
                // polygon with two or more distinct vertices always reverses
                // direction somewhere, so this is the only way to get here.
                return;
            }
            std::size_t i = start;
            do {
                const bool up = ascends(i);
                runs_.push_back({i, up});
                std::size_t k = i;
                while (ascends(k) == up) {
                    k = (k + 1) % n_;
                }
                i = k;
            } while (i != start);
        }

        bool exhausted() const { return produced_ == runs_.size(); }
        const std::vector<ChainView>& produced() const { return chains_; }

        // Unrolls the next run into the shared buffer and returns its view.
        const ChainView& produceNext() {
            const auto [begin, up] = runs_[produced_];
            const std::size_t end = runs_[(produced_ + 1) % runs_.size()].first;  // inclusive
            const std::size_t bufStart = buffer_.size();
            std::size_t idx = begin;
            buffer_.push_back(verts_[idx]);
            while (idx != end) {
                idx = (idx + 1) % n_;
                buffer_.push_back(verts_[idx]);
            }
            const std::size_t len = buffer_.size() - bufStart;
            if (!up) {
                std::reverse(buffer_.begin() + static_cast<std::ptrdiff_t>(bufStart), buffer_.end());
            }
            chains_.emplace_back(std::span<const PT>(buffer_.data() + bufStart, len), /*trusted=*/true);
            ++produced_;
            return chains_.back();
        }

      private:
        std::vector<PT> verts_;                            // translated boundary vertices
        std::vector<PT> buffer_;                            // runs unrolled ascending, contiguous
        std::vector<std::pair<std::size_t, bool>> runs_;   // (first-vertex index, ascending?)
        std::vector<ChainView> chains_;                    // materialized views into buffer_
        std::size_t n_ = 0;
        std::size_t produced_ = 0;
    };

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

template <class PointType, class LabelType, class TranslationNumber, class TranslationLabel>
constexpr auto operator+(const Polygon<PointType, LabelType>& polygon, const Point<TranslationNumber, TranslationLabel>& translation) {
    return translation + polygon;
}

template <class TranslationNumber, class TranslationLabel, class PointType, class LabelType>
constexpr auto operator+(const Point<TranslationNumber, TranslationLabel>& translation, const Polygon<PointType, LabelType>& polygon) {
    using ResultPointType = Point<TranslationNumber, typename PointType::LabelType>;
    Polygon<ResultPointType, LabelType> result(polygon);
    result += translation;
    if constexpr (detail::has_label_v<LabelType>) {
        result.label() = LabelType{};
    }
    return result;
}

template <class PointType, class LabelType, class TranslationNumber, class TranslationLabel>
constexpr auto operator-(const Polygon<PointType, LabelType>& polygon, const Point<TranslationNumber, TranslationLabel>& translation) {
    return polygon + (-translation);
}

template <class PointType, class LabelType, class Scalar>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr auto operator*(const Polygon<PointType, LabelType>& polygon, const Scalar& scalar) {
    using ResultPointType = Point<decltype(std::declval<PointType>().x() * scalar), typename PointType::LabelType>;
    Polygon<ResultPointType, LabelType> result(polygon);
    result *= scalar;
    if constexpr (detail::has_label_v<LabelType>) {
        result.label() = LabelType{};
    }
    return result;
}

template <class Scalar, class PointType, class LabelType>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr auto operator*(const Scalar& scalar, const Polygon<PointType, LabelType>& polygon) {
    return polygon * scalar;
}

template <class PointType, class LabelType, class Scalar>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr auto operator/(const Polygon<PointType, LabelType>& polygon, const Scalar& scalar) {
    using ResultPointType = Point<decltype(std::declval<PointType>().x() / scalar), typename PointType::LabelType>;
    Polygon<ResultPointType, LabelType> result(polygon);
    result /= scalar;
    if constexpr (detail::has_label_v<LabelType>) {
        result.label() = LabelType{};
    }
    return result;
}

template <class PointType, class LabelType>
std::ostream& operator<<(std::ostream& stream, const Polygon<PointType, LabelType>& polygon);

}  // namespace pgl
