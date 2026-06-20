#ifndef PGL_HPP_INCLUDED
// Entered out of order (before pgl.hpp): defer to the umbrella header,
// which re-includes this file at the correct layer.
#include "pgl.hpp"
#else
#ifndef PGL_CORE_HASH_HPP
#define PGL_CORE_HASH_HPP

/**
 * @file hash.hpp
 * @brief Hash support for Pangolin value types.
 *
 * This completes the value-type contract for shapes so they can be used in
 * standard unordered containers alongside their comparison operators.
 */

#include <cstddef>
#include <cstdint>
#include <functional>
#include <type_traits>
#include "forward.hpp"
#include "numeric.hpp"

namespace pgl::detail {

template <class T>
inline void hashCombine(std::size_t& seed, const T& value) {
#if defined(__SIZEOF_INT128__)
    // libstdc++ does not provide std::hash for the 128-bit integer extension
    // types under strict ISO modes (-std=c++NN), so hash their two 64-bit
    // halves instead of relying on std::hash<__int128>.
    if constexpr (std::is_same_v<T, __int128_t> || std::is_same_v<T, __uint128_t>) {
        const auto bits = static_cast<__uint128_t>(value);
        hashCombine(seed, static_cast<std::uint64_t>(bits));
        hashCombine(seed, static_cast<std::uint64_t>(bits >> 64));
    } else
#endif
    {
        seed ^= std::hash<T>{}(value) + 0x9e3779b9u + (seed << 6u) + (seed >> 2u);
    }
}

}  // namespace pgl::detail


namespace std {

    /**
     * @brief Hash support for Point.
     */
    template <class Number, class Label>
    struct hash<pgl::Point<Number, Label>> {
        std::size_t operator()(const pgl::Point<Number, Label>& point) const {
            std::size_t seed = pgl::detail::shapeRank<pgl::Point<Number, Label>>;
            pgl::detail::hashCombine(seed, point.x());
            pgl::detail::hashCombine(seed, point.y());
            return seed;
        }
    };


    /**
     * @brief Hash support for Rational.
     */
    template <class Int>
    struct hash<pgl::Rational<Int>> {
        size_t operator()(const pgl::Rational<Int>& r) const noexcept {
            std::size_t seed = 1;
            pgl::detail::hashCombine(seed, r.numerator());
            pgl::detail::hashCombine(seed, r.denominator());
            return seed;
        }
    };

    /**
     * @brief Hash support for BigInt.
     */
    template <>
    struct hash<pgl::BigInt> {
        std::size_t operator()(const pgl::BigInt& b) const noexcept {
            std::size_t seed = 15;
            pgl::detail::hashCombine(seed, b.negative_);
            pgl::detail::hashCombine(seed, b.small_);
            for (const auto& limb : b.limbs_) {
                pgl::detail::hashCombine(seed, limb);
            }
            return seed;
        }
    };

    /**
     * @brief Hash support for Segment.
     */
    template <class PointType, class LabelType>
    struct hash<pgl::Segment<PointType, LabelType>> {
        std::size_t operator()(const pgl::Segment<PointType, LabelType>& segment) const {
            std::size_t seed = pgl::detail::shapeRank<pgl::Segment<PointType, LabelType>>;
            pgl::detail::hashCombine(seed, segment.min());
            pgl::detail::hashCombine(seed, segment.max());
            return seed;
        }
    };

    /**
     * @brief Hash support for OrientedSegment.
     */
    template <class PointType, class LabelType>
    struct hash<pgl::OrientedSegment<PointType, LabelType>> {
        std::size_t operator()(const pgl::OrientedSegment<PointType, LabelType>& segment) const {
            std::size_t seed = pgl::detail::shapeRank<pgl::OrientedSegment<PointType, LabelType>>;
            pgl::detail::hashCombine(seed, segment.source());
            pgl::detail::hashCombine(seed, segment.target());
            return seed;
        }
    };


    /**
     * @brief Hash support for Line.
     */
    template <class PointType, class LabelType>
    struct hash<pgl::Line<PointType, LabelType>> {
        std::size_t operator()(const pgl::Line<PointType, LabelType>& line) const {
            std::size_t seed = pgl::detail::shapeRank<pgl::Line<PointType, LabelType>>;
            if (line.isDegenerate()) {
                pgl::detail::hashCombine(seed, line.min());
            }
            else if (line.isVertical()) {
                pgl::detail::hashCombine(seed, line.min().x());
            }
            else {
                using IntRational = pgl::to_integer_with_digits_t<typename pgl::Line<PointType>::NumberType>;
                using RationalKey = pgl::Rational<IntRational>;
                const auto [anum,bnum,den] = line.template dualCoordinates<IntRational>();

                pgl::detail::hashCombine(seed, RationalKey(anum,den));
                pgl::detail::hashCombine(seed, RationalKey(bnum,den));
            }
            return seed;
        }
    };


    /**
     * @brief Hash support for OrientedLine.
     */
    template <class PointType, class LabelType>
    struct hash<pgl::OrientedLine<PointType, LabelType>> {
        std::size_t operator()(const pgl::OrientedLine<PointType, LabelType>& line) const {
            std::size_t seed = pgl::detail::shapeRank<pgl::OrientedLine<PointType, LabelType>>
                             + (line.source() < line.target() ? 0u : 1u);
            using T = pgl::Line<PointType>;
            pgl::detail::hashCombine(seed, static_cast<T>(line));
            return seed;
        }
    };


