#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

// Convex::separates(Polygon) asks whether removing the convex region from the
// polygon leaves two or more connected pieces.  Triangle::separates(Polygon)
// and Rectangle::separates(Polygon) both delegate to the Convex overload.
//
// Convention reminder: a convex separator cuts the polygon when its
// intersection forms a band that crosses the body; a mere bite out of one side
// leaves the polygon connected.

TEST_CASE("Convex separates a non-convex polygon") {
    using Point = pgl::Point<int>;
    using Polygon = pgl::Polygon<Point>;
    using Convex = pgl::Convex<Point>;

    // "U" opening upward: solid base for y in [0,4] spanning the full width,
    // with two arms x in [0,4] and x in [6,10] rising to y = 10.
    const Polygon u({0, 0, 10, 0, 10, 10, 6, 10, 6, 4, 4, 4, 4, 10, 0, 10});

    SUBCASE("a band across the base detaches the two arms") {
        const Convex band({{-1, 1}, {11, 1}, {11, 3}, {-1, 3}});
        CHECK(band.separates(u));
    }

    SUBCASE("a bite out of a single arm tip leaves it connected") {
        const Convex one_tip({{-1, 8}, {2, 8}, {2, 11}, {-1, 11}});
        CHECK_FALSE(one_tip.separates(u));
    }

    SUBCASE("a convex region the polygon never reaches leaves it whole") {
        const Convex away({{20, 20}, {24, 20}, {22, 24}});
        CHECK_FALSE(away.separates(u));
    }

    SUBCASE("a convex region covering the whole polygon removes everything") {
        const Convex cover({{-5, -5}, {15, -5}, {15, 15}, {-5, 15}});
        CHECK_FALSE(cover.separates(u));
    }
}

TEST_CASE("Convex separates a convex polygon") {
    using Point = pgl::Point<int>;
    using Polygon = pgl::Polygon<Point>;
    using Convex = pgl::Convex<Point>;

    // Convex pentagon with apex at (5, 12).
    const Polygon pentagon({0, 0, 10, 0, 13, 7, 5, 12, -3, 7});

    SUBCASE("a band clear across the body cuts it in two") {
        const Convex band({{-5, 5}, {15, 5}, {15, 7}, {-5, 7}});
        CHECK(band.separates(pentagon));
    }

    SUBCASE("a region clipping only a corner leaves it connected") {
        const Convex corner({{-5, -5}, {2, -5}, {-5, 2}});
        CHECK_FALSE(corner.separates(pentagon));
    }
}

TEST_CASE("Triangle separates Polygon delegates to the convex overload") {
    using Point = pgl::Point<int>;
    using Polygon = pgl::Polygon<Point>;
    using Triangle = pgl::Triangle<Point>;

    const Polygon square({0, 0, 10, 0, 10, 10, 0, 10});

    SUBCASE("a thin triangle spanning the full width cuts the square") {
        const Triangle band({-1, 5}, {11, 4}, {11, 6});
        CHECK(band.separates(square));
    }

    SUBCASE("a triangle biting a corner leaves the square connected") {
        const Triangle corner({-1, -1}, {4, -1}, {-1, 4});
        CHECK_FALSE(corner.separates(square));
    }
}

TEST_CASE("Rectangle separates Polygon delegates to the convex overload") {
    using Point = pgl::Point<int>;
    using Polygon = pgl::Polygon<Point>;
    using Rectangle = pgl::Rectangle<Point>;

    const Polygon square({0, 0, 10, 0, 10, 10, 0, 10});
    const Polygon u({0, 0, 10, 0, 10, 10, 6, 10, 6, 4, 4, 4, 4, 10, 0, 10});

    SUBCASE("a band across the square cuts it in two") {
        const Rectangle band({-1, 4}, {11, 6});
        CHECK(band.separates(square));
    }

    SUBCASE("a band across the U base detaches the arms") {
        const Rectangle base({-1, 1}, {11, 3});
        CHECK(base.separates(u));
    }
}

