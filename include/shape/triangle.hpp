#pragma once

#include "shape/rectangle.hpp"

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

template <class PointType = Point<>, class Label>
struct Triangle;

Triangle() -> Triangle<Point<>, NoLabel>;

template <class PointType>
Triangle(PointType, PointType, PointType) -> Triangle<PointType, NoLabel>;

template <class PointType, class A>
Triangle(PointType, PointType, PointType, A) -> Triangle<PointType, std::decay_t<A>>;

template <class Number>
Triangle(Number, Number, Number, Number, Number, Number) -> Triangle<Point<Number>, NoLabel>;

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
template <class PointType_, class TLabel>
struct Triangle {
    /** Type of the triangle vertices. */
    using PointType = PointType_;
    /** Type of the vertex coordinates. */
    using NumberType = PointType::NumberType;
    /** Optional label type carried with the triangle. */
    using LabelType = TLabel;

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
     * @brief Creates a triangle from three vertices and stores a label.
     *
     * The stored vertices are canonicalized as in the unlabeled constructor.
     *
     * @tparam A Type convertible to @ref LabelType.
     */
    template <class A>
        requires(detail::has_label_v<LabelType> && std::constructible_from<LabelType, A&&>)
    constexpr Triangle(PointType first, PointType second, PointType third, A&& label)
        : points_(canonicalizeVertices(std::move(first), std::move(second), std::move(third))),
          label_(std::forward<A>(label)) {}

    /** @brief Same as the six-coordinate constructor, and stores a label. */
    template <class A>
        requires(detail::has_label_v<LabelType> && std::constructible_from<LabelType, A&&>)
    constexpr Triangle(NumberType x1, NumberType y1, NumberType x2, NumberType y2,
                       NumberType x3, NumberType y3, A&& label)
        : Triangle(PointType(x1, y1), PointType(x2, y2), PointType(x3, y3), std::forward<A>(label)) {}

    /**
     * @brief Converts a triangle with compatible vertex type.
     *
     * @tparam OtherPointType Source vertex type.
     * @param other Source triangle.
     */
    template<PointConcept OtherPointType, class OtherLabelType>
        requires(std::constructible_from<PointType, const OtherPointType&>)
    constexpr Triangle(const Triangle<OtherPointType, OtherLabelType>& other)
        : Triangle(PointType(other.a()), PointType(other.b()), PointType(other.c())) {
        label_ = detail::copyLabel<LabelType>(other);
    }

