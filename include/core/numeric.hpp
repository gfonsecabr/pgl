#pragma once

#include "core/handle.hpp"

/**
 * @file numeric.hpp
 * @brief Numeric concepts and helpers shared by exact geometry operations.
 *
 * The geometry layer uses these utilities to select common coordinate types and
 * to keep arithmetic generic over integers, floating-point values, and rationals.
 */

#include <array>
#include <cmath>
#include <compare>
#include <compare>
#include <concepts>
#include <cstdint>
#include <istream>
#include <limits>
#include <numeric>
#include <ostream>
#include <type_traits>

#if !defined(__SIZEOF_INT128__)
// Toolchains without the 128-bit extension (e.g. MSVC) fall back to Boost.
// Boost is only pulled in here, never when __int128_t is available.
#include <boost/multiprecision/cpp_int.hpp>
#endif

namespace pgl {

/**
 * @brief Forward declaration of the exact rational number class.
 *
 * @tparam T Integral storage type.
 */
template <class T>
class Rational;

/**
 * @brief Forward declaration of the arbitrary precision integer class, the
 * promotion target of ::pgl::int128.
 */
class BigInt;

/**
 * @brief Signed 128-bit integer.
 *
 * Aliases the native `__int128_t` extension when the compiler provides it
 * (recent g++ and clang++). On toolchains without it (e.g. MSVC) it falls back
 * to Boost.Multiprecision's fixed-width 128-bit integer; Boost is only included
 * in that fallback case.
 */
#if defined(__SIZEOF_INT128__)
using int128 = __int128_t;
#else
// Boost's own `int128_t` is sign-magnitude with a *128-bit magnitude*, so its
// max() is 2^128 - 1 — wider than native __int128 (two's complement, max
// 2^127 - 1). Using it would silently give pgl::int128 a different range on the
// fallback than on the native path, breaking the shared overflow guards and
// invariants. A 127-bit magnitude reproduces native's max() exactly; the only
// difference is min() is -(2^127 - 1) rather than -2^127, an extreme value the
// exact-arithmetic intermediates never depend on.
using int128 = boost::multiprecision::number<boost::multiprecision::cpp_int_backend<
    127, 127, boost::multiprecision::signed_magnitude, boost::multiprecision::unchecked, void>>;
#endif

#if defined(__SIZEOF_INT128__)
/**
 * @brief Streams a signed 128-bit integer in decimal form.
 *
 * @param stream Output stream.
 * @param value Value to print.
 * @return The output stream.
 */
inline std::ostream& operator<<(std::ostream& stream, const int128& value) {
    // The magnitude is taken unsigned: the most negative value has no negation
    // in the signed type, and forming one there is undefined behavior that
    // optimizers exploit -- a `value == -value` guard for it folds away.
    __uint128_t magnitude = value < 0 ? -static_cast<__uint128_t>(value)
                                      : static_cast<__uint128_t>(value);
    char digits[41];   // 39 decimal digits, a sign and a terminator
    char* first = digits + sizeof(digits) - 1;
    *first = '\0';
    do {
        *--first = static_cast<char>('0' + static_cast<int>(magnitude % 10));
        magnitude /= 10;
    } while (magnitude != 0);
    if (value < 0) {
        *--first = '-';
    }
    return stream << first;
}

/**
 * @brief Reads a signed 128-bit integer in decimal form.
 *
 * Consumes an optional sign followed by digits, stopping at the first
 * non-digit character (which is left in the stream) rather than reading a
 * whole whitespace-delimited token.
 *
 * @param stream Input stream.
 * @param value Value to populate.
 * @return The input stream.
 */
inline std::istream& operator>>(std::istream& stream, int128& value) {
    std::istream::sentry sentry(stream);
    if (!sentry) {
        return stream;
    }
    bool neg = false;
    int c = stream.peek();
    if (c == '+' || c == '-') {
        neg = c == '-';
        stream.get();
        c = stream.peek();
    }
    if (c < '0' || c > '9') {
        stream.setstate(std::ios::failbit);
        return stream;
    }
    // Accumulated unsigned, where the most negative value's magnitude exists
    // and wrapping is defined; a magnitude past the type wraps, as before.
    __uint128_t result = 0;
    for (; c >= '0' && c <= '9'; c = stream.peek()) {
        stream.get();
        result = result * 10 + static_cast<unsigned>(c - '0');
    }
    value = static_cast<int128>(neg ? -result : result);
    return stream;
}
#endif

}  // namespace pgl

