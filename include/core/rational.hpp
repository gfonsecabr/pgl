#pragma once

#include "core/bigint.hpp"

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
#include <cstddef>



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
 * @brief The integer type a Rational stores its parts in; any other type is its
 * own answer.
 *
 * This is the type an exact fraction collapses to once it is known to be whole,
 * which is what makes it the natural default result type for an operation that
 * hands back integers extracted from rational coordinates.
 */
template <class T>
struct rational_int {
    using type = T;
};

template <class Int>
struct rational_int<Rational<Int>> {
    using type = Int;
};

template <class T>
using rational_int_t = typename rational_int<T>::type;

/**
 * @brief The integer type a coordinate rasterizes onto by default.
 *
 * A grid cell is an integer position, so a shape can only be rasterized over a
 * signed integer coordinate type. One that already is stays itself, so an
 * integer shape rasterizes with no conversion at all, and a Rational collapses
 * to the integer it is built on, which every whole fraction fits. Anything else
 * -- a floating-point coordinate, a Rational over a @ref BigInt -- goes to
 * `int64_t`, wide enough for every whole number a `double` can name; the value
 * is checked against its range rather than wrapped into it.
 */
template <class T>
using grid_number_t = std::conditional_t<std::signed_integral<rational_int_t<T>>,
                                         rational_int_t<T>, std::int64_t>;

namespace detail {
/// @brief Exact 2^k as a pgl::BigInt for k >= 0 (exponentiation by squaring).
///
/// Used by the exact Rational-vs-floating-point comparison to apply a float's
/// binary exponent without ever overflowing a fixed-width integer.
inline pgl::BigInt pow2(int k) {
    pgl::BigInt result(1), base(2);
    while (k > 0) {
        if (k & 1) result *= base;
        base *= base;
        k >>= 1;
    }
    return result;
}
} // namespace detail


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
    Int num;                 ///< Numerator (not necessarily reduced; see normalized_)
    Int den;                 ///< Denominator (always > 0)
    bool normalized_ = true; ///< False when gcd(|num|, den) may exceed 1

    /**
     * @brief How many heap limbs a deferred fraction may reach, once it is off
     * the inline store, before @ref reductionUrgent stops tolerating it.
     *
     * Only a chain that keeps compounding — an accumulation, an iteration —
     * grows a fraction without bound, and this is what caps it. A single
     * geometric predicate does not: the widest of them, the in-circle
     * determinant, is degree four in its coordinates, so over the fractions an
     * arrangement produces its intermediates stay well inside this bound and
     * never pay a heap gcd. Eight limbs is a little under 500 bits.
     */
    static constexpr std::size_t heapReductionLimbs = 8;

    /**
     * @brief Whether leaving (n, d) unreduced before the next multiply or
     * cross-addition would cost more than reducing it now. This is the trigger
     * that forces an otherwise-deferred normalization.
     *
     * For a fixed-width Int the cost is overflow, and the bound is half the
     * available width, so the impending product of two operands still fits. For
     * a BigInt nothing overflows and the bound follows what the gcd costs at
     * that size instead; see below.
     */
    static constexpr bool reductionUrgent(const Int& n, const Int& d) {
        if constexpr (requires { n.fitsInt64(); }) {
            // BigInt, and the answer turns on what the gcd itself will cost.
            //
            // While both parts are inline (a single int128), BigInt divides on
            // its native fast path, so the gcd is a handful of machine
            // divisions. Reduce at half that width, exactly as the fixed-width
            // rule below does, and the impending multiply or cross-addition
            // stays inline too.
            //
            // Once a part has spilled onto the heap that bargain is off: every
            // step of the gcd becomes a multi-limb long division, which costs
            // far more than carrying the extra limbs through the one multiply
            // the reduction was meant to shrink — and @ref operandParts throws
            // the reduced form away with its locals, so the same value pays
            // again at every use. Past that point reduce only to stop unbounded
            // growth, not to save width.
            if (n.fitsInt128() && d.fitsInt128()) {
                return !n.fitsInt64() || !d.fitsInt64();
            }
            return !n.fitsLimbs(heapReductionLimbs) || !d.fitsLimbs(heapReductionLimbs);
        } else {
            constexpr int half = pgl::detail::numeric_limits<Int>::digits / 2 - 1;
            const Int limit = Int(1) << half;
            return pgl::detail::abs(n) >= limit || d >= limit;
        }
    }

    /**
     * @brief Whether comparing (n, d) has to reduce it first.
     *
     * A comparison only cross-multiplies and stores nothing, so the reduction
     * buys exactly one thing: keeping the widened product from overflowing.
     * An arbitrary-precision Int has nothing to overflow, and there the
     * reduction is a bad trade — a gcd is a chain of divisions, against the
     * single multiplication of two values a few limbs wide that it saves, and
     * since @ref operandParts writes the reduced form only into the caller's
     * locals, the same value pays for it again at every comparison. Profiling
     * the non-convex Minkowski sum, whose arrangement compares
     * `Rational<BigInt>` coordinates by the million, put 60% of the entire run
     * inside that gcd.
     */
    static constexpr bool comparisonNeedsReduction(const Int& n, const Int& d) {
        if constexpr (requires { n.fitsInt64(); }) {
            return false;
        } else {
            return reductionUrgent(n, d);
        }
    }

    /**
     * @brief Copy this value's numerator/denominator into (n, d), reducing first
     * only when leaving them unreduced could overflow the impending arithmetic.
     */
    constexpr void operandParts(Int& n, Int& d) const {
        n = num;
        d = den;
        if (!normalized_ && reductionUrgent(n, d)) {
            const Int g = pgl::detail::gcd(pgl::detail::abs(n), d);
            n /= g;
            d /= g;
        }
    }

