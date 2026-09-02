#pragma once

#include "algorithm/polyominoes.hpp"

/**
 * @file bitmatrix.hpp
 * @brief One bit per cell over a fixed rectangular window of the integer grid.
 */

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cmath>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <optional>
#include <ranges>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace pgl {

/**
 * @brief Which grid cells count as neighbors.
 *
 * `edge` is 4-adjacency (cells sharing a side), `vertex` is 8-adjacency (cells
 * sharing a side or only a corner).
 */
enum class GridAdjacency { edge, vertex };

// Declared here so the point-range constructor can constrain on it: a matrix is
// itself a range of points, and it must not fall into that overload.
template <class TPointType>
    requires std::signed_integral<typename TPointType::NumberType>
class BitMatrix;

namespace detail {

/** @brief Whether a type is a @ref BitMatrix. */
template <class T>
inline constexpr bool is_bit_matrix_v = false;

template <class TPointType>
inline constexpr bool is_bit_matrix_v<BitMatrix<TPointType>> = true;

/**
 * @brief Converts one coordinate onto the integer grid, exactly or not at all.
 *
 * A cell is named by integers, so a coordinate that is not a whole number names
 * no cell: rounding it would move the shape, which is why this refuses it
 * instead. A whole number that @p Int cannot hold is refused for the same
 * reason.
 *
 * @tparam Int Signed integer type of the grid.
 * @param value Coordinate to convert.
 * @return The same value as an @p Int.
 * @throws std::logic_error If the value is not a whole number, or is one that
 *         @p Int cannot hold.
 */
template <class Int, class Number>
[[nodiscard]] Int gridCoordinate(const Number& value) {
    if constexpr (std::same_as<Int, Number>) {
        return value;
    } else if constexpr (is_Rational_v<Number>) {
        if (!value.isInteger()) {
            throw std::logic_error("pgl::BitMatrix: a coordinate is not an integer");
        }
        return gridCoordinate<Int>(static_cast<rational_int_t<Number>>(value));
    } else if constexpr (std::is_floating_point_v<Number>) {
        if (!std::isfinite(value) || value != std::floor(value)) {
            throw std::logic_error("pgl::BitMatrix: a coordinate is not an integer");
        }
        // The bound is a power of two, so it and its negation are both exact in
        // any binary floating-point type: this range test never rounds, even
        // where Int carries more digits than Number does.
        const Number low = static_cast<Number>(numeric_limits<Int>::min());
        if (value < low || value >= -low) {
            throw std::logic_error("pgl::BitMatrix: a coordinate does not fit the grid");
        }
        return static_cast<Int>(value);
    } else {
        // An exact integer of some other width. Range-check it whenever Number
        // can spell Int's bounds at all; an unbounded BigInt reports no digits
        // and always can, a narrower fixed width cannot go out of range.
        constexpr bool comparable = !numeric_limits<Number>::is_specialized
                                    || numeric_limits<Number>::digits >= numeric_limits<Int>::digits;
        if constexpr (comparable) {
            if (value < static_cast<Number>(numeric_limits<Int>::min())
                || value > static_cast<Number>(numeric_limits<Int>::max())) {
                throw std::logic_error("pgl::BitMatrix: a coordinate does not fit the grid");
            }
        }
        return static_cast<Int>(value);
    }
}

/** @brief Converts one point onto the integer grid, as @ref gridCoordinate does. */
template <class GridPointType, class OtherPointType>
[[nodiscard]] GridPointType gridPoint(const OtherPointType& point) {
    using Int = typename GridPointType::NumberType;
    return GridPointType(gridCoordinate<Int>(point.x()), gridCoordinate<Int>(point.y()));
}

/**
 * @brief Converts a ring onto the integer grid, vertex by vertex.
 *
 * The result is built trusted: the source ring is canonical and an exact
 * coordinate conversion preserves that, the same reason the shapes' own
 * converting constructors renormalize nothing.
 */
template <class GridPointType, class OtherPointType, class TLabel>
[[nodiscard]] Polygon<GridPointType> gridRing(const Polygon<OtherPointType, TLabel>& ring) {
    std::vector<GridPointType> vertices;
    vertices.reserve(ring.size());
    for (const OtherPointType& vertex : ring) {
        vertices.push_back(gridPoint<GridPointType>(vertex));
    }
    return Polygon<GridPointType>(std::move(vertices), true);
}

/** @brief Converts a region onto the integer grid, ring by ring. */
template <class GridPointType, class OtherPointType, class TLabel>
[[nodiscard]] PolygonWithHoles<GridPointType> gridRegion(
    const PolygonWithHoles<OtherPointType, TLabel>& region) {
    std::vector<Polygon<GridPointType>> holes;
    holes.reserve(region.holeCount());
    for (const auto& hole : region.holes()) {
        holes.push_back(gridRing<GridPointType>(hole));
    }
    return PolygonWithHoles<GridPointType>(gridRing<GridPointType>(region.outer()), std::move(holes),
                                           true);
}

/** @brief Converts a set onto the integer grid, component by component. */
template <class GridPointType, class OtherPointType, class TLabel>
[[nodiscard]] PolygonSet<GridPointType> gridSet(const PolygonSet<OtherPointType, TLabel>& set) {
    std::vector<PolygonWithHoles<GridPointType>> components;
    components.reserve(set.componentCount());
    for (const auto& component : set.components()) {
        components.push_back(gridRegion<GridPointType>(component));
    }
    return PolygonSet<GridPointType>(std::move(components), true);
}

/**
 * @brief The point type a shape over @p PointType rasterizes onto.
 *
 * The shape's own point type when the grid keeps its coordinate type, so an
 * integer shape rasterizes onto exactly the points it is made of, labels and
 * all; otherwise the requested integer paired with the same label type.
 */
template <class PointType, class Int>
using grid_point_t = std::conditional_t<std::same_as<Int, typename PointType::NumberType>, PointType,
                                        Point<Int, typename PointType::LabelType>>;

}  // namespace detail

/**
 * @brief A bit per cell of a rectangular window of the integer grid.
 *
 * Cell `(x, y)` is the unit square `[x, x+1] x [y, y+1]`, named by the integer
 * coordinates of its lower-left corner. The bits cover the window
 * `[origin, origin + (width, height))`, which is fixed at construction and
 * never grows: writing outside it is a no-op and reading outside it gives
 * `false`. Sizing the window is the caller's job, and @ref resized and
 * @ref trimmed move a matrix to another one.
 *
 * Rows are packed into 64-bit words, which is what makes the bulk operations
 * cheap: the whole set algebra, @ref count, @ref andCount, @ref perimeter and
 * @ref eulerNumber run a word at a time, @ref latticeMinkowskiSum costs one
 * shifted or-assignment of a whole matrix per cell of the smaller operand, and
 * the connectivity and boundary walks behind @ref componentCount and
 * @ref asPolygonSet move a run of cells at a time rather than a cell at a time.
 *
 * A matrix wears two hats, and which one an operation wears is stated in its
 * documentation:
 *
 * - As **a region of the plane** -- the union of its cells as unit squares --
 *   for @ref area, @ref perimeter, @ref centroid, @ref boundingBox,
 *   @ref convexHull, @ref asPolygonWithHoles, @ref asPolygonSet, and the
 *   symmetries and Minkowski operations whose names carry no prefix.
 * - As **a set of lattice points** -- each cell standing for its lower-left
 *   corner -- for every `lattice`-prefixed operation, which is the convention
 *   that makes a structuring element behave: the sum of cells `a` and `b` is
 *   the single cell `a + b`, not the two-by-two square that adding the unit
 *   squares would give. It is what makes a one-cell matrix the identity of
 *   @ref latticeMinkowskiSum, and @ref latticeMinkowskiErosion its exact dual.
 *   Every such operation has an unprefixed counterpart that wears the region
 *   hat instead, and the pair differs by a cell: `reflected` maps cell `c` to
 *   `-c - (1,1)` where @ref latticeReflected maps it to `-c`.
 *
 * @ref minkowskiSum and @ref minkowskiErosion, without the prefix, are the
 * region sum and the regularized region erosion the shapes compute, and both
 * commute with @ref asPolygonSet.
 *
 * Two operations *compute* from the window rather than only from the cells,
 * since a complement has to be taken relative to something bounded:
 * @ref operator~ and, through it, @ref latticeMinkowskiErosion. Every other
 * construction and every geometric predicate depends only on the cells.
 *
 * The window is part of a matrix's value all the same. @ref operator==,
 * @ref operator<=> and `std::hash` compare the window along with the cells, as
 * they do for a shape's stored representation, so `a != a.trimmed()` whenever
 * trimming moves anything. @ref samePointSet is the geometric question of
 * whether two matrices cover the same region, and ignores the window. A window
 * covering no cell has one canonical form, so all such matrices compare equal
 * whatever origin they were built with.
 *
 * @tparam TPointType Point type naming a cell; its coordinates must be a signed
 *         integral type, since a cell of the grid is an integer position.
 */
template <class TPointType = Point<int>>
    requires std::signed_integral<typename TPointType::NumberType>
class BitMatrix {
public:
    /// Point type naming a cell by its lower-left corner.
    using PointType = TPointType;
    /// Coordinate type of a cell.
    using NumberType = typename PointType::NumberType;
    /// Rectangle over @ref PointType, the type of a window.
    using RectangleType = Rectangle<PointType>;
    /// Region type @ref asPolygonWithHoles produces.
    using RegionType = PolygonWithHoles<PointType>;
    /// Region-set type @ref asPolygonSet produces.
    using PolygonSetType = PolygonSet<PointType>;
    /// Convex type @ref convexHull produces.
    using ConvexType = Convex<PointType>;
    /// Cell type the iterators yield.
    using value_type = PointType;

    class Iterator;
    /// Iteration is read-only, so both iterator types are @ref Iterator.
    using iterator = Iterator;
    /// Iteration is read-only, so both iterator types are @ref Iterator.
    using const_iterator = Iterator;

    // -----------------------------------------------------------------------
    // Construction

    /** @brief Creates a matrix whose window is empty, so no cell can be set. */
    BitMatrix() = default;

    /**
     * @brief Creates an empty matrix covering a window of the grid.
     *
     * @param origin Lower-left cell of the window.
     * @param width Number of cells per row; a non-positive value empties the
     *        window.
     * @param height Number of rows; a non-positive value empties the window.
     */
    BitMatrix(PointType origin, int width, int height) {
        if (width > 0 && height > 0) {
            origin_ = std::move(origin);
            width_ = width;
            height_ = height;
        }
        words_ = wordsPerRow(width_);
        bits_.assign(words_ * static_cast<std::size_t>(height_), 0);
    }

    /**
     * @brief Creates an empty matrix over the cells a rectangle covers.
     *
     * The window runs from `box.min()` to `box.max()`, so a rectangle of width
     * `w` and height `h` gives `w * h` cells, the last of them named by
     * `box.max() - (1, 1)`. This is the inverse of @ref window.
     *
     * @param box Rectangle covering the window; it must have integer corners,
     *        which its point type guarantees.
     */
    explicit BitMatrix(const RectangleType& box)
        : BitMatrix(box.empty() ? PointType() : box.min(),
                    box.empty() ? 0 : static_cast<int>(box.width()),
                    box.empty() ? 0 : static_cast<int>(box.height())) {}

    /**
     * @brief Rasterizes a rectilinear region, one bit per covered cell.
     *
     * Fills the cells whose interior lies inside the region, over a window that
     * is exactly the region's bounding box. Crossing parity along each row does
     * it: every ring contributes its vertical edges, and the cells between the
     * first and second crossing, the third and fourth, and so on, are inside.
     * The integer coordinates make it exact, since they keep every horizontal
     * cross-section constant within a row of cells.
     *
     * This is the fast path for the rectilinear case. @ref innerRaster gives the
     * same answer for such a region and also accepts every other shape, at the
     * cost of one exact predicate per cell of the bounding box.
     *
     * @param region Region to rasterize; every edge of it must be axis-parallel.
     * @throws std::logic_error If an edge of the region is not axis-parallel.
     */
    template <class TLabel>
    explicit BitMatrix(const PolygonWithHoles<PointType, TLabel>& region)
        : BitMatrix(RectangleType(region.bbox())) {
        Crossings crossings(static_cast<std::size_t>(height_));
        addRegionCrossings(region, crossings);
        fillCrossings(crossings);
    }

    /**
     * @brief Rasterizes a rectilinear region given over another coordinate type.
     *
     * A cell is an integer position, so a region whose coordinates are not
     * integers -- a Rational or a floating-point one -- covers a set of cells
     * only when every coordinate of it happens to be whole. That is checked
     * rather than rounded: a coordinate that is not a whole number, or is one
     * @ref NumberType cannot hold, throws instead of moving a vertex.
     *
     * @param region Region to rasterize; every edge of it must be axis-parallel
     *        and every coordinate a whole number this grid can hold.
     * @throws std::logic_error If an edge is not axis-parallel, or a coordinate
     *         is not a whole number of the grid.
     */
    // Unconstrained: partial ordering already prefers the same-type overload
    // above, and a constraint here would ride onto an implicit deduction guide,
    // which the CI clang mishandles.
    template <class OtherPointType, class TLabel>
    explicit BitMatrix(const PolygonWithHoles<OtherPointType, TLabel>& region)
        : BitMatrix(detail::gridRegion<PointType>(region)) {}

    /**
     * @brief Rasterizes a rectilinear polygon, one bit per covered cell.
     *
     * The single-ring case of @ref BitMatrix(const PolygonWithHoles<PointType,
     * TLabel>&), with the same crossing parity over the same window, the
     * polygon's bounding box. A self-intersecting polygon is filled by that
     * parity rather than rejected.
     *
     * @param polygon Polygon to rasterize; every edge of it must be axis-parallel.
     * @throws std::logic_error If an edge of the polygon is not axis-parallel.
     */
    template <class TLabel>
    explicit BitMatrix(const Polygon<PointType, TLabel>& polygon)
        : BitMatrix(RectangleType(polygon.bbox())) {
        Crossings crossings(static_cast<std::size_t>(height_));
        addRingCrossings(polygon, crossings);
        fillCrossings(crossings);
    }

