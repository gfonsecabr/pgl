#pragma once

/**
 * @file forward.hpp
 * @brief Forward declarations for core numeric and geometry types.
 *
 * This header lets shape and implementation headers refer to each other without
 * forcing all definitions to be parsed immediately.
 */

#include <type_traits>
#include <vector>

namespace pgl {

/**
 * @brief Sentinel label type used by unlabeled points.
 */
struct NoLabel;

/**
 * @brief Arbitrary precision signed integer.
 */
class BigInt;

/**
 * @brief Exact rational number class template.
 *
 * @tparam T Integral storage type.
 */
template <class T>
class Rational;

/** @brief Undirected simple graph with hashable vertices. */
template <class Vertex>
class Graph;

/** @brief The empty set of points in the plane. */
template <class PointType>
struct EmptyShape;

/**
 * @brief Two-dimensional point with optional label payload.
 *
 * @tparam Number Coordinate type.
 * @tparam Label Label type.
 */
template <class Number, class Label>
struct Point;

/**
 * @brief Tests whether three points are collinear.
 *
 * @param a First point.
 * @param b Second point.
 * @param c Third point.
 * @return `true` if the points lie on a common line.
 */
template <class ANumber, class ALabel, class BNumber, class BLabel, class CNumber, class CLabel>
constexpr bool collinear(
    const Point<ANumber, ALabel>& a,
    const Point<BNumber, BLabel>& b,
    const Point<CNumber, CLabel>& c);

/** @brief Unoriented closed segment between two endpoints plus optional segment label. */
template <class PointType, class Label = NoLabel>
struct Segment;

/** @brief Directed segment preserving source-to-target order plus optional segment label. */
template <class PointType, class Label = NoLabel>
struct OrientedSegment;

/** @brief Unoriented infinite line. */
template <class PointType, class Label = NoLabel>
struct Line;

/** @brief Directed infinite line with left/right side semantics plus optional line label. */
template <class PointType, class Label = NoLabel>
struct OrientedLine;

/** @brief Half-infinite line starting from one source point plus optional ray label. */
template <class PointType, class Label = NoLabel>
struct Ray;

/** @brief Closed half-plane defined by an oriented boundary line. */
template <class PointType, class Label = NoLabel>
struct Halfplane;

/** @brief Axis-aligned rectangle stored by minimum and maximum corners. */
template <class PointType, class Label = NoLabel>
struct Rectangle;

/** @brief Closed triangle stored by three vertices. */
template <class PointType, class Label = NoLabel>
struct Triangle;

/** @brief Closed convex polygon stored by its vertices. */
template <class PointType, class Label = NoLabel>
struct Convex;

/** @brief Closed simple polygon stored by its vertices. */
template <class PointType, class Label = NoLabel>
struct Polygon;

/** @brief Intersection of closed half-planes; convex but possibly unbounded or empty. */
template <class PointType, class Label = NoLabel>
struct HalfplaneIntersection;

/** @brief Closed region bounded by one outer simple polygon minus disjoint polygonal holes. */
template <class PointType, class Label = NoLabel>
struct PolygonWithHoles;

/** @brief Set of closed regions with pairwise disjoint interiors. */
template <class PointType, class Label = NoLabel>
struct PolygonSet;

/** @brief Weakly x-monotone polyline stored by lexicographically sorted vertices. */
template <class PointType, class Label = NoLabel, class Storage = std::vector<PointType>>
struct MonotoneChain;

/** @brief Open polygonal chain stored in traversal order; may self-intersect. */
template <class PointType, class Label = NoLabel>
struct Polyline;

/** @brief Closed Euclidean disk stored by boundary points plus optional disk label. */
template <class PointType, class Label>
struct Disk;

/** @brief Runtime variant wrapper over the supported primitive shapes. */
template <class PointType>
struct Shape;

/**
 * @brief Planar subdivision induced by a set of one-dimensional shapes.
 *
 * Declared here, well before its definition in algorithm/arrangement.hpp, so
 * that an implementation header included ahead of it — the cut predicates
 * @ref pgl::Polygon::separates and @ref pgl::Polygon::crosses run on the
 * arrangement of both operands' boundaries — may name the class inside a
 * template whose instantiation happens later.
 *
 * The vertex type defaults to @ref pgl::EPoint — spelled out here, where that
 * alias does not exist yet — because an arrangement's vertices are crossings,
 * and only an exact rational point type is guaranteed to hold them.
 */
template <class PointType = Point<Rational<BigInt>, NoLabel>, class Label = NoLabel>
class Arrangement;

/**
 * @brief Affine transformation stored as a 2x3 matrix.
 *
 * Applied to a point or shape with `operator*`; composed with another
 * transformation with `operator*` as well.
 *
 * @tparam Number Matrix entry / coordinate type.
 */
template <class Number>
struct Transformation;

/** @brief Lightweight SVG canvas for visualizing Pangolin primitives. */
class Canvas;

namespace detail {

/**
 * @brief Conceptual rank of each shape kind.
 *
 * Gives the per-shape `intersection` fallbacks a single canonical delegation
 * direction: a shape only delegates `a.intersection(b)` to `b.intersection(a)`
 * when `rank(b) > rank(a)`, so the higher-ranked shape is the one that owns the
 * concrete overload. This breaks the otherwise mutual fallback recursion and
 * lets `Shape::intersection` detect support with a plain `requires`. Gaps of 10
 * leave room to slot in new shapes without renumbering.
 */
template <class T>
inline constexpr int shapeRank = -1;

template <class PointType>
inline constexpr int shapeRank<EmptyShape<PointType>> = 0;
template <class Number, class Label>
inline constexpr int shapeRank<Point<Number, Label>> = 10;
template <class PointType, class Label>
inline constexpr int shapeRank<Segment<PointType, Label>> = 20;
template <class PointType, class Label>
inline constexpr int shapeRank<OrientedSegment<PointType, Label>> = 30;
template <class PointType, class Label>
inline constexpr int shapeRank<Line<PointType, Label>> = 40;
template <class PointType, class Label>
inline constexpr int shapeRank<OrientedLine<PointType, Label>> = 50;
template <class PointType, class Label>
inline constexpr int shapeRank<Ray<PointType, Label>> = 60;
template <class PointType, class Label>
inline constexpr int shapeRank<Halfplane<PointType, Label>> = 70;
template <class PointType, class Label>
inline constexpr int shapeRank<Rectangle<PointType, Label>> = 80;
template <class PointType, class Label>
inline constexpr int shapeRank<Triangle<PointType, Label>> = 90;
template <class PointType, class Label>
inline constexpr int shapeRank<Disk<PointType, Label>> = 100;
template <class PointType, class Label>
inline constexpr int shapeRank<Convex<PointType, Label>> = 110;
template <class PointType, class Label, class Storage>
inline constexpr int shapeRank<MonotoneChain<PointType, Label, Storage>> = 115;
template <class PointType, class Label>
inline constexpr int shapeRank<Polyline<PointType, Label>> = 117;
template <class PointType, class Label>
inline constexpr int shapeRank<Polygon<PointType, Label>> = 120;
template <class PointType, class Label>
inline constexpr int shapeRank<HalfplaneIntersection<PointType, Label>> = 130;
template <class PointType, class Label>
inline constexpr int shapeRank<PolygonWithHoles<PointType, Label>> = 140;
template <class PointType, class Label>
inline constexpr int shapeRank<PolygonSet<PointType, Label>> = 150;

// Shape-detection traits: is_<shape>_v<T> is true when T (ignoring cv/ref) is a
// specialization of that shape. They back the public XxxConcept concepts below
// and the generic 'Shape' routing in predicates.hpp. Defined here, before any
// shape header, so every header can use any shape's concept regardless of the
// order shapes are included in pgl.hpp.
template <class T> struct is_empty_shape : std::false_type {};
template <class PointType> struct is_empty_shape<EmptyShape<PointType>> : std::true_type {};
template <class T> inline constexpr bool is_empty_shape_v = is_empty_shape<std::remove_cvref_t<T>>::value;

template <class T> struct is_point : std::false_type {};
template <class Number, class Label> struct is_point<Point<Number, Label>> : std::true_type {};
template <class T> inline constexpr bool is_point_v = is_point<std::remove_cvref_t<T>>::value;

template <class T> struct is_segment : std::false_type {};
template <class PointType, class Label> struct is_segment<Segment<PointType, Label>> : std::true_type {};
template <class T> inline constexpr bool is_segment_v = is_segment<std::remove_cvref_t<T>>::value;

template <class T> struct is_oriented_segment : std::false_type {};
template <class PointType, class Label> struct is_oriented_segment<OrientedSegment<PointType, Label>> : std::true_type {};
template <class T> inline constexpr bool is_oriented_segment_v = is_oriented_segment<std::remove_cvref_t<T>>::value;

template <class T> struct is_line : std::false_type {};
template <class PointType, class Label> struct is_line<Line<PointType, Label>> : std::true_type {};
template <class T> inline constexpr bool is_line_v = is_line<std::remove_cvref_t<T>>::value;

template <class T> struct is_oriented_line : std::false_type {};
template <class PointType, class Label> struct is_oriented_line<OrientedLine<PointType, Label>> : std::true_type {};
template <class T> inline constexpr bool is_oriented_line_v = is_oriented_line<std::remove_cvref_t<T>>::value;

template <class T> struct is_ray : std::false_type {};
template <class PointType, class Label> struct is_ray<Ray<PointType, Label>> : std::true_type {};
template <class T> inline constexpr bool is_ray_v = is_ray<std::remove_cvref_t<T>>::value;

template <class T> struct is_halfplane : std::false_type {};
template <class PointType, class Label> struct is_halfplane<Halfplane<PointType, Label>> : std::true_type {};
template <class T> inline constexpr bool is_halfplane_v = is_halfplane<std::remove_cvref_t<T>>::value;

template <class T> struct is_rectangle : std::false_type {};
template <class PointType, class Label> struct is_rectangle<Rectangle<PointType, Label>> : std::true_type {};
template <class T> inline constexpr bool is_rectangle_v = is_rectangle<std::remove_cvref_t<T>>::value;

template <class T> struct is_triangle : std::false_type {};
template <class PointType, class Label> struct is_triangle<Triangle<PointType, Label>> : std::true_type {};
template <class T> inline constexpr bool is_triangle_v = is_triangle<std::remove_cvref_t<T>>::value;

template <class T> struct is_convex : std::false_type {};
template <class PointType, class Label> struct is_convex<Convex<PointType, Label>> : std::true_type {};
template <class T> inline constexpr bool is_convex_v = is_convex<std::remove_cvref_t<T>>::value;

template <class T> struct is_polygon : std::false_type {};
template <class PointType, class Label> struct is_polygon<Polygon<PointType, Label>> : std::true_type {};
template <class T> inline constexpr bool is_polygon_v = is_polygon<std::remove_cvref_t<T>>::value;

template <class T> struct is_polygon_with_holes : std::false_type {};
template <class PointType, class Label> struct is_polygon_with_holes<PolygonWithHoles<PointType, Label>> : std::true_type {};
template <class T> inline constexpr bool is_polygon_with_holes_v = is_polygon_with_holes<std::remove_cvref_t<T>>::value;

template <class T> struct is_polygon_set : std::false_type {};
template <class PointType, class Label> struct is_polygon_set<PolygonSet<PointType, Label>> : std::true_type {};
template <class T> inline constexpr bool is_polygon_set_v = is_polygon_set<std::remove_cvref_t<T>>::value;

template <class T> struct is_halfplane_intersection : std::false_type {};
template <class PointType, class Label> struct is_halfplane_intersection<HalfplaneIntersection<PointType, Label>> : std::true_type {};
template <class T> inline constexpr bool is_halfplane_intersection_v = is_halfplane_intersection<std::remove_cvref_t<T>>::value;

template <class T> struct is_monotone_chain : std::false_type {};
template <class PointType, class Label, class Storage> struct is_monotone_chain<MonotoneChain<PointType, Label, Storage>> : std::true_type {};
template <class T> inline constexpr bool is_monotone_chain_v = is_monotone_chain<std::remove_cvref_t<T>>::value;

template <class T> struct is_polyline : std::false_type {};
template <class PointType, class Label> struct is_polyline<Polyline<PointType, Label>> : std::true_type {};
template <class T> inline constexpr bool is_polyline_v = is_polyline<std::remove_cvref_t<T>>::value;

template <class T> struct is_disk : std::false_type {};
template <class PointType, class Label> struct is_disk<Disk<PointType, Label>> : std::true_type {};
template <class T> inline constexpr bool is_disk_v = is_disk<std::remove_cvref_t<T>>::value;

template <class T> struct is_shape : std::false_type {};
template <class PointType> struct is_shape<Shape<PointType>> : std::true_type {};
template <class T> inline constexpr bool is_shape_v = is_shape<std::remove_cvref_t<T>>::value;

template <class T> struct is_transformation : std::false_type {};
template <class Number> struct is_transformation<Transformation<Number>> : std::true_type {};
template <class T> inline constexpr bool is_transformation_v = is_transformation<std::remove_cvref_t<T>>::value;

}  // namespace detail

// Public per-shape concepts: each is satisfied by any specialization of that
// shape (ignoring cv/ref). They live here so every shape and implementation
// header can constrain on them, e.g. `template<SegmentConcept S> ...`.
template <class T> concept EmptyShapeConcept = detail::is_empty_shape_v<T>;
template <class T> concept PointConcept = detail::is_point_v<T>;
template <class T> concept SegmentConcept = detail::is_segment_v<T>;
template <class T> concept OrientedSegmentConcept = detail::is_oriented_segment_v<T>;
template <class T> concept LineConcept = detail::is_line_v<T>;
template <class T> concept OrientedLineConcept = detail::is_oriented_line_v<T>;
template <class T> concept RayConcept = detail::is_ray_v<T>;
template <class T> concept HalfplaneConcept = detail::is_halfplane_v<T>;
template <class T> concept RectangleConcept = detail::is_rectangle_v<T>;
template <class T> concept TriangleConcept = detail::is_triangle_v<T>;
template <class T> concept ConvexConcept = detail::is_convex_v<T>;
template <class T> concept PolygonConcept = detail::is_polygon_v<T>;
template <class T> concept PolygonWithHolesConcept = detail::is_polygon_with_holes_v<T>;
template <class T> concept PolygonSetConcept = detail::is_polygon_set_v<T>;
template <class T> concept HalfplaneIntersectionConcept = detail::is_halfplane_intersection_v<T>;
template <class T> concept MonotoneChainConcept = detail::is_monotone_chain_v<T>;
template <class T> concept PolylineConcept = detail::is_polyline_v<T>;
template <class T> concept DiskConcept = detail::is_disk_v<T>;
template <class T> concept ShapeConcept = detail::is_shape_v<T>;
template <class T> concept TransformationConcept = detail::is_transformation_v<T>;

/** @brief Any concrete geometry type or the runtime @ref Shape wrapper. */
template <class T>
concept AnyShapeConcept =
    ShapeConcept<T> || detail::shapeRank<std::remove_cvref_t<T>> >= 0;

/**
 * @brief Bounded convex primitives.
 *
 * These are exactly the shapes whose Minkowski sum with each other is again a
 * bounded convex polygon with vertices at sums of input vertices, so the sum
 * stays inside the shape vocabulary and inside exact integer arithmetic.
 */
template <class T>
concept BoundedConvexConcept =
    PointConcept<T> || SegmentConcept<T> || OrientedSegmentConcept<T> ||
    RectangleConcept<T> || TriangleConcept<T> || ConvexConcept<T>;

/**
 * @brief Unbounded convex polyhedral primitives.
 *
 * A half-plane, a line, a ray and a @ref HalfplaneIntersection are each the
 * intersection of finitely many closed half-planes, so each is convex and each
 * is closed under the Minkowski sum with any of the others and with any bounded
 * convex shape: the sum of two convex polyhedra is a convex polyhedron. None of
 * them is bounded (a @ref HalfplaneIntersection may happen to be, and is still
 * one of these), which is exactly why their sums answer with a
 * @ref HalfplaneIntersection rather than a @ref Convex.
 *
 * A @ref Disk is convex and is *not* one of these: it is not polyhedral, so its
 * sums leave the shape vocabulary in every direction but four.
 */
template <class T>
concept UnboundedConvexConcept =
    HalfplaneConcept<T> || LineConcept<T> || OrientedLineConcept<T> ||
    RayConcept<T> || HalfplaneIntersectionConcept<T>;

/**
 * @brief Bounded polygonal primitives, convex or not.
 *
 * Exactly the shapes that are the convex hull of finitely many vertices their
 * `vertices()` accessor lists, which is what a support function needs: the
 * extreme point of one of these in any direction is one of its vertices, found
 * exactly by comparing cross products. @ref DiskConcept is bounded and is *not*
 * one of these — its support point is off the lattice in every direction but
 * four.
 */
template <class T>
concept BoundedPolygonalConcept =
    BoundedConvexConcept<T> || PolygonConcept<T> || PolylineConcept<T> ||
    MonotoneChainConcept<T> || PolygonWithHolesConcept<T> || PolygonSetConcept<T>;

/**
 * @brief Bounded polygonal regions: exactly the shapes a @ref PolygonSet can
 *        always represent.
 *
 * Each of these is a closed bounded subset of the plane whose boundary is a
 * finite set of straight edges, so it *is* a set of regions — one component for a
 * @ref Rectangle, a @ref Triangle, a @ref Convex, a @ref Polygon or a
 * @ref PolygonWithHoles, and however many the set already carries for a
 * @ref PolygonSet. That makes them closed under union: `A ∪ B` is again a bounded
 * polygonal region whenever both operands are, which is what lets `regularizedUnion` be
 * defined for every ordered pair of them and for no other pair.
 *
 * What is left out is left out for one of three reasons. A @ref Point, a
 * @ref Segment, a @ref Polyline and a @ref MonotoneChain carry no area, so a
 * union with one keeps a dangling piece no set of regions can hold. A
 * @ref Halfplane, a @ref Line, a @ref Ray and a @ref HalfplaneIntersection may be
 * unbounded (the last only sometimes, which is already too often for *always*).
 * A @ref Disk is bounded and has area, but its boundary is round. @ref EmptyShape
 * is a region — the empty one — but it is the identity of every union, and the
 * library keeps it out of the region vocabulary so that its operations stay the
 * uniform vacuous ones.
 *
 * A degenerate operand is admitted and contributes nothing: a rectangle with no
 * width covers no area, and a regularized union drops what has none.
 */
template <class T>
concept PolygonalRegionConcept =
    RectangleConcept<T> || TriangleConcept<T> || ConvexConcept<T> ||
    PolygonConcept<T> || PolygonWithHolesConcept<T> || PolygonSetConcept<T>;

/**
 * @brief Shape pairs whose Minkowski sum Pangolin can represent.
 *
 * The sum `A ⊕ B` is supported when
 * - either operand is the empty shape (which absorbs), or
 * - either operand is a `Point`, so the sum is a translation of the other and
 *   every shape kind is closed under it, or
 * - both operands are bounded convex (@ref BoundedConvexConcept), or
 * - one is a `Halfplane` and the other is bounded polygonal
 *   (@ref BoundedPolygonalConcept), convex or not: a half-plane absorbs
 *   everything bounded and comes back a half-plane, translated to its operand's
 *   support point, or
 * - one is unbounded convex (@ref UnboundedConvexConcept) and the other is
 *   unbounded convex or bounded **convex**: two convex polyhedra sum to a convex
 *   polyhedron, returned as a `HalfplaneIntersection`.
 *
 * A runtime-polymorphic @ref Shape on either side is always accepted; the pair
 * of stored alternatives is only checked when the sum is evaluated.
 *
 * A non-convex operand is excluded from the unbounded case, and only from it: a
 * half-plane is the one unbounded shape whose sum forgets its operand's
 * concavity, because only its support point survives. Drag a `Polygon` along a
 * ray instead and every notch of it is swept into the answer, which is then no
 * more convex than the polygon was. Curved operands (`Disk`) are excluded beyond
 * translation for the other reason: their support point is not on the lattice,
 * so their sums are inexact rather than unrepresentable, and the two pairs that
 * do have an answer carry a `ResultNumber` of their own instead of appearing
 * here.
 *
 * This concept is the only place that decides which pairs give back a **single
 * shape**. The non-convex case is not a widening of it: a sum that can enclose a
 * hole needs a `PolygonWithHoles` region result, so `Polygon::minkowskiSum`,
 * `PolygonWithHoles::minkowskiSum` and `Polyline::minkowskiSum` carry it as an
 * overload set of their own, over exactly the pairs this concept rejects. See
 * `implementation/minkowskisum.hpp`.
 */
template <class A, class B>
concept MinkowskiSummableConcept =
    (ShapeConcept<A> || detail::shapeRank<std::remove_cvref_t<A>> >= 0) &&
    (ShapeConcept<B> || detail::shapeRank<std::remove_cvref_t<B>> >= 0) &&
    (ShapeConcept<A> || ShapeConcept<B> ||
     EmptyShapeConcept<A> || EmptyShapeConcept<B> ||
     PointConcept<A> || PointConcept<B> ||
     (BoundedConvexConcept<A> && BoundedConvexConcept<B>) ||
     (HalfplaneConcept<A> && BoundedPolygonalConcept<B>) ||
     (BoundedPolygonalConcept<A> && HalfplaneConcept<B>) ||
     (UnboundedConvexConcept<A> && UnboundedConvexConcept<B>) ||
     (UnboundedConvexConcept<A> && BoundedConvexConcept<B>) ||
     (BoundedConvexConcept<A> && UnboundedConvexConcept<B>));

namespace detail {

/**
 * @brief Evaluates @p predicate on the carrier of a shape that has collapsed to
 * a point or to a segment, and returns `false` when @p other has not collapsed.
 *
 * A shape satisfying `isPoint` or `isSegment` covers exactly the point set of
 * that point or segment, so a predicate whose answer is a constant `false` for
 * a full-dimensional argument must still answer correctly for such a carrier.
 * Writing `return detail::reduceDegenerate(other, ...)` in place of
 * `return false` restores that case while keeping the constant answer for
 * arguments that cannot collapse (lines, rays, half-planes have no `getIf*`
 * accessor) and for arguments that simply are not degenerate.
 *
 * The recursion this induces terminates: the carrier of a collapsed shape is a
 * `Point` or a non-degenerate `Segment`, and neither reduces any further.
 *
 * @param other Shape that may have collapsed to a lower dimension.
 * @param predicate Callable accepting the carrier point or segment.
 * @return The predicate's value on the carrier, or `false` if there is none.
 */
template <class Other, class Predicate>
constexpr bool reduceDegenerate(const Other& other, Predicate predicate) {
    using Number = typename std::remove_cvref_t<Other>::NumberType;
    if constexpr (requires { other.template getIfPoint<Number>(); }) {
        if (const auto vertex = other.template getIfPoint<Number>()) {
            return predicate(*vertex);
        }
    } else if constexpr (requires { other.getIfPoint(); }) {
        if (const auto vertex = other.getIfPoint()) {
            return predicate(*vertex);
        }
    }
    if constexpr (requires { other.template getIfSegment<Number>(); }) {
        if (const auto carrier = other.template getIfSegment<Number>()) {
            return predicate(*carrier);
        }
    } else if constexpr (requires { other.getIfSegment(); }) {
        if (const auto carrier = other.getIfSegment()) {
            return predicate(*carrier);
        }
    }
    return false;
}

/**
 * @brief The @ref reduceDegenerate variant for an argument that answers
 * `isDegenerate` more cheaply than it answers `getIfPoint` and `getIfSegment`.
 *
 * Both carriers exist only for a degenerate argument, so a shape whose
 * degeneracy test is cheaper than the two probes -- `Rectangle`, two coordinate
 * comparisons, and `Convex`, a vertex count -- settles the common
 * non-degenerate case in one test instead of re-deriving overlapping facts in
 * each probe.
 *
 * Reserved for exactly those shapes. `Polygon` and the chains answer
 * `isDegenerate` with an exact area sum over every vertex, far dearer than the
 * probes they would replace, and a shape that cannot collapse to a segment
 * probes only once already.
 *
 * @param other Shape that may have collapsed to a point or to a segment.
 * @param predicate Callable accepting the carrier point or segment.
 * @return The predicate's value on the carrier, or `false` if there is none.
 */
template <class Other, class Predicate>
constexpr bool reduceDegenerateGuarded(const Other& other, Predicate predicate) {
    return other.isDegenerate() && reduceDegenerate(other, predicate);
}

/**
 * @brief Tests whether a shape covers no point at all.
 *
 * Every shape that has an empty state answers `empty()` for it: @ref Rectangle,
 * @ref Convex, @ref Polygon, @ref PolygonWithHoles, @ref PolygonSet,
 * @ref HalfplaneIntersection, @ref Polyline, @ref MonotoneChain, and
 * @ref Shape. Every other shape is defined by points it covers, so it is never
 * empty, and answers `false` here.
 *
 * The `size()` fallback catches @ref EmptyShape, which is always empty and has
 * no `empty()` of its own because it answers every relation directly. It is
 * safe for the shapes that reach it because a `size()` that is a fixed count --
 * the two coordinates of a @ref Point, the three defining points of a
 * @ref Triangle or @ref Disk -- can never report `0`. A new shape that spends
 * `size()` on something else, and can spend nothing, would need its own
 * `empty` rather than this fallback.
 */
template <class TShape>
constexpr bool coversNoPoint(const TShape& shape) {
    if constexpr (requires { shape.empty(); }) {
        return shape.empty();
    } else if constexpr (requires { shape.size(); }) {
        return shape.size() == 0;
    } else {
        return false;
    }
}

/**
 * @brief The @ref reduceDegenerate variant for a predicate that only a
 * point-collapsed argument can satisfy.
 *
 * When the answer is a constant `false` for every argument of positive
 * diameter -- as it is for a predicate about a boundary that is a finite point
 * set -- the segment carrier of @ref reduceDegenerate would be tested only to
 * be rejected. Skipping it matters: `getIfSegment` costs a collinearity scan,
 * one exact orientation determinant per vertex, whereas `getIfPoint` costs an
 * equality test that a shape with area fails on its second vertex.
 *
 * @param other Shape that may have collapsed to a point.
 * @param predicate Callable accepting the carrier point.
 * @return The predicate's value on the carrier point, or `false` if there is none.
 */
template <class Other, class Predicate>
constexpr bool reduceDegenerateToPoint(const Other& other, Predicate predicate) {
    using Number = typename std::remove_cvref_t<Other>::NumberType;
    if constexpr (requires { other.template getIfPoint<Number>(); }) {
        if (const auto vertex = other.template getIfPoint<Number>()) {
            return predicate(*vertex);
        }
    } else if constexpr (requires { other.getIfPoint(); }) {
        if (const auto vertex = other.getIfPoint()) {
            return predicate(*vertex);
        }
    }
    return false;
}

}  // namespace detail

}  // namespace pgl
