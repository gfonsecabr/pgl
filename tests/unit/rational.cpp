#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cmath>
#include <set>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <cstdint>
#include <limits>
#include "pgl.hpp"

TEST_CASE_TEMPLATE("Constructors from integers", Int, int32_t, int64_t, pgl::int128, pgl::BigInt) {
    {
        pgl::Rational<Int> a(13);
        CHECK(a.numerator() == 13);
        CHECK(a.denominator() == 1);
    }

    {
        pgl::Rational<Int> a(-8);
        CHECK(a.numerator() == -8);
        CHECK(a.denominator() == 1);
    }

    {
        pgl::Rational<Int> a(1,-3);
        CHECK(a.numerator() == -1);
        CHECK(a.denominator() == 3);
    }

    {
        pgl::Rational<Int> a(2,4);
        CHECK(a.numerator() == 1);
        CHECK(a.denominator() == 2);
    }

    {
        pgl::Rational<Int> a(-2,4);
        CHECK(a.numerator() == -1);
        CHECK(a.denominator() == 2);
    }
}

TEST_CASE_TEMPLATE("isInteger recognizes normalized and deferred whole values", Int,
                   int32_t, int64_t, pgl::int128, pgl::BigInt) {
    using R = pgl::Rational<Int>;
    CHECK(R(7).isInteger());
    CHECK(R(14, 2).isInteger());
    CHECK(R(-21, 3).isInteger());
    CHECK(R(0, 9).isInteger());
    CHECK_FALSE(R(1, 2).isInteger());
    CHECK_FALSE(R(-14, 3).isInteger());
}

TEST_CASE("Constructor from a BigInt numerator and truncating conversion back") {
    {
        // BigInt is not extended_integral, so this exercises the
        // own-numerator-type constructor.
        pgl::Rational<pgl::BigInt> a(pgl::BigInt(13));
        CHECK(a.numerator() == 13);
        CHECK(a.denominator() == 1);
    }
    {
        // CTAD from a BigInt numerator deduces Rational<BigInt>.
        pgl::Rational a(pgl::BigInt(-8));
        static_assert(std::is_same_v<decltype(a), pgl::Rational<pgl::BigInt>>);
        CHECK(a.numerator() == -8);
        CHECK(a.denominator() == 1);
    }
    {
        // Conversion to the numerator type truncates toward zero, matching
        // the built-in operator int / operator int64_t conversions.
        pgl::Rational<pgl::BigInt> a(pgl::BigInt(7), pgl::BigInt(2));
        CHECK(static_cast<pgl::BigInt>(a) == 3);
        pgl::Rational<pgl::BigInt> b(pgl::BigInt(-7), pgl::BigInt(2));
        CHECK(static_cast<pgl::BigInt>(b) == -3);
    }
    {
        // A bare floating-point argument must keep deducing the default
        // Rational<int64_t>, not Rational<double>.
        pgl::Rational c(5.25);
        static_assert(std::is_same_v<decltype(c), pgl::Rational<int64_t>>);
        CHECK(c.numerator() == 21);
        CHECK(c.denominator() == 4);
    }
}

TEST_CASE("Narrowing conversions divide before they narrow") {
    // Normalization is deferred, so a value as ordinary as 5 can be carried as
    // a ratio of two numbers far past `int`. The conversion has to divide in the
    // stored type and narrow the quotient; narrowing the parts first and
    // dividing those turns the answer into noise, and did -- the `int` of the
    // value below came back as -1.
    //
    // The geometry that found it: an arrangement's vertices are exactly such
    // deferred fractions, so `region.regularizedIntersection<int>(halfplane)` and
    // `region.difference<int>(halfplane)` reported vertices that were nowhere
    // near the crossings.
    SUBCASE("over BigInt") {
        using R = pgl::Rational<pgl::BigInt>;
        const R five = R(pgl::BigInt(1), pgl::BigInt(3000000000LL)) *
                       R(pgl::BigInt(15000000000LL), pgl::BigInt(1));
        REQUIRE(five == 5);
        CHECK(static_cast<int>(five) == 5);
        CHECK(static_cast<int64_t>(five) == 5);
        CHECK(static_cast<pgl::BigInt>(five) == 5);
        CHECK(static_cast<double>(five) == doctest::Approx(5.0));

        // and it still truncates toward zero, not away from it or downward.
        const R negative = R(pgl::BigInt(-7), pgl::BigInt(3000000000LL)) *
                           R(pgl::BigInt(3000000000LL), pgl::BigInt(2));
        REQUIRE(negative == pgl::Rational<pgl::BigInt>(pgl::BigInt(-7), pgl::BigInt(2)));
        CHECK(static_cast<int>(negative) == -3);
        CHECK(static_cast<int64_t>(negative) == -3);
    }

    SUBCASE("over a fixed-width Int, where the parts outrun int but not the store") {
        using R = pgl::Rational<int64_t>;
        const R five = R(1, 3000000000LL) * R(15000000000LL, 1);
        REQUIRE(five == 5);
        CHECK(static_cast<int>(five) == 5);
        CHECK(static_cast<int64_t>(five) == 5);
    }
}

