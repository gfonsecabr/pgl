#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstdint>
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

TEST_CASE("the single domino is a 1x2 rectangle outline") {
    const auto dominoes = pgl::polyominoes(2);
    REQUIRE(dominoes.size() == 1);
    // Four corners, area two.
    CHECK(dominoes.front().size() == 4);
    CHECK(dominoes.front().area() == 2);
}
