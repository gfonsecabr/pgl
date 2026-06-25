#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cmath>
#include <cstdint>
#include <functional>
#include <random>
#include <sstream>
#include <string>
#include <unordered_set>

#include "pgl.hpp"

using pgl::BigInt;

namespace {

std::string str(const BigInt& b) {
    std::ostringstream os;
    os << b;
    return os.str();
}

BigInt factorial(int n) {
    BigInt f(1);
    for (int i = 2; i <= n; ++i) {
        f *= BigInt(i);
    }
    return f;
}

BigInt power(BigInt base, int exp) {
    BigInt r(1);
    for (int i = 0; i < exp; ++i) {
        r *= base;
    }
    return r;
}

}  // namespace

TEST_CASE("Default construction and inspectors") {
    BigInt zero;
    CHECK(zero.isZero());
    CHECK(zero.sign() == 0);
    CHECK_FALSE(zero.isNegative());
    CHECK(zero.fitsInt128());
    CHECK(str(zero) == "0");

    BigInt pos(42);
    CHECK_FALSE(pos.isZero());
    CHECK(pos.sign() == 1);
    CHECK(str(pos) == "42");

    BigInt neg(-42);
    CHECK(neg.sign() == -1);
    CHECK(neg.isNegative());
    CHECK(str(neg) == "-42");
    CHECK(neg.abs() == pos);
}

TEST_CASE("Construction from various integer types and conversions") {
    CHECK(str(BigInt(static_cast<int8_t>(-7))) == "-7");
    CHECK(str(BigInt(static_cast<int64_t>(int64_t(9000000000000000000)))) == "9000000000000000000");
    CHECK(static_cast<int>(BigInt(123456)) == 123456);
    CHECK(static_cast<int64_t>(BigInt(-int64_t(987654321))) == -int64_t(987654321));
    CHECK(static_cast<double>(BigInt(1000)) == doctest::Approx(1000.0));
    CHECK(static_cast<bool>(BigInt(0)) == false);
    CHECK(static_cast<bool>(BigInt(5)) == true);
}

TEST_CASE("Construction from floating point truncates toward zero") {
    // Fractions are dropped, not rounded; the sign follows the value.
    CHECK(str(BigInt(0.0)) == "0");
    CHECK(str(BigInt(-0.0)) == "0");
    CHECK(str(BigInt(3.9)) == "3");
    CHECK(str(BigInt(-3.9)) == "-3");
    CHECK(str(BigInt(3.0)) == "3");
    CHECK(str(BigInt(-3.0)) == "-3");

    // |value| < 1 truncates to zero regardless of sign.
    CHECK(str(BigInt(0.5)) == "0");
    CHECK(str(BigInt(-0.9999)) == "0");

    // Exact integer-valued doubles round-trip across the small/limb boundary.
    CHECK(str(BigInt(123456789.0)) == "123456789");
    CHECK(str(BigInt(1e15)) == "1000000000000000");
    CHECK(str(BigInt(9007199254740992.0)) == "9007199254740992");   // 2^53
    CHECK(static_cast<int64_t>(BigInt(-1234567890123.0)) == -int64_t(1234567890123));

    // float and long double work through the same constrained constructor.
    CHECK(str(BigInt(2.5f)) == "2");
    CHECK(str(BigInt(-2.5f)) == "-2");
    CHECK(str(BigInt(123.75L)) == "123");
}

TEST_CASE("Construction from floating point keeps the integer part exact for huge exponents") {
    // The fractional mantissa is exact, so large powers of two must come out
    // bit-for-bit, far beyond what a double can represent as an integer string.
    CHECK(BigInt(std::ldexp(1.0, 100)) == power(BigInt(2), 100));
    CHECK(BigInt(std::ldexp(1.0L, 200)) == power(BigInt(2), 200));
    CHECK(BigInt(std::ldexp(1.0, 1023)) == power(BigInt(2), 1023));
    CHECK(BigInt(-std::ldexp(1.0L, 200)) == -power(BigInt(2), 200));

    CHECK(str(BigInt(std::ldexp(1.0L, 200)))
          == "1606938044258990275541962092341162602522202993782792835301376");

    // A value carrying both a large exponent and several significant bits.
    const double v = std::ldexp(3.0, 80);   // 3 * 2^80, exact in a double
    CHECK(BigInt(v) == power(BigInt(2), 80) * BigInt(3));

    // Round-trips: integer -> double -> BigInt for values within 2^53.
    BigInt orig(987654321012345LL);
    CHECK(BigInt(static_cast<double>(orig)) == orig);
}