TEST_CASE_TEMPLATE("Constructors from double", Int, int32_t, int64_t, pgl::int128) {
    {
        pgl::Rational<Int> a(0.0);
        CHECK(a.numerator() == 0);
        CHECK(a.denominator() == 1);
    }
    {
        pgl::Rational<Int> a(.5);
        CHECK(a.numerator() == 1);
        CHECK(a.denominator() == 2);
    }
    {
        pgl::Rational<Int> a(-.5);
        CHECK(a.numerator() == -1);
        CHECK(a.denominator() == 2);
    }
    {
        pgl::Rational<Int> a(1.0/1024.0);
        CHECK(a.numerator() == 1);
        CHECK(a.denominator() == 1024);
    }
    {
        pgl::Rational<Int> a(512.0);
        CHECK(a.numerator() == 512.0);
        CHECK(a.denominator() == 1);
    }
    {
        pgl::Rational<Int> a(1.0/3.0);
        CHECK((double)a.numerator()/a.denominator() == doctest::Approx(1.0/3.0).epsilon(0.001));
    }
    {
        pgl::Rational<Int> a(13.0/137.0);
        CHECK((double)a.numerator()/a.denominator() == doctest::Approx(13.0/137.0).epsilon(0.001));
        CHECK(static_cast<double>(a) == doctest::Approx(13.0/137.0).epsilon(0.001));
    }
}

TEST_CASE_TEMPLATE("Exact comparison against floating point", Int,
                   int32_t, int64_t, pgl::int128, pgl::BigInt) {
    using R = pgl::Rational<Int>;

    // Dyadic values are represented exactly by both sides.
    CHECK(R(1, 2) == 0.5);
    CHECK(R(-3, 4) == -0.75);
    CHECK(R(5, 1) == 5.0);
    CHECK_FALSE(R(1, 2) == 0.4);

    // Ordering is exact and the reversed (float-on-the-left) forms work too.
    CHECK(R(1, 2) > 0.4);
    CHECK(R(1, 2) < 0.6);
    CHECK(0.4 < R(1, 2));
    CHECK(0.6 > R(1, 2));
    CHECK(R(-3, 4) < -0.7);
    CHECK(R(-3, 4) > -0.8);

    // 1/3 is not any double: never equal, but consistently ordered against the
    // nearest double, with no rounding of the rational side.
    const double third = 1.0 / 3.0;
    CHECK_FALSE(R(1, 3) == third);
    CHECK((R(1, 3) < third) != (R(1, 3) > third));

    // Exact well beyond a double's integer precision (no overflow, no rounding).
    CHECK(R(1, 3) < 1e300);
    CHECK(R(1, 3) > -1e300);

    // NaN is unordered; infinities order as expected.
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    CHECK_FALSE(R(1, 2) == nan);
    CHECK_FALSE(R(1, 2) < nan);
    CHECK_FALSE(R(1, 2) > nan);
    CHECK_FALSE(R(1, 2) >= nan);
    CHECK(R(1, 2) < inf);
    CHECK(R(1, 2) > -inf);

    // The float->Rational conversion is explicit, so a bare float never slips
    // in as a Rational; this exercises that comparison is the float overload.
    CHECK(R(7, 2) == 3.5);
}

