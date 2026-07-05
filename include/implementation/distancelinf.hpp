#pragma once

#include "implementation/distancel1.hpp"

/**
 * @file distancelinf.hpp
 * @brief Chebyshev (LInf) distance between shapes.
 *
 * Mirrors the shape coverage of distancel1.hpp, but for the LInf metric
 * `max(|dx|, |dy|)` instead of L1. Implemented independently of L1 (no
 * coordinate-transform trick relating the two metrics) -- the two files
 * happen to share the same overall shape-recursion structure because that
 * structure only relies on convexity, which both metrics have, not on any
 * relationship between them.
 */

namespace pgl {

namespace detail {

/**
 * @brief Minimum LInf distance from @p q to the point `a + t*(b-a)`,
 * minimized over the same `t`-domain convention as
 * @ref segmentLikeDistanceL1 (see there for @p boundedLow / @p boundedHigh).
 *
 * `f(t) = max(|dx(t)|, |dy(t)|)` is convex and piecewise-linear in `t`, so
 * its constrained minimum is again among a finite candidate set: the domain
 * endpoints (if bounded), the two points where `dx(t)` or `dy(t)` is
 * individually zero, and the two points where `dx(t) = dy(t)` or
 * `dx(t) = -dy(t)` (where the two branches of the `max` cross). Every one of
 * these four crossings works out to the same closed form `|cross| / |D|`
 * for a crossing-specific denominator `D`: `dx`, `dy`, `dx-dy`, and `dx+dy`
 * respectively, where `cross = dx*py - dy*px`. As in the L1 case, a crossing
 * at `t = N/D` lies in `[0,1]` iff `N*D >= 0` and `N*D <= D*D`, which keeps
 * the domain check division-free.
 */
template <class ResultNumber>
constexpr ResultNumber segmentLikeDistanceLInf(const Point<ResultNumber>& a, const Point<ResultNumber>& b,
                                                const Point<ResultNumber>& q, bool boundedLow, bool boundedHigh) {
    const ResultNumber dx = b.x() - a.x();
    const ResultNumber dy = b.y() - a.y();
    const ResultNumber px = q.x() - a.x();
    const ResultNumber py = q.y() - a.y();

    if (dx == ResultNumber{} && dy == ResultNumber{}) {
        const ResultNumber ax = pgl::detail::abs(px);
        const ResultNumber ay = pgl::detail::abs(py);
        return ax > ay ? ax : ay;
    }

    bool haveBest = false;
    ResultNumber best{};
    const auto consider = [&](const ResultNumber& value) {
        if (!haveBest || value < best) {
            best = value;
            haveBest = true;
        }
    };

    if (boundedLow) {
        const ResultNumber ax = pgl::detail::abs(px);
        const ResultNumber ay = pgl::detail::abs(py);
        consider(ax > ay ? ax : ay);
    }
    if (boundedHigh) {
        const ResultNumber ax = pgl::detail::abs(px - dx);
        const ResultNumber ay = pgl::detail::abs(py - dy);
        consider(ax > ay ? ax : ay);
    }

    const ResultNumber cross = dx * py - dy * px;
    const auto considerCrossing = [&](const ResultNumber& n, const ResultNumber& d) {
        if (d == ResultNumber{}) {
            return;
        }
        if (boundedLow && n * d < ResultNumber{}) {
            return;
        }
        if (boundedHigh && n * d > d * d) {
            return;
        }
        consider(pgl::detail::abs(cross) / pgl::detail::abs(d));
    };
    considerCrossing(px, dx);
    considerCrossing(py, dy);
    considerCrossing(px - py, dx - dy);
    considerCrossing(px + py, dx + dy);

    return best;
}

/**
 * @brief Farthest vertex of `self` from `other` under LInf: sup_{v in self} distanceLInf(v, other).
 *
 * Mirrors @ref maxVertexDistanceL1 / `maxVertexSquaredDistance`: this only
 * relies on convexity of distance-to-`other`, which holds regardless of
 * metric, so the same vertex-supremum argument applies here too. See
 * @ref maxVertexDistanceL1 for why the vertex query falls back to an
 * untemplated call when `other` is a `Point`.
 */
template <class ResultNumber, class Self, class OtherShape>
constexpr ResultNumber maxVertexDistanceLInf(const Self& self, const OtherShape& other) {
    const auto self_vertices = self.vertices();
    const auto distanceToVertex = [&other](const auto& vertex) -> ResultNumber {
        if constexpr (requires { other.template distanceLInf<ResultNumber>(vertex); }) {
            return other.template distanceLInf<ResultNumber>(vertex);
        } else {
            return static_cast<ResultNumber>(other.distanceLInf(vertex));
        }
    };
    ResultNumber worst = distanceToVertex(self_vertices[0]);
    for (std::size_t index = 1; index < self_vertices.size(); ++index) {
        const ResultNumber current = distanceToVertex(self_vertices[index]);
        if (worst < current) {
            worst = current;
        }
    }
    return worst;
}

/**
 * @brief LInf distance from an external point to a disk's circle boundary.
 *
 * Mirrors @ref diskPointDistanceL1 but minimizes
 * `max(|a + r*cos(t)|, |b + r*sin(t)|)`; see there for why this needs a
 * numeric coarse-scan-then-golden-section search rather than a closed form.
 */
inline double diskPointDistanceLInf(double a, double b, double r) {
    const auto h = [&](double theta) {
        return std::max(std::abs(a + r * std::cos(theta)), std::abs(b + r * std::sin(theta)));
    };

    constexpr int coarseSteps = 720;
    double bestTheta = 0.0;
    double bestValue = h(0.0);
    for (int i = 1; i < coarseSteps; ++i) {
        const double theta = 2.0 * std::numbers::pi_v<double> * i / coarseSteps;
        const double value = h(theta);
        if (value < bestValue) {
            bestValue = value;
            bestTheta = theta;
        }
    }

    const double step = 2.0 * std::numbers::pi_v<double> / coarseSteps;
    double lo = bestTheta - step;
    double hi = bestTheta + step;
    constexpr double invPhi = 0.6180339887498949;
    double c1 = hi - invPhi * (hi - lo);
    double c2 = lo + invPhi * (hi - lo);
    double hc1 = h(c1);
    double hc2 = h(c2);
    for (int i = 0; i < 100; ++i) {
        if (hc1 < hc2) {
            hi = c2;
            c2 = c1;
            hc2 = hc1;
            c1 = hi - invPhi * (hi - lo);
            hc1 = h(c1);
        } else {
            lo = c1;
            c1 = c2;
            hc1 = hc2;
            c2 = lo + invPhi * (hi - lo);
            hc2 = h(c2);
        }
    }

    return std::min({bestValue, hc1, hc2});
}

}  // namespace detail

// -----------------------------------------------------------------------------
// Point

template <class Number, class Label>
template <class ResultNumber, PointConcept OtherPoint>
constexpr auto Point<Number, Label>::hausdorffDistanceLInf(const OtherPoint& other) const {
    return static_cast<ResultNumber>(distanceLInf(other));
}

// -----------------------------------------------------------------------------
// Disk

template <class PointType_, class TLabel>
template <PointConcept OtherPoint>
double Disk<PointType_, TLabel>::distanceLInf(const OtherPoint& point) const {
    if (contains(point)) {
        return 0.0;
    }
    const double a = center<double>().x() - static_cast<double>(point.x());
    const double b = center<double>().y() - static_cast<double>(point.y());
    return detail::diskPointDistanceLInf(a, b, radius<double>());
}

// -----------------------------------------------------------------------------
// Segment

template <class PointType, class LabelType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr auto Segment<PointType, LabelType>::distanceLInf(const OtherPoint& point) const {
    return detail::segmentLikeDistanceLInf<ResultNumber>(
        Point<ResultNumber>(min()), Point<ResultNumber>(max()), Point<ResultNumber>(point), true, true);
}

template <class PointType, class LabelType>
template <class ResultNumber, SegmentConcept OtherSegment>
constexpr auto Segment<PointType, LabelType>::distanceLInf(const OtherSegment& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }

