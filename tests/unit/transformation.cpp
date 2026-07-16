#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <type_traits>
#include "pgl.hpp"

using pgl::Point;
using pgl::Rational;
using pgl::Transformation;

namespace {

template <class T, class U>
constexpr bool canMultiply = requires (const T& t, const U& u) { t * u; };

}  // namespace

TEST_CASE("Factories transform integer points exactly") {
    const Point<int> p(3, 5);

    CHECK(Transformation<int>::identity() * p == p);
    CHECK(Transformation<int>::translation(1, -2) * p == Point<int>(4, 3));
    CHECK(Transformation<int>::scaling(2) * p == Point<int>(6, 10));
    CHECK(Transformation<int>::scaling(2, 3) * p == Point<int>(6, 15));
    CHECK(Transformation<int>::shearX(2) * p == Point<int>(13, 5));
    CHECK(Transformation<int>::shearY(2) * p == Point<int>(3, 11));
    CHECK(Transformation<int>::reflectionX() * p == Point<int>(3, -5));
    CHECK(Transformation<int>::reflectionY() * p == Point<int>(-3, 5));

    for (int k = -4; k <= 4; ++k) {
        CHECK(Transformation<int>::rotation90(k) * p == p.rotated90(k));
    }
}

TEST_CASE("Composition applies the right-hand transformation first") {
    const Point<int> p(1, 1);
    const auto scale = Transformation<int>::scaling(2);
    const auto move = Transformation<int>::translation(3, 0);

    CHECK((scale * move) * p == scale * (move * p));
    CHECK((scale * move) * p == Point<int>(8, 2));
    CHECK((move * scale) * p == Point<int>(5, 2));
}

TEST_CASE("A reflection swaps a Halfplane's source and target to keep its interior") {
    const pgl::Halfplane<Point<int>> h(Point<int>(0, 0), Point<int>(1, 0));
    REQUIRE(h.contains(Point<int>(0, 5)));
    REQUIRE_FALSE(h.contains(Point<int>(0, -5)));

    const auto reflected = Transformation<int>::reflectionX() * h;
    CHECK(reflected.contains(Point<int>(0, -5)));
    CHECK_FALSE(reflected.contains(Point<int>(0, 5)));
}

TEST_CASE("An OrientedLine keeps its source/target order under a reflection") {
    const pgl::OrientedLine<Point<int>> line(Point<int>(0, 0), Point<int>(1, 0));
    const auto reflected = Transformation<int>::reflectionX() * line;
    CHECK(reflected.source() == Point<int>(0, 0));
    CHECK(reflected.target() == Point<int>(1, 0));
}

TEST_CASE("A Triangle stays correctly wound after an orientation-reversing transform") {
    const pgl::Triangle<Point<int>> tri(Point<int>(0, 0), Point<int>(4, 0), Point<int>(0, 4));
    const auto reflected = Transformation<int>::reflectionX() * tri;

    // The constructor's own normalize() re-establishes CCW winding, so the
    // result must still be equal to constructing the reflected points fresh.
    const pgl::Triangle<Point<int>> expected(Point<int>(0, 0), Point<int>(4, 0), Point<int>(0, -4));
    CHECK(reflected == expected);
}

TEST_CASE("A Convex re-normalizes (Graham scan) after an orientation-reversing transform") {
    const pgl::Convex<Point<int>> square({0, 0, 4, 0, 4, 4, 0, 4});
    const auto reflected = Transformation<int>::reflectionX() * square;

    CHECK(reflected.size() == 4);
    CHECK(reflected.contains(Point<int>(2, -2)));
    CHECK_FALSE(reflected.contains(Point<int>(2, 2)));
}

TEST_CASE("A Polygon re-normalizes after an orientation-reversing transform") {
    const pgl::Polygon<Point<int>> square({0, 0, 4, 0, 4, 4, 0, 4});
    const auto reflected = Transformation<int>::reflectionX() * square;

    CHECK(reflected.size() == 4);
    CHECK(reflected.contains(Point<int>(2, -2)));
    CHECK_FALSE(reflected.contains(Point<int>(2, 2)));
}

TEST_CASE("A HalfplaneIntersection re-normalizes after an orientation-reversing transform") {
    using Halfplane = pgl::Halfplane<Point<int>>;
    using Region = pgl::HalfplaneIntersection<Point<int>>;

    // The wedge x >= 0 and y >= 0.
    const Region wedge({Halfplane(0, 0, 1, 0), Halfplane(0, 1, 0, 0)});
    REQUIRE(wedge.contains(Point<int>(2, 3)));

    // Reflecting across the x axis maps the wedge to x >= 0 and y <= 0.
    const auto reflected = Transformation<int>::reflectionX() * wedge;
    CHECK(reflected.contains(Point<int>(2, -3)));
    CHECK_FALSE(reflected.contains(Point<int>(2, 3)));
    CHECK_FALSE(reflected.isBounded());
    CHECK(reflected.vertexCount() == 1);

    // A translation shifts the region; the empty region stays empty.
    const auto shifted = Transformation<int>::translation(1, 1) * wedge;
    CHECK(shifted.contains(Point<int>(1, 1)));
    CHECK_FALSE(shifted.contains(Point<int>(0, 0)));
    const Region emptyRegion({Halfplane(0, 0, 1, 0), Halfplane(1, -1, 0, -1)});
    REQUIRE(emptyRegion.isEmpty());
    CHECK((Transformation<int>::reflectionX() * emptyRegion).isEmpty());
}

TEST_CASE("The Shape wrapper forwards to the wrapped alternative") {
    const pgl::Segment<Point<int>> segment(Point<int>(0, 0), Point<int>(2, 2));
    const pgl::Shape<Point<int>> wrapped(segment);
    const auto t = Transformation<int>::translation(1, 1);

    const auto direct = t * segment;
    const auto viaShape = t * wrapped;
    CHECK(viaShape == pgl::Shape<Point<int>>(direct));
}

TEST_CASE("inverse() round-trips exactly through a Rational matrix") {
    const Transformation<int> m(2, 1, 1, 1);
    REQUIRE(m.determinant() == 1);

    const auto inv = m.inverse<Rational<int>>();
    const Point<int> p(3, 5);
    const auto roundtrip = inv * (m * p);

    CHECK(roundtrip == Point<Rational<int>>(3, 5));
}

TEST_CASE("Transformation cannot be applied to Rectangle or Disk") {
    static_assert(!canMultiply<Transformation<int>, pgl::Rectangle<Point<int>>>);
    static_assert(!canMultiply<Transformation<int>, pgl::Disk<Point<int>>>);
    // Sanity check: the helper itself does detect valid applications.
    static_assert(canMultiply<Transformation<int>, pgl::Triangle<Point<int>>>);
}
