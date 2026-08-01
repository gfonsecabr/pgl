#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstdint>
#include <cstdlib>
#include <set>
#include <vector>

#include "pgl.hpp"

// Number of free polyominoes per size (OEIS A000105): 1, 1, 2, 5, 12, 35, ...
// From size 7 the count includes one polyomino with a hole, which pgl omits
// because a Polygon must be simple, so the hole-free counts are 107, 363, ...
TEST_CASE("polyominoes counts match the free-polyomino sequence") {
    CHECK(pgl::polyominoes(0).size() == 0);
    CHECK(pgl::polyominoes(1).size() == 1);
    CHECK(pgl::polyominoes(2).size() == 1);
    CHECK(pgl::polyominoes(3).size() == 2);
    CHECK(pgl::polyominoes(4).size() == 5);
    CHECK(pgl::polyominoes(5).size() == 12);
    CHECK(pgl::polyominoes(6).size() == 35);
    CHECK(pgl::polyominoes(7).size() == 107);  // 108 free, minus the holed one
}

TEST_CASE("each polyomino is a simple polygon of the right area") {
    for (std::size_t n = 1; n <= 7; ++n) {
        for (const auto& poly : pgl::polyominoes(n)) {
            // Area equals the cell count for a hole-free polyomino.
            CHECK(poly.area() == static_cast<int>(n));
            // Coordinates are small non-negative integers.
            for (const auto& vertex : poly) {
                CHECK(vertex.x() >= 0);
                CHECK(vertex.y() >= 0);
            }
        }
    }
}

TEST_CASE("results are distinct and honor the coordinate type") {
    const auto tetrominoes = pgl::polyominoes(4);
    const std::set<pgl::Polygon<pgl::Point<int>>> unique(tetrominoes.begin(),
                                                         tetrominoes.end());
    CHECK(unique.size() == tetrominoes.size());

    // The template argument selects the coordinate type.
    auto wide = pgl::polyominoes<std::int64_t>(3);
    static_assert(
        std::is_same_v<decltype(wide), std::vector<pgl::Polygon<pgl::Point<std::int64_t>>>>);
    CHECK(wide.size() == 2);
}

TEST_CASE("range and up-to overloads concatenate per-size results") {
    // [n1, n2] is the sum of the individual sizes.
    CHECK(pgl::polyominoes(4, 6).size() == 5 + 12 + 35);
    CHECK(pgl::polyominoes(1, 4).size() == 1 + 1 + 2 + 5);
    CHECK(pgl::polyominoes(5, 5).size() == 12);

    // Empty / degenerate ranges.
    CHECK(pgl::polyominoes(6, 4).size() == 0);
    CHECK(pgl::polyominoes(0, 0).size() == 0);

    // Up-to n is the same as the range [1, n].
    CHECK(pgl::polyominoesUpTo(5).size() == 1 + 1 + 2 + 5 + 12);
    CHECK(pgl::polyominoesUpTo(5).size() == pgl::polyominoes(1, 5).size());

    // Smallest first: the first entry is the single monomino (one cell).
    const auto upTo = pgl::polyominoesUpTo(4);
    REQUIRE(!upTo.empty());
    CHECK(upTo.front().area() == 1);

    // The template argument carries through the overloads.
    auto wide = pgl::polyominoes<std::int64_t>(2, 3);
    static_assert(
        std::is_same_v<decltype(wide), std::vector<pgl::Polygon<pgl::Point<std::int64_t>>>>);
    CHECK(wide.size() == 1 + 2);
}

TEST_CASE("the single domino is a 1x2 rectangle outline") {
    const auto dominoes = pgl::polyominoes(2);
    REQUIRE(dominoes.size() == 1);
    // Four corners, area two.
    CHECK(dominoes.front().size() == 4);
    CHECK(dominoes.front().area() == 2);
}

// --- Regions: the same enumeration, with the holed polyominoes kept ---------