    /**
     * @brief Assigns from a triangle with compatible vertex type.
     *
     * @tparam OtherPointType Source vertex type.
     * @param other Source triangle.
     * @return This triangle.
     */
    template<PointConcept OtherPointType, class OtherLabelType>
        requires(std::constructible_from<PointType, const OtherPointType&>)
    constexpr Triangle& operator=(const Triangle<OtherPointType, OtherLabelType>& other) {
        points_ = canonicalizeVertices(
            PointType(other.a()),
            PointType(other.b()),
            PointType(other.c()));
        label_ = detail::copyLabel<LabelType>(other);
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
    constexpr bool operator==(const Triangle& other) const {
        return points_ == other.points_;
    }

    /** @brief Orders triangles lexicographically by their vertices, ignoring the label. */
    constexpr auto operator<=>(const Triangle& other) const {
        return points_ <=> other.points_;
    }

    /**
     * @brief Returns the triangle label.
     *
     * The label is mutable even through a const triangle: it is metadata that
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
     * @warning Uses division by 3, so the result may be inexact even for floating-point types.
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
     * @warning Divides coordinates by 4. Inexact for integer coordinates not divisible by 4.
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
     * @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B).
     *
     * @tparam OtherPoint Point type.
     * @param point Point to test.
     * @return `true` if the point lies on any edge.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool boundaryContains(const OtherPoint& point) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool boundaryContains(const OtherSegment& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool boundaryContains(const OtherOrientedSegment& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<LineConcept OtherLine>
    [[nodiscard]] constexpr bool boundaryContains(const OtherLine& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool boundaryContains(const OtherOrientedLine& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<RayConcept OtherRay>
    [[nodiscard]] constexpr bool boundaryContains(const OtherRay& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool boundaryContains(const OtherHalfplane& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool boundaryContains(const OtherRectangle& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr bool boundaryContains(const OtherTriangle& other) const;

    /**
     * @brief Tests whether this shape contains the other shape (A ⊇ B).
     *
     * Degenerate triangles use boundary containment.
     *
     * @tparam OtherPoint Point type.
     * @param point Point to test.
     * @return `true` if the point is contained.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool contains(const OtherPoint& point) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template<SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool contains(const OtherSegment& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template<OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool contains(const OtherOrientedSegment& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template<LineConcept OtherLine>
    [[nodiscard]] constexpr bool contains(const OtherLine& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template<OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool contains(const OtherOrientedLine& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template<RayConcept OtherRay>
    [[nodiscard]] constexpr bool contains(const OtherRay& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template<HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool contains(const OtherHalfplane& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template<RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool contains(const OtherRectangle& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template<TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr bool contains(const OtherTriangle& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template<ConvexConcept OtherConvex>
    [[nodiscard]] constexpr bool contains(const OtherConvex& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template<PolygonConcept OtherPolygon>
    [[nodiscard]] constexpr bool contains(const OtherPolygon& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    template<DiskConcept OtherDisk>
    [[nodiscard]] constexpr bool contains(const OtherDisk& other) const;

    /** @brief Tests whether this shape contains the other shape (A ⊇ B). */
    [[nodiscard]] constexpr bool contains(const Shape<PointType>& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    [[nodiscard]] constexpr bool boundaryContains(const Shape<PointType>& other) const;

    // The empty set is a subset of every shape (contained in all of them) and
    // disjoint from all of them, so containment is true while separation is
    // false. These overloads let an EmptyShape flow through Shape's variant
    // dispatch without special-casing.
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
    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool separates(const EmptyShape<EmptyPoint>&) const {
        return false;
    }

    /**
     * @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B).
     *
     * @tparam OtherPoint Point type.
     * @param point Point to test.
     * @return `true` if the point is contained and not on the boundary.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorContains(const OtherPoint& point) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template<SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool interiorContains(const OtherSegment& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template<OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool interiorContains(const OtherOrientedSegment& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template<LineConcept OtherLine>
    [[nodiscard]] constexpr bool interiorContains(const OtherLine& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template<OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool interiorContains(const OtherOrientedLine& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template<RayConcept OtherRay>
    [[nodiscard]] constexpr bool interiorContains(const OtherRay& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template<HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool interiorContains(const OtherHalfplane& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template<RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool interiorContains(const OtherRectangle& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template<TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr bool interiorContains(const OtherTriangle& other) const;

    /**
     * @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅).
     *
     * @tparam OtherPoint Point type.
     * @param other Point to test.
     * @return `true` if the point is contained.
     */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool intersects(const OtherPoint& other) const;

    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template<LineConcept OtherLine>
    [[nodiscard]] constexpr bool intersects(const OtherLine& other) const;

    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template<OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool intersects(const OtherOrientedLine& other) const;

    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template<SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool intersects(const OtherSegment& other) const;

    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template<OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool intersects(const OtherOrientedSegment& other) const;

    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template<RayConcept OtherRay>
    [[nodiscard]] constexpr bool intersects(const OtherRay& other) const;

    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template<HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool intersects(const OtherHalfplane& other) const;

    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template<RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool intersects(const OtherRectangle& other) const;

    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template<TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr bool intersects(const OtherTriangle& other) const;

    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    [[nodiscard]] constexpr bool intersects(const Shape<PointType>& other) const;

    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template<typename OtherShape>
        requires (!PointConcept<OtherShape> && detail::shapeRank<OtherShape> > detail::shapeRank<Triangle>)
    [[nodiscard]] constexpr bool intersects(const OtherShape& other) const {
        return other.intersects(*this);
    }

    /** @brief Tests whether this shape and the other shape intersect (A ∩ B ≠ ∅). */
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool intersects(const EmptyShape<EmptyPoint>&) const {
        return false;
    }

    /** @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅). */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherPoint& other) const;

    /** @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅). */
    template<LineConcept OtherLine>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherLine& other) const;