    const auto this_min_to_other = other.template distanceLInf<ResultNumber>(min());
    const auto this_max_to_other = other.template distanceLInf<ResultNumber>(max());
    const auto other_min_to_this = this->template distanceLInf<ResultNumber>(other.min());
    const auto other_max_to_this = this->template distanceLInf<ResultNumber>(other.max());

    const auto best_from_this = this_min_to_other < this_max_to_other ? this_min_to_other : this_max_to_other;
    const auto best_from_other = other_min_to_this < other_max_to_this ? other_min_to_this : other_max_to_this;

    return best_from_this < best_from_other ? best_from_this : best_from_other;
}

// -----------------------------------------------------------------------------
// OrientedSegment

template <class PointType, class LabelType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr auto OrientedSegment<PointType, LabelType>::distanceLInf(const OtherPoint& point) const {
    return static_cast<Segment<PointType>>(*this).template distanceLInf<ResultNumber>(point);
}

template <class PointType, class LabelType>
template <class ResultNumber, SegmentConcept OtherSegment>
constexpr auto OrientedSegment<PointType, LabelType>::distanceLInf(const OtherSegment& other) const {
    return static_cast<Segment<PointType>>(*this).template distanceLInf<ResultNumber>(other);
}

template <class PointType, class LabelType>
template <class ResultNumber, OrientedSegmentConcept OtherOrientedSegment>
constexpr auto OrientedSegment<PointType, LabelType>::distanceLInf(const OtherOrientedSegment& other) const {
    return static_cast<Segment<PointType>>(*this).template distanceLInf<ResultNumber>(
        static_cast<Segment<typename OtherOrientedSegment::PointType>>(other));
}

// -----------------------------------------------------------------------------
// Line

template <class PointType, class LabelType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr auto Line<PointType, LabelType>::distanceLInf(const OtherPoint& point) const {
    return detail::segmentLikeDistanceLInf<ResultNumber>(
        Point<ResultNumber>(min()), Point<ResultNumber>(max()), Point<ResultNumber>(point), false, false);
}

template <class PointType, class LabelType>
template <class ResultNumber, LineConcept OtherLine>
constexpr auto Line<PointType, LabelType>::distanceLInf(const OtherLine& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return other.template distanceLInf<ResultNumber>(min());
}