    /**
     * @brief Rasterizes a rectilinear polygon given over another coordinate type.
     *
     * The single-ring case of @ref BitMatrix(const PolygonWithHoles<OtherPointType,
     * TLabel>&), with the same whole-number requirement on every coordinate.
     *
     * @param polygon Polygon to rasterize; every edge of it must be axis-parallel
     *        and every coordinate a whole number this grid can hold.
     * @throws std::logic_error If an edge is not axis-parallel, or a coordinate
     *         is not a whole number of the grid.
     */
    template <class OtherPointType, class TLabel>
    explicit BitMatrix(const Polygon<OtherPointType, TLabel>& polygon)
        : BitMatrix(detail::gridRing<PointType>(polygon)) {}

    /**
     * @brief Rasterizes a rectilinear set of regions, one bit per covered cell.
     *
     * The many-component case of @ref BitMatrix(const PolygonWithHoles<PointType,
     * TLabel>&), over a window that is the bounding box of the whole set. Every
     * ring of every component feeds the same parity pass, which the disjoint
     * interiors keep exact: a row leaves one component before it enters the
     * next, so the crossings pair up within a component.
     *
     * @param set Set to rasterize; every edge of it must be axis-parallel.
     * @throws std::logic_error If an edge of the set is not axis-parallel.
     */
    template <class TLabel>
    explicit BitMatrix(const PolygonSet<PointType, TLabel>& set)
        : BitMatrix(RectangleType(set.bbox())) {
        Crossings crossings(static_cast<std::size_t>(height_));
        for (const PolygonWithHoles<PointType>& component : set.components()) {
            addRegionCrossings(component, crossings);
        }
        fillCrossings(crossings);
    }

    /**
     * @brief Rasterizes a rectilinear set given over another coordinate type.
     *
     * The many-component case of @ref BitMatrix(const PolygonWithHoles<OtherPointType,
     * TLabel>&), with the same whole-number requirement on every coordinate.
     *
     * @param set Set to rasterize; every edge of it must be axis-parallel and
     *        every coordinate a whole number this grid can hold.
     * @throws std::logic_error If an edge is not axis-parallel, or a coordinate
     *         is not a whole number of the grid.
     */
    template <class OtherPointType, class TLabel>
    explicit BitMatrix(const PolygonSet<OtherPointType, TLabel>& set)
        : BitMatrix(detail::gridSet<PointType>(set)) {}

    /**
     * @brief Sets one cell per point of a range, over the smallest window holding them.
     *
     * This is the lattice-point reading of a matrix: every point names the cell
     * it is, rather than a corner of a region to fill. The window is the
     * smallest one holding every point, so the result has `window() == bbox()`
     * and equals its own @ref trimmed; an empty range gives an empty window.
     * Repeated points set the same cell again, which changes nothing.
     *
     * Any range of points does, except a shape or another matrix. A shape that
     * happens to iterate over its vertices -- a Polygon, a Convex, a Polyline --
     * is a boundary and not a point cloud, and the shapes that rasterize have
     * their own constructor above; a matrix carries a window that this would
     * silently trim. Feed either one's points through @ref lattice or a view to
     * ask for this reading of it anyway.
     *
     * The points may carry any coordinate type, checked rather than rounded
     * exactly as the shape constructors check theirs.
     *
     * @tparam Range Input range of points, defaulting to an initializer list so
     *         that a braced list of cells works.
     * @param points Cells to set.
     * @throws std::logic_error If a coordinate is not a whole number this grid
     *         can hold, or the points span more cells than a window holds.
     */
    template <std::ranges::input_range Range = std::initializer_list<PointType>>
        requires(detail::is_point_v<std::remove_cvref_t<std::ranges::range_value_t<Range>>>
                 && !AnyShapeConcept<std::remove_cvref_t<Range>>
                 && !detail::is_bit_matrix_v<std::remove_cvref_t<Range>>)
    explicit BitMatrix(Range&& points) {
        using SourcePoint = std::remove_cvref_t<std::ranges::range_value_t<Range>>;
        if constexpr (std::ranges::forward_range<Range>
                      && std::same_as<typename SourcePoint::NumberType, NumberType>) {
            *this = emptyOver(points);
            for (const auto& point : points) {
                set(point.x(), point.y());
            }
        } else {
            // A single-pass range cannot be walked twice, and a converting one
            // should not be: the window needs a pass of its own, so convert
            // once into a range that gives both.
            std::vector<PointType> cells;
            for (const auto& point : points) {
                cells.push_back(detail::gridPoint<PointType>(point));
            }
            *this = BitMatrix(cells);
        }
    }

    // -----------------------------------------------------------------------
    // The window

    /** @brief Lower-left cell of the window. */
    [[nodiscard]] const PointType& origin() const { return origin_; }

    /** @brief Number of cells per row of the window. */
    [[nodiscard]] int width() const { return width_; }

    /** @brief Number of rows of the window. */
    [[nodiscard]] int height() const { return height_; }

    /** @brief The window, as the rectangle its cells cover. */
    [[nodiscard]] RectangleType window() const {
        if (emptyWindow()) {
            return RectangleType();
        }
        return RectangleType(origin_, PointType(origin_.x() + static_cast<NumberType>(width_),
                                                origin_.y() + static_cast<NumberType>(height_)));
    }

    /** @brief Whether the window itself is degenerate, so no cell can be set. */
    [[nodiscard]] bool emptyWindow() const { return width_ == 0; }

    /** @brief Whether a cell is inside the window, and so can be set. */
    [[nodiscard]] bool inWindow(NumberType x, NumberType y) const {
        const std::int64_t i = localX(x), j = localY(y);
        return i >= 0 && i < width_ && j >= 0 && j < height_;
    }

    /** @brief Whether a cell is inside the window, and so can be set. */
    [[nodiscard]] bool inWindow(const PointType& cell) const { return inWindow(cell.x(), cell.y()); }

    /** @brief Whether two matrices cover the same window. */
    [[nodiscard]] bool sameWindow(const BitMatrix& other) const {
        return width_ == other.width_ && height_ == other.height_ && origin_ == other.origin_;
    }

    /**
     * @brief Returns the same cells over another window, dropping those outside.
     *
     * @param box Window of the result, read as @ref BitMatrix(const RectangleType&) reads it.
     */
    [[nodiscard]] BitMatrix resized(const RectangleType& box) const {
        BitMatrix result(box);
        result.combine(*this, [](std::uint64_t, std::uint64_t source) { return source; });
        return result;
    }

    /**
     * @brief Returns the same cells over the smallest window holding them.
     *
     * The result has `window() == bbox()`, so its origin is the lower-left
     * corner of its own bounding box.
     */
    [[nodiscard]] BitMatrix trimmed() const { return resized(bbox()); }

    // -----------------------------------------------------------------------
    // Individual cells

    /** @brief Whether the cell is set; cells outside the window are not. */
    [[nodiscard]] bool get(NumberType x, NumberType y) const {
        const std::int64_t i = localX(x), j = localY(y);
        if (i < 0 || i >= width_ || j < 0 || j >= height_) {
            return false;
        }
        return ((row(static_cast<int>(j))[static_cast<std::size_t>(i) / 64] >> (i % 64)) & 1) != 0;
    }

    /** @brief Whether the cell is set; cells outside the window are not. */
    [[nodiscard]] bool get(const PointType& cell) const { return get(cell.x(), cell.y()); }

    /** @brief Sets the cell; a cell outside the window is silently dropped. */
    void set(NumberType x, NumberType y) {
        const std::int64_t i = localX(x), j = localY(y);
        if (i < 0 || i >= width_ || j < 0 || j >= height_) {
            return;
        }
        row(static_cast<int>(j))[static_cast<std::size_t>(i) / 64] |= std::uint64_t(1) << (i % 64);
    }

    /** @brief Sets the cell; a cell outside the window is silently dropped. */
    void set(const PointType& cell) { set(cell.x(), cell.y()); }

    /** @brief Sets or clears the cell; a cell outside the window is dropped. */
    void set(NumberType x, NumberType y, bool value) {
        if (value) {
            set(x, y);
        } else {
            reset(x, y);
        }
    }

    /**
     * @brief Sets one cell per point of a range; cells outside the window are dropped.
     *
     * Every point names the cell it is, the lattice-point reading the range
     * constructor takes, except that the window here is the one the matrix
     * already has. Repeated points set the same cell again, which changes
     * nothing.
     *
     * Any range of points does, except a shape or another matrix, exactly as
     * for that constructor: a shape that iterates over its vertices is a
     * boundary and not a point cloud. The points may carry any coordinate type,
     * checked rather than rounded.
     *
     * @tparam Range Input range of points, defaulting to an initializer list so
     *         that a braced list of cells works.
     * @param points Cells to set.
     * @throws std::logic_error If a coordinate is not a whole number this grid
     *         can hold.
     */
    template <std::ranges::input_range Range = std::initializer_list<PointType>>
        requires(detail::is_point_v<std::remove_cvref_t<std::ranges::range_value_t<Range>>>
                 && !AnyShapeConcept<std::remove_cvref_t<Range>>
                 && !detail::is_bit_matrix_v<std::remove_cvref_t<Range>>)
    void set(Range&& points) {
        for (const auto& point : points) {
            set(detail::gridPoint<PointType>(point));
        }
    }

    /** @brief Clears the cell; a cell outside the window is silently dropped. */
    void reset(NumberType x, NumberType y) {
        const std::int64_t i = localX(x), j = localY(y);
        if (i < 0 || i >= width_ || j < 0 || j >= height_) {
            return;
        }
        row(static_cast<int>(j))[static_cast<std::size_t>(i) / 64] &= ~(std::uint64_t(1) << (i % 64));
    }

    /** @brief Clears the cell; a cell outside the window is silently dropped. */
    void reset(const PointType& cell) { reset(cell.x(), cell.y()); }

    /**
     * @brief Clears one cell per point of a range; cells outside the window are dropped.
     *
     * The plural of @ref reset(const PointType&), taking the same ranges the
     * range @ref set does and reading their points the same way: every point
     * names the cell it is, over the window the matrix already has. Repeated
     * points clear the same cell again, which changes nothing.
     *
     * @tparam Range Input range of points, defaulting to an initializer list so
     *         that a braced list of cells works.
     * @param points Cells to clear.
     * @throws std::logic_error If a coordinate is not a whole number this grid
     *         can hold.
     */
    template <std::ranges::input_range Range = std::initializer_list<PointType>>
        requires(detail::is_point_v<std::remove_cvref_t<std::ranges::range_value_t<Range>>>
                 && !AnyShapeConcept<std::remove_cvref_t<Range>>
                 && !detail::is_bit_matrix_v<std::remove_cvref_t<Range>>)
    void reset(Range&& points) {
        for (const auto& point : points) {
            reset(detail::gridPoint<PointType>(point));
        }
    }

    /** @brief Flips the cell; a cell outside the window is silently dropped. */
    void flip(NumberType x, NumberType y) {
        const std::int64_t i = localX(x), j = localY(y);
        if (i < 0 || i >= width_ || j < 0 || j >= height_) {
            return;
        }
        row(static_cast<int>(j))[static_cast<std::size_t>(i) / 64] ^= std::uint64_t(1) << (i % 64);
    }

    /** @brief Flips the cell; a cell outside the window is silently dropped. */
    void flip(const PointType& cell) { flip(cell.x(), cell.y()); }

    /**
     * @brief Flips one cell per point of a range; cells outside the window are dropped.
     *
     * The plural of @ref flip(const PointType&), taking the same ranges the
     * range @ref set does and reading their points the same way: every point
     * names the cell it is, over the window the matrix already has. A repeated
     * point flips the same cell twice and so leaves it as it was, unlike the
     * range @ref set and @ref reset, which repetition does not affect.
     *
     * @tparam Range Input range of points, defaulting to an initializer list so
     *         that a braced list of cells works.
     * @param points Cells to flip.
     * @throws std::logic_error If a coordinate is not a whole number this grid
     *         can hold.
     */
    template <std::ranges::input_range Range = std::initializer_list<PointType>>
        requires(detail::is_point_v<std::remove_cvref_t<std::ranges::range_value_t<Range>>>
                 && !AnyShapeConcept<std::remove_cvref_t<Range>>
                 && !detail::is_bit_matrix_v<std::remove_cvref_t<Range>>)
    void flip(Range&& points) {
        for (const auto& point : points) {
            flip(detail::gridPoint<PointType>(point));
        }
    }

    /** @brief Sets every cell of the window. */
    void setAll() {
        std::fill(bits_.begin(), bits_.end(), ~std::uint64_t(0));
        maskTails();
    }

    /** @brief Clears every cell, keeping the window. */
    void clear() { std::fill(bits_.begin(), bits_.end(), 0); }

    // -----------------------------------------------------------------------
    // Cardinality

    /** @brief Whether no cell is set, which an empty window always is. */
    [[nodiscard]] bool empty() const {
        for (std::uint64_t word : bits_) {
            if (word != 0) {
                return false;
            }
        }
        return true;
    }

    /** @brief Number of set cells. */
    [[nodiscard]] std::size_t count() const {
        std::size_t total = 0;
        for (std::uint64_t word : bits_) {
            total += static_cast<std::size_t>(std::popcount(word));
        }
        return total;
    }

    /**
     * @brief Area the set cells cover, which is their number since each is a unit square.
     *
     * @tparam ResultNumber Result type (default: NumberType).
     * @return @ref count as a measure, so `0` when no cell is set.
     */
    template <class ResultNumber = NumberType>
    [[nodiscard]] ResultNumber area() const {
        return ResultNumber(static_cast<std::int64_t>(count()));
    }

    /**
     * @brief Length of the boundary of the covered region.
     *
     * Counts the unit edges with a set cell on exactly one side, so a cell on
     * the border of the window contributes the edges facing out of it. Runs a
     * word at a time.
     *
     * @tparam ResultNumber Result type (default: NumberType).
     */
    template <class ResultNumber = NumberType>
    [[nodiscard]] ResultNumber perimeter() const {
        std::size_t adjacent = 0;
        for (int j = 0; j < height_; ++j) {
            const std::uint64_t* here = row(j);
            const std::uint64_t* above = j + 1 < height_ ? row(j + 1) : nullptr;
            for (std::size_t w = 0; w < words_; ++w) {
                const std::uint64_t word = here[w];
                if (word == 0) {
                    continue;
                }
                adjacent += static_cast<std::size_t>(
                    std::popcount(word & shiftedWord(here, words_, static_cast<std::int64_t>(w) * 64 + 1)));
                if (above != nullptr) {
                    adjacent += static_cast<std::size_t>(std::popcount(word & above[w]));
                }
            }
        }
        return ResultNumber(static_cast<std::int64_t>(4 * count() - 2 * adjacent));
    }

