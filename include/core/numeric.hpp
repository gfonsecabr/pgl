#pragma once

/**
 * @file numeric.hpp
 * @brief Numeric concepts and helpers shared by exact geometry operations.
 *
 * The geometry layer uses these utilities to select common coordinate types and
 * to keep arithmetic generic over integers, floating-point values, and rationals.
 */

#include <cmath>
#include <concepts>
#include <cstdint>
#include <numeric>
#include <ostream>
#include <type_traits>

namespace pgl {

/**
 * @brief Forward declaration of the exact rational number class.
 *
 * @tparam T Integral storage type.
 */
template <class T>
class Rational;

#if defined(__SIZEOF_INT128__)
/**
 * @brief Streams a signed 128-bit integer in decimal form.
 *
 * @param stream Output stream.
 * @param value Value to print.
 * @return The output stream.
 */
inline std::ostream& operator<<(std::ostream& stream, const __int128_t& value) {
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

#if defined(__SIZEOF_INT128__)
using int128_t = __int128_t;
#endif

template <typename T>
concept extended_integral =
    std::integral<T>
#if defined(__SIZEOF_INT128__)
    || std::same_as<std::remove_cv_t<T>, int128_t>
#endif
    ;

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
template <>
struct _promote<int8_t> {
    using type = int16_t;
};

template <>
struct _promote<pgl::Rational<int8_t>> {
    using type = pgl::Rational<int16_t>;
};

template <>
struct _promote<int16_t> {
    using type = int32_t;
};

template <>
struct _promote<pgl::Rational<int16_t>> {
    using type = pgl::Rational<int32_t>;
};

template <>
struct _promote<int32_t> {
    using type = int64_t;
};

template <>
struct _promote<pgl::Rational<int32_t>> {
    using type = pgl::Rational<int64_t>;
};

#if defined(__SIZEOF_INT128__)
template <>
struct _promote<int64_t> {
    using type = __int128_t;
};

template <>
struct _promote<pgl::Rational<int64_t>> {
    using type = pgl::Rational<__int128_t>;
};
#endif

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
 * @brief Returns the absolute value of a rational number.
 */
template <typename Int>
constexpr pgl::Rational<Int> abs(pgl::Rational<Int> value) {
    return pgl::Rational(pgl::detail::abs(value.numerator()), value.denominator(), true);
}

/**
 * @brief Returns the absolute value for signed numeric types supporting comparison and negation.
 */
inline constexpr auto abs(auto value) {
    return value >= 0 ? value : -value;
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
