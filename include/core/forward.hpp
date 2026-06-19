#pragma once

#include "pgl.hpp"

/**
 * @file forward.hpp
 * @brief Forward declarations for core numeric and geometry types.
 *
 * This header lets shape and implementation headers refer to each other without
 * forcing all definitions to be parsed immediately.
 */

#include <type_traits>

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

/** @brief Closed Euclidean disk stored by boundary points plus optional disk label. */
template <class PointType, class Label>
struct Disk;

/** @brief Runtime variant wrapper over the supported primitive shapes. */
template <class PointType>
struct Shape;

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
template <class PointType, class Label>
inline constexpr int shapeRank<Polygon<PointType, Label>> = 120;

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

template <class T> struct is_disk : std::false_type {};
template <class PointType, class Label> struct is_disk<Disk<PointType, Label>> : std::true_type {};
template <class T> inline constexpr bool is_disk_v = is_disk<std::remove_cvref_t<T>>::value;

template <class T> struct is_shape : std::false_type {};
template <class PointType> struct is_shape<Shape<PointType>> : std::true_type {};
template <class T> inline constexpr bool is_shape_v = is_shape<std::remove_cvref_t<T>>::value;

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
template <class T> concept DiskConcept = detail::is_disk_v<T>;
template <class T> concept ShapeConcept = detail::is_shape_v<T>;

}  // namespace pgl
