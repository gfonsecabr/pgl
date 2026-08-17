#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstdint>
#include <random>
#include <type_traits>
#include "pgl.hpp"

// Tests for the atomic sign predicates in implementation/orientation.hpp.
//
// The bulk of this file guards those predicates against their floating-point
// filter: for coordinate types whose exact evaluation is arbitrary precision,
// the sign is first attempted in double with an error bound, and the exact
// evaluation only runs when that bound fails to prove the sign. The filter is
// therefore only correct if it agrees with the exact value on every input it
// accepts, and abstains on the inputs it cannot decide -- most importantly the
// exactly degenerate ones (cocircular, collinear, perpendicular), where an
// accepted answer would be a coin flip.

namespace {

// The sign of an exact determinant, in the same encoding inCircleSign returns.
template <class Determinant>
std::partial_ordering signOf(const Determinant& d) {
    const Determinant zero(0);
    if (d > zero) {
        return std::partial_ordering::greater;
    }
    if (d < zero) {
        return std::partial_ordering::less;
    }
    return std::partial_ordering::equivalent;
}

}  // namespace

TEST_CASE_TEMPLATE("inCircleSign matches the determinant sign on random points", Number,
                   int, int64_t, double, pgl::Rational<int64_t>, pgl::ERational) {
    using Point = pgl::Point<Number>;

    std::mt19937_64 rng(20260813);
    std::uniform_int_distribution<int> coordinate(-10000, 10000);
    auto pick = [&] {
        return Point(Number(coordinate(rng)), Number(coordinate(rng)));
    };

    for (int i = 0; i < 5000; ++i) {
        const Point a = pick();
        const Point b = pick();
        const Point c = pick();
        const Point d = pick();
        CHECK(pgl::inCircleSign(a, b, c, d) == signOf(pgl::inCircleDeterminant(a, b, c, d)));
    }
}

TEST_CASE_TEMPLATE("inCircleSign is exact on cocircular points and their neighbours", Number,
                   int, int64_t, double, pgl::Rational<int64_t>, pgl::ERational) {
    using Point = pgl::Point<Number>;

    std::mt19937_64 rng(1729);
    std::uniform_int_distribution<int> coordinate(-10000, 10000);

    for (int i = 0; i < 2000; ++i) {
        const int x1 = coordinate(rng);
        const int x2 = coordinate(rng);
        const int y1 = coordinate(rng);
        const int y2 = coordinate(rng);
        auto at = [](int x, int y) { return Point(Number(x), Number(y)); };

        const Point a = at(x1, y1);
        const Point b = at(x2, y1);
        const Point c = at(x2, y2);

        // The four corners of an axis-aligned rectangle are cocircular, so an
        // exact coordinate type reports the determinant as zero however
        // degenerate the rectangle is. A floating-point one is not owed that
        // and is not asked for it: its determinant is evaluated in long double,
        // and whether terms around 2^57 cancel at all comes down to how many
        // mantissa bits that type has to spare -- eleven where long double is
        // x87's 80-bit format, none at all where it is a second name for
        // double, as it is on MSVC.
        if constexpr (!std::is_floating_point_v<Number>) {
            CHECK(pgl::inCircleSign(a, b, c, at(x1, y2)) == std::partial_ordering::equivalent);
        }

        // Nudging the query point off that circle by a single unit is the
        // tightest non-degenerate input there is, and the sign must follow the
        // exact determinant rather than the nudge's direction, which depends on
        // the orientation of (a,b,c).
        for (const Point& nudged : {at(x1 + 1, y2), at(x1 - 1, y2), at(x1, y2 + 1), at(x1, y2 - 1)}) {
            CHECK(pgl::inCircleSign(a, b, c, nudged) ==
                  signOf(pgl::inCircleDeterminant(a, b, c, nudged)));
        }
    }
}

