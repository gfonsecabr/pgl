#pragma once

#include "shape/halfplaneintersection.hpp"

#include <algorithm>
#include <cassert>
#include <compare>
#include <concepts>
#include <cstddef>
#include <optional>
#include <ostream>
#include <ranges>
#include <type_traits>
#include <utility>
#include <vector>


namespace pgl {

template <class PointType = Point<>, class Label>
struct PolygonWithHoles;

PolygonWithHoles() -> PolygonWithHoles<Point<>, NoLabel>;

template <PolygonConcept OuterPolygon>
PolygonWithHoles(OuterPolygon&&) -> PolygonWithHoles<typename std::remove_cvref_t<OuterPolygon>::PointType, NoLabel>;

template <PolygonConcept OuterPolygon>
PolygonWithHoles(OuterPolygon&&, bool) -> PolygonWithHoles<typename std::remove_cvref_t<OuterPolygon>::PointType, NoLabel>;

template <PolygonConcept OuterPolygon, std::ranges::input_range HoleRange>
    requires detail::is_polygon_v<std::ranges::range_value_t<HoleRange>>
PolygonWithHoles(OuterPolygon&&, HoleRange&&) -> PolygonWithHoles<typename std::remove_cvref_t<OuterPolygon>::PointType, NoLabel>;

template <PolygonConcept OuterPolygon, std::ranges::input_range HoleRange>
    requires detail::is_polygon_v<std::ranges::range_value_t<HoleRange>>
PolygonWithHoles(OuterPolygon&&, HoleRange&&, bool) -> PolygonWithHoles<typename std::remove_cvref_t<OuterPolygon>::PointType, NoLabel>;


/**
 * @brief A closed region bounded by one outer simple polygon minus a set of
 *        disjoint polygonal holes.
 *
 * The region is exactly
 *
 * ```
 * A = outer \ (hole_0° ∪ hole_1° ∪ ...)
 * ```
 *
 * — the outer polygon minus the **interiors** of the holes, so every hole
 * boundary belongs to the region and `A` is closed. Its boundary is
 * `∂A = ∂outer ∪ ∂hole_0 ∪ ∂hole_1 ∪ ...`. This identity is the definition
 * every predicate and measure below is derived from; there is no separate
 * geometric machinery.
 *
 * This is the library's first shape whose interior is not simply connected, and
 * it is the codomain that non-convex Minkowski sums, nested polygon
 * intersections, and region difference need.
 *
 * **Storage.** The outer boundary and every hole are ordinary @ref Polygon
 * values, each in @ref Polygon's own canonical form (counterclockwise,
 * lexicographically smallest vertex first) — holes are *not* stored reversed.
 * Orientation is a traversal detail supplied on demand by @ref orientedEdges,
 * which reverses hole rings so the region stays on the left of every directed
 * edge. Holes are kept sorted by `Polygon::operator<=>`, so equality, ordering
 * and hashing do not depend on the order they were supplied in.
 *
 * **Preconditions.** As with @ref Polygon, whose constructor does not check
 * simplicity, structural validity is a documented precondition rather than an
 * enforced invariant: every ring must be simple, each hole must lie inside the
 * outer polygon, and hole interiors must be pairwise disjoint.
 *
 * The contract is about *interiors* only. Ring boundaries are free to meet in
 * any way — at isolated points, or along shared stretches of edge, whether
 * between two holes or between a hole and the outer boundary. Where they do,
 * the region pinches shut and is locally one-dimensional there, which every
 * predicate accounts for. What is rejected is a hole overlapping another hole,
 * a hole escaping the outer polygon, and any self-intersecting ring.
 *
 * @ref isValid checks all of this on demand in O((n + k) log(n + k)); the
 * constructor only canonicalizes.
 *
 * @tparam PointType_ The vertex point type.
 * @tparam TLabel Optional label payload.
 */
template <class PointType_, class TLabel>
struct PolygonWithHoles {
    using PointType = PointType_;
    using NumberType = typename PointType::NumberType;
    using LabelType = TLabel;
    using PolygonType = Polygon<PointType>;
    using EdgeType = Segment<PointType>;
    static_assert(detail::is_point_v<PointType>, "PolygonWithHoles requires pgl::Point vertices");

    /**
     * @brief Creates the empty region (a vertexless outer polygon, no holes).
     */
    constexpr PolygonWithHoles() = default;

    /**
     * @brief Creates a hole-free region from its outer boundary.
     *
     * @param outer The outer boundary.
     */
    constexpr explicit PolygonWithHoles(PolygonType outer)
        : outer_(std::move(outer)) {}

    /**
     * @brief Creates a region from an outer boundary and a range of holes.
     *
     * Holes of zero area (points, segments, and @ref Polygon::isUndefined
     * rings) remove nothing from the region and are dropped. The remaining
     * holes are sorted into canonical order.
     *
     * @tparam HoleRange Range whose elements are polygons.
     * @param outer The outer boundary.
     * @param holes The holes; each must lie inside @p outer with interiors
     *        pairwise disjoint (a precondition, see @ref isValid).
     * @param trusted When `true`, adopt @p holes as given without dropping
     *        degenerate rings or sorting. Only pass `true` for a range that is
     *        already in canonical form.
     */
    template <std::ranges::input_range HoleRange>
        requires detail::is_polygon_v<std::ranges::range_value_t<HoleRange>>
    constexpr PolygonWithHoles(PolygonType outer, HoleRange&& holes, bool trusted = false)
        : outer_(std::move(outer)) {
        for (const auto& hole : holes) {
            holes_.emplace_back(hole);
        }
        if (!trusted) {
            normalize();
        }
    }

    /**
     * @brief Converts a region with compatible vertex type.
     *
     * The source rings are already canonical and a coordinate-type conversion
     * preserves both their orientation and their relative order, so no
     * renormalization is needed.
     *
     * @tparam OtherPointType Source vertex type.
     * @tparam OtherLabelType Source label type.
     * @param other Source region.
     */
    template <PointConcept OtherPointType, class OtherLabelType>
        requires(std::constructible_from<PointType, const OtherPointType&>)
    constexpr PolygonWithHoles(const PolygonWithHoles<OtherPointType, OtherLabelType>& other)
        : outer_(other.outer()) {
        holes_.reserve(other.holeCount());
        for (const auto& hole : other.holes()) {
            holes_.emplace_back(hole);
        }
    }

    /**
     * @brief Returns the region label.
     *
     * The label is mutable even through a const region: it is metadata that
     * does not participate in equality, hashing, or geometric predicates.
     *
     * @return Reference to the stored label.
     */
    template <class A = LabelType>
        requires(detail::has_label_v<A>)
    constexpr A& label() const {
        return label_;
    }

    // -------------------------------------------------------------------------
    // Ring access

    /** @brief Returns the outer boundary. */
    [[nodiscard]] constexpr const PolygonType& outer() const {
        return outer_;
    }

    /** @brief Returns the number of holes. */
    [[nodiscard]] constexpr std::size_t holeCount() const {
        return holes_.size();
    }

    /** @brief Tests whether the region has at least one hole. */
    [[nodiscard]] constexpr bool hasHoles() const {
        return !holes_.empty();
    }

    /**
     * @brief Accesses a hole by index.
     * @param index The index of the hole, in canonical (sorted) order.
     */
    [[nodiscard]] constexpr const PolygonType& hole(std::size_t index) const {
        assert(index < holes_.size());
        return holes_[index];
    }

    /** @brief Returns the holes in canonical order. */
    [[nodiscard]] constexpr const std::vector<PolygonType>& holes() const {
        return holes_;
    }

    /** @brief Returns a constant iterator to the first hole. */
    [[nodiscard]] constexpr auto begin() const { return holes_.begin(); }

    /** @brief Returns a constant iterator to the first hole. */
    [[nodiscard]] constexpr auto cbegin() const { return holes_.cbegin(); }

    /** @brief Returns a constant iterator past the last hole. */
    [[nodiscard]] constexpr auto end() const { return holes_.end(); }

