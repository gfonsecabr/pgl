#pragma once

/**
 * @file forward.hpp
 * @brief Forward declarations for core numeric and geometry types.
 *
 * This header lets shape and implementation headers refer to each other without
 * forcing all definitions to be parsed immediately.
 */

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

/** @brief Directed segment preserving source-to-target order. */
template <class PointType>
struct OrientedSegment;

/** @brief Unoriented infinite line. */
template <class PointType>
struct Line;

/** @brief Directed infinite line with left/right side semantics. */
template <class PointType>
struct OrientedLine;

/** @brief Half-infinite line starting from one source point. */
template <class PointType>
struct Ray;

/** @brief Closed half-plane defined by an oriented boundary line. */
template <class PointType>
struct Halfplane;

/** @brief Axis-aligned rectangle stored by minimum and maximum corners. */
template <class PointType>
struct Rectangle;

/** @brief Closed triangle stored by three vertices. */
template <class PointType>
struct Triangle;

/** @brief Closed convex polygon stored by its vertices. */
template <class PointType>
struct Convex;

/** @brief Closed simple polygon stored by its vertices. */
template <class PointType>
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
template <class PointType>
inline constexpr int shapeRank<OrientedSegment<PointType>> = 30;
template <class PointType>
inline constexpr int shapeRank<Line<PointType>> = 40;
template <class PointType>
inline constexpr int shapeRank<OrientedLine<PointType>> = 50;
template <class PointType>
inline constexpr int shapeRank<Ray<PointType>> = 60;
template <class PointType>
inline constexpr int shapeRank<Halfplane<PointType>> = 70;
template <class PointType>
inline constexpr int shapeRank<Rectangle<PointType>> = 80;
template <class PointType>
inline constexpr int shapeRank<Triangle<PointType>> = 90;
template <class PointType, class Label>
inline constexpr int shapeRank<Disk<PointType, Label>> = 100;
template <class PointType>
inline constexpr int shapeRank<Convex<PointType>> = 110;
template <class PointType>
inline constexpr int shapeRank<Polygon<PointType>> = 120;

}  // namespace detail

}  // namespace pgl
