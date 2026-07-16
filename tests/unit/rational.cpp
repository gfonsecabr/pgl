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

























