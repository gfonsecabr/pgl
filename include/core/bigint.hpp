#pragma once

#include "core/numeric.hpp"

/**
 * @file bigint.hpp
 * @brief Arbitrary precision signed integers, optimized for small values.
 *
 * BigInt stores its magnitude in a single ::pgl::int128 whenever it fits, so the
 * common case (numbers that never overflow 128 bits) runs at native speed with
 * no heap allocation. Larger magnitudes spill into a base-2^62 limb vector. The
 * sign is kept separately, so the stored magnitude is always non-negative.
 *
 * The public interface mirrors the one offered by ::pgl::Rational: value
 * construction from any integer, the usual arithmetic and comparison operators,
 * compound assignment, increment/decrement, conversions, and stream I/O.
 */

#include <compare>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "numeric.hpp"

// Marks the rarely-taken, out-of-line limb-path helpers cold so the int128 fast
// paths stay small enough to inline into hot callers. GCC and clang honour the
// hint; other compilers (e.g. MSVC on the Boost int128 fallback) ignore it.
#if defined(__GNUC__) || defined(__clang__)
#define PGL_BIGINT_COLD [[gnu::noinline, gnu::cold]]
#else
#define PGL_BIGINT_COLD
#endif

namespace pgl {

namespace detail {

/**
 * @brief Minimal owning limb store used by ::pgl::BigInt.
 *
 * This is a tiny stand-in for @c std::vector<pgl::int128> that occupies a single
 * pointer (8 bytes) instead of three, so a BigInt whose magnitude fits in its
 * inline ::pgl::int128 stays 32 bytes rather than 48. Shrinking the object cuts
 * the per-copy data movement that dominates limb-free workloads (the common
 * case), where the store is always empty.
 *
 * The limb count is kept in the allocation itself: a block of @c n+1 int128s is
 * allocated, slot 0 holds @c n, and @ref data_ points at slot 1 (the first
 * limb). An empty store is a null @ref data_ and owns nothing. Only the handful
 * of @c std::vector operations BigInt actually uses are provided, plus
 * @c begin / @c end so the value is range-iterable (e.g. by @c std::hash).
 */
class LimbStore {
    pgl::int128* data_ = nullptr;   ///< Points at limb 0; data_[-1] holds the count.

    std::size_t count() const { return static_cast<std::size_t>(data_[-1]); }

    void allocate(std::size_t n) {
        pgl::int128* base = new pgl::int128[n + 1];
        base[0] = static_cast<pgl::int128>(n);
        data_ = base + 1;
    }

    void copyFrom(const pgl::int128* src, std::size_t n) {
        allocate(n);
        for (std::size_t i = 0; i < n; ++i) {
            data_[i] = src[i];
        }
    }

    void release() {
        if (data_) {
            delete[] (data_ - 1);
            data_ = nullptr;
        }
    }

public:
    LimbStore() = default;
    LimbStore(const LimbStore& o) {
        if (o.data_) {
            copyFrom(o.data_, o.count());
        }
    }
    LimbStore(LimbStore&& o) noexcept : data_(o.data_) { o.data_ = nullptr; }
    LimbStore& operator=(const LimbStore& o) {
        if (this != &o) {
            release();
            if (o.data_) {
                copyFrom(o.data_, o.count());
            }
        }
        return *this;
    }
    LimbStore& operator=(LimbStore&& o) noexcept {
        if (this != &o) {
            release();
            data_ = o.data_;
            o.data_ = nullptr;
        }
        return *this;
    }
    ~LimbStore() { release(); }

    /// @brief Replace the contents with a copy of a std::vector's limbs.
    LimbStore& operator=(const std::vector<pgl::int128>& v) {
        release();
        if (!v.empty()) {
            copyFrom(v.data(), v.size());
        }
        return *this;
    }

    bool empty() const { return data_ == nullptr; }
    std::size_t size() const { return data_ ? count() : 0; }
    pgl::int128& operator[](std::size_t i) { return data_[i]; }
    const pgl::int128& operator[](std::size_t i) const { return data_[i]; }
    const pgl::int128* begin() const { return data_; }
    const pgl::int128* end() const { return data_ ? data_ + count() : nullptr; }
    void clear() { release(); }

