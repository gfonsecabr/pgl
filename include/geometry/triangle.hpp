#pragma once

#ifndef PGL_HPP_INCLUDED
#error "Do not include this Pangolin header directly; include \"pgl.hpp\" instead."
#endif

/**
 * @file triangle.hpp
 * @brief Public declaration of pgl::Triangle.
 *
 * Triangle is the smallest polygonal 2D primitive and anchors finite-area,
 * vertex-based, and edge-based operations in the current library.
 */

#include <array>
#include <cassert>
#include <compare>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <ostream>
#include <type_traits>
#include <utility>


namespace pgl {

template <class PointType = Point<>>
struct Triangle;

Triangle() -> Triangle<Point<>>;

template <class PointType>
Triangle(PointType, PointType, PointType) -> Triangle<PointType>;

template <class Number>
Triangle(Number, Number, Number, Number, Number, Number) -> Triangle<Point<Number>>;

/**
 * @brief Closed triangle represented by three canonicalized vertices.
 *
 * The first stored vertex is the lexicographically smallest one. The other two
 * vertices are ordered so that non-degenerate triangles are stored in
 * counterclockwise order. Degenerate triangles fall back to lexicographic
 * ordering of all three vertices.
 *
 * @tparam PointType_ Vertex point type.
 */
template <class PointType_>
struct Triangle {
    /** Type of the triangle vertices. */
    using PointType = PointType_;
    /** Type of the vertex coordinates. */
    using NumberType = PointType::NumberType;
    // using LabelType = detail::point_label_t<PointType>;

    /** Standard range/container typedefs over the vertex sequence. */
    using value_type = PointType;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference = const PointType&;
    using const_reference = const PointType&;
    using iterator = typename std::array<PointType, 3>::const_iterator;
    using const_iterator = iterator;

    template <bool Oriented>
    using BoundaryType = std::conditional_t<Oriented, OrientedSegment<PointType>, Segment<PointType>>;

    template <bool Oriented>
    class BoundaryIterator;

    using EdgeIterator = BoundaryIterator<false>;
    using OrientedEdgeIterator = BoundaryIterator<true>;

    static_assert(detail::is_point_v<PointType>, "Triangle requires pgl::Point vertices");

    /**
     * @brief Creates the degenerate triangle `(0,0),(0,0),(0,0)`.
     */
    constexpr Triangle() = default;

    /**
     * @brief Creates a triangle from three vertices.
     *
     * The stored representation is canonicalized to match the documented
     * Pangolin order.
     *
     * @param first First vertex.
     * @param second Second vertex.
     * @param third Third vertex.
     */
    constexpr Triangle(PointType first, PointType second, PointType third)
        : points_(canonicalizeVertices(std::move(first), std::move(second), std::move(third))) {}

    /**
     * @brief Creates a triangle from six coordinates.
     *
     * @param x1 X coordinate of the first vertex.
     * @param y1 Y coordinate of the first vertex.
     * @param x2 X coordinate of the second vertex.
     * @param y2 Y coordinate of the second vertex.
     * @param x3 X coordinate of the third vertex.
     * @param y3 Y coordinate of the third vertex.
     */
    constexpr Triangle(NumberType x1, NumberType y1, NumberType x2, NumberType y2, NumberType x3, NumberType y3)
        : Triangle(PointType(x1, y1), PointType(x2, y2), PointType(x3, y3)) {}

    /**
     * @brief Converts a triangle with compatible vertex type.
     *
     * @tparam OtherPointType Source vertex type.
     * @param other Source triangle.
     */
    template<PointConcept OtherPointType>
        requires(std::constructible_from<PointType, const OtherPointType&>)
    constexpr Triangle(const Triangle<OtherPointType>& other)
        : Triangle(PointType(other.a()), PointType(other.b()), PointType(other.c())) {}

    /**
     * @brief Assigns from a triangle with compatible vertex type.
     *
     * @tparam OtherPointType Source vertex type.
     * @param other Source triangle.
     * @return This triangle.
     */
    template<PointConcept OtherPointType>
        requires(std::constructible_from<PointType, const OtherPointType&>)
    constexpr Triangle& operator=(const Triangle<OtherPointType>& other) {
        points_ = canonicalizeVertices(
            PointType(other.a()),
            PointType(other.b()),
            PointType(other.c()));
        return *this;
    }