public:
    /**
     * @brief Default constructor (0).
     */
    constexpr Rational() : num(0), den(1) {}

    /**
     * @brief Construct from another Rational.
     *
     * The source is taken by value and simplified once, rather than read through
     * `numerator()` and `denominator()`: those each run their own gcd over the
     * same deferred pair, so reading both that way reduces it twice.
     */
    constexpr Rational(RationalConcept auto n) : num(0), den(1) {
        n.simplify();
        num = Int(n.numerator());
        den = Int(n.denominator());
    }

    /**
     * @brief Construct from integer.
     */
    constexpr Rational(pgl::detail::extended_integral auto n) : num(n), den(1) {}

    /**
     * @brief Construct from a numerator of the rational's own integer type
     * (denominator 1), covering integer types not matched by the overloads
     * above, such as BigInt.
     *
     * The parameter is templated on `std::same_as<Int>` rather than written as
     * a plain `Rational(Int)` so that `Int` is *not* deducible from the
     * argument, which keeps this constructor's synthesized implicit deduction
     * guide from ever picking `Int` off the argument during CTAD. A plain
     * `Rational(Int)` guide, constrained only by a `requires`-clause, is
     * mishandled by clang 18: it fails to drop the guide for floating-point and
     * so deduces `Rational<double>` for a bare `Rational(5.25)`. With `Int`
     * non-deducible here, the intended CTAD is pinned by the explicit guides
     * declared after the class instead. See @ref Rational deduction guides.
     */
    template <std::same_as<Int> T>
        requires(!pgl::detail::extended_integral<Int> && !std::floating_point<Int>
                 && !RationalConcept<Int>)
    constexpr Rational(T n) : num(std::move(n)), den(1) {}

    /**
     * @brief Construct from numerator and denominator.
     */
    constexpr Rational(Int n, Int d, bool normalized = false)
        : num(n), den(d), normalized_(normalized) {
        if (den < 0) {
            num = -num;
            den = -den;
        }

        assert(den > 0);
        // The reduction is deferred: it happens lazily at a read, a comparison,
        // or when an arithmetic step would otherwise risk overflow. Integers are
        // already in lowest terms, so flag them normalized to skip that work.
        if (den == 1) normalized_ = true;
    }

    /**
     * @brief Construct from floating point.
     *
     * Keeps @p digits significant bits of @p f: the integer part exactly, and
     * as many bits of the fraction as fit beside it, the rest truncated. Once
     * the integer part alone needs every one of those bits, or more, the value
     * is truncated toward zero to that integer part, which is what a plain
     * integer conversion does; only an integer part that @p Int cannot hold is
     * refused, as is a value that is not finite.
     *
     * The default @p digits is half the width of a bounded @p Int, less a few
     * bits, so that the parts leave room for the arithmetic that follows.
     *
     * @throws std::domain_error If @p f is infinite or NaN.
     * @throws std::overflow_error If the integer part of @p f does not fit Int.
     */
    template<std::floating_point Float>
    explicit constexpr Rational(Float f,
                       int digits = pgl::detail::numeric_limits<Int>::digits > 0
                           ? std::min(pgl::detail::numeric_limits<Float>::digits,
                                      pgl::detail::numeric_limits<Int>::digits / 2 - 4)
                           : pgl::detail::numeric_limits<Float>::digits) {
        if (!std::isfinite(f)) {
            throw std::domain_error("pgl::Rational: cannot construct from a non-finite floating-point value");
        }
        if (f == 0) {
            num = 0;
            den = 1;
            return;
        }

        // |f| lies in [2^(exponent - 1), 2^exponent), so exponent is how many
        // of the requested bits the integer part takes for itself.
        int exponent = 0;
        std::frexp(f, &exponent);
        const int fractionBits = exponent > 0 ? digits - exponent : digits;
        if (fractionBits <= 0) {
            // No fraction bit fits beside the integer part: the value is that
            // part, exactly, whenever the storage type holds it.
            if constexpr (pgl::detail::numeric_limits<Int>::is_bounded) {
                if (exponent > pgl::detail::numeric_limits<Int>::digits) {
                    throw std::overflow_error("pgl::Rational: floating-point value does not fit the integer type");
                }
            }
            num = Int(std::trunc(f));
            den = Int(1);
            normalized_ = true;
            return;
        }

        if constexpr (requires(Int x, Float g) { x * g; }) {
            bool negative = false;
            if (f < 0) {
                negative = true;
                f = -f;
            }
            den = Int(1) << fractionBits;
            // Wrap in Int(): for the Boost int128 fallback `den * f` yields a
            // double (see the double-interop shim in numeric.hpp), so convert it
            // back explicitly. For native __int128 / built-in ints this is the
            // same truncating conversion that the implicit assignment did.
            num = Int(negative ? -den * f : den * f);

            normalized_ = false;
            simplify();
        } else {
            // Int has no floating-point multiply (e.g. pgl::BigInt). A float's
            // significand always fits in a 128-bit integer, so build the value
            // with the requested number of fractional bits in Rational<int128>
            // and widen it. This makes float -> Rational<BigInt> work, which the
            // int128 -> BigInt type promotion relies on for mixed predicates.
            const Rational<pgl::int128> r(f, digits);
            num = Int(r.numerator());
            den = Int(r.denominator());
            normalized_ = true;
        }
    }

    /// @brief Get numerator (in lowest terms).
    constexpr Int numerator() const noexcept {
        if (normalized_) return num;
        return num / pgl::detail::gcd(pgl::detail::abs(num), den);
    }

    /// @brief Get denominator (in lowest terms).
    constexpr Int denominator() const noexcept {
        if (normalized_) return den;
        return den / pgl::detail::gcd(pgl::detail::abs(num), den);
    }

    /**
     * @brief Reduces the stored fraction to lowest terms in place (den stays > 0).
     *
     * The reduction this type defers is normally recomputed and thrown away at
     * every read: @ref numerator and @ref denominator each run their own gcd and
     * keep nothing, so a value read `k` times pays for `k` reductions — and twice
     * that when both parts are wanted. Calling this once, on a value that is
     * about to be read repeatedly (hashed into a container, compared across a
     * sweep, carried through a chain of predicates), collapses all of it to a
     * single gcd; every later read then takes the already-normalized fast path.
     *
     * Being non-const is the whole point: the result is stored, which is what
     * a const read cannot do. Nothing about the *value* changes, so this is
     * only ever a performance decision, never a semantic one.
     *
     * @see simplified() for the const version, which returns the reduced value
     *      instead of storing it.
     */
    constexpr void simplify() {
        if (normalized_) {
            return;
        }
        normalized_ = true;
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

    /**
     * @brief Returns this value reduced to lowest terms.
     *
     * The const counterpart of @ref simplify: for a value that cannot be
     * modified — one reached through a const reference, a container element, a
     * function parameter — this hands back a copy the caller can store and read
     * freely. It is also the cheap way to obtain *both* parts of a deferred
     * fraction, since `numerator()` and `denominator()` each run their own gcd
     * over the same pair, and this one runs it once.
     *
     * @return An equal value whose stored parts satisfy `gcd(|num|, den) == 1`.
     */
    [[nodiscard]] constexpr Rational simplified() const {
        if (normalized_) {
            return *this;
        }
        if (num == 0) {
            return Rational(Int(0), Int(1), true);
        }
        if (den == 1 || num == 1) {
            return Rational(num, den, true);
        }
        const Int g = pgl::detail::gcd(pgl::detail::abs(num), den);
        return Rational(num / g, den / g, true);
    }

    /**
     * @brief Reduces in place, but only when the stored parts have already grown
     * wide enough that the next arithmetic step would reduce them anyway.
     *
     * The no-regression form of @ref simplify. Arithmetic on a deferred fraction
     * reduces its operands whenever they have grown wide — see
     * @ref operandParts — and throws that reduced form away with the expression's
     * locals, so a wide value feeding many operations is reduced once per
     * operation. This runs that same reduction a single time and *keeps* it,
     * under exactly the same width test the arithmetic itself applies. A value
     * below that width is left alone, because nothing downstream would have
     * reduced it either.
     *
     * So it never spends a gcd the library was not going to spend, which makes
     * it safe to apply to a stored value whose use is not known in advance. What
     * it will not do is reduce a *narrow* fraction to keep everything computed
     * from it narrower; that is a bet on later use, and only @ref simplify makes
     * it. Where the values are known to be read many times over — the vertices
     * an arrangement interns, say — that bet pays, and @ref simplify is the right
     * call even though it costs reductions this one would skip.
     *
     * @see simplifiedIfLarge() for the const version.
     */
    constexpr void simplifyIfLarge() {
        if (!normalized_ && reductionUrgent(num, den)) {
            simplify();
        }
    }

    /**
     * @brief Returns this value reduced, but only when it is already wide enough
     * that the next arithmetic step would reduce it anyway.
     *
     * The const counterpart of @ref simplifyIfLarge, on the same width test, for
     * a value that cannot be modified in place. Returns an equal value either
     * way — reduced when the test fires, and an unchanged copy when it does not.
     *
     * @return A value equal to this one, in lowest terms when it was wide.
     */
    [[nodiscard]] constexpr Rational simplifiedIfLarge() const {
        if (!normalized_ && reductionUrgent(num, den)) {
            return simplified();
        }
        return *this;
    }

    /**
     * @brief Whether this rational is exactly an integer.
     *
     * A normalized fraction is whole exactly when its denominator is one. A
     * deferred fraction may spell an integer with a larger denominator, so it
     * is tested by divisibility without first computing or copying its reduced
     * numerator and denominator.
     */
    [[nodiscard]] constexpr bool isInteger() const {
        return normalized_ ? den == Int(1) : num % den == Int(0);
    }

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

    /// @brief Convert (truncating toward zero) to `int`.
    ///
    /// The division happens in `Int` and only the quotient is narrowed. Doing it
    /// the other way round — narrowing `num` and `den` and dividing those —
    /// would be wrong for every fraction stored unreduced: normalization is
    /// deferred, so a value as ordinary as 5 can be carried as a ratio of two
    /// numbers far past `int`, and narrowing each of them independently turns
    /// the quotient into noise. The exact value fits or it does not; the parts
    /// it happens to be written with are not the caller's business.
    explicit constexpr operator int() const {
        if (den == Int(1)) {
            return static_cast<int>(num);
        }
        return static_cast<int>(num / den);
    }

    /// @brief Convert (truncating toward zero) to `int64_t`. Divides first, for
    /// the reason given on @ref operator int() const.
    explicit constexpr operator int64_t() const {
        if (den == Int(1)) {
            return static_cast<int64_t>(num);
        }
        return static_cast<int64_t>(num / den);
    }

    /// @brief Convert (truncating toward zero) to the numerator integer type,
    /// for `Int` types not already covered by `operator int`/`operator int64_t`
    /// above, such as BigInt. `den > 0` is an invariant, so the truncating
    /// division `num / den` matches truncating the exact value regardless of
    /// whether the fraction is stored reduced.
    explicit constexpr operator Int() const
        requires(!std::same_as<Int, int> && !std::same_as<Int, int64_t>) {
        if (den == Int(1)) {
            return num;
        }
        return num / den;
    }

    /// @brief Convert to another Rational
    template <class OtherInt>
    explicit constexpr operator Rational<OtherInt>() const {
        return Rational<OtherInt>(num, den, normalized_);
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
            flt_result = std::nextafter(flt_result, -pgl::detail::numeric_limits<Float>::infinity());
            assert(i++ < 10); // Normally one iteration should be enough
        }
        // The quotient is itself a rounding of the exact value, so when the
        // narrowing landed exactly on it nothing above tells which side of the
        // value the candidate is on: settle that exactly. One step then
        // suffices, since a float strictly below the quotient is below every
        // value the quotient can have rounded from.
        if (static_cast<Double>(flt_result) == result && (*this <=> flt_result) < 0) {
            flt_result = std::nextafter(flt_result, -pgl::detail::numeric_limits<Float>::infinity());
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
    *    i.e., the result is never below the exact mathematical value.
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
            flt_result = std::nextafter(flt_result, pgl::detail::numeric_limits<Float>::infinity());
            assert(i++ < 10); // Normally one iteration should be enough
        }
        // See lowerBound: a candidate equal to the rounded quotient may still
        // be below the exact value, and one exact step settles it.
        if (static_cast<Double>(flt_result) == result && (*this <=> flt_result) > 0) {
            flt_result = std::nextafter(flt_result, pgl::detail::numeric_limits<Float>::infinity());
        }

        return flt_result;
    }

    // Arithmetic results are left unreduced (the gcd is deferred); each operand
    // is only reduced first when its magnitude would otherwise risk overflow.
    // An integer result (denominator 1) is trivially in lowest terms.

    /// @brief Whether the raw num/den can be combined as-is without first
    /// reducing (i.e. already normalized, or small enough to be safe).
    constexpr bool safeRaw() const {
        return normalized_ || !reductionUrgent(num, den);
    }

    constexpr Rational operator+(const Rational& r) const {
        if (safeRaw() && r.safeRaw()) {
            const Int rd = den * r.den;
            return Rational(num * r.den + r.num * den, rd, rd == 1);
        }
        Int an, ad, bn, bd;
        operandParts(an, ad);
        r.operandParts(bn, bd);
        const Int rd = ad * bd;
        return Rational(an * bd + bn * ad, rd, rd == 1);
    }

    constexpr Rational operator-(const Rational& r) const {
        if (safeRaw() && r.safeRaw()) {
            const Int rd = den * r.den;
            return Rational(num * r.den - r.num * den, rd, rd == 1);
        }
        Int an, ad, bn, bd;
        operandParts(an, ad);
        r.operandParts(bn, bd);
        const Int rd = ad * bd;
        return Rational(an * bd - bn * ad, rd, rd == 1);
    }

    constexpr Rational<Int> operator*(const Rational<Int>& r) const {
        if (safeRaw() && r.safeRaw()) {
            const Int rd = den * r.den;
            return Rational(num * r.num, rd, rd == 1);
        }
        Int an, ad, bn, bd;
        operandParts(an, ad);
        r.operandParts(bn, bd);
        const Int rd = ad * bd;
        return Rational(an * bn, rd, rd == 1);
    }

    constexpr Rational reciprocal() const {
        assert(num != 0);
        // Swapping numerator and denominator preserves gcd(|num|, den), so the
        // normalization state carries over unchanged.
        return num < 0 ? Rational(-den, -num, normalized_)
                       : Rational(den, num, normalized_);
    }

    constexpr Rational operator/(const Rational& r) const {
        return *this * r.reciprocal();
    }

    constexpr Rational operator-() const {
        // Negation preserves gcd(|num|, den), so the normalization state holds.
        return Rational(-num, den, normalized_);
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
    constexpr Rational operator*(const I& x) const {
        Int n, d;
        operandParts(n, d);
        return Rational(n * Int(x), d);
    }

    template <class I>
        requires (pgl::detail::extended_integral<I> || std::same_as<I, Int>)
    constexpr Rational operator/(const I& x) const {
        Int n, d;
        operandParts(n, d);
        return Rational(n, d * Int(x));
    }

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
     *
     * The three-way operator is taken only when it is exact, which is what
     * requiring a `std::strong_ordering` result checks. A mixed comparison of a
     * built-in integer against the Boost int128 fallback would otherwise be
     * answered by the `int128`-versus-`double` operator declared in numeric.hpp
     * (found as a reversed candidate, with the integer converted to `double`):
     * it reports `std::partial_ordering`, and it rounds. Such a pair falls back
     * to `<`, which Boost defines exactly for every mixed integer operand.
     */
    template <class A, class B>
    static constexpr std::strong_ordering compareValues(const A& a, const B& b) {
        if constexpr (requires {
                          { a <=> b } -> std::same_as<std::strong_ordering>;
                      }) {
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
        using Wide = pgl::detail::promoted_number_t<Int>;
        // Deferred fractions compare exactly by cross-multiplying in the wider
        // type (both denominators are positive, so the sign is preserved, and
        // compareValues yields the equal case). Reduce first only when an operand
        // is large enough that the widened product could itself overflow.
        if ((!normalized_ && comparisonNeedsReduction(num, den)) ||
            (!r.normalized_ && comparisonNeedsReduction(r.num, r.den))) {
            Int an, ad, bn, bd;
            operandParts(an, ad);
            r.operandParts(bn, bd);
            return compareValues(static_cast<Wide>(an) * bd, static_cast<Wide>(bn) * ad);
        }
        return compareValues(static_cast<Wide>(num) * r.den,
                             static_cast<Wide>(r.num) * den);
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
        using Wide = pgl::detail::promoted_number_t<Int>;
        // Deferred fractions are equal iff their cross products match; reduce
        // first only when an operand could overflow the widened product.
        if ((!normalized_ && comparisonNeedsReduction(num, den)) ||
            (!r.normalized_ && comparisonNeedsReduction(r.num, r.den))) {
            Int an, ad, bn, bd;
            operandParts(an, ad);
            r.operandParts(bn, bd);
            return static_cast<Wide>(an) * bd == static_cast<Wide>(bn) * ad;
        }
        return static_cast<Wide>(num) * r.den == static_cast<Wide>(r.num) * den;
    }
    constexpr bool operator!=(const Rational& r) const {
        return !(*this == r);
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
        return numerator() == r.numerator() && denominator() == r.denominator();
    }

    // --- comparison against floating point ---
    //
    // Exact for every finite value and exponent. The float f is decomposed by
    // frexp into an integer significand times a power of two (f == sig * 2^E,
    // with |sig| < 2^digits so it fits an int128), and num/den <=> sig*2^E is
    // settled by cross-multiplying through the positive denominator in pgl::BigInt
    // so the binary shift can never overflow. No precision is lost regardless of
    // the magnitudes involved. Being members, these also cover the reversed forms
    // (`f == r`, `f < r`, ...) via C++20 rewriting. The float -> Rational
    // constructor is explicit and lossy, so an implicit conversion never silently
    // approximates f here; this overload is the only float comparison path.
    //
    // Not constexpr: pgl::BigInt arithmetic is not constexpr-capable.

    /// @brief Exact three-way comparison against a floating-point value
    /// (partial: NaN compares unordered).
    template <std::floating_point Float>
    std::partial_ordering operator<=>(Float f) const {
        if (std::isnan(f))
            return std::partial_ordering::unordered;
        if (std::isinf(f))
            return f > 0 ? std::partial_ordering::less
                         : std::partial_ordering::greater;

        // 0 and ±1 are settled in the integer domain, skipping the BigInt
        // decomposition below entirely: den > 0, so the answer is the numerator
        // against 0 or against ±den. (-0.0 compares equal to 0 and is handled
        // here as the zero it is.)
        if (f == 0) return compareValues(num, Int(0));
        if (f == 1) return compareValues(num, den);
        if (f == -1) return compareValues(num, -den);

        // f == sig * 2^E exactly.
        int exponent;
        const Float frac = std::frexp(f, &exponent);
        const int digits = pgl::detail::numeric_limits<Float>::digits;
        const pgl::int128 sig = static_cast<pgl::int128>(std::ldexp(frac, digits));
        const int E = exponent - digits;

        // Compare num/den against sig*2^E by multiplying through by den > 0:
        //   sign(num/den - f) == sign(num - den*sig*2^E).
        // The factor of 2^|E| is moved to whichever side keeps the exponent
        // non-negative, and all of it is done in BigInt so nothing overflows.
        pgl::BigInt lhs(num);
        pgl::BigInt rhs = pgl::BigInt(den) * pgl::BigInt(sig);
        if (E >= 0)
            rhs *= detail::pow2(E);
        else
            lhs *= detail::pow2(-E);
        return lhs <=> rhs;
    }

    /// @brief Exact equality against a floating-point value.
    template <std::floating_point Float>
    bool operator==(Float f) const {
        return std::isfinite(f) &&
               (*this <=> f) == std::partial_ordering::equivalent;
    }

    // --- comparison against integers ---
    //
    // An integer converts implicitly to Rational, so `r == i` and `i < r` already
    // work through the same-type operators. `i == r` does not, and neither does
    // `i != r`: C++20 builds the reversed candidate for `==` out of the *declared*
    // operator== functions, and the same-type one only matches after converting
    // the integer, which is not something a reversed candidate is formed through.
    // The relational forms escape this because they are rewritten via operator<=>,
    // which is why `i < r` compiles while `i == r` does not — an asymmetry that
    // surfaces wherever a predicate compares two coordinates of different types,
    // such as Rectangle<Point<Rational>>::boundaryContains(Point<int>).
    //
    // Declaring the integer comparison as its own member template fixes both
    // directions at once, exactly as the floating-point overloads above already
    // do: the template matches the integer directly, so the reversed candidate is
    // formed without any user-defined conversion. `Int` is admitted alongside the
    // built-in integers to cover a storage type that is not one of them, BigInt
    // being the case that matters here.

    /// @brief Whether an integer argument is exactly -1.
    ///
    /// Spelled out rather than written inline so that `-1` is never formed in an
    /// unsigned type, where it would denote the maximum value and warn under
    /// -Wsign-compare. An unsigned argument simply never takes the branch.
    template <class I>
    static constexpr bool isNegativeOne(const I& n) {
        if constexpr (std::is_unsigned_v<I>) {
            return false;
        } else {
            return n == -1;
        }
    }

    /// @brief Exact three-way comparison against an integer value.
    ///
    /// The integer is never turned into a Rational to be compared against. Since
    /// den > 0, `num/den <=> n` is the sign of `num - n*den`, so the whole
    /// comparison stays in the integer domain; going through `Rational(n)`
    /// instead would copy the integer, multiply it by the denominator *and*
    /// multiply the numerator by one — for a BigInt, three heap-capable
    /// operations where none is needed.
    ///
    /// The cases that dominate need no multiplication at all: against 0 the
    /// answer is the sign of the numerator, against ±1 it is the numerator
    /// against ±den, and a fraction that is already an integer compares
    /// directly. Predicates over rational coordinates are full of these — every
    /// `orientationSign(...) == 0` collinearity test is a comparison against
    /// zero.
    template <class I>
        requires(pgl::detail::extended_integral<I> || std::same_as<I, Int>)
    constexpr std::strong_ordering operator<=>(const I& n) const {
        using Wide = pgl::detail::promoted_number_t<Int>;

        // The three constant tests come first: the integer is nearly always a
        // literal, and then they fold away at compile time along with every
        // branch not taken.
        if (n == 0) return compareValues(num, Int(0));
        if (n == 1) return compareValues(num, den);
        if (isNegativeOne(n)) return compareValues(num, -den);
        if (den == 1) return compareValues(num, static_cast<Wide>(n));

        // The general case multiplies through the positive denominator. Only the
        // integer is cast to the wide type: the denominator widens on its own
        // inside the product, and naming Wide for it too would copy an
        // arbitrary-precision denominator for nothing. A deferred fraction is
        // reduced first only when that product could overflow, exactly as the
        // Rational-vs-Rational comparison does.
        if (!normalized_ && comparisonNeedsReduction(num, den)) {
            Int an, ad;
            operandParts(an, ad);
            return compareValues(an, ad * static_cast<Wide>(n));
        }
        return compareValues(num, den * static_cast<Wide>(n));
    }

    /// @brief Exact equality against an integer value.
    ///
    /// Shares every fast path of the three-way comparison above; the orderings
    /// it returns are settled by the same integer comparison an equality test
    /// would perform.
    template <class I>
        requires(pgl::detail::extended_integral<I> || std::same_as<I, Int>)
    constexpr bool operator==(const I& n) const {
        return (*this <=> n) == 0;
    }

    constexpr Rational& operator++() { return *this += 1; }
    constexpr Rational operator++(int) { Rational tmp = *this; ++(*this); return tmp; }

    constexpr Rational& operator--() { return *this -= 1; }
    constexpr Rational operator--(int) { Rational tmp = *this; --(*this); return tmp; }

    /**
     * @brief Output format: num/den or just num if den==1
     */
    friend std::ostream& operator<<(std::ostream& os, const Rational& r) {
        const Int n = r.numerator();
        const Int d = r.denominator();
        if (d == 1)
            return os << n;
        else
            return os << n << "/" << d;
    }

    /**
     * @brief Input format: num/den or integer
     */
    friend std::istream& operator>>(std::istream& is, Rational& r) {
        Int n, d = 1;
        char sep = 0;

        if (!(is >> n)) return is;

        // A bare integer (no "/den" suffix) is a valid, successful parse, but
        // peek() at end-of-stream sets failbit even though nothing is wrong;
        // clear it so a plain integer at the end of the stream doesn't fail.
        if (!is.eof()) {
            if (is.peek() == '/') {
                is >> sep >> d;
            } else if (is.fail()) {
                is.clear(is.rdstate() & ~std::ios::failbit);
            }
        }

        r = Rational(n, d);
        return is;
    }
};

/**
 * @name Rational deduction guides
 *
 * Explicit guides that pin class template argument deduction so it is
 * consistent across compilers. The numerator constructor deliberately makes
 * `Int` non-deducible (see its comment), so CTAD for a bare, non-built-in
 * integer numerator such as `pgl::BigInt` is supplied here instead: as an
 * *explicit* guide, its constraint is honored even by compilers (clang 18)
 * that mishandle constraints on the guides synthesized from constrained
 * constructors. Floating-point and built-in integer arguments are excluded so
 * they continue to deduce the default `Rational<int64_t>` via the other
 * constructors' guides.
 * @{
 */
template <class T>
    requires(!pgl::detail::extended_integral<T> && !std::floating_point<T>
             && !RationalConcept<T>)
Rational(T) -> Rational<T>;
/** @} */



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
    static constexpr int bits = pgl::detail::numeric_limits<T>::digits;
    static constexpr int rounded = round_up_bits(bits);

public:
    using type = typename select_int_ge<rounded>::type;
};

