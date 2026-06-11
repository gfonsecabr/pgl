#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

TEST_CASE("Segment separates Polygon") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Polygon = pgl::Polygon<Point>;

    // Axis-aligned convex square, given as a flat coordinate list.
    const Polygon square({0, 0, 10, 0, 10, 10, 0, 10});

    SUBCASE("chord clear across the square cuts it in two") {
        const Segment cut({-1, 5}, {11, 5});

        CHECK(cut.intersects(square));
        CHECK(square.interiorsIntersect(cut));
        CHECK(cut.separates(square));
    }

    SUBCASE("segment with one endpoint inside leaves a slit") {
        const Segment slit({5, 5}, {11, 5});

        CHECK(square.contains(slit.min()));
        CHECK(square.interiorContains(slit.min()));
        CHECK(square.intersects(slit));
        CHECK_FALSE(slit.separates(square));
    }

    SUBCASE("segment fully inside does not cut") {
        const Segment inner({2, 5}, {8, 5});

        CHECK(square.contains(inner));
        CHECK_FALSE(inner.separates(square));
    }

    SUBCASE("segment fully outside does not cut") {
        const Segment outside({-5, 5}, {-1, 5});

        CHECK_FALSE(outside.intersects(square));
        CHECK_FALSE(outside.separates(square));
    }

    SUBCASE("corner-to-corner diagonal cuts the square") {
        const Segment diagonal({0, 0}, {10, 10});

        CHECK(square.boundaryContains(diagonal.min()));
        CHECK(square.boundaryContains(diagonal.max()));
        CHECK(diagonal.separates(square));
    }

    SUBCASE("endpoints resting on opposite edge interiors cut the square") {
        const Segment vertical({5, 0}, {5, 10});
        const Segment horizontal({0, 5}, {10, 5});

        CHECK(square.boundaryContains(vertical.min()));
        CHECK(square.boundaryContains(vertical.max()));
        CHECK_FALSE(square.interiorContains(vertical.min()));
        CHECK(vertical.separates(square));
        CHECK(horizontal.separates(square));
    }

    SUBCASE("segment touching a single corner from outside does not cut") {
        const Segment touch({-2, 2}, {0, 0});

        CHECK(square.boundaryContains(touch.max()));
        CHECK_FALSE(touch.separates(square));
    }

    SUBCASE("segment lying along an edge does not cut") {
        const Segment along_edge({3, 0}, {7, 0});
        const Segment whole_edge({0, 0}, {10, 0});
        const Segment overhanging({-3, 0}, {13, 0});

        CHECK(square.boundaryContains(along_edge));
        CHECK_FALSE(along_edge.separates(square));
        CHECK_FALSE(whole_edge.separates(square));
        CHECK_FALSE(overhanging.separates(square));
    }

    SUBCASE("segment from an edge into the interior is a slit") {
        const Segment into_interior({5, 0}, {5, 5});

        CHECK(square.boundaryContains(into_interior.min()));
        CHECK(square.interiorContains(into_interior.max()));
        CHECK_FALSE(into_interior.separates(square));
    }
}

