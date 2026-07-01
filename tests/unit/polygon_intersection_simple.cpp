#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <algorithm>
#include <set>
#include <variant>
#include <vector>

// Polygon::intersection<Q>(Polygon) reassembles the clipped boundary into faces
// of a planar graph, resolving pinch (articulation) vertices into separate
// cycles. The contract exercised here: when both inputs are simple, every
// *polygon* piece returned is itself simple -- a self-touching intersection
// comes back as several simple polygons meeting at a vertex, never as one
// non-simple polygon. Results use exact rationals so no rounding hides a defect.

using P = pgl::Point<int>;
using Polygon = pgl::Polygon<P>;
using Q = pgl::ERational;
using QPoint = pgl::Point<Q>;
using QPolygon = pgl::Polygon<QPoint>;

namespace {

// Every simple polygon (deduplicated) with `lo..hi` vertices drawn from a
// `g`-by-`g` integer grid. Enumerates all vertex subsets and all cyclic
// orderings, keeping the ones whose boundary does not self-intersect.
std::vector<Polygon> simplePolygonsOnGrid(int g, int lo, int hi) {
    std::vector<P> grid;
    for (int y = 0; y < g; ++y)
        for (int x = 0; x < g; ++x)
            grid.push_back(P(x, y));
    const int n = static_cast<int>(grid.size());

    std::set<Polygon> polys;
    for (int k = lo; k <= hi; ++k) {
        std::vector<bool> pick(n, false);
        std::fill(pick.begin(), pick.begin() + k, true);
        std::sort(pick.begin(), pick.end());  // iterate subsets as sorted masks
        do {
            std::vector<int> sub;
            for (int i = 0; i < n; ++i)
                if (pick[i]) sub.push_back(i);
            do {
                std::vector<P> pts;
                for (int i : sub) pts.push_back(grid[i]);
                Polygon poly(pts);
                if (poly.isSimple()) polys.insert(std::move(poly));
            } while (std::next_permutation(sub.begin(), sub.end()));
        } while (std::next_permutation(pick.begin(), pick.end()));
    }
    return {polys.begin(), polys.end()};
}

}  // namespace

// Exhaustive sweep: every unordered pair of simple triangles and quadrilaterals
// on a 3x3 grid (reflex quads included, so pinch/self-touch intersections do
// occur). Each returned polygon must be simple and positively oriented (faces
// are traced counterclockwise). A single aggregate CHECK reports the first
// offending pair instead of drowning the log in thousands of assertions.
TEST_CASE("Polygon intersection<Q>(Polygon): every polygon piece is simple (exhaustive 3x3, sizes 3-4)") {
    const std::vector<Polygon> polys = simplePolygonsOnGrid(3, 3, 4);
    REQUIRE(polys.size() > 100);  // sanity: enumeration produced a rich set

    long pairs = 0, polyPieces = 0;
    bool allSimple = true, allPositive = true;
    Polygon badA, badB;
    QPolygon badPiece;

    for (std::size_t i = 0; i < polys.size(); ++i) {
        for (std::size_t j = i; j < polys.size(); ++j) {
            ++pairs;
            for (const auto& piece : polys[i].intersection<Q>(polys[j])) {
                if (!std::holds_alternative<QPolygon>(piece)) continue;
                ++polyPieces;
                const QPolygon& g = std::get<QPolygon>(piece);
                if (!g.isSimple()) {
                    if (allSimple) { badA = polys[i]; badB = polys[j]; badPiece = g; }
                    allSimple = false;
                }
                if (g.twiceArea() <= Q(0)) allPositive = false;
            }
        }
    }

    CAPTURE(pairs);
    CAPTURE(polyPieces);
    INFO("first non-simple, if any: A=" << badA << " B=" << badB << " piece=" << badPiece);
    CHECK(allSimple);
    CHECK(allPositive);
    CHECK(polyPieces > 0);  // the sweep actually produced polygon-typed pieces
}

// Curated pinch/self-touch configurations, spelled out so a regression names the
// geometry directly rather than hiding inside the exhaustive sweep.
TEST_CASE("Polygon intersection<Q>(Polygon): named pinch cases stay simple") {
    auto polygonPieces = [](const auto& pieces) {
        std::vector<QPolygon> out;
        for (const auto& v : pieces)
            if (std::holds_alternative<QPolygon>(v)) out.push_back(std::get<QPolygon>(v));
        return out;
    };

    SUBCASE("bowtie pinch: two triangles meeting at (2,2)") {
        const Polygon a({0, 0, 4, 0, 2, 2, 4, 4, 0, 4});
        const Polygon b({0, 0, 2, 2, 0, 4, 4, 4, 4, 0});
        const auto ps = polygonPieces(a.intersection<Q>(b));
        REQUIRE(ps.size() == 2);
        for (const auto& p : ps) CHECK(p.isSimple());
    }

    SUBCASE("plus-signs overlapping into a self-touching cross") {
        // Two axis-aligned plus/cross shapes; their overlap pinches at the arms.
        const Polygon a({1, 0, 2, 0, 2, 1, 3, 1, 3, 2, 2, 2, 2, 3, 1, 3, 1, 2, 0, 2, 0, 1, 1, 1});
        const Polygon b = a;  // identical: intersection is the whole (non-convex) shape
        const auto ps = polygonPieces(a.intersection<Q>(b));
        REQUIRE(!ps.empty());
        for (const auto& p : ps) CHECK(p.isSimple());
    }
}
