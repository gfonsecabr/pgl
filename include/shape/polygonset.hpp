#pragma once

#include "shape/polygonwithholes.hpp"

#include <algorithm>
#include <cassert>
#include <compare>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <optional>
#include <ostream>
#include <ranges>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>


namespace pgl {

namespace detail {

/**
 * @brief The shapes a @ref PolygonSet owns the predicate pair with.
 *
 * A set outranks every other shape (`shapeRank` 150), so every lower-ranked
 * shape's own rank-based forwarder sends the symmetric relations here and the
 * concrete overload is written once. This concept names exactly those operands:
 * the ranked shapes below a set, leaving out the empty shape and the runtime
 * @ref Shape wrapper, which each have overloads of their own.
 *
 * A set's predicates are stated uniformly over this whole family rather than
 * one operand at a time, because their correctness argument is uniform — see
 * the section note in @ref pgl::PolygonSet. The operands that genuinely differ
 * are split out inside those definitions by shape kind, not by overload.
 */
template <class T>
concept SetOperandConcept =
    shapeRank<std::remove_cvref_t<T>> >= 0 &&
    shapeRank<std::remove_cvref_t<T>> < shapeRank<PolygonSet<Point<>, NoLabel>> &&
    !is_empty_shape_v<T>;

/**
 * @brief The operands a @ref PolygonSet's boolean operations accept.
 *
 * Exactly what the cell engine takes: a bounded polygonal shape that is not
 * self-overlapping, which is exactly @ref PolygonalRegionConcept. Everything
 * else — a half-plane and a half-plane intersection, which may be unbounded — is
 * clipped to the set's bounding box first and reaches the engine as a convex
 * polygon, through its own overload.
 */
template <class T>
concept SetBooleanOperandConcept = PolygonalRegionConcept<T>;

/**
 * @brief The operands a @ref PolygonSet's Minkowski sum accepts.
 *
 * The boolean operands plus the three that carry no area — a `Segment`, an
 * `OrientedSegment` and the two chains — which a boolean operation has no use
 * for and a sum does: dragging a set along one sweeps out area exactly as
 * dragging it along a rectangle does. What is left out is what is left out
 * everywhere: an unbounded operand, whose sum with a set is unbounded and so
 * fits in no set, and a `Disk`, whose sum is round. A `Point` is not here
 * either — that sum is a translation and belongs to the single-shape overload.
 */
template <class T>
concept SetMinkowskiOperandConcept =
    SetBooleanOperandConcept<T> || is_segment_v<T> || is_oriented_segment_v<T> ||
    is_polyline_v<T> || is_monotone_chain_v<T>;

/**
 * @brief The operands a @ref PolygonSet can measure an L1 distance to.
 *
 * A set measures whatever its components measure, and no more, so the question
 * is asked of a component rather than answered by a list here. The one operand
 * this excludes is a @ref Disk: an L1 or L∞ distance to a circle is nowhere
 * defined in the library beyond the pair `Disk`-`Point`, and a set that declared
 * the overload anyway would offer the runtime @ref Shape wrapper a member whose
 * body does not compile.
 */
template <class ResultNumber, class Component, class Operand>
concept ComponentDistanceL1Concept =
    requires(const Component& component, const Operand& operand) {
        component.template distanceL1<ResultNumber>(operand);
    };

/** @brief The L∞ counterpart of @ref ComponentDistanceL1Concept. */
template <class ResultNumber, class Component, class Operand>
concept ComponentDistanceLInfConcept =
    requires(const Component& component, const Operand& operand) {
        component.template distanceLInf<ResultNumber>(operand);
    };

}  // namespace detail

template <class PointType = Point<>, class Label>
struct PolygonSet;

// Explicit deduction guides, rather than the implicit ones the constrained
// constructors below would otherwise synthesize: clang 18 mishandles the
// constraints on such a guide, and CI compiles with it.
PolygonSet() -> PolygonSet<Point<>, NoLabel>;

template <PolygonWithHolesConcept Component>
PolygonSet(Component&&) -> PolygonSet<typename std::remove_cvref_t<Component>::PointType, NoLabel>;

template <std::ranges::input_range ComponentRange>
    requires detail::is_polygon_with_holes_v<std::ranges::range_value_t<ComponentRange>>
PolygonSet(ComponentRange&&)
    -> PolygonSet<typename std::ranges::range_value_t<ComponentRange>::PointType, NoLabel>;

template <std::ranges::input_range ComponentRange>
    requires detail::is_polygon_with_holes_v<std::ranges::range_value_t<ComponentRange>>
PolygonSet(ComponentRange&&, bool)
    -> PolygonSet<typename std::ranges::range_value_t<ComponentRange>::PointType, NoLabel>;


/**
 * @brief A closed region of the plane given as a set of @ref PolygonWithHoles
 *        components with pairwise disjoint interiors.
 *
 * The point set is exactly
 *
 * ```
 * A = A_0 ∪ A_1 ∪ ...
 * ```
 *
 * — the union of the components, each of them a closed region in its own right.
 * This is the shape the regularized boolean operations produce: a difference, a
 * union or a symmetric difference of two regions can come apart into several
 * pieces, and an island stranded inside a hole of the answer is a piece like any
 * other. Having it as a shape rather than a `std::vector` is what makes those
 * operations **closed** — a result can be fed straight back in, compared,
 * hashed, drawn, transformed and measured.
 *
 * **Storage.** The components are ordinary @ref PolygonWithHoles values, each in
 * its own canonical form, kept sorted by `PolygonWithHoles::operator<=>`. So
 * equality, ordering and hashing do not depend on the order they were supplied
 * in. Components of zero area cover nothing that survives and are dropped, as
 * @ref PolygonWithHoles drops a zero-area hole, and duplicates are erased — two
 * equal components would violate the disjointness precondition anyway.
 *
 * Components are deliberately **not** nested: a component stranded inside
 * another's hole is stored beside it, not within it. That is what the cell
 * engine emits and what a flat list can say.
 *
 * **Preconditions.** As with @ref Polygon and @ref PolygonWithHoles, structural
 * validity is a documented precondition rather than an enforced invariant:
 *
 * - every component satisfies @ref PolygonWithHoles::isValid;
 * - component interiors are pairwise disjoint;
 * - no two components share a stretch of edge — they may meet only at finitely
 *   many points.
 *
 * The third clause is what buys the identity `A° = ⋃ Aᵢ°`, on which the
 * componentwise predicates rest: two squares glued along an edge would have
 * interior points lying in no component's interior. The cell engine already
 * produces boundaries that satisfy it, so the clause costs nothing.
 *
 * @tparam PointType_ The vertex point type.
 * @tparam TLabel Optional label payload.
 */
template <class PointType_, class TLabel>
struct PolygonSet {
    using PointType = PointType_;
    using NumberType = typename PointType::NumberType;
    using LabelType = TLabel;
    using ComponentType = PolygonWithHoles<PointType>;

    class VertexIterator;
    using PolygonType = Polygon<PointType>;
    using EdgeType = Segment<PointType>;
    static_assert(detail::is_point_v<PointType>, "PolygonSet requires pgl::Point vertices");

    /**
     * @brief Creates the empty set (no components).
     */
    constexpr PolygonSet() = default;

    /**
     * @brief Creates a set with a single component.
     *
     * A component of zero area covers nothing and leaves the set empty.
     *
     * @param component The only component.
     */
    constexpr explicit PolygonSet(ComponentType component) {
        if (!component.isDegenerate()) {
            components_.push_back(std::move(component));
        }
    }

    /**
     * @brief Creates a set from a range of components.
     *
     * Components of zero area are dropped, the rest are sorted into canonical
     * order and duplicates are erased.
     *
     * @tparam ComponentRange Range whose elements are regions.
     * @param components The components; their interiors must be pairwise
     *        disjoint and no two may share a stretch of edge (a precondition,
     *        see @ref isValid).
     * @param trusted When `true`, adopt @p components as given without dropping
     *        degenerate ones, sorting, or removing duplicates. Only pass `true`
     *        for a range that is already in canonical form — over rational
     *        coordinates the area test alone dominates the construction.
     */
    template <std::ranges::input_range ComponentRange>
        requires detail::is_polygon_with_holes_v<std::ranges::range_value_t<ComponentRange>>
    constexpr PolygonSet(ComponentRange&& components, bool trusted = false) {
        for (const auto& component : components) {
            components_.emplace_back(component);
        }
        if (!trusted) {
            normalize();
        }
    }