TEST_CASE_TEMPLATE("Exact comparison against integers", Int,
                   int32_t, int64_t, pgl::int128, pgl::BigInt) {
    using R = pgl::Rational<Int>;

    // 0, 1 and -1 are answered from the numerator and the denominator alone.
    CHECK(R(0, 5) == 0);
    CHECK(R(1, 3) > 0);
    CHECK(R(-1, 3) < 0);
    CHECK(R(3, 3) == 1);
    CHECK(R(2, 3) < 1);
    CHECK(R(4, 3) > 1);
    CHECK(R(-3, 3) == -1);
    CHECK(R(-4, 3) < -1);
    CHECK(R(-2, 3) > -1);

    // A fraction is never equal to an integer it merely straddles.
    CHECK_FALSE(R(1, 2) == 0);
    CHECK_FALSE(R(1, 2) == 1);
    CHECK_FALSE(R(-1, 2) == -1);

    // Integral values, and fractions that reduce to one.
    CHECK(R(37) == 37);
    CHECK(R(37) < 38);
    CHECK(R(-37) > -38);
    CHECK(R(74, 2) == 37);
    CHECK(R(75, 2) > 37);
    CHECK(R(-75, 2) < -37);

    // The reversed forms, with the integer on the left.
    CHECK(0 == R(0, 5));
    CHECK(0 != R(1, 5));
    CHECK(1 > R(2, 3));
    CHECK(-1 < R(-2, 3));
    CHECK(37 == R(74, 2));
    CHECK(38 > R(74, 2));

    // Fractions whose reduction is still deferred compare by value, on the fast
    // paths as well as the general one.
    CHECK(R(2, 3) * R(3, 2) == 1);
    CHECK(R(2, 3) * R(-3, 2) == -1);
    CHECK(R(4, 2) * R(6, 3) == 4);
    const R sixth = R(1, 3) * R(1, 2);
    CHECK(sixth > 0);
    CHECK(sixth < 1);
    CHECK_FALSE(sixth == 0);

    // The argument keeps its own type: a wider integer, and Int itself, are
    // compared without being turned into a Rational first.
    CHECK(R(74, 2) == int64_t(37));
    CHECK(R(74, 2) < int64_t(38));
    CHECK(R(74, 2) == Int(37));
    CHECK(R(74, 2) < Int(38));
    CHECK(Int(37) == R(74, 2));
}

TEST_CASE("Rational numeric limits and promotion preserve rational types") {
    using SmallRational = pgl::Rational<int16_t>;
    using PromotedSmallRational = pgl::detail::promoted_number_t<SmallRational>;

    // Rational is never promoted: the storage type is preserved.
    static_assert(std::is_same_v<PromotedSmallRational, pgl::Rational<int16_t>>);

    CHECK(std::numeric_limits<pgl::Rational<int>>::min() == pgl::Rational<int>(std::numeric_limits<int>::min()));
    CHECK(std::numeric_limits<pgl::Rational<int>>::max() == pgl::Rational<int>(std::numeric_limits<int>::max()));
    CHECK(std::numeric_limits<pgl::Rational<int>>::lowest() == std::numeric_limits<pgl::Rational<int>>::min());
}

TEST_CASE_TEMPLATE("Stream input parses num/den and plain integers", Int,
                   int32_t, int64_t, pgl::int128, pgl::BigInt) {
    using R = pgl::Rational<Int>;

    {
        std::istringstream is("3/4");
        R r;
        is >> r;
        CHECK_FALSE(is.fail());
        CHECK(r == R(3, 4));
    }
    {
        std::istringstream is("-5/2");
        R r;
        is >> r;
        CHECK_FALSE(is.fail());
        CHECK(r == R(-5, 2));
    }
    {
        std::istringstream is("7");
        R r;
        is >> r;
        CHECK_FALSE(is.fail());
        CHECK(r == R(7));
    }
    {
        // Trailing content after the parsed rational is left in the stream.
        std::istringstream is("3/4 rest");
        R r;
        std::string rest;
        is >> r >> rest;
        CHECK_FALSE(is.fail());
        CHECK(r == R(3, 4));
        CHECK(rest == "rest");
    }
}

TEST_CASE_TEMPLATE("Arithmetic operations", Int, int32_t, int64_t, pgl::int128) {
    {
        pgl::Rational<Int> a(13);
        pgl::Rational<Int> b(7);
        CHECK((a+b).numerator() == 20);
        CHECK((a+b).denominator() == 1);
        CHECK((a-b).numerator() == 6);
        CHECK((a-b).denominator() == 1);
        CHECK((b-a).numerator() == -6);
        CHECK((b-a).denominator() == 1);
        CHECK((a*b).numerator() == 91);
        CHECK((a*b).denominator() == 1);
    }
    {
        pgl::Rational<Int> a(5,12);
        pgl::Rational<Int> b(4,12);
        CHECK((a+b).numerator() == 3);
        CHECK((a+b).denominator() == 4);
        CHECK((a-b).numerator() == 1);
        CHECK((a-b).denominator() == 12);
        CHECK((b-a).numerator() == -1);
        CHECK((b-a).denominator() == 12);
        CHECK((a*b).numerator() == 5);
        CHECK((a*b).denominator() == 36);
    }
}

