    /// @brief Set the store to @p n copies of @p val.
    void assign(std::size_t n, pgl::int128 val) {
        release();
        if (n) {
            allocate(n);
            for (std::size_t i = 0; i < n; ++i) {
                data_[i] = val;
            }
        }
    }
};

}  // namespace detail

/**
 * @brief Arbitrary precision signed integer.
 *
 * The value is stored in sign-magnitude form:
 * - @ref negative_ holds the sign (always false when the value is zero);
 * - when @ref limbs_ is empty the magnitude is @ref small_, a non-negative
 *   ::pgl::int128 (fast path, no allocation);
 * - otherwise the magnitude is the little-endian base-2^62 number spelled by
 *   @ref limbs_, whose most significant limb is non-zero and which represents a
 *   value too large to fit in a ::pgl::int128.
 */
class BigInt {
private:
    pgl::int128 small_ = 0;            ///< Magnitude when @ref limbs_ is empty (>= 0).
    detail::LimbStore limbs_;          ///< Magnitude limbs, base 2^62, little-endian.
    bool negative_ = false;            ///< Sign; false when the value is zero.

    using Limbs = std::vector<pgl::int128>;

    friend struct std::hash<BigInt>;

    /// @brief Number of value bits stored per limb.
    static constexpr int kLimbBits = 62;

    /// @brief The limb base, 2^62.
    static pgl::int128 base() { return pgl::int128(1) << kLimbBits; }

    /// @brief Largest representable single-limb digit, 2^62 - 1.
    static pgl::int128 limbMask() { return base() - 1; }

    /// @brief Largest value a ::pgl::int128 can hold.
    static pgl::int128 int128Max() { return std::numeric_limits<pgl::int128>::max(); }

    // --- magnitude helpers (operate on normalized little-endian limb vectors) ---

    /// @brief Drop most significant zero limbs so the magnitude is canonical.
    static void trim(Limbs& v) {
        while (!v.empty() && v.back() == 0) {
            v.pop_back();
        }
    }

    /// @brief Compare two magnitudes; returns -1, 0, or 1.
    static int cmpMag(const Limbs& a, const Limbs& b) {
        if (a.size() != b.size()) {
            return a.size() < b.size() ? -1 : 1;
        }
        for (std::size_t i = a.size(); i-- > 0;) {
            if (a[i] != b[i]) {
                return a[i] < b[i] ? -1 : 1;
            }
        }
        return 0;
    }

    /// @brief Add two magnitudes.
    static Limbs addMag(const Limbs& a, const Limbs& b) {
        Limbs r;
        const std::size_t n = a.size() < b.size() ? b.size() : a.size();
        r.reserve(n + 1);
        pgl::int128 carry = 0;
        for (std::size_t i = 0; i < n; ++i) {
            pgl::int128 sum = carry;
            if (i < a.size()) sum += a[i];
            if (i < b.size()) sum += b[i];
            r.push_back(sum & limbMask());
            carry = sum >> kLimbBits;
        }
        if (carry != 0) {
            r.push_back(carry);
        }
        return r;
    }

    /// @brief Subtract magnitude @p b from @p a, which must satisfy a >= b.
    static Limbs subMag(const Limbs& a, const Limbs& b) {
        Limbs r;
        r.reserve(a.size());
        pgl::int128 borrow = 0;
        for (std::size_t i = 0; i < a.size(); ++i) {
            pgl::int128 d = a[i] - borrow - (i < b.size() ? b[i] : pgl::int128(0));
            if (d < 0) {
                d += base();
                borrow = 1;
            } else {
                borrow = 0;
            }
            r.push_back(d);
        }
        trim(r);
        return r;
    }

    /// @brief Multiply two magnitudes (schoolbook).
    static Limbs mulMag(const Limbs& a, const Limbs& b) {
        if (a.empty() || b.empty()) {
            return {};
        }
        Limbs r(a.size() + b.size(), pgl::int128(0));
        for (std::size_t i = 0; i < a.size(); ++i) {
            pgl::int128 carry = 0;
            for (std::size_t j = 0; j < b.size(); ++j) {
                pgl::int128 cur = r[i + j] + a[i] * b[j] + carry;
                r[i + j] = cur & limbMask();
                carry = cur >> kLimbBits;
            }
            r[i + b.size()] += carry;
        }
        trim(r);
        return r;
    }

    /// @brief Highest set bit position of a positive ::pgl::int128.
    static int topBit(pgl::int128 x) {
        int n = 0;
        while (x > 1) {
            x >>= 1;
            ++n;
        }
        return n;
    }

