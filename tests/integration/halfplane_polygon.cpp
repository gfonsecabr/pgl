#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

#include <variant>

// Halfplane::separates(Polygon) asks whether removing the (closed) half-plane
// from the polygon leaves two or more pieces. Because the complement of a closed
// half-plane is convex, a half-plane can only ever disconnect a *non-convex*
// polygon: it must swallow a connecting neck and leave the surrounding arms
// outside. The implementation counts the boundary arcs that lie outside the
// half-plane; two or more such arcs means the area is cut.
//
// Convention reminder: Halfplane(source, target) keeps the closed region to the
// LEFT of source -> target, boundary included. So:
//   Halfplane({0,c},{1,c})  -> interior y >= c
//   Halfplane({1,c},{0,c})  -> interior y <= c

TEST_CASE("Half-plane never separates a convex polygon") {
    using Point = pgl::Point<int>;
    using Polygon = pgl::Polygon<Point>;
    using Convex = pgl::Convex<Point>;
    using Halfplane = pgl::Halfplane<Point>;

    const Polygon square({0, 0, 10, 0, 10, 10, 0, 10});
    const Convex square_convex({{0, 0}, {10, 0}, {10, 10}, {0, 10}});

    SUBCASE("a slice through the middle leaves one connected piece") {
        // Interior y <= 5: removing it leaves the open top half, still connected.
        const Halfplane slice({1, 5}, {0, 5});
        CHECK_FALSE(slice.separates(square));
        // Consistent with the convex overload, which also reports no cut.
        CHECK(slice.separates(square) == slice.separates(square_convex));
    }

    SUBCASE("covering the whole polygon removes everything, no pieces left") {
        const Halfplane covers({1, 20}, {0, 20});  // interior y <= 20
        CHECK_FALSE(covers.separates(square));
    }

    SUBCASE("missing the polygon entirely leaves it whole") {
        const Halfplane above({0, 20}, {1, 20});  // interior y >= 20
        CHECK_FALSE(above.separates(square));
    }

    SUBCASE("grazing the top edge only shaves a boundary segment") {
        const Halfplane graze({0, 10}, {1, 10});  // interior y >= 10
        CHECK_FALSE(graze.separates(square));
    }
}

TEST_CASE("Half-plane separates a U-shaped polygon at the connecting neck") {
    using Point = pgl::Point<int>;
    using Polygon = pgl::Polygon<Point>;
    using Halfplane = pgl::Halfplane<Point>;

    // A "U" opening upward: a solid base for y in [0,4] spanning the full width,
    // and two arms x in [0,4] and x in [6,10] rising to y = 10. The notch
    // (x in [4,6], y in [4,10]) is void.
    const Polygon u({0, 0, 10, 0, 10, 10, 6, 10, 6, 4, 4, 4, 4, 10, 0, 10});

    SUBCASE("removing the entire base disconnects the two arms") {
        // Interior y <= 4 swallows the whole connecting base; only the two arms
        // (y > 4) survive, leaving two pieces.
        const Halfplane cut_base({1, 4}, {0, 4});
        CHECK(cut_base.separates(u));
    }

    SUBCASE("removing only part of the base leaves the arms joined") {
        // Interior y <= 3 still leaves the full-width strip y in (3,4] joining
        // the arms, so it stays one piece.
        const Halfplane shallow({1, 3}, {0, 3});
        CHECK_FALSE(shallow.separates(u));
    }

    SUBCASE("the opposite half-plane removes the arms, leaving the solid base") {
        // Same boundary line y = 4, but interior y >= 4 removes the arms and
        // keeps the connected base (y < 4) -> one piece. This is the asymmetry
        // partner of the cutting case above.
        const Halfplane cut_arms({0, 4}, {1, 4});
        CHECK_FALSE(cut_arms.separates(u));
    }

    SUBCASE("a half-plane covering everything leaves nothing to separate") {
        const Halfplane covers({1, 100}, {0, 100});  // interior y <= 100
        CHECK_FALSE(covers.separates(u));
    }

    SUBCASE("a half-plane the polygon does not reach leaves it whole") {
        const Halfplane far({0, 100}, {1, 100});  // interior y >= 100
        CHECK_FALSE(far.separates(u));
    }
}

TEST_CASE("Half-plane separates a comb with three teeth at two or more arcs") {
    using Point = pgl::Point<int>;
    using Polygon = pgl::Polygon<Point>;
    using Halfplane = pgl::Halfplane<Point>;

    // An "E"/comb lying on its back: a solid base for y in [0,2] and three teeth
    // (x in [0,2], [4,6], [8,10]) rising to y = 8.
    const Polygon comb({0, 0, 10, 0, 10, 8, 8, 8, 8, 2,
                        6, 2, 6, 8, 4, 8, 4, 2, 2, 2, 2, 8, 0, 8});

    SUBCASE("cutting the base off detaches all three teeth") {
        // Interior y <= 2 removes the whole base; three disjoint teeth remain.
        const Halfplane cut({1, 2}, {0, 2});
        CHECK(cut.separates(comb));
    }

    SUBCASE("a shallow cut keeps the teeth joined by the remaining base") {
        const Halfplane shallow({1, 1}, {0, 1});  // interior y <= 1
        CHECK_FALSE(shallow.separates(comb));
    }
}