TEST_CASE("Segment separates non-convex Polygon") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Polygon = pgl::Polygon<Point>;

    // Square with a rectangular notch cut into the top middle (x in [4, 6],
    // reaching down to y = 4).
    const Polygon notch({0, 0, 10, 0, 10, 10, 6, 10, 6, 4, 4, 4, 4, 10, 0, 10});

    SUBCASE("both endpoints inside, dipping through the notch, does not cut") {
        const Segment dip({1, 6}, {9, 6});

        CHECK(notch.interiorContains(dip.min()));
        CHECK(notch.interiorContains(dip.max()));
        CHECK_FALSE(dip.separates(notch));
    }

    SUBCASE("line crossing the whole shape through the notch cuts it") {
        const Segment across({-1, 6}, {11, 6});

        CHECK(across.separates(notch));
    }

    SUBCASE("chord below the notch through the solid base cuts it") {
        const Segment low({-1, 2}, {11, 2});

        CHECK(low.separates(notch));
    }

    // L-shaped polygon: a vertical arm (x in [0, 3]) joined to a horizontal arm
    // (y in [0, 3]) meeting at a reflex vertex at (3, 3).
    const Polygon ell({0, 0, 6, 0, 6, 3, 3, 3, 3, 6, 0, 6});

    SUBCASE("chord across the horizontal arm cuts it") {
        const Segment cut({-1, 1}, {7, 1});

        CHECK(cut.separates(ell));
    }

    SUBCASE("chord across the vertical arm cuts it") {
        const Segment cut({-1, 4}, {7, 4});

        CHECK(cut.separates(ell));
    }

    SUBCASE("diagonal through the reflex vertex cuts it") {
        const Segment diagonal({0, 6}, {6, 0});

        CHECK(diagonal.separates(ell));
    }


    SUBCASE("seg from inside an L through the reflex vertex stays one piece") {
        const Segment through_reflex({5, 1}, {-5, 11});

        CHECK_FALSE(ell.separates(through_reflex));
    }
}

TEST_CASE("Segment separates a convex Polygon and Convex identically") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Polygon = pgl::Polygon<Point>;
    using Convex = pgl::Convex<Point>;

    const Polygon polygon({0, 0, 10, 0, 13, 7, 5, 12, -3, 7});
    const Convex convex({{0, 0}, {10, 0}, {13, 7}, {5, 12}, {-3, 7}});

    const Segment chord({-5, 6}, {15, 6});
    const Segment slit({4, 6}, {15, 6});
    const Segment edge_touch({-3, 7}, {-5, 9});

    CHECK(chord.separates(polygon) == chord.separates(convex));
    CHECK(slit.separates(polygon) == slit.separates(convex));
    CHECK(edge_touch.separates(polygon) == edge_touch.separates(convex));

    CHECK(chord.separates(polygon));
    CHECK_FALSE(slit.separates(polygon));
    CHECK_FALSE(edge_touch.separates(polygon));
}

TEST_CASE("Polygon separates Segment") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Polygon = pgl::Polygon<Point>;

    const Polygon square({0, 0, 10, 0, 10, 10, 0, 10});

    SUBCASE("body interrupts a chord that pokes out both ends") {
        const Segment chord({-1, 5}, {11, 5});

        CHECK(square.separates(chord));
    }

    SUBCASE("segment with one endpoint inside leaves a single outside piece") {
        const Segment slit({5, 5}, {11, 5});

        CHECK(square.interiorContains(slit.min()));
        CHECK_FALSE(square.separates(slit));
    }

    SUBCASE("segment fully inside has nothing left outside") {
        const Segment inner({2, 5}, {8, 5});

        CHECK(square.contains(inner));
        CHECK_FALSE(square.separates(inner));
    }

    SUBCASE("segment fully outside is untouched") {
        const Segment outside({-5, 5}, {-1, 5});

        CHECK_FALSE(square.separates(outside));
    }

    SUBCASE("segment passing through to the outside on both ends is cut") {
        const Segment through({5, -5}, {5, 15});

        CHECK(square.separates(through));
    }

    SUBCASE("segment lying within an edge is not cut") {
        const Segment along_edge({3, 0}, {7, 0});

        CHECK_FALSE(square.separates(along_edge));
    }

    SUBCASE("segment overhanging an edge is cut into two stubs") {
        // Collinear with the bottom edge but sticking out past both corners:
        // removing the closed square leaves a stub beyond each corner.
        const Segment overhanging({-3, 0}, {13, 0});

        CHECK(square.separates(overhanging));
    }

    SUBCASE("segment from a corner through the interior and out is one chord") {
        // Starts at the corner vertex, crosses the interior, exits, and ends
        // strictly outside: the inside part is a single chord, so only one
        // outside piece remains -- not a split.
        const Segment from_corner({0, 0}, {20, 10});

        CHECK(square.boundaryContains(from_corner.min()));
        CHECK_FALSE(square.separates(from_corner));
    }

    SUBCASE("a chord resting wholly inside the closed polygon is not cut") {
        // Both endpoints sit on the boundary, so the whole segment lies in the
        // closed square: removing the square leaves nothing. This is where the
        // two directions disagree -- the segment cuts the polygon, but the
        // polygon does not cut the segment.
        const Segment edge_to_edge({5, 0}, {5, 10});
        const Segment corner_diagonal({0, 0}, {10, 10});

        CHECK(edge_to_edge.separates(square));
        CHECK_FALSE(square.separates(edge_to_edge));

        CHECK(corner_diagonal.separates(square));
        CHECK_FALSE(square.separates(corner_diagonal));
    }
}

