#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <algorithm>
#include <functional>
#include <set>
#include <unordered_set>
#include <variant>
#include <vector>

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
        const Polygon farShape({100, 100, 101, 100, 101, 101, 100, 101});
        CHECK(square.intersection(farShape).empty());
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
    using Q = pgl::Rational<int64_t>;
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

// contains / boundaryContains / interiorContains for one polygon against another.
// (Polygon::separates(Polygon) and the crosses that delegates to it now have
// their own coverage in polygon_polygon_separates.cpp.)
TEST_CASE("Polygon contains another Polygon") {
    using Point = pgl::Point<int>;
    using Polygon = pgl::Polygon<Point>;

    const Polygon big({0, 0, 20, 0, 20, 20, 0, 20});

    SUBCASE("a strictly interior polygon is contained, interior too") {
        const Polygon inner({5, 5, 8, 5, 8, 8, 5, 8});
        CHECK(big.contains(inner));
        CHECK(big.interiorContains(inner));
        CHECK_FALSE(big.boundaryContains(inner));
    }

    SUBCASE("a polygon is contained but not interior-contained by itself") {
        CHECK(big.contains(big));
        CHECK_FALSE(big.interiorContains(big));   // its edges lie on the boundary
        CHECK_FALSE(big.boundaryContains(big));
    }

    SUBCASE("an inner polygon touching the boundary is contained, not interior") {
        // Shares the corner (0,0) and parts of two edges with big.
        const Polygon touching({0, 0, 8, 0, 8, 8, 0, 8});
        CHECK(big.contains(touching));
        CHECK_FALSE(big.interiorContains(touching));
    }

    SUBCASE("a polygon poking outside is not contained") {
        const Polygon overlap({10, 10, 30, 10, 30, 30, 10, 30});
        CHECK_FALSE(big.contains(overlap));
        CHECK_FALSE(big.interiorContains(overlap));
        CHECK_FALSE(big.boundaryContains(overlap));
    }

    SUBCASE("a disjoint polygon is not contained") {
        const Polygon away({100, 100, 101, 100, 101, 101, 100, 101});
        CHECK_FALSE(big.contains(away));
        CHECK_FALSE(big.interiorContains(away));
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

// Polygon::untangle() uncrosses the boundary by 2-opt flips and, where a flip is
// blocked by collinearity, by deleting a redundant vertex, until the polygon is
// simple. Surviving vertices keep their positions.
TEST_CASE("Polygon::untangle makes a polygon simple") {
    using Point = pgl::Point<long long>;
    using Polygon = pgl::Polygon<Point>;

    SUBCASE("crossing quad is uncrossed by a flip, keeping all vertices") {
        Polygon bowtie({0, 0, 4, 4, 4, 0, 0, 4});  // self-crossing bowtie
        REQUIRE_FALSE(bowtie.isSimple());
        bowtie.untangle();
        CHECK(bowtie.isSimple());
        CHECK(bowtie.size() == 4);  // no vertex needed to be dropped
    }

    SUBCASE("self-intersecting pentagram becomes a simple pentagon") {
        Polygon star({0, 0, 4, 8, 8, 0, 0, 5, 8, 5});
        REQUIRE_FALSE(star.isSimple());
        star.untangle();
        CHECK(star.isSimple());
        CHECK(star.size() == 5);
    }

    SUBCASE("large crossing polygon is uncrossed") {
        Polygon bad10({0, 0, 6, 6, 0, 6, 6, 0, 3, 8, 1, 1, 5, 1, 2, 7, 4, 7, 3, -2});
        REQUIRE_FALSE(bad10.isSimple());
        bad10.untangle();
        CHECK(bad10.isSimple());
    }

    SUBCASE("collinear touch is resolved by removing a vertex") {
        // Vertex (2,0) sits on the interior of edge (0,0)-(4,0): a flip is
        // impossible, so the collinear vertex is dropped.
        Polygon touch({0, 0, 4, 0, 2, 4, 2, 0});
        REQUIRE_FALSE(touch.isSimple());
        touch.untangle();
        CHECK(touch.isSimple());
        CHECK(touch.size() == 3);
    }

    SUBCASE("an already-simple polygon is left unchanged") {
        const Polygon square({0, 0, 4, 0, 4, 4, 0, 4});
        Polygon copy = square;
        copy.untangle();
        CHECK(copy.isSimple());
        CHECK(copy == square);
    }

    SUBCASE("result stays consistent with the label preserved") {
        using LabeledPolygon = pgl::Polygon<Point, int>;
        LabeledPolygon p({0, 0, 4, 4, 4, 0, 0, 4});
        p.label() = 7;
        p.untangle();
        CHECK(p.isSimple());
        CHECK(p.label() == 7);
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

// squaredDistance from a polygon to every other shape. The polygon owns every
// pair (it is the highest-ranked shape), so the disjoint case reduces to the
// minimum boundary-edge distance; an intersecting shape gives 0. Reverse calls
// exercise each lower shape's generic forwarder up to the polygon.
TEST_CASE("Polygon::squaredDistance to every shape") {
    using Point = pgl::Point<int>;
    using Polygon = pgl::Polygon<Point>;

    // Axis-aligned square [0,4] x [0,4]. Each disjoint probe below has its
    // closest feature on the line x = 10, so the nearest square point is (4, y)
    // and the squared distance is (10 - 4)^2 = 36.
    const Polygon square({Point(0, 0), Point(4, 0), Point(4, 4), Point(0, 4)});
    const double gap = 36.0;

    SUBCASE("point") {
        CHECK(square.squaredDistance<double>(Point(10, 0)) == doctest::Approx(gap));
        CHECK(square.squaredDistance<double>(Point(2, 2)) == doctest::Approx(0.0)); // interior
        CHECK(square.squaredDistance<double>(Point(4, 2)) == doctest::Approx(0.0)); // boundary
        CHECK(square.squaredDistance(Point(10, 0)) == 36);                          // exact int
        CHECK(Point(10, 0).squaredDistance<double>(square) == doctest::Approx(gap));
    }

    SUBCASE("segment and oriented segment") {
        const pgl::Segment<Point> seg(Point(10, -1), Point(10, 1));
        CHECK(square.squaredDistance<double>(seg) == doctest::Approx(gap));
        CHECK(seg.squaredDistance<double>(square) == doctest::Approx(gap));
        const pgl::OrientedSegment<Point> oseg(Point(10, 1), Point(10, -1));
        CHECK(square.squaredDistance<double>(oseg) == doctest::Approx(gap));
        // A segment that pierces the square meets it.
        CHECK(square.squaredDistance<double>(pgl::Segment<Point>(Point(-1, 2), Point(5, 2)))
              == doctest::Approx(0.0));
    }

    SUBCASE("line and oriented line") {
        const pgl::Line<Point> line(Point(10, 0), Point(10, 7));
        CHECK(square.squaredDistance<double>(line) == doctest::Approx(gap));
        CHECK(line.squaredDistance<double>(square) == doctest::Approx(gap));
        const pgl::OrientedLine<Point> oline(Point(10, 7), Point(10, 0));
        CHECK(square.squaredDistance<double>(oline) == doctest::Approx(gap));
        // A line through the square meets it.
        CHECK(square.squaredDistance<double>(pgl::Line<Point>(Point(-1, 2), Point(5, 2)))
              == doctest::Approx(0.0));
    }

    SUBCASE("ray and halfplane") {
        const pgl::Ray<Point> ray(Point(10, 0), Point(10, 7));
        CHECK(square.squaredDistance<double>(ray) == doctest::Approx(gap));
        CHECK(ray.squaredDistance<double>(square) == doctest::Approx(gap));
        // Boundary x = 10, interior x >= 10; the square lies outside it.
        const pgl::Halfplane<Point> hp(Point(10, 7), Point(10, 0));
        CHECK(square.squaredDistance<double>(hp) == doctest::Approx(gap));
        CHECK(hp.squaredDistance<double>(square) == doctest::Approx(gap));
    }

    SUBCASE("rectangle, triangle, convex, polygon") {
        CHECK(square.squaredDistance<double>(pgl::Rectangle<Point>(Point(10, -1), Point(13, 1)))
              == doctest::Approx(gap));
        CHECK(square.squaredDistance<double>(pgl::Triangle<Point>(Point(10, -2), Point(13, 0), Point(10, 2)))
              == doctest::Approx(gap));
        const pgl::Convex<Point> cvx({Point(10, -1), Point(13, -1), Point(13, 1), Point(10, 1)});
        CHECK(square.squaredDistance<double>(cvx) == doctest::Approx(gap));
        CHECK(cvx.squaredDistance<double>(square) == doctest::Approx(gap));
        const Polygon other({Point(10, 0), Point(14, 0), Point(14, 4), Point(10, 4)});
        CHECK(square.squaredDistance<double>(other) == doctest::Approx(gap));
        CHECK(other.squaredDistance<double>(square) == doctest::Approx(gap));
        // Overlapping rectangle / polygon give 0.
        CHECK(square.squaredDistance<double>(pgl::Rectangle<Point>(Point(2, 2), Point(6, 6)))
              == doctest::Approx(0.0));
        CHECK(square.squaredDistance<double>(Polygon({Point(2, 2), Point(6, 2), Point(6, 6), Point(2, 6)}))
              == doctest::Approx(0.0));
    }

    SUBCASE("disk (always double, exterior gap)") {
        // Centre (12,0), radius 1: centre-to-square distance is 8, gap 8 - 1 = 7.
        const pgl::Disk<Point> disk(Point(12, 0), 1);
        CHECK(square.squaredDistance(disk) == doctest::Approx(49.0));
        // A disk that reaches the square gives 0.
        CHECK(square.squaredDistance(pgl::Disk<Point>(Point(5, 0), 2)) == doctest::Approx(0.0));
    }

    SUBCASE("non-convex polygon uses the nearest boundary edge") {
        // L-shaped hexagon with a reflex vertex at (2,2); the square notch
        // x,y in (2,4) is outside the region.
        const Polygon ell({Point(0, 0), Point(4, 0), Point(4, 2),
                           Point(2, 2), Point(2, 4), Point(0, 4)});
        // (3,3) sits in the notch: nearest to both inner edges at distance 1.
        CHECK(ell.squaredDistance<double>(Point(3, 3)) == doctest::Approx(1.0));
        CHECK(ell.squaredDistance<double>(Point(1, 1)) == doctest::Approx(0.0)); // interior
    }

    SUBCASE("rational result type is exact for a perpendicular witness") {
        // Line x + 2y = 20 misses the square; the closest vertex is (4,4), whose
        // exact squared distance to the line is 6400/500 = 64/5.
        const pgl::Line<Point> line(Point(20, 0), Point(0, 10));
        CHECK(square.squaredDistance<pgl::Rational<int>>(line) == pgl::Rational<int>(64, 5));
    }
}

// ===========================================================================
// Polygon::separates(Polygon): A.separates(B) asks whether removing the region A
// disconnects B, i.e. B \ A has two or more connected components. The contract
// mirrors Convex::separates(Polygon) but A may be non-convex, so A can poke into
// B through several pockets while B stays connected -- only a genuine crosscut
// (one A-component meeting ∂B in two arcs) counts.

TEST_CASE("Polygon separates Polygon: a band cutting across") {
    using Polygon = pgl::Polygon<pgl::Point<int>>;
    // A vertical bar A laid across a square B, touching top and bottom edges:
    // B \ A is a left half and a right half.
    const Polygon square({0, 0, 6, 0, 6, 6, 0, 6});
    const Polygon band({2, -1, 4, -1, 4, 7, 2, 7});
    CHECK(band.separates(square));       // splits the square into left and right
    CHECK(square.separates(band));       // and symmetrically cuts the bar's middle out
}

TEST_CASE("Polygon separates Polygon: a single bite does not") {
    using Polygon = pgl::Polygon<pgl::Point<int>>;
    const Polygon square({0, 0, 6, 0, 6, 6, 0, 6});
    const Polygon bite({2, -1, 4, -1, 4, 3, 2, 3});  // pokes in from the bottom only
    CHECK_FALSE(bite.separates(square));
}

TEST_CASE("Polygon separates Polygon: disjoint bites do not") {
    using Polygon = pgl::Polygon<pgl::Point<int>>;
    // A U opening upward, joined by a base bar *below* B. Inside B the two prongs
    // are separate components of A ∩ B, each a single bottom-edge bite, so B stays
    // connected (you can walk around them).
    const Polygon square({0, 0, 10, 0, 10, 6, 0, 6});
    const Polygon u({1, -2, 9, -2, 9, 4, 7, 4, 7, -1, 3, -1, 3, 4, 1, 4});
    REQUIRE(u.isSimple());
    CHECK_FALSE(u.separates(square));
}

TEST_CASE("Polygon separates Polygon: interior island does not (creates a hole)") {
    using Polygon = pgl::Polygon<pgl::Point<int>>;
    const Polygon square({0, 0, 10, 0, 10, 10, 0, 10});
    const Polygon island({4, 4, 6, 4, 6, 6, 4, 6});
    CHECK_FALSE(island.separates(square));
}

TEST_CASE("Polygon separates Polygon: cover leaves nothing") {
    using Polygon = pgl::Polygon<pgl::Point<int>>;
    const Polygon inner({2, 2, 4, 2, 4, 4, 2, 4});
    const Polygon outer({0, 0, 6, 0, 6, 6, 0, 6});
    CHECK_FALSE(outer.separates(inner));  // B \ A empty, not disconnected
}

TEST_CASE("Polygon separates Polygon: non-convex A crosscuts through the interior") {
    using Polygon = pgl::Polygon<pgl::Point<int>>;
    // A is a plus/cross (non-convex); its vertical bar spans B top-to-bottom.
    const Polygon square({0, 0, 12, 0, 12, 6, 0, 6});
    const Polygon plus({5, -1, 7, -1, 7, 2, 9, 2, 9, 4, 7, 4,
                        7, 7, 5, 7, 5, 4, 3, 4, 3, 2, 5, 2});
    REQUIRE(plus.isSimple());
    CHECK(plus.separates(square));
}

TEST_CASE("Polygon separates Polygon: reflex A pinching the boundary at two points") {
    using Polygon = pgl::Polygon<pgl::Point<int>>;
    // A meets ∂B only at two isolated boundary points but its body bridges
    // between them through the interior -> crosscut.
    const Polygon square({0, 0, 6, 0, 6, 6, 0, 6});
    const Polygon pinch({0, 3, 3, 1, 6, 3, 3, 5});  // diamond touching left/right mid-edges
    CHECK(pinch.separates(square));
}

TEST_CASE("Polygon crosses Polygon: two bars mutually cut (delegates to separates)") {
    using Polygon = pgl::Polygon<pgl::Point<int>>;
    // A plus sign: each bar cuts the other into two, so the two shapes cross.
    const Polygon vbar({4, -2, 6, -2, 6, 8, 4, 8});   // tall vertical
    const Polygon hbar({-2, 2, 12, 2, 12, 4, -2, 4});  // wide horizontal
    REQUIRE(vbar.separates(hbar));
    REQUIRE(hbar.separates(vbar));
    CHECK(vbar.crosses(hbar));
    CHECK(hbar.crosses(vbar));

    // A single bite separates neither, so the shapes do not cross.
    const Polygon square({0, 0, 6, 0, 6, 6, 0, 6});
    const Polygon bite({2, -1, 4, -1, 4, 3, 2, 3});
    CHECK_FALSE(bite.crosses(square));
}

TEST_CASE("Polygon separates Polygon: agrees with Convex when A is convex (triangle band)") {
    using Polygon = pgl::Polygon<pgl::Point<int>>;
    const Polygon square({0, 0, 6, 0, 6, 6, 0, 6});
    // The polygon path must match the reference convex path for a convex A.
    const Polygon tri({3, -1, 8, 3, 3, 7});
    const pgl::Triangle<pgl::Point<int>> t({3, -1, 8, 3, 3, 7});
    CHECK(tri.separates(square) == t.separates(square));
}

// The Polygon <-> {Convex, Triangle, Rectangle} overloads all delegate to
// Polygon::separates(Polygon) through asPolygon(), so each must agree with the
// equivalent all-polygon query in both directions.
TEST_CASE("Polygon separates Convex/Triangle/Rectangle and back, via asPolygon") {
    using Point = pgl::Point<int>;
    using Polygon = pgl::Polygon<Point>;
    const Polygon barPoly({4, -2, 6, -2, 6, 8, 4, 8});   // vertical bar as a polygon
    const Polygon squarePoly({0, 0, 10, 0, 10, 6, 0, 6});

    SUBCASE("Rectangle") {
        const pgl::Rectangle<Point> rect(Point(0, 0), Point(10, 6));
        // Bar cuts the box into left/right; the box slices the bar's middle out.
        CHECK(barPoly.separates(rect) == barPoly.separates(squarePoly));
        CHECK(rect.separates(barPoly) == squarePoly.separates(barPoly));
        CHECK(barPoly.separates(rect));
        CHECK(rect.separates(barPoly));
    }

    SUBCASE("Triangle") {
        const pgl::Triangle<Point> tri({0, 0, 10, 0, 5, 8});
        const Polygon triPoly(tri.asPolygon().vertices());
        CHECK(barPoly.separates(tri) == barPoly.separates(triPoly));
        CHECK(tri.separates(barPoly) == triPoly.separates(barPoly));
    }

    SUBCASE("Convex") {
        const pgl::Convex<Point> conv({0, 0, 10, 0, 10, 6, 0, 6});
        const Polygon convPoly(conv.asPolygon().vertices());
        CHECK(barPoly.separates(conv) == barPoly.separates(convPoly));
        CHECK(conv.separates(barPoly) == convPoly.separates(barPoly));
    }
}

// ===========================================================================
// Polygon::intersection<Q>(Polygon) reassembles the clipped boundary into faces
// of a planar graph, resolving pinch (articulation) vertices into separate
// cycles. The contract exercised here: when both inputs are simple, every
// *polygon* piece returned is itself simple -- a self-touching intersection
// comes back as several simple polygons meeting at a vertex, never as one
// non-simple polygon. Results use exact rationals so no rounding hides a defect.
//
// The polygon type is aliased as `Poly` (not `Polygon`) at namespace scope: this
// content lives partly in a namespace-scope helper, and doctest pulls in
// <windows.h> on MSVC, whose GDI `Polygon` function collides with a
// namespace-scope `using Polygon`.
namespace intersection_simple {

using P = pgl::Point<int>;
using Poly = pgl::Polygon<P>;
using Q = pgl::ERational;
using QPoint = pgl::Point<Q>;
using QPoly = pgl::Polygon<QPoint>;

// Every simple polygon (deduplicated) with `lo..hi` vertices drawn from a
// `g`-by-`g` integer grid. Enumerates all vertex subsets and all cyclic
// orderings, keeping the ones whose boundary does not self-intersect.
inline std::vector<Poly> simplePolygonsOnGrid(int g, int lo, int hi) {
    std::vector<P> grid;
    for (int y = 0; y < g; ++y)
        for (int x = 0; x < g; ++x)
            grid.push_back(P(x, y));
    const int n = static_cast<int>(grid.size());

    std::set<Poly> polys;
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
                Poly poly(pts);
                if (poly.isSimple()) polys.insert(std::move(poly));
            } while (std::next_permutation(sub.begin(), sub.end()));
        } while (std::next_permutation(pick.begin(), pick.end()));
    }
    return {polys.begin(), polys.end()};
}

}  // namespace intersection_simple

// Exhaustive sweep: every unordered pair of simple triangles and quadrilaterals
// on a 3x3 grid (reflex quads included, so pinch/self-touch intersections do
// occur). Each returned polygon must be simple and positively oriented (faces
// are traced counterclockwise). A single aggregate CHECK reports the first
// offending pair instead of drowning the log in thousands of assertions.
TEST_CASE("Polygon intersection<Q>(Polygon): every polygon piece is simple (exhaustive 3x3, sizes 3-4)") {
    using namespace intersection_simple;
    const std::vector<Poly> polys = simplePolygonsOnGrid(3, 3, 4);
    REQUIRE(polys.size() > 100);  // sanity: enumeration produced a rich set

    long pairs = 0, polyPieces = 0;
    bool allSimple = true, allPositive = true;
    Poly badA, badB;
    QPoly badPiece;

    for (std::size_t i = 0; i < polys.size(); ++i) {
        for (std::size_t j = i; j < polys.size(); ++j) {
            ++pairs;
            for (const auto& piece : polys[i].intersection<Q>(polys[j])) {
                if (!std::holds_alternative<QPoly>(piece)) continue;
                ++polyPieces;
                const QPoly& g = std::get<QPoly>(piece);
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
    using namespace intersection_simple;
    auto polygonPieces = [](const auto& pieces) {
        std::vector<QPoly> out;
        for (const auto& v : pieces)
            if (std::holds_alternative<QPoly>(v)) out.push_back(std::get<QPoly>(v));
        return out;
    };

    SUBCASE("bowtie pinch: two triangles meeting at (2,2)") {
        const Poly a({0, 0, 4, 0, 2, 2, 4, 4, 0, 4});
        const Poly b({0, 0, 2, 2, 0, 4, 4, 4, 4, 0});
        const auto ps = polygonPieces(a.intersection<Q>(b));
        REQUIRE(ps.size() == 2);
        for (const auto& p : ps) CHECK(p.isSimple());
    }

    SUBCASE("plus-signs overlapping into a self-touching cross") {
        // Two axis-aligned plus/cross shapes; their overlap pinches at the arms.
        const Poly a({1, 0, 2, 0, 2, 1, 3, 1, 3, 2, 2, 2, 2, 3, 1, 3, 1, 2, 0, 2, 0, 1, 1, 1});
        const Poly b = a;  // identical: intersection is the whole (non-convex) shape
        const auto ps = polygonPieces(a.intersection<Q>(b));
        REQUIRE(!ps.empty());
        for (const auto& p : ps) CHECK(p.isSimple());
    }
}
