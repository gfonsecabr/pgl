#pragma once

#include "core/handle.hpp"

/**
 * @file radixsort.hpp
 * @brief Least-significant-digit radix sort over integral keys.
 *
 * Ordering by the bits of a key rather than by comparing keys costs a fixed
 * number of linear passes instead of n log n comparisons, and spends no
 * branches on data. Several places in the library sort large lists whose sort
 * key is an integral coordinate -- a shape tree's box ends, a hull's points --
 * and each of them reaches for this rather than for `std::sort`. It carries no
 * geometry: the caller says which integral field of its element is the key.
 */

#include <algorithm>
#include <cstddef>
#include <type_traits>
#include <utility>
#include <vector>


namespace pgl {

namespace detail {

// The key an integral coordinate is radix sorted by: the same bits for an
// unsigned type, and the sign bit flipped for a signed one, so that the
// unsigned order of the keys is the signed order of the values.
template <class Integral>
[[nodiscard]] constexpr auto radixKey(Integral value) {
    using Unsigned = std::make_unsigned_t<Integral>;
    const auto bits = static_cast<Unsigned>(value);
    if constexpr (std::is_signed_v<Integral>) {
        return static_cast<Unsigned>(bits ^ (Unsigned{1} << (8 * sizeof(Unsigned) - 1)));
    } else {
        return bits;
    }
}

// Whether a list of `T` sorted by an integral key is worth radix sorting at
// all: the key must be one this can take bytes of, and an element must be
// cheap to copy, since the passes relocate every element bodily and a type
// whose copy allocates would lose more there than the comparisons save.
template <class T, class Key>
concept RadixSortable = std::is_integral_v<Key> && sizeof(Key) <= 8
                     && std::is_trivially_copyable_v<T>;

// Sorts `v` by an integral key, one byte at a time from the least significant,
// which orders n elements in a number of linear passes fixed by the width of
// the key rather than in n log n comparisons. `scratch` is the alternate buffer
// the passes ping-pong between; it is the caller's so that sorting several
// lists reuses one allocation.
//
// The sort is stable, so sorting by one key and then by a second orders the
// elements by the second key with the first breaking its ties -- which is how
// a lexicographic order over two coordinates is reached, least significant
// coordinate first.
//
// A pass whose digit is the same for every element would only copy the array,
// so the histogram is taken for all digits in one sweep and those passes are
// skipped. Coordinates that share a sign and span less than their type -- which
// is to say most of them -- therefore cost fewer passes than the width implies.
template <class T, class KeyFn>
void radixSort(std::vector<T>& v, std::vector<T>& scratch, KeyFn key) {
    using Key = decltype(key(std::declval<const T&>()));
    static constexpr int digits = static_cast<int>(sizeof(Key));
    const std::size_t n = v.size();
    if (n == 0) {
        return;
    }

    std::size_t counts[digits][256] = {};
    for (const T& element : v) {
        const Key k = key(element);
        for (int digit = 0; digit < digits; ++digit) {
            ++counts[digit][(k >> (8 * digit)) & 0xFF];
        }
    }

    scratch.resize(n);
    T* from = v.data();
    T* to = scratch.data();
    for (int digit = 0; digit < digits; ++digit) {
        std::size_t* count = counts[digit];
        if (count[(key(from[0]) >> (8 * digit)) & 0xFF] == n) {
            continue;  // One bucket holds everything: the pass cannot reorder.
        }
        std::size_t offset = 0;
        for (int bucket = 0; bucket < 256; ++bucket) {
            const std::size_t size = count[bucket];
            count[bucket] = offset;
            offset += size;
        }
        for (std::size_t i = 0; i < n; ++i) {
            to[count[(key(from[i]) >> (8 * digit)) & 0xFF]++] = from[i];
        }
        std::swap(from, to);
    }
    if (from != v.data()) {
        std::copy(from, from + n, v.begin());
    }
}

}  // namespace detail

}  // namespace pgl