template <class PointType, class LabelType>
template <class ResultNumber, SegmentConcept OtherSegment>
constexpr auto Line<PointType, LabelType>::distanceLInf(const OtherSegment& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }

    const auto source_distance = this->template distanceLInf<ResultNumber>(other.min());
    const auto target_distance = this->template distanceLInf<ResultNumber>(other.max());
    return source_distance < target_distance ? source_distance : target_distance;
}

template <class PointType, class LabelType>
template <class ResultNumber, OrientedSegmentConcept OtherOrientedSegment>
constexpr auto Line<PointType, LabelType>::distanceLInf(const OtherOrientedSegment& other) const {
    return this->template distanceLInf<ResultNumber>(static_cast<Segment<typename OtherOrientedSegment::PointType>>(other));
}

// -----------------------------------------------------------------------------
// OrientedLine

template <class PointType, class LabelType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr auto OrientedLine<PointType, LabelType>::distanceLInf(const OtherPoint& point) const {
    return this->asLine().template distanceLInf<ResultNumber>(point);
}

template <class PointType, class LabelType>
template <class ResultNumber, LineConcept OtherLine>
constexpr auto OrientedLine<PointType, LabelType>::distanceLInf(const OtherLine& other) const {
    return this->asLine().template distanceLInf<ResultNumber>(other);
}

template <class PointType, class LabelType>
template <class ResultNumber, OrientedLineConcept OtherOrientedLine>
constexpr auto OrientedLine<PointType, LabelType>::distanceLInf(const OtherOrientedLine& other) const {
    return this->asLine().template distanceLInf<ResultNumber>(other.asLine());
}

template <class PointType, class LabelType>
template <class ResultNumber, SegmentConcept OtherSegment>
constexpr auto OrientedLine<PointType, LabelType>::distanceLInf(const OtherSegment& other) const {
    return this->asLine().template distanceLInf<ResultNumber>(other);
}

template <class PointType, class LabelType>
template <class ResultNumber, OrientedSegmentConcept OtherOrientedSegment>
constexpr auto OrientedLine<PointType, LabelType>::distanceLInf(const OtherOrientedSegment& other) const {
    return this->asLine().template distanceLInf<ResultNumber>(other);
}

// -----------------------------------------------------------------------------
// Ray

template <class PointType, class LabelType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr auto Ray<PointType, LabelType>::distanceLInf(const OtherPoint& point) const {
    return detail::segmentLikeDistanceLInf<ResultNumber>(
        Point<ResultNumber>(source()), Point<ResultNumber>(target()), Point<ResultNumber>(point), true, false);
}

template <class PointType, class LabelType>
template <class ResultNumber, LineConcept OtherLine>
constexpr auto Ray<PointType, LabelType>::distanceLInf(const OtherLine& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return other.template distanceLInf<ResultNumber>(source());
}

template <class PointType, class LabelType>
template <class ResultNumber, OrientedLineConcept OtherOrientedLine>
constexpr auto Ray<PointType, LabelType>::distanceLInf(const OtherOrientedLine& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return other.template distanceLInf<ResultNumber>(source());
}

template <class PointType, class LabelType>
template <class ResultNumber, SegmentConcept OtherSegment>
constexpr auto Ray<PointType, LabelType>::distanceLInf(const OtherSegment& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }

    const auto source_to_other = other.template distanceLInf<ResultNumber>(source());
    const auto other_min_to_this = this->template distanceLInf<ResultNumber>(other.min());
    const auto other_max_to_this = this->template distanceLInf<ResultNumber>(other.max());
    const auto best_from_segment = other_min_to_this < other_max_to_this ? other_min_to_this : other_max_to_this;
    return source_to_other < best_from_segment ? source_to_other : best_from_segment;
}

template <class PointType, class LabelType>
template <class ResultNumber, OrientedSegmentConcept OtherOrientedSegment>
constexpr auto Ray<PointType, LabelType>::distanceLInf(const OtherOrientedSegment& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }

    const auto source_to_other = other.template distanceLInf<ResultNumber>(source());
    const auto other_source_to_this = this->template distanceLInf<ResultNumber>(other.source());
    const auto other_target_to_this = this->template distanceLInf<ResultNumber>(other.target());
    const auto best_from_segment = other_source_to_this < other_target_to_this ? other_source_to_this : other_target_to_this;
    return source_to_other < best_from_segment ? source_to_other : best_from_segment;
}

template <class PointType, class LabelType>
template <class ResultNumber, RayConcept OtherRay>
constexpr auto Ray<PointType, LabelType>::distanceLInf(const OtherRay& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }

    const auto this_source_to_other = other.template distanceLInf<ResultNumber>(source());
    const auto other_source_to_this = this->template distanceLInf<ResultNumber>(other.source());
    return this_source_to_other < other_source_to_this ? this_source_to_other : other_source_to_this;
}

// -----------------------------------------------------------------------------
// Halfplane

template <class PointType, class LabelType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr auto Halfplane<PointType, LabelType>::distanceLInf(const OtherPoint& point) const {
    if (intersects(point)) {
        return ResultNumber{};
    }
    return asLine().template distanceLInf<ResultNumber>(point);
}