    /**
     * @brief Centroid of the covered region.
     *
     * @tparam ResultNumber Coordinate type of the result; the default divides
     *         exactly, as every other pgl construction that divides does.
     * @throws std::logic_error If no cell is set, since there is no centroid.
     * @warning Divides coordinates by 2, and by the cell count on top of that.
     *          Inexact for integer coordinates.
     */
    template <class ResultNumber = division_result_t<NumberType>>
    [[nodiscard]] Point<ResultNumber> centroid() const {
        // Summed a word at a time: the cells of a word share a row and a base
        // column, so only their bit offsets are walked.
        std::int64_t sumX = 0, sumY = 0, cells = 0;
        for (int j = 0; j < height_; ++j) {
            const std::uint64_t* here = row(j);
            const std::int64_t y = static_cast<std::int64_t>(origin_.y()) + j;
            for (std::size_t w = 0; w < words_; ++w) {
                std::uint64_t word = here[w];
                if (word == 0) {
                    continue;
                }
                const std::int64_t inWord = std::popcount(word);
                const std::int64_t base =
                    static_cast<std::int64_t>(origin_.x()) + static_cast<std::int64_t>(w) * 64;
                std::int64_t offsets = 0;
                for (; word != 0; word &= word - 1) {
                    offsets += std::countr_zero(word);
                }
                sumX += base * inWord + offsets;
                sumY += y * inWord;
                cells += inWord;
            }
        }
        if (cells == 0) {
            throw std::logic_error("pgl::BitMatrix::centroid: no cell is set");
        }
        // Twice the sums over twice the count, which is the cell-center offset
        // of one half folded into an exact integer ratio.
        const ResultNumber total = ResultNumber(2 * cells);
        return Point<ResultNumber>(ResultNumber(2 * sumX + cells) / total,
                                   ResultNumber(2 * sumY + cells) / total);
    }

    // -----------------------------------------------------------------------
    // Reading the cells out

    /** @brief Iterator over the set lattice points, in row-major order. */
    [[nodiscard]] Iterator begin() const { return Iterator(this); }

    /** @brief End of the iteration over the set lattice points. */
    [[nodiscard]] Iterator end() const { return Iterator(); }

    /**
     * @brief Returns a lazy view of the set cells as lattice points.
     *
     * The iteration range itself: it yields every set cell as a const reference
     * to its lower-left corner, in row-major order, copying and allocating
     * nothing. It refers to this matrix and is invalidated by anything that
     * modifies it; @ref lattice materializes the same cells into a vector.
     *
     * @return A forward view of the @ref count() set cells.
     */
    [[nodiscard]] auto latticeView() const { return std::ranges::subrange(begin(), end()); }

    /**
     * @brief Returns a lazy view of the set cells as the unit squares they cover.
     *
     * Builds each rectangle as it is reached, so nothing is allocated. Like
     * @ref latticeView it refers to this matrix; @ref cells materializes the
     * same squares into a vector.
     *
     * @return A forward view of the @ref count() set cells.
     */
    [[nodiscard]] auto cellsView() const {
        return latticeView() |
               std::views::transform([](const PointType& cell) { return cellSquare(cell); });
    }

    /**
     * @brief The set cells as lattice points, in row-major order.
     *
     * The reading every `lattice`-prefixed operation works in: a cell stands
     * for the point at its lower-left corner, which is also the name `get` and
     * `set` address it by, and what the iterators yield. @ref cells is the same
     * list read as the squares the cells cover.
     */
    [[nodiscard]] std::vector<PointType> lattice() const {
        std::vector<PointType> result;
        result.reserve(count());
        for (const PointType& cell : *this) {
            result.push_back(cell);
        }
        return result;
    }

    /**
     * @brief The set cells as the unit squares they cover, in row-major order.
     *
     * One rectangle per cell. @ref rectangles instead merges each row's runs
     * into as few rectangles as a row-major pass can, which is the cheaper
     * covering to draw or to measure.
     */
    [[nodiscard]] std::vector<RectangleType> cells() const {
        std::vector<RectangleType> result;
        result.reserve(count());
        for (const PointType& cell : *this) {
            result.push_back(cellSquare(cell));
        }
        return result;
    }

    /**
     * @brief The covered region as maximal horizontal runs of cells.
     *
     * The rectangles are disjoint and cover exactly the set cells, one per
     * maximal run, in row-major order. Streaming them into a `Canvas` draws the
     * matrix: `canvas << matrix.rectangles()`.
     */
    [[nodiscard]] std::vector<RectangleType> rectangles() const {
        std::vector<RectangleType> result;
        for (int j = 0; j < height_; ++j) {
            std::int64_t i = 0;
            while (i < width_) {
                if (!localGet(j, i)) {
                    ++i;
                    continue;
                }
                const std::int64_t start = i;
                while (i < width_ && localGet(j, i)) {
                    ++i;
                }
                result.emplace_back(
                    PointType(origin_.x() + static_cast<NumberType>(start),
                              origin_.y() + static_cast<NumberType>(j)),
                    PointType(origin_.x() + static_cast<NumberType>(i),
                              origin_.y() + static_cast<NumberType>(j + 1)));
            }
        }
        return result;
    }

    /**
     * @brief The rectangle the set cells cover, empty when no cell is set.
     *
     * Exactly the box @ref asPolygonSet would report, read off the first and
     * last set word of every row rather than built from the regions.
     */
    [[nodiscard]] RectangleType bbox() const {
        std::int64_t minX = width_, maxX = -1, minY = height_, maxY = -1;
        for (int j = 0; j < height_; ++j) {
            const std::uint64_t* here = row(j);
            std::size_t first = 0;
            while (first < words_ && here[first] == 0) {
                ++first;
            }
            if (first == words_) {
                continue;
            }
            std::size_t last = words_ - 1;
            while (here[last] == 0) {
                --last;
            }
            minX = std::min(minX, static_cast<std::int64_t>(first) * 64 + std::countr_zero(here[first]));
            maxX = std::max(maxX, static_cast<std::int64_t>(last) * 64 + 63 - std::countl_zero(here[last]));
            minY = std::min(minY, static_cast<std::int64_t>(j));
            maxY = std::max(maxY, static_cast<std::int64_t>(j));
        }
        if (maxX < 0) {
            return RectangleType();
        }
        return RectangleType(PointType(origin_.x() + static_cast<NumberType>(minX),
                                       origin_.y() + static_cast<NumberType>(minY)),
                             PointType(origin_.x() + static_cast<NumberType>(maxX + 1),
                                       origin_.y() + static_cast<NumberType>(maxY + 1)));
    }

    /**
     * @brief A floating-point bounding box of the covered region.
     *
     * @tparam ResultNumber Coordinate type of the result.
     */
    template <class ResultNumber = double>
    [[nodiscard]] Rectangle<Point<ResultNumber>> fbox() const {
        return bbox().template fbox<ResultNumber>();
    }

    /**
     * @brief A point in the interior of the covered region.
     *
     * The center of the first set cell in row-major order.
     *
     * @tparam ResultNumber Coordinate type of the result.
     * @throws std::logic_error If no cell is set, since there is no such point.
     * @warning Divides coordinates by 2.
     */
    template <class ResultNumber = division_result_t<NumberType>>
    [[nodiscard]] Point<ResultNumber> pointInside() const {
        const std::optional<PointType> cell = firstSetCell();
        if (!cell) {
            throw std::logic_error("pgl::BitMatrix::pointInside: no cell is set");
        }
        const ResultNumber half = ResultNumber(1) / ResultNumber(2);
        return Point<ResultNumber>(ResultNumber(cell->x()) + half, ResultNumber(cell->y()) + half);
    }

    /**
     * @brief The covered region, as one region with holes.
     *
     * A matrix with no cell gives an empty region.
     *
     * @throws std::logic_error If the set cells do not form a single
     *         edge-connected polyomino, which is what a rasterized region and
     *         its Minkowski sums are. Use @ref asPolygonSet when they need not.
     */
    [[nodiscard]] RegionType asPolygonWithHoles() const {
        if (componentCount() > 1) {
            throw std::logic_error("pgl::BitMatrix::asPolygonWithHoles: the cells are not edge-connected");
        }
        return regionFromLoops(boundaryLoops(
            [](const detail::PolyCell&, const detail::PolyCell&) { return true; }));
    }

    /**
     * @brief The covered region, as a set of regions with holes.
     *
     * One component per edge-connected group of cells, so this has no
     * precondition; two components touching only at a corner stay apart, and
     * `bbox()` is exactly the bounding box of the result.
     *
     * The cost is one pass over the words plus one over the boundary edges, not
     * a union of unit squares: the loops are read straight off the words, which
     * gives the outer ring and one ring per hole directly, with the vertices in
     * the middle of a straight stretch dropped along the way. A window of `w`
     * cells whose components carry `b` boundary edges and split into `r` runs of
     * cells therefore costs `O(w / 64 + b + r log r)`, against the arrangement a
     * regularized union of unit squares would have to build.
     */
    [[nodiscard]] PolygonSetType asPolygonSet() const {
        if (emptyWindow()) {
            return PolygonSetType();
        }

        // Label the components first, so that each traced loop can be handed to
        // the one it bounds: a loop's first edge names a set cell on its left,
        // and the sorted runs of that cell's row say which component holds it.
        struct LabeledRun {
            std::int64_t x0, x1;
            std::size_t component;
        };
        std::vector<std::vector<LabeledRun>> rowRuns(static_cast<std::size_t>(height_));
        std::size_t total = 0;
        visitComponentRuns(GridAdjacency::edge, [&](const std::vector<CellRun>& runs) {
            for (const CellRun& run : runs) {
                rowRuns[static_cast<std::size_t>(run.y)].push_back({run.x0, run.x1, total});
            }
            ++total;
        });
        if (total == 0) {
            return PolygonSetType();
        }
        for (std::vector<LabeledRun>& runs : rowRuns) {
            std::sort(runs.begin(), runs.end(),
                      [](const LabeledRun& a, const LabeledRun& b) { return a.x0 < b.x0; });
        }

        auto componentAt = [&](const detail::PolyCell& cell) {
            const std::vector<LabeledRun>& runs = rowRuns[static_cast<std::size_t>(cell.second)];
            const auto after = std::upper_bound(
                runs.begin(), runs.end(), static_cast<std::int64_t>(cell.first),
                [](std::int64_t at, const LabeledRun& run) { return at < run.x0; });
            assert(after != runs.begin() && cell.first < std::prev(after)->x1 &&
                   "pgl::BitMatrix::asPolygonSet: a boundary edge borders no cell");
            return std::prev(after)->component;
        };

        // Two groups touching only at a corner are two components, so a pinch
        // there stays on its own side; a pinch within one group crosses, which
        // is what keeps a hole's loop apart from the loop around it.
        std::vector<std::vector<std::vector<detail::PolyCell>>> grouped(total);
        for (std::vector<detail::PolyCell>& loop :
             boundaryLoops([&](const detail::PolyCell& here, const detail::PolyCell& across) {
                 return componentAt(here) == componentAt(across);
             })) {
            grouped[componentAt(loopSeedCell(loop))].push_back(std::move(loop));
        }

        std::vector<RegionType> components;
        components.reserve(total);
        for (const std::vector<std::vector<detail::PolyCell>>& loops : grouped) {
            components.push_back(regionFromLoops(loops));
        }
        return PolygonSetType(components);
    }

    /** @brief Convex hull of the covered region. */
    [[nodiscard]] ConvexType convexHull() const {
        std::vector<PointType> corners;
        corners.reserve(static_cast<std::size_t>(height_) * 4);
        for (int j = 0; j < height_; ++j) {
            const std::optional<std::pair<std::int64_t, std::int64_t>> extent = rowExtent(j);
            if (!extent) {
                continue;
            }
            const NumberType low = origin_.x() + static_cast<NumberType>(extent->first);
            const NumberType high = origin_.x() + static_cast<NumberType>(extent->second + 1);
            const NumberType bottom = origin_.y() + static_cast<NumberType>(j);
            const NumberType top = bottom + NumberType(1);
            corners.emplace_back(low, bottom);
            corners.emplace_back(low, top);
            corners.emplace_back(high, bottom);
            corners.emplace_back(high, top);
        }
        return ConvexType(corners);
    }

    /**
     * @brief Draws the covered region to a canvas.
     *
     * Sends @ref asPolygonSet with the current style, so the whole matrix is a
     * single element however many components it has, with its holes as holes.
     * Stream @ref rectangles instead to draw the cells as separate elements.
     *
     * @param canvas Destination canvas.
     * @param matrix Matrix whose covered region is drawn.
     * @return The canvas.
     */
    friend Canvas& operator<<(Canvas& canvas, const BitMatrix& matrix) {
        return canvas << matrix.asPolygonSet();
    }

    // -----------------------------------------------------------------------
    // Set algebra
    //
    // Every binary operator returns the smallest window that provably loses no
    // cell: the overlap of the two windows for an intersection, their hull for a
    // union or a symmetric difference, the left window for a difference. The
    // compound assignments instead never move their window, and drop whatever
    // falls outside it, exactly as `set` does.

    /**
     * @brief Returns the complement within the same window.
     *
     * The window is unchanged, so this is the complement of the set cells
     * *inside* it, not of the whole plane. Together with @ref latticeMinkowskiErosion,
     * which is built on it, this is the only operation that reads the window
     * rather than only the cells.
     */
    BitMatrix operator~() const {
        BitMatrix result(origin_, width_, height_);
        for (std::size_t w = 0; w < bits_.size(); ++w) {
            result.bits_[w] = ~bits_[w];
        }
        result.maskTails();
        return result;
    }

    /** @brief Drops every cell the other matrix does not have. */
    BitMatrix& operator&=(const BitMatrix& other) {
        combine(other, [](std::uint64_t left, std::uint64_t right) { return left & right; });
        return *this;
    }

