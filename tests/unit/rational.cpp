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

TEST_CASE_TEMPLATE("Constructors from integers", Int, int32_t, int64_t, __int128_t) {
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

TEST_CASE_TEMPLATE("Constructors from double", Int, int32_t, int64_t, __int128_t) {
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

TEST_CASE("Rational numeric limits and promotion preserve rational types") {
    using SmallRational = pgl::Rational<int16_t>;
    using PromotedSmallRational = pgl::detail::promoted_number_t<SmallRational>;

    static_assert(std::is_same_v<PromotedSmallRational, pgl::Rational<int32_t>>);

    CHECK(std::numeric_limits<pgl::Rational<int>>::min() == pgl::Rational<int>(std::numeric_limits<int>::min()));
    CHECK(std::numeric_limits<pgl::Rational<int>>::max() == pgl::Rational<int>(std::numeric_limits<int>::max()));
    CHECK(std::numeric_limits<pgl::Rational<int>>::lowest() == std::numeric_limits<pgl::Rational<int>>::min());
}

TEST_CASE_TEMPLATE("Arithmetic operations", Int, int32_t, int64_t, __int128_t) {
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

























