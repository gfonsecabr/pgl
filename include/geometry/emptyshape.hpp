#ifndef PGL_HPP_INCLUDED
// Entered out of order (before pgl.hpp): defer to the umbrella header,
// which re-includes this file at the correct layer.
#include "pgl.hpp"
#else
#ifndef PGL_GEOMETRY_EMPTYSHAPE_HPP
#define PGL_GEOMETRY_EMPTYSHAPE_HPP

/**
 * @file emptyshape.hpp
 * @brief Public declaration of pgl::EmptyShape.
 *
 * EmptyShape models the empty set of points in the plane. It is a full-fledged
 * shape alternative so it can be stored in @ref pgl::Shape, but every predicate
 * is vacuously `false`, it has no vertices, and any intersection with it is
 * again empty. Because it answers every relation generically, it can be added
 * to the Shape variant without special-casing it anywhere else.
 */

#include <compare>
#include <cstddef>
#include <ostream>
#include <stdexcept>


namespace pgl {

/**
 * @brief The empty set of points in the plane.
 *
 * @tparam PointType_ Point type the shape is nominally parameterized on; only
 * its associated types are used, since an empty shape stores no coordinates.
 */
template <class PointType_ = Point<>>
struct EmptyShape {
    using PointType = PointType_;
    using NumberType = typename PointType::NumberType;
    using LabelType = typename PointType::LabelType;

    /**
     * @brief Creates the (unique) empty shape.
     */
    constexpr EmptyShape() = default;

    /**
     * @brief All empty shapes are equal and unordered among themselves.
     */
    [[nodiscard]] constexpr bool operator==(const EmptyShape&) const = default;
    [[nodiscard]] constexpr auto operator<=>(const EmptyShape&) const = default;

    /**
     * @brief Returns the number of vertices, always `0`.
     */
    [[nodiscard]] static constexpr std::size_t size() {
        return 0;
    }

    /**
     * @brief The empty shape is never degenerate.
     */
    [[nodiscard]] constexpr bool isDegenerate() const {
        return false;
    }

    /**
     * @brief Always throws: the empty shape has no vertices.
     */
    [[nodiscard]] PointType get(std::ptrdiff_t) const {
        throw std::logic_error("EmptyShape::get: the empty shape has no vertices");
    }

    /**
     * @brief Always throws: the empty shape has no vertices.
     */
    [[nodiscard]] PointType operator[](std::size_t) const {
        throw std::logic_error("EmptyShape::operator[]: the empty shape has no vertices");
    }

    /**
     * @brief Always throws: the empty shape has no vertices.
     */
    template <class T>
    [[nodiscard]] std::ptrdiff_t index(const T&) const {
        throw std::logic_error("EmptyShape::index: the empty shape has no vertices");
    }

    // The empty set is a subset of every shape, so it contains only the empty
    // set itself: containment of any non-empty shape is false, while containment
    // of another empty shape is true.

    /** @brief The empty set contains no non-empty shape. */
    template <class T>
    [[nodiscard]] constexpr bool contains(const T&) const {
        return false;
    }
    /** @brief The empty set contains the empty set. */
    template <PointConcept OtherPoint>
    [[nodiscard]] constexpr bool contains(const EmptyShape<OtherPoint>&) const {
        return true;
    }

    /** @brief The empty set boundary-contains no non-empty shape. */
    template <class T>
    [[nodiscard]] constexpr bool boundaryContains(const T&) const {
        return false;
    }
    /** @brief The empty set boundary-contains the empty set. */
    template <PointConcept OtherPoint>
    [[nodiscard]] constexpr bool boundaryContains(const EmptyShape<OtherPoint>&) const {
        return true;
    }

    /** @brief The empty set interior-contains no non-empty shape. */
    template <class T>
    [[nodiscard]] constexpr bool interiorContains(const T&) const {
        return false;
    }
    /** @brief The empty set interior-contains the empty set. */
    template <PointConcept OtherPoint>
    [[nodiscard]] constexpr bool interiorContains(const EmptyShape<OtherPoint>&) const {
        return true;
    }

    /** @brief The empty set intersects nothing. */
    template <class T>
    [[nodiscard]] constexpr bool intersects(const T&) const {
        return false;
    }

    /** @brief The empty set has no interior, so its interior intersects nothing. */
    template <class T>
    [[nodiscard]] constexpr bool interiorsIntersect(const T&) const {
        return false;
    }

    /** @brief The empty set separates nothing. */
    template <class T>
    [[nodiscard]] constexpr bool separates(const T&) const {
        return false;
    }

    /** @brief The empty set crosses nothing. */
    template <class T>
    [[nodiscard]] constexpr bool crosses(const T&) const {
        return false;
    }

    /** @brief Intersecting anything with the empty set yields the empty set. */
    template <class ResultNumber = NumberType, class T>
    [[nodiscard]] constexpr EmptyShape intersection(const T&) const {
        return EmptyShape{};
    }
};

/**
 * @brief Streams the empty shape.
 *
 * @param stream Output stream.
 * @return The output stream.
 */
template <class PointType>
std::ostream& operator<<(std::ostream& stream, const EmptyShape<PointType>&) {
    return stream << "EmptyShape()";
}

}  // namespace pgl

#endif // PGL_GEOMETRY_EMPTYSHAPE_HPP
#endif // PGL_HPP_INCLUDED
