#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstdint>

#include "pgl.hpp"

// Reference chain used throughout: up to a peak at (2,4), down to (4,0), up the
// vertical edge to (4,4), and down to (6,0).
namespace {
const pgl::MonotoneChain<pgl::Point<int>> zigzag({0, 0, 2, 4, 4, 0, 4, 4, 6, 0});
}

TEST_CASE("MonotoneChain contains points on vertices, edge interiors, and vertical edges") {
    using Point = pgl::Point<int>;

    SUBCASE("every vertex is contained") {
        for (const Point& vertex : zigzag.vertices()) {
            CHECK_MESSAGE(zigzag.contains(vertex), zigzag, " contains ", vertex);
        }
    }

    SUBCASE("edge interior points are contained") {
        CHECK(zigzag.contains(Point(1, 2)));   // on (0,0)-(2,4)
        CHECK(zigzag.contains(Point(3, 2)));   // on (2,4)-(4,0)
        CHECK(zigzag.contains(Point(4, 2)));   // on the vertical edge
        CHECK(zigzag.contains(Point(5, 2)));   // on (4,4)-(6,0)
    }

    SUBCASE("points off the chain are not contained") {
        CHECK_FALSE(zigzag.contains(Point(1, 3)));    // above the first edge
        CHECK_FALSE(zigzag.contains(Point(1, 1)));    // below the first edge
        CHECK_FALSE(zigzag.contains(Point(4, 5)));    // above the vertical edge
        CHECK_FALSE(zigzag.contains(Point(4, -1)));   // below the vertical edge
        CHECK_FALSE(zigzag.contains(Point(-1, 0)));   // left of the x-extent
        CHECK_FALSE(zigzag.contains(Point(7, 0)));    // right of the x-extent
    }
}

TEST_CASE("MonotoneChain boundary is the two extreme vertices, the interior everything else") {
    using Point = pgl::Point<int>;

    SUBCASE("extreme vertices are boundary, not interior") {
        CHECK(zigzag.boundaryContains(Point(0, 0)));
        CHECK(zigzag.boundaryContains(Point(6, 0)));
        CHECK_FALSE(zigzag.interiorContains(Point(0, 0)));
        CHECK_FALSE(zigzag.interiorContains(Point(6, 0)));
    }

    SUBCASE("non-extreme vertices and edge interiors are interior, not boundary") {
        for (const Point p : {Point(2, 4), Point(4, 0), Point(4, 4), Point(1, 2), Point(4, 2)}) {
            CHECK_MESSAGE(zigzag.interiorContains(p), zigzag, " interiorContains ", p);
            CHECK_FALSE_MESSAGE(zigzag.boundaryContains(p), zigzag, " boundaryContains ", p);
        }
    }

    SUBCASE("contains splits exactly into boundary or interior on a grid sweep") {
        for (int x = -1; x <= 7; ++x) {
            for (int y = -2; y <= 5; ++y) {
                const Point p(x, y);
                CHECK_MESSAGE(zigzag.contains(p) ==
                                  (zigzag.boundaryContains(p) || zigzag.interiorContains(p)),
                              zigzag, " containment split at ", p);
                CHECK_FALSE_MESSAGE((zigzag.boundaryContains(p) && zigzag.interiorContains(p)),
                                    zigzag, " boundary/interior overlap at ", p);
            }
        }
    }

    SUBCASE("single-vertex chain: the point is boundary and the interior is empty") {
        const pgl::MonotoneChain<pgl::Point<int>> dot({pgl::Point<int>(3, 2)});
        CHECK(dot.contains(Point(3, 2)));
        CHECK(dot.boundaryContains(Point(3, 2)));
        CHECK_FALSE(dot.interiorContains(Point(3, 2)));
        CHECK_FALSE(dot.contains(Point(3, 3)));
    }
}

TEST_CASE("MonotoneChain intersection predicates against points, both directions") {
    using Point = pgl::Point<int>;

    for (int x = -1; x <= 7; ++x) {
        for (int y = -2; y <= 5; ++y) {
            const Point p(x, y);
            CHECK_MESSAGE(zigzag.intersects(p) == zigzag.contains(p),
                          zigzag, " intersects/contains mismatch at ", p);
            CHECK_MESSAGE(zigzag.interiorsIntersect(p) == zigzag.interiorContains(p),
                          zigzag, " interiorsIntersect/interiorContains mismatch at ", p);
            // The rank-based forwarder on Point routes through the chain.
            CHECK_MESSAGE(p.intersects(zigzag) == zigzag.intersects(p),
                          zigzag, " asymmetric intersects at ", p);
        }
    }
}

TEST_CASE("MonotoneChain point predicates stay exact at huge coordinates") {
    using Point = pgl::Point<int>;
    const pgl::MonotoneChain<Point> steep({0, 0, 1000000000, 999999999});

    // Just above the edge: the orientation determinant only fits a 64-bit
    // promotion, so a naive 32-bit evaluation would misclassify these.
    const Point above(500000000, 500000000);
    CHECK_FALSE(steep.contains(above));
    CHECK(steep.isBelow(above).has_value());
    CHECK_FALSE(steep.isAbove(above).has_value());

    const Point below(500000000, 499999999);
    CHECK_FALSE(steep.contains(below));
    CHECK(steep.isAbove(below).has_value());
    CHECK_FALSE(steep.isBelow(below).has_value());
}

TEST_CASE("MonotoneChain point predicates with other numeric types") {
    SUBCASE("rational query point on an integer chain") {
        using Rational = pgl::Rational<int>;
        const pgl::Point<Rational> onEdge(Rational(1, 2), Rational(1));   // on (0,0)-(2,4)
        const pgl::Point<Rational> offEdge(Rational(1, 2), Rational(3, 2));
        CHECK(zigzag.contains(onEdge));
        CHECK(zigzag.interiorContains(onEdge));
        CHECK_FALSE(zigzag.contains(offEdge));
    }

    SUBCASE("rational chain") {
        using Rational = pgl::Rational<int>;
        using Point = pgl::Point<Rational>;
        const pgl::MonotoneChain<Point> chain(
            {Point(Rational(0), Rational(0)), Point(Rational(2), Rational(1)),
             Point(Rational(4), Rational(0))});
        CHECK(chain.contains(Point(Rational(1), Rational(1, 2))));
        CHECK(chain.boundaryContains(Point(Rational(4), Rational(0))));
        CHECK_FALSE(chain.contains(Point(Rational(1), Rational(1, 3))));
    }

    SUBCASE("double chain") {
        using Point = pgl::Point<double>;
        const pgl::MonotoneChain<Point> chain({0.0, 0.0, 2.0, 1.0, 4.0, 0.0});
        CHECK(chain.contains(Point(1.0, 0.5)));
        CHECK_FALSE(chain.contains(Point(1.0, 0.75)));
    }
}