TEST_CASE("inCircleSign follows the orientation of the three circle points") {
    using Point = pgl::Point<pgl::ERational>;

    const Point a(0, 0);
    const Point b(4, 0);
    const Point c(4, 4);       // (a,b,c) is counterclockwise
    const Point inside(2, 2);  // the circumcircle's centre
    const Point outside(100, 100);
    const Point boundary(0, 4);

    CHECK(pgl::inCircleSign(a, b, c, inside) == std::partial_ordering::greater);
    CHECK(pgl::inCircleSign(a, b, c, outside) == std::partial_ordering::less);
    CHECK(pgl::inCircleSign(a, b, c, boundary) == std::partial_ordering::equivalent);

    // Reversing the circle points reverses the convention.
    CHECK(pgl::inCircleSign(c, b, a, inside) == std::partial_ordering::less);
    CHECK(pgl::inCircleSign(c, b, a, outside) == std::partial_ordering::greater);
    CHECK(pgl::inCircleSign(c, b, a, boundary) == std::partial_ordering::equivalent);
}

TEST_CASE("inCircleSign survives coordinates too wide for a double") {
    // 2^60 apart: the coordinates still convert to double exactly, but the
    // determinant runs to roughly 2^244 and every intermediate the filter forms
    // is rounded, so only the error bound keeps the answer honest.
    using Point = pgl::Point<pgl::ERational>;
    const int64_t big = int64_t(1) << 60;
    auto at = [](int64_t x, int64_t y) { return Point(pgl::ERational(x), pgl::ERational(y)); };

    const Point a = at(0, 0);
    const Point b = at(big, 0);
    const Point c = at(big, big);
    const Point boundary = at(0, big);

    CHECK(pgl::inCircleSign(a, b, c, boundary) == std::partial_ordering::equivalent);
    CHECK(pgl::inCircleSign(a, b, c, at(big / 2, big / 2)) == std::partial_ordering::greater);
    CHECK(pgl::inCircleSign(a, b, c, at(big, -big)) == std::partial_ordering::less);

    // One unit off the circle, at a scale where a bare double evaluation of the
    // determinant has long since lost the distinction.
    const Point justInside = at(1, big);
    CHECK(pgl::inCircleSign(a, b, c, justInside) ==
          signOf(pgl::inCircleDeterminant(a, b, c, justInside)));
}

TEST_CASE("inCircleSign handles fractional rational coordinates") {
    using Point = pgl::Point<pgl::ERational>;
    const pgl::ERational third(1, 3);
    const pgl::ERational seventh(1, 7);

    const Point a(-third, pgl::ERational(0));
    const Point b(third, pgl::ERational(0));
    const Point c(pgl::ERational(0), third);
    // The circumcircle of those three is centred at the origin with radius 1/3,
    // so a point at distance 1/7 from the centre is strictly inside it, however
    // inexact both thirds and sevenths are in binary.
    CHECK(pgl::inCircleSign(a, b, c, Point(seventh, pgl::ERational(0))) ==
          std::partial_ordering::greater);
    CHECK(pgl::inCircleSign(a, b, c, Point(pgl::ERational(0), -third)) ==
          std::partial_ordering::equivalent);
    CHECK(pgl::inCircleSign(a, b, c, Point(pgl::ERational(0), -pgl::ERational(1, 2))) ==
          std::partial_ordering::less);
}

TEST_CASE_TEMPLATE("orientationSign matches the determinant sign on random points", Number,
                   int, int64_t, double, pgl::Rational<int64_t>, pgl::ERational) {
    using Point = pgl::Point<Number>;

    std::mt19937_64 rng(20260817);
    std::uniform_int_distribution<int> coordinate(-10000, 10000);
    auto pick = [&] {
        return Point(Number(coordinate(rng)), Number(coordinate(rng)));
    };

    for (int i = 0; i < 5000; ++i) {
        const Point a = pick();
        const Point b = pick();
        const Point c = pick();
        CHECK(pgl::orientationSign(a, b, c) == signOf(pgl::orientationDeterminant(a, b, c)));
    }
}