    /** @brief Adds every cell of the other matrix that this window holds. */
    BitMatrix& operator|=(const BitMatrix& other) {
        combine(other, [](std::uint64_t left, std::uint64_t right) { return left | right; });
        return *this;
    }

    /** @brief Flips every cell the other matrix has and this window holds. */
    BitMatrix& operator^=(const BitMatrix& other) {
        combine(other, [](std::uint64_t left, std::uint64_t right) { return left ^ right; });
        return *this;
    }

    /** @brief The cells both matrices have, over the overlap of the windows. */
    BitMatrix operator&(const BitMatrix& other) const {
        BitMatrix result = resized(overlapWindow(*this, other));
        result &= other;
        return result;
    }

    /** @brief The cells either matrix has, over the hull of the windows. */
    BitMatrix operator|(const BitMatrix& other) const {
        BitMatrix result = resized(hullWindow(*this, other));
        result |= other;
        return result;
    }

    /** @brief The cells exactly one matrix has, over the hull of the windows. */
    BitMatrix operator^(const BitMatrix& other) const {
        BitMatrix result = resized(hullWindow(*this, other));
        result ^= other;
        return result;
    }

    /** @brief The cells this matrix has and the other does not, over this window. */
    [[nodiscard]] BitMatrix difference(const BitMatrix& other) const {
        BitMatrix result = *this;
        result.combine(other, [](std::uint64_t left, std::uint64_t right) { return left & ~right; });
        return result;
    }

    /** @brief The cells exactly one matrix has; the same as @ref operator^. */
    [[nodiscard]] BitMatrix symmetricDifference(const BitMatrix& other) const { return *this ^ other; }

    /**
     * @brief Whether the two matrices have the same window and the same cells.
     *
     * Compares the stored representation, as a shape's `operator==` does. Use
     * @ref samePointSet to ask whether they cover the same region regardless of
     * the window.
     */
    bool operator==(const BitMatrix& other) const {
        return width_ == other.width_ && height_ == other.height_ && origin_ == other.origin_ &&
               bits_ == other.bits_;
    }

    /**
     * @brief Orders matrices lexicographically by `(origin, width, height, bits)`.
     *
     * A total order consistent with @ref operator==, so a matrix can key a
     * `std::set` or a `std::map`. It is an order on the representation and
     * carries no geometric meaning: two matrices covering the same region over
     * different windows are ordered by their windows.
     */
    std::strong_ordering operator<=>(const BitMatrix& other) const {
        if (const std::strong_ordering order = origin_ <=> other.origin_; order != 0) {
            return order;
        }
        if (const std::strong_ordering order = width_ <=> other.width_; order != 0) {
            return order;
        }
        if (const std::strong_ordering order = height_ <=> other.height_; order != 0) {
            return order;
        }
        return bits_ <=> other.bits_;
    }

    /**
     * @brief Whether the two matrices cover the same region.
     *
     * Reads only the cells, so a matrix and its @ref trimmed copy agree, unlike
     * @ref operator==, which compares the window too.
     */
    [[nodiscard]] bool samePointSet(const BitMatrix& other) const {
        const std::size_t here = count();
        return here == other.count() && andCount(other) == here;
    }

    /**
     * @brief Whether the covered region contains the other one.
     *
     * A cell the other matrix has and this one does not sticks out of this
     * region, so this is exactly "every cell of @p other is a cell of this".
     */
    [[nodiscard]] bool contains(const BitMatrix& other) const { return other.andCount(*this) == other.count(); }

    /**
     * @brief Whether the interior of the covered region contains the other one.
     *
     * A cell of @p other whose closure touches the boundary of this region is
     * not inside its interior, and touching includes meeting at a corner, so
     * this asks whether `interior(GridAdjacency::vertex)` contains @p other.
     */
    [[nodiscard]] bool interiorContains(const BitMatrix& other) const {
        return interior(GridAdjacency::vertex).contains(other);
    }

    /**
     * @brief Whether the boundary of the covered region contains the other one.
     *
     * The boundary is a curve and a nonempty matrix covers area, so this holds
     * only for an empty @p other. It is here to answer the question rather than
     * to leave it to a wrong guess.
     */
    [[nodiscard]] bool boundaryContains(const BitMatrix& other) const { return other.empty(); }

    /**
     * @brief Whether the two covered regions share a point.
     *
     * The cells are closed unit squares, so two of them meeting along an edge or
     * at a single corner already intersect: this holds as soon as a cell of one
     * matrix is within Chebyshev distance one of a cell of the other. Use
     * @ref interiorsIntersect for the stricter question of a shared cell.
     */
    [[nodiscard]] bool intersects(const BitMatrix& other) const {
        for (std::int64_t dy = -1; dy <= 1; ++dy) {
            for (std::int64_t dx = -1; dx <= 1; ++dx) {
                if (anyCommonCell(other, dx, dy)) {
                    return true;
                }
            }
        }
        return false;
    }

    /**
     * @brief Whether the interiors of the two covered regions share a point.
     *
     * Two distinct cells have disjoint interiors, so this holds exactly when the
     * matrices share a cell.
     */
    [[nodiscard]] bool interiorsIntersect(const BitMatrix& other) const { return anyCommonCell(other, 0, 0); }

    /**
     * @brief Number of cells set in both matrices.
     *
     * The two windows need not agree; only the cells they share can be set in
     * both, so the count runs over the rows and words of the overlap, reading
     * the other matrix a word at a time, shifted into this one's alignment.
     */
    [[nodiscard]] std::size_t andCount(const BitMatrix& other) const {
        std::size_t total = 0;
        forEachAlignedWord(other, [&](std::uint64_t left, std::uint64_t right) {
            total += static_cast<std::size_t>(std::popcount(left & right));
        });
        return total;
    }

    /** @brief Number of cells set in either matrix. */
    [[nodiscard]] std::size_t orCount(const BitMatrix& other) const {
        return count() + other.count() - andCount(other);
    }

    /** @brief Number of cells set in exactly one matrix. */
    [[nodiscard]] std::size_t xorCount(const BitMatrix& other) const {
        return count() + other.count() - 2 * andCount(other);
    }

    // -----------------------------------------------------------------------
    // Translations, reflections and morphology
    //
    // These read a cell as the lattice point at its lower-left corner, so the
    // sum of two cells is one cell and a reflection maps cell `c` to cell `-c`.

    /** @brief Returns the same cells translated by a vector. */
    [[nodiscard]] BitMatrix translated(const PointType& vector) const {
        BitMatrix result = *this;
        result.origin_ = PointType(origin_.x() + vector.x(), origin_.y() + vector.y());
        return result;
    }

    /** @brief Returns the same cells translated by a vector. */
    BitMatrix operator+(const PointType& vector) const { return translated(vector); }

    /** @brief Returns the same cells translated by the opposite of a vector. */
    BitMatrix operator-(const PointType& vector) const {
        return translated(PointType(-vector.x(), -vector.y()));
    }

    /** @brief Translates the cells by a vector. */
    BitMatrix& operator+=(const PointType& vector) {
        origin_ = PointType(origin_.x() + vector.x(), origin_.y() + vector.y());
        return *this;
    }

    /** @brief Translates the cells by the opposite of a vector. */
    BitMatrix& operator-=(const PointType& vector) {
        origin_ = PointType(origin_.x() - vector.x(), origin_.y() - vector.y());
        return *this;
    }

    /**
     * @brief Returns the reflection of the covered region through the origin.
     *
     * A symmetry of the plane, so it acts on the cells as the squares they are:
     * the square of cell `c` reflects onto the square of cell `-c - (1,1)`, and
     * the result is what reflecting `asPolygonSet()` gives. @ref latticeReflected
     * is the reflection of the lattice points, which lands one cell away.
     */
    [[nodiscard]] BitMatrix reflected() const { return mapped([](std::int64_t x, std::int64_t y) {
        return std::pair<std::int64_t, std::int64_t>(-x - 1, -y - 1);
    }); }

    /** @brief Returns the reflection of the covered region through the origin. */
    [[nodiscard]] BitMatrix operator-() const { return reflected(); }

    /** @brief Returns the reflection of the covered region across the x-axis. */
    [[nodiscard]] BitMatrix reflectedX() const { return mapped([](std::int64_t x, std::int64_t y) {
        return std::pair<std::int64_t, std::int64_t>(x, -y - 1);
    }); }

    /** @brief Returns the reflection of the covered region across the y-axis. */
    [[nodiscard]] BitMatrix reflectedY() const { return mapped([](std::int64_t x, std::int64_t y) {
        return std::pair<std::int64_t, std::int64_t>(-x - 1, y);
    }); }

    /**
     * @brief Returns the reflection of the covered region across the diagonal.
     *
     * Swapping the coordinates carries the square of cell `(x, y)` onto the
     * square of cell `(y, x)`, so this is the one symmetry the two readings
     * agree on; @ref latticeTransposed is the same operation under its lattice
     * name.
     */
    [[nodiscard]] BitMatrix transposed() const { return mapped([](std::int64_t x, std::int64_t y) {
        return std::pair<std::int64_t, std::int64_t>(y, x);
    }); }

    /**
     * @brief Returns the rotation of the covered region about the origin.
     *
     * @param k Number of counterclockwise quarter turns; may be negative.
     */
    [[nodiscard]] BitMatrix rotated90(int k = 1) const {
        const int turns = ((k % 4) + 4) % 4;
        return mapped([turns](std::int64_t x, std::int64_t y) {
            switch (turns) {
                case 1: return std::pair<std::int64_t, std::int64_t>(-y - 1, x);
                case 2: return std::pair<std::int64_t, std::int64_t>(-x - 1, -y - 1);
                case 3: return std::pair<std::int64_t, std::int64_t>(y, -x - 1);
                default: return std::pair<std::int64_t, std::int64_t>(x, y);
            }
        });
    }

    /**
     * @brief Rotates the covered region about the origin.
     *
     * @param k Number of counterclockwise quarter turns; may be negative.
     */
    BitMatrix& rotate90(int k = 1) {
        *this = rotated90(k);
        return *this;
    }

    /**
     * @brief Returns the reflection of the cells as lattice points, `{-c}`.
     *
     * The reflection @ref latticeMinkowskiErosion is dual to: it is what makes
     * `~(~a).latticeMinkowskiSum(b.latticeReflected())` and
     * `a.latticeMinkowskiErosion(b)` the same question over a common window. It
     * sits one cell from @ref reflected, which reflects the squares instead.
     */
    [[nodiscard]] BitMatrix latticeReflected() const { return mapped([](std::int64_t x, std::int64_t y) {
        return std::pair<std::int64_t, std::int64_t>(-x, -y);
    }); }

    /** @brief Returns the reflection of the cells as lattice points, `{(x, -y)}`. */
    [[nodiscard]] BitMatrix latticeReflectedX() const { return mapped([](std::int64_t x, std::int64_t y) {
        return std::pair<std::int64_t, std::int64_t>(x, -y);
    }); }

    /** @brief Returns the reflection of the cells as lattice points, `{(-x, y)}`. */
    [[nodiscard]] BitMatrix latticeReflectedY() const { return mapped([](std::int64_t x, std::int64_t y) {
        return std::pair<std::int64_t, std::int64_t>(-x, y);
    }); }

    /** @brief Returns `{(y, x)}`; the same as @ref transposed, which the two readings agree on. */
    [[nodiscard]] BitMatrix latticeTransposed() const { return transposed(); }

    /**
     * @brief Returns the rotation of the cells as lattice points.
     *
     * @param k Number of counterclockwise quarter turns; may be negative.
     */
    [[nodiscard]] BitMatrix latticeRotated90(int k = 1) const {
        const int turns = ((k % 4) + 4) % 4;
        return mapped([turns](std::int64_t x, std::int64_t y) {
            switch (turns) {
                case 1: return std::pair<std::int64_t, std::int64_t>(-y, x);
                case 2: return std::pair<std::int64_t, std::int64_t>(-x, -y);
                case 3: return std::pair<std::int64_t, std::int64_t>(y, -x);
                default: return std::pair<std::int64_t, std::int64_t>(x, y);
            }
        });
    }

    /**
     * @brief Rotates the cells as lattice points.
     *
     * @param k Number of counterclockwise quarter turns; may be negative.
     */
    BitMatrix& latticeRotate90(int k = 1) {
        *this = latticeRotated90(k);
        return *this;
    }

    /**
     * @brief Returns the Minkowski sum `{a + b : a in *this, b in other}`.
     *
     * The window of the result is exactly the bounding box of the sum. The cost
     * is one shifted or-assignment of the larger operand per cell of the smaller
     * one, so summing a large region with a small structuring element is cheap.
     */
    [[nodiscard]] BitMatrix latticeMinkowskiSum(const BitMatrix& other) const {
        const BitMatrix left = trimmed(), right = other.trimmed();
        if (left.emptyWindow() || right.emptyWindow()) {
            return BitMatrix();
        }

        const bool stampIsRight = right.count() < left.count();
        const BitMatrix& stamp = stampIsRight ? right : left;
        const BitMatrix& canvas = stampIsRight ? left : right;

        BitMatrix result(PointType(left.origin_.x() + right.origin_.x(),
                                   left.origin_.y() + right.origin_.y()),
                         left.width_ + right.width_ - 1, left.height_ + right.height_ - 1);
        for (const PointType& cell : stamp) {
            result.orShifted(canvas, static_cast<std::int64_t>(cell.x() - stamp.origin_.x()),
                             static_cast<std::int64_t>(cell.y() - stamp.origin_.y()));
        }
        return result;
    }

    /**
     * @brief Returns the Minkowski sum of the two covered regions.
     *
     * The sum the shapes compute, so this commutes with the conversion:
     * `(a + b).asPolygonSet()` and `a.asPolygonSet() + b.asPolygonSet()` agree.
     * The unit square is not the identity of that sum -- `U (+) U` is the
     * two-by-two square `[0,2]^2` -- so this is @ref latticeMinkowskiSum
     * followed by a dilation with the two-by-two block of cells that the extra
     * square covers, leaving the result one cell wider and one cell taller than
     * the lattice sum. Reach for @ref latticeMinkowskiSum in morphology, where a
     * single cell has to be the identity.
     */
    [[nodiscard]] BitMatrix minkowskiSum(const BitMatrix& other) const {
        if (empty() || other.empty()) {
            return BitMatrix();
        }
        return latticeMinkowskiSum(other).latticeMinkowskiSum(unitSquareSum());
    }

