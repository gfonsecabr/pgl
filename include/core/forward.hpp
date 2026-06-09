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

}  // namespace pgl
