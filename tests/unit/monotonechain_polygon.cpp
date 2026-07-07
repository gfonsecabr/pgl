#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pgl.hpp"

// L-shaped polygon: bottom arm y in [0,2] for x in [0,6], left arm x in [0,2]
// for y in [0,6]; the notch x > 2, y > 2 is outside.
namespace {
const pgl::Polygon<pgl::Point<int>> ell({pgl::Point<int>(0, 0), pgl::Point<int>(6, 0),
                                         pgl::Point<int>(6, 2), pgl::Point<int>(2, 2),
                                         pgl::Point<int>(2, 6), pgl::Point<int>(0, 6)});
}

TEST_CASE("Polygon containment of a chain folds over the chain edges") {
    using Point = pgl::Point<int>;
    using Chain = pgl::MonotoneChain<Point>;

    // Inside the left arm.
    CHECK(ell.contains(Chain({1, 1, 1, 5})));
    CHECK(ell.interiorContains(Chain({1, 1, 1, 5})));

    // Both endpoints inside, but the edge cuts through the notch: a vertex
    // fold would wrongly accept this chain.
    const Chain acrossNotch({1, 4, 4, 1});
    CHECK(ell.contains(acrossNotch[0]));
    CHECK(ell.contains(acrossNotch[1]));
    CHECK_FALSE(ell.contains(acrossNotch));
    CHECK_FALSE(ell.interiorContains(acrossNotch));

    // Along the bottom boundary through a collinear vertex.
    const Chain bottom({0, 0, 3, 0, 6, 0});
    CHECK(ell.contains(bottom));
    CHECK(ell.boundaryContains(bottom));
    CHECK_FALSE(ell.interiorContains(bottom));
    CHECK_FALSE(Chain({1, 1, 1, 5}).contains(ell));   // a 1D chain holds no polygon
}

TEST_CASE("MonotoneChain and Polygon intersection predicates, both directions") {
    using Point = pgl::Point<int>;
    using Chain = pgl::MonotoneChain<Point>;

    const Chain acrossNotch({1, 4, 4, 1});
    CHECK(ell.intersects(acrossNotch));
    CHECK(acrossNotch.intersects(ell));     // routed through the chain's rank forwarder
    CHECK(ell.interiorsIntersect(acrossNotch));
    CHECK(acrossNotch.interiorsIntersect(ell));

    const Chain insideNotch({3, 3, 5, 5});
    CHECK_FALSE(ell.intersects(insideNotch));
    CHECK_FALSE(insideNotch.intersects(ell));

    // Sliding along the notch boundary y = 2, x in [2,6]: touch only.
    const Chain alongNotch({3, 2, 5, 2});
    CHECK(ell.intersects(alongNotch));
    CHECK_FALSE(ell.interiorsIntersect(alongNotch));
}

TEST_CASE("Chain separates a polygon") {
    using Point = pgl::Point<int>;
    using Chain = pgl::MonotoneChain<Point>;

    // Both endpoints interior but the edge escapes through the notch: two
    // slits with loose interior ends, no disconnection.
    const Chain acrossNotch({1, 4, 4, 1});
    CHECK_FALSE(acrossNotch.separates(ell));

    // Continuing out the right side turns the bottom-arm piece into a
    // boundary-to-boundary crosscut: the notch excursion is the outside stop
    // before the interior vertex (4,1), and (7,1) is the one after.
    CHECK(Chain({1, 4, 4, 1, 7, 1}).separates(ell));

    // A single edge cutting straight through the bottom arm.
    CHECK(Chain({3, -1, 4, 3}).separates(ell));
    // Sliding along the notch boundary removes boundary points only.
    CHECK_FALSE(Chain({3, 2, 5, 2}).separates(ell));
    // Strictly inside the notch: never meets the polygon.
    CHECK_FALSE(Chain({3, 3, 5, 5}).separates(ell));
}

TEST_CASE("Polygon separates a chain crossing through it") {
    using Point = pgl::Point<int>;
    using Chain = pgl::MonotoneChain<Point>;

    // A chord entering the left arm and leaving out the far side: a covered
    // middle piece with both chain ends outside the polygon.
    CHECK(ell.separates(Chain({-1, 1, 7, 1})));
    // Bending inside the polygon on the way through still cuts the chain.
    CHECK(ell.separates(Chain({-1, 1, 1, 1, 3, 5})));
    // Ending strictly inside the polygon leaves a slit, not a cut.
    CHECK_FALSE(ell.separates(Chain({-1, 1, 1, 1})));
    // A chain diving into the notch (outside) between two covered ends is not
    // cut: the notch excursion keeps a loose end on each covered piece.
    CHECK_FALSE(ell.separates(Chain({1, 1, 4, 4, 5, 1})));
    // Running along the bottom boundary removes boundary points only.
    CHECK_FALSE(ell.separates(Chain({0, 0, 3, 0, 6, 0})));
    // Disjoint from the polygon.
    CHECK_FALSE(ell.separates(Chain({8, 5, 10, 7})));

    // A chord through the left arm disconnects each shape, so they cross.
    const Chain chord({-1, 1, 7, 1});
    CHECK(ell.crosses(chord));
    CHECK(chord.crosses(ell));
}

TEST_CASE("Chain weaving through a comb polygon separates it via excursions") {
    using Point = pgl::Point<int>;
    using Chain = pgl::MonotoneChain<Point>;

    // Comb: base bar y in [0,1] under three teeth x in [0,2], [4,6], [8,10]
    // rising to y = 6; the two gaps between the teeth are outside.
    const pgl::Polygon<Point> comb({Point(0, 0), Point(10, 0), Point(10, 6), Point(8, 6),
                                    Point(8, 1), Point(6, 1), Point(6, 6), Point(4, 6),
                                    Point(4, 1), Point(2, 1), Point(2, 6), Point(0, 6)});

    // Every chain vertex is strictly interior (one per tooth), yet the chain
    // cuts the middle tooth wall to wall: only the gap excursions witness the
    // outside stops around the interior vertex (5,3).
    CHECK(Chain({1, 3, 5, 3, 9, 3}).separates(comb));
    // One excursion alone leaves two slits with loose interior ends.
    CHECK_FALSE(Chain({1, 3, 5, 3}).separates(comb));
    // Entering from the gap and stopping inside the middle tooth: a slit.
    CHECK_FALSE(Chain({3, 3, 5, 3}).separates(comb));
    // Crossing the middle tooth from gap to gap disconnects its top.
    CHECK(Chain({3, 3, 5, 3, 7, 3}).separates(comb));
}

TEST_CASE("MonotoneChain and Polygon distance") {
    using Point = pgl::Point<int>;
    using Chain = pgl::MonotoneChain<Point>;
    using Rational = pgl::Rational<int>;

    const Chain right({8, 0, 9, 1});
    CHECK(ell.squaredDistance<Rational>(right) == Rational(4));
    CHECK(right.squaredDistance<Rational>(ell) == Rational(4));   // chain forwards to Polygon
    CHECK(right.distanceL1<Rational>(ell) == Rational(2));
    CHECK(right.distanceLInf<Rational>(ell) == Rational(2));
    CHECK(ell.squaredDistance<Rational>(Chain({1, 1, 1, 5})) == Rational(0));

    // Chain hovering over the notch: nearest polygon points are on the notch
    // edges at distance 1.
    const Chain overNotch({3, 3, 5, 3});
    CHECK(ell.squaredDistance<Rational>(overNotch) == Rational(1));
}
