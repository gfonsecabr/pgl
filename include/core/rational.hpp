#pragma once

/**
 * @file rational.hpp
 * @brief Exact rational number type used when geometric results need fractions.
 *
 * Intersections and measurements can opt into Rational to preserve exactness
 * even when integral input coordinates produce non-integral output points.
 */

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <numeric>
#include <type_traits>
#include <compare>
#include <limits>
#include <functional>
#include <cstdint>
#include <utility>
#include <concepts>
#include <cassert>

#include "../pgl.hpp"


namespace pgl {
template <class T>
struct is_Rational : std::false_type {};

template <class T>
struct is_Rational<Rational<T>> : std::true_type {};

template <class T>
inline constexpr bool is_Rational_v = is_Rational<T>::value;

template <class T>
concept RationalConcept = is_Rational_v<T>;


/**
 * @brief A class representing rational numbers (fractions).
 *
 * The Rational class stores a fraction as numerator/denominator using an
 * integral type Int. The fraction is always normalized:
 * - gcd(num, den) == 1
 * - den > 0
 *
 * @tparam Int Integral type used for storage.
 */
template <class Int = int64_t>
class Rational {
private:
    Int num; ///< Numerator
    Int den; ///< Denominator (always > 0)

    /**
     * @brief Normalize the fraction.
     */
    constexpr void normalize() {
        assert(den != 0);

        if (num == 0) {
            den = 1;
            return;
        }

        if (den == 1 || num == 1) {
            return;
        }

        Int g = pgl::detail::gcd(pgl::detail::abs(num), den);
        num /= g;
        den /= g;
    }

public:
    /**
     * @brief Default constructor (0).
     */
    constexpr Rational() : num(0), den(1) {}

    /**
     * @brief Construct from another Rational.
     */
    constexpr Rational(RationalConcept auto n) : num(n.numerator()), den(n.denominator()) {}

    /**
     * @brief Construct from integer.
     */
    constexpr Rational(pgl::detail::extended_integral auto n) : num(n), den(1) {}

    /**
     * @brief Construct from numerator and denominator.
     */
    constexpr Rational(Int n, Int d, bool normalized = false) : num(n), den(d) {
        if (den < 0) {
            num = -num;
            den = -den;
        }

        assert(den > 0);
        if (!normalized) normalize();
    }

    /**
     * @brief Construct from floating point
     */
    template<std::floating_point Float>
    constexpr Rational(Float f,
                       int digits = std::min(std::numeric_limits<Float>::digits,
                                             std::numeric_limits<Int>::digits/2 - 4)) {
        if (f == 0) {
            num = 0;
            den = 1;
            return;
        }
        bool negative = false;
        if (f < 0) {
            negative = true;
            f = -f;
        }

        int exponent;
        std::frexp(f, &exponent);
        if (exponent > 0)
            digits -= exponent;
        assert(digits > 0); // Rational number overflow
        den = 1 << digits;
        num = negative ? -den * f : den * f;

        normalize();
    }

    /// @brief Get numerator
    constexpr Int numerator() const noexcept { return num; }

    /// @brief Get denominator
    constexpr Int denominator() const noexcept { return den; }

    /// @brief Convert to float
    explicit constexpr operator float() const {
        return static_cast<float>(num) / static_cast<float>(den);
    }

    /// @brief Convert to double
    explicit constexpr operator double() const {
        return static_cast<double>(num) / static_cast<double>(den);
    }

    /// @brief Convert to long double
    explicit constexpr operator long double() const {
        return static_cast<long double>(num) / static_cast<long double>(den);
    }

    /// @brief Convert to int
    explicit constexpr operator int() const {
        return static_cast<int>(num) / static_cast<int>(den);
    }

    /// @brief Convert to long int
    explicit constexpr operator long int() const {
        return static_cast<long int>(num) / static_cast<long int>(den);
    }

    /// @brief Convert to another Rational
    template <class OtherInt>
    explicit constexpr operator Rational<OtherInt>() const {
        return Rational<OtherInt>(num,den,true);
    }

