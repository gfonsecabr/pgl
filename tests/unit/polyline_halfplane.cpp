#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

using Point = pgl::Point<int>;
using Halfplane = pgl::Halfplane<Point>;
// Aliased as `PLine` (not `Polyline`) because doctest pulls in <windows.h> on
// MSVC, whose GDI `Polyline` function collides with a file-scope `using
// Polyline`.
using PLine = pgl::Polyline<Point>;

// zig-zag: (0,0) - (2,2) - (4,0)
static const PLine zig({0, 0, 2, 2, 4, 0});

TEST_CASE("Polyline and Halfplane containment and intersection") {
    const Halfplane upper({0, 1}, {1, 1});    // closed side y >= 1
    const Halfplane high({0, 3}, {1, 3});     // closed side y >= 3, misses
    const Halfplane low({0, -1}, {1, -1});    // closed side y >= -1, swallows

    CHECK(zig.intersects(upper));
    CHECK(upper.intersects(zig));
    CHECK_FALSE(zig.intersects(high));
    CHECK(low.contains(zig));
    CHECK(low.interiorContains(zig));
    CHECK_FALSE(upper.contains(zig));
    CHECK(zig.interiorsIntersect(upper));
    CHECK_FALSE(zig.interiorsIntersect(high));
    CHECK_FALSE(zig.contains(upper));   // a bounded polyline holds no halfplane
    CHECK_FALSE(zig.interiorContains(upper));

    // A polyline along the boundary line is boundary-contained, even when it
    // back-tracks over itself.
    const PLine onBoundary({0, 1, 3, 1, 2, 1});
    CHECK(upper.boundaryContains(onBoundary));
    CHECK_FALSE(upper.boundaryContains(zig));
    CHECK_FALSE(upper.interiorContains(onBoundary));
    CHECK(upper.contains(onBoundary));
}

TEST_CASE("Halfplane separates a polyline passing through it") {
    // y >= 1 clips out the apex region, leaving the two low ends.
    const Halfplane upper({0, 1}, {1, 1});
    CHECK(upper.separates(zig));
    // The clipped tent runs boundary-to-boundary through the halfplane's
    // interior, so the cut goes both ways.
    CHECK(zig.separates(upper));
    CHECK(zig.crosses(upper));

    // y >= -1 swallows the whole polyline.
    const Halfplane low({0, -1}, {1, -1});
    CHECK_FALSE(low.separates(zig));

    // x <= 1 removes an end piece only.
    const Halfplane left({1, 0}, {1, 1});
    CHECK(left.contains(Point(0, 0)));
    CHECK_FALSE(left.separates(zig));

    SUBCASE("set semantics: the loop reconnects around a clipped corner") {
        // Removing the corner region x <= 0 deletes the loop's left edge, but
        // the other three sides remain connected.
        const PLine loop({0, 0, 2, 0, 2, 2, 0, 2, 0, 0});
        const Halfplane corner({0, 0}, {0, 1});   // contains x <= 0
        CHECK(corner.contains(Point(-1, 0)));
        CHECK_FALSE(corner.separates(loop));
        // Clipping the middle x <= 1 leaves the two vertical-ish sides
        // connected along the remaining halves of top and bottom? No: the
        // remaining set {x >= 1} of the loop is the right edge plus two stubs,
        // still one connected piece.
        const Halfplane half({1, 0}, {1, 1});
        CHECK_FALSE(half.separates(loop));
    }
}

TEST_CASE("Polyline separates a halfplane by sealing off a pocket") {
    const Halfplane lower({0, 0}, {-1, 0});   // closed side y <= 0

    SUBCASE("a dip below the boundary seals a pocket") {
        const PLine dip({0, 1, 1, -1, 2, 1});
        CHECK(dip.separates(lower));
        CHECK(lower.separates(dip));   // the halfplane also severs the dip's tip
        CHECK(dip.crosses(lower));
    }

    SUBCASE("a slit hanging into the halfplane does not") {
        const PLine slit({0, 1, 1, -1});
        CHECK_FALSE(slit.separates(lower));
    }

    SUBCASE("a straight pass along the boundary does not") {
        const PLine along({0, 0, 3, 0});
        CHECK_FALSE(along.separates(lower));
    }

    SUBCASE("a loop strictly inside the halfplane seals a pocket") {
        // A closed square below y = 0: its inside is cut off from the rest of
        // the halfplane even though the loop never reaches the boundary. A
        // traversal-order crosscut scan cannot see this.
        const PLine loop({0, -1, 2, -1, 2, -3, 0, -3, 0, -1});
        CHECK(loop.separates(lower));
    }

    SUBCASE("an open zig-zag strictly inside does not") {
        const PLine inside({0, -1, 1, -2, 2, -1});
        CHECK_FALSE(inside.separates(lower));
    }

    SUBCASE("a figure that only touches the boundary from inside still cuts") {
        // Enters at (0,0), dips to (1,-1), and leaves at (2,0): the piece of
        // the halfplane under the dip is sealed against the boundary.
        const PLine touching({0, 0, 1, -1, 2, 0});
        CHECK(touching.separates(lower));
    }
}

TEST_CASE("Polyline and Halfplane distance") {
    using Rational = pgl::Rational<int>;

    const Halfplane high({0, 3}, {1, 3});   // y >= 3, one unit above the apex
    CHECK(zig.squaredDistance<Rational>(high) == Rational(1));
    CHECK(high.squaredDistance<Rational>(zig) == Rational(1));
    CHECK(zig.distanceL1<Rational>(high) == Rational(1));
    CHECK(zig.distanceLInf<Rational>(high) == Rational(1));
    CHECK(zig.squaredDistance<Rational>(Halfplane({0, 1}, {1, 1})) == Rational(0));
}

TEST_CASE("Polyline and Halfplane intersection pieces") {
    using Segment = pgl::Segment<Point>;
    const Halfplane upper({0, 1}, {1, 1});  // closed side y >= 1

    const auto pieces = zig.intersection(upper);
    REQUIRE(pieces.size() == 2);
    REQUIRE(std::holds_alternative<Segment>(pieces[0]));
    CHECK(std::get<Segment>(pieces[0]) == Segment(Point(1, 1), Point(2, 2)));
    REQUIRE(std::holds_alternative<Segment>(pieces[1]));
    CHECK(std::get<Segment>(pieces[1]) == Segment(Point(2, 2), Point(3, 1)));

    CHECK(zig.intersection(Halfplane({0, 3}, {1, 3})).empty());

    SUBCASE("a swallowing halfplane returns the edges themselves") {
        const auto whole = zig.intersection(Halfplane({0, -1}, {1, -1}));
        REQUIRE(whole.size() == 2);
        REQUIRE(std::holds_alternative<Segment>(whole[0]));
        CHECK(std::get<Segment>(whole[0]) == Segment(Point(0, 0), Point(2, 2)));
        REQUIRE(std::holds_alternative<Segment>(whole[1]));
        CHECK(std::get<Segment>(whole[1]) == Segment(Point(2, 2), Point(4, 0)));
    }
}