TEST_CASE("Equality with floating point requires a whole number") {
    BigInt three(3), nthree(-3), zero(0);

    CHECK(three == 3.0);
    CHECK(3.0 == three);          // reversed form via C++20 rewriting
    CHECK(nthree == -3.0);
    CHECK(zero == 0.0);
    CHECK(zero == -0.0);

    // A fractional value is never equal to an integer, even when truncation
    // would land on it.
    CHECK(three != 3.5);
    CHECK(three != 2.9999);
    CHECK_FALSE(3.5 == three);
    CHECK_FALSE(nthree == -3.5);

    // Non-finite values are never equal.
    CHECK(three != std::numeric_limits<double>::infinity());
    CHECK(three != std::numeric_limits<double>::quiet_NaN());

    // Exact for magnitudes far beyond a double's integer precision.
    const double big = std::ldexp(1.0, 1023);   // 2^1023, exactly representable
    CHECK(BigInt(big) == big);
    CHECK((BigInt(big) + BigInt(1)) != big);

    // float and long double overloads, plus the integer literal path is intact.
    CHECK(three == 3.0f);
    CHECK(three == 3.0L);
    CHECK(three == 3);
}

TEST_CASE("Ordering against floating point splits at the truncation point") {
    BigInt three(3), nthree(-3), zero(0);

    // Positive fraction: the integer is the smaller value.
    CHECK(three < 3.5);
    CHECK(three <= 3.0);
    CHECK(three >= 3.0);
    CHECK_FALSE(three < 3.0);
    CHECK(three > 2.5);
    CHECK(3.5 > three);
    CHECK(2.5 < three);

    // Negative fraction: trunc(-3.5) == -3, so BigInt(-3) is the larger value.
    CHECK(nthree > -3.5);
    CHECK(-3.5 < nthree);
    CHECK(nthree < -2.5);

    // Around zero the sign of the fraction decides.
    CHECK(zero < 0.5);
    CHECK(zero > -0.5);
    CHECK(zero > -0.0001);

    // Exact comparison straddling a huge power of two.
    const double big = std::ldexp(1.0, 1023);
    CHECK((BigInt(big) - BigInt(1)) < big);
    CHECK((BigInt(big) + BigInt(1)) > big);
    CHECK(big > (BigInt(big) - BigInt(1)));

    // A very large BigInt dominates a modest double regardless of sign.
    BigInt huge = power(BigInt(10), 200);
    CHECK(huge > 1e9);
    CHECK(1e9 < huge);
    CHECK(-huge < -1e9);

    // Infinities bound every finite value; NaN is unordered.
    const double inf = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    CHECK(three < inf);
    CHECK(three > -inf);
    CHECK(inf > three);
    CHECK_FALSE(three < nan);
    CHECK_FALSE(three > nan);
    CHECK((three <=> nan) == std::partial_ordering::unordered);
}

TEST_CASE("Small arithmetic matches a 64-bit reference") {
    std::mt19937_64 rng(12345);
    std::uniform_int_distribution<int64_t> dist(-int64_t(1000000000), int64_t(1000000000));

    for (int iter = 0; iter < 5000; ++iter) {
        const int64_t a = dist(rng);
        const int64_t b = dist(rng);

        CHECK(str(BigInt(a) + BigInt(b)) == std::to_string(a + b));
        CHECK(str(BigInt(a) - BigInt(b)) == std::to_string(a - b));
        CHECK(str(BigInt(a) * BigInt(b)) == std::to_string(a * b));
        if (b != 0) {
            CHECK(str(BigInt(a) / BigInt(b)) == std::to_string(a / b));
            CHECK(str(BigInt(a) % BigInt(b)) == std::to_string(a % b));
        }

        const auto expected = a <=> b;
        const auto got = BigInt(a) <=> BigInt(b);
        CHECK((got == expected));
        CHECK((BigInt(a) == BigInt(b)) == (a == b));
    }
}

TEST_CASE("Magnitude overflow crosses into the limb representation") {
    const std::string int128_max = "170141183460469231731687303715884105727";
    BigInt big_max(std::numeric_limits<pgl::int128>::max());
    CHECK(str(big_max) == int128_max);
    CHECK(big_max.fitsInt128());

    BigInt two_127 = big_max + BigInt(1);   // 2^127, no longer fits in int128
    CHECK(str(two_127) == "170141183460469231731687303715884105728");
    CHECK_FALSE(two_127.fitsInt128());

    BigInt two_128 = two_127 + two_127;
    CHECK(str(two_128) == "340282366920938463463374607431768211456");

    // Collapse back into the small representation.
    BigInt back = two_128 - two_127 - two_127;
    CHECK(back.isZero());
    CHECK(back.fitsInt128());
}

TEST_CASE("Large multiplication, division, and modulo") {
    CHECK(str(factorial(25)) == "15511210043330985984000000");
    CHECK(str(factorial(30)) == "265252859812191058636308480000000");

    BigInt a = power(BigInt(10), 30);   // 10^30
    BigInt b = power(BigInt(10), 18);   // 10^18
    CHECK(str(a) == "1000000000000000000000000000000");
    CHECK(str(a * a) == "1000000000000000000000000000000000000000000000000000000000000");  // 10^60
    CHECK(str(a / b) == "1000000000000");   // 10^12
    CHECK((a % b).isZero());

    BigInt c = a + BigInt(7);
    CHECK(str(c % b) == "7");
    CHECK(str(c / b) == "1000000000000");
}

