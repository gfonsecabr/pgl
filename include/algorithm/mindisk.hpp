#pragma once

#include "algorithm/convexhull.hpp"

/**
 * @file mindisk.hpp
 * @brief Smallest enclosing disk algorithms.
 */

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <iterator>
#include <random>
#include <type_traits>
#include <utility>
#include <vector>


namespace pgl {

namespace detail {

template <class Container>
using min_disk_input_point_t =
    std::remove_cvref_t<decltype(*std::begin(std::declval<const Container&>()))>;

template <class Container>
using min_disk_result_t =
    Disk<Point<typename min_disk_input_point_t<Container>::NumberType>>;

template <PointConcept PointType>
[[nodiscard]] constexpr Disk<PointType> pointDisk(const PointType& point) {
    return Disk<PointType>(point, point, point);
}

/**
 * Returns the circle whose diameter joins @p first and @p second.
 *
 * Disk stores three boundary points, so the third point is obtained by rotating
 * a radius by 90 degrees.
 */
template <PointConcept PointType>
[[nodiscard]] constexpr Disk<PointType> diameterDisk(const PointType& first,
                                                     const PointType& second) {
    using Number = typename PointType::NumberType;
    const Number two = static_cast<Number>(2);
    const PointType midpoint((first.x() + second.x()) / two,
                             (first.y() + second.y()) / two);
    const PointType radius((second.x() - first.x()) / two,
                           (second.y() - first.y()) / two);
    const PointType third(midpoint.x() - radius.y(), midpoint.y() + radius.x());
    return Disk<PointType>(first, second, third);
}

/**
 * Returns the disk determined by three boundary candidates.
 *
 * The incremental algorithm normally reaches this function only for a
 * non-collinear triple, whose circumdisk is required by the boundary
 * invariants. For a collinear triple, the farthest pair determines its minimum
 * enclosing disk; handling that case makes the helper total and protects the
 * algorithm from duplicate and degenerate inputs.
 */
template <PointConcept PointType>
[[nodiscard]] constexpr Disk<PointType> threePointDisk(const PointType& first,
                                                       const PointType& second,
                                                       const PointType& third) {
    if (orientationSign(first, second, third) != 0) {
        return Disk<PointType>(first, second, third);
    }

    const auto firstSecond = first.template squaredDistance<typename PointType::NumberType>(second);
    const auto secondThird = second.template squaredDistance<typename PointType::NumberType>(third);
    const auto thirdFirst = third.template squaredDistance<typename PointType::NumberType>(first);
    if (firstSecond >= secondThird && firstSecond >= thirdFirst) {
        return diameterDisk(first, second);
    }
    if (secondThird >= thirdFirst) {
        return diameterDisk(second, third);
    }
    return diameterDisk(third, first);
}

}  // namespace detail

/**
 * @brief Computes the smallest closed disk containing a set of points.
 *
 * The points are shuffled and processed by the randomized incremental
 * algorithm. Whenever a point lies outside the current disk, that point must
 * lie on the boundary of the new minimum disk; the two nested scans apply the
 * same observation to the remaining one or two boundary points. A disk in the
 * plane has at most three support points, so this yields expected linear time.
 *
 * The returned disk retains the input coordinate number type. Point labels are
 * not copied to the constructed disk's boundary points.
 *
 * @warning Constructing a disk from two support points divides coordinates by
 *          2. If an integral coordinate type is used, all input coordinates
 *          should be even; otherwise integer division can truncate the result.
 *
 * @tparam Container Container of pgl points.
 * @tparam UniformRandomBitGenerator Random-bit generator accepted by
 *         `std::shuffle`.
 * @param input Input points; they are copied and the container is not modified.
 * @param generator Generator used to randomize the incremental order.
 * @pre @p input is not empty.
 * @return The unique smallest enclosing disk.
 *
 * @complexity Expected O(n) time and O(n) additional space.
 */
template <class Container, class UniformRandomBitGenerator>
[[nodiscard]] detail::min_disk_result_t<Container>
smallestEnclosingDisk(const Container& input, UniformRandomBitGenerator&& generator) {
    using InputPoint = detail::min_disk_input_point_t<Container>;
    static_assert(PointConcept<InputPoint>,
                  "smallestEnclosingDisk requires a container of pgl::Point values");
    using ResultNumber = typename InputPoint::NumberType;
    using ResultPoint = Point<ResultNumber>;
    using ResultDisk = Disk<ResultPoint>;

    std::vector<ResultPoint> points;
    for (const auto& point : input) {
        points.emplace_back(point);
    }
    assert(!points.empty());
    std::shuffle(points.begin(), points.end(),
                 std::forward<UniformRandomBitGenerator>(generator));

    ResultDisk disk = detail::pointDisk(points.front());
    for (std::size_t i = 1; i < points.size(); ++i) {
        if (disk.contains(points[i])) {
            continue;
        }

        disk = detail::pointDisk(points[i]);
        for (std::size_t j = 0; j < i; ++j) {
            if (disk.contains(points[j])) {
                continue;
            }

            disk = detail::diameterDisk(points[i], points[j]);
            for (std::size_t k = 0; k < j; ++k) {
                if (!disk.contains(points[k])) {
                    disk = detail::threePointDisk(points[i], points[j], points[k]);
                }
            }
        }
    }
    return disk;
}

/**
 * @brief Computes the smallest closed disk containing a set of points.
 *
 * This overload creates a random generator for the randomized incremental
 * order. See the generator-taking overload for result and complexity details.
 *
 * @warning Constructing a disk from two support points divides coordinates by
 *          2. If an integral coordinate type is used, all input coordinates
 *          should be even; otherwise integer division can truncate the result.
 */
template <class Container>
[[nodiscard]] detail::min_disk_result_t<Container>
smallestEnclosingDisk(const Container& input) {
    std::mt19937 generator(std::random_device{}());
    return smallestEnclosingDisk(input, generator);
}

// -----------------------------------------------------------------------------
// Shape methods that need the algorithm above.

template <class PointType, class LabelType>
template <class UniformRandomBitGenerator>
Disk<Point<typename Convex<PointType, LabelType>::NumberType>>
Convex<PointType, LabelType>::smallestEnclosingDisk(
    UniformRandomBitGenerator&& generator) const {
    assert(!empty());
    // Qualified: the member name would otherwise hide the free function.
    return pgl::smallestEnclosingDisk(
        *this, std::forward<UniformRandomBitGenerator>(generator));
}

template <class PointType, class LabelType>
Disk<Point<typename Convex<PointType, LabelType>::NumberType>>
Convex<PointType, LabelType>::smallestEnclosingDisk() const {
    assert(!empty());
    return pgl::smallestEnclosingDisk(*this);
}

}  // namespace pgl