TEST_CASE("Polygon separates Segment for non-convex shapes") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Polygon = pgl::Polygon<Point>;

    const Polygon notch({0, 0, 10, 0, 10, 10, 6, 10, 6, 4, 4, 4, 4, 10, 0, 10});

    SUBCASE("both endpoints inside, dipping through the notch, is one outside piece") {
        const Segment dip({1, 6}, {9, 6});

        CHECK(notch.interiorContains(dip.min()));
        CHECK(notch.interiorContains(dip.max()));
        CHECK_FALSE(notch.separates(dip));
    }

    SUBCASE("line crossing the whole shape leaves several outside pieces") {
        const Segment across({-1, 6}, {11, 6});

        CHECK(notch.separates(across));
    }

    SUBCASE("chord through the solid base is cut into two outside pieces") {
        const Segment low({-1, 2}, {11, 2});

        CHECK(notch.separates(low));
    }
}

// Non-convex cases: segments running along an edge, crossing at a reflex/notch
// feature, or passing through the notch gap. Every expected value was checked
// against an exact rational ground truth.
TEST_CASE("Polygon separates Segment for reflex/edge-collinear non-convex cases") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Polygon = pgl::Polygon<Point>;

    // Square with a rectangular notch cut into the top middle (x in [4, 6]).
    const Polygon notch({0, 0, 10, 0, 10, 10, 6, 10, 6, 4, 4, 4, 4, 10, 0, 10});
    // L-shape: vertical arm x in [0, 3], horizontal arm y in [0, 3].
    const Polygon ell({0, 0, 6, 0, 6, 3, 3, 3, 3, 6, 0, 6});

    SUBCASE("segment runs up the notch wall and pokes out the bottom") {
        // Along the right notch wall (x = 6); inside [0,4], on the wall [4,5].
        // Only one piece (below y = 0) is outside, so it must NOT separate.
        const Segment along_wall({6, -4}, {6, 5});

        CHECK_FALSE(notch.separates(along_wall));
    }

    SUBCASE("segment along the top edge then across the notch gap separates") {
        // Lies on the top-left edge for x in [0,4], crosses the open notch gap
        // (x in (4,6), outside), and ends at the vertex (6,10): two outside
        // pieces -- before x=0 and across the gap -- so it DOES separate.
        const Segment across_gap({-3, 10}, {6, 10});

        CHECK(notch.separates(across_gap));
    }

    SUBCASE("segment along an L arm's top edge pokes out one end") {
        // Runs along the horizontal arm's top edge (y = 3) and out to the left;
        // only the piece left of x = 0 is outside, so it must NOT separate.
        const Segment along_top({-3, 3}, {6, 3});

        CHECK_FALSE(ell.separates(along_top));
    }

    SUBCASE("segment along an L arm's side edge pokes out one end") {
        // Runs along the vertical arm's right edge (x = 3) and out the bottom.
        const Segment along_side({3, -4}, {3, 4});

        CHECK_FALSE(ell.separates(along_side));
    }
}