    /**
     * @brief Converts a set with compatible vertex type.
     *
     * The source components are already canonical and a coordinate-type
     * conversion preserves both their own normalization and their relative
     * order, so no renormalization is needed.
     *
     * @tparam OtherPointType Source vertex type.
     * @tparam OtherLabelType Source label type.
     * @param other Source set.
     */
    template <PointConcept OtherPointType, class OtherLabelType>
        requires(std::constructible_from<PointType, const OtherPointType&>)
    constexpr PolygonSet(const PolygonSet<OtherPointType, OtherLabelType>& other) {
        components_.reserve(other.componentCount());
        for (const auto& component : other.components()) {
            components_.emplace_back(component);
        }
    }

    /**
     * @brief Returns the set label.
     *
     * The label is mutable even through a const set: it is metadata that does
     * not participate in equality, hashing, or geometric predicates.
     *
     * @return Reference to the stored label.
     */
    template <class A = LabelType>
        requires(detail::has_label_v<A>)
    constexpr A& label() const {
        return label_;
    }

    // -------------------------------------------------------------------------
    // Component access
    //
    // Deliberately not `size()` / `operator[]`: `size()` counts defining points
    // on Polygon, Convex, Polyline and MonotoneChain, and a name whose meaning
    // differs per shape is a trap in generic code. @ref PolygonWithHoles made
    // the same call for its holes.

    /** @brief Returns the number of components. */
    [[nodiscard]] constexpr std::size_t componentCount() const {
        return components_.size();
    }

    /**
     * @brief Accesses a component by index.
     * @param index The index of the component, in canonical (sorted) order.
     */
    [[nodiscard]] constexpr const ComponentType& component(std::size_t index) const {
        assert(index < components_.size());
        return components_[index];
    }

    /** @brief Returns the components in canonical order. */
    [[nodiscard]] constexpr const std::vector<ComponentType>& components() const {
        return components_;
    }

    /** @brief Returns a constant iterator to the first component. */
    [[nodiscard]] constexpr auto begin() const { return components_.begin(); }

    /** @brief Returns a constant iterator to the first component. */
    [[nodiscard]] constexpr auto cbegin() const { return components_.cbegin(); }

    /** @brief Returns a constant iterator past the last component. */
    [[nodiscard]] constexpr auto end() const { return components_.end(); }

    /** @brief Returns a constant iterator past the last component. */
    [[nodiscard]] constexpr auto cend() const { return components_.cend(); }

    /**
     * @brief Adds a component, keeping the canonical order.
     *
     * A zero-area component covers nothing and is ignored, and one equal to a
     * component already present is ignored too.
     *
     * @param component The component to add; its interior must be disjoint from
     *        the existing components' and it must share no stretch of edge with
     *        them (a precondition, see @ref isValid).
     */
    constexpr void addComponent(ComponentType component) {
        if (component.isDegenerate()) {
            return;
        }
        const auto position = std::lower_bound(components_.begin(), components_.end(), component);
        if (position != components_.end() && *position == component) {
            return;
        }
        components_.insert(position, std::move(component));
        resetCache();
    }

    /**
     * @brief Erases the component at the given index.
     *
     * Dropping a component needs no revalidation: the ones that remain still
     * have pairwise disjoint interiors and still share no stretch of edge, and
     * erasing preserves both their sorted order and the absence of zero-area
     * components, so nothing is renormalized.
     *
     * @param index The index of the component, in canonical (sorted) order.
     */
    constexpr void eraseComponent(std::size_t index) {
        assert(index < components_.size());
        components_.erase(components_.begin() + static_cast<std::ptrdiff_t>(index));
        resetCache();
    }

    /**
     * @brief Erases the component equal to the given region, if the set has one.
     *
     * The components are sorted, so this finds it by binary search: O(log k)
     * comparisons for k components, plus the element moves the erase costs.
     *
     * @param component The component to erase.
     * @return `true` when a component was erased, `false` when the set has no
     *         component equal to @p component.
     */
    constexpr bool eraseComponent(const ComponentType& component) {
        const auto position = std::lower_bound(components_.begin(), components_.end(), component);
        if (position == components_.end() || !(*position == component)) {
            return false;
        }
        components_.erase(position);
        resetCache();
        return true;
    }

    /** @brief Returns the total number of holes over all components. */
    [[nodiscard]] constexpr std::size_t holeCount() const {
        std::size_t total = 0;
        for (const auto& component : components_) {
            total += component.holeCount();
        }
        return total;
    }