    /// @brief Number of bits needed to represent a magnitude (0 for zero).
    static std::size_t bitLengthMag(const Limbs& v) {
        if (v.empty()) {
            return 0;
        }
        return (v.size() - 1) * kLimbBits + static_cast<std::size_t>(topBit(v.back())) + 1;
    }

    /// @brief Test bit @p i of a magnitude.
    static bool testBitMag(const Limbs& v, std::size_t i) {
        const std::size_t limb = i / kLimbBits;
        const std::size_t off = i % kLimbBits;
        if (limb >= v.size()) {
            return false;
        }
        return ((v[limb] >> static_cast<int>(off)) & 1) != 0;
    }

    /// @brief Magnitude division with remainder via binary int64_t division.
    ///
    /// The divisor must be non-zero. Returns { quotient, remainder }. This is
    /// O(bits^2) but only runs once magnitudes exceed 128 bits, which is rare.
    static std::pair<Limbs, Limbs> divmodMag(const Limbs& n, const Limbs& d) {
        Limbs q;
        Limbs r;
        const Limbs one = {pgl::int128(1)};
        for (std::size_t bit = bitLengthMag(n); bit-- > 0;) {
            r = addMag(r, r);                       // r <<= 1
            if (testBitMag(n, bit)) {
                r = addMag(r, one);
            }
            q = addMag(q, q);                       // q <<= 1
            if (cmpMag(r, d) >= 0) {
                r = subMag(r, d);
                q = addMag(q, one);
            }
        }
        trim(q);
        trim(r);
        return {q, r};
    }

    // --- conversions between the small and limb representations ---

    /// @brief Return the magnitude as a normalized limb vector.
    Limbs magToLimbs() const {
        if (!limbs_.empty()) {
            return Limbs(limbs_.begin(), limbs_.end());
        }
        Limbs v;
        pgl::int128 x = small_;
        while (x > 0) {
            v.push_back(x & limbMask());
            x >>= kLimbBits;
        }
        return v;
    }

    /// @brief Reconstruct an int128 from limbs, reporting whether it fits.
    static bool limbsFitInt128(const Limbs& v, pgl::int128& out) {
        pgl::int128 acc = 0;
        const pgl::int128 maxv = int128Max();
        for (std::size_t i = v.size(); i-- > 0;) {
            if (acc > maxv / base()) {
                return false;
            }
            acc *= base();
            if (acc > maxv - v[i]) {
                return false;
            }
            acc += v[i];
        }
        out = acc;
        return true;
    }

    /// @brief Assign a magnitude (given as limbs) and a sign, choosing the
    /// most compact representation.
    void setFromLimbs(Limbs v, bool neg) {
        trim(v);
        if (v.empty()) {
            small_ = 0;
            limbs_.clear();
            negative_ = false;
            return;
        }
        pgl::int128 fitted;
        if (limbsFitInt128(v, fitted)) {
            small_ = fitted;
            limbs_.clear();
        } else {
            small_ = 0;
            limbs_ = std::move(v);
        }
        negative_ = neg;
    }

    /// @brief Build a value from a non-negative int128 magnitude and a sign.
    static BigInt fromSmall(pgl::int128 magnitude, bool neg) {
        BigInt b;
        b.small_ = magnitude;
        b.negative_ = (magnitude != 0) && neg;
        return b;
    }

    /// @brief Compare magnitudes of two BigInt values; returns -1, 0, or 1.
    int compareMag(const BigInt& o) const {
        if (limbs_.empty() && o.limbs_.empty()) {
            if (small_ == o.small_) return 0;
            return small_ < o.small_ ? -1 : 1;
        }
        return compareMagGeneral(o);
    }

    /// @brief Cold out-of-line magnitude comparison once a value spilled to limbs.
    /// Keeps the int128 fast path tiny so operator== / operator<=> inline into
    /// hot callers (gcd loop conditions, Rational comparisons).
    PGL_BIGINT_COLD
    int compareMagGeneral(const BigInt& o) const {
        return cmpMag(magToLimbs(), o.magToLimbs());
    }

    /// @brief Decimal spelling of the magnitude (no sign), used for large values.
    std::string magToDecimalString() const {
        Limbs v = magToLimbs();
        if (v.empty()) {
            return "0";
        }
        const Limbs divisor = {pgl::int128(int64_t(1000000000000000000))};   // 10^18
        std::vector<int64_t> chunks;
        while (!v.empty()) {
            auto [quotient, remainder] = divmodMag(v, divisor);
            chunks.push_back(remainder.empty() ? int64_t(0) : static_cast<int64_t>(remainder[0]));
            v = std::move(quotient);
        }
        std::string s = std::to_string(chunks.back());
        for (std::size_t i = chunks.size() - 1; i-- > 0;) {
            std::string part = std::to_string(chunks[i]);
            s += std::string(18 - part.size(), '0');
            s += part;
        }
        return s;
    }