namespace {

using Cell = std::pair<int, int>;

/**
 * Recovers the cells a region covers, without using any of the tracing code:
 * a cell belongs to the polyomino exactly when its centre is inside the region.
 * The centres are half-integers, so the region is doubled first and the test
 * stays in exact integer arithmetic.
 */
std::set<Cell> cellsOf(const pgl::PolygonWithHoles<>& region) {
    pgl::PolygonWithHoles<> doubled = region;
    doubled *= 2;
    const auto& box = region.bbox();
    std::set<Cell> cells;
    for (int x = box.min().x(); x < box.max().x(); ++x) {
        for (int y = box.min().y(); y < box.max().y(); ++y) {
            if (doubled.contains(pgl::Point<>(2 * x + 1, 2 * y + 1))) {
                cells.insert({x, y});
            }
        }
    }
    return cells;
}

/// Cells reachable from `seed` by edge adjacency without leaving `allowed` —
/// used both on the filled cells and on the background.
std::set<Cell> floodFill(const std::set<Cell>& allowed, Cell seed) {
    std::set<Cell> reached{seed};
    std::vector<Cell> stack{seed};
    while (!stack.empty()) {
        const auto [x, y] = stack.back();
        stack.pop_back();
        for (const auto& [dx, dy] : {Cell{1, 0}, Cell{-1, 0}, Cell{0, 1}, Cell{0, -1}}) {
            const Cell next{x + dx, y + dy};
            if (allowed.count(next) && !reached.count(next)) {
                reached.insert(next);
                stack.push_back(next);
            }
        }
    }
    return reached;
}

/// Number of background components a cell set encloses, by flooding the padded
/// complement of its bounding box from outside and counting what is left.
std::size_t enclosedComponents(const std::set<Cell>& cells) {
    int maxX = 0, maxY = 0;
    for (const auto& [x, y] : cells) {
        maxX = std::max(maxX, x);
        maxY = std::max(maxY, y);
    }
    std::set<Cell> background;
    for (int x = -1; x <= maxX + 1; ++x) {
        for (int y = -1; y <= maxY + 1; ++y) {
            if (!cells.count({x, y})) {
                background.insert({x, y});
            }
        }
    }
    std::set<Cell> outside = floodFill(background, {-1, -1});

    std::size_t components = 0;
    std::set<Cell> seen = outside;
    for (const Cell& cell : background) {
        if (seen.count(cell)) {
            continue;
        }
        const std::set<Cell> component = floodFill(background, cell);
        seen.insert(component.begin(), component.end());
        ++components;
    }
    return components;
}

}  // namespace

// Regions keep every free polyomino, so the counts are A000105 itself rather
// than the hole-free A000104 that `polyominoes` returns.
TEST_CASE("polyomino regions count the whole free-polyomino sequence") {
    CHECK(pgl::polyominoRegions(0).size() == 0);
    CHECK(pgl::polyominoRegions(1).size() == 1);
    CHECK(pgl::polyominoRegions(2).size() == 1);
    CHECK(pgl::polyominoRegions(3).size() == 2);
    CHECK(pgl::polyominoRegions(4).size() == 5);
    CHECK(pgl::polyominoRegions(5).size() == 12);
    CHECK(pgl::polyominoRegions(6).size() == 35);
    CHECK(pgl::polyominoRegions(7).size() == 108);  // the holed one is kept
    CHECK(pgl::polyominoRegions(8).size() == 369);

    // The difference against `polyominoes` is exactly the holed polyominoes.
    for (std::size_t n = 1; n <= 8; ++n) {
        std::size_t holed = 0;
        for (const auto& region : pgl::polyominoRegions(n)) {
            holed += region.hasHoles() ? 1 : 0;
        }
        CHECK(pgl::polyominoRegions(n).size() == pgl::polyominoes(n).size() + holed);
    }
    CHECK(pgl::polyominoRegions(6).size() == pgl::polyominoes(6).size());
}

TEST_CASE("each polyomino region is valid and covers its cells") {
    for (std::size_t n = 1; n <= 8; ++n) {
        for (const auto& region : pgl::polyominoRegions(n)) {
            CHECK(region.isValid());
            CHECK(region.area() == static_cast<int>(n));
            for (const auto& vertex : region.vertices()) {
                CHECK(vertex.x() >= 0);
                CHECK(vertex.y() >= 0);
            }

            // Independent oracle: the cells the region covers, recovered by
            // point location alone, must be a polyomino of the right size whose
            // enclosed background components are exactly the region's holes.
            const std::set<Cell> cells = cellsOf(region);
            REQUIRE(cells.size() == n);
            CHECK(floodFill(cells, *cells.begin()).size() == n);
            CHECK(enclosedComponents(cells) == region.holeCount());

            // The rings must cover exactly the filled-to-empty adjacencies, so
            // the perimeter pins the boundary down where the area alone cannot.
            int adjacencies = 0;
            for (const auto& [x, y] : cells) {
                for (const auto& [dx, dy] : {Cell{1, 0}, Cell{-1, 0}, Cell{0, 1}, Cell{0, -1}}) {
                    adjacencies += cells.count({x + dx, y + dy}) ? 0 : 1;
                }
            }
            int perimeter = 0;
            for (const auto& edge : region.edges()) {
                perimeter += std::abs(edge.max().x() - edge.min().x()) +
                             std::abs(edge.max().y() - edge.min().y());
            }
            CHECK(perimeter == adjacencies);
        }
    }
}