    /**
     * @brief Returns vertex `0`, `1`, or `2`.
     *
     * @param index Vertex index.
     * @return Reference to the selected vertex.
     */
    constexpr const PointType& operator[](std::size_t index) const {
        assert(index < size());
        return points_[index];
    }

    /**
     * @brief Returns the number of vertices (always 3).
     */
    static constexpr std::size_t size() {
        return 3;
    }

    /**
     * @brief Cyclic access: same as @ref operator[] but `index` is taken
     * modulo @ref size(); negative indices wrap from the end.
     */
    constexpr const PointType& get(std::ptrdiff_t index) const {
        const std::ptrdiff_t n = static_cast<std::ptrdiff_t>(size());
        return (*this)[static_cast<std::size_t>(((index % n) + n) % n)];
    }

    /**
     * @brief Returns the smallest index `i` with `(*this)[i] == point`, or
     * `-1` if no vertex equals `point`.
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
     * @brief Returns the first vertex.
     *
     * @return Reference to the first vertex.
     */
    constexpr const PointType& a() const {
        return points_[0];
    }

    /**
     * @brief Returns the second vertex.
     *
     * @return Reference to the second vertex.
     */
    constexpr const PointType& b() const {
        return points_[1];
    }

    /**
     * @brief Returns the third vertex.
     *
     * @return Reference to the third vertex.
     */
    constexpr const PointType& c() const {
        return points_[2];
    }

    /**
     * @brief Returns an iterator to the first vertex.
     *
     * @return Const iterator to the first vertex.
     */
    constexpr auto begin() const {
        return points_.cbegin();
    }

    /**
     * @brief Returns an iterator to the first vertex.
     *
     * @return Const iterator to the first vertex.
     */
    constexpr auto cbegin() const {
        return points_.cbegin();
    }

    /**
     * @brief Returns an iterator past the last vertex.
     *
     * @return Const iterator past the last vertex.
     */
    constexpr auto end() const {
        return points_.cend();
    }

    /**
     * @brief Returns an iterator past the last vertex.
     *
     * @return Const iterator past the last vertex.
     */
    constexpr auto cend() const {
        return points_.cend();
    }

    /**
     * @brief Compares triangles lexicographically by canonical vertices.
     *
     * @param other Triangle to compare with.
     * @return Comparison result.
     */
    constexpr auto operator<=>(const Triangle& other) const = default;

    /**
     * @brief Returns twice the area of the triangle.
     *
     * @return Twice the non-negative area.
     */
    [[nodiscard]] constexpr NumberType twiceArea() const;

    /**
     * @brief Returns the non-negative area of the triangle.
     *
     * Integral coordinates produce an exact rational half-area when needed.
     *
     * @warning Uses division by 2, so may not be exact for integral types.
     * @return Absolute triangle area.
     */
    template <class ResultNumber = NumberType>
    [[nodiscard]] constexpr ResultNumber area() const;

    /**
     * @brief Tests whether the three vertices are collinear.
     *
     * @return `true` when @ref twiceArea is zero.
     */
    [[nodiscard]] constexpr bool isDegenerate() const;

    /**
     * @brief Returns the axis-aligned bounding box of the vertices.
     *
     * @return Minimum rectangle containing all three vertices.
     */
    [[nodiscard]] constexpr Rectangle<PointType> bbox() const;

    /**
     * @brief Returns a floating-point bounding box containing the triangle.
     *
     * @tparam ResultNumber Floating-point coordinate type.
     * @return Floating-point rectangle containing the triangle.
     */
    template <std::floating_point ResultNumber = double>
    [[nodiscard]] constexpr Rectangle<Point<ResultNumber>> fbox() const;

    /**
     * @brief Returns the vertices in canonical order.
     *
     * @return Array `{a(), b(), c()}`.
     */
    [[nodiscard]] constexpr std::array<PointType, 3> vertices() const;

    /**
     * @brief Returns the three unoriented boundary edges.
     *
     * @return Edges `(a,b)`, `(b,c)`, and `(c,a)`.
     */
    [[nodiscard]] constexpr std::array<Segment<PointType>, 3> edges() const;

    /**
     * @brief Returns an iterator to the first unoriented edge.
     *
     * @return Iterator to edge `(a,b)`.
     */
    constexpr EdgeIterator edgesBegin() const {
        return EdgeIterator(this, 0);
    }