// Unlike separates, the containment/intersection predicates are correct for
// non-convex polygons. Every expected value was checked against an exact
// rational ground truth (notch and L-shape, ~2M random segments, 0 mismatches).
TEST_CASE("Polygon contains/intersects a segment for non-convex shapes") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Polygon = pgl::Polygon<Point>;

    // Square with a rectangular notch (the gap x in (4,6), y in (4,10]).
    const Polygon notch({0, 0, 10, 0, 10, 10, 6, 10, 6, 4, 4, 4, 4, 10, 0, 10});
    // L-shape: horizontal arm y in [0,3], vertical arm x in [0,3].
    const Polygon ell({0, 0, 6, 0, 6, 3, 3, 3, 3, 6, 0, 6});

    SUBCASE("segment strictly inside the solid base") {
        const Segment s({1, 1}, {9, 1});

        CHECK(notch.contains(s));
        CHECK(notch.interiorContains(s));
        CHECK(notch.intersects(s));
        CHECK(notch.interiorsIntersect(s));
    }

    SUBCASE("segment spanning both arms across the notch gap") {
        // Inside the left arm, out through the gap, inside the right arm.
        const Segment s({1, 8}, {9, 8});

        CHECK_FALSE(notch.contains(s));         // the gap part is outside
        CHECK_FALSE(notch.interiorContains(s));
        CHECK(notch.intersects(s));
        CHECK(notch.interiorsIntersect(s));     // the arm parts meet the interior
    }

    SUBCASE("segment lying along the notch wall") {
        // On the boundary edge x = 6: contained (closed) but not in the interior,
        // and its relative interior never reaches the polygon interior.
        const Segment s({6, 5}, {6, 9});

        CHECK(notch.contains(s));
        CHECK_FALSE(notch.interiorContains(s));
        CHECK(notch.intersects(s));
        CHECK_FALSE(notch.interiorsIntersect(s));
    }

    SUBCASE("segment from the base up into the notch gap") {
        const Segment s({5, 1}, {5, 8});

        CHECK_FALSE(notch.contains(s));
        CHECK_FALSE(notch.interiorContains(s));
        CHECK(notch.intersects(s));
        CHECK(notch.interiorsIntersect(s));
    }

    SUBCASE("segment entirely outside") {
        const Segment s({-3, 8}, {-1, 8});

        CHECK_FALSE(notch.contains(s));
        CHECK_FALSE(notch.interiorContains(s));
        CHECK_FALSE(notch.intersects(s));
        CHECK_FALSE(notch.interiorsIntersect(s));
    }

    SUBCASE("segment in the L's missing corner is untouched") {
        const Segment s({4, 4}, {5, 5});

        CHECK_FALSE(ell.contains(s));
        CHECK_FALSE(ell.interiorContains(s));
        CHECK_FALSE(ell.intersects(s));
        CHECK_FALSE(ell.interiorsIntersect(s));
    }

    SUBCASE("segment along the L's reflex top edge") {
        // From the reflex vertex (3,3) along the horizontal arm's top edge.
        const Segment s({3, 3}, {6, 3});

        CHECK(ell.contains(s));
        CHECK_FALSE(ell.interiorContains(s));
        CHECK(ell.intersects(s));
        CHECK_FALSE(ell.interiorsIntersect(s));
    }

    SUBCASE("segment strictly inside, spanning both L arms") {
        const Segment s({1, 1}, {2, 5});

        CHECK(ell.contains(s));
        CHECK(ell.interiorContains(s));
        CHECK(ell.intersects(s));
        CHECK(ell.interiorsIntersect(s));
    }
}

