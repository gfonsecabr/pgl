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

// KNOWN BUG (expected-fail): the arc-counting implementation reports "separated"
// whenever the polygon boundary has >= 2 arcs outside the convex region.  That
// over-counts when the convex region takes two disjoint "bites" out of a
// non-convex polygon while P\C stays connected: the correct answer is FALSE but
// the implementation returns TRUE.  These cases assert the geometrically correct
// answer, so they fail today; doctest::should_fail keeps the suite green and
// will flag this case (as an unexpected pass) once the predicate is fixed.
TEST_CASE("Convex separates Polygon: disjoint bites must not separate"
          * doctest::should_fail()) {
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
        const Triangle both_tips({0, 9}, {10, 9}, {5, 12});
        CHECK_FALSE(both_tips.separates(u));
    }

    SUBCASE("convex trimming both horseshoe tips leaves it connected") {
        // Bites the right ends of both arms; the spine still joins them.
        const Convex tips({{9, 2}, {11, 2}, {11, 8}, {9, 8}});
        CHECK_FALSE(tips.separates(horseshoe));
    }
}