    /** @brief Returns the Minkowski sum of the regions; the same as @ref minkowskiSum. */
    [[nodiscard]] BitMatrix operator+(const BitMatrix& other) const { return minkowskiSum(other); }

    /**
     * @brief Returns the regularized Minkowski erosion of the covered regions.
     *
     * The erosion the shapes compute, regularized as `PolygonSet` regularizes
     * its own, so this commutes with the conversion:
     * `a.minkowskiErosion(b).asPolygonSet()` and
     * `a.asPolygonSet().minkowskiErosion(b.asPolygonSet())` agree.
     *
     * Regularizing is what keeps the answer on the grid. Writing the operand as
     * `B = latB (+) U`, the erosion splits into `(A (-) latB) (-) U`, and the
     * first step is the lattice erosion, which is a cell region. For the second,
     * a *non-integer* `p` has `p + U` inside a cell region exactly when the
     * two-by-two block of cells it straddles is, so the interior of the result
     * is a union of open cells and its closure is the lattice erosion by that
     * block. What regularizing drops is the lower-dimensional part: eroding a
     * single cell by a single cell leaves the one point `p = 0`, which the
     * shapes report as a degenerate `HalfplaneIntersection` and this reports as
     * empty.
     *
     * The two steps compose into one, so the cost is a single lattice erosion by
     * @p other dilated with the two-by-two block. Eroding by a matrix with no
     * cell is the whole plane, which no window holds; this fills the window, as
     * @ref latticeMinkowskiErosion does.
     */
    [[nodiscard]] BitMatrix minkowskiErosion(const BitMatrix& other) const {
        return latticeMinkowskiErosion(other.latticeMinkowskiSum(unitSquareSum()));
    }

    /**
     * @brief Returns the Minkowski erosion `{p : p + other is inside *this}`.
     *
     * Like @ref operator~, this reads the window: a cell outside it counts as
     * unset, so the erosion is taken against this matrix as a bounded set. The
     * window of the result is this window shrunk by the bounding box of
     * @p other, which is the largest window a translate of @p other can land in.
     * Eroding by a matrix with no cell is vacuously true everywhere, and fills
     * the whole window.
     */
    [[nodiscard]] BitMatrix latticeMinkowskiErosion(const BitMatrix& other) const {
        const BitMatrix stamp = other.trimmed();
        if (stamp.emptyWindow()) {
            BitMatrix result(origin_, width_, height_);
            result.setAll();
            return result;
        }
        const int resultWidth = width_ - stamp.width_ + 1;
        const int resultHeight = height_ - stamp.height_ + 1;
        BitMatrix result(PointType(origin_.x() - stamp.origin_.x(), origin_.y() - stamp.origin_.y()),
                         resultWidth, resultHeight);
        if (result.emptyWindow()) {
            return result;
        }
        result.setAll();
        for (const PointType& cell : stamp) {
            result.andShifted(*this, static_cast<std::int64_t>(cell.x() - stamp.origin_.x()),
                              static_cast<std::int64_t>(cell.y() - stamp.origin_.y()));
        }
        return result;
    }

    /** @brief Returns the opening, a lattice erosion by @p other followed by a lattice sum. */
    [[nodiscard]] BitMatrix latticeOpening(const BitMatrix& other) const {
        return latticeMinkowskiErosion(other).latticeMinkowskiSum(other);
    }

    /** @brief Returns the closing, a lattice sum with @p other followed by a lattice erosion. */
    [[nodiscard]] BitMatrix latticeClosing(const BitMatrix& other) const {
        return latticeMinkowskiSum(other).latticeMinkowskiErosion(other);
    }

    /**
     * @brief Returns the set cells all of whose neighbors are set.
     *
     * A cell on the border of the window has neighbors outside it, which count
     * as unset, so it never belongs to the interior.
     *
     * @param adjacency Which cells count as neighbors.
     */
    [[nodiscard]] BitMatrix interior(GridAdjacency adjacency = GridAdjacency::edge) const {
        BitMatrix result = *this;
        for (const auto& [dx, dy] : neighborOffsets(adjacency)) {
            result.andShifted(*this, dx, dy);
        }
        return result;
    }

    /**
     * @brief Returns the set cells with at least one neighbor that is not set.
     *
     * @param adjacency Which cells count as neighbors.
     */
    [[nodiscard]] BitMatrix boundary(GridAdjacency adjacency = GridAdjacency::edge) const {
        return difference(interior(adjacency));
    }

    // -----------------------------------------------------------------------
    // Connectivity

    /**
     * @brief Returns one matrix per connected group of cells, each trimmed.
     *
     * The groups come out ordered by their lowest, then leftmost cell.
     *
     * @param adjacency Which cells count as neighbors.
     */
    [[nodiscard]] std::vector<BitMatrix> connectedComponents(GridAdjacency adjacency = GridAdjacency::edge) const {
        std::vector<BitMatrix> result;
        visitComponentRuns(adjacency, [&](const std::vector<CellRun>& runs) {
            std::int64_t minX = runs.front().x0, maxX = runs.front().x1 - 1;
            int minY = runs.front().y, maxY = minY;
            for (const CellRun& run : runs) {
                minX = std::min(minX, run.x0);
                maxX = std::max(maxX, run.x1 - 1);
                minY = std::min(minY, run.y);
                maxY = std::max(maxY, run.y);
            }
            BitMatrix component(PointType(origin_.x() + static_cast<NumberType>(minX),
                                          origin_.y() + static_cast<NumberType>(minY)),
                                static_cast<int>(maxX - minX) + 1, maxY - minY + 1);
            for (const CellRun& run : runs) {
                component.setLocalRange(run.y - minY, run.x0 - minX, run.x1 - minX);
            }
            result.push_back(std::move(component));
        });
        return result;
    }

    /**
     * @brief Number of connected groups of cells.
     *
     * A scanline flood fill, so this costs one pass over the words and a
     * constant per run of cells, and never materializes a group.
     */
    [[nodiscard]] std::size_t componentCount(GridAdjacency adjacency = GridAdjacency::edge) const {
        std::size_t total = 0;
        visitComponentRuns(adjacency, [&](const std::vector<CellRun>&) { ++total; });
        return total;
    }

    /** @brief Whether the set cells form exactly one connected group. */
    [[nodiscard]] bool isConnected(GridAdjacency adjacency = GridAdjacency::edge) const {
        return !empty() && componentCount(adjacency) == 1;
    }

    /**
     * @brief Returns the cells together with every hole they enclose.
     *
     * A hole is a group of unset cells that cannot reach outside the bounding
     * box. The background is walked with the adjacency complementary to
     * @p adjacency -- 8 for a 4-connected foreground and the other way round --
     * which is what keeps a diagonal chain of cells from both being connected
     * and letting the background leak through it.
     *
     * @param adjacency Which cells count as neighbors of the *foreground*.
     */
    [[nodiscard]] BitMatrix fillHoles(GridAdjacency adjacency = GridAdjacency::edge) const {
        if (empty()) {
            return *this;
        }
        const RectangleType box = bbox();
        const RectangleType padded(PointType(box.min().x() - NumberType(1), box.min().y() - NumberType(1)),
                                   PointType(box.max().x() + NumberType(1), box.max().y() + NumberType(1)));
        const BitMatrix outside =
            (~resized(padded)).floodFrom(padded.min(), complementary(adjacency));
        return (~outside).resized(window());
    }

    /**
     * @brief Number of holes the set cells enclose.
     *
     * @param adjacency Which cells count as neighbors of the foreground.
     */
    [[nodiscard]] std::size_t holeCount(GridAdjacency adjacency = GridAdjacency::edge) const {
        return static_cast<std::size_t>(static_cast<std::int64_t>(componentCount(adjacency)) -
                                        eulerNumber(adjacency));
    }

    /**
     * @brief Euler characteristic of the covered region: components minus holes.
     *
     * Counted from the sixteen patterns a two-by-two block of cells can show,
     * so it costs one pass over the words and needs no flood fill.
     *
     * @param adjacency Which cells count as neighbors of the foreground.
     */
    [[nodiscard]] std::int64_t eulerNumber(GridAdjacency adjacency = GridAdjacency::edge) const {
        if (emptyWindow()) {
            return 0;
        }
        const std::size_t blockWords = static_cast<std::size_t>((width_ + 1 + 63) / 64);
        const int tail = (width_ + 1) % 64;
        const std::uint64_t lastMask = tail == 0 ? ~std::uint64_t(0) : (std::uint64_t(1) << tail) - 1;
        const std::vector<std::uint64_t> zeros(words_, 0);

        std::int64_t ones = 0, threes = 0, diagonals = 0;
        for (int j = -1; j < height_; ++j) {
            const std::uint64_t* lower = j >= 0 ? row(j) : zeros.data();
            const std::uint64_t* upper = j + 1 < height_ ? row(j + 1) : zeros.data();
            for (std::size_t w = 0; w < blockWords; ++w) {
                const std::int64_t base = static_cast<std::int64_t>(w) * 64 - 1;
                const std::uint64_t a = shiftedWord(lower, words_, base);
                const std::uint64_t b = shiftedWord(lower, words_, base + 1);
                const std::uint64_t c = shiftedWord(upper, words_, base);
                const std::uint64_t d = shiftedWord(upper, words_, base + 1);
                const std::uint64_t valid = w + 1 == blockWords ? lastMask : ~std::uint64_t(0);
                const std::uint64_t one = (a & ~b & ~c & ~d) | (~a & b & ~c & ~d) |
                                          (~a & ~b & c & ~d) | (~a & ~b & ~c & d);
                const std::uint64_t three = (~a & b & c & d) | (a & ~b & c & d) |
                                            (a & b & ~c & d) | (a & b & c & ~d);
                const std::uint64_t diagonal = (a & ~b & ~c & d) | (~a & b & c & ~d);
                ones += std::popcount(one & valid);
                threes += std::popcount(three & valid);
                diagonals += std::popcount(diagonal & valid);
            }
        }
        const std::int64_t sign = adjacency == GridAdjacency::edge ? 2 : -2;
        return (ones - threes + sign * diagonals) / 4;
    }

    // -----------------------------------------------------------------------
    // Convexity

    /**
     * @brief Fills the gaps of every row.
     *
     * @return Whether any cell was filled in.
     */
    bool fillRows() {
        bool changed = false;
        for (int j = 0; j < height_; ++j) {
            if (const std::optional<std::pair<std::int64_t, std::int64_t>> extent = rowExtent(j)) {
                changed |= setLocalRange(j, extent->first, extent->second + 1);
            }
        }
        return changed;
    }

    /**
     * @brief Fills the gaps of every column.
     *
     * @return Whether any cell was filled in.
     */
    bool fillColumns() {
        if (emptyWindow()) {
            return false;
        }

        // below[j] holds the columns with a set cell in some row under j.
        std::vector<std::uint64_t> below(bits_.size(), 0);
        for (int j = 1; j < height_; ++j) {
            for (std::size_t w = 0; w < words_; ++w) {
                below[static_cast<std::size_t>(j) * words_ + w] =
                    below[static_cast<std::size_t>(j - 1) * words_ + w] | row(j - 1)[w];
            }
        }

        bool changed = false;
        std::vector<std::uint64_t> above(words_, 0);  // Columns set in some row over j.
        for (int j = height_ - 1; j >= 0; --j) {
            std::uint64_t* here = row(j);
            for (std::size_t w = 0; w < words_; ++w) {
                const std::uint64_t original = here[w];
                const std::uint64_t fill =
                    above[w] & below[static_cast<std::size_t>(j) * words_ + w] & ~original;
                here[w] |= fill;
                above[w] |= original;
                changed |= fill != 0;
            }
        }
        return changed;
    }

    /**
     * @brief Fills every cell that has set cells on both sides in its row and in
     *        its column, until nothing changes.
     *
     * The result is the smallest hv-convex superset: every row and every column
     * of it meets it in a single interval. Only cells strictly between set cells
     * are added, so the window is never outgrown and the loop terminates.
     *
     * @return The number of cells that were filled in.
     */
    std::size_t makeHvConvex() {
        const std::size_t before = count();
        // Both fills run on every pass: filling the columns can open a row gap
        // and the other way round, so neither may be short-circuited away.
        for (bool changed = true; changed;) {
            const bool rows = fillRows();
            const bool columns = fillColumns();
            changed = rows || columns;
        }
        return count() - before;
    }