TEST_CASE("Half-plane separates Polygon is invariant under translation") {
    using Point = pgl::Point<int>;
    using Polygon = pgl::Polygon<Point>;
    using Halfplane = pgl::Halfplane<Point>;

    // The arc count depends only on relative positions, so a rigid translation
    // of both polygon and half-plane must preserve every answer.
    for (const Point shift : {Point(0, 0), Point(31, -17), Point(-40, 25)}) {
        const auto p = [&shift](std::initializer_list<int> coords) {
            std::vector<Point> pts;
            const std::vector<int> v(coords);
            for (std::size_t i = 0; i + 1 < v.size(); i += 2) {
                pts.emplace_back(v[i] + shift.x(), v[i + 1] + shift.y());
            }
            return Polygon(pts);
        };
        const auto hp = [&shift](Point a, Point b) {
            return Halfplane(a + shift, b + shift);
        };

        const Polygon u = p({0, 0, 10, 0, 10, 10, 6, 10, 6, 4, 4, 4, 4, 10, 0, 10});

        CHECK(hp({1, 4}, {0, 4}).separates(u));        // cut the base -> separates
        CHECK_FALSE(hp({0, 4}, {1, 4}).separates(u));  // cut the arms -> one piece
        CHECK_FALSE(hp({1, 3}, {0, 3}).separates(u));  // shallow -> still joined
    }
}

// KNOWN BUG (expected-fail): the arc-counting implementation reports "separated"
// whenever the polygon boundary has >= 2 arcs outside the half-plane.  That
// over-counts when the closed half-plane removes a region meeting the polygon in
// two disjoint pieces while P\H stays connected: the correct answer is FALSE but
// the implementation returns TRUE.  These cases assert the geometrically correct
// answer, so they fail today; doctest::should_fail keeps the suite green and
// will flag them (as unexpected passes) once the predicate is fixed.
TEST_CASE("Half-plane separates Polygon: disjoint bites must not separate"
          * doctest::should_fail()) {
    using Point = pgl::Point<int>;
    using Polygon = pgl::Polygon<Point>;
    using Halfplane = pgl::Halfplane<Point>;

    const Polygon u({0, 0, 10, 0, 10, 10, 6, 10, 6, 4, 4, 4, 4, 10, 0, 10});
    // Horseshoe ("C") opening to the right.
    const Polygon horseshoe({0, 0, 10, 0, 10, 3, 3, 3, 3, 7, 10, 7, 10, 10, 0, 10});

    SUBCASE("removing both U arm tops leaves base + lower arms connected") {
        // Interior y >= 9 trims the tops of both arms (two disjoint bites); the
        // base still joins them, so P\H is one piece.
        const Halfplane tops({0, 9}, {1, 9});
        CHECK_FALSE(tops.separates(u));
    }

    SUBCASE("removing the horseshoe's right side leaves the spine connected") {
        // Interior x >= 9 bites the right ends of both arms; the spine still
        // joins them through x < 9, so P\H is one piece.
        const Halfplane right({9, 1}, {9, 0});
        CHECK_FALSE(right.separates(horseshoe));
    }
}

// Polygon::intersection(Halfplane) clips the polygon to the closed half-plane,
// returning the surviving regions (polygons), plus any collinear boundary
// overlaps (segments) and isolated touches (points).
TEST_CASE("Polygon intersects Halfplane into polygons, segments, and points") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Halfplane = pgl::Halfplane<Point>;
    using Polygon = pgl::Polygon<Point>;
    using Piece = std::variant<Point, Segment, Polygon>;

    const Polygon square({0, 0, 10, 0, 10, 10, 0, 10});

    SUBCASE("a half-plane through the middle keeps one half polygon") {
        // Boundary y = 5 oriented +x; interior is the upper half (y >= 5).
        const auto pieces = square.intersection(Halfplane({0, 5}, {10, 5}));

        REQUIRE(pieces.size() == 1);
        CHECK(pieces[0] == Piece(Polygon({0, 5, 10, 5, 10, 10, 0, 10})));
    }

    SUBCASE("a half-plane containing the polygon returns it whole") {
        const auto pieces = square.intersection(Halfplane({-100, -5}, {100, -5}));

        REQUIRE(pieces.size() == 1);
        CHECK(pieces[0] == Piece(square));
    }

    SUBCASE("a half-plane excluding the polygon returns nothing") {
        // Boundary y = 15 oriented +x; interior y >= 15 misses the square.
        CHECK(square.intersection(Halfplane({-100, 15}, {100, 15})).empty());
    }

    SUBCASE("a half-plane tangent along the top edge yields a segment") {
        // Interior y >= 10 meets the square only along its top edge.
        const auto pieces = square.intersection(Halfplane({0, 10}, {10, 10}));

        REQUIRE(pieces.size() == 1);
        CHECK(pieces[0] == Piece(Segment({0, 10}, {10, 10})));
    }

    SUBCASE("a half-plane tangent at a single corner yields a point") {
        // Boundary x + y = 20 through (10,10); interior x + y >= 20.
        const auto pieces = square.intersection(Halfplane({0, 20}, {20, 0}));

        REQUIRE(pieces.size() == 1);
        CHECK(pieces[0] == Piece(Point(10, 10)));
    }

    SUBCASE("a half-plane can split a non-convex polygon into two polygons") {
        // U-shape: prongs x in [0,3] and [7,10], joined by the base y in [0,3].
        const Polygon u({0, 0, 10, 0, 10, 10, 7, 10, 7, 3, 3, 3, 3, 10, 0, 10});
        // Keep y >= 5: removes the base, leaving the two prongs disconnected.
        const auto pieces = u.intersection(Halfplane({0, 5}, {10, 5}));

        int polys = 0;
        int totalTwiceArea = 0;
        for (const auto& v : pieces)
            if (std::holds_alternative<Polygon>(v)) {
                ++polys;
                totalTwiceArea += std::get<Polygon>(v).twiceArea();
            }
        CHECK(polys == 2);
        CHECK(totalTwiceArea == 60);  // two 3 x 5 rectangles
    }
}
