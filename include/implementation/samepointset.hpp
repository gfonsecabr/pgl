#pragma once

#include "implementation/predicates.hpp"

/**
 * @file samepointset.hpp
 * @brief Exact equality of the point sets represented by arbitrary shapes.
 */

#include <compare>
#include <concepts>
#include <cstddef>
#include <type_traits>
#include <variant>

namespace pgl::detail {

/** Shapes whose `bbox()` is the exact bounding box of their represented set. */
template <class T>
inline constexpr bool hasExactBoundingBox =
    PointConcept<T> || SegmentConcept<T> || OrientedSegmentConcept<T> ||
    RectangleConcept<T> || TriangleConcept<T> || ConvexConcept<T> ||
    MonotoneChainConcept<T> || PolylineConcept<T> || PolygonConcept<T> ||
    PolygonWithHolesConcept<T> || PolygonSetConcept<T>;

/**
 * Dimensions a defined value of a concrete shape kind can represent.
 *
 * Bit 0 is dimension zero, bit 1 dimension one, and bit 2 dimension two.
 * Empty sets are handled separately. Undefined degeneracies do not widen a
 * kind's mask: in particular a degenerate Line or Halfplane does not make that
 * type point-valued for `samePointSet`.
 */
template <class T>
inline constexpr unsigned possiblePointSetDimensions = [] {
    if constexpr (PointConcept<T>) {
        return 0b001u;
    } else if constexpr (SegmentConcept<T> || OrientedSegmentConcept<T> ||
                         RayConcept<T> || MonotoneChainConcept<T> ||
                         PolylineConcept<T>) {
        return 0b011u;
    } else if constexpr (LineConcept<T> || OrientedLineConcept<T>) {
        return 0b010u;
    } else if constexpr (HalfplaneConcept<T>) {
        return 0b100u;
    } else if constexpr (DiskConcept<T>) {
        return 0b101u;
    } else if constexpr (RectangleConcept<T> || TriangleConcept<T> ||
                         ConvexConcept<T> || PolygonConcept<T> ||
                         HalfplaneIntersectionConcept<T> ||
                         PolygonWithHolesConcept<T> || PolygonSetConcept<T>) {
        return 0b111u;
    } else {
        return 0u;
    }
}();

template <class T>
inline constexpr bool alwaysBoundedPointSet =
    PointConcept<T> || SegmentConcept<T> || OrientedSegmentConcept<T> ||
    RectangleConcept<T> || TriangleConcept<T> || DiskConcept<T> ||
    ConvexConcept<T> || MonotoneChainConcept<T> || PolylineConcept<T> ||
    PolygonConcept<T> || PolygonWithHolesConcept<T> || PolygonSetConcept<T>;

template <class T>
inline constexpr bool alwaysUnboundedWhenDefined =
    LineConcept<T> || OrientedLineConcept<T> || HalfplaneConcept<T>;

/** @brief Tests emptiness without asking non-empty primitive types to fake it. */
template <class ShapeType>
constexpr bool pointSetEmpty(const ShapeType& shape) {
    if constexpr (EmptyShapeConcept<ShapeType>) {
        return true;
    } else if constexpr (requires { { shape.empty() } -> std::convertible_to<bool>; }) {
        return shape.empty();
    } else {
        return false;
    }
}

/** @brief Compares exact bounding boxes across coordinate types. */
template <class First, class Second>
constexpr bool sameExactBoundingBox(const First& first, const Second& second) {
    const auto firstBox = first.bbox();
    const auto secondBox = second.bbox();
    return firstBox.min() == secondBox.min() && firstBox.max() == secondBox.max();
}

/** @brief Whether a ring vertex only subdivides the straight edge around it. */
template <PolygonConcept PolygonType>
constexpr bool redundantRingVertex(const PolygonType& polygon, std::size_t index) {
    const std::size_t count = polygon.size();
    if (count < 3) {
        return false;
    }
    const auto previous = polygon[(index + count - 1) % count];
    const auto current = polygon[index];
    const auto next = polygon[(index + 1) % count];
    return orientationSign(previous, current, next) == std::partial_ordering::equivalent &&
           Segment<typename PolygonType::PointType>(previous, next).contains(current);
}

template <PolygonConcept PolygonType>
constexpr std::size_t firstEssentialRingVertex(const PolygonType& polygon) {
    for (std::size_t i = 0; i < polygon.size(); ++i) {
        if (!redundantRingVertex(polygon, i)) {
            return i;
        }
    }
    return polygon.size();
}

template <PolygonConcept PolygonType>
constexpr std::size_t essentialRingVertexCount(const PolygonType& polygon) {
    std::size_t count = 0;
    for (std::size_t i = 0; i < polygon.size(); ++i) {
        count += !redundantRingVertex(polygon, i);
    }
    return count;
}

template <PolygonConcept PolygonType>
constexpr std::size_t nextEssentialRingVertex(const PolygonType& polygon, std::size_t index) {
    do {
        index = (index + 1) % polygon.size();
    } while (redundantRingVertex(polygon, index));
    return index;
}

/**
 * @brief Compares two non-degenerate simple polygon rings in linear time.
 *
 * Normal Polygon construction already fixes winding and cyclic rotation. The
 * only remaining freedom for equal point sets is subdivision of a straight
 * edge, so both rings are streamed while such vertices are skipped.
 */
template <PolygonConcept FirstPolygon, PolygonConcept SecondPolygon>
constexpr bool samePolygonBoundary(const FirstPolygon& first, const SecondPolygon& second) {
    const std::size_t firstCount = essentialRingVertexCount(first);
    if (firstCount != essentialRingVertexCount(second)) {
        return false;
    }
    if (firstCount == 0) {
        return false;
    }

    std::size_t firstIndex = firstEssentialRingVertex(first);
    std::size_t secondIndex = firstEssentialRingVertex(second);
    for (std::size_t compared = 0; compared < firstCount; ++compared) {
        if (first[firstIndex] != second[secondIndex]) {
            return false;
        }
        firstIndex = nextEssentialRingVertex(first, firstIndex);
        secondIndex = nextEssentialRingVertex(second, secondIndex);
    }
    return true;
}

/** @brief Compares canonical, full-dimensional regions by their geometric rings. */
template <PolygonWithHolesConcept FirstRegion, PolygonWithHolesConcept SecondRegion>
constexpr bool sameRegionRings(const FirstRegion& first, const SecondRegion& second) {
    if (!samePolygonBoundary(first.outer(), second.outer()) ||
        first.holeCount() != second.holeCount()) {
        return false;
    }
    // Polygon's representational ordering includes vertex count, so equivalent
    // subdivided holes need not occupy the same sorted position. Match them by
    // geometry. Valid regions cannot contain duplicate hole interiors.
    for (const auto& firstHole : first.holes()) {
        bool found = false;
        for (const auto& secondHole : second.holes()) {
            if (samePolygonBoundary(firstHole, secondHole)) {
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

/** @brief Compares canonical polygon sets by their geometric components. */
template <PolygonSetConcept FirstSet, PolygonSetConcept SecondSet>
constexpr bool sameSetComponents(const FirstSet& first, const SecondSet& second) {
    if (first.componentCount() != second.componentCount()) {
        return false;
    }
    // As for holes, representational sorting can move a subdivided component.
    // Valid sets have no duplicate components, so a geometric lookup suffices.
    for (const auto& firstComponent : first.components()) {
        bool found = false;
        for (const auto& secondComponent : second.components()) {
            if (sameRegionRings(firstComponent, secondComponent)) {
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

/**
 * @brief Compares one holed region with a possibly split polygon set.
 *
 * A valid set is closed. If it is contained in the region and has the same
 * area, it contains every interior point of the region: missing one would leave
 * an open neighbourhood of positive area outside the closed set. What area
 * does not settle are pinched, locally one-dimensional parts retained by the
 * region definition. Those are exactly its outer/hole boundary edges, checked
 * separately against the set. This is the case mutual containment cannot
 * express when one region's interior is split among several set components.
 */
template <PolygonWithHolesConcept Region, PolygonSetConcept Set>
constexpr bool sameRegionAndSet(const Region& region, const Set& set) {
    if (!region.contains(set)) {
        return false;
    }

    using RegionArea = std::remove_cvref_t<decltype(region.twiceArea())>;
    using SetArea = std::remove_cvref_t<decltype(set.twiceArea())>;
    using CommonArea = std::common_type_t<RegionArea, SetArea>;
    if (static_cast<CommonArea>(region.twiceArea()) !=
        static_cast<CommonArea>(set.twiceArea())) {
        return false;
    }

    for (const auto& edge : region.edges()) {
        if (!set.contains(edge)) {
            return false;
        }
    }
    return true;
}

/**
 * @brief Implements point-set equality for every pair of concrete/wrapped shapes.
 *
 * Equality of sets is mutual containment. Before paying for both directions,
 * common cases take strictly cheaper exits: wrappers are unwrapped without a
 * conversion, empty sets are classified once, canonical representations use
 * their existing equality, and bounded shapes reject unequal exact bounding
 * boxes. The final containment calls retain each pair's specialized geometric
 * algorithm and exact mixed-number arithmetic.
 */
template <AnyShapeConcept First, AnyShapeConcept Second>
constexpr bool samePointSet(const First& first, const Second& second) {
    if constexpr (ShapeConcept<First>) {
        return std::visit(
            [&second](const auto& value) { return detail::samePointSet(value, second); },
            first.variant());
    } else if constexpr (ShapeConcept<Second>) {
        return std::visit(
            [&first](const auto& value) { return detail::samePointSet(first, value); },
            second.variant());
    } else {
        const bool firstEmpty = pointSetEmpty(first);
        const bool secondEmpty = pointSetEmpty(second);
        if (firstEmpty || secondEmpty) {
            return firstEmpty && secondEmpty;
        }

        if constexpr ((possiblePointSetDimensions<First> &
                       possiblePointSetDimensions<Second>) == 0) {
            return false;
        }
        if constexpr ((alwaysBoundedPointSet<First> && alwaysUnboundedWhenDefined<Second>) ||
                      (alwaysUnboundedWhenDefined<First> && alwaysBoundedPointSet<Second>)) {
            return false;
        }

        // This catches the overwhelmingly common canonical/same-type case.
        // A false result is not final: orientation, redundant polygon vertices,
        // and lower-dimensional constraint lists can differ for the same set.
        if constexpr (requires { { first == second } -> std::convertible_to<bool>; }) {
            if (first == second) {
                return true;
            }
        }

        // A one-component set is only a container around that region. Compare
        // its rings directly before bounding boxes, containment, or area. In
        // addition to being linear and allocation-free, this avoids making an
        // exact structural equality depend on mixed-coordinate predicate
        // arithmetic.
        if constexpr (PolygonWithHolesConcept<First> && PolygonSetConcept<Second>) {
            if (second.componentCount() == 1 &&
                sameRegionRings(first, second.component(0))) {
                return true;
            }
        } else if constexpr (PolygonSetConcept<First> &&
                             PolygonWithHolesConcept<Second>) {
            if (first.componentCount() == 1 &&
                sameRegionRings(first.component(0), second)) {
                return true;
            }
        }

        if constexpr (hasExactBoundingBox<First> && hasExactBoundingBox<Second>) {
            if (!sameExactBoundingBox(first, second)) {
                return false;
            }
        }

        if constexpr (PolygonConcept<First> && PolygonConcept<Second>) {
            if (!first.isDegenerate() && !second.isDegenerate()) {
                return samePolygonBoundary(first, second);
            }
        } else if constexpr (PolygonWithHolesConcept<First> && PolygonSetConcept<Second>) {
            return sameRegionAndSet(first, second);
        } else if constexpr (PolygonSetConcept<First> && PolygonWithHolesConcept<Second>) {
            return sameRegionAndSet(second, first);
        } else if constexpr (PolygonWithHolesConcept<First> &&
                             PolygonWithHolesConcept<Second>) {
            if (!first.isDegenerate() && !second.isDegenerate()) {
                // Matching rings prove equality cheaply. A mismatch is not
                // definitive because a hole may touch the outer boundary and
                // admit an equivalent notched-outer representation.
                if (sameRegionRings(first, second)) {
                    return true;
                }
            }
        } else if constexpr (PolygonSetConcept<First> && PolygonSetConcept<Second>) {
            // Components may touch at isolated points, so structurally
            // different valid decompositions still need the containment path.
            if (sameSetComponents(first, second)) {
                return true;
            }
        }

        return first.contains(second) && second.contains(first);
    }
}

}  // namespace pgl::detail

namespace pgl {

template <class Number, class Label>
template <AnyShapeConcept OtherShape>
constexpr bool Point<Number, Label>::samePointSet(const OtherShape& other) const {
    return detail::samePointSet(*this, other);
}

template <class PointType>
template <AnyShapeConcept OtherShape>
constexpr bool EmptyShape<PointType>::samePointSet(const OtherShape& other) const {
    return detail::samePointSet(*this, other);
}

template <class PointType, class LabelType>
template <AnyShapeConcept OtherShape>
constexpr bool Segment<PointType, LabelType>::samePointSet(const OtherShape& other) const {
    return detail::samePointSet(*this, other);
}

template <class PointType, class LabelType>
template <AnyShapeConcept OtherShape>
constexpr bool OrientedSegment<PointType, LabelType>::samePointSet(const OtherShape& other) const {
    return detail::samePointSet(*this, other);
}

template <class PointType, class LabelType>
template <AnyShapeConcept OtherShape>
constexpr bool Line<PointType, LabelType>::samePointSet(const OtherShape& other) const {
    return detail::samePointSet(*this, other);
}

template <class PointType, class LabelType>
template <AnyShapeConcept OtherShape>
constexpr bool OrientedLine<PointType, LabelType>::samePointSet(const OtherShape& other) const {
    return detail::samePointSet(*this, other);
}

template <class PointType, class LabelType>
template <AnyShapeConcept OtherShape>
constexpr bool Ray<PointType, LabelType>::samePointSet(const OtherShape& other) const {
    return detail::samePointSet(*this, other);
}

template <class PointType, class LabelType>
template <AnyShapeConcept OtherShape>
constexpr bool Halfplane<PointType, LabelType>::samePointSet(const OtherShape& other) const {
    return detail::samePointSet(*this, other);
}

template <class PointType, class LabelType>
template <AnyShapeConcept OtherShape>
constexpr bool Rectangle<PointType, LabelType>::samePointSet(const OtherShape& other) const {
    return detail::samePointSet(*this, other);
}

template <class PointType, class LabelType>
template <AnyShapeConcept OtherShape>
constexpr bool Triangle<PointType, LabelType>::samePointSet(const OtherShape& other) const {
    return detail::samePointSet(*this, other);
}

template <class PointType, class LabelType>
template <AnyShapeConcept OtherShape>
constexpr bool Disk<PointType, LabelType>::samePointSet(const OtherShape& other) const {
    return detail::samePointSet(*this, other);
}

template <class PointType, class LabelType>
template <AnyShapeConcept OtherShape>
constexpr bool Convex<PointType, LabelType>::samePointSet(const OtherShape& other) const {
    return detail::samePointSet(*this, other);
}

template <class PointType, class LabelType, class Storage>
template <AnyShapeConcept OtherShape>
constexpr bool MonotoneChain<PointType, LabelType, Storage>::samePointSet(const OtherShape& other) const {
    return detail::samePointSet(*this, other);
}

template <class PointType, class LabelType>
template <AnyShapeConcept OtherShape>
constexpr bool Polyline<PointType, LabelType>::samePointSet(const OtherShape& other) const {
    return detail::samePointSet(*this, other);
}

template <class PointType, class LabelType>
template <AnyShapeConcept OtherShape>
constexpr bool Polygon<PointType, LabelType>::samePointSet(const OtherShape& other) const {
    return detail::samePointSet(*this, other);
}

template <class PointType, class LabelType>
template <AnyShapeConcept OtherShape>
constexpr bool HalfplaneIntersection<PointType, LabelType>::samePointSet(const OtherShape& other) const {
    return detail::samePointSet(*this, other);
}

template <class PointType, class LabelType>
template <AnyShapeConcept OtherShape>
constexpr bool PolygonWithHoles<PointType, LabelType>::samePointSet(const OtherShape& other) const {
    return detail::samePointSet(*this, other);
}

template <class PointType, class LabelType>
template <AnyShapeConcept OtherShape>
constexpr bool PolygonSet<PointType, LabelType>::samePointSet(const OtherShape& other) const {
    return detail::samePointSet(*this, other);
}

template <class PointType>
template <AnyShapeConcept OtherShape>
constexpr bool Shape<PointType>::samePointSet(const OtherShape& other) const {
    return detail::samePointSet(*this, other);
}

}  // namespace pgl