    /**
    * @brief Computes a monotone, downward-rounded floating-point approximation of num / den.
    *
    * This function f returns a float approximation r of the rational value num / den
    * such that:
    *
    * 1. Lower bound property:
    *      r <= num / den
    *    i.e., the result never exceeds the exact mathematical value.
    *
    * 2. Monotonicity:
    *      If num'/den' >= num/den, then f(num',den') >= f(num,den)
    *    meaning the function preserves ordering between rational values.
    *
    * @return A double r such that r <= num / den and the mapping is monotone.
    *
    * @warning This function may perform multiple calls to std::nextafter in rare
    *          cases where the initial floating-point approximation overshoots
    *          by more than one ULP. In practice, this is uncommon.
    */
    template <std::floating_point Float = double>
    constexpr Float lowerBound() const {
        using Double = pgl::detail::promoted_number_t<Float>;

        Double result = static_cast<Double>(num) / static_cast<Double>(den);
        Float flt_result = static_cast<Float>(result);

#ifndef NDEBUG
        int i = 0;
#endif
        while (static_cast<Double>(flt_result) > result) {
            flt_result = std::nextafter(flt_result, -std::numeric_limits<Float>::infinity());
            assert(i++ < 10); // Normally one iteration should be enough
        }

        return flt_result;
    }

    /**
    * @brief Computes a monotone, upward-rounded floating-point approximation of num / den.
    *
    * This function f returns a float approximation r of the rational value num / den
    * such that:
    *
    * 1. Upper bound property:
    *      r >= num / den
    *    i.e., the result never exceeds the exact mathematical value.
    *
    * 2. Monotonicity:
    *      If num'/den' <= num/den, then f(num',den') <= f(num,den)
    *    meaning the function preserves ordering between rational values.
    *
    * @return A double r such that r >= num / den and the mapping is monotone.
    *
    * @warning This function may perform multiple calls to std::nextafter in rare
    *          cases where the initial floating-point approximation overshoots
    *          by more than one ULP. In practice, this is uncommon.
    */
    template <std::floating_point Float = double>
    constexpr Float upperBound() const {
        using Double = pgl::detail::promoted_number_t<Float>;

        Double result = static_cast<Double>(num) / static_cast<Double>(den);
        Float flt_result = static_cast<Float>(result);

#ifndef NDEBUG
        int i = 0;
#endif
        while (static_cast<Double>(flt_result) < result) {
            flt_result = std::nextafter(flt_result, std::numeric_limits<Float>::infinity());
            assert(i++ < 10); // Normally one iteration should be enough
        }

        return flt_result;
    }

    constexpr Rational operator+(const Rational& r) const {
        return Rational(num * r.den + r.num * den, den * r.den,
                        den==1 || r.den==1);
    }

    constexpr Rational operator-(const Rational& r) const {
        return Rational(num * r.den - r.num * den, den * r.den,
                        den==1 || r.den==1);
    }

    constexpr Rational<Int> operator*(const Rational<Int>& r) const {
        return Rational(num * r.num, den * r.den,
                        den == r.den || pgl::detail::abs(num) == pgl::detail::abs(r.num));
    }

    constexpr Rational reciprocal() const {
        assert(num != 0);
        return num < 0 ? Rational(-den, -num, true) : Rational(den, num, true);
    }

    constexpr Rational operator/(const Rational& r) const {
        return *this * r.reciprocal();
    }

    constexpr Rational operator-() const {
        return Rational(-num, den, true);
    }

    // Compound
    constexpr Rational& operator+=(const Rational& r) { return *this = *this + r; }
    constexpr Rational& operator-=(const Rational& r) { return *this = *this - r; }
    constexpr Rational& operator*=(const Rational& r) { return *this = *this * r; }
    constexpr Rational& operator/=(const Rational& r) { return *this = *this / r; }

    // Mixed integer operators. Templating on the (deduced) argument type makes
    // an integer operand bind without a conversion, so these win cleanly over
    // operator OP(const Rational&) instead of tying with it. The tie — and the
    // resulting ambiguity — only arose when `int -> Int` is itself a
    // user-defined conversion, i.e. for a class-type Int such as BigInt.
    template <class I>
        requires (pgl::detail::extended_integral<I> || std::same_as<I, Int>)
    constexpr Rational operator+(const I& x) const { return *this + Rational(Int(x), true); }

    template <class I>
        requires (pgl::detail::extended_integral<I> || std::same_as<I, Int>)
    constexpr Rational operator-(const I& x) const { return *this - Rational(Int(x), true); }

    template <class I>
        requires (pgl::detail::extended_integral<I> || std::same_as<I, Int>)
    constexpr Rational operator*(const I& x) const { return Rational(num * Int(x), den); }