// Regression cases for the old arc-counting implementation, which reported
// "separated" whenever the polygon boundary had >= 2 arcs outside the convex
// region.  That over-counts when the convex region takes two disjoint "bites"
// out of a non-convex polygon while P\C stays connected.
TEST_CASE("Convex separates Polygon: disjoint bites must not separate") {
    using Point = pgl::Point<int>;
    using Polygon = pgl::Polygon<Point>;
    using Convex = pgl::Convex<Point>;
    using Triangle = pgl::Triangle<Point>;

    const Polygon u({0, 0, 10, 0, 10, 10, 6, 10, 6, 4, 4, 4, 4, 10, 0, 10});
    // Horseshoe ("C") opening to the right.
    const Polygon horseshoe({0, 0, 10, 0, 10, 3, 3, 3, 3, 7, 10, 7, 10, 10, 0, 10});

    SUBCASE("convex trimming both U arm tips leaves it connected") {
        // Covers the tops of both arms (two separate bites); the base still
        // joins them, so P\C is one piece.
        const Convex both_tips({{-1, 9}, {11, 9}, {11, 11}, {-1, 11}});
        CHECK_FALSE(both_tips.separates(u));
    }

    SUBCASE("triangle trimming both U arm tips leaves it connected") {
        // Strictly between the walls, so each arm keeps a free column beside
        // the bite and stays connected to the base.
        const Triangle both_tips({1, 9}, {9, 9}, {5, 12});
        CHECK_FALSE(both_tips.separates(u));
    }

    SUBCASE("triangle whose base corners touch the walls seals off pockets") {
        // Same shape, widened until the base corners touch the outer walls at
        // (0,9) and (10,9): the slanted sides then close little triangular
        // pockets against each arm's tip, so this one does separate.
        const Triangle touching({0, 9}, {10, 9}, {5, 12});
        CHECK(touching.separates(u));
    }

    SUBCASE("convex trimming both horseshoe tips leaves it connected") {
        // Bites the right ends of both arms; the spine still joins them.
        const Convex tips({{9, 2}, {11, 2}, {11, 8}, {9, 8}});
        CHECK_FALSE(tips.separates(horseshoe));
    }

    SUBCASE("cutting through a single U arm separates it") {
        // Same shape family as the bites, but this band severs the left arm
        // completely: its tip detaches from the rest.
        const Convex arm_cut({{-1, 6}, {5, 6}, {5, 8}, {-1, 8}});
        CHECK(arm_cut.separates(u));
    }
}

TEST_CASE("Convex separates Polygon through boundary touch points") {
    using Point = pgl::Point<int>;
    using Polygon = pgl::Polygon<Point>;
    using Convex = pgl::Convex<Point>;

    const Polygon square({0, 0, 10, 0, 10, 10, 0, 10});

    SUBCASE("a diamond inside the square pinching two opposite edges cuts it") {
        // The diamond touches the bottom edge at (5,0) and the top edge at
        // (5,10) without poking outside; the square falls into two halves
        // joined only at those two removed points.
        const Convex pinch({{5, 0}, {8, 5}, {5, 10}, {2, 5}});
        CHECK(pinch.separates(square));
    }

    SUBCASE("a diamond touching only one edge leaves the square connected") {
        const Convex one_touch({{5, 0}, {8, 5}, {5, 9}, {2, 5}});
        CHECK_FALSE(one_touch.separates(square));
    }

    SUBCASE("a band whose ends lie exactly on the boundary still cuts") {
        // The band spans the square wall to wall without crossing outside.
        const Convex band({{0, 4}, {10, 4}, {10, 6}, {0, 6}});
        CHECK(band.separates(square));
    }
}

TEST_CASE("Convex separates Polygon with collinear boundary contact") {
    using Point = pgl::Point<int>;
    using Polygon = pgl::Polygon<Point>;
    using Convex = pgl::Convex<Point>;

    const Polygon square({0, 0, 10, 0, 10, 10, 0, 10});

    SUBCASE("a full-height band sharing the top and bottom edges cuts") {
        const Convex band({{4, 0}, {6, 0}, {6, 10}, {4, 10}});
        CHECK(band.separates(square));
    }

    SUBCASE("a slab sharing a side wall only shortens the square") {
        // Covers the top part; the boundary overlap along both side walls
        // must not be mistaken for a second cut.
        const Convex slab({{0, 4}, {10, 4}, {10, 12}, {0, 12}});
        CHECK_FALSE(slab.separates(square));
    }
}