    /** @brief Returns a constant iterator past the last hole. */
    [[nodiscard]] constexpr auto cend() const { return holes_.cend(); }

    /**
     * @brief Adds a hole, keeping the canonical order.
     *
     * A zero-area ring removes nothing and is ignored.
     *
     * @param hole The hole to add; must lie inside the outer boundary with
     *        interior disjoint from the existing holes (a precondition, see
     *        @ref isValid).
     */
    constexpr void addHole(PolygonType hole) {
        if (hole.isDegenerate()) {
            return;
        }
        const auto position = std::lower_bound(holes_.begin(), holes_.end(), hole);
        holes_.insert(position, std::move(hole));
        resetCache();
    }

    /**
     * @brief Returns the total number of vertices over all rings.
     *
     * Deliberately not named `size()`: unlike @ref Polygon::size this counts
     * the outer boundary *and* every hole, and a name shared with a shape whose
     * meaning differs would be a trap in generic code.
     */
    [[nodiscard]] constexpr std::size_t vertexCount() const {
        std::size_t total = outer_.size();
        for (const auto& hole : holes_) {
            total += hole.size();
        }
        return total;
    }

    /** @brief Returns the vertices of every ring, outer boundary first. */
    [[nodiscard]] constexpr std::vector<PointType> vertices() const {
        std::vector<PointType> result;
        result.reserve(vertexCount());
        for (const auto& vertex : outer_) {
            result.push_back(vertex);
        }
        for (const auto& hole : holes_) {
            for (const auto& vertex : hole) {
                result.push_back(vertex);
            }
        }
        return result;
    }

    /** @brief Returns the boundary edges of every ring, outer boundary first. */
    [[nodiscard]] constexpr std::vector<EdgeType> edges() const {
        std::vector<EdgeType> result;
        result.reserve(vertexCount());
        for (const auto& edge : outer_.edgesView()) {
            result.push_back(edge);
        }
        for (const auto& hole : holes_) {
            for (const auto& edge : hole.edgesView()) {
                result.push_back(edge);
            }
        }
        return result;
    }

    /**
     * @brief Returns the boundary edges directed so the region lies to the left.
     *
     * The outer ring is emitted counterclockwise as stored; hole rings are
     * emitted **reversed**, i.e. clockwise, which is the standard traversal
     * orientation for a holed region.
     */
    [[nodiscard]] constexpr std::vector<OrientedSegment<PointType>> orientedEdges() const {
        std::vector<OrientedSegment<PointType>> result;
        result.reserve(vertexCount());
        for (const auto& edge : outer_.orientedEdgesView()) {
            result.push_back(edge);
        }
        for (const auto& hole : holes_) {
            for (const auto& edge : hole.orientedEdgesView()) {
                result.emplace_back(edge.target(), edge.source());
            }
        }
        return result;
    }

    // -------------------------------------------------------------------------
    // Value semantics

    /** @brief Compares two regions by outer boundary, then by canonical hole list. */
    [[nodiscard]] constexpr auto operator<=>(const PolygonWithHoles& other) const {
        if (auto cmp = outer_ <=> other.outer_; cmp != 0) {
            return cmp;
        }
        if (auto cmp = holes_.size() <=> other.holes_.size(); cmp != 0) {
            return cmp;
        }
        for (std::size_t i = 0; i < holes_.size(); ++i) {
            if (auto cmp = holes_[i] <=> other.holes_[i]; cmp != 0) {
                return cmp;
            }
        }
        return std::strong_ordering::equal;
    }

    /** @brief Checks equality of two regions. */
    [[nodiscard]] constexpr bool operator==(const PolygonWithHoles& other) const {
        if (!(outer_ == other.outer_) || holes_.size() != other.holes_.size()) {
            return false;
        }
        for (std::size_t i = 0; i < holes_.size(); ++i) {
            if (!(holes_[i] == other.holes_[i])) {
                return false;
            }
        }
        return true;
    }

    // -------------------------------------------------------------------------
    // State queries

    /** @brief Tests whether the region has no outer boundary at all. */
    [[nodiscard]] constexpr bool isEmpty() const {
        return outer_.size() == 0;
    }

    /** @brief Tests whether the region has zero area. */
    [[nodiscard]] constexpr bool isDegenerate() const {
        return twiceArea() == NumberType(0);
    }

    /**
     * @brief Tests whether the region covers exactly one point.
     *
     * Zero-area holes are dropped at construction, so this is decided by the
     * outer boundary alone.
     */
    [[nodiscard]] constexpr bool isPoint() const {
        return outer_.isPoint();
    }

    /** @brief Tests whether the region covers exactly one segment of positive length. */
    [[nodiscard]] constexpr bool isSegment() const {
        return outer_.isSegment();
    }

    /**
     * @brief Tests whether the region is degenerate without covering a point or
     *        a segment (which includes the empty region).
     */
    [[nodiscard]] constexpr bool isUndefined() const {
        return !isPoint() && !isSegment() && isDegenerate();
    }