#if !defined(__SIZEOF_INT128__)
// The Boost.Multiprecision fallback for pgl::int128 (used when the native
// __int128 extension is unavailable, e.g. MSVC) deliberately omits mixed
// operations with floating-point types, whereas native __int128 supports them
// through the usual arithmetic conversions. These free operators restore that
// double interop so pgl::int128 stays a drop-in replacement: each one converts
// the 128-bit value to double and operates in double, exactly as the built-in
// path would.
//
// They are defined in namespace boost::multiprecision (the namespace of
// pgl::int128's underlying type) so that argument-dependent lookup finds them
// everywhere -- crucially inside the `requires(Int x, Float g){ x * g; }` probe
// in Rational's float constructor, where MSVC only consults ADL, not ordinary
// lookup. A single operator<=> covers all four relational operators in both
// argument orders, and a single operator== covers ==/!=, avoiding the C++20
// reversed-candidate ambiguity that defining both directions would create.
namespace boost::multiprecision {
inline std::partial_ordering operator<=>(const pgl::int128& a, double b) {
    return static_cast<double>(a) <=> b;
}
inline bool operator==(const pgl::int128& a, double b) {
    return static_cast<double>(a) == b;
}
inline double operator+(const pgl::int128& a, double b) { return static_cast<double>(a) + b; }
inline double operator+(double a, const pgl::int128& b) { return a + static_cast<double>(b); }
inline double operator-(const pgl::int128& a, double b) { return static_cast<double>(a) - b; }
inline double operator-(double a, const pgl::int128& b) { return a - static_cast<double>(b); }
inline double operator*(const pgl::int128& a, double b) { return static_cast<double>(a) * b; }
inline double operator*(double a, const pgl::int128& b) { return a * static_cast<double>(b); }
inline double operator/(const pgl::int128& a, double b) { return static_cast<double>(a) / b; }
inline double operator/(double a, const pgl::int128& b) { return a / static_cast<double>(b); }
}  // namespace boost::multiprecision
#endif