    /// @brief General (limb based) addition used when the fast path overflows.
    ///
    /// Kept out-of-line and cold so the int128 fast path in operator+/- stays
    /// small enough to inline into hot callers such as orientationSign.
    PGL_BIGINT_COLD
    static BigInt addGeneral(const BigInt& a, const BigInt& b) {
        Limbs av = a.magToLimbs();
        Limbs bv = b.magToLimbs();
        BigInt r;
        if (a.negative_ == b.negative_) {
            r.setFromLimbs(addMag(av, bv), a.negative_);
        } else {
            const int c = cmpMag(av, bv);
            if (c > 0) {
                r.setFromLimbs(subMag(av, bv), a.negative_);
            } else if (c < 0) {
                r.setFromLimbs(subMag(bv, av), b.negative_);
            }
            // c == 0 leaves r as the default zero.
        }
        return r;
    }

    /// @brief Quotient of two non-negative int128 magnitudes. When both fit in
    /// 64 bits (overwhelmingly the common case) this issues a single hardware
    /// 64-bit divide instead of the ~3x slower 128-bit __divti3.
    static pgl::int128 quotSmall(pgl::int128 a, pgl::int128 b) {
        const std::uint64_t u64max = std::numeric_limits<std::uint64_t>::max();
        if (a <= u64max && b <= u64max) {
            return pgl::int128(static_cast<std::uint64_t>(a) / static_cast<std::uint64_t>(b));
        }
        return a / b;
    }

    /// @brief Remainder of two non-negative int128 magnitudes; see @ref quotSmall.
    static pgl::int128 remSmall(pgl::int128 a, pgl::int128 b) {
        const std::uint64_t u64max = std::numeric_limits<std::uint64_t>::max();
        if (a <= u64max && b <= u64max) {
            return pgl::int128(static_cast<std::uint64_t>(a) % static_cast<std::uint64_t>(b));
        }
        return a % b;
    }

    /// @brief Truncated division producing both quotient and remainder.
    ///
    /// Only reached once a magnitude exceeds int128; operator/ and operator%
    /// handle the common small case directly, so keep this cold and out-of-line.
    PGL_BIGINT_COLD
    static void divmod(const BigInt& a, const BigInt& b, BigInt& q, BigInt& r) {
        if (b.isZero()) {
            throw std::domain_error("pgl::BigInt: division by zero");
        }
        const bool quotientNegative = a.negative_ != b.negative_;
        const bool remainderNegative = a.negative_;   // remainder follows the dividend
        if (a.limbs_.empty() && b.limbs_.empty()) {
            q = fromSmall(quotSmall(a.small_, b.small_), quotientNegative);
            r = fromSmall(remSmall(a.small_, b.small_), remainderNegative);
            return;
        }
        auto [qm, rm] = divmodMag(a.magToLimbs(), b.magToLimbs());
        q.setFromLimbs(std::move(qm), quotientNegative);
        r.setFromLimbs(std::move(rm), remainderNegative);
    }

public:
    /// @brief Default constructor (zero).
    BigInt() = default;

    /// @brief Construct from any integer (including ::pgl::int128).
    BigInt(pgl::detail::extended_integral auto value) {
        pgl::int128 x = static_cast<pgl::int128>(value);
        if (x == 0) {
            return;
        }
        if (x == std::numeric_limits<pgl::int128>::min()) {
            // |min| == 2^127 does not fit in a signed int128, so spell it out.
            negative_ = true;
            limbs_.assign(3, pgl::int128(0));
            limbs_[2] = pgl::int128(1) << 3;   // 8 * 2^124 == 2^127
        } else {
            negative_ = x < 0;
            small_ = x < 0 ? -x : x;
        }
    }

    // --- inspectors ---

    /// @brief Whether the value is zero.
    bool isZero() const { return limbs_.empty() && small_ == 0; }

    /// @brief Whether the value is strictly negative.
    bool isNegative() const { return negative_; }

    /// @brief Whether the magnitude fits in a single ::pgl::int128.
    bool fitsInt128() const { return limbs_.empty(); }