    /** @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅). */
    template<OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherOrientedLine& other) const;

    /** @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅). */
    template<SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherSegment& other) const;

    /** @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅). */
    template<OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherOrientedSegment& other) const;

    /** @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅). */
    template<RayConcept OtherRay>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherRay& other) const;

    /** @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅). */
    template<HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherHalfplane& other) const;

    /** @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅). */
    template<RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherRectangle& other) const;

    /** @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅). */
    template<TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherTriangle& other) const;

    /** @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅). */
    template<typename OtherShape>
        requires (!PointConcept<OtherShape> && detail::shapeRank<OtherShape> > detail::shapeRank<Triangle>)
    [[nodiscard]] constexpr bool interiorsIntersect(const OtherShape& other) const {
        return other.interiorsIntersect(*this);
    }

    /** @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅). */
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool interiorsIntersect(const EmptyShape<EmptyPoint>&) const {
        return false;
    }

    /** @brief Tests whether the interiors of the two shapes intersect ((A∖∂A) ∩ (B∖∂B) ≠ ∅). */
    [[nodiscard]] constexpr bool interiorsIntersect(const Shape<PointType>& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template<SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool separates(const OtherSegment& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool separates(const OtherPoint& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template<OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool separates(const OtherOrientedSegment& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template<LineConcept OtherLine>
    [[nodiscard]] constexpr bool separates(const OtherLine& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template<OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool separates(const OtherOrientedLine& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template<RayConcept OtherRay>
    [[nodiscard]] constexpr bool separates(const OtherRay& other) const;

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
    template<ConvexConcept OtherConvex>
    [[nodiscard]] constexpr bool separates(const OtherConvex& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template<DiskConcept OtherDisk>
    [[nodiscard]] constexpr bool separates(const OtherDisk& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    template<PolygonConcept OtherPolygon>
    [[nodiscard]] constexpr bool separates(const OtherPolygon& other) const;

    /** @brief Tests whether removing this shape disconnects the other shape (B∖A is disconnected). */
    [[nodiscard]] constexpr bool separates(const Shape<PointType>& other) const;

    // --- not-yet-implemented predicate pairs (throw); see implementation ---
    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<DiskConcept OtherDisk>
    [[nodiscard]] constexpr bool boundaryContains(const OtherDisk& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<ConvexConcept OtherConvex>
    [[nodiscard]] constexpr bool boundaryContains(const OtherConvex& other) const;

    /** @brief Tests whether this shape's boundary contains the other shape (∂A ⊇ B). */
    template<PolygonConcept OtherPolygon>
    [[nodiscard]] constexpr bool boundaryContains(const OtherPolygon& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template<DiskConcept OtherDisk>
    [[nodiscard]] constexpr bool interiorContains(const OtherDisk& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template<ConvexConcept OtherConvex>
    [[nodiscard]] constexpr bool interiorContains(const OtherConvex& other) const;

    /** @brief Tests whether this shape's interior contains the other shape (A∖∂A ⊇ B). */
    template<PolygonConcept OtherPolygon>
    [[nodiscard]] constexpr bool interiorContains(const OtherPolygon& other) const;


    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template<SegmentConcept OtherSegment>
    [[nodiscard]] constexpr bool crosses(const OtherSegment& other) const;

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template<PointConcept OtherPoint>
    [[nodiscard]] constexpr bool crosses(const OtherPoint& other) const;

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template<OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr bool crosses(const OtherOrientedSegment& other) const;

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template<LineConcept OtherLine>
    [[nodiscard]] constexpr bool crosses(const OtherLine& other) const;

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template<OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr bool crosses(const OtherOrientedLine& other) const;

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template<RayConcept OtherRay>
    [[nodiscard]] constexpr bool crosses(const OtherRay& other) const;

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template<HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr bool crosses(const OtherHalfplane& other) const;

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template<RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr bool crosses(const OtherRectangle& other) const;

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template<TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr bool crosses(const OtherTriangle& other) const;

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template<typename OtherShape>
        requires (!PointConcept<OtherShape> && detail::shapeRank<OtherShape> > detail::shapeRank<Triangle>)
    [[nodiscard]] constexpr bool crosses(const OtherShape& other) const {
        return other.crosses(*this);
    }

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    template <class EmptyPoint>
    [[nodiscard]] constexpr bool crosses(const EmptyShape<EmptyPoint>&) const {
        return false;
    }

    /** @brief Tests whether the two shapes mutually separate each other (each disconnects the other). */
    [[nodiscard]] constexpr bool crosses(const Shape<PointType>& other) const;

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

    /** @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint. @warning Divides coordinates after casting to ResultNumber. */
    template <class ResultNumber = NumberType, LineConcept OtherLine>
    [[nodiscard]] constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherLine& other) const;

    /** @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint. @warning Divides coordinates after casting to ResultNumber. */
    template <class ResultNumber = NumberType, OrientedLineConcept OtherOrientedLine>
    [[nodiscard]] constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherOrientedLine& other) const;

    /** @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint. @warning Divides coordinates after casting to ResultNumber. */
    template <class ResultNumber = NumberType, SegmentConcept OtherSegment>
    [[nodiscard]] constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherSegment& other) const;

    /** @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint. @warning Divides coordinates after casting to ResultNumber. */
    template <class ResultNumber = NumberType, OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherOrientedSegment& other) const;

    /** @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint. @warning Divides coordinates after casting to ResultNumber. */
    template <class ResultNumber = NumberType, RayConcept OtherRay>
    [[nodiscard]] constexpr std::optional<std::variant<Point<ResultNumber, typename PointType::LabelType>, Segment<Point<ResultNumber, typename PointType::LabelType>>>>
    intersection(const OtherRay& other) const;

    /** @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint. @warning Divides coordinates after casting to ResultNumber. */
    template <class ResultNumber = NumberType, HalfplaneConcept OtherHalfplane>
    [[nodiscard]] constexpr auto intersection(const OtherHalfplane& other) const;

    /** @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint. @warning Divides coordinates after casting to ResultNumber. */
    template <class ResultNumber = NumberType, RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr auto intersection(const OtherRectangle& other) const;

    /** @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint. @warning Divides coordinates after casting to ResultNumber. */
    template <class ResultNumber = NumberType, TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr auto intersection(const OtherTriangle& other) const;

    /** @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint. @warning Divides coordinates after casting to ResultNumber. */
    template <class ResultNumber = NumberType, typename OtherShape>
        requires (!PointConcept<OtherShape>
                  && (detail::shapeRank<OtherShape> > detail::shapeRank<Triangle>)
                  && requires(const OtherShape& o, const Triangle& self) {
                         o.template intersection<ResultNumber>(self);
                     })
    [[nodiscard]] constexpr auto intersection(const OtherShape& other) const {
        return other.template intersection<ResultNumber>(*this);
    }

    /** @brief Returns the intersection of the two shapes (A ∩ B), empty when they are disjoint. */
    template <class ResultNumber = NumberType, class EmptyPoint>
    [[nodiscard]] constexpr EmptyShape<EmptyPoint> intersection(const EmptyShape<EmptyPoint>&) const {
        return {};
    }

    /**
     * @brief Returns the squared Euclidean distance to the given shape.
     *
     * Zero when the triangle contains the point; otherwise the smallest squared
     * distance from the point to a triangle edge.
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

    /**
     * @brief Returns the squared Euclidean distance to the given shape.
     *
     * Forwards to the other shape's implementation so that each unordered pair
     * needs `squaredDistance` defined only once, on the higher-ranked shape.
     */
    template <class ResultNumber = NumberType, typename OtherShape>
        requires ((detail::shapeRank<OtherShape> > detail::shapeRank<Triangle>)
                  && requires(const OtherShape& o, const Triangle& self) {
                         o.template squaredDistance<ResultNumber>(self);
                     })
    [[nodiscard]] constexpr auto squaredDistance(const OtherShape& other) const {
        return other.template squaredDistance<ResultNumber>(*this);
    }

    /**
     * @brief Returns the squared Euclidean distance to a disk.
     *
     * Forwards to @ref Disk::squaredDistance, which is not templated on a result
     * type: a disk's exterior distance is irrational, so it always returns
     * `double`. The generic forwarder above does not apply because it requires
     * the templated `squaredDistance<ResultNumber>` form.
     */
    template <class DiskPointType, class DiskLabel>
    [[nodiscard]] double squaredDistance(const Disk<DiskPointType, DiskLabel>& disk) const {
        return disk.squaredDistance(*this);
    }

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

    /**
     * @brief Returns the Manhattan (L1) distance to the given shape.
     *
     * Forwards to the other shape's implementation so that each unordered pair
     * needs `distanceL1` defined only once, on the higher-ranked shape.
     */
    template <class ResultNumber = NumberType, typename OtherShape>
        requires ((detail::shapeRank<OtherShape> > detail::shapeRank<Triangle>)
                  && requires(const OtherShape& o, const Triangle& self) {
                         o.template distanceL1<ResultNumber>(self);
                     })
    [[nodiscard]] constexpr auto distanceL1(const OtherShape& other) const {
        return other.template distanceL1<ResultNumber>(*this);
    }

    /**
     * @brief Returns the distance to the given shape, using symmetry to
     * re-dispatch through the wrapper's own `distanceL1`.
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

    /**
     * @brief Returns the Chebyshev (LInf) distance to the given shape.
     *
     * Forwards to the other shape's implementation so that each unordered pair
     * needs `distanceLInf` defined only once, on the higher-ranked shape.
     */
    template <class ResultNumber = NumberType, typename OtherShape>
        requires ((detail::shapeRank<OtherShape> > detail::shapeRank<Triangle>)
                  && requires(const OtherShape& o, const Triangle& self) {
                         o.template distanceLInf<ResultNumber>(self);
                     })
    [[nodiscard]] constexpr auto distanceLInf(const OtherShape& other) const {
        return other.template distanceLInf<ResultNumber>(*this);
    }

    /**
     * @brief Returns the distance to the given shape, using symmetry to
     * re-dispatch through the wrapper's own `distanceLInf`.
     *
     * Distance is symmetric, so this just calls @p other's own `distanceLInf`,
     * which visits its wrapped alternative and throws if the pair is
     * unsupported.
     */
    template <class ResultNumber = NumberType, PointConcept OtherPoint>
    [[nodiscard]] constexpr auto distanceLInf(const Shape<OtherPoint>& other) const {
        return other.template distanceLInf<ResultNumber>(*this);
    }

    /** @brief Returns the Manhattan (L1) Hausdorff distance to the given shape. */
    template <class ResultNumber = NumberType, PointConcept OtherPoint>
    [[nodiscard]] constexpr auto hausdorffDistanceL1(const OtherPoint& point) const;

    /** @copydoc hausdorffDistanceL1(const OtherPoint&) const */
    template <class ResultNumber = NumberType, SegmentConcept OtherSegment>
    [[nodiscard]] constexpr auto hausdorffDistanceL1(const OtherSegment& other) const;

    /** @copydoc hausdorffDistanceL1(const OtherPoint&) const */
    template <class ResultNumber = NumberType, OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr auto hausdorffDistanceL1(const OtherOrientedSegment& other) const;

    /** @copydoc hausdorffDistanceL1(const OtherPoint&) const */
    template <class ResultNumber = NumberType, RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr auto hausdorffDistanceL1(const OtherRectangle& other) const;

    /** @copydoc hausdorffDistanceL1(const OtherPoint&) const */
    template <class ResultNumber = NumberType, TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr auto hausdorffDistanceL1(const OtherTriangle& other) const;

    /**
     * @brief Returns the Manhattan (L1) Hausdorff distance to the given shape.
     *
     * Forwards to the other shape's implementation so that each unordered pair
     * needs `hausdorffDistanceL1` defined only once, on the higher-ranked shape.
     */
    template <class ResultNumber = NumberType, typename OtherShape>
        requires ((detail::shapeRank<OtherShape> > detail::shapeRank<Triangle>)
                  && requires(const OtherShape& o, const Triangle& self) {
                         o.template hausdorffDistanceL1<ResultNumber>(self);
                     })
    [[nodiscard]] constexpr auto hausdorffDistanceL1(const OtherShape& other) const {
        return other.template hausdorffDistanceL1<ResultNumber>(*this);
    }

    /**
     * @brief Returns the distance to the given shape, using symmetry to
     * re-dispatch through the wrapper's own `hausdorffDistanceL1`.
     *
     * Distance is symmetric, so this just calls @p other's own `hausdorffDistanceL1`,
     * which visits its wrapped alternative and throws if the pair is
     * unsupported.
     */
    template <class ResultNumber = NumberType, PointConcept OtherPoint>
    [[nodiscard]] constexpr auto hausdorffDistanceL1(const Shape<OtherPoint>& other) const {
        return other.template hausdorffDistanceL1<ResultNumber>(*this);
    }

    /** @brief Returns the Chebyshev (LInf) Hausdorff distance to the given shape. */
    template <class ResultNumber = NumberType, PointConcept OtherPoint>
    [[nodiscard]] constexpr auto hausdorffDistanceLInf(const OtherPoint& point) const;

    /** @copydoc hausdorffDistanceLInf(const OtherPoint&) const */
    template <class ResultNumber = NumberType, SegmentConcept OtherSegment>
    [[nodiscard]] constexpr auto hausdorffDistanceLInf(const OtherSegment& other) const;

    /** @copydoc hausdorffDistanceLInf(const OtherPoint&) const */
    template <class ResultNumber = NumberType, OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr auto hausdorffDistanceLInf(const OtherOrientedSegment& other) const;

    /** @copydoc hausdorffDistanceLInf(const OtherPoint&) const */
    template <class ResultNumber = NumberType, RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr auto hausdorffDistanceLInf(const OtherRectangle& other) const;

    /** @copydoc hausdorffDistanceLInf(const OtherPoint&) const */
    template <class ResultNumber = NumberType, TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr auto hausdorffDistanceLInf(const OtherTriangle& other) const;

    /**
     * @brief Returns the Chebyshev (LInf) Hausdorff distance to the given shape.
     *
     * Forwards to the other shape's implementation so that each unordered pair
     * needs `hausdorffDistanceLInf` defined only once, on the higher-ranked shape.
     */
    template <class ResultNumber = NumberType, typename OtherShape>
        requires ((detail::shapeRank<OtherShape> > detail::shapeRank<Triangle>)
                  && requires(const OtherShape& o, const Triangle& self) {
                         o.template hausdorffDistanceLInf<ResultNumber>(self);
                     })
    [[nodiscard]] constexpr auto hausdorffDistanceLInf(const OtherShape& other) const {
        return other.template hausdorffDistanceLInf<ResultNumber>(*this);
    }

    /**
     * @brief Returns the distance to the given shape, using symmetry to
     * re-dispatch through the wrapper's own `hausdorffDistanceLInf`.
     *
     * Distance is symmetric, so this just calls @p other's own `hausdorffDistanceLInf`,
     * which visits its wrapped alternative and throws if the pair is
     * unsupported.
     */
    template <class ResultNumber = NumberType, PointConcept OtherPoint>
    [[nodiscard]] constexpr auto hausdorffDistanceLInf(const Shape<OtherPoint>& other) const {
        return other.template hausdorffDistanceLInf<ResultNumber>(*this);
    }

    /**
     * @brief Returns the squared Hausdorff distance to the given shape.
     *
     * The directed Hausdorff distance in either direction is attained at a
     * vertex of the source shape, since distance to a convex shape is convex
     * and its supremum over any polygon is attained at a vertex.
     *
     * @tparam ResultNumber Coordinate type of the returned distance (default: NumberType).
     *
     * @warning With an integer @p ResultNumber the exact squared distance is
     *          generally a fraction, so the internal division truncates and the
     *          result is inexact. Request a floating-point or pgl::Rational
     *          result type, e.g. `squaredHausdorffDistance<double>(point)`, for
     *          an accurate value.
     */
    template <class ResultNumber = NumberType, PointConcept OtherPoint>
    [[nodiscard]] constexpr auto squaredHausdorffDistance(const OtherPoint& point) const;

    /** @copydoc squaredHausdorffDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, SegmentConcept OtherSegment>
    [[nodiscard]] constexpr auto squaredHausdorffDistance(const OtherSegment& other) const;

    /** @copydoc squaredHausdorffDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, OrientedSegmentConcept OtherOrientedSegment>
    [[nodiscard]] constexpr auto squaredHausdorffDistance(const OtherOrientedSegment& other) const;

    /** @copydoc squaredHausdorffDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, RectangleConcept OtherRectangle>
    [[nodiscard]] constexpr auto squaredHausdorffDistance(const OtherRectangle& other) const;

    /** @copydoc squaredHausdorffDistance(const OtherPoint&) const */
    template <class ResultNumber = NumberType, TriangleConcept OtherTriangle>
    [[nodiscard]] constexpr auto squaredHausdorffDistance(const OtherTriangle& other) const;

    /**
     * @brief Returns the squared Hausdorff distance to the given shape.
     *
     * Forwards to the other shape's implementation so that each unordered pair
     * needs `squaredHausdorffDistance` defined only once, on the higher-ranked shape.
     */
    template <class ResultNumber = NumberType, typename OtherShape>
        requires ((detail::shapeRank<OtherShape> > detail::shapeRank<Triangle>)
                  && requires(const OtherShape& o, const Triangle& self) {
                         o.template squaredHausdorffDistance<ResultNumber>(self);
                     })
    [[nodiscard]] constexpr auto squaredHausdorffDistance(const OtherShape& other) const {
        return other.template squaredHausdorffDistance<ResultNumber>(*this);
    }

    /** @brief Translates all vertices by a point in place. */
    template<PointConcept OtherPoint>
    constexpr Triangle& operator+=(const OtherPoint& translation);

    /** @brief Translates all vertices by the opposite of a point in place. */
    template<PointConcept OtherPoint>
    constexpr Triangle& operator-=(const OtherPoint& translation);

    /** @brief Scales all vertices by a scalar in place. */
    template <class Scalar>
        requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
    constexpr Triangle& operator*=(const Scalar& scalar);

    /** @brief Divides all vertices by a scalar in place. */
    template <class Scalar>
        requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
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

    /**
     * @brief Smallest squared distance from a triangle edge to a disjoint shape.
     *
     * Used when the triangle does not intersect @p other and the closest point
     * of the triangle therefore lies on its boundary. Requires the edge segment
     * to support `squaredDistance(OtherShape)` (directly or via forwarding).
     */
    template <class ResultNumber, class OtherShape>
    constexpr ResultNumber edgeMinSquaredDistance(const OtherShape& other) const;

    /**
     * @brief Smallest squared distance from a triangle vertex to a disjoint shape.
     *
     * Used for unbounded straight shapes (lines): when such a shape misses the
     * triangle, the whole triangle lies on one side and the closest point is a
     * vertex, so @p other must support `squaredDistance(Point)`.
     */
    template <class ResultNumber, class OtherShape>
    constexpr ResultNumber vertexMinSquaredDistance(const OtherShape& other) const;

    /** @copydoc edgeMinSquaredDistance */
    template <class ResultNumber, class OtherShape>
    constexpr ResultNumber edgeMinDistanceL1(const OtherShape& other) const;

    /** @copydoc vertexMinSquaredDistance */
    template <class ResultNumber, class OtherShape>
    constexpr ResultNumber vertexMinDistanceL1(const OtherShape& other) const;

    /** @copydoc edgeMinSquaredDistance */
    template <class ResultNumber, class OtherShape>
    constexpr ResultNumber edgeMinDistanceLInf(const OtherShape& other) const;

    /** @copydoc vertexMinSquaredDistance */
    template <class ResultNumber, class OtherShape>
    constexpr ResultNumber vertexMinDistanceLInf(const OtherShape& other) const;

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
    [[no_unique_address]] mutable LabelType label_{};
};

/** @brief Returns a translated copy of a triangle. */
template <class PointType, class LabelType, class TranslationNumber, class TranslationLabel>
constexpr auto operator+(const Triangle<PointType, LabelType>& triangle, const Point<TranslationNumber, TranslationLabel>& translation);

/** @brief Returns a translated copy of a triangle with the translation on the left. */
template <class TranslationNumber, class TranslationLabel, class PointType, class LabelType>
constexpr auto operator+(const Point<TranslationNumber, TranslationLabel>& translation, const Triangle<PointType, LabelType>& triangle);

/** @brief Returns a copy of a triangle translated by the opposite point. */
template <class PointType, class LabelType, class TranslationNumber, class TranslationLabel>
constexpr auto operator-(const Triangle<PointType, LabelType>& triangle, const Point<TranslationNumber, TranslationLabel>& translation);

/** @brief Returns a scaled copy of a triangle. */
template <class PointType, class LabelType, class Scalar>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr auto operator*(const Triangle<PointType, LabelType>& triangle, const Scalar& scalar);

/** @brief Returns a scaled copy of a triangle with the scalar on the left. */
template <class Scalar, class PointType, class LabelType>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr auto operator*(const Scalar& scalar, const Triangle<PointType, LabelType>& triangle);

/** @brief Returns a copy of a triangle divided by a scalar. */
template <class PointType, class LabelType, class Scalar>
    requires(!detail::is_point_v<Scalar> && !TransformationConcept<Scalar>)
constexpr auto operator/(const Triangle<PointType, LabelType>& triangle, const Scalar& scalar);

/** @brief Streams a triangle as `<abc>` using canonical vertex order. */
template <class PointType, class LabelType>
std::ostream& operator<<(std::ostream& stream, const Triangle<PointType, LabelType>& triangle);

}  // namespace pgl