TEST_CASE_TEMPLATE("simplify and simplified reduce to lowest terms", Int,
                   int32_t, int64_t, pgl::int128, pgl::BigInt) {
    // A fraction built from parts is stored unreduced: the normalization is
    // deferred. Both methods produce the reduced form; only simplify() keeps it.
    {
        const pgl::Rational<Int> a(6, 8);
        const pgl::Rational<Int> b = a.simplified();
        CHECK(b.numerator() == 3);
        CHECK(b.denominator() == 4);
        CHECK(b == a);          // simplifying never changes the value
        CHECK(a.numerator() == 3);
        CHECK(a.denominator() == 4);

        pgl::Rational<Int> c(6, 8);
        c.simplify();
        CHECK(c.numerator() == 3);
        CHECK(c.denominator() == 4);
        CHECK(c == a);
    }
    {   // negative numerators keep the sign on top, denominator stays positive
        pgl::Rational<Int> a(-6, 8);
        CHECK(a.simplified().numerator() == -3);
        CHECK(a.simplified().denominator() == 4);
        a.simplify();
        CHECK(a.numerator() == -3);
        CHECK(a.denominator() == 4);

        pgl::Rational<Int> b(6, Int(-8));
        b.simplify();
        CHECK(b.numerator() == -3);
        CHECK(b.denominator() == 4);
    }
    {   // zero normalizes to 0/1 whatever denominator it was written over
        pgl::Rational<Int> a(0, 7);
        CHECK(a.simplified().numerator() == 0);
        CHECK(a.simplified().denominator() == 1);
        a.simplify();
        CHECK(a.numerator() == 0);
        CHECK(a.denominator() == 1);
    }
    {   // already-reduced and integer values are unchanged, and both are idempotent
        pgl::Rational<Int> a(3, 4);
        a.simplify();
        a.simplify();
        CHECK(a.numerator() == 3);
        CHECK(a.denominator() == 4);
        CHECK(a.simplified().simplified() == a);

        pgl::Rational<Int> b(13);
        b.simplify();
        CHECK(b.numerator() == 13);
        CHECK(b.denominator() == 1);
    }
    {   // an arithmetic result carries its reduction deferred; simplifying it
        // agrees with what the accessors report either way
        pgl::Rational<Int> a(5, 12);
        pgl::Rational<Int> b(4, 12);
        const pgl::Rational<Int> sum = a + b;
        CHECK(sum.simplified().numerator() == 3);
        CHECK(sum.simplified().denominator() == 4);
        CHECK(sum.simplified() == sum);
    }
    {   // hashing agrees across representations of the same value, which is what
        // std::hash<Rational> uses simplified() to guarantee
        const std::hash<pgl::Rational<Int>> hasher;
        CHECK(hasher(pgl::Rational<Int>(6, 8)) == hasher(pgl::Rational<Int>(3, 4)));
        CHECK(hasher(pgl::Rational<Int>(0, 5)) == hasher(pgl::Rational<Int>(0, 1)));
        pgl::Rational<Int> reduced(6, 8);
        reduced.simplify();
        CHECK(hasher(reduced) == hasher(pgl::Rational<Int>(6, 8)));
    }
}

TEST_CASE("simplify and simplified are usable in constant expressions") {
    // simplified() is const, so it works on a constexpr object; simplify() needs
    // a non-const object, which a constexpr function can create locally.
    constexpr pgl::Rational<int> value(6, 8);
    static_assert(value.simplified().numerator() == 3);
    static_assert(value.simplified().denominator() == 4);

    constexpr auto reduce = [](int n, int d) {
        pgl::Rational<int> r(n, d);
        r.simplify();
        return r.numerator() * 100 + r.denominator();
    };
    static_assert(reduce(6, 8) == 304);
    static_assert(reduce(0, 7) == 1);
    CHECK(reduce(6, 8) == 304);
}

