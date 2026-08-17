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

    /** @brief Tests whether another shape defines exactly the same point set. */
    template<AnyShapeConcept OtherShape>
    [[nodiscard]] constexpr bool samePointSet(const OtherShape& other) const;

    /**
     * @brief Returns the number of vertices in the polygon.
     */
    constexpr std::size_t size() const {
        return points_.size();
    }

    /**
     * @brief Computes twice the (unsigned) area of the polygon via the shoelace formula.
     *
     * @tparam ResultNumber Type the sum is accumulated in, @ref NumberType by
     *         default. The shoelace terms are products of coordinates, so a
     *         polygon whose area leaves the coordinate range wraps; pass a
     *         wider type to measure such a polygon.
     * @return Twice the area, or zero for fewer than three vertices.
     */
    template <class ResultNumber = NumberType>
    constexpr ResultNumber twiceArea() const {
        if (points_.size() < 3) {
            return ResultNumber(0);
        }
        return pgl::detail::abs(signedTwiceArea<ResultNumber>());
    }

    /**
     * @brief Computes the area of the polygon.
     * @warning Uses division by 2.
     */
    template <class ResultNumber = division_result_t<NumberType>>
    constexpr auto area() const {
        ResultNumber result = static_cast<ResultNumber>(twiceArea());
        return result / ResultNumber(2);
    }

    /**
     * @brief Returns whether the polygon is the empty set of points.
     *
     * A polygon with no vertices covers nothing, which is the state of a
     * default-constructed one and of every polygon-valued result that comes
     * back empty. It behaves as @ref EmptyShape: every predicate reads it as
     * the empty set.
     *
     * Complexity: O(1).
     *
     * @return `true` if the polygon covers no point.
     */
    [[nodiscard]] constexpr bool empty() const {
        return points_.empty();
    }

    /**
     * @brief Checks if the polygon is degenerate (has zero area).
     *
     * The empty polygon has no area either, so it is degenerate.
     *
     * Collinear vertices are what a well-defined polygon without area looks
     * like, and @ref isPoint / @ref isSegment decide that exactly whatever the
     * coordinates. Only @ref isUndefined — a boundary that retraces itself —
     * has no area without being collinear, and it is the one branch that needs
     * the shoelace sum, taken in the promoted type. `twiceArea() == 0` would
     * instead narrow that sum to @ref NumberType, where the area of an ordinary
     * polygon past the coordinate range wraps to zero.
     */
    constexpr bool isDegenerate() const {
        return empty() || isPoint() || isSegment() || hasNoArea();
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
     * is degenerate yet covers more than a segment. Such a polygon is the only
     * undefined case; the empty polygon is the well-defined empty set, so use
     * @ref empty for it.
     *
     * Complexity: O(n).
     */
    [[nodiscard]] constexpr bool isUndefined() const {
        // Ordered so the cheap emptiness and point/segment scans reject the
        // common cases before paying for the full area sum.
        return !empty() && !isPoint() && !isSegment() && hasNoArea();
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
     * segment is its own kernel. The empty polygon (@ref empty) and an
     * undefined one (@ref isUndefined) yield `std::nullopt`.
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
     * @brief Returns the visibility graph of the polygon vertices.
     *
     * Two vertices are adjacent exactly when the closed segment joining them
     * is contained in the closed polygon. Consequently boundary edges, and
     * diagonals that touch or overlap the boundary without leaving the
     * polygon, are visible.
     *
     * The answer is meaningful for a simple polygon. Degenerate polygons are
     * handled as their represented point set: an empty polygon gives an empty
     * graph, while a polygon collapsed to a point or segment connects every
     * pair of distinct contained vertices.
     *
     * Triangulates the polygon and runs one cone-clipped traversal of the mesh
     * per vertex — triangular expansion — whose cost is proportional to the part
     * of the polygon that vertex actually sees. A convex polygon skips the
     * triangulation: every segment between its vertices is inside it, so the
     * answer is the complete graph. See @ref Triangulation::visibilityGraph.
     *
     * Complexity: O(n·t + m) time for n vertices, m visibility edges and t
     * triangles seen per vertex, plus O(m) space for the returned graph.
     *
     * @return An undirected graph whose vertices are this polygon's vertices.
     */
    [[nodiscard]] Graph<PointType> visibilityGraph() const;

    /**
     * @brief Returns the clear visibility graph of the polygon vertices.
     *
     * Two vertices are adjacent exactly when the *open* segment joining them
     * lies in the *interior* of the polygon and contains no other vertex — the
     * strict reading of visibility, which admits neither grazing nor passing
     * through a vertex. The polygon's own sides are therefore absent, their
     * relative interiors lying on the boundary, and what remains is exactly the
     * set of legal triangulation diagonals. Always a subgraph of
     * @ref visibilityGraph.
     *
     * The answer is meaningful for a simple polygon. A degenerate polygon has no
     * interior, so its vertices come back with no edges at all.
     *
     * Complexity: as @ref visibilityGraph, with no convex shortcut.
     *
     * @return An undirected graph whose vertices are this polygon's vertices.
     */
    [[nodiscard]] Graph<PointType> clearVisibilityGraph() const;

    /**
     * @brief Returns the reduced visibility graph of the polygon vertices.
     *
     * The subgraph of @ref visibilityGraph holding the edges a shortest path
     * inside the polygon can use: those tangent to the boundary at both ends. An
     * edge `uv` is tangent at `u` when the two sides meeting at `u` lie in one
     * closed half-plane of the line `uv`, which is what lets a taut path bend
     * there. Every side survives; among the diagonals only the bitangents
     * between reflex corners do, so this is far sparser than
     * @ref visibilityGraph while still holding a geodesic shortest path between
     * any two vertices. A shortest path to or from a point that is not a vertex
     * needs that point's own visibility edges added back.
     *
     * The answer is meaningful for a simple polygon. A degenerate polygon has
     * every vertex collinear with every side, so the tangency test passes
     * everywhere and the result matches @ref visibilityGraph.
     *
     * Complexity: as @ref visibilityGraph, plus O(m) for m visibility edges.
     *
     * @return An undirected graph whose vertices are this polygon's vertices.
     */
    [[nodiscard]] Graph<PointType> reducedVisibilityGraph() const;

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
     * @brief Returns the polygon as a hole-free region.
     *
     * The boundary is already canonical, and a region without holes needs no
     * further normalization.
     *
     * @return PolygonWithHoles whose outer boundary is this polygon and which
     *         has no holes.
     */
    [[nodiscard]] constexpr PolygonWithHoles<PointType> asPolygonWithHoles() const {
        return PolygonWithHoles<PointType>(*this);
    }

    /**
     * @brief Returns the polygon as a one-component set of regions.
     *
     * A polygon with no area covers nothing that survives regularization, so it
     * gives back the empty set rather than a component without area.
     *
     * @return PolygonSet whose only component is this polygon as a region.
     */
    [[nodiscard]] constexpr PolygonSet<PointType> asPolygonSet() const {
        return PolygonSet<PointType>(asPolygonWithHoles());
    }

    /**
     * @brief Computes the area-weighted centroid of the polygon.
     * @tparam ResultNumber The number type for the result.
     * @warning Uses division by 3 and the area, so the result may be inexact even for floating-point types.
     */
    template <class ResultNumber = division_result_t<NumberType>>
    constexpr Point<ResultNumber> centroid() const {
        if (points_.size() < 3) {
            return verticesCentroid<ResultNumber>();
        }
        // The weights and the area they are divided by are summed in the same
        // pass and the same type: taken from signedTwiceArea() the divisor
        // would come back narrowed to NumberType, where an area past the
        // coordinate range wraps and either sends a perfectly ordinary polygon
        // down the no-area path or scales the answer by a wrapped divisor.
        ResultNumber cx = 0;
        ResultNumber cy = 0;
        ResultNumber areaTwice = 0;
        const std::size_t n = points_.size();
        for (std::size_t i = 0; i < n; ++i) {
            const auto& p1 = points_[i];
            const auto& p2 = points_[(i + 1) % n];
            const ResultNumber cross = detail::asNumber<ResultNumber>(p1.x()) * detail::asNumber<ResultNumber>(p2.y())
                                     - detail::asNumber<ResultNumber>(p2.x()) * detail::asNumber<ResultNumber>(p1.y());
            areaTwice += cross;
            cx += (detail::asNumber<ResultNumber>(p1.x()) + detail::asNumber<ResultNumber>(p2.x())) * cross;
            cy += (detail::asNumber<ResultNumber>(p1.y()) + detail::asNumber<ResultNumber>(p2.y())) * cross;
        }
        if (areaTwice == ResultNumber(0)) {
            return verticesCentroid<ResultNumber>();
        }
        const ResultNumber denom = ResultNumber(3) * areaTwice;
        return Point<ResultNumber>(cx / denom, cy / denom) + static_cast<Point<ResultNumber>>(translation_);
    }

    /**
     * @brief Computes the centroid of the vertex set (the average of the vertices).
     * @tparam ResultNumber The number type for the result.
     * @warning Uses division by the number of vertices, so the result may be inexact even for floating-point types.
     */
    template <class ResultNumber = division_result_t<NumberType>>
    constexpr Point<ResultNumber> verticesCentroid() const {
        if (points_.empty()) {
            return Point<ResultNumber>();
        }
        ResultNumber cx = 0;
        ResultNumber cy = 0;
        for (const auto& vertex : points_) {
            cx += detail::asNumber<ResultNumber>(vertex.x());
            cy += detail::asNumber<ResultNumber>(vertex.y());
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
    template <class ResultNumber = division_result_t<NumberType>>
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
     * @brief Cuts this polygon into convex pieces with disjoint interiors.
     *
     * Equivalent to `triangulation().convexPartition()`, and the polygon has the
     * same precondition it does: simple and non-degenerate. The pieces' union is
     * the polygon and their interiors are pairwise disjoint, so anything additive
     * over a decomposition — an area, a sweep, a Minkowski sum — may be summed
     * over them. There are at most four times as many as the fewest possible; see
     * @ref Triangulation::convexPartition for why that is all one should want.
     *
     * A convex polygon comes back as a single piece.
     *
     * @return The convex pieces, in canonical order.
     */
    [[nodiscard]] std::vector<Convex<PointType>> convexPartition() const;

    /**
     * @brief Covers this polygon with convex hulls derived from triangle cliques.
     *
     * The polygon is triangulated and the paper's dual-graph BFS builds a
     * full-visibility subgraph: triangles are graph vertices and every edge
     * certifies that the endpoints' joint convex hull lies in the polygon. A
     * DSATUR vertex clique cover is then converted into convex pieces by taking
     * the hull of every clique. This is the Delaunay-partition variant of the
     * clique-cover construction of Abrahamsen, Meyling, and Nusser (SoCG 2023).
     *
     * The polygon has the same precondition as @ref triangulation: simple and
     * non-degenerate. Every piece is contained in this polygon and their union
     * is the polygon, but unlike @ref convexPartition their interiors may
     * overlap. Redundant pieces are removed; the result is not necessarily a
     * minimum-cardinality cover.
     *
     * A convex polygon comes back as a single piece.
     *
     * Complexity: O(n^3 log n) worst-case time and O(n^2) space for n polygon
     * vertices; the dual search usually avoids most of the quadratic candidate
     * pairs even though the worst-case bound is unchanged.
     *
     * @return The convex covering, in canonical order.
     */
    [[nodiscard]] std::vector<Convex<PointType>> convexCovering() const;

    /**
     * @brief Builds the constrained Delaunay triangulation of this polygon with
     *        the given interior vertices and constraint segments.
     *
     * Equivalent to `Triangulation(*this, points, segments)`. The polygon must
     * be simple (non-self-intersecting) and non-degenerate, and the points and
     * segments are assumed to lie inside it (not checked).
     *
     * @return A @ref Triangulation whose in-domain triangles cover the polygon,
     *         with every point present as a vertex and every segment as a
     *         constrained edge.
     */
    template <class PointRange, class SegmentRange>
    auto triangulation(const PointRange& points, const SegmentRange& segments) const;

    /**
     * @brief Returns the regularized set difference of the two shapes (A ∖ B).
     *
     * The result is `closure(A° ∖ B)`: the part of this polygon with area that
     * survives the removal, as a set of regions with pairwise disjoint
     * interiors whose union is the difference. Lower-dimensional leftovers — a
     * stretch of the boundary that @p other touches without covering, an
     * isolated contact point — are dropped, which is the usual convention for
     * boolean operations on solids and what makes the result a set of regions.
     *
     * This is the construction @ref PolygonWithHoles exists for: removing a
     * polygon from the middle of another one leaves a hole, which no other
     * shape in the library can express. Unlike
     * @ref intersection(const OtherPolygon&) const, which never needs one, this
     * is where the nesting matters.
     *
     * The pieces are not nested: an island of this polygon stranded inside a
     * hole of the result comes back as a region of its own.
     *
     * Complexity: O(m²) for m boundary edges, then a constrained triangulation
     * over the arrangement of both boundaries.
     *
     * @tparam ResultNumber The number type for the result.
     * @param other The shape to remove.
     * @return The pieces of the difference, in canonical order.
     * @note The arrangement is built over exact rationals whatever
     *       @p ResultNumber is, and converted only at the end. So an integral
     *       result type is exact whenever the boundaries cross at integral
     *       points, and truncates only where they genuinely do not.
     */
    template <class ResultNumber = division_result_t<NumberType>, PolygonConcept OtherPolygon>
    [[nodiscard]] PolygonSet<Point<ResultNumber, typename PointType::LabelType>>
    difference(const OtherPolygon& other) const;

    /** @brief Returns the regularized set difference of the two shapes (A ∖ B). */
    template <class ResultNumber = division_result_t<NumberType>, ConvexConcept OtherConvex>
    [[nodiscard]] PolygonSet<Point<ResultNumber, typename PointType::LabelType>>
    difference(const OtherConvex& other) const;

    /** @brief Returns the regularized set difference of the two shapes (A ∖ B). */
    template <class ResultNumber = division_result_t<NumberType>, TriangleConcept OtherTriangle>
    [[nodiscard]] PolygonSet<Point<ResultNumber, typename PointType::LabelType>>
    difference(const OtherTriangle& other) const;

    /** @brief Returns the regularized set difference of the two shapes (A ∖ B). */
    template <class ResultNumber = division_result_t<NumberType>, RectangleConcept OtherRectangle>
    [[nodiscard]] PolygonSet<Point<ResultNumber, typename PointType::LabelType>>
    difference(const OtherRectangle& other) const;

    /** @brief Returns the regularized set difference of the two shapes (A ∖ B). */
    template <class ResultNumber = division_result_t<NumberType>, PolygonWithHolesConcept OtherRegion>
    [[nodiscard]] PolygonSet<Point<ResultNumber, typename PointType::LabelType>>
    difference(const OtherRegion& other) const;

    /**
     * @brief Returns the regularized set difference of the two shapes (A ∖ B).
     *
     * A difference is not symmetric, so this pair stays here rather than going
     * to the higher-ranked set the way @ref regularizedUnion(const OtherSet&) const
     * does. It costs no more for it: the set goes into the one arrangement
     * whole, exactly as it would have on the other side. See
     * @ref difference(const OtherPolygon&) const for the contract.
     */
    template <class ResultNumber = division_result_t<NumberType>, PolygonSetConcept OtherSet>
    [[nodiscard]] PolygonSet<Point<ResultNumber, typename PointType::LabelType>>
    difference(const OtherSet& other) const;

    /**
     * @brief Returns the regularized set difference of the two shapes (A ∖ B).
     *
     * A half-plane intersection may be unbounded, which stops it being a
     * @ref regularizedUnion operand but not a subtrahend: `A ∖ B` is bounded whenever
     * `A` is, however far `B` reaches, so a `PolygonSet` can hold it. See
     * @ref PolygonWithHoles::difference(const OtherIntersection&) const for the
     * clip that bounds it and for the rest of the contract.
     */
    template <class ResultNumber = division_result_t<NumberType>, HalfplaneIntersectionConcept OtherIntersection>
    [[nodiscard]] PolygonSet<Point<ResultNumber, typename PointType::LabelType>>
    difference(const OtherIntersection& other) const;

    /**
     * @brief Returns the regularized set difference of the two shapes (A ∖ B).
     *
     * A half-plane is the one-constraint half-plane intersection, and is handled
     * as one: see @ref difference(const OtherIntersection&) const.
     */
    template <class ResultNumber = division_result_t<NumberType>, HalfplaneConcept OtherHalfplane>
    [[nodiscard]] PolygonSet<Point<ResultNumber, typename PointType::LabelType>>
    difference(const OtherHalfplane& other) const;

    /**
     * @brief Returns the regularized union of the two shapes (A ∪ B).
     *
     * The result is `closure(A° ∪ B°)`, as a set of regions with pairwise
     * disjoint interiors. It needs @ref PolygonWithHoles for the same reason the
     * difference does: two shapes that wrap round between them enclose a hole
     * neither of them has, as a `U` united with the bar that caps it.
     *
     * Operands meeting only along a stretch of boundary or at a single point
     * fuse only where they have area in common to fuse through — an isolated
     * contact point comes back as two pieces meeting there, since a region may
     * not have a self-touching outer ring. Disjoint operands come back as two
     * pieces.
     *
     * Complexity: O(m²) for m boundary edges, then a constrained triangulation
     * over the arrangement of both boundaries.
     *
     * @tparam ResultNumber The number type for the result.
     * @param other The shape to unite with.
     * @return The pieces of the union, in canonical order.
     * @note The arrangement is built over exact rationals whatever
     *       @p ResultNumber is, and converted only at the end. So an integral
     *       result type is exact whenever the boundaries cross at integral
     *       points, and truncates only where they genuinely do not.
     */
    template <class ResultNumber = division_result_t<NumberType>, PolygonConcept OtherPolygon>
    [[nodiscard]] PolygonSet<Point<ResultNumber, typename PointType::LabelType>>
    regularizedUnion(const OtherPolygon& other) const;

    /** @brief Returns the regularized union of the two shapes (A ∪ B). */
    template <class ResultNumber = division_result_t<NumberType>, ConvexConcept OtherConvex>
    [[nodiscard]] PolygonSet<Point<ResultNumber, typename PointType::LabelType>>
    regularizedUnion(const OtherConvex& other) const;

    /** @brief Returns the regularized union of the two shapes (A ∪ B). */
    template <class ResultNumber = division_result_t<NumberType>, TriangleConcept OtherTriangle>
    [[nodiscard]] PolygonSet<Point<ResultNumber, typename PointType::LabelType>>
    regularizedUnion(const OtherTriangle& other) const;

    /** @brief Returns the regularized union of the two shapes (A ∪ B). */
    template <class ResultNumber = division_result_t<NumberType>, RectangleConcept OtherRectangle>
    [[nodiscard]] PolygonSet<Point<ResultNumber, typename PointType::LabelType>>
    regularizedUnion(const OtherRectangle& other) const;

    /** @brief Returns the regularized union of the two shapes (A ∪ B). */
    template <class ResultNumber = division_result_t<NumberType>, PolygonWithHolesConcept OtherRegion>
    [[nodiscard]] PolygonSet<Point<ResultNumber, typename PointType::LabelType>>
    regularizedUnion(const OtherRegion& other) const;

    /**
     * @brief Returns the regularized union of the two shapes (A ∪ B).
     *
     * A set of regions is the one @ref PolygonalRegionConcept operand ranked
     * above a polygon, and it states its operations over every operand at once,
     * so this hands the pair back to it rather than restating it. A union is
     * symmetric, so the order costs nothing — and going through the set is what
     * puts every component into one arrangement instead of folding the polygon
     * over them one at a time. See @ref regularizedUnion(const OtherPolygon&) const for
     * the contract.
     */
    template <class ResultNumber = division_result_t<NumberType>, PolygonSetConcept OtherSet>
    [[nodiscard]] auto regularizedUnion(const OtherSet& other) const {
        return other.template regularizedUnion<ResultNumber>(*this);
    }

    /**
     * @brief Returns the regularized symmetric difference of the two shapes
     *        (A △ B).
     *
     * The result is `closure((A° ∖ B) ∪ (B° ∖ A))`, as a set of regions with
     * pairwise disjoint interiors: the part covered by exactly one of the two
     * operands. It is the union of the two differences, and inherits holes from
     * both.
     *
     * Complexity: O(m²) for m boundary edges, then a constrained triangulation
     * over the arrangement of both boundaries.
     *
     * @tparam ResultNumber The number type for the result.
     * @param other The other shape.
     * @return The pieces of the symmetric difference, in canonical order.
     * @note The arrangement is built over exact rationals whatever
     *       @p ResultNumber is, and converted only at the end.
     */
    template <class ResultNumber = division_result_t<NumberType>, PolygonConcept OtherPolygon>
    [[nodiscard]] PolygonSet<Point<ResultNumber, typename PointType::LabelType>>
    symmetricDifference(const OtherPolygon& other) const;

    /** @brief Returns the regularized symmetric difference of the two shapes (A △ B). */
    template <class ResultNumber = division_result_t<NumberType>, ConvexConcept OtherConvex>
    [[nodiscard]] PolygonSet<Point<ResultNumber, typename PointType::LabelType>>
    symmetricDifference(const OtherConvex& other) const;

    /** @brief Returns the regularized symmetric difference of the two shapes (A △ B). */
    template <class ResultNumber = division_result_t<NumberType>, TriangleConcept OtherTriangle>
    [[nodiscard]] PolygonSet<Point<ResultNumber, typename PointType::LabelType>>
    symmetricDifference(const OtherTriangle& other) const;

    /** @brief Returns the regularized symmetric difference of the two shapes (A △ B). */
    template <class ResultNumber = division_result_t<NumberType>, RectangleConcept OtherRectangle>
    [[nodiscard]] PolygonSet<Point<ResultNumber, typename PointType::LabelType>>
    symmetricDifference(const OtherRectangle& other) const;

    /** @brief Returns the regularized symmetric difference of the two shapes (A △ B). */
    template <class ResultNumber = division_result_t<NumberType>, PolygonWithHolesConcept OtherRegion>
    [[nodiscard]] PolygonSet<Point<ResultNumber, typename PointType::LabelType>>
    symmetricDifference(const OtherRegion& other) const;

    /**
     * @brief Returns the regularized symmetric difference of the two shapes
     *        (A △ B).
     *
     * A set of regions is the one @ref PolygonalRegionConcept operand ranked
     * above a polygon, and it states its operations over every operand at once,
     * so this hands the pair back to it rather than restating it, exactly as
     * @ref regularizedUnion(const OtherSet&) const does. See
     * @ref symmetricDifference(const OtherPolygon&) const for the contract.
     */
    template <class ResultNumber = division_result_t<NumberType>, PolygonSetConcept OtherSet>
    [[nodiscard]] auto symmetricDifference(const OtherSet& other) const {
        return other.template symmetricDifference<ResultNumber>(*this);
    }

    /**
     * @brief Returns the regularized Minkowski sum of the two shapes (A ⊕ B).
     *
     * The sum is `{p + q : p ∈ A, q ∈ B}`, regularized to `closure((A ⊕ B)°)`
     * and returned as a region with holes. A
     * non-convex operand is what makes that necessary: sliding a shape around
     * the inside of a `U` sweeps out a region whose boundary closes over a hole,
     * and no other shape in the library can say so. This is the gap
     * @ref PolygonWithHoles was proposed to close.
     *
     * **This polygon must be nondegenerate**, and that is what buys the single
     * region: a nondegenerate polygon is the closure of its connected interior,
     * so `A ⊕ B` covers `⋃_{b ∈ B} (A° + b)`, which is connected and open for
     * any connected `B` and whose closure is the sum. A degenerate polygon —
     * one with no area — is not on this contract; the sum can then fall into
     * several pieces, of which one comes back. An empty or wholly flat
     * regularized sum is the empty region.
     *
     * Distinguish this from
     * @ref minkowskiSum(const OtherShape&) const, which sums a *bounded convex*
     * operand and returns a single `Convex` (or a `Rectangle`, or a translation
     * of this polygon by a `Point`). The two never overlap: the pairs that fit
     * in one shape are exactly the pairs @ref MinkowskiSummableConcept accepts,
     * and this overload set takes the rest.
     *
     * Complexity: one convex merge per pair of triangles of the two operands'
     * triangulations, then a constrained triangulation over the arrangement of
     * all of them.
     *
     * @tparam ResultNumber The number type for the result.
     * @param other The shape to sum with.
     * @return The sum, as one region.
     * @pre This polygon is nondegenerate.
     * @note Every vertex of every piece sum is a sum of two input vertices, so
     *       the pieces are exact; only their union can put a vertex at a
     *       crossing, and that arrangement is built over exact rationals and
     *       converted to @p ResultNumber only at the end.
     */
    template <class ResultNumber = division_result_t<NumberType>, PolygonConcept OtherPolygon>
    [[nodiscard]] PolygonWithHoles<Point<ResultNumber, typename PointType::LabelType>>
    minkowskiSum(const OtherPolygon& other) const;

    /** @brief Returns the regularized Minkowski sum of the two shapes (A ⊕ B). */
    template <class ResultNumber = division_result_t<NumberType>, ConvexConcept OtherConvex>
    [[nodiscard]] PolygonWithHoles<Point<ResultNumber, typename PointType::LabelType>>
    minkowskiSum(const OtherConvex& other) const;

    /** @brief Returns the regularized Minkowski sum of the two shapes (A ⊕ B). */
    template <class ResultNumber = division_result_t<NumberType>, TriangleConcept OtherTriangle>
    [[nodiscard]] PolygonWithHoles<Point<ResultNumber, typename PointType::LabelType>>
    minkowskiSum(const OtherTriangle& other) const;

    /** @brief Returns the regularized Minkowski sum of the two shapes (A ⊕ B). */
    template <class ResultNumber = division_result_t<NumberType>, RectangleConcept OtherRectangle>
    [[nodiscard]] PolygonWithHoles<Point<ResultNumber, typename PointType::LabelType>>
    minkowskiSum(const OtherRectangle& other) const;

    /** @brief Returns the regularized Minkowski sum of the two shapes (A ⊕ B). */
    template <class ResultNumber = division_result_t<NumberType>, PolygonWithHolesConcept OtherRegion>
    [[nodiscard]] PolygonWithHoles<Point<ResultNumber, typename PointType::LabelType>>
    minkowskiSum(const OtherRegion& other) const;

    /**
     * @brief Returns the regularized Minkowski sum of the two shapes (A ⊕ B).
     *
     * A `Polyline` summand has no area, but it sweeps this polygon along itself
     * all the same, so the sum is a region like every other one here. This is the
     * mirror of @ref Polyline::minkowskiSum(const OtherPolygon&) const and gives
     * the same single-region answer: which operand is written first never
     * decides which sum answers.
     */
    template <class ResultNumber = division_result_t<NumberType>, PolylineConcept OtherPolyline>
    [[nodiscard]] PolygonWithHoles<Point<ResultNumber, typename PointType::LabelType>>
    minkowskiSum(const OtherPolyline& other) const;

    /**
     * @brief Returns the regularized Minkowski sum of the two shapes (A ⊕ B).
     *
     * A @ref MonotoneChain summand is a polyline that happens to be sorted, and it
     * sums here exactly as one: its monotonicity is what makes
     * @ref MonotoneChain::minkowskiSum(const OtherConvex&) const one polygon, and a
     * non-convex receiver takes that away again — this polygon's own concavity can
     * strand a cavity whatever the chain does. So the chain contributes its edges
     * and the answer is one region, which may have holes.
     */
    template <class ResultNumber = division_result_t<NumberType>, MonotoneChainConcept OtherChain>
    [[nodiscard]] PolygonWithHoles<Point<ResultNumber, typename PointType::LabelType>>
    minkowskiSum(const OtherChain& other) const;

    /**
     * @brief Returns the regularized Minkowski sum of the two shapes (A ⊕ B).
     *
     * The thinnest summand that still needs a region. A segment has no area, but
     * sliding this polygon along one sweeps the band between the polygon and its
     * translate by the segment's vector, and that band closes over a cavity for
     * the same reason a wider summand's does: a `C` whose opening is no wider
     * than the vector is plugged by the sweep of its own arms. The summand is a
     * single convex piece, so this is the cheapest of these sums — one convex
     * merge per triangle of the triangulated domain.
     *
     * Distinguish it from `segment + segment`, which stays a single `Convex`:
     * two bounded convex operands never need a region, and it is the receiver's
     * concavity, not the summand's thinness, that brings one in. As everywhere
     * here the result is regularized, so a summand that has collapsed to a point
     * comes back empty rather than as a flat copy of this polygon.
     */
    template <class ResultNumber = division_result_t<NumberType>, SegmentConcept OtherSegment>
    [[nodiscard]] PolygonWithHoles<Point<ResultNumber, typename PointType::LabelType>>
    minkowskiSum(const OtherSegment& other) const;

    /**
     * @brief Returns the regularized Minkowski sum of the two shapes (A ⊕ B).
     *
     * An orientation is not part of a point set, so this is the sum with the
     * underlying segment, vertex for vertex.
     */
    template <class ResultNumber = division_result_t<NumberType>, OrientedSegmentConcept OtherSegment>
    [[nodiscard]] PolygonWithHoles<Point<ResultNumber, typename PointType::LabelType>>
    minkowskiSum(const OtherSegment& other) const;

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
    template <class ResultNumber = division_result_t<NumberType>, MonotoneChainConcept OtherChain>
    [[nodiscard]] constexpr auto squaredDistance(const OtherChain& other) const;

    /** @copydoc distanceL1(const OtherPoint&) const */
    template <class ResultNumber = division_result_t<NumberType>, MonotoneChainConcept OtherChain>
    [[nodiscard]] constexpr auto distanceL1(const OtherChain& other) const;

    /** @copydoc distanceLInf(const OtherPoint&) const */
    template <class ResultNumber = division_result_t<NumberType>, MonotoneChainConcept OtherChain>
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

    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     *
     * A region is contained exactly when its outer polygon is: the region holds
     * the whole outer ring whatever its holes do, and this shape has a
     * connected complement. See implementation/contains.hpp.
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

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template<PolylineConcept OtherPolyline>
    [[nodiscard]] constexpr bool crosses(const OtherPolyline& other) const;

    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = division_result_t<NumberType>, PolylineConcept OtherPolyline>
    [[nodiscard]] constexpr auto squaredDistance(const OtherPolyline& other) const;

    /** @copydoc distanceL1(const OtherPoint&) const */
    template <class ResultNumber = division_result_t<NumberType>, PolylineConcept OtherPolyline>
    [[nodiscard]] constexpr auto distanceL1(const OtherPolyline& other) const;

    /** @copydoc distanceLInf(const OtherPoint&) const */
    template <class ResultNumber = division_result_t<NumberType>, PolylineConcept OtherPolyline>
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

    /**
     * @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅).
     *
     * Forwards to the other shape's implementation so that each unordered pair
     * needs `intersects` defined only once, on the higher-ranked shape.
     */
    template<typename OtherShape>
        requires (!PointConcept<OtherShape> && detail::shapeRank<OtherShape> > detail::shapeRank<Polygon>)
    [[nodiscard]] constexpr bool intersects(const OtherShape& other) const {
        return other.intersects(*this);
    }

    /**
     * @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅).
     *
     * Forwards to the other shape's implementation so that each unordered pair
     * needs `interiorsIntersect` defined only once, on the higher-ranked shape.
     */
    template<typename OtherShape>
        requires (!PointConcept<OtherShape> && detail::shapeRank<OtherShape> > detail::shapeRank<Polygon>)
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherShape& other) const {
        return other.interiorsIntersect(*this);
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
    template <class ResultNumber = division_result_t<NumberType>, PolygonConcept OtherPolygon>
    [[nodiscard]] constexpr auto squaredDistance(const OtherPolygon& other) const;

    /**
     * @brief Returns the squared Euclidean distance to the given shape.
     *
     * Forwards to the other shape's implementation so that each unordered pair
     * needs `squaredDistance` defined only once, on the higher-ranked shape.
     */
    template <class ResultNumber = division_result_t<NumberType>, typename OtherShape>
        requires ((detail::shapeRank<OtherShape> > detail::shapeRank<Polygon>)
                  && requires(const OtherShape& o, const Polygon& self) {
                         o.template squaredDistance<ResultNumber>(self);
                     })
    [[nodiscard]] constexpr auto squaredDistance(const OtherShape& other) const {
        return other.template squaredDistance<ResultNumber>(*this);
    }

    /**
     * @brief Returns the squared Euclidean distance to a disk.
     *
     * Zero when the polygon's closed region intersects the disk; otherwise the
     * squared exterior gap. Reports in `detail::floating_result_t<ResultNumber>`: the gap to a
     * circle is generally irrational, so a floating-point `ResultNumber` is
     * honoured as asked and any other request falls back to `double`.
     */
    template <class ResultNumber = double, class DiskPointType, class DiskLabel>
    [[nodiscard]] detail::floating_result_t<ResultNumber> squaredDistance(
        const Disk<DiskPointType, DiskLabel>& disk) const;

    /** @brief Returns the Manhattan (L1) distance to the given shape. */
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
    template <class ResultNumber = division_result_t<NumberType>, PolygonConcept OtherPolygon>
    [[nodiscard]] constexpr auto distanceL1(const OtherPolygon& other) const;

    /**
     * @brief Returns the Manhattan (L1) distance to the given shape.
     *
     * Forwards to the other shape's implementation so that each unordered pair
     * needs `distanceL1` defined only once, on the higher-ranked shape.
     */
    template <class ResultNumber = division_result_t<NumberType>, typename OtherShape>
        requires ((detail::shapeRank<OtherShape> > detail::shapeRank<Polygon>)
                  && requires(const OtherShape& o, const Polygon& self) {
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

    /** @brief Re-dispatches a regularized intersection through a runtime shape. */
    template <class ResultNumber = division_result_t<NumberType>, PointConcept OtherPoint>
    [[nodiscard]] auto regularizedIntersection(const Shape<OtherPoint>& other) const {
        return other.template regularizedIntersection<ResultNumber>(*this);
    }

    /**
     * @brief Returns the regularized union of the two shapes (A ∪ B),
     *        re-dispatching through the wrapper's own `regularizedUnion`.
     *
     * A union is symmetric, so this just calls @p other's own `regularizedUnion`, which
     * visits its wrapped alternative and throws if the pair is unsupported —
     * here, whenever @p other turns out to hold anything but a bounded polygonal
     * region. See @ref Polygon::regularizedUnion for the contract.
     *
     * The point type is deduced from @p other so a plain concrete shape cannot
     * reach this overload through an implicit conversion to `Shape`.
     */
    template <class ResultNumber = division_result_t<NumberType>, PointConcept OtherPoint>
    [[nodiscard]] auto regularizedUnion(const Shape<OtherPoint>& other) const {
        return other.template regularizedUnion<ResultNumber>(*this);
    }

    /**
     * @brief Returns the regularized set difference of the two shapes (A ∖ B),
     *        re-dispatching through the wrapper's own `difference`.
     *
     * A difference is not symmetric, so unlike @ref regularizedUnion this cannot be
     * handed to @p other as it stands. It wraps this shape instead and lets the
     * wrapper visit both sides, which throws if the pair is unsupported — here,
     * whenever @p other turns out to hold anything without area, or a `Disk`.
     * An unbounded alternative is fine on this side, the result being contained
     * in this shape either way. See @ref difference(const OtherPolygon&) const for the contract.
     *
     * The point type is deduced from @p other so a plain concrete shape cannot
     * reach this overload through an implicit conversion to `Shape`.
     */
    template <class ResultNumber = division_result_t<NumberType>, PointConcept OtherPoint>
    [[nodiscard]] auto difference(const Shape<OtherPoint>& other) const {
        return Shape<OtherPoint>(*this).template difference<ResultNumber>(other);
    }

    /**
     * @brief Returns the regularized symmetric difference of the two shapes
     *        (A △ B), re-dispatching through the wrapper's own
     *        `symmetricDifference`.
     *
     * A symmetric difference is symmetric, so this just calls @p other's own,
     * which visits its wrapped alternative and throws if the pair is
     * unsupported. See @ref symmetricDifference(const OtherPolygon&) const for
     * the contract.
     *
     * The point type is deduced from @p other so a plain concrete shape cannot
     * reach this overload through an implicit conversion to `Shape`.
     */
    template <class ResultNumber = division_result_t<NumberType>, PointConcept OtherPoint>
    [[nodiscard]] auto symmetricDifference(const Shape<OtherPoint>& other) const {
        return other.template symmetricDifference<ResultNumber>(*this);
    }

    /**
     * @brief Returns the Manhattan (L1) distance to the given shape.
     *
     * Distance is symmetric, so this just calls @p other's own `distanceL1`,
     * which visits its wrapped alternative and throws if the pair is
     * unsupported.
     */
    template <class ResultNumber = double, PointConcept OtherPoint>
    [[nodiscard]] constexpr auto distanceL1(const Shape<OtherPoint>& other) const {
        return other.template distanceL1<ResultNumber>(*this);
    }

    /** @brief Returns the Chebyshev (LInf) distance to the given shape. */
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
    template <class ResultNumber = division_result_t<NumberType>, PolygonConcept OtherPolygon>
    [[nodiscard]] constexpr auto distanceLInf(const OtherPolygon& other) const;

    /**
     * @brief Returns the Chebyshev (LInf) distance to the given shape.
     *
     * Forwards to the other shape's implementation so that each unordered pair
     * needs `distanceLInf` defined only once, on the higher-ranked shape.
     */
    template <class ResultNumber = division_result_t<NumberType>, typename OtherShape>
        requires ((detail::shapeRank<OtherShape> > detail::shapeRank<Polygon>)
                  && requires(const OtherShape& o, const Polygon& self) {
                         o.template distanceLInf<ResultNumber>(self);
                     })
    [[nodiscard]] constexpr auto distanceLInf(const OtherShape& other) const {
        return other.template distanceLInf<ResultNumber>(*this);
    }

    /** @copydoc distanceL1(const Shape<OtherPoint>&) const */
    template <class ResultNumber = double, PointConcept OtherPoint>
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
    template <class ResultNumber = division_result_t<NumberType>, SegmentConcept OtherSegment>
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
    template <class ResultNumber = division_result_t<NumberType>, OrientedSegmentConcept OtherOrientedSegment>
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
    template <class ResultNumber = division_result_t<NumberType>, LineConcept OtherLine>
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
    template <class ResultNumber = division_result_t<NumberType>, OrientedLineConcept OtherOrientedLine>
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
    template <class ResultNumber = division_result_t<NumberType>, RayConcept OtherRay>
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
    template <class ResultNumber = division_result_t<NumberType>, PolygonConcept OtherPolygon>
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
    template <class ResultNumber = division_result_t<NumberType>, ConvexConcept OtherConvex>
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
    template <class ResultNumber = division_result_t<NumberType>, TriangleConcept OtherTriangle>
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
    template <class ResultNumber = division_result_t<NumberType>, RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr std::vector<std::variant<Point<ResultNumber, typename PointType::LabelType>, Polyline<Point<ResultNumber, typename PointType::LabelType>>, Polygon<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherRectangle& other) const;

    /**
     * @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint.
     *
     * Forwards to the other shape's implementation so that each unordered pair
     * needs `intersection` defined only once, on the higher-ranked shape. The
     * result is then the higher-ranked shape's: intersecting with a
     * @ref PolygonWithHoles gives regions, not the component vector the
     * polygon-polygon overload returns.
     */
    template <class ResultNumber = division_result_t<NumberType>, typename OtherShape>
        requires (!PointConcept<OtherShape>
                  && (detail::shapeRank<OtherShape> > detail::shapeRank<Polygon>)
                  && requires(const OtherShape& o, const Polygon& self) {
                         o.template intersection<ResultNumber>(self);
                     })
    [[nodiscard]] auto intersection(const OtherShape& other) const {
        return other.template intersection<ResultNumber>(*this);
    }

    /** @brief Forwards a regularized intersection to the shape that owns it. */
    template <class ResultNumber = division_result_t<NumberType>, typename OtherShape>
        requires (!PointConcept<OtherShape>
                  && (detail::shapeRank<OtherShape> > detail::shapeRank<Polygon>)
                  && requires(const OtherShape& o, const Polygon& self) {
                         o.template regularizedIntersection<ResultNumber>(self);
                     })
    [[nodiscard]] auto regularizedIntersection(const OtherShape& other) const {
        return other.template regularizedIntersection<ResultNumber>(*this);
    }

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
    template <class ResultNumber = division_result_t<NumberType>, HalfplaneConcept OtherHalfplane>
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
    template <class ResultNumber = division_result_t<NumberType>, PolylineConcept OtherPolyline>
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
    template <class ResultNumber = division_result_t<NumberType>, MonotoneChainConcept OtherChain>
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
        requires MinkowskiSummableConcept<Polygon<PointType_, TLabel>, OtherShape>
    [[nodiscard]] constexpr auto minkowskiSum(const OtherShape& other) const;

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
        if (!bbox_.empty()) {
            bbox_ += translation;
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
        if (!bbox_.empty()) {
            bbox_ -= translation;
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
    // mutation. The empty rectangle doubles as "not computed yet": a shape
    // whose box is genuinely empty has no vertices, so bbox() re-derives it
    // with one size check rather than any real work.
    mutable Rectangle<PointType> bbox_{};

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
     *
     * @tparam ResultNumber Type the terms are formed and accumulated in. The
     *         terms are products of coordinates, so they leave @ref NumberType
     *         long before the coordinates do; pass a promoted type wherever the
     *         sum has to be trusted rather than merely reported.
     */
    template <class ResultNumber = NumberType>
    constexpr ResultNumber signedTwiceArea() const {
        ResultNumber sum = 0;
        const std::size_t n = points_.size();
        for (std::size_t i = 0; i < n; ++i) {
            const auto& p1 = points_[i];
            const auto& p2 = points_[(i + 1) % n];
            sum += detail::asNumber<ResultNumber>(p1.x()) * detail::asNumber<ResultNumber>(p2.y())
                 - detail::asNumber<ResultNumber>(p2.x()) * detail::asNumber<ResultNumber>(p1.y());
        }
        return sum;
    }

    /**
     * @brief Whether the shoelace sum vanishes, taken in the promoted type.
     *
     * The zero-area test the degeneracy predicates need. @ref twiceArea answers
     * the same question in @ref NumberType, where an area past the coordinate
     * range wraps to zero and reports an ordinary polygon as having none.
     */
    constexpr bool hasNoArea() const {
        using Exact = detail::promoted_number_t<NumberType>;
        return signedTwiceArea<Exact>() == Exact(0);
    }

    /**
     * @brief Whether the boundary runs clockwise, decided at vertex @p pivot.
     *
     * @p pivot must be a lexicographically smallest vertex. On a simple polygon
     * such a vertex is convex — no other vertex lies in the half-plane its two
     * boundary neighbours are turned away from — so the turn it makes with them
     * is the winding of the whole boundary. Vertices equal to the pivot are
     * stepped over so a repeated vertex cannot hide that turn behind a
     * zero-length edge.
     */
    constexpr bool windsClockwise(std::size_t pivot) const {
        const std::size_t n = points_.size();
        const PointType& vertex = points_[pivot];
        std::size_t before = (pivot + n - 1) % n;
        while (before != pivot && points_[before] == vertex) {
            before = (before + n - 1) % n;
        }
        std::size_t after = (pivot + 1) % n;
        while (after != pivot && points_[after] == vertex) {
            after = (after + 1) % n;
        }
        return orientationSign(points_[before], vertex, points_[after]) < 0;
    }

    /**
     * @brief Brings the stored vertices to canonical form: counterclockwise,
     * with the lexicographically smallest vertex first.
     *
     * The winding comes from a single orientation sign at the lexicographically
     * smallest vertex (see @ref windsClockwise), not from the shoelace sum: the
     * sum is O(n) and lives in @ref NumberType, so an area past the coordinate
     * range wraps and leaves the polygon uncanonicalized — and with it `==`,
     * ordering, hashing and every canonical comparison broken on equal point
     * sets. The orientation sign is O(1) and stays in the promoted type.
     */
    constexpr void normalize() {
        if (points_.empty()) {
            return;
        }
        auto minIt = std::min_element(points_.begin(), points_.end());
        if (points_.size() >= 3 &&
            windsClockwise(static_cast<std::size_t>(minIt - points_.begin()))) {
            // Reversing relocates the smallest vertex, so it is found again
            // rather than assumed to still sit where it did.
            std::reverse(points_.begin(), points_.end());
            minIt = std::min_element(points_.begin(), points_.end());
        }
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