TEST_CASE("Signs in division and modulo follow truncation toward zero") {
    BigInt big = power(BigInt(10), 25);
    BigInt d(7);

    CHECK(str((-big) / d) == "-" + str(big / d));
    CHECK(str((-big) % d) == "-" + str(big % d));   // remainder follows the dividend
    CHECK(str(big / (-d)) == "-" + str(big / d));
    CHECK(str(big % (-d)) == str(big % d));
}

TEST_CASE("Comparison across small and large magnitudes") {
    BigInt small(5);
    BigInt large = power(BigInt(10), 40);
    BigInt neg_large = -large;

    CHECK(small < large);
    CHECK(neg_large < small);
    CHECK(neg_large < large);
    CHECK(large > small);
    CHECK(neg_large <= neg_large);
    CHECK(large == power(BigInt(10), 40));
    CHECK(large != small);
}

TEST_CASE("Mixed operations with integer literals via implicit conversion") {
    BigInt b(100);
    CHECK(str(b + 5) == "105");
    CHECK(str(5 + b) == "105");
    CHECK(str(b - 1) == "99");
    CHECK(str(2 * b) == "200");
    CHECK((b == 100));
    CHECK((100 == b));
    CHECK((b < 200));
    CHECK((50 < b));
}

TEST_CASE("Increment and decrement") {
    BigInt b(std::numeric_limits<pgl::int128>::max());
    BigInt before = b;
    CHECK(str(b++) == str(before));
    CHECK_FALSE(b.fitsInt128());          // crossed 2^127
    CHECK(str(--b) == str(before));
    CHECK(b.fitsInt128());
}

TEST_CASE("Stream round-trip parsing") {
    const std::string text = "-123456789012345678901234567890123456789";
    std::istringstream is(text);
    BigInt b;
    is >> b;
    CHECK_FALSE(is.fail());
    CHECK(str(b) == text);

    // Round-trip a computed large value.
    BigInt f = factorial(40);
    std::istringstream is2(str(f));
    BigInt parsed;
    is2 >> parsed;
    CHECK(parsed == f);
}

TEST_CASE("Division by zero throws") {
    CHECK_THROWS_AS(BigInt(1) / BigInt(0), std::domain_error);
    CHECK_THROWS_AS(BigInt(1) % BigInt(0), std::domain_error);
}

TEST_CASE("Hash supports use in unordered containers") {
    std::unordered_set<BigInt> seen;

    BigInt small(123);
    BigInt big = power(BigInt(10), 50);

    seen.insert(small);
    seen.insert(big);
    seen.insert(-big);
    CHECK(seen.size() == 3);

    // Equal values (reached by different computations) collapse to one slot.
    seen.insert(BigInt(120) + BigInt(3));
    seen.insert(power(BigInt(10), 25) * power(BigInt(10), 25));
    CHECK(seen.size() == 3);

    CHECK(seen.count(small) == 1);
    CHECK(seen.count(big) == 1);
    CHECK(seen.count(-big) == 1);
    CHECK(seen.count(BigInt(999)) == 0);

    // Equal values must hash equally.
    std::hash<BigInt> hasher;
    CHECK(hasher(big) == hasher(power(BigInt(10), 50)));
    CHECK(hasher(small) == hasher(BigInt(123)));
}

TEST_CASE("Most negative int128 round-trips through the limb path") {
    BigInt mn(std::numeric_limits<pgl::int128>::min());
    CHECK(mn.isNegative());
    CHECK_FALSE(mn.fitsInt128());
    CHECK(str(mn) == "-170141183460469231731687303715884105728");
    CHECK(str(mn + BigInt(1)) == "-170141183460469231731687303715884105727");
    CHECK((mn + mn.abs()).isZero());
}

TEST_CASE("Fibonacci number") {
    BigInt a(1), b(1);
    for (int i = 2; i < 300; ++i) {
        BigInt next = a + b;
        a = b;
        b = next;
    }
    std::stringstream s;
    s << b;

    CHECK(s.str() == "222232244629420445529739893461909967206666939096499764990979600");
}

TEST_CASE("Factorial") {
    BigInt a(1);
    for (int i = 2; i <= 50; ++i) {
        a *= i;
    }
    BigInt b(1);
    for (int i = 51; i <= 100; ++i) {
        b *= i;
    }
    a *= b;
    std::stringstream s;
    s << a;

    CHECK(s.str() == "93326215443944152681699238856266700490715968264381621468592963895217599993229915608941463976156518286253697920827223758251185210916864000000000000000000000000");
    for (int i = 4; i <= 100; ++i) {
        a /= i;
    }
    CHECK(a == 6);
}
