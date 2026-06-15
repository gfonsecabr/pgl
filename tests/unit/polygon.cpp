#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <functional>
#include <unordered_set>
#include <variant>

// Polygon::intersection(Polygon) clips the boundary of each polygon against the
// other and reassembles the pieces into isolated points, open polylines, and
// closed polygons. Pieces are returned in no particular order, so the tests
// compare by content / counts rather than position.
TEST_CASE("Polygon intersects Polygon into points, polylines, and polygons") {
    using Point = pgl::Point<int>;
    using Polyline = pgl::Polyline<Point>;
    using Polygon = pgl::Polygon<Point>;
    using Piece = std::variant<Point, Polyline, Polygon>;

    auto countPolys = [](const auto& res) {
        int n = 0;
        for (const auto& v : res) n += std::holds_alternative<Polygon>(v) ? 1 : 0;
        return n;
    };
    auto totalTwiceArea = [](const auto& res) {
        int a = 0;
        for (const auto& v : res)
            if (std::holds_alternative<Polygon>(v)) a += std::get<Polygon>(v).twiceArea();
        return a;
    };

    const Polygon square({0, 0, 10, 0, 10, 10, 0, 10});

    SUBCASE("overlapping squares meet in a square") {
        const Polygon other({5, 5, 15, 5, 15, 15, 5, 15});
        const auto pieces = square.intersection(other);

        REQUIRE(pieces.size() == 1);
        CHECK(pieces[0] == Piece(Polygon({5, 5, 10, 5, 10, 10, 5, 10})));
    }

    SUBCASE("a contained polygon is returned unchanged") {
        const Polygon big({0, 0, 20, 0, 20, 20, 0, 20});
        const Polygon small({5, 5, 8, 5, 8, 8, 5, 8});
        const auto pieces = big.intersection(small);

        REQUIRE(pieces.size() == 1);
        CHECK(pieces[0] == Piece(small));
    }

    SUBCASE("identical polygons return the polygon itself") {
        const auto pieces = square.intersection(square);

        REQUIRE(pieces.size() == 1);
        CHECK(pieces[0] == Piece(square));
    }

    SUBCASE("disjoint polygons have empty intersection") {
        const Polygon far({100, 100, 101, 100, 101, 101, 100, 101});
        CHECK(square.intersection(far).empty());
    }

    SUBCASE("polygons sharing a single corner meet at a point") {
        const Polygon corner({10, 10, 20, 10, 20, 20, 10, 20});
        const auto pieces = square.intersection(corner);

        REQUIRE(pieces.size() == 1);
        CHECK(pieces[0] == Piece(Point(10, 10)));
    }

    SUBCASE("edge-adjacent polygons meet along a polyline") {
        // Shares the whole edge x = 10, y in [0, 10]: a 1D (zero-area) meeting.
        const Polygon right({10, 0, 20, 0, 20, 10, 10, 10});
        const auto pieces = square.intersection(right);

        REQUIRE(pieces.size() == 1);
        REQUIRE(std::holds_alternative<Polyline>(pieces[0]));
        const auto& line = std::get<Polyline>(pieces[0]);
        REQUIRE(line.size() == 2);
        CHECK(((line[0] == Point(10, 0) && line[1] == Point(10, 10)) ||
               (line[0] == Point(10, 10) && line[1] == Point(10, 0))));
    }

    SUBCASE("a non-convex polygon can split the intersection into two polygons") {
        // U-shape: two prongs x in [0,3] and [7,10] joined at the bottom.
        const Polygon u({0, 0, 10, 0, 10, 10, 7, 10, 7, 3, 3, 3, 3, 10, 0, 10});
        // A band high enough to miss the joining base, hitting each prong.
        const Polygon band({-1, 5, 11, 5, 11, 9, -1, 9});
        const auto pieces = u.intersection(band);

        CHECK(countPolys(pieces) == 2);
        // Each prong contributes a 3 x 4 rectangle: twiceArea 24 each.
        CHECK(totalTwiceArea(pieces) == 48);
    }
}