template <class PointType, class LabelType>
template <class ResultNumber, SegmentConcept OtherSegment>
constexpr auto Halfplane<PointType, LabelType>::distanceLInf(const OtherSegment& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return asLine().template distanceLInf<ResultNumber>(other);
}

template <class PointType, class LabelType>
template <class ResultNumber, OrientedSegmentConcept OtherOrientedSegment>
constexpr auto Halfplane<PointType, LabelType>::distanceLInf(const OtherOrientedSegment& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return asLine().template distanceLInf<ResultNumber>(other);
}

template <class PointType, class LabelType>
template <class ResultNumber, LineConcept OtherLine>
constexpr auto Halfplane<PointType, LabelType>::distanceLInf(const OtherLine& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return asLine().template distanceLInf<ResultNumber>(other);
}

template <class PointType, class LabelType>
template <class ResultNumber, OrientedLineConcept OtherOrientedLine>
constexpr auto Halfplane<PointType, LabelType>::distanceLInf(const OtherOrientedLine& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return asLine().template distanceLInf<ResultNumber>(other);
}

template <class PointType, class LabelType>
template <class ResultNumber, RayConcept OtherRay>
constexpr auto Halfplane<PointType, LabelType>::distanceLInf(const OtherRay& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return asLine().template distanceLInf<ResultNumber>(other);
}

template <class PointType, class LabelType>
template <class ResultNumber, HalfplaneConcept OtherHalfplane>
constexpr auto Halfplane<PointType, LabelType>::distanceLInf(const OtherHalfplane& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return asLine().template distanceLInf<ResultNumber>(other.asLine());
}

// -----------------------------------------------------------------------------
// Rectangle

template <class PointType, class LabelType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr auto Rectangle<PointType, LabelType>::distanceLInf(const OtherPoint& point) const {
    const ResultNumber dx = static_cast<ResultNumber>(axisDistance(min().x(), max().x(), point.x(), point.x()));
    const ResultNumber dy = static_cast<ResultNumber>(axisDistance(min().y(), max().y(), point.y(), point.y()));
    return dx > dy ? dx : dy;
}

template <class PointType, class LabelType>
template <class ResultNumber, LineConcept OtherLine>
constexpr auto Rectangle<PointType, LabelType>::distanceLInf(const OtherLine& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }

    const auto rectangle_vertices = vertices();
    auto best_distance = other.template distanceLInf<ResultNumber>(rectangle_vertices[0]);
    for (std::size_t index = 1; index < rectangle_vertices.size(); ++index) {
        const auto current_distance = other.template distanceLInf<ResultNumber>(rectangle_vertices[index]);
        if (current_distance < best_distance) {
            best_distance = current_distance;
        }
    }

    return best_distance;
}

template <class PointType, class LabelType>
template <class ResultNumber, OrientedLineConcept OtherOrientedLine>
constexpr auto Rectangle<PointType, LabelType>::distanceLInf(const OtherOrientedLine& other) const {
    return this->template distanceLInf<ResultNumber>(other.asLine());
}

template <class PointType, class LabelType>
template <class ResultNumber, SegmentConcept OtherSegment>
constexpr auto Rectangle<PointType, LabelType>::distanceLInf(const OtherSegment& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }

    auto best_distance = this->template distanceLInf<ResultNumber>(other.min());
    const auto other_max_distance = this->template distanceLInf<ResultNumber>(other.max());
    if (other_max_distance < best_distance) {
        best_distance = other_max_distance;
    }

    const auto rectangle_edges = edges();
    for (const auto& edge : rectangle_edges) {
        const auto current_distance = edge.template distanceLInf<ResultNumber>(other);
        if (current_distance < best_distance) {
            best_distance = current_distance;
        }
    }

    return best_distance;
}

template <class PointType, class LabelType>
template <class ResultNumber, OrientedSegmentConcept OtherOrientedSegment>
constexpr auto Rectangle<PointType, LabelType>::distanceLInf(const OtherOrientedSegment& other) const {
    return this->template distanceLInf<ResultNumber>(static_cast<Segment<typename OtherOrientedSegment::PointType>>(other));
}

template <class PointType, class LabelType>
template <class ResultNumber, RayConcept OtherRay>
constexpr auto Rectangle<PointType, LabelType>::distanceLInf(const OtherRay& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }

    auto best_distance = this->template distanceLInf<ResultNumber>(other.source());
    const auto rectangle_edges = edges();
    for (const auto& edge : rectangle_edges) {
        const auto current_distance = other.template distanceLInf<ResultNumber>(edge);
        if (current_distance < best_distance) {
            best_distance = current_distance;
        }
    }

    return best_distance;
}

template <class PointType, class LabelType>
template <class ResultNumber, HalfplaneConcept OtherHalfplane>
constexpr auto Rectangle<PointType, LabelType>::distanceLInf(const OtherHalfplane& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template distanceLInf<ResultNumber>(other.asLine());
}