TEST_CASE_TEMPLATE("orientationSign is exact on collinear points and their neighbours", Number,
                   int, int64_t, double, pgl::Rational<int64_t>, pgl::ERational) {
    using Point = pgl::Point<Number>;

    std::mt19937_64 rng(4177);
    std::uniform_int_distribution<int> coordinate(-10000, 10000);
    std::uniform_int_distribution<int> step(-8, 8);

    for (int i = 0; i < 2000; ++i) {
        auto at = [](int x, int y) { return Point(Number(x), Number(y)); };
        const int ax = coordinate(rng);
        const int ay = coordinate(rng);
        const int dx = step(rng);
        const int dy = step(rng);
        const int k = step(rng);

        // Three points on one line, however short the step or however far along
        // it the third one sits: the determinant is exactly zero, so the filter
        // has to abstain and let the exact evaluation say so.
        const Point a = at(ax, ay);
        const Point b = at(ax + dx, ay + dy);
        const Point c = at(ax + k * dx, ay + k * dy);
        CHECK(pgl::orientationSign(a, b, c) == std::partial_ordering::equivalent);
        CHECK(pgl::collinear(a, b, c));

        // One unit off that line is the tightest non-degenerate input there is.
        for (const Point& nudged : {at(ax + k * dx + 1, ay + k * dy),
                                    at(ax + k * dx - 1, ay + k * dy),
                                    at(ax + k * dx, ay + k * dy + 1),
                                    at(ax + k * dx, ay + k * dy - 1)}) {
            CHECK(pgl::orientationSign(a, b, nudged) ==
                  signOf(pgl::orientationDeterminant(a, b, nudged)));
        }
    }
}

TEST_CASE("orientationSign survives coordinates too wide for a double") {
    // 2^60 apart: the coordinates still convert to double exactly, but the
    // products the filter forms run past what a double holds, so only the error
    // bound keeps the answer honest.
    using Point = pgl::Point<pgl::ERational>;
    const int64_t big = int64_t(1) << 60;
    auto at = [](int64_t x, int64_t y) { return Point(pgl::ERational(x), pgl::ERational(y)); };

    const Point a = at(0, 0);
    const Point b = at(big, big);

    CHECK(pgl::orientationSign(a, b, at(big / 2, big / 2)) == std::partial_ordering::equivalent);
    CHECK(pgl::orientationSign(a, b, at(0, big)) == std::partial_ordering::greater);
    CHECK(pgl::orientationSign(a, b, at(big, 0)) == std::partial_ordering::less);

    // One unit off the line, at a scale where a bare double evaluation has long
    // since lost the distinction.
    const Point justLeft = at(big / 2, big / 2 + 1);
    CHECK(pgl::orientationSign(a, b, justLeft) ==
          signOf(pgl::orientationDeterminant(a, b, justLeft)));
}

TEST_CASE("orientationSign handles fractional rational coordinates") {
    using Point = pgl::Point<pgl::ERational>;
    const pgl::ERational third(1, 3);
    const pgl::ERational seventh(1, 7);
    const pgl::ERational zero(0);

    // The line through (0,0) and (1/3, 1/3) is the diagonal, however inexact
    // thirds and sevenths are in binary.
    const Point a(zero, zero);
    const Point b(third, third);

    CHECK(pgl::orientationSign(a, b, Point(seventh, seventh)) == std::partial_ordering::equivalent);
    CHECK(pgl::orientationSign(a, b, Point(seventh, third)) == std::partial_ordering::greater);
    CHECK(pgl::orientationSign(a, b, Point(third, seventh)) == std::partial_ordering::less);
}