    /**
     * @brief Returns an iterator past the last unoriented edge.
     *
     * @return Sentinel iterator for @ref edgesBegin().
     */
    constexpr EdgeIterator edgesEnd() const {
        return EdgeIterator(this, edgeCount);
    }

    /**
     * @brief Returns the three oriented boundary edges.
     *
     * @return Oriented edges `a->b`, `b->c`, and `c->a`.
     */
    [[nodiscard]] constexpr std::array<OrientedSegment<PointType>, 3> orientedEdges() const;

    /**
     * @brief Returns an iterator to the first oriented edge.
     *
     * @return Iterator to edge `a->b`.
     */
    constexpr OrientedEdgeIterator orientedEdgesBegin() const {
        return OrientedEdgeIterator(this, 0);
    }

    /**
     * @brief Returns an iterator past the last oriented edge.
     *
     * @return Sentinel iterator for @ref orientedEdgesBegin().
     */
    constexpr OrientedEdgeIterator orientedEdgesEnd() const {
        return OrientedEdgeIterator(this, edgeCount);
    }

    /**
     * @brief Converts the triangle to a convex polygon.
     *
     * The three vertices already follow the canonical convex order
     * (counterclockwise, lexicographically smallest first), and degenerate
     * triangles collapse to their hull.
     *
     * @return Convex polygon with the same vertices.
     */
    [[nodiscard]] constexpr explicit operator Convex<PointType>() const {
        return Convex<PointType>(*this, !isDegenerate());
    }

    /**
     * @brief Returns the triangle as a convex polygon.
     *
     * @return Convex polygon with the same vertices.
     */
    [[nodiscard]] constexpr Convex<PointType> asConvex() const {
        return static_cast<Convex<PointType>>(*this);
    }

    /**
     * @brief Converts the triangle to a simple polygon.
     *
     * The three vertices already follow the canonical polygon order
     * (counterclockwise, lexicographically smallest first).
     *
     * @return Polygon with the same vertices.
     */
    [[nodiscard]] constexpr explicit operator Polygon<PointType>() const {
        return Polygon<PointType>(*this, !isDegenerate());
    }

    /**
     * @brief Returns the triangle as a simple polygon.
     *
     * @return Polygon with the same vertices.
     */
    [[nodiscard]] constexpr Polygon<PointType> asPolygon() const {
        return static_cast<Polygon<PointType>>(*this);
    }

    /**
     * @brief Returns the triangle rotated by 90k degrees around the origin.
     *
     * @param k Number of 90-degree CCW rotations (may be negative).
     * @return Rotated triangle.
     */
    [[nodiscard]] constexpr Triangle rotated90(int k = 1) const;

    /**
     * @brief Rotates the triangle by 90k degrees around the origin in place.
     *
     * @param k Number of 90-degree CCW rotations (may be negative).
     */
    constexpr void rotate90(int k = 1);

    /** @brief Returns the triangle with its x-coordinates multiplied by a factor. */
    template <class OtherNumber>
    [[nodiscard]] constexpr Triangle scaledUpX(const OtherNumber scalar) const;

    /** @brief Multiplies the triangle's x-coordinates by a factor in place. */
    template <class OtherNumber>
    constexpr void scaleUpX(const OtherNumber scalar);

    /** @brief Returns the triangle with its y-coordinates multiplied by a factor. */
    template <class OtherNumber>
    [[nodiscard]] constexpr Triangle scaledUpY(const OtherNumber scalar) const;

    /** @brief Multiplies the triangle's y-coordinates by a factor in place. */
    template <class OtherNumber>
    constexpr void scaleUpY(const OtherNumber scalar);

    /** @brief Returns the triangle with its x-coordinates divided by a divisor. */
    template <class OtherNumber>
    [[nodiscard]] constexpr Triangle scaledDownX(const OtherNumber scalar) const;

    /** @brief Divides the triangle's x-coordinates by a divisor in place. */
    template <class OtherNumber>
    constexpr void scaleDownX(const OtherNumber scalar);

    /** @brief Returns the triangle with its y-coordinates divided by a divisor. */
    template <class OtherNumber>
    [[nodiscard]] constexpr Triangle scaledDownY(const OtherNumber scalar) const;

    /** @brief Divides the triangle's y-coordinates by a divisor in place. */
    template <class OtherNumber>
    constexpr void scaleDownY(const OtherNumber scalar);