template <class PointType, class LabelType>
template <class ResultNumber, RectangleConcept OtherRectangle>
constexpr auto Rectangle<PointType, LabelType>::distanceLInf(const OtherRectangle& other) const {
    const ResultNumber dx = static_cast<ResultNumber>(axisDistance(min().x(), max().x(), other.min().x(), other.max().x()));
    const ResultNumber dy = static_cast<ResultNumber>(axisDistance(min().y(), max().y(), other.min().y(), other.max().y()));
    return dx > dy ? dx : dy;
}

// -----------------------------------------------------------------------------
// Triangle

template <class PointType, class LabelType>
template <class ResultNumber, class OtherShape>
constexpr ResultNumber Triangle<PointType, LabelType>::edgeMinDistanceLInf(const OtherShape& other) const {
    const auto triangle_edges = edges();
    auto best = triangle_edges[0].template distanceLInf<ResultNumber>(other);
    for (std::size_t index = 1; index < triangle_edges.size(); ++index) {
        const auto current = triangle_edges[index].template distanceLInf<ResultNumber>(other);
        if (current < best) {
            best = current;
        }
    }
    return best;
}

template <class PointType, class LabelType>
template <class ResultNumber, class OtherShape>
constexpr ResultNumber Triangle<PointType, LabelType>::vertexMinDistanceLInf(const OtherShape& other) const {
    const auto triangle_vertices = vertices();
    auto best = other.template distanceLInf<ResultNumber>(triangle_vertices[0]);
    for (std::size_t index = 1; index < triangle_vertices.size(); ++index) {
        const auto current = other.template distanceLInf<ResultNumber>(triangle_vertices[index]);
        if (current < best) {
            best = current;
        }
    }
    return best;
}

template <class PointType, class LabelType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr auto Triangle<PointType, LabelType>::distanceLInf(const OtherPoint& point) const {
    if (intersects(point)) {
        return ResultNumber{};
    }
    return this->template edgeMinDistanceLInf<ResultNumber>(point);
}

template <class PointType, class LabelType>
template <class ResultNumber, SegmentConcept OtherSegment>
constexpr auto Triangle<PointType, LabelType>::distanceLInf(const OtherSegment& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template edgeMinDistanceLInf<ResultNumber>(other);
}

template <class PointType, class LabelType>
template <class ResultNumber, OrientedSegmentConcept OtherOrientedSegment>
constexpr auto Triangle<PointType, LabelType>::distanceLInf(const OtherOrientedSegment& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template edgeMinDistanceLInf<ResultNumber>(other);
}

template <class PointType, class LabelType>
template <class ResultNumber, LineConcept OtherLine>
constexpr auto Triangle<PointType, LabelType>::distanceLInf(const OtherLine& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template vertexMinDistanceLInf<ResultNumber>(other);
}

template <class PointType, class LabelType>
template <class ResultNumber, OrientedLineConcept OtherOrientedLine>
constexpr auto Triangle<PointType, LabelType>::distanceLInf(const OtherOrientedLine& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template vertexMinDistanceLInf<ResultNumber>(other);
}

template <class PointType, class LabelType>
template <class ResultNumber, RayConcept OtherRay>
constexpr auto Triangle<PointType, LabelType>::distanceLInf(const OtherRay& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template edgeMinDistanceLInf<ResultNumber>(other);
}

template <class PointType, class LabelType>
template <class ResultNumber, HalfplaneConcept OtherHalfplane>
constexpr auto Triangle<PointType, LabelType>::distanceLInf(const OtherHalfplane& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template distanceLInf<ResultNumber>(other.asLine());
}

template <class PointType, class LabelType>
template <class ResultNumber, RectangleConcept OtherRectangle>
constexpr auto Triangle<PointType, LabelType>::distanceLInf(const OtherRectangle& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template edgeMinDistanceLInf<ResultNumber>(other);
}

template <class PointType, class LabelType>
template <class ResultNumber, TriangleConcept OtherTriangle>
constexpr auto Triangle<PointType, LabelType>::distanceLInf(const OtherTriangle& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template edgeMinDistanceLInf<ResultNumber>(other);
}

// -----------------------------------------------------------------------------
// Convex
//
// Always uses the O(n) edge scan below (no cyclic support-function fast path):
// see the distanceLInf declarations in convex.hpp for why the Euclidean fast
// path's search functional does not carry over to the LInf gauge.

template <class PointType_, class LabelType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr auto Convex<PointType_, LabelType>::distanceLInf(const OtherPoint& point) const {
    if (contains(point)) {
        return ResultNumber{};
    }
    auto edgeVector = edges();
    ResultNumber best = edgeVector[0].template distanceLInf<ResultNumber>(point);
    for (auto& e : edgeVector) {
        const ResultNumber current = e.template distanceLInf<ResultNumber>(point);
        if (current < best) {
            best = current;
        }
    }
    return best;
}

template <class PointType_, class LabelType>
template <class ResultNumber, SegmentConcept OtherSegment>
constexpr auto Convex<PointType_, LabelType>::distanceLInf(const OtherSegment& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    auto edgeVector = edges();
    ResultNumber best = edgeVector[0].template distanceLInf<ResultNumber>(other);
    for (auto& e : edgeVector) {
        const ResultNumber current = e.template distanceLInf<ResultNumber>(other);
        if (current < best) {
            best = current;
        }
    }
    return best;
}