TEST_CASE_TEMPLATE("simplifyIfLarge and simplifiedIfLarge preserve the value", Int,
                   int32_t, int64_t, pgl::int128, pgl::BigInt) {
    // These two are performance hints, not conversions: whether the reduction is
    // stored is invisible from outside, because every accessor reports the
    // reduced form either way. So what is testable is that the value never moves
    // — for a fraction narrow enough to be left alone, and for one wide enough to
    // be reduced.
    {   // narrow: below any width at which arithmetic would reduce it
        pgl::Rational<Int> a(6, 8);
        const pgl::Rational<Int> before = a;
        a.simplifyIfLarge();
        CHECK(a == before);
        CHECK(a.numerator() == 3);
        CHECK(a.denominator() == 4);
        CHECK(a.simplifiedIfLarge() == before);
    }
    {   // wide: built well past the half-width bound so the reduction fires.
        // Doubling rather than shifting, since pgl::BigInt has no operator<<.
        const int bits = pgl::detail::numeric_limits<Int>::digits > 0
                             ? pgl::detail::numeric_limits<Int>::digits / 2 + 2
                             : 66;
        Int big(1);
        for (int i = 0; i < bits; ++i) {
            big = big * Int(2);
        }
        pgl::Rational<Int> a(big * 6, big * 8);
        const pgl::Rational<Int> before = a;
        a.simplifyIfLarge();
        CHECK(a == before);
        CHECK(a.numerator() == 3);
        CHECK(a.denominator() == 4);
        CHECK(a.simplifiedIfLarge() == before);
        CHECK(a.simplifiedIfLarge().numerator() == 3);
    }
    {   // zero, integers and already-reduced values are left alone and stay equal
        pgl::Rational<Int> z(0, 7), i(13), r(3, 4);
        z.simplifyIfLarge();
        i.simplifyIfLarge();
        r.simplifyIfLarge();
        CHECK(z == pgl::Rational<Int>(0));
        CHECK(i == pgl::Rational<Int>(13));
        CHECK(r == pgl::Rational<Int>(3, 4));
        CHECK(r.denominator() == 4);
    }
    {   // agrees with simplify()/simplified() on the value, whichever path it takes
        pgl::Rational<Int> a(5, 12), b(5, 12);
        a.simplify();
        b.simplifyIfLarge();
        CHECK(a == b);
        CHECK(a.simplified() == b.simplifiedIfLarge());
    }
}

TEST_CASE("simplifyIfLarge is usable in constant expressions") {
    constexpr pgl::Rational<int> value(6, 8);
    static_assert(value.simplifiedIfLarge() == value);
    constexpr auto viaHint = [](int n, int d) {
        pgl::Rational<int> r(n, d);
        r.simplifyIfLarge();
        return r.numerator() * 100 + r.denominator();
    };
    static_assert(viaHint(6, 8) == 304);
    CHECK(viaHint(6, 8) == 304);
}

TEST_CASE("Floating-point construction keeps the integer part when no fraction bit fits") {
    // Bounded storage keeps half its width, less a few bits, for the fraction;
    // a value whose integer part alone needs more is truncated to that part.
    CHECK(pgl::Rational<int64_t>(1e9) == 1000000000);
    CHECK(pgl::Rational<int64_t>(-123456789012.0) == -123456789012LL);
    CHECK(pgl::Rational<int64_t>(200000000.5) == 200000000);   // toward zero
    CHECK(pgl::Rational<int>(3000.0) == 3000);
    CHECK(pgl::Rational<int>(-3000.75) == -3000);
    // Below the bound the fraction is still kept.
    CHECK(pgl::Rational<int64_t>(100000.5) == pgl::Rational<int64_t>(200001, 2));
    // The integer part has to fit the storage type, and the value has to be a number.
    CHECK_THROWS_AS(pgl::Rational<int64_t>(1e19), std::overflow_error);
    CHECK_THROWS_AS(pgl::Rational<int>(3e9), std::overflow_error);
    const double infinite = std::numeric_limits<double>::infinity();
    const double notANumber = std::numeric_limits<double>::quiet_NaN();
    CHECK_THROWS_AS((void)pgl::Rational<int64_t>(infinite), std::domain_error);
    CHECK_THROWS_AS((void)pgl::Rational<int64_t>(notANumber), std::domain_error);
    // Unbounded storage holds every integer part exactly.
    CHECK(pgl::ERational(1e30) == pgl::BigInt(1e30));
    CHECK(pgl::ERational(1e30).denominator() == 1);
    // The same path serves a shape conversion.
    const pgl::Segment<pgl::Point<double>> s({0.5, 1.0}, {200000000.0, 3.0});
    const pgl::Segment<pgl::Point<pgl::Rational<int64_t>>> r(s);
    CHECK(r.max().x() == 200000000);
    CHECK(r.min().x() == pgl::Rational<int64_t>(1, 2));
}