    /**
     * @brief Returns the arithmetic centroid.
     *
     * @tparam ResultNumber Coordinate type of the returned point.
     * @return Point `((ax+bx+cx)/3, (ay+by+cy)/3)`.
     */
    template <class ResultNumber = NumberType>
    [[nodiscard]] constexpr Point<ResultNumber> centroid() const;

    /**
     * @brief Returns the circumcircle of the triangle.
     *
     * The returned disk stores the three triangle vertices as boundary points.
     *
     * @return Disk passing through the three vertices.
     */
    [[nodiscard]] constexpr Disk<PointType, NoLabel> circumcircle() const;

    /**
     * @brief Returns a segment defining the diameter.
     *
     * For a triangle, the diameter is its longest side.
     *
     * @return A longest boundary edge.
     */
    [[nodiscard]] constexpr Segment<PointType> diameter() const;

    /**
     * @brief Returns a point inside the triangle.
     *
     * @tparam ResultNumber Coordinate type of the returned point.
     * @return A point strictly inside every non-degenerate triangle.
     * @warning For degenerate triangles, the point may be on the boundary. Uses division by 4, so may not be exact for integral types.
     */
    template <class ResultNumber = NumberType>
    [[nodiscard]] constexpr Point<ResultNumber> pointInside() const;

    /**
     * @brief Tests whether the triangle has a right angle.
     *
     * @return `true` if one angle is exactly 90 degrees.
     */
    [[nodiscard]] constexpr bool isRectangle() const;

    /**
     * @brief Tests whether the triangle has an obtuse angle.
     *
     * @return `true` if one angle is strictly greater than 90 degrees.
     */
    [[nodiscard]] constexpr bool isObtuse() const;

    /**
     * @brief Tests whether two sides have the same length.
     *
     * @return `true` if at least one pair of side lengths is equal.
     */
    [[nodiscard]] constexpr bool isIsosceles() const;

    /**
     * @brief Tests whether a point equals one of the vertices.
     *
     * @tparam OtherPoint Point type.
     * @param point Point to test.
     * @return `true` if the point is one of `a()`, `b()`, or `c()`.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool verticesContain(const OtherPoint& point) const;

    /**
     * @brief Tests whether a point lies on a boundary edge.
     *
     * @tparam OtherPoint Point type.
     * @param point Point to test.
     * @return `true` if the point lies on any edge.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool boundaryContains(const OtherPoint& point) const;

    /** @brief Tests whether a segment lies entirely on the boundary. */
    template<SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool boundaryContains(const OtherSegment& other) const;

    /** @brief Tests whether an oriented segment lies entirely on the boundary. */
    template<OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool boundaryContains(const OtherOrientedSegment& other) const;

    /** @brief Degenerate lines are on the boundary iff their unique point is. */
    template<LineConcept OtherLine>
    [[nodiscard]] constexpr bool boundaryContains(const OtherLine& other) const;

    /** @brief Degenerate oriented lines are on the boundary iff their unique point is. */
    template<OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool boundaryContains(const OtherOrientedLine& other) const;

    /** @brief Degenerate rays are on the boundary iff their source point is. */
    template<RayConcept OtherRay>
    [[nodiscard]] constexpr bool boundaryContains(const OtherRay& other) const;

    /** @brief Degenerate half-planes are on the boundary iff their source point is. */
    template<HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool boundaryContains(const OtherHalfplane& other) const;

    /** @brief Tests whether every rectangle vertex lies on the boundary. */
    template<RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool boundaryContains(const OtherRectangle& other) const;

    /** @brief Tests whether every triangle vertex lies on the boundary. */
    template<TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr bool boundaryContains(const OtherTriangle& other) const;

    /**
     * @brief Tests whether a point lies in the closed triangle.
     *
     * Degenerate triangles use boundary containment.
     *
     * @tparam OtherPoint Point type.
     * @param point Point to test.
     * @return `true` if the point is contained.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool contains(const OtherPoint& point) const;

    /** @brief Tests whether both endpoints of a segment lie in the closed triangle. */
    template<SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool contains(const OtherSegment& other) const;

    /** @brief Tests whether both endpoints of an oriented segment lie in the closed triangle. */
    template<OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool contains(const OtherOrientedSegment& other) const;

    /** @brief Degenerate lines are contained iff their unique point is. */
    template<LineConcept OtherLine>
    [[nodiscard]] constexpr bool contains(const OtherLine& other) const;