    /**
     * @brief Tests whether every ring is simple.
     *
     * This is a per-ring check only; it says nothing about how the rings sit
     * relative to one another. Use @ref isValid for the structural contract.
     *
     * Complexity: O(n log n) over the total vertex count.
     */
    template <class Rational = pgl::Rational<pgl::BigInt>>
    [[nodiscard]] bool isSimple() const {
        if (!outer_.template isSimple<Rational>()) {
            return false;
        }
        for (const auto& hole : holes_) {
            if (!hole.template isSimple<Rational>()) {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief Tests the structural contract: every ring simple, every hole
     *        inside the outer boundary, hole interiors pairwise disjoint.
     *
     * Ring boundaries may meet one another however they like — at isolated
     * points or along shared stretches of edge; only interiors are constrained.
     * What is rejected is one hole overlapping another, a hole escaping the
     * outer boundary, and any self-intersecting ring.
     *
     * This is a precondition of every other operation, checked on demand rather
     * than enforced by the constructor — mirroring @ref Polygon, which likewise
     * leaves simplicity to the caller.
     *
     * Complexity: O(n log n) over the total vertex count, plus one containment
     * test per hole and one interior-overlap test per bounding-box-overlapping
     * hole pair.
     */
    template <class Rational = pgl::Rational<pgl::BigInt>>
    [[nodiscard]] bool isValid() const;

    // -------------------------------------------------------------------------
    // Measures

    /**
     * @brief Computes twice the area of the region.
     *
     * `2·area(outer) − Σ 2·area(hole_i)`, exact in @ref NumberType with no
     * division.
     */
    [[nodiscard]] constexpr NumberType twiceArea() const {
        NumberType total = outer_.twiceArea();
        for (const auto& hole : holes_) {
            total -= hole.twiceArea();
        }
        return total;
    }

    /**
     * @brief Computes the area of the region.
     * @warning Uses division by 2.
     */
    template <class ResultNumber = NumberType>
    [[nodiscard]] constexpr auto area() const {
        ResultNumber result = static_cast<ResultNumber>(twiceArea());
        return result / ResultNumber(2);
    }

    /**
     * @brief Computes the area-weighted centroid of the region.
     *
     * The holes enter with negative weight:
     * `(c_outer·A_outer − Σ c_i·A_i) / (A_outer − Σ A_i)`. When the net area is
     * zero the region has no area-weighted centroid and the centroid of the
     * vertex set is returned instead, matching @ref Polygon::centroid.
     *
     * @tparam ResultNumber The number type for the result.
     * @warning Divides coordinates after casting to @p ResultNumber.
     */
    template <class ResultNumber = NumberType>
    [[nodiscard]] constexpr Point<ResultNumber> centroid() const;

    /** @brief Computes the centroid of the vertex set over all rings. */
    template <class ResultNumber = NumberType>
    [[nodiscard]] constexpr Point<ResultNumber> verticesCentroid() const;

    /**
     * @brief Returns a point strictly inside the region.
     *
     * The point is in the region's interior, so it is inside the outer boundary
     * and outside every hole. @ref Polygon finds one in O(n) from an ear of its
     * lexicographically smallest vertex; that argument does not survive holes —
     * an ear can be occupied by one, and a diagonal interrupted by one — so this
     * triangulates and takes a point inside the first triangle of the domain.
     *
     * Complexity: O(n log n) over the total vertex count.
     *
     * @tparam ResultNumber The number type for the result.
     * @return A point guaranteed to be inside the region.
     * @warning Divides coordinates by 4 (see @ref Triangle::pointInside), so it
     *          is inexact for integer coordinates not divisible by it. Undefined
     *          for a region with no area.
     */
    template <class ResultNumber = NumberType>
    [[nodiscard]] Point<ResultNumber> pointInside() const;

    /**
     * @brief Tests whether some point in this shape's relative interior lies in
     *        the strict interior of @p shape.
     *
     * Uses @ref pointInside as the witness. When integer truncation rounds that
     * witness onto or outside the boundary, this shape and @p shape are scaled
     * so the witness is exact, leaving the containment relation unchanged.
     */
    template <class OtherShape>
    [[nodiscard]] bool pointInsideInteriorContainedIn(const OtherShape& shape) const;

    /**
     * @brief Builds the constrained Delaunay triangulation of this region.
     *
     * Equivalent to `Triangulation(*this)`. Every ring becomes constrained
     * edges and the hole interiors are left out of the domain, so the in-domain
     * triangles cover exactly the part of the region that has area (a slit,
     * having none, carries no triangle). The region must satisfy @ref isValid.
     *
     * @return A @ref Triangulation whose in-domain triangles cover the region.
     */
    auto triangulation() const;

    /**
     * @brief Builds the constrained Delaunay triangulation of this region with
     *        the given interior constraint segments.
     *
     * Equivalent to `Triangulation(*this, segments)`.
     *
     * @tparam SegmentRange Range whose elements are segments.
     * @param segments Constraint edges, assumed to lie in the region.
     */
    template <class SegmentRange>
    auto triangulation(const SegmentRange& segments) const;

    /**
     * @brief Returns a segment realizing the diameter (the farthest vertex pair).
     *
     * The holes lie inside the outer boundary, so they cannot contribute a
     * farther pair: this is the outer polygon's diameter.
     */
    [[nodiscard]] constexpr Segment<PointType> diameter() const {
        return outer_.diameter();
    }

    /**
     * @brief Computes the bounding box of the region.
     *
     * The holes lie inside the outer boundary, so this is the outer polygon's
     * box, cached there.
     */
    [[nodiscard]] constexpr const Rectangle<PointType>& bbox() const {
        return outer_.bbox();
    }

    /** @brief Computes the floating-point bounding box of the region. */
    template <std::floating_point ResultNumber = double>
    [[nodiscard]] constexpr Rectangle<Point<ResultNumber>> fbox() const {
        return outer_.template fbox<ResultNumber>();
    }

    // -------------------------------------------------------------------------
    // Predicates against a point
    //
    // Direct rewritings of A = outer \ ⋃ hole°:
    //   contains(p)         outer contains p, and no hole contains p strictly
    //   interiorContains(p) outer contains p strictly, and no hole contains p
    //   boundaryContains(p) p lies on the outer ring or on some hole ring

    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     *
     * The point is in the region when the outer polygon contains it and no hole
     * contains it strictly — a point on a hole boundary is on `∂A` and is
     * therefore contained.
     *
     * Complexity: O(n) over the total vertex count.
     */
    template <PointConcept OtherPoint>
    [[nodiscard]] constexpr bool contains(const OtherPoint& point) const;

    /**
     * @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B).
     *
     * Complexity: O(n) over the total vertex count.
     */
    template <PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorContains(const OtherPoint& point) const;

    /**
     * @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B).
     *
     * The boundary is the union of all rings, so this holds when the point lies
     * on the outer ring or on any hole ring.
     *
     * Complexity: O(n) over the total vertex count.
     */
    template <PointConcept OtherPoint>
    [[nodiscard]] constexpr bool boundaryContains(const OtherPoint& point) const;

    /**
     * @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅).
     *
     * Complexity: O(n) over the total vertex count.
     */
    template <PointConcept OtherPoint>
    [[nodiscard]] constexpr bool intersects(const OtherPoint& point) const {
        return contains(point);
    }

    /**
     * @brief Tests whether the interiors of the shapes intersect (A° ∩ B° ≠ ∅).
     *
     * A point has empty interior, so this is always `false`, matching
     * @ref Polygon.
     */
    template <PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherPoint&) const {
        return false;
    }

    // -------------------------------------------------------------------------
    // Predicates against a segment
    //
    // Rewritings of A = outer \ ⋃ hole°, using the fact that a segment is
    // connected and that every point of every ring boundary belongs to A:
    //   contains(S)         S ⊆ outer, and no hole interior meets S
    //   interiorContains(S) S ⊆ outer°, and no hole meets S at all
    //   intersects(S)       S meets a ring boundary, or an endpoint decides
    //   boundaryContains(S) S ⊆ A with no piece reaching the interior

    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     *
     * The segment is in the region when the outer polygon contains it and it
     * never enters a hole interior; running along a hole boundary is allowed.
     *
     * Complexity: O(n·k) for a region of n vertices and k holes.
     */
    template <SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool contains(const OtherSegment& other) const;

    /** @copydoc contains(const OtherSegment&) const */
    template <OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool contains(const OtherOrientedSegment& other) const;

    /**
     * @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B).
     *
     * The region interior excludes every ring, so the segment must stay strictly
     * inside the outer polygon and miss every hole entirely — touching a hole
     * boundary already leaves the interior.
     */
    template <SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool interiorContains(const OtherSegment& other) const;

    /** @copydoc interiorContains(const OtherSegment&) const */
    template <OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool interiorContains(const OtherOrientedSegment& other) const;

    /**
     * @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B).
     *
     * True when the segment lies in the region without any part of it reaching
     * the region interior, which is exactly lying on the union of the rings —
     * a segment running from an outer edge onto a collinear hole edge included.
     */
    template <SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool boundaryContains(const OtherSegment& other) const;

    /** @copydoc boundaryContains(const OtherSegment&) const */
    template <OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool boundaryContains(const OtherOrientedSegment& other) const;

    /**
     * @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅).
     *
     * Complexity: O(n) segment-segment tests over the total vertex count.
     */
    template <SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool intersects(const OtherSegment& other) const;

    /** @copydoc intersects(const OtherSegment&) const */
    template <OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool intersects(const OtherOrientedSegment& other) const;

    /**
     * @brief Tests whether the interiors of the shapes intersect (A° ∩ B° ≠ ∅).
     *
     * The open segment must reach the open region: crossing a bridge between two
     * holes counts, running along a ring does not, and neither does passing
     * through a point where two rings touch — the region pinches shut there.
     *
     * Complexity: O(n + c²) for a total vertex count of n, where c is the number
     * of boundary crossings the segment makes (typically a small constant).
     */
    template <SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherSegment& other) const;

    /** @copydoc interiorsIntersect(const OtherSegment&) const */
    template <OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherOrientedSegment& other) const;

    // -------------------------------------------------------------------------
    // Predicates against a line, an oriented line, a ray, and a half-plane
    //
    // All four operands are unbounded, which decides two of the five relations
    // outright:
    //   contains/interiorContains/boundaryContains  only a degenerate operand
    //   intersects(B)                               outer.intersects(B)
    //
    // The second line needs no hole bookkeeping: an unbounded connected shape
    // that reaches the bounded outer polygon has to leave it again, so it meets
    // ∂outer, and every point of ∂outer belongs to the region because hole
    // interiors never reach it.

    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     *
     * The region is bounded and a line is not, so only a degenerate line — a
     * single point — can be contained.
     */
    template <LineConcept OtherLine>
    [[nodiscard]] constexpr bool contains(const OtherLine& other) const;

