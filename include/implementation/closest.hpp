#pragma once

#include "implementation/distance.hpp"

/**
 * @file closest.hpp
 * @brief Witnesses for `squaredDistance`: which elements realize it, and where.
 *
 * `closestSegments` names the pair of elements — one edge of each shape,
 * degenerate to a vertex where the shape has no edge — whose distance is the
 * distance between the shapes. `closestPoints` refines that pair to the two
 * points themselves, whose coordinates need a `ResultNumber` that survives a
 * division. Both answer nothing exactly when `squaredDistance` is zero.
 *
 * `closestSegments` needs both operands to be @ref BoundedPolygonalConcept — the
 * shapes covered by finitely many segments with exact endpoints — because an
 * unbounded operand realizes the distance at a point that is on no edge and at
 * no vertex, so there is no element to name. `closestPoints` has no such
 * trouble and also accepts an @ref UnboundedConvexConcept operand on one side,
 * working against its boundary pieces. A @ref Disk is out of both: its nearest
 * point is irrational.
 */

#include <array>
#include <cstddef>
#include <optional>
#include <type_traits>
#include <variant>
#include <utility>

namespace pgl {

namespace detail {

/**
 * @brief Coordinate type the closest-element search compares distances in.
 *
 * A vertex-to-edge distance is a fraction, so the comparison that picks the
 * winning pair is exact only in a type closed under division — whatever the
 * caller then asks the result to be expressed in.
 */
template <class Self, class Other>
using closestCompare_t =
    division_result_t<std::common_type_t<typename Self::NumberType, typename Other::NumberType>>;

/**
 * @brief Segments covering a bounded polygonal shape, in its own coordinates.
 *
 * A point, and any shape whose vertices span no edge, contributes one degenerate
 * segment per vertex, so the result is empty only for a shape that has no
 * vertices at all.
 */
template <class ShapeType>
constexpr auto coveringSegments(const ShapeType& shape) {
    if constexpr (PointConcept<ShapeType>) {
        return std::array<Segment<ShapeType>, 1>{Segment<ShapeType>(shape, shape)};
    } else {
        auto result = shape.edges();
        if constexpr (requires(decltype(result)& growable) { growable.emplace_back(); }) {
            if (result.empty()) {
                for (const auto& vertex : shape.vertices()) {
                    result.emplace_back(vertex, vertex);
                }
            }
        }
        return result;
    }
}

/**
 * @brief The closest pair of covering segments, in the operands' own coordinates.
 *
 * Empty when the shapes meet, which is asked of the pair's own `squaredDistance`
 * rather than of the edges: two disjoint boundaries are also what nesting looks
 * like, and a shape inside another is at distance zero from it.
 */
template <class Self, class Other>
constexpr auto closestNativeSegments(const Self& self, const Other& other) {
    using Compare = closestCompare_t<Self, Other>;
    const auto selfSegments = coveringSegments(self);
    const auto otherSegments = coveringSegments(other);
    using SelfSegment = typename std::decay_t<decltype(selfSegments)>::value_type;
    using OtherSegment = typename std::decay_t<decltype(otherSegments)>::value_type;
    using Result = std::optional<std::pair<SelfSegment, OtherSegment>>;

    if (selfSegments.empty() || otherSegments.empty()) {
        return Result{};
    }
    if (self.template squaredDistance<Compare>(other) == Compare{}) {
        return Result{};
    }

    std::size_t bestSelf = 0;
    std::size_t bestOther = 0;
    Compare best = selfSegments[0].template squaredDistance<Compare>(otherSegments[0]);
    for (std::size_t i = 0; i < selfSegments.size(); ++i) {
        for (std::size_t j = 0; j < otherSegments.size(); ++j) {
            const Compare candidate =
                selfSegments[i].template squaredDistance<Compare>(otherSegments[j]);
            if (candidate < best) {
                best = candidate;
                bestSelf = i;
                bestOther = j;
            }
        }
    }
    return Result{std::pair<SelfSegment, OtherSegment>{selfSegments[bestSelf], otherSegments[bestOther]}};
}

/** @brief Implements `closestSegments`; see @ref closestNativeSegments. */
template <class ResultNumber, class LabelType, class Self, class Other>
constexpr auto closestSegmentsOf(const Self& self, const Other& other) {
    using ResultSegment = Segment<Point<ResultNumber, LabelType>>;
    using Result = std::optional<std::array<ResultSegment, 2>>;

    const auto native = closestNativeSegments(self, other);
    if (!native) {
        return Result{};
    }
    return Result{std::array<ResultSegment, 2>{ResultSegment(native->first), ResultSegment(native->second)}};
}

/**
 * @brief The ends at which a boundary piece stops.
 *
 * Two for a segment, one for a ray, none for a line. The pair realizing the
 * distance between two such pieces always has one of these ends on one side, so
 * they are the whole candidate list.
 */
template <class Piece>
constexpr auto pieceEnds(const Piece& piece) {
    using PiecePoint = typename Piece::PointType;
    if constexpr (LineConcept<Piece>) {
        return std::array<PiecePoint, 0>{};
    } else if constexpr (RayConcept<Piece>) {
        return std::array<PiecePoint, 1>{piece.source()};
    } else {
        return std::array<PiecePoint, 2>{piece.min(), piece.max()};
    }
}

/**
 * @brief The point of @p piece closest to @p query.
 *
 * An end is returned as itself, keeping its label; a foot between the ends is
 * built from a division and carries none.
 */
template <class ResultNumber, class LabelType, class Piece, class QueryPoint>
constexpr Point<ResultNumber, LabelType> closestPointOn(const Piece& piece, const QueryPoint& query) {
    using ResultPoint = Point<ResultNumber, LabelType>;
    constexpr bool stopsAtOrigin = !LineConcept<Piece>;
    constexpr bool stopsAtHeading = !LineConcept<Piece> && !RayConcept<Piece>;

    // A ray runs from its source through its target; a segment and a line are
    // both read from their lexicographic ends.
    const auto& origin = [&piece]() -> const auto& {
        if constexpr (RayConcept<Piece>) {
            return piece.source();
        } else {
            return piece.min();
        }
    }();
    const auto& heading = [&piece]() -> const auto& {
        if constexpr (RayConcept<Piece>) {
            return piece.target();
        } else {
            return piece.max();
        }
    }();

    const Point<ResultNumber> first(origin);
    const Point<ResultNumber> second(heading);
    const Point<ResultNumber> target(query);

    const auto along = second - first;
    const auto toTarget = target - first;
    const auto squaredLength = along * along;

    if (squaredLength == ResultNumber{}) {
        return ResultPoint(origin);
    }
    if constexpr (stopsAtOrigin) {
        if (dotSign(toTarget, along) <= 0) {
            return ResultPoint(origin);
        }
    }
    if constexpr (stopsAtHeading) {
        if (dotSign(target - second, along) >= 0) {
            return ResultPoint(heading);
        }
    }
    const ResultNumber ratio = (toTarget * along) / squaredLength;
    return ResultPoint(first.x() + ratio * along.x(), first.y() + ratio * along.y());
}

/** @brief The best pair found so far, and the squared distance between them. */
template <class ResultNumber, class LabelType>
struct ClosestCandidate {
    std::optional<std::array<Point<ResultNumber, LabelType>, 2>> pair{};
    ResultNumber distance{};
};

/**
 * @brief Offers every candidate pair between two boundary pieces to @p best.
 *
 * The order is kept: the first point of a candidate is on @p first.
 */
template <class ResultNumber, class LabelType, class FirstPiece, class SecondPiece>
constexpr void offerPiecePair(const FirstPiece& first, const SecondPiece& second,
                              ClosestCandidate<ResultNumber, LabelType>& best) {
    using ResultPoint = Point<ResultNumber, LabelType>;
    const auto offer = [&best](ResultPoint here, ResultPoint there) {
        const ResultNumber distance = here.template squaredDistance<ResultNumber>(there);
        if (!best.pair || distance < best.distance) {
            best.distance = distance;
            best.pair = std::array<ResultPoint, 2>{std::move(here), std::move(there)};
        }
    };
    for (const auto& end : pieceEnds(first)) {
        offer(ResultPoint(end), closestPointOn<ResultNumber, LabelType>(second, end));
    }
    for (const auto& end : pieceEnds(second)) {
        offer(closestPointOn<ResultNumber, LabelType>(first, end), ResultPoint(end));
    }
}

/**
 * @brief Calls @p visitor on each boundary piece of an unbounded convex shape.
 *
 * A shape at positive distance is entirely outside a closed convex set, and the
 * nearest point of one of those to an outside point is on its boundary, so the
 * boundary is the whole search.
 */
template <class ResultNumber, class Unbounded, class Visitor>
constexpr void forEachBoundaryPiece(const Unbounded& shape, Visitor&& visitor) {
    if constexpr (HalfplaneIntersectionConcept<Unbounded>) {
        if (shape.empty()) {
            return;
        }
        for (std::size_t i = 0; i < shape.size(); ++i) {
            std::visit([&visitor](const auto& piece) { visitor(piece); },
                       shape.template edge<ResultNumber>(i));
        }
    } else if constexpr (LineConcept<Unbounded>) {
        visitor(shape);
    } else if constexpr (RayConcept<Unbounded>) {
        visitor(shape);
    } else {
        // An oriented line drops its direction; a half-plane at positive
        // distance is only ever reached at its boundary line.
        visitor(shape.asLine());
    }
}

/** @brief Implements `closestPoints`; see @ref closestNativeSegments. */
template <class ResultNumber, class LabelType, class Self, class Other>
constexpr auto closestPointsOf(const Self& self, const Other& other) {
    using Compare = closestCompare_t<Self, Other>;
    using ResultPoint = Point<ResultNumber, LabelType>;
    using Result = std::optional<std::array<ResultPoint, 2>>;

    ClosestCandidate<Compare, LabelType> best;
    if constexpr (BoundedPolygonalConcept<Self> && BoundedPolygonalConcept<Other>) {
        // Refine the elements `closestSegments` names, so the two methods always
        // answer about the same pair even where several realize the distance.
        const auto native = closestNativeSegments(self, other);
        if (!native) {
            return Result{};
        }
        offerPiecePair<Compare, LabelType>(native->first, native->second, best);
    } else if (self.template squaredDistance<Compare>(other) == Compare{}) {
        return Result{};
    } else if constexpr (BoundedPolygonalConcept<Self>) {
        const auto mine = coveringSegments(self);
        forEachBoundaryPiece<Compare>(other, [&mine, &best](const auto& piece) {
            for (const auto& element : mine) {
                offerPiecePair<Compare, LabelType>(element, piece, best);
            }
        });
    } else {
        const auto theirs = coveringSegments(other);
        forEachBoundaryPiece<Compare>(self, [&theirs, &best](const auto& piece) {
            for (const auto& element : theirs) {
                offerPiecePair<Compare, LabelType>(piece, element, best);
            }
        });
    }

    if (!best.pair) {
        return Result{};
    }
    return Result{std::array<ResultPoint, 2>{ResultPoint((*best.pair)[0]), ResultPoint((*best.pair)[1])}};
}

}  // namespace detail

template <class TNumber, class TLabel>
template <class ResultNumber, BoundedPolygonalConcept OtherShape>
    requires detail::ClosestPairConcept<Point<TNumber, TLabel>, OtherShape>
constexpr auto Point<TNumber, TLabel>::closestSegments(const OtherShape& other) const {
    return detail::closestSegmentsOf<ResultNumber, TLabel>(*this, other);
}

template <class TNumber, class TLabel>
template <class ResultNumber, class OtherShape>
    requires detail::ClosestPointsPairConcept<Point<TNumber, TLabel>, OtherShape>
constexpr auto Point<TNumber, TLabel>::closestPoints(const OtherShape& other) const {
    return detail::closestPointsOf<ResultNumber, TLabel>(*this, other);
}

template <class TPoint, class TLabel>
template <class ResultNumber, BoundedPolygonalConcept OtherShape>
    requires detail::ClosestPairConcept<Segment<TPoint, TLabel>, OtherShape>
constexpr auto Segment<TPoint, TLabel>::closestSegments(const OtherShape& other) const {
    return detail::closestSegmentsOf<ResultNumber, typename TPoint::LabelType>(*this, other);
}

template <class TPoint, class TLabel>
template <class ResultNumber, class OtherShape>
    requires detail::ClosestPointsPairConcept<Segment<TPoint, TLabel>, OtherShape>
constexpr auto Segment<TPoint, TLabel>::closestPoints(const OtherShape& other) const {
    return detail::closestPointsOf<ResultNumber, typename TPoint::LabelType>(*this, other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, BoundedPolygonalConcept OtherShape>
    requires detail::ClosestPairConcept<OrientedSegment<PointType_, TLabel>, OtherShape>
constexpr auto OrientedSegment<PointType_, TLabel>::closestSegments(const OtherShape& other) const {
    return detail::closestSegmentsOf<ResultNumber, typename PointType_::LabelType>(*this, other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, class OtherShape>
    requires detail::ClosestPointsPairConcept<OrientedSegment<PointType_, TLabel>, OtherShape>
constexpr auto OrientedSegment<PointType_, TLabel>::closestPoints(const OtherShape& other) const {
    return detail::closestPointsOf<ResultNumber, typename PointType_::LabelType>(*this, other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, BoundedPolygonalConcept OtherShape>
    requires detail::ClosestPairConcept<Rectangle<PointType_, TLabel>, OtherShape>
constexpr auto Rectangle<PointType_, TLabel>::closestSegments(const OtherShape& other) const {
    return detail::closestSegmentsOf<ResultNumber, typename PointType_::LabelType>(*this, other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, class OtherShape>
    requires detail::ClosestPointsPairConcept<Rectangle<PointType_, TLabel>, OtherShape>
constexpr auto Rectangle<PointType_, TLabel>::closestPoints(const OtherShape& other) const {
    return detail::closestPointsOf<ResultNumber, typename PointType_::LabelType>(*this, other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, BoundedPolygonalConcept OtherShape>
    requires detail::ClosestPairConcept<Triangle<PointType_, TLabel>, OtherShape>
constexpr auto Triangle<PointType_, TLabel>::closestSegments(const OtherShape& other) const {
    return detail::closestSegmentsOf<ResultNumber, typename PointType_::LabelType>(*this, other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, class OtherShape>
    requires detail::ClosestPointsPairConcept<Triangle<PointType_, TLabel>, OtherShape>
constexpr auto Triangle<PointType_, TLabel>::closestPoints(const OtherShape& other) const {
    return detail::closestPointsOf<ResultNumber, typename PointType_::LabelType>(*this, other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, BoundedPolygonalConcept OtherShape>
    requires detail::ClosestPairConcept<Convex<PointType_, TLabel>, OtherShape>
constexpr auto Convex<PointType_, TLabel>::closestSegments(const OtherShape& other) const {
    return detail::closestSegmentsOf<ResultNumber, typename PointType_::LabelType>(*this, other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, class OtherShape>
    requires detail::ClosestPointsPairConcept<Convex<PointType_, TLabel>, OtherShape>
constexpr auto Convex<PointType_, TLabel>::closestPoints(const OtherShape& other) const {
    return detail::closestPointsOf<ResultNumber, typename PointType_::LabelType>(*this, other);
}

template <class PointType_, class TLabel, class Storage>
template <class ResultNumber, BoundedPolygonalConcept OtherShape>
    requires detail::ClosestPairConcept<MonotoneChain<PointType_, TLabel, Storage>, OtherShape>
constexpr auto MonotoneChain<PointType_, TLabel, Storage>::closestSegments(const OtherShape& other) const {
    return detail::closestSegmentsOf<ResultNumber, typename PointType_::LabelType>(*this, other);
}

template <class PointType_, class TLabel, class Storage>
template <class ResultNumber, class OtherShape>
    requires detail::ClosestPointsPairConcept<MonotoneChain<PointType_, TLabel, Storage>, OtherShape>
constexpr auto MonotoneChain<PointType_, TLabel, Storage>::closestPoints(const OtherShape& other) const {
    return detail::closestPointsOf<ResultNumber, typename PointType_::LabelType>(*this, other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, BoundedPolygonalConcept OtherShape>
    requires detail::ClosestPairConcept<Polyline<PointType_, TLabel>, OtherShape>
constexpr auto Polyline<PointType_, TLabel>::closestSegments(const OtherShape& other) const {
    return detail::closestSegmentsOf<ResultNumber, typename PointType_::LabelType>(*this, other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, class OtherShape>
    requires detail::ClosestPointsPairConcept<Polyline<PointType_, TLabel>, OtherShape>
constexpr auto Polyline<PointType_, TLabel>::closestPoints(const OtherShape& other) const {
    return detail::closestPointsOf<ResultNumber, typename PointType_::LabelType>(*this, other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, BoundedPolygonalConcept OtherShape>
    requires detail::ClosestPairConcept<Polygon<PointType_, TLabel>, OtherShape>
constexpr auto Polygon<PointType_, TLabel>::closestSegments(const OtherShape& other) const {
    return detail::closestSegmentsOf<ResultNumber, typename PointType_::LabelType>(*this, other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, class OtherShape>
    requires detail::ClosestPointsPairConcept<Polygon<PointType_, TLabel>, OtherShape>
constexpr auto Polygon<PointType_, TLabel>::closestPoints(const OtherShape& other) const {
    return detail::closestPointsOf<ResultNumber, typename PointType_::LabelType>(*this, other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, BoundedPolygonalConcept OtherShape>
    requires detail::ClosestPairConcept<PolygonWithHoles<PointType_, TLabel>, OtherShape>
constexpr auto PolygonWithHoles<PointType_, TLabel>::closestSegments(const OtherShape& other) const {
    return detail::closestSegmentsOf<ResultNumber, typename PointType_::LabelType>(*this, other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, class OtherShape>
    requires detail::ClosestPointsPairConcept<PolygonWithHoles<PointType_, TLabel>, OtherShape>
constexpr auto PolygonWithHoles<PointType_, TLabel>::closestPoints(const OtherShape& other) const {
    return detail::closestPointsOf<ResultNumber, typename PointType_::LabelType>(*this, other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, BoundedPolygonalConcept OtherShape>
    requires detail::ClosestPairConcept<PolygonSet<PointType_, TLabel>, OtherShape>
auto PolygonSet<PointType_, TLabel>::closestSegments(const OtherShape& other) const {
    return detail::closestSegmentsOf<ResultNumber, typename PointType_::LabelType>(*this, other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, class OtherShape>
    requires detail::ClosestPointsPairConcept<PolygonSet<PointType_, TLabel>, OtherShape>
auto PolygonSet<PointType_, TLabel>::closestPoints(const OtherShape& other) const {
    return detail::closestPointsOf<ResultNumber, typename PointType_::LabelType>(*this, other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, class OtherShape>
    requires detail::ClosestPointsPairConcept<Line<PointType_, TLabel>, OtherShape>
constexpr auto Line<PointType_, TLabel>::closestPoints(const OtherShape& other) const {
    return detail::closestPointsOf<ResultNumber, typename PointType_::LabelType>(*this, other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, class OtherShape>
    requires detail::ClosestPointsPairConcept<OrientedLine<PointType_, TLabel>, OtherShape>
constexpr auto OrientedLine<PointType_, TLabel>::closestPoints(const OtherShape& other) const {
    return detail::closestPointsOf<ResultNumber, typename PointType_::LabelType>(*this, other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, class OtherShape>
    requires detail::ClosestPointsPairConcept<Ray<PointType_, TLabel>, OtherShape>
constexpr auto Ray<PointType_, TLabel>::closestPoints(const OtherShape& other) const {
    return detail::closestPointsOf<ResultNumber, typename PointType_::LabelType>(*this, other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, class OtherShape>
    requires detail::ClosestPointsPairConcept<Halfplane<PointType_, TLabel>, OtherShape>
constexpr auto Halfplane<PointType_, TLabel>::closestPoints(const OtherShape& other) const {
    return detail::closestPointsOf<ResultNumber, typename PointType_::LabelType>(*this, other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, class OtherShape>
    requires detail::ClosestPointsPairConcept<HalfplaneIntersection<PointType_, TLabel>, OtherShape>
constexpr auto HalfplaneIntersection<PointType_, TLabel>::closestPoints(const OtherShape& other) const {
    return detail::closestPointsOf<ResultNumber, typename PointType_::LabelType>(*this, other);
}

}  // namespace pgl