TEST_CASE("Polygon intersects Polygon with fractional crossings (rational result)") {
    using Q = pgl::Rational<long long>;
    using P = pgl::Point<int>;
    using QPoint = pgl::Point<Q>;
    using QPolygon = pgl::Polygon<QPoint>;

    // Two triangles whose overlap has non-integer vertices.
    const pgl::Polygon<P> a({0, 0, 6, 0, 0, 6});
    const pgl::Polygon<P> b({4, 1, 7, 1, 7, 7});
    const auto pieces = a.intersection<Q>(b);

    int polys = 0;
    for (const auto& v : pieces) polys += std::holds_alternative<QPolygon>(v) ? 1 : 0;
    CHECK(polys == 1);
    for (const auto& v : pieces)
        if (std::holds_alternative<QPolygon>(v)) {
            CHECK(std::get<QPolygon>(v).twiceArea() > Q(0));
        }
}

// Regression: two polygons sharing only a boundary segment must NOT report
// interiorsIntersect, and the predicate must be symmetric. The 1x5 rectangle's
// left edge (x=50) overlaps part of the L-shape's right edge (x=50, y in
// [13,14]); their interiors lie on opposite sides of that line.
TEST_CASE("Polygon interiorsIntersect: shared boundary segment is not an interior overlap") {
    using Polygon = pgl::Polygon<pgl::Point<int>>;
    const Polygon rect({50, 12, 51, 12, 51, 17, 50, 17});
    const Polygon ell({48, 13, 50, 13, 50, 14, 49, 14, 49, 15, 48, 15});

    CHECK_FALSE(rect.interiorsIntersect(ell));
    CHECK_FALSE(ell.interiorsIntersect(rect));  // was erroneously true
    CHECK(rect.interiorsIntersect(ell) == ell.interiorsIntersect(rect));

    // They do touch along the boundary, so the (closed) intersection predicate holds.
    CHECK(rect.intersects(ell));

    // A genuine interior overlap is still detected, both directions.
    const Polygon shifted({49, 12, 51, 12, 51, 17, 49, 17});  // covers x in [49,51]
    CHECK(shifted.interiorsIntersect(ell));
    CHECK(ell.interiorsIntersect(shifted));
}

// The boundary reassembly must handle graph nodes of degree > 2, which arise
// where the intersection pinches (two pieces meeting at a point) or where the
// two polygons share collinear boundary. These used to trip an assertion.
TEST_CASE("Polygon intersects Polygon through degree>2 boundary nodes") {
    using Point = pgl::Point<int>;
    using Polygon = pgl::Polygon<Point>;

    auto polygons = [](const auto& pieces) {
        std::vector<Polygon> out;
        for (const auto& v : pieces)
            if (std::holds_alternative<Polygon>(v)) out.push_back(std::get<Polygon>(v));
        return out;
    };
    auto twiceArea = [](const std::vector<Polygon>& ps) {
        int a = 0;
        for (const auto& p : ps) a += p.twiceArea();
        return a;
    };

    SUBCASE("bowtie pinch splits into two triangles (degree-4 node)") {
        // Square notched from the right and the left; the notch apexes meet at
        // (2,2), so the intersection is two triangles touching at that point.
        const Polygon a({0, 0, 4, 0, 2, 2, 4, 4, 0, 4});
        const Polygon b({0, 0, 2, 2, 0, 4, 4, 4, 4, 0});
        const auto ps = polygons(a.intersection(b));
        CHECK(ps.size() == 2);
        for (const auto& p : ps) CHECK(p.size() == 3);     // two triangles
        CHECK(twiceArea(ps) == 16);                        // area 4 each
    }

    SUBCASE("shared collinear boundary yields one polygon (degree-3 node)") {
        // 1x5 bar vs an L whose right edge overlaps the bar's right edge; the
        // true intersection is the simple rectangle [0,1] x [0,4].
        const Polygon bar({0, 0, 1, 0, 1, 5, 0, 5});
        const Polygon ell({0, 0, 2, 0, 2, 1, 1, 1, 1, 4, 0, 4});
        const auto ps = polygons(bar.intersection(ell));
        CHECK(ps.size() == 1);
        CHECK(twiceArea(ps) == 8);                         // area 4
    }
}