    /// @brief Whether the magnitude fits in 63 bits (i.e. in an int64_t).
    ///
    /// Used by Rational to decide when deferred reduction has become urgent:
    /// reducing operands while they still fit here keeps the impending multiply
    /// or cross-addition within the inline int128 store, off the heap limb path.
    bool fitsInt64() const {
        return limbs_.empty() && small_ < (pgl::int128(1) << 63);
    }

    /// @brief Sign of the value: -1, 0, or 1.
    int sign() const {
        if (isZero()) return 0;
        return negative_ ? -1 : 1;
    }

    /// @brief Absolute value.
    BigInt abs() const {
        BigInt r = *this;
        r.negative_ = false;
        return r;
    }

    // --- conversions ---

    /// @brief Convert to ::pgl::int128 (low bits, with sign, when it overflows).
    explicit operator pgl::int128() const {
        pgl::int128 m = 0;
        if (limbs_.empty()) {
            m = small_;
        } else {
            pgl::int128 weight = 1;
            for (std::size_t i = 0; i < limbs_.size() && i < 3; ++i) {
                m += limbs_[i] * weight;
                weight *= base();
            }
        }
        return negative_ ? -m : m;
    }

    explicit operator int() const { return static_cast<int>(static_cast<pgl::int128>(*this)); }
    explicit operator int64_t() const { return static_cast<int64_t>(static_cast<pgl::int128>(*this)); }
    explicit operator bool() const { return !isZero(); }

    /// @brief Convert to floating point.
    explicit operator long double() const {
        long double d = 0;
        if (limbs_.empty()) {
            d = static_cast<long double>(small_);
        } else {
            const long double b = static_cast<long double>(base());
            for (std::size_t i = limbs_.size(); i-- > 0;) {
                d = d * b + static_cast<long double>(limbs_[i]);
            }
        }
        return negative_ ? -d : d;
    }
    explicit operator double() const { return static_cast<double>(static_cast<long double>(*this)); }
    explicit operator float() const { return static_cast<float>(static_cast<long double>(*this)); }

    // --- arithmetic ---

    BigInt operator-() const {
        BigInt r = *this;
        if (!r.isZero()) {
            r.negative_ = !r.negative_;
        }
        return r;
    }

    BigInt operator+() const { return *this; }

    friend BigInt operator+(const BigInt& a, const BigInt& b) {
        if (a.limbs_.empty() && b.limbs_.empty()) {
            const pgl::int128 ma = a.small_;
            const pgl::int128 mb = b.small_;
            if (a.negative_ == b.negative_) {
                if (ma <= int128Max() - mb) {
                    return fromSmall(ma + mb, a.negative_);
                }
                // magnitude overflow: fall through to the limb path
            } else if (ma >= mb) {
                return fromSmall(ma - mb, a.negative_);
            } else {
                return fromSmall(mb - ma, b.negative_);
            }
        }
        return addGeneral(a, b);
    }

    friend BigInt operator-(const BigInt& a, const BigInt& b) {
        if (a.limbs_.empty() && b.limbs_.empty()) {
            const pgl::int128 ma = a.small_;
            const pgl::int128 mb = b.small_;
            // a - b == a + (-b); replicate operator+'s small path with b's sign
            // flipped, so we avoid materialising the negated temporary -b.
            if (a.negative_ != b.negative_) {
                if (ma <= int128Max() - mb) {
                    return fromSmall(ma + mb, a.negative_);
                }
            } else if (ma >= mb) {
                return fromSmall(ma - mb, a.negative_);
            } else {
                return fromSmall(mb - ma, !a.negative_);
            }
        }
        return subGeneral(a, b);
    }

    /// @brief Cold out-of-line subtraction fallback (a magnitude spilled to limbs).
    PGL_BIGINT_COLD
    static BigInt subGeneral(const BigInt& a, const BigInt& b) { return a + (-b); }

    friend BigInt operator*(const BigInt& a, const BigInt& b) {
        const bool neg = a.negative_ != b.negative_;
        if (a.limbs_.empty() && b.limbs_.empty()) {
            const pgl::int128 ma = a.small_;
            const pgl::int128 mb = b.small_;
#if defined(__SIZEOF_INT128__)
            // Native int128: both magnitudes are non-negative, so a signed-overflow
            // check on the product is exactly the "does it still fit" test we want,
            // and __builtin_mul_overflow performs it without a 128-bit division.
            pgl::int128 prod;
            if (!__builtin_mul_overflow(ma, mb, &prod)) {
                return fromSmall(prod, neg);
            }
#else
            // Boost fallback (no native int128, no overflow builtin): guard the
            // product with a division, matching operator+'s overflow style.
            if (ma == 0 || mb <= int128Max() / ma) {
                return fromSmall(ma * mb, neg);
            }
#endif
            // magnitude overflow: fall through to the limb path
        }
        return mulGeneral(a, b, neg);
    }