    template <class I>
        requires (pgl::detail::extended_integral<I> || std::same_as<I, Int>)
    constexpr Rational operator/(const I& x) const { return Rational(num, den * Int(x)); }

    friend constexpr Rational operator+(Int x, const Rational& r) { return r + x; }
    friend constexpr Rational operator-(Int x, const Rational& r) { return r + (-x); }
    friend constexpr Rational operator*(Int x, const Rational& r) { return r * x; }
    friend constexpr Rational operator/(Int x, const Rational& r) {
        return x == 1 ? r.reciprocal() : Rational(x) / r;
    }

    /**
     * @brief Three-way ordering of two values.
     *
     * Uses the native `operator<=>` when the storage type provides it (e.g.
     * `__int128_t`), and otherwise derives the ordering from `<` for types that
     * lack three-way comparison (e.g. Boost.Multiprecision numbers, used as the
     * int128 fallback).
     */
    template <class A, class B>
    static constexpr std::strong_ordering compareValues(const A& a, const B& b) {
        if constexpr (requires { a <=> b; }) {
            return a <=> b;
        } else {
            if (a < b)
                return std::strong_ordering::less;
            if (b < a)
                return std::strong_ordering::greater;
            return std::strong_ordering::equal;
        }
    }

    /**
     * @brief Three-way comparison operator.
     */
    constexpr std::strong_ordering operator<=>(const Rational& r) const {
        if (r.num == 0)
            return compareValues(num, Int{0});
        if (num == 0)
            return compareValues(Int{0}, r.num);
        if (num == r.num && den == r.den)
            return std::strong_ordering::equal;

        using Wide = pgl::detail::promoted_number_t<Int>;
        Wide lhs = static_cast<Wide>(num) * r.den;
        Wide rhs = static_cast<Wide>(r.num) * den;

        if (lhs < rhs)
            return std::strong_ordering::less;
        return std::strong_ordering::greater;
    }

    /**
     * @brief Three-way comparison against a Rational of a different integer type.
     *
     * Without this overload, comparing two distinct Rational instantiations is
     * ambiguous under C++20: the same-type operator<=> would have to convert one
     * operand up (non-reversed candidate) and the other down (reversed
     * candidate), with neither winning. Providing an exact heterogeneous match
     * resolves the comparison via the reversed-candidate tiebreaker.
     */
    template <class U>
        requires (!std::same_as<U, Int>)
    constexpr std::strong_ordering operator<=>(const Rational<U>& r) const {
        using Common = std::common_type_t<Int, U>;
        using Wide = pgl::detail::promoted_number_t<Common>;
        const Wide lhs = static_cast<Wide>(num) * static_cast<Wide>(r.denominator());
        const Wide rhs = static_cast<Wide>(r.numerator()) * static_cast<Wide>(den);
        return compareValues(lhs, rhs);
    }

    /**
     * @brief Equality comparison.
     */
    constexpr bool operator==(const Rational& r) const {
        return num == r.num && den == r.den;
    }
    constexpr bool operator!=(const Rational& r) const {
        return !(num == r.num && den == r.den);
    }

    /**
     * @brief Equality comparison against a Rational of a different integer type.
     *
     * Mirrors the heterogeneous operator<=> so mixed-type `==`/`!=` are
     * unambiguous. Both operands are normalized, so equal value implies equal
     * numerator and denominator.
     */
    template <class U>
        requires (!std::same_as<U, Int>)
    constexpr bool operator==(const Rational<U>& r) const {
        return num == r.numerator() && den == r.denominator();
    }

    constexpr Rational& operator++() { return *this += 1; }
    constexpr Rational operator++(int) { Rational tmp = *this; ++(*this); return tmp; }

    constexpr Rational& operator--() { return *this -= 1; }
    constexpr Rational operator--(int) { Rational tmp = *this; --(*this); return tmp; }

    /**
     * @brief Output format: num/den or just num if den==1
     */
    friend std::ostream& operator<<(std::ostream& os, const Rational& r) {
        if (r.den == 1)
            return os << r.num;
        else
            return os << r.num << "/" << r.den;
    }