    /**
     * @brief Whether every row meets the set cells in a single interval.
     *
     * Read off the words rather than by filling a copy: a row is an interval
     * exactly when it holds as many cells as lie between its first and its last.
     */
    [[nodiscard]] bool isRowConvex() const {
        for (int j = 0; j < height_; ++j) {
            const std::optional<std::pair<std::int64_t, std::int64_t>> extent = rowExtent(j);
            if (extent && static_cast<std::int64_t>(countRow(j)) !=
                              extent->second - extent->first + 1) {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief Whether every column meets the set cells in a single interval.
     *
     * Read off the words rather than by filling a copy: sweeping the rows
     * upwards, a column closes once a gap opens over a cell it has shown, and
     * the answer is no as soon as a closed column shows another cell.
     */
    [[nodiscard]] bool isColumnConvex() const {
        std::vector<std::uint64_t> seen(words_, 0), closed(words_, 0);
        for (int j = 0; j < height_; ++j) {
            const std::uint64_t* here = row(j);
            for (std::size_t w = 0; w < words_; ++w) {
                if ((here[w] & closed[w]) != 0) {
                    return false;
                }
                closed[w] |= seen[w] & ~here[w];
                seen[w] |= here[w];
            }
        }
        return true;
    }

    /** @brief Whether every row and every column meets the cells in one interval. */
    [[nodiscard]] bool isHvConvex() const { return isRowConvex() && isColumnConvex(); }

    // -----------------------------------------------------------------------

    /** @brief Forward iterator over the set cells, in row-major order. */
    class Iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = PointType;
        using difference_type = std::ptrdiff_t;
        using pointer = const PointType*;
        using reference = const PointType&;

        Iterator() = default;

        /** @brief The cell the iterator is on. */
        reference operator*() const { return current_; }

        /** @brief The cell the iterator is on. */
        pointer operator->() const { return &current_; }

        /** @brief Advances to the next set cell. */
        Iterator& operator++() {
            advance();
            return *this;
        }

        /** @brief Advances to the next set cell, returning the previous one. */
        Iterator operator++(int) {
            Iterator previous = *this;
            advance();
            return previous;
        }

        /** @brief Whether two iterators are on the same cell of the same matrix. */
        bool operator==(const Iterator& other) const {
            return owner_ == other.owner_ && (owner_ == nullptr || (rowIndex_ == other.rowIndex_ &&
                                                                    wordIndex_ == other.wordIndex_ &&
                                                                    rest_ == other.rest_));
        }

        /** @brief Whether two iterators are on different cells. */
        bool operator!=(const Iterator& other) const { return !(*this == other); }

    private:
        friend class BitMatrix;

        explicit Iterator(const BitMatrix* owner) : owner_(owner) {
            if (owner_->words_ == 0) {
                owner_ = nullptr;
                return;
            }
            rest_ = owner_->row(0)[0];
            advance();
        }

        void advance() {
            while (true) {
                if (rest_ != 0) {
                    const int bit = std::countr_zero(rest_);
                    rest_ &= rest_ - 1;
                    current_ = PointType(
                        owner_->origin_.x() +
                            static_cast<NumberType>(static_cast<std::int64_t>(wordIndex_) * 64 + bit),
                        owner_->origin_.y() + static_cast<NumberType>(rowIndex_));
                    return;
                }
                ++wordIndex_;
                if (wordIndex_ >= owner_->words_) {
                    wordIndex_ = 0;
                    ++rowIndex_;
                }
                if (rowIndex_ >= owner_->height_) {
                    owner_ = nullptr;
                    return;
                }
                rest_ = owner_->row(rowIndex_)[wordIndex_];
            }
        }

        const BitMatrix* owner_ = nullptr;
        int rowIndex_ = 0;
        std::size_t wordIndex_ = 0;
        std::uint64_t rest_ = 0;
        PointType current_{};
    };

private:
    friend struct std::hash<BitMatrix>;

    /** @brief A maximal run of set cells in one row, in window-local coordinates. */
    struct CellRun {
        int y;            ///< Row of the run.
        std::int64_t x0;  ///< First cell of the run.
        std::int64_t x1;  ///< One past the last cell of the run.
    };

    /// Unit steps of the four boundary directions, in the order east, north,
    /// west, south, so that turning right is stepping back one and left one on.
    static constexpr std::array<std::pair<int, int>, 4> boundarySteps{
        {{1, 0}, {0, 1}, {-1, 0}, {0, -1}}};

    /** @brief The unit square a cell covers. */
    [[nodiscard]] static RectangleType cellSquare(const PointType& cell) {
        return RectangleType(cell, PointType(cell.x() + NumberType(1), cell.y() + NumberType(1)),
                             true);
    }

    static std::size_t wordsPerRow(int width) {
        return static_cast<std::size_t>((width + 63) / 64);
    }

    /** @brief Number of cells from @p low to @p high inclusive, as a window extent. */
    static int cellSpan(NumberType low, NumberType high) {
        // Unsigned subtraction is exact here even where the difference overflows
        // the coordinate type, since high is at least low.
        const std::uint64_t span =
            static_cast<std::uint64_t>(high) - static_cast<std::uint64_t>(low);
        if (span >= static_cast<std::uint64_t>(detail::numeric_limits<int>::max())) {
            throw std::logic_error("pgl::BitMatrix: the points do not fit a window");
        }
        return static_cast<int>(span) + 1;
    }

    /** @brief An empty matrix over the smallest window holding every point of a range. */
    template <class Range>
    static BitMatrix emptyOver(Range&& points) {
        auto it = std::ranges::begin(points);
        const auto last = std::ranges::end(points);
        if (it == last) {
            return BitMatrix();
        }
        NumberType minX = (*it).x(), maxX = minX;
        NumberType minY = (*it).y(), maxY = minY;
        for (++it; it != last; ++it) {
            minX = std::min<NumberType>(minX, (*it).x());
            maxX = std::max<NumberType>(maxX, (*it).x());
            minY = std::min<NumberType>(minY, (*it).y());
            maxY = std::max<NumberType>(maxY, (*it).y());
        }
        return BitMatrix(PointType(minX, minY), cellSpan(minX, maxX), cellSpan(minY, maxY));
    }

    std::uint64_t* row(int j) { return bits_.data() + static_cast<std::size_t>(j) * words_; }
    const std::uint64_t* row(int j) const {
        return bits_.data() + static_cast<std::size_t>(j) * words_;
    }

    [[nodiscard]] std::int64_t localX(NumberType x) const {
        return static_cast<std::int64_t>(x) - static_cast<std::int64_t>(origin_.x());
    }
    [[nodiscard]] std::int64_t localY(NumberType y) const {
        return static_cast<std::int64_t>(y) - static_cast<std::int64_t>(origin_.y());
    }

    [[nodiscard]] bool localGet(int j, std::int64_t i) const {
        return ((row(j)[static_cast<std::size_t>(i) / 64] >> (i % 64)) & 1) != 0;
    }

    [[nodiscard]] std::size_t countRow(int j) const {
        std::size_t total = 0;
        for (std::size_t w = 0; w < words_; ++w) {
            total += static_cast<std::size_t>(std::popcount(row(j)[w]));
        }
        return total;
    }

    /**
     * @brief The first and the last set cell of row @p j, in local coordinates.
     *
     * Nothing if the row has no cell. Both ends are inclusive, and the search
     * skips whole empty words, so it costs one pass over the words of the row.
     */
    [[nodiscard]] std::optional<std::pair<std::int64_t, std::int64_t>> rowExtent(int j) const {
        const std::uint64_t* here = row(j);
        std::size_t first = 0;
        while (first < words_ && here[first] == 0) {
            ++first;
        }
        if (first == words_) {
            return std::nullopt;
        }
        std::size_t last = words_ - 1;
        while (here[last] == 0) {
            --last;
        }
        return std::pair{static_cast<std::int64_t>(first) * 64 + std::countr_zero(here[first]),
                         static_cast<std::int64_t>(last) * 64 + 63 - std::countl_zero(here[last])};
    }

    /// Abscissas where the rings cross each row of the window, one bucket per row.
    using Crossings = std::vector<std::vector<NumberType>>;

    /**
     * @brief Adds the crossings of one ring to the rows its edges span.
     *
     * A vertical edge crosses every row between its endpoints and a horizontal
     * one crosses none; no other edge has an exact rasterization, since it does
     * not keep the horizontal cross-section constant within a row of cells.
     *
     * @throws std::logic_error If an edge of the ring is not axis-parallel.
     */
    template <class Ring>
    void addRingCrossings(const Ring& ring, Crossings& crossings) const {
        for (std::size_t i = 0; i < ring.size(); ++i) {
            const PointType p = ring[i];
            const PointType q = ring[(i + 1) % ring.size()];
            if (p.x() == q.x()) {
                for (NumberType y = std::min(p.y(), q.y()); y < std::max(p.y(), q.y()); ++y) {
                    crossings[static_cast<std::size_t>(y - origin_.y())].push_back(p.x());
                }
            } else if (p.y() != q.y()) {
                throw std::logic_error("pgl::BitMatrix: the shape is not rectilinear");
            }
        }
    }

    /** @brief Adds the crossings of every ring of a region: outer, then holes. */
    template <class Region>
    void addRegionCrossings(const Region& region, Crossings& crossings) const {
        addRingCrossings(region.outer(), crossings);
        for (const Polygon<PointType>& hole : region.holes()) {
            addRingCrossings(hole, crossings);
        }
    }

    /**
     * @brief Sets the cells between the first and second crossing of every row,
     *        the third and fourth, and so on.
     */
    void fillCrossings(Crossings& crossings) {
        for (int j = 0; j < height_; ++j) {
            std::vector<NumberType>& crossingsInRow = crossings[static_cast<std::size_t>(j)];
            std::sort(crossingsInRow.begin(), crossingsInRow.end());
            for (std::size_t i = 0; i + 1 < crossingsInRow.size(); i += 2) {
                setLocalRange(j, static_cast<std::int64_t>(crossingsInRow[i] - origin_.x()),
                              static_cast<std::int64_t>(crossingsInRow[i + 1] - origin_.x()));
            }
        }
    }

    /**
     * @brief Sets the cells of row @p j (local coordinates) in `[low, high)`.
     *
     * @return Whether any of them was not already set.
     */
    bool setLocalRange(int j, std::int64_t low, std::int64_t high) {
        low = std::max<std::int64_t>(low, 0);
        high = std::min<std::int64_t>(high, width_);
        bool changed = false;
        for (std::int64_t i = low; i < high;) {
            const std::size_t w = static_cast<std::size_t>(i) / 64;
            const int offset = static_cast<int>(i % 64);
            const int bits = static_cast<int>(std::min<std::int64_t>(64 - offset, high - i));
            const std::uint64_t mask =
                bits == 64 ? ~std::uint64_t(0) : ((std::uint64_t(1) << bits) - 1) << offset;
            changed |= (row(j)[w] & mask) != mask;
            row(j)[w] |= mask;
            i += bits;
        }
        return changed;
    }

    /** @brief Clears the cells of a row of words in `[low, high)`. */
    static void clearWordRange(std::uint64_t* here, std::int64_t low, std::int64_t high) {
        for (std::int64_t i = low; i < high;) {
            const std::size_t w = static_cast<std::size_t>(i) / 64;
            const int offset = static_cast<int>(i % 64);
            const int bits = static_cast<int>(std::min<std::int64_t>(64 - offset, high - i));
            const std::uint64_t mask =
                bits == 64 ? ~std::uint64_t(0) : ((std::uint64_t(1) << bits) - 1) << offset;
            here[w] &= ~mask;
            i += bits;
        }
    }

    // The three searches below read a row of `words_` words holding `width_`
    // cells, the bits past the width being clear, and skip whole words. They
    // take the row rather than its index so a flood fill can run them over its
    // own copy of the bits.

    /**
     * @brief First cell at or after @p from that is set, or `width_`.
     *
     * Takes `from == width_` and answers `width_`, which is what lets a scan
     * that has just consumed the last run of a row hand its end straight back.
     *
     * @pre `0 <= from <= width_`.
     */
    [[nodiscard]] std::int64_t nextSetInRow(const std::uint64_t* here, std::int64_t from) const {
        assert(from >= 0 && from <= width_);
        if (from >= width_) {
            return width_;
        }
        std::size_t w = static_cast<std::size_t>(from) / 64;
        std::uint64_t rest = here[w] & (~std::uint64_t(0) << (from % 64));
        while (rest == 0) {
            if (++w >= words_) {
                return width_;
            }
            rest = here[w];
        }
        return static_cast<std::int64_t>(w) * 64 + std::countr_zero(rest);
    }

    /**
     * @brief First cell at or after @p from that is not set, or `width_`.
     *
     * @pre `0 <= from < width_`. Unlike @ref nextSetInRow this does not take
     *      `width_`: the word holding that cell is the one past the row when
     *      the width is a multiple of 64, and a row pointer cannot tell.
     */
    [[nodiscard]] std::int64_t nextClearInRow(const std::uint64_t* here, std::int64_t from) const {
        assert(from >= 0 && from < width_);
        std::size_t w = static_cast<std::size_t>(from) / 64;
        std::uint64_t rest = ~here[w] & (~std::uint64_t(0) << (from % 64));
        while (rest == 0) {
            if (++w >= words_) {
                return width_;
            }
            rest = ~here[w];
        }
        return std::min<std::int64_t>(
            static_cast<std::int64_t>(w) * 64 + std::countr_zero(rest), width_);
    }

    /**
     * @brief First cell of the run of set cells holding the set cell @p at.
     *
     * @pre `0 <= at < width_` and the cell is set, so the run it names exists.
     */
    [[nodiscard]] std::int64_t runStartInRow(const std::uint64_t* here, std::int64_t at) const {
        assert(at >= 0 && at < width_ && ((here[static_cast<std::size_t>(at) / 64] >>
                                           (at % 64)) & 1) != 0);
        std::size_t w = static_cast<std::size_t>(at) / 64;
        const int bit = static_cast<int>(at % 64);
        std::uint64_t rest = ~here[w] & (~std::uint64_t(0) >> (63 - bit));
        while (rest == 0) {
            if (w == 0) {
                return 0;
            }
            rest = ~here[--w];
        }
        return static_cast<std::int64_t>(w) * 64 + 64 - std::countl_zero(rest);
    }

    /**
     * @brief Clears the bits past the width in the last word of every row.
     *
     * Those bits stand for no cell, and every operation restores this invariant
     * before returning, which is what lets `count` add up whole words.
     */
    void maskTails() {
        const int tail = width_ % 64;
        if (emptyWindow() || tail == 0) {
            return;
        }
        const std::uint64_t mask = (std::uint64_t(1) << tail) - 1;
        for (int j = 0; j < height_; ++j) {
            row(j)[words_ - 1] &= mask;
        }
    }

    static std::uint64_t wordAt(const std::uint64_t* words, std::size_t count, std::int64_t index) {
        return index < 0 || index >= static_cast<std::int64_t>(count) ? 0
                                                                     : words[static_cast<std::size_t>(index)];
    }

    /**
     * @brief The 64 bits of a row starting at local bit @p offset.
     *
     * The offset may be negative or past the end of the row; the bits that name
     * no cell of the row read as zero. This is the one primitive every operation
     * between two differently aligned matrices goes through.
     */
    static std::uint64_t shiftedWord(const std::uint64_t* words, std::size_t count,
                                     std::int64_t offset) {
        const std::int64_t index = offset >= 0 ? offset / 64 : -((-offset + 63) / 64);
        const int rest = static_cast<int>(offset - index * 64);
        const std::uint64_t low = wordAt(words, count, index) >> rest;
        if (rest == 0) {
            return low;
        }
        return low | (wordAt(words, count, index + 1) << (64 - rest));
    }

    /** @brief Applies a word operation with @p other aligned to this window. */
    template <class WordOp>
    void combine(const BitMatrix& other, WordOp op) {
        const std::int64_t shift =
            static_cast<std::int64_t>(other.origin_.x()) - static_cast<std::int64_t>(origin_.x());
        for (int j = 0; j < height_; ++j) {
            const std::int64_t sourceRow = static_cast<std::int64_t>(origin_.y()) + j -
                                           static_cast<std::int64_t>(other.origin_.y());
            const bool inside = sourceRow >= 0 && sourceRow < other.height_;
            const std::uint64_t* source = inside ? other.row(static_cast<int>(sourceRow)) : nullptr;
            std::uint64_t* destination = row(j);
            for (std::size_t w = 0; w < words_; ++w) {
                const std::uint64_t word =
                    source == nullptr
                        ? 0
                        : shiftedWord(source, other.words_, static_cast<std::int64_t>(w) * 64 - shift);
                destination[w] = op(destination[w], word);
            }
        }
        maskTails();
    }

    /** @brief Calls @p fn on every non-zero word of this matrix and its counterpart. */
    template <class Fn>
    void forEachAlignedWord(const BitMatrix& other, Fn fn) const {
        const std::int64_t shift =
            static_cast<std::int64_t>(other.origin_.x()) - static_cast<std::int64_t>(origin_.x());
        for (int j = 0; j < height_; ++j) {
            const std::int64_t sourceRow = static_cast<std::int64_t>(origin_.y()) + j -
                                           static_cast<std::int64_t>(other.origin_.y());
            if (sourceRow < 0 || sourceRow >= other.height_) {
                continue;
            }
            const std::uint64_t* source = other.row(static_cast<int>(sourceRow));
            const std::uint64_t* here = row(j);
            for (std::size_t w = 0; w < words_; ++w) {
                if (here[w] == 0) {
                    continue;
                }
                fn(here[w], shiftedWord(source, other.words_, static_cast<std::int64_t>(w) * 64 - shift));
            }
        }
    }

    /**
     * @brief Whether a cell is shared with @p other translated by `(dx, dy)`.
     *
     * The early-exit counterpart of @ref andCount, and the primitive behind the
     * intersection predicates: a Chebyshev-distance-one sweep of the nine
     * offsets answers whether the closed regions meet at all.
     */
    [[nodiscard]] bool anyCommonCell(const BitMatrix& other, std::int64_t dx, std::int64_t dy) const {
        const std::int64_t shift = static_cast<std::int64_t>(other.origin_.x()) + dx -
                                   static_cast<std::int64_t>(origin_.x());
        for (int j = 0; j < height_; ++j) {
            const std::int64_t sourceRow = static_cast<std::int64_t>(origin_.y()) + j -
                                           static_cast<std::int64_t>(other.origin_.y()) - dy;
            if (sourceRow < 0 || sourceRow >= other.height_) {
                continue;
            }
            const std::uint64_t* source = other.row(static_cast<int>(sourceRow));
            const std::uint64_t* here = row(j);
            for (std::size_t w = 0; w < words_; ++w) {
                if (here[w] == 0) {
                    continue;
                }
                if ((here[w] & shiftedWord(source, other.words_,
                                           static_cast<std::int64_t>(w) * 64 - shift)) != 0) {
                    return true;
                }
            }
        }
        return false;
    }

    /**
     * @brief Or-assigns @p source into this matrix, shifted by local `(dx, dy)`.
     *
     * @pre The shifted source fits: both offsets are non-negative and the window
     *      holds the whole of it, which is how @ref latticeMinkowskiSum sizes it.
     */
    void orShifted(const BitMatrix& source, std::int64_t dx, std::int64_t dy) {
        assert(dx >= 0 && dy >= 0);
        assert(dy + source.height_ <= height_);
        const std::size_t wordShift = static_cast<std::size_t>(dx / 64);
        const int bitShift = static_cast<int>(dx % 64);
        for (int j = 0; j < source.height_; ++j) {
            const std::uint64_t* from = source.row(j);
            std::uint64_t* to = row(j + static_cast<int>(dy));
            for (std::size_t w = 0; w < source.words_; ++w) {
                const std::uint64_t word = from[w];
                if (word == 0) {
                    continue;
                }
                if (w + wordShift < words_) {
                    to[w + wordShift] |= word << bitShift;
                }
                if (bitShift != 0 && w + wordShift + 1 < words_) {
                    to[w + wordShift + 1] |= word >> (64 - bitShift);
                }
            }
        }
    }

    /** @brief And-assigns @p source, read at local `(i + dx, j + dy)`, into cell `(i, j)`. */
    void andShifted(const BitMatrix& source, std::int64_t dx, std::int64_t dy) {
        for (int j = 0; j < height_; ++j) {
            const std::int64_t sourceRow = static_cast<std::int64_t>(j) + dy;
            std::uint64_t* to = row(j);
            if (sourceRow < 0 || sourceRow >= source.height_) {
                std::fill(to, to + words_, std::uint64_t(0));
                continue;
            }
            const std::uint64_t* from = source.row(static_cast<int>(sourceRow));
            for (std::size_t w = 0; w < words_; ++w) {
                to[w] &= shiftedWord(from, source.words_, static_cast<std::int64_t>(w) * 64 + dx);
            }
        }
    }

    /** @brief Returns the image of the cells under a symmetry of the grid. */
    template <class Map>
    [[nodiscard]] BitMatrix mapped(Map map) const {
        if (emptyWindow()) {
            return BitMatrix();
        }
        const std::int64_t x0 = static_cast<std::int64_t>(origin_.x());
        const std::int64_t y0 = static_cast<std::int64_t>(origin_.y());
        const std::int64_t x1 = x0 + width_ - 1, y1 = y0 + height_ - 1;
        std::int64_t minX = 0, maxX = 0, minY = 0, maxY = 0;
        bool first = true;
        for (const std::int64_t x : {x0, x1}) {
            for (const std::int64_t y : {y0, y1}) {
                const auto [imageX, imageY] = map(x, y);
                minX = first ? imageX : std::min(minX, imageX);
                maxX = first ? imageX : std::max(maxX, imageX);
                minY = first ? imageY : std::min(minY, imageY);
                maxY = first ? imageY : std::max(maxY, imageY);
                first = false;
            }
        }
        BitMatrix result(PointType(static_cast<NumberType>(minX), static_cast<NumberType>(minY)),
                         static_cast<int>(maxX - minX) + 1, static_cast<int>(maxY - minY) + 1);
        for (const PointType& cell : *this) {
            const auto [imageX, imageY] =
                map(static_cast<std::int64_t>(cell.x()), static_cast<std::int64_t>(cell.y()));
            result.set(static_cast<NumberType>(imageX), static_cast<NumberType>(imageY));
        }
        return result;
    }

    /// The eight neighbor offsets, the four edge ones first.
    static constexpr std::array<std::pair<std::int64_t, std::int64_t>, 8> allNeighborOffsets{
        {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}}};

    static std::span<const std::pair<std::int64_t, std::int64_t>> neighborOffsets(
        GridAdjacency adjacency) {
        return std::span(allNeighborOffsets)
            .first(adjacency == GridAdjacency::vertex ? 8u : 4u);
    }

    /**
     * @brief The cells of `U (+) U`, the self-sum of the closed unit square.
     *
     * `[0,1]^2 (+) [0,1]^2` is `[0,2]^2`, which covers the four cells at the
     * offsets `{0,1}^2`. Dilating a lattice sum by it is what turns that sum
     * into the sum of the regions.
     */
    [[nodiscard]] static BitMatrix unitSquareSum() {
        BitMatrix stamp(PointType(), 2, 2);
        stamp.setAll();
        return stamp;
    }

    static GridAdjacency complementary(GridAdjacency adjacency) {
        return adjacency == GridAdjacency::edge ? GridAdjacency::vertex : GridAdjacency::edge;
    }

    /** @brief The first set cell in row-major order. */
    [[nodiscard]] std::optional<PointType> firstSetCell() const {
        for (int j = 0; j < height_; ++j) {
            const std::uint64_t* here = row(j);
            for (std::size_t w = 0; w < words_; ++w) {
                if (here[w] != 0) {
                    return PointType(
                        origin_.x() + static_cast<NumberType>(static_cast<std::int64_t>(w) * 64 +
                                                              std::countr_zero(here[w])),
                        origin_.y() + static_cast<NumberType>(j));
                }
            }
        }
        return std::nullopt;
    }

    /**
     * @brief Floods the group of set cells holding the cell (@p i, @p j).
     *
     * The cells are cleared from @p remaining, a row-major buffer laid out like
     * @ref bits_, and their runs appended to @p runs, which doubles as the queue
     * of runs still to be looked around. A run is found, cleared and queued in
     * one word-parallel step, and taking it off the queue only rescans the row
     * below and the row above it, so the flood costs one pass over the words of
     * the rows it touches plus a constant per run, with no per-cell neighbor
     * lookup and no set of visited cells.
     *
     * @param reach How far past the ends of a run a neighbor can sit: one cell
     *        for vertex adjacency, none for edge adjacency.
     */
    void floodRuns(std::uint64_t* remaining, int j, std::int64_t i, std::int64_t reach,
                   std::vector<CellRun>& runs) const {
        const std::size_t first = runs.size();
        auto rowOf = [&](int at) { return remaining + static_cast<std::size_t>(at) * words_; };
        auto take = [&](int at, std::int64_t cell) {
            std::uint64_t* here = rowOf(at);
            const CellRun run{at, runStartInRow(here, cell), nextClearInRow(here, cell)};
            clearWordRange(here, run.x0, run.x1);
            runs.push_back(run);
            return run;
        };
        auto scan = [&](int at, std::int64_t low, std::int64_t high) {
            if (at < 0 || at >= height_) {
                return;
            }
            const std::uint64_t* here = rowOf(at);
            high = std::min<std::int64_t>(high, width_);
            for (std::int64_t cell = nextSetInRow(here, std::max<std::int64_t>(low, 0)); cell < high;
                 cell = nextSetInRow(here, cell)) {
                cell = take(at, cell).x1;
            }
        };

        take(j, i);
        for (std::size_t next = first; next < runs.size(); ++next) {
            const CellRun run = runs[next];  // Copied: taking a run may reallocate.
            scan(run.y - 1, run.x0 - reach, run.x1 + reach);
            scan(run.y + 1, run.x0 - reach, run.x1 + reach);
        }
    }

    /** @brief The set cells reachable from @p seed, over this same window. */
    [[nodiscard]] BitMatrix floodFrom(const PointType& seed, GridAdjacency adjacency) const {
        BitMatrix result(origin_, width_, height_);
        if (!get(seed)) {
            return result;
        }
        std::vector<std::uint64_t> remaining = bits_;
        std::vector<CellRun> runs;
        floodRuns(remaining.data(), static_cast<int>(localY(seed.y())), localX(seed.x()),
                  adjacency == GridAdjacency::vertex ? 1 : 0, runs);
        for (const CellRun& run : runs) {
            result.setLocalRange(run.y, run.x0, run.x1);
        }
        return result;
    }

    /**
     * @brief Calls @p fn once per connected group, with the runs of the group.
     *
     * One @ref floodRuns per group over a scratch copy of the words, so the whole
     * traversal costs one pass over the words plus a constant per run of cells.
     *
     * The groups come out ordered by their lowest, then leftmost cell, and the
     * runs of a group in the order the fill reached them.
     */
    template <class Fn>
    void visitComponentRuns(GridAdjacency adjacency, Fn fn) const {
        if (emptyWindow()) {
            return;
        }
        std::vector<std::uint64_t> remaining = bits_;
        // A diagonal neighbor of a run sits one cell past either of its ends.
        const std::int64_t reach = adjacency == GridAdjacency::vertex ? 1 : 0;
        std::vector<CellRun> runs;
        for (int j = 0; j < height_; ++j) {
            const std::uint64_t* here = remaining.data() + static_cast<std::size_t>(j) * words_;
            for (std::int64_t i = nextSetInRow(here, 0); i < width_; i = nextSetInRow(here, i)) {
                runs.clear();
                floodRuns(remaining.data(), j, i, reach, runs);
                fn(std::as_const(runs));
            }
        }
    }

    /**
     * @brief Traces every boundary loop of the set cells, filled cells on the left.
     *
     * Each cell contributes its boundary edges oriented with the filled interior
     * on the left, and those directed edges split into closed loops: one
     * counterclockwise loop around each component, plus one clockwise loop
     * around each enclosed hole. The loops come out in window-local vertex
     * coordinates, which keeps the walk in `int` however far from the origin the
     * window sits.
     *
     * A vertex carries at most one outgoing edge per direction, so the edges are
     * four bit planes over the grid of `(width + 1) * (height + 1)` vertices,
     * each built a word at a time from the cell rows: an east edge at vertex
     * `(x, y)` is a set cell `(x, y)` over an unset `(x, y - 1)`, and the other
     * three directions are that same comparison shifted. Walking then only tests
     * and sets bits, so a region of `n` cells costs one pass over its `n / 64`
     * words plus a constant per boundary edge.
     *
     * At a vertex where two diagonally opposite cells are filled and the other
     * two are empty the boundary pinches, and four directed edges meet there --
     * the only vertex with an out-degree above one, and so the only place the
     * walk has a choice. Turning **right** crosses the pinch to the other filled
     * cell, which keeps the *background* on the right, so the loop around a hole
     * and the loop around the region enclosing it stay apart; turning **left**
     * stays on the near cell, so two groups of cells that touch only at that
     * corner keep separate loops. @p crosses decides, from the two filled cells,
     * which of the two the caller wants. Either way each incoming edge gets its
     * own successor, so the loops partition the boundary edges and each passes
     * the vertex at most once.
     *
     * @param crosses Called with the two diagonally opposite cells at a pinch;
     *        the walk crosses to the far one when it returns `true`.
     */
    template <class PinchCrosses>
    [[nodiscard]] std::vector<std::vector<detail::PolyCell>> boundaryLoops(
        PinchCrosses crosses) const {
        std::vector<std::vector<detail::PolyCell>> loops;
        if (emptyWindow()) {
            return loops;
        }
        const std::size_t vertexWords = wordsPerRow(width_ + 1);
        const std::size_t vertexRows = static_cast<std::size_t>(height_) + 1;
        const std::size_t plane = vertexRows * vertexWords;
        std::vector<std::uint64_t> present(4 * plane, 0);
        std::vector<std::uint64_t> used(4 * plane, 0);

        auto cellWord = [&](int j, std::size_t w) -> std::uint64_t {
            return j < 0 || j >= height_ || w >= words_ ? 0 : row(j)[w];
        };
        // The north and west edges of a vertex row come from the cells one to
        // the left, so they are built raw and then shifted across the words.
        std::vector<std::uint64_t> rawNorth(vertexWords), rawWest(vertexWords);
        for (std::size_t vertexRow = 0; vertexRow < vertexRows; ++vertexRow) {
            const int above = static_cast<int>(vertexRow);  // Cells resting on the row.
            const int below = above - 1;                    // Cells hanging under it.
            for (std::size_t w = 0; w < vertexWords; ++w) {
                const std::uint64_t upper = cellWord(above, w);
                const std::uint64_t lower = cellWord(below, w);
                const std::uint64_t upperRight = (upper >> 1) | (cellWord(above, w + 1) << 63);
                const std::uint64_t lowerLeft =
                    (lower << 1) | (w == 0 ? 0 : cellWord(below, w - 1) >> 63);
                present[0 * plane + vertexRow * vertexWords + w] = upper & ~lower;      // East.
                present[3 * plane + vertexRow * vertexWords + w] = lower & ~lowerLeft;  // South.
                rawNorth[w] = upper & ~upperRight;
                rawWest[w] = lower & ~upper;
            }
            std::uint64_t carryNorth = 0, carryWest = 0;
            for (std::size_t w = 0; w < vertexWords; ++w) {
                present[1 * plane + vertexRow * vertexWords + w] = (rawNorth[w] << 1) | carryNorth;
                present[2 * plane + vertexRow * vertexWords + w] = (rawWest[w] << 1) | carryWest;
                carryNorth = rawNorth[w] >> 63;
                carryWest = rawWest[w] >> 63;
            }
        }

        auto index = [&](int direction, int y, int x) {
            return static_cast<std::size_t>(direction) * plane +
                   static_cast<std::size_t>(y) * vertexWords + static_cast<std::size_t>(x) / 64;
        };
        auto hasEdge = [&](int direction, int y, int x) {
            return ((present[index(direction, y, x)] >> (x % 64)) & 1) != 0;
        };
        auto markUsed = [&](int direction, int y, int x) {
            used[index(direction, y, x)] |= std::uint64_t(1) << (x % 64);
        };

        auto trace = [&](int direction, int startX, int startY) {
            std::vector<detail::PolyCell> loop;
            int x = startX, y = startY;
            for (;;) {
                markUsed(direction, y, x);
                loop.emplace_back(x, y);
                x += boundarySteps[static_cast<std::size_t>(direction)].first;
                y += boundarySteps[static_cast<std::size_t>(direction)].second;
                if (x == startX && y == startY) {
                    break;
                }
                int next = (direction + 3) % 4;        // Sharpest right.
                const int left = (direction + 1) % 4;  // Sharpest left.
                if (hasEdge(next, y, x) && hasEdge(left, y, x)) {
                    // The cell just walked along is on the left of the edge that
                    // arrived here, which left the vertex one step back.
                    const auto& step = boundarySteps[static_cast<std::size_t>(direction)];
                    if (!crosses(leftCell(direction, x - step.first, y - step.second),
                                 leftCell(next, x, y))) {
                        next = left;
                    }
                } else {
                    // Preference order: sharpest right, straight, left, reverse.
                    for (int turn = 0; turn < 3 && !hasEdge(next, y, x); ++turn) {
                        next = (next + 1) % 4;
                    }
                }
                assert(hasEdge(next, y, x) && "pgl::BitMatrix: the boundary walk ran off an edge");
                direction = next;
            }
            return loop;
        };

        for (int direction = 0; direction < 4; ++direction) {
            for (int y = 0; y <= height_; ++y) {
                for (std::size_t w = 0; w < vertexWords; ++w) {
                    const std::size_t at =
                        static_cast<std::size_t>(direction) * plane +
                        static_cast<std::size_t>(y) * vertexWords + w;
                    while (const std::uint64_t rest = present[at] & ~used[at]) {
                        loops.push_back(trace(
                            direction, static_cast<int>(w) * 64 + std::countr_zero(rest), y));
                    }
                }
            }
        }
        return loops;
    }

    /** @brief The set cell on the left of the boundary edge leaving a vertex. */
    static detail::PolyCell leftCell(int direction, int x, int y) {
        switch (direction) {
            case 0:
                return {x, y};  // East, along the bottom edge of the cell.
            case 1:
                return {x - 1, y};  // North, along the right edge.
            case 2:
                return {x - 1, y - 1};  // West, along the top edge.
            default:
                return {x, y - 1};  // South, along the left edge.
        }
    }

    /** @brief The set cell on the left of the first edge of a boundary loop. */
    static detail::PolyCell loopSeedCell(const std::vector<detail::PolyCell>& loop) {
        const auto [x, y] = loop.front();
        const auto [nextX, nextY] = loop[1];
        return leftCell(nextX > x ? 0 : nextY > y ? 1 : nextX < x ? 2 : 3, x, y);
    }

    /**
     * @brief Assembles boundary loops into the region they bound.
     *
     * The counterclockwise loop becomes the outer boundary and every clockwise
     * loop a hole, with the vertices in the middle of a straight stretch dropped
     * on the way. The loops are in window-local coordinates, so the region is
     * translated back to the window here.
     */
    [[nodiscard]] RegionType regionFromLoops(
        const std::vector<std::vector<detail::PolyCell>>& loops) const {
        Polygon<Point<NumberType>> outer;
        std::vector<Polygon<Point<NumberType>>> holes;
        for (const std::vector<detail::PolyCell>& loop : loops) {
            if (detail::loopTwiceArea(loop) > 0) {
                outer = Polygon<Point<NumberType>>(detail::loopCorners<NumberType>(loop));
            } else {
                holes.emplace_back(detail::loopCorners<NumberType>(loop));
            }
        }
        return RegionType(
            Transformation<NumberType>::translation(origin_.x(), origin_.y()) *
            PolygonWithHoles<Point<NumberType>>(std::move(outer), std::move(holes)));
    }

    static RectangleType hullWindow(const BitMatrix& left, const BitMatrix& right) {
        if (left.emptyWindow()) {
            return right.window();
        }
        if (right.emptyWindow()) {
            return left.window();
        }
        const RectangleType a = left.window(), b = right.window();
        return RectangleType(PointType(std::min(a.min().x(), b.min().x()), std::min(a.min().y(), b.min().y())),
                             PointType(std::max(a.max().x(), b.max().x()), std::max(a.max().y(), b.max().y())),
                             true);
    }

    static RectangleType overlapWindow(const BitMatrix& left, const BitMatrix& right) {
        if (left.emptyWindow() || right.emptyWindow()) {
            return RectangleType();
        }
        const RectangleType a = left.window(), b = right.window();
        return RectangleType(PointType(std::max(a.min().x(), b.min().x()), std::max(a.min().y(), b.min().y())),
                             PointType(std::min(a.max().x(), b.max().x()), std::min(a.max().y(), b.max().y())),
                             true);
    }

    PointType origin_{};
    int width_ = 0;
    int height_ = 0;
    std::size_t words_ = 0;
    std::vector<std::uint64_t> bits_;
};

/** @brief Returns the same cells translated by a vector. */
template <class PointType>
BitMatrix<PointType> operator+(const PointType& vector, const BitMatrix<PointType>& matrix) {
    return matrix.translated(vector);
}

// Written out rather than left to the implicit guides: the class template is
// constrained, and clang 18 mishandles a constraint carried onto an implicit
// guide.
template <class PointType>
BitMatrix(PointType, int, int) -> BitMatrix<PointType>;

template <class PointType, class LabelType>
BitMatrix(const Rectangle<PointType, LabelType>&) -> BitMatrix<PointType>;

template <class PointType, class LabelType>
BitMatrix(const PolygonWithHoles<PointType, LabelType>&) -> BitMatrix<PointType>;

template <class PointType, class LabelType>
BitMatrix(const Polygon<PointType, LabelType>&) -> BitMatrix<PointType>;

template <class PointType, class LabelType>
BitMatrix(const PolygonSet<PointType, LabelType>&) -> BitMatrix<PointType>;

// The point-range constructor deduces nothing on its own -- PointType does not
// appear in its signature -- so without this guide a range of points would take
// the default point type whatever it holds.
template <std::ranges::input_range Range>
    requires(detail::is_point_v<std::remove_cvref_t<std::ranges::range_value_t<Range>>>
             && !AnyShapeConcept<std::remove_cvref_t<Range>>
             && !detail::is_bit_matrix_v<std::remove_cvref_t<Range>>)
BitMatrix(Range&&) -> BitMatrix<std::remove_cvref_t<std::ranges::range_value_t<Range>>>;

// Out-of-line: asBitMatrix is declared in the shape headers (which precede this
// one in the layering) but can only be defined once BitMatrix is visible. Each
// is the rasterizing constructor for that shape, so it is the constructor that
// documents the window, the fill rule and the rectilinear requirement.
template <class PointType_, class TLabel>
template <class ResultNumber>
    requires(std::signed_integral<ResultNumber>)
auto Polygon<PointType_, TLabel>::asBitMatrix() const {
    return BitMatrix<detail::grid_point_t<PointType_, ResultNumber>>(*this);
}

template <class PointType_, class TLabel>
template <class ResultNumber>
    requires(std::signed_integral<ResultNumber>)
auto PolygonWithHoles<PointType_, TLabel>::asBitMatrix() const {
    return BitMatrix<detail::grid_point_t<PointType_, ResultNumber>>(*this);
}

template <class PointType_, class TLabel>
template <class ResultNumber>
    requires(std::signed_integral<ResultNumber>)
auto PolygonSet<PointType_, TLabel>::asBitMatrix() const {
    return BitMatrix<detail::grid_point_t<PointType_, ResultNumber>>(*this);
}

namespace detail {

/** @brief Tests every cell of a window and sets the ones the predicate keeps. */
template <class PointType, class Predicate>
BitMatrix<PointType> rasterize(const Rectangle<PointType>& window, Predicate keep) {
    using Number = typename PointType::NumberType;
    BitMatrix<PointType> result(window);
    for (int j = 0; j < result.height(); ++j) {
        for (int i = 0; i < result.width(); ++i) {
            const Number x = result.origin().x() + static_cast<Number>(i);
            const Number y = result.origin().y() + static_cast<Number>(j);
            if (keep(Rectangle<PointType>(PointType(x, y),
                                          PointType(x + Number(1), y + Number(1)), true))) {
                result.set(x, y);
            }
        }
    }
    return result;
}

}  // namespace detail

/**
 * @brief Rasterizes a shape into the cells it meets: its outer approximation.
 *
 * A cell is set when the shape intersects it, boundary included, so the result
 * covers the shape. Costs one exact predicate per cell of @p window, and works
 * for every shape; @ref BitMatrix::BitMatrix(const PolygonWithHoles<PointType, TLabel>&) is the
 * cheap
 * path for a rectilinear region.
 *
 * @tparam PointType Cell type of the result, deduced from @p window.
 * @param shape Shape to rasterize.
 * @param window Window of the result, read as @ref BitMatrix::BitMatrix(const RectangleType&) reads it.
 */
template <class PointType, class ShapeType>
BitMatrix<PointType> outerRaster(const ShapeType& shape, const Rectangle<PointType>& window) {
    return detail::rasterize<PointType>(
        window, [&shape](const Rectangle<PointType>& cell) { return shape.intersects(cell); });
}

/**
 * @brief Rasterizes a shape into the cells it covers: its inner approximation.
 *
 * A cell is set when the shape contains the whole of it, so the result is
 * covered by the shape. Costs one exact predicate per cell of @p window.
 *
 * @tparam PointType Cell type of the result, deduced from @p window.
 * @param shape Shape to rasterize.
 * @param window Window of the result, read as @ref BitMatrix::BitMatrix(const RectangleType&) reads it.
 */
template <class PointType, class ShapeType>
BitMatrix<PointType> innerRaster(const ShapeType& shape, const Rectangle<PointType>& window) {
    return detail::rasterize<PointType>(
        window, [&shape](const Rectangle<PointType>& cell) { return shape.contains(cell); });
}

/**
 * @brief Rasterizes a bounded shape over its own bounding box.
 *
 * @tparam PointType Cell type of the result; `Point<int>` unless given.
 * @param shape Bounded shape with integer coordinates.
 */
template <class PointType = Point<int>, class ShapeType>
    requires std::signed_integral<
        std::remove_cvref_t<decltype(std::declval<const ShapeType&>().bbox().min().x())>>
BitMatrix<PointType> outerRaster(const ShapeType& shape) {
    return outerRaster<PointType>(shape, Rectangle<PointType>(shape.bbox()));
}

/**
 * @brief Rasterizes a bounded shape over its own bounding box.
 *
 * @tparam PointType Cell type of the result; `Point<int>` unless given.
 * @param shape Bounded shape with integer coordinates.
 */
template <class PointType = Point<int>, class ShapeType>
    requires std::signed_integral<
        std::remove_cvref_t<decltype(std::declval<const ShapeType&>().bbox().min().x())>>
BitMatrix<PointType> innerRaster(const ShapeType& shape) {
    return innerRaster<PointType>(shape, Rectangle<PointType>(shape.bbox()));
}

}  // namespace pgl

namespace std {

/**
 * @brief Hash support for BitMatrix.
 *
 * Hashes the window and the cells, so it agrees with `operator==`. Two matrices
 * covering the same region over different windows are different values and hash
 * apart; `trimmed()` brings them to the one form that hashes alike. The cells
 * are hashed as the packed words that `operator==` compares, a word at a time
 * rather than a cell at a time.
 */
template <class PointType>
struct hash<pgl::BitMatrix<PointType>> {
    std::size_t operator()(const pgl::BitMatrix<PointType>& matrix) const {
        std::size_t seed = 0;
        pgl::detail::hashCombine(seed, matrix.origin());
        pgl::detail::hashCombine(seed, matrix.width());
        pgl::detail::hashCombine(seed, matrix.height());
        for (const std::uint64_t word : matrix.bits_) {
            pgl::detail::hashCombine(seed, word);
        }
        return seed;
    }
};

}  // namespace std