// Polygon::isSimple(): small/floating-point polygons take the O(n^2) pairwise
// path; larger exact ones take the Bentley-Ottmann sweep. Both must agree, and
// degenerate inputs are not simple.
TEST_CASE("Polygon::isSimple recognizes simple and non-simple polygons") {
    using Point = pgl::Point<int>;
    using Polygon = pgl::Polygon<Point>;

    SUBCASE("small simple polygons (brute-force path)") {
        CHECK(Polygon({0, 0, 4, 0, 4, 4, 0, 4}).isSimple());          // square
        CHECK(Polygon({0, 0, 4, 0, 2, 3}).isSimple());                // triangle
        CHECK(Polygon({0, 0, 4, 0, 2, 2, 4, 4, 0, 4}).isSimple());    // non-convex
        CHECK(Polygon({0, 0, 2, 0, 4, 0, 4, 4, 0, 4}).isSimple());    // flat collinear vertex
    }

    SUBCASE("small non-simple polygons") {
        CHECK_FALSE(Polygon({0, 0, 4, 4, 4, 0, 0, 4}).isSimple());    // self-crossing quad
        CHECK_FALSE(Polygon({0, 0, 2, 0, 4, 0}).isSimple());          // collinear (degenerate)
    }

    SUBCASE("large polygons take the Bentley-Ottmann path and still agree") {
        // A simple 10-vertex staircase outline (> 8 vertices => sweep).
        const Polygon simple10({0, 0, 1, 0, 1, 1, 3, 1, 3, 2, 2, 2, 2, 3, 1, 3, 1, 2, 0, 2});
        REQUIRE(simple10.size() == 10);
        CHECK(simple10.isSimple());

        // A 10-vertex polygon whose edges cross.
        const Polygon bad10({0, 0, 6, 6, 0, 6, 6, 0, 3, 8, 1, 1, 5, 1, 2, 7, 4, 7, 3, -2});
        REQUIRE(bad10.size() == 10);
        CHECK_FALSE(bad10.isSimple());
    }
}

TEST_CASE("Polygon hashing is consistent and cached across mutations") {
    using Point = pgl::Point<int>;
    using Polygon = pgl::Polygon<Point>;
    const std::hash<Polygon> hasher{};

    const Polygon square({0, 0, 4, 0, 4, 3, 0, 3});

    SUBCASE("equal polygons hash equally regardless of starting vertex") {
        // Same square, vertices listed starting from a different corner.
        const Polygon rotated({4, 3, 0, 3, 0, 0, 4, 0});
        REQUIRE(square == rotated);
        CHECK(hasher(square) == hasher(rotated));
    }

    SUBCASE("repeated hashing returns the same cached value") {
        const auto first = hasher(square);
        CHECK(hasher(square) == first);
        CHECK(hasher(square) == first);
    }

    SUBCASE("translation invalidates the cache and recomputes correctly") {
        Polygon p = square;
        const auto original = hasher(p);  // primes the cache

        p += Point(10, 7);
        const Polygon shifted({10, 7, 14, 7, 14, 10, 10, 10});
        REQUIRE(p == shifted);
        CHECK(hasher(p) == hasher(shifted));

        p -= Point(10, 7);  // back to the original position
        REQUIRE(p == square);
        CHECK(hasher(p) == original);
    }

    SUBCASE("scaling invalidates the cache") {
        Polygon p = square;
        const auto original = hasher(p);  // primes the cache
        p *= 2;
        const Polygon scaled({0, 0, 8, 0, 8, 6, 0, 6});
        REQUIRE(p == scaled);
        CHECK(hasher(p) == hasher(scaled));
        p /= 2;
        REQUIRE(p == square);
        CHECK(hasher(p) == original);
    }

    SUBCASE("usable as a key in an unordered_set") {
        std::unordered_set<Polygon> seen;
        seen.insert(square);
        seen.insert(Polygon({4, 3, 0, 3, 0, 0, 4, 0}));  // equal
        CHECK(seen.size() == 1);
        seen.insert(Polygon({0, 0, 1, 0, 0, 1}));  // different
        CHECK(seen.size() == 2);
    }
}