TEST_CASE("Convex separates Polygon into more than two pieces") {
    using Point = pgl::Point<int>;
    using Polygon = pgl::Polygon<Point>;
    using Convex = pgl::Convex<Point>;

    const Polygon square({0, 0, 10, 0, 10, 10, 0, 10});

    SUBCASE("a triangle poking through three edges leaves three pieces") {
        const Convex poker({{-1, 5}, {5, -1}, {11, 5}});
        CHECK(poker.separates(square));
    }

    SUBCASE("a diamond poking through all four edges leaves the corners") {
        const Convex diamond({{5, -2}, {12, 5}, {5, 12}, {-2, 5}});
        CHECK(diamond.separates(square));
    }
}

TEST_CASE("Convex separates Polygon mixing a cut with a disjoint bite") {
    using Point = pgl::Point<int>;
    using Polygon = pgl::Polygon<Point>;
    using Convex = pgl::Convex<Point>;

    // U opening upward, as above.
    const Polygon u({0, 0, 10, 0, 10, 10, 6, 10, 6, 4, 4, 4, 4, 10, 0, 10});

    // Severs the left arm and also takes a separate bite out of the right
    // arm's inner wall: still a genuine cut despite the extra pocket.
    const Convex wide({{-1, 6}, {7, 6}, {7, 8}, {-1, 8}});
    CHECK(wide.separates(u));
}

// Containment / intersection between a Convex and a Polygon, both directions.
// boundaryContains on a real (>= 3 vertex) area is always false — a filled
// region is never confined to a 1D boundary. crosses and Polygon::separates
// (Convex) throw "not implemented" and are out of scope here.

TEST_CASE("Convex contains a polygon") {
    using Point = pgl::Point<int>;
    using Polygon = pgl::Polygon<Point>;
    using Convex = pgl::Convex<Point>;

    const Convex square({{0, 0}, {10, 0}, {10, 10}, {0, 10}});
    const Polygon inside({2, 2, 8, 2, 5, 7});   // triangle strictly within the square

    CHECK(square.contains(inside));
    CHECK(square.interiorContains(inside));
    CHECK_FALSE(square.boundaryContains(inside));
    CHECK(square.intersects(inside));
    CHECK(square.interiorsIntersect(inside));

    // The small triangle cannot contain the square back.
    CHECK_FALSE(inside.contains(square));
    CHECK_FALSE(inside.interiorContains(square));
    CHECK_FALSE(inside.boundaryContains(square));
    CHECK(inside.intersects(square));
    CHECK(inside.interiorsIntersect(square));
}

TEST_CASE("Polygon contains a convex") {
    using Point = pgl::Point<int>;
    using Polygon = pgl::Polygon<Point>;
    using Convex = pgl::Convex<Point>;

    const Polygon big({0, 0, 20, 0, 20, 20, 0, 20});
    const Convex small({{5, 5}, {15, 5}, {15, 15}, {5, 15}});   // strictly within big

    CHECK(big.contains(small));
    CHECK(big.interiorContains(small));
    CHECK_FALSE(big.boundaryContains(small));
    CHECK(big.intersects(small));
    CHECK(big.interiorsIntersect(small));

    CHECK_FALSE(small.contains(big));
    CHECK_FALSE(small.interiorContains(big));
    CHECK_FALSE(small.boundaryContains(big));
    CHECK(small.intersects(big));
    CHECK(small.interiorsIntersect(big));

    // Snug against the boundary: every contact is degenerate — no vertex of either
    // strictly inside the other, no pair of edges properly crossing — yet the two
    // share interior points. Coincident regions are the extreme of the same case.
    const Convex snug({{0, 0}, {20, 0}, {20, 10}, {0, 10}});  // shares three sides
    CHECK(big.contains(snug));
    CHECK(big.interiorsIntersect(snug));
    CHECK(snug.interiorsIntersect(big));

    const Convex same({{0, 0}, {20, 0}, {20, 20}, {0, 20}});
    CHECK(big.interiorsIntersect(same));
    CHECK(same.interiorsIntersect(big));
}