    /** @brief Degenerate oriented lines are contained iff their unique point is. */
    template<OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool contains(const OtherOrientedLine& other) const;

    /** @brief Degenerate rays are contained iff their source point is. */
    template<RayConcept OtherRay>
    [[nodiscard]] constexpr bool contains(const OtherRay& other) const;

    /** @brief Degenerate half-planes are contained iff their source point is. */
    template<HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool contains(const OtherHalfplane& other) const;

    /** @brief Tests whether every rectangle vertex lies in the closed triangle. */
    template<RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool contains(const OtherRectangle& other) const;

    /** @brief Tests whether every vertex of another triangle lies in the closed triangle. */
    template<TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr bool contains(const OtherTriangle& other) const;

    template<ConvexConcept OtherConvex>
    [[nodiscard]] constexpr bool contains(const OtherConvex& other) const;

    /** @brief Polygon overload; see the Convex overload. */
    template<PolygonConcept OtherPolygon>
    [[nodiscard]] constexpr bool contains(const OtherPolygon& other) const;

    [[nodiscard]] constexpr bool contains(const Shape<PointType>& other) const;

    [[nodiscard]] constexpr bool boundaryContains(const Shape<PointType>& other) const;

    // The empty set is a subset of every shape (contained in all of them) and
    // disjoint from all of them, so containment is true while separation is
    // false. These overloads let an EmptyShape flow through Shape's variant
    // dispatch without special-casing.
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool contains(const EmptyShape<EmptyPoint>&) const {
        return true;
    }
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool boundaryContains(const EmptyShape<EmptyPoint>&) const {
        return true;
    }
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool interiorContains(const EmptyShape<EmptyPoint>&) const {
        return true;
    }
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool separates(const EmptyShape<EmptyPoint>&) const {
        return false;
    }

    /**
     * @brief Tests whether a point lies in the strict interior.
     *
     * @tparam OtherPoint Point type.
     * @param point Point to test.
     * @return `true` if the point is contained and not on the boundary.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorContains(const OtherPoint& point) const;

    /** @brief Tests whether both endpoints of a segment lie in the strict interior. */
    template<SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool interiorContains(const OtherSegment& other) const;

    /** @brief Tests whether both endpoints of an oriented segment lie in the strict interior. */
    template<OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool interiorContains(const OtherOrientedSegment& other) const;

    /** @brief Degenerate lines are in the interior iff their unique point is. */
    template<LineConcept OtherLine>
    [[nodiscard]] constexpr bool interiorContains(const OtherLine& other) const;

    /** @brief Degenerate oriented lines are in the interior iff their unique point is. */
    template<OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool interiorContains(const OtherOrientedLine& other) const;

    /** @brief Degenerate rays are in the interior iff their source point is. */
    template<RayConcept OtherRay>
    [[nodiscard]] constexpr bool interiorContains(const OtherRay& other) const;

    /** @brief Degenerate half-planes are in the interior iff their source point is. */
    template<HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool interiorContains(const OtherHalfplane& other) const;

    /** @brief Tests whether every rectangle vertex lies in the strict interior. */
    template<RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool interiorContains(const OtherRectangle& other) const;

    /** @brief Tests whether every vertex of another triangle lies in the strict interior. */
    template<TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr bool interiorContains(const OtherTriangle& other) const;

    /**
     * @brief Tests whether the triangle intersects a point.
     *
     * @tparam OtherPoint Point type.
     * @param other Point to test.
     * @return `true` if the point is contained.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool intersects(const OtherPoint& other) const;

    /** @brief Tests whether the triangle intersects a line. */
    template<LineConcept OtherLine>
    [[nodiscard]] constexpr bool intersects(const OtherLine& other) const;

    /** @brief Tests whether the triangle intersects an oriented line. */
    template<OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool intersects(const OtherOrientedLine& other) const;

    /** @brief Tests whether the triangle intersects a segment. */
    template<SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool intersects(const OtherSegment& other) const;

    /** @brief Tests whether the triangle intersects an oriented segment. */
    template<OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool intersects(const OtherOrientedSegment& other) const;

    /** @brief Tests whether the triangle intersects a ray. */
    template<RayConcept OtherRay>
    [[nodiscard]] constexpr bool intersects(const OtherRay& other) const;