template <class PointType_, class LabelType>
template <class ResultNumber, OrientedSegmentConcept OtherOrientedSegment>
constexpr auto Convex<PointType_, LabelType>::distanceLInf(const OtherOrientedSegment& other) const {
    return this->template distanceLInf<ResultNumber>(
        static_cast<Segment<typename OtherOrientedSegment::PointType>>(other));
}

template <class PointType_, class LabelType>
template <class ResultNumber, ConvexConcept OtherConvex>
constexpr auto Convex<PointType_, LabelType>::distanceLInf(const OtherConvex& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }

    const auto minOverEdges = [](const auto& source, const auto& target) {
        const auto edgeVector = source.edges();
        ResultNumber best = target.template distanceLInf<ResultNumber>(edgeVector[0]);
        for (const auto& edge : edgeVector) {
            const ResultNumber current = target.template distanceLInf<ResultNumber>(edge);
            if (current < best) {
                best = current;
            }
        }
        return best;
    };

    if (size() <= other.size()) {
        return minOverEdges(*this, other);
    }
    return minOverEdges(other, *this);
}

template <class PointType_, class LabelType>
template <class ResultNumber, TriangleConcept OtherTriangle>
constexpr auto Convex<PointType_, LabelType>::distanceLInf(const OtherTriangle& other) const {
    return this->template distanceLInf<ResultNumber>(other.asConvex());
}

template <class PointType_, class LabelType>
template <class ResultNumber, RectangleConcept OtherRectangle>
constexpr auto Convex<PointType_, LabelType>::distanceLInf(const OtherRectangle& other) const {
    return this->template distanceLInf<ResultNumber>(other.asConvex());
}

template <class PointType_, class LabelType>
template <class ResultNumber, LineConcept OtherLine>
constexpr auto Convex<PointType_, LabelType>::distanceLInf(const OtherLine& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    ResultNumber best = other.template distanceLInf<ResultNumber>(get(0));
    for (std::ptrdiff_t i = 1; i < static_cast<std::ptrdiff_t>(size()); ++i) {
        const ResultNumber current = other.template distanceLInf<ResultNumber>(get(i));
        if (current < best) {
            best = current;
        }
    }
    return best;
}

template <class PointType_, class LabelType>
template <class ResultNumber, OrientedLineConcept OtherOrientedLine>
constexpr auto Convex<PointType_, LabelType>::distanceLInf(const OtherOrientedLine& other) const {
    return this->template distanceLInf<ResultNumber>(other.asLine());
}

template <class PointType_, class LabelType>
template <class ResultNumber, RayConcept OtherRay>
constexpr auto Convex<PointType_, LabelType>::distanceLInf(const OtherRay& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    auto edgeVector = edges();
    ResultNumber best = other.template distanceLInf<ResultNumber>(edgeVector[0]);
    for (const auto& e : edgeVector) {
        const ResultNumber current = other.template distanceLInf<ResultNumber>(e);
        if (current < best) {
            best = current;
        }
    }
    return best;
}

template <class PointType_, class LabelType>
template <class ResultNumber, HalfplaneConcept OtherHalfplane>
constexpr auto Convex<PointType_, LabelType>::distanceLInf(const OtherHalfplane& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template distanceLInf<ResultNumber>(other.asLine());
}

// -----------------------------------------------------------------------------
// Polygon

template <class PointType_, class TLabel>
template <class ResultNumber, class OtherShape>
constexpr ResultNumber Polygon<PointType_, TLabel>::edgeMinDistanceLInf(const OtherShape& other) const {
    const auto boundaryEdges = edges();
    ResultNumber best = boundaryEdges[0].template distanceLInf<ResultNumber>(other);
    for (std::size_t index = 1; index < boundaryEdges.size(); ++index) {
        const ResultNumber current = boundaryEdges[index].template distanceLInf<ResultNumber>(other);
        if (current < best) {
            best = current;
        }
    }
    return best;
}

template <class PointType_, class TLabel>
template <class ResultNumber, PointConcept OtherPoint>
constexpr auto Polygon<PointType_, TLabel>::distanceLInf(const OtherPoint& point) const {
    if (intersects(point)) {
        return ResultNumber{};
    }
    return this->template edgeMinDistanceLInf<ResultNumber>(point);
}