    /** @copydoc contains(const OtherLine&) const */
    template <OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool contains(const OtherOrientedLine& other) const;

    /** @copydoc contains(const OtherLine&) const */
    template <RayConcept OtherRay>
    [[nodiscard]] constexpr bool contains(const OtherRay& other) const;

    /** @copydoc contains(const OtherLine&) const */
    template <HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool contains(const OtherHalfplane& other) const;

    /**
     * @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B).
     *
     * Unbounded, so again only a degenerate operand qualifies.
     */
    template <LineConcept OtherLine>
    [[nodiscard]] constexpr bool interiorContains(const OtherLine& other) const;

    /** @copydoc interiorContains(const OtherLine&) const */
    template <OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool interiorContains(const OtherOrientedLine& other) const;

    /** @copydoc interiorContains(const OtherLine&) const */
    template <RayConcept OtherRay>
    [[nodiscard]] constexpr bool interiorContains(const OtherRay& other) const;

    /** @copydoc interiorContains(const OtherLine&) const */
    template <HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool interiorContains(const OtherHalfplane& other) const;

    /**
     * @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B).
     *
     * The boundary is bounded too, so only a degenerate operand qualifies.
     */
    template <LineConcept OtherLine>
    [[nodiscard]] constexpr bool boundaryContains(const OtherLine& other) const;

    /** @copydoc boundaryContains(const OtherLine&) const */
    template <OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool boundaryContains(const OtherOrientedLine& other) const;

    /** @copydoc boundaryContains(const OtherLine&) const */
    template <RayConcept OtherRay>
    [[nodiscard]] constexpr bool boundaryContains(const OtherRay& other) const;

    /** @copydoc boundaryContains(const OtherLine&) const */
    template <HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool boundaryContains(const OtherHalfplane& other) const;

    /**
     * @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅).
     *
     * Complexity: O(n) over the total vertex count.
     */
    template <LineConcept OtherLine>
    [[nodiscard]] constexpr bool intersects(const OtherLine& other) const;

    /** @copydoc intersects(const OtherLine&) const */
    template <OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool intersects(const OtherOrientedLine& other) const;

    /** @copydoc intersects(const OtherLine&) const */
    template <RayConcept OtherRay>
    [[nodiscard]] constexpr bool intersects(const OtherRay& other) const;

    /** @copydoc intersects(const OtherLine&) const */
    template <HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool intersects(const OtherHalfplane& other) const;

    /**
     * @brief Tests whether the interiors of the shapes intersect (A° ∩ B° ≠ ∅).
     *
     * The line has to reach the open region: crossing a hole, running along a
     * ring, and passing through a point where two rings touch all fail, and a
     * line swallowed by a hole that touches the outer ring twice fails as well.
     *
     * Complexity: O(n + c²) for a total vertex count of n, where c is the number
     * of boundary crossings the line makes.
     */
    template <LineConcept OtherLine>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherLine& other) const;