namespace pgl::detail {

/**
 * @brief Portable replacement for std::numeric_limits used throughout pgl.
 *
 * For every type the primary template simply forwards to std::numeric_limits.
 * The one type it must override is the native __int128 extension: libstdc++ and
 * libc++ specialize std::numeric_limits<__int128>, but MSVC's STL (used by
 * clang-cl) does not, so there ::max()/::min()/::digits silently return 0 and
 * corrupt every magnitude/overflow check that relies on them. Routing all pgl
 * code through pgl::detail::numeric_limits keeps the int128 fast path correct on
 * every toolchain. (The Boost int128 fallback specializes std::numeric_limits,
 * so the forwarding primary template already covers it.)
 */
template <class T>
struct numeric_limits : std::numeric_limits<T> {};

#if defined(__SIZEOF_INT128__)
template <>
struct numeric_limits<__int128_t> {
    static constexpr bool is_specialized = true;
    static constexpr bool is_signed = true;
    static constexpr bool is_integer = true;
    static constexpr bool is_exact = true;
    static constexpr bool is_bounded = true;
    static constexpr int digits = 127;     // value bits of a signed 128-bit int
    static constexpr int digits10 = 38;
    static constexpr __int128_t min() noexcept {
        return static_cast<__int128_t>(static_cast<__uint128_t>(1) << 127);
    }
    static constexpr __int128_t lowest() noexcept { return min(); }
    static constexpr __int128_t max() noexcept {
        return static_cast<__int128_t>(~(static_cast<__uint128_t>(1) << 127));
    }
};
#endif

template <typename T>
concept extended_integral =
    std::integral<T>
    || std::same_as<std::remove_cv_t<T>, pgl::int128>;

/**
 * @brief Total ordering of two values, returned as a std::strong_ordering.
 *
 * Prefers std::strong_order, which gives a true total order for built-in
 * integers and floating-point values (and for types whose `operator<=>`
 * already yields a strong ordering, such as Rational and BigInt). Types that
 * std::strong_order rejects fall back to `<`. The `<` fallback is what lets
 * number types without a conforming `operator<=>` (e.g. Boost.Multiprecision,
 * used as the int128 fallback) participate in the geometry comparison
 * operators that need a strong total order.
 *
 * Numerically equal values compare equal first, which matters for exactly one
 * pair: std::strong_order implements the IEEE totalOrder predicate, and that
 * puts -0.0 strictly below +0.0. They are the same number, so leaving them
 * apart would make two geometrically identical points compare unequal — and a
 * -0.0 coordinate is easy to produce, e.g. by dividing a zero numerator by a
 * negative determinant. Everything else keeps the total order it had: NaNs are
 * never equal to anything, so they still reach std::strong_order and stay
 * ordered among themselves.
 */
template <class A, class B>
constexpr std::strong_ordering strongOrder(const A& a, const B& b) {
    if constexpr (requires { std::strong_order(a, b); }) {
        if constexpr (std::is_floating_point_v<A> || std::is_floating_point_v<B>) {
            if (a == b) {
                return std::strong_ordering::equal;
            }
        }
        return std::strong_order(a, b);
    } else {
        if (a < b) return std::strong_ordering::less;
        if (b < a) return std::strong_ordering::greater;
        return std::strong_ordering::equal;
    }
}

/**
 * @brief Three-way comparison preserving the natural ordering category.
 *
 * Returns `a <=> b` when the type provides it (a `partial_ordering` for
 * floating point, a `strong_ordering` for integers), so callers such as the
 * orientation predicates keep their exact float semantics (e.g. -0.0 and +0.0
 * compare equal). Types without `operator<=>` (Boost.Multiprecision on the
 * int128 fallback) derive a strong ordering from `<`.
 */
template <class A, class B>
constexpr auto threeWay(const A& a, const B& b) {
    if constexpr (requires { a <=> b; }) {
        return a <=> b;
    } else {
        return a < b ? std::strong_ordering::less
             : b < a ? std::strong_ordering::greater
                     : std::strong_ordering::equal;
    }
}

/**
 * @brief Type-level coordinate promotion helper.
 *
 * Specializations map narrow arithmetic types to a wider companion type used
 * to reduce overflow risk in intermediate geometric computations.
 *
 * @tparam T Input arithmetic type.
 */
template <typename T>
struct _promote {
    using type = T;
};

#ifndef PGL_DISABLE_PROMOTION
// Rational is deliberately never promoted: it manages its own overflow by
// reducing to lowest terms, so the storage type stays as the user chose it.
template <>
struct _promote<int8_t> {
    using type = int16_t;
};

template <>
struct _promote<int16_t> {
    using type = int32_t;
};

template <>
struct _promote<int32_t> {
    using type = int64_t;
};

template <>
struct _promote<int64_t> {
    using type = pgl::int128;
};

template <>
struct _promote<pgl::int128> {
    using type = pgl::BigInt;
};

template <>
struct _promote<float> {
    using type = double;
};

template <>
struct _promote<double> {
    using type = long double;
};
#endif

template <typename T>
using promoted_number_t = typename _promote<T>::type;

/**
 * @brief A value as a @p Target number, without the copy a cast to the type it
 *        already has would make.
 *
 * The promotion rules above deliberately leave @ref pgl::Rational and
 * @ref pgl::BigInt alone, so an operation that casts each operand to its promoted
 * coordinate type is, for those two, casting to the type the operand already has.
 * Spelled as a `static_cast` that is a copy the language mandates and no
 * optimizer removes: the argument is an lvalue, so the C++17 elision rules do not
 * reach it, and @ref pgl::BigInt's copy constructor is non-trivial — it
 * conditionally allocates limbs — which is what stops the compiler from reasoning
 * it away. On a limb-carrying value it allocates, once per cast. This hands the
 * operand over by `const` reference instead, and converts only when the type
 * really changes, so every promoting instantiation — `int` to `int64_t` and the
 * like — still compiles to what the plain cast did.
 *
 * Worth knowing where it does not pay: on the shape-pair benchmarks the exact
 * types gain across the board, by a median of 7-11% and up to 65% on the cheap
 * predicates that were mostly copying, but a `Disk<ERational>` pair loses about
 * 10%. Its determinants are the heaviest exact arithmetic in the library, and
 * there a private copy the optimizer knows aliases nothing is worth more than the
 * copy costs. @ref pgl::Rational's own comparison operators behave the same way
 * and deliberately still cast.
 *
 * @warning A returned reference refers to the argument, so it lives only as long
 * as what was passed in. Use it inside the expression that consumes it, as the
 * callers here do; do not bind it to a longer-lived reference, and do not pass a
 * temporary. Neither restriction gives anything up, since a temporary argument is
 * a prvalue whose `static_cast` the language already elides.
 *
 * @tparam Target The number type the caller wants to compute in.
 * @param value The operand to view as a @p Target.
 */
template <class Target, class Number>
constexpr decltype(auto) asNumber(const Number& value) {
    if constexpr (std::is_same_v<Target, Number>) {
        return (value);
    } else {
        return static_cast<Target>(value);
    }
}

/**
 * @brief Whether a number type grows to hold its values instead of wrapping.
 *
 * True for @ref BigInt and for any @ref Rational built on one. These are the
 * types whose arithmetic allocates and runs in time proportional to the operand
 * size, rather than compiling to a fixed handful of machine instructions, which
 * is what makes a floating-point pre-check worth attempting before them.
 */
template <class T>
inline constexpr bool arbitraryPrecision = false;

template <>
inline constexpr bool arbitraryPrecision<pgl::BigInt> = true;

template <class Int>
inline constexpr bool arbitraryPrecision<pgl::Rational<Int>> = arbitraryPrecision<Int>;

/**
 * @brief The floating-point type an inexact measurement reports its result in.
 *
 * A few measurements have no closed form in the coordinate type — every
 * distance to a @ref Disk, whose nearest point sits on a circle — and are found
 * numerically rather than exactly. They still take the usual `ResultNumber`
 * parameter, but they cannot honour an exact one: the answer is generally
 * irrational whatever the input. This is the type they return instead.
 *
 * A floating-point `ResultNumber` is passed through, so
 * `squaredDistance<long double>(disk)` really does compute and report in
 * `long double`. Any other request — an integer, a @ref Rational — falls back
 * to `double`, the widest precision such a call can actually be served at.
 */
template <class ResultNumber>
using floating_result_t =
    std::conditional_t<std::is_floating_point_v<ResultNumber>, ResultNumber, double>;

/**
 * @brief Returns the greatest common divisor of two integral values.
 */
constexpr auto gcd(std::integral auto a, std::integral auto b) {
    return std::gcd(a, b);
}

/**
 * @brief Returns the greatest common divisor for modulo-capable numeric types.
 */
inline auto gcd(auto a, auto b) {
    if (a == b) {
        return a;
    }

    while (b != 0) {
        const auto remainder = a % b;
        a = b;
        b = remainder;
    }

    return a;
}

/**
 * @brief Extended Euclidean algorithm over any modulo-capable integer type.
 *
 * Returns `(g, s, t)` with `a * s + b * t == g` and `g == gcd(|a|, |b|) >= 0`.
 * The Bezout coefficients are what turn a solvable linear Diophantine equation
 * into an actual solution; the library uses them to place a lattice point on a
 * line whose direction is a primitive integer vector.
 *
 * Division truncating toward zero keeps the identity exact for signed inputs;
 * only the sign of the last remainder needs fixing at the end.
 */
template <class Integer>
inline std::array<Integer, 3> extendedGcd(Integer a, Integer b) {
    Integer remainder = a, currentRemainder = b;
    Integer coefficientA(1), nextCoefficientA(0);
    Integer coefficientB(0), nextCoefficientB(1);
    while (currentRemainder != Integer(0)) {
        const Integer quotient = remainder / currentRemainder;
        Integer next = remainder - quotient * currentRemainder;
        remainder = currentRemainder;
        currentRemainder = next;
        next = coefficientA - quotient * nextCoefficientA;
        coefficientA = nextCoefficientA;
        nextCoefficientA = next;
        next = coefficientB - quotient * nextCoefficientB;
        coefficientB = nextCoefficientB;
        nextCoefficientB = next;
    }
    if (remainder < Integer(0)) {
        remainder = -remainder;
        coefficientA = -coefficientA;
        coefficientB = -coefficientB;
    }
    return {remainder, coefficientA, coefficientB};
}

/**
 * @brief Returns `numerator / divisor` rounded to the nearest integer, with
 * halves rounded up. The divisor must be positive.
 */
template <class Integer>
inline Integer nearestQuotient(const Integer& numerator, const Integer& divisor) {
    Integer quotient = numerator / divisor;          // truncates toward zero
    Integer remainder = numerator - quotient * divisor;
    if (remainder < Integer(0)) {                    // make it a floor division
        quotient -= Integer(1);
        remainder += divisor;
    }
    if (Integer(2) * remainder >= divisor) {
        quotient += Integer(1);
    }
    return quotient;
}

/**
 * @brief Whether an exact integer value is representable in @p Target.
 *
 * Arbitrary-precision targets (@ref pgl::BigInt) hold every value, which is what
 * `is_bounded == false` reports; a fixed-width one is compared against its own
 * limits in the wider type the value is already held in.
 */
template <class Target, class Integer>
inline bool representableAs(const Integer& value) {
    if constexpr (!numeric_limits<Target>::is_bounded) {
        return true;
    } else {
        return value >= Integer(numeric_limits<Target>::lowest())
            && value <= Integer(numeric_limits<Target>::max());
    }
}

/**
 * @brief Converts an exact integer that @ref representableAs has accepted into
 * the target type.
 *
 * An arbitrary-precision source converts only to the widest integers, so the
 * step down to a narrower target goes through ::pgl::int128 — which is exact
 * precisely because the value was checked to fit first.
 */
template <class Target, class Integer>
inline Target narrowTo(const Integer& value) {
    if constexpr (std::same_as<Target, Integer>) {
        return value;
    } else {
        return static_cast<Target>(static_cast<pgl::int128>(value));
    }
}

/**
 * @brief Returns the absolute value of an integral number.
 */
constexpr auto abs(extended_integral auto value) {
    using T = decltype(value);
    if constexpr (std::is_unsigned_v<T>) {
        return value;
    } else {
        return value < 0 ? -value : value;
    }
}

/**
 * @brief Returns the absolute value of a floating-point number.
 */
constexpr auto abs(std::floating_point auto value) {
    return std::abs(value);
}

/**
 * @brief Returns the absolute value for signed numeric types supporting comparison and negation.
 *
 * Declared before the rational overload below so that its qualified call to
 * pgl::detail::abs on the numerator can resolve to this catch-all for class-type
 * integers such as pgl::BigInt (qualified lookup only sees earlier declarations).
 */
inline constexpr auto abs(auto value) {
    return value >= 0 ? value : -value;
}

/**
 * @brief Returns the absolute value of a rational number.
 */
template <typename Int>
constexpr pgl::Rational<Int> abs(pgl::Rational<Int> value) {
    // The parameter is a copy, so simplify it in place and read both parts from
    // the reduced form; numerator() and denominator() would each reduce it anew.
    value.simplify();
    return pgl::Rational(pgl::detail::abs(value.numerator()), value.denominator(), true);
}

/**
 * @brief Converts one coordinate value to another coordinate type.
 *
 * This is the central hook used by point and shape conversions.
 *
 * @tparam To Destination coordinate type.
 * @tparam From Source coordinate type.
 * @param value Source value.
 * @return Converted coordinate.
 */
template <class To, class From>
[[nodiscard]] constexpr To convertCoordinate(From value) {
    return static_cast<To>(value);
}

}  // namespace pgl::detail