TEST_CASE("Convex and polygon overlap without either containing the other") {
    using Point = pgl::Point<int>;
    using Polygon = pgl::Polygon<Point>;
    using Convex = pgl::Convex<Point>;

    const Convex square({{0, 0}, {10, 0}, {10, 10}, {0, 10}});
    const Polygon shifted({5, 5, 15, 5, 15, 15, 5, 15});   // overlaps the corner [5,10]^2

    CHECK_FALSE(square.contains(shifted));
    CHECK_FALSE(shifted.contains(square));
    CHECK_FALSE(square.interiorContains(shifted));
    CHECK_FALSE(shifted.interiorContains(square));
    CHECK(square.intersects(shifted));
    CHECK(shifted.intersects(square));
    CHECK(square.interiorsIntersect(shifted));
    CHECK(shifted.interiorsIntersect(square));
}

TEST_CASE("Convex and polygon that only touch at a corner") {
    using Point = pgl::Point<int>;
    using Polygon = pgl::Polygon<Point>;
    using Convex = pgl::Convex<Point>;

    const Convex square({{0, 0}, {10, 0}, {10, 10}, {0, 10}});
    const Polygon touch({10, 10, 15, 12, 15, 18});   // meets the square only at (10,10)

    CHECK(square.intersects(touch));
    CHECK(touch.intersects(square));
    // Boundaries kiss but interiors stay apart.
    CHECK_FALSE(square.interiorsIntersect(touch));
    CHECK_FALSE(touch.interiorsIntersect(square));
    CHECK_FALSE(square.contains(touch));
    CHECK_FALSE(touch.contains(square));
}

TEST_CASE("Convex and polygon that are disjoint") {
    using Point = pgl::Point<int>;
    using Polygon = pgl::Polygon<Point>;
    using Convex = pgl::Convex<Point>;

    const Convex square({{0, 0}, {10, 0}, {10, 10}, {0, 10}});
    const Polygon far_away({20, 20, 30, 20, 25, 30});

    CHECK_FALSE(square.intersects(far_away));
    CHECK_FALSE(far_away.intersects(square));
    CHECK_FALSE(square.interiorsIntersect(far_away));
    CHECK_FALSE(far_away.interiorsIntersect(square));
    CHECK_FALSE(square.contains(far_away));
    CHECK_FALSE(far_away.contains(square));
    CHECK_FALSE(square.boundaryContains(far_away));
    CHECK_FALSE(far_away.boundaryContains(square));
}

// Polygon::intersection(Convex) forwards to the Polygon overload via
// Convex::asPolygon. The result type is requested as pgl::ERational so that
// fractional crossing coordinates stay exact (no floating-point rounding).
TEST_CASE("Polygon::intersection(Convex) forwards to the polygon overload") {
    using EPoint = pgl::Point<pgl::ERational>;
    using EPolygon = pgl::Polygon<EPoint>;

    const pgl::Polygon<pgl::Point<int>> square({0, 0, 10, 0, 10, 10, 0, 10});

    auto twiceAreaOfPolys = [](const auto& res) {
        pgl::ERational a(0);
        for (const auto& v : res)
            if (std::holds_alternative<EPolygon>(v)) a += std::get<EPolygon>(v).twiceArea();
        return a;
    };

    SUBCASE("overlapping region is a polygon and matches the explicit polygon overload") {
        const pgl::Convex<pgl::Point<int>> conv({5, 5, 15, 5, 15, 15, 5, 15});
        const auto pieces = square.intersection<pgl::ERational>(conv);

        // Delegation must yield exactly the explicit polygon-overload result.
        CHECK(pieces == square.intersection<pgl::ERational>(conv.asPolygon()));
        REQUIRE(pieces.size() == 1);
        REQUIRE(std::holds_alternative<EPolygon>(pieces[0]));
        CHECK(twiceAreaOfPolys(pieces) == pgl::ERational(50));  // the 5x5 overlap
    }

    SUBCASE("fractional crossings stay exact with ERational") {
        // A tilted convex triangle crossing the square at non-integer points.
        const pgl::Convex<pgl::Point<int>> conv({3, -2, 14, 9, 3, 9});
        const auto pieces = square.intersection<pgl::ERational>(conv);

        CHECK(pieces == square.intersection<pgl::ERational>(conv.asPolygon()));
        CHECK(twiceAreaOfPolys(pieces) > pgl::ERational(0));
    }

    SUBCASE("disjoint shapes give no intersection") {
        const pgl::Convex<pgl::Point<int>> away({20, 20, 30, 20, 25, 30});
        CHECK(square.intersection<pgl::ERational>(away).empty());
    }
}
