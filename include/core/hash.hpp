#pragma once

/**
 * @file hash.hpp
 * @brief Hash support for Pangolin value types.
 *
 * This completes the value-type contract for shapes so they can be used in
 * standard unordered containers alongside their comparison operators.
 */

#include <cstddef>
#include <functional>
#include "forward.hpp"
#include "numeric.hpp"
#include "../pgl.hpp"

namespace pgl::detail {

template <class T>
inline void hashCombine(std::size_t& seed, const T& value) {
    seed ^= std::hash<T>{}(value) + 0x9e3779b9u + (seed << 6u) + (seed >> 2u);
}

}  // namespace pgl::detail


namespace std {

    /**
     * @brief Hash support for Point.
     */
    template <class Number, class Label>
    struct hash<pgl::Point<Number, Label>> {
        std::size_t operator()(const pgl::Point<Number, Label>& point) const {
            std::size_t seed = 0;
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
     * @brief Hash support for Segment.
     */
    template <class PointType>
    struct hash<pgl::Segment<PointType>> {
        std::size_t operator()(const pgl::Segment<PointType>& segment) const {
            std::size_t seed = 2;
            pgl::detail::hashCombine(seed, segment.min());
            pgl::detail::hashCombine(seed, segment.max());
            return seed;
        }
    };

    /**
     * @brief Hash support for OrientedSegment.
     */
    template <class PointType>
    struct hash<pgl::OrientedSegment<PointType>> {
        std::size_t operator()(const pgl::OrientedSegment<PointType>& segment) const {
            std::size_t seed = 3;
            pgl::detail::hashCombine(seed, segment.source());
            pgl::detail::hashCombine(seed, segment.target());
            return seed;
        }
    };


    /**
     * @brief Hash support for Line.
     */
    template <class PointType>
    struct hash<pgl::Line<PointType>> {
        std::size_t operator()(const pgl::Line<PointType>& line) const {
            std::size_t seed = 4;
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
    template <class PointType>
    struct hash<pgl::OrientedLine<PointType>> {
        std::size_t operator()(const pgl::OrientedLine<PointType>& line) const {
            std::size_t seed = line.source() < line.target() ? 5 : 6;
            using T = pgl::Line<PointType>;
            pgl::detail::hashCombine(seed, static_cast<T>(line));
            return seed;
        }
    };


    /**
     * @brief Hash support for Ray.
     */
    template <class PointType>
    struct hash<pgl::Ray<PointType>> {
        std::size_t operator()(const pgl::Ray<PointType>& ray) const {
            std::size_t seed = 7;
            pgl::detail::hashCombine(seed, ray.source());
            using T = pgl::Line<PointType>;
            pgl::detail::hashCombine(seed, static_cast<T>(ray));
            return seed;
        }
    };

    /**
     * @brief Hash support for Halfplane.
     */
    template <class PointType>
    struct hash<pgl::Halfplane<PointType>> {
        std::size_t operator()(const pgl::Halfplane<PointType>& halfplane) const {
            std::size_t seed = halfplane.source() < halfplane.target() ? 8 : 9;
            using T = pgl::Line<PointType>;
            pgl::detail::hashCombine(seed, static_cast<T>(halfplane));
            return seed;
        }
    };

    /**
     * @brief Hash support for Rectangle.
     */
    template <class PointType>
    struct hash<pgl::Rectangle<PointType>> {
        std::size_t operator()(const pgl::Rectangle<PointType>& rectangle) const {
            std::size_t seed = 10;
            pgl::detail::hashCombine(seed, rectangle.min());
            pgl::detail::hashCombine(seed, rectangle.max());
            return seed;
        }
    };

    /**
     * @brief Hash support for Triangle.
     */
    template <class PointType>
    struct hash<pgl::Triangle<PointType>> {
        std::size_t operator()(const pgl::Triangle<PointType>& triangle) const {
            std::size_t seed = 11;
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
            std::size_t seed = 14;
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
    template <class PointType>
    struct hash<pgl::Convex<PointType>> {
        std::size_t operator()(const pgl::Convex<PointType>& convex) const {
            std::size_t seed = 13;
            for (const auto& vertex : convex) {
                pgl::detail::hashCombine(seed, vertex);
            }
            return seed;
        }
    };

    /**
     * @brief Hash support for Polygon.
     */
    template <class PointType>
    struct hash<pgl::Polygon<PointType>> {
        std::size_t operator()(const pgl::Polygon<PointType>& polygon) const {
            std::size_t seed = 13;
            for (const auto& vertex : polygon) {
                pgl::detail::hashCombine(seed, vertex);
            }
            return seed;
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