    /** @brief Tests whether the triangle intersects a half-plane. */
    template<HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool intersects(const OtherHalfplane& other) const;

    /** @brief Tests whether the triangle intersects a rectangle. */
    template<RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool intersects(const OtherRectangle& other) const;

    /** @brief Tests whether the triangle intersects another triangle. */
    template<TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr bool intersects(const OtherTriangle& other) const;

    [[nodiscard]] constexpr bool intersects(const Shape<PointType>& other) const;

    template<typename OtherShape>
        requires (!PointConcept<OtherShape> && detail::shapeRank<OtherShape> > detail::shapeRank<Triangle>)
    [[nodiscard]] constexpr bool intersects(const OtherShape& other) const {
        return other.intersects(*this);
    }

    /** @brief The empty set never meets another shape. */
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool intersects(const EmptyShape<EmptyPoint>&) const {
        return false;
    }

    /** @brief Tests whether a point lies in the strict triangle interior. */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherPoint& other) const;

    /** @brief Tests whether a line meets the strict triangle interior. */
    template<LineConcept OtherLine>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherLine& other) const;

    /** @brief Tests whether an oriented line meets the strict triangle interior. */
    template<OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherOrientedLine& other) const;

    /** @brief Tests whether a segment interior meets the strict triangle interior. */
    template<SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherSegment& other) const;

    /** @brief Tests whether an oriented segment interior meets the strict triangle interior. */
    template<OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherOrientedSegment& other) const;

    /** @brief Tests whether a ray interior meets the strict triangle interior. */
    template<RayConcept OtherRay>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherRay& other) const;

    /** @brief Tests whether a half-plane interior meets the strict triangle interior. */
    template<HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherHalfplane& other) const;

    /** @brief Tests whether a rectangle interior meets the strict triangle interior. */
    template<RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherRectangle& other) const;

    /** @brief Tests whether two triangle interiors overlap. */
    template<TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherTriangle& other) const;

    template<typename OtherShape>
        requires (!PointConcept<OtherShape> && detail::shapeRank<OtherShape> > detail::shapeRank<Triangle>)
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherShape& other) const {
        return other.interiorsIntersect(*this);
    }

    /** @brief The empty set never meets another shape. */
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool interiorsIntersect(const EmptyShape<EmptyPoint>&) const {
        return false;
    }

    [[nodiscard]] constexpr bool interiorsIntersect(const Shape<PointType>& other) const;

    /** @brief Tests whether the triangle separates a segment. */
    template<SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool separates(const OtherSegment& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool separates(const OtherPoint& other) const;

    /** @brief Tests whether the triangle separates an oriented segment. */
    template<OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool separates(const OtherOrientedSegment& other) const;

    /** @brief Tests whether the triangle separates a line. */
    template<LineConcept OtherLine>
    [[nodiscard]] constexpr bool separates(const OtherLine& other) const;

    /** @brief Tests whether the triangle separates an oriented line. */
    template<OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool separates(const OtherOrientedLine& other) const;

    /** @brief Tests whether the triangle separates a ray. */
    template<RayConcept OtherRay>
    [[nodiscard]] constexpr bool separates(const OtherRay& other) const;

    /** @brief Triangles never separate half-planes in this API. */
    template<HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool separates(const OtherHalfplane& other) const;

    /** @brief Tests whether the triangle separates a rectangle. */
    template<RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool separates(const OtherRectangle& other) const;

    /** @brief Tests whether the triangle separates another triangle. */
    template<TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr bool separates(const OtherTriangle& other) const;

    template<ConvexConcept OtherConvex>
    [[nodiscard]] constexpr bool separates(const OtherConvex& other) const;

    /** @brief Polygon overload; see the Convex overload. */
    template<PolygonConcept OtherPolygon>
    [[nodiscard]] constexpr bool separates(const OtherPolygon& other) const;

    [[nodiscard]] constexpr bool separates(const Shape<PointType>& other) const;

    // --- not-yet-implemented predicate pairs (throw); see implementation ---
    template<DiskConcept OtherDisk>
    [[nodiscard]] constexpr bool boundaryContains(const OtherDisk& other) const;

    template<ConvexConcept OtherConvex>
    [[nodiscard]] constexpr bool boundaryContains(const OtherConvex& other) const;

    template<PolygonConcept OtherPolygon>
    [[nodiscard]] constexpr bool boundaryContains(const OtherPolygon& other) const;

    template<DiskConcept OtherDisk>
    [[nodiscard]] constexpr bool interiorContains(const OtherDisk& other) const;

    template<ConvexConcept OtherConvex>
    [[nodiscard]] constexpr bool interiorContains(const OtherConvex& other) const;

    template<PolygonConcept OtherPolygon>
    [[nodiscard]] constexpr bool interiorContains(const OtherPolygon& other) const;


    /** @brief Tests whether the triangle crosses a segment. */
    template<SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool crosses(const OtherSegment& other) const;

    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool crosses(const OtherPoint& other) const;

    /** @brief Tests whether the triangle crosses an oriented segment. */
    template<OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool crosses(const OtherOrientedSegment& other) const;

    /** @brief Tests whether the triangle crosses a line. */
    template<LineConcept OtherLine>
    [[nodiscard]] constexpr bool crosses(const OtherLine& other) const;

    /** @brief Tests whether the triangle crosses an oriented line. */
    template<OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool crosses(const OtherOrientedLine& other) const;

    /** @brief Tests whether the triangle crosses a ray. */
    template<RayConcept OtherRay>
    [[nodiscard]] constexpr bool crosses(const OtherRay& other) const;

    /** @brief Triangles never cross half-planes in this API. */
    template<HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool crosses(const OtherHalfplane& other) const;

    /** @brief Tests whether the triangle crosses a rectangle. */
    template<RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool crosses(const OtherRectangle& other) const;

    /** @brief Tests whether the triangle crosses another triangle. */
    template<TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr bool crosses(const OtherTriangle& other) const;

    template<typename OtherShape>
        requires (!PointConcept<OtherShape> && detail::shapeRank<OtherShape> > detail::shapeRank<Triangle>)
    [[nodiscard]] constexpr bool crosses(const OtherShape& other) const {
        return other.crosses(*this);
    }

    /** @brief The empty set never meets another shape. */
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool crosses(const EmptyShape<EmptyPoint>&) const {
        return false;
    }

    [[nodiscard]] constexpr bool crosses(const Shape<PointType>& other) const;

    /**
     * @brief Returns the point intersection when a point lies in the triangle.
     *
     * @tparam ResultNumber Coordinate type of the returned point.
     * @tparam OtherPoint Point type.
     * @param other Point to intersect with.
     * @return The point when contained, otherwise empty.
     */
    template <class ResultNumber = NumberType, PointConcept OtherPoint>
    [[nodiscard]] constexpr std::optional<Point<ResultNumber, typename PointType::LabelType>>
    intersection(const OtherPoint& other) const;

    /** @brief Returns the intersection with a line as a point or segment. */
    template <class ResultNumber = NumberType, LineConcept OtherLine>
    [[nodiscard]] constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherLine& other) const;

    /** @brief Returns the intersection with an oriented line as a point or segment. */
    template <class ResultNumber = NumberType, OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherOrientedLine& other) const;

    /** @brief Returns the intersection with a segment as a point or segment. */
    template <class ResultNumber = NumberType, SegmentConcept OtherSegment>
    [[nodiscard]] constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherSegment& other) const;

    /** @brief Returns the intersection with an oriented segment as a point or segment. */
    template <class ResultNumber = NumberType, OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherOrientedSegment& other) const;

    /** @brief Returns the intersection with a ray as a point or segment. */
    template <class ResultNumber = NumberType, RayConcept OtherRay>
    [[nodiscard]] constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherRay& other) const;

    template <class ResultNumber = NumberType, HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr auto intersection(const OtherHalfplane& other) const;

    template <class ResultNumber = NumberType, RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr auto intersection(const OtherRectangle& other) const;

    template <class ResultNumber = NumberType, TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr auto intersection(const OtherTriangle& other) const;

    template <class ResultNumber = NumberType, typename OtherShape>
        requires (!PointConcept<OtherShape>
                  && (detail::shapeRank<OtherShape> > detail::shapeRank<Triangle>)
                  && requires(const OtherShape& o, const Triangle& self) {
                         o.template intersection<ResultNumber>(self);
                     })
    [[nodiscard]] constexpr auto intersection(const OtherShape& other) const {
        return other.template intersection<ResultNumber>(*this);
    }

    /** @brief Intersecting with the empty set yields the empty set. */
    template <class ResultNumber = NumberType, class EmptyPoint>
    [[nodiscard]] constexpr EmptyShape<EmptyPoint> intersection(const EmptyShape<EmptyPoint>&) const {
        return {};
    }

    /** @brief Translates all vertices by a point in place. */
    template<PointConcept OtherPoint>
    constexpr Triangle& operator+=(const OtherPoint& translation);

    /** @brief Translates all vertices by the opposite of a point in place. */
    template<PointConcept OtherPoint>
    constexpr Triangle& operator-=(const OtherPoint& translation);

    /** @brief Scales all vertices by a scalar in place. */
    template <class Scalar>
        requires(!detail::is_point_v<Scalar>)
    constexpr Triangle& operator*=(const Scalar& scalar);

    /** @brief Divides all vertices by a scalar in place. */
    template <class Scalar>
        requires(!detail::is_point_v<Scalar>)
    constexpr Triangle& operator/=(const Scalar& scalar);

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
            assert(triangle != nullptr);
            return triangle->template boundaryAt<Oriented>(index);
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
        friend struct Triangle;

        constexpr BoundaryIterator(const Triangle* triangle_arg, std::size_t index_arg)
            : triangle(triangle_arg), index(index_arg) {}

        const Triangle* triangle = nullptr;
        std::size_t index = 0;
    };

  private:
    static constexpr std::size_t edgeCount = 3;

    static constexpr std::array<PointType, 3> canonicalizeVertices(PointType first, PointType second, PointType third) {
        std::array<PointType, 3> vertices{
            std::move(first),
            std::move(second),
            std::move(third),
        };

        if (vertices[1] < vertices[0]) {
            std::swap(vertices[0], vertices[1]);
        }
        if (vertices[2] < vertices[1]) {
            std::swap(vertices[1], vertices[2]);
        }
        if (vertices[1] < vertices[0]) {
            std::swap(vertices[0], vertices[1]);
        }

        if (orientationDeterminant(vertices[0], vertices[1], vertices[2]) < 0) {
            std::swap(vertices[1], vertices[2]);
        }

        return vertices;
    }

    constexpr void normalize() {
        points_ = canonicalizeVertices(points_[0], points_[1], points_[2]);
    }

    template <bool Oriented>
    constexpr BoundaryType<Oriented> boundaryAt(std::size_t index) const {
        assert(index < edgeCount);

        switch (index) {
        case 0:
            return BoundaryType<Oriented>(a(), b());
        case 1:
            return BoundaryType<Oriented>(b(), c());
        default:
            return BoundaryType<Oriented>(c(), a());
        }
    }

    std::array<PointType, 3> points_{};
};