// Where a polyomino has no hole the region must be the polygon `polyominoes`
// already returns, which is separately tested.
TEST_CASE("hole-free polyomino regions agree with the polygon enumeration") {
    for (std::size_t n = 1; n <= 7; ++n) {
        std::set<pgl::Polygon<>> outers;
        for (const auto& region : pgl::polyominoRegions(n)) {
            if (!region.hasHoles()) {
                outers.insert(region.outer());
            }
        }
        const auto polygons = pgl::polyominoes(n);
        const std::set<pgl::Polygon<>> expected(polygons.begin(), polygons.end());
        CHECK(outers == expected);
    }
}

// The smallest holed polyomino pinches its hole shut against the outside at a
// single point, which is a region whose rings touch — allowed by isValid.
TEST_CASE("the seven-cell holed polyomino is a pinched region") {
    std::vector<pgl::PolygonWithHoles<>> holed;
    for (const auto& region : pgl::polyominoRegions(7)) {
        if (region.hasHoles()) {
            holed.push_back(region);
        }
    }
    REQUIRE(holed.size() == 1);
    const pgl::PolygonWithHoles<>& region = holed.front();

    const pgl::PolygonWithHoles<> expected(
        pgl::Polygon<>{0, 0, 3, 0, 3, 2, 2, 2, 2, 3, 0, 3},
        std::vector<pgl::Polygon<>>{pgl::Polygon<>{1, 1, 2, 1, 2, 2, 1, 2}});
    CHECK(region == expected);
    CHECK(region.holeCount() == 1);
    CHECK(region.twiceArea() == 14);

    // (2,2) is the pinch: the hole meets the outer ring there, so the point is
    // in the region but has no region interior around it.
    CHECK(region.contains(pgl::Point<>(2, 2)));
    CHECK(!region.interiorContains(pgl::Point<>(2, 2)));

    // The hole interior is outside the region, though the outer ring holds it.
    // Its centre is a half-integer point, so both shapes are doubled to test it.
    pgl::PolygonWithHoles<> doubled = region;
    doubled *= 2;
    CHECK(!doubled.contains(pgl::Point<>(3, 3)));
    CHECK(doubled.outer().contains(pgl::Point<>(3, 3)));
}

TEST_CASE("polyomino regions are distinct and honor the coordinate type") {
    const auto heptominoes = pgl::polyominoRegions(7);
    const std::set<pgl::PolygonWithHoles<>> unique(heptominoes.begin(), heptominoes.end());
    CHECK(unique.size() == heptominoes.size());

    auto wide = pgl::polyominoRegions<std::int64_t>(3);
    static_assert(std::is_same_v<
                  decltype(wide),
                  std::vector<pgl::PolygonWithHoles<pgl::Point<std::int64_t>>>>);
    CHECK(wide.size() == 2);
}

TEST_CASE("polyomino region range and up-to overloads concatenate per size") {
    CHECK(pgl::polyominoRegions(4, 6).size() == 5 + 12 + 35);
    CHECK(pgl::polyominoRegions(5, 7).size() == 12 + 35 + 108);
    CHECK(pgl::polyominoRegions(5, 5).size() == 12);

    // Empty / degenerate ranges.
    CHECK(pgl::polyominoRegions(6, 4).size() == 0);
    CHECK(pgl::polyominoRegions(0, 0).size() == 0);

    CHECK(pgl::polyominoRegionsUpTo(5).size() == 1 + 1 + 2 + 5 + 12);
    CHECK(pgl::polyominoRegionsUpTo(5).size() == pgl::polyominoRegions(1, 5).size());

    // Smallest first: the first entry is the single monomino (one cell).
    const auto upTo = pgl::polyominoRegionsUpTo(4);
    REQUIRE(!upTo.empty());
    CHECK(upTo.front().area() == 1);

    auto wide = pgl::polyominoRegions<std::int64_t>(2, 3);
    static_assert(std::is_same_v<
                  decltype(wide),
                  std::vector<pgl::PolygonWithHoles<pgl::Point<std::int64_t>>>>);
    CHECK(wide.size() == 1 + 2);
}