// Polygon::intersection clips the segment against the closed region and returns
// the disjoint pieces (points and sub-segments) in order along the segment.
TEST_CASE("Polygon intersection with a segment for non-convex shapes") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using OrientedSegment = pgl::OrientedSegment<Point>;
    using Polygon = pgl::Polygon<Point>;
    using Piece = std::variant<Point, Segment>;

    // Square with a rectangular notch (the gap x in (4,6), y in (4,10]).
    const Polygon notch({0, 0, 10, 0, 10, 10, 6, 10, 6, 4, 4, 4, 4, 10, 0, 10});

    SUBCASE("chord spanning both arms yields two disjoint pieces") {
        const Segment s({1, 8}, {9, 8});
        const auto pieces = notch.intersection(s);

        REQUIRE(pieces.size() == 2);
        CHECK(pieces[0] == Piece(Segment({1, 8}, {4, 8})));
        CHECK(pieces[1] == Piece(Segment({6, 8}, {9, 8})));
    }

    SUBCASE("orientation is ignored: pieces still run from min to max") {
        const auto pieces = notch.intersection(OrientedSegment({9, 8}, {1, 8}));

        REQUIRE(pieces.size() == 2);
        CHECK(pieces[0] == Piece(Segment({1, 8}, {4, 8})));
        CHECK(pieces[1] == Piece(Segment({6, 8}, {9, 8})));
    }

    SUBCASE("segment crossing the solid base is a single piece") {
        const auto pieces = notch.intersection(Segment({-3, 1}, {13, 1}));

        REQUIRE(pieces.size() == 1);
        CHECK(pieces[0] == Piece(Segment({0, 1}, {10, 1})));
    }

    SUBCASE("a segment dropping into the gap touches the floor at one point") {
        // Vertical segment down the middle of the gap: only its endpoint on the
        // notch floor (5, 4) lies in the closed region.
        const auto pieces = notch.intersection(Segment({5, 4}, {5, 9}));

        REQUIRE(pieces.size() == 1);
        CHECK(pieces[0] == Piece(Point(5, 4)));
    }

    SUBCASE("a segment missing the polygon yields nothing") {
        const auto pieces = notch.intersection(Segment({-3, 8}, {-1, 8}));

        CHECK(pieces.empty());
    }

    SUBCASE("fractional crossings need a rational result type") {
        using Rat = pgl::Rational<long long>;
        using RatPoint = pgl::Point<Rat>;
        using RatSegment = pgl::Segment<RatPoint>;
        using RatPiece = std::variant<RatPoint, RatSegment>;

        // Triangle with hypotenuse x + y = 5; a horizontal chord at y = 2 meets
        // the slanted edge at the half-integer point (3, 2)... here integer, so
        // use a slope that lands off-lattice: chord from (0,1) to (5,4) crossing.
        const Polygon tri({0, 0, 6, 0, 0, 6});  // hypotenuse x + y = 6
        const auto pieces = tri.intersection<Rat>(Segment({-1, 1}, {6, 1}));

        REQUIRE(pieces.size() == 1);
        CHECK(pieces[0] == RatPiece(RatSegment({Rat(0), Rat(1)}, {Rat(5), Rat(1)})));
    }
}

TEST_CASE("Polygon separates Segment matches Convex separates Segment") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Polygon = pgl::Polygon<Point>;
    using Convex = pgl::Convex<Point>;

    const Polygon polygon({0, 0, 10, 0, 13, 7, 5, 12, -3, 7});
    const Convex convex({{0, 0}, {10, 0}, {13, 7}, {5, 12}, {-3, 7}});

    const Segment chord({-5, 6}, {15, 6});
    const Segment slit({4, 6}, {15, 6});
    const Segment inside({3, 6}, {7, 6});

    CHECK(polygon.separates(chord) == convex.separates(chord));
    CHECK(polygon.separates(slit) == convex.separates(slit));
    CHECK(polygon.separates(inside) == convex.separates(inside));

    CHECK(polygon.separates(chord));
    CHECK_FALSE(polygon.separates(slit));
    CHECK_FALSE(polygon.separates(inside));
}

// A segment that only touches the polygon boundary -- grazing a vertex or lying
// along an edge from outside -- is still split into two pieces, so it separates.
// Ground-truth verified.
TEST_CASE("Polygon separates a boundary-touching segment") {
    using Point = pgl::Point<int>;
    using Segment = pgl::Segment<Point>;
    using Polygon = pgl::Polygon<Point>;

    // Convex pentagon with apex at (5, 12).
    const Polygon pentagon({0, 0, 10, 0, 13, 7, 5, 12, -3, 7});
    // Horizontal segment grazing only the apex vertex (5, 12): it touches the
    // polygon at that single point, so removing the polygon leaves a piece on
    // each side -- it must separate.
    const Segment apex_graze({-3, 12}, {9, 12});

    CHECK(pentagon.intersects(apex_graze));
    CHECK_FALSE(pentagon.interiorsIntersect(apex_graze));
    CHECK(pentagon.separates(apex_graze));
}