TEST_CASE_TEMPLATE("crossSign and dotSign match their exact evaluation on random vectors", Number,
                   int, int64_t, double, pgl::Rational<int64_t>, pgl::ERational) {
    using Point = pgl::Point<Number>;

    std::mt19937_64 rng(31337);
    std::uniform_int_distribution<int> coordinate(-10000, 10000);
    auto pick = [&] {
        return Point(Number(coordinate(rng)), Number(coordinate(rng)));
    };
    const Point origin(Number(0), Number(0));

    for (int i = 0; i < 5000; ++i) {
        const Point u = pick();
        const Point v = pick();
        CHECK(pgl::crossSign(u, v) == signOf(pgl::orientationDeterminant(origin, u, v)));
        CHECK(pgl::dotSign(u, v) == signOf(u.x() * v.x() + u.y() * v.y()));

        // The four-point forms are the same signs on the difference vectors,
        // reached without the caller ever forming them.
        const Point a = pick();
        const Point b = pick();
        const Point p = pick();
        const Point q = pick();
        const Point ab(b.x() - a.x(), b.y() - a.y());
        const Point pq(q.x() - p.x(), q.y() - p.y());
        CHECK(pgl::crossSign(a, b, p, q) == pgl::crossSign(ab, pq));
        CHECK(pgl::crossSign(a, b, p, q) == signOf(pgl::orientationDeterminant(origin, ab, pq)));
        CHECK(pgl::dotSign(a, b, p, q) == pgl::dotSign(ab, pq));
        CHECK(pgl::dotSign(a, b, p, q) == signOf(ab.x() * pq.x() + ab.y() * pq.y()));
    }
}

TEST_CASE_TEMPLATE("crossSign and dotSign are exact on parallel and perpendicular vectors", Number,
                   int, int64_t, double, pgl::Rational<int64_t>, pgl::ERational) {
    using Point = pgl::Point<Number>;

    std::mt19937_64 rng(2718);
    std::uniform_int_distribution<int> coordinate(-1000, 1000);
    std::uniform_int_distribution<int> scale(-8, 8);

    for (int i = 0; i < 2000; ++i) {
        auto at = [](int x, int y) { return Point(Number(x), Number(y)); };
        const int x = coordinate(rng);
        const int y = coordinate(rng);
        const int k = scale(rng);

        const Point u = at(x, y);
        // Exactly parallel (or anti-parallel, or zero): the cross product
        // vanishes, and only the exact evaluation may say so.
        CHECK(pgl::crossSign(u, at(k * x, k * y)) == std::partial_ordering::equivalent);
        CHECK(pgl::sameDirection(at(0, 0), u, at(0, 0), at(k * x, k * y)));
        // Exactly perpendicular: so does the dot product.
        CHECK(pgl::dotSign(u, at(-k * y, k * x)) == std::partial_ordering::equivalent);

        // One unit off either degeneracy has to follow the exact sign.
        const Point nudgedCross = at(k * x + 1, k * y);
        const Point nudgedDot = at(-k * y + 1, k * x);
        CHECK(pgl::crossSign(u, nudgedCross) ==
              signOf(pgl::orientationDeterminant(at(0, 0), u, nudgedCross)));
        CHECK(pgl::dotSign(u, nudgedDot) ==
              signOf(u.x() * nudgedDot.x() + u.y() * nudgedDot.y()));
    }
}

TEST_CASE("crossSign and dotSign survive wide and fractional rational coordinates") {
    using Point = pgl::Point<pgl::ERational>;
    const int64_t big = int64_t(1) << 60;
    auto at = [](int64_t x, int64_t y) { return Point(pgl::ERational(x), pgl::ERational(y)); };

    CHECK(pgl::crossSign(at(big, big), at(big / 2, big / 2)) == std::partial_ordering::equivalent);
    CHECK(pgl::crossSign(at(big, big), at(big / 2, big / 2 + 1)) == std::partial_ordering::greater);
    CHECK(pgl::dotSign(at(big, big), at(-big, big)) == std::partial_ordering::equivalent);
    CHECK(pgl::dotSign(at(big, big), at(-big, big + 1)) == std::partial_ordering::greater);

    const pgl::ERational third(1, 3);
    const pgl::ERational seventh(1, 7);
    const Point u(third, third);
    CHECK(pgl::crossSign(u, Point(seventh, seventh)) == std::partial_ordering::equivalent);
    CHECK(pgl::crossSign(u, Point(seventh, third)) == std::partial_ordering::greater);
    CHECK(pgl::dotSign(u, Point(-seventh, seventh)) == std::partial_ordering::equivalent);
    CHECK(pgl::dotSign(u, Point(-seventh, third)) == std::partial_ordering::greater);
}