    /**
     * @brief Input format: num/den or integer
     */
    friend std::istream& operator>>(std::istream& is, Rational& r) {
        Int n, d = 1;
        char sep = 0;

        if (!(is >> n)) return is;

        if (is.peek() == '/') {
            is >> sep >> d;
        }

        r = Rational(n, d);
        return is;
    }
};



// Primary template: no valid type by default
template <int Bits>
struct select_int_ge
{
    using type = void;
};

// Specializations for supported sizes
template <> struct select_int_ge<8>  { using type = std::int8_t;  };
template <> struct select_int_ge<16> { using type = std::int16_t; };
template <> struct select_int_ge<32> { using type = std::int32_t; };
template <> struct select_int_ge<64> { using type = std::int64_t; };
template <> struct select_int_ge<128> { using type = pgl::int128; };

// Helper to round required bits up to the next supported size
constexpr int round_up_bits(int bits) {
    if (bits <= 8)   return 8;
    if (bits <= 16)  return 16;
    if (bits <= 32)  return 32;
    if (bits <= 64)  return 64;
    return 128;
}

template <typename T>
struct is_int128 : std::false_type {};

template <> struct is_int128<pgl::int128> : std::true_type {};
#ifdef __SIZEOF_INT128__
template <> struct is_int128<__uint128_t> : std::true_type {};
#endif

template <typename T>
concept NumericType =
    std::is_integral_v<T> ||
    std::is_floating_point_v<T> ||
    is_int128<T>::value;

template <NumericType T>
struct to_integer_with_digits {
private:
    static constexpr int bits = std::numeric_limits<T>::digits;
    static constexpr int rounded = round_up_bits(bits);

public:
    using type = typename select_int_ge<rounded>::type;
};

template <typename T>
using to_integer_with_digits_t = typename to_integer_with_digits<T>::type;

}// pgl

template<class U, class V>
struct std::common_type<pgl::Rational<U>, pgl::Rational<V>>{
    using type = pgl::Rational<std::common_type_t<U,V>>;
};

/**
 * @brief numeric_limits specialization for Rational.
 */
template <class Int>
struct std::numeric_limits<pgl::Rational<Int>> {
    static constexpr bool is_specialized = true;

    static constexpr pgl::Rational<Int> min() noexcept {
        return pgl::Rational<Int>(std::numeric_limits<Int>::min(), 1);
    }

    static constexpr pgl::Rational<Int> max() noexcept {
        return pgl::Rational<Int>(std::numeric_limits<Int>::max(), 1);
    }

    static constexpr pgl::Rational<Int> lowest() noexcept {
        return min();
    }

    static constexpr int digits = std::numeric_limits<Int>::digits;
    static constexpr int digits10 = std::numeric_limits<Int>::digits10;

    static constexpr bool is_signed = std::numeric_limits<Int>::is_signed;
    static constexpr bool is_integer = false;
    static constexpr bool is_exact = true;

    // Remaining members keep the specialization complete so generic trait
    // machinery (e.g. Boost.Multiprecision, used as the int128 fallback) can
    // classify Rational. With is_integer == false and max_exponent == 0 it is
    // treated as an "unknown" category, so Boost never tries to convert it.
    static constexpr int radix = 2;
    static constexpr int min_exponent = 0;
    static constexpr int min_exponent10 = 0;
    static constexpr int max_exponent = 0;
    static constexpr int max_exponent10 = 0;
    static constexpr bool is_bounded = std::numeric_limits<Int>::is_bounded;
    static constexpr bool is_modulo = false;
    static constexpr bool is_iec559 = false;
    static constexpr bool has_infinity = false;
    static constexpr bool has_quiet_NaN = false;
    static constexpr bool has_signaling_NaN = false;
    static constexpr bool traps = false;
    static constexpr bool tinyness_before = false;
    static constexpr std::float_round_style round_style = std::round_toward_zero;

    static constexpr pgl::Rational<Int> epsilon() noexcept { return pgl::Rational<Int>(0, 1); }
    static constexpr pgl::Rational<Int> round_error() noexcept { return pgl::Rational<Int>(0, 1); }
    static constexpr pgl::Rational<Int> infinity() noexcept { return pgl::Rational<Int>(0, 1); }
    static constexpr pgl::Rational<Int> quiet_NaN() noexcept { return pgl::Rational<Int>(0, 1); }
    static constexpr pgl::Rational<Int> signaling_NaN() noexcept { return pgl::Rational<Int>(0, 1); }
    static constexpr pgl::Rational<Int> denorm_min() noexcept { return pgl::Rational<Int>(0, 1); }
};
