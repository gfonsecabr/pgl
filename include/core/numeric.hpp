#pragma once

#include "core/forward.hpp"

/**
 * @file numeric.hpp
 * @brief Numeric concepts and helpers shared by exact geometry operations.
 *
 * The geometry layer uses these utilities to select common coordinate types and
 * to keep arithmetic generic over integers, floating-point values, and rationals.
 */

#include <cmath>
#include <compare>
#include <concepts>
#include <cstdint>
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
using int128 = boost::multiprecision::int128_t;
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
    if (value < 0 && value == -value) {
        return stream << "-170141183460469231731687303715884105728";
    }
    if (value < 0) {
        return stream << "-" << -value;
    }
    if (value < 10) {
        return stream << static_cast<char>(value + '0');
    }
    return stream << value / 10 << static_cast<char>(value % 10 + '0');
}
#endif

}  // namespace pgl

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
        return static_cast<__int128_t>(static_cast<unsigned __int128>(1) << 127);
    }
    static constexpr __int128_t lowest() noexcept { return min(); }
    static constexpr __int128_t max() noexcept {
        return static_cast<__int128_t>(~(static_cast<unsigned __int128>(1) << 127));
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
 */
template <class A, class B>
constexpr std::strong_ordering strongOrder(const A& a, const B& b) {
    if constexpr (requires { std::strong_order(a, b); }) {
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
