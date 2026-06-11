#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

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