    /// @brief Cold out-of-line schoolbook multiplication (a magnitude exceeds int128).
    PGL_BIGINT_COLD
    static BigInt mulGeneral(const BigInt& a, const BigInt& b, bool neg) {
        BigInt out;
        out.setFromLimbs(mulMag(a.magToLimbs(), b.magToLimbs()), neg);
        return out;
    }

    friend BigInt operator/(const BigInt& a, const BigInt& b) {
        if (a.limbs_.empty() && b.limbs_.empty()) {
            if (b.small_ == 0) {
                throw std::domain_error("pgl::BigInt: division by zero");
            }
            // The quotient is all the caller wants, so skip computing a remainder.
            return fromSmall(quotSmall(a.small_, b.small_), a.negative_ != b.negative_);
        }
        BigInt q;
        BigInt r;
        divmod(a, b, q, r);
        return q;
    }

    friend BigInt operator%(const BigInt& a, const BigInt& b) {
        if (a.limbs_.empty() && b.limbs_.empty()) {
            if (b.small_ == 0) {
                throw std::domain_error("pgl::BigInt: division by zero");
            }
            // The remainder is all the caller wants, so skip computing a quotient.
            return fromSmall(remSmall(a.small_, b.small_), a.negative_);
        }
        BigInt q;
        BigInt r;
        divmod(a, b, q, r);
        return r;
    }

    BigInt& operator+=(const BigInt& o) { return *this = *this + o; }
    BigInt& operator-=(const BigInt& o) { return *this = *this - o; }
    BigInt& operator*=(const BigInt& o) { return *this = *this * o; }
    BigInt& operator/=(const BigInt& o) { return *this = *this / o; }
    BigInt& operator%=(const BigInt& o) { return *this = *this % o; }

    BigInt& operator++() { return *this += BigInt(1); }
    BigInt operator++(int) { BigInt tmp = *this; ++(*this); return tmp; }
    BigInt& operator--() { return *this -= BigInt(1); }
    BigInt operator--(int) { BigInt tmp = *this; --(*this); return tmp; }

    // --- comparison ---

    std::strong_ordering operator<=>(const BigInt& o) const {
        if (negative_ != o.negative_) {
            return negative_ ? std::strong_ordering::less : std::strong_ordering::greater;
        }
        int c = compareMag(o);
        if (negative_) {
            c = -c;   // among negatives, the larger magnitude is the smaller value
        }
        if (c < 0) return std::strong_ordering::less;
        if (c > 0) return std::strong_ordering::greater;
        return std::strong_ordering::equal;
    }

    bool operator==(const BigInt& o) const {
        return negative_ == o.negative_ && compareMag(o) == 0;
    }

    // --- stream I/O ---

    friend std::ostream& operator<<(std::ostream& os, const BigInt& b) {
        if (b.negative_) {
            os << '-';
        }
        if (b.limbs_.empty()) {
            os << b.small_;   // uses the pgl::int128 stream operator
        } else {
            os << b.magToDecimalString();
        }
        return os;
    }

    friend std::istream& operator>>(std::istream& is, BigInt& b) {
        std::string token;
        if (!(is >> token)) {
            return is;
        }
        std::size_t i = 0;
        bool neg = false;
        if (i < token.size() && (token[i] == '+' || token[i] == '-')) {
            neg = token[i] == '-';
            ++i;
        }
        if (i >= token.size()) {
            is.setstate(std::ios::failbit);
            return is;
        }
        BigInt result;
        const BigInt ten(10);
        for (; i < token.size(); ++i) {
            if (token[i] < '0' || token[i] > '9') {
                is.setstate(std::ios::failbit);
                return is;
            }
            result = result * ten + BigInt(token[i] - '0');
        }
        b = neg ? -result : result;
        return is;
    }
};

/// @brief Free-function absolute value, matching the integer helpers.
inline BigInt abs(const BigInt& v) { return v.abs(); }

}  // namespace pgl

#undef PGL_BIGINT_COLD