template <typename T>
using to_integer_with_digits_t = typename to_integer_with_digits<T>::type;

/** @brief Exact, overflow-free result used when integral coordinates require fractions. */
using ERational = Rational<BigInt>;

/**
 * @brief Default result type for an operation that may require division.
 *
 * Floating-point coordinates retain their native type instead of being
 * rationalized after they have already been approximated. Rational coordinates
 * are already closed under division and likewise retain their native type.
 * Integral coordinate types, including @ref BigInt, use the overflow-free
 * @ref ERational type so fractional results remain exact.
 *
 * Users may specialize this trait for custom coordinate types.
 */
template <class Number>
struct DivisionResult {
    using NumberType = std::remove_cvref_t<Number>;
    using type = std::conditional_t<
        std::floating_point<NumberType> || RationalConcept<NumberType>,
        NumberType,
        ERational>;
};

/** @brief Convenience alias for @ref DivisionResult. */
template <class Number>
using division_result_t = typename DivisionResult<Number>::type;

}// pgl

template<class U, class V>
struct std::common_type<pgl::Rational<U>, pgl::Rational<V>>{
    using type = pgl::Rational<std::common_type_t<U,V>>;
};

// Mixing a Rational with a floating-point type yields the floating-point type:
// the exact value is intentionally abandoned (the float→Rational conversion is
// explicit and lossy), so promotion collapses toward the inexact representation
// rather than silently approximating the float as a Rational.
template<class U, std::floating_point F>
struct std::common_type<pgl::Rational<U>, F>{
    using type = F;
};
template<std::floating_point F, class V>
struct std::common_type<F, pgl::Rational<V>>{
    using type = F;
};

/**
 * @brief numeric_limits specialization for Rational.
 */
template <class Int>
struct std::numeric_limits<pgl::Rational<Int>> {
    static constexpr bool is_specialized = true;

    static constexpr pgl::Rational<Int> min() noexcept {
        return pgl::Rational<Int>(pgl::detail::numeric_limits<Int>::min(), 1);
    }

    static constexpr pgl::Rational<Int> max() noexcept {
        return pgl::Rational<Int>(pgl::detail::numeric_limits<Int>::max(), 1);
    }

    static constexpr pgl::Rational<Int> lowest() noexcept {
        return min();
    }

    static constexpr int digits = pgl::detail::numeric_limits<Int>::digits;
    static constexpr int digits10 = pgl::detail::numeric_limits<Int>::digits10;

    static constexpr bool is_signed = pgl::detail::numeric_limits<Int>::is_signed;
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
    static constexpr bool is_bounded = pgl::detail::numeric_limits<Int>::is_bounded;
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