    /**
     * @brief Hash support for Ray.
     */
    template <class PointType, class LabelType>
    struct hash<pgl::Ray<PointType, LabelType>> {
        std::size_t operator()(const pgl::Ray<PointType, LabelType>& ray) const {
            std::size_t seed = pgl::detail::shapeRank<pgl::Ray<PointType, LabelType>>;
            pgl::detail::hashCombine(seed, ray.source());
            using T = pgl::Line<PointType>;
            pgl::detail::hashCombine(seed, static_cast<T>(ray));
            return seed;
        }
    };

    /**
     * @brief Hash support for Halfplane.
     */
    template <class PointType, class LabelType>
    struct hash<pgl::Halfplane<PointType, LabelType>> {
        std::size_t operator()(const pgl::Halfplane<PointType, LabelType>& halfplane) const {
            std::size_t seed = pgl::detail::shapeRank<pgl::Halfplane<PointType, LabelType>>
                             + (halfplane.source() < halfplane.target() ? 0u : 1u);
            using T = pgl::Line<PointType>;
            pgl::detail::hashCombine(seed, static_cast<T>(halfplane));
            return seed;
        }
    };

    /**
     * @brief Hash support for Rectangle.
     */
    template <class PointType, class LabelType>
    struct hash<pgl::Rectangle<PointType, LabelType>> {
        std::size_t operator()(const pgl::Rectangle<PointType, LabelType>& rectangle) const {
            std::size_t seed = pgl::detail::shapeRank<pgl::Rectangle<PointType, LabelType>>;
            pgl::detail::hashCombine(seed, rectangle.min());
            pgl::detail::hashCombine(seed, rectangle.max());
            return seed;
        }
    };

    /**
     * @brief Hash support for Triangle.
     */
    template <class PointType, class LabelType>
    struct hash<pgl::Triangle<PointType, LabelType>> {
        std::size_t operator()(const pgl::Triangle<PointType, LabelType>& triangle) const {
            std::size_t seed = pgl::detail::shapeRank<pgl::Triangle<PointType, LabelType>>;
            pgl::detail::hashCombine(seed, triangle.a());
            pgl::detail::hashCombine(seed, triangle.b());
            pgl::detail::hashCombine(seed, triangle.c());
            return seed;
        }
    };

    /**
     * @brief Hash support for Disk.
     */
    template <class PointType, class LabelType>
    struct hash<pgl::Disk<PointType, LabelType>> {
        std::size_t operator()(const pgl::Disk<PointType, LabelType>& disk) const {
            std::size_t seed = pgl::detail::shapeRank<pgl::Disk<PointType, LabelType>>;
            pgl::detail::hashCombine(seed, disk.isDegenerate());
            if (disk.isDegenerate()) {
                pgl::detail::hashCombine(seed, disk.a());
                pgl::detail::hashCombine(seed, disk.b());
                pgl::detail::hashCombine(seed, disk.c());
            } else {
                pgl::detail::hashCombine(seed, disk.center());
                pgl::detail::hashCombine(seed, disk.squaredRadius());
            }
            return seed;
        }
    };

    /**
     * @brief Hash support for Convex.
     */
    template <class PointType, class LabelType>
    struct hash<pgl::Convex<PointType, LabelType>> {
        std::size_t operator()(const pgl::Convex<PointType, LabelType>& convex) const {
            using Shape = pgl::Convex<PointType, LabelType>;
            if (convex.hash_ != Shape::hashUnset_) {
                return convex.hash_;
            }
            std::size_t seed = pgl::detail::shapeRank<Shape>;
            for (const auto& vertex : convex) {
                pgl::detail::hashCombine(seed, vertex);
            }
            // Never store the sentinel: remap the single colliding value so the
            // cache can always distinguish "computed" from "not computed".
            if (seed == Shape::hashUnset_) {
                seed = Shape::hashUnset_ - 1;
            }
            convex.hash_ = seed;
            return seed;
        }
    };

    /**
     * @brief Hash support for Polygon.
     */
    template <class PointType, class LabelType>
    struct hash<pgl::Polygon<PointType, LabelType>> {
        std::size_t operator()(const pgl::Polygon<PointType, LabelType>& polygon) const {
            using Shape = pgl::Polygon<PointType, LabelType>;
            if (polygon.hash_ != Shape::hashUnset_) {
                return polygon.hash_;
            }
            std::size_t seed = pgl::detail::shapeRank<Shape>;
            for (const auto& vertex : polygon) {
                pgl::detail::hashCombine(seed, vertex);
            }
            // Never store the sentinel: remap the single colliding value so the
            // cache can always distinguish "computed" from "not computed".
            if (seed == Shape::hashUnset_) {
                seed = Shape::hashUnset_ - 1;
            }
            polygon.hash_ = seed;
            return seed;
        }
    };

    /**
     * @brief Hash support for EmptyShape.
     */
    template <class PointType>
    struct hash<pgl::EmptyShape<PointType>> {
        std::size_t operator()(const pgl::EmptyShape<PointType>&) const {
            return pgl::detail::shapeRank<pgl::EmptyShape<PointType>>;
        }
    };

    /**
     * @brief Hash support for Shape.
     */
    template <class PointType>
    struct hash<pgl::Shape<PointType>> {
        std::size_t operator()(const pgl::Shape<PointType>& shape) const {
            std::size_t seed = 12;
            pgl::detail::hashCombine(seed, shape.variant().index());
            std::visit(
                [&seed](const auto& value) {
                    pgl::detail::hashCombine(seed, value);
                },
                shape.variant());
            return seed;
        }
    };

}//std

#endif // PGL_CORE_HASH_HPP
#endif // PGL_HPP_INCLUDED
