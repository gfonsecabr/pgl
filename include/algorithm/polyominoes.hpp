#pragma once

#include "algorithm/sortpoints.hpp"
#include <cstdint>

/**
 * @file polyominoes.hpp
 * @brief Enumeration of polyominoes as Pangolin polygons.
 *
 * A polyomino is a finite, edge-connected set of unit cells of the integer
 * grid. This header enumerates the *free* polyominoes of a given size (cells
 * counted up to translation, rotation and reflection) and returns each as the
 * `Polygon` tracing its boundary, or — for @ref polyominoRegions — as the
 * `PolygonWithHoles` bounded by all of its boundary loops.
 */

#include <algorithm>
#include <array>
#include <cstddef>
#include <map>
#include <set>
#include <utility>
#include <vector>

namespace pgl {

namespace detail {

/// A grid cell, identified by the integer coordinates of its lower-left corner.
using PolyCell = std::pair<int, int>;
/// A polyomino as a sorted, translation-normalized list of its cells.
using CellSet = std::vector<PolyCell>;

/// The four edge-neighbor offsets of a cell.
inline constexpr std::array<PolyCell, 4> polyNeighbors{
    PolyCell{1, 0}, PolyCell{-1, 0}, PolyCell{0, 1}, PolyCell{0, -1}};

/// Sorts the cells and translates them so the minimum x and y are both zero.
inline CellSet normalizeCells(CellSet cells) {
    if (cells.empty()) {
        return cells;
    }
    int minX = cells.front().first;
    int minY = cells.front().second;
    for (const auto& [x, y] : cells) {
        minX = std::min(minX, x);
        minY = std::min(minY, y);
    }
    for (auto& [x, y] : cells) {
        x -= minX;
        y -= minY;
    }
    std::sort(cells.begin(), cells.end());
    return cells;
}

/**
 * @brief Generates every fixed polyomino (translation class) of @p n cells.
 *
 * Grows polyominoes one cell at a time from the single-cell seed, normalizing
 * and de-duplicating after each step. Fixed polyominoes distinguish rotations
 * and reflections; @ref polyominoes folds those away afterwards.
 */
inline std::vector<CellSet> fixedPolyominoes(int n) {
    if (n <= 0) {
        return {};
    }
    std::set<CellSet> current{normalizeCells({{0, 0}})};
    for (int k = 1; k < n; ++k) {
        std::set<CellSet> next;
        for (const auto& poly : current) {
            const std::set<PolyCell> occupied(poly.begin(), poly.end());
            for (const auto& cell : poly) {
                for (const auto& [dx, dy] : polyNeighbors) {
                    const PolyCell candidate{cell.first + dx, cell.second + dy};
                    if (occupied.count(candidate)) {
                        continue;
                    }
                    CellSet grown = poly;
                    grown.push_back(candidate);
                    next.insert(normalizeCells(std::move(grown)));
                }
            }
        }
        current.swap(next);
    }
    return {current.begin(), current.end()};
}

/// Applies one of the eight square symmetries (bit 0: reflect; bits 1-2:
/// number of 90-degree rotations) and renormalizes.
inline CellSet transformCells(const CellSet& cells, int symmetry) {
    CellSet out;
    out.reserve(cells.size());
    for (auto [x, y] : cells) {
        if (symmetry & 1) {
            x = -x;
        }
        for (int rot = symmetry >> 1; rot > 0; --rot) {
            const int nx = -y;
            const int ny = x;
            x = nx;
            y = ny;
        }
        out.push_back({x, y});
    }
    return normalizeCells(std::move(out));
}

/**
 * @brief Tests whether a polyomino encloses a hole.
 *
 * Flood-fills the background starting outside the bounding box; any empty cell
 * inside the box that the fill cannot reach is an enclosed hole. Holey
 * polyominoes (possible from seven cells onward) are not simple polygons, so
 * @ref polyominoes drops them.
 */
inline bool polyominoHasHole(const CellSet& cells) {
    if (cells.empty()) {
        return false;
    }
    int maxX = 0, maxY = 0;
    for (const auto& [x, y] : cells) {
        maxX = std::max(maxX, x);
        maxY = std::max(maxY, y);
    }
    const std::set<PolyCell> occupied(cells.begin(), cells.end());

    // Flood the exterior, padded by one cell so it surrounds the whole box.
    const auto inside = [&](int x, int y) {
        return x >= -1 && x <= maxX + 1 && y >= -1 && y <= maxY + 1;
    };
    std::set<PolyCell> reached{{-1, -1}};
    std::vector<PolyCell> stack{{-1, -1}};
    while (!stack.empty()) {
        const auto [x, y] = stack.back();
        stack.pop_back();
        for (const auto& [dx, dy] : polyNeighbors) {
            const PolyCell next{x + dx, y + dy};
            if (!inside(next.first, next.second) || occupied.count(next) ||
                reached.count(next)) {
                continue;
            }
            reached.insert(next);
            stack.push_back(next);
        }
    }

    for (int x = 0; x <= maxX; ++x) {
        for (int y = 0; y <= maxY; ++y) {
            const PolyCell cell{x, y};
            if (!occupied.count(cell) && !reached.count(cell)) {
                return true;
            }
        }
    }
    return false;
}

/// Canonical representative of a polyomino's free class: the lexicographically
/// smallest of its eight rotated/reflected, normalized forms.
inline CellSet canonicalFree(const CellSet& cells) {
    CellSet best = transformCells(cells, 0);
    for (int symmetry = 1; symmetry < 8; ++symmetry) {
        CellSet candidate = transformCells(cells, symmetry);
        if (candidate < best) {
            best = std::move(candidate);
        }
    }
    return best;
}

/**
 * @brief Traces every boundary loop of a polyomino, filled cells on the left.
 *
 * Each cell contributes its boundary edges oriented with the filled interior on
 * the left. Those directed edges split into closed loops: one counterclockwise
 * loop around the polyomino, plus one clockwise loop around each enclosed hole.
 *
 * At a vertex where two diagonally opposite cells are filled and the other two
 * are empty the boundary pinches, and four directed edges meet there — the only
 * place the walk has a choice. It takes the sharpest **right** turn, which keeps
 * the *background* component on its right across the pinch, so each loop stays
 * the boundary of one such component and the loops through the vertex stay
 * apart. The two empty cells at a pinch always belong to different background
 * components — the filled path joining the two diagonal cells closes a curve
 * through the pinch that separates them — so each loop passes it at most once.
 */
inline std::vector<std::vector<PolyCell>> polyominoLoops(const CellSet& cells) {
    const std::set<PolyCell> occupied(cells.begin(), cells.end());

    // Directed boundary edges keyed by source vertex; interior on the left.
    std::map<PolyCell, std::vector<PolyCell>> outgoing;
    for (const auto& [x, y] : cells) {
        if (!occupied.count({x, y - 1})) {
            outgoing[{x, y}].push_back({x + 1, y});          // bottom, heading east
        }
        if (!occupied.count({x + 1, y})) {
            outgoing[{x + 1, y}].push_back({x + 1, y + 1});  // right, heading north
        }
        if (!occupied.count({x, y + 1})) {
            outgoing[{x + 1, y + 1}].push_back({x, y + 1});  // top, heading west
        }
        if (!occupied.count({x - 1, y})) {
            outgoing[{x, y + 1}].push_back({x, y});          // left, heading south
        }
    }

    std::set<std::pair<PolyCell, PolyCell>> used;
    std::vector<std::vector<PolyCell>> loops;
    for (const auto& [source, targets] : outgoing) {
        for (const PolyCell& first : targets) {
            if (used.count({source, first})) {
                continue;
            }
            used.insert({source, first});
            std::vector<PolyCell> loop{source};
            int dx = first.first - source.first;
            int dy = first.second - source.second;
            PolyCell current = first;

            // The preference gives every directed edge one successor and every
            // vertex as many outgoing edges as incoming, so the successors of
            // the edges leaving a vertex are distinct: following them from an
            // unused edge walks a whole loop and stops back at its source.
            while (current != source) {
                loop.push_back(current);
                // Preference order: sharpest right, straight, left, then reverse.
                const std::array<PolyCell, 4> preferred{
                    PolyCell{dy, -dx}, PolyCell{dx, dy}, PolyCell{-dy, dx},
                    PolyCell{-dx, -dy}};
                const std::vector<PolyCell>& edges = outgoing.at(current);
                for (const auto& [pdx, pdy] : preferred) {
                    const PolyCell target{current.first + pdx, current.second + pdy};
                    if (std::find(edges.begin(), edges.end(), target) == edges.end()) {
                        continue;
                    }
                    used.insert({current, target});
                    dx = pdx;
                    dy = pdy;
                    current = target;
                    break;
                }
            }
            loops.push_back(std::move(loop));
        }
    }
    return loops;
}

/// Twice the signed area of a closed loop of grid vertices; positive when the
/// loop runs counterclockwise, so it tells an outer boundary from a hole.
inline int64_t loopTwiceArea(const std::vector<PolyCell>& loop) {
    int64_t twice = 0;
    const std::size_t m = loop.size();
    for (std::size_t i = 0; i < m; ++i) {
        const PolyCell here = loop[i];
        const PolyCell next = loop[(i + 1) % m];
        twice += static_cast<int64_t>(here.first) * next.second -
                 static_cast<int64_t>(next.first) * here.second;
    }
    return twice;
}

/// Drops the collinear vertices of a boundary loop, keeping only its corners.
template <class T>
std::vector<Point<T>> loopCorners(const std::vector<PolyCell>& loop) {
    std::vector<Point<T>> corners;
    const std::size_t m = loop.size();
    for (std::size_t i = 0; i < m; ++i) {
        const PolyCell prev = loop[(i + m - 1) % m];
        const PolyCell here = loop[i];
        const PolyCell next = loop[(i + 1) % m];
        const int64_t cross =
            static_cast<int64_t>(here.first - prev.first) * (next.second - here.second) -
            static_cast<int64_t>(here.second - prev.second) * (next.first - here.first);
        if (cross != 0) {
            corners.emplace_back(static_cast<T>(here.first), static_cast<T>(here.second));
        }
    }
    return corners;
}

/**
 * @brief Traces the outer boundary of a polyomino into counterclockwise corners.
 *
 * The counterclockwise loop of @ref polyominoLoops, reduced to its corners. A
 * hole-free polyomino (see @ref polyominoHasHole) has that loop and no other,
 * which is what @ref polyominoes relies on; a holed one loses its holes here.
 */
template <class T>
std::vector<Point<T>> polyominoOutline(const CellSet& cells) {
    for (const auto& loop : polyominoLoops(cells)) {
        if (loopTwiceArea(loop) > 0) {
            return loopCorners<T>(loop);
        }
    }
    return {};
}

/**
 * @brief Builds the region a polyomino covers, holes included.
 *
 * The counterclockwise loop becomes the outer boundary and every clockwise loop
 * a hole. A connected polyomino has exactly one of the former: its complement's
 * unbounded component has connected boundary, and each bounded component is
 * simply connected, since a background component enclosing filled cells would
 * split the polyomino in two.
 */
template <class T>
PolygonWithHoles<Point<T>> polyominoRegion(const CellSet& cells) {
    Polygon<Point<T>> outer;
    std::vector<Polygon<Point<T>>> holes;
    for (const auto& loop : polyominoLoops(cells)) {
        if (loopTwiceArea(loop) > 0) {
            outer = Polygon<Point<T>>(loopCorners<T>(loop));
        } else {
            holes.emplace_back(loopCorners<T>(loop));
        }
    }
    return PolygonWithHoles<Point<T>>(std::move(outer), std::move(holes));
}

}  // namespace detail

/**
 * @brief Enumerates the free polyominoes of a given size as polygons.
 *
 * Returns one `Polygon<Point<T>>` per free polyomino of @p size cells, i.e.
 * counted up to translation, rotation and reflection (5 for size 4, 12 for
 * size 5, and so on). Each polygon traces the polyomino's boundary with small
 * non-negative integer coordinates, normalized like any other `Polygon`
 * (counterclockwise, lexicographically smallest vertex first).
 *
 * Polyominoes that enclose a hole (possible from seven cells onward) are
 * omitted: their boundary is not a simple polygon, which `Polygon` cannot
 * represent. Use @ref polyominoRegions to get every polyomino, holes and all.
 *
 * @tparam T Coordinate type of the returned points (defaults to `int`).
 * @param size Number of cells in each polyomino; `0` yields no polyominoes.
 * @return The hole-free free polyominoes of @p size cells, in a deterministic order.
 */
template <class T = int>
std::vector<Polygon<Point<T>>> polyominoes(std::size_t size) {
    std::vector<Polygon<Point<T>>> result;
    if (size == 0) {
        return result;
    }

    std::set<detail::CellSet> freeForms;
    for (const auto& fixed : detail::fixedPolyominoes(static_cast<int>(size))) {
        freeForms.insert(detail::canonicalFree(fixed));
    }

    for (const auto& form : freeForms) {
        if (detail::polyominoHasHole(form)) {
            continue;
        }
        result.emplace_back(detail::polyominoOutline<T>(form));
    }
    return result;
}

/**
 * @brief Enumerates the free polyominoes of every size in `[n1, n2]`.
 *
 * Concatenates @ref polyominoes for each size from @p n1 to @p n2 inclusive,
 * smallest first. An empty range (`n1 > n2`) yields no polyominoes.
 *
 * @tparam T Coordinate type of the returned points (defaults to `int`).
 * @param n1 Smallest size to include.
 * @param n2 Largest size to include.
 * @return The hole-free free polyominoes of every size in the range.
 */
template <class T = int>
std::vector<Polygon<Point<T>>> polyominoes(std::size_t n1, std::size_t n2) {
    std::vector<Polygon<Point<T>>> result;
    for (std::size_t size = n1; size <= n2; ++size) {
        std::vector<Polygon<Point<T>>> sized = polyominoes<T>(size);
        result.insert(result.end(), std::make_move_iterator(sized.begin()),
                      std::make_move_iterator(sized.end()));
    }
    return result;
}

/**
 * @brief Enumerates the free polyominoes of every size from `1` to @p n.
 *
 * Convenience for @ref polyominoes(std::size_t, std::size_t) with a lower bound
 * of one, smallest first.
 *
 * @tparam T Coordinate type of the returned points (defaults to `int`).
 * @param n Largest size to include.
 * @return The hole-free free polyominoes of every size up to @p n.
 */
template <class T = int>
std::vector<Polygon<Point<T>>> polyominoesUpTo(std::size_t n) {
    return polyominoes<T>(1, n);
}

/**
 * @brief Enumerates the free polyominoes of a given size as regions.
 *
 * Like @ref polyominoes, but returning a `PolygonWithHoles<Point<T>>` per free
 * polyomino and omitting **none** of them: a region represents an enclosed hole,
 * so the holed polyominoes — which appear from seven cells on and are a growing
 * fraction of the answer — are kept. The count is therefore the full free
 * polyomino sequence (108 for size seven, where @ref polyominoes returns 107).
 *
 * Each region has small non-negative integer coordinates and canonical rings.
 * Its area is the cell count, and its holes may touch the outer boundary at a
 * point: two diagonally opposite cells of a polyomino can pinch a hole shut
 * against the outside, which @ref PolygonWithHoles::isValid accepts.
 *
 * @tparam T Coordinate type of the returned points (defaults to `int`).
 * @param size Number of cells in each polyomino; `0` yields no polyominoes.
 * @return The free polyominoes of @p size cells, in a deterministic order.
 */
template <class T = int>
std::vector<PolygonWithHoles<Point<T>>> polyominoRegions(std::size_t size) {
    std::vector<PolygonWithHoles<Point<T>>> result;
    if (size == 0) {
        return result;
    }

    std::set<detail::CellSet> freeForms;
    for (const auto& fixed : detail::fixedPolyominoes(static_cast<int>(size))) {
        freeForms.insert(detail::canonicalFree(fixed));
    }

    for (const auto& form : freeForms) {
        result.push_back(detail::polyominoRegion<T>(form));
    }
    return result;
}

/**
 * @brief Enumerates the free polyominoes of every size in `[n1, n2]` as regions.
 *
 * Concatenates @ref polyominoRegions for each size from @p n1 to @p n2
 * inclusive, smallest first. An empty range (`n1 > n2`) yields no polyominoes.
 *
 * @tparam T Coordinate type of the returned points (defaults to `int`).
 * @param n1 Smallest size to include.
 * @param n2 Largest size to include.
 * @return The free polyominoes of every size in the range, holes included.
 */
template <class T = int>
std::vector<PolygonWithHoles<Point<T>>> polyominoRegions(std::size_t n1, std::size_t n2) {
    std::vector<PolygonWithHoles<Point<T>>> result;
    for (std::size_t size = n1; size <= n2; ++size) {
        std::vector<PolygonWithHoles<Point<T>>> sized = polyominoRegions<T>(size);
        result.insert(result.end(), std::make_move_iterator(sized.begin()),
                      std::make_move_iterator(sized.end()));
    }
    return result;
}

/**
 * @brief Enumerates the free polyominoes of every size from `1` to @p n as regions.
 *
 * Convenience for @ref polyominoRegions(std::size_t, std::size_t) with a lower
 * bound of one, smallest first.
 *
 * @tparam T Coordinate type of the returned points (defaults to `int`).
 * @param n Largest size to include.
 * @return The free polyominoes of every size up to @p n, holes included.
 */
template <class T = int>
std::vector<PolygonWithHoles<Point<T>>> polyominoRegionsUpTo(std::size_t n) {
    return polyominoRegions<T>(1, n);
}

}  // namespace pgl
