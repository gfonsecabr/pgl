#pragma once

#include "core/forward.hpp"

/**
 * @file handle.hpp
 * @brief Strongly typed index handles shared by the topological data structures.
 *
 * A @ref pgl::detail::Handle is an index that knows which family of cells it
 * indexes. @ref pgl::Arrangement and @ref pgl::Triangulation both hand out
 * handles rather than raw indices, so a vertex handle cannot bind where a face
 * or triangle one is meant, and the two families can take part in overload
 * resolution.
 */

#include <cstddef>
#include <cstdint>
#include <functional>

namespace pgl {

namespace detail {

/**
 * @brief A strongly typed index handle.
 *
 * @tparam Tag Empty type that makes each handle family distinct, so a vertex
 *         handle cannot bind where a face handle is meant and the families can
 *         take part in overload resolution.
 *
 * A default-constructed handle is the invalid one, which removes the need for a
 * `NO_FACE`-style sentinel constant per family. The class costs nothing:
 * `sizeof(Handle) == sizeof(std::uint32_t)` and it is trivially copyable, so a
 * `std::vector<Handle>` has exactly the layout of a `std::vector<std::uint32_t>`
 * and the topology arrays stay as dense as they would be with raw indices.
 *
 * Construction and @ref index are both explicit: an implicit conversion in
 * either direction would give back exactly the confusion the type exists to
 * prevent.
 */
template <class Tag>
class Handle {
public:
    /** @brief The underlying index type. */
    using IndexType = std::uint32_t;

    /** @brief Creates the invalid handle. */
    constexpr Handle() = default;

    /**
     * @brief Creates a handle for a given index.
     *
     * @param index Index of the cell.
     */
    constexpr explicit Handle(IndexType index) : index_(index) {}

    /** @brief Returns the underlying index. */
    [[nodiscard]] constexpr IndexType index() const {
        return index_;
    }

    /** @brief Tells whether the handle refers to a cell. */
    [[nodiscard]] constexpr bool valid() const {
        return index_ != invalidIndex;
    }

    /** @brief Same as @ref valid, for use in a condition. */
    constexpr explicit operator bool() const {
        return valid();
    }

    /** @brief Compares two handles of the same family for equality. */
    constexpr bool operator==(const Handle&) const = default;

    /**
     * @brief Orders two handles of the same family.
     *
     * The order is the index order: arbitrary, but stable, which is all the
     * radial buckets and boundary sets that sort handles need.
     */
    constexpr auto operator<=>(const Handle&) const = default;

private:
    static constexpr IndexType invalidIndex = ~IndexType{};
    IndexType index_ = invalidIndex;
};

}  // namespace detail

}  // namespace pgl

namespace std {

/** @brief Hash support for @ref pgl::detail::Handle, so handles can key a set or map. */
template <class Tag>
struct hash<pgl::detail::Handle<Tag>> {
    /** @brief Returns the hash of the handle's index. */
    std::size_t operator()(const pgl::detail::Handle<Tag>& handle) const noexcept {
        return std::hash<std::uint32_t>{}(handle.index());
    }
};

}  // namespace std