    /** @brief Tests whether any component has a hole. */
    [[nodiscard]] constexpr bool hasHoles() const {
        for (const auto& component : components_) {
            if (component.hasHoles()) {
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Returns the total number of vertices over every ring of every
     *        component.
     *
     * Same name and same meaning as @ref PolygonWithHoles::vertexCount, and
     * deliberately not `size()` for the same reason.
     */
    [[nodiscard]] constexpr std::size_t vertexCount() const {
        std::size_t total = 0;
        for (const auto& component : components_) {
            total += component.vertexCount();
        }
        return total;
    }

    /** @brief Returns the vertices of every ring of every component. */
    [[nodiscard]] constexpr std::vector<PointType> vertices() const {
        std::vector<PointType> result;
        result.reserve(vertexCount());
        for (const auto& component : components_) {
            for (const auto& vertex : component.vertices()) {
                result.push_back(vertex);
            }
        }
        return result;
    }

    /**
     * @brief Returns a lazy view over the vertices of every ring of every
     * component, without allocating a vector.
     *
     * Same vertex sequence as @ref vertices(), which materializes one vector
     * per component before copying into its result; this allocates nothing.
     * Unlike @ref begin(), which walks the *components*, this walks points.
     */
    [[nodiscard]] constexpr auto verticesView() const {
        return std::ranges::subrange(verticesBegin(), verticesEnd());
    }

    /** @brief Returns an iterator to the first vertex of the first component. */
    [[nodiscard]] constexpr VertexIterator verticesBegin() const {
        return VertexIterator(this, 0);
    }

    /** @brief Returns an iterator past the last vertex of the last component. */
    [[nodiscard]] constexpr VertexIterator verticesEnd() const {
        return VertexIterator(this, components_.size());
    }

    /** @brief Returns the boundary edges of every ring of every component. */
    [[nodiscard]] constexpr std::vector<EdgeType> edges() const {
        std::vector<EdgeType> result;
        result.reserve(vertexCount());
        for (const auto& component : components_) {
            for (const auto& edge : component.edges()) {
                result.push_back(edge);
            }
        }
        return result;
    }

    /**
     * @brief Returns the boundary edges directed so the set lies to the left.
     *
     * Each component contributes its own @ref PolygonWithHoles::orientedEdges:
     * outer rings counterclockwise, hole rings reversed.
     */
    [[nodiscard]] constexpr std::vector<OrientedSegment<PointType>> orientedEdges() const {
        std::vector<OrientedSegment<PointType>> result;
        result.reserve(vertexCount());
        for (const auto& component : components_) {
            for (const auto& edge : component.orientedEdges()) {
                result.push_back(edge);
            }
        }
        return result;
    }

    // -------------------------------------------------------------------------
    // Value semantics

    /** @brief Compares two sets by component count, then lexicographically. */
    [[nodiscard]] constexpr auto operator<=>(const PolygonSet& other) const {
        if (auto cmp = components_.size() <=> other.components_.size(); cmp != 0) {
            return cmp;
        }
        for (std::size_t i = 0; i < components_.size(); ++i) {
            if (auto cmp = components_[i] <=> other.components_[i]; cmp != 0) {
                return cmp;
            }
        }
        return std::strong_ordering::equal;
    }

    /** @brief Checks equality of two sets. */
    [[nodiscard]] constexpr bool operator==(const PolygonSet& other) const {
        if (components_.size() != other.components_.size()) {
            return false;
        }
        for (std::size_t i = 0; i < components_.size(); ++i) {
            if (!(components_[i] == other.components_[i])) {
                return false;
            }
        }
        return true;
    }

    /** @brief Tests whether another shape defines exactly the same point set. */
    template<AnyShapeConcept OtherShape>
    [[nodiscard]] constexpr bool samePointSet(const OtherShape& other) const;

    // -------------------------------------------------------------------------
    // State queries

    /** @brief Tests whether the set has no components at all. */
    [[nodiscard]] constexpr bool empty() const {
        return components_.empty();
    }

    /**
     * @brief Tests whether the set has zero area.
     *
     * A canonical set drops its zero-area components, so this is exactly
     * @ref empty for one; a set adopted with `trusted` may still carry a
     * component without area, which is why the areas are summed rather than the
     * components counted. The sum is taken in the promoted type: narrowed to
     * @ref NumberType it wraps to zero for a set larger than the coordinate
     * range and reports an ordinary set as degenerate.
     */
    [[nodiscard]] constexpr bool isDegenerate() const {
        using Exact = detail::promoted_number_t<NumberType>;
        return twiceArea<Exact>() == Exact(0);
    }

    /**
     * @brief Tests whether the set covers exactly one point.
     *
     * Only a single component can, and only a `trusted` construction can leave
     * one that does.
     */
    [[nodiscard]] constexpr bool isPoint() const {
        return components_.size() == 1 && components_[0].isPoint();
    }

    /** @brief Tests whether the set covers exactly one segment of positive length. */
    [[nodiscard]] constexpr bool isSegment() const {
        return components_.size() == 1 && components_[0].isSegment();
    }

    /**
     * @brief Tests whether the set is degenerate without covering a point or a
     *        segment (which includes the empty set).
     */
    [[nodiscard]] constexpr bool isUndefined() const {
        return !isPoint() && !isSegment() && isDegenerate();
    }

    /**
     * @brief Tests whether every ring of every component is simple.
     *
     * This is a per-ring check only; it says nothing about how the rings or the
     * components sit relative to one another. Use @ref isValid for the
     * structural contract.
     *
     * Complexity: O(n log n) over the total vertex count.
     */
    template <class Rational = pgl::Rational<pgl::BigInt>>
    [[nodiscard]] bool isSimple() const {
        for (const auto& component : components_) {
            if (!component.template isSimple<Rational>()) {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief Tests the structural contract: every component valid, component
     *        interiors pairwise disjoint, no two components sharing a stretch of
     *        edge.
     *
     * The third clause is what the componentwise predicates rest on: it is what
     * makes `A° = ⋃ Aᵢ°`, and hence what lets a question about the set be
     * answered one component at a time. Two components glued along an edge have
     * interior points that lie in no component's interior, and the identity
     * fails.
     *
     * This is a precondition of every other operation, checked on demand rather
     * than enforced by the constructor — mirroring @ref Polygon and
     * @ref PolygonWithHoles, which likewise leave their contracts to the caller.
     *
     * Complexity: the components' own @ref PolygonWithHoles::isValid, plus one
     * interior-overlap test and one edge-overlap scan per pair of components
     * whose bounding boxes meet.
     */
    template <class Rational = pgl::Rational<pgl::BigInt>>
    [[nodiscard]] bool isValid() const;

    /**
     * @brief Tests whether the set is the closure of its own interior
     *        (`A = closure(A°)`).
     *
     * A valid set is `⋃ Aᵢ` with the components meeting at finitely many points
     * at most, so the only material with no area beside it is a component's own
     * **slit** — see @ref PolygonWithHoles::isRegular. The set is therefore
     * regular exactly when every component is.
     *
     * The empty set is regular (`∅ = closure(∅°)`).
     *
     * The set must satisfy @ref isValid.
     *
     * Complexity: O(n²) over the total vertex count.
     */
    [[nodiscard]] bool isRegular() const {
        for (const auto& component : components_) {
            if (!component.isRegular()) {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief Returns the set without its slits (`closure(A°)`).
     *
     * This is @ref PolygonWithHoles::regularized applied to every component and
     * the pieces gathered back into a set — which is exactly where a set of
     * regions starts paying for itself: the result has the type it was called
     * on, so regularization is idempotent in the type system and not only in
     * the mathematics. Dropping a component's slits can break it into several
     * components, and a component without area comes back as nothing at all.
     *
     * The set must satisfy @ref isValid.
     *
     * @tparam ResultNumber The number type for the result.
     * @return `closure(A°)`, in canonical order.
     */
    template <class ResultNumber = division_result_t<NumberType>>
    [[nodiscard]] PolygonSet<Point<ResultNumber, typename PointType::LabelType>> regularized() const;

    // -------------------------------------------------------------------------
    // Measures

    /**
     * @brief Computes twice the area of the set.
     *
     * `Σ 2·area(Aᵢ)`, exact in @ref NumberType with no division. The components
     * have pairwise disjoint interiors, so the sum is the area of their union.
     *
     * @tparam ResultNumber Type the components are measured and summed in,
     *         @ref NumberType by default. Twice an area is a sum of coordinate
     *         products, so a set larger than the coordinate range wraps; pass a
     *         wider type to measure such a set.
     */
    template <class ResultNumber = NumberType>
    [[nodiscard]] constexpr ResultNumber twiceArea() const {
        ResultNumber total{};
        for (const auto& component : components_) {
            total += component.template twiceArea<ResultNumber>();
        }
        return total;
    }

    /**
     * @brief Computes the area of the set.
     * @warning Uses division by 2.
     */
    template <class ResultNumber = division_result_t<NumberType>>
    [[nodiscard]] constexpr auto area() const {
        ResultNumber result = static_cast<ResultNumber>(twiceArea());
        return result / ResultNumber(2);
    }

    /**
     * @brief Computes the area-weighted centroid of the set.
     *
     * The components enter with their own areas as weights. When the total area
     * is zero the set has no area-weighted centroid and the centroid of the
     * vertex set is returned instead, matching @ref Polygon::centroid and
     * @ref PolygonWithHoles::centroid.
     *
     * @tparam ResultNumber The number type for the result.
     * @warning Divides coordinates after casting to @p ResultNumber.
     */
    template <class ResultNumber = division_result_t<NumberType>>
    [[nodiscard]] constexpr Point<ResultNumber> centroid() const;

    /** @brief Computes the centroid of the vertex set over every ring of every component. */
    template <class ResultNumber = division_result_t<NumberType>>
    [[nodiscard]] constexpr Point<ResultNumber> verticesCentroid() const;

    /**
     * @brief Returns a point strictly inside the set.
     *
     * Any component's interior is in the set's, so this is the first component's
     * own @ref PolygonWithHoles::pointInside.
     *
     * @tparam ResultNumber The number type for the result.
     * @return A point guaranteed to be inside the set.
     * @warning Divides coordinates by 4 (see @ref Triangle::pointInside), so it
     *          is inexact for integer coordinates not divisible by it. Undefined
     *          for a set with no area.
     */
    template <class ResultNumber = division_result_t<NumberType>>
    [[nodiscard]] Point<ResultNumber> pointInside() const;

    /**
     * @brief Returns a segment realizing the diameter (the farthest vertex pair).
     *
     * Unlike @ref PolygonWithHoles::diameter, this cannot be delegated to any
     * one component: the farthest pair generally has its two ends in different
     * components. Every hole lies inside its own component's outer ring, though,
     * so only the outer rings can carry the pair, and the diameter is that of
     * their convex hull.
     */
    [[nodiscard]] constexpr Segment<PointType> diameter() const {
        std::vector<PointType> hullPoints;
        hullPoints.reserve(vertexCount());
        for (const auto& component : components_) {
            for (const auto& vertex : component.outer()) {
                hullPoints.push_back(vertex);
            }
        }
        return Convex<PointType>(std::move(hullPoints)).diameter();
    }

    /**
     * @brief Returns the convex hull of the set's vertices.
     *
     * Every hole lies inside its own component's outer ring, so only the outer
     * rings can contribute a hull vertex.
     */
    [[nodiscard]] constexpr Convex<PointType> convexHull() const {
        std::vector<PointType> hullPoints;
        hullPoints.reserve(vertexCount());
        for (const auto& component : components_) {
            for (const auto& vertex : component.outer()) {
                hullPoints.push_back(vertex);
            }
        }
        return Convex<PointType>(std::move(hullPoints));
    }

    /**
     * @brief Computes the bounding box of the set.
     *
     * The union of the components' boxes, cached — unlike a region, which
     * delegates to the box its outer ring already caches.
     */
    [[nodiscard]] constexpr const Rectangle<PointType>& bbox() const;

    /** @brief Computes the floating-point bounding box of the set. */
    template <std::floating_point ResultNumber = double>
    [[nodiscard]] constexpr Rectangle<Point<ResultNumber>> fbox() const;

    // -------------------------------------------------------------------------
    // Decompositions

    /**
     * @brief Builds the constrained Delaunay triangulation of this set.
     *
     * Equivalent to `Triangulation(*this)`. Every ring of every component
     * becomes constrained edges; the hole interiors and the gaps between
     * components are left out of the domain, so the in-domain triangles cover
     * exactly the part of the set that has area. The set must satisfy
     * @ref isValid.
     *
     * @return A @ref Triangulation whose in-domain triangles cover the set.
     */
    auto triangulation() const;

    /**
     * @brief Builds the constrained Delaunay triangulation of this set with the
     *        given interior constraint segments.
     *
     * Equivalent to `Triangulation(*this, segments)`.
     *
     * @tparam SegmentRange Range whose elements are segments.
     * @param segments Constraint edges, assumed to lie in the set.
     */
    template <class SegmentRange>
    auto triangulation(const SegmentRange& segments) const;

    /**
     * @brief Cuts this set into convex pieces with disjoint interiors.
     *
     * Equivalent to `triangulation().convexPartition()`, with the same
     * precondition (@ref isValid). What the pieces cover is the part of the set
     * that has **area** — `closure(interior)` — so the holes and the gaps
     * between components are where there is no piece, and a slit appears in none
     * of them.
     *
     * @return The convex pieces, in canonical order.
     */
    [[nodiscard]] std::vector<Convex<PointType>> convexPartition() const;

    /**
     * @brief Covers this set with a greedily selected set of convex polygons.
     *
     * Equivalent to `triangulation().convexCovering()`, with the same
     * precondition (@ref isValid). Every piece is contained in this set and
     * together they cover `closure(interior)`. Unlike @ref convexPartition,
     * piece interiors may overlap. The returned cover is irredundant, not
     * necessarily minimum.
     *
     * @return The convex covering, in canonical order.
     */
    [[nodiscard]] std::vector<Convex<PointType>> convexCovering() const;

    /**
     * @brief Rasterizes this set into a @ref BitMatrix, one bit per covered cell.
     *
     * Equivalent to `BitMatrix(*this)`: the window is the bounding box of the
     * whole set and the set cells are the ones its components cover, holes left
     * unset. Only a rectilinear set is exactly a set of grid cells, so every
     * edge of every ring must be axis-parallel; use @ref innerRaster or
     * @ref outerRaster to approximate any other set.
     *
     * A cell is an integer position, so the coordinates must be whole numbers
     * too. An integer set rasterizes as it stands; one over a Rational or a
     * floating-point type is checked vertex by vertex and throws unless every
     * coordinate happens to be whole -- rounding one would move the set.
     *
     * Two components that touch only at a corner stay apart, since the cells do
     * too and @ref BitMatrix::asPolygonSet splits on edge connectivity, so the
     * round trip through the raster recovers a rectilinear set.
     *
     * @tparam ResultNumber Integer coordinate type of the grid (default: the
     *         coordinate type itself when it is a signed integer, the integer a
     *         Rational is built on, and `int64_t` otherwise).
     * @return A @ref BitMatrix over the bounding box, covering this set.
     * @throws std::logic_error If an edge of a ring is not axis-parallel, or a
     *         coordinate is not a whole number the grid can hold.
     */
    template <class ResultNumber = grid_number_t<typename PointType_::NumberType>>
        requires(std::signed_integral<ResultNumber>)
    [[nodiscard]] auto asBitMatrix() const;

    // -------------------------------------------------------------------------
    // Boolean operations
    //
    // This is what the shape is for. `Polygon` and `PolygonWithHoles` already
    // produce a set of regions from a difference, a union or a symmetric
    // difference, and now say so in their return type; a set does the same and
    // takes one back, so the four operations are **closed** and a result can be
    // fed straight into the next one.
    //
    // The engine does not care that a receiver is a set: it is the arrangement
    // of both operands' boundaries with one witness test per cell, and a set
    // contributes its components' rings the way a region contributes its own.
    // In particular the operands go in together rather than being folded over
    // one component at a time, so one arrangement settles the whole answer.

    /**
     * @brief Returns the regularized set difference of the two shapes (A ∖ B).
     *
     * The result is `closure(A° ∖ B)`: the part of this set with area that
     * survives the removal. See @ref Polygon::difference for the full contract.
     *
     * @tparam ResultNumber The number type for the result.
     * @param other The shape to remove.
     * @return The difference, in canonical order.
     */
    template <class ResultNumber = division_result_t<NumberType>, detail::SetBooleanOperandConcept OtherShape>
    [[nodiscard]] PolygonSet<Point<ResultNumber, typename PointType::LabelType>>
    difference(const OtherShape& other) const;

    /**
     * @brief Returns the regularized set difference of the two shapes (A ∖ B).
     *
     * A half-plane intersection may be unbounded, which stops it being a
     * @ref regularizedUnion operand but not a subtrahend: `A ∖ B` is bounded whenever
     * `A` is, however far `B` reaches, so a set of regions can hold it. See
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
     * The result is `closure(A° ∪ B°)`. A union can close a new hole into being
     * where the operands wrap round between them, and can join two components
     * that were apart into one. See @ref Polygon::regularizedUnion for the full
     * contract.
     */
    template <class ResultNumber = division_result_t<NumberType>, detail::SetBooleanOperandConcept OtherShape>
    [[nodiscard]] PolygonSet<Point<ResultNumber, typename PointType::LabelType>>
    regularizedUnion(const OtherShape& other) const;

    /**
     * @brief Returns the regularized intersection of the two shapes (A ∩ B).
     *
     * The result is `closure(A° ∩ B°)`: the part both operands cover, with
     * lower-dimensional leftovers dropped. Like
     * @ref PolygonWithHoles::regularizedIntersection(const OtherPolygon&) const, this keeps
     * the holes of a holed operand, and it can split one component into several.
     */
    template <class ResultNumber = division_result_t<NumberType>, detail::SetBooleanOperandConcept OtherShape>
    [[nodiscard]] PolygonSet<Point<ResultNumber, typename PointType::LabelType>>
    regularizedIntersection(const OtherShape& other) const;

    /**
     * @brief Returns the regularized symmetric difference of the two shapes (A △ B).
     *
     * The result is `closure((A° ∖ B) ∪ (B° ∖ A))`: the part covered by exactly
     * one of the two operands.
     */
    template <class ResultNumber = division_result_t<NumberType>, detail::SetBooleanOperandConcept OtherShape>
    [[nodiscard]] PolygonSet<Point<ResultNumber, typename PointType::LabelType>>
    symmetricDifference(const OtherShape& other) const;

    /**
     * @brief Returns the regularized intersection of the two shapes (A ∩ B).
     *
     * A half-plane and a half-plane intersection may be unbounded, but this set
     * is not, so the operand is first clipped to a box strictly containing the
     * bounding rectangle — which leaves `A ∩ B` untouched and makes it a convex
     * polygon. One with empty interior contributes nothing to a regularized
     * result.
     */
    template <class ResultNumber = division_result_t<NumberType>, HalfplaneIntersectionConcept OtherIntersection>
    [[nodiscard]] PolygonSet<Point<ResultNumber, typename PointType::LabelType>>
    regularizedIntersection(const OtherIntersection& other) const;

    /** @copydoc regularizedIntersection(const OtherIntersection&) const */
    template <class ResultNumber = division_result_t<NumberType>, HalfplaneConcept OtherHalfplane>
    [[nodiscard]] PolygonSet<Point<ResultNumber, typename PointType::LabelType>>
    regularizedIntersection(const OtherHalfplane& other) const;

    /**
     * @brief Returns the intersection of the two shapes (A ∩ B), empty when they
     *        are disjoint.
     *
     * The literal point set `A ∩ B`, as its connected pieces — the regions
     * @ref regularizedIntersection(const OtherShape&) const returns plus the
     * lower-dimensional material it drops. See
     * @ref PolygonWithHoles::intersection(const OtherPolygon&) const for the full
     * contract; a set reaches the same engine as any other operand, and goes in
     * whole rather than one component at a time.
     *
     * @tparam ResultNumber The number type for the result.
     * @param other The shape to intersect with.
     * @return The pieces of the intersection: isolated points first, then
     *         strands, then the areas in canonical order.
     */
    template <class ResultNumber = division_result_t<NumberType>, detail::SetBooleanOperandConcept OtherShape>
    [[nodiscard]] std::vector<std::variant<Point<ResultNumber, typename PointType::LabelType>,
                                           Polyline<Point<ResultNumber, typename PointType::LabelType>>,
                                           PolygonWithHoles<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherShape& other) const;

    /**
     * @brief Returns the intersection of the two shapes (A ∩ B), empty when they
     *        are disjoint.
     *
     * A half-plane and a half-plane intersection may be unbounded, but this set
     * is not, so the operand is first clipped to a box strictly containing the
     * bounding rectangle — which leaves `A ∩ B` untouched and makes it a convex
     * polygon. Unlike
     * @ref regularizedIntersection(const OtherIntersection&) const, a clip with
     * empty interior is not the end of it: the set can still meet the carrier it
     * collapsed to in points and segments.
     */
    template <class ResultNumber = division_result_t<NumberType>, HalfplaneIntersectionConcept OtherIntersection>
    [[nodiscard]] std::vector<std::variant<Point<ResultNumber, typename PointType::LabelType>,
                                           Polyline<Point<ResultNumber, typename PointType::LabelType>>,
                                           PolygonWithHoles<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherIntersection& other) const;

    /** @copydoc intersection(const OtherIntersection&) const */
    template <class ResultNumber = division_result_t<NumberType>, HalfplaneConcept OtherHalfplane>
    [[nodiscard]] std::vector<std::variant<Point<ResultNumber, typename PointType::LabelType>,
                                           Polyline<Point<ResultNumber, typename PointType::LabelType>>,
                                           PolygonWithHoles<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherHalfplane& other) const;

    /** @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint. */
    template <class ResultNumber = NumberType, class EmptyPoint>
    [[nodiscard]] constexpr EmptyShape<EmptyPoint> intersection(const EmptyShape<EmptyPoint>&) const {
        return {};
    }

    // -------------------------------------------------------------------------
    // Predicates
    //
    // With `A = ⋃ Aᵢ`, interiors pairwise disjoint and contacts finite, four of
    // the five relations fold over the components outright:
    //
    //   intersects(x)          ∃i Aᵢ.intersects(x)          — A is the union
    //   interiorsIntersect(x)  ∃i Aᵢ.interiorsIntersect(x)   — A° = ⋃ Aᵢ°
    //   interiorContains(x)    ∃i Aᵢ.interiorContains(x)     — see below
    //   contains(point)        ∃i Aᵢ.contains(point)
    //
    // `A° = ⋃ Aᵢ°` is what the no-shared-edge clause of @ref isValid buys, and
    // it settles `interiorContains` for every operand: the `Aᵢ°` are open and
    // pairwise disjoint, so a connected operand inside their union is inside one
    // of them, and every shape but another set is connected.
    //
    // `contains` and `boundaryContains` are the two that do not fold, and they
    // fail for exactly one kind of operand. `∃i` is still necessary and
    // sufficient whenever the operand stays connected after finitely many points
    // are removed — every operand with area, and every point — because the
    // components meet at finitely many points at most. A **one-dimensional**
    // operand does not: two unit squares meeting corner to corner at the origin
    // contain the segment from (-1,-1) to (1,1) between them and neither
    // contains it alone. Those operands are settled by splitting them at every
    // component-boundary contact and classifying each piece, which is exact and
    // costs nothing when the components do not touch at all (@ref isPinched).

    /**
     * @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅).
     *
     * The set is the union of its components, so it meets a shape exactly when
     * one of them does — exact for every operand.
     */
    template <detail::SetOperandConcept OtherShape>
    [[nodiscard]] bool intersects(const OtherShape& other) const;

    /**
     * @brief Tests whether the interiors of the shapes intersect (A° ∩ B° ≠ ∅).
     *
     * `A° = ⋃ Aᵢ°` for a valid set, so this too is exact componentwise.
     */
    template <detail::SetOperandConcept OtherShape>
    [[nodiscard]] bool interiorsIntersect(const OtherShape& other) const;

    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     *
     * Componentwise for every operand but a one-dimensional one, which may run
     * from one component into another through a point where they touch; see the
     * section note above.
     *
     * Complexity: O(n) over the total vertex count for the componentwise answer,
     * plus one split of the operand against every component boundary when the
     * components touch and no single one contains it.
     */
    template <detail::SetOperandConcept OtherShape>
    [[nodiscard]] bool contains(const OtherShape& other) const;

    /**
     * @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B).
     *
     * The component interiors are open and pairwise disjoint, so a connected
     * operand inside the set's interior is inside one component's — exact
     * componentwise for every operand.
     */
    template <detail::SetOperandConcept OtherShape>
    [[nodiscard]] bool interiorContains(const OtherShape& other) const;

    /**
     * @brief Tests whether this shape's interior contains the segment's interior.
     *
     * Every point of the open segment must lie strictly inside one component;
     * either endpoint may lie on the set boundary. A degenerate segment is
     * accepted exactly when its sole point is contained.
     */
    template <SegmentConcept OtherSegment>
    [[nodiscard]] bool interiorContainsInterior(const OtherSegment& other) const;

    /**
     * @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B).
     *
     * `∂A = ⋃ ∂Aᵢ`, which has no area, so only an operand that has collapsed can
     * lie on it. Like @ref contains, a one-dimensional one may run from one
     * component's boundary onto another's.
     */
    template <detail::SetOperandConcept OtherShape>
    [[nodiscard]] bool boundaryContains(const OtherShape& other) const;

    /**
     * @brief Tests whether removing this shape disconnects the other shape
     *        (B∖A is disconnected).
     *
     * Not componentwise in either direction: removing one component may leave
     * the operand in one piece while removing them all cuts it. This goes to the
     * cell engine of implementation/separates.hpp, which assumes nothing about
     * either operand, with the unbounded ones clipped to a box holding the set
     * first.
     */
    template <detail::SetOperandConcept OtherShape>
    [[nodiscard]] bool separates(const OtherShape& other) const;

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template <detail::SetOperandConcept OtherShape>
    [[nodiscard]] bool crosses(const OtherShape& other) const;

    // -------------------------------------------------------------------------
    // The self pair. A set operand is the one operand that need not be
    // connected, so the relations that lean on the operand's connectedness fold
    // over *its* components instead of assuming it lies in one piece.

    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template <PolygonSetConcept OtherSet>
    [[nodiscard]] bool intersects(const OtherSet& other) const;

    /** @brief Tests whether the interiors of the shapes intersect (A° ∩ B° ≠ ∅). */
    template <PolygonSetConcept OtherSet>
    [[nodiscard]] bool interiorsIntersect(const OtherSet& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template <PolygonSetConcept OtherSet>
    [[nodiscard]] bool contains(const OtherSet& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template <PolygonSetConcept OtherSet>
    [[nodiscard]] bool interiorContains(const OtherSet& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template <PolygonSetConcept OtherSet>
    [[nodiscard]] bool boundaryContains(const OtherSet& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template <PolygonSetConcept OtherSet>
    [[nodiscard]] bool separates(const OtherSet& other) const;

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template <PolygonSetConcept OtherSet>
    [[nodiscard]] bool crosses(const OtherSet& other) const;

    // -------------------------------------------------------------------------
    // The empty set is a subset of every shape, so its containment relations are
    // true and its intersection relations are false.

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
    // Runtime Shape argument: visit the wrapped alternative and re-dispatch to
    // the matching per-shape overload (defined in the implementation layer).

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template <PointConcept OtherPoint>
    [[nodiscard]] bool contains(const Shape<OtherPoint>& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template <PointConcept OtherPoint>
    [[nodiscard]] bool interiorContains(const Shape<OtherPoint>& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template <PointConcept OtherPoint>
    [[nodiscard]] bool boundaryContains(const Shape<OtherPoint>& other) const;

    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template <PointConcept OtherPoint>
    [[nodiscard]] bool intersects(const Shape<OtherPoint>& other) const;

    /** @brief Tests whether the interiors of the shapes intersect (A° ∩ B° ≠ ∅). */
    template <PointConcept OtherPoint>
    [[nodiscard]] bool interiorsIntersect(const Shape<OtherPoint>& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template <PointConcept OtherPoint>
    [[nodiscard]] bool separates(const Shape<OtherPoint>& other) const;

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template <PointConcept OtherPoint>
    [[nodiscard]] bool crosses(const Shape<OtherPoint>& other) const;

    // -------------------------------------------------------------------------
    // Distances
    //
    // The distance to a union is the minimum of the distances, with no caveat at
    // all: every one of these is exactly the minimum over the components.

    /**
     * @brief Computes the squared Euclidean distance to the other shape.
     *
     * Zero when the shapes intersect; otherwise the smallest squared distance
     * between them, which is the smallest over the components. An empty set is
     * infinitely far from everything and reports zero, having no component to
     * measure from.
     *
     * @tparam ResultNumber Coordinate type of the returned distance (default: @ref division_result_t).
     *
     * @warning With an integer @p ResultNumber the exact squared distance is
     *          generally a fraction, so the internal division truncates and the
     *          result is inexact. Request a floating-point or pgl::Rational
     *          result type for an accurate value.
     */
    template <class ResultNumber = division_result_t<NumberType>, detail::SetOperandConcept OtherShape>
    [[nodiscard]] auto squaredDistance(const OtherShape& other) const;

    /** @copydoc squaredDistance(const OtherShape&) const */
    template <class ResultNumber = division_result_t<NumberType>, PolygonSetConcept OtherSet>
    [[nodiscard]] auto squaredDistance(const OtherSet& other) const;

    /**
     * @copydoc squaredDistance(const OtherShape&) const
     *
     * A set measures whatever its components measure, and no more; see
     * @ref detail::ComponentDistanceL1Concept for the one operand that leaves
     * out.
     */
    template <class ResultNumber = division_result_t<NumberType>, detail::SetOperandConcept OtherShape>
        requires detail::ComponentDistanceL1Concept<ResultNumber, PolygonWithHoles<PointType_>, OtherShape>
    [[nodiscard]] auto distanceL1(const OtherShape& other) const;

    /** @copydoc squaredDistance(const OtherShape&) const */
    template <class ResultNumber = division_result_t<NumberType>, PolygonSetConcept OtherSet>
    [[nodiscard]] auto distanceL1(const OtherSet& other) const;

    /** @copydoc distanceL1(const OtherShape&) const */
    template <class ResultNumber = division_result_t<NumberType>, detail::SetOperandConcept OtherShape>
        requires detail::ComponentDistanceLInfConcept<ResultNumber, PolygonWithHoles<PointType_>, OtherShape>
    [[nodiscard]] auto distanceLInf(const OtherShape& other) const;

    /** @copydoc squaredDistance(const OtherShape&) const */
    template <class ResultNumber = division_result_t<NumberType>, PolygonSetConcept OtherSet>
    [[nodiscard]] auto distanceLInf(const OtherSet& other) const;

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
     * in this shape either way. See @ref Polygon::difference for the contract.
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
     * unsupported. See @ref Polygon::symmetricDifference for the contract.
     *
     * The point type is deduced from @p other so a plain concrete shape cannot
     * reach this overload through an implicit conversion to `Shape`.
     */
    template <class ResultNumber = division_result_t<NumberType>, PointConcept OtherPoint>
    [[nodiscard]] auto symmetricDifference(const Shape<OtherPoint>& other) const {
        return other.template symmetricDifference<ResultNumber>(*this);
    }

    /**
     * @brief Returns the Manhattan (L1) distance to the given shape, using
     * symmetry to re-dispatch through the wrapper's own `distanceL1`.
     */
    template <class ResultNumber = double, PointConcept OtherPoint>
    [[nodiscard]] constexpr auto distanceL1(const Shape<OtherPoint>& other) const {
        return other.template distanceL1<ResultNumber>(*this);
    }

    /**
     * @brief Returns the Chebyshev (L∞) distance to the given shape, using
     * symmetry to re-dispatch through the wrapper's own `distanceLInf`.
     */
    template <class ResultNumber = double, PointConcept OtherPoint>
    [[nodiscard]] constexpr auto distanceLInf(const Shape<OtherPoint>& other) const {
        return other.template distanceLInf<ResultNumber>(*this);
    }

    /**
     * @brief Tests whether two components touch each other anywhere.
     *
     * `false` is the cheap exact case: components that stay apart make the set a
     * disjoint union of closed sets at positive distance, and then every
     * relation folds componentwise exactly, one-dimensional operands included.
     * Memoized, since the whole point of it is to be asked before the general
     * machinery is.
     *
     * Complexity: one bounding-box test per component pair, plus one
     * @ref PolygonWithHoles::intersects per pair whose boxes meet.
     */
    [[nodiscard]] bool isPinched() const;

    /**
     * @brief Tests whether the set is connected as a point set.
     *
     * A set of regions is the library's first shape that need not be: two
     * components that never touch are two pieces. Each component is connected on
     * its own (@ref PolygonWithHoles is, however its rings meet), so the set is
     * connected exactly when the graph joining components that intersect is —
     * and the empty set is connected by convention, having nothing to come apart.
     *
     * This is what the cut predicates ask before dismissing a remover that
     * misses the set: `B ∖ A` is disconnected whenever `B` already was.
     *
     * Complexity: one @ref PolygonWithHoles::intersects per component pair whose
     * bounding boxes meet.
     */
    [[nodiscard]] bool isConnected() const;

    /**
     * @brief Returns the Minkowski sum of this shape and another (A ⊕ B).
     *
     * The sum is the point set `{a + b : a ∈ A, b ∈ B}`. Summing with a `Point`
     * is a translation, so it gives back a set over the promoted coordinate
     * type — this is the reading `set + point` has always had. A set of regions
     * is not convex, so @ref MinkowskiSummableConcept admits nothing else here;
     * a `Halfplane` operand, which absorbs the set and gives a half-plane, is
     * the one exception it does admit.
     *
     * @tparam OtherShape Type of the other shape.
     * @param other Shape to sum with.
     * @return The Minkowski sum, in the tightest type that represents it.
     */
    template <class OtherShape>
        requires MinkowskiSummableConcept<PolygonSet<PointType_, TLabel>, OtherShape>
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
     * A set eroded by a bounded operand is again a set.
     *
     * Eroding by a shape that covers no point is the whole plane, which a
     * @ref HalfplaneIntersection returns and the tighter result types cannot.
     *
     * @tparam OtherShape Type of the shape to erode by.
     * @param other Shape to erode by.
     * @return The erosion, in the tightest type that represents it.
     */
    template <class OtherShape>
        requires MinkowskiSummableConcept<PolygonSet<PointType_, TLabel>, OtherShape>
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
        requires (!MinkowskiSummableConcept<PolygonSet<PointType_, TLabel>, OtherShape> &&
                  BoundedPolygonalConcept<OtherShape>)
    [[nodiscard]] PolygonSet<Point<ResultNumber, typename PointType_::LabelType>>
    minkowskiErosion(const OtherShape& other) const;

    /**
     * @brief Returns the regularized Minkowski sum of the two shapes (A ⊕ B),
     *        as a set of regions.
     *
     * The sum distributes over a union, and a set *is* a union:
     *
     *     (⋃ᵢ Aᵢ) ⊕ B = ⋃ᵢ (Aᵢ ⊕ B),
     *
     * so this sums each component against the operand — against each of *its*
     * components too, when the operand is a set — and unites the results in a
     * single arrangement rather than one per step. Each component sum is the
     * region-valued construction of `implementation/minkowskisum.hpp`; see
     * @ref Polygon::minkowskiSum for what it does and what it costs.
     *
     * **This is the one receiver with no precondition to observe**, and the one
     * whose answer needs a set however nondegenerate its operands are: the
     * components of a set are disjoint, and an operand small relative to the
     * gaps between them leaves them disjoint. What a body-shaped operand does
     * buy is that each *component's* sum is a single region — components merge
     * or stay apart, but none of them shatters.
     *
     * Complexity: the per-component sums, then one arrangement over all the
     * regions they leave.
     *
     * @tparam ResultNumber The number type for the result.
     * @param other The shape to sum with.
     * @return The pieces of the sum, in canonical order.
     * @note The component sums are built over exact rationals and converted to
     *       @p ResultNumber once, when the union is assembled.
     */
    template <class ResultNumber = division_result_t<NumberType>,
              detail::SetMinkowskiOperandConcept OtherShape>
    [[nodiscard]] PolygonSet<Point<ResultNumber, typename PointType::LabelType>>
    minkowskiSum(const OtherShape& other) const;

    // -------------------------------------------------------------------------
    // Transformations

    /** @brief Translates the set in place. */
    template <class TranslationNumber, class TranslationLabel>
    constexpr PolygonSet& operator+=(const Point<TranslationNumber, TranslationLabel>& translation) {
        for (auto& component : components_) {
            component += translation;
        }
        resetCache();
        return *this;
    }

    /** @brief Translates the set in place by the opposite vector. */
    template <class TranslationNumber, class TranslationLabel>
    constexpr PolygonSet& operator-=(const Point<TranslationNumber, TranslationLabel>& translation) {
        return *this += (-translation);
    }

    /**
     * @brief Scales the set in place.
     *
     * A negative factor reflects the components, which can change their relative
     * order, and a zero one collapses every one of them, so the result is
     * re-canonicalized.
     */
    template <class Scalar>
        requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
    constexpr PolygonSet& operator*=(const Scalar& scalar) {
        for (auto& component : components_) {
            component *= scalar;
        }
        normalize();
        return *this;
    }

    /** @copydoc operator*=(const Scalar&) */
    template <class Scalar>
        requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
    constexpr PolygonSet& operator/=(const Scalar& scalar) {
        for (auto& component : components_) {
            component /= scalar;
        }
        normalize();
        return *this;
    }

    /** @brief Returns the set rotated by 90k degrees around the origin. */
    [[nodiscard]] constexpr PolygonSet rotated90(int k) const {
        return mappedComponents([k](const ComponentType& component) { return component.rotated90(k); });
    }

    /** @brief Rotates the set by 90k degrees around the origin in place. */
    constexpr void rotate90(int k) {
        auto saved = label_;
        *this = rotated90(k);
        label_ = std::move(saved);
    }

    /**
     * @brief Returns the set with its x-coordinates multiplied by @p scalar.
     *
     * A negative factor reflects the components, which can change their relative
     * order, and a zero one collapses them onto the y-axis; either way the
     * result is re-canonicalized exactly as @ref operator*= does.
     */
    template <class OtherNumber>
    [[nodiscard]] constexpr PolygonSet scaledUpX(const OtherNumber scalar) const {
        return mappedComponents(
            [scalar](const ComponentType& component) { return component.scaledUpX(scalar); });
    }

    /** @brief Scales the set's x-coordinates up in place. */
    template <class OtherNumber>
    constexpr void scaleUpX(const OtherNumber scalar) {
        auto saved = label_;
        *this = scaledUpX(scalar);
        label_ = std::move(saved);
    }

    /** @copydoc scaledUpX */
    template <class OtherNumber>
    [[nodiscard]] constexpr PolygonSet scaledUpY(const OtherNumber scalar) const {
        return mappedComponents(
            [scalar](const ComponentType& component) { return component.scaledUpY(scalar); });
    }

    /** @brief Scales the set's y-coordinates up in place. */
    template <class OtherNumber>
    constexpr void scaleUpY(const OtherNumber scalar) {
        auto saved = label_;
        *this = scaledUpY(scalar);
        label_ = std::move(saved);
    }

    /** @copydoc scaledUpX */
    template <class OtherNumber>
    [[nodiscard]] constexpr PolygonSet scaledDownX(const OtherNumber scalar) const {
        return mappedComponents(
            [scalar](const ComponentType& component) { return component.scaledDownX(scalar); });
    }

    /** @brief Scales the set's x-coordinates down in place. */
    template <class OtherNumber>
    constexpr void scaleDownX(const OtherNumber scalar) {
        auto saved = label_;
        *this = scaledDownX(scalar);
        label_ = std::move(saved);
    }

    /** @copydoc scaledUpX */
    template <class OtherNumber>
    [[nodiscard]] constexpr PolygonSet scaledDownY(const OtherNumber scalar) const {
        return mappedComponents(
            [scalar](const ComponentType& component) { return component.scaledDownY(scalar); });
    }

    /** @brief Scales the set's y-coordinates down in place. */
    template <class OtherNumber>
    constexpr void scaleDownY(const OtherNumber scalar) {
        auto saved = label_;
        *this = scaledDownY(scalar);
        label_ = std::move(saved);
    }

    /**
     * @brief Forward iterator flattening every component's rings into one
     *        vertex sequence, components in canonical order.
     *
     * Each component already flattens its own rings, so this pairs a component
     * index with that component's @ref PolygonWithHoles::VertexIterator and
     * rolls over to the next component when the inner iterator runs out.
     */
    class VertexIterator {
      public:
        using iterator_category = std::forward_iterator_tag;
        using iterator_concept = std::forward_iterator_tag;
        using value_type = PointType;
        using difference_type = std::ptrdiff_t;
        using reference = value_type;

        constexpr VertexIterator() = default;

        constexpr value_type operator*() const {
            assert(set != nullptr);
            return *inner;
        }

        constexpr VertexIterator& operator++() {
            ++inner;
            skipExhausted();
            return *this;
        }

        constexpr VertexIterator operator++(int) {
            VertexIterator copy(*this);
            ++(*this);
            return copy;
        }

        constexpr bool operator==(const VertexIterator& other) const = default;

      private:
        friend struct PolygonSet;

        constexpr VertexIterator(const PolygonSet* set_arg, std::size_t component_arg)
            : set(set_arg), component(component_arg) {
            enterComponent();
            skipExhausted();
        }

        // Seeds `inner` from the current component, or clears it at the end so
        // that a walked-to-the-end iterator compares equal to verticesEnd().
        constexpr void enterComponent() {
            const std::size_t count = set == nullptr ? 0 : set->components_.size();
            inner = component < count ? set->components_[component].verticesBegin()
                                      : typename ComponentType::VertexIterator{};
        }

        // Steps over components holding no vertices. A canonical set has none —
        // addComponent() drops the ones without area — but this keeps the end
        // state reachable by increment alone for a set built any other way.
        constexpr void skipExhausted() {
            const std::size_t count = set == nullptr ? 0 : set->components_.size();
            while (component < count && inner == set->components_[component].verticesEnd()) {
                ++component;
                enterComponent();
            }
        }

        const PolygonSet* set = nullptr;
        std::size_t component = 0;
        typename ComponentType::VertexIterator inner{};
    };

  private:
    /**
     * @brief Invokes @p relation on every component and stops at the first
     *        `true`.
     *
     * The shape of every componentwise predicate above.
     */
    template <class ComponentRelation>
    constexpr bool anyComponent(ComponentRelation&& relation) const {
        for (const auto& component : components_) {
            if (relation(component)) {
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Tests whether a segment lies in the set (@p boundaryOnly `false`)
     *        or on its boundary (@p boundaryOnly `true`).
     *
     * This is the one-dimensional case no single component need settle. The
     * segment is cut at every point where it meets a component boundary; on each
     * open piece between two consecutive cuts, membership in every component is
     * constant, so the midpoint speaks for the piece and the cut points speak
     * for themselves. All the cuts lie on one line, where the lexicographic
     * order of points is the order along it, so sorting them needs no
     * projection.
     *
     * Exact: the cuts are crossings of two segments and are held over the pair's
     * exact rational type throughout.
     */
    template <class OtherSegment>
    bool segmentIn(const OtherSegment& segment, bool boundaryOnly) const;

    /**
     * @brief @ref segmentIn applied to every edge of a chain, a chain being
     *        exactly the union of its edges.
     */
    template <class OtherChain>
    bool chainIn(const OtherChain& chain, bool boundaryOnly) const;

    /**
     * @brief Tests whether a region lies in the set when no single component
     *        holds it.
     *
     * The two-dimensional case no single component need settle. A region is
     * connected, but its *interior* need not be — a hole touching the outer
     * boundary pinches it apart — and each piece of that interior may go to a
     * different component. Three conditions decide it, and together they are
     * exactly `A ⊇ B`:
     *
     * - **The boundary.** `∂B` is one-dimensional, so every edge of it goes
     *   through @ref segmentIn, which lets the components share an edge.
     * - **No component boundary inside `B°`.** An edge of `∂Aᵢ` has `Aᵢ°` on at
     *   most one side, and the other side is in no component either — a second
     *   one there would share a stretch of edge with the first, which
     *   @ref isValid rules out. So an edge that meets `B°` at all meets it in a
     *   stretch (an open set cut by a segment leaves an interval) and drags a
     *   piece of `B°` out of the set with it.
     * - **One witness per piece of `B°`.** With no component boundary crossing
     *   it, each connected piece of `B°` lies wholly inside the set or wholly
     *   outside, so one point settles it. The witnesses come from the vertical
     *   strips between consecutive vertex abscissas of `B`: a piece is bounded
     *   left and right by vertices of `B` — its own edges cross nowhere but at
     *   their endpoints, `B` being valid — so it spans a whole strip and is met
     *   by the vertical line down the middle of that one, where the edges
     *   crossing the line cut the pieces out along it.
     *
     * Exact: the strip midpoints and the crossings along them are held over the
     * pair's exact rational type, as in @ref segmentIn.
     *
     * Complexity: `O(nB)` strips, each cut by the `O(nB)` edges of the operand,
     * with one point location per component per elementary interval.
     */
    template <class OtherRegion>
    bool regionIn(const OtherRegion& region) const;

    /**
     * @brief The smallest value of @p distance over the components.
     *
     * The minimum is held in whatever type a component answers in, which is not
     * always the type the caller requested: a distance to a @ref Disk is
     * realized on a circle, so @ref PolygonWithHoles reports it in
     * `detail::floating_result_t<ResultNumber>`, and converting that back here
     * would be both lossy and, for an exact @p ResultNumber, ill-formed.
     */
    template <class ComponentDistance>
    auto minOverComponents(ComponentDistance&& distance) const {
        using ResultNumber = std::decay_t<decltype(distance(std::declval<const ComponentType&>()))>;
        ResultNumber best{};
        bool seeded = false;
        for (const auto& component : components_) {
            const ResultNumber current = distance(component);
            if (!seeded || current < best) {
                best = current;
                seeded = true;
            }
        }
        return best;
    }

    /**
     * @brief Applies a component-to-component transformation to every component,
     *        then re-canonicalizes.
     *
     * Shared by @ref rotated90 and the four axis-scaling accessors, which differ
     * only in which @ref PolygonWithHoles transformation they ask each component
     * for.
     */
    template <class ComponentTransform>
    constexpr PolygonSet mappedComponents(ComponentTransform&& transform) const {
        PolygonSet result;
        result.components_.reserve(components_.size());
        for (const auto& component : components_) {
            result.components_.push_back(transform(component));
        }
        result.normalize();
        return result;
    }

    std::vector<ComponentType> components_{};
    [[no_unique_address]] mutable LabelType label_{};

    // Cached bounding box. A region reads its own off its outer ring, which
    // caches one already; a set has no single ring to ask, so it caches the
    // union of the components' boxes itself.
    mutable Rectangle<PointType> bbox_{};

    // Tri-state cache for @ref isPinched: -1 not yet computed, 0 no two
    // components touch, 1 some two do.
    mutable signed char pinched_ = -1;

    // Memoized hash, computed lazily by std::hash<PolygonSet>, with the same
    // sentinel scheme as Polygon and PolygonWithHoles: hashUnset_ means "not yet
    // computed", and the one true hash colliding with it is remapped so the
    // sentinel is never stored as a real value.
    static constexpr std::size_t hashUnset_ = pgl::detail::numeric_limits<std::size_t>::max();
    mutable std::size_t hash_ = hashUnset_;
    friend struct std::hash<PolygonSet>;

    template <class OtherPointType, class OtherLabelType>
    friend struct PolygonSet;

    constexpr void resetCache() const {
        hash_ = hashUnset_;
        bbox_ = {};
        pinched_ = -1;
    }

    /**
     * @brief Brings the components to canonical form: zero-area ones dropped,
     *        the rest sorted by @ref PolygonWithHoles::operator<=> with
     *        duplicates erased.
     *
     * Each component is already canonical on its own — @ref PolygonWithHoles's
     * constructor and mutators guarantee that — so only the component list needs
     * work.
     */
    constexpr void normalize() {
        std::erase_if(components_,
                      [](const ComponentType& component) { return component.isDegenerate(); });
        std::sort(components_.begin(), components_.end());
        components_.erase(std::unique(components_.begin(), components_.end()), components_.end());
        resetCache();
    }
};

// `set + point` is the translating Minkowski sum, spelled by the generic
// operator+ in implementation/minkowski.hpp like every other shape's: writing a
// second one here would shadow it and, holding the set's own point type, would
// silently truncate a translation that does not fit it.

/** @brief Returns a copy of a set translated by the opposite point. */
template <class PointType, class LabelType, class TranslationNumber, class TranslationLabel>
constexpr auto operator-(const PolygonSet<PointType, LabelType>& set,
                         const Point<TranslationNumber, TranslationLabel>& translation) {
    return set + (-translation);
}

template <class PointType, class LabelType, class Scalar>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr auto operator*(const PolygonSet<PointType, LabelType>& set, const Scalar& scalar) {
    using ResultPointType = Point<decltype(std::declval<PointType>().x() * scalar), typename PointType::LabelType>;
    PolygonSet<ResultPointType, LabelType> result(set);
    result *= scalar;
    if constexpr (detail::has_label_v<LabelType>) {
        result.label() = LabelType{};
    }
    return result;
}

template <class Scalar, class PointType, class LabelType>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr auto operator*(const Scalar& scalar, const PolygonSet<PointType, LabelType>& set) {
    return set * scalar;
}

template <class PointType, class LabelType, class Scalar>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr auto operator/(const PolygonSet<PointType, LabelType>& set, const Scalar& scalar) {
    using ResultPointType = Point<decltype(std::declval<PointType>().x() / scalar), typename PointType::LabelType>;
    PolygonSet<ResultPointType, LabelType> result(set);
    result /= scalar;
    if constexpr (detail::has_label_v<LabelType>) {
        result.label() = LabelType{};
    }
    return result;
}

template <class PointType, class LabelType>
std::ostream& operator<<(std::ostream& stream, const PolygonSet<PointType, LabelType>& set);

}  // namespace pgl