    /** @copydoc interiorsIntersect(const OtherLine&) const */
    template <OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherOrientedLine& other) const;

    /** @copydoc interiorsIntersect(const OtherLine&) const */
    template <RayConcept OtherRay>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherRay& other) const;

    /**
     * @brief Tests whether the interiors of the shapes intersect (A° ∩ B° ≠ ∅).
     *
     * The open half-plane has to reach the open region. Unlike @ref Polygon,
     * where a vertex strictly inside the half-plane settles it, a ring vertex
     * here can be a place where the region is only one-dimensional (the tip of
     * a slit); such a vertex carries no region interior with it and does not
     * count. See @ref isSolidVertex.
     *
     * Complexity: O(n) when the region has no ring contacts, O(n³) in the worst
     * case, for a total vertex count of n.
     */
    template <HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherHalfplane& other) const;

    // -------------------------------------------------------------------------
    // Predicates against a bounded shape with area: a rectangle, a triangle, a
    // convex polygon, a simple polygon, and another region with holes.
    //
    // All five are closed, connected, and — when they have any area — the
    // closure of their own interior, which is what lets the rewritings of
    // A = outer ∖ ⋃ hole° stay in terms of the operand itself:
    //   contains(B)         B ⊆ outer, and no hole interior meets B
    //   interiorContains(B) B ⊆ outer°, and no hole meets B at all
    //   boundaryContains(B) only a B without area, edge by edge
    //   intersects(B)       B meets a ring, or one point of B decides
    //   interiorsIntersect(B) an edge of B reaches A°, or a domain triangle
    //                       of A meets B°
    //
    // The last one is the only one that cannot be read off the rings: A° is
    // neither simply connected nor even connected, so the witness arguments the
    // simply connected shapes use do not carry over. See @ref
    // areaInteriorsIntersect.

    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     *
     * The shape is in the region when the outer boundary contains it and it
     * never enters a hole interior; touching or running along a hole boundary is
     * allowed, and so is enclosing a hole from outside — that hole's boundary is
     * part of the region, but its interior is not, so a shape that swallows one
     * is *not* contained.
     *
     * Complexity: O(n·m) for a region of n vertices and an operand of m.
     */
    template <RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool contains(const OtherRectangle& other) const;

    /** @copydoc contains(const OtherRectangle&) const */
    template <TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr bool contains(const OtherTriangle& other) const;

    /** @copydoc contains(const OtherRectangle&) const */
    template <ConvexConcept OtherConvex>
    [[nodiscard]] constexpr bool contains(const OtherConvex& other) const;

    /** @copydoc contains(const OtherRectangle&) const */
    template <PolygonConcept OtherPolygon>
    [[nodiscard]] constexpr bool contains(const OtherPolygon& other) const;

    /** @copydoc contains(const OtherRectangle&) const */
    template <PolygonWithHolesConcept OtherRegion>
    [[nodiscard]] constexpr bool contains(const OtherRegion& other) const;

    /**
     * @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B).
     *
     * The region interior is the open outer polygon with every *closed* hole
     * removed, so the shape must stay strictly inside the outer boundary and
     * miss every hole outright — touching a hole boundary already leaves the
     * interior.
     */
    template <RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool interiorContains(const OtherRectangle& other) const;

    /** @copydoc interiorContains(const OtherRectangle&) const */
    template <TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr bool interiorContains(const OtherTriangle& other) const;

    /** @copydoc interiorContains(const OtherRectangle&) const */
    template <ConvexConcept OtherConvex>
    [[nodiscard]] constexpr bool interiorContains(const OtherConvex& other) const;

    /** @copydoc interiorContains(const OtherRectangle&) const */
    template <PolygonConcept OtherPolygon>
    [[nodiscard]] constexpr bool interiorContains(const OtherPolygon& other) const;

    /** @copydoc interiorContains(const OtherRectangle&) const */
    template <PolygonWithHolesConcept OtherRegion>
    [[nodiscard]] constexpr bool interiorContains(const OtherRegion& other) const;

    /**
     * @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B).
     *
     * The boundary is a finite union of segments and therefore has no area, so
     * only an operand that has collapsed can lie on it. A collapsed operand is
     * exactly the union of its edges, which the segment overload settles.
     */
    template <RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool boundaryContains(const OtherRectangle& other) const;

    /** @copydoc boundaryContains(const OtherRectangle&) const */
    template <TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr bool boundaryContains(const OtherTriangle& other) const;

    /** @copydoc boundaryContains(const OtherRectangle&) const */
    template <ConvexConcept OtherConvex>
    [[nodiscard]] constexpr bool boundaryContains(const OtherConvex& other) const;

    /** @copydoc boundaryContains(const OtherRectangle&) const */
    template <PolygonConcept OtherPolygon>
    [[nodiscard]] constexpr bool boundaryContains(const OtherPolygon& other) const;

    /** @copydoc boundaryContains(const OtherRectangle&) const */
    template <PolygonWithHolesConcept OtherRegion>
    [[nodiscard]] constexpr bool boundaryContains(const OtherRegion& other) const;

    /**
     * @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅).
     *
     * Complexity: O(n·m) for a region of n vertices and an operand of m.
     */
    template <RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool intersects(const OtherRectangle& other) const;

    /** @copydoc intersects(const OtherRectangle&) const */
    template <TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr bool intersects(const OtherTriangle& other) const;

    /** @copydoc intersects(const OtherRectangle&) const */
    template <ConvexConcept OtherConvex>
    [[nodiscard]] constexpr bool intersects(const OtherConvex& other) const;

    /** @copydoc intersects(const OtherRectangle&) const */
    template <PolygonConcept OtherPolygon>
    [[nodiscard]] constexpr bool intersects(const OtherPolygon& other) const;

    /** @copydoc intersects(const OtherRectangle&) const */
    template <PolygonWithHolesConcept OtherRegion>
    [[nodiscard]] constexpr bool intersects(const OtherRegion& other) const;

    /**
     * @brief Tests whether the interiors of the shapes intersect (A° ∩ B° ≠ ∅).
     *
     * Enclosing a hole does not count, and neither does meeting the region only
     * where two rings touch. When the operand's boundary misses the open region
     * entirely this triangulates, because the open region may come apart into
     * several pieces and no single witness point speaks for all of them.
     *
     * Complexity, for a region of n vertices and an operand of m: O(n·m) when an
     * edge of the operand settles it, and O(n log n + n·m) for the triangulated
     * fallback. Against another region with holes there is no edge shortcut —
     * a region need not have interior beside its own boundary — and both domains
     * are triangulated and compared triangle by triangle, O(n log n + m log m +
     * n·m).
     */
    template <RectangleConcept OtherRectangle>
    [[nodiscard]] bool interiorsIntersect(const OtherRectangle& other) const;

    /** @copydoc interiorsIntersect(const OtherRectangle&) const */
    template <TriangleConcept OtherTriangle>
    [[nodiscard]] bool interiorsIntersect(const OtherTriangle& other) const;

    /** @copydoc interiorsIntersect(const OtherRectangle&) const */
    template <ConvexConcept OtherConvex>
    [[nodiscard]] bool interiorsIntersect(const OtherConvex& other) const;

    /** @copydoc interiorsIntersect(const OtherRectangle&) const */
    template <PolygonConcept OtherPolygon>
    [[nodiscard]] bool interiorsIntersect(const OtherPolygon& other) const;

    /** @copydoc interiorsIntersect(const OtherRectangle&) const */
    template <PolygonWithHolesConcept OtherRegion>
    [[nodiscard]] bool interiorsIntersect(const OtherRegion& other) const;

    // -------------------------------------------------------------------------
    // Predicates against a polygonal chain: a monotone chain and a polyline.
    //
    // Both are one-dimensional and are exactly the union of their edges, so the
    // four set-level relations are settled edge by edge with no hole bookkeeping
    // of their own — the segment overloads already carry it. Only
    // interiorsIntersect needs the chain's own convention: its relative interior
    // is the chain minus its two extreme points, i.e. the open edges together
    // with the vertices between them.

    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     *
     * The chain is exactly the union of its edges, so it is in the region when
     * every edge is; an empty chain is contained trivially.
     *
     * Complexity: O(n·m) for a region of n vertices and a chain of m.
     */
    template <MonotoneChainConcept OtherChain>
    [[nodiscard]] constexpr bool contains(const OtherChain& other) const;

    /** @copydoc contains(const OtherChain&) const */
    template <PolylineConcept OtherPolyline>
    [[nodiscard]] constexpr bool contains(const OtherPolyline& other) const;

    /** @copydoc contains(const OtherChain&) const */
    template <MonotoneChainConcept OtherChain>
    [[nodiscard]] constexpr bool interiorContains(const OtherChain& other) const;

    /** @copydoc contains(const OtherChain&) const */
    template <PolylineConcept OtherPolyline>
    [[nodiscard]] constexpr bool interiorContains(const OtherPolyline& other) const;

    /** @copydoc contains(const OtherChain&) const */
    template <MonotoneChainConcept OtherChain>
    [[nodiscard]] constexpr bool boundaryContains(const OtherChain& other) const;

    /** @copydoc contains(const OtherChain&) const */
    template <PolylineConcept OtherPolyline>
    [[nodiscard]] constexpr bool boundaryContains(const OtherPolyline& other) const;

    /** @copydoc contains(const OtherChain&) const */
    template <MonotoneChainConcept OtherChain>
    [[nodiscard]] constexpr bool intersects(const OtherChain& other) const;

    /** @copydoc contains(const OtherChain&) const */
    template <PolylineConcept OtherPolyline>
    [[nodiscard]] constexpr bool intersects(const OtherPolyline& other) const;

    /**
     * @brief Tests whether the interiors of the shapes intersect (A° ∩ B° ≠ ∅).
     *
     * The chain's relative interior is the chain minus its two extreme points,
     * so it reaches the open region when an open edge does or when a vertex
     * between two edges lies strictly inside it. The region interior is open and
     * two-dimensional, which is what lets the shared chain helper answer this:
     * an open edge point that has to be discarded — an extreme the chain passes
     * through again — is surrounded by edge points that do not.
     */
    template <MonotoneChainConcept OtherChain>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherChain& other) const;

    /** @copydoc interiorsIntersect(const OtherChain&) const */
    template <PolylineConcept OtherPolyline>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherPolyline& other) const;

    // -------------------------------------------------------------------------
    // Predicates against a disk
    //
    // A disk is closed, bounded, connected, and — unless it has degenerated —
    // the closure of its own interior. That last property is what the area
    // operands of §3 could not assume, and having it back brings the direct
    // per-hole rewriting of A = outer ∖ ⋃ hole° with it:
    //   contains(D)         outer contains D, and no hole interior meets D
    //   interiorContains(D) outer° contains D, and no hole meets D at all
    // The disk has no edges, though, so interiorsIntersect has no boundary scan
    // to fall back on and goes to the triangulated domain directly.

    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     *
     * The disk is in the region when the outer polygon contains it and it never
     * enters a hole interior; a disk tangent to a hole from outside is contained,
     * one that swallows a hole is not.
     *
     * A degenerate disk goes to the point overload with @ref Disk::a: that is
     * exactly the disk when its radius is zero, and a disk whose defining points
     * are collinear but not all equal is undefined (it determines no circle), so
     * any terminating answer meets the contract. The region cannot leave this to
     * the outer polygon — a point of a hole interior is inside the outer polygon
     * and outside the region.
     */
    template <DiskConcept OtherDisk>
    [[nodiscard]] constexpr bool contains(const OtherDisk& other) const;

    /** @copydoc contains(const OtherDisk&) const */
    template <DiskConcept OtherDisk>
    [[nodiscard]] constexpr bool interiorContains(const OtherDisk& other) const;

    /** @copydoc contains(const OtherDisk&) const */
    template <DiskConcept OtherDisk>
    [[nodiscard]] constexpr bool boundaryContains(const OtherDisk& other) const;

    /** @copydoc contains(const OtherDisk&) const */
    template <DiskConcept OtherDisk>
    [[nodiscard]] constexpr bool intersects(const OtherDisk& other) const;

    /**
     * @brief Tests whether the interiors of the shapes intersect (A° ∩ B° ≠ ∅).
     *
     * A disk contributes no edges to scan, so once the cheap witness test fails
     * this triangulates: the domain triangles tile closure(A°), and the open
     * disk meets A° exactly when it meets one of their interiors.
     *
     * Complexity: O(n log n) for a region of n vertices.
     */
    template <DiskConcept OtherDisk>
    [[nodiscard]] bool interiorsIntersect(const OtherDisk& other) const;

    // -------------------------------------------------------------------------
    // Predicates against a half-plane intersection
    //
    // The operand is convex and closed but need not be bounded, which splits the
    // work in two. The three containment relations want a bounded operand — the
    // region is bounded — and a bounded, non-degenerate half-plane intersection
    // is a convex polygon, so they hand it to the area path as one. A degenerate
    // one is a point, a segment, a ray, or a line, and goes to the overload for
    // that carrier. intersects and interiorsIntersect keep the unbounded case:
    // the first by the ring-contact argument the other operands use, the second
    // by clipping the operand to the region's bounding box first, which changes
    // no answer because the region interior lies strictly inside that box.

    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     *
     * Only a bounded operand can lie in the bounded region; the empty region is
     * contained by convention, matching the other shapes.
     */
    template <HalfplaneIntersectionConcept OtherIntersection>
    [[nodiscard]] constexpr bool contains(const OtherIntersection& other) const;

    /** @copydoc contains(const OtherIntersection&) const */
    template <HalfplaneIntersectionConcept OtherIntersection>
    [[nodiscard]] constexpr bool interiorContains(const OtherIntersection& other) const;

    /** @copydoc contains(const OtherIntersection&) const */
    template <HalfplaneIntersectionConcept OtherIntersection>
    [[nodiscard]] constexpr bool boundaryContains(const OtherIntersection& other) const;

    /** @copydoc contains(const OtherIntersection&) const */
    template <HalfplaneIntersectionConcept OtherIntersection>
    [[nodiscard]] constexpr bool intersects(const OtherIntersection& other) const;

    /**
     * @brief Tests whether the interiors of the shapes intersect (A° ∩ B° ≠ ∅).
     *
     * The operand is clipped to the region's bounding box, which leaves the
     * answer alone — A° is an open subset of that box and therefore misses its
     * boundary — and turns an unbounded operand into a convex polygon the area
     * path already handles.
     */
    template <HalfplaneIntersectionConcept OtherIntersection>
    [[nodiscard]] bool interiorsIntersect(const OtherIntersection& other) const;

    // -------------------------------------------------------------------------
    // Cut predicates
    //
    // `A.separates(B)` asks whether `B ∖ A` is disconnected, and
    // `A.crosses(B)` whether each shape separates the other. Both are collected
    // here rather than split per operand family because a region settles every
    // one of them the same way — the cell engine of implementation/
    // separates.hpp, which assumes nothing about either operand — while a
    // region without holes forwards to its outer polygon throughout.
    //
    // A region is connected however its rings meet — its complement is a
    // disjoint union of simply connected open sets, which encloses nothing —
    // so `B ∖ A` comes apart only when the removal genuinely severs it. What
    // does change against the simply connected operands is what suffices to
    // sever: a region is cut by a single point at a pinch, or by a segment run
    // from one hole to another, neither of which can cut a polygon.

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template <PointConcept OtherPoint>
    [[nodiscard]] bool separates(const OtherPoint& other) const;

    /** @copydoc separates(const OtherPoint&) const */
    template <SegmentConcept OtherSegment>
    [[nodiscard]] bool separates(const OtherSegment& other) const;

    /** @copydoc separates(const OtherPoint&) const */
    template <OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] bool separates(const OtherOrientedSegment& other) const;

    /** @copydoc separates(const OtherPoint&) const */
    template <LineConcept OtherLine>
    [[nodiscard]] bool separates(const OtherLine& other) const;

    /** @copydoc separates(const OtherPoint&) const */
    template <OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] bool separates(const OtherOrientedLine& other) const;

    /** @copydoc separates(const OtherPoint&) const */
    template <RayConcept OtherRay>
    [[nodiscard]] bool separates(const OtherRay& other) const;

    /** @copydoc separates(const OtherPoint&) const */
    template <HalfplaneConcept OtherHalfplane>
    [[nodiscard]] bool separates(const OtherHalfplane& other) const;

    /** @copydoc separates(const OtherPoint&) const */
    template <RectangleConcept OtherRectangle>
    [[nodiscard]] bool separates(const OtherRectangle& other) const;

    /** @copydoc separates(const OtherPoint&) const */
    template <TriangleConcept OtherTriangle>
    [[nodiscard]] bool separates(const OtherTriangle& other) const;

    /** @copydoc separates(const OtherPoint&) const */
    template <ConvexConcept OtherConvex>
    [[nodiscard]] bool separates(const OtherConvex& other) const;

    /** @copydoc separates(const OtherPoint&) const */
    template <PolygonConcept OtherPolygon>
    [[nodiscard]] bool separates(const OtherPolygon& other) const;

    /** @copydoc separates(const OtherPoint&) const */
    template <PolygonWithHolesConcept OtherRegion>
    [[nodiscard]] bool separates(const OtherRegion& other) const;

    /** @copydoc separates(const OtherPoint&) const */
    template <MonotoneChainConcept OtherChain>
    [[nodiscard]] bool separates(const OtherChain& other) const;

    /** @copydoc separates(const OtherPoint&) const */
    template <PolylineConcept OtherPolyline>
    [[nodiscard]] bool separates(const OtherPolyline& other) const;

    /** @copydoc separates(const OtherPoint&) const */
    template <HalfplaneIntersectionConcept OtherIntersection>
    [[nodiscard]] bool separates(const OtherIntersection& other) const;

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template <PointConcept OtherPoint>
    [[nodiscard]] bool crosses(const OtherPoint& other) const;

    /** @copydoc crosses(const OtherPoint&) const */
    template <SegmentConcept OtherSegment>
    [[nodiscard]] bool crosses(const OtherSegment& other) const;

    /** @copydoc crosses(const OtherPoint&) const */
    template <OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] bool crosses(const OtherOrientedSegment& other) const;

    /** @copydoc crosses(const OtherPoint&) const */
    template <LineConcept OtherLine>
    [[nodiscard]] bool crosses(const OtherLine& other) const;

    /** @copydoc crosses(const OtherPoint&) const */
    template <OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] bool crosses(const OtherOrientedLine& other) const;

    /** @copydoc crosses(const OtherPoint&) const */
    template <RayConcept OtherRay>
    [[nodiscard]] bool crosses(const OtherRay& other) const;

    /** @copydoc crosses(const OtherPoint&) const */
    template <HalfplaneConcept OtherHalfplane>
    [[nodiscard]] bool crosses(const OtherHalfplane& other) const;

    /** @copydoc crosses(const OtherPoint&) const */
    template <RectangleConcept OtherRectangle>
    [[nodiscard]] bool crosses(const OtherRectangle& other) const;

    /** @copydoc crosses(const OtherPoint&) const */
    template <TriangleConcept OtherTriangle>
    [[nodiscard]] bool crosses(const OtherTriangle& other) const;

    /** @copydoc crosses(const OtherPoint&) const */
    template <ConvexConcept OtherConvex>
    [[nodiscard]] bool crosses(const OtherConvex& other) const;

    /** @copydoc crosses(const OtherPoint&) const */
    template <PolygonConcept OtherPolygon>
    [[nodiscard]] bool crosses(const OtherPolygon& other) const;

    /** @copydoc crosses(const OtherPoint&) const */
    template <PolygonWithHolesConcept OtherRegion>
    [[nodiscard]] bool crosses(const OtherRegion& other) const;

    /** @copydoc crosses(const OtherPoint&) const */
    template <MonotoneChainConcept OtherChain>
    [[nodiscard]] bool crosses(const OtherChain& other) const;

    /** @copydoc crosses(const OtherPoint&) const */
    template <PolylineConcept OtherPolyline>
    [[nodiscard]] bool crosses(const OtherPolyline& other) const;

    /** @copydoc crosses(const OtherPoint&) const */
    template <HalfplaneIntersectionConcept OtherIntersection>
    [[nodiscard]] bool crosses(const OtherIntersection& other) const;

    // -------------------------------------------------------------------------
    // Distances
    //
    // The region is closed, so whenever it misses the other shape the nearest
    // pair is realized on ∂A — the minimum over the edges of every ring.

    /**
     * @brief Computes the squared Euclidean distance to the other shape.
     *
     * Zero when the shapes intersect; otherwise the smallest squared distance
     * between them, which the region attains on one of its ring edges.
     *
     * Complexity: O(n) edge queries over the total vertex count.
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

    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, PolygonWithHolesConcept OtherRegion>
    [[nodiscard]] constexpr auto squaredDistance(const OtherRegion& other) const;

    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, MonotoneChainConcept OtherChain>
    [[nodiscard]] constexpr auto squaredDistance(const OtherChain& other) const;

    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, PolylineConcept OtherPolyline>
    [[nodiscard]] constexpr auto squaredDistance(const OtherPolyline& other) const;

    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, HalfplaneIntersectionConcept OtherIntersection>
    [[nodiscard]] constexpr auto squaredDistance(const OtherIntersection& other) const;

    /**
     * @brief Computes the squared Euclidean distance to a disk.
     *
     * Always returns `double`, like every other distance to a @ref Disk: the
     * nearest point of a disjoint disk is on its circle, so the exact value is
     * generally irrational.
     */
    template <class ResultNumber = NumberType, DiskConcept OtherDisk>
    [[nodiscard]] double squaredDistance(const OtherDisk& other) const;

    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, PointConcept OtherPoint>
    [[nodiscard]] constexpr auto distanceL1(const OtherPoint& point) const;

    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, SegmentConcept OtherSegment>
    [[nodiscard]] constexpr auto distanceL1(const OtherSegment& other) const;

    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr auto distanceL1(const OtherOrientedSegment& other) const;

    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, LineConcept OtherLine>
    [[nodiscard]] constexpr auto distanceL1(const OtherLine& other) const;

    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr auto distanceL1(const OtherOrientedLine& other) const;

    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, RayConcept OtherRay>
    [[nodiscard]] constexpr auto distanceL1(const OtherRay& other) const;

    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr auto distanceL1(const OtherHalfplane& other) const;

    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr auto distanceL1(const OtherRectangle& other) const;

    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr auto distanceL1(const OtherTriangle& other) const;

    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, ConvexConcept OtherConvex>
    [[nodiscard]] constexpr auto distanceL1(const OtherConvex& other) const;

    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, PolygonConcept OtherPolygon>
    [[nodiscard]] constexpr auto distanceL1(const OtherPolygon& other) const;

    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, PolygonWithHolesConcept OtherRegion>
    [[nodiscard]] constexpr auto distanceL1(const OtherRegion& other) const;

    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, MonotoneChainConcept OtherChain>
    [[nodiscard]] constexpr auto distanceL1(const OtherChain& other) const;

    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, PolylineConcept OtherPolyline>
    [[nodiscard]] constexpr auto distanceL1(const OtherPolyline& other) const;

    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, HalfplaneIntersectionConcept OtherIntersection>
    [[nodiscard]] constexpr auto distanceL1(const OtherIntersection& other) const;

    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, PointConcept OtherPoint>
    [[nodiscard]] constexpr auto distanceLInf(const OtherPoint& point) const;

    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, SegmentConcept OtherSegment>
    [[nodiscard]] constexpr auto distanceLInf(const OtherSegment& other) const;

    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr auto distanceLInf(const OtherOrientedSegment& other) const;

    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, LineConcept OtherLine>
    [[nodiscard]] constexpr auto distanceLInf(const OtherLine& other) const;

    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr auto distanceLInf(const OtherOrientedLine& other) const;

    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, RayConcept OtherRay>
    [[nodiscard]] constexpr auto distanceLInf(const OtherRay& other) const;

    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr auto distanceLInf(const OtherHalfplane& other) const;

    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr auto distanceLInf(const OtherRectangle& other) const;

    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr auto distanceLInf(const OtherTriangle& other) const;

    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, ConvexConcept OtherConvex>
    [[nodiscard]] constexpr auto distanceLInf(const OtherConvex& other) const;

    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, PolygonConcept OtherPolygon>
    [[nodiscard]] constexpr auto distanceLInf(const OtherPolygon& other) const;

    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, PolygonWithHolesConcept OtherRegion>
    [[nodiscard]] constexpr auto distanceLInf(const OtherRegion& other) const;

    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, MonotoneChainConcept OtherChain>
    [[nodiscard]] constexpr auto distanceLInf(const OtherChain& other) const;

    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, PolylineConcept OtherPolyline>
    [[nodiscard]] constexpr auto distanceLInf(const OtherPolyline& other) const;

    /** @copydoc squaredDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, HalfplaneIntersectionConcept OtherIntersection>
    [[nodiscard]] constexpr auto distanceLInf(const OtherIntersection& other) const;

    // -------------------------------------------------------------------------
    // The empty set is a subset of every shape, so its containment relations
    // are true and its intersection relations are false.

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool contains(const EmptyShape<EmptyPoint>&) const {
        return true;
    }

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool interiorContains(const EmptyShape<EmptyPoint>&) const {
        return true;
    }

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool boundaryContains(const EmptyShape<EmptyPoint>&) const {
        return true;
    }

    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool intersects(const EmptyShape<EmptyPoint>&) const {
        return false;
    }

    /** @brief Tests whether the interiors of the shapes intersect (A° ∩ B° ≠ ∅). */
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool interiorsIntersect(const EmptyShape<EmptyPoint>&) const {
        return false;
    }

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool separates(const EmptyShape<EmptyPoint>&) const {
        return false;
    }

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool crosses(const EmptyShape<EmptyPoint>&) const {
        return false;
    }

    // -------------------------------------------------------------------------
    // Transformations

    /** @brief Translates the region in place. */
    template <class TranslationNumber, class TranslationLabel>
    constexpr PolygonWithHoles& operator+=(const Point<TranslationNumber, TranslationLabel>& translation) {
        outer_ += translation;
        for (auto& hole : holes_) {
            hole += translation;
        }
        resetCache();
        return *this;
    }

    /** @brief Translates the region in place by the opposite vector. */
    template <class TranslationNumber, class TranslationLabel>
    constexpr PolygonWithHoles& operator-=(const Point<TranslationNumber, TranslationLabel>& translation) {
        return *this += (-translation);
    }

    /**
     * @brief Scales the region in place.
     *
     * A negative factor reflects the rings, which reverses their orientation
     * and can change their relative order, so the holes are re-sorted.
     */
    template <class Scalar>
        requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
    constexpr PolygonWithHoles& operator*=(const Scalar& scalar) {
        outer_ *= scalar;
        for (auto& hole : holes_) {
            hole *= scalar;
        }
        normalize();
        return *this;
    }

    /** @copydoc operator*=(const Scalar&) */
    template <class Scalar>
        requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
    constexpr PolygonWithHoles& operator/=(const Scalar& scalar) {
        outer_ /= scalar;
        for (auto& hole : holes_) {
            hole /= scalar;
        }
        normalize();
        return *this;
    }

    /** @brief Returns the region rotated by 90k degrees around the origin. */
    [[nodiscard]] constexpr PolygonWithHoles rotated90(int k) const {
        PolygonWithHoles result;
        result.outer_ = outer_.rotated90(k);
        result.holes_.reserve(holes_.size());
        for (const auto& hole : holes_) {
            result.holes_.push_back(hole.rotated90(k));
        }
        result.normalize();
        return result;
    }

    /** @brief Rotates the region by 90k degrees around the origin in place. */
    constexpr void rotate90(int k) {
        auto saved = label_;
        *this = rotated90(k);
        label_ = std::move(saved);
    }

  private:
    /**
     * @brief Invokes @p predicate on every boundary edge — the outer ring first,
     *        then each hole — and stops at the first `true`.
     *
     * The edges are materialized lazily by @ref Polygon::edgesView, so a
     * predicate loop over ∂A allocates nothing.
     */
    template <class EdgePredicate>
    constexpr bool anyBoundaryEdge(EdgePredicate&& predicate) const {
        for (const auto& edge : outer_.edgesView()) {
            if (predicate(edge)) {
                return true;
            }
        }
        for (const auto& hole : holes_) {
            for (const auto& edge : hole.edgesView()) {
                if (predicate(edge)) {
                    return true;
                }
            }
        }
        return false;
    }

    /**
     * @brief Shared core of @ref interiorsIntersect against a segment, a ray,
     *        and a line.
     *
     * The three differ only in which points of the operand itself are split
     * points — both endpoints for a segment, the source for a ray, none for a
     * line — and in how the operand is parametrized. Everything after that is
     * common: collect the ring vertices on the operand, take any unpinched
     * boundary crossing as an answer, and classify the pieces between
     * consecutive split points by their midpoints.
     *
     * @param other The operand.
     * @param contacts The operand's own split points; ring vertices are added.
     * @param base A point of the operand, the origin of the parametrization.
     * @param direction The operand's direction vector.
     */
    template <class OtherLinear, class ContactNumber>
    constexpr bool linearInteriorsIntersect(const OtherLinear& other,
                                            std::vector<Point<ContactNumber>> contacts,
                                            const Point<ContactNumber>& base,
                                            const Point<ContactNumber>& direction) const;

    /**
     * @brief Tests whether the region is two-dimensional at one of its vertices.
     *
     * True when the vertex is in the closure of the region interior, i.e. when
     * some region interior sits in every neighbourhood of it. This is what the
     * whole boundary looks like as long as the rings stay apart, and it fails
     * only where they touch: at the tip of a slit, and at a point where holes
     * close over the outer boundary, the region is locally one-dimensional.
     *
     * The test walks the boundary edges through the vertex, and for each one
     * looks at the stretch reaching from the vertex to the nearest ring vertex
     * beyond it. That stretch is covered by one ring edge or by two: one means
     * region interior on exactly one side of it, hence a two-dimensional
     * vertex; two means the region pinches shut along it. Multiplicity is read
     * off the doubled edges at the doubled midpoint, which keeps it exact and
     * division-free. A region without holes skips all of it: a lone simple ring
     * has region interior along all of itself.
     */
    constexpr bool isSolidVertex(const PointType& vertex) const;

    /**
     * @brief Tests whether the open outer polygon alone contains a bounded shape.
     *
     * The open outer polygon has a closed, connected, unbounded complement, so a
     * bounded shape lies strictly inside it exactly when its boundary does —
     * which is what @ref Polygon::interiorContains already answers directly for
     * every operand it knows about. A region with holes is not one of those
     * (that direction is a later phase), so its boundary is handed over edge by
     * edge instead.
     */
    template <class OtherArea>
    constexpr bool outerInteriorContains(const OtherArea& other) const;

    /**
     * @brief Shared core of the five predicates against a bounded shape with
     *        area — a rectangle, a triangle, a convex polygon, a simple
     *        polygon, or another region with holes.
     *
     * The operand is closed and connected in every case, and when it has area
     * it is the closure of its own interior; those are the only properties the
     * rewritings below use, which is why one implementation serves all five.
     */
    template <class OtherArea>
    constexpr bool areaContains(const OtherArea& other) const;

    /** @copydoc areaContains */
    template <class OtherArea>
    constexpr bool areaInteriorContains(const OtherArea& other) const;

    /** @copydoc areaContains */
    template <class OtherArea>
    constexpr bool areaBoundaryContains(const OtherArea& other) const;

    /** @copydoc areaContains */
    template <class OtherArea>
    constexpr bool areaIntersects(const OtherArea& other) const;

    /** @copydoc areaContains */
    template <class OtherArea>
    bool areaInteriorsIntersect(const OtherArea& other) const;

    /**
     * @brief Shared core of the four set-level predicates against a polygonal
     *        chain — a monotone chain or a polyline.
     *
     * A chain is the union of its edges, so each relation holds for the chain
     * exactly when it holds for every edge (for @ref intersects, for some edge).
     * @p relation is called with each edge in turn and its results combined by
     * @p all: `true` to require every edge, `false` to accept any.
     */
    template <class OtherChain, class EdgeRelation>
    constexpr bool chainRelation(const OtherChain& other, bool all, EdgeRelation&& relation) const;

    /**
     * @brief The operand as a convex polygon, for the predicates that need a
     *        bounded, non-degenerate half-plane intersection as an area operand.
     *
     * Its vertices are intersections of constraint lines and therefore rational,
     * so the conversion picks a coordinate type exact enough to hold them.
     */
    template <class OtherIntersection>
    static constexpr auto asConvexOperand(const OtherIntersection& other);

    /**
     * @brief Applies @p relation to the shape a degenerate half-plane
     *        intersection has collapsed to.
     *
     * The carrier is a point, a segment, a ray or a line, and the region has an
     * overload for each; the two unbounded ones answer the containment
     * predicates false by themselves, so the visit needs no case analysis.
     */
    template <class OtherIntersection, class Relation>
    constexpr bool degenerateIntersectionRelation(const OtherIntersection& other,
                                                  Relation&& relation) const;

    /**
     * @brief Smallest squared distance from a boundary edge to a disjoint shape.
     *
     * Used when the region does not intersect @p other and its closest point
     * therefore lies on ∂A. Requires the edge segment to support
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

    PolygonType outer_{};
    std::vector<PolygonType> holes_{};
    [[no_unique_address]] mutable LabelType label_{};

    // Memoized hash, computed lazily by std::hash<PolygonWithHoles>, with the
    // same sentinel scheme as Polygon: hashUnset_ means "not yet computed", and
    // the one true hash colliding with it is remapped so the sentinel is never
    // stored as a real value.
    static constexpr std::size_t hashUnset_ = pgl::detail::numeric_limits<std::size_t>::max();
    mutable std::size_t hash_ = hashUnset_;
    friend struct std::hash<PolygonWithHoles>;

    template <class OtherPointType, class OtherLabelType>
    friend struct PolygonWithHoles;

    constexpr void resetCache() const {
        hash_ = hashUnset_;
    }

    /**
     * @brief Brings the holes to canonical form: zero-area rings dropped, the
     * rest sorted by @ref Polygon::operator<=>.
     *
     * Each ring is already canonical on its own — @ref Polygon's constructor
     * and mutators guarantee that — so only the hole list needs work.
     */
    constexpr void normalize() {
        std::erase_if(holes_, [](const PolygonType& hole) { return hole.isDegenerate(); });
        std::sort(holes_.begin(), holes_.end());
        resetCache();
    }
};

