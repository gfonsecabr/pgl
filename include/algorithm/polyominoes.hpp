#pragma once

#ifndef PGL_HPP_INCLUDED
#error "Do not include this Pangolin header directly; include \"pgl.hpp\" instead."
#endif

/**
 * @file polyominoes.hpp
 * @brief Enumeration of polyominoes as Pangolin polygons.
 *
 * A polyomino is a finite, edge-connected set of unit cells of the integer
 * grid. This header enumerates the *free* polyominoes of a given size (cells
 * counted up to translation, rotation and reflection) and returns each as the
 * `Polygon` tracing its boundary.
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
 * @brief Traces the boundary of a polyomino into counterclockwise corners.
 *
 * Each cell contributes its boundary edges oriented with the filled interior on
 * the left, so the collected directed edges form counterclockwise loops. The
 * walk starts at the lexicographically smallest vertex (a lower-left corner,
 * always on the boundary). Collinear vertices are dropped so only true corners
 * remain. The polyomino must be hole-free (see @ref polyominoHasHole), which
 * @ref polyominoes guarantees; its boundary is then a single simple loop.
 */
template <class T>
std::vector<Point<T>> polyominoOutline(const CellSet& cells) {
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

    auto take = [&outgoing](PolyCell from, PolyCell to) {
        auto& edges = outgoing[from];
        edges.erase(std::find(edges.begin(), edges.end(), to));
    };

    const PolyCell start = outgoing.begin()->first;  // global minimum vertex
    std::vector<PolyCell> loop{start};

    int dx = 1, dy = 0;                              // leave the start heading east
    PolyCell current{start.first + 1, start.second};
    take(start, current);

    while (current != start) {
        loop.push_back(current);
        // Preference order: sharpest left, straight, right, then reverse.
        const std::array<PolyCell, 4> preferred{
            PolyCell{-dy, dx}, PolyCell{dx, dy}, PolyCell{dy, -dx}, PolyCell{-dx, -dy}};
        auto& edges = outgoing[current];
        for (const auto& [pdx, pdy] : preferred) {
            const PolyCell target{current.first + pdx, current.second + pdy};
            auto it = std::find(edges.begin(), edges.end(), target);
            if (it != edges.end()) {
                edges.erase(it);
                dx = pdx;
                dy = pdy;
                current = target;
                break;
            }
        }
    }

    // Drop collinear vertices, keeping only corners.
    std::vector<Point<T>> corners;
    const std::size_t m = loop.size();
    for (std::size_t i = 0; i < m; ++i) {
        const PolyCell prev = loop[(i + m - 1) % m];
        const PolyCell here = loop[i];
        const PolyCell next = loop[(i + 1) % m];
        const long cross =
            static_cast<long>(here.first - prev.first) * (next.second - here.second) -
            static_cast<long>(here.second - prev.second) * (next.first - here.first);
        if (cross != 0) {
            corners.emplace_back(static_cast<T>(here.first), static_cast<T>(here.second));
        }
    }
    return corners;
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
 * represent.
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

}  // namespace pgl
