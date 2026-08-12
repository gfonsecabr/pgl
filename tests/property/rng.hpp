#pragma once

/**
 * @file rng.hpp
 * @brief Deterministic pseudo-random source for the property harness.
 *
 * The harness promises that `--seed N` reproduces a run exactly, on any
 * compiler and any standard library. `<random>` cannot keep that promise: the
 * *engines* are specified bit-for-bit, but the *distributions* are not, so
 * `std::uniform_int_distribution` on libstdc++ and on libc++ consume a
 * different number of engine outputs and hand back different values from the
 * same seed. A witness reported by the g++ build would then not reproduce under
 * clang++, which is precisely when one wants to reproduce it.
 *
 * So the generator is spelled out here: splitmix64 for the stream, and
 * Lemire-style rejection for the bounded draw. Both are a few lines, both are
 * fully specified by this file, and neither depends on the standard library
 * beyond fixed-width integers.
 */

#include <cstdint>
#include <string>
#include <vector>

namespace pglprop {

/**
 * @brief Reproducible 64-bit random stream.
 *
 * Every draw the harness makes goes through one of these methods, so the whole
 * run is a pure function of the seed.
 */
class Rng {
public:
    /**
     * @brief Seeds the stream.
     *
     * @param seed Any 64-bit value; zero is remapped, splitmix64 being weakest
     *        there.
     */
    explicit Rng(std::uint64_t seed)
        : state_(seed == 0 ? 0x9E3779B97F4A7C15ULL : seed) {}

    /** @brief Returns the next 64-bit output and advances the stream. */
    std::uint64_t next() {
        state_ += 0x9E3779B97F4A7C15ULL;
        std::uint64_t z = state_;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }

    /**
     * @brief Returns a uniform value in `[0, bound)`.
     *
     * Rejection sampling, so the result is exactly uniform rather than uniform
     * up to a modulo bias — the bias would otherwise fall on the small
     * coordinates, which is where the interesting degeneracies live.
     *
     * @param bound Exclusive upper bound; `0` returns `0`.
     */
    std::uint64_t below(std::uint64_t bound) {
        if (bound <= 1) {
            return 0;
        }
        // Largest multiple of bound that fits, so everything at or above it is
        // rejected rather than folded back in.
        const std::uint64_t limit = UINT64_MAX - (UINT64_MAX % bound) - 1;
        std::uint64_t draw = next();
        while (draw > limit) {
            draw = next();
        }
        return draw % bound;
    }

    /**
     * @brief Returns a uniform integer in the closed range `[lo, hi]`.
     *
     * @param lo Lower bound, inclusive.
     * @param hi Upper bound, inclusive; must not be below @p lo.
     */
    int inRange(int lo, int hi) {
        if (hi <= lo) {
            return lo;
        }
        const std::uint64_t span = static_cast<std::uint64_t>(hi - lo) + 1;
        return lo + static_cast<int>(below(span));
    }

    /**
     * @brief Returns `true` with probability @p percent / 100.
     *
     * @param percent Probability in percent, clamped to `[0, 100]` by the
     *        comparison itself.
     */
    bool chance(int percent) { return inRange(1, 100) <= percent; }

    /**
     * @brief Returns an index into a range of @p size elements.
     *
     * @param size Number of elements; `0` returns `0`, which the caller must
     *        not use.
     */
    std::size_t index(std::size_t size) { return static_cast<std::size_t>(below(size)); }

private:
    std::uint64_t state_;
};

}  // namespace pglprop