template <class PointType, class LabelType, class TranslationNumber, class TranslationLabel>
constexpr auto operator+(const PolygonWithHoles<PointType, LabelType>& region,
                         const Point<TranslationNumber, TranslationLabel>& translation) {
    PolygonWithHoles<PointType, LabelType> result(region);
    result += translation;
    return result;
}

template <class PointType, class LabelType, class TranslationNumber, class TranslationLabel>
constexpr auto operator-(const PolygonWithHoles<PointType, LabelType>& region,
                         const Point<TranslationNumber, TranslationLabel>& translation) {
    return region + (-translation);
}

template <class PointType, class LabelType, class Scalar>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr auto operator*(const PolygonWithHoles<PointType, LabelType>& region, const Scalar& scalar) {
    using ResultPointType = Point<decltype(std::declval<PointType>().x() * scalar), typename PointType::LabelType>;
    PolygonWithHoles<ResultPointType, LabelType> result(region);
    result *= scalar;
    if constexpr (detail::has_label_v<LabelType>) {
        result.label() = LabelType{};
    }
    return result;
}

template <class Scalar, class PointType, class LabelType>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr auto operator*(const Scalar& scalar, const PolygonWithHoles<PointType, LabelType>& region) {
    return region * scalar;
}

template <class PointType, class LabelType, class Scalar>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr auto operator/(const PolygonWithHoles<PointType, LabelType>& region, const Scalar& scalar) {
    using ResultPointType = Point<decltype(std::declval<PointType>().x() / scalar), typename PointType::LabelType>;
    PolygonWithHoles<ResultPointType, LabelType> result(region);
    result /= scalar;
    if constexpr (detail::has_label_v<LabelType>) {
        result.label() = LabelType{};
    }
    return result;
}

template <class PointType, class LabelType>
std::ostream& operator<<(std::ostream& stream, const PolygonWithHoles<PointType, LabelType>& region);

}  // namespace pgl