/** @brief Returns a translated copy of a triangle. */
template <class PointType, class TranslationNumber, class TranslationLabel>
constexpr auto operator+(const Triangle<PointType>& triangle, const Point<TranslationNumber, TranslationLabel>& translation);

/** @brief Returns a translated copy of a triangle with the translation on the left. */
template <class TranslationNumber, class TranslationLabel, class PointType>
constexpr auto operator+(const Point<TranslationNumber, TranslationLabel>& translation, const Triangle<PointType>& triangle);

/** @brief Returns a copy of a triangle translated by the opposite point. */
template <class PointType, class TranslationNumber, class TranslationLabel>
constexpr auto operator-(const Triangle<PointType>& triangle, const Point<TranslationNumber, TranslationLabel>& translation);

/** @brief Returns a scaled copy of a triangle. */
template <class PointType, class Scalar>
    requires(!detail::is_point_v<Scalar>)
constexpr auto operator*(const Triangle<PointType>& triangle, const Scalar& scalar);

/** @brief Returns a scaled copy of a triangle with the scalar on the left. */
template <class Scalar, class PointType>
    requires(!detail::is_point_v<Scalar>)
constexpr auto operator*(const Scalar& scalar, const Triangle<PointType>& triangle);

/** @brief Returns a copy of a triangle divided by a scalar. */
template <class PointType, class Scalar>
    requires(!detail::is_point_v<Scalar>)
constexpr auto operator/(const Triangle<PointType>& triangle, const Scalar& scalar);

/** @brief Streams a triangle as `<abc>` using canonical vertex order. */
template <class PointType>
std::ostream& operator<<(std::ostream& stream, const Triangle<PointType>& triangle);

}  // namespace pgl