template <class PointType_, class TLabel>
template <class ResultNumber, SegmentConcept OtherSegment>
constexpr auto Polygon<PointType_, TLabel>::distanceLInf(const OtherSegment& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template edgeMinDistanceLInf<ResultNumber>(other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, OrientedSegmentConcept OtherOrientedSegment>
constexpr auto Polygon<PointType_, TLabel>::distanceLInf(const OtherOrientedSegment& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template edgeMinDistanceLInf<ResultNumber>(other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, LineConcept OtherLine>
constexpr auto Polygon<PointType_, TLabel>::distanceLInf(const OtherLine& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template edgeMinDistanceLInf<ResultNumber>(other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, OrientedLineConcept OtherOrientedLine>
constexpr auto Polygon<PointType_, TLabel>::distanceLInf(const OtherOrientedLine& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template edgeMinDistanceLInf<ResultNumber>(other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, RayConcept OtherRay>
constexpr auto Polygon<PointType_, TLabel>::distanceLInf(const OtherRay& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template edgeMinDistanceLInf<ResultNumber>(other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, HalfplaneConcept OtherHalfplane>
constexpr auto Polygon<PointType_, TLabel>::distanceLInf(const OtherHalfplane& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template edgeMinDistanceLInf<ResultNumber>(other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, RectangleConcept OtherRectangle>
constexpr auto Polygon<PointType_, TLabel>::distanceLInf(const OtherRectangle& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template edgeMinDistanceLInf<ResultNumber>(other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, TriangleConcept OtherTriangle>
constexpr auto Polygon<PointType_, TLabel>::distanceLInf(const OtherTriangle& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template edgeMinDistanceLInf<ResultNumber>(other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, ConvexConcept OtherConvex>
constexpr auto Polygon<PointType_, TLabel>::distanceLInf(const OtherConvex& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template edgeMinDistanceLInf<ResultNumber>(other);
}

template <class PointType_, class TLabel>
template <class ResultNumber, PolygonConcept OtherPolygon>
constexpr auto Polygon<PointType_, TLabel>::distanceLInf(const OtherPolygon& other) const {
    if (intersects(other)) {
        return ResultNumber{};
    }
    return this->template edgeMinDistanceLInf<ResultNumber>(other);
}

// -----------------------------------------------------------------------------
// Hausdorff distance (added only where a Euclidean squaredHausdorffDistance
// overload already exists; Polygon has none today, so it gets none here).

template <class PointType, class LabelType>
template <class ResultNumber, SegmentConcept OtherSegment>
constexpr auto Segment<PointType, LabelType>::hausdorffDistanceLInf(const OtherSegment& other) const {
    const auto worst_from_this = detail::maxVertexDistanceLInf<ResultNumber>(*this, other);
    const auto worst_from_other = detail::maxVertexDistanceLInf<ResultNumber>(other, *this);
    return worst_from_this > worst_from_other ? worst_from_this : worst_from_other;
}

template <class PointType, class LabelType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr auto Segment<PointType, LabelType>::hausdorffDistanceLInf(const OtherPoint& point) const {
    return detail::maxVertexDistanceLInf<ResultNumber>(*this, point);
}

template <class PointType, class LabelType>
template <class ResultNumber, SegmentConcept OtherSegment>
constexpr auto OrientedSegment<PointType, LabelType>::hausdorffDistanceLInf(const OtherSegment& other) const {
    return static_cast<Segment<PointType>>(*this).template hausdorffDistanceLInf<ResultNumber>(other);
}

template <class PointType, class LabelType>
template <class ResultNumber, OrientedSegmentConcept OtherOrientedSegment>
constexpr auto OrientedSegment<PointType, LabelType>::hausdorffDistanceLInf(const OtherOrientedSegment& other) const {
    return static_cast<Segment<PointType>>(*this).template hausdorffDistanceLInf<ResultNumber>(
        static_cast<Segment<typename OtherOrientedSegment::PointType>>(other));
}

template <class PointType, class LabelType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr auto OrientedSegment<PointType, LabelType>::hausdorffDistanceLInf(const OtherPoint& point) const {
    return static_cast<Segment<PointType>>(*this).template hausdorffDistanceLInf<ResultNumber>(point);
}

template <class PointType, class LabelType>
template <class ResultNumber, RectangleConcept OtherRectangle>
constexpr auto Rectangle<PointType, LabelType>::hausdorffDistanceLInf(const OtherRectangle& other) const {
    const auto worst_from_this = detail::maxVertexDistanceLInf<ResultNumber>(*this, other);
    const auto worst_from_other = detail::maxVertexDistanceLInf<ResultNumber>(other, *this);
    return worst_from_this > worst_from_other ? worst_from_this : worst_from_other;
}

template <class PointType, class LabelType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr auto Rectangle<PointType, LabelType>::hausdorffDistanceLInf(const OtherPoint& point) const {
    return detail::maxVertexDistanceLInf<ResultNumber>(*this, point);
}

template <class PointType, class LabelType>
template <class ResultNumber, SegmentConcept OtherSegment>
constexpr auto Rectangle<PointType, LabelType>::hausdorffDistanceLInf(const OtherSegment& other) const {
    const auto worst_from_this = detail::maxVertexDistanceLInf<ResultNumber>(*this, other);
    const auto worst_from_other = detail::maxVertexDistanceLInf<ResultNumber>(other, *this);
    return worst_from_this > worst_from_other ? worst_from_this : worst_from_other;
}

template <class PointType, class LabelType>
template <class ResultNumber, OrientedSegmentConcept OtherOrientedSegment>
constexpr auto Rectangle<PointType, LabelType>::hausdorffDistanceLInf(const OtherOrientedSegment& other) const {
    const auto worst_from_this = detail::maxVertexDistanceLInf<ResultNumber>(*this, other);
    const auto worst_from_other = detail::maxVertexDistanceLInf<ResultNumber>(other, *this);
    return worst_from_this > worst_from_other ? worst_from_this : worst_from_other;
}

template <class PointType, class LabelType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr auto Triangle<PointType, LabelType>::hausdorffDistanceLInf(const OtherPoint& point) const {
    return detail::maxVertexDistanceLInf<ResultNumber>(*this, point);
}

template <class PointType, class LabelType>
template <class ResultNumber, SegmentConcept OtherSegment>
constexpr auto Triangle<PointType, LabelType>::hausdorffDistanceLInf(const OtherSegment& other) const {
    const auto worst_from_this = detail::maxVertexDistanceLInf<ResultNumber>(*this, other);
    const auto worst_from_other = detail::maxVertexDistanceLInf<ResultNumber>(other, *this);
    return worst_from_this > worst_from_other ? worst_from_this : worst_from_other;
}

template <class PointType, class LabelType>
template <class ResultNumber, OrientedSegmentConcept OtherOrientedSegment>
constexpr auto Triangle<PointType, LabelType>::hausdorffDistanceLInf(const OtherOrientedSegment& other) const {
    const auto worst_from_this = detail::maxVertexDistanceLInf<ResultNumber>(*this, other);
    const auto worst_from_other = detail::maxVertexDistanceLInf<ResultNumber>(other, *this);
    return worst_from_this > worst_from_other ? worst_from_this : worst_from_other;
}

template <class PointType, class LabelType>
template <class ResultNumber, RectangleConcept OtherRectangle>
constexpr auto Triangle<PointType, LabelType>::hausdorffDistanceLInf(const OtherRectangle& other) const {
    const auto worst_from_this = detail::maxVertexDistanceLInf<ResultNumber>(*this, other);
    const auto worst_from_other = detail::maxVertexDistanceLInf<ResultNumber>(other, *this);
    return worst_from_this > worst_from_other ? worst_from_this : worst_from_other;
}

template <class PointType, class LabelType>
template <class ResultNumber, TriangleConcept OtherTriangle>
constexpr auto Triangle<PointType, LabelType>::hausdorffDistanceLInf(const OtherTriangle& other) const {
    const auto worst_from_this = detail::maxVertexDistanceLInf<ResultNumber>(*this, other);
    const auto worst_from_other = detail::maxVertexDistanceLInf<ResultNumber>(other, *this);
    return worst_from_this > worst_from_other ? worst_from_this : worst_from_other;
}

template <class PointType_, class LabelType>
template <class ResultNumber, PointConcept OtherPoint>
constexpr auto Convex<PointType_, LabelType>::hausdorffDistanceLInf(const OtherPoint& point) const {
    return detail::maxVertexDistanceLInf<ResultNumber>(*this, point);
}

template <class PointType_, class LabelType>
template <class ResultNumber, SegmentConcept OtherSegment>
constexpr auto Convex<PointType_, LabelType>::hausdorffDistanceLInf(const OtherSegment& other) const {
    const auto worst_from_this = detail::maxVertexDistanceLInf<ResultNumber>(*this, other);
    const auto worst_from_other = detail::maxVertexDistanceLInf<ResultNumber>(other, *this);
    return worst_from_this > worst_from_other ? worst_from_this : worst_from_other;
}

template <class PointType_, class LabelType>
template <class ResultNumber, OrientedSegmentConcept OtherOrientedSegment>
constexpr auto Convex<PointType_, LabelType>::hausdorffDistanceLInf(const OtherOrientedSegment& other) const {
    const auto worst_from_this = detail::maxVertexDistanceLInf<ResultNumber>(*this, other);
    const auto worst_from_other = detail::maxVertexDistanceLInf<ResultNumber>(other, *this);
    return worst_from_this > worst_from_other ? worst_from_this : worst_from_other;
}

template <class PointType_, class LabelType>
template <class ResultNumber, RectangleConcept OtherRectangle>
constexpr auto Convex<PointType_, LabelType>::hausdorffDistanceLInf(const OtherRectangle& other) const {
    const auto worst_from_this = detail::maxVertexDistanceLInf<ResultNumber>(*this, other);
    const auto worst_from_other = detail::maxVertexDistanceLInf<ResultNumber>(other, *this);
    return worst_from_this > worst_from_other ? worst_from_this : worst_from_other;
}

template <class PointType_, class LabelType>
template <class ResultNumber, TriangleConcept OtherTriangle>
constexpr auto Convex<PointType_, LabelType>::hausdorffDistanceLInf(const OtherTriangle& other) const {
    const auto worst_from_this = detail::maxVertexDistanceLInf<ResultNumber>(*this, other);
    const auto worst_from_other = detail::maxVertexDistanceLInf<ResultNumber>(other, *this);
    return worst_from_this > worst_from_other ? worst_from_this : worst_from_other;
}

template <class PointType_, class LabelType>
template <class ResultNumber, ConvexConcept OtherConvex>
constexpr auto Convex<PointType_, LabelType>::hausdorffDistanceLInf(const OtherConvex& other) const {
    const auto worst_from_this = detail::maxVertexDistanceLInf<ResultNumber>(*this, other);
    const auto worst_from_other = detail::maxVertexDistanceLInf<ResultNumber>(other, *this);
    return worst_from_this > worst_from_other ? worst_from_this : worst_from_other;
}

}  // namespace pgl
