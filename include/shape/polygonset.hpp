#pragma once

#include "shape/polygonwithholes.hpp"

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

    // -------------------------------------------------------------------------
    // State queries

    /** @brief Tests whether the set has no components at all. */
    [[nodiscard]] constexpr bool isEmpty() const {
        return components_.empty();
    }

    /**
     * @brief Tests whether the set has zero area.
     *
     * A canonical set drops its zero-area components, so this is exactly
     * @ref isEmpty for one; a set adopted with `trusted` may still carry a
     * component without area, which is why the areas are summed rather than the
     * components counted.
     */
    [[nodiscard]] constexpr bool isDegenerate() const {
        return twiceArea() == NumberType(0);
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
     */
    [[nodiscard]] constexpr NumberType twiceArea() const {
        NumberType total{};
        for (const auto& component : components_) {
            total += component.twiceArea();
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
     * @brief Returns the Minkowski sum of this shape and another (A ⊕ B).
     *
     * The sum is the point set `{a + b : a ∈ A, b ∈ B}`. Summing with a `Point`
     * is a translation, so it gives back a set over the promoted coordinate
     * type — this is the reading `set + point` has always had. A set of regions
     * is not convex, so @ref MinkowskiSummableConcept admits nothing else here,
     * exactly as it admits nothing else for a @ref Polygon or a
     * @ref PolygonWithHoles.
     *
     * @tparam OtherShape Type of the other shape.
     * @param other Shape to sum with.
     * @return The Minkowski sum, in the tightest type that represents it.
     */
    template <class OtherShape>
        requires MinkowskiSummableConcept<PolygonSet<PointType_, TLabel>, OtherShape>
    [[nodiscard]] constexpr auto minkowskiSum(const OtherShape& other) const;

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

  private:
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
    mutable std::optional<Rectangle<PointType>> bbox_{};

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
        bbox_.reset();
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
